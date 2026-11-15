/* SPDX-License-Identifier: MIT. BIA controller for Pam Rev62, not BIA firmware. */
#include "bia.h"
#include "../store.h"
#include "pnw_board.h"
static BiaApp app;static NfStore presets;static volatile bool initialized;
void pnw_tick_hook(void) {
    pnw_cv_service();if(!initialized)return;
    bia_cv(&app,pnw_cv_raw(0),pnw_cv_raw(1),pnw_cv_valid());
    uint32_t kind,us;while(pnw_input_take(&kind,&us))bia_input(&app,kind,us);
    bia_tick(&app,pnw_micros());pnw_outputs_request(app.output);
}
int main(void) {
    pnw_board_init();uint32_t saved=pnw_interrupt_lock();bia_init(&app,pnw_controls_raw(),pnw_millis());pnw_interrupt_restore(saved);
    NfStoreIo io={0,pnw_preset_read,pnw_preset_erase,pnw_preset_program};
    if(nf_store_open(&presets,io)==NF_STORE_IO)app.ui.preset_status=NF_PRESET_IO;
    else (void)nf_presets_boot(&presets,&app.ui);if(presets.future)app.ui.preset_status=NF_PRESET_NEWER;
    saved=pnw_interrupt_lock();bia_sync(&app);pnw_inputs_discard();initialized=true;pnw_outputs_control(app.output);pnw_interrupt_restore(saved);
    pnw_gate_service();bia_ui_render(&app.ui);pnw_oled_flush();pnw_controls_discard();
    unsigned page=4;
    for(;;) {
        bool dirty=false;uint8_t controls;uint32_t sampled_at;
        while(pnw_controls_take(&controls,&sampled_at))
            dirty=bia_ui_update(&app.ui,controls,sampled_at)||dirty;
        saved=pnw_interrupt_lock();dirty=bia_refresh(&app)||dirty;pnw_interrupt_restore(saved);
        if((pnw_gate_overruns() || pnw_input_overruns()) && !app.ui.timing_fault){app.ui.timing_fault=true;dirty=true;}
        if(dirty){saved=pnw_interrupt_lock();bia_sync(&app);pnw_outputs_control(app.output);pnw_interrupt_restore(saved);pnw_gate_service();bia_ui_render(&app.ui);page=0;}
        if(app.ui.preset_command) {
            pnw_oled_flush();pnw_gate_service();saved=pnw_interrupt_lock();bool stopped=!app.requested && !app.running && !app.auditioning;pnw_interrupt_restore(saved);
            if(app.ui.preset_command==3) {
                app.ui.preset_command=0;
                if(!stopped)app.ui.preset_status=NF_PRESET_STOP;
                else {bool ok=pnw_preset_erase(0,0) && pnw_preset_erase(0,1);(void)nf_store_open(&presets,io);app.ui.preset_mask=presets.mask;app.ui.preset_status=ok?NF_PRESET_EMPTY:NF_PRESET_IO;}
            } else (void)nf_presets_execute(&presets,&app.ui,stopped);
            saved=pnw_interrupt_lock();bia_sync(&app);if(stopped){nf_sync_init(&app.sync,nf_ppqn_values[app.patch.ppqn_index]);app.reset_input=false;pnw_inputs_discard();}
            pnw_outputs_control(app.output);pnw_interrupt_restore(saved);pnw_gate_service();bia_ui_render(&app.ui);page=0;
            pnw_controls_discard();
        }
        if(page<4)pnw_oled_flush_page(page++);
    }
}
