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
#include <string.h>

#include "utils.h"
#include "onvif_simple_server.h"

extern service_context_t service_ctx;

int media_get_service_capabilities()
{
    if (service_ctx.profiles_num > 0) {
        long size = cat(NULL, "media_service_files/GetServiceCapabilities.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetServiceCapabilities.xml", 0);
    } else {
        long size = cat(NULL, "generic_files/Error.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "generic_files/Error.xml", 0);
    }
}

int media_get_video_sources()
{
    // Get the video source from the 1st profile
    char stmp_w[16], stmp_h[16];

    if (service_ctx.profiles_num > 0) {
        sprintf(stmp_w, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h, "%d", service_ctx.profiles[0].height);
        long size = cat(NULL, "media_service_files/GetVideoSources.xml", 4,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetVideoSources.xml", 4,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);
    } else {
        long size = cat(NULL, "generic_files/Error.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "generic_files/Error.xml", 0);
    }
}

int media_get_video_source_configurations()
{
    // Get the video source configuration from the 1st profile
    char stmp_w[16], stmp_h[16];

    if (service_ctx.profiles_num > 0) {
        sprintf(stmp_w, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h, "%d", service_ctx.profiles[0].height);
        long size = cat(NULL, "media_service_files/GetVideoSourceConfigurations.xml", 4,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetVideoSourceConfigurations.xml", 4,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);
    } else {
        long size = cat(NULL, "generic_files/Error.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "generic_files/Error.xml", 0);
    }
}

int media_get_video_source_configuration()
{
    // Get the video source configuration from the 1st profile
    char stmp_w[16], stmp_h[16];

    if (service_ctx.profiles_num > 0) {
        sprintf(stmp_w, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h, "%d", service_ctx.profiles[0].height);
        long size = cat(NULL, "media_service_files/GetVideoSourceConfiguration.xml", 4,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetVideoSourceConfiguration.xml", 4,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);
    } else {
        long size = cat(NULL, "generic_files/Error.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "generic_files/Error.xml", 0);
    }
}

int media_get_compatible_video_source_configurations()
{
    // Get the video source configuration from the 1st profile
    char stmp_w[16], stmp_h[16];

    if (service_ctx.profiles_num > 0) {
        sprintf(stmp_w, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h, "%d", service_ctx.profiles[0].height);
        long size = cat(NULL, "media_service_files/GetCompatibleVideoSourceConfigurations.xml", 4,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetCompatibleVideoSourceConfigurations.xml", 4,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);
    } else {
        long size = cat(NULL, "generic_files/Error.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "generic_files/Error.xml", 0);
    }
}

int media_get_video_source_configuration_options()
{
    // Get the video source configuration from the 1st profile
    char stmp_w[16], stmp_h[16];

    if (service_ctx.profiles_num > 0) {
        sprintf(stmp_w, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h, "%d", service_ctx.profiles[0].height);
        long size = cat(NULL, "media_service_files/GetVideoSourceConfigurationOptions.xml", 4,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetVideoSourceConfigurationOptions.xml", 4,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);
    } else {
        long size = cat(NULL, "generic_files/Error.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "generic_files/Error.xml", 0);
    }
}

int media_get_profiles()
{
    char stmp_w_l[16], stmp_h_l[16];
    char stmp_w_h[16], stmp_h_h[16];

    if (service_ctx.profiles_num == 1) {
        if (strcasecmp(service_ctx.profiles[0].name, "Profile_0") == 0) {
            sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
            sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
            long size = cat(NULL, "media_service_files/GetProfiles_high.xml", 8,
                    "%VSC_WIDTH%", stmp_w_h,
                    "%VSC_HEIGHT%", stmp_h_h,
                    "%VEC_WIDTH_HIGH%", stmp_w_h,
                    "%VEC_HEIGHT_HIGH%", stmp_h_h);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "media_service_files/GetProfiles_high.xml", 8,
                    "%VSC_WIDTH%", stmp_w_h,
                    "%VSC_HEIGHT%", stmp_h_h,
                    "%VEC_WIDTH_HIGH%", stmp_w_h,
                    "%VEC_HEIGHT_HIGH%", stmp_h_h);

        } else if (strcasecmp(service_ctx.profiles[0].name, "Profile_1") == 0) {
            sprintf(stmp_w_l, "%d", service_ctx.profiles[0].width);
            sprintf(stmp_h_l, "%d", service_ctx.profiles[0].height);
            long size = cat(NULL, "media_service_files/GetProfiles_low.xml", 8,
                    "%VSC_WIDTH%", stmp_w_l,
                    "%VSC_HEIGHT%", stmp_h_l,
                    "%VEC_WIDTH_LOW%", stmp_w_l,
                    "%VEC_HEIGHT_LOW%", stmp_h_l);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "media_service_files/GetProfiles_low.xml", 8,
                    "%VSC_WIDTH%", stmp_w_l,
                    "%VSC_HEIGHT%", stmp_h_l,
                    "%VEC_WIDTH_LOW%", stmp_w_l,
                    "%VEC_HEIGHT_LOW%", stmp_h_l);
        }
    } else if (service_ctx.profiles_num == 2) {
        sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
        sprintf(stmp_w_l, "%d", service_ctx.profiles[1].width);
        sprintf(stmp_h_l, "%d", service_ctx.profiles[1].height);
        long size = cat(NULL, "media_service_files/GetProfiles_both.xml", 12,
                    "%VSC_WIDTH%", stmp_w_h,
                    "%VSC_HEIGHT%", stmp_h_h,
                    "%VEC_WIDTH_HIGH%", stmp_w_h,
                    "%VEC_HEIGHT_HIGH%", stmp_h_h,
                    "%VEC_WIDTH_LOW%", stmp_w_l,
                    "%VEC_HEIGHT_LOW%", stmp_h_l);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetProfiles_both.xml", 12,
                    "%VSC_WIDTH%", stmp_w_h,
                    "%VSC_HEIGHT%", stmp_h_h,
                    "%VEC_WIDTH_HIGH%", stmp_w_h,
                    "%VEC_HEIGHT_HIGH%", stmp_h_h,
                    "%VEC_WIDTH_LOW%", stmp_w_l,
                    "%VEC_HEIGHT_LOW%", stmp_h_l);
    } else {
        long size = cat(NULL, "generic_files/Error.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "generic_files/Error.xml", 0);
    }
}

int media_get_profile(char *input)
{
    char stmp_vsc_w[16], stmp_vsc_h[16];
    char stmp_w[16], stmp_h[16];

    if (((service_ctx.profiles_num == 1) &&
            (strcasecmp(service_ctx.profiles[0].name, "Profile_0") == 0) &&
            (find_element(input, "ProfileToken", "Profile_0") == 0)) ||
            ((service_ctx.profiles_num == 2) &&
            (find_element(input, "ProfileToken", "Profile_0") == 0))) {

        sprintf(stmp_vsc_w, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_vsc_h, "%d", service_ctx.profiles[0].height);
        sprintf(stmp_w, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h, "%d", service_ctx.profiles[0].height);
        long size = cat(NULL, "media_service_files/GetProfile_high.xml", 8,
                "%VSC_WIDTH%", stmp_vsc_w,
                "%VSC_HEIGHT%", stmp_vsc_h,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetProfile_high.xml", 8,
                "%VSC_WIDTH%", stmp_vsc_w,
                "%VSC_HEIGHT%", stmp_vsc_h,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);

    } else if ((service_ctx.profiles_num == 1) &&
            (strcasecmp(service_ctx.profiles[0].name, "Profile_1") == 0) &&
            (find_element(input, "ProfileToken", "Profile_1") == 0)) {

        sprintf(stmp_vsc_w, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_vsc_h, "%d", service_ctx.profiles[0].height);
        sprintf(stmp_w, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h, "%d", service_ctx.profiles[0].height);
        long size = cat(NULL, "media_service_files/GetProfile_low.xml", 8,
                "%VSC_WIDTH%", stmp_vsc_w,
                "%VSC_HEIGHT%", stmp_vsc_h,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetProfile_low.xml", 8,
                "%VSC_WIDTH%", stmp_vsc_w,
                "%VSC_HEIGHT%", stmp_vsc_h,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);

    } else if ((service_ctx.profiles_num == 2) &&
            (find_element(input, "ProfileToken", "Profile_1") == 0)) {

        sprintf(stmp_vsc_w, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_vsc_h, "%d", service_ctx.profiles[0].height);
        sprintf(stmp_w, "%d", service_ctx.profiles[1].width);
        sprintf(stmp_h, "%d", service_ctx.profiles[1].height);
        long size = cat(NULL, "media_service_files/GetProfile_low.xml", 8,
                "%VSC_WIDTH%", stmp_vsc_w,
                "%VSC_HEIGHT%", stmp_vsc_h,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetProfile_low.xml", 8,
                "%VSC_WIDTH%", stmp_vsc_w,
                "%VSC_HEIGHT%", stmp_vsc_h,
                "%WIDTH%", stmp_w,
                "%HEIGHT%", stmp_h);

    } else {
        long size = cat(NULL, "generic_files/Error.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "generic_files/Error.xml", 0);
    }
}

int media_get_video_encoder_configurations()
{
    char stmp_w_l[16], stmp_h_l[16];
    char stmp_w_h[16], stmp_h_h[16];

    if (service_ctx.profiles_num == 1) {
        if (strcasecmp(service_ctx.profiles[0].name, "Profile_0") == 0) {
            sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
            sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
            long size = cat(NULL, "media_service_files/GetVideoEncoderConfigurations_high.xml", 4,
                    "%WIDTH_HIGH%", stmp_w_h,
                    "%HEIGHT_HIGH%", stmp_h_h);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "media_service_files/GetVideoEncoderConfigurations_high.xml", 4,
                    "%WIDTH_HIGH%", stmp_w_h,
                    "%HEIGHT_HIGH%", stmp_h_h);

        } else if (strcasecmp(service_ctx.profiles[0].name, "Profile_1") == 0) {
            sprintf(stmp_w_l, "%d", service_ctx.profiles[0].width);
            sprintf(stmp_h_l, "%d", service_ctx.profiles[0].height);
            long size = cat(NULL, "media_service_files/GetVideoEncoderConfigurations_low.xml", 4,
                    "%WIDTH_LOW%", stmp_w_l,
                    "%HEIGHT_LOW%", stmp_h_l);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "media_service_files/GetVideoEncoderConfigurations_low.xml", 4,
                    "%WIDTH_LOW%", stmp_w_l,
                    "%HEIGHT_LOW%", stmp_h_l);
        }
    } else if (service_ctx.profiles_num == 2) {
        sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
        sprintf(stmp_w_l, "%d", service_ctx.profiles[1].width);
        sprintf(stmp_h_l, "%d", service_ctx.profiles[1].height);
        long size = cat(NULL, "media_service_files/GetVideoEncoderConfigurations_both.xml", 8,
                    "%WIDTH_HIGH%", stmp_w_h,
                    "%HEIGHT_HIGH%", stmp_h_h,
                    "%WIDTH_LOW%", stmp_w_l,
                    "%HEIGHT_LOW%", stmp_h_l);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetVideoEncoderConfigurations_both.xml", 8,
                    "%WIDTH_HIGH%", stmp_w_h,
                    "%HEIGHT_HIGH%", stmp_h_h,
                    "%WIDTH_LOW%", stmp_w_l,
                    "%HEIGHT_LOW%", stmp_h_l);
    } else {
        long size = cat(NULL, "generic_files/Error.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "generic_files/Error.xml", 0);
    }
}

int media_get_video_encoder_configuration(char *input)
{
    char stmp_w_l[16], stmp_h_l[16];
    char stmp_w_h[16], stmp_h_h[16];

    if (((service_ctx.profiles_num == 1) &&
            (strcasecmp(service_ctx.profiles[0].name, "Profile_0") == 0) &&
            (find_element(input, "ConfigurationToken", "Profile_0") == 0)) ||
            ((service_ctx.profiles_num == 2) &&
            (find_element(input, "ConfigurationToken", "Profile_0") == 0))) {

        sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
        long size = cat(NULL, "media_service_files/GetVideoEncoderConfiguration_high.xml", 4,
                "%WIDTH_HIGH%", stmp_w_h,
                "%HEIGHT_HIGH%", stmp_h_h);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetVideoEncoderConfiguration_high.xml", 4,
                "%WIDTH_HIGH%", stmp_w_h,
                "%HEIGHT_HIGH%", stmp_h_h);

    } else if ((service_ctx.profiles_num == 1) &&
            (strcasecmp(service_ctx.profiles[0].name, "Profile_1") == 0) &&
            (find_element(input, "ConfigurationToken", "Profile_1") == 0)) {

        sprintf(stmp_w_l, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h_l, "%d", service_ctx.profiles[0].height);
        long size = cat(NULL, "media_service_files/GetVideoEncoderConfiguration_low.xml", 4,
                "%WIDTH_LOW%", stmp_w_l,
                "%HEIGHT_LOW%", stmp_h_l);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetVideoEncoderConfiguration_low.xml", 4,
                "%WIDTH_LOW%", stmp_w_l,
                "%HEIGHT_LOW%", stmp_h_l);

    } else if ((service_ctx.profiles_num == 2) &&
            (find_element(input, "ConfigurationToken", "Profile_1") == 0)) {

        sprintf(stmp_w_l, "%d", service_ctx.profiles[1].width);
        sprintf(stmp_h_l, "%d", service_ctx.profiles[1].height);
        long size = cat(NULL, "media_service_files/GetVideoEncoderConfiguration_low.xml", 4,
                    "%WIDTH_LOW%", stmp_w_l,
                    "%HEIGHT_LOW%", stmp_h_l);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetVideoEncoderConfiguration_low.xml", 4,
                    "%WIDTH_LOW%", stmp_w_l,
                    "%HEIGHT_LOW%", stmp_h_l);
    } else {
        long size = cat(NULL, "generic_files/Error.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "generic_files/Error.xml", 0);
    }
}

int media_get_compatible_video_encoder_configurations(char *input)
{
    char stmp_w_l[16], stmp_h_l[16];
    char stmp_w_h[16], stmp_h_h[16];

    if (((service_ctx.profiles_num == 1) &&
            (strcasecmp(service_ctx.profiles[0].name, "Profile_0") == 0) &&
            (find_element(input, "ProfileToken", "Profile_0") == 0)) ||
            ((service_ctx.profiles_num == 2) &&
            (find_element(input, "ProfileToken", "Profile_0") == 0))) {

        sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
        long size = cat(NULL, "media_service_files/GetCompatibleVideoEncoderConfigurations_high.xml", 4,
                "%WIDTH_HIGH%", stmp_w_h,
                "%HEIGHT_HIGH%", stmp_h_h);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetCompatibleVideoEncoderConfigurations_high.xml", 4,
                "%WIDTH_HIGH%", stmp_w_h,
                "%HEIGHT_HIGH%", stmp_h_h);

    } else if ((service_ctx.profiles_num == 1) &&
        (strcasecmp(service_ctx.profiles[0].name, "Profile_1") == 0) &&
        (find_element(input, "ProfileToken", "Profile_1") == 0)) {

        sprintf(stmp_w_l, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h_l, "%d", service_ctx.profiles[0].height);
        long size = cat(NULL, "media_service_files/GetCompatibleVideoEncoderConfigurations_low.xml", 4,
                "%WIDTH_LOW%", stmp_w_l,
                "%HEIGHT_LOW%", stmp_h_l);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetCompatibleVideoEncoderConfigurations_low.xml", 4,
                "%WIDTH_LOW%", stmp_w_l,
                "%HEIGHT_LOW%", stmp_h_l);

    } else if ((service_ctx.profiles_num == 2) &&
            (find_element(input, "ProfileToken", "Profile_1") == 0)) {

        sprintf(stmp_w_l, "%d", service_ctx.profiles[1].width);
        sprintf(stmp_h_l, "%d", service_ctx.profiles[1].height);
        long size = cat(NULL, "media_service_files/GetCompatibleVideoEncoderConfigurations_low.xml", 4,
                "%WIDTH_LOW%", stmp_w_l,
                "%HEIGHT_LOW%", stmp_h_l);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetCompatibleVideoEncoderConfigurations_low.xml", 4,
                "%WIDTH_LOW%", stmp_w_l,
                "%HEIGHT_LOW%", stmp_h_l);

    } else {
        long size = cat(NULL, "generic_files/Error.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "generic_files/Error.xml", 0);
    }
}

int media_get_video_encoder_configuration_options(char *input)
{
    char stmp_w_l[16], stmp_h_l[16];
    char stmp_w_h[16], stmp_h_h[16];

    if (((service_ctx.profiles_num == 1) &&
            (strcasecmp(service_ctx.profiles[0].name, "Profile_0") == 0) &&
            (find_element(input, "ConfigurationToken", "Profile_0") == 0)) ||
            ((service_ctx.profiles_num == 2) &&
            (find_element(input, "ConfigurationToken", "Profile_0") == 0))) {

        sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
        long size = cat(NULL, "media_service_files/GetVideoEncoderConfigurationOptions.xml", 6,
                "%WIDTH%", stmp_w_h,
                "%HEIGHT%", stmp_h_h,
                "%PROFILE%", "High");

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetVideoEncoderConfigurationOptions.xml", 6,
                "%WIDTH%", stmp_w_h,
                "%HEIGHT%", stmp_h_h,
                "%PROFILE%", "High");

    } else if ((service_ctx.profiles_num == 1) &&
        (strcasecmp(service_ctx.profiles[0].name, "Profile_1") == 0) &&
        (find_element(input, "ConfigurationToken", "Profile_1") == 0)) {

        sprintf(stmp_w_l, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h_l, "%d", service_ctx.profiles[0].height);
        long size = cat(NULL, "media_service_files/GetVideoEncoderConfigurationOptions.xml", 6,
                "%WIDTH%", stmp_w_l,
                "%HEIGHT%", stmp_h_l,
                "%PROFILE%", "Main");

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetVideoEncoderConfigurationOptions.xml", 6,
                "%WIDTH%", stmp_w_l,
                "%HEIGHT%", stmp_h_l,
                "%PROFILE%", "Main");

    } else if ((service_ctx.profiles_num == 2) &&
            (find_element(input, "ConfigurationToken", "Profile_1") == 0)) {

        sprintf(stmp_w_l, "%d", service_ctx.profiles[1].width);
        sprintf(stmp_h_l, "%d", service_ctx.profiles[1].height);
        long size = cat(NULL, "media_service_files/GetVideoEncoderConfigurationOptions.xml", 6,
                "%WIDTH%", stmp_w_l,
                "%HEIGHT%", stmp_h_l,
                "%PROFILE%", "Main");

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetVideoEncoderConfigurationOptions.xml", 6,
                "%WIDTH%", stmp_w_l,
                "%HEIGHT%", stmp_h_l,
                "%PROFILE%", "Main");

    } else {
        long size = cat(NULL, "generic_files/Error.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "generic_files/Error.xml", 0);
    }

}

int media_get_snapshot_uri(char *input)
{
    char address[16];
    char netmask[16];
    char *s;
    char line[MAX_LEN];

    memset(line, '\0', sizeof(line));

    get_ip_address(address, netmask, service_ctx.ifs);

    if (((service_ctx.profiles_num == 1) &&
            (strcasecmp(service_ctx.profiles[0].name, "Profile_0") == 0) &&
            (find_element(input, "ProfileToken", "Profile_0") == 0)) ||
            ((service_ctx.profiles_num == 2) &&
            (find_element(input, "ProfileToken", "Profile_0") == 0))) {

        if (str_subst(line, service_ctx.profiles[0].snapurl, "%s", address) < 0) {
            strcpy(line, service_ctx.profiles[0].snapurl);
        }
        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media_service_files/GetSnapshotUri.xml", 2,
                    "%URI%", line);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetSnapshotUri.xml", 2,
                    "%URI%", line);

    } else if ((service_ctx.profiles_num == 1) &&
            (strcasecmp(service_ctx.profiles[0].name, "Profile_1") == 0) &&
            (find_element(input, "ProfileToken", "Profile_1") == 0)) {

        if (str_subst(line, service_ctx.profiles[0].snapurl, "%s", address) < 0) {
            strcpy(line, service_ctx.profiles[0].snapurl);
        }
        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media_service_files/GetSnapshotUri.xml", 2,
                    "%URI%", line);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetSnapshotUri.xml", 2,
                    "%URI%", line);

    } else if ((service_ctx.profiles_num == 2) &&
            (find_element(input, "ProfileToken", "Profile_1") == 0)) {

        if (str_subst(line, service_ctx.profiles[1].snapurl, "%s", address) < 0) {
            strcpy(line, service_ctx.profiles[1].snapurl);
        }
        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media_service_files/GetSnapshotUri.xml", 2,
                    "%URI%", line);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetSnapshotUri.xml", 2,
                    "%URI%", line);

    } else {
        long size = cat(NULL, "generic_files/Error.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "generic_files/Error.xml", 0);
    }
}

int media_get_stream_uri(char *input)
{
    char address[16];
    char netmask[16];
    char *s;
    char line[MAX_LEN];

    memset(line, '\0', sizeof(line));

    get_ip_address(address, netmask, service_ctx.ifs);

    if (((service_ctx.profiles_num == 1) &&
            (strcasecmp(service_ctx.profiles[0].name, "Profile_0") == 0) &&
            (find_element(input, "ProfileToken", "Profile_0") == 0)) ||
            ((service_ctx.profiles_num == 2) &&
            (find_element(input, "ProfileToken", "Profile_0") == 0))) {

        if (str_subst(line, service_ctx.profiles[0].url, "%s", address) < 0) {
            strcpy(line, service_ctx.profiles[0].url);
        }
        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media_service_files/GetStreamUri.xml", 2,
                    "%URI%", line);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetStreamUri.xml", 2,
                    "%URI%", line);

    } else if ((service_ctx.profiles_num == 1) &&
            (strcasecmp(service_ctx.profiles[0].name, "Profile_1") == 0) &&
            (find_element(input, "ProfileToken", "Profile_1") == 0)) {

        if (str_subst(line, service_ctx.profiles[0].url, "%s", address) < 0) {
            strcpy(line, service_ctx.profiles[0].url);
        }
        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media_service_files/GetStreamUri.xml", 2,
                    "%URI%", line);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetStreamUri.xml", 2,
                    "%URI%", line);

    } else if ((service_ctx.profiles_num == 2) &&
            (find_element(input, "ProfileToken", "Profile_1") == 0)) {

        if (str_subst(line, service_ctx.profiles[1].url, "%s", address) < 0) {
            strcpy(line, service_ctx.profiles[1].url);
        }
        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media_service_files/GetStreamUri.xml", 2,
                    "%URI%", line);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "media_service_files/GetStreamUri.xml", 2,
                    "%URI%", line);

    } else {
        long size = cat(NULL, "generic_files/Error.xml", 0);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "generic_files/Error.xml", 0);
    }
}

int media_unsupported(char *action)
{
    char response[MAX_LEN];
    sprintf(response, "%sResponse", action);

    long size = cat(NULL, "media_service_files/Unsupported.xml", 2,
            "%UNSUPPORTED%", response);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "media_service_files/Unsupported.xml", 2,
            "%UNSUPPORTED%", response);
}
