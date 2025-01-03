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
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "deviceio_service.h"
#include "fault.h"
#include "utils.h"
#include "log.h"
#include "ezxml_wrapper.h"
#include "onvif_simple_server.h"

extern service_context_t service_ctx;

int deviceio_get_video_sources()
{
    long size = cat(NULL, "deviceio_service_files/GetVideoSources.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "deviceio_service_files/GetVideoSources.xml", 0);
}

int deviceio_get_service_capabilities()
{
    char audio_sources[2], audio_outputs[2];

    if ((service_ctx.profiles[0].audio_encoder != AUDIO_NONE) ||
            ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE))) {

        sprintf(audio_sources, "%d", 1);
    } else {
        sprintf(audio_sources, "%d", 0);
    }
    if ((service_ctx.profiles[0].audio_decoder != AUDIO_NONE) ||
            ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_decoder != AUDIO_NONE))) {

        sprintf(audio_outputs, "%d", 1);
    } else {
        sprintf(audio_outputs, "%d", 0);
    }

    long size = cat(NULL, "deviceio_service_files/GetServiceCapabilities.xml", 4,
            "%AUDIO_SOURCES%", audio_sources,
            "%AUDIO_OUTPUTS%", audio_outputs);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "deviceio_service_files/GetServiceCapabilities.xml", 4,
            "%AUDIO_SOURCES%", audio_sources,
            "%AUDIO_OUTPUTS%", audio_outputs);
}

int deviceio_get_audio_outputs()
{
    char audio_output_token[MAX_LEN];

    if ((service_ctx.profiles[0].audio_decoder != AUDIO_NONE) ||
            ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_decoder != AUDIO_NONE))) {

        sprintf(audio_output_token, "%s", "<tmd:Token>AudioOutputToken</tmd:Token>");
    } else {
        sprintf(audio_output_token, "%s", "");
    }

    long size = cat(NULL, "deviceio_service_files/GetAudioOutputs.xml", 2,
            "%AUDIO_OUTPUT_TOKEN%", audio_output_token);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "deviceio_service_files/GetAudioOutputs.xml", 2,
            "%AUDIO_OUTPUT_TOKEN%", audio_output_token);
}

int deviceio_get_audio_sources()
{
    char audio_source_token[MAX_LEN];

    if ((service_ctx.profiles[0].audio_encoder != AUDIO_NONE) ||
            ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE))) {

        sprintf(audio_source_token, "%s", "<tmd:Token>AudioSourceToken</tmd:Token>");
    } else {
        sprintf(audio_source_token, "%s", "");
    }

    long size = cat(NULL, "deviceio_service_files/GetAudioSources.xml", 2,
            "%AUDIO_SOURCE_TOKEN%", audio_source_token);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "deviceio_service_files/GetAudioSources.xml", 2,
            "%AUDIO_SOURCE_TOKEN%", audio_source_token);
}

int deviceio_get_relay_outputs()
{
    send_empty_response("tds", "GetRelayOutputs");
}

int deviceio_unsupported(const char *method)
{
    if (service_ctx.adv_fault_if_unknown == 1)
        send_action_failed_fault("deviceio_service", -1);
    else
        send_empty_response("tmd", (char *) method);
    return -1;
}
