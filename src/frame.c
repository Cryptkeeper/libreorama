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

#define FRAME_BUFFER_LENGTH_GROW_SCALE 2

bool frame_equals(struct frame_t a,
                  struct frame_t b) {
    if (a.action != b.action) {
        return false;
    }

    switch (a.action) {
        case LOR_ACTION_CHANNEL_SET_BRIGHTNESS:
            return a.set_brightness == b.set_brightness;

        case LOR_ACTION_CHANNEL_FADE:
            return a.fade.from == b.fade.from && a.fade.to == b.fade.to && a.fade.duration == b.fade.duration;

        case LOR_ACTION_CHANNEL_ON:
        case LOR_ACTION_CHANNEL_TWINKLE:
        case LOR_ACTION_CHANNEL_SHIMMER:
            return true;

        default:
            fprintf(stderr, "unable to compare frame equality: %d", a.action);
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
        return 1;
    }

    return 0;
}

// frame_buffer_t is backed by a unsigned char *
// each write requests a blob, which is a subsection of that backing memory allocation
// #frame_buffer_get_blob ensures each blob is a minimum length, otherwise reallocing the backing memory allocation
// this ensures each write is safely within bounds (assuming each write is within the defined maximum length)
int frame_buffer_get_blob(struct frame_buffer_t *frame_buffer,
                          unsigned char **blob,
                          size_t blob_length) {
    // if written_length + requested blob_length is under max
    //  then a blob is already available, instantly return its pointer
    if (frame_buffer->written_length + blob_length <= frame_buffer->max_length) {
        goto frame_buffer_get_blob_return;
    }

    size_t realloc_max_length = frame_buffer->max_length * FRAME_BUFFER_LENGTH_GROW_SCALE;

    // max_length may be default value of 0
    // safely handle initial resizing logic
    if (realloc_max_length == 0) {
        realloc_max_length = blob_length;
    }

    unsigned char *data = realloc(frame_buffer->data, realloc_max_length);

    if (data == NULL) {
        return 1;
    }

    fprintf(stderr, "reallocated frame buffer to %zu bytes (increase pre-allocation?)\n", realloc_max_length);

    frame_buffer->data       = data;
    frame_buffer->max_length = realloc_max_length;

    frame_buffer_get_blob_return:
    // select the next available blob where the write index ends
    // this packs the blobs next to each other but ensures a minimum length
    //  as requested by the blob_length parameter
    *blob = frame_buffer->data + frame_buffer->written_length;

    return 0;
}

int frame_buffer_reset(struct frame_buffer_t *frame_buffer) {
    // reset the writer index
    frame_buffer->written_length = 0;

    return 0;
}