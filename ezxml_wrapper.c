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

#include "string.h"

#include "ezxml_wrapper.h"
#include "utils.h"
#include "log.h"

ezxml_t root_xml;

/**
 * Init xml parser
 * @param buffer The buffer contaning the xml file
 * @param buffer_size The size of the buffer
 */
void init_xml(char *buffer, int buffer_size)
{
    root_xml = ezxml_parse_str(buffer, buffer_size);
}

/**
 * Close xml parser
 */
void close_xml()
{
    ezxml_free(root_xml); 
}

/**
 * Get the method (the 1st element after <Body>)
 * @param skip_prefix 1 to skip the prefix of the element (<prefix:method>)
 * @return A pointer to the name of the method, NULL if not found
 */
const char *get_method(int skip_prefix)
{
    ezxml_t xml;
    char first_node[256];
    int len;
    char *c;

    // Check if "Body" element exists
    snprintf(first_node, sizeof(first_node), "%s", "Body");
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
    snprintf(first_node, sizeof(first_node), "%s", ":Body");
    while (xml) {
        len = strlen(xml->name);
        if ((len >= 5) && (strcmp(first_node, &(xml->name)[len - 5]) == 0)) break;
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

/**
 * Internal recursive function: get the 1st element with name "name" starting from "first_node"
 * @param xml The xml structure
 * @param name The name of the element to find
 * @param first_node The node where to find the element
 * @return A pointer to the value of the element, NULL if not found
 */
const char *get_element_rec(ezxml_t xml, char *name, char *first_node)
{
    const char *ret;
    char name_copy[256];
    ezxml_t px, pk;

    if (go_to_parent) return NULL;

    // If it's the root of the document choose the first node ("Header" or "Body")
    if (xml->parent == NULL) {
        px = xml->child;
        snprintf(name_copy, sizeof(name_copy), "%s", first_node);
        while (px) {
            if ((strlen(px->name) >= strlen(first_node)) &&
                    (strcmp(first_node, &(px->name)[strlen(px->name) - strlen(first_node)]) == 0)) {
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
        snprintf(&name_copy[1], sizeof(name_copy) - 1, "%s", name);
        if ((strlen(xml->name) >= strlen(name_copy)) &&
                (strcmp(name_copy, &(xml->name)[strlen(xml->name) - strlen(name_copy)]) == 0)) {
            return xml->txt;
        }

        // Check children
        if (xml->child) {
            ret = get_element_rec(xml->child, name, first_node);
            go_to_parent = 0;
            if (ret != NULL) return ret;
        }

        // Check brothers
        if((pk = xml->ordered)) {
            ret = get_element_rec(pk, name, first_node);
            if (go_to_parent) return NULL;
            if (ret != NULL) return ret;
        }

        go_to_parent = 1;
    }

    return NULL;
}

/**
 * Get the 1st element with name "name" starting from "first_node"
 * @param name The name of the element to find
 * @param first_node The node where to find the element
 * @return A pointer to the value of the element, NULL if not found
 */
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

/**
 * Internal recursive function: get the 1st element with name "name" starting from "start_from"
 * @param xml The xml structure
 * @param name The name of the element to find
 * @param first_node The node where to find the element
 * @return ezxml_t type pointing to the element, NULL if not found
 */
ezxml_t get_element_rec_ptr(ezxml_t xml, char *name, char *first_node)
{
    ezxml_t ret;
    char name_copy[256];
    ezxml_t px, pk;

    if (go_to_parent) return NULL;

    // If it's the root of the document choose the first node ("Header" or "Body")
    if (xml->parent == NULL) {
        px = xml->child;
        snprintf(name_copy, sizeof(name_copy), "%s", first_node);
        while (px) {
            if ((strlen(px->name) >= strlen(first_node)) &&
                    (strcmp(first_node, &(px->name)[strlen(px->name) - strlen(first_node)]) == 0)) {
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
        snprintf(&name_copy[1], sizeof(name_copy) - 1, "%s", name);
        if ((strlen(xml->name) >= strlen(name_copy)) &&
                (strcmp(name_copy, &(xml->name)[strlen(xml->name) - strlen(name_copy)]) == 0)) {
            return xml;
        }

        // Check children
        if (xml->child) {
            ret = get_element_rec_ptr(xml->child, name, first_node);
            go_to_parent = 0;
            if (ret != NULL) return ret;
        }

        // Check brothers
        if((pk = xml->ordered)) {
            ret = get_element_rec_ptr(pk, name, first_node);
            if (go_to_parent) return NULL;
            if (ret != NULL) return ret;
        }

        go_to_parent = 1;
    }

    return NULL;
}

/**
 * Get the 1st element with name "name" starting from "start_from"
 * @param start_from The element where to start
 * @param name The name of the element to find
 * @param first_node The 1st level node where to find the element
 * @return ezxml_t type pointing to the element, NULL if not found
 */
ezxml_t get_element_ptr(ezxml_t start_from, char *name, char *first_node)
{
    ezxml_t ret;

    go_to_parent = 0;
    if (start_from == NULL)
        ret = get_element_rec_ptr(root_xml, name, first_node);
    else
        ret = get_element_rec_ptr(start_from, name, first_node);

    return ret;
}

/**
 * Check whether an element tag is "<name>" or "<prefix:name>".
 */
static int tag_matches(const char *tag, const char *name)
{
    size_t tag_len = strlen(tag);
    size_t name_len = strlen(name);

    if (strcmp(tag, name) == 0)
        return 1;

    return tag_len > name_len && tag[tag_len - name_len - 1] == ':' &&
            strcmp(&tag[tag_len - name_len], name) == 0;
}

/**
 * Get the child element with name "name" starting from node "father"
 * @param name The name of the element to find
 * @param father The node where to find the element
 * @return A pointer to the value of the element, NULL if not found
 */
const char *get_element_in_element(const char *name, ezxml_t father)
{
    ezxml_t pk;

    for (pk = father->child; pk != NULL; pk = pk->ordered) {
        if (tag_matches(pk->name, name))
            return pk->txt;
    }

    return NULL;
}

/**
 * Get the child element with name "name" starting from node "father"
 * @param name The name of the element to find
 * @param father The node where to find the element
 * @return ezxml_t type pointing to the element, NULL if not found
 */
ezxml_t get_element_in_element_ptr(const char *name, ezxml_t father)
{
    ezxml_t pk;

    for (pk = father->child; pk != NULL; pk = pk->ordered) {
        if (tag_matches(pk->name, name))
            return pk;
    }

    return NULL;
}

/**
 * Get the attribute of a node with name "name"
 * @param node The node where to find the attribute
 * @param name The name of the attribute to find
 * @return A pointer to the value of the attribute, NULL if not found
 */
const char *get_attribute(ezxml_t node, char *name)
{
    return ezxml_attr(node, name);
}
