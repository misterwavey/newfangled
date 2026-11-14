/* Timing oracles use elapsed phase and observed edges, independent of scheduling. */
#include "app.h"
#include <assert.h>
#include <stdio.h>
static NfVoice voice(unsigned swing,unsigned ratchet,unsigned ratio,unsigned shape) {
    NfVoice v={.config={(uint8_t)ratio,(uint8_t)shape,50,0,100,0},.rhythm={3,3,0,100,0},.groove={(uint8_t)swing,(uint8_t)ratchet}};nf_voice_reset(&v,0);return v;
}
static void swing(void) {
    const unsigned bpms[]={10,120,137,300},swings[]={0,1,33,50};
    for(unsigned r=0;r<NF_RATIOS;++r)for(unsigned b=0;b<4;++b)for(unsigned s=0;s<4;++s) {
        NfVoice v=voice(swings[s],1,r,0);uint32_t base=60000u*nf_divisors[r],longer=base+base*swings[s]/100u,inc=bpms[b]*nf_multipliers[r];
        for(unsigned t=0;t<30000;++t) {
            uint64_t elapsed=(uint64_t)t*inc;unsigned pair=(unsigned)(elapsed%(2u*base));bool odd=pair>=longer;
            assert(v.swing_odd==odd);assert(v.phase==(odd?pair-longer:pair));
            unsigned cycles=(unsigned)(2u*(elapsed/(2u*base)))+(odd?1u:0u);assert(v.index==cycles%3u);nf_voice_tick(&v,bpms[b]);
        }
    }
    NfVoice v=voice(33,1,5,0);unsigned edges[5],n=0;bool was=false;
    for(unsigned t=0;t<2001;++t){bool high=nf_voice_value(&v,true,120)>0;if(high && !was && n<5)edges[n++]=t;was=high;nf_voice_tick(&v,120);}
    unsigned expected[]={0,665,1000,1665,2000};assert(n==5);for(unsigned i=0;i<n;++i)assert(edges[i]==expected[i]);
    v=voice(50,1,5,0);for(unsigned i=0;i<800;++i)nf_voice_tick(&v,120);assert(v.swing_odd && v.index==1);
    unsigned phase=v.phase;nf_voice_retime(&v,4);assert(v.swing_odd && v.index==1 && v.phase<=phase*2u && phase*2u-v.phase<60u);
    nf_voice_reset(&v,0);assert(!v.swing_odd && !v.index && !v.phase);
}
static void ratchets(void) {
    NfVoice v=voice(0,4,5,0);unsigned edges[5],n=0;bool was=false;
    for(unsigned t=0;t<=500;++t){bool high=nf_voice_value(&v,true,120)>0;if(high&&!was && n<5)edges[n++]=t;was=high;nf_voice_tick(&v,120);}
    assert(n==5);for(unsigned i=0;i<5;++i)assert(edges[i]==i*125u);
    /* Every requested pulse must be observable when sampling capacity permits. */
    for(unsigned count=1;count<=8;++count)for(unsigned width=1;width<=99;width+=98) {
        v=voice(0,count,5,0);v.config.width=(uint8_t)width;n=0;was=false;
        for(unsigned t=0;t<500;++t){bool high=nf_voice_value(&v,true,120)>0;if(high&&!was)++n;was=high;nf_voice_tick(&v,120);}assert(n==count);
    }
    v=voice(50,8,10,0);assert(nf_voice_ratchets(&v,300)==8);for(unsigned i=0;i<19;++i)nf_voice_tick(&v,300);assert(v.swing_odd);assert(nf_voice_ratchets(&v,300)==3);
    for(unsigned shape=1;shape<NF_SHAPES;++shape){v=voice(33,8,5,shape);NfVoice plain=v;plain.groove.ratchet=1;assert(nf_voice_ratchets(&v,120)==1);
        for(unsigned t=0;t<5000;++t){assert(nf_voice_value(&v,true,120)==nf_voice_value(&plain,true,120));nf_voice_tick(&v,120);nf_voice_tick(&plain,120);}}
    /* Ratchets never reroll probability or random state inside a step. */
    v=voice(33,8,5,0);v.rhythm.probability=37;nf_voice_reset(&v,0);NfVoice plain=v;plain.groove.ratchet=1;
    for(unsigned t=0;t<20000;++t){assert(v.allowed==plain.allowed && v.chance==plain.chance && v.random==plain.random);nf_voice_tick(&v,120);nf_voice_tick(&plain,120);}
    v.rhythm.hits=0;nf_voice_reset(&v,0);v.config.offset=100;for(unsigned t=0;t<1000;++t){assert(!nf_voice_value(&v,true,120));nf_voice_tick(&v,120);}
}
static void integration(void) {
    OriginalApp a;original_app_init(&a,15,0);a.ui.grooves[0]=(NfGroove){33,4};a.ui.running=true;original_app_sync(&a);
    for(unsigned i=0;i<665;++i)original_app_tick(&a);assert(a.voices[0].swing_odd);
    bool odd=a.voices[0].swing_odd;uint32_t phase=a.voices[0].phase;
    a.ui.grooves[1]=(NfGroove){50,8};original_app_sync(&a);assert(a.voices[0].phase==phase && a.voices[0].swing_odd==odd);
    ++a.ui.reset_count;original_app_sync(&a);assert(!a.voices[0].swing_odd && !a.voices[0].index);
    a.ui.running=false;original_app_sync(&a);for(unsigned i=0;i<8;++i)assert(!a.output[i]);
}
static void external(void) {
    OriginalApp a;original_app_init(&a,15,0);a.ui.external=true;a.ui.ppqn_index=0;a.ui.running=true;a.ui.grooves[0]=(NfGroove){33,4};original_app_sync(&a);
    unsigned edges[9],n=0;bool was=false;
    for(unsigned t=0;t<=1500;++t){
        if(t%500u==0)original_app_input(&a,NF_INPUT_CLOCK,t*1000u);
        original_app_tick_at(&a,t*1000u);bool high=a.output[0]>0;
        if(high && !was && n<9)edges[n++]=t;was=high;
    }
    unsigned expected[]={500,667,833,999,1165,1249,1333,1417,1500};assert(n==9);
    for(unsigned i=0;i<n;++i){int error=(int)edges[i]-(int)expected[i];assert(error>=-1 && error<=1);}
    for(unsigned t=1501;t<3000;++t)original_app_tick_at(&a,t*1000u);
    assert(!a.clock.running);for(unsigned i=0;i<8;++i)assert(!a.output[i]);
}
int main(void){swing();ratchets();integration();external();puts("Swing pair alignment, ratchet edges/caps, probability, shapes, reset and Stop passed.");}
