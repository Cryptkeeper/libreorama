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
#include "controller.h"

#include <lightorama/brightness_curve.h>
#include <lightorama/protocol.h>
#include <lightorama/io.h>

// todo: rename encoder?

size_t controller_write_frame(unsigned char *frame_buf,
                              const struct sequence_t *sequence,
                              frame_index_t frame_index) {
    const unsigned char *frame_buf_init = frame_buf;

    // automatically push heartbeat messages into the frame buffer
    // this is timed for every 500ms, based off the frame index
    if (frame_index % (500 / sequence->step_time_ms) == 0) {
        frame_buf += lor_write_heartbeat(frame_buf);
    }

    for (size_t i = 0; i < sequence->channels_count; i++) {
        struct channel_t *channel = &sequence->channels[i];
        const frame_t    frame    = channel_get_frame(channel, frame_index);

        // prevent writing duplicate updates
        // the LOR protocol is stateful and this causes "reset" glithes
        if (channel->last_frame == frame) {
            channel->last_frame = frame;

            // encode the frame data using liblightorama
            lor_brightness_t lor_brightness = lor_brightness_curve_linear((float) frame / 100.0f);

            // offset channel by 1 since configurations start at index 1
            //  but the LOR network protocol starts at index 0
            frame_buf += lor_write_channel_set_brightness(channel->unit, LOR_CHANNEL_ID, channel->channel - 1, lor_brightness, frame_buf);
        }
    }

    // return the difference in the pointer as length in bytes
    return frame_buf - frame_buf_init;
}