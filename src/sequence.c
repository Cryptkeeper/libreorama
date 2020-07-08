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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "seqtypes/lormedia.h"

void sequence_free(struct sequence_t *sequence) {
    if (sequence->merged_frame_data != NULL) {
        free(sequence->merged_frame_data);

        // mark as null to remove dangling pointer
        // subsection pointers within channel_t are still dangling
        sequence->merged_frame_data = NULL;
    }

    if (sequence->channels_count > 0) {
        free(sequence->channels);

        // mark as empty to prevent double free bugs
        sequence->channels       = NULL;
        sequence->channels_count = 0;
    }
}

int sequence_add_channel(struct sequence_t *sequence,
                         lor_unit_t unit,
                         lor_channel_t channel_i,
                         struct channel_t **channel) {
    struct channel_t *channels = realloc(sequence->channels, sizeof(struct channel_t) * (sequence->channels_count + 1));

    if (channels == NULL) {
        return 1;
    }

    sequence->channels = channels;
    sequence->channels_count++;

    struct channel_t *new_channel = &channels[sequence->channels_count - 1];

    // initialize the channel using a basic zeroed struct const
    *new_channel = CHANNEL_EMPTY;
    new_channel->unit    = unit;
    new_channel->channel = channel_i;

    // update the pointer to the newly allocated channel_t
    *channel = new_channel;

    return 0;
}

int sequence_merge_frame_data(struct sequence_t *sequence) {
    // calculate the total count of all frame_data values
    // this is used to allocate a single block of memory
    size_t sum_count = 0, sum_count_max = 0;

    for (size_t i = 0; i < sequence->channels_count; i++) {
        struct channel_t channel = sequence->channels[i];

        sum_count += channel.frame_data_count;
        sum_count_max += channel.frame_data_count_max;
    }

    printf("merging %zu frame data allocations (%zu bytes) into %zu bytes\n", sequence->channels_count,
           sizeof(frame_t) * sum_count_max, sizeof(frame_t) * sum_count);

    // allocate a single block of sum_count size
    // various subsections of this will be passed to each channel
    frame_t *merged_frame_data = malloc(sizeof(frame_t) * sum_count);

    if (merged_frame_data == NULL) {
        return 1;
    }

    size_t merged_frame_data_index = 0;

    for (size_t i = 0; i < sequence->channels_count; i++) {
        struct channel_t *channel = &sequence->channels[i];

        // copy channel's frame_data into merged_frame_data
        // free the previous memory allocation
        memcpy(merged_frame_data + merged_frame_data_index, channel->frame_data, channel->frame_data_count);
        
        free(channel->frame_data);

        // point channel's frame_data to the new slice
        // update max count value to match written length
        channel->frame_data           = merged_frame_data + merged_frame_data_index;
        channel->frame_data_count_max = channel->frame_data_count;

        // shift the writer index forward by the copied length
        merged_frame_data_index += channel->frame_data_count;
    }

    // ensure the final index value matches the sum
    // this ensures all writes are correctly aligned
    if (merged_frame_data_index != sum_count) {
        fprintf(stderr, "final writer index %zu does not match sum %zu\n", merged_frame_data_index, sum_count);
        return 1;
    }

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