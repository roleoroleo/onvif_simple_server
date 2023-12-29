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
#include <time.h>
#include <errno.h>
#include <limits.h>

#include "events_service.h"
#include "fault.h"
#include "utils.h"
#include "log.h"
#include "ezxml_wrapper.h"
#include "onvif_simple_server.h"

extern service_context_t service_ctx;

shm_t *subs_evts;

int events_get_service_capabilities()
{
    long size = cat(NULL, "events_service_files/GetServiceCapabilities.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "events_service_files/GetServiceCapabilities.xml", 0);
}

int events_create_pull_point_subscription()
{
    const char *element;
    const char *itt;
    time_t now;
    char iso_str[21];
    char iso_str_2[21];
    int i, subsel;

    char my_address[16];
    char my_netmask[16];
    char my_port[8];
    char events_service_address[MAX_LEN];

    log_info("CreatePullPointSubscription received");

    get_ip_address(my_address, my_netmask, service_ctx.ifs);
    my_port[0] = '\0';
    if (service_ctx.port != 80)
        sprintf(my_port, ":%d", service_ctx.port);

    // Filter is not supported at the moment
    element = get_element("Filter", "Body");
    if (element != NULL) {
        element = get_element("TopicExpression", "Body");
//        if ((element != NULL) && (strstr(element, "VideoSource/MotionAlarm") == NULL) && (strlen(element) > 0)) {
//            log_error("Invalid filter");
//            send_fault("events_service", "Receiver", "wsrf-rw:InvalidFilterFault", "wsrf-rw:ResourceUnknownFault", "Invalid filter", "");
//            return -2;
//        }
        element = get_element("MessageContent", "Body");
//        if ((element != NULL) && (strlen(element) > 0)) {
//            log_error("Invalid filter");
//            send_fault("events_service", "Receiver", "wsrf-rw:InvalidFilterFault", "", "Invalid filter", "");
//            return -2;
//        }
    }

    itt = get_element("InitialTerminationTime", "Body");
    if (itt == NULL) {
        log_error("No InitialTerminationTime element for Subscribe method");
        send_fault("events_service", "Receiver", "wsntw:UnacceptableInitialTerminationTimeFault", "wsntw:UnacceptableInitialTerminationTimeFault", "Unacceptable initial termination time", "");
        return -3;
    }

    time(&now);
    subs_evts = (shm_t *) create_shared_memory(0);
    if (subs_evts == NULL) {
        log_error("No shared memory found, is onvif_notify_server running?");
        send_fault("events_service", "Receiver", "wsntw:UnableToCreatePullPointFault", "wsntw:UnableToCreatePullPointFault", "Unable to create pull point", "");
        return -4;
    }
    sem_memory_wait();
    for (i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (subs_evts->subscriptions[i].used == SUB_UNUSED) {
            memset(subs_evts->subscriptions[i].reference, '\0', CONSUMER_REFERENCE_MAX_SIZE);
            subs_evts->subscriptions[i].used = SUB_PULL;
            subs_evts->subscriptions[i].expire = now + interval2sec(itt);
            break;
        }
    }
    sem_memory_post();
    destroy_shared_memory((void *) subs_evts, 0);

    subsel = i + 1;
    sprintf(events_service_address, "http://%s%s/onvif/events_service?sub=%d", my_address, my_port, subsel);

    to_iso_date(iso_str, sizeof(iso_str), now);
    now += interval2sec(itt);
    to_iso_date(iso_str_2, sizeof(iso_str_2), now);

    log_debug("Subscription data: expire %s - subscription index %d", iso_str_2, subsel);

    long size = cat(NULL, "events_service_files/CreatePullPointSubscription.xml", 6,
        "%ADDRESS%", events_service_address,
        "%CURRENT_TIME%", iso_str,
        "%TERMINATION_TIME%", iso_str_2);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "events_service_files/CreatePullPointSubscription.xml", 6,
        "%ADDRESS%", events_service_address,
        "%CURRENT_TIME%", iso_str,
        "%TERMINATION_TIME%", iso_str_2);
}

int events_pull_messages()
{
    const char *timeout;
    const char *message_limit;
    int limit, count;
    time_t now, now_p_timeout, expire_time;
    char iso_str[21];
    char iso_str_2[21];
    char iso_str_3[21];

    int qs_size;
    char *qs_string;
    char *sub_index_s;
    int sub_index;

    int at_least_one_message;
    int i, c;
    char *endptr;
    char value[8];
    char dest_a[] = "stdout";
    char *dest;
    long size, total_size;

    // Subscription manager replies to address http://%s%s/onvif/events_service?sub=%d
    log_info("PullMessages request received");

    get_from_query_string(&qs_string, &qs_size, "sub");
    if ((qs_size == -1) || (qs_string == NULL)) {
        log_error("No sub parameter in query string for PullMessages method");
        send_fault("events_service", "Receiver", "wsrf-rw:ResourceUnknownFault", "wsrf-rw:ResourceUnknownFault", "Resource unknown", "");
        return -1;
    }
    sub_index_s = (char *) malloc((qs_size + 1) * sizeof(char));
    memset(sub_index_s, '\0', qs_size + 1);
    strncpy(sub_index_s, qs_string, qs_size);
    sub_index = atoi(sub_index_s);
    free(sub_index_s);
    if ((sub_index <= 0) || (sub_index > MAX_SUBSCRIPTIONS)) {
        log_error("sub index out of range for PullMessages method");
        send_fault("events_service", "Receiver", "wsrf-rw:ResourceUnknownFault", "wsrf-rw:ResourceUnknownFault", "Resource unknown", "");
        return -2;
    }
    sub_index--;

    subs_evts = (shm_t *) create_shared_memory(0);
    if (subs_evts == NULL) {
        log_error("No shared memory found, is onvif_notify_server running?");
        send_action_failed_fault(-3);
        return -3;
    }
    sem_memory_wait();
    if (subs_evts->subscriptions[sub_index].used != SUB_PULL) {
        sem_memory_post();
        destroy_shared_memory((void *) subs_evts, 0);
        send_fault("events_service", "Receiver", "wsrf-rw:ResourceUnknownFault", "wsrf-rw:ResourceUnknownFault", "Resource unknown", "");
        return -4;
    }
//    subs_evts->subscriptions[sub_index].used = SUB_PUSH;
//    subs_evts->subscriptions[sub_index].expire = now + interval2sec(tt);
    expire_time = subs_evts->subscriptions[sub_index].expire;
    sem_memory_post();

    timeout = get_element("Timeout", "Body");
    if (timeout == NULL) {
        log_error("No Timeout element for PullMessages method");
        destroy_shared_memory((void *) subs_evts, 0);
        send_action_failed_fault(-5);
        return -5;
    }
    time(&now);
    to_iso_date(iso_str, sizeof(iso_str), now);
    to_iso_date(iso_str_2, sizeof(iso_str_2), expire_time);
    now_p_timeout = now + interval2sec(timeout);

    message_limit = get_element("MessageLimit", "Body");
    if (message_limit == NULL) {
        log_error("No MessageLimit element for PullMessages method");
        destroy_shared_memory((void *) subs_evts, 0);
        send_action_failed_fault(-6);
        return -6;
    }

    limit = strtol(message_limit, &endptr, 10);
    /* Check for various possible errors */
    if ((errno == ERANGE && (limit == LONG_MAX || limit == LONG_MIN)) || (errno != 0 && limit == 0)) {
        log_error("Wrong MessageLimit value for PullMessages method");
        destroy_shared_memory((void *) subs_evts, 0);
        send_action_failed_fault(-7);
        return -7;
    }

    log_debug("Pull message request with timeout %d seconds and message limit %d", interval2sec(timeout), limit);

    // Check if at least 1 message was triggered
    // or SetSynchronizationPoint request is received
    at_least_one_message = 0;
    while ((at_least_one_message == 0) && (now <= now_p_timeout)) {
        for (i = 0; i < service_ctx.events_num; i++) {
            sem_memory_wait();
            if (subs_evts->events[i].pull_notify & (1 << sub_index)) {
                at_least_one_message = 1;
                sem_memory_post();
                break;
            }
            sem_memory_post();
        }
        if (at_least_one_message == 1)
            break;
        sleep(1);
        time(&now);
    }


//    if (now > now_p_timeout) {
//        send_pull_messages_fault((char *) timeout, (char *) message_limit);
//        return -8;
//    }

    // We need 1st step to evaluate content length
    sem_memory_wait();
    for (c = 0; c < 2; c++) {
        if (c == 0) {
            dest = NULL;
        } else {
            dest = dest_a;
            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", total_size);
        }

        size = cat(dest, "events_service_files/PullMessages_1.xml", 4,
                    "%CURRENT_TIME%", iso_str,
                    "%TERMINATION_TIME%", iso_str_2);
        if (c == 0) total_size = size;

        count = 0;
        for (i = 0; i < service_ctx.events_num; i++) {
            if (count >= limit)
                break;
            if ((subs_evts->events[i].pull_notify & (1 << sub_index))) {
                if (subs_evts->events[i].is_on) {
                    strcpy(value, "true");
                } else {
                    strcpy(value, "false");
                }
                to_iso_date(iso_str_3, sizeof(iso_str_3), subs_evts->events[i].e_time);

                size = cat(dest, "events_service_files/PullMessages_2.xml", 12,
                    "%TOPIC%", service_ctx.events[i].topic,
                    "%UTC_TIME%", iso_str_3,
                    "%PROPERTY%", "Changed",
                    "%SOURCE_NAME%", service_ctx.events[i].source_name,
                    "%SOURCE_VALUE%", service_ctx.events[i].source_value,
                    "%VALUE%", value);
                if (c == 0) total_size += size;

                if (c == 1) subs_evts->events[i].pull_notify &= ~(1 << sub_index);
            }
            count++;
        }
        sem_memory_post();

        size = cat(dest, "events_service_files/PullMessages_3.xml", 0);
        if (c == 0) total_size += size;
    }
    destroy_shared_memory((void *) subs_evts, 0);

    return total_size;
}

int events_subscribe()
{
    const char *address;
    const char *element;
    const char *itt;
    char msg_uuid[UUID_LEN + 1];
    const char *relates_to_uuid;
    time_t now;
    char iso_str[21];
    char iso_str_2[21];
    int i, subsel;

    char my_address[16];
    char my_netmask[16];
    char my_port[8];
    char events_service_address[MAX_LEN];

    log_info("Subscribe request received");

    get_ip_address(my_address, my_netmask, service_ctx.ifs);
    my_port[0] = '\0';
    if (service_ctx.port != 80)
        sprintf(my_port, ":%d", service_ctx.port);

    address = get_element("Address", "Body");
    if (address == NULL) {
        log_error("No Address element for Subscribe method");
        send_fault("events_service", "Receiver", "wsrf-rw:ResourceUnknownFault", "wsrf-rw:ResourceUnknownFault", "Resource unknown", "");
        return -1;
    }

    // Filter is not supported at the moment
    element = get_element("Filter", "Body");
    if (element != NULL) {
        element = get_element("TopicExpression", "Body");
//        if ((element != NULL) && (strstr(element, "VideoSource/MotionAlarm") == NULL) && (strlen(element) > 0)) {
//            log_error("Invalid filter");
//            send_fault("events_service", "Receiver", "wsrf-rw:InvalidFilterFault", "wsrf-rw:ResourceUnknownFault", "Invalid filter", "");
//            return -2;
//        }
        element = get_element("MessageContent", "Body");
//        if ((element != NULL) && (strlen(element) > 0)) {
//            log_error("Invalid filter");
//            send_fault("events_service", "Receiver", "wsrf-rw:InvalidFilterFault", "", "Invalid filter", "");
//            return -2;
//        }
    }

    itt = get_element("InitialTerminationTime", "Body");
    if (itt == NULL) {
        log_error("No InitialTerminationTime element for Subscribe method");
        send_fault("events_service", "Receiver", "wsntw:UnacceptableInitialTerminationTimeFault", "", "Unacceptable initial termination time", "");
        return -3;
    }

    time(&now);
    subs_evts = (shm_t *) create_shared_memory(0);
    if (subs_evts == NULL) {
        log_error("No shared memory found, is onvif_notify_server running?");
        send_action_failed_fault(-4);
        return -4;
    }
    sem_memory_wait();
    for (i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (subs_evts->subscriptions[i].used == SUB_UNUSED) {
            if (strlen(address) < CONSUMER_REFERENCE_MAX_SIZE) {
                memset(subs_evts->subscriptions[i].reference, '\0', CONSUMER_REFERENCE_MAX_SIZE);
                strcpy(subs_evts->subscriptions[i].reference, address);
                subs_evts->subscriptions[i].used = SUB_PUSH;
                subs_evts->subscriptions[i].expire = now + interval2sec(itt);
            }
            break;
        }
    }
    sem_memory_post();
    destroy_shared_memory((void *) subs_evts, 0);

    subsel = i + 1;
    sprintf(events_service_address, "http://%s%s/onvif/events_service?sub=%d", my_address, my_port, subsel);

    gen_uuid(msg_uuid);

    relates_to_uuid = get_element("MessageID", "Header");
    if (relates_to_uuid == NULL) {
        log_error("No MessageID element for Subscribe method");
        send_action_failed_fault(-5);
        return -5;
    }

    to_iso_date(iso_str, sizeof(iso_str), now);
    now += interval2sec(itt);
    to_iso_date(iso_str_2, sizeof(iso_str_2), now);

    log_debug("Subscription data: reference %s - expire %s - subscription index %d", address, iso_str_2, subsel);

    long size = cat(NULL, "events_service_files/Subscribe.xml", 10,
        "%MSG_UUID%", msg_uuid,
        "%REL_TO_UUID%", relates_to_uuid,
        "%REFERENCE%", events_service_address,
        "%CURRENT_TIME%", iso_str,
        "%TERMINATION_TIME%", iso_str_2);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "events_service_files/Subscribe.xml", 10,
        "%MSG_UUID%", msg_uuid,
        "%REL_TO_UUID%", relates_to_uuid,
        "%REFERENCE%", events_service_address,
        "%CURRENT_TIME%", iso_str,
        "%TERMINATION_TIME%", iso_str_2);
}

int events_renew()
{
    const char *tt;
    char msg_uuid[UUID_LEN + 1];
    const char *relates_to_uuid;
    time_t now;
    char iso_str[21];
    char iso_str_2[21];

    int qs_size;
    char *qs_string;
    char *sub_index_s;
    int sub_index;

    // Subscription manager replies to address http://%s%s/onvif/events_service?sub=%d
    log_info("Renew request received");

    get_from_query_string(&qs_string, &qs_size, "sub");
    if ((qs_size == -1) || (qs_string == NULL)) {
        log_error("No sub parameter in query string for Renew method");
        send_fault("events_service", "Receiver", "wsrf-rw:ResourceUnknownFault", "wsrf-rw:ResourceUnknownFault", "Resource unknown", "");
        return -1;
    }
    sub_index_s = (char *) malloc((qs_size + 1) * sizeof(char));
    memset(sub_index_s, '\0', qs_size + 1);
    strncpy(sub_index_s, qs_string, qs_size);
    sub_index = atoi(sub_index_s);
    free(sub_index_s);
    if ((sub_index <= 0) || (sub_index > MAX_SUBSCRIPTIONS)) {
        log_error("sub index out of range for Renew method");
        send_fault("events_service", "Receiver", "wsrf-rw:ResourceUnknownFault", "wsrf-rw:ResourceUnknownFault", "Resource unknown", "");
        return -2;
    }
    sub_index--;

    tt = get_element("TerminationTime", "Body");
    if (tt == NULL) {
        log_error("No TerminationTime element for Renew method");
        send_fault("events_service", "Receiver", "wsntw:UnacceptableInitialTerminationTimeFault", "", "Unacceptable initial termination time", "");
        return -3;
    }

    time(&now);
    subs_evts = (shm_t *) create_shared_memory(0);
    if (subs_evts == NULL) {
        log_error("No shared memory found, is onvif_notify_server running?");
        send_action_failed_fault(-4);
        return -4;
    }
    sem_memory_wait();
    if (subs_evts->subscriptions[sub_index].used == SUB_UNUSED) {
        sem_memory_post();
        destroy_shared_memory((void *) subs_evts, 0);
        send_fault("events_service", "Receiver", "wsrf-rw:ResourceUnknownFault", "wsrf-rw:ResourceUnknownFault", "Resource unknown", "");
        return -5;
    }
//    subs_evts->subscriptions[sub_index].used = SUB_PUSH;
    subs_evts->subscriptions[sub_index].expire = now + interval2sec(tt);
    sem_memory_post();
    destroy_shared_memory((void *) subs_evts, 0);

    gen_uuid(msg_uuid);
    relates_to_uuid = get_element("MessageID", "Header");
    if (relates_to_uuid == NULL) {
        log_error("No MessageID element for Renew method");
        send_action_failed_fault(-6);
        return -6;
    }

    to_iso_date(iso_str, sizeof(iso_str), now);
    now += interval2sec(tt);
    to_iso_date(iso_str_2, sizeof(iso_str_2), now);

    log_debug("Subscription data: expire %s - subscription index %d", iso_str_2, sub_index);

    long size = cat(NULL, "events_service_files/Renew.xml", 8,
        "%MSG_UUID%", msg_uuid,
        "%REL_TO_UUID%", relates_to_uuid,
        "%CURRENT_TIME%", iso_str,
        "%TERMINATION_TIME%", iso_str_2);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "events_service_files/Renew.xml", 8,
        "%MSG_UUID%", msg_uuid,
        "%REL_TO_UUID%", relates_to_uuid,
        "%CURRENT_TIME%", iso_str,
        "%TERMINATION_TIME%", iso_str_2);
}

int events_get_event_properties()
{
    int c, i, j, ret;
    char topic[1024];
    char topic_ls[3][256];
    char topic_le[3][256];
    char *token;
    long size, total_size;
    char dest_a[] = "stdout";
    char *dest;

    // We need 1st step to evaluate content length
    for (c = 0; c < 2; c++) {
        if (c == 0) {
            dest = NULL;
        } else {
            dest = dest_a;
            fprintf(stdout, "Content-type: application/soap+xml\r\n");
            fprintf(stdout, "Content-Length: %ld\r\n\r\n", total_size);
        }

        size = cat(dest, "events_service_files/GetEventProperties_1.xml", 0);
        if (c == 0) total_size = size;

        for (i = 0; i < service_ctx.events_num; i++) {
            topic_ls[0][0] = '\0';
            topic_ls[1][0] = '\0';
            topic_ls[2][0] = '\0';
            topic_le[0][0] = '\0';
            topic_le[1][0] = '\0';
            topic_le[2][0] = '\0';
            strcpy(topic, service_ctx.events[i].topic);
            token = strtok(topic, "/");

            /* walk through other tokens */
            for (j = 0; j < 3; j++) {
                sprintf(topic_ls[j], "<%s", token);
                sprintf(topic_le[j], "</%s>", token);

                token = strtok(NULL, "/");
                if (token == NULL) {
                    strcat(topic_ls[j], "  wstop:topic=\"true\">");
                    break;
                } else {
                    strcat(topic_ls[j], ">");
                }
            }
            if ((c == 0) && (j == 3) && (token != NULL)) {
                log_error("The topic has too many levels");
                send_action_failed_fault(-1);
                return -1;
            }

            size = cat(dest, "events_service_files/GetEventProperties_2.xml", 14,
                "%TOPIC_L1_START%", topic_ls[0],
                "%TOPIC_L2_START%", topic_ls[1],
                "%TOPIC_L3_START%", topic_ls[2],
                "%SOURCE_NAME%", service_ctx.events[i].source_name,
                "%TOPIC_L3_END%", topic_le[2],
                "%TOPIC_L2_END%", topic_le[1],
                "%TOPIC_L1_END%", topic_le[0]);
            if (c == 0) total_size += size;
        }

        size = cat(dest, "events_service_files/GetEventProperties_3.xml", 0);
        if (c == 0) total_size += size;
    }

    return total_size;
}

int events_unsubscribe()
{
    int qs_size;
    char *qs_string;
    char *sub_index_s;
    int sub_index;

    // Subscription manager replies to address http://%s%s/onvif/events_service?sub=%d
    log_info("Unsubscribe request received");

    get_from_query_string(&qs_string, &qs_size, "sub");
    if ((qs_size == -1) || (qs_string == NULL)) {
        log_error("No sub parameter in query string for Unsubscribe method");
        send_fault("events_service", "Receiver", "wsrf-rw:ResourceUnknownFault", "wsrf-rw:ResourceUnknownFault", "Resource unknown", "");
        return 1;
    }

    sub_index_s = (char *) malloc((qs_size + 1) * sizeof(char));
    memset(sub_index_s, '\0', qs_size + 1);
    strncpy(sub_index_s, qs_string, qs_size);
    sub_index = atoi(sub_index_s);
    free(sub_index_s);
    if ((sub_index <= 0) || (sub_index > MAX_SUBSCRIPTIONS)) {
        log_error("sub index out of range for Unsubscribe method");
        send_fault("events_service", "Receiver", "wsrf-rw:ResourceUnknownFault", "wsrf-rw:ResourceUnknownFault", "Resource unknown", "");
        return -2;
    }
    sub_index--;

    subs_evts = (shm_t *) create_shared_memory(0);
    if (subs_evts == NULL) {
        log_error("No shared memory found, is onvif_notify_server running?");
        send_action_failed_fault(-3);
        return -3;
    }
    sem_memory_wait();
    if (subs_evts->subscriptions[sub_index].used == SUB_UNUSED) {
        sem_memory_post();
        destroy_shared_memory((void *) subs_evts, 0);
        send_fault("events_service", "Receiver", "wsrf-rw:ResourceUnknownFault", "wsrf-rw:ResourceUnknownFault", "Resource unknown", "");
        return -4;
    }
    memset(&(subs_evts->subscriptions[sub_index]), '\0', sizeof (subscription_shm_t));
    sem_memory_post();
    destroy_shared_memory((void *) subs_evts, 0);

    log_debug("Subscription data: subscription index %d", sub_index);

    long size = cat(NULL, "events_service_files/Unsubscribe.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "events_service_files/Unsubscribe.xml", 0);
}

int events_set_synchronization_point()
{
    int i;
    int qs_size;
    char *qs_string;
    char *sub_index_s;
    int sub_index;

    get_from_query_string(&qs_string, &qs_size, "sub");
    if ((qs_size == -1) || (qs_string == NULL)) {
        log_error("No sub parameter in query string for Renew method");
        send_fault("events_service", "Receiver", "wsrf-rw:ResourceUnknownFault", "wsrf-rw:ResourceUnknownFault", "Resource unknown", "");
        return -1;
    }
    sub_index_s = (char *) malloc((qs_size + 1) * sizeof(char));
    memset(sub_index_s, '\0', qs_size + 1);
    strncpy(sub_index_s, qs_string, qs_size);
    sub_index = atoi(sub_index_s);
    free(sub_index_s);
    if ((sub_index <= 0) || (sub_index > MAX_SUBSCRIPTIONS)) {
        log_error("sub index out of range for Renew method");
        send_fault("events_service", "Receiver", "wsrf-rw:ResourceUnknownFault", "wsrf-rw:ResourceUnknownFault", "Resource unknown", "");
        return -2;
    }
    sub_index--;

    log_info("SetSynchronizationPoint request received for subscription %d", sub_index);

    subs_evts = (shm_t *) create_shared_memory(0);
    if (subs_evts == NULL) {
        log_error("No shared memory found, is onvif_notify_server running?");
        send_action_failed_fault(-3);
        return -3;
    }
    sem_memory_wait();
    subs_evts->subscriptions[sub_index].need_sync = 1;

    for (i = 0; i < service_ctx.events_num; i++) {
        subs_evts->events[i].pull_notify |= (1 << sub_index);
    }
    sem_memory_post();
    destroy_shared_memory((void *) subs_evts, 0);

    long size = cat(NULL, "events_service_files/SetSynchronizationPoint.xml", 0);

    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "events_service_files/SetSynchronizationPoint.xml", 0);
}

int events_unsupported(const char *method)
{
    send_action_failed_fault(-1);
    return -1;
}
