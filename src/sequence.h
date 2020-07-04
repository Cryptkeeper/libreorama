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
#ifndef LIBREORAMA_SEQUENCE_H
#define LIBREORAMA_SEQUENCE_H

#include <stddef.h>

#include "seqtypes/lormedia.h"

enum sequence_type_t {
    SEQUENCE_TYPE_LOR_MEDIA,
    SEQUENCE_TYPE_FALCON,
    SEQUENCE_TYPE_UNKNOWN,
};

char *sequence_type_string(enum sequence_type_t sequence_type);

enum sequence_type_t sequence_type_from_file_extension(const char *file_ext);

typedef int (*sequence_loader_t)(const char *sequence_file,
                                 char **audio_file,
                                 unsigned long *step_time_ms,
                                 unsigned long *frame_count);

sequence_loader_t sequence_type_get_loader(enum sequence_type_t sequence_type);

#endif //LIBREORAMA_SEQUENCE_H
