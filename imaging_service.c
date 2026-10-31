/*
 * Copyright (c) 2026 Oleksandr Kravchuk.
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

/* Parse a value in -1.0 .. 1.0. */
static int parse_speed(const char *s, double *out)
{
    char *end;
    double d = strtod(s, &end);

    if ((end == s) || (*end != '\0') || !(d >= -1.0) || (d > 1.0))
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

/* Focus is only advertised when imaging_focus_command is set. */
static int has_focus()
{
    return (service_ctx.imaging_focus_command != NULL) && (service_ctx.imaging_focus_command[0] != '\0');
}

int imaging_get_imaging_settings()
{
    char brightness[16], contrast[16], saturation[16], sharpness[16], focus[96] = "";

    if (check_video_source_token(get_element("VideoSourceToken", "Body")) != 0)
        return -1;

    if (has_focus())
        snprintf(focus, sizeof(focus), "<tt:Focus><tt:AutoFocusMode>%s</tt:AutoFocusMode></tt:Focus>",
                service_ctx.imaging_focus_auto ? "AUTO" : "MANUAL");

    snprintf(brightness, sizeof(brightness), "%.4f", service_ctx.imaging_brightness);
    snprintf(contrast, sizeof(contrast), "%.4f", service_ctx.imaging_contrast);
    snprintf(saturation, sizeof(saturation), "%.4f", service_ctx.imaging_saturation);
    snprintf(sharpness, sizeof(sharpness), "%.4f", service_ctx.imaging_sharpness);

    long size = cat(NULL, "imaging_service_files/GetImagingSettings.xml", 18,
            "%BRIGHTNESS%", brightness,
            "%CONTRAST%", contrast,
            "%SATURATION%", saturation,
            "%SHARPNESS%", sharpness,
            "%EXPOSURE_MODE%", service_ctx.imaging_exposure_auto ? "AUTO" : "MANUAL",
            "%IRCUT_MODE%", service_ctx.imaging_ircut_on ? "ON" : "OFF",
            "%WDR_MODE%", service_ctx.imaging_wdr_on ? "ON" : "OFF",
            "%WB_MODE%", service_ctx.imaging_wb_auto ? "AUTO" : "MANUAL",
            "%FOCUS%", focus);

    output_http_headers(size);

    return cat("stdout", "imaging_service_files/GetImagingSettings.xml", 18,
            "%BRIGHTNESS%", brightness,
            "%CONTRAST%", contrast,
            "%SATURATION%", saturation,
            "%SHARPNESS%", sharpness,
            "%EXPOSURE_MODE%", service_ctx.imaging_exposure_auto ? "AUTO" : "MANUAL",
            "%IRCUT_MODE%", service_ctx.imaging_ircut_on ? "ON" : "OFF",
            "%WDR_MODE%", service_ctx.imaging_wdr_on ? "ON" : "OFF",
            "%WB_MODE%", service_ctx.imaging_wb_auto ? "AUTO" : "MANUAL",
            "%FOCUS%", focus);
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
        "ONVIF_IMAGING_EXPOSURE", "ONVIF_IMAGING_WIDE_DYNAMIC_RANGE",
        "ONVIF_IMAGING_AUTO_FOCUS_MODE"
    };
    char values[9][32];
    int present[9] = {0};
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

    sub = get_element_in_element_ptr("Focus", settings);
    val = sub ? get_element_in_element("AutoFocusMode", sub) : NULL;
    if (val != NULL) {
        if ((strcmp(val, "AUTO") != 0) && (strcmp(val, "MANUAL") != 0))
            goto invalid;
        snprintf(values[8], sizeof(values[8]), "%s", val);
        present[8] = 1;
    }

    for (i = 0; i < 9; i++) {
        if (present[i])
            setenv(names[i], values[i], 1);
        else
            unsetenv(names[i]);
    }

    ret = run_command(service_ctx.imaging_set_command);
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

    const char *focus = has_focus() ?
            "<tt:Focus><tt:AutoFocusModes>AUTO</tt:AutoFocusModes><tt:AutoFocusModes>MANUAL</tt:AutoFocusModes></tt:Focus>" : "";
    long size = cat(NULL, "imaging_service_files/GetOptions.xml", 2, "%FOCUS%", focus);

    output_http_headers(size);

    return cat("stdout", "imaging_service_files/GetOptions.xml", 2, "%FOCUS%", focus);
}

int imaging_get_move_options()
{
    if (check_video_source_token(get_element("VideoSourceToken", "Body")) != 0)
        return -1;

    const char *continuous = has_focus() ?
            "<tt:Continuous><tt:Speed><tt:Min>-1</tt:Min><tt:Max>1</tt:Max></tt:Speed></tt:Continuous>" : "";
    long size = cat(NULL, "imaging_service_files/GetMoveOptions.xml", 2, "%CONTINUOUS%", continuous);

    output_http_headers(size);

    return cat("stdout", "imaging_service_files/GetMoveOptions.xml", 2, "%CONTINUOUS%", continuous);
}

/* Run imaging_focus_command with the speed (-1.0 .. 1.0, 0 = stopped) in
 * ONVIF_IMAGING_FOCUS_SPEED. Positive focuses far, negative near. */
static int run_focus(double speed)
{
    char buf[32];
    int ret;

    snprintf(buf, sizeof(buf), "%.4f", speed);
    setenv("ONVIF_IMAGING_FOCUS_SPEED", buf, 1);
    ret = system(service_ctx.imaging_focus_command);
    if (ret != 0) {
        log_error("imaging_focus_command failed with status %d", ret);
        send_action_failed_fault("imaging_service", -3);
        return -3;
    }
    return 0;
}

/* Only continuous focus is supported, as GetMoveOptions advertises. */
int imaging_move()
{
    ezxml_t speed_node;
    const char *val;
    double speed;

    if (!has_focus())
        return send_action_not_supported_fault("imaging_service");

    if (check_video_source_token(get_element("VideoSourceToken", "Body")) != 0)
        return -1;

    speed_node = get_element_ptr(NULL, "Continuous", "Body");
    val = speed_node ? get_element_in_element("Speed", speed_node) : NULL;
    if ((val == NULL) || (parse_speed(val, &speed) != 0)) {
        send_fault("imaging_service", "Sender", "ter:InvalidArgVal", "ter:InvalidFocus",
                "Invalid focus", "Only a continuous Speed from -1.0 to 1.0 is supported");
        return -2;
    }

    if (run_focus(speed) != 0)
        return -3;

    return send_empty_response("timg", "Move");
}

int imaging_stop()
{
    if (has_focus()) {
        if (check_video_source_token(get_element("VideoSourceToken", "Body")) != 0)
            return -1;
        if (run_focus(0.0) != 0)
            return -3;
    }

    return send_empty_response("timg", "Stop");
}

/* Run imaging_status_command and read "<position> <state>" from its output.
 * Any failure or unexpected output leaves the UNKNOWN defaults in place. */
static void read_focus_status(double *pos, char *state, size_t state_len)
{
    char line[64], s[16];
    double p;
    FILE *f;

    if ((service_ctx.imaging_status_command == NULL) || (service_ctx.imaging_status_command[0] == '\0'))
        return;

    f = popen(service_ctx.imaging_status_command, "r");
    if (f == NULL)
        return;
    if ((fgets(line, sizeof(line), f) != NULL) && (sscanf(line, "%lf %15s", &p, s) == 2) &&
            (p >= 0.0) && (p <= 1.0) &&
            ((strcmp(s, "IDLE") == 0) || (strcmp(s, "MOVING") == 0) || (strcmp(s, "UNKNOWN") == 0))) {
        *pos = p;
        snprintf(state, state_len, "%s", s);
    } else {
        log_error("imaging_status_command printed unexpected output");
    }
    pclose(f);
}

int imaging_get_status()
{
    if (check_video_source_token(get_element("VideoSourceToken", "Body")) != 0)
        return -1;

    char focus[160] = "";

    if (has_focus()) {
        double pos = 0.0;
        char state[16] = "UNKNOWN";

        read_focus_status(&pos, state, sizeof(state));
        snprintf(focus, sizeof(focus),
                "<tt:FocusStatus20><tt:Position>%.4f</tt:Position><tt:MoveStatus>%s</tt:MoveStatus></tt:FocusStatus20>",
                pos, state);
    }
    long size = cat(NULL, "imaging_service_files/GetStatus.xml", 2, "%FOCUS%", focus);

    output_http_headers(size);

    return cat("stdout", "imaging_service_files/GetStatus.xml", 2, "%FOCUS%", focus);
}

int imaging_unsupported(const char *method)
{
    /* An unimplemented action must return a SOAP fault, not an empty 200
     * response (ONVIF: env:Receiver / ter:ActionNotSupported). */
    (void) method;
    return send_action_not_supported_fault("imaging_service");
}
