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

#ifndef ANALYTICS_SERVICE_H
#define ANALYTICS_SERVICE_H

#include <stddef.h>

#include "analytics_desc.h"

/* Every RuleSupport capability reads this flag. */
#define ANALYTICS_RULE_SUPPORT 0

#define METADATA_CONFIG_TOKEN "MetadataConfigToken"
#define ANALYTICS_CONFIG_TOKEN "AnalyticsConfigToken"

int analytics_get_service_capabilities();
int analytics_get_supported_analytics_modules();
int analytics_get_analytics_modules();
int analytics_get_supported_metadata();
int analytics_create_analytics_modules();
int analytics_delete_analytics_modules();

int analytics_unsupported(const char *method);
int analytics_empty_response(const char *method);

/* Attributes of tan:Capabilities */
void analytics_capabilities(char *buf, size_t size);
/* Number of profiles that use the metadata and analytics configurations */
int analytics_use_count();
/* tt:VideoAnalyticsConfiguration as element (tr2:Analytics or tr2:Configurations) */
long analytics_render_configuration(char *dest, const char *element, const analytics_desc_t *desc);

#endif //ANALYTICS_SERVICE_H
