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

#ifndef EZXML_WRAPPER_H
#define EZXML_WRAPPER_H

#include "ezxml/ezxml.h"

int init_xml(char *buffer, int buffer_size);
void close_xml();
const char *get_method(int skip_prefix);
const char *get_element(char *name, char *first_node);
ezxml_t get_element_ptr(ezxml_t start_from, char *name, char *first_node);
const char *get_attribute(ezxml_t node, char *name);

#endif //EZXML_WRAPPER_H
