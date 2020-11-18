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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <lightorama/io.h>
#include <lightorama/brightness_curve.h>

#include "../err/lbr.h"

static lor_brightness_t encode_brightness(unsigned char brightness) {
    return lor_brightness_curve_squared((float) brightness / 255.0f);
}

#define ENCODE_BUFFER_LENGTH_GROW_SCALE 2

unsigned char *encode_buffer;
size_t        encode_buffer_length;

static size_t encode_buffer_max_length;

void encode_buffer_free() {
    if (encode_buffer != NULL) {
        free(encode_buffer);

        // remove dangling pointer and reset indexes
        encode_buffer            = NULL;
        encode_buffer_length     = 0;
        encode_buffer_max_length = 0;
    }
}

int encode_buffer_alloc(size_t initial_length) {
    encode_buffer            = malloc(initial_length);
    encode_buffer_length     = 0;
    encode_buffer_max_length = initial_length;

    if (encode_buffer == NULL) {
        return LBR_EERRNO;
    }

    return 0;
}

int encode_buffer_append(unsigned char *data,
                         size_t len) {
    // if written_length + requested len is over the max_length
    //  then it the encode buffer's underlying allocation must be resized
    if (encode_buffer_length + len > encode_buffer_max_length) {
        size_t realloc_max_length = encode_buffer_max_length * ENCODE_BUFFER_LENGTH_GROW_SCALE;

        // max_length may be default value of 0
        // safely handle initial resizing logic
        if (realloc_max_length == 0) {
            realloc_max_length = len;
        }

        unsigned char *realloc_data = realloc(encode_buffer, realloc_max_length);
        if (data == NULL) {
            return LBR_EERRNO;
        }

        fprintf(stderr, "reallocated encode buffer to %zu bytes (increase pre-allocation?)\n", realloc_max_length);

        encode_buffer            = realloc_data;
        encode_buffer_max_length = realloc_max_length;
    }

    // copy the incoming buffer data to the final encode buffer
    unsigned char *start_addr = encode_buffer + encode_buffer_length;
    memcpy(start_addr, data, len);

    // advance the written_length counter for the next append
    encode_buffer_length += len;

    return 0;
}

void encode_buffer_reset() {
    encode_buffer_length = 0;
}

#define ENCODE_FLIP_BUFFER_MAXIMUM_WRITE_LENGTH 16

// the flip buffer is used as a the initial encoding buffer, before being copied into the full encode_buffer
// assumes no individual lor_write_* call will use more than N bytes
// see https://github.com/Cryptkeeper/liblightorama#memory-allocations
static unsigned char encode_flip_buffer[ENCODE_FLIP_BUFFER_MAXIMUM_WRITE_LENGTH];

int encode_frame(lor_unit_t unit,
                 enum lor_channel_type_t channel_type,
                 lor_channel_t channel,
                 struct frame_t frame) {
    size_t written;

    switch (frame.action) {
        case LOR_ACTION_CHANNEL_SET_BRIGHTNESS:
            written = lor_write_channel_set_brightness(unit, channel_type, channel, encode_brightness(frame.fade.to), encode_flip_buffer);
            break;
        case LOR_ACTION_CHANNEL_FADE:
            written = lor_write_channel_fade(unit, channel_type, channel, encode_brightness(frame.fade.from), encode_brightness(frame.fade.to), frame.fade.duration, encode_flip_buffer);
            break;
        case LOR_ACTION_CHANNEL_ON:
        case LOR_ACTION_CHANNEL_SHIMMER:
        case LOR_ACTION_CHANNEL_TWINKLE:
            written = lor_write_channel_action(unit, channel_type, channel, frame.action, encode_flip_buffer);
            break;
        default:
            return LBR_ENCODE_EUNSUPACTION;
    }

    if (written > ENCODE_FLIP_BUFFER_MAXIMUM_WRITE_LENGTH) {
        return LBR_ENCODE_EBUFFERTOOSMALL;
    }

    // copy the flip buffer to the full encode buffer
    int err;
    if ((err = encode_buffer_append(encode_flip_buffer, written))) {
        return err;
    }

    return 0;
}

int encode_heartbeat_frame(frame_index_t frame_index,
                           unsigned short step_time_ms) {
    // automatically push heartbeat messages into the encode buffer
    // this is timed for every 500ms, based off the frame index
    if (frame_index % (500 / step_time_ms) == 0) {
        size_t written = lor_write_heartbeat(encode_flip_buffer);

        // copy the flip buffer to the full encode buffer
        int err;
        if ((err = encode_buffer_append(encode_flip_buffer, written))) {
            return err;
        }
    }

    return 0;
}

int encode_reset_frame() {
    size_t written = lor_write_unit_action(LOR_UNIT_ID_BROADCAST, LOR_ACTION_UNIT_OFF, encode_flip_buffer);

    // copy the flip buffer to the full encode buffer
    int err;
    if ((err = encode_buffer_append(encode_flip_buffer, written))) {
        return err;
    }

    return 0;
}
