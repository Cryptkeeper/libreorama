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
#ifndef LIBREORAMA_PLAYER_H
#define LIBREORAMA_PLAYER_H

#include <stdbool.h>
#include <stdio.h>

#include <AL/alut.h>

#include "sequence.h"

struct player_t {
    char              **sequence_files;
    size_t            sequence_files_cnt;
    size_t            sequence_files_cur;
    ALuint            al_source;
    ALuint            current_al_buffer;
    bool is_infinite_loop: 1;
    bool has_al_source: 1;
    bool has_al_buffer: 1;
    struct sequence_t *sequence_current;
};

int player_init(struct player_t *player,
                int is_infinite_loop,
                const char *show_file_path);

bool player_has_next(struct player_t *player);

int player_start(struct player_t *player);

void player_free(const struct player_t *player);

#endif //LIBREORAMA_PLAYER_H
