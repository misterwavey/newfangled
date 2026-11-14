/* SPDX-License-Identifier: MIT
 * Application-level simulator board: the actual original UI/font, no MCU
 * emulator, recovered source, peripheral registers or vendor data.
 */
#include <stdint.h>
#include <stddef.h>
#include "pnw_board.h"
#include "app.h"
static uint32_t now;
static OriginalApp app;
#define ui (app.ui)
#define EDGE_CAPACITY 2048u
static uint32_t edge_times[EDGE_CAPACITY],edge_levels[EDGE_CAPACITY],edge_count;
static uint16_t last_output;
static void capture_gate(void) {
    uint16_t value=app.output[0];
    if(value!=last_output) {
        unsigned slot=edge_count%EDGE_CAPACITY;edge_times[slot]=now;edge_levels[slot]=value;++edge_count;last_output=value;
    }
}
static uint8_t framebuffer[512],raw;
static uint32_t cv[2],run,clock_enabled,clock_bpm,clock_ppqn,clock_phase;
void *memset(void *destination,int value,size_t size) {
    unsigned char *p=destination;while(size--)*p++=(unsigned char)value;return destination;
}
void *memcpy(void *destination,const void *source,size_t size) {
    unsigned char *d=destination;const unsigned char *s=source;while(size--)*d++=*s++;return destination;
}
void pnw_oled_clear(void) {for(unsigned i=0;i<512;++i)framebuffer[i]=0;}
void pnw_oled_pixel(unsigned x,unsigned y,bool on) {
    if(x>=128 || y>=32)return;
    uint8_t mask=(uint8_t)(1u<<(y&7u));unsigned index=(y>>3)*128+x;
    if(on)framebuffer[index]|=mask;else framebuffer[index]&=(uint8_t)~mask;
}
void sim_reset(void) {
    raw=15;now=0;cv[0]=0;cv[1]=2048;run=0;clock_enabled=0;clock_bpm=120;clock_ppqn=24;clock_phase=0;
    edge_count=0;last_output=0;original_app_init(&app,raw,now);original_ui_render(&ui);
}
void sim_advance(uint32_t milliseconds) {
    for(uint32_t i=0;i<milliseconds;++i) {
        ++now;
        if(clock_enabled)clock_phase=(clock_phase+clock_bpm*clock_ppqn)%60000u;
        original_app_tick(&app);capture_gate();
        if(original_ui_update(&ui,raw,now)) {original_app_sync(&app);capture_gate();original_ui_render(&ui);}
    }
}
void sim_raw(uint32_t value) {raw=(uint8_t)(value&15u);}
void sim_turn(uint32_t clockwise) {
    /* Same electrical transition sequence as the host UI regression fixture. */
    static const uint8_t forward[]={2,0,1,3},reverse[]={1,0,2,3};
    const uint8_t *sequence=clockwise?forward:reverse;
    for(unsigned i=0;i<4;++i) {raw=(uint8_t)((raw&12u)|sequence[i]);sim_advance(1);}
}
void sim_click(uint32_t program) {
    uint8_t mask=(uint8_t)(program?4u:8u);
    raw&=(uint8_t)~mask;sim_advance(25);raw|=mask;sim_advance(25);
}
void sim_hold_program(void) {
    raw&=(uint8_t)~4u;sim_advance(550);raw|=4u;sim_advance(25);
}
void sim_reset_phase(void) {
    raw&=(uint8_t)~4u;sim_advance(25);raw&=(uint8_t)~8u;sim_advance(25);raw|=12u;sim_advance(25);
}
void sim_input(uint32_t index,uint32_t value) {
    if(index<2u)cv[index]=value>4095u?4095u:value;
    else if(index==2u)run=value!=0;
    else if(index==3u) {clock_enabled=value!=0;if(!clock_enabled)clock_phase=0;}
    else if(index==4u)clock_bpm=value<10u?10u:value>330u?330u:value;
    else if(index==5u)clock_ppqn=value<1u?1u:value>48u?48u:value;
}
uint32_t sim_get(uint32_t index) {
    switch(index) {
    case 0:return now;case 1:return ui.bpm;case 2:return ui.page;case 3:return ui.editing;
    case 4:return cv[0];case 5:return cv[1];case 6:return run;
    case 7:return clock_enabled && clock_phase<15000u;
    case 8:return clock_enabled;case 9:return clock_bpm;case 10:return clock_ppqn;
    case 11:return raw;case 12:return app.clock.running;case 13:return app.clock.phase;
    case 14:return app.clock.beats;case 15:return ui.reset_count;case 16:return edge_count;case 17:return ui.selecting;case 18:return ui.field;default:return 0;
    }
}
uintptr_t sim_framebuffer(void) {return (uintptr_t)framebuffer;}
uint32_t sim_output(uint32_t index) {return index<8?app.output[index]:0u;}
uint32_t sim_edge_count(void) {return edge_count;}
uint32_t sim_edge_time(uint32_t index) {return index>=edge_count || edge_count-index>EDGE_CAPACITY?UINT32_MAX:edge_times[index%EDGE_CAPACITY];}
uint32_t sim_edge_level(uint32_t index) {return index>=edge_count || edge_count-index>EDGE_CAPACITY?UINT32_MAX:edge_levels[index%EDGE_CAPACITY];}
