/* Independently checked musical behavior, timing and cross-output semantics. */
#include "app.h"
#include "settings.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static NfVoice voice(unsigned shape) {
    NfVoice v={.config={5,(uint8_t)shape,50,0,100,0}};nf_voice_reset(&v,0);return v;
}
static void hold(void) {
    NfVoice v=voice(4);nf_voice_tone(&v,(NfTone){1,0,2,1});v.allowed=false;
    assert(nf_voice_output(&v,true,120,true)==0);
    v.allowed=true;v.random=400;unsigned first=nf_voice_output(&v,true,120,true);assert(first>0);
    v.random=800;(void)nf_voice_output(&v,true,120,true);assert(v.slew_remaining==19);
    v.allowed=false;v.random=0;unsigned previous=first;
    for(unsigned ms=0;ms<19;++ms){unsigned next=nf_voice_output(&v,true,120,true);assert(next>=previous);previous=next;}
    assert(v.slew_remaining==0 && previous==v.held_pitch);
    for(unsigned ms=0;ms<1000;++ms)assert(nf_voice_output(&v,true,120,true)==previous);
    assert(!nf_voice_output(&v,false,120,true));nf_voice_reset(&v,0);v.allowed=false;assert(!nf_voice_output(&v,true,120,true));
    v.tone.scale=0;assert(!nf_voice_output(&v,true,120,true));
}
static void waves(void) {
    NfVoice tri=voice(1);tri.config.width=25;tri.phase=15000;assert(nf_voice_value(&tri,true,120)==1023);
    tri.phase=30000;assert(nf_voice_value(&tri,true,120)>670 && nf_voice_value(&tri,true,120)<690);
    NfVoice sine=voice(5);
    for(unsigned ms=0;ms<500;++ms) {
        sine.phase=ms*120;double expected=(1.0-cos(6.283185307179586*ms/500.0))*511.5;
        assert(fabs((double)nf_voice_value(&sine,true,120)-expected)<6.0);
    }
    NfVoice env=voice(6);env.phase=2000;unsigned early=nf_voice_value(&env,true,120);
    env.phase=20000;assert(nf_voice_value(&env,true,120)<early);env.phase=50000;assert(!nf_voice_value(&env,true,120));
    env.config.width=99;assert(nf_voice_value(&env,true,120)>0);
    for(unsigned ratio=0;ratio<NF_RATIOS;++ratio)for(unsigned shape=0;shape<NF_SHAPES;++shape) {
        NfVoice v=voice(shape);v.config.ratio=(uint8_t)ratio;v.config.phase=99;v.config.width=1;
        for(unsigned tick=0;tick<100;++tick) {nf_voice_advance(&v,300);assert(nf_voice_value(&v,true,300)<=1023);}
        v.config.width=99;v.phase=60000u*nf_divisors[ratio]-1u;assert(nf_voice_value(&v,true,300)<=1023);
    }
    /* /512 previously overflowed phase*1024; check against a wide oracle. */
    NfVoice slow=voice(2);slow.config.ratio=15;slow.phase=60000u*512u*3u/4u;
    assert(nf_voice_value(&slow,true,120)==768);nf_voice_retime(&slow,24);assert(slow.phase==45000);
    /* X3/2 is thirty cycles in ten seconds at 120 BPM. */
    NfVoice fractional=voice(0);fractional.config.ratio=17;fractional.rhythm.steps=32;fractional.rhythm.hits=32;nf_voice_reset(&fractional,0);
    for(unsigned ms=0;ms<10000;++ms)nf_voice_advance(&fractional,120);
    assert(fractional.index==30 && fractional.phase==0);
}
static void beat_loops(void) {
    NfVoice v=voice(4);v.config.ratio=8;v.rhythm=(NfRhythm){5,3,1,37,0};v.extra.loop_beats=3;nf_voice_reset(&v,0);NfVoice start=v;
    for(unsigned ms=0;ms<1500;++ms)nf_voice_advance(&v,120);
    assert(v.phase==0 && v.index==0 && v.loop_phase==0 && v.random==start.random && v.chance==start.chance && v.allowed==start.allowed);
    for(unsigned ms=0;ms<1499;++ms)nf_voice_advance(&v,120);
    nf_voice_advance(&v,150);assert(v.loop_phase==30 && v.phase==120 && v.index==0);
    NfVoice slow=voice(4);slow.config.ratio=0;slow.extra.loop_beats=2;nf_voice_reset(&slow,0);
    for(unsigned ms=0;ms<1000;++ms)nf_voice_advance(&slow,120);
    assert(slow.phase==0 && slow.random==slow.seed);
}
static void logic(void) {
    for(unsigned dest=0;dest<8;++dest)for(unsigned partner=0;partner<8;++partner)for(unsigned op=1;op<=3;++op)for(unsigned bits=0;bits<4;++bits) {
        if(dest==partner)continue;
        OriginalApp a;original_app_init(&a,15,0);
        for(unsigned i=0;i<8;++i)a.ui.outputs[i]=(NfConfig){5,0,50,0,0,0};
        bool x=(bits&1)!=0,y=(bits&2)!=0;
        a.ui.outputs[dest].offset=x?100:0;a.ui.outputs[partner].offset=y?100:0;
        a.ui.extras[dest]=(NfExtra){0,(uint8_t)op,(uint8_t)partner};
        /* Bidirectional combinations must both use original raw values. */
        a.ui.extras[partner]=(NfExtra){0,3,(uint8_t)dest};a.ui.running=true;original_app_sync(&a);
        bool result=op==1?(x&&y):op==2?(x||y):(x!=y);
        assert(a.output[dest]==(result?1023:0));assert(a.output[partner]==((x!=y)?1023:0));
        a.ui.muted=(uint8_t)(1u<<dest);original_app_sync(&a);assert(a.output[dest]==0);
        a.ui.running=false;original_app_sync(&a);for(unsigned i=0;i<8;++i)assert(a.output[i]==0);
    }
}
static void cv(void) {
    OriginalApp a;original_app_init(&a,15,0);a.ui.rhythms[0]=(NfRhythm){8,0,0,0,0};a.ui.grooves[0].swing=0;
    for(unsigned f=6;f<NF_CV_FIELDS;++f)a.ui.routes[0][f]=(NfCvRoute){1,100,0};
    a.ui.running=true;original_app_sync(&a);original_app_cv(&a,4095,0,1);
    for(unsigned i=0;i<100;++i)original_app_tick(&a);
    assert(a.voices[0].rhythm.hits==8 && a.voices[0].rhythm.rotate==7 && a.voices[0].rhythm.probability==100 && a.voices[0].groove.swing==50);
    uint32_t phase=a.voices[0].phase,random=a.voices[0].random;
    for(unsigned i=0;i<100;++i)original_app_sync(&a);
    assert(a.voices[0].phase==phase && a.voices[0].random==random);
    original_app_cv(&a,0,0,1);for(unsigned i=0;i<250;++i)original_app_tick(&a);
    assert(a.voices[0].rhythm.hits==0 && a.voices[0].rhythm.rotate==0 && a.voices[0].rhythm.probability==0 && a.voices[0].groove.swing==0);
    assert(a.ui.rhythms[0].hits==0 && a.ui.rhythms[0].probability==0);
    original_app_cv(&a,4095,0,0);for(unsigned i=0;i<8;++i)original_app_tick(&a);assert(!a.voices[0].rhythm.hits);
}
static void persistence(void) {
    OriginalUi u;original_ui_init(&u,15,0);u.extras[0]=(NfExtra){128,3,7};u.outputs[0].ratio=29;u.outputs[0].shape=6;
    for(unsigned f=6;f<NF_CV_FIELDS;++f)u.routes[0][f]=(NfCvRoute){2,-73,21};
    NfSettings s,t;nf_settings_capture(&u,&s);uint8_t bytes[NF_SETTINGS_BYTES];assert(nf_settings_encode(&s,bytes));assert(nf_settings_decode(&t,bytes));
    assert(t.extras[0].loop_beats==128 && t.extras[0].logic==3 && t.extras[0].partner==7 && t.outputs[0].ratio==29 && t.outputs[0].shape==6);
    assert(t.routes[0][9].depth==-73 && t.routes[0][9].bias==21);
    assert(original_ui_copy_output(&u,0,7));assert(u.extras[7].loop_beats==128 && u.routes[7][9].source==2);
    assert(original_ui_clear_output(&u,7));assert(!u.extras[7].loop_beats && !u.extras[7].logic && !u.routes[7][9].source);
    bytes[304]=4;assert(!nf_settings_decode(&t,bytes));bytes[304]=3;bytes[305]=8;assert(!nf_settings_decode(&t,bytes));
}
int main(void){hold();waves();beat_loops();logic();cv();persistence();puts("Pitch hold/glide, waveform oracles, all ratios, beat loops, simultaneous logic, expanded CV and presets passed.");}
