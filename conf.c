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
    service_ctx.adv_enable_media2 = 0;
    service_ctx.adv_fault_if_unknown = 0;
    service_ctx.adv_fault_if_set = 0;
    service_ctx.adv_synology_nvr = 0;
    service_ctx.profiles = NULL;
    service_ctx.profiles_num = 0;
    service_ctx.scopes = NULL;
    service_ctx.scopes_num = 0;
    service_ctx.ptz_node.enable = 0;
    service_ctx.events = NULL;
    service_ctx.events_enable = EVENTS_NONE;
    service_ctx.events_num = 0;

    service_ctx.ptz_node.max_step_x = 360.0;
    service_ctx.ptz_node.max_step_y = 180.0;
    service_ctx.ptz_node.get_position = NULL;
    service_ctx.ptz_node.is_moving = NULL;
    service_ctx.ptz_node.move_left = NULL;
    service_ctx.ptz_node.move_right = NULL;
    service_ctx.ptz_node.move_up = NULL;
    service_ctx.ptz_node.move_down = NULL;
    service_ctx.ptz_node.move_stop = NULL;
    service_ctx.ptz_node.move_preset = NULL;
    service_ctx.ptz_node.goto_home_position = NULL;
    service_ctx.ptz_node.set_preset = NULL;
    service_ctx.ptz_node.set_home_position = NULL;
    service_ctx.ptz_node.remove_preset = NULL;
    service_ctx.ptz_node.jump_to_abs = NULL;
    service_ctx.ptz_node.jump_to_rel = NULL;

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
        if (second == NULL) {
            log_warn("Unrecognized option: %s", line);
            continue;
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
            if (value[0] == '\0') {
                log_warn("Wrong value %s, use default", param);
            } else {
                errno = 0;    /* To distinguish success/failure after call */
                service_ctx.port = strtol(value, &endptr, 10);

                /* Check for various possible errors */
                if ((errno == ERANGE && (service_ctx.port == LONG_MAX || service_ctx.port == LONG_MIN)) || (errno != 0 && service_ctx.port == 0)) {
                    log_error("Wrong option: %s", line);
                    return -2;
                }
                if (endptr == value) {
                    log_error("Wrong option: %s", line);
                    return -2;
                }
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
        } else if (strcasecmp(param, "adv_enable_media2") == 0) {
            if (strcasecmp(value, "1") == 0)
                service_ctx.adv_enable_media2 = 1;
        } else if (strcasecmp(param, "adv_fault_if_unknown") == 0) {
            if (strcasecmp(value, "1") == 0)
                service_ctx.adv_fault_if_unknown = 1;
        } else if (strcasecmp(param, "adv_fault_if_set") == 0) {
            if (strcasecmp(value, "1") == 0)
                service_ctx.adv_fault_if_set = 1;
        } else if (strcasecmp(param, "adv_synology_nvr") == 0) {
            if (strcasecmp(value, "1") == 0)
                service_ctx.adv_synology_nvr = 1;

        //Media Profile for ONVIF Media Service
        } else if (strcasecmp(param, "name") == 0) {
            service_ctx.profiles_num++;
            service_ctx.profiles = (stream_profile_t *) realloc(service_ctx.profiles, service_ctx.profiles_num * sizeof(stream_profile_t));

            service_ctx.profiles[service_ctx.profiles_num - 1].name = (char *) malloc(strlen(value) + 1);
            strcpy(service_ctx.profiles[service_ctx.profiles_num - 1].name, value);
            service_ctx.profiles[service_ctx.profiles_num - 1].width = 0;
            service_ctx.profiles[service_ctx.profiles_num - 1].height = 0;
            service_ctx.profiles[service_ctx.profiles_num - 1].url = NULL;
            service_ctx.profiles[service_ctx.profiles_num - 1].snapurl = NULL;
            service_ctx.profiles[service_ctx.profiles_num - 1].type = 0;
            service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = AAC;
            service_ctx.profiles[service_ctx.profiles_num - 1].audio_decoder = AUDIO_NONE;
        } else if (strcasecmp(param, "width") == 0) {
            errno = 0;
            service_ctx.profiles[service_ctx.profiles_num - 1].width = strtol(value, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (service_ctx.profiles[service_ctx.profiles_num - 1].width == LONG_MAX || service_ctx.profiles[service_ctx.profiles_num - 1].width == LONG_MIN)) || (errno != 0 && service_ctx.profiles[service_ctx.profiles_num - 1].width == 0)) {
                log_error("Wrong option: %s", line);
                return -2;
            }
            if (endptr == value) {
                log_error("Wrong option: %s", line);
                return -2;
            }
        } else if (strcasecmp(param, "height") == 0) {
            errno = 0;
            service_ctx.profiles[service_ctx.profiles_num - 1].height = strtol(value, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (service_ctx.profiles[service_ctx.profiles_num - 1].height == LONG_MAX || service_ctx.profiles[service_ctx.profiles_num - 1].height == LONG_MIN)) || (errno != 0 && service_ctx.profiles[service_ctx.profiles_num - 1].height == 0)) {
                log_error("Wrong option: %s", line);
                return -2;
            }
            if (endptr == value) {
                log_error("Wrong option: %s", line);
                return -2;
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
            else if (strcasecmp(value, "H265") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].type = H265;
        } else if (strcasecmp(param, "audio_encoder") == 0) {
            if (strcasecmp(value, "NONE") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = AUDIO_NONE;
            else if (strcasecmp(value, "G711") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = G711;
            else if (strcasecmp(value, "G726") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = G711;
            else if (strcasecmp(value, "AAC") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = AAC;
        } else if (strcasecmp(param, "audio_decoder") == 0) {
            if (strcasecmp(value, "NONE") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_decoder = AUDIO_NONE;
            else if (strcasecmp(value, "G711") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_decoder = G711;
            else if (strcasecmp(value, "G726") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_decoder = G726;
            else if (strcasecmp(value, "AAC") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_decoder = AAC;

        //PTZ Profile for ONVIF PTZ Service
        } else if (strcasecmp(param, "ptz") == 0) {
            if (strcasecmp(value, "1") == 0) {
                service_ctx.ptz_node.enable = 1;
            }
            service_ctx.ptz_node.max_step_x = 360.0;
            service_ctx.ptz_node.max_step_y = 180.0;
            service_ctx.ptz_node.get_position = NULL;
            service_ctx.ptz_node.is_moving = NULL;
            service_ctx.ptz_node.move_left = NULL;
            service_ctx.ptz_node.move_right = NULL;
            service_ctx.ptz_node.move_up = NULL;
            service_ctx.ptz_node.move_down = NULL;
            service_ctx.ptz_node.move_stop = NULL;
            service_ctx.ptz_node.move_preset = NULL;
            service_ctx.ptz_node.goto_home_position = NULL;
            service_ctx.ptz_node.set_preset = NULL;
            service_ctx.ptz_node.set_home_position = NULL;
            service_ctx.ptz_node.remove_preset = NULL;
            service_ctx.ptz_node.jump_to_abs = NULL;
            service_ctx.ptz_node.jump_to_rel = NULL;
        } else if (strcasecmp(param, "max_step_x") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (value[0] == '\0') {
                    log_warn("Empty value for %s, use default", param);
                } else {
                    errno = 0;
                    service_ctx.ptz_node.max_step_x = strtod(value, &endptr);

                    /* Check for various possible errors */
                    if (errno == ERANGE || (errno != 0 && service_ctx.ptz_node.max_step_x == 0.0)) {
                        log_error("Wrong option: %s", line);
                        return -2;
                    }
                    if (endptr == value) {
                        log_error("Wrong option: %s", line);
                        return -2;
                    }
                }
            }
        } else if (strcasecmp(param, "max_step_y") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (value[0] == '\0') {
                    log_warn("Empty value for %s, use default", param);
                } else {
                    errno = 0;
                    service_ctx.ptz_node.max_step_y = strtod(value, &endptr);

                    /* Check for various possible errors */
                    if (errno == ERANGE || (errno != 0 && service_ctx.ptz_node.max_step_y == 0.0)) {
                        log_error("Wrong option: %s", line);
                        return -2;
                    }
                    if (endptr == value) {
                        log_error("Wrong option: %s", line);
                        return -2;
                    }
                }
            }
        } else if (strcasecmp(param, "get_position") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                service_ctx.ptz_node.get_position = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.ptz_node.get_position, value);
            }
        } else if (strcasecmp(param, "is_moving") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                service_ctx.ptz_node.is_moving = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.ptz_node.is_moving, value);
            }
        } else if (strcasecmp(param, "move_left") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                service_ctx.ptz_node.move_left = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.ptz_node.move_left, value);
            }
        } else if (strcasecmp(param, "move_right") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                service_ctx.ptz_node.move_right = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.ptz_node.move_right, value);
            }
        } else if (strcasecmp(param, "move_up") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                service_ctx.ptz_node.move_up = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.ptz_node.move_up, value);
            }
        } else if (strcasecmp(param, "move_down") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                service_ctx.ptz_node.move_down = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.ptz_node.move_down, value);
            }
        } else if (strcasecmp(param, "move_stop") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                service_ctx.ptz_node.move_stop = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.ptz_node.move_stop, value);
            }
        } else if (strcasecmp(param, "move_preset") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                service_ctx.ptz_node.move_preset = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.ptz_node.move_preset, value);
            }
        } else if (strcasecmp(param, "goto_home_position") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                service_ctx.ptz_node.goto_home_position = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.ptz_node.goto_home_position, value);
            }
        } else if (strcasecmp(param, "set_preset") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                service_ctx.ptz_node.set_preset = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.ptz_node.set_preset, value);
            }
        } else if (strcasecmp(param, "set_home_position") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                service_ctx.ptz_node.set_home_position = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.ptz_node.set_home_position, value);
            }
        } else if (strcasecmp(param, "remove_preset") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                service_ctx.ptz_node.remove_preset = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.ptz_node.remove_preset, value);
            }
        } else if (strcasecmp(param, "jump_to_abs") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                service_ctx.ptz_node.jump_to_abs = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.ptz_node.jump_to_abs, value);
            }
        } else if (strcasecmp(param, "jump_to_rel") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                service_ctx.ptz_node.jump_to_rel = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.ptz_node.jump_to_rel, value);
            }
        } else if (strcasecmp(param, "get_presets") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                service_ctx.ptz_node.get_presets = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.ptz_node.get_presets, value);
            }

        //Events Profile for ONVIF Events Service
        } else if (strcasecmp(param, "events") == 0) {
            if (strcasecmp(value, "1") == 0) {
                service_ctx.events_enable = EVENTS_PULLPOINT;        // PullPoint
            } else if (strcasecmp(value, "2") == 0) {
                service_ctx.events_enable = EVENTS_BASESUBSCRIPTION; // Base Subscription
            } else if (strcasecmp(value, "3") == 0) {
                service_ctx.events_enable = EVENTS_BOTH;             // PullPoint and Base Subscription
            }
        } else if (strcasecmp(param, "topic") == 0) {
            if (service_ctx.events_enable != EVENTS_NONE) {
                service_ctx.events_num++;
                if (service_ctx.events_num >= MAX_EVENTS) {
                    log_error("Too many events, max is: %d", MAX_EVENTS);
                    return -2;
                }
                service_ctx.events = (event_t *) realloc(service_ctx.events, service_ctx.events_num * sizeof(event_t));
                service_ctx.events[service_ctx.events_num - 1].topic = NULL;
                service_ctx.events[service_ctx.events_num - 1].source_name = NULL;
                service_ctx.events[service_ctx.events_num - 1].source_value = NULL;
                service_ctx.events[service_ctx.events_num - 1].input_file = NULL;
                service_ctx.events[service_ctx.events_num - 1].topic = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.events[service_ctx.events_num - 1].topic, value);
            }
        } else if (strcasecmp(param, "source_name") == 0) {
            if (service_ctx.events_enable != EVENTS_NONE) {
                service_ctx.events[service_ctx.events_num - 1].source_name = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.events[service_ctx.events_num - 1].source_name, value);
            }
        } else if (strcasecmp(param, "source_value") == 0) {
            if (service_ctx.events_enable != EVENTS_NONE) {
                service_ctx.events[service_ctx.events_num - 1].source_value = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.events[service_ctx.events_num - 1].source_value, value);
            }
        } else if (strcasecmp(param, "input_file") == 0) {
            if (service_ctx.events_enable != EVENTS_NONE) {
                service_ctx.events[service_ctx.events_num - 1].input_file = (char *) malloc(strlen(value) + 1);
                strcpy(service_ctx.events[service_ctx.events_num - 1].input_file, value);
            }
        } else {
            log_warn("Unrecognized option: %s", line);
        }
    }

    // If a string option is NULL, set a default value
    if (service_ctx.manufacturer == NULL) {
        service_ctx.manufacturer = (char *) malloc(strlen(DEFAULT_MANUFACTURER) + 1);
        strcpy(service_ctx.manufacturer, DEFAULT_MANUFACTURER);
    }
    if (service_ctx.model == NULL) {
        service_ctx.model = (char *) malloc(strlen(DEFAULT_MODEL) + 1);
        strcpy(service_ctx.model, DEFAULT_MODEL);
    }
    if (service_ctx.firmware_ver == NULL) {
        service_ctx.firmware_ver = (char *) malloc(strlen(DEFAULT_FW_VER) + 1);
        strcpy(service_ctx.firmware_ver, DEFAULT_FW_VER);
    }
    if (service_ctx.serial_num == NULL) {
        service_ctx.serial_num = (char *) malloc(strlen(DEFAULT_SERIAL_NUM) + 1);
        strcpy(service_ctx.serial_num, DEFAULT_SERIAL_NUM);
    }
    if (service_ctx.hardware_id == NULL) {
        service_ctx.hardware_id = (char *) malloc(strlen(DEFAULT_HW_ID) + 1);
        strcpy(service_ctx.hardware_id, DEFAULT_HW_ID);
    }
    if (service_ctx.ifs == NULL) {
        service_ctx.ifs = (char *) malloc(strlen(DEFAULT_IFS) + 1);
        strcpy(service_ctx.ifs, DEFAULT_IFS);
    }
}

void free_conf_file()
{
    int i;

    if (service_ctx.events_enable == 1) {
        for (i = service_ctx.events_num - 1; i >= 0; i--) {
            if (service_ctx.events[i].input_file != NULL) free(service_ctx.events[i].input_file);
            if (service_ctx.events[i].source_value != NULL) free(service_ctx.events[i].source_value);
            if (service_ctx.events[i].source_name != NULL) free(service_ctx.events[i].source_name);
            if (service_ctx.events[i].topic != NULL) free(service_ctx.events[i].topic);
        }
    }

    if (service_ctx.ptz_node.enable == 1) {
        if (service_ctx.ptz_node.jump_to_rel != NULL) free(service_ctx.ptz_node.jump_to_rel);
        if (service_ctx.ptz_node.jump_to_abs != NULL) free(service_ctx.ptz_node.jump_to_abs);
        if (service_ctx.ptz_node.remove_preset != NULL) free(service_ctx.ptz_node.remove_preset);
        if (service_ctx.ptz_node.set_home_position != NULL) free(service_ctx.ptz_node.set_home_position);
        if (service_ctx.ptz_node.set_preset != NULL) free(service_ctx.ptz_node.set_preset);
        if (service_ctx.ptz_node.goto_home_position != NULL) free(service_ctx.ptz_node.goto_home_position);
        if (service_ctx.ptz_node.move_preset != NULL) free(service_ctx.ptz_node.move_preset);
        if (service_ctx.ptz_node.move_stop != NULL) free(service_ctx.ptz_node.move_stop);
        if (service_ctx.ptz_node.move_down != NULL) free(service_ctx.ptz_node.move_down);
        if (service_ctx.ptz_node.move_up != NULL) free(service_ctx.ptz_node.move_up);
        if (service_ctx.ptz_node.move_right != NULL) free(service_ctx.ptz_node.move_right);
        if (service_ctx.ptz_node.move_left != NULL) free(service_ctx.ptz_node.move_left);
        if (service_ctx.ptz_node.is_moving != NULL) free(service_ctx.ptz_node.is_moving);
        if (service_ctx.ptz_node.get_position != NULL) free(service_ctx.ptz_node.get_position);
    }

    for (i = service_ctx.profiles_num - 1; i >= 0; i--) {
        if (service_ctx.profiles[i].snapurl != NULL) free(service_ctx.profiles[i].snapurl);
        if (service_ctx.profiles[i].url != NULL) free(service_ctx.profiles[i].url);
        if (service_ctx.profiles[i].name != NULL) free(service_ctx.profiles[i].name);
    }
    if (service_ctx.profiles != NULL) free(service_ctx.profiles);

    for (i = service_ctx.scopes_num - 1; i >= 0; i--) {
        free(service_ctx.scopes[i]);
    }
    if (service_ctx.scopes != NULL) free(service_ctx.scopes);

    if (service_ctx.events != NULL) free(service_ctx.events);
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
    fprintf(stderr, "\nCreate a configuration file following this example:\n\n");
    fprintf(stderr, "Example:\n");
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
    fprintf(stderr, "\t#Advanced options\n");
    fprintf(stderr, "\tadv_enable_media2=0\n");
    fprintf(stderr, "\tadv_fault_if_unknown=0\n");
    fprintf(stderr, "\tadv_fault_if_set=0\n");
    fprintf(stderr, "\tadv_synology_nvr=0\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t#Profile 0\n");
    fprintf(stderr, "\tname=Profile_0\n");
    fprintf(stderr, "\twidth=1920\n");
    fprintf(stderr, "\theight=1080\n");
    fprintf(stderr, "\turl=rtsp://%%s/ch0_0.h264\n");
    fprintf(stderr, "\tsnapurl=http://%%s/cgi-bin/snapshot.sh?res=high&watermark=yes\n");
    fprintf(stderr, "\ttype=H264\n");
    fprintf(stderr, "\taudio_encoder=AAC\n");
    fprintf(stderr, "\taudio_decoder=G711\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t#Profile 1\n");
    fprintf(stderr, "\tname=Profile_1\n");
    fprintf(stderr, "\twidth=640\n");
    fprintf(stderr, "\theight=360\n");
    fprintf(stderr, "\turl=rtsp://%%s/ch0_1.h264\n");
    fprintf(stderr, "\tsnapurl=http://%%s/cgi-bin/snapshot.sh?res=low&watermark=yes\n");
    fprintf(stderr, "\ttype=H264\n");
    fprintf(stderr, "\taudio_encoder=AAC\n");
    fprintf(stderr, "\taudio_decoder=NONE\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t#PTZ\n");
    fprintf(stderr, "\tptz=1\n");
    fprintf(stderr, "\tmax_step_x=360\n");
    fprintf(stderr, "\tmax_step_y=180\n");
    fprintf(stderr, "\tget_position=/tmp/sd/yi-hack/bin/ipc_cmd -g\n");
    fprintf(stderr, "\tis_moving=/tmp/sd/yi-hack/bin/ipc_cmd -u\n");
    fprintf(stderr, "\tmove_left=/tmp/sd/yi-hack/bin/ipc_cmd -m left\n");
    fprintf(stderr, "\tmove_right=/tmp/sd/yi-hack/bin/ipc_cmd -m right\n");
    fprintf(stderr, "\tmove_up=/tmp/sd/yi-hack/bin/ipc_cmd -m up\n");
    fprintf(stderr, "\tmove_down=/tmp/sd/yi-hack/bin/ipc_cmd -m down\n");
    fprintf(stderr, "\tmove_stop=/tmp/sd/yi-hack/bin/ipc_cmd -m stop\n");
    fprintf(stderr, "\tmove_preset=/tmp/sd/yi-hack/bin/ipc_cmd -p %%d\n");
    fprintf(stderr, "\tgoto_home_position=/tmp/sd/yi-hack/bin/ipc_cmd -p 0\n");
    fprintf(stderr, "\tset_preset=/tmp/sd/yi-hack/script/ptz_presets.sh -a add_preset -m %%s\n");
    fprintf(stderr, "\tset_home_position=/tmp/sd/yi-hack/bin/ipc_cmd -H\n");
    fprintf(stderr, "\tremove_preset=/tmp/sd/yi-hack/script/ptz_presets.sh -a del_preset -n %%d\n");
    fprintf(stderr, "\tjump_to_abs=/tmp/sd/yi-hack/bin/ipc_cmd -j %%f,%%f\n");
    fprintf(stderr, "\tjump_to_rel=/tmp/sd/yi-hack/bin/ipc_cmd -J %%f,%%f\n");
    fprintf(stderr, "\tget_presets=/tmp/sd/yi-hack/script/ptz_presets.sh -a get_presets\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t#EVENTS\n");
    fprintf(stderr, "\tevents=1\n");
    fprintf(stderr, "\t#Event 0\n");
    fprintf(stderr, "\ttopic=tns1:VideoSource/MotionAlarm\n");
    fprintf(stderr, "\tsource_name=VideoSourceConfigurationToken\n");
    fprintf(stderr, "\tsource_value=VideoSourceToken\n");
    fprintf(stderr, "\tinput_file=/tmp/motion_alarm\n");
    fprintf(stderr, "\t#Event 1\n");
    fprintf(stderr, "\ttopic=tns1:RuleEngine/MyRuleDetector/PeopleDetect\n");
    fprintf(stderr, "\tsource_name=VideoSourceConfigurationToken\n");
    fprintf(stderr, "\tsource_value=VideoSourceToken\n");
    fprintf(stderr, "\tinput_file=/tmp/human_detection\n");
    fprintf(stderr, "\t#Event 2\n");
    fprintf(stderr, "\ttopic=tns1:RuleEngine/MyRuleDetector/VehicleDetect\n");
    fprintf(stderr, "\tsource_name=VideoSourceConfigurationToken\n");
    fprintf(stderr, "\tsource_value=VideoSourceToken\n");
    fprintf(stderr, "\tinput_file=/tmp/vehicle_detection\n");
    fprintf(stderr, "\t#Event 3\n");
    fprintf(stderr, "\ttopic=tns1:RuleEngine/MyRuleDetector/DogCatDetect\n");
    fprintf(stderr, "\tsource_name=VideoSourceConfigurationToken\n");
    fprintf(stderr, "\tsource_value=VideoSourceToken\n");
    fprintf(stderr, "\tinput_file=/tmp/animal_detection\n");
    fprintf(stderr, "\t#Event 4\n");
    fprintf(stderr, "\ttopic=tns1:RuleEngine/MyRuleDetector/BabyCryingDetect\n");
    fprintf(stderr, "\tsource_name=VideoSourceConfigurationToken\n");
    fprintf(stderr, "\tsource_value=VideoSourceToken\n");
    fprintf(stderr, "\tinput_file=/tmp/baby_crying\n");
    fprintf(stderr, "\t#Event 5\n");
    fprintf(stderr, "\ttopic=tns1:AudioAnalytics/Audio/DetectedSound\n");
    fprintf(stderr, "\tsource_name=VideoSourceConfigurationToken\n");
    fprintf(stderr, "\tsource_value=VideoSourceToken\n");
    fprintf(stderr, "\tinput_file=/tmp/sound_detection\n");
}
