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

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <errno.h>
#include <net/if.h>

#include <json-c/json.h>
#include "conf.h"
#include "utils.h"
#include "log.h"
#include "onvif_simple_server.h"

extern service_context_t service_ctx;

/* Grow arr from n - 1 to n elements and zero the new last one (callers add
 * exactly one element at a time). Returns NULL on failure; arr is still valid then. */
static void *grow(void *arr, int n, size_t size)
{
    void *p;

    if (n <= 0 || size == 0 || (size_t) n > SIZE_MAX / size) {
        log_error("Invalid array size");
        return NULL;
    }
    p = realloc(arr, n * size);
    if (p == NULL)
        log_error("Out of memory");
    else
        memset((char *) p + (n - 1) * size, 0, size);
    return p;
}

/* Copy src into a new string stored in *dst, freeing the previous value so a
 * repeated option does not leak. *dst must be NULL or a heap string.
 * Returns -1 on failure and leaves *dst untouched. */
static int dup_str(char **dst, const char *src)
{
    char *p = strdup(src);

    if (p == NULL) {
        log_error("Out of memory");
        return -1;
    }
    free(*dst);
    *dst = p;
    return 0;
}

int process_conf_file(char *file)
{
    FILE *fF;
    void *grown;
    char line[MAX_LEN];
    char param[MAX_LEN];
    char value[MAX_LEN];
    char stmp[MAX_LEN];

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
    service_ctx.factory_default_command = NULL;
    service_ctx.adv_enable_media2 = 0;
    service_ctx.adv_enable_imaging = 0;
    service_ctx.adv_empty_if_unknown = 0;
    service_ctx.adv_fault_if_unknown = 0;
    service_ctx.adv_fault_if_set = 0;
    service_ctx.adv_synology_nvr = 0;
    service_ctx.profiles = NULL;
    service_ctx.profiles_num = 0;
    service_ctx.scopes = NULL;
    service_ctx.scopes_num = 0;
    service_ctx.ptz_node.enable = 0;
    service_ctx.ptz_node.zoom_enable = -1;
    service_ctx.relay_outputs = NULL;
    service_ctx.relay_outputs_num = 0;
    service_ctx.events = NULL;
    service_ctx.events_enable = EVENTS_NONE;
    service_ctx.events_num = 0;
    service_ctx.ptz_node.min_step_x = 0;
    service_ctx.ptz_node.max_step_x = 360.0;
    service_ctx.ptz_node.min_step_y = 0;
    service_ctx.ptz_node.max_step_y = 180.0;
    service_ctx.ptz_node.min_step_z = 0.0;
    service_ctx.ptz_node.max_step_z = 0.0;
    service_ctx.ptz_node.get_position = NULL;
    service_ctx.ptz_node.is_moving = NULL;
    service_ctx.ptz_node.move_left = NULL;
    service_ctx.ptz_node.move_right = NULL;
    service_ctx.ptz_node.move_up = NULL;
    service_ctx.ptz_node.move_down = NULL;
    service_ctx.ptz_node.move_in = NULL;
    service_ctx.ptz_node.move_out = NULL;
    service_ctx.ptz_node.move_stop = NULL;
    service_ctx.ptz_node.move_preset = NULL;
    service_ctx.ptz_node.goto_home_position = NULL;
    service_ctx.ptz_node.set_preset = NULL;
    service_ctx.ptz_node.set_home_position = NULL;
    service_ctx.ptz_node.remove_preset = NULL;
    service_ctx.ptz_node.jump_to_abs = NULL;
    service_ctx.ptz_node.jump_to_rel = NULL;
    service_ctx.imaging_set_command = NULL;
    service_ctx.imaging_focus_command = NULL;
    service_ctx.imaging_status_command = NULL;
    service_ctx.imaging_focus_auto = 1;
    service_ctx.imaging_brightness = 0.5;
    service_ctx.imaging_contrast = 0.5;
    service_ctx.imaging_saturation = 0.5;
    service_ctx.imaging_sharpness = 0.5;
    service_ctx.imaging_ircut_on = 1;
    service_ctx.imaging_wb_auto = 1;
    service_ctx.imaging_exposure_auto = 1;
    service_ctx.imaging_wdr_on = 0;

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
        size_t value_len = strlen(value);
        if ((value_len >= 2) && (value[0] == '"') && (value[value_len - 1] == '"')) {
            memmove(value, &value[1], value_len - 2);
            value[value_len - 2] = '\0';
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
            if (dup_str(&service_ctx.user, value) != 0)
                return -2;
        } else if (strcasecmp(param, "password") == 0) {
            if (dup_str(&service_ctx.password, value) != 0)
                return -2;
        } else if (strcasecmp(param, "manufacturer") == 0) {
            if (dup_str(&service_ctx.manufacturer, value) != 0)
                return -2;
        } else if (strcasecmp(param, "model") == 0) {
            if (dup_str(&service_ctx.model, value) != 0)
                return -2;
        } else if (strcasecmp(param, "firmware_ver") == 0) {
            if (dup_str(&service_ctx.firmware_ver, value) != 0)
                return -2;
        } else if (strcasecmp(param, "serial_num") == 0) {
            if (dup_str(&service_ctx.serial_num, value) != 0)
                return -2;
        } else if (strcasecmp(param, "hardware_id") == 0) {
            if (dup_str(&service_ctx.hardware_id, value) != 0)
                return -2;
        } else if (strcasecmp(param, "scope") == 0) {
            service_ctx.scopes_num++;
            grown = grow(service_ctx.scopes, service_ctx.scopes_num, sizeof(char *));
            if (grown == NULL) {
                service_ctx.scopes_num--;
                return -2;
            }
            service_ctx.scopes = grown;
            if (dup_str(&service_ctx.scopes[service_ctx.scopes_num - 1], value) != 0)
                return -2;
        } else if (strcasecmp(param, "ifs") == 0) {
            if (dup_str(&service_ctx.ifs, value) != 0)
                return -2;
        } else if (strcasecmp(param, "adv_enable_media2") == 0) {
            if (strcasecmp(value, "1") == 0)
                service_ctx.adv_enable_media2 = 1;
        } else if (strcasecmp(param, "adv_enable_imaging") == 0) {
            if (strcasecmp(value, "1") == 0)
                service_ctx.adv_enable_imaging = 1;
        } else if (strcasecmp(param, "adv_empty_if_unknown") == 0) {
            if (strcasecmp(value, "1") == 0)
                service_ctx.adv_empty_if_unknown = 1;
        } else if (strcasecmp(param, "adv_fault_if_unknown") == 0) {
            if (strcasecmp(value, "1") == 0)
                service_ctx.adv_fault_if_unknown = 1;
        } else if (strcasecmp(param, "adv_fault_if_set") == 0) {
            if (strcasecmp(value, "1") == 0)
                service_ctx.adv_fault_if_set = 1;
        } else if (strcasecmp(param, "adv_synology_nvr") == 0) {
            if (strcasecmp(value, "1") == 0)
                service_ctx.adv_synology_nvr = 1;
        } else if (strcasecmp(param, "factory_default_command") == 0) {
            if (dup_str(&service_ctx.factory_default_command, value) != 0)
                return -2;

        //Media Profile for ONVIF Media Service
        } else if (strcasecmp(param, "name") == 0) {
            service_ctx.profiles_num++;
            grown = grow(service_ctx.profiles, service_ctx.profiles_num, sizeof(stream_profile_t));
            if (grown == NULL) {
                service_ctx.profiles_num--;
                return -2;
            }
            service_ctx.profiles = grown;

            size_t vlen = strlen(value);
            while (vlen > 0 && value[vlen - 1] == '_') value[--vlen] = '\0';
            if (vlen > 64) {
                log_warn("Profile name '%s' exceeds 64 chars; truncating", value);
                value[64] = '\0';
            }
            for (char *p = value; *p; p++) {
                if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-' && *p != '.') {
                    log_warn("Profile name '%s' contains invalid char '%c'; replacing with '_'", value, *p);
                    *p = '_';
                }
            }
            if (dup_str(&service_ctx.profiles[service_ctx.profiles_num - 1].name, value) != 0)
                return -2;
            service_ctx.profiles[service_ctx.profiles_num - 1].width = 0;
            service_ctx.profiles[service_ctx.profiles_num - 1].height = 0;
            service_ctx.profiles[service_ctx.profiles_num - 1].url = NULL;
            service_ctx.profiles[service_ctx.profiles_num - 1].snapurl = NULL;
            service_ctx.profiles[service_ctx.profiles_num - 1].type = H264;
            service_ctx.profiles[service_ctx.profiles_num - 1].bitrate = 0;
            service_ctx.profiles[service_ctx.profiles_num - 1].framerate = 30;
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
            if (dup_str(&service_ctx.profiles[service_ctx.profiles_num - 1].url, value) != 0)
                return -2;
        } else if (strcasecmp(param, "snapurl") == 0) {
            if (dup_str(&service_ctx.profiles[service_ctx.profiles_num - 1].snapurl, value) != 0)
                return -2;
        } else if (strcasecmp(param, "type") == 0) {
            if (strcasecmp(value, "JPEG") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].type = JPEG;
            else if (strcasecmp(value, "MPEG4") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].type = MPEG4;
            else if (strcasecmp(value, "H264") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].type = H264;
            else if (strcasecmp(value, "H265") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].type = H265;
        } else if (strcasecmp(param, "bitrate") == 0) {
            service_ctx.profiles[service_ctx.profiles_num - 1].bitrate = atoi(value);
        } else if (strcasecmp(param, "framerate") == 0) {
            service_ctx.profiles[service_ctx.profiles_num - 1].framerate = atoi(value);
        } else if (strcasecmp(param, "audio_encoder") == 0) {
            if (strcasecmp(value, "NONE") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = AUDIO_NONE;
            else if (strcasecmp(value, "G711") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = G711;
            else if (strcasecmp(value, "G726") == 0)
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = G726;
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
            service_ctx.ptz_node.min_step_x = 0.0;
            service_ctx.ptz_node.zoom_enable = -1;
            service_ctx.ptz_node.max_step_x = 360.0;
            service_ctx.ptz_node.min_step_y = 0.0;
            service_ctx.ptz_node.max_step_y = 180.0;
            service_ctx.ptz_node.min_step_z = 0.0;
            service_ctx.ptz_node.max_step_z = 0.0;
            service_ctx.ptz_node.get_position = NULL;
            service_ctx.ptz_node.is_moving = NULL;
            service_ctx.ptz_node.move_left = NULL;
            service_ctx.ptz_node.move_right = NULL;
            service_ctx.ptz_node.move_up = NULL;
            service_ctx.ptz_node.move_down = NULL;
            service_ctx.ptz_node.move_in = NULL;
            service_ctx.ptz_node.move_out = NULL;
            service_ctx.ptz_node.move_stop = NULL;
            service_ctx.ptz_node.move_preset = NULL;
            service_ctx.ptz_node.goto_home_position = NULL;
            service_ctx.ptz_node.set_preset = NULL;
            service_ctx.ptz_node.set_home_position = NULL;
            service_ctx.ptz_node.remove_preset = NULL;
            service_ctx.ptz_node.jump_to_abs = NULL;
            service_ctx.ptz_node.jump_to_rel = NULL;
        } else if (strcasecmp(param, "zoom") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if ((strcasecmp(value, "1") == 0) || (strcasecmp(value, "yes") == 0) || (strcasecmp(value, "true") == 0)) {
                    service_ctx.ptz_node.zoom_enable = 1;
                } else if ((strcasecmp(value, "0") == 0) || (strcasecmp(value, "no") == 0) || (strcasecmp(value, "false") == 0)) {
                    service_ctx.ptz_node.zoom_enable = 0;
                } else {
                    service_ctx.ptz_node.zoom_enable = -1;
                }
            }
        } else if (strcasecmp(param, "min_step_x") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (value[0] == '\0') {
                    log_warn("Empty value for %s, use default", param);
                } else {
                    errno = 0;
                    service_ctx.ptz_node.min_step_x = strtod(value, &endptr);

                    /* Check for various possible errors */
                    if (errno == ERANGE || (errno != 0 && service_ctx.ptz_node.min_step_x == 0.0)) {
                        log_error("Wrong option: %s", line);
                        return -2;
                    }
                    if (endptr == value) {
                        log_error("Wrong option: %s", line);
                        return -2;
                    }
                }
            }
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
        } else if (strcasecmp(param, "min_step_y") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (value[0] == '\0') {
                    log_warn("Empty value for %s, use default", param);
                } else {
                    errno = 0;
                    service_ctx.ptz_node.min_step_y = strtod(value, &endptr);

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
        } else if (strcasecmp(param, "min_step_z") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (value[0] == '\0') {
                    log_warn("Empty value for %s, use default", param);
                } else {
                    errno = 0;
                    service_ctx.ptz_node.min_step_z = strtod(value, &endptr);

                    /* Check for various possible errors */
                    if (errno == ERANGE || (errno != 0 && service_ctx.ptz_node.max_step_z == 0.0)) {
                        log_error("Wrong option: %s", line);
                        return -2;
                    }
                    if (endptr == value) {
                        log_error("Wrong option: %s", line);
                        return -2;
                    }
                }
            }
        } else if (strcasecmp(param, "max_step_z") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (value[0] == '\0') {
                    log_warn("Empty value for %s, use default", param);
                } else {
                    errno = 0;
                    service_ctx.ptz_node.max_step_z = strtod(value, &endptr);

                    /* Check for various possible errors */
                    if (errno == ERANGE || (errno != 0 && service_ctx.ptz_node.max_step_z == 0.0)) {
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
                if (dup_str(&service_ctx.ptz_node.get_position, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "is_moving") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (dup_str(&service_ctx.ptz_node.is_moving, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "move_left") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (dup_str(&service_ctx.ptz_node.move_left, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "move_right") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (dup_str(&service_ctx.ptz_node.move_right, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "move_up") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (dup_str(&service_ctx.ptz_node.move_up, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "move_down") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (dup_str(&service_ctx.ptz_node.move_down, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "move_in") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (dup_str(&service_ctx.ptz_node.move_in, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "move_out") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (dup_str(&service_ctx.ptz_node.move_out, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "move_stop") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (dup_str(&service_ctx.ptz_node.move_stop, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "move_preset") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (dup_str(&service_ctx.ptz_node.move_preset, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "goto_home_position") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (dup_str(&service_ctx.ptz_node.goto_home_position, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "set_preset") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (dup_str(&service_ctx.ptz_node.set_preset, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "set_home_position") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (dup_str(&service_ctx.ptz_node.set_home_position, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "remove_preset") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (dup_str(&service_ctx.ptz_node.remove_preset, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "jump_to_abs") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (dup_str(&service_ctx.ptz_node.jump_to_abs, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "jump_to_rel") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (dup_str(&service_ctx.ptz_node.jump_to_rel, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "get_presets") == 0) {
            if (service_ctx.ptz_node.enable == 1) {
                if (dup_str(&service_ctx.ptz_node.get_presets, value) != 0)
                    return -2;
            }

        //Relay outputs
        } else if (strcasecmp(param, "idle_state") == 0) {
            service_ctx.relay_outputs_num++;
            if (service_ctx.relay_outputs_num >= MAX_RELAY_OUTPUTS) {
                log_error("Too many relay outputs, max is: %d", MAX_RELAY_OUTPUTS);
                return -2;
            }
            grown = grow(service_ctx.relay_outputs, service_ctx.relay_outputs_num, sizeof(relay_output_t));
            if (grown == NULL) {
                service_ctx.relay_outputs_num--;
                return -2;
            }
            service_ctx.relay_outputs = grown;
            if (strcasecmp(value, "close") == 0) {
                service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].idle_state = IDLE_STATE_CLOSE;
            } else {
                service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].idle_state = IDLE_STATE_OPEN;
            }
            service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].close = NULL;
            service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].open = NULL;
        } else if (strcasecmp(param, "close") == 0) {
            if (dup_str(&service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].close, value) != 0)
                return -2;
        } else if (strcasecmp(param, "open") == 0) {
            if (dup_str(&service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].open, value) != 0)
                return -2;

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
                if (service_ctx.events_num > MAX_EVENTS) {
                    log_error("Too many events, max is: %d", MAX_EVENTS);
                    return -2;
                }
                grown = grow(service_ctx.events, service_ctx.events_num, sizeof(event_t));
                if (grown == NULL) {
                    service_ctx.events_num--;
                    return -2;
                }
                service_ctx.events = grown;
                service_ctx.events[service_ctx.events_num - 1].topic = NULL;
                service_ctx.events[service_ctx.events_num - 1].source_name = NULL;
                service_ctx.events[service_ctx.events_num - 1].source_type = NULL;
                service_ctx.events[service_ctx.events_num - 1].source_value = NULL;
                service_ctx.events[service_ctx.events_num - 1].input_file = NULL;
                if (dup_str(&service_ctx.events[service_ctx.events_num - 1].topic, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "source_name") == 0) {
            if (service_ctx.events_enable != EVENTS_NONE) {
                if (dup_str(&service_ctx.events[service_ctx.events_num - 1].source_name, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "source_type") == 0) {
            if (service_ctx.events_enable != EVENTS_NONE) {
                if (dup_str(&service_ctx.events[service_ctx.events_num - 1].source_type, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "source_value") == 0) {
            if (service_ctx.events_enable != EVENTS_NONE) {
                if (dup_str(&service_ctx.events[service_ctx.events_num - 1].source_value, value) != 0)
                    return -2;
            }
        } else if (strcasecmp(param, "input_file") == 0) {
            if (service_ctx.events_enable != EVENTS_NONE) {
                if (dup_str(&service_ctx.events[service_ctx.events_num - 1].input_file, value) != 0)
                    return -2;
            }

        //ONVIF Imaging Service
        } else if (strcasecmp(param, "imaging_set_command") == 0) {
            if (dup_str(&service_ctx.imaging_set_command, value) != 0)
                return -2;
        } else if (strcasecmp(param, "imaging_focus_command") == 0) {
            if (dup_str(&service_ctx.imaging_focus_command, value) != 0)
                return -2;
        } else if (strcasecmp(param, "imaging_status_command") == 0) {
            if (dup_str(&service_ctx.imaging_status_command, value) != 0)
                return -2;
        } else if (strcasecmp(param, "imaging_focus_auto") == 0) {
            service_ctx.imaging_focus_auto = (strcasecmp(value, "1") == 0);
        } else if (strcasecmp(param, "imaging_brightness") == 0) {
            service_ctx.imaging_brightness = atof(value);
        } else if (strcasecmp(param, "imaging_contrast") == 0) {
            service_ctx.imaging_contrast = atof(value);
        } else if (strcasecmp(param, "imaging_saturation") == 0) {
            service_ctx.imaging_saturation = atof(value);
        } else if (strcasecmp(param, "imaging_sharpness") == 0) {
            service_ctx.imaging_sharpness = atof(value);
        } else if (strcasecmp(param, "imaging_ircut_on") == 0) {
            service_ctx.imaging_ircut_on = (strcasecmp(value, "1") == 0);
        } else if (strcasecmp(param, "imaging_wb_auto") == 0) {
            service_ctx.imaging_wb_auto = (strcasecmp(value, "1") == 0);
        } else if (strcasecmp(param, "imaging_exposure_auto") == 0) {
            service_ctx.imaging_exposure_auto = (strcasecmp(value, "1") == 0);
        } else if (strcasecmp(param, "imaging_wdr_on") == 0) {
            service_ctx.imaging_wdr_on = (strcasecmp(value, "1") == 0);

        } else {
            log_warn("Unrecognized option: %s", line);
        }
    }

    if (service_ctx.relay_outputs != NULL) {
        if (service_ctx.events_enable == EVENTS_NONE) service_ctx.events_enable = EVENTS_PULLPOINT;

        for (i = 0; i < service_ctx.relay_outputs_num; i++) {
            service_ctx.events_num++;
            if (service_ctx.events_num > MAX_EVENTS) {
                log_error("Unable to add relay event, too many events, max is: %d", MAX_EVENTS);
                return -2;
            }
            log_debug("Adding event for relay output %d", i);
            grown = grow(service_ctx.events, service_ctx.events_num, sizeof(event_t));
            if (grown == NULL) {
                service_ctx.events_num--;
                return -2;
            }
            service_ctx.events = grown;
            if (dup_str(&service_ctx.events[service_ctx.events_num - 1].topic, "tns1:Device/Trigger/Relay") != 0)
                return -2;
            log_debug("topic: tns1:Device/Trigger/Relay");
            if (dup_str(&service_ctx.events[service_ctx.events_num - 1].source_name, "RelayToken") != 0)
                return -2;
            log_debug("source_name: RelayToken");
            if (dup_str(&service_ctx.events[service_ctx.events_num - 1].source_type, "tt:ReferenceToken") != 0)
                return -2;
            log_debug("source_type: tt:ReferenceToken");
            snprintf(stmp, sizeof(stmp), "RelayOutputToken_%d", i);
            if (dup_str(&service_ctx.events[service_ctx.events_num - 1].source_value, stmp) != 0)
                return -2;
            log_debug("source_value: %s", stmp);
            snprintf(stmp, sizeof(stmp), "/tmp/onvif_notify_server/relay_output_%d", i);
            if (dup_str(&service_ctx.events[service_ctx.events_num - 1].input_file, stmp) != 0)
                return -2;
            log_debug("input_file: %s", stmp);
        }
    }

    // If a string option is NULL, set a default value
    if (service_ctx.manufacturer == NULL) {
        if (dup_str(&service_ctx.manufacturer, DEFAULT_MANUFACTURER) != 0)
            return -2;
    }
    if (service_ctx.model == NULL) {
        if (dup_str(&service_ctx.model, DEFAULT_MODEL) != 0)
            return -2;
    }
    if (service_ctx.firmware_ver == NULL) {
        if (dup_str(&service_ctx.firmware_ver, DEFAULT_FW_VER) != 0)
            return -2;
    }
    if (service_ctx.serial_num == NULL) {
        if (dup_str(&service_ctx.serial_num, DEFAULT_SERIAL_NUM) != 0)
            return -2;
    }
    if (service_ctx.hardware_id == NULL) {
        if (dup_str(&service_ctx.hardware_id, DEFAULT_HW_ID) != 0)
            return -2;
    }
    /* Address and interface resolution */
    if (service_ctx.ifs != NULL) {
        /* Interface explicitly configured: derive address from it */
        char netmask[16];
        if (get_ip_address(service_ctx.address, netmask, service_ctx.ifs) < 0) {
            log_fatal("Cannot get IP address from interface %s.", service_ctx.ifs);
            exit(EXIT_FAILURE);
        }
    } else {
        /* Auto-detect: derive address from REMOTE_ADDR, then detect interface */
        const char *remote = getenv("REMOTE_ADDR");
        if (!remote || detect_local_address(remote, service_ctx.address,
                                            sizeof(service_ctx.address)) != 0) {
            log_fatal("Cannot determine local address: REMOTE_ADDR not available.");
            exit(EXIT_FAILURE);
        }
        char auto_ifs[IFNAMSIZ];
        if (get_ifname_by_addr(service_ctx.address, auto_ifs, sizeof(auto_ifs)) == 0) {
            if (dup_str(&service_ctx.ifs, auto_ifs) != 0)
                return -2;
        } else {
            if (dup_str(&service_ctx.ifs, DEFAULT_IFS) != 0)
                return -2;
        }
    }
    if (strchr(service_ctx.address, ':'))
        snprintf(service_ctx.address_url, sizeof(service_ctx.address_url),
                 "[%s]", service_ctx.address);
    else
        strncpy(service_ctx.address_url, service_ctx.address,
                sizeof(service_ctx.address_url) - 1);
    service_ctx.address_url[sizeof(service_ctx.address_url) - 1] = '\0';

    /* Stable device UUID from MAC */
    {
        uint8_t mac[6] = {0};
        if (get_mac_by_ifname(service_ctx.ifs, mac) == 0)
            gen_uuid_v5_mac(service_ctx.device_uuid, mac);
        else
            gen_uuid(service_ctx.device_uuid);
        log_info("Device UUID: %s", service_ctx.device_uuid);
    }

    return 0;
}

// Remember to free the string
int get_string_from_json(char** var, json_object* j, char* name)
{
    json_object* s = NULL;

    if (json_object_object_get_ex(j, name, &s)) {
        if (json_object_is_type(s, json_type_string)) {
            const char* str_val = json_object_get_string(s);
            if (str_val != NULL)
                return dup_str(var, str_val);
        }
    }
    return 0;
}

void get_int_from_json(int* var, json_object* j, char* name)
{
    json_object* i = NULL;

    if (json_object_object_get_ex(j, name, &i)) {
        if (json_object_is_type(i, json_type_int)) {
            *var = json_object_get_int(i);
        }
    }
}

void get_double_from_json(double* var, json_object* j, char* name)
{
    json_object* d = NULL;

    if (json_object_object_get_ex(j, name, &d)) {
        if (json_object_is_type(d, json_type_double)) {
            *var = json_object_get_double(d);
        }
    }
}

void get_bool_from_json(int* var, json_object* j, char* name)
{
    json_object* b = NULL;

    if (json_object_object_get_ex(j, name, &b)) {
        if (json_object_is_type(b, json_type_boolean)) {
            *var = json_object_get_boolean(b) ? 1 : 0;
        } else {
            *var = 0;
        }
    } else {
        *var = 0;
    }
}

/* Like get_bool_from_json/get_double_from_json, but keep the current (default)
 * value when the key is absent. */
static void get_bool_from_json_opt(int* var, json_object* j, char* name)
{
    json_object* b = NULL;

    if (json_object_object_get_ex(j, name, &b))
        get_bool_from_json(var, j, name);
}

static void get_number_from_json_opt(double* var, json_object* j, char* name)
{
    json_object* d = NULL;

    if (json_object_object_get_ex(j, name, &d) &&
            (json_object_is_type(d, json_type_double) || json_object_is_type(d, json_type_int)))
        *var = json_object_get_double(d);
}

int process_json_conf_file(char *file)
{
    FILE *fF;
    json_object *value, *item;
    char *buffer, *tmp;
    void *grown;
    json_object* json_file;

    int i, json_size;
    char stmp[MAX_LEN];

    fF = fopen(file, "r");
    if (fF == NULL) {
        log_error("Failed to open JSON configuration file");
        return -1;
    }

    fseek(fF, 0L, SEEK_END);
    json_size = ftell(fF);
    fseek(fF, 0L, SEEK_SET);

    buffer = (char *) malloc((json_size + 1) * sizeof(char));
    if (buffer == NULL) {
        log_error("Failed to allocate memory for JSON configuration file");
        fclose(fF);
        return -1;
    }

    if (fread(buffer, 1, json_size, fF) != json_size) {
        log_error("Failed to read JSON configuration file");
        free(buffer);
        fclose(fF);
        return -1;
    }
    fclose(fF);

    // Null-terminate the buffer to prevent buffer overflow in json_tokener_parse
    buffer[json_size] = '\0';

    json_file = json_tokener_parse(buffer);
    if (json_file == NULL) {
        log_error("Failed to parse JSON configuration file");
        free(buffer);
        return -2;
    }

    // Init variables before reading
    service_ctx.port = 80;
    service_ctx.user = NULL;
    service_ctx.password = NULL;
    service_ctx.manufacturer = NULL;
    service_ctx.model = NULL;
    service_ctx.firmware_ver = NULL;
    service_ctx.serial_num = NULL;
    service_ctx.hardware_id = NULL;
    service_ctx.ifs = NULL;
    service_ctx.factory_default_command = NULL;
    service_ctx.adv_enable_media2 = 0;
    service_ctx.adv_enable_imaging = 0;
    service_ctx.adv_empty_if_unknown = 0;
    service_ctx.adv_fault_if_unknown = 0;
    service_ctx.adv_fault_if_set = 0;
    service_ctx.adv_synology_nvr = 0;
    service_ctx.profiles = NULL;
    service_ctx.profiles_num = 0;
    service_ctx.scopes = NULL;
    service_ctx.scopes_num = 0;
    service_ctx.ptz_node.enable = 0;
    service_ctx.ptz_node.zoom_enable = -1;
    service_ctx.relay_outputs = NULL;
    service_ctx.relay_outputs_num = 0;
    service_ctx.events = NULL;
    service_ctx.events_enable = EVENTS_NONE;
    service_ctx.events_num = 0;

    service_ctx.ptz_node.min_step_x = 0;
    service_ctx.ptz_node.max_step_x = 360.0;
    service_ctx.ptz_node.min_step_y = 0;
    service_ctx.ptz_node.max_step_y = 180.0;
    service_ctx.ptz_node.min_step_z = 0.0;
    service_ctx.ptz_node.max_step_z = 0.0;
    service_ctx.ptz_node.get_position = NULL;
    service_ctx.ptz_node.is_moving = NULL;
    service_ctx.ptz_node.move_left = NULL;
    service_ctx.ptz_node.move_right = NULL;
    service_ctx.ptz_node.move_up = NULL;
    service_ctx.ptz_node.move_down = NULL;
    service_ctx.ptz_node.move_in = NULL;
    service_ctx.ptz_node.move_out = NULL;
    service_ctx.ptz_node.move_stop = NULL;
    service_ctx.ptz_node.move_preset = NULL;
    service_ctx.ptz_node.goto_home_position = NULL;
    service_ctx.ptz_node.set_preset = NULL;
    service_ctx.ptz_node.set_home_position = NULL;
    service_ctx.ptz_node.remove_preset = NULL;
    service_ctx.ptz_node.jump_to_abs = NULL;
    service_ctx.ptz_node.jump_to_rel = NULL;

    service_ctx.imaging_set_command = NULL;
    service_ctx.imaging_focus_command = NULL;
    service_ctx.imaging_status_command = NULL;
    service_ctx.imaging_focus_auto = 1;
    service_ctx.imaging_brightness = 0.5;
    service_ctx.imaging_contrast = 0.5;
    service_ctx.imaging_saturation = 0.5;
    service_ctx.imaging_sharpness = 0.5;
    service_ctx.imaging_ircut_on = 1;
    service_ctx.imaging_wb_auto = 1;
    service_ctx.imaging_exposure_auto = 1;
    service_ctx.imaging_wdr_on = 0;

    if (get_string_from_json(&(service_ctx.model), json_file, "model") != 0)
        goto oom;
    if (get_string_from_json(&(service_ctx.manufacturer), json_file, "manufacturer") != 0)
        goto oom;
    if (get_string_from_json(&(service_ctx.firmware_ver), json_file, "firmware_ver") != 0)
        goto oom;
    if (get_string_from_json(&(service_ctx.hardware_id), json_file, "hardware_id") != 0)
        goto oom;
    if (get_string_from_json(&(service_ctx.serial_num), json_file, "serial_num") != 0)
        goto oom;
    if (get_string_from_json(&(service_ctx.factory_default_command), json_file, "factory_default_command") != 0)
        goto oom;


    if (get_string_from_json(&(service_ctx.ifs), json_file, "ifs") != 0)
        goto oom;
    get_int_from_json(&(service_ctx.port), json_file, "port");

    if (json_object_object_get_ex(json_file, "scopes", &value)) {
        if (json_object_is_type(value, json_type_array)) {
            size_t array_len = json_object_array_length(value);
            for (size_t i = 0; i < array_len; i++) {
                item = json_object_array_get_idx(value, i);
                if (item && json_object_is_type(item, json_type_string)) {
                    service_ctx.scopes_num++;
                    grown = grow(service_ctx.scopes, service_ctx.scopes_num, sizeof(char*));
                    if (grown == NULL) {
                        service_ctx.scopes_num--;
                        goto oom;
                    }
                    service_ctx.scopes = grown;
                    const char* str_val = json_object_get_string(item);
                    if (dup_str(&service_ctx.scopes[service_ctx.scopes_num - 1], str_val) != 0)
                        goto oom;
                }
            }
        }
    }
    if (get_string_from_json(&(service_ctx.user), json_file, "user") != 0)
        goto oom;
    if (get_string_from_json(&(service_ctx.password), json_file, "password") != 0)
        goto oom;
    get_bool_from_json(&(service_ctx.adv_enable_media2), json_file, "adv_enable_media2");
    get_bool_from_json(&(service_ctx.adv_enable_imaging), json_file, "adv_enable_imaging");
    get_bool_from_json(&(service_ctx.adv_empty_if_unknown), json_file, "adv_empty_if_unknown");
    get_bool_from_json(&(service_ctx.adv_fault_if_unknown), json_file, "adv_fault_if_unknown");
    get_bool_from_json(&(service_ctx.adv_fault_if_set), json_file, "adv_fault_if_set");
    get_bool_from_json(&(service_ctx.adv_synology_nvr), json_file, "adv_synology_nvr");

    // Apply sane defaults for required fields if not provided in config
    if (service_ctx.manufacturer == NULL) {
        if (dup_str(&service_ctx.manufacturer, DEFAULT_MANUFACTURER) != 0)
            goto oom;
    }
    if (service_ctx.model == NULL) {
        if (dup_str(&service_ctx.model, DEFAULT_MODEL) != 0)
            goto oom;
    }
    if (service_ctx.firmware_ver == NULL) {
        if (dup_str(&service_ctx.firmware_ver, DEFAULT_FW_VER) != 0)
            goto oom;
    }
    if (service_ctx.serial_num == NULL) {
        if (dup_str(&service_ctx.serial_num, DEFAULT_SERIAL_NUM) != 0)
            goto oom;
    }
    if (service_ctx.hardware_id == NULL) {
        if (dup_str(&service_ctx.hardware_id, DEFAULT_HW_ID) != 0)
            goto oom;
    }
    /* Address and interface resolution */
    if (service_ctx.ifs != NULL) {
        /* Interface explicitly configured: derive address from it */
        char netmask[16];
        if (get_ip_address(service_ctx.address, netmask, service_ctx.ifs) < 0) {
            log_fatal("Cannot get IP address from interface %s.", service_ctx.ifs);
            exit(EXIT_FAILURE);
        }
    } else {
        /* Auto-detect: derive address from REMOTE_ADDR, then detect interface */
        const char *remote = getenv("REMOTE_ADDR");
        if (!remote || detect_local_address(remote, service_ctx.address,
                                            sizeof(service_ctx.address)) != 0) {
            log_fatal("Cannot determine local address: REMOTE_ADDR not available.");
            exit(EXIT_FAILURE);
        }
        char auto_ifs[IFNAMSIZ];
        if (get_ifname_by_addr(service_ctx.address, auto_ifs, sizeof(auto_ifs)) == 0) {
            if (dup_str(&service_ctx.ifs, auto_ifs) != 0)
                goto oom;
        } else {
            if (dup_str(&service_ctx.ifs, DEFAULT_IFS) != 0)
                goto oom;
        }
    }
    if (strchr(service_ctx.address, ':'))
        snprintf(service_ctx.address_url, sizeof(service_ctx.address_url),
                 "[%s]", service_ctx.address);
    else
        strncpy(service_ctx.address_url, service_ctx.address,
                sizeof(service_ctx.address_url) - 1);
    service_ctx.address_url[sizeof(service_ctx.address_url) - 1] = '\0';

    /* Stable device UUID from MAC */
    {
        uint8_t mac[6] = {0};
        if (get_mac_by_ifname(service_ctx.ifs, mac) == 0)
            gen_uuid_v5_mac(service_ctx.device_uuid, mac);
        else
            gen_uuid(service_ctx.device_uuid);
        log_info("Device UUID: %s", service_ctx.device_uuid);
    }

    // Print debug
    log_debug("model: %s", service_ctx.model);
    log_debug("manufacturer: %s", service_ctx.manufacturer);
    log_debug("firmware_ver: %s", service_ctx.firmware_ver);
    log_debug("hardware_id: %s", service_ctx.hardware_id);
    log_debug("serial_num: %s", service_ctx.serial_num);
    log_debug("ifs: %s", service_ctx.ifs);
    log_debug("factory_default_command: %s", service_ctx.factory_default_command ? service_ctx.factory_default_command : "(none)");
    log_debug("port: %d", service_ctx.port);
    log_debug("scopes:");
    for (i = 0; i < service_ctx.scopes_num; i++) {
        log_debug("\t%s", service_ctx.scopes[i]);
    }
    if (service_ctx.user != NULL) {
        log_debug("user: %s", service_ctx.user);
        log_debug("password: ********");
    }
    log_debug("adv_enable_media2: %d", service_ctx.adv_enable_media2);
    log_debug("adv_enable_imaging: %d", service_ctx.adv_enable_imaging);
    log_debug("adv_empty_if_unknown: %d", service_ctx.adv_empty_if_unknown);
    log_debug("adv_fault_if_unknown: %d", service_ctx.adv_fault_if_unknown);
    log_debug("adv_fault_if_set: %d", service_ctx.adv_fault_if_set);
    log_debug("adv_synology_nvr: %d", service_ctx.adv_synology_nvr);
    log_debug("");

    // Load profiles from main configuration file
    if (json_object_object_get_ex(json_file, "profiles", &value)) {
        if (json_object_is_type(value, json_type_array)) {
            size_t array_len = json_object_array_length(value);
            log_debug("Found %zu profile entries in configuration", array_len);
            for (size_t i = 0; i < array_len; i++) {
                item = json_object_array_get_idx(value, i);
                if (!item)
                    continue;

                service_ctx.profiles_num++;
                grown = grow(service_ctx.profiles, service_ctx.profiles_num, sizeof(stream_profile_t));
                if (grown == NULL) {
                    service_ctx.profiles_num--;
                    goto oom;
                }
                service_ctx.profiles = grown;

                // Init defaults
                service_ctx.profiles[service_ctx.profiles_num - 1].name = NULL;
                service_ctx.profiles[service_ctx.profiles_num - 1].width = 0;
                service_ctx.profiles[service_ctx.profiles_num - 1].height = 0;
                service_ctx.profiles[service_ctx.profiles_num - 1].url = NULL;
                service_ctx.profiles[service_ctx.profiles_num - 1].snapurl = NULL;
                service_ctx.profiles[service_ctx.profiles_num - 1].type = H264;
                service_ctx.profiles[service_ctx.profiles_num - 1].bitrate = 0;
                service_ctx.profiles[service_ctx.profiles_num - 1].framerate = 30;
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = AAC;
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_decoder = AUDIO_NONE;

                if (get_string_from_json(&(service_ctx.profiles[service_ctx.profiles_num - 1].name), item, "name") != 0)

                    goto oom;
                char *n = service_ctx.profiles[service_ctx.profiles_num - 1].name;
                if (n != NULL) {
                    size_t nlen = strlen(n);
                    if (nlen > 64) {
                        log_warn("Profile name '%s' exceeds 64 chars; truncating", n);
                        n[64] = '\0';
                    }
                    for (char *p = n; *p; p++) {
                        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-' && *p != '.') {
                            log_warn("Profile name '%s' contains invalid char '%c'; replacing with '_'", n, *p);
                            *p = '_';
                        }
                    }
                }
                get_int_from_json(&(service_ctx.profiles[service_ctx.profiles_num - 1].width), item, "width");
                get_int_from_json(&(service_ctx.profiles[service_ctx.profiles_num - 1].height), item, "height");
                get_int_from_json(&(service_ctx.profiles[service_ctx.profiles_num - 1].framerate), item, "framerate");
                if (get_string_from_json(&(service_ctx.profiles[service_ctx.profiles_num - 1].url), item, "url") != 0)
                    goto oom;
                if (get_string_from_json(&(service_ctx.profiles[service_ctx.profiles_num - 1].snapurl), item, "snapurl") != 0)
                    goto oom;
                tmp = NULL;
                if (get_string_from_json(&tmp, item, "type") != 0)
                    goto oom;
                if (tmp != NULL) {
                    if (strcasecmp(tmp, "JPEG") == 0)
                        service_ctx.profiles[service_ctx.profiles_num - 1].type = JPEG;
                    else if (strcasecmp(tmp, "MPEG4") == 0)
                        service_ctx.profiles[service_ctx.profiles_num - 1].type = MPEG4;
                    else if (strcasecmp(tmp, "H264") == 0)
                        service_ctx.profiles[service_ctx.profiles_num - 1].type = H264;
                    else if (strcasecmp(tmp, "H265") == 0)
                        service_ctx.profiles[service_ctx.profiles_num - 1].type = H265;
                    free(tmp);
                }
                tmp = NULL;
                if (get_string_from_json(&tmp, item, "audio_encoder") != 0)
                    goto oom;
                if (tmp != NULL) {
                    if (strcasecmp(tmp, "NONE") == 0)
                        service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = AUDIO_NONE;
                    else if (strcasecmp(tmp, "G711") == 0)
                        service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = G711;
                    else if (strcasecmp(tmp, "G726") == 0)
                        service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = G726;
                    else if (strcasecmp(tmp, "AAC") == 0)
                        service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = AAC;
                    free(tmp);
                }
                tmp = NULL;
                if (get_string_from_json(&tmp, item, "audio_decoder") != 0)
                    goto oom;
                if (tmp != NULL) {
                    if (strcasecmp(tmp, "NONE") == 0)
                        service_ctx.profiles[service_ctx.profiles_num - 1].audio_decoder = AUDIO_NONE;
                    else if (strcasecmp(tmp, "G711") == 0)
                        service_ctx.profiles[service_ctx.profiles_num - 1].audio_decoder = G711;
                    else if (strcasecmp(tmp, "G726") == 0)
                        service_ctx.profiles[service_ctx.profiles_num - 1].audio_decoder = G726;
                    else if (strcasecmp(tmp, "AAC") == 0)
                        service_ctx.profiles[service_ctx.profiles_num - 1].audio_decoder = AAC;
                    free(tmp);
                }

                // Print debug
                log_debug("Profile %d", service_ctx.profiles_num - 1);
                log_debug("\tname: %s", service_ctx.profiles[service_ctx.profiles_num - 1].name);
                log_debug("\twidth: %d", service_ctx.profiles[service_ctx.profiles_num - 1].width);
                log_debug("\theight: %d", service_ctx.profiles[service_ctx.profiles_num - 1].height);
                log_debug("\turl: %s", service_ctx.profiles[service_ctx.profiles_num - 1].url);
                log_debug("\tsnapurl: %s", service_ctx.profiles[service_ctx.profiles_num - 1].snapurl);
                log_debug("\ttype: %d", service_ctx.profiles[service_ctx.profiles_num - 1].type);
                log_debug("\tframerate: %d", service_ctx.profiles[service_ctx.profiles_num - 1].framerate);
                log_debug("\taudio_encoder: %d", service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder);
                log_debug("\taudio_decoder: %d", service_ctx.profiles[service_ctx.profiles_num - 1].audio_decoder);
                log_debug("");
            }
        }
    }

    // Load PTZ configuration from main configuration file
    if (json_object_object_get_ex(json_file, "ptz", &value)) {
        log_debug("Found PTZ section in configuration");

        get_int_from_json(&(service_ctx.ptz_node.enable), value, "enable");
        service_ctx.ptz_node.zoom_enable = -1;
        if (json_object_object_get_ex(value, "zoom", &item)) {
            if (json_object_is_type(item, json_type_boolean)) {
                service_ctx.ptz_node.zoom_enable = json_object_get_boolean(item) ? 1 : 0;
            } else if (json_object_is_type(item, json_type_int)) {
                service_ctx.ptz_node.zoom_enable = json_object_get_int(item) ? 1 : 0;
            }
        }
        get_double_from_json(&(service_ctx.ptz_node.min_step_x), value, "min_step_x");
        get_double_from_json(&(service_ctx.ptz_node.max_step_x), value, "max_step_x");
        get_double_from_json(&(service_ctx.ptz_node.min_step_y), value, "min_step_y");
        get_double_from_json(&(service_ctx.ptz_node.max_step_y), value, "max_step_y");
        get_double_from_json(&(service_ctx.ptz_node.min_step_z), value, "min_step_z");
        get_double_from_json(&(service_ctx.ptz_node.max_step_z), value, "max_step_z");
        if (get_string_from_json(&(service_ctx.ptz_node.get_position), value, "get_position") != 0)
            goto oom;
        if (get_string_from_json(&(service_ctx.ptz_node.is_moving), value, "is_moving") != 0)
            goto oom;
        if (get_string_from_json(&(service_ctx.ptz_node.move_left), value, "move_left") != 0)
            goto oom;
        if (get_string_from_json(&(service_ctx.ptz_node.move_right), value, "move_right") != 0)
            goto oom;
        if (get_string_from_json(&(service_ctx.ptz_node.move_up), value, "move_up") != 0)
            goto oom;
        if (get_string_from_json(&(service_ctx.ptz_node.move_down), value, "move_down") != 0)
            goto oom;
        if (get_string_from_json(&(service_ctx.ptz_node.move_in), value, "move_in") != 0)
            goto oom;
        if (get_string_from_json(&(service_ctx.ptz_node.move_out), value, "move_out") != 0)
            goto oom;
        if (get_string_from_json(&(service_ctx.ptz_node.move_stop), value, "move_stop") != 0)
            goto oom;
        if (get_string_from_json(&(service_ctx.ptz_node.move_preset), value, "move_preset") != 0)
            goto oom;
        if (get_string_from_json(&(service_ctx.ptz_node.goto_home_position), value, "goto_home_position") != 0)
            goto oom;
        if (get_string_from_json(&(service_ctx.ptz_node.set_preset), value, "set_preset") != 0)
            goto oom;
        if (get_string_from_json(&(service_ctx.ptz_node.set_home_position), value, "set_home_position") != 0)
            goto oom;
        if (get_string_from_json(&(service_ctx.ptz_node.remove_preset), value, "remove_preset") != 0)
            goto oom;
        if (get_string_from_json(&(service_ctx.ptz_node.jump_to_abs), value, "jump_to_abs") != 0)
            goto oom;
        if (get_string_from_json(&(service_ctx.ptz_node.jump_to_rel), value, "jump_to_rel") != 0)
            goto oom;
        if (get_string_from_json(&(service_ctx.ptz_node.get_presets), value, "get_presets") != 0)
            goto oom;

        // Print debug
        log_debug("enable: %d", service_ctx.ptz_node.enable);
        log_debug("zoom: %d", service_ctx.ptz_node.zoom_enable);
        log_debug("min_step_x: %.1f", service_ctx.ptz_node.min_step_x);
        log_debug("max_step_x: %.1f", service_ctx.ptz_node.max_step_x);
        log_debug("min_step_y: %.1f", service_ctx.ptz_node.min_step_y);
        log_debug("max_step_y: %.1f", service_ctx.ptz_node.max_step_y);
        log_debug("min_step_z: %.1f", service_ctx.ptz_node.min_step_z);
        log_debug("max_step_z: %.1f", service_ctx.ptz_node.max_step_z);
        log_debug("get_position: %s", service_ctx.ptz_node.get_position);
        log_debug("is_moving: %s", service_ctx.ptz_node.is_moving);
        log_debug("move_left: %s", service_ctx.ptz_node.move_left);
        log_debug("move_right: %s", service_ctx.ptz_node.move_right);
        log_debug("move_up: %s", service_ctx.ptz_node.move_up);
        log_debug("move_down: %s", service_ctx.ptz_node.move_down);
        log_debug("move_in: %s", service_ctx.ptz_node.move_in);
        log_debug("move_out: %s", service_ctx.ptz_node.move_out);
        log_debug("move_stop: %s", service_ctx.ptz_node.move_stop);
        log_debug("move_preset: %s", service_ctx.ptz_node.move_preset);
        log_debug("goto_home_position: %s", service_ctx.ptz_node.goto_home_position);
        log_debug("set_preset: %s", service_ctx.ptz_node.set_preset);
        log_debug("set_home_position: %s", service_ctx.ptz_node.set_home_position);
        log_debug("remove_preset: %s", service_ctx.ptz_node.remove_preset);
        log_debug("jump_to_abs: %s", service_ctx.ptz_node.jump_to_abs);
        log_debug("jump_to_rel: %s", service_ctx.ptz_node.jump_to_rel);
        log_debug("get_presets: %s", service_ctx.ptz_node.get_presets);
        log_debug("");
    }

    // Load relays configuration from main configuration file
    if (json_object_object_get_ex(json_file, "relays", &value)) {
        if (json_object_is_type(value, json_type_array)) {
            size_t array_len = json_object_array_length(value);
            log_debug("Found %zu relay entries in configuration", array_len);
            for (size_t i = 0; i < array_len; i++) {
                item = json_object_array_get_idx(value, i);
                if (!item)
                    continue;

                service_ctx.relay_outputs_num++;
                if (service_ctx.relay_outputs_num >= MAX_RELAY_OUTPUTS) {
                    log_error("Ignore relay, too many relay outputs, max is: %d", MAX_RELAY_OUTPUTS);
                    service_ctx.relay_outputs_num--;
                } else {
                    grown = grow(service_ctx.relay_outputs, service_ctx.relay_outputs_num, sizeof(relay_output_t));
                    if (grown == NULL) {
                        service_ctx.relay_outputs_num--;
                        goto oom;
                    }
                    service_ctx.relay_outputs = grown;

                    // Init variables before reading
                    service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].idle_state = IDLE_STATE_CLOSE;
                    service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].close = NULL;
                    service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].open = NULL;

                    tmp = NULL;
                    if (get_string_from_json(&tmp, item, "idle_state") != 0)
                        goto oom;
                    if (tmp != NULL) {
                        if (strcasecmp(tmp, "close") == 0) {
                            service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].idle_state = IDLE_STATE_CLOSE;
                        } else {
                            service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].idle_state = IDLE_STATE_OPEN;
                        }
                        free(tmp);
                    }
                    if (get_string_from_json(&(service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].close), item, "close") != 0)
                        goto oom;
                    if (get_string_from_json(&(service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].open), item, "open") != 0)
                        goto oom;

                    // Print debug
                    log_debug("Relay: %d", service_ctx.relay_outputs_num - 1);
                    log_debug("\tidle_state: %d", service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].idle_state);
                    log_debug("\tclose: %s", service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].close);
                    log_debug("\topen: %s", service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].open);
                    log_debug("");
                }
            }
        }
    }

    // Load events configuration from main configuration file
    if (json_object_object_get_ex(json_file, "events", &value)) {
        get_int_from_json(&(service_ctx.events_enable), value, "enable");

        log_debug("Found Events section in configuration");
        log_debug("Events");
        log_debug("\tenable: %d", service_ctx.events_enable);
        log_debug("");

        if (json_object_object_get_ex(json_file, "events", &value)) {
            if (value && json_object_is_type(value, json_type_array)) {
                size_t array_len = json_object_array_length(value);
                for (size_t i = 0; i < array_len; i++) {
                    item = json_object_array_get_idx(value, i);
                    if (!item)
                        continue;

                    service_ctx.events_num++;
                    if (service_ctx.events_num >= MAX_EVENTS) {
                        log_error("Ignore event, too many events, max is: %d", MAX_EVENTS);
                        service_ctx.events_num--;
                    } else {
                        grown = grow(service_ctx.events, service_ctx.events_num, sizeof(event_t));
                        if (grown == NULL) {
                            service_ctx.events_num--;
                            goto oom;
                        }
                        service_ctx.events = grown;

                        // Init variables before reading
                        service_ctx.events[service_ctx.events_num - 1].topic = NULL;
                        service_ctx.events[service_ctx.events_num - 1].source_name = NULL;
                        service_ctx.events[service_ctx.events_num - 1].source_type = NULL;
                        service_ctx.events[service_ctx.events_num - 1].source_value = NULL;
                        service_ctx.events[service_ctx.events_num - 1].input_file = NULL;

                        if (get_string_from_json(&(service_ctx.events[service_ctx.events_num - 1].topic), item, "topic") != 0)

                            goto oom;
                        if (get_string_from_json(&(service_ctx.events[service_ctx.events_num - 1].source_name), item, "source_name") != 0)
                            goto oom;
                        if (get_string_from_json(&(service_ctx.events[service_ctx.events_num - 1].source_type), item, "source_type") != 0)
                            goto oom;
                        if (get_string_from_json(&(service_ctx.events[service_ctx.events_num - 1].source_value), item, "source_value") != 0)
                            goto oom;
                        if (get_string_from_json(&(service_ctx.events[service_ctx.events_num - 1].input_file), item, "input_file") != 0)
                            goto oom;

                        // Print debug
                        log_debug("Event: %d", service_ctx.events_num - 1);
                        log_debug("\ttopic: %s", service_ctx.events[service_ctx.events_num - 1].topic);
                        log_debug("\tsource_name: %s", service_ctx.events[service_ctx.events_num - 1].source_name);
                        log_debug("\tsource_type: %s", service_ctx.events[service_ctx.events_num - 1].source_type);
                        log_debug("\tsource_value: %s", service_ctx.events[service_ctx.events_num - 1].source_value);
                        log_debug("\tinput_file: %s", service_ctx.events[service_ctx.events_num - 1].input_file);
                        log_debug("");
                    }
                }
            }
        }
    }

    if (service_ctx.relay_outputs != NULL) {
        if (service_ctx.events_enable == EVENTS_NONE) service_ctx.events_enable = EVENTS_PULLPOINT;

        for (i = 0; i < service_ctx.relay_outputs_num; i++) {
            service_ctx.events_num++;
            if (service_ctx.events_num > MAX_EVENTS) {
                log_error("Unable to add relay event, too many events, max is: %d", MAX_EVENTS);
                return -2;
            }
            log_debug("Adding event for relay output %d", i);
            grown = grow(service_ctx.events, service_ctx.events_num, sizeof(event_t));
            if (grown == NULL) {
                service_ctx.events_num--;
                goto oom;
            }
            service_ctx.events = grown;
            if (dup_str(&service_ctx.events[service_ctx.events_num - 1].topic, "tns1:Device/Trigger/Relay") != 0)
                goto oom;
            log_debug("topic: tns1:Device/Trigger/Relay");
            if (dup_str(&service_ctx.events[service_ctx.events_num - 1].source_name, "RelayToken") != 0)
                goto oom;
            log_debug("source_name: RelayToken");
            if (dup_str(&service_ctx.events[service_ctx.events_num - 1].source_type, "tt:ReferenceToken") != 0)
                goto oom;
            log_debug("source_type: tt:ReferenceToken");
            snprintf(stmp, sizeof(stmp), "RelayOutputToken_%d", i);
            if (dup_str(&service_ctx.events[service_ctx.events_num - 1].source_value, stmp) != 0)
                goto oom;
            log_debug("source_value: %s", stmp);
            snprintf(stmp, sizeof(stmp), "/tmp/onvif_notify_server/relay_output_%d", i);
            if (dup_str(&service_ctx.events[service_ctx.events_num - 1].input_file, stmp) != 0)
                goto oom;
            log_debug("input_file: %s", stmp);
        }
    }

    // Load imaging configuration from main configuration file
    if (get_string_from_json(&(service_ctx.imaging_set_command), json_file, "imaging_set_command") != 0)
        goto oom;
    if (get_string_from_json(&(service_ctx.imaging_focus_command), json_file, "imaging_focus_command") != 0)
        goto oom;
    if (get_string_from_json(&(service_ctx.imaging_status_command), json_file, "imaging_status_command") != 0)
        goto oom;
    get_bool_from_json_opt(&(service_ctx.imaging_focus_auto), json_file, "imaging_focus_auto");
    get_number_from_json_opt(&(service_ctx.imaging_brightness), json_file, "imaging_brightness");
    get_number_from_json_opt(&(service_ctx.imaging_contrast), json_file, "imaging_contrast");
    get_number_from_json_opt(&(service_ctx.imaging_saturation), json_file, "imaging_saturation");
    get_number_from_json_opt(&(service_ctx.imaging_sharpness), json_file, "imaging_sharpness");
    get_bool_from_json_opt(&(service_ctx.imaging_ircut_on), json_file, "imaging_ircut_on");
    get_bool_from_json_opt(&(service_ctx.imaging_wb_auto), json_file, "imaging_wb_auto");
    get_bool_from_json_opt(&(service_ctx.imaging_exposure_auto), json_file, "imaging_exposure_auto");
    get_bool_from_json_opt(&(service_ctx.imaging_wdr_on), json_file, "imaging_wdr_on");

    // Print debug
    log_debug("imaging_set_command: %s", service_ctx.imaging_set_command);
    log_debug("imaging_focus_command: %s", service_ctx.imaging_focus_command);
    log_debug("imaging_status_command: %s", service_ctx.imaging_status_command);
    log_debug("imaging_focus_auto: %d", service_ctx.imaging_focus_auto);
    log_debug("imaging_brightness: %.4f", service_ctx.imaging_brightness);
    log_debug("imaging_contrast: %.4f", service_ctx.imaging_contrast);
    log_debug("imaging_saturation: %.4f", service_ctx.imaging_saturation);
    log_debug("imaging_sharpness: %.4f", service_ctx.imaging_sharpness);
    log_debug("imaging_ircut_on: %d", service_ctx.imaging_ircut_on);
    log_debug("imaging_wb_auto: %d", service_ctx.imaging_wb_auto);
    log_debug("imaging_exposure_auto: %d", service_ctx.imaging_exposure_auto);
    log_debug("imaging_wdr_on: %d", service_ctx.imaging_wdr_on);

    json_object_put(json_file);
    free(buffer);

    return 0;

oom:
    json_object_put(json_file);
    free(buffer);
    return -2;
}

void free_conf_file()
{
    int i;

    if (service_ctx.events_enable == 1) {
        for (i = service_ctx.events_num - 1; i >= 0; i--) {
            if (service_ctx.events[i].input_file != NULL) free(service_ctx.events[i].input_file);
            if (service_ctx.events[i].source_value != NULL) free(service_ctx.events[i].source_value);
            if (service_ctx.events[i].source_type != NULL) free(service_ctx.events[i].source_type);
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
        if (service_ctx.ptz_node.move_out != NULL) free(service_ctx.ptz_node.move_out);
        if (service_ctx.ptz_node.move_in != NULL) free(service_ctx.ptz_node.move_in);
        if (service_ctx.ptz_node.move_down != NULL) free(service_ctx.ptz_node.move_down);
        if (service_ctx.ptz_node.move_up != NULL) free(service_ctx.ptz_node.move_up);
        if (service_ctx.ptz_node.move_right != NULL) free(service_ctx.ptz_node.move_right);
        if (service_ctx.ptz_node.move_left != NULL) free(service_ctx.ptz_node.move_left);
        if (service_ctx.ptz_node.is_moving != NULL) free(service_ctx.ptz_node.is_moving);
        if (service_ctx.ptz_node.get_position != NULL) free(service_ctx.ptz_node.get_position);
    }

    for (i = service_ctx.relay_outputs_num - 1; i >= 0; i--) {
        if (service_ctx.relay_outputs[i].open != NULL) free(service_ctx.relay_outputs[i].open);
        if (service_ctx.relay_outputs[i].close != NULL) free(service_ctx.relay_outputs[i].close);
    }
    if (service_ctx.relay_outputs != NULL) free(service_ctx.relay_outputs);

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
    if (service_ctx.factory_default_command != NULL) free(service_ctx.factory_default_command);
    if (service_ctx.password != NULL) free(service_ctx.password);
    if (service_ctx.user != NULL) free(service_ctx.user);
    if (service_ctx.imaging_set_command != NULL) free(service_ctx.imaging_set_command);
    if (service_ctx.imaging_focus_command != NULL) free(service_ctx.imaging_focus_command);
    if (service_ctx.imaging_status_command != NULL) free(service_ctx.imaging_status_command);
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
    fprintf(stderr, "\t#Imaging service: 1 to advertise it\n");
    fprintf(stderr, "\tadv_enable_imaging=0\n");
    fprintf(stderr, "\tadv_empty_if_unknown=0\n");
    fprintf(stderr, "\tadv_fault_if_unknown=0\n");
    fprintf(stderr, "\tadv_fault_if_set=0\n");
    fprintf(stderr, "\tadv_synology_nvr=0\n");
    fprintf(stderr, "\t#Shell command run on SetSystemFactoryDefault (empty = operation not supported)\n");
    fprintf(stderr, "\tfactory_default_command=\n");
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
    fprintf(stderr, "\tzoom=1\n");
    fprintf(stderr, "\tmin_step_x=0\n");
    fprintf(stderr, "\tmax_step_x=360\n");
    fprintf(stderr, "\tmin_step_y=0\n");
    fprintf(stderr, "\tmax_step_y=180\n");
    fprintf(stderr, "\tmin_step_z=0\n");
    fprintf(stderr, "\tmax_step_z=0\n");
    fprintf(stderr, "\tget_position=/tmp/sd/yi-hack/bin/ipc_cmd -g\n");
    fprintf(stderr, "\tis_moving=/tmp/sd/yi-hack/bin/ipc_cmd -u\n");
    fprintf(stderr, "\tmove_left=/tmp/sd/yi-hack/bin/ipc_cmd -m left -s %%f\n");
    fprintf(stderr, "\tmove_right=/tmp/sd/yi-hack/bin/ipc_cmd -m right -s %%f\n");
    fprintf(stderr, "\tmove_up=/tmp/sd/yi-hack/bin/ipc_cmd -m up -s %%f\n");
    fprintf(stderr, "\tmove_down=/tmp/sd/yi-hack/bin/ipc_cmd -m down -s %%f\n");
    fprintf(stderr, "\tmove_in=/tmp/sd/yi-hack/bin/ipc_cmd -m in -s %%f\n");
    fprintf(stderr, "\tmove_out=/tmp/sd/yi-hack/bin/ipc_cmd -m out -s %%f\n");
    fprintf(stderr, "\tmove_stop=/tmp/sd/yi-hack/bin/ipc_cmd -m stop -t %%s\n");
    fprintf(stderr, "\tmove_preset=/tmp/sd/yi-hack/bin/ipc_cmd -p %%d\n");
    fprintf(stderr, "\tgoto_home_position=/tmp/sd/yi-hack/bin/ipc_cmd -p 0\n");
    fprintf(stderr, "\tset_preset=/tmp/sd/yi-hack/script/ptz_presets.sh -a add_preset -n %%d -m %%s\n");
    fprintf(stderr, "\tset_home_position=/tmp/sd/yi-hack/bin/ipc_cmd -H\n");
    fprintf(stderr, "\tremove_preset=/tmp/sd/yi-hack/script/ptz_presets.sh -a del_preset -n %%d\n");
    fprintf(stderr, "\tjump_to_abs=/tmp/sd/yi-hack/bin/ipc_cmd -j %%f,%%f,%%f\n");
    fprintf(stderr, "\tjump_to_rel=/tmp/sd/yi-hack/bin/ipc_cmd -J %%f,%%f,%%f\n");
    fprintf(stderr, "\tget_presets=/tmp/sd/yi-hack/script/ptz_presets.sh -a get_presets\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t#RELAY OUTPUTS\n");
    fprintf(stderr, "\t#Relay 0\n");
    fprintf(stderr, "\tidle_state=open\n");
    fprintf(stderr, "\tclose=/usr/local/bin/set_relay -n 0 -a close\n");
    fprintf(stderr, "\topen=/usr/local/bin/set_relay -n 0 -a open\n");
    fprintf(stderr, "\t#Relay 1\n");
    fprintf(stderr, "\tidle_state=open\n");
    fprintf(stderr, "\tclose=/usr/local/bin/set_relay -n 1 -a close\n");
    fprintf(stderr, "\topen=/usr/local/bin/set_relay -n 1 -a open\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t#EVENTS\n");
    fprintf(stderr, "\tevents=1\n");
    fprintf(stderr, "\t#Event 0\n");
    fprintf(stderr, "\ttopic=tns1:VideoSource/MotionAlarm\n");
    fprintf(stderr, "\tsource_name=VideoSourceConfigurationToken\n");
    fprintf(stderr, "\tsource_type=tt:ReferenceToken\n");
    fprintf(stderr, "\tsource_value=VideoSourceToken\n");
    fprintf(stderr, "\tinput_file=/tmp/motion_alarm\n");
    fprintf(stderr, "\t#Event 1\n");
    fprintf(stderr, "\ttopic=tns1:RuleEngine/MyRuleDetector/PeopleDetect\n");
    fprintf(stderr, "\tsource_name=VideoSourceConfigurationToken\n");
    fprintf(stderr, "\tsource_type=xsd:string\n");
    fprintf(stderr, "\tsource_value=VideoSourceToken\n");
    fprintf(stderr, "\tinput_file=/tmp/human_detection\n");
    fprintf(stderr, "\t#Event 2\n");
    fprintf(stderr, "\ttopic=tns1:RuleEngine/MyRuleDetector/VehicleDetect\n");
    fprintf(stderr, "\tsource_name=VideoSourceConfigurationToken\n");
    fprintf(stderr, "\tsource_type=xsd:string\n");
    fprintf(stderr, "\tsource_value=VideoSourceToken\n");
    fprintf(stderr, "\tinput_file=/tmp/vehicle_detection\n");
    fprintf(stderr, "\t#Event 3\n");
    fprintf(stderr, "\ttopic=tns1:RuleEngine/MyRuleDetector/DogCatDetect\n");
    fprintf(stderr, "\tsource_name=VideoSourceConfigurationToken\n");
    fprintf(stderr, "\tsource_type=xsd:string\n");
    fprintf(stderr, "\tsource_value=VideoSourceToken\n");
    fprintf(stderr, "\tinput_file=/tmp/animal_detection\n");
    fprintf(stderr, "\t#Event 4\n");
    fprintf(stderr, "\ttopic=tns1:RuleEngine/MyRuleDetector/BabyCryingDetect\n");
    fprintf(stderr, "\tsource_name=AudioSourceConfigurationToken\n");
    fprintf(stderr, "\tsource_type=xsd:string\n");
    fprintf(stderr, "\tsource_value=AudioSourceToken\n");
    fprintf(stderr, "\tinput_file=/tmp/baby_crying\n");
    fprintf(stderr, "\t#Event 5\n");
    fprintf(stderr, "\ttopic=tns1:AudioAnalytics/Audio/DetectedSound\n");
    fprintf(stderr, "\tsource_name=AudioSourceConfigurationToken\n");
    fprintf(stderr, "\tsource_type=tt:ReferenceToken\n");
    fprintf(stderr, "\tsource_value=AudioSourceToken\n");
    fprintf(stderr, "\tinput_file=/tmp/sound_detection\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t#IMAGING\n");
    fprintf(stderr, "\t#Command run on SetImagingSettings (empty = read-only)\n");
    fprintf(stderr, "\timaging_set_command=\n");
    fprintf(stderr, "\t#Command run on imaging Move/Stop, speed in ONVIF_IMAGING_FOCUS_SPEED (empty = no focus control)\n");
    fprintf(stderr, "\timaging_focus_command=\n");
    fprintf(stderr, "\t#Command printing the lens state as \"<position> <IDLE|MOVING|UNKNOWN>\" for GetStatus (empty = UNKNOWN)\n");
    fprintf(stderr, "\timaging_status_command=\n");
    fprintf(stderr, "\timaging_focus_auto=1\n");
    fprintf(stderr, "\timaging_brightness=0.5\n");
    fprintf(stderr, "\timaging_contrast=0.5\n");
    fprintf(stderr, "\timaging_saturation=0.5\n");
    fprintf(stderr, "\timaging_sharpness=0.5\n");
    fprintf(stderr, "\timaging_ircut_on=1\n");
    fprintf(stderr, "\timaging_wb_auto=1\n");
    fprintf(stderr, "\timaging_exposure_auto=1\n");
    fprintf(stderr, "\timaging_wdr_on=0\n");
}
