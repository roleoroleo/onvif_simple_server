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

#ifndef ANALYTICS_DESC_H
#define ANALYTICS_DESC_H

#include "utils.h"

#define MAX_ANALYTICS_MODULES 16
#define MAX_ANALYTICS_RULES   16
#define MAX_ANALYTICS_PARAMS  16

struct json_object;

typedef struct {
    const char *name;
    const char *type;       /* QName of the value type */
    const char *value;
} analytics_param_t;

/* A module or rule instance (tt:Config) */
typedef struct {
    const char *name;
    const char *type;       /* QName in the description file namespace */
    int params_num;
    analytics_param_t params[MAX_ANALYTICS_PARAMS];
} analytics_config_t;

typedef struct {
    analytics_config_t config;
    const char *sample_frame_file;
    int messages_num;
    int messages[MAX_EVENTS];   /* indexes into service_ctx.events */
} analytics_module_t;

/* The analytics description file. All strings belong to root. */
typedef struct {
    struct json_object *root;
    const char *ns_prefix;
    const char *ns_uri;
    int modules_num;
    analytics_module_t modules[MAX_ANALYTICS_MODULES];
    int rules_num;
    analytics_config_t rules[MAX_ANALYTICS_RULES];
} analytics_desc_t;

/* Load the description file. Invalid entries are skipped with an error in the log.
 * Returns 0, or -1 if the file is unusable (desc then holds no modules).
 * Call analytics_desc_free() in both cases. */
int analytics_desc_load(analytics_desc_t *desc, const char *file);
void analytics_desc_free(analytics_desc_t *desc);

#endif //ANALYTICS_DESC_H
