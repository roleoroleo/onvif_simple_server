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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analytics_service.h"
#include "analytics_desc.h"
#include "fault.h"
#include "utils.h"
#include "log.h"
#include "ezxml_wrapper.h"
#include "onvif_simple_server.h"

extern service_context_t service_ctx;

#define TT_NAMESPACE "http://www.onvif.org/ver10/schema"
#define MAX_SAMPLE_FRAME (64 * 1024)
#define MAX_FRAME_NS 32
#define MAX_FRAME_DEPTH 32
#define MAX_FRAME_TAGS 2048
#define MAX_FRAME_ATTRS 64

void analytics_capabilities(char *buf, size_t size)
{
    snprintf(buf, size,
            "RuleSupport=\"%s\" AnalyticsModuleSupport=\"true\" CellBasedSceneDescriptionSupported=\"false\" "
            "RuleOptionsSupported=\"false\" AnalyticsModuleOptionsSupported=\"false\" SupportedMetadata=\"true\"",
            ANALYTICS_RULE_SUPPORT ? "true" : "false");
}

int analytics_use_count()
{
    int i, n = 0;

    for (i = 0; i < service_ctx.profiles_num; i++) {
        if (service_ctx.profiles[i].metadata)
            n++;
    }
    return n;
}

/* The module namespace declaration, or "" without modules */
static void ns_decl(char *buf, size_t size, const analytics_desc_t *desc)
{
    char uri[MAX_LEN];

    buf[0] = '\0';
    if ((desc->modules_num > 0) && (desc->ns_prefix != NULL)) {
        xml_text(uri, sizeof(uri), desc->ns_uri);
        snprintf(buf, size, " xmlns:%s=\"%s\"", desc->ns_prefix, uri);
    }
}

/* A module or rule instance (tt:Config) as element */
static long render_config(char *dest, const char *element, const analytics_config_t *config)
{
    char value[MAX_LEN];
    long size;
    int i;

    size = cat(dest, "analytics_service_files/Config_start.xml", 6,
            "%ELEMENT%", element,
            "%NAME%", config->name,
            "%TYPE%", config->type);
    for (i = 0; i < config->params_num; i++) {
        xml_text(value, sizeof(value), config->params[i].value);
        size += cat(dest, "analytics_service_files/Config_item.xml", 4,
                "%NAME%", config->params[i].name,
                "%VALUE%", value);
    }
    size += cat(dest, "analytics_service_files/Config_end.xml", 2,
            "%ELEMENT%", element);

    return size;
}

long analytics_render_configuration(char *dest, const char *element, const analytics_desc_t *desc)
{
    char use_count[16];
    char nsd[MAX_LEN + 64];
    long size;
    int i;

    snprintf(use_count, sizeof(use_count), "%d", analytics_use_count());
    ns_decl(nsd, sizeof(nsd), desc);

    size = cat(dest, "analytics_service_files/VideoAnalyticsConfiguration_start.xml", 6,
            "%ELEMENT%", element,
            "%NS_DECL%", nsd,
            "%USE_COUNT%", use_count);
    for (i = 0; i < desc->modules_num; i++)
        size += render_config(dest, "tt:AnalyticsModule", &desc->modules[i].config);
    size += cat(dest, "analytics_service_files/VideoAnalyticsConfiguration_middle.xml", 0);
    for (i = 0; i < desc->rules_num; i++)
        size += render_config(dest, "tt:Rule", &desc->rules[i]);
    size += cat(dest, "analytics_service_files/VideoAnalyticsConfiguration_end.xml", 2,
            "%ELEMENT%", element);

    return size;
}

/* Index of the first module with the type of module i */
static int first_of_type(const analytics_desc_t *desc, int i)
{
    int j;

    for (j = 0; j < i; j++) {
        if (strcmp(desc->modules[j].config.type, desc->modules[i].config.type) == 0)
            break;
    }
    return j;
}

static int check_configuration_token()
{
    const char *token = get_element("ConfigurationToken", "Body");

    if ((token == NULL) || (strcasecmp(token, ANALYTICS_CONFIG_TOKEN) != 0)) {
        send_fault("analytics_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The requested configuration indicated does not exist");
        return -1;
    }
    return 0;
}

int analytics_get_service_capabilities()
{
    char capabilities[MAX_LEN];

    analytics_capabilities(capabilities, sizeof(capabilities));

    long size = cat(NULL, "analytics_service_files/GetServiceCapabilities.xml", 2,
            "%CAPABILITIES%", capabilities);

    output_http_headers(size);

    return cat("stdout", "analytics_service_files/GetServiceCapabilities.xml", 2,
            "%CAPABILITIES%", capabilities);
}

static long render_module_description(char *dest, const analytics_desc_t *desc, int i)
{
    const analytics_module_t *m = &desc->modules[i];
    const event_t *e;
    const char *source_type, *data_name, *data_type;
    char max_instances[16];
    long size;
    int j, n = 0;

    for (j = i; j < desc->modules_num; j++) {
        if (strcmp(desc->modules[j].config.type, m->config.type) == 0)
            n++;
    }
    snprintf(max_instances, sizeof(max_instances), "%d", n);

    size = cat(dest, "analytics_service_files/ModuleDescription_start.xml", 4,
            "%TYPE%", m->config.type,
            "%MAX_INSTANCES%", max_instances);
    for (j = 0; j < m->config.params_num; j++) {
        size += cat(dest, "analytics_service_files/ModuleDescription_parameter.xml", 4,
                "%NAME%", m->config.params[j].name,
                "%TYPE%", m->config.params[j].type);
    }
    size += cat(dest, "analytics_service_files/ModuleDescription_parameters_end.xml", 0);
    for (j = 0; j < m->messages_num; j++) {
        e = &service_ctx.events[m->messages[j]];
        get_event_description(e, &source_type, &data_name, &data_type);
        size += cat(dest, "analytics_service_files/ModuleDescription_message.xml", 10,
                "%SOURCE_NAME%", e->source_name,
                "%SOURCE_TYPE%", source_type,
                "%DATA_NAME%", data_name,
                "%DATA_TYPE%", data_type,
                "%TOPIC%", e->topic);
    }
    size += cat(dest, "analytics_service_files/ModuleDescription_end.xml", 0);

    return size;
}

int analytics_get_supported_analytics_modules()
{
    analytics_desc_t desc;
    char nsd[MAX_LEN + 64];
    char dest_a[] = "stdout";
    char *dest;
    long size = 0;
    int c, i;

    if (check_configuration_token() != 0)
        return -1;

    analytics_desc_load(&desc, service_ctx.analytics_file);
    ns_decl(nsd, sizeof(nsd), &desc);

    // We need 1st step to evaluate content length
    for (c = 0; c < 2; c++) {
        if (c == 0) {
            dest = NULL;
        } else {
            dest = dest_a;
            output_http_headers(size);
        }

        size = cat(dest, "analytics_service_files/Response_header.xml", 4,
                "%METHOD%", "GetSupportedAnalyticsModules",
                "%NS_DECL%", nsd);
        size += cat(dest, "analytics_service_files/SupportedAnalyticsModules_start.xml", 0);
        for (i = 0; i < desc.modules_num; i++) {
            if (first_of_type(&desc, i) == i)
                size += render_module_description(dest, &desc, i);
        }
        size += cat(dest, "analytics_service_files/SupportedAnalyticsModules_end.xml", 0);
        size += cat(dest, "analytics_service_files/Response_footer.xml", 2,
                "%METHOD%", "GetSupportedAnalyticsModules");
    }

    analytics_desc_free(&desc);

    return size;
}

int analytics_get_analytics_modules()
{
    analytics_desc_t desc;
    char nsd[MAX_LEN + 64];
    char dest_a[] = "stdout";
    char *dest;
    long size = 0;
    int c, i;

    if (check_configuration_token() != 0)
        return -1;

    analytics_desc_load(&desc, service_ctx.analytics_file);
    ns_decl(nsd, sizeof(nsd), &desc);

    // We need 1st step to evaluate content length
    for (c = 0; c < 2; c++) {
        if (c == 0) {
            dest = NULL;
        } else {
            dest = dest_a;
            output_http_headers(size);
        }

        size = cat(dest, "analytics_service_files/Response_header.xml", 4,
                "%METHOD%", "GetAnalyticsModules",
                "%NS_DECL%", nsd);
        for (i = 0; i < desc.modules_num; i++)
            size += render_config(dest, "tan:AnalyticsModule", &desc.modules[i].config);
        size += cat(dest, "analytics_service_files/Response_footer.xml", 2,
                "%METHOD%", "GetAnalyticsModules");
    }

    analytics_desc_free(&desc);

    return size;
}

typedef struct {
    const char *prefix;
    const char *uri;
} frame_ns_t;

static int is_qname(const char *name)
{
    const char *c = strchr(name, ':');

    if (c == NULL)
        return is_ncname(name, strlen(name));
    return is_ncname(name, c - name) && is_ncname(c + 1, strlen(c + 1));
}

static int is_ns_declaration(const char *name)
{
    return (strcmp(name, "xmlns") == 0) || (strncmp(name, "xmlns:", 6) == 0);
}

static const char *local_name(const char *name)
{
    const char *c = strchr(name, ':');

    return (c == NULL) ? name : c + 1;
}

/* Namespace URI of a name: "" without prefix, NULL if the prefix is undeclared */
static const char *name_namespace(const char *name, const frame_ns_t *ns, int ns_num)
{
    const char *c = strchr(name, ':');
    size_t n;
    int i;

    if (c == NULL)
        return "";
    n = c - name;
    if ((n == 3) && (strncmp(name, "xml", 3) == 0))
        return "http://www.w3.org/XML/1998/namespace";
    for (i = ns_num - 1; i >= 0; i--) {
        if ((strlen(ns[i].prefix) == n) && (strncmp(ns[i].prefix, name, n) == 0))
            return ns[i].uri;
    }
    return NULL;
}

/* The sample frame is embedded in the response. ezxml accepts and writes back
 * some malformed input, so check every name, text and attribute here. The
 * frame must also declare every prefix it uses itself. */
static int frame_is_valid(ezxml_t x, frame_ns_t *ns, int ns_num, int depth)
{
    ezxml_t child;
    const char *uri;
    int i, j;

    if ((depth > MAX_FRAME_DEPTH) || !is_qname(x->name) || !is_xml_chars(x->txt))
        return 0;
    for (i = 0; x->attr[i] != NULL; i += 2) {
        if ((i / 2 >= MAX_FRAME_ATTRS) || !is_qname(x->attr[i]) || !is_xml_chars(x->attr[i + 1]))
            return 0;
        for (j = 0; j < i; j += 2) {
            if (strcmp(x->attr[i], x->attr[j]) == 0)
                return 0;
        }
        if (is_ns_declaration(x->attr[i]) && is_reserved_namespace(x->attr[i + 1]))
            return 0;
        if (strncmp(x->attr[i], "xmlns:", 6) == 0) {
            // xml... prefixes are reserved, and a prefix cannot be undeclared
            if ((ns_num == MAX_FRAME_NS) || (strncasecmp(x->attr[i] + 6, "xml", 3) == 0) ||
                    (x->attr[i + 1][0] == '\0'))
                return 0;
            ns[ns_num].prefix = x->attr[i] + 6;
            ns[ns_num].uri = x->attr[i + 1];
            ns_num++;
        }
    }
    if (name_namespace(x->name, ns, ns_num) == NULL)
        return 0;
    // Attribute names must also differ after namespace expansion
    for (i = 0; x->attr[i] != NULL; i += 2) {
        if (is_ns_declaration(x->attr[i]))
            continue;
        uri = name_namespace(x->attr[i], ns, ns_num);
        if (uri == NULL)
            return 0;
        for (j = 0; j < i; j += 2) {
            if (!is_ns_declaration(x->attr[j]) &&
                    (strcmp(uri, name_namespace(x->attr[j], ns, ns_num)) == 0) &&
                    (strcmp(local_name(x->attr[i]), local_name(x->attr[j])) == 0))
                return 0;
        }
    }
    for (child = x->child; child != NULL; child = child->ordered) {
        if (!frame_is_valid(child, ns, ns_num, depth + 1))
            return 0;
    }
    return 1;
}

/* ezxml decodes "&#-1;" or a huge number into a NUL or wrong bytes, which
 * breaks its own buffer handling, and knows only the predefined entities. */
static int frame_references_valid(const char *buf)
{
    const char *p, *d;
    unsigned long c;
    size_t n;
    int hex;

    for (p = strchr(buf, '&'); p != NULL; p = strchr(p + 1, '&')) {
        if (p[1] == '#') {
            hex = (p[2] == 'x');
            d = p + 2 + hex;
            n = strspn(d, hex ? "0123456789abcdefABCDEF" : "0123456789");
            if ((n == 0) || (n > 8) || (d[n] != ';'))
                return 0;
            c = strtoul(d, NULL, hex ? 16 : 10);
            if (!((c == 0x9) || (c == 0xa) || (c == 0xd) || ((c >= 0x20) && (c <= 0xd7ff)) ||
                    ((c >= 0xe000) && (c <= 0xfffd)) || ((c >= 0x10000) && (c <= 0x10ffff))))
                return 0;
        } else if ((strncmp(p, "&lt;", 4) != 0) && (strncmp(p, "&gt;", 4) != 0) &&
                (strncmp(p, "&amp;", 5) != 0) && (strncmp(p, "&apos;", 6) != 0) &&
                (strncmp(p, "&quot;", 6) != 0)) {
            return 0;
        }
    }
    return 1;
}

/* Text checks before parsing. ezxml takes a leading 0xFE or 0xFF byte for
 * UTF-16, writes processing instructions back outside the root element, and
 * recurses once per element when it frees the tree. */
static int frame_text_is_valid(const char *buf)
{
    const char *p, *end;
    int tags = 0;

    if (!is_xml_chars(buf) || !frame_references_valid(buf) ||
            (strstr(buf, "<!DOCTYPE") != NULL) || (strstr(buf, "<!ENTITY") != NULL))
        return 0;
    for (p = buf; *p != '\0'; p++) {
        if (*p == '<')
            tags++;
    }
    if (tags > MAX_FRAME_TAGS)
        return 0;

    // Only an XML declaration, before the first element
    p = strstr(buf, "<?");
    if (p == NULL)
        return 1;
    if ((strchr(buf, '<') != p) || (strncmp(p, "<?xml", 5) != 0) || !isspace((unsigned char) p[5]))
        return 0;
    end = strstr(p, "?>");
    return (end != NULL) && (strstr(end, "<?") == NULL);
}

static int is_tt_frame(ezxml_t x)
{
    const char *c = strchr(x->name, ':');
    const char *uri;
    char attr[64];

    if ((c == NULL) || (strcmp(c + 1, "Frame") != 0) || (c - x->name > 32))
        return 0;
    snprintf(attr, sizeof(attr), "xmlns:%.*s", (int) (c - x->name), x->name);
    uri = ezxml_attr(x, attr);
    return (uri != NULL) && (strcmp(uri, TT_NAMESPACE) == 0);
}

/* Read a sample frame file (root tt:Frame) and return it as tan:SampleFrame,
 * which has the type tt:Frame. Returns a malloc'ed string or NULL. */
static char *load_sample_frame(const char *path)
{
    frame_ns_t ns[MAX_FRAME_NS];
    const char *utc;
    char *buf, *r, *w, *s = NULL;
    size_t len;
    ezxml_t x;

    if (path == NULL) {
        log_error("Analytics module without sample frame file");
        return NULL;
    }
    buf = read_file(path, MAX_SAMPLE_FRAME, &len);
    if (buf == NULL) {
        log_error("Unable to read sample frame file %s (regular file of at most %d bytes)", path, MAX_SAMPLE_FRAME);
        return NULL;
    }
    if ((len == 0) || (strlen(buf) != len) || !frame_text_is_valid(buf)) {
        log_error("Sample frame file %s is empty or not plain XML", path);
        free(buf);
        return NULL;
    }

    // XML line end handling, else the parser keeps CR characters
    for (r = w = buf; *r != '\0'; r++) {
        if (*r == '\r') {
            *w++ = '\n';
            if (r[1] == '\n')
                r++;
        } else {
            *w++ = *r;
        }
    }
    *w = '\0';

    // ezxml_parse_str works on (and changes) buf
    x = ezxml_parse_str(buf, w - buf);
    if (x == NULL) {
        free(buf);
        return NULL;
    }
    // The renamed root must stay in the tan namespace of the response
    utc = ezxml_attr(x, "UtcTime");
    if ((ezxml_error(x)[0] != '\0') || !is_tt_frame(x) || (utc == NULL) || (utc[0] == '\0') ||
            (ezxml_attr(x, "xmlns:tan") != NULL) || !frame_is_valid(x, ns, 0, 0)) {
        log_error("Sample frame file %s is not a valid tt:Frame", path);
    } else {
        x->name = "tan:SampleFrame";
        s = ezxml_toxml(x);
    }
    ezxml_free(x);
    free(buf);

    return s;
}

/* Namespace URI bound to the prefix (length n) at node, from the request */
static const char *request_namespace(ezxml_t node, const char *prefix, size_t n)
{
    const char *uri;
    char attr[64];

    if (n > 32)
        return NULL;
    snprintf(attr, sizeof(attr), "xmlns:%.*s", (int) n, prefix);
    for (; node != NULL; node = node->parent) {
        uri = ezxml_attr(node, attr);
        if (uri != NULL)
            return uri;
    }
    return NULL;
}

/* Compare the requested QName with a module type by namespace URI and local
 * name, so the client may use its own prefix. */
static int type_matches(const char *requested, ezxml_t node, const analytics_desc_t *desc, const char *type)
{
    const char *c = strchr(requested, ':');
    const char *uri;

    if (c != NULL) {
        uri = request_namespace(node, requested, c - requested);
        if (uri != NULL)
            return (strcmp(uri, desc->ns_uri) == 0) &&
                    (strcmp(c + 1, type + strlen(desc->ns_prefix) + 1) == 0);
    }
    return strcmp(requested, type) == 0;
}

int analytics_get_supported_metadata()
{
    analytics_desc_t desc;
    char *frames[MAX_ANALYTICS_MODULES];
    char nsd[MAX_LEN + 64];
    char dest_a[] = "stdout";
    char *dest;
    const char *type = get_element("Type", "Body");
    ezxml_t type_node = get_element_ptr(NULL, "Type", "Body");
    long size = 0;
    int c, i, found = 0;

    analytics_desc_load(&desc, service_ctx.analytics_file);
    for (i = 0; i < desc.modules_num; i++) {
        frames[i] = NULL;
        if (first_of_type(&desc, i) != i)
            continue;
        if ((type != NULL) && !type_matches(type, type_node, &desc, desc.modules[i].config.type))
            continue;
        found = 1;
        frames[i] = load_sample_frame(desc.modules[i].sample_frame_file);
    }

    if ((type != NULL) && !found) {
        analytics_desc_free(&desc);
        send_fault("analytics_service", "Sender", "ter:InvalidArgVal", "ter:TypeNotExistent", "Type not existent", "The requested analytics module type does not exist");
        return -1;
    }

    ns_decl(nsd, sizeof(nsd), &desc);

    // We need 1st step to evaluate content length
    for (c = 0; c < 2; c++) {
        if (c == 0) {
            dest = NULL;
        } else {
            dest = dest_a;
            output_http_headers(size);
        }

        size = cat(dest, "analytics_service_files/Response_header.xml", 4,
                "%METHOD%", "GetSupportedMetadata",
                "%NS_DECL%", nsd);
        for (i = 0; i < desc.modules_num; i++) {
            if (frames[i] == NULL)
                continue;
            size += cat(dest, "analytics_service_files/MetadataInfo_start.xml", 2,
                    "%TYPE%", desc.modules[i].config.type);
            if (dest != NULL)
                fputs(frames[i], stdout);
            size += strlen(frames[i]);
            size += cat(dest, "analytics_service_files/MetadataInfo_end.xml", 0);
        }
        size += cat(dest, "analytics_service_files/Response_footer.xml", 2,
                "%METHOD%", "GetSupportedMetadata");
    }

    for (i = 0; i < desc.modules_num; i++)
        free(frames[i]);
    analytics_desc_free(&desc);

    return size;
}

static int send_fixed_modules_fault()
{
    if (check_configuration_token() != 0)
        return -1;

    send_fault("analytics_service", "Sender", "ter:Action", "ter:FixedModules", "Fixed modules", "The analytics modules are fixed and cannot be created or deleted");
    return -2;
}

int analytics_create_analytics_modules()
{
    return send_fixed_modules_fault();
}

int analytics_delete_analytics_modules()
{
    return send_fixed_modules_fault();
}

int analytics_unsupported(const char *method)
{
    (void) method;
    return send_action_not_supported_fault("analytics_service");
}

/* Like send_empty_response(), but generic_files/Empty.xml does not declare tan */
int analytics_empty_response(const char *method)
{
    // The method name goes into the reply as an element name
    if (!is_ncname(method, strlen(method)))
        return analytics_unsupported(method);

    long size = cat(NULL, "analytics_service_files/Empty.xml", 2,
            "%METHOD%", method);

    output_http_headers(size);

    return cat("stdout", "analytics_service_files/Empty.xml", 2,
            "%METHOD%", method);
}
