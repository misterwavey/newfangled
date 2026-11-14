/* SPDX-License-Identifier: MIT */
#ifndef ORIGINAL_CLOCK_H
#define ORIGINAL_CLOCK_H
#include <stdbool.h>
#include <stdint.h>
#define ORIGINAL_CLOCK_HZ 1000u
#define ORIGINAL_BEAT_PHASE 60000u
typedef struct {
    uint32_t phase,beats;
    unsigned bpm;
    bool running,gate;
} OriginalClock;
void original_clock_init(OriginalClock *clock,unsigned bpm);
void original_clock_tempo(OriginalClock *clock,unsigned bpm);
void original_clock_running(OriginalClock *clock,bool running);
void original_clock_reset(OriginalClock *clock);
void original_clock_tick(OriginalClock *clock);
void original_clock_advance(OriginalClock *clock,unsigned step);
#endif
