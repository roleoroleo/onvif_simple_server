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

#include "utils.h"
#include "log.h"
#include "onvif_simple_server.h"

extern service_context_t service_ctx;

int send_empty_response(char *ns, char *method)
{
    int ret;
    char *response = (char *) malloc(strlen(ns) + strlen(method) + 10);
    sprintf(response, "%s:%sResponse", ns, method);

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
    char address[16];
    char netmask[16];

    get_ip_address(address, netmask, service_ctx.ifs);
    char device_address[MAX_LEN];
    char service_address[MAX_LEN];
    char port[8];
    char *cap;

    port[0] = '\0';
    if (service_ctx.port != 80)
        sprintf(port, ":%d", service_ctx.port);
    sprintf(device_address, "http://%s%s/onvif", address, port);
    sprintf(service_address, "http://%s%s/onvif/%s", address, port, service);

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
    sprintf(error_string, "The requested SOAP action failed: error %d", code);
    send_fault(service, "Receiver", "ter:Action", "ter:ActionFailed", "Action failed", error_string);
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
