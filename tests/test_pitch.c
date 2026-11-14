/* Independent note-set and elapsed-time oracles for random pitch and glide. */
#include "app.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
static const unsigned degrees[7][12]={{0},{0,1,2,3,4,5,6,7,8,9,10,11},{0,2,4,5,7,9,11},{0,2,3,5,7,8,10},{0,2,3,5,7,9,10},{0,2,4,7,9},{0,3,5,7,10}};
static const unsigned lengths[7]={0,12,7,7,7,5,5};
static bool in_scale(unsigned scale,unsigned note){for(unsigned i=0;i<lengths[scale];++i)if(note%12==degrees[scale][i])return true;return false;}
static void notes(void) {
    for(unsigned scale=1;scale<NF_SCALES;++scale)for(unsigned root=0;root<12;++root)for(unsigned range=1;range<=4;++range) {
        NfTone tone={(uint8_t)scale,(uint8_t)root,(uint8_t)range,0};unsigned prior=0;
        for(unsigned code=0;code<1024;++code) {
            unsigned best=0,distance=UINT_MAX;
            for(unsigned n=0;n<=range*12;++n)if(in_scale(scale,n)) {
                int error=(int)(n*1023)-(int)(code*range*12);unsigned d=(unsigned)(error<0?-error:error);
                if(d<distance){distance=d;best=n;}
            }
            unsigned note=nf_pitch_note(tone,code);assert(note==root+best);assert(note>=prior && note<=59);prior=note;
        }
        assert(nf_pitch_note(tone,0)==root);assert(nf_pitch_note(tone,1023)==root+range*12);
    }
    assert(nf_pitch_note((NfTone){2,0,1,0},85)==0);assert(nf_pitch_note((NfTone){2,0,1,0},86)==2);
    assert(nf_pitch_note((NfTone){0,0,1,0},500)==255);
    assert(nf_pitch_note((NfTone){7,0,1,0},500)==255);assert(nf_pitch_note((NfTone){1,12,1,0},500)==255);
    assert(nf_pitch_note((NfTone){1,0,5,0},500)==255);
}
static NfVoice voice(void) {NfVoice v={.config={5,4,50,0,100,0}};nf_voice_reset(&v,0);return v;}
static void voltages(void) {
    NfVoice v=voice();nf_voice_tone(&v,(NfTone){1,0,1,0});
    for(unsigned semitone=0;semitone<=12;++semitone){v.random=(semitone*1023+6)/12;assert(nf_voice_value(&v,true,120)==(semitone*1023+30)/60);}
    v.random=1023;v.config.level=50;nf_voice_tone(&v,(NfTone){2,6,2,0});unsigned value=nf_voice_value(&v,true,120);
    unsigned note=nf_pitch_note(v.tone,512);assert(value==(note*1023+30)/60);
    v.config.offset=100;assert(nf_voice_value(&v,true,120)==(30u*1023u+30u)/60u);
    v.allowed=false;assert(nf_voice_value(&v,true,120)==0);v.allowed=true;assert(!nf_voice_value(&v,false,120));
    for(unsigned shape=0;shape<4;++shape){v.config.shape=(uint8_t)shape;NfVoice old=v;old.tone=NF_TONE_DEFAULT;
        for(unsigned t=0;t<1000;++t){assert(nf_voice_value(&v,true,120)==nf_voice_value(&old,true,120));nf_voice_tick(&v,120);nf_voice_tick(&old,120);}}
}
static void slew(void) {
    NfVoice v=voice();nf_voice_tone(&v,(NfTone){0,0,1,1});v.random=0;assert(nf_voice_output(&v,true,120,false)==0);
    v.random=1023;assert(nf_voice_output(&v,true,120,false)==0);assert(v.slew_remaining==20);
    for(unsigned i=0;i<100;++i)assert(nf_voice_output(&v,true,120,false)==0);assert(v.slew_remaining==20);
    for(unsigned ms=1;ms<=20;++ms){unsigned value=nf_voice_output(&v,true,120,true);int error=(int)value-(int)((1023u*ms+10)/20);assert(error>=-1 && error<=1);}
    assert(v.slew_remaining==0 && nf_voice_output(&v,true,120,false)==1023);
    v.random=0;for(unsigned ms=1;ms<=20;++ms){unsigned value=nf_voice_output(&v,true,120,true);int error=(int)value-(int)((1023u*(20-ms)+10)/20);assert(error>=-1 && error<=1);}
    v.random=1023;for(unsigned i=0;i<10;++i)(void)nf_voice_output(&v,true,120,true);unsigned midway=nf_voice_output(&v,true,120,false);assert(midway>=511 && midway<=512);
    v.random=0;unsigned next=nf_voice_output(&v,true,120,true);assert(next<midway && next>0);
    assert(!nf_voice_output(&v,false,120,false));assert(!v.slew_primed && !v.slew_remaining);
    v.random=777;assert(nf_voice_output(&v,true,120,false)==777);v.allowed=false;assert(!nf_voice_output(&v,true,120,true));v.allowed=true;assert(nf_voice_output(&v,true,120,false)==777);
    nf_voice_tone(&v,(NfTone){0,0,1,100});v.random=0;(void)nf_voice_output(&v,true,120,false);v.random=1023;
    for(unsigned i=0;i<1999;++i)(void)nf_voice_output(&v,true,120,true);assert(v.slew_remaining==1);assert(nf_voice_output(&v,true,120,true)==1023);
    v.config.shape=0;v.phase=0;assert(nf_voice_output(&v,true,120,true)==1023);v.phase=30000;assert(!nf_voice_output(&v,true,120,true));
}
static void integration(void) {
    OriginalApp a;original_app_init(&a,15,0);a.ui.outputs[0].shape=4;a.ui.tones[0]=(NfTone){3,2,3,10};a.ui.running=true;original_app_sync(&a);
    for(unsigned i=0;i<500;++i)original_app_tick(&a);assert(a.voices[0].slew_remaining>0);
    unsigned before=a.output[0],remaining=a.voices[0].slew_remaining;
    for(unsigned i=0;i<100;++i){original_app_sync(&a);assert(a.output[0]==before && a.voices[0].slew_remaining==remaining);}
    a.ui.running=false;original_app_sync(&a);for(unsigned i=0;i<8;++i)assert(!a.output[i]);
    a.ui.running=true;original_app_sync(&a);unsigned first=a.output[0];for(unsigned i=0;i<2000;++i)original_app_tick(&a);
    ++a.ui.reset_count;original_app_sync(&a);assert(a.output[0]==first && !a.voices[0].slew_remaining);
}
int main(void){notes();voltages();slew();integration();puts("Pitch scales/roots/ranges, nominal DAC notes, slew timing/retarget, timeless UI and Stop/reset passed.");}
