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
#ifndef LIBREORAMA_ENCODE_H
#define LIBREORAMA_ENCODE_H

#include "../err/al.h"
#include "../player/sequence.h"

#define ENCODE_BUFFER_MAX_LENGTH 4096

extern unsigned char encode_buffer[ENCODE_BUFFER_MAX_LENGTH];
extern size_t        encode_buffer_index;

unsigned char *encode_buffer_write_index();

int encode_buffer_advance(size_t len);

void encode_buffer_reset();

int encode_frame(lor_unit_t unit,
                 enum lor_channel_type_t channel_type,
                 lor_channel_t channel,
                 struct frame_t frame);

int encode_heartbeat_frame(frame_index_t frame_index,
                           unsigned short step_time_ms);

int encode_reset_frame();

#endif //LIBREORAMA_ENCODE_H
