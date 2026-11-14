/* SPDX-License-Identifier: MIT
 * Eight-output v0.1 application. The interrupt owns clock advancement; the
 * foreground owns UI/rendering. No EEPROM operations or external sync yet.
 */
#include "pnw_board.h"
#include "app.h"
#ifdef PNW_INPUT_DIAGNOSTICS
#include "font.h"
#endif
static OriginalApp app;
static volatile bool initialized;
static void publish_gate(void) {
    pnw_outputs_request(app.output);
}
void pnw_tick_hook(void) {
    if(!initialized)return;
    original_app_tick(&app);publish_gate();
}
int main(void) {
    pnw_board_init();
#ifdef PNW_INPUT_DIAGNOSTICS
    /* Read pins directly, bypassing UI debounce and transport entirely. */
    for(;;) {
        uint8_t raw=pnw_controls_raw()^12u; /* physical button levels */
        char bits[]="RAW 0000  T0";
        for(unsigned i=0;i<4;++i)bits[4+i]=(char)('0'+((raw>>(3-i))&1u));
        bits[11]=(char)('0'+((pnw_millis()/250u)%10u));
        pnw_oled_clear();
        original_text(0,0,"INPUT CHECK",1);
        original_text(0,8,(raw&8u)?"START PB4: HIGH":"START PB4: LOW",1);
        original_text(0,16,(raw&4u)?"ENC PB5: HIGH":"ENC PB5: LOW",1);
        original_text(0,24,bits,1);pnw_oled_flush();pnw_delay(10);
    }
#endif
    uint32_t saved=pnw_interrupt_lock();
    original_app_init(&app,pnw_controls_raw(),pnw_millis());initialized=true;
    pnw_outputs_request(app.output);pnw_interrupt_restore(saved);pnw_gate_service();
    original_ui_render(&app.ui);pnw_oled_flush();
    unsigned page=4;
    for(;;) {
        bool dirty=original_ui_update(&app.ui,pnw_controls_raw(),pnw_millis());
        if(pnw_gate_overruns() && !app.ui.timing_fault) {app.ui.timing_fault=true;dirty=true;}
        if(dirty) {
            saved=pnw_interrupt_lock();original_app_sync(&app);pnw_outputs_control(app.output);pnw_interrupt_restore(saved);
            pnw_gate_service();original_ui_render(&app.ui);page=0;
        }
        if(page<4)pnw_oled_flush_page(page++);
    }
}
