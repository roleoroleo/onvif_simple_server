/*
 * Copyright (c) 2026 roleo.
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

/* Largest UDP payload that fits one datagram at the IPv6 minimum MTU (1280). */
#define WSD_MAX_MSG_LEN 1232

/* The type scopes are advertised whether or not the device supports them,
 * and none are taken from the scope= entries of the onvif_simple_server
 * config. Use -s to send the device's own list. */
#define DEFAULT_SCOPES "onvif://www.onvif.org/type/video_encoder " \
                       "onvif://www.onvif.org/type/audio_encoder " \
                       "onvif://www.onvif.org/type/ptz " \
                       "onvif://www.onvif.org/hardware/%s " \
                       "onvif://www.onvif.org/name/%s " \
                       "onvif://www.onvif.org/location/anywhere"

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
/* Service socket: dual-stack AF_INET6 (IPV6_V6ONLY=0) when an IPv6 address is
 * present, else a plain AF_INET socket (IPv4-only, e.g. kernels without IPv6). */
int sock;
int sock_family;
/* IPv4 */
char address[16], netmask[16];
struct sockaddr_storage addr_mcast4; /* IPv4 WSD mcast dest; family per sock_family */
socklen_t addr_mcast4_len;
char xaddr[1024];
/* IPv6 (auto-detected at startup) */
char address6[INET6_ADDRSTRLEN];
struct sockaddr_in6 addr_mcast6; /* [FF02::C]:3702 */
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
char scopes[1024];       /* Hello and Bye */
char scopes_probe[1024]; /* ProbeMatches */

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
    inet_pton(AF_INET, MULTICAST_ADDRESS, &dst.sin_addr);
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
    snprintf(buf, sizeof(buf), "%ld\n", (long) getpid());
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
    if (len > WSD_MAX_MSG_LEN)
        log_warn("WSD message is %zu bytes (limit %d) and may be fragmented.",
                 len, WSD_MAX_MSG_LEN);
    return (sendto(s, buf, len, 0, dst, dst_len) < 0) ? -1 : 0;
}

static void send_bye(int s, const struct sockaddr *dst, socklen_t dst_len,
                     const char *xaddr_str)
{
    char s_tmp[32];
    long size;

    msg_number++;
    snprintf(s_tmp, sizeof(s_tmp), "%d", msg_number);
    gen_uuid(msg_uuid);

    snprintf(template_file, sizeof(template_file), "%s/Bye.xml", template_dir);
    size = cat(NULL, template_file, 14,
               "%MSG_UUID%", msg_uuid, "%MSG_NUMBER%", s_tmp, "%UUID%", uuid,
               "%SCOPES%", scopes,
               "%HARDWARE%", hardware, "%NAME%", model, "%ADDRESS%", xaddr_str);
    message = (char *) malloc((size + 1) * sizeof(char));
    if (!message) { log_fatal("Malloc error."); return; }
    cat_n(message, size + 1, template_file, 14,
        "%MSG_UUID%", msg_uuid, "%MSG_NUMBER%", s_tmp, "%UUID%", uuid,
        "%SCOPES%", scopes,
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
    fprintf(stderr, "\nUsage: %s [-i INTERFACE] -x XADDR [-m MODEL] [-n MANUFACTURER] [-s SCOPES] -p PID_FILE [-t TEMPLATE_DIR] [-f] [-d LEVEL]\n\n", progname);
    fprintf(stderr, "\t-i, --if_name\n");
    fprintf(stderr, "\t\tnetwork interface (auto-detected; use -i to force a specific interface)\n");
    fprintf(stderr, "\t-x, --xaddr\n");
    fprintf(stderr, "\t\tresource address template (use %%s as placeholder for the detected IP;\n");
    fprintf(stderr, "\t\tIPv6 is auto-detected and the address is bracketed automatically)\n");
    fprintf(stderr, "\t-m, --model\n");
    fprintf(stderr, "\t\tmodel name\n");
    fprintf(stderr, "\t-n, --hardware\n");
    fprintf(stderr, "\t\thardware manufacturer\n");
    fprintf(stderr, "\t-s, --scopes\n");
    fprintf(stderr, "\t\tspace-separated scope list (default: built-in list)\n");
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
    snprintf(s_tmp, sizeof(s_tmp), "%d", msg_number);
    gen_uuid(msg_uuid);

    relates_to_uuid = get_element("MessageID", "Header");
    if (!relates_to_uuid) { log_error("Cannot find MessageID."); close_xml(); return; }
    close_xml();

    log_info("Sending ProbeMatches message.");
    snprintf(template_file, sizeof(template_file), "%s/ProbeMatches.xml", template_dir);
    size = cat(NULL, template_file, 16,
               "%MSG_UUID%", msg_uuid, "%REL_TO_UUID%", relates_to_uuid,
               "%MSG_NUMBER%", s_tmp, "%UUID%", uuid, "%SCOPES%", scopes_probe,
               "%HARDWARE%", hardware, "%NAME%", model, "%ADDRESS%", xaddr_str);
    message_loop = (char *) malloc((size + 1) * sizeof(char));
    if (!message_loop) { log_error("Malloc error."); return; }
    cat_n(message_loop, size + 1, template_file, 16,
        "%MSG_UUID%", msg_uuid, "%REL_TO_UUID%", relates_to_uuid,
        "%MSG_NUMBER%", s_tmp, "%UUID%", uuid, "%SCOPES%", scopes_probe,
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
    char *if_name, *pid_file, *xaddr_s;
    unsigned int if6_idx = 0;
    int have_v4 = 0;
    int foreground, no = 0;
    char s_tmp[32];
    long size;
    char recv_buffer[RECV_BUFFER_LEN];

    if_name = pid_file = xaddr_s = NULL;
    snprintf(model, sizeof(model), "%s", "MODEL_NAME");
    snprintf(hardware, sizeof(hardware), "%s", "HARDWARE_MANUFACTURER");
    snprintf(template_dir, sizeof(template_dir), "%s", DEFAULT_TEMPLATE_DIR);
    foreground = 0;
    debug = 5;
    sock = -1;

    while (1) {
        static struct option long_options[] = {
            {"if_name",      required_argument, 0, 'i'},
            {"pid_file",     required_argument, 0, 'p'},
            {"xaddr",        required_argument, 0, 'x'},

            {"model",        required_argument, 0, 'm'},
            {"hardware",     required_argument, 0, 'n'},
            {"scopes",       required_argument, 0, 's'},
            {"template_dir", required_argument, 0, 't'},
            {"foreground",   no_argument,       0, 'f'},
            {"debug",        required_argument, 0, 'd'},
            {"help",         no_argument,       0, 'h'},
            {0, 0, 0, 0}
        };
        int option_index = 0;
        c = getopt_long(argc, argv, "i:p:x:m:n:s:t:fd:h", long_options, &option_index);
        if (c == -1) break;

        switch (c) {
        case 'i': if_name  = optarg; break;
        case 'x': xaddr_s  = optarg; break;

        case 'm':
            if (strlen(optarg) < sizeof(model)) snprintf(model, sizeof(model), "%s", optarg);
            else { print_usage(argv[0]); exit(EXIT_FAILURE); }
            break;
        case 'n':
            if (strlen(optarg) < sizeof(hardware)) snprintf(hardware, sizeof(hardware), "%s", optarg);
            else { print_usage(argv[0]); exit(EXIT_FAILURE); }
            break;
        case 's':
            if (strlen(optarg) < sizeof(scopes)) {
                snprintf(scopes, sizeof(scopes), "%s", optarg);
                snprintf(scopes_probe, sizeof(scopes_probe), "%s", optarg);
            } else { print_usage(argv[0]); exit(EXIT_FAILURE); }
            break;
        case 'p': pid_file = optarg; break;
        case 't':
            if (strlen(optarg) < sizeof(template_dir)) snprintf(template_dir, sizeof(template_dir), "%s", optarg);
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

    if (!scopes[0]) {
        snprintf(scopes, sizeof(scopes), DEFAULT_SCOPES, hardware, model);
        snprintf(scopes_probe, sizeof(scopes_probe),
                 DEFAULT_SCOPES " onvif://www.onvif.org/Profile/Streaming", hardware, model);
    }

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
    log_debug("Argument scopes = %s", scopes);


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
        have_v4 = (address[0] != '\0');
    } else {
        if (detect_outbound_address(address) == 0) {
            have_v4 = 1;
            log_info("Auto-detected outbound IPv4 address: %s", address);
        } else {
            log_warn("Cannot auto-detect outbound IPv4 address; will try IPv6.");
        }
    }
    if (have_v4) log_debug("IPv4 address = %s", address);

    /* ---- IPv6 address detection (always attempted) --------------------- */
    memset(address6, 0, sizeof(address6));
    xaddr6[0] = '\0';
    {
        int v6_ok;
        if (if_name)
            v6_ok = (get_ip6_address(address6, sizeof(address6), if_name) == 0);
        else
            v6_ok = (detect_outbound_address_v6(address6, sizeof(address6), &if6_idx) == 0);
        if (v6_ok) {
            char tmp6[INET6_ADDRSTRLEN + 4];
            log_info("IPv6 address: %s", address6);
            snprintf(tmp6, sizeof(tmp6), "[%s]", address6);
            snprintf(xaddr6, sizeof(xaddr6), xaddr_s, tmp6);
        } else {
            log_debug("No global IPv6 address found; IPv6 WSD disabled.");
        }
    }

    if (!have_v4 && !xaddr6[0]) {
        log_fatal("No usable IPv4 or IPv6 address found. Use -i <interface> or check network.");
        fclose(fLog); exit(EXIT_FAILURE);
    }

    /* ---- Stable UUID from MAC (WS-Discovery 1.1 EPR stability req.) ---- */
    {
        uint8_t mac[6] = {0};
        int mac_ok;
        if (if_name) {
            mac_ok = (get_mac_by_ifname(if_name, mac) == 0);
        } else if (have_v4) {
            mac_ok = (get_mac_by_ip(address, mac) == 0);
        } else {
            /* IPv6-only: derive interface name from if6_idx */
            char if6_name[IF_NAMESIZE] = {0};
            mac_ok = (if6_idx && if_indextoname(if6_idx, if6_name) &&
                      get_mac_by_ifname(if6_name, mac) == 0);
        }
        if (mac_ok) {
            gen_uuid_v5_mac(uuid, mac);
            log_info("Device UUID (stable, MAC-based): %s", uuid);
        } else {
            gen_uuid(uuid);
            log_info("Device UUID (random, MAC unavailable): %s", uuid);
        }
    }

    /* ---- Service socket: dual-stack AF_INET6, or AF_INET (IPv4-only) ------- */
    /* Family follows the IPv6 detection result: dual-stack only when there is
     * an IPv6 address to serve, else plain AF_INET -- so WSD still runs on
     * kernels built without IPv6, where socket(AF_INET6) fails. */
    {
        int yes = 1;
        sock_family = xaddr6[0] ? AF_INET6 : AF_INET;

        if ((sock = socket(sock_family, SOCK_DGRAM, 0)) < 0) {
            /* v6 detected but AF_INET6 unavailable: drop to IPv4-only. */
            if (sock_family == AF_INET6) {
                log_warn("AF_INET6 socket unavailable (%s); falling back to IPv4-only.", strerror(errno));
                sock_family = AF_INET;
                xaddr6[0] = '\0';
                sock = socket(AF_INET, SOCK_DGRAM, 0);
            }
            if (sock < 0) {
                log_fatal("Unable to create socket: %s", strerror(errno));
                fclose(fLog); exit(EXIT_FAILURE);
            }
        }

        /* IPV6_V6ONLY=0: the dual-stack socket also receives IPv4 (::ffff:x.x.x.x). */
        if (sock_family == AF_INET6)
            setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (sock_family == AF_INET6) {
            struct sockaddr_in6 bind6;
            memset(&bind6, 0, sizeof(bind6));
            bind6.sin6_family = AF_INET6; bind6.sin6_port = htons(PORT);
            bind6.sin6_addr = in6addr_any;
            if (bind(sock, (struct sockaddr *)&bind6, sizeof(bind6)) < 0) {
                log_fatal("Unable to bind socket: %s", strerror(errno));
                close(sock); fclose(fLog); exit(EXIT_FAILURE);
            }
        } else {
            struct sockaddr_in bind4;
            memset(&bind4, 0, sizeof(bind4));
            bind4.sin_family = AF_INET; bind4.sin_port = htons(PORT);
            bind4.sin_addr.s_addr = INADDR_ANY;
            if (bind(sock, (struct sockaddr *)&bind4, sizeof(bind4)) < 0) {
                log_fatal("Unable to bind socket: %s", strerror(errno));
                close(sock); fclose(fLog); exit(EXIT_FAILURE);
            }
        }

        /* Join the IPv4 WSD multicast group. IP_ADD_MEMBERSHIP with ip_mreq
         * works on a plain AF_INET socket and, via IPV6_V6ONLY=0, on the
         * dual-stack AF_INET6 socket too (MCAST_JOIN_GROUP with IPPROTO_IPV6
         * does not, because the kernel IPv6 mc path rejects AF_INET groups). */
        if (have_v4) {
            struct ip_mreq mreq4;
            memset(&mreq4, 0, sizeof(mreq4));
            inet_pton(AF_INET, MULTICAST_ADDRESS, &mreq4.imr_multiaddr);
            mreq4.imr_interface.s_addr = INADDR_ANY;
            if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq4, sizeof(mreq4)) < 0) {
                log_fatal("Error joining IPv4 multicast group: %s", strerror(errno));
                close(sock); fclose(fLog); exit(EXIT_FAILURE);
            }
        }

        /* IPv4 mcast destination: IPv4-mapped on the dual-stack socket, real IPv4 on AF_INET. */
        memset(&addr_mcast4, 0, sizeof(addr_mcast4));
        if (sock_family == AF_INET6) {
            struct sockaddr_in6 *m = (struct sockaddr_in6 *)&addr_mcast4;
            m->sin6_family = AF_INET6; m->sin6_port = htons(PORT);
            inet_pton(AF_INET6, "::ffff:" MULTICAST_ADDRESS, &m->sin6_addr);
            addr_mcast4_len = sizeof(struct sockaddr_in6);
        } else {
            struct sockaddr_in *m = (struct sockaddr_in *)&addr_mcast4;
            m->sin_family = AF_INET; m->sin_port = htons(PORT);
            inet_pton(AF_INET, MULTICAST_ADDRESS, &m->sin_addr);
            addr_mcast4_len = sizeof(struct sockaddr_in);
        }
    }

    /* ---- IPv6 multicast (when IPv6 address was detected) ----------------- */
    if (xaddr6[0]) {
        struct ipv6_mreq mr6;
        unsigned int if_idx = if_name ? if_nametoindex(if_name) : if6_idx;

        /* if_idx must be non-zero for link-local multicast (FF02::). */
        memset(&mr6, 0, sizeof(mr6));
        inet_pton(AF_INET6, MULTICAST_ADDRESS_V6, &mr6.ipv6mr_multiaddr);
        mr6.ipv6mr_interface = if_idx;
        if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mr6, sizeof(mr6)) < 0) {
            log_warn("Cannot join IPv6 multicast group; IPv6 WSD disabled: %s", strerror(errno));
            xaddr6[0] = '\0';
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
    if (have_v4) snprintf(xaddr, sizeof(xaddr), xaddr_s, address);

    /* ---- Send Hello ---------------------------------------------------- */
    msg_number = 1;
    snprintf(s_tmp, sizeof(s_tmp), "%d", msg_number);
    gen_uuid(msg_uuid);

    if (have_v4) {
        snprintf(template_file, sizeof(template_file), "%s/Hello.xml", template_dir);
        size = cat(NULL, template_file, 14,
                   "%MSG_UUID%", msg_uuid, "%MSG_NUMBER%", s_tmp, "%UUID%", uuid,
                   "%SCOPES%", scopes,
                   "%HARDWARE%", hardware, "%NAME%", model, "%ADDRESS%", xaddr);
        message = (char *) malloc((size + 1) * sizeof(char));
        if (!message) {
            log_fatal("Malloc error."); close(sock); fclose(fLog); exit(EXIT_FAILURE);
        }
        cat_n(message, size + 1, template_file, 14,
            "%MSG_UUID%", msg_uuid, "%MSG_NUMBER%", s_tmp, "%UUID%", uuid,
            "%SCOPES%", scopes,
            "%HARDWARE%", hardware, "%NAME%", model, "%ADDRESS%", xaddr);
        if (send_wsd_msg(sock, message, strlen(message),
                         (struct sockaddr *)&addr_mcast4, addr_mcast4_len) < 0) {
            log_fatal("Error sending Hello (IPv4).");
            free(message); close(sock); fclose(fLog); exit(EXIT_FAILURE);
        }
        free(message);
        log_info("Hello (IPv4) sent.");
    }

    if (xaddr6[0]) {
        msg_number++;
        snprintf(s_tmp, sizeof(s_tmp), "%d", msg_number);
        gen_uuid(msg_uuid);
        snprintf(template_file, sizeof(template_file), "%s/Hello.xml", template_dir);
        size = cat(NULL, template_file, 14,
                   "%MSG_UUID%", msg_uuid, "%MSG_NUMBER%", s_tmp, "%UUID%", uuid,
                   "%SCOPES%", scopes,
                   "%HARDWARE%", hardware, "%NAME%", model, "%ADDRESS%", xaddr6);
        message = (char *) malloc((size + 1) * sizeof(char));
        if (message) {
            cat_n(message, size + 1, template_file, 14,
                "%MSG_UUID%", msg_uuid, "%MSG_NUMBER%", s_tmp, "%UUID%", uuid,
                "%SCOPES%", scopes,
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
    if (have_v4 && xaddr6[0])
        log_info("Starting main loop (IPv4 + IPv6).");
    else if (have_v4)
        log_info("Starting main loop (IPv4 only).");
    else
        log_info("Starting main loop (IPv6 only).");
    exit_main = 0;

    while (!exit_main) {
        struct sockaddr_storage sender;
        socklen_t slen = sizeof(sender);
        const char *probe_xaddr;
        int is_v4;
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        if (select(sock + 1, &rfds, NULL, NULL, NULL) <= 0) continue;

        memset(recv_buffer, 0, RECV_BUFFER_LEN);
        if (recvfrom(sock, recv_buffer, RECV_BUFFER_LEN, 0,
                     (struct sockaddr *)&sender, &slen) <= 0) continue;

        /* Native IPv4 (AF_INET socket) or IPv4-mapped IPv6 (dual-stack socket). */
        is_v4 = (sender.ss_family == AF_INET) ||
                ((sender.ss_family == AF_INET6) &&
                 IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *)&sender)->sin6_addr));
        if (is_v4) {
            if (!have_v4) { log_debug("IPv4 probe received but IPv4 WSD not active; ignoring."); continue; }
            probe_xaddr = xaddr;
        } else if (xaddr6[0]) {
            probe_xaddr = xaddr6;
        } else {
            log_debug("IPv6 probe received but IPv6 WSD not configured; ignoring.");
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
