/*
 * Copyright (c) 2024 roleo.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h> 
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/reboot.h>
#include <ctype.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>

#ifdef HAVE_WOLFSSL
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/sha.h>
#include <wolfssl/wolfcrypt/coding.h>
#else
#ifdef HAVE_MBEDTLS
#include <mbedtls/sha1.h>
#include <mbedtls/base64.h>
#else
#include <tomcrypt.h>
#endif
#endif

#include <zlib.h>

#include "utils.h"
#include "onvif_simple_server.h"
#include "log.h"

#define SHMOBJ_PATH "/onvif_subscription"
#define MEM_LOCK_FILE "/sub_mem_lock"

sem_t *sem_memory_lock = SEM_FAILED;

/**
 * Open a semaphore
 * @return 0 on success, -1 on error
 */
int sem_memory_open()
{
    sem_memory_lock = sem_open(MEM_LOCK_FILE, O_CREAT, S_IRUSR | S_IWUSR, 1);
    if (sem_memory_lock == SEM_FAILED) {
        fprintf(stderr, "Error opening semaphore file %s\n", MEM_LOCK_FILE);
        return -1;
    }
    return 0;
}

/**
 * Close a semaphore
 */
void sem_memory_close()
{
    if (sem_memory_lock != SEM_FAILED) {
        sem_close(sem_memory_lock);
        sem_memory_lock = SEM_FAILED;
        sem_unlink(MEM_LOCK_FILE);
    }
    return;
}

/**
 * Create or open shared memory to share subscriptions and events
 * @param create Set to 1 if the memory must be created, 0 if not
 * @return a pointer to the shared memory
 */
void *create_shared_memory(int create) {
    int shmfd, rc;
    int shared_seg_size = sizeof(shm_t);
    char *shared_area;      /* the pointer to the shared segment */

    /* creating the shared memory object.
    int shm_open(const char *name, int oflag, mode_t mode);
    oflags:
       O_CREAT    Create the shared memory object if it does not  exist.
       O_RDWR     Open the object for read-write access.
       O_RDONLY   Open the object for read access.
       O_EXCL     If  O_CREAT  was  also specified, and a shared memory object
                  with the given name already exists, return  an  error.   The
                  check  for  the existence of the object, and its creation if
                  it does not exist, are performed atomically.
       O_TRUNC    If the shared memory object already exists, truncate  it  to
                  zero bytes.
    mode:
       S_IRWXU    file owner has read, write and execute permission.
       S_IRWXG    group has read, write and execute permission.
       S_IRWXO    others have read, write and execute permission.
       S_IROTH    others have read permission.

    A new shared memory object  initially  has  zero  length.
    The size of the object can be set using ftruncate(2).
    Return Value: On successful completion  shm_open()  returns  a  new
               nonnegative file  descriptor referring to the shared memory object.
               On failure,  shm_open()  returns -1.
    */
    if (create) {
        shmfd = shm_open(SHMOBJ_PATH, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
    } else {
        shmfd = shm_open(SHMOBJ_PATH, O_RDWR, S_IRWXU | S_IRWXG);
    }
    if (shmfd < 0) {
        log_error("shm_open() failed");
        return NULL;
    }
    log_debug("Created shared memory object %s", SHMOBJ_PATH);

    /* adjusting mapped file size (make room for the whole segment to map) */
    rc = ftruncate(shmfd, shared_seg_size);
    if (rc != 0) {
        log_error("ftruncate() failed");
        shm_unlink(SHMOBJ_PATH);
        return NULL;
    }

    /* requesting the shared segment */
    shared_area = (char *)mmap(NULL, shared_seg_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (shared_area == MAP_FAILED /* is ((void*)-1) */ ) {
        log_error("mmap() failed");
        shm_unlink(SHMOBJ_PATH);
        return NULL;
    }
    log_debug("Shared memory segment allocated correctly (%d bytes) at address %p", shared_seg_size, shared_area);

    if (sem_memory_open() != 0) {
        fprintf(stderr, "Error, could not open semaphore\n") ;
        munmap(shared_area, shared_seg_size);
        shm_unlink(SHMOBJ_PATH);
        return NULL;
    }

    return shared_area;
}

/**
 * Destroy shared memory
 * @param shared_area Pointer to the shared memory
 * @param destroy_all Set to 1 to unlink the file
 */
void destroy_shared_memory(void *shared_area, int destroy_all)
{
    int shared_seg_size = sizeof(shm_t);

    // Check for NULL pointer to prevent segfaults
    if (shared_area == NULL) {
        log_debug("destroy_shared_memory called with NULL pointer, skipping");
        return;
    }

    sem_memory_close();

    if (munmap(shared_area, shared_seg_size) != 0) {
        log_error("munmap() failed");
        return;
    }

    if (destroy_all) {
        if (shm_unlink(SHMOBJ_PATH) != 0) {
            log_error("shm_unlink() failed");
            return;
        }
    }
    log_debug("Shared memory segment deallocated correctly");
}

int sem_memory_wait()
{
    struct timespec ts;

    if (sem_memory_lock == SEM_FAILED) {
        log_error("Semaphore not initialized");
        return -1;
    }
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 5;
    if (sem_timedwait(sem_memory_lock, &ts) != 0) {
        log_error("Semaphore wait timed out or failed -- semaphore may be stale");
        return -1;
    }
    return 0;
}

int sem_memory_post()
{
    if (sem_memory_lock == SEM_FAILED) {
        log_error("Semaphore not initialized");
        return -1;
    }
    return sem_post(sem_memory_lock);
}

#ifdef USE_ZLIB
/**
 * Decompress a gzipped file
 * @param file_in The input file name
 * @param file_out The output file pointer
 * @return 0 on success, negative on error
 */
int gzip_d(FILE *file_out, char *file_in)
{
    char buf[1024];
    char file_in_gz[MAX_LEN];

    sprintf(file_in_gz, "%s.gz", file_in);
    gzFile fi;
    if (access(file_in_gz, F_OK) == 0) {
        fi = gzopen(file_in_gz, "rb");
    } else {
        fi = gzopen(file_in, "rb");
    }
    if (!fi) {
        return -1;
    }
    if (file_out == NULL) {
        gzclose(fi);
        return -2;
    }

    gzrewind(fi);
    while (!gzeof(fi)) {
        int len = gzread(fi, buf, sizeof(buf));
        if ((len < 0) || ((len == 0) && (!gzeof(fi)))) {
            gzclose(fi);
            return -3;
        }
        if (fwrite(buf, 1, len, file_out) < 0) {
            gzclose(fi);
            return -4;
        }
    }
    gzclose(fi);

    return 0;
}
#endif

/**
 * Generate ONVIF-compliant SOAP fault response
 * @param out The output type: "stdout", char *ptr or NULL
 * @param fault_subcode The ONVIF fault subcode (e.g., "ter:ActionNotSupported")
 * @param fault_reason The human-readable fault reason
 * @param fault_detail Additional fault details
 * @return the number of bytes written
 */
// Global flag to indicate if the last cat() call returned a SOAP fault
int g_last_response_was_soap_fault = 0;

/**
 * Output appropriate HTTP headers based on whether the response is a SOAP fault
 * @param content_length The content length to include in headers
 */
void output_http_headers(long content_length)
{
    if (g_last_response_was_soap_fault) {
        fprintf(stdout, "Status: 500 Internal Server Error\r\n");
    }
    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n", content_length);
    fprintf(stdout, "Connection: close\r\n\r\n");
}

long cat_soap_fault(char* out, const char* fault_subcode, const char* fault_reason, const char* fault_detail)
{
    const char* soap_fault_template = "<?xml version=\"1.0\" ?>"
                                      "<soapenv:Envelope xmlns:soapenv=\"http://www.w3.org/2003/05/soap-envelope\""
                                      " xmlns:ter=\"http://www.onvif.org/ver10/error\""
                                      " xmlns:xs=\"http://www.w3.org/2000/10/XMLSchema\">"
                                      "<soapenv:Body>"
                                      "<soapenv:Fault>"
                                      "<soapenv:Code>"
                                      "<soapenv:Value>env:Receiver</soapenv:Value>"
                                      "<soapenv:Subcode>"
                                      "<soapenv:Value>%s</soapenv:Value>"
                                      "</soapenv:Subcode>"
                                      "</soapenv:Code>"
                                      "<soapenv:Reason>"
                                      "<soapenv:Text xml:lang=\"en\">%s</soapenv:Text>"
                                      "</soapenv:Reason>"
                                      "<soapenv:Node>http://www.w3.org/2003/05/soap-envelope/node/ultimateReceiver</soapenv:Node>"
                                      "<soapenv:Role>http://www.w3.org/2003/05/soap-envelope/role/ultimateReceiver</soapenv:Role>"
                                      "<soapenv:Detail>"
                                      "<soapenv:Text>%s</soapenv:Text>"
                                      "</soapenv:Detail>"
                                      "</soapenv:Fault>"
                                      "</soapenv:Body>"
                                      "</soapenv:Envelope>\r\n";

    char soap_fault[4096];
    int len = snprintf(soap_fault, sizeof(soap_fault), soap_fault_template, fault_subcode, fault_reason, fault_detail);

    // Set global flag to indicate this is a SOAP fault
    g_last_response_was_soap_fault = 1;

    if (out == NULL) {
        // First call - just return size for Content-Length calculation
        return len;
    } else if (strcmp(out, "stdout") == 0) {
        // Second call - output the SOAP fault body only
        // The service function will handle HTTP status and headers
        printf("%s", soap_fault);
        fflush(stdout);
    } else {
        // Output to buffer
        strcpy(out, soap_fault);
    }

    return len;
}

/**
 * Read a file line by line and send to output after replacing arguments
 * @param out The output type: "sdout", char *ptr or NULL
 * @param filename The input file to process
 * @param num The number of variable arguments
 * @param ... The argument list to replace: src1, dst1, src2, dst2, etc...
 * @return the number of processed bytes
 */
long cat(char *out, char *filename, int num, ...)
{
    va_list valist;
    char new_line[MAX_CAT_LEN];
    char *l;
    char *ptr = out;
    char *pars, *pare, *par_to_find, *par_to_sub;
    int i;
    long ret = 0;
    FILE *file;

    // Reset SOAP fault flag for normal file operations
    g_last_response_was_soap_fault = 0;

#ifdef USE_ZLIB
    file = tmpfile();
    if (file == NULL) {
        log_error("Unable to open file %s", filename);
        return -1;
    }
    if (gzip_d(file, filename) != 0) {
        log_error("Unable to decompress file %s", filename);
        fclose(file);
        return -2;
    }
    rewind(file);
#else
    file = fopen(filename, "r");
    if (!file) {
        log_error("Unable to open file %s", filename);
        return -3;
    }
#endif

    if (!file) {
        log_error("Unable to open file %s", filename);
        // Return ONVIF-compliant SOAP fault instead of empty response
        return cat_soap_fault(out,
                              "ter:ActionNotSupported",
                              "Optional Action Not Implemented",
                              "The requested XML template is not available on this device");
    }

    char line[MAX_CAT_LEN];

    while (fgets(line, sizeof(line), file)) {
        va_start(valist, num);

        memset(new_line, '\0', sizeof(new_line));
        for (i = 0; i < num/2; i++) {
            par_to_find = va_arg(valist, char *);
            par_to_sub = va_arg(valist, char *);
            pars = strstr(line, par_to_find);
            if (pars != NULL) {
                pare = pars + strlen(par_to_find);
                size_t prefix_len = pars - line;
                size_t sub_len = strlen(par_to_sub);
                size_t suffix_len = line + strlen(line) - pare;

                // Check if the replacement will fit in new_line buffer
                if (prefix_len + sub_len + suffix_len < MAX_CAT_LEN - 1) {
                    strncpy(new_line, line, prefix_len);
                    new_line[prefix_len] = '\0'; // Ensure null termination
                    strcpy(&new_line[prefix_len], par_to_sub);
                    strncpy(&new_line[prefix_len + sub_len], pare, suffix_len);
                    new_line[prefix_len + sub_len + suffix_len] = '\0'; // Ensure null termination
                } else {
                    log_error("String replacement would overflow buffer in cat function");
                    // Keep original line unchanged if replacement would overflow
                }
            }
            if (new_line[0] != '\0') {
                strcpy(line, new_line);
                memset(new_line, '\0', sizeof(new_line));
            }
        }
        if (new_line[0] == '\0') {
            l = trim(line);
        } else {
            l = trim(new_line);
        }

        if ((out != NULL) && (*l != '\0')) {
            if (strcmp("stdout", out) == 0) {
                if (*l != '<') {
                    fprintf(stdout, " ");
                }
                fprintf(stdout, "%s", l);
            } else {
                if (*l != '<') {
                    sprintf(ptr, " ");
                    ptr++;
                }
                sprintf(ptr, "%s", l);
                ptr += strlen(l);
            }
        }
        if ((*l != '\0') && (*l != '<')) {
            ret++;
        }
        ret += strlen(l);
        va_end(valist);
    }
    fclose(file);

    return ret;
}

/**
 * Get the IP address/netmask of an interface "name"
 * @param name The name of the interface
 * @param netmask String that will contain the netmask
 * @param address String that will contain the address
 * @return 0 on success, negative on error
 */
int get_ip_address(char *address, char *netmask, char *name)
{
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char *host;
    char *mask;
    struct sockaddr_in *sa, *san;
    int found = 0;

    if (getifaddrs(&ifaddr) == -1) {
        log_error("Error in getifaddrs()");
        return -1;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        if ((strcmp(ifa->ifa_name, name) == 0) && (ifa->ifa_addr->sa_family == AF_INET)) {

            sa = (struct sockaddr_in *) ifa->ifa_addr;
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr), address, 16);
            san = (struct sockaddr_in *) ifa->ifa_netmask;
            inet_ntop(AF_INET, &(((struct sockaddr_in *)san)->sin_addr), netmask, 16);

            log_debug("Interface: <%s>", ifa->ifa_name);
            log_debug("Address: <%s>", address);
            log_debug("Netmask: <%s>", netmask);
            found = 1;
        }
    }

    freeifaddrs(ifaddr);

    if (found != 1)
        return -2;

    return 0;
}

/**
 * Get the MAC address of an interface "name"
 * @param name The name of the interface
 * @param address String that will contain the address
 * @return 0 on success, negative on error
 */
int get_mac_address(char *address, char *name)
{
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[MAX_LEN];
    int success = 0;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) {
        return -1;
    }

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) {
        close(sock);
        return -2;
    }

    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

    for (; it != end; ++it) {
        strcpy(ifr.ifr_name, it->ifr_name);
        if (strcmp(name, ifr.ifr_name) == 0) {
            if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
                if (! (ifr.ifr_flags & IFF_LOOPBACK)) {
                    if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
                        success = 1;
                        break;
                    }
                }
            }
        }
    }

    if (success) {
        sprintf(address, "%02x:%02x:%02x:%02x:%02x:%02x",
                (unsigned char) ifr.ifr_hwaddr.sa_data[0],
                (unsigned char) ifr.ifr_hwaddr.sa_data[1],
                (unsigned char) ifr.ifr_hwaddr.sa_data[2],
                (unsigned char) ifr.ifr_hwaddr.sa_data[3],
                (unsigned char) ifr.ifr_hwaddr.sa_data[4],
                (unsigned char) ifr.ifr_hwaddr.sa_data[5]);
    } else {
        log_error("Unable to get  mac address");
        close(sock);
        return -4;
    }

    log_debug("MAC address: <%s>", address);
    close(sock);

    return 0;
}

/**
 * Convert the netmask to a len
 * @param netmask The netmask to convert
 * @return the len of the netmask
 */
int netmask2prefixlen(char *netmask)
{
    int n;
    int i = 0;

    inet_pton(AF_INET, netmask, &n);

    while (n > 0) {
            n = n >> 1;
            i++;
    }

    log_debug("Prefix length: %d", i);

    return i;
}

/**
 * Get MTU for interface if_name
 * @param if_name The name of the interface
 * @return The value of the MTU
 */
int get_mtu(char *if_name)
{
    int ret = 0;
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    struct ifreq ifr;
    strcpy(ifr.ifr_name, if_name);
    if(ioctl(sock, SIOCGIFMTU, &ifr) == 0) {
        ret = ifr.ifr_mtu;
    }
    return ret;
}

/**
 * Remove spaces from the left of the string
 * @param s The input string
 * @return A pointer to the resulting string
 */
char *ltrim(char *s)
{
    char *p = s;

    while(isspace(*p)) p++;
    return p;
}

/**
 * Remove spaces from the right of the string
 * @param s The input string
 * @return A pointer to the resulting string
 */
char *rtrim(char *s)
{
    int iret = strlen(s);
    char* back = s + iret;
    back--;
    while (isspace(*back)) {
        *back = '\0';
        back--;
    }
    return s;
}

/**
 * Remove spaces from the left and the right of the string
 * @param s The input string
 * @return A pointer to the resulting string
 */
char *trim(char *s)
{
    return ltrim(rtrim(s));
}

/**
 * Remove space, tab, CR and LF from the left of the string
 * @param s The input string
 * @return A pointer to the resulting string
 */
char *ltrim_mf(char *s)
{
    char *p = s;

    while((*p == ' ') || (*p == '\t') || (*p == '\n') || (*p == '\r')) p++;
    return p;
}

/**
 * Remove space, tab, CR and LF from the right of the string
 * @param s The input string
 * @return A pointer to the resulting string
 */
char *rtrim_mf(char *s)
{
    int iret = strlen(s);
    char* back = s + iret;

    if (iret == 0) return s;
    back--;
    while ((back >= s) && ((*back == ' ') || (*back == '\t') || (*back == '\n') || (*back == '\r'))) {
        *back = '\0';
        back--;
    }
    return s;
}

/**
 * Remove space, tab, CR and LF from the left and the right of the string
 * @param s The input string
 * @return A pointer to the resulting string
 */
char *trim_mf(char *s)
{
    return ltrim_mf(rtrim_mf(s));
}

int html_escape(char *url, int max_len)
{
    int i, count = 0;
    char s_tmp[max_len];
    char *f, *t;

    memset(s_tmp, '\0', max_len);

    // Count chars to escape
    for (i = 0; i < strlen(url); i++)
    {
        switch (url[i]) {
        case '\"':
            count += 5;
            break;
        case '&':
            count += 4;
            break;
        case '\'':
            count += 4;
            break;
        case '<':
            count += 3;
            break;
        case '>':
            count += 3;
            break;
        }
    }

    if (strlen(url) + count + 1 > max_len) {
        return -1;
    }

    f = url;
    t = (char *) &s_tmp[0];
    while (*f != '\0' && (t - s_tmp) < (max_len - 10)) { // Leave room for escape sequences
        switch (*f) {
        case '\"':
            if ((t - s_tmp) < (max_len - 6)) { // &quot; = 6 chars
                *t = '&';
                t++;
                *t = 'q';
                t++;
                *t = 'u';
                t++;
                *t = 'o';
                t++;
                *t = 't';
                t++;
                *t = ';';
            } else {
                *t = *f; // Fallback if no room for escape
            }
            break;
        case '&':
            if ((t - s_tmp) < (max_len - 5)) { // &amp; = 5 chars
                *t = '&';
                t++;
                *t = 'a';
                t++;
                *t = 'm';
                t++;
                *t = 'p';
                t++;
                *t = ';';
            } else {
                *t = *f;
            }
            break;
        case '\'':
            if ((t - s_tmp) < (max_len - 5)) { // &#39; = 5 chars
                *t = '&';
                t++;
                *t = '#';
                t++;
                *t = '3';
                t++;
                *t = '9';
                t++;
                *t = ';';
            } else {
                *t = *f;
            }
            break;
        case '<':
            if ((t - s_tmp) < (max_len - 4)) { // &lt; = 4 chars
               *t = '&';
                t++;
                *t = 'l';
                t++;
                *t = 't';
                t++;
                *t = ';';
            } else {
                *t = *f;
            }
            break;
        case '>':
            if ((t - s_tmp) < (max_len - 4)) { // &gt; = 4 chars
                *t = '&';
                t++;
                *t = 'g';
                t++;
                *t = 't';
                t++;
                *t = ';';
            } else {
                *t = *f;
            }
            break;
        default:
            *t = *f;
        }
        t++;
        f++;
    }
    *t = '\0'; // Ensure null termination

    strcpy(url, (char *) s_tmp);
}

/**
 * Hashes a given sequence of bytes using the SHA1 algorithm
 * @param output The hash
 * @param output_size The size of the buffer containing the hash (>= 20)
 * @param input The input sequence pointer
 * @param input_size The size of the input sequence
 * @return A malloc-allocated pointer to the resulting data. 20 bytes long.
 */
int hashSHA1(char* input, unsigned long input_size, char *output, int output_size)
{
    if (output_size < 20)
        return -1;

#ifdef HAVE_WOLFSSL
    wc_Sha sha;
    wc_InitSha(&sha);
    wc_ShaUpdate(&sha, input, input_size);
    wc_ShaFinal(&sha, output);
#else
#ifdef HAVE_MBEDTLS
    //Initial
    mbedtls_sha1_context ctx;

    //Initialize a state variable for the hash
    mbedtls_sha1_init(&ctx);
    mbedtls_sha1_starts(&ctx);
    //Process the text - remember you can call process() multiple times
    mbedtls_sha1_update(&ctx, (const unsigned char*) input, input_size);
    //Finish the hash calculation
    mbedtls_sha1_finish(&ctx, output);
    mbedtls_sha1_free(&ctx);
#else
    //Initial
    hash_state md;

    //Initialize a state variable for the hash
    sha1_init(&md);
    //Process the text - remember you can call process() multiple times
    sha1_process(&md, (const unsigned char*) input, input_size);
    //Finish the hash calculation
    sha1_done(&md, output);
#endif //HAVE_MBEDTLS
#endif //HAVE_WOLFSSL

    return 0;
}

/**
 * Decode a base64 string
 * @param output The decoded sequence pointer
 * @param output_size The size of the buffer/decoded sequence
 * @param input The input sequence pointer
 * @param input_size The size of the input sequence
 */
void b64_decode(unsigned char *input, unsigned int input_size, unsigned char *output, unsigned long *output_size)
{
#ifdef HAVE_WOLFSSL
    word32 olen;
    Base64_Decode((const unsigned char*) input, input_size, output, &olen);
    *output_size = olen;
#else
#ifdef HAVE_MBEDTLS
    size_t olen;
    mbedtls_base64_decode(output, *output_size, &olen, input, input_size);
    *output_size = olen;
#else
    base64_decode(input, input_size, output, output_size);
#endif
#endif
}

/**
 * Encode a base64 string
 * @param output The encoded sequence pointer
 * @param output_size The size of the buffer/encoded sequence
 * @param input The input sequence pointer
 * @param input_size The size of the input sequence
 */
void b64_encode(unsigned char *input, unsigned int input_size, unsigned char *output, unsigned long *output_size)
{
#ifdef HAVE_WOLFSSL
    word32 olen;
    Base64_Encode_NoNl((const unsigned char*) input, input_size, output, &olen);
    *output_size = olen;
#else
#ifdef HAVE_MBEDTLS
    size_t olen;
    mbedtls_base64_encode(output, *output_size, &olen, input, input_size);
    *output_size = olen;
#else
    base64_encode(input, input_size, output, output_size);
#endif
#endif
}

/**
 * Convert an interval in ONVIF notation to a interval in seconds
 * @param interval String that represents the interval
 * @return Time interval in seconds
 */
int interval2sec(const char *interval)
{
    int d1 = -1, d2 = -1, d3 = -1, n, ret;
    char c1 = 'c', c2 = 'c', c3 = 'c';

    n = sscanf(interval, "PT%d%c%d%c%d%c", &d1, &c1, &d2, &c2, &d3, &c3);
    if (n % 2 == 1)
        return -1;
    if ((n < 2) || (n > 6))
        return -1;

    ret = -2;

    switch (c1) {
        case 's':
        case 'S':
            ret = d1;
            break;
        case 'm':
        case 'M':
            ret = d1 * 60;
            break;
        case 'h':
        case 'H':
            ret = d1 * 3600;
            break;
        default:
            return ret;
    }
    switch (c2) {
        case 's':
        case 'S':
            ret += d2;
            break;
        case 'm':
        case 'M':
            ret += d2 * 60;
            break;
        case 'h':
        case 'H':
            ret += d2 * 3600;
            break;
        default:
            return ret;
    }
    switch (c3) {
        case 's':
        case 'S':
            ret += d3;
            break;
        case 'm':
        case 'M':
            ret += d3 * 60;
            break;
        case 'h':
        case 'H':
            ret += d3 * 3600;
            break;
        default:
            return ret;
    }

    return ret;
}

/**
 * Convert a time_t to a datetime in ISO format
 * @param timestamp Time in time_t
 * @param iso_date Output time in ISO format
 * @return 0 on success, negative on error
 */
int to_iso_date(char *iso_date, int size, time_t timestamp)
{
    struct tm my_tm;
    gmtime_r(&timestamp, &my_tm);

    if (size < 21) return -1;

    sprintf(iso_date, "%04d-%02d-%02dT%02d:%02d:%02dZ",
            my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
            my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec);

    return 0;
}

/**
 * Convert a datetime in ISO format to a time_t
 * @param iso_date Time in ISO format
 * @return Time in time_t
 */
time_t from_iso_date(const char *date)
{
    struct tm tt = {0};
    double seconds = 0;

    // Try date with seconds
    if (sscanf(date, "%04d-%02d-%02dT%02d:%02d:%lfZ",
               &tt.tm_year, &tt.tm_mon, &tt.tm_mday,
               &tt.tm_hour, &tt.tm_min, &seconds) != 6) {

        // Try date without seconds
        if (sscanf(date, "%04d-%02d-%02dT%02d:%02dZ",
                   &tt.tm_year, &tt.tm_mon, &tt.tm_mday,
                   &tt.tm_hour, &tt.tm_min) != 5) {

            return 0;
        }
    }

    tt.tm_sec   = (int) seconds;
    tt.tm_mon  -= 1;
    tt.tm_year -= 1900;
    tt.tm_isdst =-1;

    return timegm(&tt);
}

/**
 * Create a random UUID
 * @param g_uuid output to store the UUID
 * @return 0 on success, negative on error
 */
int gen_uuid(char *g_uuid)
{
    int i;
    char v[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    //3fb17ebc-bc38-4939-bc8b-74f2443281d4
    //8 dash 4 dash 4 dash 4 dash 12

    //gen random for all spaces because lazy
    for(i = 0; i < UUID_LEN; ++i) {
        g_uuid[i] = v[rand()%16];
    }


    //put dashes in place
    g_uuid[8] = '-';
    g_uuid[13] = '-';
    g_uuid[18] = '-';
    g_uuid[23] = '-';


    //put 4 in place
    g_uuid[14] = '4';


    //needs end byte
    g_uuid[36] = '\0';

    return 0;
}

/* -------------------------------------------------------------------------
 * SHA-1 (RFC 3174) helpers used only for UUID v5 generation.
 * NOT suitable for general-purpose cryptographic use.
 * ---------------------------------------------------------------------- */
static void hash_uuid5_store32(uint8_t *b, uint32_t v)
{
    b[0] = v >> 24; b[1] = v >> 16; b[2] = v >> 8; b[3] = v;
}

static void hash_uuid5_compress(uint32_t h[5], const uint8_t blk[64])
{
    uint32_t w[80], a, b, c, d, e, f, k, t;
    int i;

    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)blk[i*4] << 24) | ((uint32_t)blk[i*4+1] << 16) |
               ((uint32_t)blk[i*4+2] << 8) | blk[i*4+3];
    for (i = 16; i < 80; i++) {
        t = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
        w[i] = (t << 1) | (t >> 31);
    }

    a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4];
    for (i = 0; i < 80; i++) {
        if      (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999U; }
        else if (i < 40) { f = b ^ c ^ d;           k = 0x6ED9EBA1U; }
        else if (i < 60) { f = (b&c)|(b&d)|(c&d);  k = 0x8F1BBCDCU; }
        else             { f = b ^ c ^ d;            k = 0xCA62C1D6U; }
        t = ((a << 5) | (a >> 27)) + f + e + k + w[i];
        e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

static void hash_uuid5(const uint8_t *data, size_t len, uint8_t out[20])
{
    uint32_t h[5] = { 0x67452301U, 0xEFCDAB89U, 0x98BADCFEU,
                      0x10325476U, 0xC3D2E1F0U };
    uint8_t block[64];
    size_t i, r = len % 64;
    uint64_t bits;

    for (i = 0; i + 64 <= len; i += 64)
        hash_uuid5_compress(h, data + i);

    memset(block, 0, 64);
    if (r) memcpy(block, data + i, r);
    block[r] = 0x80;
    if (r >= 56) {
        hash_uuid5_compress(h, block);
        memset(block, 0, 64);
    }
    bits = (uint64_t)len * 8;
    hash_uuid5_store32(block + 56, (uint32_t)(bits >> 32));
    hash_uuid5_store32(block + 60, (uint32_t)bits);
    hash_uuid5_compress(h, block);

    for (i = 0; i < 5; i++)
        hash_uuid5_store32(out + i*4, h[i]);
}

/* RFC 4122 DNS namespace: 6ba7b810-9dad-11d1-80b4-00c04fd430c8 */
static const uint8_t UUID_NS_DNS[16] = {
    0x6b, 0xa7, 0xb8, 0x10,  0x9d, 0xad,  0x11, 0xd1,
    0x80, 0xb4,  0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8
};

int get_mac_by_ifname(const char *if_name, uint8_t mac_out[6])
{
    struct ifreq ifr;
    int fd, ret;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);
    ret = ioctl(fd, SIOCGIFHWADDR, &ifr);
    close(fd);
    if (ret < 0) return -1;
    memcpy(mac_out, ifr.ifr_hwaddr.sa_data, 6);
    return 0;
}

int get_mac_by_ip(const char *ip_str, uint8_t mac_out[6])
{
    struct ifaddrs *ifap, *ifa;
    char found[IFNAMSIZ] = {0};

    if (getifaddrs(&ifap) < 0) return -1;
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        char buf[16];
        inet_ntop(AF_INET, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr, buf, sizeof(buf));
        if (strcmp(buf, ip_str) == 0) { strncpy(found, ifa->ifa_name, IFNAMSIZ - 1); break; }
    }
    freeifaddrs(ifap);
    return (found[0] == '\0') ? -1 : get_mac_by_ifname(found, mac_out);
}

/* Find interface name owning addr_str (IPv4 or IPv6, scope ID stripped). */
int get_ifname_by_addr(const char *addr_str, char *ifname, size_t len)
{
    char clean[INET6_ADDRSTRLEN];
    strncpy(clean, addr_str, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    char *pct = strchr(clean, '%');
    const char *scope_if = pct ? pct + 1 : NULL;
    if (pct) *pct = '\0';

    if (scope_if && scope_if[0]) {
        strncpy(ifname, scope_if, len - 1);
        ifname[len - 1] = '\0';
        return 0;
    }

    int is_v6 = (strchr(clean, ':') != NULL);
    struct ifaddrs *ifap, *ifa;
    char found[IFNAMSIZ] = {0};

    if (getifaddrs(&ifap) < 0) return -1;
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        char buf[INET6_ADDRSTRLEN];
        if (!is_v6 && ifa->ifa_addr->sa_family == AF_INET) {
            inet_ntop(AF_INET, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr,
                      buf, sizeof(buf));
            if (strcmp(buf, clean) == 0) { strncpy(found, ifa->ifa_name, IFNAMSIZ - 1); break; }
        } else if (is_v6 && ifa->ifa_addr->sa_family == AF_INET6) {
            inet_ntop(AF_INET6, &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr,
                      buf, sizeof(buf));
            if (strcmp(buf, clean) == 0) { strncpy(found, ifa->ifa_name, IFNAMSIZ - 1); break; }
        }
    }
    freeifaddrs(ifap);
    if (!found[0]) return -1;
    strncpy(ifname, found, len - 1);
    ifname[len - 1] = '\0';
    return 0;
}

/* MAC lookup for IPv4 or IPv6 address (scope ID handled). */
int get_mac_by_addr(const char *addr_str, uint8_t mac_out[6])
{
    char ifname[IFNAMSIZ];
    if (get_ifname_by_addr(addr_str, ifname, sizeof(ifname)) != 0) return -1;
    return get_mac_by_ifname(ifname, mac_out);
}

/* -------------------------------------------------------------------------
 * Auto-detect the local address to use in ONVIF service URLs.
 *
 * Priority:
 *   1. REMOTE_ADDR is loopback (local proxy) -> WSD multicast connect trick
 *   2. REMOTE_ADDR is a real client         -> connect trick to REMOTE_ADDR
 *
 * Sets local_addr to a plain IP string (no scope ID -- safe for URLs).
 * ---------------------------------------------------------------------- */
#define WSD_MCAST_V4  "239.255.255.250"
#define WSD_MCAST_V6  "ff02::c"
#define WSD_PROBE_PORT 3702

static int connect_trick_v4(const char *dst_ip, char *local, size_t len)
{
    struct sockaddr_in dst = {0};
    dst.sin_family = AF_INET;
    inet_pton(AF_INET, dst_ip, &dst.sin_addr);
    dst.sin_port = htons(WSD_PROBE_PORT);

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    if (connect(fd, (struct sockaddr *)&dst, sizeof(dst)) < 0) { close(fd); return -1; }
    struct sockaddr_in src = {0};
    socklen_t src_len = sizeof(src);
    getsockname(fd, (struct sockaddr *)&src, &src_len);
    close(fd);
    if (src.sin_addr.s_addr == htonl(INADDR_ANY)) return -1;
    inet_ntop(AF_INET, &src.sin_addr, local, len);
    return 0;
}

static int connect_trick_v6(const char *dst_ip, unsigned int scope_id,
                             char *local, size_t len)
{
    struct sockaddr_in6 dst = {0};
    dst.sin6_family = AF_INET6;
    inet_pton(AF_INET6, dst_ip, &dst.sin6_addr);
    dst.sin6_port    = htons(WSD_PROBE_PORT);
    dst.sin6_scope_id = scope_id;

    int fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    if (connect(fd, (struct sockaddr *)&dst, sizeof(dst)) < 0) { close(fd); return -1; }
    struct sockaddr_in6 src = {0};
    socklen_t src_len = sizeof(src);
    getsockname(fd, (struct sockaddr *)&src, &src_len);
    close(fd);
    if (IN6_IS_ADDR_UNSPECIFIED(&src.sin6_addr)) return -1;
    inet_ntop(AF_INET6, &src.sin6_addr, local, len);
    return 0;
}

int detect_local_address(const char *remote_addr, char *local_addr, size_t len)
{
    /* Strip IPv6 scope ID; remember interface for scoped connect */
    char clean[INET6_ADDRSTRLEN];
    strncpy(clean, remote_addr, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    char *pct = strchr(clean, '%');
    unsigned int scope_id = 0;
    if (pct) {
        scope_id = if_nametoindex(pct + 1);
        *pct = '\0';
    }

    int is_v6      = (strchr(clean, ':') != NULL);
    int is_loopback = is_v6 ? (strcmp(clean, "::1") == 0)
                             : (strncmp(clean, "127.", 4) == 0);

    if (is_loopback) {
        /* Local proxy in front: use WSD multicast to find real outbound IF */
        if (connect_trick_v4(WSD_MCAST_V4, local_addr, len) == 0) return 0;
        if (connect_trick_v6(WSD_MCAST_V6, 0, local_addr, len) == 0) return 0;
        return -1;
    }

    /* Real client: ask kernel which source address it would use */
    if (is_v6)
        return connect_trick_v6(clean, scope_id, local_addr, len);
    else
        return connect_trick_v4(clean, local_addr, len);
}

void gen_uuid_v5_mac(char *uuid_str, const uint8_t mac[6])
{
    char name[32];
    uint8_t input[40];   /* 16-byte NS + up to 23-byte name */
    uint8_t hash[20];
    size_t name_len;

    snprintf(name, sizeof(name), "mac:%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    name_len = strlen(name);

    memcpy(input, UUID_NS_DNS, 16);
    memcpy(input + 16, name, name_len);
    hash_uuid5(input, 16 + name_len, hash);

    hash[6] = (hash[6] & 0x0F) | 0x50;   /* version = 5 */
    hash[8] = (hash[8] & 0x3F) | 0x80;   /* variant = RFC 4122 */

    snprintf(uuid_str, UUID_LEN + 1,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x"
             "-%02x%02x%02x%02x%02x%02x",
             hash[0],  hash[1],  hash[2],  hash[3],
             hash[4],  hash[5],  hash[6],  hash[7],
             hash[8],  hash[9],
             hash[10], hash[11], hash[12], hash[13], hash[14], hash[15]);
}

/**
 * Get the value of a parameter in the query string
 * @param par The name of the parameter
 * @param ret_size the size of the destination string
 * @param ret The output string
 * @return 0 on success, negative on error
 */
int get_from_query_string(char **ret, int *ret_size, char *par)
{
    char *query_string = getenv("QUERY_STRING");
    char *query;
    char *tokens;
    char *p;

    *ret = NULL;
    *ret_size = -1;

    if (query_string == NULL)
        return -1;

    query = strdup(query_string);
    tokens = query;
    p = query;

    while ((p = strsep (&tokens, "&\n"))) {
        char *var = strtok (p, "=");
        char *val = NULL;

        if (var && (val = strtok (NULL, "="))) {
            if (strcasecmp(par, var) == 0) {
                *ret = query_string + (val - query);
                *ret_size = strlen(val);
                return 0;
            }
        }
    }
    free (query);

    return -1;
}

/**
 * Convert a video codec from number to string
 * @param ver The version of media service
 * @param codec The number of the codec
 * @param buffer_len The size of the destination buffer
 * @param buffer The output buffer
 * @return 0 on success, negative on error
 */
int set_video_codec(char *buffer, int buffer_len, int codec, int ver)
{
    if (buffer_len < 16) return -1;

    if (codec == JPEG) {
        sprintf(buffer, "JPEG");
    } else if (codec == MPEG4) {
        sprintf(buffer, "MPEG4");
    } else if (codec == H264) {
        sprintf(buffer, "H264");
    } else if (codec == H265) {
        if (ver == 1) {
            sprintf(buffer, "H264");
        } else {
            sprintf(buffer, "H265");
        }
    } else {
        return -2;
    }

    return 0;
}

/**
 * Convert an audio codec from number to string
 * @param ver The version of media service
 * @param codec The number of the codec
 * @param buffer_len The size of the destination buffer
 * @param buffer The output buffer
 * @return 0 on success, negative on error
 */
int set_audio_codec(char *buffer, int buffer_len, int codec, int ver)
{
    if (buffer_len < 16) return -1;

    if (codec == G711) {
        if (ver == 1) {
            sprintf(buffer, "G711");
        } else {
            sprintf(buffer, "PCMU");
        }
    } else if (codec == G726) {
        sprintf(buffer, "G726");
    } else if (codec == AAC) {
        if (ver == 1) {
            sprintf(buffer, "AAC");
        } else {
            sprintf(buffer, "MPEG4-GENERIC");
        }
    } else {
        return -2;
    }

    return 0;
}

/**
 * Convert a TopicExpression string to a struct
 * @param input The string containing the expression
 * @return The pointer to a new allocated struct: remember to free it
 */
topic_expressions_t *parse_topic_expression(const char *input)
{
    char input_copy[MAX_LEN];
    char *str;
    int number = 0;
    int len;
    int error = 0;
    topic_expressions_t *out = (topic_expressions_t *) malloc(sizeof(topic_expressions_t));
    out->topics = NULL;

    if (strlen(input) > MAX_LEN - 1) {
        free(out);
        return NULL;
    }
    if (strlen(input) <= 3) {
        free(out);
        return NULL;
    }

    strcpy(input_copy, input);

    char *token = strtok(input_copy, "|");
    while (token != NULL) {
        number++;
        len = strlen(token);
        if (len <= 3) {
            error = 1;
            break;
        }

        topic_expression_t* temp = (topic_expression_t*) realloc(out->topics, number * sizeof(topic_expression_t));
        if (temp == NULL) {
            error = 1;
            break;
        }
        out->topics = temp;

        out->topics[number - 1].topic = malloc((len + 1) * sizeof(char));
        if (out->topics[number - 1].topic == NULL) {
            error = 1;
            break;
        }
        out->topics[number - 1].match_sub_tree = 0;
        str = out->topics[number - 1].topic;
        strcpy(str, token);

        if (str[len - 3] == '/' && str[len - 2] == '/' && str[len - 1] == '.') {
            str[len - 3] = '\0';
            out->topics[number - 1].match_sub_tree = 1;
        }

        token = strtok(NULL, "|");
    }

    if (error) {
        // Clean up any allocated memory before returning
        if (out->topics != NULL) {
            for (int j = 0; j < number - 1; j++) {
                if (out->topics[j].topic != NULL) {
                    free(out->topics[j].topic);
                }
            }
            free(out->topics);
        }
        free(out);
        return NULL;
    }
    out->number = number;

    return out;
}

/**
 * Free the struct topic_expressions_t
 * @param p The pointer to the struct
 */
void free_topic_expression(topic_expressions_t* p)
{
    int i;

    if (p == NULL) {
        return;
    }

    if (p->topics != NULL) {
        for (i = p->number - 1; i >= 0; i--) {
            if (p->topics[i].topic != NULL) {
                free(p->topics[i].topic);
            }
        }
        free(p->topics);
    }

    free(p);
}

/**
 * Check if a TopicExpression contains a topic
 * @param topic The topic to find
 * @param topic_expression The TopicExpression string
 * @return 1 on success or if topic_expression is empty, 1 if not found
 */
int is_topic_in_expression(const char *topic_expression, char *topic)
{
    int i;
    topic_expressions_t *te;

    if ((topic_expression == NULL) || (topic_expression[0] == '\0')) {
        return 1;
    }

    if (topic == NULL) {
        return 0;
    }

    te = parse_topic_expression(topic_expression);

    if (te == NULL) return 0;

    for (i = 0; i < te->number; i++) {
        if (te->topics[i].topic == NULL) {
            continue;
        }
        if (te->topics[i].match_sub_tree) {
            if (strncmp(te->topics[i].topic, topic, strlen(te->topics[i].topic)) == 0) {
                free_topic_expression(te);
                return 1;
            }
        } else {
            if (strcmp(te->topics[i].topic, topic) == 0) {
                free_topic_expression(te);
                return 1;
            }
        }
    }
    free_topic_expression(te); // Fix memory leak
    return 0;
}

/**
 * Thread function to run a reboot
 * @param arg Not used
 * @return NULL
 */
void *reboot_thread(void *arg)
{
    sync();
    setuid(0);
    sync();
    sleep(3);
    sync();
    reboot(RB_AUTOBOOT);

    return NULL;
}
