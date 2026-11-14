/* SPDX-License-Identifier: MIT */
#include "app.h"
static void reset_voices(OriginalApp *a) {for(unsigned i=0;i<8;++i)nf_voice_reset(&a->voices[i],i);}
static void values(OriginalApp *a,bool tick) {
    unsigned step=a->step?a->step:a->clock.bpm;
    for(unsigned i=0;i<8;++i)a->output[i]=nf_voice_output(&a->voices[i],a->clock.running,step,tick);
}
static void cv_values(OriginalApp *a,unsigned manual_resets) {
    static const unsigned minimum[NF_FIELDS]={0,0,1,0,0,0};
    static const unsigned maximum[NF_FIELDS]={NF_RATIOS-1,NF_SHAPES-1,99,99,100,100};
    for(unsigned i=0;i<8;++i) {
        const NfConfig *b=&a->base[i],*old=&a->voices[i].config;
        unsigned base[NF_FIELDS]={b->ratio,b->shape,b->width,b->phase,b->level,b->offset};
        unsigned current[NF_FIELDS]={old->ratio,old->shape,old->width,old->phase,old->level,old->offset};
        uint8_t value[NF_FIELDS];
        for(unsigned field=0;field<NF_FIELDS;++field)
            value[field]=(uint8_t)nf_cv_apply(&a->cv,&a->cv_plans[i][field],base[field],current[field],maximum[field],minimum[field],field<2);
        if(manual_resets&(1u<<i))nf_voice_reset(&a->voices[i],i);
        else nf_voice_retime(&a->voices[i],value[0]);
        a->voices[i].config=(NfConfig){value[0],value[1],value[2],value[3],value[4],value[5]};
    }
}
void original_app_cv(OriginalApp *a,unsigned adc0,unsigned adc1,unsigned valid) {
    a->cv_adc[0]=(uint16_t)(adc0>4095?4095:adc0);a->cv_adc[1]=(uint16_t)(adc1>4095?4095:adc1);
    a->cv_adc_valid=(uint8_t)(valid&3u);
}
static bool enabled(const OriginalApp *a) {return a->requested && (a->run_mode!=NF_RUN_GATE || a->run_high);}
static void transport(OriginalApp *a) {
    bool run=enabled(a);nf_sync_enable(&a->sync,run);
    if(a->external)run=run && a->sync.active;
    if(a->clock.running!=run) {original_clock_running(&a->clock,run);reset_voices(a);}
}
bool original_app_refresh_ui(OriginalApp *a) {
    unsigned status=0,bpm=0;
    if(a->external) {status=a->sync.locked?2u:a->sync.lost?3u:1u;bpm=nf_sync_bpm(&a->sync);}
    if(a->requested && a->run_mode==NF_RUN_GATE && !a->run_high)status=4;
    else if(a->external && a->requested && !a->sync.active && !a->sync.lost)status=5;
    bool dirty=a->ui.active!=a->clock.running || a->ui.sync_status!=status || a->ui.external_bpm!=bpm;
    a->ui.active=a->clock.running;a->ui.run_high=a->run_high;a->ui.sync_status=(uint8_t)status;a->ui.external_bpm=bpm;
    if((uint32_t)(a->now_us-a->cv_display_us)>=50000u) {
        a->cv_display_us=a->now_us;
        unsigned field=a->ui.cv_menu?a->ui.cv_target:a->ui.selecting?a->ui.field:0;
        if(field>=NF_FIELDS)field=0;
        if(a->ui.cv_monitor) {
            unsigned input=a->ui.cv_input,bit=1u<<input;
            if((a->ui.cv_raw_valid&bit)!=(a->cv.raw_valid&bit) || (a->ui.cv_valid&bit)!=(a->cv.valid&bit) ||
               a->ui.cv_raw[input]!=a->cv.raw[input] || a->ui.cv_normalized[input]!=a->cv.normalized[input])dirty=true;
        }
        bool value_visible=!a->ui.cv_monitor && !a->ui.editing &&
            (a->ui.cv_menu?a->ui.cv_field==4:!a->ui.selecting || a->ui.field<NF_FIELDS);
        if(a->ui.page && value_visible) {
            const uint8_t *previous=(const uint8_t *)&a->ui.effective[a->ui.page-1];
            const uint8_t *current=(const uint8_t *)&a->voices[a->ui.page-1].config;
            if(previous[field]!=current[field])dirty=true;
        }
        for(unsigned i=0;i<2;++i) {a->ui.cv_raw[i]=a->cv.raw[i];a->ui.cv_normalized[i]=a->cv.normalized[i];}
        a->ui.cv_raw_valid=a->cv.raw_valid;a->ui.cv_valid=a->cv.valid;
        for(unsigned i=0;i<8;++i)a->ui.effective[i]=a->voices[i].config;
    }
    return dirty;
}
void original_app_init(OriginalApp *a,uint8_t raw,uint32_t now) {
    *a=(OriginalApp){0};original_ui_init(&a->ui,raw,now);original_clock_init(&a->clock,a->ui.bpm);
    a->ppqn=nf_ppqn_values[a->ui.ppqn_index];nf_sync_init(&a->sync,a->ppqn);a->now_us=now*1000u;
    nf_cv_init(&a->cv);
    for(unsigned i=0;i<8;++i) {
        a->base[i]=a->ui.outputs[i];a->voices[i].config=a->base[i];
        for(unsigned f=0;f<NF_FIELDS;++f)a->cv_plans[i][f]=(NfCvPlan){.config=a->ui.routes[i][f],.gain=NF_CV_Q};
    }
    reset_voices(a);values(a,false);(void)original_app_refresh_ui(a);
}
void original_app_sync(OriginalApp *a) {
    unsigned ppqn=nf_ppqn_values[a->ui.ppqn_index];
    bool source_changed=a->external!=a->ui.external || (a->ui.external && a->ppqn!=ppqn);
    bool reset=a->reset_seen!=a->ui.reset_count;
    a->requested=a->ui.running;a->external=a->ui.external;a->ppqn=ppqn;a->run_mode=a->ui.run_mode;
    if(source_changed) {nf_sync_init(&a->sync,ppqn);a->sync.last_tick_us=a->now_us;original_clock_running(&a->clock,false);reset_voices(a);}
    if(!a->external) {original_clock_tempo(&a->clock,a->ui.bpm);a->step=a->clock.bpm;}
    if(reset) {
        a->reset_seen=a->ui.reset_count;original_clock_reset(&a->clock);reset_voices(a);
        if(a->external)nf_sync_rearm(&a->sync);
    }
    unsigned manual_resets=0;
    for(unsigned i=0;i<8;++i) {
        if(a->base[i].ratio!=a->ui.outputs[i].ratio && !a->ui.routes[i][0].source)manual_resets|=1u<<i;
        a->base[i]=a->ui.outputs[i];nf_voice_rhythm(&a->voices[i],a->ui.rhythms[i]);nf_voice_groove(&a->voices[i],a->ui.grooves[i]);nf_voice_tone(&a->voices[i],a->ui.tones[i]);
        for(unsigned field=0;field<NF_FIELDS;++field)nf_cv_plan(&a->cv_plans[i][field],a->ui.routes[i][field]);
    }
    nf_cv_configure(&a->cv,a->ui.cv_swapped,a->ui.calibration);nf_cv_normalize(&a->cv);cv_values(a,manual_resets);
    for(unsigned i=0;i<8;++i)a->ui.effective[i]=a->voices[i].config;
    transport(a);values(a,false);(void)original_app_refresh_ui(a);
}
void original_app_input(OriginalApp *a,unsigned kind,uint32_t us) {
    if(kind==NF_INPUT_CLOCK) {
        if(a->external) {
            /* A Run reset and clock arriving in the same input batch must
               restart on that edge, rather than dropping it at tick time. */
            if(a->reset_input) {a->reset_input=false;original_clock_reset(&a->clock);reset_voices(a);nf_sync_rearm(&a->sync);}
            nf_sync_edge(&a->sync,us);
        }
    }
    else if(kind==NF_INPUT_RUN_HIGH || kind==NF_INPUT_RUN_LOW) {
        bool high=kind==NF_INPUT_RUN_HIGH;
        if(high && !a->run_high && a->run_mode==NF_RUN_RESET)a->reset_input=true;
        a->run_high=high;if(a->external)nf_sync_enable(&a->sync,enabled(a));
    }
}
void original_app_tick_at(OriginalApp *a,uint32_t now_us) {
    a->now_us=now_us;
    nf_cv_sample(&a->cv,a->cv_adc[0],a->cv_adc[1],a->cv_adc_valid);
    a->cv_divider=(uint8_t)((a->cv_divider+1u)&3u);
    if(!a->cv_divider) {nf_cv_normalize(&a->cv);cv_values(a,0);}
    if(a->reset_input) {
        a->reset_input=false;original_clock_reset(&a->clock);reset_voices(a);
        if(a->external)nf_sync_rearm(&a->sync);
    }
    nf_sync_enable(&a->sync,enabled(a));
    unsigned step=a->external?nf_sync_tick(&a->sync,now_us):a->clock.bpm;
    transport(a);
    if(a->external && a->sync.restart) {
        a->sync.restart=false;original_clock_reset(&a->clock);reset_voices(a);
    }
    if(a->external && a->sync.locked)a->clock.bpm=nf_sync_bpm(&a->sync);
    a->step=step;original_clock_advance(&a->clock,step);
    if(a->clock.running)for(unsigned i=0;i<8;++i)nf_voice_advance(&a->voices[i],step);
    values(a,true);
}
void original_app_tick(OriginalApp *a) {original_app_tick_at(a,a->now_us+1000u);}
