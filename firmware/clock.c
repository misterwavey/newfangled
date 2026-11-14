/* SPDX-License-Identifier: MIT
 * One beat spans 60000 phase units. Adding BPM each 1 ms gives the exact
 * rational average frequency without rounding a millisecond period. Edges are
 * quantized to 1 ms; oscillator error and physical DAC latency are separate.
 */
#include "clock.h"
void original_clock_tempo(OriginalClock *clock,unsigned bpm) {
    clock->bpm=bpm<10?10:bpm>300?300:bpm;
}
void original_clock_init(OriginalClock *clock,unsigned bpm) {
    *clock=(OriginalClock){0};original_clock_tempo(clock,bpm);
}
void original_clock_reset(OriginalClock *clock) {
    clock->phase=0;clock->beats=0;clock->gate=clock->running;
}
void original_clock_running(OriginalClock *clock,bool running) {
    if(clock->running==running)return;
    clock->running=running;original_clock_reset(clock);
}
void original_clock_tick(OriginalClock *clock) {
    if(!clock->running)return;
    clock->phase+=clock->bpm;
    if(clock->phase>=ORIGINAL_BEAT_PHASE) {clock->phase-=ORIGINAL_BEAT_PHASE;++clock->beats;}
    clock->gate=clock->phase<ORIGINAL_BEAT_PHASE/2u;
}
