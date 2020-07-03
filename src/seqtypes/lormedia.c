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
#include "lormedia.h"

#include <stdio.h>
#include <string.h>

#include <libxml/tree.h>
#include <libxml/parser.h>

static xmlNode *xml_find_node(const xmlNode *parent,
                              const xmlChar *name) {
    // iterate from the provided parent to each child
    // see http://xmlsoft.org/tutorial/ar01s04.html
    xmlNode *cur = parent;

    while (cur != NULL) {
        if (xmlStrEqual(cur->name, name)) {
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
static char *xml_get_property(const xmlNode *node,
                              const char *key) {
    const xmlChar *value = xmlGetProp(node, (const xmlChar *) key);

    if (value == NULL) {
        return NULL;
    }

    // determine the string's length and allocate a copy buffer
    // size for + 1 to ensure null termination
    const size_t str_len = xmlStrlen(value);
    char         *copy   = malloc(str_len + 1);

    if (copy == NULL) {
        // ensure the allocated xmlChar *string is always freed
        xmlFree((void *) value);

        perror("failed to allocate string buffer in xml_get_property");
        return NULL;
    }

    // cast xmlChar back to char
    // this is seemingly dangerous, but apparently expected usage
    // see https://stackoverflow.com/questions/2282424/libxml2-xmlchar-cast-to-char
    strncpy(copy, (const char *) value, str_len);

    // explicitly null terminate the string
    copy[str_len] = 0;

    // free the allocated xmlChar *string from xmlGetProp
    xmlFree((void *) value);

    return copy;
}

// todo: cleanup
int lormedia_sequence_load(const char *sequence_file,
                           char **audio_file,
                           struct timespec *step_time) {
    xmlInitParser();

    // implementation is derived from xmlsoft.org example
    // see http://www.xmlsoft.org/examples/tree1.c
    xmlDocPtr doc = xmlReadFile(sequence_file, NULL, 0);

    if (doc == NULL) {
        perror("failed to parse XML file");
        return 1;
    }

    // the document will have a single root element, named sequence
    xmlNode *root_element = xmlDocGetRootElement(doc);

    const xmlNode *sequence_element = xml_find_node(root_element, (const xmlChar *) "sequence");

    *audio_file = xml_get_property(sequence_element, "musicFilename");

    xmlFreeDoc(doc);

    xmlCleanupParser();

    step_time->tv_sec  = 1; // todo
    step_time->tv_nsec = 0;

    return 0;
}