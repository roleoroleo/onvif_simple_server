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

#include "device_service.h"
#include "fault.h"
#include "utils.h"
#include "log.h"
#include "ezxml_wrapper.h"
#include "onvif_simple_server.h"

extern service_context_t service_ctx;

int device_get_services()
{
    char address[16];
    char netmask[16];

    get_ip_address(address, netmask, service_ctx.ifs);
    char device_service_address[MAX_LEN];
    char media_service_address[MAX_LEN];
    char ptz_service_address[MAX_LEN];
    char events_service_address[MAX_LEN];
    char port[8];
    const char *cap;

    port[0] = '\0';
    if (service_ctx.port != 80)
        sprintf(port, ":%d", service_ctx.port);
    sprintf(device_service_address, "http://%s%s/onvif/device_service", address, port);
    sprintf(media_service_address, "http://%s%s/onvif/media_service", address, port);
    sprintf(ptz_service_address, "http://%s%s/onvif/ptz_service", address, port);
    sprintf(events_service_address, "http://%s%s/onvif/events_service", address, port);

    cap = get_element("IncludeCapability", "Body");
    if ((cap != NULL) && (strcasecmp(cap, "true")) == 0) {
        if (service_ctx.ptz_node.enable == 0) {
            long size = cat(NULL, "device_service_files/GetServices_with_capabilities_no_ptz.xml", 6,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "device_service_files/GetServices_with_capabilities_no_ptz.xml", 6,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address);
        } else {
            long size = cat(NULL, "device_service_files/GetServices_with_capabilities_ptz.xml", 8,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "device_service_files/GetServices_with_capabilities_ptz.xml", 8,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address);
        }
    } else {
        if (service_ctx.ptz_node.enable == 0) {
            long size = cat(NULL, "device_service_files/GetServices_no_ptz.xml", 6,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "device_service_files/GetServices_no_ptz.xml", 6,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address);
        } else {
            long size = cat(NULL, "device_service_files/GetServices_ptz.xml", 8,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "device_service_files/GetServices_ptz.xml", 8,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address);
        }
    }
}

int device_get_service_capabilities()
{
    long size = cat(NULL, "device_service_files/GetServiceCapabilities.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "device_service_files/GetServiceCapabilities.xml", 0);
}

int device_get_device_information()
{
    long size = cat(NULL, "device_service_files/GetDeviceInformation.xml", 10,
            "%MANUFACTURER%", service_ctx.manufacturer,
            "%MODEL%", service_ctx.model,
            "%FIRMWARE_VERSION%", service_ctx.firmware_ver,
            "%SERIAL_NUMBER%", service_ctx.serial_num,
            "%HARDWARE_ID%", service_ctx.hardware_id);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "device_service_files/GetDeviceInformation.xml", 10,
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

    char isfalse[] = "false";
    char istrue[] = "true";
    char *dst = isfalse;
    char hour[3];
    char minute[3];
    char second[3];
    char year[5];
    char month[3];
    char day[3];

    if (tm->tm_isdst) dst = istrue;
    sprintf(hour, "%d", tm->tm_hour);
    sprintf(minute, "%d", tm->tm_min);
    sprintf(second, "%d", tm->tm_sec);
    sprintf(year, "%d", tm->tm_year + 1900);
    sprintf(month, "%d", tm->tm_mon + 1);
    sprintf(day, "%d", tm->tm_mday);

    long size = cat(NULL, "device_service_files/GetSystemDateAndTime.xml", 14,
            "%DST%", dst,
            "%HOUR%", hour,
            "%MINUTE%", minute,
            "%SECOND%", second,
            "%YEAR%", year,
            "%MONTH%", month,
            "%DAY%", day);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "device_service_files/GetSystemDateAndTime.xml", 14,
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

    long size = cat(NULL, "device_service_files/SystemReboot.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    ret = cat("stdout", "device_service_files/SystemReboot.xml", 0);
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

    long size = cat(NULL, "device_service_files/GetScopes.xml", 2,
            "%SCOPES%", scopes);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    ret = cat("stdout", "device_service_files/GetScopes.xml", 2,
            "%SCOPES%", scopes);
    free(scopes);

    return ret;
}

int device_get_users()
{
    long size = cat(NULL, "device_service_files/GetUsers.xml", 2,
            "%USER%", service_ctx.user);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "device_service_files/GetUsers.xml", 2,
            "%USER%", service_ctx.user);
}

int device_get_wsdl_url()
{
    long size = cat(NULL, "device_service_files/GetWsdlUrl.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "device_service_files/GetWsdlUrl.xml", 0);
}

int device_get_capabilities()
{
    char address[16];
    char netmask[16];

    get_ip_address(address, netmask, service_ctx.ifs);
    char device_service_address[MAX_LEN];
    char media_service_address[MAX_LEN];
    char ptz_service_address[MAX_LEN];
    char events_service_address[MAX_LEN];
    char port[8];
    int icategory;
    const char *category;

    category = get_element("Category", "Body");
    if (category != NULL) {
        if (strcasecmp(category, "Device") == 0) {
            icategory = 1;
        } else if (strcasecmp(category, "Media") == 0) {
            icategory = 2;
        } else if (strcasecmp(category, "PTZ") == 0) {
            icategory = 4;
        } else if (strcasecmp(category, "Events") == 0) {
            icategory = 8;
        } else if (strcasecmp(category, "All") == 0) {
            icategory = 15;
        } else {
            send_fault();
            return -1;
        }
    } else {
        icategory = 15;
    }

    port[0] = '\0';
    if (service_ctx.port != 80)
        sprintf(port, ":%d", service_ctx.port);
    sprintf(device_service_address, "http://%s%s/onvif/device_service", address, port);
    sprintf(media_service_address, "http://%s%s/onvif/media_service", address, port);
    sprintf(ptz_service_address, "http://%s%s/onvif/ptz_service", address, port);
    sprintf(events_service_address, "http://%s%s/onvif/events_service", address, port);

    if (icategory == 1) {
        long size = cat(NULL, "device_service_files/GetDeviceCapabilities.xml", 2,
                "%DEVICE_SERVICE_ADDRESS%", device_service_address);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "device_service_files/GetDeviceCapabilities.xml", 2,
                "%DEVICE_SERVICE_ADDRESS%", device_service_address);
    } else if (icategory == 2) {
        long size = cat(NULL, "device_service_files/GetMediaCapabilities.xml", 2,
                "%MEDIA_SERVICE_ADDRESS%", media_service_address);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "device_service_files/GetMediaCapabilities.xml", 2,
                "%MEDIA_SERVICE_ADDRESS%", media_service_address);
    } else if (icategory == 4) {
        if (service_ctx.ptz_node.enable == 1) {
            long size = cat(NULL, "device_service_files/GetPTZCapabilities.xml", 2,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "device_service_files/GetPTZCapabilities.xml", 2,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address);
        } else {
            // TODO
        }
    } else if (icategory == 8) {
        long size = cat(NULL, "device_service_files/GetEventsCapabilities.xml", 2,
                "%EVENTS_SERVICE_ADDRESS%", events_service_address);

        fprintf(stdout, "Content-type: application/soap+xml\r\n");
        fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

        return cat("stdout", "device_service_files/GetEventsCapabilities.xml", 2,
                "%EVENTS_SERVICE_ADDRESS%", events_service_address);
    } else {
        if (service_ctx.ptz_node.enable == 0) {
            long size = cat(NULL, "device_service_files/GetCapabilities_no_ptz.xml", 6,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "device_service_files/GetCapabilities_no_ptz.xml", 6,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address);
        } else {
            long size = cat(NULL, "device_service_files/GetCapabilities_ptz.xml", 8,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address);

            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

            return cat("stdout", "device_service_files/GetCapabilities_ptz.xml", 8,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address);
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

    long size = cat(NULL, "device_service_files/GetNetworkInterfaces.xml", 8,
            "%INTERFACE%", service_ctx.ifs,
            "%MAC_ADDRESS%", mac_address,
            "%IP_ADDRESS%", address,
            "%NETMASK%", sprefix_len);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "device_service_files/GetNetworkInterfaces.xml", 8,
            "%INTERFACE%", service_ctx.ifs,
            "%MAC_ADDRESS%", mac_address,
            "%IP_ADDRESS%", address,
            "%NETMASK%", sprefix_len);
}

int device_unsupported(const char *method)
{
    send_fault();
    return -1;
}

int device_authentication_error()
{
    long size = cat(NULL, "generic_files/AuthenticationError.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Status: 400 Bad request\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "generic_files/AuthenticationError.xml", 0);
}
