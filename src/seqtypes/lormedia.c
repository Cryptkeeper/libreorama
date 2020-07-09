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

#include <string.h>

#include <libxml/tree.h>

#include "../lbrerr.h"

#define LORMEDIA_MAX_INTENSITY 100

static xmlNode *xml_find_node_next(const xmlNode *root,
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
static int xml_get_property(const xmlNode *node,
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

static long xml_get_propertyl(const xmlNode *node,
                              const char *key) {
    xmlChar *value = xmlGetProp(node, (const xmlChar *) key);
    if (value == NULL) {
        return 0;
    }
    const long l = strtol((const char *) value, NULL, 10);
    xmlFree((void *) value);
    return l;
}

static unsigned char lormedia_effect_brightness(unsigned char effect_intensity) {
    // LMS files use 0-100 for brightness scales
    // normalize the value and scale it against 255 (full byte)
    // this value will be encoded to a lor_brightness_t by encode.h
    return (unsigned char) (((float) effect_intensity / 100.0f) * 255);
}

static int lormedia_get_frame(const xmlNode *effect_node,
                              struct frame_t *frame,
                              unsigned long start_cs,
                              unsigned long end_cs) {
    xmlChar *effect_type = xmlGetProp(effect_node, (const xmlChar *) "type");

    if (effect_type == NULL) {
        return LBR_LOADER_EMALFDATA;
    }

    // return_code is zeroed whenever frame is defined
    // this prevents accidental flagging from complex logic
    int return_code = LBR_LOADER_EUNSUPDATA;

    // intensity effect comes in 2 forms
    // - contains "intensity" property to set the target brightness to
    // - contains "startIntensity" & "endIntensity" properties for fading
    if (xmlStrcmp(effect_type, (const xmlChar *) "intensity") == 0) {
        if (xmlHasProp(effect_node, (const xmlChar *) "intensity")) {
            const unsigned char intensity = (unsigned char) xml_get_propertyl(effect_node, "intensity");

            if (intensity == LORMEDIA_MAX_INTENSITY) {
                // since intensity is 100%, use ON action instead
                *frame = (struct frame_t) {
                        .action = LOR_ACTION_CHANNEL_ON,
                };
            } else {
                *frame = (struct frame_t) {
                        .action = LOR_ACTION_CHANNEL_SET_BRIGHTNESS,
                        {
                                .set_brightness = lormedia_effect_brightness(intensity)
                        }
                };
            }

            return_code = 0;
            goto lormedia_get_frame_return;
        }

        if (xmlHasProp(effect_node, (const xmlChar *) "startIntensity") && xmlHasProp(effect_node, (const xmlChar *) "endIntensity")) {
            const unsigned char start_intensity = (unsigned char) xml_get_propertyl(effect_node, "startIntensity");
            const unsigned char end_intensity   = (unsigned char) xml_get_propertyl(effect_node, "endIntensity");

            *frame = (struct frame_t) {
                    .action = LOR_ACTION_CHANNEL_FADE,
                    {
                            .fade = {
                                    .from = lormedia_effect_brightness(start_intensity),
                                    .to = lormedia_effect_brightness(end_intensity),
                                    .duration = lor_duration_of((float) (end_cs - start_cs) / 100.0f),
                            }
                    }
            };

            return_code = 0;
            goto lormedia_get_frame_return;
        }
    }

    if (xmlStrcmp(effect_type, (const xmlChar *) "shimmer") == 0) {
        *frame = (struct frame_t) {
                .action = LOR_ACTION_CHANNEL_SHIMMER
        };

        return_code = 0;
        goto lormedia_get_frame_return;
    }

    if (xmlStrcmp(effect_type, (const xmlChar *) "twinkle") == 0) {
        *frame = (struct frame_t) {
                .action = LOR_ACTION_CHANNEL_TWINKLE
        };

        return_code = 0;
        goto lormedia_get_frame_return;
    }

    lormedia_get_frame_return:
    free(effect_type);

    return return_code;
}

int lormedia_sequence_load(const char *sequence_file,
                           char **audio_file_hint,
                           struct sequence_t *sequence) {
    int return_code = 0;

    xmlInitParser();

    // implementation is derived from xmlsoft.org example
    // see http://www.xmlsoft.org/examples/tree1.c
    xmlDocPtr doc = xmlReadFile(sequence_file, NULL, 0);

    if (doc == NULL) {
        return_code = LBR_EERRNO;
        goto lormedia_free;
    }

    // the document will have a single element, named sequence
    // this assumes it is the 2nd element in the document at root level
    const xmlNode *root_element     = xmlDocGetRootElement(doc);
    const xmlNode *sequence_element = xml_find_node_next(root_element, "sequence");

    int err;
    if ((err = xml_get_property(sequence_element, "musicFilename", audio_file_hint))) {
        return err;
    }

    // find the <channels> element and iterate over each children's children
    // use the startCentisecond & endCentisecond properties to understand each effects time length
    // select the lowest value to be used for the step_time
    // this ensures the program automatically runs at the precision needed
    const xmlNode *channels_element = xml_find_node_child(sequence_element, "channels");

    xmlNode *channel_node = channels_element->children;

    while (channel_node != NULL) {
        if (channel_node->type == XML_ELEMENT_NODE) {
            // append the channel_node to the sequence channels
            // offset channel by 1 since circuit is index 1 based
            const lor_unit_t    unit    = (lor_unit_t) xml_get_propertyl(channel_node, "unit");
            const lor_channel_t circuit = (lor_channel_t) (xml_get_propertyl(channel_node, "circuit") - 1);

            // append the channel_node to the sequence channels
            struct channel_t *channel_ptr = NULL;

            if ((err = sequence_add_channel(sequence, unit, circuit, &channel_ptr))) {
                return_code = err;
                goto lormedia_free;
            }

            // iterate over each child node in channels_child
            // these are the actual effects entries containing time data
            xmlNode *effect_node = channel_node->children;

            while (effect_node != NULL) {
                if (effect_node->type == XML_ELEMENT_NODE) {
                    const unsigned long start_cs = (unsigned long) xml_get_propertyl(effect_node, "startCentisecond");
                    const unsigned long end_cs   = (unsigned long) xml_get_propertyl(effect_node, "endCentisecond");

                    struct frame_t frame = FRAME_EMPTY;

                    if ((err = lormedia_get_frame(effect_node, &frame, start_cs, end_cs))) {
                        return_code = err;
                        goto lormedia_free;
                    }

                    // test if the difference, in milliseconds, is below the smallest step time threshold
                    const unsigned short current_step_time_ms = (unsigned short) ((end_cs - start_cs) * 10);

                    if (current_step_time_ms > 0 && current_step_time_ms < sequence->step_time_ms) {
                        sequence->step_time_ms = current_step_time_ms;
                    }

                    // from start/end_cs_prop (start time in centiseconds), scale against step_time_ms
                    //  to determine the frame_index for this effect_node
                    // this is because effect_nodes may be out of order, or in variable interval
                    const frame_index_t frame_index_start = (frame_index_t) ((start_cs * 10) / sequence->step_time_ms);

                    if ((err = channel_set_frame_data(channel_ptr, frame_index_start, frame))) {
                        return_code = err;
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
            const unsigned long total_cs = (unsigned long) xml_get_propertyl(track_node, "totalCentiseconds");

            if (total_cs > highest_total_cs) {
                highest_total_cs = total_cs;
            }
        }

        track_node = track_node->next;
    }

    // convert the highest_total_cs value from centiseconds into a frame_count
    // this used the previously determined step_time as a frame interval time
    sequence->frame_count = (frame_index_t) ((highest_total_cs * 10) / sequence->step_time_ms);

    lormedia_free:
    xmlFreeDoc(doc);

    // cleanup parser state, this pairs with #xmlInitParser
    // it may be a CPU waste to init/cleanup each load call
    // but this ensures that during playback, there is no wasted memory
    xmlCleanupParser();

    return return_code;
}