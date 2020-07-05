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
#include "sequence.h"

#include <string.h>

struct sequence_t sequence_init() {
    // pass a step_time_ms default value of 50ms (20 FPS)
    // this provides a minimum step time for the program
    return (struct sequence_t) {
            .step_time_ms = 50
    };
}

char *sequence_type_string(enum sequence_type_t sequence_type) {
    switch (sequence_type) {
        case SEQUENCE_TYPE_LOR_MEDIA:
            return "Light-O-Rama Media Sequence (lms)";
        case SEQUENCE_TYPE_FALCON:
            return "Falcon Sequence (fseq)";
        case SEQUENCE_TYPE_UNKNOWN:
        default:
            return "unknown";
    }
}

enum sequence_type_t sequence_type_from_file_extension(const char *file_ext) {
    if (strncmp(file_ext, ".lms", 4) == 0) {
        return SEQUENCE_TYPE_LOR_MEDIA;
    } else if (strncmp(file_ext, ".fseq", 5) == 0) {
        return SEQUENCE_TYPE_FALCON;
    } else {
        return SEQUENCE_TYPE_UNKNOWN;
    }
}

sequence_loader_t sequence_type_get_loader(enum sequence_type_t sequence_type) {
    switch (sequence_type) {
        case SEQUENCE_TYPE_LOR_MEDIA:
            return lormedia_sequence_load;
        default:
            return NULL;
    }
}