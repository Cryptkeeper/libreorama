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
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <getopt.h>

#include "err.h"
#include "player.h"

static void print_usage(void) {
    printf("Usage: libreorama [options] <serial port name>\n");
    printf("\n");
    printf("Options:\n");
    printf("\t-b <serial port baud rate> (defaults to 19200)\n");
    printf("\t-f <show file path> (defaults to \"show.txt\")\n");
    printf("\t-l loop show infinitely (defaults to false)\n");
}

static struct sp_port  *serial_port = NULL;
static struct player_t player;

static enum sp_return sp_init_port(const char *device_name,
                                   int baud_rate) {
    enum sp_return err;
    if ((err = sp_get_port_by_name(device_name, &serial_port)) != SP_OK) {
        sp_perror(err, "failed to get serial port by name");
        return err;
    }
    if ((err = sp_open(serial_port, SP_MODE_WRITE)) != SP_OK) {
        sp_perror(err, "failed to open serial port for writing");
        return err;
    }
    if ((err = sp_set_baudrate(serial_port, baud_rate)) != SP_OK) {
        sp_perror(err, "failed to set serial port baud");
        return err;
    }
    return SP_OK;
}

static void handle_exit(void) {
    // close the serial_port if initialized
    if (serial_port != NULL) {
        enum sp_return sp_return;
        if ((sp_return = sp_close(serial_port)) != SP_OK) {
            // since this occurs during exit, error returns can be mostly ignored
            // they are printed only for visibility to the user and should not be explicitly handled
            sp_perror(sp_return, "sp_close returned error code during exit (this can likely be ignored)");
        }
    }

    // free the player, it will safely handle partially initialized state internally
    // this will not modify player, be aware of potential dangling pointers
    player_free(&player);

    // fire alutExit, this assumes it was initialized already
    // this must happen after #player_free since player holds OpenAL sources/buffers
    alutExit();

    ALenum err;
    if ((err = al_get_error())) {
        al_perror(err, "failed to exut ALUT");
    }
}

int main(int argc,
         char **argv) {
    int  baud_rate        = 19200;
    char *show_file_path  = "show.txt";
    bool is_infinite_loop = true;

    // prefix optstring with : to enable missing option case
    // see "man 3 getopt" for more information
    int c;
    while ((c = getopt(argc, argv, ":hb:f:l")) != -1) {
        switch (c) {
            case 'h':
                print_usage();
                return 0;
            case ':':
                fprintf(stderr, "argument is missing option: %c\n", optopt);
                return 1;
            case '?':
                fprintf(stderr, "unknown argument: %c\n", optopt);
                return 1;
            case 'b': {
                long baud_ratel = strtol(optarg, NULL, 10);

                // bounds check before downcasting long to int
                // int is used internally to match the type used by libserialport
                if (baud_ratel <= 0 || baud_ratel > INT_MAX) {
                    fprintf(stderr, "invalid baud rate: %ld\n", baud_ratel);
                    return 1;
                }
                baud_rate = (int) baud_ratel;
                break;
            }
            case 'f': {
                show_file_path = optarg;
                break;
            }
            case 'l':
                is_infinite_loop = true;
                break;
        }
    }

    // remove optind (option index) from argc to avoid re-processing options
    // advance argv to align with offset argc indexes
    argc -= optind;
    argv += optind;

    if (argc < 1) {
        fprintf(stderr, "missing serial port argument\n");
        return 1;
    }

    atexit(handle_exit);

    // initialize the serial port name from argv
    // cleanup of any successfully opened sp_port is handled by #handle_exit
    if (sp_init_port(argv[0], baud_rate) != SP_OK) {
        return 1;
    }

    // initialize ALUT
    alutInit(NULL, NULL);

    ALenum err;
    if ((err = al_get_error())) {
        al_perror(err, "failed to initialize ALUT");
        return 1;
    }

    // initialize player and load show file
    // player_init handles error printing internally
    if (player_init(&player, is_infinite_loop, show_file_path)) {
        return 1;
    }

    // begin main program loop
    // this is managed by player directly to reduce scoping
    // #player_next will return true as long as there is a sequence to play
    // false values will break the loop and cleanly terminate the program
    while (player_has_next(&player)) {
        // load and buffer the sequence
        // this will internally block for playback
        if (player_start(&player)) {
            return 1;
        }
    }

    return 0;
}
