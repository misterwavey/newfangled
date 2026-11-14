/* SPDX-License-Identifier: MIT */
#ifndef NEWFANGLED_SYNC_H
#define NEWFANGLED_SYNC_H
#include <stdbool.h>
#include <stdint.h>
#define NF_PPQN_COUNT 8u
#define NF_SYNC_CYCLE 2880000u /* 48 beats: common multiple of /3 and /16. */
#define NF_INPUT_CLOCK 1u
#define NF_INPUT_RUN_HIGH 2u
#define NF_INPUT_RUN_LOW 3u
#define NF_RUN_OFF 0u
#define NF_RUN_GATE 1u
#define NF_RUN_RESET 2u
extern const uint8_t nf_ppqn_values[NF_PPQN_COUNT];
typedef struct {
    uint32_t last_us,last_tick_us,beat_us,intervals[8],sum,count,next;
    uint32_t pulse,phase,remainder,rejected;
    int32_t correction;
    unsigned ppqn,bpm;
    bool have_edge,locked,lost,enabled,active,restart;
} NfSync;
void nf_sync_init(NfSync *s,unsigned ppqn);
void nf_sync_enable(NfSync *s,bool enabled);
void nf_sync_rearm(NfSync *s);
void nf_sync_edge(NfSync *s,uint32_t us);
unsigned nf_sync_bpm(const NfSync *s);
/* One call per 1 ms. Returns nonnegative phase advance; never reverses waves. */
unsigned nf_sync_tick(NfSync *s,uint32_t now_us);
#endif
