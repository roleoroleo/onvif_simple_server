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
#include <sys/select.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
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

#define MULTICAST_ADDRESS    "239.255.255.250"
#define MULTICAST_ADDRESS_V6 "FF02::C"
#define PORT 3702
#define TYPE "NetworkVideoTransmitter"

#define DEFAULT_LOG_FILE     "/var/log/wsd_simple_server.log"
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
/* Single dual-stack socket (AF_INET6, IPV6_V6ONLY=0) */
int sock;
/* IPv4 */
char address[16], netmask[16];
struct sockaddr_in6 addr_mcast4; /* ::ffff:239.255.255.250:3702 */
char xaddr[1024];
/* IPv6 (optional, only when -6 is given) */
char address6[INET6_ADDRSTRLEN];
struct sockaddr_in6 addr_mcast6; /* FF02::C:3702 */
char xaddr6[1024];

char *message;
char *message_loop;
char template_dir[1024];
int exit_main;
int msg_number;
char uuid[UUID_LEN + 1];
char msg_uuid[UUID_LEN + 1];

char model[64];
char hardware[64];

/* -------------------------------------------------------------------------
 * IPv4 interface auto-detection: connect a UDP socket (no packet sent) to
 * the WSD multicast group and read back the kernel's chosen source address.
 * Returns 0 on success, -1 on failure.
 * ---------------------------------------------------------------------- */
static int detect_outbound_address(char *addr_str)
{
    int fd;
    struct sockaddr_in dst, src;
    socklen_t src_len = sizeof(src);

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    memset(&dst, 0, sizeof(dst));
    dst.sin_family      = AF_INET;
    dst.sin_addr.s_addr = inet_addr(MULTICAST_ADDRESS);
    dst.sin_port        = htons(PORT);

    if (connect(fd, (struct sockaddr *)&dst, sizeof(dst)) < 0) { close(fd); return -1; }

    memset(&src, 0, sizeof(src));
    if (getsockname(fd, (struct sockaddr *)&src, &src_len) < 0) { close(fd); return -1; }
    close(fd);

    if (src.sin_addr.s_addr == htonl(INADDR_ANY)) return -1;

    strncpy(addr_str, inet_ntoa(src.sin_addr), 15);
    addr_str[15] = 0;
    return 0;
}

/* -------------------------------------------------------------------------
 * IPv6 interface auto-detection: scan interfaces for the first non-loopback
 * IPv6 address.  Prefers global unicast over link-local.
 * Returns 0 on success, writing the address into addr_str and the interface
 * index into *if_idx_out; returns -1 if no usable address is found.
 *
 * Note: the connect-to-multicast trick used for IPv4 does not work for
 * FF02::C (link-local scope) -- Linux requires sin6_scope_id != 0 for
 * link-local multicast, so we use getifaddrs instead.
 * ---------------------------------------------------------------------- */
static int detect_outbound_address_v6(char *addr_str, size_t len,
                                      unsigned int *if_idx_out)
{
    struct ifaddrs *ifap, *ifa;
    char best[INET6_ADDRSTRLEN] = {0};
    unsigned int best_idx = 0;
    int found_global = 0;

    if (getifaddrs(&ifap) < 0) return -1;
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET6) continue;
        struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)ifa->ifa_addr;
        if (IN6_IS_ADDR_LOOPBACK(&s6->sin6_addr)) continue;
        if (found_global && IN6_IS_ADDR_LINKLOCAL(&s6->sin6_addr)) continue;
        char buf[INET6_ADDRSTRLEN];
        if (!inet_ntop(AF_INET6, &s6->sin6_addr, buf, sizeof(buf))) continue;
        strncpy(best, buf, INET6_ADDRSTRLEN - 1);
        best_idx = if_nametoindex(ifa->ifa_name);
        if (!IN6_IS_ADDR_LINKLOCAL(&s6->sin6_addr)) { found_global = 1; break; }
    }
    freeifaddrs(ifap);
    if (!best[0]) return -1;
    strncpy(addr_str, best, len - 1); addr_str[len - 1] = 0;
    if (if_idx_out) *if_idx_out = best_idx;
    return 0;
}

/* -------------------------------------------------------------------------
 * Get the first non-loopback IPv6 address for a named interface.
 * Prefers global unicast over link-local.
 * ---------------------------------------------------------------------- */
static int get_ip6_address(char *addr_str, size_t len, const char *if_name)
{
    struct ifaddrs *ifap, *ifa;
    char candidate[INET6_ADDRSTRLEN];
    int found_global = 0, found_any = 0;

    if (getifaddrs(&ifap) < 0) return -1;

    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET6) continue;
        if (strcmp(ifa->ifa_name, if_name) != 0) continue;
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
        if (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr)) continue;
        if (inet_ntop(AF_INET6, &sin6->sin6_addr, candidate, sizeof(candidate)) == NULL) continue;
        if (!IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
            strncpy(addr_str, candidate, len - 1); addr_str[len-1] = 0;
            found_global = 1; break;
        } else if (!found_any) {
            strncpy(addr_str, candidate, len - 1); addr_str[len-1] = 0;
            found_any = 1;
        }
    }
    freeifaddrs(ifap);
    return (found_global || found_any) ? 0 : -1;
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
 * UUID v5 (SHA-1 name-based, RFC 4122) from device MAC address.
 *
 * Satisfies WS-Discovery 1.1: "The endpoint reference MUST be stable
 * across restarts of the Target Service."
 *
 * Both wsd_simple_server and onvif_simple_server can derive the same UUID
 * independently from the same MAC, with no shared state or IPC required.
 * ---------------------------------------------------------------------- */

/* RFC 4122 DNS namespace: 6ba7b810-9dad-11d1-80b4-00c04fd430c8 */
static const uint8_t UUID_NS_DNS[16] = {
    0x6b, 0xa7, 0xb8, 0x10,  0x9d, 0xad,  0x11, 0xd1,
    0x80, 0xb4,  0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8
};

static void gen_uuid_v5_mac(char *uuid_str, const uint8_t mac[6])
{
    char name[32];
    uint8_t input[16 + 24];
    uint8_t hash[20];
    size_t name_len;

    snprintf(name, sizeof(name), "mac:%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    name_len = strlen(name);

    memcpy(input, UUID_NS_DNS, 16);
    memcpy(input + 16, name, name_len);
    sha1(input, 16 + name_len, hash);

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

static int get_mac_by_ifname(const char *if_name, uint8_t mac_out[6])
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

static int get_mac_by_ip(const char *ip_str, uint8_t mac_out[6])
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

/* -------------------------------------------------------------------------
 * Daemon / PID helpers
 * ---------------------------------------------------------------------- */
int daemonize(int flags)
{
    int maxfd, fd;

    switch(fork()) { case -1: return -1; case 0: break; default: _exit(EXIT_SUCCESS); }
    if(setsid() == -1) return -1;
    switch(fork()) { case -1: return -1; case 0: break; default: _exit(EXIT_SUCCESS); }

    if(!(flags & BD_NO_UMASK0))      umask(0);
    if(!(flags & BD_NO_CHDIR))       chdir("/");
    if(!(flags & BD_NO_CLOSE_FILES)) {
        maxfd = sysconf(_SC_OPEN_MAX);
        if(maxfd == -1) maxfd = BD_MAX_CLOSE;
        for(fd = 0; fd < maxfd; fd++) close(fd);
    }
    if(!(flags & BD_NO_REOPEN_STD_FDS)) {
        close(STDIN_FILENO);
        fd = open("/dev/null", O_RDWR);
        if(fd != STDIN_FILENO) return -1;
        if(dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO) return -2;
        if(dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO) return -3;
    }
    return 0;
}

int check_pid(char *file_name)
{
    FILE *f; long pid; char buf[PID_SIZE];
    f = fopen(file_name, "r");
    if(!f) return 0;
    if(!fgets(buf, PID_SIZE, f)) { fclose(f); return 0; }
    fclose(f);
    if(sscanf(buf, "%ld", &pid) != 1) return 0;
    return (kill(pid, 0) == 0) ? 1 : 0;
}

int create_pid(char *file_name)
{
    FILE *f; char buf[PID_SIZE];
    f = fopen(file_name, "w");
    if(!f) return -1;
    memset(buf, '\0', PID_SIZE);
    sprintf(buf, "%ld\n", (long) getpid());
    if(fwrite(buf, strlen(buf), 1, f) != 1) { fclose(f); return -2; }
    fclose(f);
    return 0;
}

/* -------------------------------------------------------------------------
 * WSD message helpers
 * ---------------------------------------------------------------------- */
static int send_wsd_msg(int s, const char *buf, size_t len,
                        const struct sockaddr *dst, socklen_t dst_len)
{
    return (sendto(s, buf, len, 0, dst, dst_len) < 0) ? -1 : 0;
}

static void send_bye(int s, const struct sockaddr *dst, socklen_t dst_len,
                     const char *xaddr_str)
{
    char s_tmp[32];
    long size;

    msg_number++;
    sprintf(s_tmp, "%d", msg_number);
    gen_uuid(msg_uuid);

    sprintf(template_file, "%s/Bye.xml", template_dir);
    size = cat(NULL, template_file, 12,
               "%MSG_UUID%", msg_uuid, "%MSG_NUMBER%", s_tmp, "%UUID%", uuid,
               "%HARDWARE%", hardware, "%NAME%", model, "%ADDRESS%", xaddr_str);
    message = (char *) malloc((size + 1) * sizeof(char));
    if (!message) { log_fatal("Malloc error."); return; }
    cat(message, template_file, 12,
        "%MSG_UUID%", msg_uuid, "%MSG_NUMBER%", s_tmp, "%UUID%", uuid,
        "%HARDWARE%", hardware, "%NAME%", model, "%ADDRESS%", xaddr_str);

    if (send_wsd_msg(s, message, strlen(message), dst, dst_len) < 0)
        log_fatal("Error sending Bye message.");
    else
        log_info("Bye sent.");
    free(message);
}

void signal_handler(int signal)
{
    log_info("Sending Bye message(s).");
    if (sock >= 0) {
        send_bye(sock, (struct sockaddr *)&addr_mcast4, sizeof(addr_mcast4), xaddr);
        if (xaddr6[0])
            send_bye(sock, (struct sockaddr *)&addr_mcast6, sizeof(addr_mcast6), xaddr6);
        shutdown(sock, SHUT_RDWR); close(sock); sock = -1;
    }
    exit_main = 1;
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [-i INTERFACE] -x XADDR [-6 XADDR6] [-m MODEL] [-n MANUFACTURER] -p PID_FILE [-t TEMPLATE_DIR] [-f] [-d LEVEL]\n\n", progname);
    fprintf(stderr, "\t-i, --if_name\n");
    fprintf(stderr, "\t\tnetwork interface (auto-detected; use -i to force a specific interface)\n");
    fprintf(stderr, "\t-x, --xaddr\n");
    fprintf(stderr, "\t\tIPv4 resource address (use %%s as placeholder for the detected IP)\n");
    fprintf(stderr, "\t-6, --xaddr6\n");
    fprintf(stderr, "\t\tIPv6 resource address template (use %%s as placeholder for the detected\n");
    fprintf(stderr, "\t\tIPv6 address; include brackets, e.g. http://[%%s]:PORT/path)\n");
    fprintf(stderr, "\t\tIPv6 WS-Discovery is enabled only when this option is provided\n");
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

static void handle_probe(int s, struct sockaddr *dst, socklen_t dst_len,
                         char *buf, const char *xaddr_str)
{
    const char *relates_to_uuid;
    char s_tmp[32];
    long size;

    if (strstr(buf, "XAddrs") != NULL) return;

    log_debug("%s", buf);
    init_xml(buf, strlen(buf));
    const char *method = get_method(1);
    if (!method || strcasecmp("Probe", method) != 0) {
        log_debug("This is not a Probe message");
        close_xml(); return;
    }
    log_debug("Probe message");

    msg_number++;
    sprintf(s_tmp, "%d", msg_number);
    gen_uuid(msg_uuid);

    relates_to_uuid = get_element("MessageID", "Header");
    if (!relates_to_uuid) { log_error("Cannot find MessageID."); close_xml(); return; }
    close_xml();

    log_info("Sending ProbeMatches message.");
    sprintf(template_file, "%s/ProbeMatches.xml", template_dir);
    size = cat(NULL, template_file, 14,
               "%MSG_UUID%", msg_uuid, "%REL_TO_UUID%", relates_to_uuid,
               "%MSG_NUMBER%", s_tmp, "%UUID%", uuid,
               "%HARDWARE%", hardware, "%NAME%", model, "%ADDRESS%", xaddr_str);
    message_loop = (char *) malloc((size + 1) * sizeof(char));
    if (!message_loop) { log_error("Malloc error."); return; }
    cat(message_loop, template_file, 14,
        "%MSG_UUID%", msg_uuid, "%REL_TO_UUID%", relates_to_uuid,
        "%MSG_NUMBER%", s_tmp, "%UUID%", uuid,
        "%HARDWARE%", hardware, "%NAME%", model, "%ADDRESS%", xaddr_str);

    if (send_wsd_msg(s, message_loop, strlen(message_loop), dst, dst_len) < 0)
        log_error("Error sending ProbeMatches message.");
    else
        log_info("Sent.");
    free(message_loop);
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */
int main(int argc, char **argv)
{
    int errno;
    char *endptr;
    int c, ret;
    char *if_name, *pid_file, *xaddr_s, *xaddr6_s;
    unsigned int if6_idx = 0;
    int foreground, no = 0;
    char s_tmp[32];
    long size;
    char recv_buffer[RECV_BUFFER_LEN];

    if_name = pid_file = xaddr_s = xaddr6_s = NULL;
    strcpy(model, "MODEL_NAME");
    strcpy(hardware, "HARDWARE_MANUFACTURER");
    strcpy(template_dir, DEFAULT_TEMPLATE_DIR);
    foreground = 0;
    debug = 5;
    sock = -1;

    while (1) {
        static struct option long_options[] = {
            {"if_name",      required_argument, 0, 'i'},
            {"pid_file",     required_argument, 0, 'p'},
            {"xaddr",        required_argument, 0, 'x'},
            {"xaddr6",       required_argument, 0, '6'},
            {"model",        required_argument, 0, 'm'},
            {"hardware",     required_argument, 0, 'n'},
            {"template_dir", required_argument, 0, 't'},
            {"foreground",   no_argument,       0, 'f'},
            {"debug",        required_argument, 0, 'd'},
            {"help",         no_argument,       0, 'h'},
            {0, 0, 0, 0}
        };
        int option_index = 0;
        c = getopt_long(argc, argv, "i:p:x:6:m:n:t:fd:h", long_options, &option_index);
        if (c == -1) break;

        switch (c) {
        case 'i': if_name  = optarg; break;
        case 'x': xaddr_s  = optarg; break;
        case '6': xaddr6_s = optarg; break;
        case 'm':
            if (strlen(optarg) < sizeof(model)) strcpy(model, optarg);
            else { print_usage(argv[0]); exit(EXIT_FAILURE); }
            break;
        case 'n':
            if (strlen(optarg) < sizeof(hardware)) strcpy(hardware, optarg);
            else { print_usage(argv[0]); exit(EXIT_FAILURE); }
            break;
        case 'p': pid_file = optarg; break;
        case 't':
            if (strlen(optarg) < sizeof(template_dir)) strcpy(template_dir, optarg);
            else { print_usage(argv[0]); exit(EXIT_FAILURE); }
            break;
        case 'f': foreground = 1; break;
        case 'd':
            debug = strtol(optarg, &endptr, 10);
            if ((errno == ERANGE && (debug == LONG_MAX || debug == LONG_MIN)) ||
                (errno != 0 && debug == 0) || endptr == optarg ||
                debug < LOG_TRACE || debug > LOG_FATAL) {
                print_usage(argv[0]); exit(EXIT_FAILURE);
            }
            debug = 5 - debug;
            break;
        case 'h': print_usage(argv[0]); exit(EXIT_SUCCESS);
        case '?': break;
        default:  print_usage(argv[0]); exit(EXIT_SUCCESS);
        }
    }

    if (!xaddr_s || !pid_file) { print_usage(argv[0]); exit(EXIT_SUCCESS); }

    signal(SIGTERM, signal_handler);
    signal(SIGINT,  signal_handler);

    if (!foreground) {
        ret = daemonize(0);
        if (ret) { fprintf(stderr, "Error starting daemon.\n"); exit(EXIT_FAILURE); }
    }

    fLog = fopen(DEFAULT_LOG_FILE, "w");
    log_add_fp(fLog, debug);
    log_set_level(debug);
    log_set_quiet(1);
    log_info("Starting program.");

    log_debug("Argument if_name = %s", if_name ? if_name : "(auto-detect)");
    log_debug("Argument pid_file = %s", pid_file);
    log_debug("Argument xaddr = %s", xaddr_s);
    if (xaddr6_s) log_debug("Argument xaddr6 = %s", xaddr6_s);

    if (check_pid(pid_file) == 1) {
        log_fatal("Program is already running."); fclose(fLog); exit(EXIT_FAILURE);
    }
    if (create_pid(pid_file) < 0) {
        log_fatal("Error creating pid file %s", pid_file); fclose(fLog); exit(EXIT_FAILURE);
    }

    if (access(template_dir, F_OK) == -1) {
        log_fatal("Unable to open directory %s", template_dir); fclose(fLog); exit(EXIT_FAILURE);
    }
    {
        DIR *dirptr = opendir(template_dir);
        if (!dirptr) {
            log_fatal("Unable to open directory %s", template_dir); fclose(fLog); exit(EXIT_FAILURE);
        }
        closedir(dirptr);
    }

    srand((unsigned int) time(NULL));

    /* ---- IPv4 address detection ---------------------------------------- */
    if (if_name) {
        get_ip_address(address, netmask, if_name);
    } else {
        if (detect_outbound_address(address) != 0) {
            log_fatal("Cannot auto-detect network interface. Use -i <interface>.");
            fclose(fLog); exit(EXIT_FAILURE);
        }
        log_info("Auto-detected outbound IPv4 address: %s", address);
    }
    log_debug("IPv4 address = %s", address);

    /* ---- IPv6 address detection (only when -6 is given) ---------------- */
    memset(address6, 0, sizeof(address6));
    if (xaddr6_s) {
        if (if_name) {
            if (get_ip6_address(address6, sizeof(address6), if_name) != 0) {
                log_warn("No IPv6 address on interface %s; IPv6 WSD disabled.", if_name);
                xaddr6_s = NULL;
            }
        } else {
            if (detect_outbound_address_v6(address6, sizeof(address6), &if6_idx) != 0) {
                log_warn("Cannot auto-detect outbound IPv6 address; IPv6 WSD disabled.");
                xaddr6_s = NULL;
            } else {
                log_info("Auto-detected outbound IPv6 address: %s", address6);
            }
        }
    }

    /* ---- Stable UUID from MAC (WS-Discovery 1.1 EPR stability req.) ---- */
    {
        uint8_t mac[6] = {0};
        int mac_ok = (if_name ? get_mac_by_ifname(if_name, mac)
                              : get_mac_by_ip(address, mac)) == 0;
        if (mac_ok) {
            gen_uuid_v5_mac(uuid, mac);
            log_info("Device UUID (stable, MAC-based): %s", uuid);
        } else {
            gen_uuid(uuid);
            log_info("Device UUID (random, MAC unavailable): %s", uuid);
        }
    }

    /* ---- Single dual-stack UDP socket ------------------------------------- */
    {
        struct sockaddr_in6 bind6;
        struct group_req gr4;
        int yes = 1;

        if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
            log_fatal("Unable to create socket: %s", strerror(errno));
            fclose(fLog); exit(EXIT_FAILURE);
        }
        /* IPV6_V6ONLY = 0: dual-stack -- :::3702 receives both IPv4 (as ::ffff:x.x.x.x)
         * and IPv6.  Explicitly set to override net.ipv6.bindv6only on any platform. */
        setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        memset(&bind6, 0, sizeof(bind6));
        bind6.sin6_family = AF_INET6; bind6.sin6_port = htons(PORT);
        bind6.sin6_addr = in6addr_any;
        if (bind(sock, (struct sockaddr *)&bind6, sizeof(bind6)) < 0) {
            log_fatal("Unable to bind socket: %s", strerror(errno));
            close(sock); fclose(fLog); exit(EXIT_FAILURE);
        }

        /* Join IPv4 WSD multicast group.
         * MCAST_JOIN_GROUP (RFC 3678 protocol-independent API) works on AF_INET6
         * dual-stack sockets for both IPv4 and IPv6 multicast groups. */
        memset(&gr4, 0, sizeof(gr4));
        gr4.gr_interface = if_name ? if_nametoindex(if_name) : 0;
        ((struct sockaddr_in *)&gr4.gr_group)->sin_family      = AF_INET;
        ((struct sockaddr_in *)&gr4.gr_group)->sin_addr.s_addr = inet_addr(MULTICAST_ADDRESS);
        if (setsockopt(sock, IPPROTO_IPV6, MCAST_JOIN_GROUP, &gr4, sizeof(gr4)) < 0) {
            log_fatal("Error joining IPv4 multicast group: %s", strerror(errno));
            close(sock); fclose(fLog); exit(EXIT_FAILURE);
        }

        /* IPv4 multicast destination as IPv4-mapped IPv6 address */
        memset(&addr_mcast4, 0, sizeof(addr_mcast4));
        addr_mcast4.sin6_family = AF_INET6; addr_mcast4.sin6_port = htons(PORT);
        inet_pton(AF_INET6, "::ffff:" MULTICAST_ADDRESS, &addr_mcast4.sin6_addr);
    }

    /* ---- IPv6 multicast (optional) --------------------------------------- */
    if (xaddr6_s) {
        struct ipv6_mreq mr6;
        unsigned int if_idx = if_name ? if_nametoindex(if_name) : if6_idx;

        /* if_idx must be non-zero for link-local multicast (FF02::). */
        memset(&mr6, 0, sizeof(mr6));
        inet_pton(AF_INET6, MULTICAST_ADDRESS_V6, &mr6.ipv6mr_multiaddr);
        mr6.ipv6mr_interface = if_idx;
        if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mr6, sizeof(mr6)) < 0) {
            log_warn("Cannot join IPv6 multicast group; IPv6 WSD disabled: %s", strerror(errno));
            xaddr6_s = NULL;
        } else {
            if (if_idx) setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, &if_idx, sizeof(if_idx));
            memset(&addr_mcast6, 0, sizeof(addr_mcast6));
            addr_mcast6.sin6_family = AF_INET6; addr_mcast6.sin6_port = htons(PORT);
            inet_pton(AF_INET6, MULTICAST_ADDRESS_V6, &addr_mcast6.sin6_addr);
            addr_mcast6.sin6_scope_id = if_idx;
            log_info("IPv6 WSD multicast joined (FF02::C, if_idx=%u).", if_idx);
        }
    }

    /* ---- Build xaddr strings ------------------------------------------- */
    sprintf(xaddr, xaddr_s, address);
    if (xaddr6_s) sprintf(xaddr6, xaddr6_s, address6);

    /* ---- Send Hello ---------------------------------------------------- */
    msg_number = 1;
    sprintf(s_tmp, "%d", msg_number);
    gen_uuid(msg_uuid);

    sprintf(template_file, "%s/Hello.xml", template_dir);
    size = cat(NULL, template_file, 12,
               "%MSG_UUID%", msg_uuid, "%MSG_NUMBER%", s_tmp, "%UUID%", uuid,
               "%HARDWARE%", hardware, "%NAME%", model, "%ADDRESS%", xaddr);
    message = (char *) malloc((size + 1) * sizeof(char));
    if (!message) {
        log_fatal("Malloc error."); close(sock); fclose(fLog); exit(EXIT_FAILURE);
    }
    cat(message, template_file, 12,
        "%MSG_UUID%", msg_uuid, "%MSG_NUMBER%", s_tmp, "%UUID%", uuid,
        "%HARDWARE%", hardware, "%NAME%", model, "%ADDRESS%", xaddr);
    if (send_wsd_msg(sock, message, strlen(message),
                     (struct sockaddr *)&addr_mcast4, sizeof(addr_mcast4)) < 0) {
        log_fatal("Error sending Hello (IPv4).");
        free(message); close(sock); fclose(fLog); exit(EXIT_FAILURE);
    }
    free(message);
    log_info("Hello (IPv4) sent.");

    if (xaddr6[0]) {
        msg_number++;
        sprintf(s_tmp, "%d", msg_number);
        gen_uuid(msg_uuid);
        sprintf(template_file, "%s/Hello.xml", template_dir);
        size = cat(NULL, template_file, 12,
                   "%MSG_UUID%", msg_uuid, "%MSG_NUMBER%", s_tmp, "%UUID%", uuid,
                   "%HARDWARE%", hardware, "%NAME%", model, "%ADDRESS%", xaddr6);
        message = (char *) malloc((size + 1) * sizeof(char));
        if (message) {
            cat(message, template_file, 12,
                "%MSG_UUID%", msg_uuid, "%MSG_NUMBER%", s_tmp, "%UUID%", uuid,
                "%HARDWARE%", hardware, "%NAME%", model, "%ADDRESS%", xaddr6);
            if (send_wsd_msg(sock, message, strlen(message),
                             (struct sockaddr *)&addr_mcast6, sizeof(addr_mcast6)) < 0)
                log_warn("Error sending Hello (IPv6).");
            else
                log_info("Hello (IPv6) sent.");
            free(message);
        }
    }

    sleep(1);

    /* ---- Main loop ----------------------------------------------------- */
    log_info("Starting main loop (IPv4%s).", xaddr6[0] ? " + IPv6" : " only");
    exit_main = 0;

    while (!exit_main) {
        struct sockaddr_in6 sender;
        socklen_t slen = sizeof(sender);
        const char *probe_xaddr;
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        if (select(sock + 1, &rfds, NULL, NULL, NULL) <= 0) continue;

        memset(recv_buffer, 0, RECV_BUFFER_LEN);
        if (recvfrom(sock, recv_buffer, RECV_BUFFER_LEN, 0,
                     (struct sockaddr *)&sender, &slen) <= 0) continue;

        if (IN6_IS_ADDR_V4MAPPED(&sender.sin6_addr)) {
            probe_xaddr = xaddr;
        } else if (xaddr6[0]) {
            probe_xaddr = xaddr6;
        } else {
            log_debug("IPv6 probe received but -6 not configured; ignoring.");
            continue;
        }
        handle_probe(sock, (struct sockaddr *)&sender, slen, recv_buffer, probe_xaddr);
    }

    if (sock >= 0) { shutdown(sock, SHUT_RDWR); close(sock); }

    log_info("Terminating program.");
    fclose(fLog);
    unlink(pid_file);
    return 0;
}
