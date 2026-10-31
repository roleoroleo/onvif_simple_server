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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"
#include "log.h"
#include "onvif_simple_server.h"

extern service_context_t service_ctx;

int send_empty_response(char *ns, const char *method)
{
    int ret;
    char *response = (char *) malloc(strlen(ns) + strlen(method) + 10);
    snprintf(response, strlen(ns) + strlen(method) + 10, "%s:%sResponse", ns, method);

    long size = cat(NULL, "generic_files/Empty.xml", 2,
            "%METHOD%", response);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    ret = cat("stdout", "generic_files/Empty.xml", 2,
            "%METHOD%", response);

    free(response);
    return ret;
}

int send_fault(char *service, char *rec_send, char *subcode, char *subcode_ex, char *reason, char *detail)
{
    char msg_uuid[UUID_LEN + 1];
    char device_address[MAX_LEN];
    char service_address[MAX_LEN];
    char port[8];
    char *cap;

    port[0] = '\0';
    if (service_ctx.port != 80)
        snprintf(port, sizeof(port), ":%d", service_ctx.port);
    snprintf(device_address, sizeof(device_address), "http://%s%s/onvif", service_ctx.address_url, port);
    snprintf(service_address, sizeof(service_address), "http://%s%s/onvif/%s", service_ctx.address_url, port, service);

    gen_uuid(msg_uuid);

    long size = cat(NULL, "generic_files/Fault.xml", 16,
            "%UUID%", msg_uuid,
            "%ADDRESS%", device_address,
            "%SERVICE%", service_address,
            "%REC_SEND%", rec_send,
            "%SUBCODE%", subcode,
            "%SUBCODE_EX%", subcode_ex,
            "%REASON%", reason,
            "%DETAIL%", detail);

//    fprintf(stdout, "Status: 500 Internal Server Error\r\n");
    fprintf(stdout, "HTTP/1.1 500 Internal Server Error\r\n");
    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n", size);
    fprintf(stdout, "\r\n");

    return cat("stdout", "generic_files/Fault.xml", 16,
            "%UUID%", msg_uuid,
            "%ADDRESS%", device_address,
            "%SERVICE%", service_address,
            "%REC_SEND%", rec_send,
            "%SUBCODE%", subcode,
            "%SUBCODE_EX%", subcode_ex,
            "%REASON%", reason,
            "%DETAIL%", detail);
}

int send_pull_messages_fault(char *timeout, char *message_limit)
{
    long size = cat(NULL, "generic_files/PullMessagesFaultResponse.xml", 4,
        "%MAX_TIMEOUT%", timeout,
        "%MAX_MESSAGE_LIMIT%", message_limit);

//    fprintf(stdout, "Status: 500 Internal Server Error\r\n");
    fprintf(stdout, "HTTP/1.1 500 Internal Server Error\r\n");
    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "generic_files/PullMessagesFaultResponse.xml", 4,
        "%MAX_TIMEOUT%", timeout,
        "%MAX_MESSAGE_LIMIT%", message_limit);
}

int send_action_failed_fault(char *service, int code)
{
    char error_string[1024];
    snprintf(error_string, sizeof(error_string), "The requested SOAP action failed: error %d", code);
    return send_fault(service, "Receiver", "ter:Action", "ter:ActionFailed", "Action failed", error_string);
}

int send_action_not_supported_fault(char *service)
{
    return send_fault(service, "Receiver", "ter:ActionNotSupported", "ter:NotSupported",
            "Optional Action Not Implemented",
            "The requested action is not implemented by the device.");
}

int send_authentication_error()
{
    long size = cat(NULL, "generic_files/AuthenticationError.xml", 0);

//    fprintf(stdout, "Status: 400 Bad request\r\n");
    fprintf(stdout, "HTTP/1.1 400 Bad request\r\n");
    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "generic_files/AuthenticationError.xml", 0);
}
