/* SPDX-License-Identifier: MIT
 * Eight-output v0.7 application. The interrupt owns clock advancement; the
 * foreground owns UI/rendering. No EEPROM operations; explicit stopped-only presets use reserved internal flash. External clock/Run edges are captured by EXTI.
 */
#include "pnw_board.h"
#include "app.h"
#include "store.h"
#ifdef PNW_INPUT_DIAGNOSTICS
#include "font.h"
#endif
static OriginalApp app;
static NfStore presets;
static volatile bool initialized;
static void publish_gate(void) {
    pnw_outputs_request(app.output);
}
void pnw_tick_hook(void) {
    pnw_cv_service();
    if(!initialized)return;
    original_app_cv(&app,pnw_cv_raw(0),pnw_cv_raw(1),pnw_cv_valid());
    uint32_t kind,us;
    while(pnw_input_take(&kind,&us))original_app_input(&app,kind,us);
    original_app_tick_at(&app,pnw_micros());publish_gate();
}
int main(void) {
    pnw_board_init();
#ifdef PNW_INPUT_DIAGNOSTICS
    /* Read pins directly, bypassing UI debounce and transport entirely. */
    unsigned clocks=0;
    for(;;) {
        uint32_t kind,us;while(pnw_input_take(&kind,&us))if(kind==1u)++clocks;
        uint8_t raw=pnw_controls_raw()^12u; /* physical button levels */
        char counter[]="CLOCK COUNT 0";counter[12]=(char)('0'+clocks%10u);
        pnw_oled_clear();
        original_text(0,0,(raw&8u)?"START PB4: HIGH":"START PB4: LOW",1);
        original_text(0,8,(raw&4u)?"ENC PB5: HIGH":"ENC PB5: LOW",1);
        original_text(0,16,counter,1);
        original_text(0,24,pnw_run_input()?"RUN JACK: HIGH":"RUN JACK: LOW",1);
        pnw_oled_flush();pnw_delay(10);
    }
#endif
    uint32_t saved=pnw_interrupt_lock();
    original_app_init(&app,pnw_controls_raw(),pnw_millis());pnw_interrupt_restore(saved);
    NfStoreIo io={0,pnw_preset_read,pnw_preset_erase,pnw_preset_program};
    (void)nf_store_open(&presets,io);(void)nf_presets_boot(&presets,&app.ui);
    saved=pnw_interrupt_lock();original_app_sync(&app);pnw_inputs_discard();initialized=true;
    pnw_outputs_request(app.output);pnw_interrupt_restore(saved);pnw_gate_service();
    original_ui_render(&app.ui);pnw_oled_flush();
    unsigned page=4;
    for(;;) {
        bool dirty=original_ui_update(&app.ui,pnw_controls_raw(),pnw_millis());
        saved=pnw_interrupt_lock();
        dirty=original_app_refresh_ui(&app)||dirty;pnw_interrupt_restore(saved);
        if((pnw_gate_overruns() || pnw_input_overruns()) && !app.ui.timing_fault) {app.ui.timing_fault=true;dirty=true;}
        if(dirty) {
            saved=pnw_interrupt_lock();original_app_sync(&app);pnw_outputs_control(app.output);pnw_interrupt_restore(saved);
            pnw_gate_service();original_ui_render(&app.ui);page=0;
        }
        if(app.ui.preset_command) {
            pnw_oled_flush();pnw_gate_service();
            saved=pnw_interrupt_lock();bool disarmed=!app.requested && !app.clock.running;pnw_interrupt_restore(saved);
            (void)nf_presets_execute(&presets,&app.ui,disarmed);
            saved=pnw_interrupt_lock();original_app_sync(&app);
            if(disarmed) {nf_sync_init(&app.sync,app.ppqn);app.reset_input=false;pnw_inputs_discard();}
            pnw_outputs_control(app.output);pnw_interrupt_restore(saved);pnw_gate_service();
            original_ui_render(&app.ui);page=0;
        }
        if(page<4)pnw_oled_flush_page(page++);
    }
}
