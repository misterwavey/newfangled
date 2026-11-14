/* SPDX-License-Identifier: MIT
 * Independent external timing, loss/reacquisition, transport and Run requirements.
 */
#include "app.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
static unsigned circular_distance(unsigned a,unsigned b) {
    unsigned d=a>b?a-b:b-a;return d>NF_SYNC_CYCLE/2?NF_SYNC_CYCLE-d:d;
}
static void tracking(unsigned ppqn,unsigned beat_us,unsigned duration_ms,uint32_t origin) {
    NfSync s;nf_sync_init(&s,ppqn);nf_sync_enable(&s,true);
    unsigned pulse=0;uint32_t anchor=0;bool started=false;
    for(unsigned ms=0;ms<=duration_ms;++ms) {
        uint32_t elapsed=ms*1000u;
        while((uint64_t)pulse*beat_us/ppqn<=elapsed) {
            uint32_t edge=(uint32_t)((uint64_t)pulse*beat_us/ppqn);
            nf_sync_edge(&s,origin+edge);++pulse;
            if(s.restart && !started) {anchor=edge;started=true;}
        }
        unsigned step=nf_sync_tick(&s,origin+elapsed);
        assert(step<=450);
        if(s.restart)s.restart=false;
        if(started && elapsed>anchor+beat_us) {
            unsigned expected=(unsigned)(((uint64_t)(elapsed-anchor)*60000u/beat_us)%NF_SYNC_CYCLE);
            assert(circular_distance(s.phase,expected)<500u);
        }
    }
    assert(started && s.locked && s.active);
    assert(abs((int)nf_sync_bpm(&s)-(int)((60000000u+beat_us/2)/beat_us))<=1);
}
int main(void) {
    for(unsigned p=0;p<NF_PPQN_COUNT;++p) {
        tracking(nf_ppqn_values[p],500000,30000,0);
        tracking(nf_ppqn_values[p],436681,30000,0); /* 137.4 BPM: not rounded to integer tempo. */
        tracking(nf_ppqn_values[p],200000,3000,0xffff0000u); /* Timer wrap. */
        tracking(nf_ppqn_values[p],6000000,13000,0);
    }
    NfSync s;nf_sync_init(&s,1);nf_sync_enable(&s,true);
    nf_sync_edge(&s,0);assert(!s.active && !s.locked);
    nf_sync_edge(&s,500000);assert(s.active && s.locked && s.restart);
    unsigned rejected=s.rejected;nf_sync_edge(&s,500100);assert(s.rejected==rejected+1 && s.last_us==500000);
    s.restart=false;(void)nf_sync_tick(&s,1501001);assert(!s.active && !s.locked && s.lost);
    nf_sync_edge(&s,2000000);assert(!s.locked);nf_sync_edge(&s,2500000);assert(s.locked && s.restart);
    nf_sync_enable(&s,false);assert(!s.active);nf_sync_enable(&s,true);assert(!s.active);
    nf_sync_edge(&s,3000000);assert(s.active && s.restart);

    OriginalApp a;original_app_init(&a,15,0);a.ui.running=true;original_app_sync(&a);
    for(unsigned i=0;i<125;++i)original_app_tick(&a);
    assert(a.clock.phase==15000);
    a.ui.ppqn_index=0;original_app_sync(&a);assert(a.clock.phase==15000); /* Internal PPQN edits do not reset. */
    a.ui.run_mode=NF_RUN_GATE;original_app_sync(&a);
    assert(!a.clock.running);for(unsigned i=0;i<8;++i)assert(!a.output[i]);
    original_app_input(&a,NF_INPUT_RUN_HIGH,a.now_us);original_app_tick(&a);assert(a.clock.running);
    original_app_input(&a,NF_INPUT_RUN_LOW,a.now_us);original_app_tick(&a);assert(!a.clock.running);
    a.ui.run_mode=NF_RUN_RESET;original_app_sync(&a);assert(a.clock.running);
    for(unsigned i=0;i<200;++i)original_app_tick(&a);
    original_app_input(&a,NF_INPUT_RUN_HIGH,a.now_us);original_app_tick(&a);assert(a.clock.phase==120);
    for(unsigned i=0;i<20;++i)original_app_tick(&a);
    original_app_input(&a,NF_INPUT_RUN_HIGH,a.now_us);original_app_tick(&a);assert(a.clock.phase==2640); /* No repeated reset while held. */
    a.ui.running=false;original_app_sync(&a);original_app_input(&a,NF_INPUT_RUN_LOW,a.now_us);
    original_app_input(&a,NF_INPUT_RUN_HIGH,a.now_us);original_app_tick(&a);assert(!a.clock.running);

    original_app_init(&a,15,0);a.ui.external=true;a.ui.ppqn_index=0;a.ui.running=true;original_app_sync(&a);
    assert(!a.clock.running && a.ui.sync_status==5);
    original_app_input(&a,NF_INPUT_CLOCK,0);original_app_tick_at(&a,0);assert(!a.clock.running);
    original_app_input(&a,NF_INPUT_CLOCK,500000);original_app_tick_at(&a,500000);
    assert(a.clock.running && a.output[0]==1023 && a.clock.phase==0);
    for(unsigned i=1;i<250;++i)original_app_tick_at(&a,500000+i*1000);
    assert(a.output[0]==1023);original_app_tick_at(&a,750000);assert(!a.output[0]);
    for(unsigned i=751;i<=1502;++i)original_app_tick_at(&a,i*1000);
    assert(!a.clock.running);for(unsigned i=0;i<8;++i)assert(!a.output[i]);
    (void)original_app_refresh_ui(&a);assert(a.ui.sync_status==3 && !a.ui.active && a.ui.bpm==120);
    a.ui.external=false;original_app_sync(&a);assert(a.clock.running && a.clock.bpm==120);
    /* Tempo changes and jitter settle without reversing phase or losing lock. */
    nf_sync_init(&s,24);nf_sync_enable(&s,true);uint32_t next_edge=0;
    for(unsigned ms=0;ms<20000;++ms) {
        uint32_t now=ms*1000;
        while(next_edge<=now) {
            nf_sync_edge(&s,next_edge);
            uint32_t interval=ms<5000?20833u:ms<10000?18200u:25000u;
            next_edge+=interval;
        }
        unsigned step=nf_sync_tick(&s,now);assert(step<=450);
        if(s.restart)s.restart=false;
        if(ms>1000)assert(s.active && s.locked);
    }
    assert(nf_sync_bpm(&s)==100);

    /* Run gate plus a coincident clock starts on that edge after pre-lock. */
    original_app_init(&a,15,0);a.ui.external=true;a.ui.ppqn_index=0;
    a.ui.running=true;a.ui.run_mode=NF_RUN_GATE;original_app_sync(&a);
    original_app_input(&a,NF_INPUT_CLOCK,0);original_app_tick_at(&a,0);
    original_app_input(&a,NF_INPUT_CLOCK,500000);original_app_tick_at(&a,500000);assert(!a.clock.running);
    original_app_input(&a,NF_INPUT_RUN_HIGH,1000000);original_app_input(&a,NF_INPUT_CLOCK,1000000);
    original_app_tick_at(&a,1000000);assert(a.clock.running && a.clock.phase==0);
    a.ui.run_mode=NF_RUN_RESET;original_app_sync(&a);
    original_app_input(&a,NF_INPUT_RUN_LOW,1500000);original_app_input(&a,NF_INPUT_RUN_HIGH,1500000);
    original_app_input(&a,NF_INPUT_CLOCK,1500000);original_app_tick_at(&a,1500000);
    assert(a.clock.running && a.clock.phase==0 && !a.reset_input);
    return 0;
}
