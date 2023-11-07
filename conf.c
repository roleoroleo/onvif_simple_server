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
#include <limits.h>
#include <errno.h>

#include "conf.h"
#include "utils.h"
#include "log.h"
#include "onvif_simple_server.h"

extern service_context_t service_ctx;

int process_conf_file(char *file)
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
    service_ctx.profiles = NULL;
    service_ctx.profiles_num = 0;
    service_ctx.scopes = NULL;
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
        } else if ((strcasecmp(param, "ptz") == 0) && (strcasecmp(value, "1") == 0)) {
            service_ctx.ptz_node.enable = 1;
            service_ctx.ptz_node.move_left = NULL;
            service_ctx.ptz_node.move_right = NULL;
            service_ctx.ptz_node.move_up = NULL;
            service_ctx.ptz_node.move_down = NULL;
            service_ctx.ptz_node.move_stop = NULL;
            service_ctx.ptz_node.move_preset = NULL;
            service_ctx.ptz_node.set_preset = NULL;
            service_ctx.ptz_node.set_home_position = NULL;
            service_ctx.ptz_node.remove_preset = NULL;
        } else if ((strcasecmp(param, "move_left") == 0) && (service_ctx.ptz_node.enable == 1)) {
            service_ctx.ptz_node.move_left = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.move_left, value);
        } else if ((strcasecmp(param, "move_right") == 0) && (service_ctx.ptz_node.enable == 1)) {
            service_ctx.ptz_node.move_right = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.move_right, value);
        } else if ((strcasecmp(param, "move_up") == 0) && (service_ctx.ptz_node.enable == 1)) {
            service_ctx.ptz_node.move_up = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.move_up, value);
        } else if ((strcasecmp(param, "move_down") == 0) && (service_ctx.ptz_node.enable == 1)) {
            service_ctx.ptz_node.move_down = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.move_down, value);
        } else if ((strcasecmp(param, "move_stop") == 0) && (service_ctx.ptz_node.enable == 1)) {
            service_ctx.ptz_node.move_stop = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.move_stop, value);
        } else if ((strcasecmp(param, "move_preset") == 0) && (service_ctx.ptz_node.enable == 1)) {
            service_ctx.ptz_node.move_preset = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.move_preset, value);
        } else if ((strcasecmp(param, "set_preset") == 0) && (service_ctx.ptz_node.enable == 1)) {
            service_ctx.ptz_node.set_preset = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.set_preset, value);
        } else if ((strcasecmp(param, "set_home_position") == 0) && (service_ctx.ptz_node.enable == 1)) {
            service_ctx.ptz_node.set_home_position = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.set_home_position, value);
        } else if ((strcasecmp(param, "remove_preset") == 0) && (service_ctx.ptz_node.enable == 1)) {
            service_ctx.ptz_node.remove_preset = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.ptz_node.remove_preset, value);

        } else {
            log_warn("Unrecognized option: %s", line);
        }
    }
}

void free_conf_file()
{
    int i;

    if (service_ctx.ptz_node.enable == 1) {
        if (service_ctx.ptz_node.remove_preset != NULL) free(service_ctx.ptz_node.remove_preset);
        if (service_ctx.ptz_node.set_home_position != NULL) free(service_ctx.ptz_node.set_home_position);
        if (service_ctx.ptz_node.set_preset != NULL) free(service_ctx.ptz_node.set_preset);
        if (service_ctx.ptz_node.move_preset != NULL) free(service_ctx.ptz_node.move_preset);
        if (service_ctx.ptz_node.move_stop != NULL) free(service_ctx.ptz_node.move_stop);
        if (service_ctx.ptz_node.move_down != NULL) free(service_ctx.ptz_node.move_down);
        if (service_ctx.ptz_node.move_up != NULL) free(service_ctx.ptz_node.move_up);
        if (service_ctx.ptz_node.move_right != NULL) free(service_ctx.ptz_node.move_right);
        if (service_ctx.ptz_node.move_left != NULL) free(service_ctx.ptz_node.move_left);
    }

    for (i = service_ctx.profiles_num - 1; i >= 0; i--) {
        free(service_ctx.profiles[i].snapurl);
        free(service_ctx.profiles[i].url);
        free(service_ctx.profiles[i].name);
    }
    if (service_ctx.profiles != NULL) free(service_ctx.profiles);

    for (i = service_ctx.scopes_num - 1; i >= 0; i--) {
        free(service_ctx.scopes[i]);
    }
    if (service_ctx.scopes != NULL) free(service_ctx.scopes);

    if (service_ctx.ifs != NULL) free(service_ctx.ifs);
    if (service_ctx.hardware_id != NULL) free(service_ctx.hardware_id);
    if (service_ctx.serial_num != NULL) free(service_ctx.serial_num);
    if (service_ctx.firmware_ver != NULL) free(service_ctx.firmware_ver);
    if (service_ctx.model != NULL) free(service_ctx.model);
    if (service_ctx.manufacturer != NULL) free(service_ctx.manufacturer);
    if (service_ctx.password != NULL) free(service_ctx.password);
    if (service_ctx.user != NULL) free(service_ctx.user);
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
