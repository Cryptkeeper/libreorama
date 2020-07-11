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
#include "sp.h"

#include <stdio.h>

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
            return "unknown sp_return error";
    }
}