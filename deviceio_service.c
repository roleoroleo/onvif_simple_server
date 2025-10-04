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

    output_http_headers(size);

    return cat("stdout", "deviceio_service_files/GetVideoSources.xml", 0);
}

int deviceio_get_service_capabilities()
{
    char relay_outputs[2], audio_sources[2], audio_outputs[2];

    sprintf(relay_outputs, "%d", service_ctx.relay_outputs_num);
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

    long size = cat(NULL, "deviceio_service_files/GetServiceCapabilities.xml", 6,
            "%RELAY_OUTPUTS%", relay_outputs,
            "%AUDIO_SOURCES%", audio_sources,
            "%AUDIO_OUTPUTS%", audio_outputs);

    output_http_headers(size);

    return cat("stdout", "deviceio_service_files/GetServiceCapabilities.xml", 6,
            "%RELAY_OUTPUTS%", relay_outputs,
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

    output_http_headers(size);

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

    output_http_headers(size);

    return cat("stdout", "deviceio_service_files/GetAudioSources.xml", 2,
            "%AUDIO_SOURCE_TOKEN%", audio_source_token);
}

int deviceio_get_relay_outputs()
{
    long size;
    int c, i;
    char dest_a[] = "stdout";
    char *dest;
    char token[32];
    char idle_state[8];

    // We need 1st step to evaluate content length
    for (c = 0; c < 2; c++) {
        if (c == 0) {
            dest = NULL;
        } else {
            dest = dest_a;
            output_http_headers(size);
        }
        size = cat(dest, "deviceio_service_files/GetRelayOutputs_header.xml", 0);

        for (i = 0; i < service_ctx.relay_outputs_num; i++) {
            sprintf(token, "RelayOutputToken_%d", i);
            if (service_ctx.relay_outputs[i].idle_state == IDLE_STATE_OPEN)
                strcpy(idle_state, "open");
            else
                strcpy(idle_state, "close");
            size += cat(dest, "deviceio_service_files/GetRelayOutputs_item.xml", 4,
                    "%RELAY_OUTPUT_TOKEN%", token,
                    "%RELAY_IDLE_STATE%", idle_state);
        }
        size += cat(dest, "deviceio_service_files/GetRelayOutputs_footer.xml", 0);
    }

    return size;
}

int deviceio_get_relay_output_options()
{
    long size;
    int c, i;
    char dest_a[] = "stdout";
    char *dest;
    int itoken;
    char stoken[32];
    char idle_state[32];
    const char *token = get_element("RelayOutputToken", "Body");

    // We need 1st step to evaluate content length
    for (c = 0; c < 2; c++) {
        if (c == 0) {
            dest = NULL;
        } else {
            dest = dest_a;
            output_http_headers(size);
        }

        size = cat(dest, "deviceio_service_files/GetRelayOutputOptions_header.xml", 0);

        if (token == NULL) {
            for (i = 0; i < service_ctx.relay_outputs_num; i++) {
                sprintf(stoken, "RelayOutputToken_%d", i);
                if (service_ctx.relay_outputs[i].idle_state == IDLE_STATE_OPEN) {
                    strcpy(idle_state, "open");
                } else {
                    strcpy(idle_state, "close");
                }
                size += cat(dest, "deviceio_service_files/GetRelayOutputOptions_item.xml", 2,
                        "%RELAY_OUTPUT_TOKEN%", stoken);
            }
        } else if ((strlen(token) == 18) && (strncasecmp("RelayOutputToken_", token, 17) == 0)) {
            itoken = token[17] - 48;

            if ((itoken >= 0) && (itoken < service_ctx.relay_outputs_num)) {
                sprintf(stoken, "RelayOutputToken_%d", itoken);

                if (service_ctx.relay_outputs[itoken].idle_state == IDLE_STATE_OPEN) {
                    strcpy(idle_state, "open");
                } else {
                    strcpy(idle_state, "close");
                }
                size += cat(dest, "deviceio_service_files/GetRelayOutputOptions_item.xml", 2,
                        "%RELAY_OUTPUT_TOKEN%", stoken);
            }
        }
        size += cat(dest, "deviceio_service_files/GetRelayOutputOptions_footer.xml", 0);
    }
}

int deviceio_set_relay_output_settings()
{
    int itoken;
    ezxml_t node;
    const char *token = NULL;

    node = get_element_ptr(node, "RelayOutput", "Body");
    if (node != NULL) {
        token = get_attribute(node, "token");
    }

    if ((token != NULL) && (strlen(token) == 18) && (strncasecmp("RelayOutputToken_", token, 17) == 0)) {
        itoken = token[17] - 48;

        if ((itoken >= 0) && (itoken < service_ctx.relay_outputs_num)) {
            long size = cat(NULL, "deviceio_service_files/SetRelayOutputSettings.xml", 0);

            output_http_headers(size);

            return cat("stdout", "deviceio_service_files/SetRelayOutputSettings.xml", 0);
        } else {
            send_fault("deviceio_service", "Sender", "ter:InvalidArgVal", "ter:RelayToken", "Relay token",  "Unknown relay token reference");
            return -1;
        }
    } else {
        send_fault("deviceio_service", "Sender", "ter:InvalidArgVal", "ter:RelayToken", "Relay token", "Unknown relay token reference");
        return -2;
    }
}

int deviceio_set_relay_output_state()
{
    int itoken;
    const char *token = get_element("RelayOutputToken", "Body");
    const char *state = get_element("LogicalState", "Body");
    char sys_command[MAX_LEN];

    sys_command[0] = '\0';

    if ((token != NULL) && (strlen(token) == 18) && (strncasecmp("RelayOutputToken_", token, 17) == 0)) {
        itoken = token[17] - 48;

        if ((itoken >= 0) && (itoken < service_ctx.relay_outputs_num)) {
            if (strcasecmp("active", state) == 0) {
                if (service_ctx.relay_outputs[itoken].idle_state == IDLE_STATE_OPEN) {
                    sprintf(sys_command, service_ctx.relay_outputs[itoken].close);
                } else {
                    sprintf(sys_command, service_ctx.relay_outputs[itoken].open);
                }
            } else {
                if (service_ctx.relay_outputs[itoken].idle_state == IDLE_STATE_OPEN) {
                    sprintf(sys_command, service_ctx.relay_outputs[itoken].open);
                } else {
                    sprintf(sys_command, service_ctx.relay_outputs[itoken].close);
                }
            }
        } else {
            send_fault("deviceio_service", "Sender", "ter:InvalidArgVal", "ter:RelayToken", "Relay token",  "Unknown relay token reference");
            return -1;
        }
    } else {
        send_fault("deviceio_service", "Sender", "ter:InvalidArgVal", "ter:RelayToken", "Relay token",  "Unknown relay token reference");
        return -2;
    }

    if (sys_command[0] != '\0') {
        system(sys_command);
    }

    long size = cat(NULL, "deviceio_service_files/SetRelayOutputState.xml", 0);

    output_http_headers(size);

    return cat("stdout", "deviceio_service_files/SetRelayOutputState.xml", 0);
}

int deviceio_unsupported(const char *method)
{
    if (service_ctx.adv_fault_if_unknown == 1)
        send_action_failed_fault("deviceio_service", -1);
    else
        send_empty_response("tmd", (char *) method);
    return -1;
}
