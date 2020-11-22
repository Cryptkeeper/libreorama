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

#include "../err/al.h"
#include "../err/lbr.h"
#include "../lorinterface/encode.h"
#include "../lorinterface/minify.h"
#include "../lorinterface/state.h"
#include "../file.h"
#include "../interval.h"
#include "../seqtypes/lormedia.h"

static size_t       sequence_files_cur;
static ALuint       al_source;
static ALuint       current_al_buffer;
static unsigned int show_loop_counter;
static bool         has_al_source;
static bool         has_al_buffer;

static int player_load_sequence_file(struct sequence_t *current_sequence,
                                     const char *sequence_file,
                                     char **audio_file_hint) {
    // locate a the last dot char in the string, if any
    // this is used to locate the file extension for determing the sequence type
    const char *dot = strrchr(sequence_file, '.');

    // check dot == current_sequence_file to ignore hidden files such as "~/.lms"
    if (dot == NULL || dot == sequence_file) {
        return LBR_PLAYER_EBADEXT;
    }

    // detect failure to match the file extension to a sequence type
    // halt playback since otherwise only the audio would be played
    if (strncmp(dot, ".lms", 4) != 0) {
        return LBR_PLAYER_EUNSUPEXT;
    }

    int err;
    if ((err = lormedia_sequence_load(sequence_file, audio_file_hint, current_sequence))) {
        return err;
    }

    if (channel_buffer_index == 0) {
        return LBR_SEQUENCE_ENOCHANNELS;
    } else if (current_sequence->frame_count == 0) {
        return LBR_SEQUENCE_ENOFRAMES;
    }

    return 0;
}

static int player_load_audio_file(char *audio_file_hint) {
    ALenum al_err;

    // if an AL buffer is already initialized, unload it first
    if (has_al_buffer) {
        has_al_buffer = false;

        // unqueue the buffer from the active source
        // otherwise the delete will fail since it is considered in use
        alSourceUnqueueBuffers(al_source, 1, &current_al_buffer);
        if ((al_err = al_get_error())) {
            al_perror(al_err, "failed to unqueue previous player buffer from source");
            return LBR_ESPERR;
        }

        // delete the buffer, freeing the memory
        alDeleteBuffers(1, &current_al_buffer);
        if ((al_err = al_get_error())) {
            al_perror(al_err, "failed to delete previous player buffer");
            return LBR_ESPERR;
        }
    }

    current_al_buffer = alutCreateBufferFromFile(audio_file_hint);

    // free allocated string
    free(audio_file_hint);

    // test for buffering errors
    if ((al_err = al_get_error())) {
        al_perror(al_err, "failed to buffer audio file");
        return LBR_ESPERR;
    }

    // only flag has_al_buffer as true if #al_get_error returns ok
    // otherwise the current_al_buffer value may be invalid but flagged as set
    has_al_buffer = true;

    // assign the OpenAL to the source
    // this enables #player_start to simply play the source to start
    alSourcei(al_source, AL_BUFFER, current_al_buffer);

    if ((al_err = al_get_error())) {
        al_perror(al_err, "failed to assign OpenAL source buffer");
        return LBR_ESPERR;
    }

    return 0;
}

static int player_reset_encode_buffer(player_frame_interrupt_t frame_interrupt,
                                      unsigned short step_time_ms) {
    int err;
    if ((err = encode_reset_frame())) {
        return err;
    }
    if ((err = frame_interrupt(step_time_ms))) {
        return err;
    }
    return 0;
}

static void player_advance(struct player_t *player) {
    // increment at last step to avoid skipping 0 index
    sequence_files_cur++;

    // wrap the sequence_files_cur value if the show should loop
    // this controls #player_has_next logic by keeping sequence_files_cur < sequence_files_cnt
    // a -1 show_loop_count value indicates and infinite loop
    if (player->show_loop_count == -1 || ++(show_loop_counter) < player->show_loop_count) {
        sequence_files_cur %= player->sequence_files_cnt;
    }
}

int player_init(struct player_t *player,
                const char *show_file_path) {
    // generate the single OpenAL source
    // this is used for all player playback behavior
    alGenSources(1, &al_source);

    ALenum err;
    if ((err = al_get_error())) {
        al_perror(err, "failed to generate OpenAL source");
        return LBR_ESPERR;
    }

    // only flag has_al_source as true if initialized without error
    has_al_source = true;

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
    // to apply loop controls, sequence_files_cur is wrapped by #player_advance
    return sequence_files_cur < player->sequence_files_cnt;
}

int player_start(struct player_t *player,
                 player_frame_interrupt_t frame_interrupt,
                 unsigned short time_correction_ms) {
    const char *current_sequence_file = player->sequence_files[sequence_files_cur];

    // ready the current_sequence value for loading
    // pass a step_time_ms default value of 50ms (20 FPS)
    // this provides a minimum step time for the program
    struct sequence_t current_sequence = (struct sequence_t) {
            .step_time_ms = 50,
    };

    // load the sequence file into memory
    // this will buffer the initial data, and remainder is streamed during updates
    char *audio_file_hint = NULL;

    int err;
    if ((err = player_load_sequence_file(&current_sequence, current_sequence_file, &audio_file_hint))) {
        // ensure the allocated audio_file_hint buf is freed
        free(audio_file_hint);
        return err;
    }

    printf("sequence_file: %s\n", current_sequence_file);
    printf("audio_file_hint: %s\n", audio_file_hint);
    printf("step_time_ms: %dms (%d FPS)\n", current_sequence.step_time_ms, 1000 / current_sequence.step_time_ms);
    printf("frame_count: %d\n", current_sequence.frame_count);
    printf("channels_count: %zu\n", channel_buffer_index);

    // attempt to load audio file provided by determined sequence type
    // this will delegate or fallback internally as needed
    if ((err = player_load_audio_file(audio_file_hint))) {
        return err;
    }

    printf("playing...\n");

    // notify OpenAL to start source playback
    // OpenAL will automatically stop playback at EOF
    alSourcePlay(al_source);

    ALenum al_err;
    if ((al_err = al_get_error())) {
        al_perror(al_err, "failed to play OpenAL source");
        return LBR_ESPERR;
    }

    ALint source_state;

    // construct a timespec copy of step_time_ms
    // this is used by interval_timer as the "normal" interval time
    struct timespec step_time;

    step_time.tv_sec  = current_sequence.step_time_ms / 1000;
    step_time.tv_nsec = (long) (current_sequence.step_time_ms % 1000) * 1000000;

    struct interval_t interval_timer;

    interval_init(&interval_timer, step_time);

    // convert time_correction_ms into its corresponding frame_index_t
    // use this as a starting point to (optionally) shift forward
    frame_index_t frame_index = time_correction_ms / current_sequence.step_time_ms;

    printf("initial frame_index: %u\n", frame_index);

    // reset the initial output state
    // otherwise channels may still be active when initially booted
    if ((err = player_reset_encode_buffer(frame_interrupt, current_sequence.step_time_ms))) {
        return err;
    }

    while (true) {
        if ((err = interval_wake(&interval_timer))) {
            return err;
        }

        // write the current frame index into the frame_buf
        // pass an interrupt call back to the parent
        if ((err = minify_frame(current_sequence, frame_index))) {
            return err;
        }

        if ((err = encode_heartbeat_frame(frame_index, current_sequence.step_time_ms))) {
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
        alGetSourcei(al_source, AL_SOURCE_STATE, &source_state);

        if ((al_err = al_get_error())) {
            al_perror(al_err, "failed to get player source state");
            return LBR_ESPERR;
        }

        // break loop prior to sleep
        if (source_state != AL_PLAYING) {
            break;
        }

        // sleep after each loop iteration
        // interval internally manages time spent to ensure
        //  that each sleep maintains the expected step_time normal
        if ((err = interval_sleep(&interval_timer))) {
            return err;
        }
    }

    // encode a reset frame and trigger a final interrupt
    // this resets any active light output states
    if ((err = player_reset_encode_buffer(frame_interrupt, current_sequence.step_time_ms))) {
        return err;
    }

    channel_buffer_reset();
    channel_output_state_reset();
    frame_buffer_free();

    player_advance(player);

    return 0;
}

void player_free(const struct player_t *player) {
    // free the string array generated by #freadlines
    freadlines_free(player->sequence_files, player->sequence_files_cnt);

    ALenum err;

    // test the source & buffer fields of player for initialization
    // delete each if set
    if (has_al_buffer) {
        alDeleteBuffers(1, &current_al_buffer);

        if ((err = al_get_error())) {
            al_perror(err, "failed to delete current OpenAL buffer");
        }
    }

    if (has_al_source) {
        alDeleteSources(1, &al_source);

        if ((err = al_get_error())) {
            al_perror(err, "failed to delete OpenAL source");
        }
    }
}
