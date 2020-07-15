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
#include "lbr.h"

#include <stdio.h>

void lbr_perror(int err,
                const char *msg) {
    fprintf(stderr, "libreorama error\n");
    fprintf(stderr, "%s\n", msg);
    fprintf(stderr, "%s (%d)\n", lbr_error_string(err), err);

    // if err == LBR_EERRNO, then the error comes from
    //  the stdlib and errno is set with more information
    if (err == LBR_EERRNO) {
        perror(NULL);
    }
}

char *lbr_error_string(int err) {
    switch (err) {
        case LBR_EERRNO:
            return "LBR_EERRNO (system error, see errno)";
        case LBR_EALERR:
            return "LBR_EALERR (OpenAl/ALUT error)";
        case LBR_ESPERR:
            return "LBR_ESPERR (libserialport error)";

        case LBR_SEQUENCE_ENOFRAMES:
            return "LBR_SEQUENCE_ENOFRAMES (sequence contains no frames)";
        case LBR_SEQUENCE_ENOCHANNELS:
            return "LBR_SEQUENCE_ENOCHANNELS (sequence contains no channels)";
        case LBR_SEQUENCE_EWRITEINDEX:
            return "LBR_SEQUENCE_EWRITEINDEX (writer index mismatch)";

        case LBR_PLAYER_EUNSUPEXT:
            return "LBR_PLAYER_EUNSUPEXT (unsupported file extension)";
        case LBR_PLAYER_EBADEXT:
            return "LBR_PLAYER_EBADEXT (bad file extension)";
        case LBR_PLAYER_ESHOWEMPTY:
            return "LBR_PLAYER_ESHOWEMPTY (show is empty)";

        case LBR_ENCODE_EBLOBTOOSMALL:
            return "LBR_ENCODE_EBLOBTOOSMALL (overwrote frame buffer blob)";
        case LBR_ENCODE_EUNSUPACTION:
            return "LBR_ENCODE_EUNSUPACTION (unsupported action)";

        case LBR_LOADER_EMALFDATA:
            return "LBR_LOADER_EMALFDATA (malformed data)";
        case LBR_LOADER_EUNSUPDATA:
            return "LBR_LOADER_EUNSUPDATA (unsupported data)";

        case LBR_MINIFY_EUNCONDATA:
            return "LBR_MINIFY_EUNCONDATA (unconsumed frame data)";

        default:
            return "unknown LBR error";
    }
}