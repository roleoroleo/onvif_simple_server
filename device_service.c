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
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "utils.h"
#include "onvif_simple_server.h"

extern service_context_t service_ctx;

int device_get_services(char *input)
{
    char address[16];
    char netmask[16];

    get_ip_address(address, netmask, service_ctx.ifs);
    char device_service_address[MAX_LEN];
    char media_service_address[MAX_LEN];
    char ptz_service_address[MAX_LEN];
    char port[8];

    port[0] = '\0';
    if (service_ctx.port != 80)
        sprintf(port, ":%d", service_ctx.port);
    sprintf(device_service_address, "http://%s%s/onvif/device_service", address, port);
    sprintf(media_service_address, "http://%s%s/onvif/media_service", address, port);
    sprintf(ptz_service_address, "http://%s%s/onvif/ptz_service", address, port);

    if (find_element(input, "IncludeCapability", "true") == 0) {
        if (service_ctx.ptz_node.enable == 0) {
            long size = get_file_size("device_service_files/GetServices_with_capabilities_no_ptz.xml");
            size += strlen(device_service_address) - strlen("%DEVICE_SERVICE_ADDRESS%") +
                    strlen(media_service_address) - strlen("%MEDIA_SERVICE_ADDRESS%");

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("device_service_files/GetServices_with_capabilities_no_ptz.xml", 4,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address);
        } else {
            long size = get_file_size("device_service_files/GetServices_with_capabilities_ptz.xml");
            size += strlen(device_service_address) - strlen("%DEVICE_SERVICE_ADDRESS%") +
                    strlen(media_service_address) - strlen("%MEDIA_SERVICE_ADDRESS%") +
                    strlen(ptz_service_address) - strlen("%PTZ_SERVICE_ADDRESS%");

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("device_service_files/GetServices_with_capabilities_ptz.xml", 6,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address);
        }
    } else {
        if (service_ctx.ptz_node.enable == 0) {
            long size = get_file_size("device_service_files/GetServices_no_ptz.xml");
            size += strlen(device_service_address) - strlen("%DEVICE_SERVICE_ADDRESS%") +
                    strlen(media_service_address) - strlen("%MEDIA_SERVICE_ADDRESS%");

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("device_service_files/GetServices_no_ptz.xml", 4,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address);
        } else {
            long size = get_file_size("device_service_files/GetServices_ptz.xml");
            size += strlen(device_service_address) - strlen("%DEVICE_SERVICE_ADDRESS%") +
                    strlen(media_service_address) - strlen("%MEDIA_SERVICE_ADDRESS%") +
                    strlen(ptz_service_address) - strlen("%PTZ_SERVICE_ADDRESS%");

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("device_service_files/GetServices_ptz.xml", 6,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address);
        }
    }
}

int device_get_service_capabilities()
{
    long size = get_file_size("device_service_files/GetServiceCapabilities.xml");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("device_service_files/GetServiceCapabilities.xml", 0);
}

int device_get_device_information()
{
    long size = get_file_size("device_service_files/GetDeviceInformation.xml");
    size += strlen(service_ctx.manufacturer) - strlen("%MANUFACTURER%") +
            strlen(service_ctx.model) - strlen("%MODEL%") +
            strlen(service_ctx.firmware_ver) - strlen("%FIRMWARE_VERSION%") +
            strlen(service_ctx.serial_num) - strlen("%SERIAL_NUMBER%") +
            strlen(service_ctx.hardware_id) - strlen("%HARDWARE_ID%");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("device_service_files/GetDeviceInformation.xml", 10,
            "%MANUFACTURER%", service_ctx.manufacturer,
            "%MODEL%", service_ctx.model,
            "%FIRMWARE_VERSION%", service_ctx.firmware_ver,
            "%SERIAL_NUMBER%", service_ctx.serial_num,
            "%HARDWARE_ID%", service_ctx.hardware_id);
}

int device_get_system_date_and_time()
{
    time_t timestamp = time(NULL);
    struct tm *tm = gmtime(&timestamp);

    char false[] = "false";
    char true[] = "true";
    char *dst = false;
    char hour[3];
    char minute[3];
    char second[3];
    char year[5];
    char month[3];
    char day[3];

    if (tm->tm_isdst) dst = true;
    sprintf(hour, "%d", tm->tm_hour);
    sprintf(minute, "%d", tm->tm_min);
    sprintf(second, "%d", tm->tm_sec);
    sprintf(year, "%d", tm->tm_year + 1900);
    sprintf(month, "%d", tm->tm_mon + 1);
    sprintf(day, "%d", tm->tm_mday);

    long size = get_file_size("device_service_files/GetSystemDateAndTime.xml");
    size += strlen(dst) - strlen("%DST%") +
            strlen(hour) - strlen("%HOUR%") +
            strlen(minute) - strlen("%MINUTE%") +
            strlen(second) - strlen("%SECOND%") +
            strlen(year) - strlen("%YEAR%") +
            strlen(month) - strlen("%MONTH%") +
            strlen(day) - strlen("%DAY%");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("device_service_files/GetSystemDateAndTime.xml", 14,
            "%DST%", dst,
            "%HOUR%", hour,
            "%MINUTE%", minute,
            "%SECOND%", second,
            "%YEAR%", year,
            "%MONTH%", month,
            "%DAY%", day);
}

int device_system_reboot()
{
    int ret;
    pthread_t reboot_pthread;

    long size = get_file_size("device_service_files/SystemReboot.xml");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    ret = cat("device_service_files/SystemReboot.xml", 0);
    sleep(1);

    pthread_create(&reboot_pthread, NULL, reboot_thread, NULL);
    pthread_detach(reboot_pthread);

    return ret;
}

int device_get_scopes()
{
    int i;
    char *scopes = (char *) malloc(service_ctx.scopes_num * MAX_LEN * sizeof(char));
    char line[MAX_LEN];
    int ret;

    scopes[0] = '\0';
    for (i = 0; i < service_ctx.scopes_num; i++) {
        sprintf(line, "\t    <tds:Scopes>\n\t\t<tt:ScopeDef>Fixed</tt:ScopeDef>\n\t\t<tt:ScopeItem>%s</tt:ScopeItem>\n\t    </tds:Scopes>\n", service_ctx.scopes[i]);
        strcat(scopes, line);
    }

    long size = get_file_size("device_service_files/GetScopes.xml");
    size += strlen(scopes) - strlen("%SCOPES%");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    ret = cat("device_service_files/GetScopes.xml", 2,
            "%SCOPES%", scopes);
    free(scopes);

    return ret;
}

int device_get_users()
{
    long size = get_file_size("device_service_files/GetUsers.xml");
    size += strlen(service_ctx.user) - strlen("%USER%");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("device_service_files/GetUsers.xml", 2,
            "%USER%", service_ctx.user);
}

int device_get_wsdl_url()
{
    long size = get_file_size("device_service_files/GetWsdlUrl.xml");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("device_service_files/GetWsdlUrl.xml", 0);
}

int device_get_capabilities(char *request)
{
    char address[16];
    char netmask[16];

    get_ip_address(address, netmask, service_ctx.ifs);
    char device_service_address[MAX_LEN];
    char media_service_address[MAX_LEN];
    char ptz_service_address[MAX_LEN];
    char port[8];
    int icategory;

    if (find_element(request, "Category", "Device") == 0) {
            icategory = 1;
    } else if (find_element(request, "Category", "Media") == 0) {
            icategory = 2;
    } else if (find_element(request, "Category", "PTZ") == 0) {
            icategory = 4;
    } else {
        icategory = 7;
    }

    port[0] = '\0';
    if (service_ctx.port != 80)
        sprintf(port, ":%d", service_ctx.port);
    sprintf(device_service_address, "http://%s%s/onvif/device_service", address, port);
    sprintf(media_service_address, "http://%s%s/onvif/media_service", address, port);
    sprintf(ptz_service_address, "http://%s%s/onvif/ptz_service", address, port);

    if (icategory == 1) {
        long size = get_file_size("device_service_files/GetDeviceCapabilities.xml");
        size += strlen(device_service_address) - strlen("%DEVICE_SERVICE_ADDRESS%");

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("device_service_files/GetDeviceCapabilities.xml", 2,
                "%DEVICE_SERVICE_ADDRESS%", device_service_address);
    } else if (icategory == 2) {
        long size = get_file_size("device_service_files/GetMediaCapabilities.xml");
        size += strlen(media_service_address) - strlen("%MEDIA_SERVICE_ADDRESS%");

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("device_service_files/GetMediaCapabilities.xml", 2,
                "%MEDIA_SERVICE_ADDRESS%", media_service_address);
    } else if (icategory == 4) {
        if (service_ctx.ptz_node.enable == 1) {
            long size = get_file_size("device_service_files/GetPTZCapabilities.xml");
            size += strlen(ptz_service_address) - strlen("%PTZ_SERVICE_ADDRESS%");

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("device_service_files/GetPTZCapabilities.xml", 2,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address);
        } else {
            // TODO
        }
    } else {
        if (service_ctx.ptz_node.enable == 0) {
            long size = get_file_size("device_service_files/GetCapabilities_no_ptz.xml");
            size += strlen(device_service_address) - strlen("%DEVICE_SERVICE_ADDRESS%") +
                    strlen(media_service_address) - strlen("%MEDIA_SERVICE_ADDRESS%");

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("device_service_files/GetCapabilities_no_ptz.xml", 4,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address);
        } else {
            long size = get_file_size("device_service_files/GetCapabilities_ptz.xml");
            size += strlen(device_service_address) - strlen("%DEVICE_SERVICE_ADDRESS%") +
                    strlen(media_service_address) - strlen("%MEDIA_SERVICE_ADDRESS%") +
                    strlen(ptz_service_address) - strlen("%PTZ_SERVICE_ADDRESS%");

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("device_service_files/GetCapabilities_ptz.xml", 6,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address);
        }
    }
}

int device_get_network_interfaces()
{
    char address[16];
    char netmask[16];
    char mac_address[16];
    int prefix_len;
    char sprefix_len[3];

    get_ip_address(address, netmask, service_ctx.ifs);
    prefix_len = netmask2prefixlen(netmask);
    sprintf(sprefix_len, "%d", prefix_len);
    get_mac_address(mac_address, service_ctx.ifs);

    long size = get_file_size("device_service_files/GetNetworkInterfaces.xml");
    size += strlen(service_ctx.ifs) - strlen("%INTERFACE%") +
            strlen(mac_address) - strlen("%MAC_ADDRESS%") +
            strlen(address) - strlen("%IP_ADDRESS%") +
            strlen(sprefix_len) - strlen("%NETMASK%");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("device_service_files/GetNetworkInterfaces.xml", 8,
            "%INTERFACE%", service_ctx.ifs,
            "%MAC_ADDRESS%", mac_address,
            "%IP_ADDRESS%", address,
            "%NETMASK%", sprefix_len);
}

int device_unsupported(char *action)
{
    char response[MAX_LEN];
    sprintf(response, "%sResponse", action);

    long size = get_file_size("device_service_files/Unsupported.xml");
    size += strlen(response) - strlen("%UNSUPPORTED%");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("device_service_files/Unsupported.xml", 2,
            "%UNSUPPORTED%", response);
}

int device_authentication_error()
{
    long size = get_file_size("device_service_files/AuthenticationError.xml");

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Status: 400 Bad request\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("device_service_files/AuthenticationError.xml", 0);
}
