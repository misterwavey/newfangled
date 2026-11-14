/* Per-output clear must be silent, isolated and explicit, including while running. */
#include "app.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t now;
static void click(OriginalUi *u) {
    now+=30;(void)original_ui_update(u,11,now);now+=21;(void)original_ui_update(u,11,now);
    now+=10;(void)original_ui_update(u,15,now);now+=21;(void)original_ui_update(u,15,now);
}
static void turn(OriginalUi *u,bool forward) {
    const uint8_t cw[]={2,0,1,3},ccw[]={1,0,2,3};const uint8_t *pins=forward?cw:ccw;
    for(unsigned i=0;i<4;++i)(void)original_ui_update(u,(uint8_t)(12u|pins[i]),++now);
}
static void hold(OriginalUi *u) {
    now+=30;(void)original_ui_update(u,11,now);now+=21;(void)original_ui_update(u,11,now);now+=550;(void)original_ui_update(u,11,now);
    now+=10;(void)original_ui_update(u,15,now);now+=21;(void)original_ui_update(u,15,now);
}
static void gestures(void) {
    OriginalUi u;now=0;original_ui_init(&u,15,0);u.page=3;u.outputs[2].level=73;
    click(&u);assert(u.selecting && u.field==0);turn(&u,false);assert(u.field==NF_CLEAR_FIELD);
    click(&u);assert(u.clear_confirm && !u.clear_yes && !u.editing);original_ui_render(&u);
    click(&u);assert(!u.clear_confirm && u.outputs[2].level==73 && !u.output_reset[2]);
    click(&u);turn(&u,true);assert(u.clear_yes);hold(&u);assert(!u.clear_confirm && !u.selecting && u.outputs[2].level==73);
    click(&u);turn(&u,false);click(&u);turn(&u,true);turn(&u,false);assert(!u.clear_yes);click(&u);assert(u.outputs[2].level==73);
    click(&u);turn(&u,true);original_ui_render(&u);click(&u);
    assert(!u.clear_confirm && !u.selecting && !u.editing && u.page==3);assert(u.outputs[2].level==0 && u.output_reset[2]==1);
}
static void isolation(void) {
    for(unsigned selected=0;selected<8;++selected) {
        OriginalApp a;original_app_init(&a,15,0);a.ui.bpm=137;a.ui.running=true;
        a.ui.calibration[0]=(NfCvCalibration){83,3900};a.ui.calibration[1]=(NfCvCalibration){2010,120};a.ui.cv_swapped=true;
        a.ui.preset_mask=0xa5;a.ui.preset_slot=4;
        for(unsigned i=0;i<8;++i) {
            a.ui.outputs[i]=(NfConfig){6,4,23,12,90,7};a.ui.rhythms[i]=(NfRhythm){13,11,1,87,1};a.ui.grooves[i]=(NfGroove){33,4};a.ui.tones[i]=(NfTone){2,3,3,20};
            for(unsigned f=0;f<NF_FIELDS;++f)a.ui.routes[i][f]=(NfCvRoute){2,-73,21};
        }
        original_app_sync(&a);for(unsigned t=0;t<777;++t)original_app_tick(&a);
        OriginalApp before=a;assert(original_ui_clear_output(&a.ui,selected));original_app_sync(&a);
        assert(a.ui.running && a.requested && a.clock.running);assert(a.clock.phase==before.clock.phase && a.clock.beats==before.clock.beats);
        assert(a.ui.bpm==137 && a.ui.cv_swapped && a.ui.preset_mask==0xa5 && a.ui.preset_slot==4 && a.ui.reset_count==before.ui.reset_count);
        assert(!memcmp(a.ui.calibration,before.ui.calibration,sizeof a.ui.calibration));
        for(unsigned i=0;i<8;++i) {
            if(i==selected) {
                assert(a.ui.outputs[i].ratio==5 && a.ui.outputs[i].shape==0 && a.ui.outputs[i].width==50 && a.ui.outputs[i].phase==0 && !a.ui.outputs[i].level && !a.ui.outputs[i].offset);
                assert(a.ui.rhythms[i].steps==1 && a.ui.rhythms[i].hits==1 && a.ui.rhythms[i].probability==100 && !a.ui.rhythms[i].rotate && !a.ui.rhythms[i].loop);
                assert(!a.ui.grooves[i].swing && a.ui.grooves[i].ratchet==1 && !a.ui.tones[i].scale && !a.ui.tones[i].root && a.ui.tones[i].range==1 && !a.ui.tones[i].slew);
                assert(!a.output[i] && !a.voices[i].phase && !a.voices[i].index && !a.voices[i].slew_remaining);
                for(unsigned f=0;f<NF_FIELDS;++f)assert(!a.ui.routes[i][f].source && a.ui.routes[i][f].depth==100 && !a.ui.routes[i][f].bias);
            } else {
                assert(!memcmp(&a.ui.outputs[i],&before.ui.outputs[i],sizeof(NfConfig)));
                assert(!memcmp(&a.ui.rhythms[i],&before.ui.rhythms[i],sizeof(NfRhythm)));
                assert(!memcmp(a.ui.routes[i],before.ui.routes[i],sizeof a.ui.routes[i]));
                assert(a.voices[i].phase==before.voices[i].phase && a.voices[i].random==before.voices[i].random && a.voices[i].index==before.voices[i].index && a.output[i]==before.output[i]);
            }
        }
        for(unsigned t=0;t<2000;++t){original_app_cv(&a,4095,4095,3);original_app_tick(&a);assert(!a.output[selected]);}
        /* An already-clear output still resets its own runtime on another clear. */
        assert(a.voices[selected].phase);assert(original_ui_clear_output(&a.ui,selected));original_app_sync(&a);assert(!a.voices[selected].phase);
        OriginalUi unchanged=a.ui;assert(!original_ui_clear_output(&a.ui,8));assert(!memcmp(&unchanged,&a.ui,sizeof unchanged));
        a.ui.outputs[selected].level=100;original_app_sync(&a);assert(a.output[selected]>0);
    }
}
int main(void){gestures();isolation();puts("Output clear: NO/YES/hold, all eight outputs, running isolation, silence under CV and re-enable passed.");}
