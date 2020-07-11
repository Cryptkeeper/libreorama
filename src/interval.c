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
#include "interval.h"

#include "err/lbr.h"

#define INTERVAL_NS_IN_S 1000000000

static void timespec_sub(struct timespec a,
                         struct timespec b,
                         struct timespec *out) {
    out->tv_sec  = a.tv_sec - b.tv_sec;
    out->tv_nsec = a.tv_nsec - b.tv_nsec;
}

static void timespec_add(struct timespec a,
                         struct timespec b,
                         struct timespec *out) {
    out->tv_sec  = a.tv_sec + b.tv_sec;
    out->tv_nsec = a.tv_nsec + b.tv_nsec;

    // nanosleep will return EINVAL for tv_nsec values >=1000 million (1 second)
    // modulus tv_nsec to avoid overflowing its value into seconds
    out->tv_sec += out->tv_nsec / INTERVAL_NS_IN_S;
    out->tv_nsec %= INTERVAL_NS_IN_S;
}

static const struct interval_t INTERVAL_EMPTY;

void interval_init(struct interval_t *interval,
                   struct timespec sleep_duration_normal) {
    *interval = INTERVAL_EMPTY;

    interval->sleep_duration_normal = sleep_duration_normal;
}

int interval_wake(struct interval_t *interval) {
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &interval->wake_time)) {
        return LBR_EERRNO;
    }

    if (interval->has_slept) {
        timespec_sub(interval->wake_time, interval->sleep_time, &interval->sleep_duration_spent);
    } else {
        interval->has_slept = 1;
    }

    return 0;
}

int interval_sleep(struct interval_t *interval) {
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &interval->sleep_time)) {
        return LBR_EERRNO;
    }

    timespec_sub(interval->sleep_duration_goal, interval->sleep_duration_spent, &interval->sleep_duration);
    timespec_add(interval->sleep_duration, interval->sleep_duration_normal, &interval->sleep_duration);

    interval->sleep_duration_goal = interval->sleep_duration;

    if (nanosleep(&interval->sleep_duration, NULL)) {
        return LBR_EERRNO;
    }

    return 0;
}