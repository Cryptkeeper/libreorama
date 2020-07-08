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

#define CHANNEL_FRAME_DATA_COUNT_DEFAULT 512
#define CHANNEL_FRAME_DATA_COUNT_GROW_SCALE 2

const struct channel_t CHANNEL_EMPTY = (struct channel_t) {
        .unit = 0,
        .channel = 0,
        .frame_data = NULL,
        .frame_data_count = 0,
        .frame_data_count_max = 0,
        .last_frame_data = 0,
        .first_frame_offset = 0,
        .has_first_frame_offset = false
};

frame_t *channel_get_frame(const struct channel_t *channel,
                           frame_index_t frame_index) {
    // prevent read attempts if the current frame_index is
    //  lower than (before) the first frame_index supported by the channel
    // this is important since frame_index_t is unsigned
    //  and the subtraction below could underflow
    if (channel->has_first_frame_offset && frame_index < channel->first_frame_offset) {
        return NULL;
    }

    const frame_index_t offset_frame_index = frame_index - channel->first_frame_offset;
    if (offset_frame_index >= channel->frame_data_count) {
        return NULL;
    } else {
        return &channel->frame_data[offset_frame_index];
    }
}

int channel_set_frame_data(struct channel_t *channel,
                           frame_index_t frame_index_start,
                           frame_index_t frame_index_end,
                           frame_t frame) {
    // if this is the first frame of the channel, offset indexes by frame_index_start
    // this prevents padding allocations for later used channels
    if (!channel->has_first_frame_offset) {
        channel->has_first_frame_offset = true;
        channel->first_frame_offset     = frame_index_start;
    }

    // offset incoming frame index values
    // this aligns first_frame_offset with 0
    if (channel->has_first_frame_offset) {
        frame_index_start -= channel->first_frame_offset;
        frame_index_end -= channel->first_frame_offset;
    }

    // frame_index_start & frame_index_end represent an array of the same value
    // ensure the full block fits within the current allocated frame_data
    // otherwise, resize the array to ensure frame_index_end fits in it
    if (frame_index_end >= channel->frame_data_count_max) {
        frame_index_t resized_count_max = channel->frame_data_count_max;

        // safely handle initial 0 value
        // realloc at Nx the size each time ceiling is hit
        // #channel_shrink_frame_data will handle properly resizing the buffers
        if (resized_count_max == 0) {
            // default to a frame_data allocation of N frames
            resized_count_max = CHANNEL_FRAME_DATA_COUNT_DEFAULT;
        } else {
            resized_count_max *= CHANNEL_FRAME_DATA_COUNT_GROW_SCALE;
        }

        frame_t *frame_data = realloc(channel->frame_data, sizeof(frame_t) * resized_count_max);

        if (frame_data == NULL) {
            return 1;
        }

        // ensure the newly allocated memory portion is zeroed
        // offset from frame_data + previous max count in bytes to avoid clearing previous data
        const frame_index_t frame_count_diff = resized_count_max - channel->frame_data_count_max;

        memset(frame_data + channel->frame_data_count_max, 0, frame_count_diff);

        // adjust dangling pointers and update ceiling value
        channel->frame_data           = frame_data;
        channel->frame_data_count_max = resized_count_max;
    }

    // set each frame index between start and end
    for (frame_index_t frame_index = frame_index_start; frame_index < frame_index_end; frame_index++) {
        channel->frame_data[frame_index] = frame;
    }

    // frame_data_count is used to track where the frame_data buffer ends
    // this is used later by #channel_shrink_frame_data
    channel->frame_data_count = frame_index_end;

    return 0;
}

