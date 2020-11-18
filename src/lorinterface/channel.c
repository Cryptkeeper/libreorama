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

#include "../err/lbr.h"

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

    memset(checkout, 0, sizeof(struct channel_t));

    // by requiring params, this ensures any downstream
    //  usages are forced to initialize these values
    checkout->unit       = unit;
    checkout->circuit    = circuit;
    checkout->frame_data = calloc(frame_count, sizeof(struct frame_t)); // TODO: optimize this to pull from a central buffer

    if (checkout->frame_data == NULL) {
        return LBR_EERRNO;
    }

    *channel = checkout;
    channel_buffer_index++;

    return 0;
}

void channel_buffer_reset() {
    for (size_t i = 0; i < channel_buffer_index; i++) {
        struct channel_t *channel = &channel_buffer[i];

        if (channel->frame_data != NULL) {
            free(channel->frame_data);

            // remove danging pointers to ensure any checked out
            //  channels are ready for re-use
            channel->frame_data = NULL;
        }
    }

    // reset index back to 0 for next checkout request
    channel_buffer_index = 0;
}
