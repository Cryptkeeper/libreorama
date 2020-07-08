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
#include "frame.h"

#include <stdlib.h>
#include <stdio.h>

const struct frame_buffer_t FRAME_BUFFER_EMPTY = (struct frame_buffer_t) {
        .data = NULL,
        .written_length = 0,
        .max_length = 0
};

void frame_buffer_free(struct frame_buffer_t *frame_buffer) {
    if (frame_buffer->data != NULL) {
        free(frame_buffer->data);

        // remove dangling pointer and reset indexes
        frame_buffer->data           = NULL;
        frame_buffer->written_length = 0;
        frame_buffer->max_length     = 0;
    }
}

int frame_buffer_alloc(struct frame_buffer_t *frame_buffer,
                       size_t initial_length) {
    frame_buffer->data       = malloc(initial_length);
    frame_buffer->max_length = initial_length;
    if (frame_buffer->data == NULL) {
        perror("failed to malloc initial frame buffer length");
        return 1;
    }
    return 0;
}

// frame_buffer_t is backed by a unsigned char *
// each write requests a blob, which is a subsection of that backing memory allocation
// #frame_buffer_get_blob ensures each blob is a minimum length, otherwise reallocing the backing memory allocation
// this ensures each write is safely within bounds (assuming each write is within the defined maximum length)
int frame_buffer_get_blob(struct frame_buffer_t *frame_buffer,
                          unsigned char **blob,
                          size_t blob_length) {
    return 0;
}

int frame_buffer_reset(struct frame_buffer_t *frame_buffer) {
    return 0;
}