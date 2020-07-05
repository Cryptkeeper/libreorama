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

static xmlNode *xml_find_node_next(const xmlNode *root,
                                   const char *name) {
    // iterate from the provided element to each next
    // see http://xmlsoft.org/tutorial/ar01s04.html
    xmlNode *cur = root;

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

static xmlNode *xml_find_node_child(const xmlNode *parent,
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

int lormedia_sequence_load(const char *sequence_file,
                           char **audio_file_hint,
                           struct sequence_t *sequence) {
    xmlInitParser();

    // implementation is derived from xmlsoft.org example
    // see http://www.xmlsoft.org/examples/tree1.c
    xmlDocPtr doc = xmlReadFile(sequence_file, NULL, 0);

    if (doc == NULL) {
        perror("failed to parse XML file");
        return 1;
    }

    // the document will have a single element, named sequence
    // this assumes it is the 2nd element in the document at root level
    const xmlNode *root_element     = xmlDocGetRootElement(doc);
    const xmlNode *sequence_element = xml_find_node_next(root_element, "sequence");

    *audio_file_hint = xml_get_property(sequence_element, "musicFilename");

    // find the <channels> element and iterate over each children's children
    // use the startCentisecond & endCentisecond properties to understand each effects time length
    // select the lowest value to be used for the step_time
    // this ensures the program automatically runs at the precision needed
    const xmlNode *channels_element = xml_find_node_child(sequence_element, "channels");

    xmlNode *channel_node = channels_element->children;
    xmlNode *effect_node  = NULL;

    while (channel_node != NULL) {
        if (channel_node->type == XML_ELEMENT_NODE) {
            // iterate over each child node in channels_child
            // these are the actual effects entries containing time data
            effect_node = channel_node->children;

            while (effect_node != NULL) {
                if (effect_node->type == XML_ELEMENT_NODE) {
                    char       *start_cs_prop = xml_get_property(effect_node, "startCentisecond");
                    const long start_cs       = strtol(start_cs_prop, NULL, 10);
                    free(start_cs_prop);

                    char       *end_cs_prop = xml_get_property(effect_node, "endCentisecond");
                    const long end_cs       = strtol(end_cs_prop, NULL, 10);
                    free(end_cs_prop);

                    // test if the difference, in milliseconds, is below
                    //  the smallest step time threshold
                    const long current_step_time_ms = (end_cs - start_cs) * 10;

                    if (current_step_time_ms < sequence->step_time_ms) {
                        sequence->step_time_ms = current_step_time_ms;
                    }
                }

                effect_node = effect_node->next;
            }
        }

        channel_node = channel_node->next;
    }

    // read the <tracks> element
    // each child will contain a "totalCentiseconds" property
    // locate the highest value to be used as a "total sequence duration" value
    const xmlNode *tracks_element = xml_find_node_child(sequence_element, "tracks");
    xmlNode       *track_node     = tracks_element->children;

    unsigned long highest_total_cs = 0;

    while (track_node != NULL) {
        if (track_node->type == XML_ELEMENT_NODE) {
            char                *total_cs_prop = xml_get_property(track_node, "totalCentiseconds");
            const unsigned long total_cs       = strtol(total_cs_prop, NULL, 10);
            free(total_cs_prop);

            if (total_cs > highest_total_cs) {
                highest_total_cs = total_cs;
            }
        }

        track_node = track_node->next;
    }

    // convert the highest_total_cs value from centiseconds into a frame_count
    // this used the previously determined step_time as a frame interval time
    sequence->frame_count = (highest_total_cs * 10) / sequence->step_time_ms;

    xmlFreeDoc(doc);

    // cleanup parser state, this pairs with #xmlInitParser
    // it may be a CPU waste to init/cleanup each load call
    // but this ensures that during playback, there is no wasted memory
    xmlCleanupParser();

    return 0;
}