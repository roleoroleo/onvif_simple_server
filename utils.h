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

#ifndef UTILS_H
#define UTILS_H

#include <stdarg.h>
#include <time.h>
#include <stdint.h>
#include <semaphore.h>

#define MAX_LEN     1024
#define MAX_CAT_LEN 2048

#define UUID_LEN 36

#define MAX_RELAY_OUTPUTS 8
#define MAX_SUBSCRIPTIONS 8 // MAX 32
#define MAX_EVENTS        8 // MAX 32
#define CONSUMER_REFERENCE_MAX_SIZE 256

#define EVENTS_NONE 0
#define EVENTS_PULLPOINT 1        // PullPoint
#define EVENTS_BASESUBSCRIPTION 2 // Base Subscription
#define EVENTS_BOTH 3             // PullPoint and Base Subscription

typedef enum {
    SUB_UNUSED,
    SUB_PULL,
    SUB_PUSH
} subscription_type;

typedef struct {
    int id;
    char reference[CONSUMER_REFERENCE_MAX_SIZE];
    subscription_type used;
    time_t expire;
    int push_need_sync;
    char topic_expression[MAX_LEN];
} subscription_shm_t;

typedef struct {
    time_t e_time;
    int is_on;
    uint32_t pull_send_initialized; // Bit mask: 1 if the value is not known to the client (new subscription) and must be sent
    uint32_t pull_notify; // Bit mask: 1 if the client must be notified
} event_shm_t;

typedef struct {
    subscription_shm_t subscriptions[MAX_SUBSCRIPTIONS];
    event_shm_t events[MAX_EVENTS];
} shm_t;

typedef struct {
    char *topic;
    int match_sub_tree;
} topic_expression_t;

typedef struct {
    topic_expression_t *topics;
    int number;
} topic_expressions_t;

void *create_shared_memory(int create);
void destroy_shared_memory(void *shared_area, int destroy_all);
int sem_memory_wait();
int sem_memory_post();
long cat(char *out, char *filename, int num, ...);
long cat_soap_fault(char* out, const char* fault_subcode, const char* fault_reason, const char* fault_detail);
void output_http_headers(long content_length);

// Global flag to indicate if the last cat() call returned a SOAP fault
extern int g_last_response_was_soap_fault;
int get_ip_address(char *address, char *netmask, char *name);
int get_ipv6_address(const char *ifname, char *ll_addr, int *ll_prefix, char *gl_addr, int *gl_prefix);
int get_mac_address(char *address, char *name);
int netmask2prefixlen(char *netmask);
int get_mtu(char *if_name);
char *trim(char *s);
char *trim_mf(char *s);
int html_escape(char *url, int max_len);
int hashSHA1(char* input, unsigned long inputSize, char *output, int output_size);
void b64_decode(unsigned char *input, unsigned int input_size, unsigned char *output, unsigned long *output_size);
void b64_encode(unsigned char *input, unsigned int input_size, unsigned char *output, unsigned long *output_size);
int interval2sec(const char *interval);
int to_iso_date(char *iso_date, int size, time_t timestamp);
time_t from_iso_date(const char *date);
int gen_uuid(char *g_uuid);
int get_mac_by_ifname(const char *if_name, uint8_t mac_out[6]);
int get_mac_by_ip(const char *ip_str, uint8_t mac_out[6]);
int get_ifname_by_addr(const char *addr_str, char *ifname, size_t len);
int get_mac_by_addr(const char *addr_str, uint8_t mac_out[6]);
int detect_outbound_address_v6(char *addr_str, size_t len, unsigned int *if_idx_out);
int get_ip6_address(char *addr_str, size_t len, const char *if_name);
int detect_local_address(const char *remote_addr, char *local_addr, size_t len);
void gen_uuid_v5_mac(char *uuid_str, const uint8_t mac[6]);
int get_from_query_string(char **ret, int *ret_size, char *par);
int set_video_codec(char *buffer, int buffer_len, int codec, int ver);
int set_audio_codec(char *buffer, int buffer_len, int codec, int ver);
topic_expressions_t *parseTopicExpression(const char *input);
void free_topic_expression(topic_expressions_t *p);
int is_topic_in_expression(const char *topic_expression, char *topic);
void *reboot_thread(void *arg);

#endif //UTILS_H
