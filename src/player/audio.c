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
#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../err/lbr.h"

int audio_find_sequence_file(const char *sequence_file,
                             const char *audio_file_hint,
                             char **audio_file) {
    // test if audio_file_hint exists before loading
    // if not, attempt to locate a fallback using a basic pattern
    if (audio_file_hint == NULL) {
        fprintf(stderr, "sequence file returned no audio file hint\n");
    } else if (access(audio_file_hint, F_OK) != -1) {
        *audio_file = (char *) audio_file_hint;
        return 0;
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
        return LBR_EERRNO;
    }

    // copy the sequence_file string & append an explicit file extension
    // this string is already null terminated by malloc sizing with +1
    strncat(buf, sequence_file, len);
    strncat(buf, file_ext, file_ext_len);

    // ensure the string is null terminated
    buf[len + file_ext_len] = 0;

    *audio_file = buf;

    return 0;
}