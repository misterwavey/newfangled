/* SPDX-License-Identifier: MIT */
#include "app.h"
static void values(OriginalApp *a) {for(unsigned i=0;i<8;++i)a->output[i]=nf_voice_value(&a->voices[i],a->clock.running,a->clock.bpm);}
void original_app_init(OriginalApp *a,uint8_t raw,uint32_t now) {
    original_ui_init(&a->ui,raw,now);original_clock_init(&a->clock,a->ui.bpm);a->reset_seen=0;
    for(unsigned i=0;i<8;++i) {a->voices[i].config=a->ui.outputs[i];nf_voice_reset(&a->voices[i],i);}
    values(a);
}
void original_app_sync(OriginalApp *a) {
    bool reset=a->clock.running!=a->ui.running || a->reset_seen!=a->ui.reset_count;
    original_clock_tempo(&a->clock,a->ui.bpm);original_clock_running(&a->clock,a->ui.running);
    if(a->reset_seen!=a->ui.reset_count) {original_clock_reset(&a->clock);a->reset_seen=a->ui.reset_count;}
    for(unsigned i=0;i<8;++i) {
        bool ratio_changed=a->voices[i].config.ratio!=a->ui.outputs[i].ratio;
        a->voices[i].config=a->ui.outputs[i];
        if(reset||ratio_changed)nf_voice_reset(&a->voices[i],i);
    }
    values(a);
}
void original_app_tick(OriginalApp *a) {
    original_clock_tick(&a->clock);
    if(a->clock.running)for(unsigned i=0;i<8;++i)nf_voice_tick(&a->voices[i],a->clock.bpm);
    values(a);
}
