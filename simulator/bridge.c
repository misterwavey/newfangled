/* SPDX-License-Identifier: MIT
 * Application-level simulator board: the actual original UI/font, no MCU
 * emulator, recovered source, peripheral registers or vendor data.
 */
#include <stdint.h>
#include <stddef.h>
#include "pnw_board.h"
#include "app.h"
#include "store.h"
void *memcpy(void *,const void *,size_t);
void *memset(void *,int,size_t);
static NfStore presets;
static uint8_t media[NF_STORE_BYTES],settings_buffer[NF_SETTINGS_BYTES];
static bool media_read(void *c,unsigned at,void *p,unsigned n) {(void)c;if(at>sizeof media || n>sizeof media-at)return false;memcpy(p,media+at,n);return true;}
static bool media_erase(void *c,unsigned bank) {(void)c;if(bank>1)return false;memset(media+bank*NF_BANK_BYTES,255,NF_BANK_BYTES);return true;}
static bool media_program(void *c,unsigned at,const void *p,unsigned n) {(void)c;if(at>sizeof media || n>sizeof media-at)return false;const uint8_t *b=p;for(unsigned i=0;i<n;++i){if((media[at+i]&b[i])!=b[i])return false;media[at+i]=b[i];}return true;}
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
static uint32_t generated_interval;
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
void sim_reboot(void) {
    raw=15;now=0;cv[0]=0;cv[1]=2048;run=0;clock_enabled=0;clock_bpm=120;clock_ppqn=24;clock_phase=0;
    generated_interval=0;edge_count=0;last_output=0;original_app_init(&app,raw,now);NfStoreIo io={0,media_read,media_erase,media_program};(void)nf_store_open(&presets,io);(void)nf_presets_boot(&presets,&ui);original_app_sync(&app);original_ui_render(&ui);
}
void sim_reset(void) {memset(media,255,sizeof media);sim_reboot();}
void sim_advance(uint32_t milliseconds) {
    for(uint32_t i=0;i<milliseconds;++i) {
        ++now;
        if(clock_enabled) {
            uint32_t prior=clock_phase;clock_phase=(clock_phase+clock_bpm*clock_ppqn)%60000u;
            if(clock_phase<prior || !generated_interval) {
                /* Within-tick interpolation gives the generated edge a us
                   timestamp, while synthesis still advances at 1 kHz. */
                uint32_t us=now*1000u;
                if(generated_interval)us-=clock_phase*1000u/(clock_bpm*clock_ppqn);else us=(now-1u)*1000u;
                original_app_input(&app,NF_INPUT_CLOCK,us);generated_interval=1;
            }
        }
        original_app_cv(&app,cv[0],cv[1],3);original_app_tick_at(&app,now*1000u);capture_gate();
        bool dirty=original_ui_update(&ui,raw,now);
        if(dirty) {original_app_sync(&app);capture_gate();}
        if(ui.preset_command) {
            bool disarmed=!app.requested && !app.clock.running;
            (void)nf_presets_execute(&presets,&ui,disarmed);original_app_sync(&app);
            if(disarmed) {nf_sync_init(&app.sync,app.ppqn);app.reset_input=false;if(run)original_app_input(&app,NF_INPUT_RUN_HIGH,now*1000u);}
            dirty=true;
        }
        dirty=original_app_refresh_ui(&app)||dirty;if(dirty)original_ui_render(&ui);
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
    else if(index==2u) {
        unsigned high=value!=0;if(high!=run)original_app_input(&app,high?NF_INPUT_RUN_HIGH:NF_INPUT_RUN_LOW,now*1000u);
        run=high;
    }
    else if(index==3u) {clock_enabled=value!=0;if(!clock_enabled) {clock_phase=0;generated_interval=0;}}
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
    case 14:return app.clock.beats;case 15:return ui.reset_count;case 16:return edge_count;case 17:return ui.selecting;case 18:return ui.field;case 19:return ui.external;case 20:return nf_ppqn_values[ui.ppqn_index];
    case 21:return ui.run_mode;case 22:return ui.sync_status;case 23:return ui.external_bpm;case 24:return app.sync.rejected;case 25:return ui.running;case 26:return ui.cv_menu;case 27:return ui.cv_monitor;
    case 28:return ui.cv_field;case 29:return ui.cv_target;case 30:return ui.cv_input;
    case 31:return (uint32_t)ui.cv_normalized[0];case 32:return (uint32_t)ui.cv_normalized[1];
    case 33:return ui.cv_valid;case 34:return ui.cv_swapped;case 35:return ui.preset_menu;case 36:return ui.preset_slot;
    case 37:return ui.preset_field;case 38:return ui.preset_status;case 39:return presets.mask;
    case 40:return presets.last_slot;case 41:return presets.sequence;case 42:return ui.clear_confirm;case 43:return ui.clear_yes;case 44:return ui.copy_mode;case 45:return ui.copy_target;case 46:return ui.copy_yes;default:return 0;
    }
}
uintptr_t sim_framebuffer(void) {return (uintptr_t)framebuffer;}
uint32_t sim_output(uint32_t index) {return index<8?app.output[index]:0u;}
uint32_t sim_edge_count(void) {return edge_count;}
uint32_t sim_edge_time(uint32_t index) {return index>=edge_count || edge_count-index>EDGE_CAPACITY?UINT32_MAX:edge_times[index%EDGE_CAPACITY];}
uint32_t sim_edge_level(uint32_t index) {return index>=edge_count || edge_count-index>EDGE_CAPACITY?UINT32_MAX:edge_levels[index%EDGE_CAPACITY];}

uint32_t sim_cv_route(uint32_t channel,uint32_t field,uint32_t item) {
    if(channel>=8 || field>=NF_FIELDS)return 0;
    NfCvRoute route=ui.routes[channel][field];
    return item==0?route.source:item==1?(uint32_t)(int32_t)route.depth:item==2?(uint32_t)(int32_t)route.bias:0;
}
uint32_t sim_effective(uint32_t channel,uint32_t field) {
    if(channel>=8 || field>=NF_FIELDS)return 0;
    const NfConfig *c=&app.voices[channel].config;
    unsigned values[]={c->ratio,c->shape,c->width,c->phase,c->level,c->offset};return values[field];
}

uintptr_t sim_settings_buffer(void) {return (uintptr_t)settings_buffer;}
uint32_t sim_preset_read(uint32_t slot) {
    NfSettings settings;NfStoreResult r=nf_store_load(&presets,slot,&settings);
    return r==NF_STORE_OK && nf_settings_encode(&settings,settings_buffer)?0u:(uint32_t)(r?r:NF_STORE_INVALID);
}
uint32_t sim_preset_import(uint32_t slot) {
    if(ui.running || ui.active || app.requested || app.clock.running)return NF_STORE_INVALID;
    NfSettings settings;if(!nf_settings_decode(&settings,settings_buffer))return NF_STORE_INVALID;
    NfStoreResult r=nf_store_save(&presets,slot,&settings);ui.preset_mask=presets.mask;return r;
}

uint32_t sim_rhythm(uint32_t channel,uint32_t item) {
    if(channel>=8)return 0;NfRhythm r=ui.rhythms[channel];
    unsigned values[]={r.steps,r.hits,r.rotate,r.probability,r.loop,app.voices[channel].index,app.voices[channel].allowed,app.voices[channel].mask};
    return item<8?values[item]:0;
}

uint32_t sim_groove(uint32_t channel,uint32_t item) {
    if(channel>=8)return 0;
    return item==0?ui.grooves[channel].swing:item==1?ui.grooves[channel].ratchet:item==2?nf_voice_ratchets(&app.voices[channel],app.step?app.step:app.clock.bpm):item==3?app.voices[channel].swing_odd:0;
}

uint32_t sim_pitch(uint32_t channel,uint32_t item) {
    if(channel>=8)return 0;NfTone t=ui.tones[channel];
    unsigned values[]={t.scale,t.root,t.range,t.slew*20u,app.clock.running && app.voices[channel].allowed?nf_voice_note(&app.voices[channel]):255,app.voices[channel].slew_remaining};
    return item<6?values[item]:0;
}
