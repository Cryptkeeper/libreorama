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
#include "minify.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../err/lbr.h"
#include "encode.h"
#include "state.h"

enum frame_equals_mode_t {
    EQUALS_MODE_STRICT,
    EQUALS_MODE_VALUE
};

static bool frame_equals(const struct frame_t a,
                         const struct frame_t b,
                         enum frame_equals_mode_t equals_mode) {
    if (!frame_is_set(a) && !frame_is_set(b)) {
        return true;
    } else if (frame_is_set(a) != frame_is_set(b)) {
        return false;
    }

    if (a.action != b.action) {
        return false;
    }

    switch (a.action) {
        case LOR_ACTION_CHANNEL_SET_BRIGHTNESS:
            return a.set_brightness == b.set_brightness;

        case LOR_ACTION_CHANNEL_FADE:
            if (equals_mode == EQUALS_MODE_STRICT) {
                // fade actions are stateful internally to the hardware
                // they cannot be equal and shouldn't be
                return false;
            } else if (equals_mode == EQUALS_MODE_VALUE) {
                return a.fade.duration == b.fade.duration && a.fade.to == b.fade.to && a.fade.from == b.fade.from;
            } else {
                fprintf(stderr, "unsupported equals_mode: %d\n", equals_mode);
                return false;
            }

        case LOR_ACTION_CHANNEL_ON:
        case LOR_ACTION_CHANNEL_TWINKLE:
        case LOR_ACTION_CHANNEL_SHIMMER:
            return true;

        default:
            fprintf(stderr, "unable to compare frame equality: %d\n", a.action);
            return false;
    }
}

static int minify_channel_compare(const void *a,
                                  const void *b) {
    const struct channel_t *channel_a = (struct channel_t *) a;
    const struct channel_t *channel_b = (struct channel_t *) b;

    if (channel_a->unit != channel_b->unit) {
        return channel_a->unit - channel_b->unit;
    }

    return channel_a->circuit - channel_b->circuit;
}

static int minify_write_frames_unoptimized(struct channel_t *channels,
                                           size_t len) {
    for (size_t i = 0; i < len; i++) {
        const struct channel_t        channel = channels[i];
        struct channel_output_state_t *state  = &output_state[i];

        if (frame_is_set(state->pending_send_frame)) {
            int err;
            if ((err = encode_frame(channel.unit, LOR_CHANNEL_ID, channel.circuit, state->pending_send_frame))) {
                return err;
            }

            // null the current frame
            // this ensures each frame is consumed
            state->pending_send_frame = ZERO_FRAME;
        }
    }

    return 0;
}

static int minify_write_frames_optimized(lor_unit_t unit,
                                         struct channel_t *channels,
                                         size_t len) {
    // iterate over each next frame
    // ensure it has not already been processed
    // then, find all channels using a copy of that frame
    // use each channel to set a bitmask
    for (size_t i = 0; i < len; i++) {
        struct channel_output_state_t *base_state = &output_state[i];

        if (!frame_is_set(base_state->pending_send_frame)) {
            continue;
        }

        const struct frame_t base_frame_copy = base_state->pending_send_frame;

        lor_channel_t channel_mask = 0;

        // find all similar values of this frame
        // null their frames_diff entry since the channel will be set in the mask
        for (size_t x = 0; x < len; x++) {
            const struct channel_t        other_channel = channels[x];
            struct channel_output_state_t *other_state  = &output_state[x];

            if (frame_equals(base_state->pending_send_frame, other_state->pending_send_frame, EQUALS_MODE_VALUE)) {
                // set the channel's circuit in the bitmask
                channel_mask |= (1u << other_channel.circuit);

                // this channel no longer needs to write its frame individually
                // it has been merged into the current channel_mask
                // this is what ultimately consumes each next_frame value
                other_state->pending_send_frame = ZERO_FRAME;
            }
        }

        // if channel_mask can fit within an 8 bit mask, encode is as a LOR_CHANNEL_MASK8
        // this prevents writing the empty upper byte and saves bandwidth
        const LORChannelType channel_type = channel_mask <= UINT8_MAX ? LOR_CHANNEL_MASK8 : LOR_CHANNEL_MASK16;

        int err;
        if ((err = encode_frame(unit, channel_type, channel_mask, base_frame_copy))) {
            return err;
        }
    }

    return 0;
}

static bool minify_channels_fit_bitmask(const struct channel_t *channels,
                                        size_t len) {
    static const size_t max_length = sizeof(lor_channel_t) * 8;

    if (len > max_length) {
        return false;
    }

    // ensure that each circuit id fits in the bitmask
    for (size_t i = 0; i < len; i++) {
        if (channels[i].circuit >= max_length) {
            return false;
        }
    }

    return true;
}

static int minify_unit(lor_unit_t unit,
                       struct channel_t *channels,
                       struct frame_t *upcoming_frames,
                       size_t len) {
    int return_code = 0;

    // test if any frames have changed from previous values
    size_t no_change_count = 0;

    for (size_t i = 0; i < len; i++) {
        struct channel_output_state_t *state         = &output_state[i];
        struct frame_t                upcoming_frame = upcoming_frames[i];

        // detect matching frames
        //  include a OR condition for next_frame == NULL, this enables frames switching to NULL value
        //  from being considered "different" frames when NULL frames are effectively no-op values
        if (frame_equals(state->last_sent_frame, upcoming_frame, EQUALS_MODE_STRICT) || !frame_is_set(upcoming_frame)) {
            no_change_count++;
        } else {
            state->pending_send_frame = upcoming_frame;
        }
    }

    // no changes between frames, instantly return
    if (no_change_count == len) {
        goto minify_unit_return;
    }

    const bool fits_in_mask = minify_channels_fit_bitmask(channels, len);

    if (fits_in_mask) {
        return_code = minify_write_frames_optimized(unit, channels, len);
    } else {
        // this is a fallback handler if the channels do not fit in the max bitmask length
        // this writes each frame individually, unoptimized
        // this is arguably the worst case scenario
        return_code = minify_write_frames_unoptimized(channels, len);
    }

    if (return_code) {
        goto minify_unit_return;
    }

    // ensure all frame differences are null
    // otherwise this indicates failure to consume all frames
    for (size_t i = 0; i < len; i++) {
        if (frame_is_set(output_state[i].pending_send_frame)) {
            return_code = LBR_MINIFY_EUNCONDATA;
            goto minify_unit_return;
        }
    }

    minify_unit_return:
    // mark all channels as non-dirty
    // update last sent frame value to new frame value
    for (size_t i = 0; i < len; i++) {
        output_state[i].last_sent_frame = upcoming_frames[i];
    }

    return return_code;
}

static struct channel_t channel_sort_buffer[CHANNEL_BUFFER_MAX_COUNT];
static struct frame_t   upcoming_frames_buffer[CHANNEL_BUFFER_MAX_COUNT];

int minify_frame(struct sequence_t sequence,
                 frame_index_t frame_index) {
    // sort channels by unit+circuit in descending order
    // this allows a iteration loop to easily detect unit "breaks"
    memcpy(channel_sort_buffer, channel_buffer, sizeof(struct channel_t) * channel_buffer_index);

    qsort(channel_sort_buffer, channel_buffer_index, sizeof(struct channel_t), minify_channel_compare);

    // create an array of the new frame values
    // this is derived from the sorted channels array so indexes match
    memset(upcoming_frames_buffer, 0, sizeof(struct frame_t) * channel_buffer_index);

    if (frame_index < sequence.frame_count) {
        for (size_t i = 0; i < channel_buffer_index; i++) {
            upcoming_frames_buffer[i] = channel_sort_buffer[i].frame_data[frame_index];
        }
    }

    // iterate over channels
    // each time the unit changes, push that grouping into #minify_unit
    // start at index 1 to avoid underflowing 0
    size_t last_break = 0;

    for (size_t i = 1; i < channel_buffer_index; i++) {
        const struct channel_t last_channel = channel_sort_buffer[i - 1];

        if (last_channel.unit == channel_sort_buffer[i].unit) {
            continue;
        }

        int err;
        if ((err = minify_unit(last_channel.unit, &channel_sort_buffer[last_break], &upcoming_frames_buffer[last_break], i - last_break))) {
            return err;
        }

        last_break = i;
    }

    // TODO: ensure if last channel is different id, it is always consumed

    // if last_group_index == 0 and channels length > 0
    // then all channels are in a single unit group
    if (last_break == 0 && channel_buffer_index > 0) {
        int err;
        if ((err = minify_unit(channel_sort_buffer[0].unit, channel_sort_buffer, upcoming_frames_buffer, channel_buffer_index))) {
            return err;
        }
    }

    return 0;
}
