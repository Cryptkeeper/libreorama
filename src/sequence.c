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

#include <stdlib.h>
#include <string.h>

#include "seqtypes/lormedia.h"

void sequence_free(struct sequence_t *sequence) {
    if (sequence->channels_count > 0) {
        for (size_t i = 0; i < sequence->channels_count; i++) {
            free(sequence->channels[i].frame_data);
        }

        free(sequence->channels);

        // mark as empty to prevent double free bugs
        sequence->channels_count = 0;
    }
}

int sequence_add_index(struct sequence_t *sequence,
                       unsigned char unit,
                       unsigned short channel,
                       unsigned short *index) {
    struct channel_t *channels = realloc(sequence->channels, sizeof(struct channel_t) * (sequence->channels_count + 1));

    if (channels == NULL) {
        return 1;
    }

    sequence->channels = channels;
    sequence->channels[sequence->channels_count] = (struct channel_t) {
            .unit = unit,
            .channel = channel,
            .frame_data = NULL // todo: alloc
    };

    // write out the previous value before incrementing
    *index = sequence->channels_count;

    sequence->channels_count++;

    return 0;
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