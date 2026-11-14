/* Copy settings, not runtime: only the destination may restart. */
#include "app.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t now;
static void click(OriginalUi *u){now+=30;(void)original_ui_update(u,11,now);now+=21;(void)original_ui_update(u,11,now);now+=10;(void)original_ui_update(u,15,now);now+=21;(void)original_ui_update(u,15,now);}
static void turn(OriginalUi *u,bool forward){const uint8_t cw[]={2,0,1,3},ccw[]={1,0,2,3};const uint8_t *p=forward?cw:ccw;for(unsigned i=0;i<4;++i)(void)original_ui_update(u,(uint8_t)(12u|p[i]),++now);}
static void hold(OriginalUi *u){now+=30;(void)original_ui_update(u,11,now);now+=21;(void)original_ui_update(u,11,now);now+=550;(void)original_ui_update(u,11,now);now+=10;(void)original_ui_update(u,15,now);now+=21;(void)original_ui_update(u,15,now);}
static void open_copy(OriginalUi *u){click(u);turn(u,false);turn(u,false);assert(u->field==NF_COPY_FIELD);click(u);assert(u->copy_mode==1);}
static void gestures(void){
    OriginalUi u;original_ui_init(&u,15,0);now=0;u.page=3;u.outputs[2].level=73;
    open_copy(&u);assert(u.copy_target==3);
    for(unsigned i=0;i<21;++i){turn(&u,true);assert(u.copy_target<8 && u.copy_target!=2);}assert(u.copy_target==3);
    for(unsigned i=0;i<21;++i){turn(&u,false);assert(u.copy_target<8 && u.copy_target!=2);}assert(u.copy_target==3);
    original_ui_render(&u);click(&u);assert(u.copy_mode==2 && !u.copy_yes);original_ui_render(&u);click(&u);
    assert(!u.copy_mode && u.outputs[3].level==100 && !u.output_reset[3]);
    click(&u);hold(&u);assert(!u.copy_mode && !u.selecting);
    open_copy(&u);click(&u);turn(&u,true);hold(&u);assert(!u.copy_mode && !u.selecting && u.outputs[3].level==100);
    open_copy(&u);click(&u);turn(&u,true);turn(&u,false);assert(!u.copy_yes);turn(&u,true);click(&u);
    assert(!u.copy_mode && !u.selecting && !u.editing && u.page==3);assert(u.outputs[3].level==73 && u.outputs[2].level==73 && u.output_reset[3]==1 && !u.output_reset[2]);
    OriginalUi before=u;assert(!original_ui_copy_output(&u,8,0));assert(!original_ui_copy_output(&u,0,8));assert(!original_ui_copy_output(&u,3,3));assert(!memcmp(&before,&u,sizeof u));
}
static void settings_equal(const OriginalUi *u,unsigned a,unsigned b){
    assert(!memcmp(&u->outputs[a],&u->outputs[b],sizeof(NfConfig)));
    assert(!memcmp(&u->rhythms[a],&u->rhythms[b],sizeof(NfRhythm)));
    assert(!memcmp(&u->grooves[a],&u->grooves[b],sizeof(NfGroove)));
    assert(!memcmp(&u->tones[a],&u->tones[b],sizeof(NfTone)));
    assert(!memcmp(u->routes[a],u->routes[b],sizeof u->routes[a]));
}
static void isolation(void){
    for(unsigned source=0;source<8;++source)for(unsigned destination=0;destination<8;++destination){
        if(source==destination)continue;
        OriginalApp a;original_app_init(&a,15,0);a.ui.bpm=137;a.ui.running=true;a.ui.cv_swapped=true;
        a.ui.calibration[0]=(NfCvCalibration){83,3900};a.ui.calibration[1]=(NfCvCalibration){2010,120};a.ui.preset_mask=0xa5;a.ui.preset_slot=4;
        a.ui.outputs[source]=(NfConfig){6,4,23,12,83,7};a.ui.rhythms[source]=(NfRhythm){13,9,4,87,1};a.ui.grooves[source]=(NfGroove){33,4};a.ui.tones[source]=(NfTone){3,3,3,20};
        for(unsigned f=0;f<NF_FIELDS;++f)a.ui.routes[source][f]=(NfCvRoute){2,-73,21};
        a.ui.muted=(uint8_t)(source%2u?(1u<<destination):(1u<<source));
        original_app_sync(&a);for(unsigned t=0;t<777;++t)original_app_tick(&a);OriginalApp before=a;
        assert(original_ui_copy_output(&a.ui,source,destination));original_app_sync(&a);settings_equal(&a.ui,source,destination);assert(((a.ui.muted>>source)&1u)==((a.ui.muted>>destination)&1u));
        assert(a.ui.running && a.requested && a.clock.running && a.clock.phase==before.clock.phase && a.clock.beats==before.clock.beats);
        assert(a.ui.bpm==137 && a.ui.cv_swapped && a.ui.preset_mask==0xa5 && a.ui.preset_slot==4 && a.ui.reset_count==before.ui.reset_count);
        assert(!memcmp(a.ui.calibration,before.ui.calibration,sizeof a.ui.calibration));
        for(unsigned i=0;i<8;++i){
            if(i==destination){assert(a.ui.output_reset[i]==1 && !a.voices[i].phase && !a.voices[i].index && !a.voices[i].slew_remaining);assert(a.voices[i].random==0x9e3779b9u*(i+1u));}
            else {
                assert(!memcmp(&a.ui.outputs[i],&before.ui.outputs[i],sizeof(NfConfig)));
                assert(!memcmp(&a.ui.rhythms[i],&before.ui.rhythms[i],sizeof(NfRhythm)));
                assert(!memcmp(&a.ui.grooves[i],&before.ui.grooves[i],sizeof(NfGroove)));
                assert(!memcmp(&a.ui.tones[i],&before.ui.tones[i],sizeof(NfTone)));
                assert(!memcmp(a.ui.routes[i],before.ui.routes[i],sizeof a.ui.routes[i]));
                assert(a.voices[i].phase==before.voices[i].phase && a.voices[i].index==before.voices[i].index && a.voices[i].random==before.voices[i].random && a.voices[i].slew_remaining==before.voices[i].slew_remaining && a.output[i]==before.output[i]);
            }
        }
        /* Copying identical settings must still restart destination state. */
        for(unsigned t=0;t<123;++t)original_app_tick(&a);assert(a.voices[destination].phase);
        assert(original_ui_copy_output(&a.ui,source,destination));original_app_sync(&a);assert(!a.voices[destination].phase);
        /* Copying an empty output also removes the destination's CV routes. */
        assert(original_ui_clear_output(&a.ui,source));assert(original_ui_copy_output(&a.ui,source,destination));original_app_sync(&a);
        for(unsigned t=0;t<1000;++t){original_app_cv(&a,4095,4095,3);original_app_tick(&a);assert(!a.output[source] && !a.output[destination]);}
    }
}
int main(void){gestures();isolation();puts("Output copy: NO/YES/hold, skip self, all 56 pairs, full settings, destination-only restart and empty copy passed.");}
