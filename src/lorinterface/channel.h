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

#define CHANNEL_BUFFER_MAX_COUNT 128

struct channel_t {
    lor_unit_t     unit;
    lor_channel_t  circuit;
    struct frame_t *frame_data;
};

extern struct channel_t channel_buffer[CHANNEL_BUFFER_MAX_COUNT];
extern size_t           channel_buffer_index;

int channel_buffer_request(lor_unit_t unit,
                           lor_channel_t circuit,
                           frame_index_t frame_count,
                           struct channel_t **channel);

void channel_buffer_reset();

#endif //LIBREORAMA_CHANNEL_H
