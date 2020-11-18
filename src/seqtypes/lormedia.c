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

#include "loreffect.h"
#include "lorparse.h"
#include "../err/lbr.h"

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

    // prior to computing any timing sensitive values
    //  iterate through all channels and their effects to calculate the lowest necessary step_time_ms
    //  this will be used to determine the full frame_count to be allocated for channels
    // find the <channels> element and iterate over each children's children
    // use the startCentisecond & endCentisecond properties to understand each effects time length
    // select the lowest value to be used for the step_time
    // this ensures the program automatically runs at the precision needed
    const xmlNode *channels_element = xml_find_node_child(sequence_element, "channels");
    xmlNode       *channel_node     = channels_element->children;

    while (channel_node != NULL) {
        if (xml_is_named_node(channel_node, "channel")) {
            // iterate over each child node in channels_child
            // these are the actual effects entries containing time data
            xmlNode *effect_node = channel_node->children;

            while (effect_node != NULL) {
                if (xml_is_named_node(effect_node, "effect")) {
                    const unsigned long start_cs = (unsigned long) xml_get_propertyl(effect_node, "startCentisecond");
                    const unsigned long end_cs   = (unsigned long) xml_get_propertyl(effect_node, "endCentisecond");

                    // test if the difference, in milliseconds, is below the smallest step time threshold
                    const unsigned short current_step_time_ms = (unsigned short) ((end_cs - start_cs) * 10);

                    if (current_step_time_ms > 0 && current_step_time_ms < sequence->step_time_ms) {
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
        if (xml_is_named_node(track_node, "track")) {
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

    // reset channel_node value since it was previously iterated
    channel_node = channels_element->children;

    while (channel_node != NULL) {
        if (xml_is_named_node(channel_node, "channel")) {
            // append the channel_node to the sequence channels
            // offset channel by 1 since circuit is index 1 based
            const lor_unit_t    unit    = (lor_unit_t) xml_get_propertyl(channel_node, "unit");
            const lor_channel_t circuit = (lor_channel_t) (xml_get_propertyl(channel_node, "circuit") - 1);

            // append the channel_node to the sequence channels
            struct channel_t *channel = NULL;

            if ((err = channel_buffer_request(unit, circuit, sequence->frame_count, &channel))) {
                return_code = err;
                goto lormedia_free;
            }

            // iterate over each child node in channels_child
            // these are the actual effects entries containing time data
            xmlNode *effect_node = channel_node->children;

            while (effect_node != NULL) {
                if (xml_is_named_node(effect_node, "effect")) {
                    const unsigned long start_cs = (unsigned long) xml_get_propertyl(effect_node, "startCentisecond");
                    const unsigned long end_cs   = (unsigned long) xml_get_propertyl(effect_node, "endCentisecond");

                    // from start/end_cs_prop (start time in centiseconds), scale against step_time_ms
                    //  to determine the frame_index for this effect_node
                    // this is because effect_nodes may be out of order, or in variable interval
                    const frame_index_t frame_index_start = (frame_index_t) ((start_cs * 10) / sequence->step_time_ms);
                    struct frame_t      *frame            = &channel->frame_data[frame_index_start];

                    if ((err = loreffect_get_frame(effect_node, frame, start_cs, end_cs))) {
                        return_code = err;
                        goto lormedia_free;
                    }
                }

                effect_node = effect_node->next;
            }
        }

        channel_node = channel_node->next;
    }

    lormedia_free:
    xmlFreeDoc(doc);

    // cleanup parser state, this pairs with #xmlInitParser
    // it may be a CPU waste to init/cleanup each load call
    // but this ensures that during playback, there is no wasted memory
    xmlCleanupParser();

    return return_code;
}
