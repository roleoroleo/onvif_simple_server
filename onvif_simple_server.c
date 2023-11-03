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

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>

#include "onvif_simple_server.h"
#include "device_service.h"
#include "media_service.h"
#include "ptz_service.h"
#include "ezxml_wrapper.h"
#include "utils.h"
#include "log.h"

#define DEFAULT_CONF_FILE "/etc/onvif_simple_server.conf"
#define DEFAULT_LOG_FILE "/var/log/onvif_simple_server.log"
#define DEBUG_FILE "/tmp/onvif_simple_server.debug"

service_context_t service_ctx;

FILE *fLog;
int debug = 5;

void dump_env()
{
    log_debug("Dump environment variables");
    log_debug("AUTH_TYPE: %s", getenv("AUTH_TYPE"));
    log_debug("CONTENT_LENGTH: %s", getenv("CONTENT_LENGTH"));
    log_debug("CONTENT_TYPE: %s", getenv("CONTENT_TYPE"));
    log_debug("DOCUMENT_ROOT: %s", getenv("DOCUMENT_ROOT"));
    log_debug("GATEWAY_INTERFACE: %s", getenv("GATEWAY_INTERFACE"));
    log_debug("HTTP_ACCEPT: %s", getenv("HTTP_ACCEPT"));
    log_debug("HTTP_COOKIE: %s", getenv("HTTP_COOKIE"));
    log_debug("HTTP_FROM: %s", getenv("HTTP_FROM"));
    log_debug("HTTP_REFERER: %s", getenv("HTTP_REFERER"));
    log_debug("HTTP_USER_AGENT: %s", getenv("HTTP_USER_AGENT"));
    log_debug("PATH_INFO: %s", getenv("PATH_INFO"));
    log_debug("PATH_TRANSLATED: %s", getenv("PATH_TRANSLATED"));
    log_debug("QUERY_STRING: %s", getenv("QUERY_STRING"));
    log_debug("REMOTE_ADDR: %s", getenv("REMOTE_ADDR"));
    log_debug("REMOTE_HOST: %s", getenv("REMOTE_HOST"));
    log_debug("REMOTE_PORT: %s", getenv("REMOTE_PORT"));
    log_debug("REMOTE_IDENT: %s", getenv("REMOTE_IDENT"));
    log_debug("REMOTE_USER: %s", getenv("REMOTE_USER"));
    log_debug("REQUEST_METHOD: %s", getenv("REQUEST_METHOD"));
    log_debug("REQUEST_URI: %s", getenv("REQUEST_URI"));
    log_debug("SCRIPT_FILENAME: %s", getenv("SCRIPT_FILENAME"));
    log_debug("SCRIPT_NAME: %s", getenv("SCRIPT_NAME"));
    log_debug("SERVER_NAME: %s", getenv("SERVER_NAME"));
    log_debug("SERVER_PORT: %s", getenv("SERVER_PORT"));
    log_debug("SERVER_PROTOCOL: %s", getenv("SERVER_PROTOCOL"));
    log_debug("SERVER_SOFTWARE: %s\n", getenv("SERVER_SOFTWARE"));
}

int processing_conf_file(char *file)
{
    FILE *fF;
    char line[MAX_LEN];
    char param[MAX_LEN];
    char value[MAX_LEN];

    int i, errno;
    char *endptr;

    fF = fopen(file, "r");
    if(fF == NULL)
        return -1;

    service_ctx.port = 80;
    service_ctx.user = NULL;
    service_ctx.password = NULL;
    service_ctx.manufacturer = NULL;
    service_ctx.model = NULL;
    service_ctx.firmware_ver = NULL;
    service_ctx.serial_num = NULL;
    service_ctx.hardware_id = NULL;
    service_ctx.ifs = NULL;
    service_ctx.profiles_num = 0;
    service_ctx.scopes_num = 0;
    service_ctx.ptz_node.enable = 0;

    while(fgets(line, MAX_LEN, fF)) {
        char *first = line;
        char *second;
        unsigned int line_len;

        memset(param, '\0', sizeof(param));
        memset(value, '\0', sizeof(value));

        for (i = strlen(line) - 1; i >= 0; i--) {
            if ((line[i] == '\r') || (line[i] == '\n'))
                line[i] = '\0';
        }
        line_len = strlen(line);
        if (line_len == 0) {
            continue;
        }
        if (line[0] == '#') {
            continue;
        }

        second = strchr(line, '=');
        //first has index of start of token
        //second has index of end of token + 1;
        if(second == NULL) {
            log_error("Wrong option: %s", line);
            return -1;
        }
        strncpy(param, first, second - first);
        if (second == &line[line_len - 1]) {
            value[0] = '\0';
        } else {
            strncpy(value, second + 1, line_len - (second - first) - 1);
        }
        if ((value[0] != '\0') && (value[0] == '"') && (value[strlen(value) - 1] == '"')) {
            strncpy(value, &value[1], strlen(value) - 2);
        }
        log_debug("%s: %s", param, value);

        //ONVIF Service options (context)
        if (strcasecmp(param, "port") == 0) {
            errno = 0;    /* To distinguish success/failure after call */
            service_ctx.port = strtol(value, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (service_ctx.port == LONG_MAX || service_ctx.port == LONG_MIN)) || (errno != 0 && service_ctx.port == 0)) {
                log_error("Wrong option: %s", line);
                return -1;
            }
            if (endptr == value) {
                log_error("Wrong option: %s", line);
                return -1;
            }
        } else if (strcasecmp(param, "user") == 0) {
            service_ctx.user = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.user, value);
        } else if (strcasecmp(param, "password") == 0) {
            service_ctx.password = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.password, value);
        } else if (strcasecmp(param, "manufacturer") == 0) {
            service_ctx.manufacturer = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.manufacturer, value);
        } else if (strcasecmp(param, "model") == 0) {
            service_ctx.model = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.model, value);
        } else if (strcasecmp(param, "firmware_ver") == 0) {
            service_ctx.firmware_ver= (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.firmware_ver, value);
        } else if (strcasecmp(param, "serial_num") == 0) {
            service_ctx.serial_num = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.serial_num, value);
        } else if (strcasecmp(param, "hardware_id") == 0) {
            service_ctx.hardware_id = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.hardware_id, value);
        } else if (strcasecmp(param, "scope") == 0) {
            service_ctx.scopes_num++;
            service_ctx.scopes = (char **) realloc(service_ctx.scopes, service_ctx.scopes_num * sizeof(char *));
            service_ctx.scopes[service_ctx.scopes_num - 1] = (char *) malloc((strlen(value) + 1) * sizeof(char));
            strcpy(service_ctx.scopes[service_ctx.scopes_num - 1], value);
        } else if (strcasecmp(param, "ifs") == 0) {
            service_ctx.ifs = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ifs, value);

        //Media Profile for ONVIF Media Service
        } else if (strcasecmp(param, "name") == 0) {
            service_ctx.profiles_num++;
            service_ctx.profiles = (stream_profile_t *) realloc(service_ctx.profiles, service_ctx.profiles_num * sizeof(stream_profile_t));

            service_ctx.profiles[service_ctx.profiles_num - 1].name = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.profiles[service_ctx.profiles_num - 1].name, value);
        } else if (strcasecmp(param, "width") == 0) {
            errno = 0;
            service_ctx.profiles[service_ctx.profiles_num - 1].width = strtol(value, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (service_ctx.profiles[service_ctx.profiles_num - 1].width == LONG_MAX || service_ctx.profiles[service_ctx.profiles_num - 1].width == LONG_MIN)) || (errno != 0 && service_ctx.profiles[service_ctx.profiles_num - 1].width == 0)) {
                log_error("Wrong option: %s", line);
                return -1;
            }
            if (endptr == value) {
                log_error("Wrong option: %s", line);
                return -1;
            }
        } else if (strcasecmp(param, "height") == 0) {
            errno = 0;
            service_ctx.profiles[service_ctx.profiles_num - 1].height = strtol(value, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (service_ctx.profiles[service_ctx.profiles_num - 1].height == LONG_MAX || service_ctx.profiles[service_ctx.profiles_num - 1].height == LONG_MIN)) || (errno != 0 && service_ctx.profiles[service_ctx.profiles_num - 1].height == 0)) {
                log_error("Wrong option: %s", line);
                return -1;
            }
            if (endptr == value) {
                log_error("Wrong option: %s", line);
                return -1;
            }
        } else if (strcasecmp(param, "url") == 0) {
            service_ctx.profiles[service_ctx.profiles_num - 1].url = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.profiles[service_ctx.profiles_num - 1].url, value);
        } else if (strcasecmp(param, "snapurl") == 0) {
            service_ctx.profiles[service_ctx.profiles_num - 1].snapurl = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.profiles[service_ctx.profiles_num - 1].snapurl, value);
        } else if (strcasecmp(param, "type") == 0) {
            if (strcasecmp(value, "JPEG") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].type = JPEG;
            else if (strcasecmp(value, "MPEG4") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].type = MPEG4;
            else if (strcasecmp(value, "H264") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].type = H264;

        //PTZ Profile for ONVIF PTZ Service
        } else if (strcasecmp(param, "ptz") == 0) {
            service_ctx.ptz_node.enable = 1;
        } else if (strcasecmp(param, "move_left") == 0) {
            service_ctx.ptz_node.move_left = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.move_left, value);
        } else if (strcasecmp(param, "move_right") == 0) {
            service_ctx.ptz_node.move_right = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.move_right, value);
        } else if (strcasecmp(param, "move_up") == 0) {
            service_ctx.ptz_node.move_up = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.move_up, value);
        } else if (strcasecmp(param, "move_down") == 0) {
            service_ctx.ptz_node.move_down = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.move_down, value);
        } else if (strcasecmp(param, "move_stop") == 0) {
            service_ctx.ptz_node.move_stop = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.move_stop, value);
        } else if (strcasecmp(param, "move_preset") == 0) {
            service_ctx.ptz_node.move_preset = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.move_preset, value);
        } else if (strcasecmp(param, "set_preset") == 0) {
            service_ctx.ptz_node.set_preset = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.set_preset, value);
        } else if (strcasecmp(param, "set_home_position") == 0) {
            service_ctx.ptz_node.set_home_position = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.set_home_position, value);
        } else if (strcasecmp(param, "remove_preset") == 0) {
            service_ctx.ptz_node.remove_preset = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.remove_preset, value);
        } else {
            log_warn("Unrecognized option: %s", line);
        }
    }
}

int check_debug_file(char *file_name)
{
    FILE *f;
    int debug_value = -1;
    char debug_buffer[4];

    f = fopen(file_name, "r");
    if (f == NULL)
        return -1;

    if (fgets(debug_buffer, 4, f) == NULL) {
        fclose(f);
        return -1;
    }
    fclose(f);

    if (sscanf(debug_buffer, "%d", &debug_value) != 1) {
        return -1;
    }

    return debug_value;
}

void rotate_log()
{
    char tmp_log_file_1[256];
    char tmp_log_file_2[256];
    char *p1, *p2;

    sprintf(tmp_log_file_1, DEFAULT_LOG_FILE);
    sprintf(tmp_log_file_2, DEFAULT_LOG_FILE);
    p1 = strrchr(tmp_log_file_1, '.');
    p1++;
    strcpy(p1, "1.log");
    p2 = strrchr(tmp_log_file_2, '.');
    p2++;
    strcpy(p2, "2.log");
    remove(tmp_log_file_2);
    rename(tmp_log_file_1, tmp_log_file_2);
    rename(DEFAULT_LOG_FILE, tmp_log_file_1);
}

void print_conf_help()
{
    fprintf(stderr, "\nCreate a configuration file with the following parameters:\n\n");
    fprintf(stderr, "\tport\n");
    fprintf(stderr, "\tuser\n");
    fprintf(stderr, "\tpassword\n");
    fprintf(stderr, "\tmanufacturer\n");
    fprintf(stderr, "\tmodel\n");
    fprintf(stderr, "\tfirmware_ver\n");
    fprintf(stderr, "\tserial_num\n");
    fprintf(stderr, "\thardware_id\n");
    fprintf(stderr, "\tscope\n");
    fprintf(stderr, "\tifs\n");
    fprintf(stderr, "\tname\n");
    fprintf(stderr, "\twidth\n");
    fprintf(stderr, "\theight\n");
    fprintf(stderr, "\turl\n");
    fprintf(stderr, "\tsnapurl\n");
    fprintf(stderr, "\ttype\n");
    fprintf(stderr, "\tptz\n");
    fprintf(stderr, "\tmove_left\n");
    fprintf(stderr, "\tmove_right\n");
    fprintf(stderr, "\tmove_up\n");
    fprintf(stderr, "\tmove_down\n");
    fprintf(stderr, "\tmove_stop\n");
    fprintf(stderr, "\tmove_preset\n");

    fprintf(stderr, "\nExample:\n");
    fprintf(stderr, "\tmodel=Yi Hack\n");
    fprintf(stderr, "\tmanufacturer=Yi\n");
    fprintf(stderr, "\tfirmware_ver=x.y.z\n");
    fprintf(stderr, "\thardware_id=AFUS\n");
    fprintf(stderr, "\tserial_num=AFUSY12ABCDE3F456789\n");
    fprintf(stderr, "\tifs=wlan0\n");
    fprintf(stderr, "\tport=80\n");
    fprintf(stderr, "\tscope=onvif://www.onvif.org/Profile/Streaming\n");
    fprintf(stderr, "\tuser=\n");
    fprintf(stderr, "\tpassword=\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t#Profile 0\n");
    fprintf(stderr, "\tname=Profile_0\n");
    fprintf(stderr, "\twidth=1920\n");
    fprintf(stderr, "\theight=1080\n");
    fprintf(stderr, "\turl=rtsp://%%s/ch0_0.h264\n");
    fprintf(stderr, "\tsnapurl=http://%%s/cgi-bin/snapshot.sh?res=high&watermark=yes\n");
    fprintf(stderr, "\ttype=H264\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t#Profile 1\n");
    fprintf(stderr, "\tname=Profile_1\n");
    fprintf(stderr, "\twidth=640\n");
    fprintf(stderr, "\theight=360\n");
    fprintf(stderr, "\turl=rtsp://%%s/ch0_1.h264\n");
    fprintf(stderr, "\tsnapurl=http://%%s/cgi-bin/snapshot.sh?res=low&watermark=yes\n");
    fprintf(stderr, "\ttype=H264\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t#PTZ\n");
    fprintf(stderr, "\tptz=1\n");
    fprintf(stderr, "\tmove_left=/tmp/sd/yi-hack/bin/ipc_cmd -M left\n");
    fprintf(stderr, "\tmove_right=/tmp/sd/yi-hack/bin/ipc_cmd -M right\n");
    fprintf(stderr, "\tmove_up=/tmp/sd/yi-hack/bin/ipc_cmd -m up\n");
    fprintf(stderr, "\tmove_down=/tmp/sd/yi-hack/bin/ipc_cmd -m down\n");
    fprintf(stderr, "\tmove_stop=/tmp/sd/yi-hack/bin/ipc_cmd -M stop\n");
    fprintf(stderr, "\tmove_preset=/tmp/sd/yi-hack/bin/ipc_cmd -p %%t\n");
    fprintf(stderr, "\tset_preset=/tmp/sd/yi-hack/bin/ipc_cmd -P %%t\n");
    fprintf(stderr, "\tset_home_position=/tmp/sd/yi-hack/bin/ipc_cmd -H\n");
    fprintf(stderr, "\tremove_preset=/tmp/sd/yi-hack/bin/ipc_cmd -R %%t\n");
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [-c CONF_FILE] [-d] [-f]\n\n", progname);
    fprintf(stderr, "\t-c CONF_FILE, --conf_file CONF_FILE\n");
    fprintf(stderr, "\t\tpath of the configuration file\n");
    fprintf(stderr, "\t-d LEVEL, --debug LEVEL\n");
    fprintf(stderr, "\t\tenable debug with LEVEL = 0..5 (default 0 = fatal errors)\n");
    fprintf(stderr, "\t-f, --conf_help\n");
    fprintf(stderr, "\t\tprint the help for the configuration file\n");
    fprintf(stderr, "\t-h, --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

int main(int argc, char ** argv)
{
    char *tmp;
    int errno;
    char *endptr;
    int c, ret, i, itmp;
    char *conf_file;
    char *prog_name;
    const char *method;
    username_token_t security;
    int auth_error = 0;

    // Set default log file and conf file
    rotate_log();
    fLog = fopen(DEFAULT_LOG_FILE, "w");
    conf_file = (char *) malloc((strlen(DEFAULT_CONF_FILE) + 1) * sizeof(char));
    strcpy(conf_file, DEFAULT_CONF_FILE);

    while (1) {
        static struct option long_options[] =
        {
            {"conf_file",  required_argument, 0, 'c'},
            {"debug",  required_argument, 0, 'd'},
            {"conf_help",  no_argument, 0, 'f'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "c:d:fh",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {

        case 'c':
            /* Check for various possible errors */
            if (strlen(optarg) < MAX_LEN - 1) {
                free(conf_file);
                conf_file = (char *) malloc((strlen(optarg) + 1) * sizeof(char));
                strcpy(conf_file, optarg);
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'd':
            debug = strtol(optarg, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (debug == LONG_MAX || debug == LONG_MIN)) || (errno != 0 && debug == 0)) {
                print_usage(argv[0]);
                free(conf_file);
                exit(EXIT_FAILURE);
            }
            if (endptr == optarg) {
                print_usage(argv[0]);
                free(conf_file);
                exit(EXIT_FAILURE);
            }

            if ((debug < LOG_TRACE) || (debug > LOG_FATAL)) {
                print_usage(argv[0]);
                free(conf_file);
                exit(EXIT_FAILURE);
            }
            debug = 5 - debug;
            break;

        case 'f':
            print_conf_help();
            free(conf_file);
            exit(EXIT_SUCCESS);
            break;

        case 'h':
            print_usage(argv[0]);
            free(conf_file);
            exit(EXIT_SUCCESS);
            break;

        case '?':
            /* getopt_long already printed an error message. */
            break;

        default:
            print_usage(argv[0]);
            free(conf_file);
            exit(EXIT_SUCCESS);
        }
    }

    // Check if the service name is sent as a last argument
    if (argc > 1) {
        tmp = argv[argc - 1];
        if ((strstr(tmp, "device_service") != NULL) ||
                (strstr(tmp, "media_service") != NULL) ||
                (strstr(tmp, "ptz_service") != NULL)) {
            tmp = argv[argc - 1];
        } else {
            tmp = argv[0];
        }
    } else {
        tmp = argv[0];
    }
    prog_name = basename(tmp);

    if (conf_file[0] == '\0') {
        print_usage(argv[0]);
        free(conf_file);
        exit(EXIT_SUCCESS);
    }

    int debug2 = check_debug_file(DEBUG_FILE);
    if (debug2 != -1) debug = debug2 - 5;

    log_add_fp(fLog, debug);
    log_set_level(debug);
    log_set_quiet(1);
    log_info("Starting program.");

    dump_env();

    log_info("Processing configuration file %s...", conf_file);
    if (processing_conf_file(conf_file) == -1) {
        log_fatal("Unable to find configuration file %s", conf_file);
        fclose(fLog);
        free(conf_file);
        exit(EXIT_FAILURE);
    } else if (processing_conf_file(conf_file) < -1) {
        log_fatal("Wrong syntax in configuration file %s", conf_file);
        fclose(fLog);
        free(conf_file);
        exit(EXIT_FAILURE);
    }
    log_info("Completed.");

    int input_size;
    char *input = (char *) malloc (16 * 1024 * sizeof(char));
    if (input == NULL) {
        log_fatal("Memory error");
        fclose(fLog);
        free(conf_file);
        exit(EXIT_FAILURE);
    }
    input_size = fread(input, 1, 16 * 1024 * sizeof(char), stdin);
    if (realloc(input, input_size * sizeof(char)) == NULL) {
        log_fatal("Memory error");
        free(input);
        fclose(fLog);
        free(conf_file);
        exit(EXIT_FAILURE);
    }
    log_debug("Input:\n%s", input);
    log_debug("Url: %s", prog_name);

    init_xml(input, input_size);
    method = get_method(1);
    if (method == NULL) {
        log_fatal("XML parsing error");
        close_xml();
        free(input);
        fclose(fLog);
        free(conf_file);
        exit(EXIT_FAILURE);
    }

    log_debug("Method: %s", method);

    if (service_ctx.user != NULL) {
        if ((strstr(input, "Header") != NULL) && (strstr(input, "Security") != NULL) && (strstr(input, "UsernameToken") != NULL)) {
            int element_size;
            const char *element;
            unsigned long nonce_size = 128;
            unsigned long auth_size = 128;
            unsigned long sha1_size = 20;
            unsigned long digest_size = 128;
            unsigned char nonce[128];
            unsigned char auth[128];
            char sha1[20];
            char digest[128];

            security.enable = 1;
            element = get_element("Username", "Header");
            if (element != NULL) {
                security.username = (char *) malloc((element_size + 1) * sizeof(char));
                log_info("Security: username = %s", security.username);
            }
            element = get_element("Password", "Header");
            if (element != NULL) {
                security.password = (char *) malloc((element_size + 1) * sizeof(char));
                log_info("Security: password = %s", security.password);
            }
            element = get_element("Nonce", "Header");
            if (element != NULL) {
                security.nonce = (char *) malloc((element_size + 1) * sizeof(char));
                log_info("Security: nonce = %s", security.nonce);
                b64_decode(security.nonce, strlen(security.nonce), nonce, &nonce_size);
            }
            element = get_element("Created", "Header");
            if (element != NULL) {
                security.created = (char *) malloc((element_size + 1) * sizeof(char));
                log_info("Security: created = %s", security.created);
            }

            // Calculate digest and check the password
            // Digest = B64ENCODE( SHA1( B64DECODE( Nonce ) + Date + Password ) )
            memcpy(auth, nonce, nonce_size);
            memcpy(&auth[nonce_size], security.created, strlen(security.created));
            memcpy(&auth[nonce_size + strlen(security.created)], service_ctx.password, strlen(service_ctx.password));
            auth_size = nonce_size + strlen(security.created) + strlen(service_ctx.password);
            hashSHA1(auth, auth_size, sha1, sha1_size);
            b64_encode(sha1, sha1_size, digest, &digest_size);
            log_debug("Calculated digest: %s", digest);
            log_debug("Received digest: %s", security.password);

            if ((strcmp(service_ctx.user, security.username) != 0) ||
                    (strcmp(security.password, digest) != 0)) {

                auth_error = 1;
            }
        } else {
            auth_error = 1;
        }
    } else {
        security.enable = 0;
    }

    if ((strcmp("GetSystemDateAndTime", method) == 0) || (strcmp("GetUsers", method) == 0)) {
        auth_error = 0;
    }

    if (auth_error == 0) {

        if (strcasecmp("device_service", prog_name) == 0) {
            if (strcasecmp(method, "GetServices") == 0) {
                device_get_services(input);
            } else if (strcasecmp(method, "GetServiceCapabilities") == 0) {
                device_get_service_capabilities();
            } else if (strcasecmp(method, "GetDeviceInformation") == 0) {
                device_get_device_information();
            } else if (strcasecmp(method, "GetSystemDateAndTime") == 0) {
                device_get_system_date_and_time();
            } else if (strcasecmp(method, "SystemReboot") == 0) {
                device_system_reboot();
            } else if (strcasecmp(method, "GetScopes") == 0) {
                device_get_scopes();
            } else if (strcasecmp(method, "GetUsers") == 0) {
                device_get_users();
            } else if (strcasecmp(method, "GetWsdlUrl") == 0) {
                device_get_wsdl_url();
            } else if (strcasecmp(method, "GetCapabilities") == 0) {
                device_get_capabilities(input);
            } else if (strcasecmp(method, "GetNetworkInterfaces") == 0) {
                device_get_network_interfaces();
            } else {
                device_unsupported(method);
            }
        } else if (strcasecmp("media_service", prog_name) == 0) {
            if (strcasecmp(method, "GetServiceCapabilities") == 0) {
                media_get_service_capabilities();
            } else if (strcasecmp(method, "GetVideoSources") == 0) {
                media_get_video_sources();
            } else if (strcasecmp(method, "GetVideoSourceConfigurations") == 0) {
                media_get_video_source_configurations();
            } else if (strcasecmp(method, "GetVideoSourceConfiguration") == 0) {
                media_get_video_source_configuration();
            } else if (strcasecmp(method, "GetCompatibleVideoSourceConfigurations") == 0) {
                media_get_compatible_video_source_configurations();
            } else if (strcasecmp(method, "GetVideoSourceConfigurationOptions") == 0) {
                media_get_video_source_configuration_options();
            } else if (strcasecmp(method, "GetProfiles") == 0) {
                media_get_profiles();
            } else if (strcasecmp(method, "GetProfile") == 0) {
                media_get_profile(input);
            } else if (strcasecmp(method, "GetVideoEncoderConfigurations") == 0) {
                media_get_video_encoder_configurations();
            } else if (strcasecmp(method, "GetVideoEncoderConfiguration") == 0) {
                media_get_video_encoder_configuration(input);
            } else if (strcasecmp(method, "GetCompatibleVideoEncoderConfigurations") == 0) {
                media_get_compatible_video_encoder_configurations(input);
            } else if (strcasecmp(method, "GetVideoEncoderConfigurationOptions") == 0) {
                media_get_video_encoder_configuration_options(input);
            } else if (strcasecmp(method, "GetSnapshotUri") == 0) {
                media_get_snapshot_uri(input);
            } else if (strcasecmp(method, "GetStreamUri") == 0) {
                media_get_stream_uri(input);
            } else {
                media_unsupported(method);
            }
        } else if (strcasecmp("ptz_service", prog_name) == 0) {
            if (strcasecmp(method, "GetServiceCapabilities") == 0) {
                ptz_get_service_capabilities();
            } else if (strcasecmp(method, "GetConfigurations") == 0) {
                ptz_get_configurations();
            } else if (strcasecmp(method, "GetConfiguration") == 0) {
                ptz_get_configuration();
            } else if (strcasecmp(method, "GetConfigurationOptions") == 0) {
                ptz_get_configuration_options();
            } else if (strcasecmp(method, "GetNodes") == 0) {
                ptz_get_nodes();
            } else if (strcasecmp(method, "GetNode") == 0) {
                ptz_get_node();
            } else if (strcasecmp(method, "GetPresets") == 0) {
                ptz_get_presets();
            } else if (strcasecmp(method, "GotoPreset") == 0) {
                ptz_goto_preset(input);
            } else if (strcasecmp(method, "GotoHomePosition") == 0) {
                ptz_goto_home_position();
            } else if (strcasecmp(method, "ContinuousMove") == 0) {
                ptz_continuous_move(input);
            } else if (strcasecmp(method, "RelativeMove") == 0) {
                ptz_relative_move(input);
            } else if (strcasecmp(method, "Stop") == 0) {
                ptz_stop();
            } else if (strcasecmp(method, "GetStatus") == 0) {
                ptz_get_status();
            } else if (strcasecmp(method, "SetPreset") == 0) {
                ptz_set_preset();
            } else if (strcasecmp(method, "SetHomePosition") == 0) {
                ptz_set_home_position();
            } else if (strcasecmp(method, "RemovePreset") == 0) {
                ptz_remove_preset();
            } else {
                ptz_unsupported(method);
            }
        }
    } else {
        device_authentication_error();
    }

    if (security.enable == 1) {
        free(security.username);
        free(security.password);
        free(security.nonce);
        free(security.created);
    }

    close_xml();
    free(input);

    free(service_ctx.user);
    free(service_ctx.password);
    free(service_ctx.manufacturer);
    free(service_ctx.model);
    free(service_ctx.firmware_ver);
    free(service_ctx.serial_num);
    free(service_ctx.hardware_id);
    for (i = service_ctx.scopes_num - 1; i >= 0; i--) {
        free(service_ctx.scopes[i]);
    }
    free(service_ctx.scopes);
    free(service_ctx.ifs);
    for (i = service_ctx.profiles_num - 1; i >= 0; i--) {
        free(service_ctx.profiles[i].name);
        free(service_ctx.profiles[i].url);
        free(service_ctx.profiles[i].snapurl);
    }
    free(service_ctx.profiles);
    free(service_ctx.ptz_node.move_left);
    free(service_ctx.ptz_node.move_right);
    free(service_ctx.ptz_node.move_up);
    free(service_ctx.ptz_node.move_down);
    free(service_ctx.ptz_node.move_stop);
    free(service_ctx.ptz_node.move_preset);
    free(service_ctx.ptz_node.set_preset);
    free(service_ctx.ptz_node.set_home_position);
    free(service_ctx.ptz_node.remove_preset);

    free(conf_file);

    log_info("Program terminated.");
    fclose(fLog);

    return 0;
}
