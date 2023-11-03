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

#include "string.h"

#include "ezxml_wrapper.h"
#include "utils.h"
#include "log.h"

ezxml_t root_xml;

int init_xml(char *buffer, int buffer_size)
{
    root_xml = ezxml_parse_str(buffer, buffer_size);
}

int init_xml_from_file(char *file)
{
    root_xml = ezxml_parse_file(file);
}

void close_xml()
{
    ezxml_free(root_xml); 
}

const char *get_method(int skip_prefix)
{
    ezxml_t xml;
    char first_node[256];
    int len;
    char *c;

    // Check if "Body" element exists
    strcpy(first_node, "Body");
    xml = root_xml->child;
    while (xml && strcmp(first_node, xml->name)) xml = xml->ordered;
    if ((xml) && (xml->child)) {
        if (skip_prefix) {
            c = strchr(xml->child->name, ':');
            if (c == NULL) {
                return xml->child->name;
            } else {
                c++;
                return c;
            }
        }
    }

    // Check if "something:Body" element exists
    xml = root_xml->child;
    strcpy(first_node, ":Body");
    while (xml) {
        len = strlen(xml->name);
        if (strcmp(first_node, &(xml->name)[len - 5]) == 0) break;
        xml = xml->ordered;
    }
    if ((xml) && (xml->child)) {
        if (skip_prefix) {
            c = strchr(xml->child->name, ':');
            if (c == NULL) {
                return xml->child->name;
            } else {
                c++;
                return c;
            }
        }
    }

    return NULL;
}

int go_to_parent;

const char *get_element_rec(ezxml_t xml, char *name, char *first_node)
{
    const char *ret;
    char name_copy[256];
    ezxml_t px, pk;

    if (go_to_parent) return NULL;

    // If it's the root of the document choose the first node ("Header" or "Body")
    if (xml->parent == NULL) {
        px = xml->child;
        strcpy(name_copy, first_node);
        while (px) {
            if (strcmp(first_node, &(px->name)[strlen(px->name) - strlen(first_node)]) == 0) {
                ret = get_element_rec(px, name, first_node);
                return ret;
            } else {
                px = px->ordered;
            }
        }
        // 1st node not found, exit
    } else {
        // Check if this node is "<name>"
        if (strcmp(name, xml->name) == 0) {
            return xml->txt;
        }

        // Check if this node is "<something:name>"
        name_copy[0] = ':';
        strcpy(&name_copy[1], name);
        if (strcmp(name_copy, &(xml->name)[strlen(xml->name) - strlen(name_copy)]) == 0) {
            return xml->txt;
        }

        // Check children
        if (xml->child) {
            ret = get_element_rec(xml->child, name, first_node);
            go_to_parent = 0;
            if (ret != NULL) return ret;
        }

        // Check brothers
        while(pk = xml->ordered) {
            ret = get_element_rec(pk, name, first_node);
            if (go_to_parent) return NULL;
            if (ret != NULL) return ret;
        }

        go_to_parent = 1;
    }

    return NULL;
}

const char *get_element(char *name, char *first_node)
{
    char *ret;

    go_to_parent = 0;
    ret = (char *) get_element_rec(root_xml, name, first_node);
    if (ret == NULL) return NULL;
    else return trim_mf(ret);
    // Warning: the line above change the xml content
    // ret should be a (const char *)
}

ezxml_t get_element_rec_ptr(ezxml_t xml, char *name, char *first_node)
{
    ezxml_t ret;
    char name_copy[256];
    ezxml_t px, pk;

    if (go_to_parent) return NULL;

    // If it's the root of the document choose the first node ("Header" or "Body")
    if (xml->parent == NULL) {
        px = xml->child;
        strcpy(name_copy, first_node);
        while (px) {
            if (strcmp(first_node, &(px->name)[strlen(px->name) - strlen(first_node)]) == 0) {
                ret = get_element_rec_ptr(px, name, first_node);
                return ret;
            } else {
                px = px->ordered;
            }
        }
        // 1st node not found, exit
    } else {
        // Check if this node is "<name>"
        if (strcmp(name, xml->name) == 0) {
            return xml;
        }

        // Check if this node is "<something:name>"
        name_copy[0] = ':';
        strcpy(&name_copy[1], name);
        if (strcmp(name_copy, &(xml->name)[strlen(xml->name) - strlen(name_copy)]) == 0) {
            return xml;
        }

        // Check children
        if (xml->child) {
            ret = get_element_rec_ptr(xml->child, name, first_node);
            go_to_parent = 0;
            if (ret != NULL) return ret;
        }

        // Check brothers
        while(pk = xml->ordered) {
            ret = get_element_rec_ptr(pk, name, first_node);
            if (go_to_parent) return NULL;
            if (ret != NULL) return ret;
        }

        go_to_parent = 1;
    }

    return NULL;
}

ezxml_t get_element_ptr(ezxml_t start_from, char *name, char *first_node)
{
    ezxml_t ret;

    go_to_parent = 0;
    if (start_from == NULL)
        ret = get_element_rec_ptr(root_xml, name, first_node);
    else
        ret = get_element_rec_ptr(start_from, name, first_node);

    if (ret == NULL) return NULL;
    else return ret;
}

const char *get_attribute(ezxml_t node, char *name)
{
    return ezxml_attr(node, name);
}
