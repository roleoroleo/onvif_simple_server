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

/* Limits keep every substituted template line below MAX_CAT_LEN and every
 * escaped value below MAX_LEN. */
#define MAX_DESC_NAME  256
#define MAX_DESC_VALUE 160
#define MAX_DESC_FILE  (256 * 1024)

/* Text for an attribute or element: cat() rescans substituted text for
 * placeholders, so no '%'; no markup and no whitespace either. */
static int is_plain(const char *s, size_t max)
{
    size_t n;

    if ((s == NULL) || !is_xml_chars(s))
        return 0;
    n = strlen(s);
    if ((n == 0) || (n > max))
        return 0;
    for (; *s != '\0'; s++) {
        unsigned char c = (unsigned char) *s;
        if ((c <= ' ') || (c == 0x7f) || (strchr("%<>&\"'", c) != NULL))
            return 0;
    }
    return 1;
}

/* Like is_plain(), but spaces and markup are allowed; xml_text() escapes it. */
static int is_value(const char *s, size_t max)
{
    size_t n;

    if ((s == NULL) || !is_xml_chars(s))
        return 0;
    n = strlen(s);
    if (n > max)
        return 0;
    for (; *s != '\0'; s++) {
        unsigned char c = (unsigned char) *s;
        if ((c < ' ') || (c == 0x7f) || (c == '%'))
            return 0;
    }
    return 1;
}

/* Returns 1 if qname is "prefix:local" with both parts NCNames and the given prefix. */
static int is_qname_in(const char *qname, const char *prefix)
{
    const char *c;

    if ((qname == NULL) || !is_plain(qname, MAX_DESC_NAME))
        return 0;
    c = strchr(qname, ':');
    if ((c == NULL) || !is_ncname(qname, c - qname) || !is_ncname(c + 1, strlen(c + 1)))
        return 0;
    return ((size_t) (c - qname) == strlen(prefix)) && (strncmp(qname, prefix, c - qname) == 0);
}

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

/* Name, type and parameters of a module. Returns 0, or -1 if it is unusable. */
static int load_config(analytics_config_t *c, json_object *o, const char *prefix)
{
    json_object *params, *p;
    const char *name, *type, *value;
    size_t i, j, n;

    c->name = get_string(o, "name");
    c->type = get_string(o, "type");
    c->params_num = 0;
    if (!is_plain(c->name, MAX_DESC_NAME) || !is_qname_in(c->type, prefix)) {
        log_error("Analytics module with invalid name or type skipped");
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
            log_error("Invalid parameter of analytics module %s skipped", c->name);
            continue;
        }
        for (j = 0; j < (size_t) c->params_num; j++) {
            if (strcmp(c->params[j].name, name) == 0)
                break;
        }
        if (j < (size_t) c->params_num) {
            log_error("Duplicate parameter %s of analytics module %s skipped", name, c->name);
            continue;
        }
        if (c->params_num == MAX_ANALYTICS_PARAMS) {
            log_error("Too many parameters in analytics module %s", c->name);
            break;
        }
        c->params[c->params_num].name = name;
        c->params[c->params_num].type = type;
        c->params[c->params_num].value = value;
        c->params_num++;
    }

    return 0;
}

/* Index of the first conf event with a usable description for topic, or -1. */
static int find_event(const char *topic)
{
    const char *source_type, *data_name, *data_type;
    int i;

    /* The event service declares only tns1 and splits topics at '/' */
    if (!is_plain(topic, MAX_DESC_NAME) || (strncmp(topic, "tns1:", 5) != 0) ||
            (strchr(topic + 5, ':') != NULL)) {
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

    get_event_description(&service_ctx.events[i], &source_type, &data_name, &data_type);
    if (!is_plain(service_ctx.events[i].source_name, MAX_DESC_NAME) ||
            (strcmp(service_ctx.events[i].source_name, data_name) == 0) ||
            !(is_qname_in(source_type, "xsd") || is_qname_in(source_type, "tt"))) {
        log_error("Event %s has no usable source name or type for an analytics message", topic);
        return -1;
    }

    return i;
}

static void load_messages(analytics_module_t *m, json_object *o)
{
    json_object *messages;
    size_t i, n;
    int e, j;

    messages = get_array(o, "messages");
    n = (messages != NULL) ? json_object_array_length(messages) : 0;
    for (i = 0; i < n; i++) {
        e = find_event(get_string(json_object_array_get_idx(messages, i), "topic"));
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

int analytics_desc_load(analytics_desc_t *desc, const char *file)
{
    json_object *ns, *modules, *o;
    analytics_module_t *m;
    char *buf;
    size_t i, n;
    int j;

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

    modules = get_array(desc->root, "modules");
    n = (modules != NULL) ? json_object_array_length(modules) : 0;
    for (i = 0; i < n; i++) {
        if (desc->modules_num == MAX_ANALYTICS_MODULES) {
            log_error("Too many analytics modules in %s", file);
            break;
        }
        o = json_object_array_get_idx(modules, i);
        m = &desc->modules[desc->modules_num];
        memset(m, 0, sizeof(*m));
        if (load_config(&m->config, o, desc->ns_prefix) != 0)
            continue;
        for (j = 0; j < desc->modules_num; j++) {
            if (strcmp(desc->modules[j].config.name, m->config.name) == 0)
                break;
        }
        if (j < desc->modules_num) {
            log_error("Duplicate analytics module name %s skipped", m->config.name);
            continue;
        }
        m->sample_frame_file = get_string(o, "sample_frame_file");
        load_messages(m, o);
        desc->modules_num++;
    }

    return 0;
}

void analytics_desc_free(analytics_desc_t *desc)
{
    if (desc->root != NULL)
        json_object_put(desc->root);
    memset(desc, 0, sizeof(*desc));
}
