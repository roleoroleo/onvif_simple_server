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
    long size = cat(NULL, "ptz_service_files/GetPresets.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/GetPresets.xml", 0);
}

int ptz_goto_preset()
{
    const char *preset;
    char sys_command[MAX_LEN];

    preset = get_element("PresetToken", "Body");

    if ((preset[0] >= '0') && (preset[0] < '8')) {
        str_subst(sys_command, service_ctx.ptz_node.move_preset, "%t", (char *) preset);
        system(sys_command);

        long size = cat(NULL, "ptz_service_files/GotoPreset.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "ptz_service_files/GotoPreset.xml", 0);
    } else {
        send_fault();
        return -1;
    }
}

int ptz_goto_home_position()
{
    char sys_command[MAX_LEN];

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

    if (ret == 0) {
        long size = cat(NULL, "ptz_service_files/ContinuousMove.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "ptz_service_files/ContinuousMove.xml", 0);

    } else {
        send_fault();
        return -1;
    }
}

int ptz_relative_move()
{
    char const *x = NULL;
    char const *y = NULL;
    char const *z = NULL;
    double dx, dy;
    char sys_command[MAX_LEN];
    int ret = -1;
    ezxml_t node;

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

    if (x != NULL) {
        dx = atof(x);

        if (dx > 0.0) {
            strcpy(sys_command, service_ctx.ptz_node.move_right);
            system(sys_command);
            usleep(300000);
            strcpy(sys_command, service_ctx.ptz_node.move_stop);
            system(sys_command);
            ret = 0;
        } else if (dx < 0.0) {
            strcpy(sys_command, service_ctx.ptz_node.move_left);
            system(sys_command);
            usleep(300000);
            strcpy(sys_command, service_ctx.ptz_node.move_stop);
            system(sys_command);
            ret = 0;
        }
    }

    if (y != NULL) {
        dy = atof(y);

        if (dy > 0.0) {
            strcpy(sys_command, service_ctx.ptz_node.move_up);
            system(sys_command);
            usleep(300000);
            strcpy(sys_command, service_ctx.ptz_node.move_stop);
            system(sys_command);
            ret = 0;
        } else if (dy < 0.0) {
            strcpy(sys_command, service_ctx.ptz_node.move_down);
            system(sys_command);
            usleep(300000);
            strcpy(sys_command, service_ctx.ptz_node.move_stop);
            system(sys_command);
            ret = 0;
        }
    }

    if (z != NULL) {
        // Do nothing
        ret = 0;
    }

    if (ret == 0) {
        long size = cat(NULL, "ptz_service_files/RelativeMove.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "ptz_service_files/RelativeMove.xml", 0);

    } else {
        send_fault();
        return -1;
    }
}

int ptz_stop()
{
    char sys_command[MAX_LEN];

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

    sprintf(utctime, "%04d-%02d-%02dT%02d:%02d:%02dZ",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec);

    long size = cat(NULL, "ptz_service_files/GetStatus.xml", 2,
            "%TIME%", utctime);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "ptz_service_files/GetStatus.xml", 2,
            "%TIME%", utctime);
}

int ptz_set_preset()
{
    char sys_command[MAX_LEN];
    const char *preset_name;

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
    send_fault();
    return -1;
}
