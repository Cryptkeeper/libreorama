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
#include "encode.h"

#include <lightorama/io.h>
#include <lightorama/brightness_curve.h>

#include "../err/lbr.h"

static lor_brightness_t encode_brightness(unsigned char brightness) {
    return lor_brightness_curve_squared((float) brightness / 255.0f);
}

#define ENCODE_BUFFER_MAX_LENGTH 4096

unsigned char encode_buffer[ENCODE_BUFFER_MAX_LENGTH];
size_t        encode_buffer_index;

unsigned char *encode_buffer_write_index() {
    return &encode_buffer[encode_buffer_index];
}

int encode_buffer_advance(size_t len) {
    encode_buffer_index += len;

    if (encode_buffer_index > ENCODE_BUFFER_MAX_LENGTH) {
        return LBR_ENCODE_EBUFFERTOOSMALL;
    }

    return 0;
}

void encode_buffer_reset() {
    encode_buffer_index = 0;
}

int encode_frame(lor_unit_t unit,
                 enum lor_channel_type_t channel_type,
                 lor_channel_t channel,
                 struct frame_t frame) {
    size_t written;

    switch (frame.action) {
        case LOR_ACTION_CHANNEL_SET_BRIGHTNESS:
            written = lor_write_channel_set_brightness(unit, channel_type, channel, encode_brightness(frame.fade.to), encode_buffer_write_index());
            break;
        case LOR_ACTION_CHANNEL_FADE:
            written = lor_write_channel_fade(unit, channel_type, channel, encode_brightness(frame.fade.from), encode_brightness(frame.fade.to), frame.fade.duration, encode_buffer_write_index());
            break;
        case LOR_ACTION_CHANNEL_ON:
        case LOR_ACTION_CHANNEL_SHIMMER:
        case LOR_ACTION_CHANNEL_TWINKLE:
            written = lor_write_channel_action(unit, channel_type, channel, frame.action, encode_buffer_write_index());
            break;
        default:
            return LBR_ENCODE_EUNSUPACTION;
    }

    int err;
    if ((err = encode_buffer_advance(written))) {
        return err;
    }

    return 0;
}

int encode_heartbeat_frame(frame_index_t frame_index,
                           unsigned short step_time_ms) {
    // automatically push heartbeat messages into the encode buffer
    // this is timed for every 500ms, based off the frame index
    if (frame_index % (500 / step_time_ms) == 0) {
        size_t written = lor_write_heartbeat(encode_buffer_write_index());

        int err;
        if ((err = encode_buffer_advance(written))) {
            return err;
        }
    }

    return 0;
}

int encode_reset_frame() {
    size_t written = lor_write_unit_action(LOR_UNIT_ID_BROADCAST, LOR_ACTION_UNIT_OFF, encode_buffer_write_index());

    int err;
    if ((err = encode_buffer_advance(written))) {
        return err;
    }

    return 0;
}
