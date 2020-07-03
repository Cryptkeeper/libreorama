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
#include <unistd.h>

#include "err.h"
#include "file.h"
#include "sequence.h"

static int player_load_sequence_file(struct player_t *player,
                                     const char *sequence_file,
                                     char **audio_file,
                                     unsigned long *step_time_ms,
                                     enum sequence_type_t *sequence_type) {
    // locate a the last dot char in the string, if any
    // this is used to locate the file extension for determing the sequence type
    const char *dot = strrchr(sequence_file, '.');

    // check dot == current_sequence_file to ignore hidden files such as "~/.lms"
    if (dot == NULL || dot == sequence_file) {
        fprintf(stderr, "sequence \"%s\" has no file extension, unable to determine sequence type\n", sequence_file);
        return 1;
    }

    *sequence_type = sequence_type_from_file_extension(dot);

    // detect failure to match the file extension to a sequence type
    // halt playback since otherwise only the audio would be played
    if (*sequence_type == SEQUENCE_TYPE_UNKNOWN) {
        fprintf(stderr, "unknown/unsupported sequence file extension: %s\n", dot);
        return 1;
    }

    const sequence_loader_t sequence_loader = sequence_type_get_loader(*sequence_type);

    // pass off to whatever function is provided by #sequence_type_get_loader
    if (sequence_loader(sequence_file, audio_file, step_time_ms)) {
        // each impl should internally handle errors, but a generic top level error message
        //  is provided to prevent silent error returns caused by bad loader impls
        fprintf(stderr, "failed to load sequence file: %s\n", sequence_file);
        return 1;
    }

    return 0;
}

static char *player_get_audio_file_or_fallback(const char *sequence_file,
                                               const char *audio_file_hint) {
    // test if audio_file_hint exists before loading
    // if not, attempt to locate a fallback using a basic pattern
    if (audio_file_hint == NULL) {
        fprintf(stderr, "sequence file returned no audio file hint\n");
    } else if (access(audio_file_hint, F_OK) != -1) {
        return (char *) audio_file_hint;
    } else {
        fprintf(stderr, "audio file hint \"%s\" does not exist!\n", audio_file_hint);
    }

    static const char *file_ext    = ".wav";
    static const int  file_ext_len = 4;

    // sequence_file is already null terminated at call time
    // allocate a larger buffer and assign a file extension
    // this must be freed by the upstream caller
    const size_t len  = strlen(sequence_file);
    char         *buf = malloc(len + file_ext_len + 1);

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

static int player_load_audio_file(struct player_t *player,
                                  const char *sequence_file,
                                  char *audio_file_hint) {
    ALenum err;

    // if an AL buffer is already initialized, unload it first
    if (player->has_al_buffer) {
        player->has_al_buffer = false;

        // unqueue the buffer from the active source
        // otherwise the delete will fail since it is considered in use
        alSourceUnqueueBuffers(player->al_source, 1, &player->current_al_buffer);

        if ((err = al_get_error())) {
            al_perror(err, "failed to unqueue previous player buffer from source");
            return 1;
        }

        // delete the buffer, freeing the memory
        alDeleteBuffers(1, &player->current_al_buffer);

        if ((err = al_get_error())) {
            al_perror(err, "failed to delete previous player buffer");
            return 1;
        }
    }

    // use #player_get_audio_file_or_fallback to determine if the file exists
    // if it doesn't, it will internally attempt to locate it at another path
    char *audio_file_or_fallback = player_get_audio_file_or_fallback(sequence_file, audio_file_hint);

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

static int player_step(struct player_t *player,
                       enum sequence_type_t sequence_type) {
    // todo: this is going to need some sort of metadata store
    return 0;
}

int player_init(struct player_t *player,
                int is_infinite_loop,
                const char *show_file_path) {
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

    // ensure file is closed
    fclose(show_file);

    // test for null result which indicates an error
    // include additional ferror check to prevent silent errors
    if (player->sequence_files == NULL) {
        perror("failed to read show file");
        return 1;
    }

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
    const char *current_sequence_file = player->sequence_files[player->sequence_files_cur];

    // load the sequence file into memory
    // this will buffer the initial data, and remainder is streamed during updates
    // pass a step_time_ms default value of 50ms (20 FPS)
    // this provides a minimum step time for the program
    char                 *audio_file_hint = NULL;
    unsigned long        step_time_ms     = 50;
    enum sequence_type_t sequence_type;

    if (player_load_sequence_file(player, current_sequence_file, &audio_file_hint, &step_time_ms, &sequence_type)) {
        // ensure the allocated audio_file_hint buf is freed
        free(audio_file_hint);

        return 1;
    }

    printf("sequence_file: %s\n", current_sequence_file);
    printf("sequence_type: %s\n", sequence_type_string(sequence_type));
    printf("audio_file_hint: %s\n", audio_file_hint);
    printf("step_time_ms: %lums (%lu FPS)\n", step_time_ms, 1000 / step_time_ms);

    // attempt to load audio file provided by determined sequence type
    // this will delegate or fallback internally as needed
    if (player_load_audio_file(player, current_sequence_file, audio_file_hint)) {
        return 1;
    }

    printf("playing...\n");

    // notify OpenAL to start source playback
    // OpenAL will automatically stop playback at EOF
    alSourcePlay(player->al_source);

    ALenum err;
    if ((err = al_get_error())) {
        al_perror(err, "failed to play OpenAL source");
        return 1;
    }

    ALint source_state;

    // construct a timespec copy of step_time_ms
    // this is used by the downstream nanosleep calls
    struct timespec step_time;

    step_time.tv_sec  = step_time_ms / 1000;
    step_time.tv_nsec = (long) (step_time_ms % 1000) * 1000000;

    while (1) {
        if (player_step(player, sequence_type)) {
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