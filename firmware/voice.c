/* SPDX-License-Identifier: MIT
 * Independently authored unipolar 10-bit waveforms on a 1 kHz timebase.
 */
#include "voice.h"
const uint8_t nf_multipliers[NF_RATIOS]={1,1,1,1,1,1,2,3,4,8,16};
const uint8_t nf_divisors[NF_RATIOS]={16,8,4,3,2,1,1,1,1,1,1};
const char *const nf_ratio_names[NF_RATIOS]={"/16","/8","/4","/3","/2","X1","X2","X3","X4","X8","X16"};
const char *const nf_shape_names[NF_SHAPES]={"GATE","TRIANGLE","RAMP UP","RAMP DOWN","RANDOM"};
static uint32_t period(const NfVoice *v) {return 60000u*nf_divisors[v->config.ratio];}
void nf_voice_reset(NfVoice *v,unsigned channel) {v->phase=0;v->random=0x9e3779b9u*(channel+1u);}
void nf_voice_tick(NfVoice *v,unsigned bpm) {
    uint32_t span=period(v);v->phase+=bpm*nf_multipliers[v->config.ratio];
    if(v->phase>=span) {
        v->phase-=span;uint32_t x=v->random;x^=x<<13;x^=x>>17;x^=x<<5;v->random=x;
    }
}
uint16_t nf_voice_value(const NfVoice *v,bool running,unsigned bpm) {
    if(!running)return 0;
    uint32_t span=period(v),phase=v->phase+(span/100u)*v->config.phase;
    if(phase>=span)phase-=span;
    uint32_t p=phase*1024u/span,raw;
    switch(v->config.shape) {
    case 1:raw=p<512u?p*2u:(1023u-p)*2u;break;
    case 2:raw=p;break;
    case 3:raw=1023u-p;break;
    case 4:raw=v->random&1023u;break;
    default: {
        uint32_t step=bpm*nf_multipliers[v->config.ratio],width=(span/100u)*v->config.width;
        /* Guarantee at least one sampled high and low at fast clock ratios. */
        if(width<step)width=step;
        if(width>span-step)width=span-step;
        raw=phase<width?1023u:0u;break;
    }
    }
    uint32_t value=(raw*v->config.level+50u)/100u+(1023u*v->config.offset+50u)/100u;
    return (uint16_t)(value>1023u?1023u:value);
}
