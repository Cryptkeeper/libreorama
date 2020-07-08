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

frame_t channel_get_frame(const struct channel_t *channel,
                          frame_index_t frame_index) {
    if (frame_index >= channel->frame_data_max_index) {
        return 0;
    } else {
        return channel->frame_data[frame_index];
    }
}

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

int sequence_add_channel(struct sequence_t *sequence,
                         lor_unit_t unit,
                         lor_channel_t channel,
                         size_t *channel_index) {
    struct channel_t *channels = realloc(sequence->channels, sizeof(struct channel_t) * (sequence->channels_count + 1));

    if (channels == NULL) {
        return 1;
    }

    sequence->channels = channels;
    sequence->channels[sequence->channels_count] = (struct channel_t) {
            .unit = unit,
            .channel = channel,
            .frame_data = NULL,
            .last_frame = 0, // todo: const value?
            .frame_data_max_index = 0,
            .frame_data_last_index = 0
    };

    // write out the previous value before incrementing
    *channel_index = sequence->channels_count;

    sequence->channels_count++;

    return 0;
}

// todo: move to use channel directly
int sequence_frame_data_set(struct sequence_t *sequence,
                            size_t channel_index,
                            frame_index_t frame_index_start,
                            frame_index_t frame_index_end,
                            frame_t frame) {
    struct channel_t *channel = &sequence->channels[channel_index];

    // frame_index_start & frame_index_end represent an array of the same value
    // ensure the full block fits within the current allocated frame_data
    // otherwise, resize the array to ensure frame_index_end fits in it
    if (frame_index_end >= channel->frame_data_max_index) {
        frame_index_t resized_max_index = channel->frame_data_max_index;

        // safely handle initial 0 value
        // realloc at Nx the size each time ceiling is hit
        // #sequence_frame_data_shrink will handle properly resizing the buffers
        if (resized_max_index == 0) {
            // default to a frame_data allocation of N frames
            resized_max_index = 512;
        } else {
            resized_max_index *= 2;
        }

        frame_t *frame_data = realloc(channel->frame_data, sizeof(frame_t) * resized_max_index);

        if (frame_data == NULL) {
            return 1;
        }

        // ensure the newly allocated memory portion is zeroed
        // offset from frame_data + previous max index in bytes to avoid clearing previous data
        const frame_index_t frame_index_diff = resized_max_index - channel->frame_data_max_index;

        memset(frame_data + (sizeof(frame_t) * channel->frame_data_max_index), 0, sizeof(frame_t) * frame_index_diff);

        // adjust dangling pointers and update ceiling value
        channel->frame_data           = frame_data;
        channel->frame_data_max_index = resized_max_index;
    }

    // set each frame index between start and end
    for (frame_index_t frame_index = frame_index_start; frame_index < frame_index_end; frame_index++) {
        channel->frame_data[frame_index] = frame;
    }

    // frame_data_last_index is used to track where the frame_data buffer ends
    // this is used later by #sequence_frame_data_shrink
    channel->frame_data_last_index = frame_index_end;

    return 0;
}

// todo: move to use channel directly
int sequence_frame_data_shrink(struct sequence_t *sequence) {
    // resize frame_data of each channel_t to perfectly fit the final frame data
    for (size_t i = 0; i < sequence->channels_count; i++) {
        struct channel_t *channel = &sequence->channels[i];

        if (channel->frame_data_last_index < channel->frame_data_max_index) {
            // todo: allocate memory in neighboring blocks to prevent memory read jumps
            frame_t *frame_data = realloc(channel->frame_data, sizeof(frame_t) * channel->frame_data_last_index);

            if (frame_data == NULL) {
                return 1;
            }

            channel->frame_data           = frame_data;
            channel->frame_data_max_index = channel->frame_data_last_index;
        }
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