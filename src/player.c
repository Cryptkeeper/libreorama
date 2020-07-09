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
#include "player.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "audio.h"
#include "err.h"
#include "lbrerr.h"
#include "encode.h"
#include "file.h"

static int player_load_sequence_file(struct sequence_t *current_sequence,
                                     const char *sequence_file,
                                     char **audio_file_hint,
                                     enum sequence_type_t *sequence_type) {
    // locate a the last dot char in the string, if any
    // this is used to locate the file extension for determing the sequence type
    const char *dot = strrchr(sequence_file, '.');

    // check dot == current_sequence_file to ignore hidden files such as "~/.lms"
    if (dot == NULL || dot == sequence_file) {
        return LBR_PLAYER_EBADEXT;
    }

    *sequence_type = sequence_type_from_file_extension(dot);

    // detect failure to match the file extension to a sequence type
    // halt playback since otherwise only the audio would be played
    if (*sequence_type == SEQUENCE_TYPE_UNKNOWN) {
        return LBR_PLAYER_EUNSUPEXT;
    }

    const sequence_loader_t sequence_loader = sequence_type_get_loader(*sequence_type);

    // pass off to whatever function is provided by #sequence_type_get_loader
    int err;
    if ((err = sequence_loader(sequence_file, audio_file_hint, current_sequence))) {
        return err;
    }

    // shrink the sequence before it is passed back to the caller
    // this minimizes & merges frame_data allocations internally
    if ((err = sequence_merge_frame_data(current_sequence))) {
        return err;
    }

    return 0;
}

static int player_load_audio_file(struct player_t *player,
                                  const char *sequence_file,
                                  char *audio_file_hint) {
    ALenum al_err;

    // if an AL buffer is already initialized, unload it first
    if (player->has_al_buffer) {
        player->has_al_buffer = false;

        // unqueue the buffer from the active source
        // otherwise the delete will fail since it is considered in use
        alSourceUnqueueBuffers(player->al_source, 1, &player->current_al_buffer);

        if ((al_err = al_get_error())) {
            al_perror(al_err, "failed to unqueue previous player buffer from source");
            return LBR_ESPERR;
        }

        // delete the buffer, freeing the memory
        alDeleteBuffers(1, &player->current_al_buffer);

        if ((al_err = al_get_error())) {
            al_perror(al_err, "failed to delete previous player buffer");
            return LBR_ESPERR;
        }
    }

    // use #audio_find_sequence_file to determine if the file exists
    // if it doesn't, it will internally attempt to locate it at another path
    char *audio_file_or_fallback = NULL;

    int err;
    if ((err = audio_find_sequence_file(sequence_file, audio_file_hint, &audio_file_or_fallback))) {
        // ensure audio_file_hint is always freed
        free(audio_file_hint);

        return err;
    }

    printf("using audio file: %s\n", audio_file_or_fallback);

    player->current_al_buffer = alutCreateBufferFromFile(audio_file_or_fallback);

    // audio_file_or_fallback is the audio_file pointer or a newly allocated one
    // as such, audio_file should always be freed, but audio_file_or_fallback
    // is only freed if it is not the same pointer as audio_file_hint
    if (audio_file_or_fallback != audio_file_hint) {
        free(audio_file_or_fallback);
    }

    free(audio_file_hint);

    // test for buffering errors
    if ((al_err = al_get_error())) {
        al_perror(al_err, "failed to buffer audio file");
        return LBR_ESPERR;
    }

    // only flag has_al_buffer as true if #al_get_error returns ok
    // otherwise the current_al_buffer value may be invalid but flagged as set
    player->has_al_buffer = true;

    // assign the OpenAL to the source
    // this enables #player_start to simply play the source to start
    alSourcei(player->al_source, AL_BUFFER, player->current_al_buffer);

    if ((al_err = al_get_error())) {
        al_perror(al_err, "failed to assign OpenAL source buffer");
        return LBR_ESPERR;
    }

    return 0;
}

static int player_reset_frame_buffer(player_frame_interrupt_t frame_interrupt,
                                     struct frame_buffer_t *frame_buffer,
                                     unsigned short step_time_ms) {
    int err;
    if ((err = encode_reset_frame(frame_buffer))) {
        return err;
    }
    if ((err = frame_interrupt(step_time_ms))) {
        return err;
    }
    return 0;
}

int player_init(struct player_t *player,
                bool is_infinite_loop,
                const char *show_file_path) {
    player->is_infinite_loop = is_infinite_loop;

    // generate the single OpenAL source
    // this is used for all player playback behavior
    alGenSources(1, &player->al_source);

    ALenum err;
    if ((err = al_get_error())) {
        al_perror(err, "failed to generate OpenAL source");
        return LBR_ESPERR;
    }

    // only flag has_al_source as true if initialized without error
    player->has_al_source = true;

    // open the show file for reading
    FILE *show_file = fopen(show_file_path, "rb");

    if (show_file == NULL) {
        return LBR_EERRNO;
    }

    // read each line of the show file into a string array
    // this internally allocates and must be freed
    // see #player_free
    player->sequence_files = freadlines(show_file, &player->sequence_files_cnt);

    // ensure file is closed
    fclose(show_file);

    // test for null result which indicates an error
    if (player->sequence_files == NULL) {
        return LBR_EERRNO;
    }

    // test if the show file is empty
    if (player->sequence_files_cnt == 0) {
        return LBR_PLAYER_ESHOWEMPTY;
    }

    return 0;
}

bool player_has_next(struct player_t *player) {
    // no need to check is_infinite_loop since sequence_files_cur is always wrapped by #player_start
    return player->sequence_files_cur < player->sequence_files_cnt;
}

int player_start(struct player_t *player,
                 player_frame_interrupt_t frame_interrupt,
                 struct frame_buffer_t *frame_buffer,
                 unsigned short time_correction_ms) {
    const char *current_sequence_file = player->sequence_files[player->sequence_files_cur];

    // ready the current_sequence value for loading
    // pass a step_time_ms default value of 50ms (20 FPS)
    // this provides a minimum step time for the program
    struct sequence_t current_sequence = (struct sequence_t) {
            .step_time_ms = 50,
    };

    // load the sequence file into memory
    // this will buffer the initial data, and remainder is streamed during updates
    enum sequence_type_t sequence_type;

    char *audio_file_hint = NULL;

    int err;
    if ((err = player_load_sequence_file(&current_sequence, current_sequence_file, &audio_file_hint, &sequence_type))) {
        // ensure the allocated audio_file_hint buf is freed
        free(audio_file_hint);
        return err;
    }

    printf("sequence_file: %s\n", current_sequence_file);
    printf("sequence_type: %s\n", sequence_type_string(sequence_type));
    printf("audio_file_hint: %s\n", audio_file_hint);
    printf("step_time_ms: %dms (%d FPS)\n", current_sequence.step_time_ms, 1000 / current_sequence.step_time_ms);
    printf("frame_count: %d\n", current_sequence.frame_count);
    printf("channels_count: %zu\n", current_sequence.channels_count);

    // attempt to load audio file provided by determined sequence type
    // this will delegate or fallback internally as needed
    if ((err = player_load_audio_file(player, current_sequence_file, audio_file_hint))) {
        return err;
    }

    printf("playing...\n");

    // notify OpenAL to start source playback
    // OpenAL will automatically stop playback at EOF
    alSourcePlay(player->al_source);

    ALenum al_err;
    if ((al_err = al_get_error())) {
        al_perror(al_err, "failed to play OpenAL source");
        return LBR_ESPERR;
    }

    ALint source_state;

    // construct a timespec copy of step_time_ms
    // this is used by the downstream nanosleep calls
    struct timespec step_time;

    step_time.tv_sec  = current_sequence.step_time_ms / 1000;
    step_time.tv_nsec = (long) (current_sequence.step_time_ms % 1000) * 1000000;

    // convert time_correction_ms into its corresponding frame_index_t
    // use this as a starting point to (optionally) shift forward
    frame_index_t frame_index = time_correction_ms / current_sequence.step_time_ms;

    printf("initial frame_index: %u\n", frame_index);

    // reset the initial output state
    // otherwise channels may still be active when initially booted
    if ((err = player_reset_frame_buffer(frame_interrupt, frame_buffer, current_sequence.step_time_ms))) {
        return err;
    }

    while (true) {
        // write the current frame index into the frame_buf
        // pass an interrupt call back to the parent
        if ((err = encode_sequence_frame(frame_buffer, &current_sequence, frame_index))) {
            return err;
        }

        if ((err = frame_interrupt(current_sequence.step_time_ms))) {
            return err;
        }

        // move to next frame for next iteration
        frame_index++;

        // test if playback is still happening
        // this defers to the audio time rather than the sequence
        // this helps ensure a consistent result
        alGetSourcei(player->al_source, AL_SOURCE_STATE, &source_state);

        if ((al_err = al_get_error())) {
            al_perror(al_err, "failed to get player source state");
            return LBR_ESPERR;
        }

        // break loop prior to sleep
        if (source_state != AL_PLAYING) {
            break;
        }

        // sleep after each loop iteration
        // this time comes from #player_load_sequence_file and should be considered dynamic
        if (nanosleep(&step_time, NULL)) {
            return LBR_EERRNO;
        }
    }

    // encode a reset frame and trigger a final interrupt
    // this resets any active light output states
    if ((err = player_reset_frame_buffer(frame_interrupt, frame_buffer, current_sequence.step_time_ms))) {
        return err;
    }

    // free the current sequence
    // this frees any internal allocations and removes dangling pointers
    sequence_free(&current_sequence);

    // increment at last step to avoid skipping 0 index
    player->sequence_files_cur++;

    // wrap the sequence_files_cur value in infinite loop to avoid
    // index out of bounds error when referencing sequence_files
    if (player->is_infinite_loop) {
        player->sequence_files_cur %= player->sequence_files_cnt;
    }

    return 0;
}

void player_free(const struct player_t *player) {
    // fixme: current_sequence is not freed by #player_free

    // free the string array generated by #freadlines
    freadlines_free(player->sequence_files, player->sequence_files_cnt);

    ALenum err;

    // test the source & buffer fields of player for initialization
    // delete each if set
    if (player->has_al_buffer) {
        alDeleteBuffers(1, &player->current_al_buffer);

        if ((err = al_get_error())) {
            al_perror(err, "failed to delete current OpenAL buffer");
        }
    }

    if (player->has_al_source) {
        alDeleteSources(1, &player->al_source);

        if ((err = al_get_error())) {
            al_perror(err, "failed to delete OpenAL source");
        }
    }
}