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

#include "../err/lbr.h"
#include "encode.h"

#define MINIFY_MAX_BITMASK_LENGTH 16

static int minify_channel_compare(const void *a,
                                  const void *b) {
    const struct channel_t *channel_a = (struct channel_t *) a;
    const struct channel_t *channel_b = (struct channel_t *) b;

    if (channel_a->unit != channel_b->unit) {
        return channel_a->unit - channel_b->unit;
    }

    return channel_a->circuit - channel_b->circuit;
}

static int minify_write_frames_unoptimized(struct frame_buffer_t *frame_buffer,
                                           struct channel_t **channels,
                                           size_t len) {
    for (size_t i = 0; i < len; i++) {
        struct channel_t *channel = channels[i];

        if (channel->next_frame != NULL) {
            int err;
            if ((err = encode_frame(frame_buffer, channel->unit, LOR_CHANNEL_ID, channel->circuit, *channel->next_frame))) {
                return err;
            }

            // null the current frame
            // this ensures each frame is consumed
            channel->next_frame = NULL;
        }
    }

    return 0;
}

static int minify_write_frames_optimized(struct frame_buffer_t *frame_buffer,
                                         lor_unit_t unit,
                                         struct channel_t **channels,
                                         size_t len) {
    // iterate over each next frame
    // ensure it has not already been processed
    // then, find all channels using a copy of that frame
    // use each channel to set a bitmask
    for (size_t i = 0; i < len; i++) {
        struct channel_t *base_channel = channels[i];

        if (base_channel->next_frame == NULL) {
            continue;
        }

        const struct frame_t base_frame_copy = *base_channel->next_frame;
        lor_channel_t        channel_mask    = 0;

        // find all similar values of this frame
        // null their frames_diff entry since the channel will be set in the mask
        for (size_t x = 0; x < len; x++) {
            struct channel_t *other_channel = channels[x];

            if (frame_equals(base_channel->next_frame, other_channel->next_frame, EQUALS_MODE_VALUE)) {
                // set the channel's circuit in the bitmask
                channel_mask |= (1u << other_channel->circuit);

                // this channel no longer needs to write its frame individually
                // it has been merged into the current channel_mask
                // this is what ultimately consumes each next_frame value
                other_channel->next_frame = NULL;
            }
        }

        // if channel_mask can fit within an 8 bit mask, encode is as a LOR_CHANNEL_MASK8
        // this prevents writing the empty upper byte and saves bandwidth
        const LORChannelType channel_type = channel_mask <= UINT8_MAX ? LOR_CHANNEL_MASK8 : LOR_CHANNEL_MASK16;

        int err;
        if ((err = encode_frame(frame_buffer, unit, channel_type, channel_mask, base_frame_copy))) {
            return err;
        }
    }

    return 0;
}

static bool minify_channels_fit_bitmask(struct channel_t **channels,
                                        size_t len) {
    if (len > MINIFY_MAX_BITMASK_LENGTH) {
        return false;
    }

    // ensure that each circuit id fits in the bitmask
    // len > MINIFY_MAX_BITMASK_LENGTH is a basic pre-check to avoid obvious false cases
    for (size_t i = 0; i < len; i++) {
        if (channels[i]->circuit >= MINIFY_MAX_BITMASK_LENGTH) {
            return false;
        }
    }

    return true;
}

static int minify_unit(struct frame_buffer_t *frame_buffer,
                       lor_unit_t unit,
                       struct channel_t **channels,
                       struct frame_t **frames,
                       size_t len) {
    int return_code = 0;

    // test if any frames have changed from previous values
    size_t no_change_count = 0;

    for (size_t i = 0; i < len; i++) {
        const struct frame_t *previous_frame = channels[i]->last_sent_frame;
        struct frame_t       *next_frame     = frames[i];

        // detect matching frames
        //  include a OR condition for next_frame == NULL, this enables frames switching to NULL value
        //  from being considered "different" frames when NULL frames are effectively no-op values
        if (frame_equals(previous_frame, next_frame, EQUALS_MODE_STRICT) || next_frame == NULL) {
            no_change_count++;
        } else {
            channels[i]->next_frame = next_frame;
        }
    }

    // no changes between frames, instantly return
    if (no_change_count == len) {
        goto minify_unit_return;
    }

    const bool fits_in_mask = minify_channels_fit_bitmask(channels, len);

    if (fits_in_mask) {
        return_code = minify_write_frames_optimized(frame_buffer, unit, channels, len);
    } else {
        // this is a fallback handler if the channels do not fit in the max bitmask length
        // this writes each frame individually, unoptimized
        // this is arguably the worst case scenario
        return_code = minify_write_frames_unoptimized(frame_buffer, channels, len);
    }

    if (return_code) {
        goto minify_unit_return;
    }

    // ensure all frame differences are null
    // otherwise this indicates failure to consume all frames
    for (size_t i = 0; i < len; i++) {
        if (channels[i]->next_frame != NULL) {
            return_code = LBR_MINIFY_EUNCONDATA;
            goto minify_unit_return;
        }
    }

    minify_unit_return:
    // mark all channels as non-dirty
    // update last sent frame value to new frame value
    for (size_t i = 0; i < len; i++) {
        channels[i]->last_sent_frame = frames[i];
    }

    return return_code;
}

// todo: reduce allocations
int minify_frame(struct frame_buffer_t *frame_buffer,
                 const struct sequence_t *sequence,
                 frame_index_t frame_index) {
    int return_code = 0;

    // sort channels by unit+circuit in descending order
    // this allows a iteration loop to easily detect unit "breaks"
    struct channel_t **channels = malloc(sizeof(struct channel_t *) * sequence->channels_count);

    if (channels == NULL) {
        return LBR_EERRNO;
    }

    for (size_t i = 0; i < sequence->channels_count; i++) {
        channels[i] = &sequence->channels[i];
    }

    qsort(channels, sequence->channels_count, sizeof(struct channel_t *), minify_channel_compare);

    // create an array of the new frame values
    // this is derived from the sorted channels array so indexes match
    struct frame_t **frames = malloc(sizeof(struct frame_t *) * sequence->channels_count);

    if (frames == NULL) {
        return_code = LBR_EERRNO;
        goto minify_frame_return;
    }

    for (size_t i = 0; i < sequence->channels_count; i++) {
        frames[i] = channel_get_frame(channels[i], frame_index);
    }

    // iterate over channels
    // each time the unit changes, push that grouping into #minify_unit
    // start at index 1 to avoid underflowing 0
    size_t last_group_index = 0;

    for (size_t i = 1; i < sequence->channels_count; i++) {
        const struct channel_t *last_channel = channels[i - 1];

        if (last_channel->unit != channels[i]->unit) {
            int err;
            if ((err = minify_unit(frame_buffer, last_channel->unit,
                                   channels + last_group_index, frames + last_group_index, i - last_group_index))) {
                return_code = err;
                goto minify_frame_return;
            }

            last_group_index = i;
        }
    }

    // if last_group_index == 0 and channels length > 0
    // then all channels are in a single unit group
    if (last_group_index == 0 && sequence->channels_count > 0) {
        int err;
        if ((err = minify_unit(frame_buffer, channels[0]->unit, channels, frames, sequence->channels_count))) {
            return_code = err;
            goto minify_frame_return;
        }
    }

    minify_frame_return:
    free(channels);
    free(frames);

    return return_code;
}