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
#include "err.h"

#include <stdio.h>

ALenum al_get_error() {
    ALenum err;
    if ((err = alutGetError()) || (err = alGetError())) {
        return err;
    }

    // both ALUT & OpenAL use 0 as a "no error" indicator
    // see AL_NO_ERROR & ALUT_ERROR_NO_ERROR
    return 0;
}

void al_perror(ALenum err,
               char *msg) {
    // avoid calls with NO_ERROR conditions
    // this abuses the fact AL_NO_ERROR & ALUT_ERROR_NO_ERROR are 0 value
    if (!err) {
        fprintf(stderr, "unknown error: %s\n", msg);
        return;
    }

    // OpenAL errors have the upper byte 0xA0
    // ALUT errors have an upper byte of 0x2, which conflicts with some AL namespaces
    // check for a OpenAL error first, falling back to ALUT
    if ((err & 0xA000)) {
        fprintf(stderr, "OpenAL error (version 0x%04x)\n", AL_VERSION);
        fprintf(stderr, "%s\n", msg);
        fprintf(stderr, "%s (0x%02x)\n", al_error_string(err), err);
    } else {
        fprintf(stderr, "ALUT error (version %d.%d)\n", alutGetMajorVersion(), alutGetMinorVersion());
        fprintf(stderr, "%s\n", msg);
        fprintf(stderr, "%s (0x%04x)\n", alutGetErrorString(err), err);

        // ALUT_ERROR_IO_ERROR errors set errno
        // see http://distro.ibiblio.org/rootlinux/rootlinux-ports/more/freealut/freealut-1.1.0/doc/alut.html
        if (err == ALUT_ERROR_IO_ERROR) {
            perror(NULL);
        }
    }
}

char *al_error_string(ALenum err) {
    // attempt to match the OpenAL definition to a string
    // this is exhaustive for OpenAL version 1.1 but may break in the future
    switch (err) {
        case AL_INVALID_NAME:
            return "AL_INVALID_NAME";
        case AL_INVALID_ENUM:
            return "AL_INVALID_ENUM";
        case AL_INVALID_VALUE:
            return "AL_INVALID_VALUE";
        case AL_INVALID_OPERATION:
            return "AL_INVALID_OPERATION";
        case AL_OUT_OF_MEMORY:
            return "AL_OUT_OF_MEMORY";
        default:
            return "unknown ALenum error value";
    }
}

void sp_perror(enum sp_return sp_return,
               const char *msg) {
    fprintf(stderr, "libserialport error (version %s)\n", SP_LIB_VERSION_STRING);
    fprintf(stderr, "%s\n", msg);
    fprintf(stderr, "%s (%d)\n", sp_error_string(sp_return), sp_return);

    // SP_ERR_FAIL sets optional sp_last_error_message
    if (sp_return == SP_ERR_FAIL) {
        char *last_err_msg = sp_last_error_message();
        fprintf(stderr, "%s\n", last_err_msg);
        sp_free_error_message(last_err_msg);
    }
}

char *sp_error_string(enum sp_return sp_return) {
    switch (sp_return) {
        case SP_ERR_ARG:
            return "SP_ERR_ARG";
        case SP_ERR_FAIL:
            return "SP_ERR_FAIL";
        case SP_ERR_SUPP:
            return "SP_ERR_SUPP";
        case SP_ERR_MEM:
            return "SP_ERR_MEM";
        default:
            return "unknown sp_return error value";
    }
}