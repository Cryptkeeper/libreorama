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

/**
 * err.h provides a unified method for testing and printing errors for libserialport, ALUT & OpenAL.
 * It mirrors the errno & perror style of error handling with *_get_error & *_perror functions.
 */
#ifndef LIBREORAMA_ERR_H
#define LIBREORAMA_ERR_H

#include <AL/alut.h>
#include <libserialport.h>

ALenum al_get_error();

void al_perror(ALenum err,
               const char *msg);

char *al_error_string(ALenum err);

void sp_perror(enum sp_return sp_return,
               const char *msg);

char *sp_error_string(enum sp_return sp_return);

#endif //LIBREORAMA_ERR_H
