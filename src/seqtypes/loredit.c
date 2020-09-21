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
#include "loredit.h"

#include "loreffect.h"
#include "lorparse.h"
#include "../err/lbr.h"

int loredit_sequence_load(const char *sequence_file,
                          char **audio_file_hint,
                          struct sequence_t *sequence) {
    int return_code = 0;

    xmlInitParser();

    // implementation is derived from xmlsoft.org example
    // see http://www.xmlsoft.org/examples/tree1.c
    xmlDocPtr doc = xmlReadFile(sequence_file, NULL, 0);

    if (doc == NULL) {
        return_code = LBR_EERRNO;
        goto loredit_free;
    }

    // the document will have a single element, named sequence
    // this assumes it is the 2nd element in the document at root level
    const xmlNode *root_element     = xmlDocGetRootElement(doc);
    const xmlNode *sequence_element = xml_find_node_next(root_element, "sequence");

    int err;
    if ((err = xml_get_property(sequence_element, "musicFilename", audio_file_hint))) {
        return err;
    }

    // find the <SequenceProps> element and iterate over each children's children
    // use the startCentisecond & endCentisecond properties to understand each effects time length
    // select the lowest value to be used for the step_time
    // this ensures the program automatically runs at the precision needed
    const xmlNode *sequence_props_element = xml_find_node_child(sequence_element, "SequenceProps");

    xmlNode *prop_node = sequence_props_element->children;

    int circuit = 1;
    int unit = 1;

    while (prop_node != NULL) {
        if (xml_is_named_node(prop_node, "SeqProp")) {
            xmlNode *channel_node = prop_node->children;

            while (channel_node != NULL) {
                if (xml_is_named_node(channel_node, "channel")) {
                    // append the channel_node to the sequence channels
                    struct channel_t *channel_ptr = NULL;

                    if ((err = sequence_add_channel(sequence, unit, circuit, &channel_ptr))) {
                        return_code = err;
                        goto loredit_free;
                    }

                    // todo: circuit/unit is currently assigned given iteration order
                    // this will not work for many uses
                    // the true values seem to be stored within each SeqProp's corresponding PropChannel->ChannelGrid value
                    if (circuit++ > 16) {
                        circuit = 1;
                        unit++;
                    }

                    // iterate over each child node in channels_child
                    // these are the actual effects entries containing time data
                    xmlNode *effect_node = channel_node->children;

                    while (effect_node != NULL) {
                        if (xml_is_named_node(effect_node, "effect")) {
                            const unsigned long start_cs = (unsigned long) xml_get_propertyl(effect_node, "startCentisecond");
                            const unsigned long end_cs   = (unsigned long) xml_get_propertyl(effect_node, "endCentisecond");

                            struct frame_t frame = FRAME_EMPTY;

                            if ((err = loreffect_get_frame(effect_node, &frame, start_cs, end_cs))) {
                                return_code = err;
                                goto loredit_free;
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
                                goto loredit_free;
                            }
                        }

                        effect_node = effect_node->next;
                    }
                }

                channel_node = channel_node->next;
            }
        }

        prop_node = prop_node->next;
    }

    // convert the totalCentiseconds value from centiseconds into a frame_count
    // this used the previously determined step_time as a frame interval time
    const long total_cs = xml_get_propertyl(sequence_element, "totalCentiseconds");

    sequence->frame_count = (frame_index_t) ((total_cs * 10) / sequence->step_time_ms);

    loredit_free:
    xmlFreeDoc(doc);

    // cleanup parser state, this pairs with #xmlInitParser
    // it may be a CPU waste to init/cleanup each load call
    // but this ensures that during playback, there is no wasted memory
    xmlCleanupParser();

    return return_code;
}