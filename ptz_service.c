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
    int num;
    double x, y, z;
    char name[MAX_LEN];
    char *p;

    presets.count = 0;
    presets.items = (preset_t *) malloc(sizeof(preset_t));

    // Run command that returns to stdout the list of the presets in the form number=name,pan,tilt,zoom (zoom is optional)
    fp = popen(service_ctx.ptz_node.get_presets, "r");
    if (fp == NULL) {
        return -1;
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
                return -2;
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
    long size = cat(NULL, "ptz_service_files/GetServiceCapabilities.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/GetServiceCapabilities.xml", 0);
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
    const char *preset;
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

    preset = get_element("PresetToken", "Body");

    if ((preset[0] >= '0') && (preset[0] < '8')) {
        str_subst(sys_command, service_ctx.ptz_node.move_preset, "%t", (char *) preset);
        system(sys_command);

        long size = cat(NULL, "ptz_service_files/GotoPreset.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "ptz_service_files/GotoPreset.xml", 0);
    } else {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "The requested preset token does not exist");
        return -3;
    }
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

    str_subst(sys_command, service_ctx.ptz_node.move_preset, "%t", "0");
    system(sys_command);

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
            strcpy(sys_command, service_ctx.ptz_node.move_right);
            system(sys_command);
            ret = 0;
        } else if (dx < 0.0) {
            strcpy(sys_command, service_ctx.ptz_node.move_left);
            system(sys_command);
            ret = 0;
        }
    }

    if (y != NULL) {
        dy = atof(y);

        if (dy > 0.0) {
            strcpy(sys_command, service_ctx.ptz_node.move_up);
            system(sys_command);
            ret = 0;
        } else if (dy < 0.0) {
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
    double dx, dy;
    char sys_command_tmp[MAX_LEN];
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

    node = get_element_ptr(NULL, "Translation", "Body");
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

    if (x == NULL) {
        str_subst(sys_command_tmp, service_ctx.ptz_node.jump_to_rel, "%x", "0");
    } else {
        dx = atof(x);
        if ((dx > 360.0) || (dx < -360.0)) {
            ret = -3;
        } else {
            str_subst(sys_command_tmp, service_ctx.ptz_node.jump_to_rel, "%x", (char *) x);
        }
    }
    if (y == NULL) {
        str_subst(sys_command, sys_command_tmp, "%y", "0");
    } else {
        dy = atof(y);
        if ((dy > 180.0) || (dy < -180.0)) {
            ret = -4;
        } else {
            str_subst(sys_command, sys_command_tmp, "%y", (char *) y);
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
    double dx, dy;
    char sys_command_tmp[MAX_LEN];
    char sys_command[MAX_LEN];
    int ret = 0;
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

    node = get_element_ptr(NULL, "Position", "Body");
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

    if (x == NULL) {
        ret = -3;
    } else {
        dx = atof(x);
        if ((dx > 360.0) || (dx < 0.0)) {
            ret = -4;
        } else {
            str_subst(sys_command_tmp, service_ctx.ptz_node.jump_to_abs, "%x", (char *) x);
        }
    }
    if (y == NULL) {
        ret = -5;
    } else {
        dy = atof(y);
        if ((dy > 180.0) || (dy < 0.0)) {
            ret = -6;
        } else {
            str_subst(sys_command, sys_command_tmp, "%y", (char *) y);
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
    int x, y;
    char out[256], sx[128], sy[128], sz[128];
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
    fp = popen(service_ctx.ptz_node.get_position, "r");
    if (fp == NULL) {
        ret = -3;
    } else {
        if (fgets(out, sizeof(out), fp) == NULL) {
            ret = -4;
        } else {
            if (sscanf(out, "%d,%d", &x, &y) != 2) {
                ret = -4;
            }
        }
        pclose(fp);
    }

    sprintf(sx, "%d", x);
    sprintf(sy, "%d", y);
    sprintf(sz, "%d", 1);
    if (ret == 0) {
        long size = cat(NULL, "ptz_service_files/GetStatus.xml", 8,
                "%X%", sx,
                "%Y%", sy,
                "%Z%", sz,
                "%TIME%", utctime);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "ptz_service_files/GetStatus.xml", 8,
                "%X%", sx,
                "%Y%", sy,
                "%Z%", sz,
                "%TIME%", utctime);
    } else {
        send_fault("ptz_service", "Receiver", "ter:Action", "ter:NoStatus", "No status", "No PTZ status is available in the requested Media Profile");
        return ret;
    }
}

int ptz_set_preset()
{
    char sys_command[MAX_LEN];
    const char *preset_name;
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

    preset_name = get_element("PresetName", "Body");
    str_subst(sys_command, service_ctx.ptz_node.set_preset, "%t", (char *) preset_name);
    system(sys_command);

    long size = cat(NULL, "ptz_service_files/SetPreset.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/SetPreset.xml", 0);
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
    str_subst(sys_command, service_ctx.ptz_node.remove_preset, "%t", (char *) preset_token);
    system(sys_command);

    long size = cat(NULL, "ptz_service_files/RemovePreset.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/RemovePreset.xml", 0);
}

int ptz_unsupported(const char *method)
{
    send_action_failed_fault(-1);
    return -1;
}
