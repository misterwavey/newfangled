/* SPDX-License-Identifier: MIT
 * Page zero edits tempo; pages 1..8 edit independent outputs. Start toggles transport.
 * Hold Program and press Start to reset phase without changing transport.
 */
#include "ui.h"
#include "font.h"
#include "pnw_board.h"
void original_ui_init(OriginalUi *ui,uint8_t raw,uint32_t now) {
    *ui=(OriginalUi){.bpm=120,.previous=raw,.candidate=raw,.stable=raw,.changed_at=now,.ppqn_index=6,.calibration={{0,4095},{2048,4095}}};
    static const uint8_t ratios[8]={5,6,8,4,2,1,5,5};
    for(unsigned i=0;i<8;++i) {
        ui->outputs[i]=(NfConfig){ratios[i],i==6?1:i==7?2:0,50,0,100,0};ui->effective[i]=ui->outputs[i];
        for(unsigned field=0;field<NF_FIELDS;++field)ui->routes[i][field]=(NfCvRoute){0,100,0};
    }
}
static bool hold_program(OriginalUi *ui) {
    ui->program_consumed=true;
    if(ui->editing || ui->selecting || ui->cv_menu || ui->cv_monitor) {
        ui->editing=false;ui->selecting=false;ui->cv_menu=false;ui->cv_monitor=false;return true;
    }
    if(!ui->page) {ui->selecting=true;ui->field=0;return true;}
    return false;
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
        if(ui->cv_monitor) {ui->cv_input^=1u;dirty=true;}
        else if(ui->cv_menu) {
            if(!ui->editing)ui->cv_field=(uint8_t)(((int)ui->cv_field+step+(int)NF_CV_CONTROLS)%NF_CV_CONTROLS);
            else {
                NfCvRoute *route=&ui->routes[ui->page-1][ui->cv_target];
                if(ui->cv_field==0) {int v=(int)ui->cv_target+step;if(v>=0 && v<(int)NF_FIELDS)ui->cv_target=(uint8_t)v;}
                else if(ui->cv_field==1) {int v=(int)route->source+step;if(v>=0 && v<=2)route->source=(uint8_t)v;}
                else if(ui->cv_field==2) {int v=(int)route->depth+step;if(v>=-100 && v<=100)route->depth=(int8_t)v;}
                else if(ui->cv_field==3) {int v=(int)route->bias+step;if(v>=-100 && v<=100)route->bias=(int8_t)v;}
            }
            dirty=true;
        } else if(ui->editing && ui->page) {
            NfConfig *c=&ui->outputs[ui->page-1];
            uint8_t *values[]={&c->ratio,&c->shape,&c->width,&c->phase,&c->level,&c->offset};
            unsigned minimum=ui->field==2?1u:0u;
            unsigned maximum=ui->field==0?NF_RATIOS-1:ui->field==1?NF_SHAPES-1:ui->field==2||ui->field==3?99u:100u;
            int value=(int)*values[ui->field]+step;
            if(value>=(int)minimum && value<=(int)maximum) {*values[ui->field]=(uint8_t)value;dirty=true;}
        } else if(!ui->page && ui->selecting && ui->editing) {
            if(ui->field==0)ui->external=step>0;
            else if(ui->field==1) {int value=(int)ui->ppqn_index+step;if(value>=0 && value<(int)NF_PPQN_COUNT)ui->ppqn_index=(uint8_t)value;}
            else if(ui->field==2) {int value=(int)ui->run_mode+step;if(value>=0 && value<=2)ui->run_mode=(uint8_t)value;}
            else if(ui->field==4)ui->cv_swapped=step>0;
            dirty=true;
        } else if(ui->selecting) {
            unsigned fields=ui->page?NF_OUTPUT_FIELDS:NF_GLOBAL_FIELDS;
            ui->field=(unsigned)((int)ui->field+step+(int)fields)%fields;dirty=true;
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
                ++ui->reset_count;ui->editing=false;ui->selecting=false;ui->cv_menu=false;ui->cv_monitor=false;ui->program_consumed=true;
            } else ui->running=!ui->running;
            dirty=true;
        }
        if(released && !ui->program_consumed) {
            if((uint32_t)(now-ui->program_at)>=500u) {
                (void)hold_program(ui);
            } else if(ui->cv_monitor)ui->cv_input^=1u;
            else if(ui->cv_menu) {if(ui->cv_field<4)ui->editing=!ui->editing;}
            else if(ui->page) {
                if(!ui->selecting) {ui->selecting=true;ui->field=0;}
                else if(ui->field<NF_FIELDS)ui->editing=!ui->editing;
                else if(ui->field==6) {ui->cv_menu=true;ui->cv_field=0;ui->editing=false;}
                else {ui->cv_monitor=true;ui->editing=false;}
            } else if(ui->selecting) {
                if(ui->field==3) {ui->cv_monitor=true;ui->editing=false;}
                else if(ui->field>=5) {
                    unsigned input=(ui->field-5u)/2u;
                    if(ui->cv_raw_valid&(1u<<input)) {
                        if(ui->field&1u)ui->calibration[input].zero=ui->cv_raw[input];
                        else ui->calibration[input].positive=ui->cv_raw[input];
                    }
                } else ui->editing=!ui->editing;
            } else if(!ui->external)ui->editing=!ui->editing;
            dirty=true;
        }
    }
    if(!(ui->stable&4u) && !ui->program_consumed && (uint32_t)(now-ui->program_at)>=500u) {
        dirty=hold_program(ui)||dirty;
    }
    return dirty;
}
static void number(unsigned value,char text[6]) {
    char reverse[5];unsigned n=0,out=0;
    do {reverse[n++]=(char)('0'+value%10u);value/=10u;}while(value && n<5);
    while(n)text[out++]=reverse[--n];text[out]=0;
}
static unsigned signed_value(unsigned x,unsigned y,int value,unsigned scale) {
    char digits[6];if(value<0) {x=original_text(x,y,"-",scale);value=-value;}
    number((unsigned)value,digits);return original_text(x,y,digits,scale);
}
static void parameter(const NfConfig *c,unsigned field) {
    const char *labels[]={"RATIO ","SHAPE ","WIDTH ","PHASE ","LEVEL ","OFFS "};
    const char *shapes[]={"GATE","TRI","RISE","FALL","RAND"};
    char digits[6];unsigned x=original_text(0,18,labels[field],2);
    if(field==0)original_text(x,18,nf_ratio_names[c->ratio],2);
    else if(field==1)original_text(x,18,shapes[c->shape],2);
    else {unsigned values[]={0,0,c->width,c->phase,c->level,c->offset};number(values[field],digits);x=original_text(x,18,digits,2);original_text(x,18,"%",2);}
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
    char value[6];pnw_oled_clear();
    if(ui->cv_monitor) {
        if(ui->page) {original_text(0,0,"OUT",2);number(ui->page,value);original_text(40,0,value,2);transport_icon(119,ui->active);}
        else {transport_icon(0,ui->active);original_text(14,0,"CV",2);}
        original_text(64,0,"MON",1);
        unsigned input=ui->cv_input,bit=1u<<input;
        if(ui->cv_raw_valid&bit) {number(ui->cv_raw[input],value);unsigned x=original_text(64,8,"R",1);original_text(x,8,value,1);}
        unsigned x=original_text(0,18,input?"CV2 ":"CV1 ",2);
        if(!(ui->cv_raw_valid&bit))original_text(x,18,"---",2);
        else if(!(ui->cv_valid&bit))original_text(x,18,"CAL?",2);
        else {int32_t n=ui->cv_normalized[input]*100;n+=n<0?-NF_CV_Q/2:NF_CV_Q/2;x=signed_value(x,18,n/NF_CV_Q,2);original_text(x,18,"%",2);}
        return;
    }
    if(!ui->page) {
        transport_icon(0,ui->active);
        if(ui->selecting) {
            original_text(14,0,"SET",2);original_text(76,0,ui->field>=5?"CAPTURE":ui->editing?"EDIT":"SELECT",1);
            if(ui->field==0) {unsigned x=original_text(0,18,"SRC ",2);original_text(x,18,ui->external?"EXT":"INT",2);}
            else if(ui->field==1) {unsigned x=original_text(0,18,"PPQN ",2);number(nf_ppqn_values[ui->ppqn_index],value);original_text(x,18,value,2);}
            else if(ui->field==2) {const char *modes[]={"OFF","GATE","RESET"};unsigned x=original_text(0,18,"RUN ",2);original_text(x,18,modes[ui->run_mode],2);}
            else if(ui->field==3)original_text(0,18,"CV MON",2);
            else if(ui->field==4) {unsigned x=original_text(0,18,"SWAP ",2);original_text(x,18,ui->cv_swapped?"ON":"OFF",2);}
            else {
                unsigned input=(ui->field-5u)/2u;bool zero=(ui->field&1u)!=0;
                unsigned x=original_text(0,18,input?(zero?"C2 0V ":"C2 +5 "):(zero?"C1 0V ":"C1 +5 "),2);
                number(zero?ui->calibration[input].zero:ui->calibration[input].positive,value);original_text(x,18,value,2);
            }
            return;
        }
        original_text(14,0,"NEWFANGLED 0.3",1);
        number(ui->external?ui->external_bpm:ui->bpm,value);original_text(0,10,ui->external && !ui->external_bpm?"---":value,2);original_text(43,15,"BPM",1);
        original_text(83,15,ui->editing?"EDIT":"VIEW",1);
        const char *status[]={"TURN OUT / CLICK BPM","EXT WAIT","EXT LOCK","EXT LOST","RUN LOW","EXT ARMED"};
        original_text(0,25,ui->timing_fault?"TIMING FAULT":ui->sync_status?status[ui->sync_status]:ui->editing?"TURN BPM / CLICK DONE":"TURN OUT / CLICK BPM",1);
    } else {
        /* Two readable rows: output above, parameter name and value below. */
        original_text(0,0,"OUT",2);number(ui->page,value);original_text(40,0,value,2);
        unsigned field=ui->selecting?ui->field:0;
        bool routed_base=ui->editing && !ui->cv_menu && field<NF_FIELDS && ui->routes[ui->page-1][field].source;
        original_text(64,0,routed_base?"BASE":ui->editing?"EDIT":ui->selecting?"SELECT":"VIEW",1);
        transport_icon(119,ui->active);
        if(ui->timing_fault) {original_text(0,21,"TIMING FAULT",1);return;}
        if(ui->cv_menu) {
            NfCvRoute route=ui->routes[ui->page-1][ui->cv_target];
            original_text(64,8,ui->cv_field==4?"ACTUAL":"CV SET",1);
            unsigned x;
            if(ui->cv_field==0) {const char *names[]={"RATIO","SHAPE","WIDTH","PHASE","LEVEL","OFFS"};x=original_text(0,18,"DEST ",2);original_text(x,18,names[ui->cv_target],2);}
            else if(ui->cv_field==1) {const char *sources[]={"OFF","CV1","CV2"};x=original_text(0,18,"SRC ",2);original_text(x,18,sources[route.source],2);}
            else if(ui->cv_field<4) {x=original_text(0,18,ui->cv_field==2?"DEP ":"BIAS ",2);x=signed_value(x,18,ui->cv_field==2?route.depth:route.bias,2);original_text(x,18,"%",2);}
            else {parameter(&ui->effective[ui->page-1],ui->cv_target);}
        } else if(field>=NF_FIELDS)original_text(0,18,field==6?"CV SETUP":"CV MON",2);
        else {
            unsigned source=ui->routes[ui->page-1][field].source;
            if(source) {original_text(64,8,source==1?"CV1":"CV2",1);}
            parameter(ui->editing?&ui->outputs[ui->page-1]:&ui->effective[ui->page-1],field);
        }
    }
}
