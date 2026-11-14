/* Mute is a final voltage mask; transport and all latent voice state keep time. */
#include "app.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t now;
static void click(OriginalUi *u){now+=30;(void)original_ui_update(u,11,now);now+=21;(void)original_ui_update(u,11,now);now+=10;(void)original_ui_update(u,15,now);now+=21;(void)original_ui_update(u,15,now);}
static void left(OriginalUi *u){const uint8_t pins[]={1,0,2,3};for(unsigned i=0;i<4;++i)(void)original_ui_update(u,(uint8_t)(12u|pins[i]),++now);}
static void gestures(void){
    OriginalUi u;original_ui_init(&u,15,0);now=0;u.page=6;click(&u);left(&u);assert(u.field==NF_CLEAR_FIELD);left(&u);assert(u.field==NF_COPY_FIELD);left(&u);assert(u.field==NF_MUTE_FIELD);
    OriginalUi before=u;click(&u);assert(u.muted==(1u<<5) && !u.editing && u.selecting);original_ui_render(&u);click(&u);assert(!u.muted);
    assert(!memcmp(u.outputs,before.outputs,sizeof u.outputs) && !memcmp(u.routes,before.routes,sizeof u.routes));assert(u.reset_count==before.reset_count && u.output_reset[5]==before.output_reset[5]);
}
static void same_voice(const OriginalApp *a,const OriginalApp *b,unsigned i){
    const NfVoice *v=&a->voices[i],*w=&b->voices[i];
    assert(v->phase==w->phase && v->random==w->random && v->chance==w->chance && v->index==w->index && v->allowed==w->allowed && v->swing_odd==w->swing_odd);
    assert(v->slew_position==w->slew_position && v->slew_remaining==w->slew_remaining && v->slew_target==w->slew_target && v->slew_primed==w->slew_primed);
}
static void continuity(void){
    for(unsigned selected=0;selected<8;++selected){
        OriginalApp reference;original_app_init(&reference,15,0);reference.ui.running=true;
        reference.ui.outputs[selected].shape=4;reference.ui.outputs[selected].ratio=5;reference.ui.tones[selected]=(NfTone){3,1,4,25};
        original_app_sync(&reference);for(unsigned i=0;i<10000 && !reference.voices[selected].slew_remaining;++i)original_app_tick(&reference);assert(reference.voices[selected].slew_remaining);for(unsigned i=0;i<37;++i)original_app_tick(&reference);
        OriginalApp muted=reference;muted.ui.muted=(uint8_t)(1u<<selected);original_app_sync(&muted);original_app_sync(&reference);
        assert(muted.ui.running && muted.clock.running && muted.requested && !muted.output[selected]);
        for(unsigned t=0;t<4000;++t){
            original_app_cv(&reference,4095,4095,3);original_app_cv(&muted,4095,4095,3);original_app_tick(&reference);original_app_tick(&muted);
            assert(reference.clock.phase==muted.clock.phase && reference.clock.beats==muted.clock.beats);
            for(unsigned i=0;i<8;++i){same_voice(&reference,&muted,i);if(i==selected)assert(!muted.output[i]);else assert(muted.output[i]==reference.output[i]);}
        }
        muted.ui.muted=0;original_app_sync(&muted);original_app_sync(&reference);assert(muted.output[selected]==reference.output[selected] && muted.output[selected]>0);same_voice(&reference,&muted,selected);
        for(unsigned shape=0;shape<NF_SHAPES;++shape){muted.ui.outputs[selected].shape=(uint8_t)shape;muted.ui.outputs[selected].offset=100;muted.ui.muted=(uint8_t)(1u<<selected);original_app_sync(&muted);
            for(unsigned t=0;t<1000;++t){original_app_tick(&muted);assert(!muted.output[selected]);}}
        muted.ui.muted=255;original_app_sync(&muted);unsigned phase=muted.clock.phase;for(unsigned t=0;t<111;++t)original_app_tick(&muted);assert(muted.clock.phase!=phase && muted.clock.running);for(unsigned i=0;i<8;++i)assert(!muted.output[i]);
        muted.ui.running=false;original_app_sync(&muted);muted.ui.muted=0;original_app_sync(&muted);for(unsigned i=0;i<8;++i)assert(!muted.output[i]);
    }
}
static void external(void){
    OriginalApp reference;original_app_init(&reference,15,0);reference.ui.external=true;reference.ui.ppqn_index=0;reference.ui.running=true;original_app_sync(&reference);
    OriginalApp muted=reference;muted.ui.muted=1;original_app_sync(&muted);
    for(unsigned t=0;t<4000;++t){if(t%500u==0){original_app_input(&reference,NF_INPUT_CLOCK,t*1000u);original_app_input(&muted,NF_INPUT_CLOCK,t*1000u);}original_app_tick_at(&reference,t*1000u);original_app_tick_at(&muted,t*1000u);same_voice(&reference,&muted,0);assert(!muted.output[0]);}
    muted.ui.muted=0;original_app_sync(&muted);original_app_sync(&reference);assert(muted.output[0]==reference.output[0]);
}
int main(void){gestures();continuity();external();puts("Mute: direct toggle, all waveforms/outputs, offset/CV masking, live phase/random/slew continuity, unmute and external sync passed.");}
