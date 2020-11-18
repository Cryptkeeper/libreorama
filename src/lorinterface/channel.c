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
#include "channel.h"

#include <string.h>

#include "../err/lbr.h"

struct frame_t *channel_get_frame(struct channel_t channel,
                                  frame_index_t frame_count,
                                  frame_index_t index) {
    if (index >= frame_count) {
        return NULL;
    }

    struct frame_t *frame = &(channel.frame_data[index]);

    // do not return empty frames
    // these are simply allocated, but do not contain metadata
    // frames are allocated with calloc, so action is always initialized to 0
    if (frame->action == 0) {
        return NULL;
    } else {
        return frame;
    }
}

struct channel_t channel_buffer[CHANNEL_BUFFER_MAX_COUNT];
size_t           channel_buffer_index;

int channel_buffer_request(lor_unit_t unit,
                           lor_channel_t circuit,
                           frame_index_t frame_count,
                           struct channel_t **channel) {
    if (channel_buffer_index >= CHANNEL_BUFFER_MAX_COUNT) {
        return LBR_SEQUENCE_EINCCHANNELBUF;
    }

    struct channel_t *checkout = &channel_buffer[channel_buffer_index];

    // always initialize channel_t since they can be reused
    memset(checkout, 0, sizeof(struct channel_t));

    // checkout a frame buffer
    int err;
    if ((err = frame_buffer_request(frame_count, &(checkout->frame_data)))) {
        return err;
    }

    // by requiring params, this ensures any downstream
    //  usages are forced to initialize these values
    checkout->unit    = unit;
    checkout->circuit = circuit;

    *channel = checkout;
    channel_buffer_index++;

    return 0;
}

void channel_buffer_reset() {
    // reset index back to 0 for next checkout request
    channel_buffer_index = 0;
}
