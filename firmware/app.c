/* SPDX-License-Identifier: MIT */
#include "app.h"
static void reset_voices(OriginalApp *a) {for(unsigned i=0;i<8;++i)nf_voice_reset(&a->voices[i],i);}
static void values(OriginalApp *a) {
    unsigned step=a->step?a->step:a->clock.bpm;
    for(unsigned i=0;i<8;++i)a->output[i]=nf_voice_value(&a->voices[i],a->clock.running,step);
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
    return dirty;
}
void original_app_init(OriginalApp *a,uint8_t raw,uint32_t now) {
    *a=(OriginalApp){0};original_ui_init(&a->ui,raw,now);original_clock_init(&a->clock,a->ui.bpm);
    a->ppqn=nf_ppqn_values[a->ui.ppqn_index];nf_sync_init(&a->sync,a->ppqn);a->now_us=now*1000u;
    for(unsigned i=0;i<8;++i)a->voices[i].config=a->ui.outputs[i];
    reset_voices(a);values(a);(void)original_app_refresh_ui(a);
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
    for(unsigned i=0;i<8;++i) {
        bool ratio_changed=a->voices[i].config.ratio!=a->ui.outputs[i].ratio;
        a->voices[i].config=a->ui.outputs[i];
        if(ratio_changed)nf_voice_reset(&a->voices[i],i);
    }
    transport(a);values(a);(void)original_app_refresh_ui(a);
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
    values(a);
}
void original_app_tick(OriginalApp *a) {original_app_tick_at(a,a->now_us+1000u);}
