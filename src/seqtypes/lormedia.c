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

#include "../math.h"

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
    xmlChar *value = xmlGetProp(node, (const xmlChar *) key);

    if (value == NULL) {
        return NULL;
    }

    // determine the string's length and allocate a copy buffer
    // size for + 1 to ensure null termination
    const size_t str_len = xmlStrlen(value);
    char         *copy   = malloc(str_len + 1);

    if (copy == NULL) {
        perror("failed to allocate string buffer in xml_get_property");
        goto xml_get_property_free;
    }

    // cast xmlChar back to char
    // this is seemingly dangerous, but apparently expected usage
    // see https://stackoverflow.com/questions/2282424/libxml2-xmlchar-cast-to-char
    strncpy(copy, (const char *) value, str_len);

    // explicitly null terminate the string
    copy[str_len] = 0;

    xml_get_property_free:
    // free the allocated xmlChar *string from xmlGetProp
    xmlFree((void *) value);

    return copy;
}

static long xml_get_propertyl(const xmlNode *node,
                              const char *key) {
    xmlChar *value = xmlGetProp(node, (const xmlChar *) key);
    if (value == NULL) {
        return 0;
    }
    const long l = strtol((char *) value, NULL, 10);
    xmlFree((void *) value);
    return l;
}

static int lormedia_get_effect_intensity(const xmlNode *effect_node,
                                         unsigned char *effect_intensity) {
    // attempt to match an instant "intensity" field value
    char *intensity_prop = xml_get_property(effect_node, "intensity");

    // todo: fade support
    if (intensity_prop == NULL) {
        return 1;
    }

    // rescale the intensity_prop long value (between 0-100) into a float
    // clampf to avoid potential overflows from invalid files
    // this is scaled against 255 to create a single full byte range
    const float effect_intensity_f = clampf((float) strtol(intensity_prop, NULL, 10) / 100.0f, 0, 1);

    *effect_intensity = (unsigned char) (effect_intensity_f * 255);

    // free the intensity_prop allocated by #xml_get_property
    free(intensity_prop);

    return 0;
}

// todo: refactor effect naming? use types
int lormedia_sequence_load(const char *sequence_file,
                           char **audio_file_hint,
                           struct sequence_t *sequence) {
    int return_code = 0;

    xmlInitParser();

    // implementation is derived from xmlsoft.org example
    // see http://www.xmlsoft.org/examples/tree1.c
    xmlDocPtr doc = xmlReadFile(sequence_file, NULL, 0);

    if (doc == NULL) {
        perror("failed to parse XML file");
        return_code = 1;
        goto lormedia_free;
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

    while (channel_node != NULL) {
        if (channel_node->type == XML_ELEMENT_NODE) {
            // append the channel_node to the sequence channels
            const lor_unit_t    unit    = xml_get_propertyl(channel_node, "unit");
            const lor_channel_t channel = xml_get_propertyl(channel_node, "circuit");

            struct channel_t *channel_ptr = NULL;

            if (sequence_add_channel(sequence, unit, channel, &channel_ptr)) {
                perror("failed to add index");
                return_code = 1;
                goto lormedia_free;
            }

            // iterate over each child node in channels_child
            // these are the actual effects entries containing time data
            xmlNode *effect_node = channel_node->children;

            while (effect_node != NULL) {
                if (effect_node->type == XML_ELEMENT_NODE) {
                    const long start_cs = xml_get_propertyl(effect_node, "startCentisecond");
                    const long end_cs   = xml_get_propertyl(effect_node, "endCentisecond");

                    // test if the difference, in milliseconds, is below
                    //  the smallest step time threshold
                    const long current_step_time_ms = (end_cs - start_cs) * 10;

                    if (current_step_time_ms < sequence->step_time_ms) {
                        sequence->step_time_ms = current_step_time_ms;
                    }

                    unsigned char effect_intensity = 0;

                    if (lormedia_get_effect_intensity(effect_node, &effect_intensity)) {
                        char *type_prop = xml_get_property(effect_node, "type");
                        fprintf(stderr, "unable to get effect intensity: %s\n", type_prop);
                        free(type_prop);

                        return_code = 1;
                        goto lormedia_free;
                    }

                    // from start/end_cs_prop (start time in centiseconds), scale against step_time_ms
                    //  to determine the frame_index for this effect_node
                    // this is because effect_nodes may be out of order, or in variable interval
                    const frame_index_t frame_index_start = (start_cs * 10) / sequence->step_time_ms;
                    const frame_index_t frame_index_end   = (end_cs * 10) / sequence->step_time_ms;

                    if (channel_set_frame_data(channel_ptr, frame_index_start, frame_index_end, effect_intensity)) {
                        perror("failed to set frame data");
                        return_code = 1;
                        goto lormedia_free;
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
            const unsigned long total_cs = xml_get_propertyl(track_node, "totalCentiseconds");

            if (total_cs > highest_total_cs) {
                highest_total_cs = total_cs;
            }
        }

        track_node = track_node->next;
    }

    // convert the highest_total_cs value from centiseconds into a frame_count
    // this used the previously determined step_time as a frame interval time
    sequence->frame_count = (highest_total_cs * 10) / sequence->step_time_ms;

    lormedia_free:
    xmlFreeDoc(doc);

    // cleanup parser state, this pairs with #xmlInitParser
    // it may be a CPU waste to init/cleanup each load call
    // but this ensures that during playback, there is no wasted memory
    xmlCleanupParser();

    return return_code;
}