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

#include <arpa/inet.h>
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

/* Imaging is opt-in (imaging_enable): when disabled the fragments are empty. */
static void build_imaging_service(char *buf, size_t size, const char *address, int with_capabilities)
{
    if (!service_ctx.imaging_enable) {
        buf[0] = '\0';
        return;
    }
    snprintf(buf, size,
            "<tds:Service>"
            "<tds:Namespace>http://www.onvif.org/ver20/imaging/wsdl</tds:Namespace>"
            "<tds:XAddr>%s</tds:XAddr>"
            "%s"
            "<tds:Version><tt:Major>2</tt:Major><tt:Minor>0</tt:Minor></tds:Version>"
            "</tds:Service>",
            address,
            with_capabilities ?
                "<tds:Capabilities><timg:Capabilities xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" "
                "ImageStabilization=\"false\" Presets=\"false\"/></tds:Capabilities>" : "");
}

static void build_imaging_capability(char *buf, size_t size, const char *address)
{
    if (!service_ctx.imaging_enable) {
        buf[0] = '\0';
        return;
    }
    snprintf(buf, size, "<tt:Imaging><tt:XAddr>%s</tt:XAddr></tt:Imaging>", address);
}

int device_get_services()
{
    char device_service_address[MAX_LEN];
    char media_service_address[MAX_LEN];
    char media2_service_address[MAX_LEN];
    char ptz_service_address[MAX_LEN];
    char events_service_address[MAX_LEN];
    char deviceio_service_address[MAX_LEN];
    char imaging_service_address[MAX_LEN];
    char imaging_service[1024];
    char port[8];
    const char *cap;

    char ebasesubscription[8], epullpoint[8];

    char audio_sources[2], audio_outputs[2], relay_outputs[2];

    port[0] = '\0';
    if (service_ctx.port != 80)
        snprintf(port, sizeof(port), ":%d", service_ctx.port);
    snprintf(device_service_address, sizeof(device_service_address), "http://%s%s/onvif/device_service", service_ctx.address_url, port);
    snprintf(media_service_address, sizeof(media_service_address), "http://%s%s/onvif/media_service", service_ctx.address_url, port);
    snprintf(media2_service_address, sizeof(media2_service_address), "http://%s%s/onvif/media2_service", service_ctx.address_url, port);
    snprintf(ptz_service_address, sizeof(ptz_service_address), "http://%s%s/onvif/ptz_service", service_ctx.address_url, port);
    snprintf(events_service_address, sizeof(events_service_address), "http://%s%s/onvif/events_service", service_ctx.address_url, port);
    snprintf(deviceio_service_address, sizeof(deviceio_service_address), "http://%s%s/onvif/deviceio_service", service_ctx.address_url, port);
    snprintf(imaging_service_address, sizeof(imaging_service_address), "http://%s%s/onvif/imaging_service", service_ctx.address_url, port);

    if ((service_ctx.events_enable == EVENTS_PULLPOINT) || (service_ctx.events_enable == EVENTS_BOTH)) {
        snprintf(epullpoint, sizeof(epullpoint), "%s", "true");
    } else {
        snprintf(epullpoint, sizeof(epullpoint), "%s", "false");
    }
    if ((service_ctx.events_enable == EVENTS_BASESUBSCRIPTION) || (service_ctx.events_enable == EVENTS_BOTH)) {
        snprintf(ebasesubscription, sizeof(ebasesubscription), "%s", "true");
    } else {
        snprintf(ebasesubscription, sizeof(ebasesubscription), "%s", "false");
    }

    if ((service_ctx.profiles[0].audio_encoder != AUDIO_NONE) ||
            ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE))) {

        snprintf(audio_sources, sizeof(audio_sources), "%d", 1);
    } else {
        snprintf(audio_sources, sizeof(audio_sources), "%d", 0);
    }
    if ((service_ctx.profiles[0].audio_decoder != AUDIO_NONE) ||
            ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_decoder != AUDIO_NONE))) {

        snprintf(audio_outputs, sizeof(audio_outputs), "%d", 1);
    } else {
        snprintf(audio_outputs, sizeof(audio_outputs), "%d", 0);
    }

    snprintf(relay_outputs, sizeof(relay_outputs), "%d", service_ctx.relay_outputs_num);

    cap = get_element("IncludeCapability", "Body");
    build_imaging_service(imaging_service, sizeof(imaging_service), imaging_service_address,
            (cap != NULL) && (strcasecmp(cap, "true") == 0));
    if ((cap != NULL) && (strcasecmp(cap, "true")) == 0) {
        if ((service_ctx.ptz_node.enable == 0) && (service_ctx.adv_enable_media2 == 0)) {
            long size = cat(NULL, "device_service_files/GetServices_with_capabilities_no_ptz_no_media2.xml", 20,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%EVENTS_BASESUBSCRIPTION%", ebasesubscription,
                    "%EVENTS_PULLPOINT%", epullpoint,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%AUDIO_SOURCES%", audio_sources,
                    "%AUDIO_OUTPUTS%", audio_outputs,
                    "%RELAY_OUTPUTS%", relay_outputs,
                    "%IMAGING_SERVICE%", imaging_service);

            output_http_headers(size);

            return cat("stdout", "device_service_files/GetServices_with_capabilities_no_ptz_no_media2.xml", 20,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%EVENTS_BASESUBSCRIPTION%", ebasesubscription,
                    "%EVENTS_PULLPOINT%", epullpoint,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%AUDIO_SOURCES%", audio_sources,
                    "%AUDIO_OUTPUTS%", audio_outputs,
                    "%RELAY_OUTPUTS%", relay_outputs,
                    "%IMAGING_SERVICE%", imaging_service);

        } else if ((service_ctx.ptz_node.enable == 0) && (service_ctx.adv_enable_media2 == 1)) {
            long size = cat(NULL, "device_service_files/GetServices_with_capabilities_no_ptz_media2.xml", 22,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%MEDIA2_SERVICE_ADDRESS%", media2_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%EVENTS_BASESUBSCRIPTION%", ebasesubscription,
                    "%EVENTS_PULLPOINT%", epullpoint,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%AUDIO_SOURCES%", audio_sources,
                    "%AUDIO_OUTPUTS%", audio_outputs,
                    "%RELAY_OUTPUTS%", relay_outputs,
                    "%IMAGING_SERVICE%", imaging_service);

            output_http_headers(size);

            return cat("stdout", "device_service_files/GetServices_with_capabilities_no_ptz_media2.xml", 22,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%MEDIA2_SERVICE_ADDRESS%", media2_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%EVENTS_BASESUBSCRIPTION%", ebasesubscription,
                    "%EVENTS_PULLPOINT%", epullpoint,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%AUDIO_SOURCES%", audio_sources,
                    "%AUDIO_OUTPUTS%", audio_outputs,
                    "%RELAY_OUTPUTS%", relay_outputs,
                    "%IMAGING_SERVICE%", imaging_service);

        } else if ((service_ctx.ptz_node.enable == 1) && (service_ctx.adv_enable_media2 == 0)) {
            long size = cat(NULL, "device_service_files/GetServices_with_capabilities_ptz_no_media2.xml", 22,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%EVENTS_BASESUBSCRIPTION%", ebasesubscription,
                    "%EVENTS_PULLPOINT%", epullpoint,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%AUDIO_SOURCES%", audio_sources,
                    "%AUDIO_OUTPUTS%", audio_outputs,
                    "%RELAY_OUTPUTS%", relay_outputs,
                    "%IMAGING_SERVICE%", imaging_service);

            output_http_headers(size);

            return cat("stdout", "device_service_files/GetServices_with_capabilities_ptz_no_media2.xml", 22,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%EVENTS_BASESUBSCRIPTION%", ebasesubscription,
                    "%EVENTS_PULLPOINT%", epullpoint,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%AUDIO_SOURCES%", audio_sources,
                    "%AUDIO_OUTPUTS%", audio_outputs,
                    "%RELAY_OUTPUTS%", relay_outputs,
                    "%IMAGING_SERVICE%", imaging_service);

        } else if ((service_ctx.ptz_node.enable == 1) && (service_ctx.adv_enable_media2 == 1)) {
            long size = cat(NULL, "device_service_files/GetServices_with_capabilities_ptz_media2.xml", 24,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%MEDIA2_SERVICE_ADDRESS%", media2_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%EVENTS_BASESUBSCRIPTION%", ebasesubscription,
                    "%EVENTS_PULLPOINT%", epullpoint,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%AUDIO_SOURCES%", audio_sources,
                    "%AUDIO_OUTPUTS%", audio_outputs,
                    "%RELAY_OUTPUTS%", relay_outputs,
                    "%IMAGING_SERVICE%", imaging_service);

            output_http_headers(size);

            return cat("stdout", "device_service_files/GetServices_with_capabilities_ptz_media2.xml", 24,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%MEDIA2_SERVICE_ADDRESS%", media2_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%EVENTS_BASESUBSCRIPTION%", ebasesubscription,
                    "%EVENTS_PULLPOINT%", epullpoint,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%AUDIO_SOURCES%", audio_sources,
                    "%AUDIO_OUTPUTS%", audio_outputs,
                    "%RELAY_OUTPUTS%", relay_outputs,
                    "%IMAGING_SERVICE%", imaging_service);
        }
    } else {
        if ((service_ctx.ptz_node.enable == 0) && (service_ctx.adv_enable_media2 == 0)) {
            long size = cat(NULL, "device_service_files/GetServices_no_ptz_no_media2.xml", 10,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%IMAGING_SERVICE%", imaging_service);

            output_http_headers(size);

            return cat("stdout", "device_service_files/GetServices_no_ptz_no_media2.xml", 10,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%IMAGING_SERVICE%", imaging_service);

        } else if ((service_ctx.ptz_node.enable == 0) && (service_ctx.adv_enable_media2 == 1)) {
            long size = cat(NULL, "device_service_files/GetServices_no_ptz_media2.xml", 12,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%MEDIA2_SERVICE_ADDRESS%", media2_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%IMAGING_SERVICE%", imaging_service);

            output_http_headers(size);

            return cat("stdout", "device_service_files/GetServices_no_ptz_media2.xml", 12,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%MEDIA2_SERVICE_ADDRESS%", media2_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%IMAGING_SERVICE%", imaging_service);

        } else if ((service_ctx.ptz_node.enable == 1) && (service_ctx.adv_enable_media2 == 0)) {
            long size = cat(NULL, "device_service_files/GetServices_ptz_no_media2.xml", 12,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%IMAGING_SERVICE%", imaging_service);

            output_http_headers(size);

            return cat("stdout", "device_service_files/GetServices_ptz_no_media2.xml", 12,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%IMAGING_SERVICE%", imaging_service);

        } else if ((service_ctx.ptz_node.enable == 1) && (service_ctx.adv_enable_media2 == 1)) {
            long size = cat(NULL, "device_service_files/GetServices_ptz_media2.xml", 14,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%MEDIA2_SERVICE_ADDRESS%", media2_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%IMAGING_SERVICE%", imaging_service);

            output_http_headers(size);

            return cat("stdout", "device_service_files/GetServices_ptz_media2.xml", 14,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%MEDIA2_SERVICE_ADDRESS%", media2_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%IMAGING_SERVICE%", imaging_service);
        }
    }
}

int device_get_service_capabilities()
{
    long size = cat(NULL, "device_service_files/GetServiceCapabilities.xml", 0);

    output_http_headers(size);

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

    output_http_headers(size);

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
    struct tm *tm;

    char isfalse[] = "false";
    char istrue[] = "true";
    char *dst = isfalse;
    char tz_str[32];
    char hour[3], minute[3], second[3];
    char year[5], month[3], day[3];
    char local_hour[3], local_minute[3], local_second[3];
    char local_year[5], local_month[3], local_day[3];

    /* UTC date/time for UTCDateTime element */
    tm = gmtime(&timestamp);
    snprintf(hour,   sizeof(hour),   "%d", tm->tm_hour);
    snprintf(minute, sizeof(minute), "%d", tm->tm_min);
    snprintf(second, sizeof(second), "%d", tm->tm_sec);
    snprintf(year,   sizeof(year),   "%d", tm->tm_year + 1900);
    snprintf(month,  sizeof(month),  "%d", tm->tm_mon + 1);
    snprintf(day,    sizeof(day),    "%d", tm->tm_mday);

    /* Local date/time for DST flag, POSIX TZ string, and LocalDateTime element */
    tzset();
    tm = localtime(&timestamp);
    if (tm->tm_isdst) dst = istrue;
    snprintf(local_hour,   sizeof(local_hour),   "%d", tm->tm_hour);
    snprintf(local_minute, sizeof(local_minute), "%d", tm->tm_min);
    snprintf(local_second, sizeof(local_second), "%d", tm->tm_sec);
    snprintf(local_year,   sizeof(local_year),   "%d", tm->tm_year + 1900);
    snprintf(local_month,  sizeof(local_month),  "%d", tm->tm_mon + 1);
    snprintf(local_day,    sizeof(local_day),    "%d", tm->tm_mday);

    /* TZ string for the SOAP response.
     * ONVIF_TZ_STRING env var (set by start_onvif.sh) takes full precedence --
     * the script controls format and any XML escaping needed.
     * Fallback: derive from tzset() globals, e.g. "CET-1CEST" or "+03-3". */
    {
        const char *env_tz = getenv("ONVIF_TZ_STRING");
        if (env_tz && env_tz[0] != '\0')
            snprintf(tz_str, sizeof(tz_str), "%s", env_tz);
        else
            snprintf(tz_str, sizeof(tz_str), "%s%ld%s",
                     tzname[0], timezone / 3600, daylight ? tzname[1] : "");
    }

    long size = cat(NULL, "device_service_files/GetSystemDateAndTime.xml", 28,
            "%DST%", dst,
            "%TZ%", tz_str,
            "%HOUR%", hour,
            "%MINUTE%", minute,
            "%SECOND%", second,
            "%YEAR%", year,
            "%MONTH%", month,
            "%DAY%", day,
            "%LOCAL_HOUR%", local_hour,
            "%LOCAL_MINUTE%", local_minute,
            "%LOCAL_SECOND%", local_second,
            "%LOCAL_YEAR%", local_year,
            "%LOCAL_MONTH%", local_month,
            "%LOCAL_DAY%", local_day);

    output_http_headers(size);

    return cat("stdout", "device_service_files/GetSystemDateAndTime.xml", 28,
            "%DST%", dst,
            "%TZ%", tz_str,
            "%HOUR%", hour,
            "%MINUTE%", minute,
            "%SECOND%", second,
            "%YEAR%", year,
            "%MONTH%", month,
            "%DAY%", day,
            "%LOCAL_HOUR%", local_hour,
            "%LOCAL_MINUTE%", local_minute,
            "%LOCAL_SECOND%", local_second,
            "%LOCAL_YEAR%", local_year,
            "%LOCAL_MONTH%", local_month,
            "%LOCAL_DAY%", local_day);
}

int device_system_reboot()
{
    int ret;
    pthread_t reboot_pthread;

    long size = cat(NULL, "device_service_files/SystemReboot.xml", 0);

    output_http_headers(size);

    ret = cat("stdout", "device_service_files/SystemReboot.xml", 0);
    fflush(stdout);
    sleep(1);

    pthread_create(&reboot_pthread, NULL, reboot_thread, NULL);
    pthread_join(reboot_pthread, NULL);

    return ret;
}

int device_set_system_factory_default()
{
    const char *fd_type;
    int ret;

    /* Opt-in: without a configured command the operation is not supported. */
    if ((service_ctx.factory_default_command == NULL) ||
            (service_ctx.factory_default_command[0] == '\0')) {
        return send_action_not_supported_fault("device_service");
    }

    /* FactoryDefault is "Hard" or "Soft" (default Hard). Pass it to the hook via
     * an env var -- never interpolated into the shell command -- so the request
     * cannot inject shell syntax. */
    fd_type = get_element("FactoryDefault", "Body");
    if ((fd_type != NULL) && (strcmp(fd_type, "Soft") == 0))
        setenv("ONVIF_FACTORY_DEFAULT_TYPE", "Soft", 1);
    else
        setenv("ONVIF_FACTORY_DEFAULT_TYPE", "Hard", 1);

    long size = cat(NULL, "device_service_files/SetSystemFactoryDefault.xml", 0);
    output_http_headers(size);
    ret = cat("stdout", "device_service_files/SetSystemFactoryDefault.xml", 0);
    fflush(stdout);
    sleep(1);

    /* Fire-and-forget action (the command typically wipes config and reboots) --
     * same "respond, then act" flow as device_system_reboot(). */
    system(service_ctx.factory_default_command);

    return ret;
}

int device_get_scopes()
{
    int i;
    size_t count = (size_t) service_ctx.scopes_num;
    size_t alloc = count > 0 ? count * MAX_LEN : 1;
    char* scopes = (char*) malloc(alloc);
    char line[MAX_LEN];
    int ret;

    if (!scopes) {
        log_error("device_get_scopes: out of memory");
        send_action_failed_fault("device_service", -1);
        return -1;
    }

    scopes[0] = '\0';
    for (i = 0; i < service_ctx.scopes_num; i++) {
        snprintf(line, sizeof(line), "\t    <tds:Scopes>\n\t\t<tt:ScopeDef>Fixed</tt:ScopeDef>\n\t\t<tt:ScopeItem>%s</tt:ScopeItem>\n\t    </tds:Scopes>\n", service_ctx.scopes[i]);
        strncat(scopes, line, alloc - strlen(scopes) - 1);
    }

    long size = cat(NULL, "device_service_files/GetScopes.xml", 2,
            "%SCOPES%", scopes);

    output_http_headers(size);

    ret = cat("stdout", "device_service_files/GetScopes.xml", 2,
            "%SCOPES%", scopes);
    free(scopes);

    return ret;
}

int device_get_users()
{
    // For security reason, returns empty message instead of service_ctx.user
    return send_empty_response("tds", "GetUsers");
}

int device_get_wsdl_url()
{
    long size = cat(NULL, "device_service_files/GetWsdlUrl.xml", 0);

    output_http_headers(size);

    return cat("stdout", "device_service_files/GetWsdlUrl.xml", 0);
}

int device_get_capabilities()
{
    char device_service_address[MAX_LEN];
    char media_service_address[MAX_LEN];
    char ptz_service_address[MAX_LEN];
    char events_service_address[MAX_LEN];
    char deviceio_service_address[MAX_LEN];
    char imaging_service_address[MAX_LEN];
    char imaging_capability[MAX_LEN];
    char port[8];
    int icategory;
    const char *category;

    char ebasesubscription[8], epullpoint[8];

    char audio_sources[2], audio_outputs[2], relay_outputs[2];

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
        } else if (strcasecmp(category, "Imaging") == 0) {
            icategory = 16;
        } else if (strcasecmp(category, "All") == 0) {
            icategory = 15;
        } else {
            send_fault("device_service", "Receiver", "ter:ActionNotSupported", "ter:NoSuchService", "No such service", "The requested WSDL service category is not supported by the device");
            return -2;
        }
    } else {
        icategory = 15;
    }

    port[0] = '\0';
    if (service_ctx.port != 80)
        snprintf(port, sizeof(port), ":%d", service_ctx.port);
    snprintf(device_service_address, sizeof(device_service_address), "http://%s%s/onvif/device_service", service_ctx.address_url, port);
    snprintf(media_service_address, sizeof(media_service_address), "http://%s%s/onvif/media_service", service_ctx.address_url, port);
    snprintf(ptz_service_address, sizeof(ptz_service_address), "http://%s%s/onvif/ptz_service", service_ctx.address_url, port);
    snprintf(events_service_address, sizeof(events_service_address), "http://%s%s/onvif/events_service", service_ctx.address_url, port);
    snprintf(deviceio_service_address, sizeof(deviceio_service_address), "http://%s%s/onvif/deviceio_service", service_ctx.address_url, port);
    snprintf(imaging_service_address, sizeof(imaging_service_address), "http://%s%s/onvif/imaging_service", service_ctx.address_url, port);

    if ((service_ctx.events_enable == EVENTS_PULLPOINT) || (service_ctx.events_enable == EVENTS_BOTH)) {
        snprintf(epullpoint, sizeof(epullpoint), "%s", "true");
    } else {
        snprintf(epullpoint, sizeof(epullpoint), "%s", "false");
    }
    if ((service_ctx.events_enable == EVENTS_BASESUBSCRIPTION) || (service_ctx.events_enable == EVENTS_BOTH)) {
        snprintf(ebasesubscription, sizeof(ebasesubscription), "%s", "true");
    } else {
        snprintf(ebasesubscription, sizeof(ebasesubscription), "%s", "false");
    }

    if ((service_ctx.profiles[0].audio_encoder != AUDIO_NONE) ||
            ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE))) {

        snprintf(audio_sources, sizeof(audio_sources), "%d", 1);
    } else {
        snprintf(audio_sources, sizeof(audio_sources), "%d", 0);
    }
    if ((service_ctx.profiles[0].audio_decoder != AUDIO_NONE) ||
            ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_decoder != AUDIO_NONE))) {

        snprintf(audio_outputs, sizeof(audio_outputs), "%d", 1);
    } else {
        snprintf(audio_outputs, sizeof(audio_outputs), "%d", 0);
    }

    snprintf(relay_outputs, sizeof(relay_outputs), "%d", service_ctx.relay_outputs_num);

    build_imaging_capability(imaging_capability, sizeof(imaging_capability), imaging_service_address);

    if (icategory == 1) {
        long size = cat(NULL, "device_service_files/GetDeviceCapabilities.xml", 2,
                "%DEVICE_SERVICE_ADDRESS%", device_service_address);

        output_http_headers(size);

        return cat("stdout", "device_service_files/GetDeviceCapabilities.xml", 2,
                "%DEVICE_SERVICE_ADDRESS%", device_service_address);
    } else if (icategory == 2) {
        long size = cat(NULL, "device_service_files/GetMediaCapabilities.xml", 2,
                "%MEDIA_SERVICE_ADDRESS%", media_service_address);

        output_http_headers(size);

        return cat("stdout", "device_service_files/GetMediaCapabilities.xml", 2,
                "%MEDIA_SERVICE_ADDRESS%", media_service_address);
    } else if (icategory == 4) {
        if (service_ctx.ptz_node.enable == 1) {
            long size = cat(NULL, "device_service_files/GetPTZCapabilities.xml", 2,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address);

            output_http_headers(size);

            return cat("stdout", "device_service_files/GetPTZCapabilities.xml", 2,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address);
        } else {
            send_fault("device_service", "Receiver", "ter:ActionNotSupported", "ter:NoSuchService", "No such service", "The requested WSDL service category is not supported by the device");
            return -3;
        }
    } else if (icategory == 8) {
        long size = cat(NULL, "device_service_files/GetEventsCapabilities.xml", 6,
                "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                "%EVENTS_BASESUBSCRIPTION%", ebasesubscription,
                "%EVENTS_PULLPOINT%", epullpoint);

        output_http_headers(size);

        return cat("stdout", "device_service_files/GetEventsCapabilities.xml", 6,
                "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                "%EVENTS_BASESUBSCRIPTION%", ebasesubscription,
                "%EVENTS_PULLPOINT%", epullpoint);
    } else if (icategory == 16) {
        if (!service_ctx.imaging_enable) {
            send_fault("device_service", "Receiver", "ter:ActionNotSupported", "ter:NoSuchService", "No such service", "The requested WSDL service category is not supported by the device");
            return -3;
        }
        long size = cat(NULL, "device_service_files/GetImagingCapabilities.xml", 2,
                "%IMAGING_CAPABILITY%", imaging_capability);

        output_http_headers(size);

        return cat("stdout", "device_service_files/GetImagingCapabilities.xml", 2,
                "%IMAGING_CAPABILITY%", imaging_capability);
    } else {
        if (service_ctx.ptz_node.enable == 0) {
            long size = cat(NULL, "device_service_files/GetCapabilities_no_ptz.xml", 20,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%EVENTS_BASESUBSCRIPTION%", ebasesubscription,
                    "%EVENTS_PULLPOINT%", epullpoint,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%AUDIO_SOURCES%", audio_sources,
                    "%AUDIO_OUTPUTS%", audio_outputs,
                    "%RELAY_OUTPUTS%", relay_outputs,
                    "%IMAGING_CAPABILITY%", imaging_capability);

            output_http_headers(size);

            return cat("stdout", "device_service_files/GetCapabilities_no_ptz.xml", 20,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%EVENTS_BASESUBSCRIPTION%", ebasesubscription,
                    "%EVENTS_PULLPOINT%", epullpoint,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%AUDIO_SOURCES%", audio_sources,
                    "%AUDIO_OUTPUTS%", audio_outputs,
                    "%RELAY_OUTPUTS%", relay_outputs,
                    "%IMAGING_CAPABILITY%", imaging_capability);
        } else {
            long size = cat(NULL, "device_service_files/GetCapabilities_ptz.xml", 22,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%EVENTS_BASESUBSCRIPTION%", ebasesubscription,
                    "%EVENTS_PULLPOINT%", epullpoint,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%AUDIO_SOURCES%", audio_sources,
                    "%AUDIO_OUTPUTS%", audio_outputs,
                    "%RELAY_OUTPUTS%", relay_outputs,
                    "%IMAGING_CAPABILITY%", imaging_capability);

            output_http_headers(size);

            return cat("stdout", "device_service_files/GetCapabilities_ptz.xml", 22,
                    "%DEVICE_SERVICE_ADDRESS%", device_service_address,
                    "%MEDIA_SERVICE_ADDRESS%", media_service_address,
                    "%PTZ_SERVICE_ADDRESS%", ptz_service_address,
                    "%EVENTS_SERVICE_ADDRESS%", events_service_address,
                    "%EVENTS_BASESUBSCRIPTION%", ebasesubscription,
                    "%EVENTS_PULLPOINT%", epullpoint,
                    "%DEVICEIO_SERVICE_ADDRESS%", deviceio_service_address,
                    "%AUDIO_SOURCES%", audio_sources,
                    "%AUDIO_OUTPUTS%", audio_outputs,
                    "%RELAY_OUTPUTS%", relay_outputs,
                    "%IMAGING_CAPABILITY%", imaging_capability);
        }
    }
}

int device_get_network_interfaces()
{
    char address[16];
    char netmask[16];
    char mac_address[18];
    char mtu[16];
    int prefix_len;
    char sprefix_len[3];
    char ll_addr[INET6_ADDRSTRLEN] = "";
    char gl_addr[INET6_ADDRSTRLEN] = "";
    int ll_prefix = 64, gl_prefix = 64;
    char ipv6_enabled[8];
    char ipv6_config[512];
    int ipv6_found;
    int ret;

    ret = get_ip_address(address, netmask, service_ctx.ifs);
    if (ret < 0) {
        log_error("Unable to get ip address from interface %s", service_ctx.ifs);
        send_action_failed_fault("device_service", -1);
        return -1;
    }
    prefix_len = netmask2prefixlen(netmask);
    snprintf(sprefix_len, sizeof(sprefix_len), "%d", prefix_len);
    ret = get_mac_address(mac_address, service_ctx.ifs);
    if (ret < 0) {
        mac_address[0] = '\0';
    }
    snprintf(mtu, sizeof(mtu), "%d", get_mtu(service_ctx.ifs));

    ipv6_found = get_ipv6_address(service_ctx.ifs, ll_addr, &ll_prefix, gl_addr, &gl_prefix);
    if (ipv6_found > 0) {
        char ll_elem[160] = "";
        char gl_elem[160] = "";
        if (ipv6_found & 1)
            snprintf(ll_elem, sizeof(ll_elem),
                "<tt:LinkLocal><tt:Address>%s</tt:Address>"
                "<tt:PrefixLength>%d</tt:PrefixLength></tt:LinkLocal>",
                ll_addr, ll_prefix);
        if (ipv6_found & 2)
            snprintf(gl_elem, sizeof(gl_elem),
                "<tt:FromRA><tt:Address>%s</tt:Address>"
                "<tt:PrefixLength>%d</tt:PrefixLength></tt:FromRA>",
                gl_addr, gl_prefix);
        snprintf(ipv6_enabled, sizeof(ipv6_enabled), "%s", "true");
        snprintf(ipv6_config, sizeof(ipv6_config),
            "<tt:Config><tt:AcceptRouterAdvert>true</tt:AcceptRouterAdvert>"
            "<tt:DHCP>Off</tt:DHCP>%s%s</tt:Config>",
            ll_elem, gl_elem);
    } else {
        snprintf(ipv6_enabled, sizeof(ipv6_enabled), "%s", "false");
        ipv6_config[0] = '\0';
    }

    long size = cat(NULL, "device_service_files/GetNetworkInterfaces.xml", 14,
            "%INTERFACE%", service_ctx.ifs,
            "%MAC_ADDRESS%", mac_address,
            "%MTU%", mtu,
            "%IP_ADDRESS%", address,
            "%NETMASK%", sprefix_len,
            "%IPV6_ENABLED%", ipv6_enabled,
            "%IPV6_CONFIG%", ipv6_config);

    output_http_headers(size);

    return cat("stdout", "device_service_files/GetNetworkInterfaces.xml", 14,
            "%INTERFACE%", service_ctx.ifs,
            "%MAC_ADDRESS%", mac_address,
            "%MTU%", mtu,
            "%IP_ADDRESS%", address,
            "%NETMASK%", sprefix_len,
            "%IPV6_ENABLED%", ipv6_enabled,
            "%IPV6_CONFIG%", ipv6_config);
}

int device_get_discovery_mode()
{
    long size = cat(NULL, "device_service_files/GetDiscoveryMode.xml", 0);

    output_http_headers(size);

    return cat("stdout", "device_service_files/GetDiscoveryMode.xml", 0);
}

int device_get_endpoint_reference()
{
    long size = cat(NULL, "device_service_files/GetEndpointReference.xml", 2,
            "%DEVICE_UUID%", service_ctx.device_uuid);

    output_http_headers(size);

    return cat("stdout", "device_service_files/GetEndpointReference.xml", 2,
            "%DEVICE_UUID%", service_ctx.device_uuid);
}

int device_get_network_default_gateway()
{
    char gw[INET_ADDRSTRLEN] = "";
    get_default_gateway(gw, sizeof(gw));
    long size = cat(NULL, "device_service_files/GetNetworkDefaultGateway.xml", 2,
            "%GATEWAY%", gw);
    output_http_headers(size);
    return cat("stdout", "device_service_files/GetNetworkDefaultGateway.xml", 2,
            "%GATEWAY%", gw);
}

int device_get_network_protocols()
{
    long size = cat(NULL, "device_service_files/GetNetworkProtocols.xml", 0);
    output_http_headers(size);
    return cat("stdout", "device_service_files/GetNetworkProtocols.xml", 0);
}

int device_get_hostname()
{
    char hostname[256] = "";
    gethostname(hostname, sizeof(hostname));

    long size = cat(NULL, "device_service_files/GetHostname.xml", 2,
            "%HOSTNAME%", hostname);
    output_http_headers(size);
    return cat("stdout", "device_service_files/GetHostname.xml", 2,
            "%HOSTNAME%", hostname);
}

int device_get_dns()
{
    /* GetDNS returns tt:DNSInformation, whose DNSManual element is an
     * unbounded list: report every configured resolver, one entry each. */
    char dns_list[512];
    char dns_block[2048] = "";
    if (get_dns_server(dns_list, sizeof(dns_list)) > 0) {
        char *save = NULL;
        char *s;
        for (s = strtok_r(dns_list, "\n", &save); s != NULL; s = strtok_r(NULL, "\n", &save)) {
            char entry[256];
            if (strchr(s, ':')) {
                snprintf(entry, sizeof(entry),
                        "<tt:DNSManual><tt:Type>IPv6</tt:Type><tt:IPv6Address>%s</tt:IPv6Address></tt:DNSManual>",
                        s);
            } else {
                snprintf(entry, sizeof(entry),
                        "<tt:DNSManual><tt:Type>IPv4</tt:Type><tt:IPv4Address>%s</tt:IPv4Address></tt:DNSManual>",
                        s);
            }
            strncat(dns_block, entry, sizeof(dns_block) - strlen(dns_block) - 1);
        }
    }

    long size = cat(NULL, "device_service_files/GetDNS.xml", 2,
            "%DNS_BLOCK%", dns_block);
    output_http_headers(size);
    return cat("stdout", "device_service_files/GetDNS.xml", 2,
            "%DNS_BLOCK%", dns_block);
}

int device_get_ntp()
{
    /* GetNTP returns tt:NTPInformation, whose NTPManual element is an
     * unbounded list: report every configured NTP server, one entry each.
     * NetworkHost Type reflects whether the entry is an IPv4/IPv6 literal
     * or a DNS name. */
    char ntp_list[512];
    char ntp_block[2048] = "";
    if (get_ntp_server(ntp_list, sizeof(ntp_list)) > 0) {
        char *save = NULL;
        char *s;
        struct in_addr v4;
        struct in6_addr v6;
        for (s = strtok_r(ntp_list, "\n", &save); s != NULL; s = strtok_r(NULL, "\n", &save)) {
            char entry[320];
            if (inet_pton(AF_INET, s, &v4) == 1) {
                snprintf(entry, sizeof(entry),
                        "<tt:NTPManual><tt:Type>IPv4</tt:Type><tt:IPv4Address>%s</tt:IPv4Address></tt:NTPManual>",
                        s);
            } else if (inet_pton(AF_INET6, s, &v6) == 1) {
                snprintf(entry, sizeof(entry),
                        "<tt:NTPManual><tt:Type>IPv6</tt:Type><tt:IPv6Address>%s</tt:IPv6Address></tt:NTPManual>",
                        s);
            } else {
                snprintf(entry, sizeof(entry),
                        "<tt:NTPManual><tt:Type>DNS</tt:Type><tt:DNSname>%s</tt:DNSname></tt:NTPManual>",
                        s);
            }
            strncat(ntp_block, entry, sizeof(ntp_block) - strlen(ntp_block) - 1);
        }
    }

    long size = cat(NULL, "device_service_files/GetNTP.xml", 2,
            "%NTP_BLOCK%", ntp_block);
    output_http_headers(size);
    return cat("stdout", "device_service_files/GetNTP.xml", 2,
            "%NTP_BLOCK%", ntp_block);
}

int device_unsupported(const char *method)
{
    /* An unimplemented action must return a SOAP fault, not an empty 200
     * response (ONVIF: env:Receiver / ter:ActionNotSupported). */
    (void) method;
    return send_action_not_supported_fault("device_service");
}
