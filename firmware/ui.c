/* SPDX-License-Identifier: MIT
 * Page zero edits tempo; pages 1..8 edit independent outputs. Start toggles transport.
 * Hold Program and press Start to reset phase without changing transport.
 */
#include "ui.h"
#include "controls.h"
#include "font.h"
#include "pnw_board.h"
void original_ui_init(OriginalUi *ui,uint8_t raw,uint32_t now) {
    *ui=(OriginalUi){.bpm=120,.previous=raw,.candidate=raw,.stable=raw,.changed_at=now,.ppqn_index=6,.calibration={{0,4095},{2048,4095}}};
    static const uint8_t ratios[8]={5,6,8,4,2,1,5,5};
    for(unsigned i=0;i<8;++i) {
        ui->outputs[i]=(NfConfig){ratios[i],i==6?1:i==7?2:0,50,0,100,0};ui->effective[i]=ui->outputs[i];ui->rhythms[i]=NF_RHYTHM_DEFAULT;ui->grooves[i]=NF_GROOVE_DEFAULT;ui->tones[i]=NF_TONE_DEFAULT;
        for(unsigned field=0;field<NF_CV_FIELDS;++field)ui->routes[i][field]=(NfCvRoute){0,100,0};
    }
}
bool original_ui_clear_output(OriginalUi *ui,unsigned output) {
    if(output>=8)return false;
    ui->extras[output]=(NfExtra){0};
    ui->muted&=(uint8_t)~(1u<<output);
    ui->outputs[output]=(NfConfig){5,0,50,0,0,0};ui->effective[output]=ui->outputs[output];
    ui->rhythms[output]=NF_RHYTHM_DEFAULT;ui->grooves[output]=NF_GROOVE_DEFAULT;ui->tones[output]=NF_TONE_DEFAULT;
    for(unsigned f=0;f<NF_CV_FIELDS;++f)ui->routes[output][f]=(NfCvRoute){0,100,0};
    ++ui->output_reset[output];return true;
}
bool original_ui_copy_output(OriginalUi *ui,unsigned source,unsigned destination) {
    if(source>=8 || destination>=8 || source==destination)return false;
    ui->muted=(uint8_t)((ui->muted&~(1u<<destination))|((ui->muted&(1u<<source))?(1u<<destination):0u));
    ui->outputs[destination]=ui->outputs[source];ui->effective[destination]=ui->outputs[source];
    ui->extras[destination]=ui->extras[source];
    ui->rhythms[destination]=ui->rhythms[source];ui->grooves[destination]=ui->grooves[source];ui->tones[destination]=ui->tones[source];
    for(unsigned f=0;f<NF_CV_FIELDS;++f)ui->routes[destination][f]=ui->routes[source][f];
    ++ui->output_reset[destination];return true;
}
static void leave_edit(OriginalUi *ui) {
    ui->editing=false;ui->selecting=false;ui->cv_menu=false;ui->cv_monitor=false;
    ui->preset_menu=false;ui->clear_confirm=false;ui->clear_yes=false;ui->copy_mode=0;ui->copy_yes=false;
}
static bool hold_program(OriginalUi *ui) {
    ui->program_consumed=true;
    if(ui->editing || ui->selecting || ui->cv_menu || ui->cv_monitor || ui->preset_menu || ui->clear_confirm || ui->copy_mode) {
        leave_edit(ui);return true;
    }
    if(!ui->page) {ui->selecting=true;ui->field=0;return true;}
    return false;
}
/* Share range-checked byte edits to keep the full UI within application flash. */
__attribute__((noinline)) static bool adjust_byte(uint8_t *value,int step,int minimum,int maximum) {
    int next=(int)*value+step;
    if(next<minimum || next>maximum)return false;
    *value=(uint8_t)next;return true;
}
bool original_ui_update(OriginalUi *ui,uint8_t raw,uint32_t now) {
    bool dirty=false;
    int step=nf_encoder_step(&ui->encoder,ui->previous,raw);
    ui->previous=raw;
    if(step) {
        if(ui->copy_mode) {
            if(ui->copy_mode==2)ui->copy_yes=step>0;
            else {
                int target=((int)ui->copy_target+step+8)%8;
                if(target==(int)ui->page-1)target=(target+step+8)%8;
                ui->copy_target=(uint8_t)target;
            }
            dirty=true;
        } else if(ui->clear_confirm) {ui->clear_yes=step>0;dirty=true;}
        else if(ui->preset_menu) {
            if(ui->editing) {(void)adjust_byte(&ui->preset_slot,step,0,7);}
            else ui->preset_field=(uint8_t)(((int)ui->preset_field+step+3)%3);
            ui->preset_status=NF_PRESET_IDLE;dirty=true;
        } else if(ui->cv_monitor) {ui->cv_input^=1u;dirty=true;}
        else if(ui->cv_menu) {
            if(!ui->editing)ui->cv_field=(uint8_t)(((int)ui->cv_field+step+(int)NF_CV_CONTROLS)%NF_CV_CONTROLS);
            else {
                NfCvRoute *route=&ui->routes[ui->page-1][ui->cv_target];
                if(ui->cv_field==0) {(void)adjust_byte(&ui->cv_target,step,0,(int)NF_CV_FIELDS-1);}
                else if(ui->cv_field==1) {int v=(int)route->source+step;if(v>=0 && v<=2)route->source=(uint8_t)v;}
                else if(ui->cv_field==2) {int v=(int)route->depth+step;if(v>=-100 && v<=100)route->depth=(int8_t)v;}
                else if(ui->cv_field==3) {int v=(int)route->bias+step;if(v>=-100 && v<=100)route->bias=(int8_t)v;}
            }
            dirty=true;
        } else if(ui->editing && ui->page) {
            if(ui->field>=19) {
                NfExtra *e=&ui->extras[ui->page-1];
                int current=ui->field==19?e->loop_beats:ui->field==20?e->logic:e->partner;
                int maximum=ui->field==19?128:ui->field==20?3:7,value=current+step;
                if(value>=0 && value<=maximum) {
                    if(ui->field==19)e->loop_beats=(uint16_t)value;
                    else if(ui->field==20)e->logic=(uint8_t)value;else e->partner=(uint8_t)value;
                    dirty=true;
                }
            } else if(ui->field>=15) {
                NfTone *t=&ui->tones[ui->page-1];uint8_t *p[]={&t->slew,&t->scale,&t->root,&t->range};
                unsigned field=ui->field-15;int minimum=field==3?1:0,maximum=field==0?100:field==1?(int)NF_SCALES-1:field==2?11:4;
                dirty=adjust_byte(p[field],step,minimum,maximum)||dirty;
            } else if(ui->field>=13) {
                NfGroove *g=&ui->grooves[ui->page-1];uint8_t *p=ui->field==13?&g->swing:&g->ratchet;
                int minimum=ui->field==13?0:1,maximum=ui->field==13?50:8;
                dirty=adjust_byte(p,step,minimum,maximum)||dirty;
            } else if(ui->field>=8) {
                NfRhythm *r=&ui->rhythms[ui->page-1];uint8_t *p[]={&r->steps,&r->hits,&r->rotate,&r->probability,&r->loop};
                unsigned field=ui->field-8,minimum=field==0?1u:0u;
                unsigned maximum=field==0?32u:field==1?r->steps:field==2?r->steps-1u:field==3?100u:1u;
                if(adjust_byte(p[field],step,(int)minimum,(int)maximum)) {if(r->hits>r->steps)r->hits=r->steps;if(r->rotate>=r->steps)r->rotate=(uint8_t)(r->steps-1u);dirty=true;
                }
            } else {
            NfConfig *c=&ui->outputs[ui->page-1];
            uint8_t *values[]={&c->ratio,&c->shape,&c->width,&c->phase,&c->level,&c->offset};
            unsigned minimum=ui->field==2?1u:0u;
            unsigned maximum=ui->field==0?NF_RATIOS-1:ui->field==1?NF_SHAPES-1:ui->field==2||ui->field==3?99u:100u;
            dirty=adjust_byte(values[ui->field],step,(int)minimum,(int)maximum)||dirty;
        }
        } else if(!ui->page && ui->selecting && ui->editing) {
            if(ui->field==0)ui->external=step>0;
            else if(ui->field==1) {(void)adjust_byte(&ui->ppqn_index,step,0,(int)NF_PPQN_COUNT-1);}
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
                ++ui->reset_count;leave_edit(ui);ui->program_consumed=true;
            } else ui->running=!ui->running;
            dirty=true;
        }
        if(released && !ui->program_consumed) {
            if((uint32_t)(now-ui->program_at)>=500u) {
                (void)hold_program(ui);
            } else if(ui->copy_mode) {
                if(ui->copy_mode==1) {ui->copy_mode=2;ui->copy_yes=false;}
                else {
                    bool copy=ui->copy_yes;ui->copy_mode=0;ui->copy_yes=false;
                    if(copy && original_ui_copy_output(ui,ui->page-1u,ui->copy_target)) {ui->editing=false;ui->selecting=false;ui->field=0;}
                }
            } else if(ui->clear_confirm) {
                bool clear=ui->clear_yes;ui->clear_confirm=false;ui->clear_yes=false;
                if(clear && original_ui_clear_output(ui,ui->page-1u)){ui->editing=false;ui->selecting=false;ui->field=0;}
            } else if(ui->preset_menu) {
                if(!ui->preset_field)ui->editing=!ui->editing;
                else if(ui->running || ui->active)ui->preset_status=NF_PRESET_STOP;
                else {ui->preset_command=ui->preset_field==1?NF_PRESET_SAVE:NF_PRESET_LOAD;ui->preset_status=NF_PRESET_BUSY;ui->editing=false;}
            } else if(ui->cv_monitor)ui->cv_input^=1u;
            else if(ui->cv_menu) {if(ui->cv_field<4)ui->editing=!ui->editing;}
            else if(ui->page) {
                if(!ui->selecting) {ui->selecting=true;ui->field=0;}
                else if(ui->field==NF_MUTE_FIELD) {ui->muted^=(uint8_t)(1u<<(ui->page-1u));ui->editing=false;}
                else if(ui->field==NF_COPY_FIELD) {ui->copy_mode=1;ui->copy_target=(uint8_t)(ui->page%8u);ui->copy_yes=false;ui->editing=false;}
                else if(ui->field==NF_CLEAR_FIELD) {ui->clear_confirm=true;ui->clear_yes=false;ui->editing=false;}
                else if(ui->field<NF_FIELDS || ui->field>=8)ui->editing=!ui->editing;
                else if(ui->field==6) {ui->cv_menu=true;ui->cv_field=0;ui->editing=false;}
                else {ui->cv_monitor=true;ui->editing=false;}
            } else if(ui->selecting) {
                if(ui->field==3) {ui->cv_monitor=true;ui->editing=false;}
                else if(ui->field==9) {ui->preset_menu=true;ui->preset_field=0;ui->editing=false;ui->preset_status=NF_PRESET_IDLE;}
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
    const char *labels[]={"RAT ","SHAPE ","WIDTH ","PHASE ","LEVEL ","OFFS "};
    const char *shapes[]={"GATE","TRI","RISE","FALL","RAND","SINE","ENV"};
    char digits[6];unsigned x=original_text(0,18,labels[field],2);
    if(field==0)original_text(x,18,nf_ratio_names[c->ratio],2);
    else if(field==1)original_text(x,18,shapes[c->shape],2);
    else {unsigned values[]={0,0,c->width,c->phase,c->level,c->offset};number(values[field],digits);x=original_text(x,18,digits,2);original_text(x,18,"%",2);}
}
static unsigned routed_field(unsigned field) {
    return field<6?field:field==9?6:field==10?7:field==11?8:field==13?9:NF_CV_FIELDS;
}
static void transport_icon(unsigned x,bool running) {
    /* Filled hi-fi symbols, drawn directly so they do not depend on a font. */
    for(unsigned row=0;row<9;++row) {
        unsigned distance=row>4?row-4:4-row;
        unsigned width=running?9u-2u*distance:9u;
        for(unsigned col=0;col<width;++col)if(running || col<3u || col>=6u)pnw_oled_pixel(x+col,row,true);
    }
}
void original_ui_render(const OriginalUi *ui) {
    char value[6];pnw_oled_clear();
    if(ui->preset_menu) {
        transport_icon(0,ui->active);original_text(14,0,"MEM",2);
        original_text(76,0,ui->editing?"EDIT":"SELECT",1);
        const char *status[]={"","BUSY","SAVED","LOADED","EMPTY","STOP FIRST","BAD DATA","I/O ERROR","NEWER DATA"};
        unsigned state=ui->preset_status;
        original_text(64,8,state<9 && state?status[state]:(ui->preset_mask&(1u<<ui->preset_slot))?"STORED":"EMPTY",1);
        const char *labels[]={"SLOT ","SAVE ","LOAD "};unsigned x=original_text(0,18,labels[ui->preset_field],2);
        number(ui->preset_slot+1u,value);original_text(x,18,value,2);return;
    }
    if(ui->cv_monitor) {
        if(ui->page) {original_text(0,0,"OUT",2);number(ui->page,value);original_text(40,0,value,2);transport_icon(119,ui->active);}
        else {transport_icon(0,ui->active);original_text(14,0,"CV",2);}
        original_text(64,0,"MON",1);if(ui->page && (ui->muted&(1u<<(ui->page-1u))))original_text(110,0,"M",1);
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
            original_text(14,0,"SET",2);original_text(76,0,ui->field>=5 && ui->field<9?"CAPTURE":ui->editing?"EDIT":"SELECT",1);
            if(ui->field==0) {unsigned x=original_text(0,18,"SRC ",2);original_text(x,18,ui->external?"EXT":"INT",2);}
            else if(ui->field==1) {unsigned x=original_text(0,18,"PPQN ",2);number(nf_ppqn_values[ui->ppqn_index],value);original_text(x,18,value,2);}
            else if(ui->field==2) {const char *modes[]={"OFF","GATE","RESET"};unsigned x=original_text(0,18,"RUN ",2);original_text(x,18,modes[ui->run_mode],2);}
            else if(ui->field==3)original_text(0,18,"CV MON",2);
            else if(ui->field==4) {unsigned x=original_text(0,18,"SWAP ",2);original_text(x,18,ui->cv_swapped?"ON":"OFF",2);}
            else if(ui->field==9)original_text(0,18,"PRESETS",2);
            else {
                unsigned input=(ui->field-5u)/2u;bool zero=(ui->field&1u)!=0;
                unsigned x=original_text(0,18,input?(zero?"C2 0V ":"C2 +5 "):(zero?"C1 0V ":"C1 +5 "),2);
                number(zero?ui->calibration[input].zero:ui->calibration[input].positive,value);original_text(x,18,value,2);
            }
            return;
        }
        original_text(14,0,"NEWFANGLED 0.11",1);
        number(ui->external?ui->external_bpm:ui->bpm,value);original_text(0,10,ui->external && !ui->external_bpm?"---":value,2);original_text(43,15,"BPM",1);
        original_text(83,15,ui->editing?"EDIT":"VIEW",1);
        const char *status[]={"TURN OUT / CLICK BPM","EXT WAIT","EXT LOCK","EXT LOST","RUN LOW","EXT ARMED"};
        original_text(0,25,ui->timing_fault?"TIMING FAULT":ui->sync_status?status[ui->sync_status]:ui->editing?"TURN BPM / CLICK DONE":"TURN OUT / CLICK BPM",1);
    } else {
        /* Two readable rows: output above, parameter name and value below. */
        original_text(0,0,"OUT",2);number(ui->page,value);original_text(40,0,value,2);
        unsigned field=ui->selecting?ui->field:0;
        unsigned destination=routed_field(field);
        bool routed_base=ui->editing && !ui->cv_menu && destination<NF_CV_FIELDS && ui->routes[ui->page-1][destination].source;
        original_text(64,0,ui->copy_mode==2 || ui->clear_confirm?"CONFIRM":ui->copy_mode?"DEST":routed_base?"BASE":ui->editing?"EDIT":ui->selecting?"SELECT":(ui->muted&(1u<<(ui->page-1u)))?"MUTED":"VIEW",1);
        if(ui->muted&(1u<<(ui->page-1u)))original_text(110,0,"M",1);
        transport_icon(119,ui->active);
        if(!ui->cv_menu && destination>=6 && destination<NF_CV_FIELDS && ui->routes[ui->page-1][destination].source)
            original_text(64,8,ui->routes[ui->page-1][destination].source==1?"CV1":"CV2",1);
        if(ui->copy_mode) {
            if(ui->copy_mode==1) {unsigned x=original_text(0,18,"TO OUT",2);number(ui->copy_target+1u,value);original_text(x,18,value,2);}
            else {unsigned x=original_text(64,8,"TO OUT",1);number(ui->copy_target+1u,value);original_text(x,8,value,1);original_text(0,18,ui->copy_yes?"COPY? YES":"COPY? NO",2);}
            return;
        }
        if(ui->clear_confirm) {original_text(0,18,ui->clear_yes?"CLEAR? YES":"CLEAR? NO",2);return;}
        if(ui->timing_fault) {original_text(0,21,"TIMING FAULT",1);return;}
        if(ui->cv_menu) {
            NfCvRoute route=ui->routes[ui->page-1][ui->cv_target];
            original_text(64,8,ui->cv_field==4?"ACTUAL":"CV SET",1);
            unsigned x;
            if(ui->cv_field==0) {const char *names[]={"RATIO","SHAPE","WIDTH","PHASE","LEVEL","OFFS","HITS","ROT","PROB","SWING"};x=original_text(0,18,"DEST ",2);original_text(x,18,names[ui->cv_target],2);}
            else if(ui->cv_field==1) {const char *sources[]={"OFF","CV1","CV2"};x=original_text(0,18,"SRC ",2);original_text(x,18,sources[route.source],2);}
            else if(ui->cv_field<4) {x=original_text(0,18,ui->cv_field==2?"DEP ":"BIAS ",2);x=signed_value(x,18,ui->cv_field==2?route.depth:route.bias,2);original_text(x,18,"%",2);}
            else if(ui->cv_target<NF_FIELDS)parameter(&ui->effective[ui->page-1],ui->cv_target);
            else {const char *names[]={"HITS ","ROT ","PROB ","SWING "};unsigned f=ui->cv_target-6;
                x=original_text(0,18,names[f],2);number(ui->effective_cv[ui->page-1][f],value);original_text(x,18,value,2);}
        } else if(field==NF_MUTE_FIELD)original_text(0,18,(ui->muted&(1u<<(ui->page-1u)))?"MUTE ON":"MUTE OFF",2);
        else if(field==NF_COPY_FIELD)original_text(0,18,"COPY TO",2);
        else if(field==NF_CLEAR_FIELD)original_text(0,18,"CLEAR OUT",2);
        else if(field>=19) {
            NfExtra e=ui->extras[ui->page-1];unsigned x;
            if(field==19) {x=original_text(0,18,"BEATS ",2);if(e.loop_beats){number(e.loop_beats,value);original_text(x,18,value,2);}else original_text(x,18,"OFF",2);}
            else if(field==20) {const char *names[]={"OFF","AND","OR","XOR"};x=original_text(0,18,"LOGIC ",2);original_text(x,18,names[e.logic],2);}
            else {x=original_text(0,18,"WITH ",2);number(e.partner+1u,value);original_text(x,18,value,2);}
        } else if(field>=15) {
            NfTone t=ui->tones[ui->page-1];unsigned x;
            if(field==15) {x=original_text(0,18,"SLW ",2);number(t.slew*20u,value);x=original_text(x,18,value,2);original_text(x,18,"MS",2);}
            else if(field==16) {x=original_text(0,18,"SCL ",2);original_text(x,18,nf_scale_names[t.scale],2);}
            else if(field==17) {x=original_text(0,18,"ROOT ",2);original_text(x,18,nf_note_names[t.root],2);}
            else {x=original_text(0,18,"OCTS ",2);number(t.range,value);original_text(x,18,value,2);}
        } else if(field>=13) {
            NfGroove g=ui->grooves[ui->page-1];if(!ui->editing && ui->routes[ui->page-1][9].source)g.swing=ui->effective_cv[ui->page-1][3];unsigned x=original_text(0,18,field==13?"SWING ":"RATCH ",2);
            number(field==13?g.swing:g.ratchet,value);x=original_text(x,18,value,2);if(field==13)original_text(x,18,"%",2);
        } else if(field>=8) {
            NfRhythm actual=ui->rhythms[ui->page-1];
            if(!ui->editing){uint8_t *v[]={&actual.hits,&actual.rotate,&actual.probability};for(unsigned f=0;f<3;++f)if(ui->routes[ui->page-1][f+6].source)*v[f]=ui->effective_cv[ui->page-1][f];}
            const NfRhythm *r=&actual;const char *labels[]={"STEPS ","HITS ","ROT ","PROB ","LOOP "};
            unsigned f=field-8,x=original_text(0,18,labels[f],2);unsigned values[]={r->steps,r->hits,r->rotate,r->probability,r->loop};
            if(f==4)original_text(x,18,r->loop?"ON":"OFF",2);
            else {number(values[f],value);x=original_text(x,18,value,2);if(f==3)original_text(x,18,"%",2);}
        } else if(field>=NF_FIELDS)original_text(0,18,field==6?"CV SETUP":"CV MON",2);
        else {
            unsigned source=ui->routes[ui->page-1][field].source;
            if(source) {original_text(64,8,source==1?"CV1":"CV2",1);}
            parameter(ui->editing?&ui->outputs[ui->page-1]:&ui->effective[ui->page-1],field);
        }
    }
}
