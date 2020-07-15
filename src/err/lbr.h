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
#ifndef LIBREORAMA_LBR_H
#define LIBREORAMA_LBR_H

#define LBR_EERRNO                  1
#define LBR_EALERR                  2
#define LBR_ESPERR                  3

#define LBR_SEQUENCE_ENOFRAMES      4
#define LBR_SEQUENCE_ENOCHANNELS    5
#define LBR_SEQUENCE_EWRITEINDEX    6

#define LBR_PLAYER_EUNSUPEXT        7
#define LBR_PLAYER_EBADEXT          8
#define LBR_PLAYER_ESHOWEMPTY       9

#define LBR_ENCODE_EBLOBTOOSMALL    10
#define LBR_ENCODE_EUNSUPACTION     11

#define LBR_LOADER_EMALFDATA        12
#define LBR_LOADER_EUNSUPDATA       13

#define LBR_MINIFY_EUNCONDATA       14

void lbr_perror(int err,
                const char *msg);

char *lbr_error_string(int err);

#endif //LIBREORAMA_LBR_H
