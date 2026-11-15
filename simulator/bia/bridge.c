/* SPDX-License-Identifier: MIT. Host board for the actual BIA C firmware. */
#include <stddef.h>
#include <stdint.h>
#include "../../firmware/bia/bia.h"
#include "../../firmware/store.h"
#include "pnw_board.h"
static BiaApp app;static NfStore store;static uint8_t media[8192],bytes[NF_SETTINGS_BYTES],pixels[512],raw;
static uint32_t now,adc[2],clock_phase,clock_bpm,clock_ppqn,edges;static bool clock_on,first_edge,run;
static uint32_t edge_time[2048];static uint16_t edge_code[2048],last_gate;
void *memset(void *d,int v,size_t n){uint8_t *p=d;while(n--)*p++=(uint8_t)v;return d;}
void *memcpy(void *d,const void *s,size_t n){uint8_t *p=d;const uint8_t *q=s;while(n--)*p++=*q++;return d;}
static bool rd(void *c,unsigned at,void *p,unsigned n){(void)c;if(at>8192 || n>8192-at)return false;memcpy(p,media+at,n);return true;}
static bool er(void *c,unsigned bank){(void)c;if(bank>1)return false;memset(media+bank*4096,255,4096);return true;}
static bool wr(void *c,unsigned at,const void *p,unsigned n){(void)c;if(at>8192 || n>8192-at)return false;const uint8_t *q=p;for(unsigned i=0;i<n;++i){if((media[at+i]&q[i])!=q[i])return false;media[at+i]=q[i];}return true;}
void pnw_oled_clear(void){memset(pixels,0,512);}
void pnw_oled_pixel(unsigned x,unsigned y,bool on){if(x>=128 || y>=32)return;unsigned at=x+(y>>3)*128u;uint8_t mask=(uint8_t)(1u<<(y&7u));if(on)pixels[at]|=mask;else pixels[at]&=(uint8_t)~mask;}
static void capture(void){if(app.output[0]!=last_gate){unsigned i=edges++%2048u;edge_time[i]=now;edge_code[i]=app.output[0];last_gate=app.output[0];}}
void bs_reboot(void){
    now=0;raw=15;adc[0]=0;adc[1]=2048;clock_phase=0;clock_bpm=120;clock_ppqn=24;clock_on=false;first_edge=true;run=false;edges=0;last_gate=0;
    bia_init(&app,raw,0);NfStoreIo io={0,rd,er,wr};(void)nf_store_open(&store,io);(void)nf_presets_boot(&store,&app.ui);bia_sync(&app);bia_ui_render(&app.ui);
}
void bs_reset(void){memset(media,255,8192);bs_reboot();}
static void commands(void){
    if(!app.ui.preset_command)return;
    bool stopped=!app.requested && !app.running && !app.auditioning;
    if(app.ui.preset_command==3){app.ui.preset_command=0;if(stopped){(void)er(0,0);(void)er(0,1);NfStoreIo io={0,rd,er,wr};(void)nf_store_open(&store,io);app.ui.preset_mask=0;app.ui.preset_status=NF_PRESET_EMPTY;}else app.ui.preset_status=NF_PRESET_STOP;}
    else (void)nf_presets_execute(&store,&app.ui,stopped);
    bia_sync(&app);if(stopped)nf_sync_init(&app.sync,nf_ppqn_values[app.patch.ppqn_index]);
}
void bs_advance(uint32_t ms){
    if(ms>60000u)ms=60000u;
    for(unsigned i=0;i<ms;++i){++now;
        if(clock_on){unsigned prior=clock_phase;clock_phase=(clock_phase+clock_bpm*clock_ppqn)%60000u;if(clock_phase<prior || first_edge){uint32_t us=now*1000u;if(!first_edge)us-=clock_phase*1000u/(clock_bpm*clock_ppqn);else us=(now-1u)*1000u;bia_input(&app,NF_INPUT_CLOCK,us);first_edge=false;}}
        bia_cv(&app,adc[0],adc[1],3);bia_tick(&app,now*1000u);capture();bool dirty=bia_ui_update(&app.ui,raw,now);
        if(dirty){bia_sync(&app);capture();}if(app.ui.preset_command){commands();dirty=true;}dirty=bia_refresh(&app)||dirty;if(dirty)bia_ui_render(&app.ui);
    }
}
void bs_turn(uint32_t clockwise){static const uint8_t f[]={2,0,1,3},r[]={1,0,2,3};const uint8_t *s=clockwise?f:r;for(unsigned i=0;i<4;++i){raw=(uint8_t)((raw&12u)|s[i]);bs_advance(1);}}
void bs_button(uint32_t program,uint32_t pressed){uint8_t bit=program?4u:8u;if(pressed)raw&=(uint8_t)~bit;else raw|=bit;bs_advance(25);}
void bs_perform(uint32_t effect,uint32_t active){if(effect>=3)return;app.ui.patch.effect=(uint8_t)effect;app.ui.performance=active && app.requested && app.running;bia_sync(&app);capture();bia_ui_render(&app.ui);}
void bs_abort(void){app.ui.performance=false;app.ui.start_consumed=true;raw|=12u;bia_sync(&app);capture();bs_advance(25);bia_ui_render(&app.ui);}
void bs_click(uint32_t program){uint8_t bit=program?4u:8u;raw&=(uint8_t)~bit;bs_advance(25);raw|=bit;bs_advance(25);}
void bs_hold(void){raw&=(uint8_t)~4u;bs_advance(550);raw|=4u;bs_advance(25);}
void bs_phase_reset(void){raw&=(uint8_t)~4u;bs_advance(25);raw&=(uint8_t)~8u;bs_advance(25);raw|=12u;bs_advance(25);}
void bs_input(uint32_t kind,uint32_t value){
    if(kind<2)adc[kind]=value>4095?4095:value;
    else if(kind==2){bool high=value!=0;if(high!=run)bia_input(&app,high?NF_INPUT_RUN_HIGH:NF_INPUT_RUN_LOW,now*1000u);run=high;}
    else if(kind==3){clock_on=value!=0;if(!clock_on){clock_phase=0;first_edge=true;}}
    else if(kind==4)clock_bpm=value<10?10:value>300?300:value;
    else if(kind==5)clock_ppqn=value<1?1:value>48?48:value;
}
uint32_t bs_edit(uint32_t kind,uint32_t index,uint32_t field,uint32_t value){
    BiaPatch p=app.ui.patch;
    if(kind==0){if(field==0){if(value>65535)return 0;p.bpm=(uint16_t)value;}else {if(value>255)return 0;uint8_t *v[]={&p.variation,&p.delay_ms,&p.pulse_ms,&p.swing,&p.external,&p.ppqn_index,&p.run_mode,&p.cv_macro,&p.swapped};if(field>=10 && field<=13){if(field==10)p.effect=(uint8_t)value;else p.strength[field-11]=(uint8_t)value;}
        else {if(field<1 || field>9)return 0;*v[field-1]=(uint8_t)value;}}}
    else if(kind==1){if(index>=16 || field>=4 || value>255)return 0;uint8_t *v[]={&p.steps[index].scene,&p.steps[index].probability,&p.steps[index].ratchets,&p.steps[index].enabled};*v[field]=(uint8_t)value;}
    else if(kind==2){if(index>=8 || field>=7 || value>1023)return 0;p.scene[index][field]=(uint16_t)value;}
    else if(kind==3){if(index>=2 || field>=2 || value>4095)return 0;if(field)p.calibration[index].positive=(uint16_t)value;else p.calibration[index].zero=(uint16_t)value;}
    else if(kind==4){
        if(index>=8 || field>=14)return 0;BiaMotion *m=&p.motion[index];int n=(int32_t)value;
        if(field==0 || field==2){if(n<-100 || n>100)return 0;m->envelope[field==0?0:1]=(int8_t)n;}
        else if(field==1 || field==3){if(value<1 || value>100)return 0;m->decay[field==1?0:1]=(uint8_t)value;}
        else if(field==4){if(value>1)return 0;m->retrigger=value!=0;}
        else if(field<8){if(n<-100 || n>100)return 0;m->accent[field-5]=(int8_t)n;}
        else {if(value>100)return 0;m->variation[field-8]=(uint8_t)value;}
    } else if(kind==5){
        if(index>=16 || field>1)return 0;
        if(field==0){if(value>100)return 0;p.step_accent[index]=(uint8_t)value;}
        else {if(value>1)return 0;if(value)p.fill_mask|=(uint16_t)(1u<<index);else p.fill_mask&=(uint16_t)~(1u<<index);}
    }
    else return 0;
    if(!bia_patch_valid(&p))return 0;app.ui.patch=p;bia_sync(&app);capture();bia_ui_render(&app.ui);return 1;
}
uint32_t bs_get(uint32_t field){switch(field){case 0:return now;case 1:return app.running;case 2:return app.requested;case 3:return app.index;case 4:return app.phase;case 5:return app.ratchets;case 6:return app.age;case 7:return app.ui.sync_status;case 8:return app.auditioning;case 9:return store.mask;case 10:return store.last_slot;case 11:return store.sequence;case 12:return app.ui.page;case 13:return app.ui.field;case 14:return app.ui.selecting;case 15:return app.ui.editing;case 16:return app.ui.preset_status;case 17:return app.lead;case 18:return app.pulse;case 19:return app.spacing;case 20:return app.event;case 21:return app.ui.external_bpm;case 22:return app.performance;case 23:return app.effect;case 24:return app.amount;
case 25:return app.envelope_age;case 26:return app.ui.performance_page;case 27:return app.ui.start_pending;default:return 0;}}
uint32_t bs_output(uint32_t i){return i<8?app.output[i]:0;}
uintptr_t bs_pixels(void){return (uintptr_t)pixels;}
uintptr_t bs_buffer(void){return (uintptr_t)bytes;}
uint32_t bs_patch_read(void){return nf_settings_encode(&app.ui.patch,bytes);}
uint32_t bs_validate(void){BiaPatch p;return nf_settings_decode(&p,bytes);}
uint32_t bs_patch_import(void){BiaPatch p;if(app.requested || app.running || app.auditioning || !nf_settings_decode(&p,bytes))return 0;nf_settings_apply(&app.ui,&p);bia_sync(&app);bia_ui_render(&app.ui);return 1;}
uint32_t bs_preset_read(uint32_t slot){BiaPatch p;return nf_store_load(&store,slot,&p)==NF_STORE_OK && nf_settings_encode(&p,bytes);}
uint32_t bs_preset_import(uint32_t slot){BiaPatch p;if(app.requested || app.running || app.auditioning || !nf_settings_decode(&p,bytes))return 0;return nf_store_save(&store,slot,&p)==NF_STORE_OK;}
uint32_t bs_preset(uint32_t slot,uint32_t load){if(slot>=8)return 0;app.ui.preset_slot=(uint8_t)slot;app.ui.preset_command=load?NF_PRESET_LOAD:NF_PRESET_SAVE;commands();bia_ui_render(&app.ui);return app.ui.preset_status;}
void bs_audition(uint32_t scene){if(scene>=8 || app.requested || app.running)return;app.ui.audition_scene=(uint8_t)scene;++app.ui.audition_count;bia_sync(&app);capture();bia_ui_render(&app.ui);}
uint32_t bs_edges(void){return edges;}
uint32_t bs_edge(uint32_t i,uint32_t field){if(i>=edges || edges-i>2048)return UINT32_MAX;return field?edge_code[i%2048]:edge_time[i%2048];}
