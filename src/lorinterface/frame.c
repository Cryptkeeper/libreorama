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
#include "frame.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../err/lbr.h"

#define FRAME_BUFFER_LENGTH_GROW_SCALE 2

bool frame_equals(const struct frame_t *a,
                  const struct frame_t *b,
                  enum frame_equals_mode_t equals_mode) {
    if (a == NULL && b == NULL) {
        return true;
    } else if ((a == NULL) != (b == NULL)) {
        return false;
    }

    if (a->action != b->action) {
        return false;
    }

    switch (a->action) {
        case LOR_ACTION_CHANNEL_SET_BRIGHTNESS:
            return a->set_brightness == b->set_brightness;

        case LOR_ACTION_CHANNEL_FADE:
            if (equals_mode == EQUALS_MODE_STRICT) {
                // fade actions are stateful internally to the hardware
                // they cannot be equal and shouldn't be
                return false;
            } else if (equals_mode == EQUALS_MODE_VALUE) {
                return a->fade.duration == b->fade.duration && a->fade.to == b->fade.to && a->fade.from == b->fade.from;
            }

        case LOR_ACTION_CHANNEL_ON:
        case LOR_ACTION_CHANNEL_TWINKLE:
        case LOR_ACTION_CHANNEL_SHIMMER:
            return true;

        default:
            fprintf(stderr, "unable to compare frame equality: %d", a->action);
            return false;
    }
}

void frame_buffer_free(struct frame_buffer_t *frame_buffer) {
    if (frame_buffer->data != NULL) {
        free(frame_buffer->data);

        // remove dangling pointer and reset indexes
        frame_buffer->data           = NULL;
        frame_buffer->written_length = 0;
        frame_buffer->max_length     = 0;
    }
}

int frame_buffer_alloc(struct frame_buffer_t *frame_buffer,
                       size_t initial_length) {
    frame_buffer->data       = malloc(initial_length);
    frame_buffer->max_length = initial_length;

    if (frame_buffer->data == NULL) {
        return LBR_EERRNO;
    }

    return 0;
}

int frame_buffer_append(struct frame_buffer_t *frame_buffer,
                        unsigned char *data,
                        size_t len) {
    // if written_length + requested len is over the max_length
    //  then it the frame buffer's underlying allocation must be resized
    if (frame_buffer->written_length + len > frame_buffer->max_length) {
        size_t realloc_max_length = frame_buffer->max_length * FRAME_BUFFER_LENGTH_GROW_SCALE;

        // max_length may be default value of 0
        // safely handle initial resizing logic
        if (realloc_max_length == 0) {
            realloc_max_length = len;
        }

        unsigned char *realloc_data = realloc(frame_buffer->data, realloc_max_length);
        if (data == NULL) {
            return LBR_EERRNO;
        }

        fprintf(stderr, "reallocated frame buffer to %zu bytes (increase pre-allocation?)\n", realloc_max_length);

        frame_buffer->data       = realloc_data;
        frame_buffer->max_length = realloc_max_length;
    }

    // copy the incoming buffer data to the final frame buffer
    unsigned char *start_addr = frame_buffer->data + frame_buffer->written_length;
    memcpy(start_addr, data, len);

    // advance the written_length counter for the next append
    frame_buffer->written_length += len;

    return 0;
}

int frame_buffer_reset_writer(struct frame_buffer_t *frame_buffer) {
    // reset the writer index
    frame_buffer->written_length = 0;

    return 0;
}
