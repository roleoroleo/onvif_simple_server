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

int send_fault()
{
    char msg_uuid[UUID_LEN + 1];
    char address[16];
    char netmask[16];

    get_ip_address(address, netmask, service_ctx.ifs);
    char device_address[MAX_LEN];
    char events_service_address[MAX_LEN];
    char port[8];
    char *cap;

    port[0] = '\0';
    if (service_ctx.port != 80)
        sprintf(port, ":%d", service_ctx.port);
    sprintf(device_address, "http://%s%s/onvif", address, port);
    sprintf(events_service_address, "http://%s%s/onvif/events_service", address, port);

    gen_uuid(msg_uuid);

    long size = cat(NULL, "generic_files/Fault.xml", 6,
            "%UUID%", msg_uuid,
            "%ADDRESS%", device_address,
            "%SERVICE%", events_service_address);

//    fprintf(stdout, "Status: 500 Internal Server Error\r\n");
    fprintf(stdout, "HTTP/1.1 500 Internal Server Error\r\n");
    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n", size);
    fprintf(stdout, "\r\n");

    return cat("stdout", "generic_files/Fault.xml", 6,
            "%UUID%", msg_uuid,
            "%ADDRESS%", device_address,
            "%SERVICE%", events_service_address);
}

int authentication_error()
{
    long size = cat(NULL, "generic_files/AuthenticationError.xml", 0);

//    fprintf(stdout, "Status: 400 Bad request\r\n");
    fprintf(stdout, "HTTP/1.1 400 Bad request\r\n");
    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "generic_files/AuthenticationError.xml", 0);
}
