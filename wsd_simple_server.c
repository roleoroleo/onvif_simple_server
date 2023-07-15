/*
 * Copyright (c) 2023 roleo.
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
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>

#include "utils.h"
#include "log.h"

#define MULTICAST_ADDRESS "239.255.255.250"
#define PORT 3702
#define TYPE "NetworkVideoTransmitter"

#define DEFAULT_LOG_FILE "/var/log/wsd_simple_server.log"
#define TEMPLATE_DIR "/etc/wsd_simple_server"

#define UUID_LEN 36
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
int exit_main;
int msg_number;
char uuid[UUID_LEN + 1];
char msg_uuid[UUID_LEN + 1];

char model[64];
char hardware[64];

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

int gen_uuid(char *g_uuid) {
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
    sprintf(template_file, "%s/Bye.xml", TEMPLATE_DIR);
    size = cat(NULL, template_file, 12,
            "%MSG_UUID%", msg_uuid,
            "%MSG_NUMBER%", s_tmp,
            "%UUID%", uuid,
            "%HARDWARE%", hardware,
            "%NAME%", model,
            "%ADDRESS%", xaddr);

    message = (char *) malloc((size + 1) * sizeof(char));
    if (message == NULL) {
        log_fatal("Malloc error.\n");
        exit(EXIT_FAILURE);
    }

    cat(message, template_file, 12,
            "%MSG_UUID%", msg_uuid,
            "%MSG_NUMBER%", s_tmp,
            "%UUID%", uuid,
            "%HARDWARE%", hardware,
            "%NAME%", model,
            "%ADDRESS%", xaddr);

    if (sendto(sock, message, strlen(message), 0, (struct sockaddr *) &addr_in, sizeof(addr_in)) < 0) {
        log_fatal("Error sending Bye message.\n");
        free(message);
        exit(EXIT_FAILURE);
    }
    free(message);
    log_info("Sent.");

    // Exit from main loop
    shutdown(sock, SHUT_RDWR);
    exit_main = 1;
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s -i INTERFACE -x XADDR [-m MODEL] [-n MANUFACTURER] -p PID_FILE [-f] [-d LEVEL]\n\n", progname);
    fprintf(stderr, "\t-i, --if_name\n");
    fprintf(stderr, "\t\tnetwork interface\n");
    fprintf(stderr, "\t-x, --xaddr\n");
    fprintf(stderr, "\t\tresource address\n");
    fprintf(stderr, "\t-m, --model\n");
    fprintf(stderr, "\t\tmodel name\n");
    fprintf(stderr, "\t-n, --hardware\n");
    fprintf(stderr, "\t\thardware manufacturer\n");
    fprintf(stderr, "\t-p, --pid_file\n");
    fprintf(stderr, "\t\tpid file\n");
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

    struct ip_mreq mr;
    long size;
    char recv_buffer[RECV_BUFFER_LEN];

    if_name = NULL;
    pid_file = NULL;
    xaddr_s = NULL;
    strcpy(model, "MODEL_NAME");
    strcpy(hardware, "HARDWARE_MANUFACTURER");
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
            {"foreground",  no_argument, 0, 'f'},
            {"debug",  required_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "i:p:x:m:n:fd:h",
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

    if (if_name == NULL) {
        print_usage(argv[0]);
        exit(EXIT_SUCCESS);
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

    log_debug("Argument if_name = %s", if_name);
    log_debug("Argument pid_file = %s", pid_file);
    log_debug("Argument xaddr = %s", xaddr_s);

    // Checking pid file
    if (check_pid(pid_file) == 1) {
        log_fatal("Program is already running.\n");
        exit(EXIT_FAILURE);
    }
    if (create_pid(pid_file) < 0) {
        log_fatal("Error creating pid file %s\n", pid_file);
        exit(EXIT_FAILURE);
    }

    srand((unsigned int) time(NULL));

    // Configure socket
    memset(&addr_in, '\0', sizeof(addr_in));
    addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(PORT);

    get_ip_address(address, netmask, if_name);
    log_debug("Address = %s", address);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        log_fatal("Unable to create socket.\n");
        exit(EXIT_FAILURE);
    }

    if (bind(sock, (struct sockaddr *) &addr_in, sizeof(addr_in)) == -1) {
        log_fatal("Unable to bind socket\n");
        exit(EXIT_FAILURE);
    }

    mr.imr_multiaddr.s_addr = inet_addr(MULTICAST_ADDRESS);
    mr.imr_interface.s_addr = inet_addr(address);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mr, sizeof(mr)) == -1) {
        log_fatal("Error joining multicast group\n");
        exit(EXIT_FAILURE);
    }

    // Prepare Hello message
    msg_number = 1;
    sprintf(s_tmp, "%d", msg_number);
    gen_uuid(uuid);
    gen_uuid(msg_uuid);

    str_subst(xaddr, xaddr_s, "%s", address);

    // Send Hello message
    log_info("Sending Hello message.");
    sprintf(template_file, "%s/Hello.xml", TEMPLATE_DIR);
    size = cat(NULL, template_file, 12,
            "%MSG_UUID%", msg_uuid,
            "%MSG_NUMBER%", s_tmp,
            "%UUID%", uuid,
            "%HARDWARE%", hardware,
            "%NAME%", model,
            "%ADDRESS%", xaddr);

    message = (char *) malloc((size + 1) * sizeof(char));
    if (message == NULL) {
        log_fatal("Malloc error.\n");
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
        log_fatal("Error sending Hello message.\n");
        free(message);
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
        if (recvfrom(sock, recv_buffer, RECV_BUFFER_LEN, 0, (struct sockaddr *) &addr_in, &addr_len) >= 0) {

            // Check if the message is a response
            if ((strstr(recv_buffer, TYPE) != NULL) && (strstr(recv_buffer, "XAddrs") == NULL)) {

                char *sm, *relates_to_uuid;
                int sm_size;

                log_debug("%s", recv_buffer);

                // Prepare ProbeMatches message
                msg_number++;
                sprintf(s_tmp, "%d", msg_number);
                gen_uuid(msg_uuid);
                relates_to_uuid = NULL;

                sm = get_element(&sm_size, recv_buffer, "MessageID");
                if (sm == NULL) {
                    log_error("Cannot find MessageID.\n");
                    continue;
                }
                relates_to_uuid = (char *) malloc((sm_size + 1) * sizeof(char));
                strncpy(relates_to_uuid, sm, sm_size);
                relates_to_uuid[sm_size] = '\0';

                // Send ProbeMatches message
                log_info("Sending ProbeMatches message.");
                sprintf(template_file, "%s/ProbeMatches.xml", TEMPLATE_DIR);
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
                    log_error("Malloc error.\n");
                    free(relates_to_uuid);
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
                    log_error("Error sending ProbeMatches message.\n");
                    free(message_loop);
                    free(relates_to_uuid);
                    continue;
                }
                free(message_loop);
                free(relates_to_uuid);
                log_info("Sent.");
            }
        }
    }

    unlink(pid_file);
    log_info("Terminating program.");

    return 0;
}
