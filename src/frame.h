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
#ifndef LIBREORAMA_FRAME_H
#define LIBREORAMA_FRAME_H

#include <stdbool.h>
#include <stddef.h>

#include "effect.h"

struct frame_t {
    lor_channel_action_t action;
    union {
        lor_brightness_t           set_brightness;
        struct frame_effect_fade_t fade;
    };
} __attribute__((packed));

static const struct frame_t FRAME_EMPTY;

bool frame_equals(struct frame_t a,
                  struct frame_t b);

typedef unsigned short frame_index_t;

struct frame_buffer_t {
    unsigned char *data;
    size_t        written_length;
    size_t        max_length;
};

static const struct frame_buffer_t FRAME_BUFFER_EMPTY;

void frame_buffer_free(struct frame_buffer_t *frame_buffer);

int frame_buffer_alloc(struct frame_buffer_t *frame_buffer,
                       size_t initial_length);

int frame_buffer_get_blob(struct frame_buffer_t *frame_buffer,
                          unsigned char **blob,
                          size_t blob_length);

int frame_buffer_reset(struct frame_buffer_t *frame_buffer);

#endif //LIBREORAMA_FRAME_H
