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

#include "err.h"
#include "file.h"

static char *player_get_audio_file(const char *sequence_file) {
    static const char *file_ext    = ".wav";
    static const int  file_ext_len = 4;

    // sequence_file is already null terminated at call time
    // allocate a larger buffer and assign a file extension
    // this must be freed by the upstream caller
    size_t len  = strlen(sequence_file);
    char   *buf = malloc(len + file_ext_len + 1);

    if (buf == NULL) {
        return buf;
    }

    // copy the sequence_file string & append an explicit file extension
    // this string is already null terminated by malloc sizing with +1
    strncat(buf, sequence_file, len);
    strncat(buf, file_ext, file_ext_len);

    // ensure the string is null terminated
    buf[len + file_ext_len] = 0;

    return buf;
}

static int player_load_audio_file(struct player_t *player) {
    // if an AL buffer is already initialized, unload it first
    if (player->has_al_buffer) {
        player->has_al_buffer = false;

        // delete the buffer, freeing the memory
        alDeleteBuffers(1, &player->current_al_buffer);

        ALenum err;
        if ((err = al_get_error())) {
            al_perror(err, "failed to delete previous player buffer");
            return 1;
        }
    }

    const char *current_sequence_file = player->sequence_files[player->sequence_files_cur];

    // determine the audio file of the current sequence file
    // read & buffer its data into memory for OpenAL playback
    char *audio_file = player_get_audio_file(current_sequence_file);

    if (audio_file == NULL) {
        perror("failed to get audio file of current sequence file");
        return 1;
    }

    player->current_al_buffer = alutCreateBufferFromFile(audio_file);

    // #player_get_audio_file allocates internally
    // free prior to error checking so it is always freed before exit
    free(audio_file);

    // test for buffering errors
    ALenum err;
    if ((err = al_get_error())) {
        al_perror(err, "failed to buffer audio file");
        return 1;
    }

    // only flag has_al_buffer as true if #al_get_error returns ok
    // otherwise the current_al_buffer value may be invalid but flagged as set
    player->has_al_buffer = true;

    // assign the OpenAL to the source
    // this enables #player_start to simply play the source to start
    alSourcei(player->al_source, AL_BUFFER, player->current_al_buffer);

    if ((err = al_get_error())) {
        al_perror(err, "failed to assign OpenAL source buffer");
        return 1;
    }

    return 0;
}

static int player_load_sequence_file(struct player_t *player,
                                     struct timespec *step_time) {
    step_time->tv_sec  = 1; // todo
    step_time->tv_nsec = 0;
    return 0;
}

static int player_step(struct player_t *player) {
    return 0;
}

int player_init(struct player_t *player,
                int is_infinite_loop,
                char *show_file_path) {
    player->is_infinite_loop = is_infinite_loop;

    // generate the single OpenAL source
    // this is used for all player playback behavior
    alGenSources(1, &player->al_source);

    ALenum err;
    if ((err = al_get_error())) {
        al_perror(err, "failed to generate OpenAL source");
        return 1;
    }

    // only flag has_al_source as true if initialized without error
    player->has_al_source = true;

    // open the show file for reading
    FILE *show_file = fopen(show_file_path, "rb");

    if (show_file == NULL) {
        perror("failed to open show file");
        return 1;
    }

    // read each line of the show file into a string array
    // this internally allocates and must be freed
    // see #player_free
    player->sequence_files = freadlines(show_file, &player->sequence_files_cnt);

    // test for null result which indicates an error
    // include additional ferror check to prevent silent errors
    if (player->sequence_files == NULL || ferror(show_file)) {
        // ensure file is closed
        fclose(show_file);

        perror("failed to read show file");
        return 1;
    }

    // ensure file is closed
    fclose(show_file);

    // test if the show file is empty
    if (player->sequence_files_cnt == 0) {
        fprintf(stderr, "show file is empty!\n");
        return 1;
    }

    return 0;
}

bool player_has_next(struct player_t *player) {
    // no need to check is_infinite_loop since sequence_files_cur is always wrapped by #player_start
    return player->sequence_files_cur < player->sequence_files_cnt;
}

int player_start(struct player_t *player) {
    if (player_load_audio_file(player)) {
        return 1;
    }

    // load the sequence file into memory
    // this will buffer the initial data, and remainder is streamed during updates
    // pass a timespec value that is used to control the playback while loop
    struct timespec step_time;

    if (player_load_sequence_file(player, &step_time)) {
        return 1;
    }

    printf("playing %s\n", player->sequence_files[player->sequence_files_cur]);

    // notify OpenAL to start source playback
    // OpenAL will automatically stop playback at EOF
    alSourcePlay(player->al_source);

    ALenum err;
    if ((err = al_get_error())) {
        al_perror(err, "failed to play OpenAL source");
        return 1;
    }

    ALint source_state;

    while (1) {
        if (player_step(player)) {
            return 1;
        }

        // test if playback is still happening
        // this defers to the audio time rather than the sequence
        // this helps ensure a consistent result
        alGetSourcei(player->al_source, AL_SOURCE_STATE, &source_state);

        if ((err = al_get_error())) {
            al_perror(err, "failed to get player source state");
            return 1;
        }

        // break loop prior to sleep
        if (source_state != AL_PLAYING) {
            break;
        }

        // sleep after each loop iteration
        // this time comes from #player_load_sequence_file and should be considered dynamic
        if (nanosleep(&step_time, NULL)) {
            perror("failed to nanosleep during playback step");
            return 1;
        }
    }

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