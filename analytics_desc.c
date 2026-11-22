/*
 * Copyright (c) 2026 poeggi.
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

#include <json-c/json.h>

#include "analytics_desc.h"
#include "log.h"
#include "onvif_simple_server.h"

extern service_context_t service_ctx;

#define MAX_DESC_FILE  (256 * 1024)

static const char *get_string(json_object *o, const char *key)
{
    json_object *v;

    if (!json_object_object_get_ex(o, key, &v) || !json_object_is_type(v, json_type_string))
        return NULL;
    return json_object_get_string(v);
}

static json_object *get_array(json_object *o, const char *key)
{
    json_object *v;

    if (!json_object_object_get_ex(o, key, &v) || !json_object_is_type(v, json_type_array))
        return NULL;
    return v;
}

/* Name, type and parameters of a module or rule (kind). A rule type may also be an
 * ONVIF type (tt). Returns 0, or -1 if it is unusable. */
static int load_config(analytics_config_t *c, json_object *o, const char *prefix, int allow_tt, const char *kind)
{
    json_object *params, *p;
    const char *name, *type, *value;
    size_t i, j, n;

    c->name = get_string(o, "name");
    c->type = get_string(o, "type");
    c->params_num = 0;
    if (!is_plain(c->name, MAX_DESC_NAME) ||
            !(is_qname_in(c->type, prefix) || (allow_tt && is_qname_in(c->type, "tt")))) {
        log_error("Analytics %s with invalid name or type skipped", kind);
        return -1;
    }

    params = get_array(o, "parameters");
    n = (params != NULL) ? json_object_array_length(params) : 0;
    for (i = 0; i < n; i++) {
        p = json_object_array_get_idx(params, i);
        name = get_string(p, "name");
        type = get_string(p, "type");
        value = get_string(p, "value");
        if (!is_plain(name, MAX_DESC_NAME) || !is_value(value, MAX_DESC_VALUE) ||
                !(is_qname_in(type, "xsd") || is_qname_in(type, "tt") || is_qname_in(type, prefix))) {
            log_error("Invalid parameter of analytics %s %s skipped", kind, c->name);
            continue;
        }
        for (j = 0; j < (size_t) c->params_num; j++) {
            if (strcmp(c->params[j].name, name) == 0)
                break;
        }
        if (j < (size_t) c->params_num) {
            log_error("Duplicate parameter %s of analytics %s %s skipped", name, kind, c->name);
            continue;
        }
        if (c->params_num == MAX_ANALYTICS_PARAMS) {
            log_error("Too many parameters in analytics %s %s", kind, c->name);
            break;
        }
        c->params[c->params_num].name = name;
        c->params[c->params_num].type = type;
        c->params[c->params_num].value = value;
        c->params_num++;
    }

    return 0;
}

/* Returns 1 if topic is a tns1 topic. One child topic may be an extension topic
 * in the description namespace (WS-Topics 6.1), like tns1:RuleEngine/ai:Event.
 * A namespace appears only once in a topic path. */
static int is_topic(const char *topic, const char *prefix)
{
    const char *s, *end, *c;
    int ext = 0;

    if (!is_plain(topic, MAX_DESC_NAME) || (strncmp(topic, "tns1:", 5) != 0))
        return 0;
    for (s = topic + 5; ; s = end + 1) {
        end = s + strcspn(s, "/");
        c = memchr(s, ':', end - s);
        if (c != NULL) {
            if ((s == topic + 5) || ext || ((size_t) (c - s) != strlen(prefix)) ||
                    (strncmp(s, prefix, c - s) != 0))
                return 0;
            ext = 1;
            s = c + 1;
        }
        if (!is_ncname(s, end - s))
            return 0;
        if (*end == '\0')
            return 1;
    }
}

/* Index of the first conf event with a usable description for topic, or -1. */
static int find_event(const char *topic, const char *prefix)
{
    const char *source_type, *data_name, *data_type;
    int i;

    if (!is_topic(topic, prefix)) {
        log_error("Invalid analytics message topic %s", topic != NULL ? topic : "(none)");
        return -1;
    }
    for (i = 0; i < service_ctx.events_num; i++) {
        if ((service_ctx.events[i].topic != NULL) && (strcmp(service_ctx.events[i].topic, topic) == 0))
            break;
    }
    if (i == service_ctx.events_num) {
        log_error("No event configured for analytics message topic %s", topic);
        return -1;
    }

    /* conf checks described events with event_check() */
    get_event_description(&service_ctx.events[i], &source_type, &data_name, &data_type);
    if (!EVENT_IS_DESCRIBED(&service_ctx.events[i]) && (!is_plain(service_ctx.events[i].source_name, MAX_DESC_NAME) ||
            (strcmp(service_ctx.events[i].source_name, data_name) == 0) ||
            !(is_qname_in(source_type, "xsd") || is_qname_in(source_type, "tt")))) {
        log_error("Event %s has no usable source name or type for an analytics message", topic);
        return -1;
    }

    return i;
}

static void load_messages(analytics_module_t *m, json_object *o, const char *prefix)
{
    json_object *messages;
    size_t i, n;
    int e, j;

    messages = get_array(o, "messages");
    n = (messages != NULL) ? json_object_array_length(messages) : 0;
    for (i = 0; i < n; i++) {
        e = find_event(get_string(json_object_array_get_idx(messages, i), "topic"), prefix);
        if (e < 0)
            continue;
        for (j = 0; j < m->messages_num; j++) {
            if (m->messages[j] == e)
                break;
        }
        if ((j < m->messages_num) || (m->messages_num == MAX_EVENTS))
            continue;
        m->messages[m->messages_num++] = e;
    }
}

/* 1 if both configs have the same parameter names and types */
static int same_params(const analytics_config_t *a, const analytics_config_t *b)
{
    int i, j;

    if (a->params_num != b->params_num)
        return 0;
    for (i = 0; i < a->params_num; i++) {
        for (j = 0; j < b->params_num; j++) {
            if (strcmp(a->params[i].name, b->params[j].name) == 0)
                break;
        }
        if ((j == b->params_num) || (strcmp(a->params[i].type, b->params[j].type) != 0))
            return 0;
    }
    return 1;
}

/* Load the modules or rules from the array key into list. Returns their number. */
static int load_list(analytics_module_t *list, int max, const char *key, int is_rule,
        const analytics_desc_t *desc, const char *file)
{
    const char *kind = is_rule ? "rule" : "module";
    json_object *a, *o;
    analytics_module_t *m;
    size_t i, n;
    int j, num = 0;

    a = get_array(desc->root, key);
    n = (a != NULL) ? json_object_array_length(a) : 0;
    for (i = 0; i < n; i++) {
        if (num == max) {
            log_error("Too many analytics %ss in %s", kind, file);
            break;
        }
        o = json_object_array_get_idx(a, i);
        m = &list[num];
        memset(m, 0, sizeof(*m));
        if (load_config(&m->config, o, desc->ns_prefix, is_rule, kind) != 0)
            continue;
        for (j = 0; j < num; j++) {
            if (strcmp(list[j].config.name, m->config.name) == 0)
                break;
        }
        if (j < num) {
            log_error("Duplicate analytics %s name %s skipped", kind, m->config.name);
            continue;
        }
        if (is_rule) {
            // GetSupportedRules describes a rule type once
            for (j = 0; j < num; j++) {
                if (strcmp(list[j].config.type, m->config.type) == 0)
                    break;
            }
            if ((j < num) && !same_params(&list[j].config, &m->config)) {
                log_error("Analytics rule %s skipped: its parameters differ from rule %s of the same type",
                        m->config.name, list[j].config.name);
                continue;
            }
        } else {
            m->sample_frame_file = get_string(o, "sample_frame_file");
        }
        load_messages(m, o, desc->ns_prefix);
        num++;
    }

    return num;
}

int analytics_desc_has_rules(const char *file)
{
    analytics_desc_t desc;
    int ret;

    analytics_desc_load(&desc, file);
    ret = (desc.rules_num > 0);
    analytics_desc_free(&desc);

    return ret;
}

int analytics_desc_load(analytics_desc_t *desc, const char *file)
{
    json_object *ns;
    char *buf;

    memset(desc, 0, sizeof(*desc));
    if (file == NULL) {
        log_error("analytics_file is not set");
        return -1;
    }
    buf = read_file(file, MAX_DESC_FILE, NULL);
    if (buf == NULL) {
        log_error("Unable to read analytics file %s (regular file of at most %d bytes)", file, MAX_DESC_FILE);
        return -1;
    }
    desc->root = json_tokener_parse(buf);
    free(buf);
    if ((desc->root == NULL) || !json_object_is_type(desc->root, json_type_object)) {
        log_error("Unable to read analytics file %s", file);
        return -1;
    }

    if (json_object_object_get_ex(desc->root, "namespace", &ns)) {
        desc->ns_prefix = get_string(ns, "prefix");
        desc->ns_uri = get_string(ns, "uri");
    }
    /* The responses declare these prefixes themselves */
    if ((desc->ns_prefix == NULL) || !is_ncname(desc->ns_prefix, strlen(desc->ns_prefix)) ||
            (strlen(desc->ns_prefix) > 32) || (strncasecmp(desc->ns_prefix, "xml", 3) == 0) ||
            (strcmp(desc->ns_prefix, "tt") == 0) || (strcmp(desc->ns_prefix, "tr2") == 0) ||
            (strcmp(desc->ns_prefix, "tan") == 0) || (strcmp(desc->ns_prefix, "tds") == 0) ||
            (strcmp(desc->ns_prefix, "xsd") == 0) || (strcmp(desc->ns_prefix, "tns1") == 0) ||
            !is_value(desc->ns_uri, MAX_DESC_VALUE) || (desc->ns_uri[0] == '\0') ||
            (strchr(desc->ns_uri, ' ') != NULL) || is_reserved_namespace(desc->ns_uri)) {
        log_error("Invalid namespace in analytics file %s", file);
        desc->ns_prefix = NULL;
        desc->ns_uri = NULL;
        return -1;
    }

    desc->modules_num = load_list(desc->modules, MAX_ANALYTICS_MODULES, "modules", 0, desc, file);
    desc->rules_num = load_list(desc->rules, MAX_ANALYTICS_RULES, "rules", 1, desc, file);

    return 0;
}

void analytics_desc_free(analytics_desc_t *desc)
{
    if (desc->root != NULL)
        json_object_put(desc->root);
    memset(desc, 0, sizeof(*desc));
}
