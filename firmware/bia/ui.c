/* SPDX-License-Identifier: MIT */
#include "bia.h"
#include "../controls.h"
#include "../font.h"
#include "../preset_format.h"
#include "pnw_board.h"
static void number(unsigned n,char text[6]) {char reverse[5];unsigned count=0,i=0;do{reverse[count++]=(char)('0'+n%10u);n/=10;}while(n && count<5);while(count)text[i++]=reverse[--count];text[i]=0;}
static void value(const char *label,unsigned n,const char *unit) {char text[6];unsigned x=original_text(0,18,label,2);number(n,text);x=original_text(x,18,text,2);original_text(x,18,unit,2);}
static void signed_value(const char *label,int n,const char *unit){char digits[6];unsigned x=original_text(0,18,label,2);if(n<0){x=original_text(x,18,"-",2);n=-n;}number((unsigned)n,digits);x=original_text(x,18,digits,2);original_text(x,18,unit,2);}
static const char *const effects[3]={"FILL ","ACC ","VAR "};
static void icon(unsigned x,bool playing) {for(unsigned r=0;r<9;++r){unsigned d=r>4?r-4:4-r;for(unsigned c=0;c<(playing?9u-2u*d:9u);++c)if(playing || c<3u || c>=6u)pnw_oled_pixel(x+c,r,true);}}
void bia_ui_init(BiaUi *u,uint8_t raw,uint32_t now) {*u=(BiaUi){.previous=raw,.candidate=raw,.stable=raw,.changed_at=now};bia_defaults(&u->patch);}
static void back(BiaUi *u) {u->selecting=false;u->editing=false;u->performance_page=false;u->preset_menu=false;u->initialize_confirm=false;u->initialize_yes=false;u->consumed=true;}
static void turn(BiaUi *u,int delta) {
    if(u->start_pending && u->start_playback && !u->performance)return;
    if(u->performance || u->performance_page){
        if(u->performance || u->editing){
            bool fill=u->patch.effect==BIA_FILL;
            int n=(int)(fill?bia_fill_level(u->patch.strength[BIA_FILL]):u->patch.strength[u->patch.effect])+delta;
            if(n>=0 && n<=(fill?5:100))u->patch.strength[u->patch.effect]=(uint8_t)(fill?bia_fill_strength((unsigned)n):(unsigned)n);
        }
        else u->patch.effect=(uint8_t)(((int)u->patch.effect+delta+3)%3);
        return;
    }
    if(u->initialize_confirm){u->initialize_yes=delta>0;return;}
    if(u->preset_menu) {
        if(u->editing){int v=(int)u->preset_slot+delta;if(v>=0 && v<8)u->preset_slot=(uint8_t)v;}
        else u->preset_field=(uint8_t)(((int)u->preset_field+delta+3)%3);
        u->preset_status=NF_PRESET_IDLE;return;
    }
    if(!u->selecting && !u->editing){u->page=(unsigned)((int)u->page+delta+25)%25u;return;}
    if(u->selecting && !u->editing){unsigned count=u->page==0?16:u->page<=16?6:22;u->field=(unsigned)((int)u->field+delta+(int)count)%count;return;}
    if(!u->page) {
        if(u->field==0){int v=(int)u->patch.bpm+delta;if(v>=10 && v<=300)u->patch.bpm=(uint16_t)v;return;}
        uint8_t *fields[]={&u->patch.variation,&u->patch.delay_ms,&u->patch.pulse_ms,&u->patch.swing,&u->patch.external,&u->patch.ppqn_index,&u->patch.run_mode,&u->patch.cv_macro,&u->patch.swapped};
        static const uint8_t min[]={0,1,1,0,0,0,0,0,0},max[]={100,20,10,50,1,7,2,2,1};
        if(u->field<10){unsigned f=u->field-1u;int v=(int)*fields[f]+delta;if(v>=min[f] && v<=max[f])*fields[f]=(uint8_t)v;}
    } else if(u->page<=16) {
        unsigned index=u->page-1;
        if(u->field==4){int n=(int)u->patch.step_accent[index]+delta;if(n>=0 && n<=100)u->patch.step_accent[index]=(uint8_t)n;return;}
        if(u->field==5){if(delta>0)u->patch.fill_mask|=(uint16_t)(1u<<index);else u->patch.fill_mask&=(uint16_t)~(1u<<index);return;}
        BiaStep *s=&u->patch.steps[index];uint8_t *fields[]={&s->enabled,&s->scene,&s->probability,&s->ratchets};
        const unsigned min[]={0,0,0,1},max[]={1,7,100,8};int v=(int)*fields[u->field]+delta;
        if(v>=(int)min[u->field] && v<=(int)max[u->field])*fields[u->field]=(uint8_t)v;
    } else if(u->field>=8) {
        BiaMotion *m=&u->patch.motion[u->page-17];unsigned field=u->field;
        if(field==12){m->retrigger=delta>0;return;}
        if(field>=16){unsigned f=field-16;int n=(int)m->variation[f]+delta;if(n>=0 && n<=100)m->variation[f]=(uint8_t)n;return;}
        if(field==9 || field==11){unsigned f=field==9?0u:1u;int n=(int)m->decay[f]+delta;if(n>=1 && n<=100)m->decay[f]=(uint8_t)n;return;}
        int8_t *target=field<12?&m->envelope[field==8?0:1]:&m->accent[field-13];int n=(int)*target+delta;if(n>=-100 && n<=100)*target=(int8_t)n;
    } else if(u->field) {
        unsigned lane=u->field-1,maximum=lane?100:60;uint16_t *code=&u->patch.scene[u->page-17][lane];
        int v=(int)((*code*maximum+511u)/1023u)+delta;if(v>=0 && v<=(int)maximum)*code=(uint16_t)(((unsigned)v*1023u+maximum/2u)/maximum);
    }
}
static void click(BiaUi *u) {
    if(u->performance_page){u->editing=!u->editing;return;}
    if(u->initialize_confirm){if(u->initialize_yes && !u->running && !u->active && !u->auditioning)u->preset_command=3;u->initialize_confirm=false;u->initialize_yes=false;return;}
    if(u->preset_menu) {
        if(!u->preset_field)u->editing=!u->editing;
        else if(u->running || u->active || u->auditioning)u->preset_status=NF_PRESET_STOP;
        else {u->preset_command=u->preset_field==1?NF_PRESET_SAVE:NF_PRESET_LOAD;u->preset_status=NF_PRESET_BUSY;u->editing=false;}return;
    }
    if(!u->page) {
        if(!u->selecting){u->performance_page=true;u->editing=false;return;}
        if(u->field<10)u->editing=!u->editing;
        else if(u->field<14) {
            unsigned input=(u->field-10u)/2u;if(u->cv_valid&(1u<<input)) {
                if(!(u->field&1u))u->patch.calibration[input].zero=u->cv_raw[input];else u->patch.calibration[input].positive=u->cv_raw[input];
            }
        } else if(u->field==14){u->preset_menu=true;u->preset_field=0;u->editing=false;}
        else {u->initialize_confirm=true;u->initialize_yes=false;u->editing=false;}return;
    }
    if(!u->selecting){u->selecting=true;u->field=0;return;}
    if(u->page>16 && !u->field) {
        if(!u->running && !u->active){u->audition_scene=(uint8_t)(u->page-17);++u->audition_count;}
    } else u->editing=!u->editing;
}
bool bia_ui_update(BiaUi *u,uint8_t raw,uint32_t now) {
    bool dirty=false;int step=nf_encoder_step(&u->encoder,u->previous,raw);
    u->previous=raw;
    if(step){turn(u,step);dirty=true;}
    if((raw&12u)!=(u->candidate&12u)){u->candidate=raw;u->changed_at=now;}
    if((uint32_t)(now-u->changed_at)>=20u && (raw&12u)!=(u->stable&12u)) {
        bool program=(u->stable&4u) && !(raw&4u),released=!(u->stable&4u) && (raw&4u);
        bool start=(u->stable&8u) && !(raw&8u),start_released=!(u->stable&8u) && (raw&8u);
        bool program_before=!(u->stable&4u);u->stable=raw;
        if(program){u->program_at=now;u->consumed=false;}
        if(start){
            u->start_at=now;u->start_consumed=false;u->start_playback=u->running || u->active;u->start_audition=u->auditioning;
            if(program_before){++u->reset_count;back(u);u->start_consumed=true;u->start_pending=false;u->performance=false;}
            else u->start_pending=true;
            dirty=true;
        }
        if(start_released && u->start_pending){
            bool held=u->start_playback && (uint32_t)(now-u->start_at)>=300u;
            if(!u->start_consumed && !held){if(u->start_audition){++u->cancel_count;u->running=false;}else u->running=!u->running;}
            u->performance=false;u->start_pending=false;u->start_consumed=true;
            if(!(raw&4u))u->consumed=true;dirty=true;
        }
        if(released && !u->consumed){
            if(u->start_pending){if(u->performance)u->patch.effect=(uint8_t)((u->patch.effect+1u)%BIA_EFFECTS);u->consumed=true;}
            else if((uint32_t)(now-u->program_at)<500u)click(u);else back(u);
            dirty=true;
        }
    }
    if(u->start_pending && u->start_playback && !u->start_consumed && (uint32_t)(now-u->start_at)>=300u){
        u->performance=true;u->start_consumed=true;u->editing=false;dirty=true;
    }
    if(!(u->stable&4u) && !u->consumed && !u->start_pending && (uint32_t)(now-u->program_at)>=500u){
        if(u->page==0 && !u->selecting && !u->editing && !u->performance_page && !u->preset_menu && !u->initialize_confirm){u->selecting=true;u->field=0;u->consumed=true;}
        else back(u);dirty=true;
    }
    return dirty;
}
void bia_ui_render(const BiaUi *u) {
    char text[6];pnw_oled_clear();icon(119,u->active || u->auditioning);
    if(u->performance || u->performance_page){
        original_text(0,0,"BIA",2);original_text(42,0,u->performance?"LIVE":"PERFORM",1);
        original_text(42,8,u->performance?"RELEASE END":u->editing?"EDIT AMOUNT":"TURN SELECT",1);
        bool fill=u->patch.effect==BIA_FILL;
        value(effects[u->patch.effect],fill?bia_fill_level(u->patch.strength[BIA_FILL]):u->patch.strength[u->patch.effect],fill?"":"%");return;
    }
    if(u->initialize_confirm){original_text(0,0,"ERASE BIA",2);original_text(0,18,u->initialize_yes?"SURE? YES":"SURE? NO",2);return;}
    if(u->preset_menu) {
        original_text(0,0,"MEM",2);const char *status[]={"","BUSY","SAVED","LOADED","EMPTY","STOP FIRST","BAD DATA","IO ERROR","OTHER DATA"};
        unsigned s=u->preset_status;original_text(42,0,s<9?status[s]:"",1);
        original_text(42,8,u->editing?"EDIT":"SELECT",1);
        const char *labels[]={"SLOT ","SAVE ","LOAD "};value(labels[u->preset_field],u->preset_slot+1u,"");return;
    }
    if(!u->page) {
        if(u->selecting) {
            original_text(0,0,"BIA SET",2);
            switch(u->field) {
            case 0:value("BPM ",u->patch.bpm,"");break;
            case 1:value("VAR ",u->patch.variation,"%");break;case 2:value("DELAY ",u->patch.delay_ms,"MS");break;
            case 3:value("PULSE ",u->patch.pulse_ms,"MS");break;case 4:value("SWING ",u->patch.swing,"%");break;
            case 5:original_text(0,18,u->patch.external?"SRC EXT":"SRC INT",2);break;
            case 6:value("PPQN ",nf_ppqn_values[u->patch.ppqn_index],"");break;
            case 7:{const char *names[]={"RUN OFF","RUN GATE","RUN RESET"};original_text(0,18,names[u->patch.run_mode],2);break;}
            case 8:{const char *names[]={"VARCV OFF","VARCV CV1","VARCV CV2"};original_text(0,18,names[u->patch.cv_macro],2);break;}
            case 9:original_text(0,18,u->patch.swapped?"SWAP ON":"SWAP OFF",2);break;
            case 14:original_text(0,18,"PRESETS",2);break;case 15:original_text(0,18,"ERASE MEM",2);break;
            default:{unsigned input=(u->field-10u)/2u;bool zero=(u->field&1u)==0;value(input?(zero?"C2 0 ":"C2 +5 "):(zero?"C1 0 ":"C1 +5 "),zero?u->patch.calibration[input].zero:u->patch.calibration[input].positive,"");break;}
            }
        } else {
            original_text(0,0,"BIA",2);unsigned bpm=u->patch.external?u->external_bpm:u->patch.bpm;value("BPM ",bpm,"");
            const char *states[]={"STOPPED","EXT WAIT","EXT LOCK","EXT LOST","RUN LOW"};original_text(42,0,u->timing_fault?"TIMING FAULT":u->auditioning?"AUDITION":u->sync_status?states[u->sync_status]:u->active?"RUNNING":"STOPPED",1);
            number(u->playing_step+1u,text);unsigned x=original_text(42,8,"STEP ",1);original_text(x,8,text,1);
        }
    } else if(u->page<=16) {
        unsigned step=u->page-1;original_text(0,0,"STEP",2);number(step+1u,text);original_text(60,0,text,2);BiaStep s=u->patch.steps[step];
        if(!u->selecting){original_text(0,18,s.enabled?bia_scene_names[s.scene]:"REST",2);}
        else if(u->field==0)original_text(0,18,s.enabled?"HIT ON":"HIT OFF",2);
        else if(u->field==1)original_text(0,18,bia_scene_names[s.scene],2);
        else if(u->field==2)value("PROB ",s.probability,"%");else if(u->field==3)value("RATCH ",s.ratchets,"");
        else if(u->field==4)value("ACC ",u->patch.step_accent[step],"%");
        else original_text(0,18,(u->patch.fill_mask&(1u<<step))?"FILL ON":"FILL OFF",2);
    } else {
        unsigned scene=u->page-17;original_text(0,0,bia_scene_names[scene],2);
        if(!u->selecting || !u->field)original_text(0,18,"AUDITION",2);
        else if(u->field>=8){
            const BiaMotion *m=&u->patch.motion[scene];unsigned field=u->field;
            if(field==8 || field==10)signed_value(field==8?"FENV ":"HENV ",m->envelope[field==8?0:1],"%");
            else if(field==9 || field==11)value(field==9?"F MS ":"H MS ",m->decay[field==9?0:1]*5u,"");
            else if(field==12)original_text(0,18,m->retrigger?"RETRIG ON":"RETRIG OFF",2);
            else if(field<16){const char *names[]={"FACC ","HACC ","DACC "};signed_value(names[field-13],m->accent[field-13],"%");}
            else {const char *names[]={"DVAR ","HVAR ","FVAR ","MVAR ","SVAR ","AVAR "};value(names[field-16],m->variation[field-16],"%");}
        } else {unsigned f=u->field-1,maximum=f?100u:60u;unsigned n=(u->patch.scene[scene][f]*maximum+511u)/1023u;
            const char *labels[]={"PIT ","DEC ","HARM ","FOLD ","MORPH ","SPRD ","ATK "};value(labels[f],n,f?"%":"ST");}
    }
    if(u->editing)original_text(100,10,"EDIT",1);
}
