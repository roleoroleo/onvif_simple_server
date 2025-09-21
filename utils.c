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
    if (sem_memory_lock == SEM_FAILED) {
        log_error("Semaphore not initialized");
        return -1;
    }
    return sem_wait(sem_memory_lock);
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
        return -4;
    }

    log_debug("MAC address: <%s>", address);

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
    char *query = strdup(query_string);
    char *tokens = query;
    char *p = query;

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
