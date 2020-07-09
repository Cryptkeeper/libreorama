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

#include <stdio.h>

#include <lightorama/io.h>
#include <lightorama/brightness_curve.h>

// assumes no individual lor_write_* call will use more than 16 bytes
// see https://github.com/Cryptkeeper/liblightorama#memory-allocations
#define ENCODE_MAXIMUM_BLOB_LENGTH 16

static lor_brightness_t encode_brightness(unsigned char brightness) {
    // target lor_brightness_curve_t used for encoding brightness values
    return lor_brightness_curve_squared((float) brightness / 255.0f);
}

static size_t encode_frame(unsigned char *blob,
                           struct channel_t *channel,
                           struct frame_t frame) {
    switch (frame.action) {
        case LOR_ACTION_CHANNEL_SET_BRIGHTNESS:
            return lor_write_channel_set_brightness(channel->unit, LOR_CHANNEL_ID, channel->channel, encode_brightness(frame.fade.to), blob);

        case LOR_ACTION_CHANNEL_FADE:
            return lor_write_channel_fade(channel->unit, LOR_CHANNEL_ID, channel->channel, encode_brightness(frame.fade.from), encode_brightness(frame.fade.to), frame.fade.duration, blob);

        case LOR_ACTION_CHANNEL_ON:
        case LOR_ACTION_CHANNEL_SHIMMER:
        case LOR_ACTION_CHANNEL_TWINKLE:
            return lor_write_channel_action(channel->unit, LOR_CHANNEL_ID, channel->channel, frame.action, blob);

        default:
            fprintf(stderr, "failed to encode frame, unsupported action: %d\n", frame.action);
            return 0;
    }
}

int encode_sequence_frame(struct frame_buffer_t *frame_buffer,
                          const struct sequence_t *sequence,
                          frame_index_t frame_index) {
    unsigned char *blob = NULL;

    // automatically push heartbeat messages into the frame buffer
    // this is timed for every 500ms, based off the frame index
    if (frame_index % (500 / sequence->step_time_ms) == 0) {
        if (frame_buffer_get_blob(frame_buffer, &blob, ENCODE_MAXIMUM_BLOB_LENGTH)) {
            perror("failed to get frame buffer blob (heartbeat)");
            return 1;
        }

        frame_buffer->written_length += lor_write_heartbeat(blob);
    }

    for (size_t i = 0; i < sequence->channels_count; i++) {
        struct channel_t *channel = &sequence->channels[i];
        struct frame_t   *frame   = channel_get_frame(channel, frame_index);

        if (frame == NULL) {
            continue;
        }

        // prevent writing duplicate updates
        // the LOR protocol is stateful and this causes "reset" glitches
        if (channel->last_sent_frame != NULL && frame_equals(*channel->last_sent_frame, *frame)) {
            continue;
        }

        channel->last_sent_frame = frame;

        if (frame_buffer_get_blob(frame_buffer, &blob, ENCODE_MAXIMUM_BLOB_LENGTH)) {
            perror("failed to get frame buffer blob (channel frame)");
            return 1;
        }

        size_t written;
        if ((written = encode_frame(blob, channel, *frame)) == 0) {
            // 0 returns indicate internal error
            // since frames at this point are non-empty, something should always be written
            return 1;
        }

        if (written > ENCODE_MAXIMUM_BLOB_LENGTH) {
            fprintf(stderr, "written %zu exceeds maximum blob length! %d\n", written, ENCODE_MAXIMUM_BLOB_LENGTH);
            return 1;
        }

        // move the writer index forward
        // written may be >=0
        frame_buffer->written_length += written;
    }

    return 0;
}

int encode_reset_frame(struct frame_buffer_t *frame_buffer) {
    unsigned char *blob = NULL;

    if (frame_buffer_get_blob(frame_buffer, &blob, ENCODE_MAXIMUM_BLOB_LENGTH)) {
        perror("failed to get frame buffer blob (reset)");
        return 1;
    }

    frame_buffer->written_length += lor_write_unit_action(LOR_UNIT_ID_BROADCAST, LOR_ACTION_UNIT_OFF, blob);

    return 0;
}