/* Independent behavioral oracles for Euclidean patterns and stochastic cycles. */
#include "app.h"
#include <assert.h>
#include <stdio.h>
static unsigned count(uint32_t bits) {unsigned n=0;while(bits){n+=bits&1u;bits>>=1;}return n;}
static void masks(void) {
    for(unsigned steps=1;steps<=32;++steps)for(unsigned hits=0;hits<=steps;++hits)for(unsigned rotate=0;rotate<steps;++rotate) {
        NfRhythm r={(uint8_t)steps,(uint8_t)hits,(uint8_t)rotate,100,0};uint32_t mask=nf_rhythm_mask(r);
        assert(count(mask)==hits);if(steps<32)assert(!(mask>>steps));
        for(unsigned i=0;i<steps;++i)assert(((mask>>i)&1u)==((((i+steps-rotate)%steps)*hits)%steps<hits));
        if(hits) {
            unsigned first=32,last=0,min=33,max=0;
            for(unsigned i=0;i<steps;++i)if(mask&(1u<<i)){if(first==32)first=i;else {unsigned gap=i-last;if(gap<min)min=gap;if(gap>max)max=gap;}last=i;}
            unsigned gap=first+steps-last;if(gap<min)min=gap;if(gap>max)max=gap;assert(max-min<=1);
        }
    }
    assert(nf_rhythm_mask((NfRhythm){8,3,0,100,0})==0x49);
    assert(nf_rhythm_mask((NfRhythm){8,3,1,100,0})==0x92);
    assert(nf_rhythm_mask((NfRhythm){32,32,0,100,0})==UINT32_MAX);
}
static NfVoice voice(unsigned channel,NfRhythm rhythm,unsigned shape) {
    NfVoice v={.config={5,(uint8_t)shape,50,0,100,100},.rhythm=rhythm};nf_voice_reset(&v,channel);return v;
}
static void cycles(void) {
    for(unsigned shape=0;shape<NF_SHAPES;++shape) {
        NfVoice v=voice(0,(NfRhythm){8,3,0,100,0},shape);
        for(unsigned cycle=0;cycle<24;++cycle) {
            assert(v.index==cycle%8);assert(v.allowed==((0x49u>>(cycle%8))&1u));
            for(unsigned tick=0;tick<500;++tick){uint16_t value=nf_voice_value(&v,true,120);if(!v.allowed)assert(value==0);else assert(value==1023);nf_voice_tick(&v,120);}
        }
        v=voice(0,(NfRhythm){1,0,0,100,0},shape);assert(!nf_voice_value(&v,true,120));
        v=voice(0,(NfRhythm){1,1,0,0,0},shape);assert(!nf_voice_value(&v,true,120));
    }
    unsigned successes=0;NfVoice v=voice(0,(NfRhythm){32,32,0,37,0},0);
    for(unsigned i=0;i<10000;++i){successes+=v.allowed;nf_voice_advance(&v,60000);}
    assert(successes>3400 && successes<4000);
    /* Locked random values and probability repeat together, including step 0. */
    v=voice(3,(NfRhythm){13,8,4,73,1},4);uint32_t values[13];bool allowed[13];
    for(unsigned i=0;i<13;++i){values[i]=v.random;allowed[i]=v.allowed;nf_voice_advance(&v,60000);}
    for(unsigned i=0;i<130;++i){assert(v.random==values[i%13] && v.allowed==allowed[i%13]);nf_voice_advance(&v,60000);}
    nf_voice_reset(&v,3);assert(v.random==values[0] && v.allowed==allowed[0]);
    NfVoice independent=voice(4,v.rhythm,4);assert(independent.random!=v.random);
    v.rhythm.loop=0;nf_voice_reset(&v,3);uint32_t first=v.random;
    for(unsigned i=0;i<13;++i)nf_voice_advance(&v,60000);assert(v.random!=first);
}
static void integration(void) {
    OriginalApp a;original_app_init(&a,15,0);a.ui.rhythms[0]=(NfRhythm){8,3,1,100,1};a.ui.running=true;original_app_sync(&a);
    assert(!a.output[0]);for(unsigned i=0;i<500;++i)original_app_tick(&a);assert(a.voices[0].index==1 && a.output[0]==1023);
    unsigned index=a.voices[0].index;nf_voice_retime(&a.voices[0],6);assert(a.voices[0].index==index);
    ++a.ui.reset_count;original_app_sync(&a);assert(a.voices[0].index==0 && !a.output[0]);
    a.ui.running=false;original_app_sync(&a);for(unsigned i=0;i<8;++i)assert(!a.output[i]);
    a.ui.running=true;original_app_sync(&a);assert(a.voices[0].index==0);
    a.ui.rhythms[0]=(NfRhythm){32,32,31,100,0};original_app_sync(&a);assert(a.voices[0].mask==UINT32_MAX);
    /* Editing another output does not restart this output's pattern. */
    for(unsigned i=0;i<500;++i)original_app_tick(&a);index=a.voices[0].index;
    a.ui.rhythms[1]=(NfRhythm){4,2,0,50,1};original_app_sync(&a);assert(a.voices[0].index==index);
}
int main(void){masks();cycles();integration();puts("Euclidean masks, all waveforms, probability, repeatable loops and transport passed.");}
