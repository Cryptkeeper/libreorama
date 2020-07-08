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

#include <stdlib.h>
#include <string.h>

void channel_free(struct channel_t *channel) {
    if (channel->frame_data != NULL) {
        free(channel->frame_data);

        // remove dangling pointer and index values
        // this prevents out of bounds iterations or double free bugs
        channel->frame_data_max_index  = 0;
        channel->frame_data_last_index = 0;
        channel->frame_data            = NULL;
    }
}

frame_t channel_get_frame(const struct channel_t *channel,
                          frame_index_t frame_index) {
    if (frame_index >= channel->frame_data_max_index) {
        return 0;
    } else {
        return channel->frame_data[frame_index];
    }
}

int channel_set_frame_data(struct channel_t *channel,
                           frame_index_t frame_index_start,
                           frame_index_t frame_index_end,
                           frame_t frame) {
    // frame_index_start & frame_index_end represent an array of the same value
    // ensure the full block fits within the current allocated frame_data
    // otherwise, resize the array to ensure frame_index_end fits in it
    if (frame_index_end >= channel->frame_data_max_index) {
        frame_index_t resized_max_index = channel->frame_data_max_index;

        // safely handle initial 0 value
        // realloc at Nx the size each time ceiling is hit
        // #sequence_frame_data_shrink will handle properly resizing the buffers
        if (resized_max_index == 0) {
            // default to a frame_data allocation of N frames
            resized_max_index = 512;
        } else {
            resized_max_index *= 2;
        }

        frame_t *frame_data = realloc(channel->frame_data, sizeof(frame_t) * resized_max_index);

        if (frame_data == NULL) {
            return 1;
        }

        // ensure the newly allocated memory portion is zeroed
        // offset from frame_data + previous max index in bytes to avoid clearing previous data
        const frame_index_t frame_index_diff = resized_max_index - channel->frame_data_max_index;

        memset(frame_data + (sizeof(frame_t) * channel->frame_data_max_index), 0, sizeof(frame_t) * frame_index_diff);

        // adjust dangling pointers and update ceiling value
        channel->frame_data           = frame_data;
        channel->frame_data_max_index = resized_max_index;
    }

    // set each frame index between start and end
    for (frame_index_t frame_index = frame_index_start; frame_index < frame_index_end; frame_index++) {
        channel->frame_data[frame_index] = frame;
    }

    // frame_data_last_index is used to track where the frame_data buffer ends
    // this is used later by #sequence_frame_data_shrink
    channel->frame_data_last_index = frame_index_end;

    return 0;
}

int channel_shrink_frame_data(struct channel_t *channel) {
    // resize frame_data of channel_t to perfectly fit the final frame data
    // this trims any oversized buffers by #channel_set_frame_data
    if (channel->frame_data_last_index < channel->frame_data_max_index) {
        // todo: allocate memory in neighboring blocks to prevent memory read jumps
        frame_t *frame_data = realloc(channel->frame_data, sizeof(frame_t) * channel->frame_data_last_index);

        if (frame_data == NULL) {
            return 1;
        }

        channel->frame_data           = frame_data;
        channel->frame_data_max_index = channel->frame_data_last_index;
    }

    return 0;
}
