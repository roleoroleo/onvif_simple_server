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
struct sockaddr_in6 addr_mcast4; /* [::ffff:239.255.255.250]:3702 */
char xaddr[1024];
/* IPv6 (optional, only when -6 is given) */
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
    int have_v4 = 0, xaddr6_auto = 0;
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
    {
        int v6_ok;
        if (if_name)
            v6_ok = (get_ip6_address(address6, sizeof(address6), if_name) == 0);
        else
            v6_ok = (detect_outbound_address_v6(address6, sizeof(address6), &if6_idx) == 0);
        if (v6_ok) {
            log_info("IPv6 address: %s", address6);
            if (!xaddr6_s) { xaddr6_s = xaddr_s; xaddr6_auto = 1; }
        } else {
            if (xaddr6_s) log_warn("IPv6 address detection failed; IPv6 WSD disabled.");
            xaddr6_s = NULL;
        }
    }

    if (!have_v4 && !xaddr6_s) {
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

    /* ---- Single dual-stack UDP socket ------------------------------------- */
    {
        struct sockaddr_in6 bind6;
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

        /* Join IPv4 WSD multicast group via IPPROTO_IP on the dual-stack socket.
         * IPV6_V6ONLY=0 causes Linux to route IPPROTO_IP options to the IPv4
         * path, so IP_ADD_MEMBERSHIP works on AF_INET6 dual-stack sockets.
         * MCAST_JOIN_GROUP with IPPROTO_IPV6 does not work here because the
         * kernel's IPv6 mc path rejects AF_INET addresses in gr_group. */
        if (have_v4) {
            struct ip_mreq mreq4;
            memset(&mreq4, 0, sizeof(mreq4));
            mreq4.imr_multiaddr.s_addr = inet_addr(MULTICAST_ADDRESS);
            mreq4.imr_interface.s_addr = INADDR_ANY;
            if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq4, sizeof(mreq4)) < 0) {
                log_fatal("Error joining IPv4 multicast group: %s", strerror(errno));
                close(sock); fclose(fLog); exit(EXIT_FAILURE);
            }
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
    if (have_v4) sprintf(xaddr, xaddr_s, address);
    if (xaddr6_s) {
        if (xaddr6_auto) {
            /* auto: bracket the IPv6 address for a valid RFC 2732 URL */
            char tmp6[INET6_ADDRSTRLEN + 4];
            snprintf(tmp6, sizeof(tmp6), "[%s]", address6);
            snprintf(xaddr6, sizeof(xaddr6), xaddr6_s, tmp6);
        } else {
            snprintf(xaddr6, sizeof(xaddr6), xaddr6_s, address6);
        }
    }

    /* ---- Send Hello ---------------------------------------------------- */
    msg_number = 1;
    sprintf(s_tmp, "%d", msg_number);
    gen_uuid(msg_uuid);

    if (have_v4) {
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
    }

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
    if (have_v4 && xaddr6[0])
        log_info("Starting main loop (IPv4 + IPv6).");
    else if (have_v4)
        log_info("Starting main loop (IPv4 only).");
    else
        log_info("Starting main loop (IPv6 only).");
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
