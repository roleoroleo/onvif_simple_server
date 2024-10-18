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
#include <string.h>

#include "media2_service.h"
#include "conf.h"
#include "fault.h"
#include "utils.h"
#include "log.h"
#include "ezxml_wrapper.h"
#include "onvif_simple_server.h"

extern service_context_t service_ctx;

int media2_get_service_capabilities()
{
    if (service_ctx.ptz_node.enable == 0) {
        long size = cat(NULL, "media2_service_files/GetServiceCapabilities_no_ptz.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetServiceCapabilities_no_ptz.xml", 0);
    } else {
        long size = cat(NULL, "media2_service_files/GetServiceCapabilities_ptz.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetServiceCapabilities_ptz.xml", 0);
    }
}

int media2_get_profiles()
{
    char profiles_num[2];
    char stmp_w_h[16], stmp_h_h[16];
    char stmp_w_l[16], stmp_h_l[16];
    char audio_enc_h[16], audio_enc_l[16];
    char video_enc_h[16], video_enc_l[16];
    const char *profile_token = get_element("Token", "Body");
    const char *type = get_element("Type", "Body");
    char xml_name[MAX_LEN];

    audio_enc_h[0] = '\0';
    audio_enc_l[0] = '\0';
    video_enc_h[0] = '\0';
    video_enc_l[0] = '\0';
    sprintf(profiles_num, "%d", service_ctx.profiles_num);

    // Handle only type=NULL and type="All"
    if (service_ctx.profiles_num == 0) {
        long size = cat(NULL, "media2_service_files/GetProfiles_none.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetProfiles_high.xml", 0);

    } else if ((type == NULL) && (profile_token == NULL)) {
        if (service_ctx.profiles_num == 1) {
            long size = cat(NULL, "media2_service_files/GetProfiles_high.xml", 0);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "media2_service_files/GetProfiles_high.xml", 0);

        } else if (service_ctx.profiles_num == 2) {
            long size = cat(NULL, "media2_service_files/GetProfiles_both.xml", 0);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "media2_service_files/GetProfiles_both.xml", 0);
        }
    } else if ((type == NULL) && (service_ctx.profiles_num > 0) &&
            (strcasecmp(service_ctx.profiles[0].name, profile_token) == 0)) {

        long size = cat(NULL, "media2_service_files/GetProfiles_high.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetProfiles_high.xml", 0);

    } else if ((type == NULL) && (service_ctx.profiles_num == 2) &&
            (strcasecmp(service_ctx.profiles[1].name, profile_token) == 0)) {

        long size = cat(NULL, "media2_service_files/GetProfiles_low.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetProfiles_low.xml", 0);

    } else if ((type != NULL) && (profile_token == NULL) && (service_ctx.profiles_num == 1)) {

        if (service_ctx.profiles[0].audio_decoder != AUDIO_NONE) {

            strcpy(xml_name, "media2_service_files/GetProfiles_high_audio_decoder_high_config.xml");

        } else {

            strcpy(xml_name, "media2_service_files/GetProfiles_high_audio_decoder_none_config.xml");

        }

        sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
        set_video_codec(video_enc_h, 16, service_ctx.profiles[0].type, 2);
        set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
        long size = cat(NULL, xml_name, 14,
                "%PROFILES_NUM%", profiles_num,
                "%VSC_WIDTH%", stmp_w_h,
                "%VSC_HEIGHT%", stmp_h_h,
                "%VEC_WIDTH_HIGH%", stmp_w_h,
                "%VEC_HEIGHT_HIGH%", stmp_h_h,
                "%VIDEO_ENCODING_HIGH%", video_enc_h,
                "%AUDIO_ENCODING_HIGH%", audio_enc_h);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", xml_name, 14,
                "%PROFILES_NUM%", profiles_num,
                "%VSC_WIDTH%", stmp_w_h,
                "%VSC_HEIGHT%", stmp_h_h,
                "%VEC_WIDTH_HIGH%", stmp_w_h,
                "%VEC_HEIGHT_HIGH%", stmp_h_h,
                "%VIDEO_ENCODING_HIGH%", video_enc_h,
                "%AUDIO_ENCODING_HIGH%", audio_enc_h);

    } else if ((type != NULL) && (profile_token == NULL) && (service_ctx.profiles_num == 2)) {

        if ((service_ctx.profiles[0].audio_decoder != AUDIO_NONE) && (service_ctx.profiles[1].audio_decoder == AUDIO_NONE)) {

            strcpy(xml_name, "media2_service_files/GetProfiles_both_audio_decoder_high_config.xml");

        } else if ((service_ctx.profiles[0].audio_decoder == AUDIO_NONE) && (service_ctx.profiles[1].audio_decoder != AUDIO_NONE)) {

            strcpy(xml_name, "media2_service_files/GetProfiles_both_audio_decoder_low_config.xml");

        } else if ((service_ctx.profiles[0].audio_decoder != AUDIO_NONE) && (service_ctx.profiles[1].audio_decoder != AUDIO_NONE)) {

            strcpy(xml_name, "media2_service_files/GetProfiles_both_audio_decoder_both_config.xml");

        } else {

            strcpy(xml_name, "media2_service_files/GetProfiles_both_audio_decoder_none_config.xml");

        }

        sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
        sprintf(stmp_w_l, "%d", service_ctx.profiles[1].width);
        sprintf(stmp_h_l, "%d", service_ctx.profiles[1].height);
        set_video_codec(video_enc_h, 16, service_ctx.profiles[0].type, 2);
        set_video_codec(video_enc_l, 16, service_ctx.profiles[1].type, 2);
        set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
        set_audio_codec(audio_enc_l, 16, service_ctx.profiles[1].audio_encoder, 2);
        long size = cat(NULL, xml_name, 22,
                    "%PROFILES_NUM%", profiles_num,
                    "%VSC_WIDTH%", stmp_w_h,
                    "%VSC_HEIGHT%", stmp_h_h,
                    "%VEC_WIDTH_HIGH%", stmp_w_h,
                    "%VEC_HEIGHT_HIGH%", stmp_h_h,
                    "%VEC_WIDTH_LOW%", stmp_w_l,
                    "%VEC_HEIGHT_LOW%", stmp_h_l,
                    "%VIDEO_ENCODING_HIGH%", video_enc_h,
                    "%VIDEO_ENCODING_LOW%", video_enc_l,
                    "%AUDIO_ENCODING_HIGH%", audio_enc_h,
                    "%AUDIO_ENCODING_LOW%", audio_enc_l);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", xml_name, 22,
                    "%PROFILES_NUM%", profiles_num,
                    "%VSC_WIDTH%", stmp_w_h,
                    "%VSC_HEIGHT%", stmp_h_h,
                    "%VEC_WIDTH_HIGH%", stmp_w_h,
                    "%VEC_HEIGHT_HIGH%", stmp_h_h,
                    "%VEC_WIDTH_LOW%", stmp_w_l,
                    "%VEC_HEIGHT_LOW%", stmp_h_l,
                    "%VIDEO_ENCODING_HIGH%", video_enc_h,
                    "%VIDEO_ENCODING_LOW%", video_enc_l,
                    "%AUDIO_ENCODING_HIGH%", audio_enc_h,
                    "%AUDIO_ENCODING_LOW%", audio_enc_l);

    } else if ((type != NULL) && (strcasecmp(service_ctx.profiles[0].name, profile_token) == 0)) {

        if (service_ctx.profiles[0].audio_decoder != AUDIO_NONE) {

            strcpy(xml_name, "media2_service_files/GetProfiles_high_audio_decoder_high_config.xml");

        } else if (service_ctx.profiles[0].audio_decoder == AUDIO_NONE) {

            strcpy(xml_name, "media2_service_files/GetProfiles_high_audio_decoder_none_config.xml");

        }

        sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
        set_video_codec(video_enc_h, 16, service_ctx.profiles[0].type, 2);
        set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
        long size = cat(NULL, xml_name, 14,
                "%PROFILES_NUM%", profiles_num,
                "%VSC_WIDTH%", stmp_w_h,
                "%VSC_HEIGHT%", stmp_h_h,
                "%VEC_WIDTH_HIGH%", stmp_w_h,
                "%VEC_HEIGHT_HIGH%", stmp_h_h,
                "%VIDEO_ENCODING_HIGH%", video_enc_h,
                "%AUDIO_ENCODING_HIGH%", audio_enc_h);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", xml_name, 14,
                "%PROFILES_NUM%", profiles_num,
                "%VSC_WIDTH%", stmp_w_h,
                "%VSC_HEIGHT%", stmp_h_h,
                "%VEC_WIDTH_HIGH%", stmp_w_h,
                "%VEC_HEIGHT_HIGH%", stmp_h_h,
                "%VIDEO_ENCODING_HIGH%", video_enc_h,
                "%AUDIO_ENCODING_HIGH%", audio_enc_h);

    } else if ((type != NULL) && (service_ctx.profiles_num == 2) &&
            (strcasecmp(service_ctx.profiles[1].name, profile_token) == 0)) {

        if (service_ctx.profiles[1].audio_decoder != AUDIO_NONE) {

            strcpy(xml_name, "media2_service_files/GetProfiles_low_audio_decoder_low_config.xml");

        } else if (service_ctx.profiles[1].audio_decoder == AUDIO_NONE) {

            strcpy(xml_name, "media2_service_files/GetProfiles_low_audio_decoder_none_config.xml");

        }

        sprintf(stmp_w_l, "%d", service_ctx.profiles[1].width);
        sprintf(stmp_h_l, "%d", service_ctx.profiles[1].height);
        set_video_codec(video_enc_l, 16, service_ctx.profiles[1].type, 2);
        set_audio_codec(audio_enc_l, 16, service_ctx.profiles[1].audio_encoder, 2);
        long size = cat(NULL, xml_name, 14,
                "%PROFILES_NUM%", profiles_num,
                "%VSC_WIDTH%", stmp_w_l,
                "%VSC_HEIGHT%", stmp_h_l,
                "%VEC_WIDTH_LOW%", stmp_w_l,
                "%VEC_HEIGHT_LOW%", stmp_h_l,
                "%VIDEO_ENCODING_LOW%", video_enc_l,
                "%AUDIO_ENCODING_LOW%", audio_enc_l);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", xml_name, 14,
                "%PROFILES_NUM%", profiles_num,
                "%VSC_WIDTH%", stmp_w_l,
                "%VSC_HEIGHT%", stmp_h_l,
                "%VEC_WIDTH_LOW%", stmp_w_l,
                "%VEC_HEIGHT_LOW%", stmp_h_l,
                "%VIDEO_ENCODING_LOW%", video_enc_l,
                "%AUDIO_ENCODING_LOW%", audio_enc_l);
    } else {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -1;
    }
}

int media2_get_video_source_configurations()
{
    // Ignore ConfigurationToken and ProfileToken
    char profiles_num[2], stmp_w[16], stmp_h[16];

    sprintf(profiles_num, "%d", service_ctx.profiles_num);
    sprintf(stmp_w, "%d", service_ctx.profiles[0].width);
    sprintf(stmp_h, "%d", service_ctx.profiles[0].height);
    long size = cat(NULL, "media2_service_files/GetVideoSourceConfigurations.xml", 6,
            "%PROFILES_NUM%", profiles_num,
            "%WIDTH%", stmp_w,
            "%HEIGHT%", stmp_h);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "media2_service_files/GetVideoSourceConfigurations.xml", 6,
            "%PROFILES_NUM%", profiles_num,
            "%WIDTH%", stmp_w,
            "%HEIGHT%", stmp_h);
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

        sprintf(stmp_w, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h, "%d", service_ctx.profiles[0].height);
        long size = cat(NULL, "media2_service_files/GetVideoSourceConfigurationOptions.xml", 4,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetVideoSourceConfigurationOptions.xml", 4,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);

    } else {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The requested configuration indicated does not exist");
        return -1;
    }
}

int media2_get_video_encoder_configurations()
{
    char stmp_w_l[16], stmp_h_l[16];
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

    if (service_ctx.profiles_num == 0) {
        long size = cat(NULL, "media2_service_files/GetVideoEncoderConfigurations_none.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetVideoEncoderConfigurations_none.xml", 0);

    } else if (token[0] == '\0') {
        if (service_ctx.profiles_num == 1) {
            sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
            sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
            set_video_codec(video_enc_h, 16, service_ctx.profiles[0].type, 2);
            long size = cat(NULL, "media2_service_files/GetVideoEncoderConfigurations_high.xml", 6,
                    "%WIDTH_HIGH%", stmp_w_h,
                    "%HEIGHT_HIGH%", stmp_h_h,
                    "%VIDEO_ENCODING_HIGH%", video_enc_h);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "media2_service_files/GetVideoEncoderConfigurations_high.xml", 6,
                    "%WIDTH_HIGH%", stmp_w_h,
                    "%HEIGHT_HIGH%", stmp_h_h,
                    "%VIDEO_ENCODING_HIGH%", video_enc_h);

        } else if (service_ctx.profiles_num == 2) {
            sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
            sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
            sprintf(stmp_w_l, "%d", service_ctx.profiles[1].width);
            sprintf(stmp_h_l, "%d", service_ctx.profiles[1].height);
            set_video_codec(video_enc_h, 16, service_ctx.profiles[0].type, 2);
            set_video_codec(video_enc_l, 16, service_ctx.profiles[1].type, 2);
            long size = cat(NULL, "media2_service_files/GetVideoEncoderConfigurations_both.xml", 12,
                        "%WIDTH_HIGH%", stmp_w_h,
                        "%HEIGHT_HIGH%", stmp_h_h,
                        "%WIDTH_LOW%", stmp_w_l,
                        "%HEIGHT_LOW%", stmp_h_l,
                        "%VIDEO_ENCODING_HIGH%", video_enc_h,
                        "%VIDEO_ENCODING_LOW%", video_enc_l);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "media2_service_files/GetVideoEncoderConfigurations_both.xml", 12,
                        "%WIDTH_HIGH%", stmp_w_h,
                        "%HEIGHT_HIGH%", stmp_h_h,
                        "%WIDTH_LOW%", stmp_w_l,
                        "%HEIGHT_LOW%", stmp_h_l,
                        "%VIDEO_ENCODING_HIGH%", video_enc_h,
                        "%VIDEO_ENCODING_LOW%", video_enc_l);
        }
    } else if ((service_ctx.profiles_num > 0) &&
            (strcasecmp(service_ctx.profiles[0].name, token) == 0)) {

        sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
        set_video_codec(video_enc_h, 16, service_ctx.profiles[0].type, 2);
        long size = cat(NULL, "media2_service_files/GetVideoEncoderConfigurations_high.xml", 6,
                "%WIDTH_HIGH%", stmp_w_h,
                "%HEIGHT_HIGH%", stmp_h_h,
                "%VIDEO_ENCODING_HIGH%", video_enc_h);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetVideoEncoderConfigurations_high.xml", 6,
                "%WIDTH_HIGH%", stmp_w_h,
                "%HEIGHT_HIGH%", stmp_h_h,
                "%VIDEO_ENCODING_HIGH%", video_enc_h);

    } else if ((service_ctx.profiles_num == 2) &&
            (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {

        sprintf(stmp_w_l, "%d", service_ctx.profiles[1].width);
        sprintf(stmp_h_l, "%d", service_ctx.profiles[1].height);
        set_video_codec(video_enc_l, 16, service_ctx.profiles[1].type, 2);
        long size = cat(NULL, "media2_service_files/GetVideoEncoderConfigurations_low.xml", 6,
                "%WIDTH_LOW%", stmp_w_l,
                "%HEIGHT_LOW%", stmp_h_l,
                "%VIDEO_ENCODING_LOW%", video_enc_l);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetVideoEncoderConfigurations_low.xml", 6,
                "%WIDTH_LOW%", stmp_w_l,
                "%HEIGHT_LOW%", stmp_h_l,
                "%VIDEO_ENCODING_LOW%", video_enc_l);
    } else {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
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

        sprintf(stmp_w, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h, "%d", service_ctx.profiles[0].height);
        set_video_codec(video_enc, 16, service_ctx.profiles[0].type, 2);
        long size = cat(NULL, "media2_service_files/GetVideoEncoderConfigurationOptions.xml", 8,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h,
                "%PROFILE%", "High",
                "%VIDEO_ENCODING%", video_enc);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetVideoEncoderConfigurationOptions.xml", 8,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h,
                "%PROFILE%", "High",
                "%VIDEO_ENCODING%", video_enc);

    } else if ((service_ctx.profiles_num == 2) &&
            (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {

        sprintf(stmp_w, "%d", service_ctx.profiles[1].width);
        sprintf(stmp_h, "%d", service_ctx.profiles[1].height);
        set_video_codec(video_enc, 16, service_ctx.profiles[1].type, 2);
        long size = cat(NULL, "media2_service_files/GetVideoEncoderConfigurationOptions.xml", 8,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h,
                "%PROFILE%", "Main",
                "%VIDEO_ENCODING%", video_enc);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetVideoEncoderConfigurationOptions.xml", 8,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h,
                "%PROFILE%", "Main",
                "%VIDEO_ENCODING%", video_enc);

    } else {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -1;
    }
}

int media2_get_audio_source_configurations()
{
    // Ignore ConfigurationToken and ProfileToken
    char profiles_num[2];

    sprintf(profiles_num, "%d", service_ctx.profiles_num);

    long size = cat(NULL, "media2_service_files/GetAudioSourceConfigurations.xml", 2,
            "%PROFILES_NUM%", profiles_num);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "media2_service_files/GetAudioSourceConfigurations.xml", 2,
            "%PROFILES_NUM%", profiles_num);
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
        strncpy(token, service_ctx.profiles[0].name, 9);
    }

    if ((strcasecmp(service_ctx.profiles[0].name, token) == 0) ||
            (strcasecmp(service_ctx.profiles[1].name, token) == 0) ||
            (strcasecmp("AudioSourceConfigToken", token) == 0)) {

        long size = cat(NULL, "media2_service_files/GetAudioSourceConfigurationOptions.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetAudioSourceConfigurationOptions.xml", 0);

    } else {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The requested configuration indicated does not exist");
        return -2;
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

    if (service_ctx.profiles_num == 0) {
        long size = cat(NULL, "media2_service_files/GetAudioEncoderConfigurations_none.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetAudioEncoderConfigurations_none.xml", 0);

    } else if (token[0] == '\0') {
        if (service_ctx.profiles_num == 1) {
            set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
            long size = cat(NULL, "media2_service_files/GetAudioEncoderConfigurations_high.xml", 2,
                    "%AUDIO_ENCODING_HIGH%", audio_enc_h);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "media2_service_files/GetAudioEncoderConfigurations_high.xml", 2,
                    "%AUDIO_ENCODING_HIGH%", audio_enc_h);

        } else if (service_ctx.profiles_num == 2) {
            set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
            set_audio_codec(audio_enc_l, 16, service_ctx.profiles[1].audio_encoder, 2);
            long size = cat(NULL, "media2_service_files/GetAudioEncoderConfigurations_both.xml", 4,
                    "%AUDIO_ENCODING_HIGH%", audio_enc_h,
                    "%AUDIO_ENCODING_LOW%", audio_enc_l);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "media2_service_files/GetAudioEncoderConfigurations_both.xml", 4,
                    "%AUDIO_ENCODING_HIGH%", audio_enc_h,
                    "%AUDIO_ENCODING_LOW%", audio_enc_l);
        }
    } else if ((service_ctx.profiles_num > 0) &&
            (strcasecmp(service_ctx.profiles[0].name, token) == 0)) {

        set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
        long size = cat(NULL, "media2_service_files/GetAudioEncoderConfigurations_high.xml", 2,
                "%AUDIO_ENCODING_HIGH%", audio_enc_h);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetAudioEncoderConfigurations_high.xml", 2,
                "%AUDIO_ENCODING_HIGH%", audio_enc_h);

    } else if ((service_ctx.profiles_num == 2) &&
            (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {

        set_audio_codec(audio_enc_l, 16, service_ctx.profiles[1].audio_encoder, 2);
        long size = cat(NULL, "media2_service_files/GetAudioEncoderConfigurations_low.xml", 2,
                "%AUDIO_ENCODING_LOW%", audio_enc_l);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetAudioEncoderConfigurations_low.xml", 2,
                "%AUDIO_ENCODING_LOW%", audio_enc_l);
    } else {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -1;
    }
}

int media2_get_audio_encoder_configuration_options()
{
    char audio_enc[16];
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[10];

    memset(token, '\0', sizeof(token));
    if (configuration_token != NULL) {
        // Extract "Profile_x" from token Profile_x_VideoEncoderConfig
        strncpy(token, configuration_token, 9);
    } else if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else {
        strncpy(token, service_ctx.profiles[0].name, 9);
    }

    if ((service_ctx.profiles_num > 0) &&
            (strcasecmp(service_ctx.profiles[0].name, token) == 0)) {

        set_audio_codec(audio_enc, 16, service_ctx.profiles[0].audio_encoder, 2);

    } else if ((service_ctx.profiles_num == 2) &&
            (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {

        set_audio_codec(audio_enc, 16, service_ctx.profiles[1].audio_encoder, 2);

    } else {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -1;
    }

    long size = cat(NULL, "media2_service_files/GetAudioEncoderConfigurationOptions.xml", 2,
            "%AUDIO_ENCODING%", audio_enc);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "media2_service_files/GetAudioEncoderConfigurationOptions.xml", 2,
            "%AUDIO_ENCODING%", audio_enc);
}

int media2_get_audio_output_configurations()
{
    // Ignore ConfigurationToken and ProfileToken
    char profiles_num[2];

    sprintf(profiles_num, "%d", service_ctx.profiles_num);

    long size = cat(NULL, "media2_service_files/GetAudioOutputConfigurations.xml", 2,
            "%PROFILES_NUM%", profiles_num);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "media2_service_files/GetAudioOutputConfigurations.xml", 2,
            "%PROFILES_NUM%", profiles_num);
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
        strncpy(token, service_ctx.profiles[0].name, 9);
    }

    if ((strcasecmp(service_ctx.profiles[0].name, token) == 0) ||
            (strcasecmp(service_ctx.profiles[1].name, token) == 0) ||
            (strcasecmp("AudioOutputConfigToken", token) == 0)) {

        long size = cat(NULL, "media2_service_files/GetAudioOutputConfigurationOptions.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetAudioOutputConfigurationOptions.xml", 0);
    } else {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The requested configuration indicated does not exist");
        return -1;
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

    if (service_ctx.profiles_num == 0) {
        long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations_none.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetAudioEncoderConfigurations_none.xml", 0);

    } else if (token[0] == '\0') {
        if ((service_ctx.profiles_num == 1) && (service_ctx.profiles[0].audio_decoder != AUDIO_NONE)) {

            long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations_high.xml", 0);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations_high.xml", 0);

        } else if ((service_ctx.profiles_num == 2)  && (service_ctx.profiles[0].audio_decoder == AUDIO_NONE)
                && (service_ctx.profiles[1].audio_decoder != AUDIO_NONE)) {

            long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations_low.xml", 0);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations_low.xml", 0);

        } else if ((service_ctx.profiles_num == 2)  && (service_ctx.profiles[0].audio_decoder != AUDIO_NONE)
                && (service_ctx.profiles[1].audio_decoder != AUDIO_NONE)) {

            long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations_both.xml", 0);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations_both.xml", 0);
        }
    } else if ((service_ctx.profiles_num > 0) && (strcasecmp(service_ctx.profiles[0].name, token) == 0)) {

        if (service_ctx.profiles[0].audio_decoder == AUDIO_NONE) {

            long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations_none.xml", 0);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations_none.xml", 0);
        } else {

            long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations_high.xml", 0);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations_high.xml", 0);
        }
    } else if ((service_ctx.profiles_num == 2) && (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {

        if (service_ctx.profiles[1].audio_decoder == AUDIO_NONE) {

            long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations_none.xml", 0);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations_none.xml", 0);
        } else {

            long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations_low.xml", 0);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations_low.xml", 0);
        }
    } else {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The requested configuration indicated does not exist");
        return -1;
    }
}

int media2_get_audio_decoder_configuration_options()
{
    int decoder_type = AUDIO_NONE;
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[10];
    char audio_decoder[16];

    memset(token, '\0', sizeof(token));
    if (configuration_token != NULL) {
        // Extract "Profile_x" from token Profile_x_VideoEncoderConfig
        strncpy(token, configuration_token, 9);
    } else if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else {
        strncpy(token, service_ctx.profiles[0].name, 9);
    }

    if ((service_ctx.profiles_num > 0) && (strcasecmp(service_ctx.profiles[0].name, token) == 0)) {
        decoder_type = service_ctx.profiles[0].audio_decoder;
    } else if ((service_ctx.profiles_num == 2) && (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {
        decoder_type = service_ctx.profiles[1].audio_decoder;
    }

    if (decoder_type != AUDIO_NONE) {

        set_audio_codec(audio_decoder, 16, decoder_type, 2);

        long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurationOptions.xml", 2,
                "%AUDIO_DECODING%", audio_decoder);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetAudioDecoderConfigurationOptions.xml", 2,
                "%AUDIO_DECODING%", audio_decoder);

    } else {

        long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurationOptions_none.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetAudioDecoderConfigurationOptions_none.xml", 0);
    }
}

int media2_get_snapshot_uri()
{
    char address[16];
    char netmask[16];
    char *s;
    char line[MAX_LEN];
    const char *profile_token = get_element("ProfileToken", "Body");

    memset(line, '\0', sizeof(line));

    get_ip_address(address, netmask, service_ctx.ifs);

    if (profile_token == NULL) {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -1;
    }

    if ((service_ctx.profiles_num > 0) &&
            (strcasecmp(service_ctx.profiles[0].name, profile_token) == 0)) {

        if (service_ctx.profiles[0].snapurl == NULL) {
            send_fault("media_service", "Receiver", "ter:Action", "ter:IncompleteConfiguration", "Incomplete configuration", "The specified media profile does not contain either a reference to a video encoder configuration or a reference to a video source configuration");
            return -2;
        }

        if (sprintf(line, service_ctx.profiles[0].snapurl, address) < 0) {
            strcpy(line, service_ctx.profiles[0].snapurl);
        }
        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media2_service_files/GetSnapshotUri.xml", 2,
                    "%URI%", line);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetSnapshotUri.xml", 2,
                    "%URI%", line);

    } else if ((service_ctx.profiles_num == 2) &&
            (strcasecmp(service_ctx.profiles[1].name, profile_token) == 0)) {

        if (service_ctx.profiles[1].snapurl == NULL) {
            send_fault("media_service", "Receiver", "ter:Action", "ter:IncompleteConfiguration", "Incomplete configuration", "The specified media profile does not contain either a reference to a video encoder configuration or a reference to a video source configuration");
            return -2;
        }

        if (sprintf(line, service_ctx.profiles[1].snapurl, address) < 0) {
            strcpy(line, service_ctx.profiles[1].snapurl);
        }
        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media2_service_files/GetSnapshotUri.xml", 2,
                    "%URI%", line);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetSnapshotUri.xml", 2,
                    "%URI%", line);

    } else {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -3;
    }
}

int media2_get_stream_uri()
{
    char address[16];
    char netmask[16];
    char *s;
    char line[MAX_LEN];
    const char *profile_token = get_element("ProfileToken", "Body");

    memset(line, '\0', sizeof(line));

    get_ip_address(address, netmask, service_ctx.ifs);

    if (profile_token == NULL) {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -1;
    }

    if (strcasecmp(service_ctx.profiles[0].name, profile_token) == 0) {

        if (service_ctx.profiles[0].url == NULL) {
            send_fault("media_service", "Receiver", "ter:Action", "ter:IncompleteConfiguration", "Incomplete configuration", "The specified media profile does contain either unused sources or encoder configurations without a corresponding source");
            return -2;
        }

        if (sprintf(line, service_ctx.profiles[0].url, address) < 0) {
            strcpy(line, service_ctx.profiles[0].url);
        }
        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media2_service_files/GetStreamUri.xml", 2,
                    "%URI%", line);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetStreamUri.xml", 2,
                    "%URI%", line);

    } else if ((service_ctx.profiles_num == 2) &&
            (strcasecmp(service_ctx.profiles[1].name, profile_token) == 0)) {

        if (service_ctx.profiles[1].url == NULL) {
            send_fault("media_service", "Receiver", "ter:Action", "ter:IncompleteConfiguration", "Incomplete configuration", "The specified media profile does contain either unused sources or encoder configurations without a corresponding source");
            return -2;
        }

        if (sprintf(line, service_ctx.profiles[1].url, address) < 0) {
            strcpy(line, service_ctx.profiles[1].url);
        }
        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media2_service_files/GetStreamUri.xml", 2,
                    "%URI%", line);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media2_service_files/GetStreamUri.xml", 2,
                    "%URI%", line);

    } else {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -3;
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
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -1;
    }

    if (strcasecmp("VideoSourceConfigToken", token) == 0) {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:ConfigModify", "Config modify", "The configuration parameters are not possible to set");
        return -2;
    } else {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -1;
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
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -1;
    }

    if (strcasecmp("AudioSourceConfigToken", token) == 0) {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:ConfigModify", "Config modify", "The configuration parameters are not possible to set");
        return -2;
    } else {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -1;
    }
}

int media2_set_video_encoder_configuration()
{
    send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:ConfigModify", "Config modify", "The configuration parameters are not possible to set");
    return -1;
}

int media2_set_audio_encoder_configuration()
{
    send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:ConfigModify", "Config modify", "The configuration parameters are not possible to set");
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
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -1;
    }

    if (strcasecmp("AudioOutputConfigToken", token) == 0) {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:ConfigModify", "Config modify", "The configuration parameters are not possible to set");
        return -2;
    } else {
        send_fault("media_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -1;
    }
}

int media2_unsupported(const char *method)
{
    if (service_ctx.adv_fault_if_unknown == 1)
        send_action_failed_fault("media2_service", -1);
    else
        send_empty_response("tr2", (char *) method);
    return -1;
}
