/* SPDX-License-Identifier: MIT
 * Page zero edits tempo; pages 1..8 edit independent outputs. Start toggles transport.
 * Hold Program and press Start to reset phase without changing transport.
 */
#include "ui.h"
#include "font.h"
#include "pnw_board.h"
void original_ui_init(OriginalUi *ui,uint8_t raw,uint32_t now) {
    *ui=(OriginalUi){.bpm=120,.previous=raw,.candidate=raw,.stable=raw,.changed_at=now};
    static const uint8_t ratios[8]={5,6,8,4,2,1,5,5};
    for(unsigned i=0;i<8;++i)ui->outputs[i]=(NfConfig){ratios[i],i==6?1:i==7?2:0,50,0,100,0};
}
bool original_ui_update(OriginalUi *ui,uint8_t raw,uint32_t now) {
    bool dirty=false;
    unsigned old=ui->previous&3u,next=raw&3u;
    if((old^next)==3u)ui->encoder=0;
    /* Rev 62 bench observation: clockwise follows 3,2,0,1,3. */
    else if(old!=next)ui->encoder+=((old&1u)^((next>>1)&1u))?-1:1;
    ui->previous=raw;
    if(ui->encoder>=4 || ui->encoder<=-4) {
        int step=ui->encoder>0?1:-1;ui->encoder=0;
        if(ui->editing && ui->page) {
            NfConfig *c=&ui->outputs[ui->page-1];
            uint8_t *values[]={&c->ratio,&c->shape,&c->width,&c->phase,&c->level,&c->offset};
            unsigned minimum=ui->field==2?1u:0u;
            unsigned maximum=ui->field==0?NF_RATIOS-1:ui->field==1?NF_SHAPES-1:ui->field==2||ui->field==3?99u:100u;
            int value=(int)*values[ui->field]+step;
            if(value>=(int)minimum && value<=(int)maximum) {*values[ui->field]=(uint8_t)value;dirty=true;}
        } else if(ui->page && ui->selecting) {
            ui->field=(unsigned)((int)ui->field+step+NF_FIELDS)%NF_FIELDS;dirty=true;
        } else if(ui->editing) {
            int bpm=(int)ui->bpm+step;
            if(bpm>=10 && bpm<=300) {ui->bpm=(unsigned)bpm;dirty=true;}
        } else {ui->page=(unsigned)((int)ui->page+step+9)%9u;dirty=true;}
    }
    if((raw&12u)!=(ui->candidate&12u)) {ui->candidate=raw;ui->changed_at=now;}
    if((uint32_t)(now-ui->changed_at)>=20u && (raw&12u)!=(ui->stable&12u)) {
        bool program=(ui->stable&4u) && !(raw&4u);
        bool released=!(ui->stable&4u) && (raw&4u);
        bool start=(ui->stable&8u) && !(raw&8u);
        ui->stable=raw;
        if(program) {ui->program_at=now;ui->program_consumed=false;}
        if(start) {
            if(!(raw&4u)) {
                ++ui->reset_count;ui->editing=false;ui->selecting=false;ui->program_consumed=true;
            } else ui->running=!ui->running;
            dirty=true;
        }
        if(released && !ui->program_consumed) {
            if((uint32_t)(now-ui->program_at)>=500u) {
                ui->editing=false;ui->selecting=false;ui->program_consumed=true;
            } else if(ui->page) {
                if(!ui->selecting) {ui->selecting=true;ui->field=0;}
                else ui->editing=!ui->editing;
            } else ui->editing=!ui->editing;
            dirty=true;
        }
    }
    if(!(ui->stable&4u) && !ui->program_consumed && (uint32_t)(now-ui->program_at)>=500u) {
        ui->program_consumed=true;
        if(ui->editing || ui->selecting) {ui->editing=false;ui->selecting=false;dirty=true;}
    }
    return dirty;
}
static void number(unsigned value,char text[4]) {
    unsigned n=0;
    if(value>=100) {text[n++]=(char)('0'+value/100);value%=100;}
    if(n || value>=10) {text[n++]=(char)('0'+value/10);value%=10;}
    text[n++]=(char)('0'+value);text[n]=0;
}
static void transport_icon(unsigned x,bool running) {
    /* Filled hi-fi symbols, drawn directly so they do not depend on a font. */
    for(unsigned row=0;row<9;++row) {
        unsigned distance=row>4?row-4:4-row;
        unsigned width=running?9u-2u*distance:9u;
        for(unsigned col=0;col<width;++col)pnw_oled_pixel(x+col,row,true);
    }
}
void original_ui_render(const OriginalUi *ui) {
    char value[4];pnw_oled_clear();
    if(!ui->page) {
        transport_icon(0,ui->running);original_text(14,0,"NEWFANGLED 0.1",1);
        number(ui->bpm,value);original_text(0,10,value,2);original_text(43,15,"BPM",1);
        original_text(83,15,ui->editing?"EDIT":"VIEW",1);
        original_text(0,25,ui->timing_fault?"TIMING FAULT":ui->editing?"TURN BPM / CLICK DONE":"TURN OUT / CLICK BPM",1);
    } else {
        /* Two readable rows: output above, parameter name and value below. */
        original_text(0,0,"OUT",2);number(ui->page,value);original_text(40,0,value,2);
        const NfConfig *c=&ui->outputs[ui->page-1];
        const char *labels[]={"RATIO ","SHAPE ","WIDTH ","PHASE ","LEVEL ","OFFS "};
        const char *shapes[]={"GATE","TRI","RISE","FALL","RAND"};
        unsigned field=ui->selecting?ui->field:0;
        original_text(64,0,ui->editing?"EDIT":ui->selecting?"SELECT":"VIEW",1);
        transport_icon(119,ui->running);
        if(ui->timing_fault) {original_text(0,21,"TIMING FAULT",1);return;}
        unsigned x=original_text(0,18,labels[field],2);
        if(field==0)original_text(x,18,nf_ratio_names[c->ratio],2);
        else if(field==1)original_text(x,18,shapes[c->shape],2);
        else {unsigned values[]={0,0,c->width,c->phase,c->level,c->offset};number(values[field],value);x=original_text(x,18,value,2);original_text(x,18,"%",2);}
    }
}
