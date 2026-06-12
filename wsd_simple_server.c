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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <time.h>
#include <signal.h>
#include <getopt.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>

#include "utils.h"
#include "log.h"
#include "ezxml_wrapper.h"

#define MULTICAST_ADDRESS "239.255.255.250"
#define PORT 3702
#define TYPE "NetworkVideoTransmitter"

#define DEFAULT_LOG_FILE "/var/log/wsd_simple_server.log"
#define DEFAULT_TEMPLATE_DIR "/etc/wsd_simple_server"

#define RECV_BUFFER_LEN 4096

#define BD_NO_CHDIR          01
#define BD_NO_CLOSE_FILES    02
#define BD_NO_REOPEN_STD_FDS 04

#define BD_NO_UMASK0        010
#define BD_MAX_CLOSE       8192

#define PID_SIZE 32

// Global variables
int debug;
FILE *fLog;
char template_file[1024];
char address[16], netmask[16];
char *message;
char *message_loop;
int sock;
struct sockaddr_in addr_in;
int addr_len;
char xaddr[1024];
char template_dir[1024];
int exit_main;
int msg_number;
char uuid[UUID_LEN + 1];
char msg_uuid[UUID_LEN + 1];

char model[64];
char hardware[64];

/*
 * Auto-detect the outbound IPv4 address for WS-Discovery by querying the
 * kernel's routing table: connect a UDP socket (no packet is sent) to the
 * WS-Discovery multicast group and read back the local address the kernel
 * would select.  Falls back to -i / --if_name for multi-homed systems.
 * Returns 0 on success and writes a dotted-decimal string into addr_str
 * (caller must provide at least 16 bytes).  Returns -1 on failure.
 */
static int detect_outbound_address(char *addr_str)
{
    int fd;
    struct sockaddr_in dst, src;
    socklen_t src_len = sizeof(src);

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return -1;

    memset(&dst, 0, sizeof(dst));
    dst.sin_family      = AF_INET;
    dst.sin_addr.s_addr = inet_addr(MULTICAST_ADDRESS);
    dst.sin_port        = htons(PORT);

    if (connect(fd, (struct sockaddr *)&dst, sizeof(dst)) < 0) {
        close(fd);
        return -1;
    }

    memset(&src, 0, sizeof(src));
    if (getsockname(fd, (struct sockaddr *)&src, &src_len) < 0) {
        close(fd);
        return -1;
    }
    close(fd);

    if (src.sin_addr.s_addr == htonl(INADDR_ANY))
        return -1;

    strncpy(addr_str, inet_ntoa(src.sin_addr), 15);
    addr_str[15] = 0;
    return 0;
}

/* -------------------------------------------------------------------------
 * Compact SHA-1 (RFC 3174 / FIPS 180-4) -- used only for UUID v5 generation.
 * No external dependencies.
 * ---------------------------------------------------------------------- */

static void sha1_store32(uint8_t *b, uint32_t v)
{
    b[0] = v >> 24; b[1] = v >> 16; b[2] = v >> 8; b[3] = v;
}

static void sha1_compress(uint32_t h[5], const uint8_t blk[64])
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

static void sha1(const uint8_t *data, size_t len, uint8_t out[20])
{
    uint32_t h[5] = { 0x67452301U, 0xEFCDAB89U, 0x98BADCFEU,
                      0x10325476U, 0xC3D2E1F0U };
    uint8_t block[64];
    size_t i, r = len % 64;
    uint64_t bits;

    for (i = 0; i + 64 <= len; i += 64)
        sha1_compress(h, data + i);

    memset(block, 0, 64);
    if (r) memcpy(block, data + i, r);
    block[r] = 0x80;
    if (r >= 56) {
        sha1_compress(h, block);
        memset(block, 0, 64);
    }
    bits = (uint64_t)len * 8;
    sha1_store32(block + 56, (uint32_t)(bits >> 32));
    sha1_store32(block + 60, (uint32_t)bits);
    sha1_compress(h, block);

    for (i = 0; i < 5; i++)
        sha1_store32(out + i*4, h[i]);
}

/* -------------------------------------------------------------------------
 * UUID v5 (SHA-1 name-based, RFC 4122).
 *
 * Generates a stable, deterministic UUID from the device's MAC address.
 * The DNS namespace UUID is used as per RFC 4122 Appendix C.
 * Satisfies the WS-Discovery 1.1 requirement:
 *   "The endpoint reference MUST be stable across restarts of the Target Service."
 * ---------------------------------------------------------------------- */

/* RFC 4122 DNS namespace: 6ba7b810-9dad-11d1-80b4-00c04fd430c8 */
static const uint8_t UUID_NS_DNS[16] = {
    0x6b, 0xa7, 0xb8, 0x10,  0x9d, 0xad,  0x11, 0xd1,
    0x80, 0xb4,  0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8
};

/*
 * Build a UUID v5 from a MAC address and write it into uuid_str.
 * The name fed to the hash is "mac:xx:xx:xx:xx:xx:xx" (lower-case hex).
 * uuid_str must hold at least UUID_LEN + 1 bytes.
 */
static void gen_uuid_v5_mac(char *uuid_str, const uint8_t mac[6])
{
    char name[32];
    uint8_t input[16 + 24];  /* namespace (16) + name (max 23 + NUL) */
    uint8_t hash[20];
    size_t name_len;

    snprintf(name, sizeof(name), "mac:%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    name_len = strlen(name);

    memcpy(input, UUID_NS_DNS, 16);
    memcpy(input + 16, name, name_len);

    sha1(input, 16 + name_len, hash);

    /* Set version 5 and RFC 4122 variant bits */
    hash[6] = (hash[6] & 0x0F) | 0x50;   /* version = 5 */
    hash[8] = (hash[8] & 0x3F) | 0x80;   /* variant = 10xx */

    snprintf(uuid_str, UUID_LEN + 1,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x"
             "-%02x%02x%02x%02x%02x%02x",
             hash[0],  hash[1],  hash[2],  hash[3],
             hash[4],  hash[5],
             hash[6],  hash[7],
             hash[8],  hash[9],
             hash[10], hash[11], hash[12], hash[13], hash[14], hash[15]);
}

/*
 * Look up the MAC address for a named network interface.
 * Returns 0 on success, -1 on failure.
 */
static int get_mac_by_ifname(const char *if_name, uint8_t mac_out[6])
{
    struct ifreq ifr;
    int fd, ret;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return -1;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);
    ret = ioctl(fd, SIOCGIFHWADDR, &ifr);
    close(fd);

    if (ret < 0)
        return -1;

    memcpy(mac_out, ifr.ifr_hwaddr.sa_data, 6);
    return 0;
}

/*
 * Find which interface owns ip_str (dotted-decimal) and return its MAC.
 * Used in auto-detect mode when no interface name is available.
 * Returns 0 on success, -1 on failure.
 */
static int get_mac_by_ip(const char *ip_str, uint8_t mac_out[6])
{
    struct ifaddrs *ifap, *ifa;
    char found[IFNAMSIZ] = {0};

    if (getifaddrs(&ifap) < 0)
        return -1;

    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
            continue;
        char buf[16];
        struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
        inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
        if (strcmp(buf, ip_str) == 0) {
            strncpy(found, ifa->ifa_name, IFNAMSIZ - 1);
            break;
        }
    }
    freeifaddrs(ifap);

    if (found[0] == '\0')
        return -1;

    return get_mac_by_ifname(found, mac_out);
}

int daemonize(int flags)
{
    int maxfd, fd;

    switch(fork()) {
        case -1: return -1;
        case 0: break;
        default: _exit(EXIT_SUCCESS);
    }

    if(setsid() == -1)
        return -1;

    switch(fork()) {
        case -1: return -1;
        case 0: break;
        default: _exit(EXIT_SUCCESS);
    }

    if(!(flags & BD_NO_UMASK0))
        umask(0);

    if(!(flags & BD_NO_CHDIR))
        chdir("/");

    if(!(flags & BD_NO_CLOSE_FILES)) {
        maxfd = sysconf(_SC_OPEN_MAX);
        if(maxfd == -1)
            maxfd = BD_MAX_CLOSE;
        for(fd = 0; fd < maxfd; fd++)
            close(fd);
    }

    if(!(flags & BD_NO_REOPEN_STD_FDS)) {
        close(STDIN_FILENO);

        fd = open("/dev/null", O_RDWR);
        if(fd != STDIN_FILENO)
            return -1;
        if(dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
            return -2;
        if(dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
            return -3;
    }

    return 0;
}

int check_pid(char *file_name)
{
    FILE *f;
    long pid;
    char pid_buffer[PID_SIZE];

    f = fopen(file_name, "r");
    if(f == NULL)
        return 0;

    if (fgets(pid_buffer, PID_SIZE, f) == NULL) {
        fclose(f);
        return 0;
    }
    fclose(f);

    if (sscanf(pid_buffer, "%ld", &pid) != 1) {
        return 0;
    }

    if (kill(pid, 0) == 0) {
        return 1;
    }

    return 0;
}

int create_pid(char *file_name)
{
    FILE *f;
    char pid_buffer[PID_SIZE];

    f = fopen(file_name, "w");
    if (f == NULL)
        return -1;

    memset(pid_buffer, '\0', PID_SIZE);
    sprintf(pid_buffer, "%ld\n", (long) getpid());
    if (fwrite(pid_buffer, strlen(pid_buffer), 1, f) != 1) {
        fclose(f);
        return -2;
    }
    fclose(f);

    return 0;
}

void signal_handler(int signal)
{
    char s_tmp[32];
    int size;

    // Prepare Bye message
    msg_number++;
    sprintf(s_tmp, "%d", msg_number);
    gen_uuid(msg_uuid);

    // Send Bye message
    log_info("Sending Bye message.");
    sprintf(template_file, "%s/Bye.xml", template_dir);
    size = cat(NULL, template_file, 12,
            "%MSG_UUID%", msg_uuid,
            "%MSG_NUMBER%", s_tmp,
            "%UUID%", uuid,
            "%HARDWARE%", hardware,
            "%NAME%", model,
            "%ADDRESS%", xaddr);

    message = (char *) malloc((size + 1) * sizeof(char));
    if (message == NULL) {
        log_fatal("Malloc error.");
        shutdown(sock, SHUT_RDWR);
        close(sock);
        // Exit from main loop
        exit_main = 1;
    }

    cat(message, template_file, 12,
            "%MSG_UUID%", msg_uuid,
            "%MSG_NUMBER%", s_tmp,
            "%UUID%", uuid,
            "%HARDWARE%", hardware,
            "%NAME%", model,
            "%ADDRESS%", xaddr);

    if (sendto(sock, message, strlen(message), 0, (struct sockaddr *) &addr_in, sizeof(addr_in)) < 0) {
        log_fatal("Error sending Bye message.");
        free(message);
        shutdown(sock, SHUT_RDWR);
        close(sock);
        // Exit from main loop
        exit_main = 1;
        exit(EXIT_FAILURE);
    }
    log_info("Sent.");
    free(message);

    // Exit from main loop
    shutdown(sock, SHUT_RDWR);
    close(sock);
    exit_main = 1;
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [-i INTERFACE] -x XADDR [-m MODEL] [-n MANUFACTURER] -p PID_FILE [-t TEMPLATE_DIR] [-f] [-d LEVEL]\n\n", progname);
    fprintf(stderr, "\t-i, --if_name\n");
    fprintf(stderr, "\t\tnetwork interface (auto-detected; use -i to force a specific interface)\n");
    fprintf(stderr, "\t-x, --xaddr\n");
    fprintf(stderr, "\t\tresource address\n");
    fprintf(stderr, "\t-m, --model\n");
    fprintf(stderr, "\t\tmodel name\n");
    fprintf(stderr, "\t-n, --hardware\n");
    fprintf(stderr, "\t\thardware manufacturer\n");
    fprintf(stderr, "\t-p, --pid_file\n");
    fprintf(stderr, "\t\tpid file\n");
    fprintf(stderr, "\t-t, --template_dir\n");
    fprintf(stderr, "\t\ttemplate directory (default: %s)\n", DEFAULT_TEMPLATE_DIR);
    fprintf(stderr, "\t-f, --foreground\n");
    fprintf(stderr, "\t\tdon't daemonize\n");
    fprintf(stderr, "\t-d LEVEL, --debug LEVEL\n");
    fprintf(stderr, "\t\tenable debug with LEVEL = 0..5 (default 0 = log fatal errors)\n");
    fprintf(stderr, "\t-h, --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

int main(int argc, char **argv)  {

    int errno;
    char *endptr;
    int c, ret;
    char *if_name;
    char *pid_file;
    char *xaddr_s;
    int foreground;
    char s_tmp[32];
    const char *method;

    struct ip_mreq mr;
    long size;
    char recv_buffer[RECV_BUFFER_LEN];
    int recv_len;
    char *recv_copy;

    if_name = NULL;
    pid_file = NULL;
    xaddr_s = NULL;
    strcpy(model, "MODEL_NAME");
    strcpy(hardware, "HARDWARE_MANUFACTURER");
    strcpy(template_dir, DEFAULT_TEMPLATE_DIR);
    foreground = 0;
    debug = 5;

    while (1) {
        static struct option long_options[] =
        {
            {"if_name",  required_argument, 0, 'i'},
            {"pid_file",  required_argument, 0, 'p'},
            {"xaddr",  required_argument, 0, 'x'},
            {"model",  required_argument, 0, 'm'},
            {"hardware",  required_argument, 0, 'n'},
            {"template_dir",  required_argument, 0, 't'},
            {"foreground",  no_argument, 0, 'f'},
            {"debug",  required_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "i:p:x:m:n:t:fd:h",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 'i':
            if_name = optarg;
            break;

        case 'x':
            xaddr_s = optarg;
            break;

        case 'm':
            if (strlen(optarg) < sizeof(model)) {
                strcpy(model, optarg);
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'n':
            if (strlen(optarg) < sizeof(hardware)) {
                strcpy(hardware, optarg);
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'p':
            pid_file = optarg;
            break;

        case 't':
            if (strlen(optarg) < sizeof(template_dir)) {
                strcpy(template_dir, optarg);
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'f':
            foreground = 1;
            break;

        case 'd':
            debug = strtol(optarg, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (debug == LONG_MAX || debug == LONG_MIN)) || (errno != 0 && debug == 0)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (endptr == optarg) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }

            if ((debug < LOG_TRACE) || (debug > LOG_FATAL)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            debug = 5 - debug;
            break;

        case 'h':
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
            break;

        case '?':
            /* getopt_long already printed an error message. */
            break;

        default:
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        }
    }

    if (xaddr_s == NULL) {
        print_usage(argv[0]);
        exit(EXIT_SUCCESS);
    }

    if (pid_file == NULL) {
        print_usage(argv[0]);
        exit(EXIT_SUCCESS);
    }

    // Set signal handler
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    if (foreground == 0) {
        ret = daemonize(0);
        if (ret) {
            fprintf(stderr, "Error starting daemon.\n");
            exit(EXIT_FAILURE);
        }
    }

    // Open file log
    fLog = fopen(DEFAULT_LOG_FILE, "w");
    log_add_fp(fLog, debug);
    log_set_level(debug);
    log_set_quiet(1);
    log_info("Starting program.");

    log_debug("Argument if_name = %s", if_name ? if_name : "(auto-detect)");
    log_debug("Argument pid_file = %s", pid_file);
    log_debug("Argument xaddr = %s", xaddr_s);

    // Checking pid file
    if (check_pid(pid_file) == 1) {
        log_fatal("Program is already running.");
        fclose(fLog);
        exit(EXIT_FAILURE);
    }
    if (create_pid(pid_file) < 0) {
        log_fatal("Error creating pid file %s", pid_file);
        fclose(fLog);
        exit(EXIT_FAILURE);
    }

    // Check if template_dir exists
    if (access(template_dir, F_OK ) != -1) {
        // file exists
        DIR *dirptr;
        if ((dirptr = opendir(template_dir)) != NULL) {
            closedir (dirptr);
        } else {
            log_fatal("Unable to open directory %s", template_dir);
            fclose(fLog);
            exit(EXIT_FAILURE);
        }
    } else {
        log_fatal("Unable to open directory %s", template_dir);
        fclose(fLog);
        exit(EXIT_FAILURE);
    }

    srand((unsigned int) time(NULL));

    // Configure socket
    memset(&addr_in, '\0', sizeof(addr_in));
    addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(PORT);

    if (if_name != NULL) {
        get_ip_address(address, netmask, if_name);
    } else {
        if (detect_outbound_address(address) != 0) {
            log_fatal("Cannot auto-detect network interface. Use -i <interface> to specify one.");
            close(sock);
            fclose(fLog);
            exit(EXIT_FAILURE);
        }
        log_info("Auto-detected outbound address: %s", address);
    }
    log_debug("Address = %s", address);

    // Generate stable UUID from MAC address (WS-Discovery 1.1: EPR MUST be stable across restarts)
    {
        uint8_t mac[6] = {0};
        int mac_ok;

        if (if_name != NULL)
            mac_ok = (get_mac_by_ifname(if_name, mac) == 0);
        else
            mac_ok = (get_mac_by_ip(address, mac) == 0);

        if (mac_ok) {
            gen_uuid_v5_mac(uuid, mac);
            log_info("Device UUID (stable, MAC-based): %s", uuid);
        } else {
            gen_uuid(uuid);
            log_info("Device UUID (random, MAC unavailable): %s", uuid);
        }
    }

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        log_fatal("Unable to create socket.");
        fclose(fLog);
        exit(EXIT_FAILURE);
    }

    if (bind(sock, (struct sockaddr *) &addr_in, sizeof(addr_in)) == -1) {
        log_fatal("Unable to bind socket");
        shutdown(sock, SHUT_RDWR);
        close(sock);
        fclose(fLog);
        exit(EXIT_FAILURE);
    }

    mr.imr_multiaddr.s_addr = inet_addr(MULTICAST_ADDRESS);
    mr.imr_interface.s_addr = inet_addr(address);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mr, sizeof(mr)) == -1) {
        log_fatal("Error joining multicast group");
        shutdown(sock, SHUT_RDWR);
        close(sock);
        fclose(fLog);
        exit(EXIT_FAILURE);
    }

    // Prepare Hello message
    msg_number = 1;
    sprintf(s_tmp, "%d", msg_number);
    gen_uuid(msg_uuid);

    sprintf(xaddr, xaddr_s, address);

    // Send Hello message
    log_info("Sending Hello message.");
    sprintf(template_file, "%s/Hello.xml", template_dir);
    size = cat(NULL, template_file, 12,
            "%MSG_UUID%", msg_uuid,
            "%MSG_NUMBER%", s_tmp,
            "%UUID%", uuid,
            "%HARDWARE%", hardware,
            "%NAME%", model,
            "%ADDRESS%", xaddr);

    message = (char *) malloc((size + 1) * sizeof(char));
    if (message == NULL) {
        log_fatal("Malloc error.");
        shutdown(sock, SHUT_RDWR);
        close(sock);
        fclose(fLog);
        exit(EXIT_FAILURE);
    }

    cat(message, template_file, 12,
            "%MSG_UUID%", msg_uuid,
            "%MSG_NUMBER%", s_tmp,
            "%UUID%", uuid,
            "%HARDWARE%", hardware,
            "%NAME%", model,
            "%ADDRESS%", xaddr);

    addr_in.sin_addr.s_addr = inet_addr(MULTICAST_ADDRESS);
    if (sendto(sock, message, strlen(message), 0, (struct sockaddr *) &addr_in, sizeof(addr_in)) < 0) {
        log_fatal("Error sending Hello message.");
        free(message);
        shutdown(sock, SHUT_RDWR);
        close(sock);
        fclose(fLog);
        exit(EXIT_FAILURE);
    }
    free(message);
    log_info("Sent.");

    sleep(1);

    // Enter main loop
    log_info("Starting main loop");
    exit_main = 0;
    while (!exit_main) {

        // Read from socket
        memset(recv_buffer, '\0', RECV_BUFFER_LEN);
        addr_len = sizeof(addr_in);  // init address

        if (recvfrom(sock, recv_buffer, RECV_BUFFER_LEN, 0, (struct sockaddr *) &addr_in, &addr_len) > 0) {

            // Check if the message is a response
            if (strstr(recv_buffer, "XAddrs") == NULL) {

                const char *relates_to_uuid;

                log_debug("%s", recv_buffer);

                // Check if it's a Probe message
                init_xml(recv_buffer, strlen(recv_buffer));
                method = get_method(1);
                if ((method == NULL) || (strcasecmp("Probe", method) != 0)) {
                    log_debug("This is not a Probe message");
                    close_xml();
                    continue;
                }
                log_debug("Probe message");

                // Prepare ProbeMatches message
                msg_number++;
                sprintf(s_tmp, "%d", msg_number);
                gen_uuid(msg_uuid);
                relates_to_uuid = NULL;

                relates_to_uuid = get_element("MessageID", "Header");
                if (relates_to_uuid == NULL) {
                    log_error("Cannot find MessageID.");
                    continue;
                }
                close_xml();

                // Send ProbeMatches message
                log_info("Sending ProbeMatches message.");
                sprintf(template_file, "%s/ProbeMatches.xml", template_dir);
                size = cat(NULL, template_file, 14,
                        "%MSG_UUID%", msg_uuid,
                        "%REL_TO_UUID%", relates_to_uuid,
                        "%MSG_NUMBER%", s_tmp,
                        "%UUID%", uuid,
                        "%HARDWARE%", hardware,
                        "%NAME%", model,
                        "%ADDRESS%", xaddr);

                message_loop = (char *) malloc((size + 1) * sizeof(char));
                if (message_loop == NULL) {
                    log_error("Malloc error.");
                    continue;
                }

                cat(message_loop, template_file, 14,
                        "%MSG_UUID%", msg_uuid,
                        "%REL_TO_UUID%", relates_to_uuid,
                        "%MSG_NUMBER%", s_tmp,
                        "%UUID%", uuid,
                        "%HARDWARE%", hardware,
                        "%NAME%", model,
                        "%ADDRESS%", xaddr);

                if (sendto(sock, message_loop, strlen(message_loop), 0, (struct sockaddr *) &addr_in, sizeof(addr_in)) < 0) {
                    log_error("Error sending ProbeMatches message.");
                    free(message_loop);
                    continue;
                }
                free(message_loop);
                log_info("Sent.");
            }
        }
    }

    shutdown(sock, SHUT_RDWR);
    close(sock);

    log_info("Terminating program.");
    fclose(fLog);

    unlink(pid_file);

    return 0;
}
