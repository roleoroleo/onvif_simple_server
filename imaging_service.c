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

#include "imaging_service.h"
#include "conf.h"
#include "fault.h"
#include "utils.h"
#include "log.h"
#include "ezxml_wrapper.h"
#include "onvif_simple_server.h"

extern service_context_t service_ctx;

/* The device exposes a single video source, the one GetVideoSources reports. */
#define VIDEO_SOURCE_TOKEN "VideoSourceToken"

static int check_video_source_token(const char *token)
{
    if ((token == NULL) || (strcmp(token, VIDEO_SOURCE_TOKEN) != 0)) {
        send_fault("imaging_service", "Sender", "ter:InvalidArgVal", "ter:NoSource",
                "No source", "The requested VideoSource does not exist");
        return -1;
    }
    return 0;
}

/* Parse a value in 0.0 .. 1.0. Returns 0 and fills out on success. */
static int parse_unit_range(const char *s, double *out)
{
    char *end;
    double d = strtod(s, &end);

    if ((end == s) || (*end != '\0') || !(d >= 0.0) || (d > 1.0))
        return -1;
    *out = d;
    return 0;
}

int imaging_get_service_capabilities()
{
    long size = cat(NULL, "imaging_service_files/GetServiceCapabilities.xml", 0);

    output_http_headers(size);

    return cat("stdout", "imaging_service_files/GetServiceCapabilities.xml", 0);
}

int imaging_get_imaging_settings()
{
    char brightness[16], contrast[16], saturation[16], sharpness[16];

    if (check_video_source_token(get_element("VideoSourceToken", "Body")) != 0)
        return -1;

    snprintf(brightness, sizeof(brightness), "%.4f", service_ctx.imaging_brightness);
    snprintf(contrast, sizeof(contrast), "%.4f", service_ctx.imaging_contrast);
    snprintf(saturation, sizeof(saturation), "%.4f", service_ctx.imaging_saturation);
    snprintf(sharpness, sizeof(sharpness), "%.4f", service_ctx.imaging_sharpness);

    long size = cat(NULL, "imaging_service_files/GetImagingSettings.xml", 16,
            "%BRIGHTNESS%", brightness,
            "%CONTRAST%", contrast,
            "%SATURATION%", saturation,
            "%SHARPNESS%", sharpness,
            "%EXPOSURE_MODE%", service_ctx.imaging_exposure_auto ? "AUTO" : "MANUAL",
            "%IRCUT_MODE%", service_ctx.imaging_ircut_on ? "ON" : "OFF",
            "%WDR_MODE%", service_ctx.imaging_wdr_on ? "ON" : "OFF",
            "%WB_MODE%", service_ctx.imaging_wb_auto ? "AUTO" : "MANUAL");

    output_http_headers(size);

    return cat("stdout", "imaging_service_files/GetImagingSettings.xml", 16,
            "%BRIGHTNESS%", brightness,
            "%CONTRAST%", contrast,
            "%SATURATION%", saturation,
            "%SHARPNESS%", sharpness,
            "%EXPOSURE_MODE%", service_ctx.imaging_exposure_auto ? "AUTO" : "MANUAL",
            "%IRCUT_MODE%", service_ctx.imaging_ircut_on ? "ON" : "OFF",
            "%WDR_MODE%", service_ctx.imaging_wdr_on ? "ON" : "OFF",
            "%WB_MODE%", service_ctx.imaging_wb_auto ? "AUTO" : "MANUAL");
}

/*
 * SetImagingSettings: validate the request, then hand the changed values to
 * imaging_set_command through ONVIF_IMAGING_* environment variables (never
 * interpolated into the command line, so the request cannot inject shell
 * syntax). A non-zero exit status of the command is reported as a fault.
 *
 * The request has several sibling <tt:Mode> elements (WhiteBalance, Exposure,
 * WideDynamicRange), so each one is looked up inside its own parent.
 */
int imaging_set_imaging_settings()
{
    static const char *const names[] = {
        "ONVIF_IMAGING_BRIGHTNESS", "ONVIF_IMAGING_CONTRAST",
        "ONVIF_IMAGING_COLOR_SATURATION", "ONVIF_IMAGING_SHARPNESS",
        "ONVIF_IMAGING_IRCUT_FILTER", "ONVIF_IMAGING_WHITE_BALANCE",
        "ONVIF_IMAGING_EXPOSURE", "ONVIF_IMAGING_WIDE_DYNAMIC_RANGE"
    };
    char values[8][32];
    int present[8] = {0};
    ezxml_t settings, sub;
    const char *val;
    double d;
    int i, ret;

    if ((service_ctx.imaging_set_command == NULL) ||
            (service_ctx.imaging_set_command[0] == '\0')) {
        return send_action_not_supported_fault("imaging_service");
    }

    if (check_video_source_token(get_element("VideoSourceToken", "Body")) != 0)
        return -1;

    settings = get_element_ptr(NULL, "ImagingSettings", "Body");
    if (settings == NULL)
        goto invalid;

    /* Validate everything before running the command: a rejected request must
     * not be partially applied. */
    for (i = 0; i < 4; i++) {
        static const char *const tags[] = { "Brightness", "Contrast", "ColorSaturation", "Sharpness" };

        val = get_element_in_element(tags[i], settings);
        if (val == NULL)
            continue;
        if (parse_unit_range(val, &d) != 0)
            goto invalid;
        snprintf(values[i], sizeof(values[i]), "%.4f", d);
        present[i] = 1;
    }

    val = get_element_in_element("IrCutFilter", settings);
    if (val != NULL) {
        if ((strcmp(val, "ON") != 0) && (strcmp(val, "OFF") != 0))
            goto invalid;
        snprintf(values[4], sizeof(values[4]), "%s", val);
        present[4] = 1;
    }

    for (i = 5; i < 8; i++) {
        static const char *const tags[] = { "WhiteBalance", "Exposure", "WideDynamicRange" };
        static const char *const modes[][3] = {
            { "AUTO", "MANUAL", NULL }, { "AUTO", "MANUAL", NULL }, { "ON", "OFF", NULL }
        };
        int m;

        sub = get_element_in_element_ptr(tags[i - 5], settings);
        if (sub == NULL)
            continue;
        val = get_element_in_element("Mode", sub);
        if (val == NULL)
            continue;
        for (m = 0; modes[i - 5][m] != NULL; m++) {
            if (strcmp(val, modes[i - 5][m]) == 0)
                break;
        }
        if (modes[i - 5][m] == NULL)
            goto invalid;
        snprintf(values[i], sizeof(values[i]), "%s", val);
        present[i] = 1;
    }

    for (i = 0; i < 8; i++) {
        if (present[i])
            setenv(names[i], values[i], 1);
        else
            unsetenv(names[i]);
    }

    ret = system(service_ctx.imaging_set_command);
    if (ret != 0) {
        log_error("imaging_set_command failed with status %d", ret);
        send_action_failed_fault("imaging_service", -3);
        return -3;
    }

    return send_empty_response("timg", "SetImagingSettings");

invalid:
    send_fault("imaging_service", "Sender", "ter:InvalidArgVal", "ter:SettingsInvalid",
            "Invalid settings", "One or more imaging settings values are out of range or unsupported");
    return -2;
}

int imaging_get_options()
{
    if (check_video_source_token(get_element("VideoSourceToken", "Body")) != 0)
        return -1;

    long size = cat(NULL, "imaging_service_files/GetOptions.xml", 0);

    output_http_headers(size);

    return cat("stdout", "imaging_service_files/GetOptions.xml", 0);
}

int imaging_get_move_options()
{
    if (check_video_source_token(get_element("VideoSourceToken", "Body")) != 0)
        return -1;

    long size = cat(NULL, "imaging_service_files/GetMoveOptions.xml", 0);

    output_http_headers(size);

    return cat("stdout", "imaging_service_files/GetMoveOptions.xml", 0);
}

/* No focus control: GetMoveOptions advertises no move type, so a client has no
 * reason to call Move, and nothing is ever moving for Stop to interrupt. */
int imaging_move()
{
    return send_action_not_supported_fault("imaging_service");
}

int imaging_stop()
{
    return send_empty_response("timg", "Stop");
}

int imaging_get_status()
{
    if (check_video_source_token(get_element("VideoSourceToken", "Body")) != 0)
        return -1;

    long size = cat(NULL, "imaging_service_files/GetStatus.xml", 0);

    output_http_headers(size);

    return cat("stdout", "imaging_service_files/GetStatus.xml", 0);
}

int imaging_unsupported(const char *method)
{
    if (service_ctx.adv_fault_if_unknown == 1)
        send_action_failed_fault("imaging_service", -1);
    else
        send_empty_response("timg", (char *) method);
    return -1;
}
