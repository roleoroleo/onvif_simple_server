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
#include <unistd.h>
#include <time.h>

#include "ptz_service.h"
#include "fault.h"
#include "utils.h"
#include "log.h"
#include "ezxml_wrapper.h"
#include "onvif_simple_server.h"

extern service_context_t service_ctx;
presets_t presets;

int init_presets()
{
    FILE *fp;
    char out[MAX_LEN];
    int i, num;
    double x, y, z;
    char name[MAX_LEN];
    char *p;

    presets.count = 0;
    presets.items = (preset_t *) malloc(sizeof(preset_t));

    // Run command that returns to stdout the list of the presets in the form number=name,pan,tilt,zoom (zoom is optional)
    if (service_ctx.ptz_node.get_presets == NULL) {
        pclose(fp);
        return -1;
    }
    fp = popen(service_ctx.ptz_node.get_presets, "r");
    if (fp == NULL) {
        return -2;
    } else {
        while (fgets(out, sizeof(out), fp)) {
            p = out;
            name[0] = '\0';
            x = -1.0;
            y = -1.0;
            z = 1.0;
            while((p = strchr(p, ',')) != NULL) {
                *p++ = ' ';
            }
            if (sscanf(out, "%d=%s %lf %lf %lf", &num, name, &x, &y, &z) == 0) {
                pclose(fp);
                return -3;
            } else {
                if (strlen(name) != 0) {
                    presets.count++;
                    presets.items = (preset_t *) realloc(presets.items, sizeof(preset_t) * presets.count);
                    presets.items[presets.count - 1].name = (char *) malloc(strlen(name) + 1);
                    strcpy(presets.items[presets.count - 1].name, name);
                    presets.items[presets.count - 1].number = num;
                    presets.items[presets.count - 1].x = x;
                    presets.items[presets.count - 1].y = y;
                    presets.items[presets.count - 1].z = z;
                }
            }
        }
        pclose(fp);
    }

    for (i = 0; i < presets.count; i++) {
        if (presets.items[i].name != NULL) {
            log_debug("Preset %d - %d - %s - %f - %f", i, presets.items[i].number,
                    presets.items[i].name, presets.items[i].x, presets.items[i].y);
        }
    }

    return 0;
}

void destroy_presets()
{
    int i;

    for (i = presets.count - 1; i >= 0; i--) {
        free(presets.items[i].name);
    }
    free(presets.items);
}

int ptz_get_service_capabilities()
{
    char move_status[8], status_position[8];

    if (service_ctx.ptz_node.is_moving != NULL) {
        strcpy(move_status, "true");
    } else {
        strcpy(move_status, "false");
    }
    if (service_ctx.ptz_node.get_position != NULL) {
        strcpy(status_position, "true");
    } else {
        strcpy(status_position, "false");
    }

    long size = cat(NULL, "ptz_service_files/GetServiceCapabilities.xml", 4,
            "%MOVE_STATUS%", move_status,
            "%STATUS_POSITION%", status_position);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/GetServiceCapabilities.xml", 4,
            "%MOVE_STATUS%", move_status,
            "%STATUS_POSITION%", status_position);
}

int ptz_get_configurations()
{
    long size = cat(NULL, "ptz_service_files/GetConfigurations.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/GetConfigurations.xml", 0);
}

int ptz_get_configuration()
{
    long size = cat(NULL, "ptz_service_files/GetConfiguration.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/GetConfiguration.xml", 0);
}

int ptz_get_configuration_options()
{
    long size = cat(NULL, "ptz_service_files/GetConfigurationOptions.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/GetConfigurationOptions.xml", 0);
}

int ptz_get_nodes()
{
    long size = cat(NULL, "ptz_service_files/GetNodes.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/GetNodes.xml", 0);
}

int ptz_get_node()
{
    const char *node_token = get_element("NodeToken", "Body");
    if (strcmp("PTZNodeToken", node_token) != 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoEntity", "No entity", "No such node on the device");
        return -1;
    }

    long size = cat(NULL, "ptz_service_files/GetNode.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/GetNode.xml", 0);
}

int ptz_get_presets()
{
    ezxml_t node;
    int i, c;
    char dest_a[] = "stdout";
    char *dest;
    char token[16];
    char sx[16], sy[16], sz[16];
    long size, total_size;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoPTZProfile", "No PTZ profile", "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    init_presets();

    // We need 1st step to evaluate content length
    for (c = 0; c < 2; c++) {
        if (c == 0) {
            dest = NULL;
        } else {
            dest = dest_a;
            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", total_size);
        }

        size = cat(dest, "ptz_service_files/GetPresets_1.xml", 0);
        if (c == 0) total_size = size;

        for (i = 0; i < presets.count; i++) {
            sprintf(token, "PresetToken_%d", presets.items[i].number);
            sprintf(sx, "%.1f", presets.items[i].x);
            sprintf(sy, "%.1f", presets.items[i].y);
            sprintf(sz, "%.1f", presets.items[i].z);
            size = cat(dest, "ptz_service_files/GetPresets_2.xml", 10,
                "%TOKEN%", token,
                "%NAME%", presets.items[i].name,
                "%X%", sx,
                "%Y%", sy,
                "%Z%", sz);
            if (c == 0) total_size += size;
        }

        size = cat(dest, "ptz_service_files/GetPresets_3.xml", 0);
        if (c == 0) total_size += size;
    }

    destroy_presets();

    return total_size;
}

int ptz_goto_preset()
{
    const char *preset_token;
    int i, preset_number, count, found;
    char sys_command[MAX_LEN];
    ezxml_t node;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoPTZProfile", "No PTZ profile", "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    preset_token = get_element("PresetToken", "Body");
    if (sscanf(preset_token, "PresetToken_%d", &preset_number) != 1) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "The requested preset token does not exist");
        return -3;
    }

    init_presets();
    count = presets.count;
    found = 0;
    for (i = 0; i < presets.count; i++) {
        if (presets.items[i].number == preset_number) {
            found = 1;
            break;
        }
    }
    destroy_presets();

    if (found == 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "The requested preset token does not exist");
        return -4;
    }
    if (service_ctx.ptz_node.move_preset == NULL) {
        send_action_failed_fault("ptz_service", -5);
        return -5;
    }

    sprintf(sys_command, service_ctx.ptz_node.move_preset, preset_number);
    system(sys_command);
    long size = cat(NULL, "ptz_service_files/GotoPreset.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/GotoPreset.xml", 0);
}

int ptz_goto_home_position()
{
    char sys_command[MAX_LEN];
    ezxml_t node;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoPTZProfile", "No PTZ profile", "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    init_presets();
    if (presets.count == 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoHomePosition", "No home position", "No home position has been defined for this profile");
        return -3;
    }

    if (service_ctx.ptz_node.move_preset == NULL) {
        send_action_failed_fault("ptz_service", -4);
        return -4;
    }
    strcpy(sys_command, service_ctx.ptz_node.goto_home_position);
    system(sys_command);

    destroy_presets();

    long size = cat(NULL, "ptz_service_files/GotoHomePosition.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/GotoHomePosition.xml", 0);
}

int ptz_continuous_move()
{
    const char *x = NULL;
    const char *y = NULL;
    const char *z = NULL;
    double dx, dy;
    char sys_command[MAX_LEN];
    int ret = -1;
    ezxml_t node;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoPTZProfile", "No PTZ profile", "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    node = get_element_ptr(NULL, "Velocity", "Body");
    if (node != NULL) {
        node = get_element_ptr(node, "PanTilt", "Body");
        if (node != NULL) {
            x = get_attribute(node, "x");
            y = get_attribute(node, "y");
        }
        node = get_element_ptr(node, "Zoom", "Body");
        if (node != NULL) {
            z = get_attribute(node, "x");
        }
    }

    if (x != NULL) {
        dx = atof(x);

        if (dx > 0.0) {
            if (service_ctx.ptz_node.move_right == NULL) {
                send_action_failed_fault("ptz_service", -3);
                return -3;
            }
            strcpy(sys_command, service_ctx.ptz_node.move_right);
            system(sys_command);
            ret = 0;
        } else if (dx < 0.0) {
            if (service_ctx.ptz_node.move_left == NULL) {
                send_action_failed_fault("ptz_service", -4);
                return -4;
            }
            strcpy(sys_command, service_ctx.ptz_node.move_left);
            system(sys_command);
            ret = 0;
        }
    }

    if (y != NULL) {
        dy = atof(y);

        if (dy > 0.0) {
            if (service_ctx.ptz_node.move_up == NULL) {
                send_action_failed_fault("ptz_service", -5);
                return -5;
            }
            strcpy(sys_command, service_ctx.ptz_node.move_up);
            system(sys_command);
            ret = 0;
        } else if (dy < 0.0) {
            if (service_ctx.ptz_node.move_down == NULL) {
                send_action_failed_fault("ptz_service", -6);
                return -6;
            }
            strcpy(sys_command, service_ctx.ptz_node.move_down);
            system(sys_command);
            ret = 0;
        }
    }

    if (z != NULL) {
        ret = 0;
    }

    long size = cat(NULL, "ptz_service_files/ContinuousMove.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/ContinuousMove.xml", 0);
}

int ptz_relative_move()
{
    char const *x = NULL;
    char const *y = NULL;
    char const *z = NULL;
    char const *space_p = NULL;
    char const *space_z = NULL;
    double dx = 0;
    double dy = 0;
    double dz = 0;
    char sys_command_tmp[MAX_LEN];
    char sys_command[MAX_LEN];
    int ret = 0;
    ezxml_t node, node_p, node_z;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoPTZProfile", "No PTZ profile", "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    if (service_ctx.ptz_node.jump_to_rel == NULL) {
        send_action_failed_fault("ptz_service", -3);
        return -3;
    }

    node = get_element_ptr(NULL, "Translation", "Body");
    if (node != NULL) {
        node_p = get_element_in_element_ptr("PanTilt", node);
        if (node_p != NULL) {
            x = get_attribute(node_p, "x");
            y = get_attribute(node_p, "y");
            space_p = get_attribute(node_p, "space");
        }
        node_z = get_element_in_element_ptr("Zoom", node);
        if (node_z != NULL) {
            z = get_attribute(node_z, "x");
            space_z = get_attribute(node_z, "space");
        }
    }

    node = get_element_ptr(NULL, "Speed", "Body");
    if (node != NULL) {
        // do nothing
    }

    if (node_p != NULL) {
        if ((space_p == NULL) || (strcmp("http://www.onvif.org/ver10/tptz/PanTiltSpaces/TranslationGenericSpace", space_p) == 0)) {

            if (((x == NULL) && (y == NULL)) ||
                    ((x == NULL) && (y != NULL)) ||
                    ((x != NULL) && (y == NULL))) {

                ret = -4;

            } else {
                if ((x != NULL) && (y != NULL)) {
                    dx = atof(x);
                    if ((dx > 360.0) || (dx < -360.0)) {
                        ret = -5;
                    }
                    dy = atof(y);
                    if ((dy > 180.0) || (dy < -180.0)) {
                        ret = -6;
                    } else {
                        sprintf(sys_command, service_ctx.ptz_node.jump_to_rel, dx, dy);
                    }
                }
                if (z != NULL) {
                    dz = atof(z);
                    if (dz != 0.0) {
                        // Ignore wrong zoom values
                        // It should be = 0.0
                    } else {
                        // do nothing
                    }
                }
            }
        } else if (strcmp("http://www.onvif.org/ver10/tptz/PanTiltSpaces/TranslationSpaceFov", space_p) == 0) {
            node = get_element_ptr(NULL, "Speed", "Body");
            if (node != NULL) {
                // do nothing
            }

            if (((x == NULL) && (y == NULL) && (z == NULL)) ||
                    ((x == NULL) && (y != NULL)) ||
                    ((x != NULL) && (y == NULL))) {

                ret = -7;

            } else {
                if ((x != NULL) && (y != NULL)) {
                    dx = atof(x);
                    if ((dx > 100.0) || (dx < -100.0)) {
                        ret = -8;
                    }
                    dy = atof(y);
                    if ((dy > 100.0) || (dy < -100.0)) {
                        ret = -9;
                    } else {
                        // Convert -100/+100 to degrees values based on FOV
                        dx = (dx / 100.0) * (63.0 / 2.0);
                        dy = (dy / 100.0) * (37.0 / 2.0);
                        sprintf(sys_command, service_ctx.ptz_node.jump_to_rel, dx, dy);
                    }
                }
                if (z != NULL) {
                    dz = atof(z);
                    if (dz != 0.0) {
                        // Ignore wrong zoom values
                        // It should be = 0.0
                    } else {
                        // do nothing
                    }
                }
            }
        }
    }

    if (node_z != NULL) {
        if (strcmp("http://www.onvif.org/ver10/tptz/ZoomSpaces/TranslationGenericSpace", space_z) == 0) {
            // do nothing
        }
    }

    if (ret == 0) {
        system(sys_command);

        long size = cat(NULL, "ptz_service_files/RelativeMove.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "ptz_service_files/RelativeMove.xml", 0);

    } else {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:InvalidTranslation", "Invalid translation", "The requested translation is out of bounds");
        return ret;
    }
}

int ptz_absolute_move()
{
    char const *x = NULL;
    char const *y = NULL;
    char const *z = NULL;
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;
    char sys_command_tmp[MAX_LEN];
    char sys_command[MAX_LEN];
    int ret = 0;
    ezxml_t node, node_c;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoPTZProfile", "No PTZ profile", "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    if (service_ctx.ptz_node.jump_to_abs == NULL) {
        send_action_failed_fault("ptz_service", -3);
        return -3;
    }

    node = get_element_ptr(NULL, "Position", "Body");
    if (node != NULL) {
        node_c = get_element_in_element_ptr("PanTilt", node);
        if (node_c != NULL) {
            x = get_attribute(node_c, "x");
            y = get_attribute(node_c, "y");
        }
        node_c = get_element_in_element_ptr("Zoom", node);
        if (node_c != NULL) {
            z = get_attribute(node_c, "x");
        }
    }

    node = get_element_ptr(NULL, "Speed", "Body");
    if (node != NULL) {
        // do nothing
    }

    if (((x == NULL) && (y == NULL) && (z == NULL)) ||
            ((x == NULL) && (y != NULL)) ||
            ((x != NULL) && (y == NULL))) {

        ret = -4;

    } else {
        if ((x != NULL) && (y != NULL)) {
            dx = atof(x);
            if ((dx > 360.0) || (dx < 0.0)) {
                ret = -5;
            }
            dy = atof(y);
            if ((dy > 180.0) || (dy < 0.0)) {
                ret = -6;
            } else {
                sprintf(sys_command, service_ctx.ptz_node.jump_to_abs, dx, dy);
            }
        }
        if (z != NULL) {
            dz = atof(z);
            if (dz != 1.0) {
                // Ignore wrong zoom values
                // It should be = 1.0
            } else {
                // do nothing
            }
        }
    }

    if (ret == 0) {
        system(sys_command);

        long size = cat(NULL, "ptz_service_files/AbsoluteMove.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "ptz_service_files/AbsoluteMove.xml", 0);

    } else {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:InvalidPosition", "Invalid position", "The requested position is out of bounds");
        return ret;
    }
}

int ptz_stop()
{
    char sys_command[MAX_LEN];
    ezxml_t node;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoPTZProfile", "No PTZ profile", "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    if (service_ctx.ptz_node.move_stop == NULL) {
        send_action_failed_fault("ptz_service", -3);
        return -3;
    }

    sprintf(sys_command, service_ctx.ptz_node.move_stop);
    system(sys_command);

    long size = cat(NULL, "ptz_service_files/Stop.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/Stop.xml", 0);
}

int ptz_get_status()
{
    char utctime[32];
    time_t timestamp = time(NULL);
    struct tm *tm = gmtime(&timestamp);
    int ret = 0;
    FILE *fp;
    double x, y, z = 1.0;
    int i = 0;
    char out[256], sx[128], sy[128], sz[128], si[128];
    ezxml_t node;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoPTZProfile", "No PTZ profile", "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    sprintf(utctime, "%04d-%02d-%02dT%02d:%02d:%02dZ",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec);

    // Run command that returns to stdout x and y position in the form x,y
    if (service_ctx.ptz_node.get_position != NULL) {
        fp = popen(service_ctx.ptz_node.get_position, "r");
        if (fp == NULL) {
            ret = -3;
        } else {
            if (fgets(out, sizeof(out), fp) == NULL) {
                ret = -4;
            } else {
                if (sscanf(out, "%lf,%lf,%lf", &x, &y, &z) < 2) {
                    ret = -5;
                }
            }
            pclose(fp);
        }
    } else {
        // If the cam doesn't know the status, return a fault
        ret = -6;
    }

    // Run command that returns to stdout if PTZ is moving (1) or not (0)
    if (service_ctx.ptz_node.is_moving != NULL) {
        fp = popen(service_ctx.ptz_node.is_moving, "r");
        if (fp == NULL) {
            ret = -7;
        } else {
            if (fgets(out, sizeof(out), fp) == NULL) {
                ret = -8;
            } else {
                if (sscanf(out, "%d", &i) < 1) {
                    ret = -9;
                }
            }
            pclose(fp);
        }
    } else {
        // If the cam doesn't know the status, return IDLE
        i = 0;
    }

    if (ret == 0) {
        sprintf(sx, "%f", x);
        sprintf(sy, "%f", y);
        sprintf(sz, "%f", z);
        if (i == 1)
            strcpy(si, "MOVING");
        else
            strcpy(si, "IDLE");

        long size = cat(NULL, "ptz_service_files/GetStatus.xml", 12,
                "%X%", sx,
                "%Y%", sy,
                "%Z%", sz,
                "%MOVE_STATUS_PT%", si,
                "%MOVE_STATUS_ZOOM%", "IDLE",
                "%TIME%", utctime);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "ptz_service_files/GetStatus.xml", 12,
                "%X%", sx,
                "%Y%", sy,
                "%Z%", sz,
                "%MOVE_STATUS_PT%", si,
                "%MOVE_STATUS_ZOOM%", "IDLE",
                "%TIME%", utctime);
    } else {
        send_fault("ptz_service", "Receiver", "ter:Action", "ter:NoStatus", "No status", "No PTZ status is available in the requested Media Profile");
        return ret;
    }
}

int ptz_set_preset()
{
    int i;
    char sys_command[MAX_LEN];
    const char *preset_name;
    char preset_name_out[16];
    const char *preset_token;
    ezxml_t node;
    char preset_token_out[16];
    int preset_number = -1;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile token does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoPTZProfile", "No PTZ profile", "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    preset_token = get_element("PresetToken", "Body");
    if (preset_token != NULL) {
        if (sscanf(preset_token, "PresetToken_%s", &preset_number) != 1) {
            send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "The requested preset token does not exist");
            return -3;
        }
        // Update of an existing preset is not supported
        send_action_failed_fault("ptz_service", -4);
        return -4;
    }

    init_presets();
    preset_name = get_element("PresetName", "Body");
    if (preset_name == NULL)
        sprintf(preset_name_out, "PRESET_%d", presets.count);
    else
        strcpy(preset_name_out, preset_name);

    if ((strchr(preset_name_out, ' ') != NULL) || (strlen(preset_name_out) == 0) || (strlen(preset_name_out) > 64)) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:InvalidPresetName", "Invalid preset name", "The preset name is either too long or contains invalid characters");
        return -5;
    }
    for (i = 0; i < presets.count; i++) {
        if (strcasecmp(presets.items[i].name, preset_name_out) == 0) {
            send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:PresetExist", "Preset exists", "The requested name already exist for another preset");
            return -6;
        }
    }
    destroy_presets();

    if (service_ctx.ptz_node.set_preset == NULL) {
        send_action_failed_fault("ptz_service", -7);
        return -7;
    }

    sprintf(sys_command, service_ctx.ptz_node.set_preset, (char *) preset_name_out);
    system(sys_command);
    sleep(1);

    init_presets();
    preset_token_out[0] = '\0';
    for (i = 0; i < presets.count; i++) {
        if (strcasecmp(presets.items[i].name, preset_name_out) == 0) {
            sprintf(preset_token_out, "PresetToken_%d", presets.items[i].number);
            break;
        }
    }
    destroy_presets();
    if (preset_token_out[0] == '\0') {
        send_fault("ptz_service", "Receiver", "ter:Action", "ter:TooManyPresets", "Too many presets", "Maximum number of presets reached");
        return -8;
    }

    long size = cat(NULL, "ptz_service_files/SetPreset.xml", 2,
            "%PRESET_TOKEN%", preset_token_out);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/SetPreset.xml", 2,
            "%PRESET_TOKEN%", preset_token_out);
}

int ptz_set_home_position()
{
    char sys_command[MAX_LEN];
    ezxml_t node;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoPTZProfile", "No PTZ profile", "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    if (service_ctx.ptz_node.set_home_position == NULL) {
        send_action_failed_fault("ptz_service", -3);
        return -3;
    }

    strcpy(sys_command, service_ctx.ptz_node.set_home_position);
    system(sys_command);

    long size = cat(NULL, "ptz_service_files/SetHomePosition.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/SetHomePosition.xml", 0);
}

int ptz_remove_preset()
{
    char sys_command[MAX_LEN];
    const char *preset_token;
    int preset_number;
    ezxml_t node;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoPTZProfile", "No PTZ profile", "The requested profile token does not reference a PTZ configuration");
        return -2;
    }
    preset_token = get_element("PresetToken", "Body");
    if (sscanf(preset_token, "PresetToken_%d", &preset_number) != 1) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "The requested preset token does not exist");
        return -3;
    }

    if (service_ctx.ptz_node.remove_preset == NULL) {
        send_action_failed_fault("ptz_service", -4);
        return -4;
    }

    sprintf(sys_command, service_ctx.ptz_node.remove_preset, preset_number);
    system(sys_command);

    long size = cat(NULL, "ptz_service_files/RemovePreset.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/RemovePreset.xml", 0);
}

int ptz_unsupported(const char *method)
{
    if (service_ctx.adv_fault_if_unknown == 1)
        send_action_failed_fault("ptz_service", -1);
    else
        send_empty_response("tptz", (char *) method);
    return -1;
}
