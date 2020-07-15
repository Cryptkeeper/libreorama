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

#include <lightorama/io.h>
#include <lightorama/brightness_curve.h>

#include "../err/lbr.h"

// assumes no individual lor_write_* call will use more than 16 bytes
// see https://github.com/Cryptkeeper/liblightorama#memory-allocations
#define ENCODE_MAXIMUM_BLOB_LENGTH 16

static lor_brightness_t encode_brightness(unsigned char brightness) {
    return lor_brightness_curve_squared((float) brightness / 255.0f);
}

int encode_frame(struct frame_buffer_t *frame_buffer,
                 lor_unit_t unit,
                 LORChannelType channel_type,
                 lor_channel_t channel,
                 struct frame_t frame) {
    unsigned char *blob = NULL;

    int err;
    if ((err = frame_buffer_get_blob(frame_buffer, &blob, ENCODE_MAXIMUM_BLOB_LENGTH))) {
        return err;
    }

    size_t written;

    switch (frame.action) {
        case LOR_ACTION_CHANNEL_SET_BRIGHTNESS:
            written = lor_write_channel_set_brightness(unit, channel_type, channel, encode_brightness(frame.fade.to), blob);
            break;

        case LOR_ACTION_CHANNEL_FADE:
            written = lor_write_channel_fade(unit, channel_type, channel, encode_brightness(frame.fade.from), encode_brightness(frame.fade.to), frame.fade.duration, blob);
            break;

        case LOR_ACTION_CHANNEL_ON:
        case LOR_ACTION_CHANNEL_SHIMMER:
        case LOR_ACTION_CHANNEL_TWINKLE:
            written = lor_write_channel_action(unit, channel_type, channel, frame.action, blob);
            break;

        default:
            return LBR_ENCODE_EUNSUPACTION;
    }

    if (written > ENCODE_MAXIMUM_BLOB_LENGTH) {
        return LBR_ENCODE_EBLOBTOOSMALL;
    }

    frame_buffer->written_length += written;

    return 0;
}

int encode_heartbeat_frame(struct frame_buffer_t *frame_buffer,
                           frame_index_t frame_index,
                           unsigned short step_time_ms) {
    unsigned char *blob = NULL;

    int err;
    if ((err = frame_buffer_get_blob(frame_buffer, &blob, ENCODE_MAXIMUM_BLOB_LENGTH))) {
        return err;
    }

    // automatically push heartbeat messages into the frame buffer
    // this is timed for every 500ms, based off the frame index
    if (frame_index % (500 / step_time_ms) == 0) {
        if ((err = frame_buffer_get_blob(frame_buffer, &blob, ENCODE_MAXIMUM_BLOB_LENGTH))) {
            return err;
        }

        frame_buffer->written_length += lor_write_heartbeat(blob);
    }

    return 0;
}

int encode_reset_frame(struct frame_buffer_t *frame_buffer) {
    unsigned char *blob = NULL;

    int err;
    if ((err = frame_buffer_get_blob(frame_buffer, &blob, ENCODE_MAXIMUM_BLOB_LENGTH))) {
        return err;
    }

    frame_buffer->written_length += lor_write_unit_action(LOR_UNIT_ID_BROADCAST, LOR_ACTION_UNIT_OFF, blob);

    return 0;
}