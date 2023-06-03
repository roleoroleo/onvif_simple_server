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

int events_unsupported(char *action)
{
    char response[MAX_LEN];
    sprintf(response, "%sResponse", action);

    long size = get_file_size("events_service_files/Unsupported.xml");
    size += strlen(response) - strlen("%UNSUPPORTED%");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("events_service_files/Unsupported.xml", 2,
            "%UNSUPPORTED%", response);
}
