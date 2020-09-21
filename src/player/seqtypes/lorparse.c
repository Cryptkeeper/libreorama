/*
 * MIT License
 *
 * Copyright (c) 2020 Nick Krecklow
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "lorparse.h"

#include <string.h>

#include "../../err/lbr.h"

xmlNode *xml_find_node_next(const xmlNode *root,
                            const char *name) {
    // iterate from the provided element to each next
    // see http://xmlsoft.org/tutorial/ar01s04.html
    xmlNode *cur = (xmlNode *) root;

    while (cur != NULL) {
        if (xmlStrEqual(cur->name, (const xmlChar *) name)) {
            return cur;
        }

        // move forward to next node
        // this has a search depth of 1 (immediate children only)
        cur = cur->next;
    }

    return NULL;
}

xmlNode *xml_find_node_child(const xmlNode *parent,
                             const char *name) {
    // iterate from the provided element to each child
    // see http://xmlsoft.org/tutorial/ar01s04.html
    xmlNode *cur = parent->children;

    while (cur != NULL) {
        if (xmlStrEqual(cur->name, (const xmlChar *) name)) {
            return cur;
        }

        // move forward to next node
        // this has a search depth of 1 (immediate children only)
        cur = cur->next;
    }

    return NULL;
}

// finds the node property, if any, and copies the string into a char *buffer
// the buffer must be manually freed by the caller function
// returns NULL if the node does not contain the property or malloc fails
int xml_get_property(const xmlNode *node,
                     const char *key,
                     char **out) {
    xmlChar *value = xmlGetProp(node, (const xmlChar *) key);

    if (value == NULL) {
        return 0;
    }

    int return_code = 0;

    // determine the string's length and allocate a copy buffer
    // size for + 1 to ensure null termination
    const size_t str_len = (size_t) xmlStrlen(value);
    char         *copy   = malloc(str_len + 1);

    if (copy == NULL) {
        return_code = LBR_EERRNO;
        goto xml_get_property_free;
    }

    // cast xmlChar back to char
    // this is seemingly dangerous, but apparently expected usage
    // see https://stackoverflow.com/questions/2282424/libxml2-xmlchar-cast-to-char
    strncpy(copy, (const char *) value, str_len);

    // explicitly null terminate the string
    copy[str_len] = 0;

    *out = copy;

    xml_get_property_free:
    // free the allocated xmlChar *string from xmlGetProp
    xmlFree((void *) value);

    return return_code;
}

long xml_get_propertyl(const xmlNode *node,
                       const char *key) {
    xmlChar *value = xmlGetProp(node, (const xmlChar *) key);
    if (value == NULL) {
        return 0;
    }
    const long l = strtol((const char *) value, NULL, 10);
    xmlFree((void *) value);
    return l;
}

int xml_is_named_node(const xmlNode *node,
                      const char *key) {
    return node->type == XML_ELEMENT_NODE && xmlStrEqual(node->name, (const xmlChar *) key);
}