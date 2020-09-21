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
#include "loreffect.h"

#include "lorparse.h"
#include "../../err/lbr.h"

#define LOREFFECT_MAX_INTENSITY 100

unsigned char loreffect_brightness(unsigned char effect_intensity) {
    // LMS files use 0-100 for brightness scales
    // normalize the value and scale it against 255 (full byte)
    // this value will be encoded to a lor_brightness_t by encode.h
    return (unsigned char) (((float) effect_intensity / 100.0f) * 255);
}

int loreffect_get_frame(const xmlNode *effect_node,
                       struct frame_t *frame,
                       unsigned long start_cs,
                       unsigned long end_cs) {
    xmlChar *effect_type = xmlGetProp(effect_node, (const xmlChar *) "type");

    if (effect_type == NULL) {
        // if NULL, attempt to match settings property, used in .loredit files
        effect_type = xmlGetProp(effect_node, (const xmlChar *) "settings");
    }

    if (effect_type == NULL) {
        return LBR_LOADER_EMALFDATA;
    }

    // return_code is zeroed whenever frame is defined
    // this prevents accidental flagging from complex logic
    int return_code = LBR_LOADER_EUNSUPDATA;

    // intensity effect comes in 2 forms
    // - contains "intensity" property to set the target brightness to
    // - contains "startIntensity" & "endIntensity" properties for fading
    if (xmlStrcasecmp(effect_type, (const xmlChar *) "intensity") == 0) {
        if (xmlHasProp(effect_node, (const xmlChar *) "intensity")) {
            const unsigned char intensity = (unsigned char) xml_get_propertyl(effect_node, "intensity");

            if (intensity == LOREFFECT_MAX_INTENSITY) {
                // since intensity is 100%, use ON action instead
                *frame = (struct frame_t) {
                        .action = LOR_ACTION_CHANNEL_ON,
                };
            } else {
                *frame = (struct frame_t) {
                        .action = LOR_ACTION_CHANNEL_SET_BRIGHTNESS,
                        {
                                .set_brightness = loreffect_brightness(intensity)
                        }
                };
            }

            return_code = 0;
            goto loreffect_get_frame_return;
        }

        if (xmlHasProp(effect_node, (const xmlChar *) "startIntensity") && xmlHasProp(effect_node, (const xmlChar *) "endIntensity")) {
            const unsigned char start_intensity = (unsigned char) xml_get_propertyl(effect_node, "startIntensity");
            const unsigned char end_intensity   = (unsigned char) xml_get_propertyl(effect_node, "endIntensity");

            *frame = (struct frame_t) {
                    .action = LOR_ACTION_CHANNEL_FADE,
                    {
                            .fade = {
                                    .from = loreffect_brightness(start_intensity),
                                    .to = loreffect_brightness(end_intensity),
                                    .duration = lor_duration_of((float) (end_cs - start_cs) / 100.0f),
                            }
                    }
            };

            return_code = 0;
            goto loreffect_get_frame_return;
        }
    }

    if (xmlStrcasecmp(effect_type, (const xmlChar *) "shimmer") == 0) {
        *frame = (struct frame_t) {
                .action = LOR_ACTION_CHANNEL_SHIMMER
        };

        return_code = 0;
        goto loreffect_get_frame_return;
    }

    if (xmlStrcasecmp(effect_type, (const xmlChar *) "twinkle") == 0) {
        *frame = (struct frame_t) {
                .action = LOR_ACTION_CHANNEL_TWINKLE
        };

        return_code = 0;
        goto loreffect_get_frame_return;
    }

    loreffect_get_frame_return:
    free(effect_type);

    return return_code;
}