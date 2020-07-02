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
#include "file.h"

#include <stdlib.h>
#include <string.h>

void freadlines_free(char **lines,
                     size_t len) {
    for (size_t i = 0; i < len; i++) {
        free(lines[i]);
    }

    free(lines);
}

char **freadlines(FILE *file,
                  size_t *len) {
    char   *cur_line;
    size_t cur_line_len;

    char   **lines   = NULL;
    size_t lines_len = 0;

    while ((cur_line = fgetln(file, &cur_line_len)) != NULL) {
        // man 3 fgetln notes that the line may include a newline char at the end
        // strip any newline terminator and manually resize to introduce a null terminator
        if (cur_line[cur_line_len - 1] == '\n') {
            cur_line_len--;
        }

        char *buf = malloc(cur_line_len + 1);

        if (buf == NULL) {
            // failed to allocate buffer for string
            // free anything that has been allocated thus far
            freadlines_free(lines, lines_len);

            return NULL;
        }

        strncpy(buf, cur_line, cur_line_len);

        // ensure the string is null terminated
        buf[cur_line_len] = 0;

        // realloc lines to ensure capacity for incoming cur_line_null
        char **lines_realloc = realloc(lines, sizeof(char *) * (lines_len + 1));

        if (lines_realloc == NULL) {
            // failed to reallocate larger array
            // free anything that has been allocated thus far
            freadlines_free(lines, lines_len);

            return NULL;
        } else {
            // copy lines_realloc back to lines
            // this ensures lines is not overwritten if realloc fails
            // otherwise the lines reference is lost and will leak
            lines = lines_realloc;
        }

        lines[lines_len] = buf;
        lines_len++;
    }

    // successfully read all lines, modify external state
    // this does not check against ferror
    *len = lines_len;

    return lines;
}