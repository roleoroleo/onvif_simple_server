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

#include "utils.h"
#include "onvif_simple_server.h"

extern service_context_t service_ctx;

int ptz_get_service_capabilities()
{
    long size = get_file_size("ptz_service_files/GetServiceCapabilities.xml");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("ptz_service_files/GetServiceCapabilities.xml", 0);
}

int ptz_get_configurations()
{
    long size = get_file_size("ptz_service_files/GetConfigurations.xml");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("ptz_service_files/GetConfigurations.xml", 0);
}

int ptz_get_configuration()
{
    long size = get_file_size("ptz_service_files/GetConfiguration.xml");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("ptz_service_files/GetConfiguration.xml", 0);
}

int ptz_get_configuration_options()
{
    long size = get_file_size("ptz_service_files/GetConfigurationOptions.xml");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("ptz_service_files/GetConfigurationOptions.xml", 0);
}

int ptz_get_nodes()
{
    long size = get_file_size("ptz_service_files/GetNodes.xml");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("ptz_service_files/GetNodes.xml", 0);
}

int ptz_get_node()
{
    long size = get_file_size("ptz_service_files/GetNode.xml");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("ptz_service_files/GetNode.xml", 0);
}

int ptz_get_presets()
{
    long size = get_file_size("ptz_service_files/GetPresets.xml");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("ptz_service_files/GetPresets.xml", 0);
}

int ptz_goto_preset(char *input)
{
    char preset[2];
    char sys_command[MAX_LEN];

    if (find_element(input, "PresetToken", "0") == 0) {
        sprintf(preset, "0");
    } else if (find_element(input, "PresetToken", "1") == 0) {
        sprintf(preset, "1");
    } else if (find_element(input, "PresetToken", "2") == 0) {
        sprintf(preset, "2");
    } else if (find_element(input, "PresetToken", "3") == 0) {
        sprintf(preset, "3");
    } else if (find_element(input, "PresetToken", "4") == 0) {
        sprintf(preset, "4");
    } else if (find_element(input, "PresetToken", "5") == 0) {
        sprintf(preset, "5");
    } else if (find_element(input, "PresetToken", "6") == 0) {
        sprintf(preset, "6");
    } else if (find_element(input, "PresetToken", "7") == 0) {
        sprintf(preset, "7");
    }

    if (preset >= 0) {
        str_subst(sys_command, service_ctx.ptz_node.move_preset, "%t", preset);
        system(sys_command);

        long size = get_file_size("ptz_service_files/GotoPreset.xml");

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("ptz_service_files/GotoPreset.xml", 0);
    } else {
        long size = get_file_size("generic_files/Error.xml");

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("generic_files/Error.xml", 0);
    }
}

int ptz_goto_home_position()
{
    char sys_command[MAX_LEN];

    str_subst(sys_command, service_ctx.ptz_node.move_preset, "%t", "0");
    system(sys_command);

    long size = get_file_size("ptz_service_files/GotoHomePosition.xml");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("ptz_service_files/GotoHomePosition.xml", 0);
}

int ptz_continuous_move(char *input)
{
    char *sv, *sp, *sz, *sx, *sy, *sq;
    char *sqx = NULL, *sqy = NULL;
    char *x = NULL, *y = NULL;
    double dx, dy;
    char sys_command[MAX_LEN];
    int ret = -1;

    sv = strstr(input, "Velocity");
    if (sv != NULL) {
        sp = strstr(sv, "PanTilt");
        if (sp != NULL) {
            sx = strstr(sp, "x");
            if (sx != NULL) {
                sq = strstr(sx, "\"");
                if (sq != NULL) {
                    x = sq + 1;
                    sqx = strstr(x, "\"");
                }
            }
            sy = strstr(sp, "y");
            if (sy != NULL) {
                sq = strstr(sy, "\"");
                if (sq != NULL) {
                    y = sq + 1;
                    sqy = strstr(y, "\"");
                }
            }
        }
        sz = strstr(sv, "Zoom");
        if (sz != NULL) ret = 0;

    }

    if ((sqx != NULL) && (x != NULL)) {

        sqx = '\0';
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

    if ((sqy != NULL) && (y != NULL)) {

        sqy = '\0';
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

    if (ret == 0) {
        long size = get_file_size("ptz_service_files/ContinuousMove.xml");

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("ptz_service_files/ContinuousMove.xml", 0);

    } else {
        long size = get_file_size("generic_files/Error.xml");

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("generic_files/Error.xml", 0);
    }
}

int ptz_relative_move(char *input)
{
    char *sv, *sp, *sz, *sx, *sy, *sq;
    char *sqx, *sqy;
    char *x = NULL, *y = NULL;
    double dx, dy;
    char sys_command[MAX_LEN];
    int ret = -1;

    sv = strstr(input, "Translation");
    if (sv != NULL) {
        sp = strstr(sv, "PanTilt");
        if (sp != NULL) {
            sx = strstr(sp, "x");
            if (sx != NULL) {
                sq = strstr(sx, "\"");
                if (sq != NULL) {
                    x = sq + 1;
                    sqx = strstr(x, "\"");
                }
            }
            sy = strstr(sp, "y");
            if (sy != NULL) {
                sq = strstr(sy, "\"");
                if (sq != NULL) {
                    y = sq + 1;
                    sqy = strstr(y, "\"");
                }
            }
        }
        sz = strstr(sv, "Zoom");
        if (sz != NULL) ret = 0;
    }

    if ((sqx != NULL) && (x != NULL)) {

        sqx = '\0';
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

    if ((sqy != NULL) && (y != NULL)) {

        sqy = '\0';
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

    if (ret == 0) {
        long size = get_file_size("ptz_service_files/RelativeMove.xml");

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("ptz_service_files/RelativeMove.xml", 0);

    } else {
        long size = get_file_size("generic_files/Error.xml");

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("generic_files/Error.xml", 0);
    }
}

int ptz_stop()
{
    char sys_command[MAX_LEN];

    sprintf(sys_command, service_ctx.ptz_node.move_stop);
    system(sys_command);

    long size = get_file_size("ptz_service_files/Stop.xml");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("ptz_service_files/Stop.xml", 0);
}

int ptz_get_status()
{
    char utctime[32];
    time_t timestamp = time(NULL);
    struct tm *tm = gmtime(&timestamp);

    sprintf(utctime, "%04d-%02d-%02dT%02d:%02d:%02dZ",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec);

    long size = get_file_size("ptz_service_files/GetStatus.xml");
    size += strlen(utctime) - strlen("%TIME%");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("ptz_service_files/GetStatus.xml", 2,
            "%TIME%", utctime);
}

int ptz_unsupported(char *action)
{
    char response[MAX_LEN];
    sprintf(response, "%sResponse", action);

    long size = get_file_size("ptz_service_files/Unsupported.xml");
    size += strlen(response) - strlen("%UNSUPPORTED%");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("ptz_service_files/Unsupported.xml", 2,
            "%UNSUPPORTED%", response);
}
