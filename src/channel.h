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
#ifndef LIBREORAMA_CHANNEL_H
#define LIBREORAMA_CHANNEL_H

#include <stdbool.h>

#include <lightorama/protocol.h>

#include "frame.h"

struct channel_t {
    lor_unit_t    unit;
    lor_channel_t channel;
    frame_t       *frame_data;
    frame_index_t frame_data_count;
    frame_index_t frame_data_count_max;
    frame_t       last_frame_data;
    frame_index_t first_frame_offset;
    bool has_first_frame_offset;
};

extern const struct channel_t CHANNEL_EMPTY;

frame_t *channel_get_frame(const struct channel_t *channel,
                           frame_index_t frame_index);

int channel_set_frame_data(struct channel_t *channel,
                           frame_index_t frame_index_start,
                           frame_index_t frame_index_end,
                           frame_t frame);

#endif //LIBREORAMA_CHANNEL_H
