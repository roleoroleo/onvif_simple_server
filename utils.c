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

#include "utils.h"
#include "onvif_simple_server.h"
#include "log.h"

#define SHMOBJ_PATH "/onvif_subscription"
#define MEM_LOCK_FILE "/sub_mem_lock"

sem_t *sem_memory_lock = SEM_FAILED;

int sem_memory_open()
{
    sem_memory_lock = sem_open(MEM_LOCK_FILE, O_CREAT, S_IRUSR | S_IWUSR, 1);
    if (sem_memory_lock == SEM_FAILED) {
        fprintf(stderr, "Error opening semaphore file %s\n", MEM_LOCK_FILE);
        return -1;
    }
    return 0;
}

void sem_memory_close()

{
    if (sem_memory_lock != SEM_FAILED) {
        sem_close(sem_memory_lock);
        sem_memory_lock = SEM_FAILED;
        sem_unlink(MEM_LOCK_FILE);
    }
    return;
}

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

void destroy_shared_memory(void *shared_area, int destroy_all)
{
    int shared_seg_size = sizeof(shm_t);

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
    return sem_wait(sem_memory_lock);
}

int sem_memory_post()
{
    return sem_post(sem_memory_lock);
}

/**
 * Read a file line by line and send to output after replacing arguments
 * @param out The output type: "sdout", char *ptr or NULL
 * @param filename The input file to process
 * @param num The number of variable arguments
 * @param ... The argument list to replace: src1, dst1, src2, dst2, etc...
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

    FILE* file = fopen(filename, "r");
    if (!file) {
        log_error("Unable to open file %s\n", filename);
        return -1;
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
                strncpy(new_line, line, pars - line);
                strcpy(&new_line[pars - line], par_to_sub);
                strncpy(&new_line[pars + strlen(par_to_sub) - line], pare, line + strlen(line) - pare);
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
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr), address, 15);
            san = (struct sockaddr_in *) ifa->ifa_netmask;
            inet_ntop(AF_INET, &(((struct sockaddr_in *)san)->sin_addr), netmask, 15);

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
    while((*back == ' ') || (*back == '\t') || (*back == '\n') || (*back == '\r')) {
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
    int s_tmp[max_len];
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
    while (*f != '\0')
    {
        switch (*f) {
        case '\"':
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
            break;
        case '&':
            *t = '&';
            t++;
            *t = 'a';
            t++;
            *t = 'm';
            t++;
            *t = 'p';
            t++;
            *t = ';';
            break;
        case '\'':
            *t = '&';
            t++;
            *t = '#';
            t++;
            *t = '3';
            t++;
            *t = '9';
            t++;
            *t = ';';
            break;
        case '<':
            *t = '&';
            t++;
            *t = 'l';
            t++;
            *t = 't';
            t++;
            *t = ';';
            break;
        case '>':
            *t = '&';
            t++;
            *t = 'g';
            t++;
            *t = 't';
            t++;
            *t = ';';
            break;
        default:
            *t = *f;
        }
        t++;
        f++;
    }

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
