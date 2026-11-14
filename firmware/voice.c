/* SPDX-License-Identifier: MIT
 * Independently authored unipolar 10-bit waveforms on a 1 kHz timebase.
 */
#include "voice.h"
const uint8_t nf_multipliers[NF_RATIOS]={1,1,1,1,1,1,2,3,4,8,16};
const uint8_t nf_divisors[NF_RATIOS]={16,8,4,3,2,1,1,1,1,1,1};
const char *const nf_ratio_names[NF_RATIOS]={"/16","/8","/4","/3","/2","X1","X2","X3","X4","X8","X16"};
const char *const nf_shape_names[NF_SHAPES]={"GATE","TRIANGLE","RAMP UP","RAMP DOWN","RANDOM"};
static uint32_t period(const NfVoice *v) {return 60000u*nf_divisors[v->config.ratio];}
static uint32_t cycle_span(const NfVoice *v) {
    uint32_t base=period(v),delay=600u*nf_divisors[v->config.ratio]*v->groove.swing;
    return v->swing_odd?base-delay:base+delay;
}
unsigned nf_voice_ratchets(const NfVoice *v,unsigned bpm) {
    if(v->config.shape || v->groove.ratchet<=1)return 1;
    unsigned increment=bpm*nf_multipliers[v->config.ratio];if(!increment)increment=1;
    unsigned maximum=cycle_span(v)/(2u*increment);if(!maximum)maximum=1;
    unsigned count=v->groove.ratchet?v->groove.ratchet:1u;
    return count>maximum?maximum:count;
}
static uint32_t random_next(uint32_t x) {x^=x<<13;x^=x>>17;x^=x<<5;return x;}
uint32_t nf_rhythm_mask(NfRhythm r) {
    if(!r.steps || r.steps>32 || r.hits>r.steps || r.rotate>=r.steps)return 0;
    uint32_t mask=0;unsigned bucket=0,position=r.rotate;
    for(unsigned i=0;i<r.steps;++i) {
        if(bucket<r.hits)mask|=1u<<position;
        bucket+=r.hits;if(bucket>=r.steps)bucket-=r.steps;
        if(++position>=r.steps)position=0;
    }
    return mask;
}
static void decide(NfVoice *v) {
    v->chance=random_next(v->chance);
    v->allowed=(v->mask&(1u<<v->index)) && (v->rhythm.probability==100 || v->chance%100u<v->rhythm.probability);
}
static void restart(NfVoice *v) {
    v->phase=0;v->swing_odd=false;v->index=0;v->random=v->seed;v->chance=v->seed^0xa511e9b3u;decide(v);
}
void nf_voice_reset(NfVoice *v,unsigned channel) {
    if(!v->rhythm.steps)v->rhythm=NF_RHYTHM_DEFAULT;
    if(!v->groove.ratchet)v->groove=NF_GROOVE_DEFAULT;
    v->seed=0x9e3779b9u*(channel+1u);v->mask=nf_rhythm_mask(v->rhythm);restart(v);
}
void nf_voice_rhythm(NfVoice *v,NfRhythm r) {
    if(!r.steps)r=NF_RHYTHM_DEFAULT;
    NfRhythm old=v->rhythm;
    if(old.steps==r.steps && old.hits==r.hits && old.rotate==r.rotate && old.probability==r.probability && old.loop==r.loop)return;
    v->rhythm=r;v->mask=nf_rhythm_mask(r);restart(v);
}
void nf_voice_groove(NfVoice *v,NfGroove g) {
    if(!g.ratchet)g=NF_GROOVE_DEFAULT;
    if(v->groove.swing==g.swing && v->groove.ratchet==g.ratchet)return;
    v->groove=g;restart(v);
}
void nf_voice_retime(NfVoice *v,unsigned ratio) {
    if(ratio==v->config.ratio)return;
    uint32_t position=v->phase*1024u/cycle_span(v);
    v->config.ratio=(uint8_t)ratio;v->phase=position*cycle_span(v)/1024u;
}
void nf_voice_tick(NfVoice *v,unsigned bpm) {nf_voice_advance(v,bpm);}
void nf_voice_advance(NfVoice *v,unsigned step) {
    uint32_t span=cycle_span(v);v->phase+=step*nf_multipliers[v->config.ratio];
    while(v->phase>=span) {
        v->phase-=span;v->swing_odd=!v->swing_odd;
        if(++v->index>=v->rhythm.steps) {
            v->index=0;
            if(v->rhythm.loop) {v->random=v->seed;v->chance=v->seed^0xa511e9b3u;}
            else v->random=random_next(v->random);
        } else v->random=random_next(v->random);
        decide(v);span=cycle_span(v);
    }
}
uint16_t nf_voice_value(const NfVoice *v,bool running,unsigned bpm) {
    if(!running || !v->allowed)return 0;
    uint32_t span=cycle_span(v),phase=v->phase+(span/100u)*v->config.phase;
    if(phase>=span)phase-=span;
    uint32_t p=phase*1024u/span,raw;
    switch(v->config.shape) {
    case 1:raw=p<512u?p*2u:(1023u-p)*2u;break;
    case 2:raw=p;break;
    case 3:raw=1023u-p;break;
    case 4:raw=v->random&1023u;break;
    default: {
        unsigned ratchets=nf_voice_ratchets(v,bpm);
        if(ratchets>1)phase=(phase*ratchets)%span;
        uint32_t step=bpm*nf_multipliers[v->config.ratio]*ratchets,width=(span/100u)*v->config.width;
        /* Guarantee at least one sampled high and low at fast clock ratios. */
        if(width<step)width=step;
        if(width>span-step)width=span-step;
        raw=phase<width?1023u:0u;break;
    }
    }
    uint32_t value=(raw*v->config.level+50u)/100u+(1023u*v->config.offset+50u)/100u;
    return (uint16_t)(value>1023u?1023u:value);
}
