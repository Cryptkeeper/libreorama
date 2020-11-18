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
#include <string.h>

#include "err/al.h"
#include "err/sp.h"
#include "err/lbr.h"
#include "player/player.h"
#include "lorinterface/encode.h"

static void print_usage(void) {
    printf("Usage: libreorama [options] <serial port name>\n");
    printf("\n");
    printf("Options:\n");
    printf("\t-b <serial port baud rate> (defaults to 19200)\n");
    printf("\t-f <show file path> (defaults to \"show.txt\")\n");
    printf("\t-c <time correction offset in milliseconds> (defaults to 0)\n");
    printf("\t-l <show loop count> (defaults to 1, \"i\" to infinitely loop)\n");
}

static struct sp_port  *serial_port = NULL;
static struct player_t player;

static int sp_init_port(const char *device_name,
                        int baud_rate) {
    enum sp_return err;
    if ((err = sp_get_port_by_name(device_name, &serial_port)) != SP_OK) {
        sp_perror(err, "failed to get serial port by name");
        return LBR_ESPERR;
    }
    if ((err = sp_open(serial_port, SP_MODE_WRITE)) != SP_OK) {
        sp_perror(err, "failed to open serial port for writing");
        return LBR_ESPERR;
    }
    if ((err = sp_set_baudrate(serial_port, baud_rate)) != SP_OK) {
        sp_perror(err, "failed to set serial port baud");
        return LBR_ESPERR;
    }

    return 0;
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

    encode_buffer_free();

    // fire alutExit, this assumes it was initialized already
    // this must happen after #player_free since player holds OpenAL sources/buffers
    alutExit();

    ALenum err;
    if ((err = al_get_error())) {
        al_perror(err, "failed to exit ALUT");
    }
}

static int handle_frame_interrupt(unsigned short step_time_ms) {
    if (encode_buffer_length > 0 && serial_port != NULL) {
        enum sp_return sp_return;

        // sp_nonblocking_write returns bytes written when non-error (<0 - SP_OK)
        // feed step_time_ms as timeout to avoid blocking writes from stalling playback
        if ((sp_return = sp_blocking_write(serial_port, encode_buffer, encode_buffer_length, step_time_ms / 2u)) < SP_OK) {
            sp_perror(sp_return, "failed to write frame data to serial port");
            return LBR_ESPERR;
        }
    }

    // reset the encode buffer writer back to 0
    // always fire regardless of serial_port to avoid over allocating
    encode_buffer_reset();

    return 0;
}

int main(int argc,
         char **argv) {
    int            baud_rate          = 19200;
    char           *show_file_path    = "show.txt";
    unsigned short time_correction_ms = 0;
    int            show_loop_count    = 1;

    // prefix optstring with : to enable missing option case
    // see "man 3 getopt" for more information
    int c;
    while ((c = getopt(argc, argv, ":hb:f:c:l:")) != -1) {
        switch (c) {
            case 'h':
                print_usage();
                return 0;
            case ':':
                fprintf(stderr, "argument is missing option: %c\n", optopt);
                return 1;
            case '?':
            default:
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
            case 'c': {
                long time_correction_msl = strtol(optarg, NULL, 10);

                // bounds check before downcasting long to unsigned short
                // this is mostly used for the < 0 check to prevent negative input
                if (time_correction_msl < 0 || time_correction_msl > USHRT_MAX) {
                    fprintf(stderr, "invalid time correction: %ld\n", time_correction_msl);
                    return 1;
                }
                time_correction_ms = (unsigned short) time_correction_msl;
                break;
            }
            case 'l': {
                if (strncmp(optarg, "i", 1) == 0) {
                    // a -1 show_loop_count value indicates and infinite loop
                    show_loop_count = -1;
                } else {
                    long show_loop_countl = strtol(optarg, NULL, 10);

                    // bounds check before downcasting long to int
                    // this is mostly used for the <=0 check to prevent negative input
                    if (show_loop_countl <= 0 || show_loop_countl > INT_MAX) {
                        fprintf(stderr, "invalid show loop count: %ld\n", show_loop_countl);
                        return 1;
                    }
                    show_loop_count = (int) show_loop_countl;
                }
                break;
            }
        }
    }

    // remove optind (option index) from argc to avoid re-processing options
    // advance argv to align with offset argc indexes
    argc -= optind;
    argv += optind;

    atexit(handle_exit);

    // initialize the serial port name from argv
    // cleanup of any successfully opened sp_port is handled by #handle_exit
    int err;

    if (argc > 0) {
        if ((err = sp_init_port(argv[0], baud_rate))) {
            lbr_perror(err, "failed to initialize serial port");
            return 1;
        }
    } else {
        fprintf(stderr, "no serial port specified, defaulting to NULL (no output)\n");
    }

    // initialize ALUT
    alutInit(NULL, NULL);

    ALenum al_err;
    if ((al_err = al_get_error())) {
        al_perror(al_err, "failed to initialize ALUT");
        return 1;
    }

    // initialize player and load show file
    // player_init handles error printing internally
    player.show_loop_count = show_loop_count;

    if ((err = player_init(&player, show_file_path))) {
        lbr_perror(err, "failed to initialize player");
        return 1;
    }

    // begin main program loop
    // this is managed by player directly to reduce scoping
    // #player_next will return true as long as there is a sequence to play
    // false values will break the loop and cleanly terminate the program
    while (player_has_next(&player)) {
        // free the encode buffer between sequences
        // this ensures that if expanded by a sequence, that allocation is not maintained
        encode_buffer_free();

        // load and buffer the sequence
        // this will internally block for playback
        if ((err = player_start(&player, handle_frame_interrupt, time_correction_ms))) {
            lbr_perror(err, "failed to start player");
            return 1;
        }
    }

    return 0;
}
