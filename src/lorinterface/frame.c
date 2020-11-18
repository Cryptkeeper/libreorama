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
#include <string.h>

#include "../err/lbr.h"

static struct frame_t *frame_buffer = NULL;
static size_t         frame_buffer_count;

int frame_buffer_request(frame_index_t count,
                         struct frame_t **frames) {
    frame_buffer = realloc(frame_buffer, sizeof(struct frame_t) * (frame_buffer_count + count));

    if (frame_buffer == NULL) {
        return LBR_EERRNO;
    }

    // initialize the allocated portion of the memory to be returned
    // this ensures the returned frame pointers are always safe
    memset(&frame_buffer[frame_buffer_count], 0, sizeof(struct frame_t) * count);

    // checkout a portion of the full buffer
    *frames = &frame_buffer[frame_buffer_count];
    frame_buffer_count += count;

    return 0;
}

void frame_buffer_free() {
    if (frame_buffer != NULL) {
        free(frame_buffer);

        // release any dangling pointers and avoid double free
        frame_buffer = NULL;
    }

    frame_buffer_count = 0;
}
