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

#include "sequence.h"

struct player_t {
    FILE *show_file;
    int  show_loop_count;
};

typedef int (*player_frame_interrupt_t)(unsigned short step_time_ms);

int player_init(struct player_t *player,
                const char *show_file_path);

int player_next_sequence(struct player_t *player,
                         char **next_sequence);

int player_start(player_frame_interrupt_t frame_interrupt,
                 unsigned short time_correction_ms,
                 char *sequence_file_path);

void player_free(const struct player_t *player);

#endif //LIBREORAMA_PLAYER_H
