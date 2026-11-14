/* SPDX-License-Identifier: MIT */
#include "app.h"
#include <assert.h>
#include <string.h>
static void settle(NfCv *cv,unsigned a,unsigned b) {
    for(unsigned i=0;i<160;++i)nf_cv_sample(cv,a,b,3);nf_cv_normalize(cv);
}
int main(void) {
    NfCv cv;nf_cv_init(&cv);settle(&cv,0,2048);
    assert(cv.valid==3 && cv.normalized[0]==0 && cv.normalized[1]==0);
    settle(&cv,4095,4095);assert(cv.normalized[0]==NF_CV_Q && cv.normalized[1]==NF_CV_Q);
    settle(&cv,0,0);assert(cv.normalized[0]==0 && cv.normalized[1]==-NF_CV_Q);
    NfCvCalibration reversed[2]={{4095,0},{2047,0}};
    nf_cv_configure(&cv,false,reversed);settle(&cv,0,0);
    assert(cv.normalized[0]==NF_CV_Q && cv.normalized[1]==NF_CV_Q);
    settle(&cv,4095,4095);assert(cv.normalized[0]==0 && cv.normalized[1]==-NF_CV_Q);
    nf_cv_configure(&cv,true,reversed);settle(&cv,0,4095);
    assert(cv.raw[0]==4095 && cv.raw[1]==0 && cv.normalized[0]==0 && cv.normalized[1]==NF_CV_Q);
    nf_cv_sample(&cv,0,4095,0);nf_cv_normalize(&cv);assert(!cv.valid);
    NfCvPlan plan={0};nf_cv_plan(&plan,(NfCvRoute){1,100,0});
    assert(nf_cv_apply(&cv,&plan,23,50,100,0,false)==23); /* Invalid ADC bypass. */
    reversed[0]=(NfCvCalibration){100,100};nf_cv_configure(&cv,false,reversed);settle(&cv,4095,0);
    assert(!(cv.valid&1));assert(nf_cv_apply(&cv,&plan,23,50,100,0,false)==23);
    nf_cv_init(&cv);settle(&cv,4095,2048);
    assert(nf_cv_apply(&cv,&plan,0,0,100,0,false)==100);
    nf_cv_plan(&plan,(NfCvRoute){1,-100,0});assert(nf_cv_apply(&cv,&plan,100,100,100,0,false)==0);
    nf_cv_plan(&plan,(NfCvRoute){1,50,0});assert(nf_cv_apply(&cv,&plan,0,0,100,0,false)==50);
    settle(&cv,0,2048);nf_cv_plan(&plan,(NfCvRoute){1,100,25});assert(nf_cv_apply(&cv,&plan,0,0,100,0,false)==25);
    nf_cv_plan(&plan,(NfCvRoute){0,100,100});assert(nf_cv_apply(&cv,&plan,23,50,100,0,false)==23);
    nf_cv_plan(&plan,(NfCvRoute){2,100,0});settle(&cv,0,0);assert(nf_cv_apply(&cv,&plan,50,50,99,1,false)==1);
    settle(&cv,0,4095);assert(nf_cv_apply(&cv,&plan,50,50,99,1,false)==99);
    nf_cv_plan(&plan,(NfCvRoute){1,100,0});cv.valid=1;
    cv.normalized[0]=1802;assert(nf_cv_apply(&cv,&plan,5,5,10,0,true)==5); /* 5.55 */
    cv.normalized[0]=2130;assert(nf_cv_apply(&cv,&plan,5,5,10,0,true)==6); /* 5.65 */
    cv.normalized[0]=1802;assert(nf_cv_apply(&cv,&plan,5,6,10,0,true)==6);
    cv.normalized[0]=1146;assert(nf_cv_apply(&cv,&plan,5,6,10,0,true)==5);
    NfVoice voice={.config={5,0,50,0,100,0},.phase=30000};nf_voice_retime(&voice,1);
    assert(voice.phase==240000);nf_voice_retime(&voice,5);assert(voice.phase==30000);
    /* Endpoint, calibration direction, clipping and signed gain fuzz matrix. */
    for(unsigned raw=0;raw<=4095;raw+=17)for(int depth=-100;depth<=100;depth+=25) {
        nf_cv_init(&cv);settle(&cv,raw,4095-raw);nf_cv_plan(&plan,(NfCvRoute){2,(int8_t)depth,(int8_t)(depth/2)});
        for(unsigned base=0;base<=100;base+=10)assert(nf_cv_apply(&cv,&plan,base,base,100,0,false)<=100);
    }
    OriginalApp app;original_app_init(&app,15,0);
    app.ui.outputs[0].level=0;app.ui.routes[0][4]=(NfCvRoute){1,100,0};
    app.ui.running=true;original_app_sync(&app);
    original_app_cv(&app,4095,2048,3);for(unsigned t=0;t<160;++t)original_app_tick(&app);
    assert(app.voices[0].config.level==100 && app.ui.outputs[0].level==0);
    original_app_cv(&app,0,2048,3);for(unsigned t=0;t<160;++t)original_app_tick(&app);
    assert(app.voices[0].config.level==0 && app.ui.outputs[0].level==0);
    for(unsigned i=0;i<8;++i)for(unsigned f=0;f<NF_FIELDS;++f)app.ui.routes[i][f]=(NfCvRoute){2,(int8_t)(f&1?-100:100),0};
    original_app_sync(&app);NfConfig base[8];memcpy(base,app.ui.outputs,sizeof base);
    for(unsigned t=0;t<5000;++t) {
        original_app_cv(&app,t%4096,(t*17u)%4096,3);original_app_tick(&app);
        for(unsigned i=0;i<8;++i) {
            NfConfig c=app.voices[i].config;
            assert(c.ratio<NF_RATIOS && c.shape<NF_SHAPES && c.width>=1 && c.width<=99);
            assert(c.phase<=99 && c.level<=100 && c.offset<=100 && app.output[i]<=1023);
        }
    }
    assert(!memcmp(base,app.ui.outputs,sizeof base));
    app.ui.running=false;original_app_sync(&app);for(unsigned i=0;i<8;++i)assert(!app.output[i]);
    /* CV and external synchronization coexist; UI changes cannot rearm lock. */
    original_app_init(&app,15,0);app.ui.external=true;app.ui.ppqn_index=0;app.ui.running=true;
    app.ui.outputs[0].level=0;app.ui.routes[0][4]=(NfCvRoute){1,100,0};original_app_sync(&app);
    for(unsigned ms=0;ms<=4000;++ms) {
        original_app_cv(&app,ms%4096,2048,3);
        if(ms%500==0)original_app_input(&app,NF_INPUT_CLOCK,ms*1000u);
        original_app_tick_at(&app,ms*1000u);
        if(ms>=500)assert(app.clock.running && app.sync.locked);
        if(ms==2000) {app.ui.routes[0][4].bias=-10;original_app_sync(&app);assert(app.clock.running);}
    }
    assert(app.clock.beats==7 && app.clock.phase==0 && app.ui.outputs[0].level==0);
    return 0;
}
