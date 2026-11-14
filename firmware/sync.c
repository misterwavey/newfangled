/* SPDX-License-Identifier: MIT
 * Timestamp-based period estimator and bounded phase correction. No ALM code.
 */
#include "sync.h"
const uint8_t nf_ppqn_values[NF_PPQN_COUNT]={1,2,4,8,12,16,24,48};
void nf_sync_init(NfSync *s,unsigned ppqn) {
    bool valid=false;for(unsigned i=0;i<NF_PPQN_COUNT;++i)if(ppqn==nf_ppqn_values[i])valid=true;
    *s=(NfSync){.ppqn=valid?ppqn:24};
}
void nf_sync_rearm(NfSync *s) {s->active=false;s->restart=false;s->correction=0;s->remainder=0;}
void nf_sync_enable(NfSync *s,bool enabled) {
    if(s->enabled!=enabled)nf_sync_rearm(s);
    s->enabled=enabled;
}
unsigned nf_sync_bpm(const NfSync *s) {return s->locked?s->bpm:0;}
void nf_sync_edge(NfSync *s,uint32_t us) {
    if(s->have_edge) {
        uint32_t interval=us-s->last_us;
        /* Acquisition margins around the supported 10..300 BPM range. Fast
           glitches do not move the reference timestamp. Long gaps reacquire. */
        if(interval<180000u/s->ppqn) {++s->rejected;return;}
        if(interval>6600000u/s->ppqn) {
            s->count=0;s->sum=0;s->next=0;s->locked=false;s->lost=true;nf_sync_rearm(s);
        } else {
            if(s->count==8)s->sum-=s->intervals[s->next];else ++s->count;
            s->intervals[s->next]=interval;s->sum+=interval;s->next=(s->next+1u)&7u;
            s->beat_us=(s->sum*s->ppqn+s->count/2u)/s->count;
            s->bpm=(60000000u+s->beat_us/2u)/s->beat_us;s->locked=true;s->lost=false;
        }
    }
    s->last_us=us;s->have_edge=true;s->lost=false;
    if(!s->enabled || !s->locked)return;
    if(!s->active) {
        s->active=true;s->restart=true;s->phase=0;s->pulse=0;s->remainder=0;s->correction=0;
    } else {
        s->pulse=(s->pulse+1u)%(48u*s->ppqn);
        uint32_t lag=us-s->last_tick_us;if(lag>10000u)lag=0;
        uint32_t predicted=(s->phase+lag*60000u/s->beat_us)%NF_SYNC_CYCLE;
        int32_t error=(int32_t)(s->pulse*(60000u/s->ppqn))-(int32_t)predicted;
        if(error>(int32_t)NF_SYNC_CYCLE/2)error-=(int32_t)NF_SYNC_CYCLE;
        if(error<-(int32_t)NF_SYNC_CYCLE/2)error+=(int32_t)NF_SYNC_CYCLE;
        s->correction=error;
    }
}
unsigned nf_sync_tick(NfSync *s,uint32_t now_us) {
    s->last_tick_us=now_us;
    uint32_t timeout=s->locked?2u*(s->beat_us/s->ppqn)+1000u:6600000u/s->ppqn;
    if(timeout<100000u)timeout=100000u;
    if(s->have_edge && (uint32_t)(now_us-s->last_us)>timeout) {
        s->have_edge=false;s->locked=false;s->lost=true;s->count=0;s->sum=0;s->next=0;nf_sync_rearm(s);
    }
    if(!s->active || !s->enabled || s->restart)return 0;
    s->remainder+=60000000u;
    unsigned step=s->remainder/s->beat_us;s->remainder%=s->beat_us;
    int32_t limit=(int32_t)(step/4u+1u),adjust=s->correction;
    if(adjust>limit)adjust=limit;
    if(adjust< -limit)adjust= -limit;
    s->correction-=adjust;step=(unsigned)((int32_t)step+adjust);
    s->phase+=step;if(s->phase>=NF_SYNC_CYCLE)s->phase-=NF_SYNC_CYCLE;
    return step;
}
