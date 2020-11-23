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

#include <string.h>

#include "err/lbr.h"

#define FILE_LINE_BUFFER_MAX_LENGTH 256

// this is unsafe, but it avoids dynamic allocations
//  assume any single line, each pointing to a file, is no more than 256 bytes
// this is a reused, singleton buffer and assumes no multi-threaded access
static char file_line_buffer[FILE_LINE_BUFFER_MAX_LENGTH];

int file_read_line(FILE *file,
                   char **out) {
    char   *read_line = NULL;
    size_t read_len   = 0;

    if ((read_line = fgetln(file, &read_len)) != NULL) {
        // return an error if the read_len is too long for the buffer
        // it needs to be <= FILE_LINE_BUFFER_MAX_LENGTH - 1 to ensure it is null terminated
        if (read_len + 1 > FILE_LINE_BUFFER_MAX_LENGTH) {
            return LBR_FILE_EPATHTOOLONG;
        }

        // man 3 fgetln notes that the line may include a newline char at the end
        // strip any newline terminator and manually resize to introduce a null terminator
        if (read_line[read_len - 1] == '\n') {
            read_len--;
        }

        // copy the read_line into the reused buffer
        // the pointer returned by fgetln does not persist
        strncpy(file_line_buffer, read_line, read_len);

        // ensure the copied string is null terminated
        file_line_buffer[read_len] = 0;

        // copy the final state to the out params
        *out = &file_line_buffer[0];

        return 0;
    }

    // assume ferror/feof returned a non-zero value
    return LBR_EERRNO;
}
