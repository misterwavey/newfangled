/* SPDX-License-Identifier: MIT */
#ifndef NF_CONTROLS_H
#define NF_CONTROLS_H
#include <stdbool.h>
#include <stdint.h>
/* Rev62 clockwise: 3,2,0,1,3. Accept complete paths ending at the
 * released detent (3); contact bounce retraces a path and cancels itself.
 * Always re-anchor at the detent, even after an incomplete/invalid path.
 * In particular, a push must never leave a partial count for the next turn. */
static inline int nf_encoder_step(int *progress,uint8_t previous,uint8_t raw) {
    unsigned old=previous&3u,next=raw&3u;
    if(old==next)return 0;
    if((old^next)==3u)*progress=0;
    else *progress+=((old&1u)^((next>>1)&1u))?-1:1;
    if(next!=3u)return 0;
    int step=*progress==4?1:*progress==-4?-1:0;
    *progress=0;return step;
}
/* One sample per ms, including unchanged levels: button debounce/hold events
 * must precede later encoder events even when foreground rendering is busy.
 * ISR is the sole producer. Consumer must exclude the ISR while taking/resetting.
 * Overflow drops new samples and is reported, never silently overwrites history. */
#define NF_CONTROL_SAMPLES 32u
typedef struct {
    volatile uint32_t time[NF_CONTROL_SAMPLES];
    volatile uint8_t raw[NF_CONTROL_SAMPLES];
    volatile unsigned head,tail,overruns;
} NfControlQueue;
static inline void nf_controls_push(NfControlQueue *q,uint8_t raw,uint32_t time) {
    unsigned next=(q->head+1u)%NF_CONTROL_SAMPLES;
    if(next==q->tail){++q->overruns;return;}
    q->raw[q->head]=raw;q->time[q->head]=time;q->head=next;
}
static inline bool nf_controls_take(NfControlQueue *q,uint8_t *raw,uint32_t *time) {
    if(q->head==q->tail)return false;
    *raw=q->raw[q->tail];*time=q->time[q->tail];
    q->tail=(q->tail+1u)%NF_CONTROL_SAMPLES;return true;
}
#endif
