/*
 * Copyright (c) 2026 roleo.
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
#include <string.h>

#include "media2_service.h"
#include "conf.h"
#include "fault.h"
#include "utils.h"
#include "log.h"
#include "ezxml_wrapper.h"
#include "onvif_simple_server.h"
#include "ptz_service.h"

extern service_context_t service_ctx;

/*
 * The ONVIF VideoSource models the physical sensor, which is independent of any
 * single encoder profile. Report it from the profile with the largest frame area
 * so a low resolution sub stream never shrinks the advertised source dimensions.
 */
static int get_video_source_index()
{
    int i, idx = 0;
    long best = -1;

    for (i = 0; i < service_ctx.profiles_num; i++) {
        long area = (long) service_ctx.profiles[i].width * service_ctx.profiles[i].height;
        if (area > best) {
            best = area;
            idx = i;
        }
    }

    return idx;
}

int media2_get_service_capabilities()
{
    char cap[256];

    cap[0] = '\0';
    snprintf(cap + strlen(cap), sizeof(cap) - strlen(cap), "%s", "VideoSource VideoEncoder");
    if ((service_ctx.profiles[0].audio_encoder != AUDIO_NONE) ||
            ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE))) {

        snprintf(cap + strlen(cap), sizeof(cap) - strlen(cap), "%s", " AudioSource AudioEncoder");
    }
    if ((service_ctx.profiles[0].audio_decoder != AUDIO_NONE) ||
            ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_decoder != AUDIO_NONE))) {

        snprintf(cap + strlen(cap), sizeof(cap) - strlen(cap), "%s", " AudioOutput AudioDecoder");
    }
    if (service_ctx.ptz_node.enable == 1) {
        snprintf(cap + strlen(cap), sizeof(cap) - strlen(cap), "%s", " PTZ");
    }

    long size = cat(NULL, "media2_service_files/GetServiceCapabilities.xml", 2,
            "%CAPABILITIES%", cap);

    output_http_headers(size);

    return cat("stdout", "media2_service_files/GetServiceCapabilities.xml", 2,
            "%CAPABILITIES%", cap);
}

int media2_get_profiles()
{
    char profiles_num[2];
    char stmp_w_h[16], stmp_h_h[16];
    char stmp_w_l[16], stmp_h_l[16];
    char stmp_fps_h[16], stmp_fps_l[16];
    char audio_enc_h[16], audio_enc_l[16];
    char video_enc_h[16], video_enc_l[16];
    const char *profile_token = get_element("Token", "Body");
    ezxml_t x_type;
    int n_type;
    char xml_name[MAX_LEN];
    long size;
    int q, h;
    char *h264profile[] = { "High", "Main" };
    char *profile[] = { "Profile_0", "Profile_1" };
    int c;
    char dest_a[] = "stdout";
    char *dest;
    int typeVSC = 0, typeASC = 0, typeVEC = 0, typeAEC = 0, typePTZ = 0, typeAOC = 0, typeADC = 0;
    char min_x[256], max_x[256], min_y[256], max_y[256], min_z[256], max_z[256];
    int vsrc = get_video_source_index();

    audio_enc_h[0] = '\0';
    audio_enc_l[0] = '\0';
    video_enc_h[0] = '\0';
    video_enc_l[0] = '\0';
    snprintf(profiles_num, sizeof(profiles_num), "%d", service_ctx.profiles_num);
    snprintf(min_x, sizeof(min_x), "%.1f", service_ctx.ptz_node.min_step_x);
    snprintf(max_x, sizeof(max_x), "%.1f", service_ctx.ptz_node.max_step_x);
    snprintf(min_y, sizeof(min_y), "%.1f", service_ctx.ptz_node.min_step_y);
    snprintf(max_y, sizeof(max_y), "%.1f", service_ctx.ptz_node.max_step_y);
    snprintf(min_z, sizeof(min_z), "%.1f", service_ctx.ptz_node.min_step_z);
    snprintf(max_z, sizeof(max_z), "%.1f", service_ctx.ptz_node.max_step_z);

    x_type = get_element_ptr(NULL, "Type", "Body");
    n_type = 0;
    do {
        if (x_type != NULL) {
            if (strstr(x_type->txt, "VideoSource") != NULL) typeVSC = 1;
            if (strstr(x_type->txt, "AudioSource") != NULL) typeASC = 1;
            if (strstr(x_type->txt, "VideoEncoder") != NULL) typeVEC = 1;
            if (strstr(x_type->txt, "AudioEncoder") != NULL) typeAEC = 1;
            if (strstr(x_type->txt, "PTZ") != NULL) typePTZ = 1;
            if (strstr(x_type->txt, "AudioOutput") != NULL) typeAOC = 1;
            if (strstr(x_type->txt, "AudioDecoder") != NULL) typeADC = 1;
            if (strstr(x_type->txt, "All") != NULL) {
                typeVSC = 1;
                typeASC = 1;
                typeVEC = 1;
                typeAEC = 1;
                typePTZ = 1;
                typeAOC = 1;
                typeADC = 1;
            }
            n_type++;
            x_type = x_type->next;
        }
    } while (x_type != NULL);

    if (service_ctx.profiles_num == 0) {
        q = 0;
        size = cat(NULL, "media2_service_files/GetProfiles_none.xml", 0);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetProfiles_none.xml", 0);

    } else if (profile_token == NULL) {
        if (service_ctx.profiles_num == 1) {
            q = 1;
            h = 0;
        } else if (service_ctx.profiles_num == 2) {
            q = 2;
        }
    } else if ((service_ctx.profiles_num > 0) &&
            (strcasecmp(service_ctx.profiles[0].name, profile_token) == 0)) {
        q = 1;
        h = 0;
    } else if ((service_ctx.profiles_num == 2) &&
            (strcasecmp(service_ctx.profiles[1].name, profile_token) == 0)) {
        q = 1;
        h = 1;
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -1;
    }

    if (q == 0) {
        size = cat(NULL, "media2_service_files/GetProfiles_none.xml", 0);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetProfiles_none.xml", 0);

    } else if (q == 1) {
        // We need 1st step to evaluate content length
        for (c = 0; c < 2; c++) {
            if (c == 0) {
                dest = NULL;
            } else {
                dest = dest_a;
                output_http_headers(size);
            }

            size = cat(dest, "media2_service_files/GetProfiles_header.xml", 2,
                    "%PROFILE%", profile[h]);

            if (n_type > 0) {
                size += cat(dest, "media2_service_files/GetProfiles_confstart.xml", 0);

                if (typeVSC) {
                    // Get VideoSource from the largest profile
                    snprintf(stmp_w_h, sizeof(stmp_w_h), "%d", service_ctx.profiles[vsrc].width);
                    snprintf(stmp_h_h, sizeof(stmp_h_h), "%d", service_ctx.profiles[vsrc].height);
                    size += cat(dest, "media2_service_files/GetProfiles_VSC.xml", 6,
                            "%PROFILES_NUM%", profiles_num,
                            "%VSC_WIDTH%", stmp_w_h,
                            "%VSC_HEIGHT%", stmp_h_h);
                }
                if (typeASC) {
                    if (((service_ctx.profiles_num > 0) && (service_ctx.profiles[0].audio_encoder != AUDIO_NONE)) ||
                            ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE))) {

                        size += cat(dest, "media2_service_files/GetProfiles_ASC.xml", 2,
                                "%PROFILES_NUM%", profiles_num);
                    }
                }
                if (typeVEC) {
                    snprintf(stmp_w_h, sizeof(stmp_w_h), "%d", service_ctx.profiles[h].width);
                    snprintf(stmp_h_h, sizeof(stmp_h_h), "%d", service_ctx.profiles[h].height);
                    snprintf(stmp_fps_h, sizeof(stmp_fps_h), "%d", service_ctx.profiles[h].framerate > 0 ? service_ctx.profiles[h].framerate : 30);
                    set_video_codec(video_enc_h, 16, service_ctx.profiles[h].type, 2);
                    size += cat(dest, "media2_service_files/GetProfiles_VEC.xml", 12,
                            "%H264PROFILE%", h264profile[h],
                            "%PROFILE%", profile[h],
                            "%VIDEO_ENCODING%", video_enc_h,
                            "%VEC_WIDTH%", stmp_w_h,
                            "%VEC_HEIGHT%", stmp_h_h,
                            "%FRAMERATE%", stmp_fps_h);
                }
                if (typeAEC) {
                    if (service_ctx.profiles[h].audio_encoder != AUDIO_NONE) {
                        set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
                        size += cat(dest, "media2_service_files/GetProfiles_AEC.xml", 4,
                                "%PROFILE%", profile[h],
                                "%AUDIO_ENCODING%", audio_enc_h);
                    }
                }
                if (typePTZ) {
                    if (service_ctx.ptz_node.enable == 1) {
                        size += cat(dest, ptz_supports_zoom() ? "media2_service_files/GetProfiles_PTZ.xml" : "media2_service_files/GetProfiles_PTZ_nozoom.xml", 12,
                                "%MIN_X%", min_x,
                                "%MAX_X%", max_x,
                                "%MIN_Y%", min_y,
                                "%MAX_Y%", max_y,
                                "%MIN_Z%", min_z,
                                "%MAX_Z%", max_z);
                    }
                }
                if (service_ctx.profiles[h].audio_decoder != AUDIO_NONE) {
                    if (typeAOC) {
                        size += cat(dest, "media2_service_files/GetProfiles_AOC.xml", 2,
                                "%PROFILES_NUM%", profiles_num);
                    }
                    if (typeADC) {
                        size += cat(dest, "media2_service_files/GetProfiles_ADC.xml", 2,
                                "%PROFILE%", profile[h]);
                    }
                }

                size += cat(dest, "media2_service_files/GetProfiles_confend.xml", 0);
            }
            size += cat(dest, "media2_service_files/GetProfiles_footer.xml", 0);
        }
    } else if (q == 2) {
        // We need 1st step to evaluate content length
        for (c = 0; c < 2; c++) {
            if (c == 0) {
                dest = NULL;
            } else {
                dest = dest_a;
                output_http_headers(size);
            }

            h = 0;
            size = cat(dest, "media2_service_files/GetProfiles_header.xml", 2,
                    "%PROFILE%", profile[h]);

            if (n_type > 0) {
                size += cat(dest, "media2_service_files/GetProfiles_confstart.xml", 0);

                if (typeVSC) {
                    // Get VideoSource from the largest profile
                    snprintf(stmp_w_h, sizeof(stmp_w_h), "%d", service_ctx.profiles[vsrc].width);
                    snprintf(stmp_h_h, sizeof(stmp_h_h), "%d", service_ctx.profiles[vsrc].height);
                    size += cat(dest, "media2_service_files/GetProfiles_VSC.xml", 6,
                            "%PROFILES_NUM%", profiles_num,
                            "%VSC_WIDTH%", stmp_w_h,
                            "%VSC_HEIGHT%", stmp_h_h);
                }
                if (typeASC) {
                    if (((service_ctx.profiles_num > 0) && (service_ctx.profiles[0].audio_encoder != AUDIO_NONE)) ||
                            ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE))) {

                        size += cat(dest, "media2_service_files/GetProfiles_ASC.xml", 2,
                                "%PROFILES_NUM%", profiles_num);
                    }
                }
                if (typeVEC) {
                    snprintf(stmp_w_h, sizeof(stmp_w_h), "%d", service_ctx.profiles[h].width);
                    snprintf(stmp_h_h, sizeof(stmp_h_h), "%d", service_ctx.profiles[h].height);
                    snprintf(stmp_fps_h, sizeof(stmp_fps_h), "%d", service_ctx.profiles[h].framerate > 0 ? service_ctx.profiles[h].framerate : 30);
                    set_video_codec(video_enc_h, 16, service_ctx.profiles[h].type, 2);
                    size += cat(dest, "media2_service_files/GetProfiles_VEC.xml", 12,
                            "%H264PROFILE%", h264profile[h],
                            "%PROFILE%", profile[h],
                            "%VIDEO_ENCODING%", video_enc_h,
                            "%VEC_WIDTH%", stmp_w_h,
                            "%VEC_HEIGHT%", stmp_h_h,
                            "%FRAMERATE%", stmp_fps_h);
                }
                if (typeAEC) {
                    if (service_ctx.profiles[h].audio_encoder != AUDIO_NONE) {
                        set_audio_codec(audio_enc_h, 16, service_ctx.profiles[h].audio_encoder, 2);
                        size += cat(dest, "media2_service_files/GetProfiles_AEC.xml", 4,
                                "%PROFILE%", profile[h],
                                "%AUDIO_ENCODING%", audio_enc_h);
                    }
                }
                if (typePTZ) {
                    if (service_ctx.ptz_node.enable == 1) {
                        size += cat(dest, ptz_supports_zoom() ? "media2_service_files/GetProfiles_PTZ.xml" : "media2_service_files/GetProfiles_PTZ_nozoom.xml", 12,
                                "%MIN_X%", min_x,
                                "%MAX_X%", max_x,
                                "%MIN_Y%", min_y,
                                "%MAX_Y%", max_y,
                                "%MIN_Z%", min_z,
                                "%MAX_Z%", max_z);
                    }
                }
                if (service_ctx.profiles[h].audio_decoder != AUDIO_NONE) {
                    if (typeAOC) {
                        size += cat(dest, "media2_service_files/GetProfiles_AOC.xml", 2,
                                "%PROFILES_NUM%", profiles_num);
                    }
                    if (typeADC) {
                        size += cat(dest, "media2_service_files/GetProfiles_ADC.xml", 2,
                                "%PROFILE%", profile[h]);
                    }
                }

                size += cat(dest, "media2_service_files/GetProfiles_confend.xml", 0);
            }

            h = 1;
            size += cat(dest, "media2_service_files/GetProfiles_middle.xml", 2,
                    "%PROFILE%", profile[h]);

            if (n_type > 0) {
                size += cat(dest, "media2_service_files/GetProfiles_confstart.xml", 0);

                if (typeVSC) {
                    // Get VideoSource from the largest profile
                    snprintf(stmp_w_l, sizeof(stmp_w_l), "%d", service_ctx.profiles[vsrc].width);
                    snprintf(stmp_h_l, sizeof(stmp_h_l), "%d", service_ctx.profiles[vsrc].height);
                    size += cat(dest, "media2_service_files/GetProfiles_VSC.xml", 6,
                            "%PROFILES_NUM%", profiles_num,
                            "%VSC_WIDTH%", stmp_w_l,
                            "%VSC_HEIGHT%", stmp_h_l);
                }
                if (typeASC) {
                    if (((service_ctx.profiles_num > 0) && (service_ctx.profiles[0].audio_encoder != AUDIO_NONE)) ||
                            ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE))) {

                        size += cat(dest, "media2_service_files/GetProfiles_ASC.xml", 2,
                                "%PROFILES_NUM%", profiles_num);
                    }
                }
                if (typeVEC) {
                    snprintf(stmp_w_l, sizeof(stmp_w_l), "%d", service_ctx.profiles[h].width);
                    snprintf(stmp_h_l, sizeof(stmp_h_l), "%d", service_ctx.profiles[h].height);
                    snprintf(stmp_fps_l, sizeof(stmp_fps_l), "%d", service_ctx.profiles[h].framerate > 0 ? service_ctx.profiles[h].framerate : 30);
                    set_video_codec(video_enc_l, 16, service_ctx.profiles[h].type, 2);
                    size += cat(dest, "media2_service_files/GetProfiles_VEC.xml", 12,
                            "%H264PROFILE%", h264profile[h],
                            "%PROFILE%", profile[h],
                            "%VIDEO_ENCODING%", video_enc_l,
                            "%VEC_WIDTH%", stmp_w_l,
                            "%VEC_HEIGHT%", stmp_h_l,
                            "%FRAMERATE%", stmp_fps_l);
                }
                if (typeAEC) {
                    if (service_ctx.profiles[h].audio_encoder != AUDIO_NONE) {
                        set_audio_codec(audio_enc_l, 16, service_ctx.profiles[h].audio_encoder, 2);
                        size += cat(dest, "media2_service_files/GetProfiles_AEC.xml", 4,
                                "%PROFILE%", profile[h],
                                "%AUDIO_ENCODING%", audio_enc_l);
                    }
                }
                if (typePTZ) {
                    if (service_ctx.ptz_node.enable == 1) {
                        size += cat(dest, ptz_supports_zoom() ? "media2_service_files/GetProfiles_PTZ.xml" : "media2_service_files/GetProfiles_PTZ_nozoom.xml", 12,
                                "%MIN_X%", min_x,
                                "%MAX_X%", max_x,
                                "%MIN_Y%", min_y,
                                "%MAX_Y%", max_y,
                                "%MIN_Z%", min_z,
                                "%MAX_Z%", max_z);
                    }
                }
                if (service_ctx.profiles[h].audio_decoder != AUDIO_NONE) {
                    if (typeAOC) {
                        size += cat(dest, "media2_service_files/GetProfiles_AOC.xml", 2,
                                "%PROFILES_NUM%", profiles_num);
                    }
                    if (typeADC) {
                        size += cat(dest, "media2_service_files/GetProfiles_ADC.xml", 2,
                                "%PROFILE%", profile[h]);
                    }
                }

                size += cat(dest, "media2_service_files/GetProfiles_confend.xml", 0);
            }
            size += cat(dest, "media2_service_files/GetProfiles_footer.xml", 0);
        }
    }
}

int media2_get_video_source_modes()
{
    const char *token = get_element("VideoSourceToken", "Body");
    char stmp_w[16], stmp_h[16], video_enc[16];

    if (strcasecmp("VideoSourceToken", token) == 0) {

        // Get data from the largest profile (the physical source)
        char stmp_fps[8];
        int vsrc = get_video_source_index();
        snprintf(stmp_w, sizeof(stmp_w), "%d", service_ctx.profiles[vsrc].width);
        snprintf(stmp_h, sizeof(stmp_h), "%d", service_ctx.profiles[vsrc].height);
        snprintf(stmp_fps, sizeof(stmp_fps), "%d", service_ctx.profiles[vsrc].framerate);
        set_video_codec(video_enc, 16, service_ctx.profiles[vsrc].type, 2);
        long size = cat(NULL, "media2_service_files/GetVideoSourceModes.xml", 8,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h,
                "%VIDEO_ENCODING%", video_enc,
                "%FRAMERATE%", stmp_fps);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetVideoSourceModes.xml", 8,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h,
                "%VIDEO_ENCODING%", video_enc,
                "%FRAMERATE%", stmp_fps);

    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoVideoSource", "No video source", "The requested video source does not exist");
        return -1;
    }
}

int media2_get_video_source_configurations()
{
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[23];
    char profiles_num[2], stmp_w[16], stmp_h[16];

    memset(token, '\0', sizeof(token));
    if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else if (configuration_token != NULL) {
        strncpy(token, configuration_token, 22);
    } else if (service_ctx.profiles_num > 0) {
        strncpy(token, service_ctx.profiles[0].name, 9);
    }

    if ((strcasecmp(service_ctx.profiles[0].name, token) == 0) ||
            (strcasecmp(service_ctx.profiles[1].name, token) == 0) ||
            (strcasecmp("VideoSourceConfigToken", token) == 0)) {

        int vsrc = get_video_source_index();
        snprintf(profiles_num, sizeof(profiles_num), "%d", service_ctx.profiles_num);
        snprintf(stmp_w, sizeof(stmp_w), "%d", service_ctx.profiles[vsrc].width);
        snprintf(stmp_h, sizeof(stmp_h), "%d", service_ctx.profiles[vsrc].height);
        long size = cat(NULL, "media2_service_files/GetVideoSourceConfigurations.xml", 6,
                "%PROFILES_NUM%", profiles_num,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetVideoSourceConfigurations.xml", 6,
                "%PROFILES_NUM%", profiles_num,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The requested configuration indicated does not exist");
        return -1;
    }
}

int media2_get_video_source_configuration_options()
{
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[23];
    char stmp_w[16], stmp_h[16];

    memset(token, '\0', sizeof(token));
    if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else if (configuration_token != NULL) {
        strncpy(token, configuration_token, 22);
    } else if (service_ctx.profiles_num > 0) {
        strncpy(token, service_ctx.profiles[0].name, 9);
    }

    if ((strcasecmp(service_ctx.profiles[0].name, token) == 0) ||
            (strcasecmp(service_ctx.profiles[1].name, token) == 0) ||
            (strcasecmp("VideoSourceConfigToken", token) == 0)) {

        int vsrc = get_video_source_index();
        snprintf(stmp_w, sizeof(stmp_w), "%d", service_ctx.profiles[vsrc].width);
        snprintf(stmp_h, sizeof(stmp_h), "%d", service_ctx.profiles[vsrc].height);
        long size = cat(NULL, "media2_service_files/GetVideoSourceConfigurationOptions.xml", 4,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetVideoSourceConfigurationOptions.xml", 4,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);

    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The requested configuration indicated does not exist");
        return -1;
    }
}

int media2_get_video_encoder_configurations()
{
    char stmp_w_l[16], stmp_h_l[16];
    char stmp_fps_h[16], stmp_fps_l[16];
    char stmp_w_h[16], stmp_h_h[16];
    const char *profile_token = get_element("ProfileToken", "Body");
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    char token[10];
    char video_enc_h[16], video_enc_l[16];

    memset(token, '\0', sizeof(token));
    if (configuration_token != NULL) {
        // Extract "Profile_x" from token Profile_x_VideoEncoderConfig
        strncpy(token, configuration_token, 9);
    } else if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    }

    if (token[0] == '\0') {
        if (service_ctx.profiles_num == 1) {
            snprintf(stmp_w_h, sizeof(stmp_w_h), "%d", service_ctx.profiles[0].width);
            snprintf(stmp_h_h, sizeof(stmp_h_h), "%d", service_ctx.profiles[0].height);
            snprintf(stmp_fps_h, sizeof(stmp_fps_h), "%d", service_ctx.profiles[0].framerate > 0 ? service_ctx.profiles[0].framerate : 30);
            set_video_codec(video_enc_h, 16, service_ctx.profiles[0].type, 2);
            long size = cat(NULL, "media2_service_files/GetVideoEncoderConfigurations.xml", 12,
                    "%H264PROFILE%", "High",
                    "%PROFILE%", "Profile_0",
                    "%WIDTH%", stmp_w_h,
                    "%HEIGHT%", stmp_h_h,
                    "%FRAMERATE%", stmp_fps_h,
                    "%VIDEO_ENCODING%", video_enc_h);

            output_http_headers(size);

            return cat("stdout", "media2_service_files/GetVideoEncoderConfigurations.xml", 12,
                    "%H264PROFILE%", "High",
                    "%PROFILE%", "Profile_0",
                    "%WIDTH%", stmp_w_h,
                    "%HEIGHT%", stmp_h_h,
                    "%FRAMERATE%", stmp_fps_h,
                    "%VIDEO_ENCODING%", video_enc_h);

        } else if (service_ctx.profiles_num == 2) {
            snprintf(stmp_w_h, sizeof(stmp_w_h), "%d", service_ctx.profiles[0].width);
            snprintf(stmp_h_h, sizeof(stmp_h_h), "%d", service_ctx.profiles[0].height);
            snprintf(stmp_w_l, sizeof(stmp_w_l), "%d", service_ctx.profiles[1].width);
            snprintf(stmp_h_l, sizeof(stmp_h_l), "%d", service_ctx.profiles[1].height);
            snprintf(stmp_fps_h, sizeof(stmp_fps_h), "%d", service_ctx.profiles[0].framerate > 0 ? service_ctx.profiles[0].framerate : 30);
            snprintf(stmp_fps_l, sizeof(stmp_fps_l), "%d", service_ctx.profiles[1].framerate > 0 ? service_ctx.profiles[1].framerate : 30);
            set_video_codec(video_enc_h, 16, service_ctx.profiles[0].type, 2);
            set_video_codec(video_enc_l, 16, service_ctx.profiles[1].type, 2);
            long size = cat(NULL, "media2_service_files/GetVideoEncoderConfigurations_both.xml", 16,
                        "%WIDTH_HIGH%", stmp_w_h,
                        "%HEIGHT_HIGH%", stmp_h_h,
                        "%WIDTH_LOW%", stmp_w_l,
                        "%HEIGHT_LOW%", stmp_h_l,
                        "%FRAMERATE_HIGH%", stmp_fps_h,
                        "%FRAMERATE_LOW%", stmp_fps_l,
                        "%VIDEO_ENCODING_HIGH%", video_enc_h,
                        "%VIDEO_ENCODING_LOW%", video_enc_l);

            output_http_headers(size);

            return cat("stdout", "media2_service_files/GetVideoEncoderConfigurations_both.xml", 16,
                        "%WIDTH_HIGH%", stmp_w_h,
                        "%HEIGHT_HIGH%", stmp_h_h,
                        "%WIDTH_LOW%", stmp_w_l,
                        "%HEIGHT_LOW%", stmp_h_l,
                        "%FRAMERATE_HIGH%", stmp_fps_h,
                        "%FRAMERATE_LOW%", stmp_fps_l,
                        "%VIDEO_ENCODING_HIGH%", video_enc_h,
                        "%VIDEO_ENCODING_LOW%", video_enc_l);
        }
    } else if ((service_ctx.profiles_num > 0) &&
            (strcasecmp(service_ctx.profiles[0].name, token) == 0)) {

        snprintf(stmp_w_h, sizeof(stmp_w_h), "%d", service_ctx.profiles[0].width);
        snprintf(stmp_h_h, sizeof(stmp_h_h), "%d", service_ctx.profiles[0].height);
        snprintf(stmp_fps_h, sizeof(stmp_fps_h), "%d", service_ctx.profiles[0].framerate > 0 ? service_ctx.profiles[0].framerate : 30);
        set_video_codec(video_enc_h, 16, service_ctx.profiles[0].type, 2);
        long size = cat(NULL, "media2_service_files/GetVideoEncoderConfigurations.xml", 12,
                "%H264PROFILE%", "High",
                "%PROFILE%", "Profile_0",
                "%WIDTH%", stmp_w_h,
                "%HEIGHT%", stmp_h_h,
                "%FRAMERATE%", stmp_fps_h,
                "%VIDEO_ENCODING%", video_enc_h);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetVideoEncoderConfigurations.xml", 12,
                "%H264PROFILE%", "High",
                "%PROFILE%", "Profile_0",
                "%WIDTH%", stmp_w_h,
                "%HEIGHT%", stmp_h_h,
                "%FRAMERATE%", stmp_fps_h,
                "%VIDEO_ENCODING%", video_enc_h);

    } else if ((service_ctx.profiles_num == 2) &&
            (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {

        snprintf(stmp_w_l, sizeof(stmp_w_l), "%d", service_ctx.profiles[1].width);
        snprintf(stmp_h_l, sizeof(stmp_h_l), "%d", service_ctx.profiles[1].height);
        snprintf(stmp_fps_l, sizeof(stmp_fps_l), "%d", service_ctx.profiles[1].framerate > 0 ? service_ctx.profiles[1].framerate : 30);
        set_video_codec(video_enc_l, 16, service_ctx.profiles[1].type, 2);
        long size = cat(NULL, "media2_service_files/GetVideoEncoderConfigurations.xml", 12,
                "%H264PROFILE%", "Main",
                "%PROFILE%", "Profile_1",
                "%WIDTH%", stmp_w_l,
                "%HEIGHT%", stmp_h_l,
                "%FRAMERATE%", stmp_fps_l,
                "%VIDEO_ENCODING%", video_enc_l);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetVideoEncoderConfigurations.xml", 12,
                "%H264PROFILE%", "Main",
                "%PROFILE%", "Profile_1",
                "%WIDTH%", stmp_w_l,
                "%HEIGHT%", stmp_h_l,
                "%FRAMERATE%", stmp_fps_l,
                "%VIDEO_ENCODING%", video_enc_l);
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -1;
    }
}

int media2_get_video_encoder_configuration_options()
{
    char stmp_w[16], stmp_h[16];
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[10];
    char video_enc[16];

    memset(token, '\0', sizeof(token));
    if (configuration_token != NULL) {
        // Extract "Profile_x" from token Profile_x_VideoEncoderConfig
        strncpy(token, configuration_token, 9);
    } else if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else if (service_ctx.profiles_num > 0) {
        strncpy(token, service_ctx.profiles[0].name, 9);
    }

    if ((service_ctx.profiles_num > 0) &&
            (strcasecmp(service_ctx.profiles[0].name, token) == 0)) {

        snprintf(stmp_w, sizeof(stmp_w), "%d", service_ctx.profiles[0].width);
        snprintf(stmp_h, sizeof(stmp_h), "%d", service_ctx.profiles[0].height);
        set_video_codec(video_enc, 16, service_ctx.profiles[0].type, 2);
        long size = cat(NULL, "media2_service_files/GetVideoEncoderConfigurationOptions.xml", 8,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h,
                "%H264PROFILE%", "High",
                "%VIDEO_ENCODING%", video_enc);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetVideoEncoderConfigurationOptions.xml", 8,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h,
                "%H264PROFILE%", "High",
                "%VIDEO_ENCODING%", video_enc);

    } else if ((service_ctx.profiles_num == 2) &&
            (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {

        snprintf(stmp_w, sizeof(stmp_w), "%d", service_ctx.profiles[1].width);
        snprintf(stmp_h, sizeof(stmp_h), "%d", service_ctx.profiles[1].height);
        set_video_codec(video_enc, 16, service_ctx.profiles[1].type, 2);
        long size = cat(NULL, "media2_service_files/GetVideoEncoderConfigurationOptions.xml", 8,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h,
                "%H264PROFILE%", "Main",
                "%VIDEO_ENCODING%", video_enc);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetVideoEncoderConfigurationOptions.xml", 8,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h,
                "%H264PROFILE%", "Main",
                "%VIDEO_ENCODING%", video_enc);

    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -1;
    }
}

int media2_get_audio_source_configurations()
{
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[23];
    char stmp_w[16], stmp_h[16];
    char s_profiles_num[2];
    int profiles_num;

    memset(token, '\0', sizeof(token));
    if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else if (configuration_token != NULL) {
        strncpy(token, configuration_token, 22);
    } else if (service_ctx.profiles_num > 0) {
        strncpy(token, service_ctx.profiles[0].name, 9);
    }

    if ((strcasecmp(service_ctx.profiles[0].name, token) == 0) ||
            (strcasecmp(service_ctx.profiles[1].name, token) == 0) ||
            (strcasecmp("AudioSourceConfigToken", token) == 0)) {

        profiles_num = service_ctx.profiles_num;

        if (profiles_num == 1) {
            if (service_ctx.profiles[0].audio_encoder == AUDIO_NONE) {
                profiles_num--;
            }
        } else if (profiles_num == 2) {
            if (service_ctx.profiles[0].audio_encoder == AUDIO_NONE) {
                profiles_num--;
            }
            if (service_ctx.profiles[1].audio_encoder == AUDIO_NONE) {
                profiles_num--;
            }
        }

        snprintf(s_profiles_num, sizeof(s_profiles_num), "%d", profiles_num);

        if (profiles_num > 0) {
            long size = cat(NULL, "media2_service_files/GetAudioSourceConfigurations.xml", 2,
                    "%PROFILES_NUM%", s_profiles_num);

            output_http_headers(size);

            return cat("stdout", "media2_service_files/GetAudioSourceConfigurations.xml", 2,
                    "%PROFILES_NUM%", s_profiles_num);
        } else {
            send_fault("media2_service", "Receiver", "ter:ActionNotSupported", "ter:AudioNotSupported", "AudioNotSupported", "The device does not support audio");
            return -1;
        }
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The requested configuration indicated does not exist");
        return -2;
    }
}

int media2_get_audio_source_configuration_options()
{
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[23];

    memset(token, '\0', sizeof(token));
    if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else if (configuration_token != NULL) {
        strncpy(token, configuration_token, 22);
    } else {
        if ((service_ctx.profiles_num > 0) && (service_ctx.profiles[0].audio_encoder != AUDIO_NONE)) {
            strncpy(token, service_ctx.profiles[0].name, 9);
        } else if ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE)) {
            strncpy(token, service_ctx.profiles[1].name, 9);
        } else {
            send_fault("media2_service", "Receiver", "ter:ActionNotSupported", "ter:AudioNotSupported", "AudioNotSupported", "The device does not support audio");
            return -1;
        }
    }

    if (((service_ctx.profiles_num > 0) && (service_ctx.profiles[0].audio_encoder != AUDIO_NONE)) ||
            ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE))) {

        if (((service_ctx.profiles_num > 0) && (strcasecmp(service_ctx.profiles[0].name, token) == 0)) ||
                ((service_ctx.profiles_num == 2) && (strcasecmp(service_ctx.profiles[1].name, token) == 0)) ||
                (strcasecmp("AudioSourceConfigToken", token) == 0)) {

            long size = cat(NULL, "media2_service_files/GetAudioSourceConfigurationOptions.xml", 0);

            output_http_headers(size);

            return cat("stdout", "media2_service_files/GetAudioSourceConfigurationOptions.xml", 0);
        } else {
            send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The requested configuration indicated does not exist");
            return -2;
        }
    } else {
        send_fault("media2_service", "Receiver", "ter:ActionNotSupported", "ter:AudioNotSupported", "AudioNotSupported", "The device does not support audio");
        return -3;
    }
}

int media2_get_audio_encoder_configurations()
{
    char audio_enc_h[16];
    char audio_enc_l[16];
    const char *profile_token = get_element("ProfileToken", "Body");
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    char token[10];

    memset(token, '\0', sizeof(token));
    if (configuration_token != NULL) {
        // Extract "Profile_x" from token Profile_x_VideoEncoderConfig
        strncpy(token, configuration_token, 9);
    } else if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    }

    if (((service_ctx.profiles_num == 0) && (service_ctx.profiles[0].audio_encoder == AUDIO_NONE)) ||
            ((service_ctx.profiles_num == 2) &&
            (service_ctx.profiles[0].audio_encoder == AUDIO_NONE) && (service_ctx.profiles[1].audio_encoder == AUDIO_NONE))) {

        send_fault("media2_service", "Receiver", "ter:ActionNotSupported", "ter:AudioNotSupported", "AudioNotSupported", "The device does not support audio");
        return -1;
    }

    if (token[0] == '\0') {
        if (service_ctx.profiles_num == 1) {
            if (service_ctx.profiles[0].audio_encoder != AUDIO_NONE) {
                set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
                long size = cat(NULL, "media2_service_files/GetAudioEncoderConfigurations.xml", 4,
                        "%PROFILE%", "Profile_0",
                        "%AUDIO_ENCODING%", audio_enc_h);

                output_http_headers(size);

                return cat("stdout", "media2_service_files/GetAudioEncoderConfigurations.xml", 4,
                        "%PROFILE%", "Profile_0",
                        "%AUDIO_ENCODING%", audio_enc_h);

            } else {
                send_fault("media2_service", "Receiver", "ter:ActionNotSupported", "ter:AudioNotSupported", "AudioNotSupported", "The device does not support audio");
                return -2;
            }

        } else if (service_ctx.profiles_num == 2) {
            if ((service_ctx.profiles[0].audio_encoder != AUDIO_NONE) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE)) {
                set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
                set_audio_codec(audio_enc_l, 16, service_ctx.profiles[1].audio_encoder, 2);
                long size = cat(NULL, "media2_service_files/GetAudioEncoderConfigurations_both.xml", 4,
                        "%AUDIO_ENCODING_HIGH%", audio_enc_h,
                        "%AUDIO_ENCODING_LOW%", audio_enc_l);

                output_http_headers(size);

                return cat("stdout", "media2_service_files/GetAudioEncoderConfigurations_both.xml", 4,
                        "%AUDIO_ENCODING_HIGH%", audio_enc_h,
                        "%AUDIO_ENCODING_LOW%", audio_enc_l);

            } else if (service_ctx.profiles[0].audio_encoder != AUDIO_NONE) {
                set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
                set_audio_codec(audio_enc_l, 16, service_ctx.profiles[1].audio_encoder, 2);
                long size = cat(NULL, "media2_service_files/GetAudioEncoderConfigurations.xml", 4,
                        "%PROFILE%", "Profile_0",
                        "%AUDIO_ENCODING%", audio_enc_h);

                output_http_headers(size);

                return cat("stdout", "media2_service_files/GetAudioEncoderConfigurations.xml", 4,
                        "%PROFILE%", "Profile_0",
                        "%AUDIO_ENCODING%", audio_enc_h);

            } else if (service_ctx.profiles[1].audio_encoder != AUDIO_NONE) {
                set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
                set_audio_codec(audio_enc_l, 16, service_ctx.profiles[1].audio_encoder, 2);
                long size = cat(NULL, "media2_service_files/GetAudioEncoderConfigurations.xml", 4,
                        "%PROFILE%", "Profile_1",
                        "%AUDIO_ENCODING%", audio_enc_l);

                output_http_headers(size);

                return cat("stdout", "media2_service_files/GetAudioEncoderConfigurations.xml", 4,
                        "%PROFILE%", "Profile_1",
                        "%AUDIO_ENCODING%", audio_enc_l);

            } else {
                send_fault("media2_service", "Receiver", "ter:ActionNotSupported", "ter:AudioNotSupported", "AudioNotSupported", "The device does not support audio");
                return -3;
            }
        }
    } else if ((service_ctx.profiles_num > 0) &&
            (strcasecmp(service_ctx.profiles[0].name, token) == 0)) {

        if (service_ctx.profiles[0].audio_encoder != AUDIO_NONE) {

            set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
            long size = cat(NULL, "media2_service_files/GetAudioEncoderConfigurations.xml", 4,
                    "%PROFILE%", "Profile_0",
                    "%AUDIO_ENCODING%", audio_enc_h);

            output_http_headers(size);

            return cat("stdout", "media2_service_files/GetAudioEncoderConfigurations.xml", 4,
                    "%PROFILE%", "Profile_0",
                    "%AUDIO_ENCODING%", audio_enc_h);
        } else {
            send_fault("media2_service", "Receiver", "ter:ActionNotSupported", "ter:AudioNotSupported", "AudioNotSupported", "The device does not support audio");
            return -4;
        }

    } else if ((service_ctx.profiles_num == 2) &&
            (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {

        if (service_ctx.profiles[1].audio_encoder != AUDIO_NONE) {
            set_audio_codec(audio_enc_l, 16, service_ctx.profiles[1].audio_encoder, 2);
            long size = cat(NULL, "media2_service_files/GetAudioEncoderConfigurations.xml", 4,
                    "%PROFILE%", "Profile_1",
                    "%AUDIO_ENCODING%", audio_enc_l);

            output_http_headers(size);

            return cat("stdout", "media2_service_files/GetAudioEncoderConfigurations.xml", 4,
                    "%PROFILE%", "Profile_1",
                    "%AUDIO_ENCODING%", audio_enc_l);
        } else {
            send_fault("media2_service", "Receiver", "ter:ActionNotSupported", "ter:AudioNotSupported", "AudioNotSupported", "The device does not support audio");
            return -5;
        }
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -6;
    }
}

int media2_get_audio_encoder_configuration_options()
{
    char audio_enc[16];
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[10];

    char bitrate[4], samplerate[4];

    memset(token, '\0', sizeof(token));
    if (configuration_token != NULL) {
        // Extract "Profile_x" from token Profile_x_VideoEncoderConfig
        strncpy(token, configuration_token, 9);
    } else if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else {
        if ((service_ctx.profiles_num > 0) && (service_ctx.profiles[0].audio_encoder != AUDIO_NONE)) {
            strncpy(token, service_ctx.profiles[0].name, 9);
        } else if ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE)) {
            strncpy(token, service_ctx.profiles[1].name, 9);
        } else {
            send_fault("media2_service", "Receiver", "ter:ActionNotSupported", "ter:AudioNotSupported", "AudioNotSupported", "The device does not support audio");
            return -1;
        }
    }

    if ((service_ctx.profiles_num > 0) &&
            (strcasecmp(service_ctx.profiles[0].name, token) == 0)) {

        // G726 is not suppoerted
        if ((service_ctx.profiles[0].audio_encoder != AUDIO_NONE) && (service_ctx.profiles[0].audio_encoder != G726)) {

            set_audio_codec(audio_enc, 16, service_ctx.profiles[0].audio_encoder, 2);

            if (service_ctx.profiles[0].audio_encoder == G711) {
                snprintf(bitrate, sizeof(bitrate), "%d", 64);
                snprintf(samplerate, sizeof(samplerate), "%d", 8);
            } else if (service_ctx.profiles[0].audio_encoder == AAC) {
                snprintf(bitrate, sizeof(bitrate), "%d", 50);
                snprintf(samplerate, sizeof(samplerate), "%d", 16);
            }
        } else {
            send_fault("media2_service", "Receiver", "ter:ActionNotSupported", "ter:AudioNotSupported", "AudioNotSupported", "The device does not support audio");
            return -2;
        }
    } else if ((service_ctx.profiles_num == 2) &&
            (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {

        // G726 is not suppoerted
        if ((service_ctx.profiles[1].audio_encoder != AUDIO_NONE) && (service_ctx.profiles[1].audio_encoder != G726)) {

            set_audio_codec(audio_enc, 16, service_ctx.profiles[1].audio_encoder, 2);

            if (service_ctx.profiles[1].audio_encoder == G711) {
                snprintf(bitrate, sizeof(bitrate), "%d", 64);
                snprintf(samplerate, sizeof(samplerate), "%d", 8);
            } else if (service_ctx.profiles[1].audio_encoder == AAC) {
                snprintf(bitrate, sizeof(bitrate), "%d", 50);
                snprintf(samplerate, sizeof(samplerate), "%d", 16);
            }
        } else {
            send_fault("media2_service", "Receiver", "ter:ActionNotSupported", "ter:AudioNotSupported", "AudioNotSupported", "The device does not support audio");
            return -3;
        }
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -4;
    }

    long size = cat(NULL, "media2_service_files/GetAudioEncoderConfigurationOptions.xml", 6,
            "%AUDIO_ENCODING%", audio_enc,
            "%BITRATE%", bitrate,
            "%SAMPLERATE%", samplerate);

    output_http_headers(size);

    return cat("stdout", "media2_service_files/GetAudioEncoderConfigurationOptions.xml", 6,
            "%AUDIO_ENCODING%", audio_enc,
            "%BITRATE%", bitrate,
            "%SAMPLERATE%", samplerate);
}

int media2_get_audio_output_configurations()
{
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[23];
    char stmp_w[16], stmp_h[16];
    char s_profiles_num[2];
    int profiles_num;

    memset(token, '\0', sizeof(token));
    if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else if (configuration_token != NULL) {
        strncpy(token, configuration_token, 22);
    } else if (service_ctx.profiles_num > 0) {
        strncpy(token, service_ctx.profiles[0].name, 9);
    }

    if ((strcasecmp(service_ctx.profiles[0].name, token) == 0) ||
            (strcasecmp(service_ctx.profiles[1].name, token) == 0) ||
            (strcasecmp("AudioOutputConfigToken", token) == 0)) {

        profiles_num = service_ctx.profiles_num;

        if (profiles_num == 1) {
            if (service_ctx.profiles[0].audio_decoder == AUDIO_NONE) {
                profiles_num--;
            }
        } else if (profiles_num == 2) {
            if (service_ctx.profiles[0].audio_decoder == AUDIO_NONE) {
                profiles_num--;
            }
            if (service_ctx.profiles[1].audio_decoder == AUDIO_NONE) {
                profiles_num--;
            }
        }

        snprintf(s_profiles_num, sizeof(s_profiles_num), "%d", profiles_num);

        if (profiles_num > 0) {
            long size = cat(NULL, "media2_service_files/GetAudioOutputConfigurations.xml", 2,
                    "%PROFILES_NUM%", s_profiles_num);

            output_http_headers(size);

            return cat("stdout", "media2_service_files/GetAudioOutputConfigurations.xml", 2,
                    "%PROFILES_NUM%", s_profiles_num);
        } else {
            send_fault("media2_service", "Receiver", "ter:ActionNotSupported", "ter:AudioOutputNotSupported", "AudioOutputNotSupported", "Audio or Audio Outputs are not supported by the device");
            return -1;
        }
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The requested configuration indicated does not exist");
        return -2;
    }
}

int media2_get_audio_output_configuration_options()
{
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[23];

    memset(token, '\0', sizeof(token));
    if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else if (configuration_token != NULL) {
        strncpy(token, configuration_token, 22);
    } else {
        if ((service_ctx.profiles_num > 0) && (service_ctx.profiles[0].audio_decoder != AUDIO_NONE)) {
            strncpy(token, service_ctx.profiles[0].name, 9);
        } else if ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_decoder != AUDIO_NONE)) {
            strncpy(token, service_ctx.profiles[1].name, 9);
        } else {
            send_fault("media2_service", "Receiver", "ter:ActionNotSupported", "ter:AudioOutputNotSupported", "AudioOutputNotSupported", "Audio or Audio Outputs are not supported by the device");
            return -1;
        }
    }

    if (((service_ctx.profiles_num > 0) && (service_ctx.profiles[0].audio_decoder != AUDIO_NONE)) ||
            ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_decoder != AUDIO_NONE))) {

        if (((service_ctx.profiles_num > 0) && (strcasecmp(service_ctx.profiles[0].name, token) == 0)) ||
                ((service_ctx.profiles_num == 2) && (strcasecmp(service_ctx.profiles[1].name, token) == 0)) ||
                (strcasecmp("AudioOutputConfigToken", token) == 0)) {

            long size = cat(NULL, "media2_service_files/GetAudioOutputConfigurationOptions.xml", 0);

            output_http_headers(size);

            return cat("stdout", "media2_service_files/GetAudioOutputConfigurationOptions.xml", 0);
        } else {
            send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The requested configuration indicated does not exist");
            return -2;
        }
    } else {
        send_fault("media2_service", "Receiver", "ter:ActionNotSupported", "ter:AudioOutputNotSupported", "AudioOutputNotSupported", "Audio or Audio Outputs are not supported by the device");
        return -3;
    }
}

int media2_get_audio_decoder_configurations()
{
    const char *profile_token = get_element("ProfileToken", "Body");
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    char token[10];

    memset(token, '\0', sizeof(token));
    if (configuration_token != NULL) {
        // Extract "Profile_x" from token Profile_x_VideoDecoderConfig
        strncpy(token, configuration_token, 9);
    } else if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    }

    if (token[0] == '\0') {
        if (service_ctx.profiles_num == 1) {
            if (service_ctx.profiles[0].audio_decoder != AUDIO_NONE) {
                long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations.xml", 2,
                        "%PROFILE%", "Profile_0");

                output_http_headers(size);

                return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations.xml", 2,
                        "%PROFILE%", "Profile_0");
            } else {
                send_fault("media2_service2", "Receiver", "ter:ActionNotSupported", "ter:AudioDecodingNotSupported", "AudioDecodingNotSupported", "Audio or Audio decoding is not supported by the device");
                return -1;
            }
        } else if (service_ctx.profiles_num == 2) {
            if ((service_ctx.profiles[0].audio_decoder != AUDIO_NONE) && (service_ctx.profiles[1].audio_decoder != AUDIO_NONE)) {
                long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations_both.xml", 0);

                output_http_headers(size);

                return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations_both.xml", 0);

            } else if (service_ctx.profiles[0].audio_decoder != AUDIO_NONE) {

                long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations.xml", 2,
                        "%PROFILE%", "Profile_0");

                output_http_headers(size);

                return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations.xml", 2,
                        "%PROFILE%", "Profile_0");
            } else if (service_ctx.profiles[1].audio_decoder != AUDIO_NONE) {

                long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations.xml", 2,
                        "%PROFILE%", "Profile_1");

                output_http_headers(size);

                return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations.xml", 2,
                        "%PROFILE%", "Profile_1");
            } else {
                send_fault("media2_service2", "Receiver", "ter:ActionNotSupported", "ter:AudioDecodingNotSupported", "AudioDecodingNotSupported", "Audio or Audio decoding is not supported by the device");
                return -2;
            }
        }
    } else if ((service_ctx.profiles_num > 0) && (strcasecmp(service_ctx.profiles[0].name, token) == 0)) {

        if (service_ctx.profiles[0].audio_decoder != AUDIO_NONE) {

            long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations.xml", 2,
                    "%PROFILE%", "Profile_0");

            output_http_headers(size);

            return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations.xml", 2,
                    "%PROFILE%", "Profile_0");
        } else {
            send_fault("media2_service2", "Receiver", "ter:ActionNotSupported", "ter:AudioDecodingNotSupported", "AudioDecodingNotSupported", "Audio or Audio decoding is not supported by the device");
            return -3;
        }
    } else if ((service_ctx.profiles_num == 2) && (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {

        if (service_ctx.profiles[1].audio_decoder != AUDIO_NONE) {

            long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations.xml", 2,
                    "%PROFILE%", "Profile_1");

            output_http_headers(size);

            return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations.xml", 2,
                    "%PROFILE%", "Profile_1");
        } else {
            send_fault("media2_service2", "Receiver", "ter:ActionNotSupported", "ter:AudioDecodingNotSupported", "AudioDecodingNotSupported", "Audio or Audio decoding is not supported by the device");
            return -4;
        }
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The requested configuration indicated does not exist");
        return -5;
    }
}

int media2_get_audio_decoder_configuration_options()
{
    int decoder_type = AUDIO_NONE;
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[10];
    char audio_decoder[16];
    char bitrate[4], samplerate[4];

    memset(token, '\0', sizeof(token));
    if (configuration_token != NULL) {
        // Extract "Profile_x" from token Profile_x_VideoEncoderConfig
        strncpy(token, configuration_token, 9);
    } else if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else {
        if ((service_ctx.profiles_num > 0) && (service_ctx.profiles[0].audio_decoder != AUDIO_NONE)) {
            strncpy(token, service_ctx.profiles[0].name, 9);
        } else if ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_decoder != AUDIO_NONE)) {
            strncpy(token, service_ctx.profiles[1].name, 9);
        } else {
            send_fault("media2_service", "Receiver", "ter:ActionNotSupported", "ter:AudioDecodingNotSupported", "AudioDecodingNotSupported", "Audio or Audio decoding is not supported by the device");
            return -1;
        }
    }

    if ((service_ctx.profiles_num > 0) && (strcasecmp(service_ctx.profiles[0].name, token) == 0)) {
        decoder_type = service_ctx.profiles[0].audio_decoder;
    } else if ((service_ctx.profiles_num == 2) && (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {
        decoder_type = service_ctx.profiles[1].audio_decoder;
    }

    // G726 is not suppoerted
    if ((decoder_type != AUDIO_NONE) && (decoder_type != G726)) {

        set_audio_codec(audio_decoder, 16, decoder_type, 2);
        if (decoder_type == G711) {
            snprintf(bitrate, sizeof(bitrate), "%d", 64);
            snprintf(samplerate, sizeof(samplerate), "%d", 8);
        } else if (decoder_type == AAC) {
            snprintf(bitrate, sizeof(bitrate), "%d", 50);
            snprintf(samplerate, sizeof(samplerate), "%d", 16);
        }

        long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurationOptions.xml", 6,
                "%AUDIO_DECODING%", audio_decoder,
                "%BITRATE%", bitrate,
                "%SAMPLERATE%", samplerate);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetAudioDecoderConfigurationOptions.xml", 6,
                "%AUDIO_DECODING%", audio_decoder,
                "%BITRATE%", bitrate,
                "%SAMPLERATE%", samplerate);

    } else {
        send_fault("media2_service", "Receiver", "ter:ActionNotSupported", "ter:AudioDecodingNotSupported", "AudioDecodingNotSupported", "Audio or Audio decoding is not supported by the device");
        return -2;
    }
}

int media2_get_snapshot_uri()
{
    char *s;
    char line[MAX_LEN];
    const char *profile_token = get_element("ProfileToken", "Body");

    memset(line, '\0', sizeof(line));

    if (profile_token == NULL) {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -1;
    }

    if ((service_ctx.profiles_num > 0) &&
            (strcasecmp(service_ctx.profiles[0].name, profile_token) == 0)) {

        if (service_ctx.profiles[0].snapurl == NULL) {
            send_fault("media2_service", "Receiver", "ter:Action", "ter:IncompleteConfiguration", "Incomplete configuration", "The specified media profile does not contain either a reference to a video encoder configuration or a reference to a video source configuration");
            return -2;
        }

        if (snprintf(line, sizeof(line), service_ctx.profiles[0].snapurl, service_ctx.address_url) < 0) {
            snprintf(line, sizeof(line), "%s", service_ctx.profiles[0].snapurl);
        }
        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media2_service_files/GetSnapshotUri.xml", 2,
                    "%URI%", line);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetSnapshotUri.xml", 2,
                    "%URI%", line);

    } else if ((service_ctx.profiles_num == 2) &&
            (strcasecmp(service_ctx.profiles[1].name, profile_token) == 0)) {

        if (service_ctx.profiles[1].snapurl == NULL) {
            send_fault("media2_service", "Receiver", "ter:Action", "ter:IncompleteConfiguration", "Incomplete configuration", "The specified media profile does not contain either a reference to a video encoder configuration or a reference to a video source configuration");
            return -3;
        }

        if (snprintf(line, sizeof(line), service_ctx.profiles[1].snapurl, service_ctx.address_url) < 0) {
            snprintf(line, sizeof(line), "%s", service_ctx.profiles[1].snapurl);
        }
        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media2_service_files/GetSnapshotUri.xml", 2,
                    "%URI%", line);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetSnapshotUri.xml", 2,
                    "%URI%", line);

    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -4;
    }
}

int media2_get_stream_uri()
{
    char *s;
    char line[MAX_LEN];
    const char *profile_token = get_element("ProfileToken", "Body");

    memset(line, '\0', sizeof(line));

    if (profile_token == NULL) {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -1;
    }

    if (strcasecmp(service_ctx.profiles[0].name, profile_token) == 0) {

        if (service_ctx.profiles[0].url == NULL) {
            send_fault("media2_service", "Receiver", "ter:Action", "ter:IncompleteConfiguration", "Incomplete configuration", "The specified media profile does contain either unused sources or encoder configurations without a corresponding source");
            return -2;
        }

        if (snprintf(line, sizeof(line), service_ctx.profiles[0].url, service_ctx.address_url) < 0) {
            snprintf(line, sizeof(line), "%s", service_ctx.profiles[0].url);
        }
        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media2_service_files/GetStreamUri.xml", 2,
                    "%URI%", line);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetStreamUri.xml", 2,
                    "%URI%", line);

    } else if ((service_ctx.profiles_num == 2) &&
            (strcasecmp(service_ctx.profiles[1].name, profile_token) == 0)) {

        if (service_ctx.profiles[1].url == NULL) {
            send_fault("media2_service", "Receiver", "ter:Action", "ter:IncompleteConfiguration", "Incomplete configuration", "The specified media profile does contain either unused sources or encoder configurations without a corresponding source");
            return -3;
        }

        if (snprintf(line, sizeof(line), service_ctx.profiles[1].url, service_ctx.address_url) < 0) {
            snprintf(line, sizeof(line), "%s", service_ctx.profiles[1].url);
        }
        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media2_service_files/GetStreamUri.xml", 2,
                    "%URI%", line);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetStreamUri.xml", 2,
                    "%URI%", line);

    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -4;
    }
}

int media2_set_video_source_configuration()
{
    const char *token = NULL;
    ezxml_t node;

    node = get_element_ptr(NULL, "Configuration", "Body");
    if (node != NULL) {
        token = get_attribute(node, "token");
    }

    if ((node == NULL) || (token == NULL)) {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -1;
    }

    if (strcasecmp("VideoSourceConfigToken", token) == 0) {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:ConfigModify", "Config modify", "The configuration parameters are not possible to set");
        return -2;
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -3;
    }
}

int media2_set_audio_source_configuration()
{
    const char *token = NULL;
    ezxml_t node;

    node = get_element_ptr(NULL, "Configuration", "Body");
    if (node != NULL) {
        token = get_attribute(node, "token");
    }

    if ((node == NULL) || (token == NULL)) {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -1;
    }

    if (strcasecmp("AudioSourceConfigToken", token) == 0) {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:ConfigModify", "Config modify", "The configuration parameters are not possible to set");
        return -2;
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -3;
    }
}

int media2_set_video_encoder_configuration()
{
    send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:ConfigModify", "Config modify", "The configuration parameters are not possible to set");
    return -1;
}

int media2_set_audio_encoder_configuration()
{
    send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:ConfigModify", "Config modify", "The configuration parameters are not possible to set");
    return -1;
}

int media2_set_audio_output_configuration()
{
    const char *token = NULL;
    ezxml_t node;

    node = get_element_ptr(NULL, "Configuration", "Body");
    if (node != NULL) {
        token = get_attribute(node, "token");
    }

    if ((node == NULL) || (token == NULL)) {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -1;
    }

    if (strcasecmp("AudioOutputConfigToken", token) == 0) {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:ConfigModify", "Config modify", "The configuration parameters are not possible to set");
        return -2;
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -3;
    }
}

int media2_unsupported(const char *method)
{
    /* An unimplemented action must return a SOAP fault, not an empty 200
     * response (ONVIF: env:Receiver / ter:ActionNotSupported). */
    (void) method;
    return send_action_not_supported_fault("media2_service");
}
