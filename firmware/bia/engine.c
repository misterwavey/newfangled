/* SPDX-License-Identifier: MIT
 * One event owns trigger probability, a CV snapshot and all of its ratchets.
 * Scene CV is published at age 0; no trigger is allowed before a later tick.
 */
#include "bia.h"
const char *const bia_lane_names[7]={"PITCH","DECAY","HARM","FOLD","MORPH","SPREAD","ATTACK"};
const char *const bia_scene_names[8]={"KICK","SNARE","HAT","BASS","TOM","CLANG","WOOD","NOISE"};
unsigned bia_fill_level(unsigned strength) {
    return !strength?0u:strength<=25?1u:strength<=50?2u:strength<=66?3u:strength<=83?4u:5u;
}
unsigned bia_fill_strength(unsigned level) {
    static const uint8_t strengths[6]={0,25,50,66,83,100};
    return strengths[level>5?5:level];
}
void bia_legacy_extras(BiaPatch *p) {
    p->fill_mask=0xa000u;p->effect=BIA_FILL;p->strength[0]=50;p->strength[1]=50;p->strength[2]=25;
    for(unsigned i=0;i<16;++i)p->step_accent[i]=0;
    for(unsigned i=0;i<8;++i){p->motion[i]=(BiaMotion){.decay={10,10},.retrigger=true};for(unsigned f=0;f<6;++f)p->motion[i].variation[f]=20;}
}
void bia_defaults(BiaPatch *p) {
    *p=(BiaPatch){.bpm=120,.delay_ms=3,.pulse_ms=3,.ppqn_index=6,.calibration={{0,4095},{2048,4095}}};
    /* Starting CV recipes, not measured BIA factory sounds. Pitch in semitones,
       the other columns in percent. Knob positions remain part of the patch. */
    static const uint8_t recipes[8][7]={{0,20,5,0,0,0,50},{7,25,70,20,30,60,10},{36,8,90,40,70,90,5},{0,60,20,20,20,0,50},
        {12,30,20,0,5,10,50},{24,45,90,50,40,80,50},{24,8,20,20,60,30,50},{12,50,100,70,100,100,0}};
    for(unsigned s=0;s<8;++s)for(unsigned f=0;f<7;++f)p->scene[s][f]=(uint16_t)((recipes[s][f]*1023u+(f?50u:30u))/(f?100u:60u));
    bia_legacy_extras(p);
    for(unsigned i=0;i<8;++i){p->motion[i].accent[0]=10;p->motion[i].accent[1]=10;p->motion[i].accent[2]=5;
        static const uint8_t limits[6]={10,20,20,10,10,0};for(unsigned f=0;f<6;++f)p->motion[i].variation[f]=limits[f];}
    for(unsigned i=0;i<16;++i)p->steps[i]=(BiaStep){(uint8_t)(i%8u==0?0:i%4u==0?1:2),100,1,(uint8_t)(!(i&1u))};
}
bool bia_patch_valid(const BiaPatch *p) {
    if(p->bpm<10 || p->bpm>300 || p->variation>100 || p->delay_ms<1 || p->delay_ms>20 || p->pulse_ms<1 || p->pulse_ms>10 ||
       p->swing>50 || p->external>1 || p->ppqn_index>=NF_PPQN_COUNT || p->run_mode>NF_RUN_RESET || p->cv_macro>2 || p->swapped>1)return false;
    for(unsigned i=0;i<2;++i)if(p->calibration[i].zero>4095 || p->calibration[i].positive>4095)return false;
    for(unsigned s=0;s<8;++s)for(unsigned f=0;f<7;++f)if(p->scene[s][f]>1023)return false;
    for(unsigned i=0;i<16;++i){BiaStep s=p->steps[i];if(s.scene>=8 || s.probability>100 || s.ratchets<1 || s.ratchets>8 || s.enabled>1)return false;}
    if(p->effect>=BIA_EFFECTS)return false;
    for(unsigned i=0;i<3;++i)if(p->strength[i]>100)return false;
    for(unsigned i=0;i<16;++i)if(p->step_accent[i]>100)return false;
    for(unsigned i=0;i<8;++i){const BiaMotion *m=&p->motion[i];
        for(unsigned f=0;f<2;++f)if(m->envelope[f]<-100 || m->envelope[f]>100 || m->decay[f]<1 || m->decay[f]>100)return false;
        for(unsigned f=0;f<3;++f)if(m->accent[f]<-100 || m->accent[f]>100)return false;
        for(unsigned f=0;f<6;++f)if(m->variation[f]>100)return false;
    }
    return true;
}
static int divide_signed(int n,unsigned d){return n<0?-(int)((unsigned)(-n)/d):(int)((unsigned)n/d);}
static uint32_t random_next(BiaApp *a) {uint32_t x=a->seed;x^=x<<13;x^=x>>17;x^=x<<5;return a->seed=x;}
static unsigned span(const BiaApp *a) {unsigned d=150u*a->patch.swing;return a->index&1u?15000u-d:15000u+d;}
static uint16_t bounded(int x){return (uint16_t)(x<0?0:x>1023?1023:x);}
static void cancel_performance(BiaApp *a) {
    a->performance=false;a->ui.performance=false;
    if(a->ui.start_pending && a->ui.start_playback)a->ui.start_consumed=true;
}
static void zero(BiaApp *a) {
    for(unsigned i=0;i<8;++i)a->output[i]=0;
    a->event=false;a->auditioning=false;a->ratchets=0;a->fill_event=false;a->fill_boost=false;cancel_performance(a);
}
static unsigned variation_amount(const BiaApp *a) {
    unsigned amount=a->variation;
    if(a->performance && a->effect==BIA_VARIATION)amount+=a->amount;
    return amount>100?100:amount;
}
static void publish(BiaApp *a) {
    if(!a->running && !a->auditioning){zero(a);return;}
    unsigned amount=variation_amount(a),accent=a->accent;
    if(a->performance && a->effect==BIA_ACCENT)accent+=a->amount;
    if(accent>100)accent=100;
    for(unsigned f=0;f<7;++f){
        int value=a->held[f];
        if(f)value+=divide_signed(a->deviation[f-1]*(int)amount*a->limits[f-1],2000u);
        if(f==3 || f==2 || f==1){unsigned n=f==3?0u:f==2?1u:2u;value+=divide_signed(a->accent_delta[n]*(int)accent,100u);}
        if(f==3 || f==2){unsigned n=f==3?0u:1u;
            if(a->envelope_age<a->decay[n])value+=divide_signed(a->envelope[n]*(int)(a->decay[n]-a->envelope_age),a->decay[n]);}
        a->output[f+1]=bounded(value);
    }
    a->output[0]=0;
    if(a->event && a->age>=a->lead) {
        unsigned elapsed=a->age-a->lead;
        if(elapsed/a->spacing<a->ratchets && elapsed%a->spacing<a->pulse)a->output[0]=1023;
    }
}
static uint32_t hash(uint32_t x){x^=x<<13;x^=x>>17;x^=x<<5;return x;}
static void schedule(BiaApp *a,unsigned count,unsigned duration) {
    if(duration<3)duration=3;
    a->lead=a->patch.delay_ms;if(a->lead>duration-2u)a->lead=duration-2u;
    a->pulse=a->patch.pulse_ms;if(a->pulse>duration-a->lead-1u)a->pulse=duration-a->lead-1u;
    unsigned maximum=(duration-a->lead)/(a->pulse+1u);if(!maximum)maximum=1;
    a->ratchets=count>maximum?maximum:count;
    a->spacing=(duration-a->lead)/a->ratchets;a->age=0;a->envelope_age=0;a->duration=duration;a->event=true;
}
static void begin(BiaApp *a,unsigned scene,unsigned count,unsigned duration) {
    unsigned amount=a->patch.variation;
    if(a->patch.cv_macro && (a->cv.valid&(1u<<(a->patch.cv_macro-1u)))) {
        int modulation=a->cv.normalized[a->patch.cv_macro-1u]*100/NF_CV_Q+(int)amount;
        amount=modulation<0?0:modulation>100?100:(unsigned)modulation;
    }
    a->variation=(uint8_t)amount;a->scene=(uint8_t)scene;
    const BiaMotion *m=&a->patch.motion[scene];
    for(unsigned f=0;f<7;++f)a->held[f]=a->patch.scene[scene][f];
    for(unsigned f=0;f<6;++f){
        /* Legacy draws are unchanged when base VAR is nonzero. At VAR=0,
           use a separate deterministic stream so performance cannot change
           the base sequence's probability decisions. */
        uint32_t r=amount?random_next(a):hash(a->seed^((f+1u)*0x9e3779b9u));
        a->deviation[f]=(int16_t)((int)(r%411u)-205);a->limits[f]=m->variation[f];
    }
    for(unsigned f=0;f<3;++f)a->accent_delta[f]=(int16_t)divide_signed(m->accent[f]*1023,100u);
    for(unsigned f=0;f<2;++f){a->envelope[f]=(int16_t)divide_signed(m->envelope[f]*1023,100u);a->decay[f]=(uint16_t)(m->decay[f]*5u);}
    a->retrigger=m->retrigger;schedule(a,count,duration);
}
static void step_event(BiaApp *a,unsigned increment) {
    BiaStep s=a->patch.steps[a->index];uint32_t chance=random_next(a);
    a->event=false;a->ratchets=0;a->fill_event=false;a->fill_boost=false;
    bool fill=a->performance && a->effect==BIA_FILL && (a->patch.fill_mask&(1u<<a->index));
    bool extra=false;
    if(fill && !s.enabled && a->amount){
        unsigned rank=0,total=0;for(unsigned i=0;i<16;++i)if((a->patch.fill_mask&(1u<<i)) && !a->patch.steps[i].enabled){if(i<a->index)++rank;++total;}
        extra=total && a->amount>=1u+rank*50u/total;
    }
    if((s.enabled || extra) && (s.probability==100 || chance%100u<s.probability)) {
        unsigned count=s.ratchets;
        if(fill && a->amount>50){unsigned roll=1u+((a->amount-50u)*3u+49u)/50u;if(roll>count)count=roll;}
        if(!increment)increment=a->patch.external && a->sync.locked?nf_sync_bpm(&a->sync):a->patch.bpm;
        uint32_t seed=a->seed;begin(a,s.scene,count,(span(a)+increment-1u)/increment);
        if(extra)a->seed=seed; /* Added events never perturb future base chance draws. */
        a->accent=a->patch.step_accent[a->index];a->base_ratchets=s.ratchets;a->fill_event=extra;a->fill_boost=count>s.ratchets;
    }
}
static void remove_fill(BiaApp *a) {
    if(a->fill_event){a->event=false;a->ratchets=0;}
    else if(a->fill_boost){
        if(a->age<a->lead){unsigned age=a->age;schedule(a,a->base_ratchets,a->duration);a->age=age;}
        else {a->event=false;a->ratchets=0;}
    }
    a->fill_event=false;a->fill_boost=false;
}
static void reset(BiaApp *a,unsigned increment) {
    a->phase=0;a->index=0;a->seed=0x6b8b4567u;a->auditioning=false;
    for(unsigned f=0;f<7;++f)a->held[f]=0;
    a->variation=0;a->accent=0;a->envelope_age=500;
    for(unsigned f=0;f<6;++f){a->deviation[f]=0;a->limits[f]=0;}
    for(unsigned f=0;f<3;++f)a->accent_delta[f]=0;
    for(unsigned f=0;f<2;++f){a->envelope[f]=0;a->decay[f]=0;}
    step_event(a,increment);
}
static bool enabled(const BiaApp *a) {return a->requested && (a->patch.run_mode!=NF_RUN_GATE || a->run_high);}
void bia_init(BiaApp *a,uint8_t raw,uint32_t now) {
    *a=(BiaApp){0};bia_ui_init(&a->ui,raw,now);a->patch=a->ui.patch;nf_sync_init(&a->sync,nf_ppqn_values[a->patch.ppqn_index]);nf_cv_init(&a->cv);a->seed=0x6b8b4567u;
}
void bia_sync(BiaApp *a) {
    BiaPatch p=a->ui.patch;
    bool changed=p.external!=a->patch.external || p.ppqn_index!=a->patch.ppqn_index;
    bool old_fill=a->performance && a->effect==BIA_FILL;
    a->patch=p;a->requested=a->ui.running;
    a->performance=a->ui.performance && a->requested && a->running;
    a->effect=p.effect;a->amount=p.effect==BIA_FILL?(uint8_t)bia_fill_strength(bia_fill_level(p.strength[p.effect])):p.strength[p.effect];
    if(old_fill && !(a->performance && a->effect==BIA_FILL))remove_fill(a);
    nf_cv_configure(&a->cv,p.swapped!=0,p.calibration);nf_cv_normalize(&a->cv);
    if(changed) {nf_sync_init(&a->sync,nf_ppqn_values[p.ppqn_index]);a->sync.last_tick_us=a->now_us;a->running=false;zero(a);}
    if(a->cancel_seen!=a->ui.cancel_count) {a->cancel_seen=a->ui.cancel_count;zero(a);}
    if(a->reset_seen!=a->ui.reset_count) {
        a->reset_seen=a->ui.reset_count;cancel_performance(a);if(p.external)nf_sync_rearm(&a->sync);
        if(a->running)reset(a,p.bpm);else zero(a);
    }
    nf_sync_enable(&a->sync,enabled(a));
    bool run=enabled(a) && (!p.external || a->sync.active);
    if(run!=a->running) {a->running=run;if(run)reset(a,p.external?nf_sync_bpm(&a->sync):p.bpm);else zero(a);}
    if(a->audition_seen!=a->ui.audition_count) {
        a->audition_seen=a->ui.audition_count;
        if(!a->requested) {a->auditioning=true;a->accent=0;a->fill_event=false;a->fill_boost=false;begin(a,a->ui.audition_scene,1,1000);}
    }
    publish(a);(void)bia_refresh(a);
}
void bia_cv(BiaApp *a,unsigned x,unsigned y,unsigned valid) {a->adc[0]=(uint16_t)(x>4095?4095:x);a->adc[1]=(uint16_t)(y>4095?4095:y);a->adc_valid=(uint8_t)(valid&3u);}
void bia_input(BiaApp *a,unsigned kind,uint32_t us) {
    if(kind==NF_INPUT_CLOCK && a->patch.external) {
        if(a->reset_input){a->reset_input=false;cancel_performance(a);nf_sync_rearm(&a->sync);a->running=false;zero(a);}
        nf_sync_edge(&a->sync,us);
    } else if(kind==NF_INPUT_RUN_HIGH || kind==NF_INPUT_RUN_LOW) {
        bool high=kind==NF_INPUT_RUN_HIGH;
        if(high && !a->run_high && a->patch.run_mode==NF_RUN_RESET)a->reset_input=true;
        a->run_high=high;nf_sync_enable(&a->sync,enabled(a));
    }
}
void bia_tick(BiaApp *a,uint32_t us) {
    bool reset_now=false;
    a->now_us=us;nf_cv_sample(&a->cv,a->adc[0],a->adc[1],a->adc_valid);nf_cv_normalize(&a->cv);
    if(a->reset_input) {
        a->reset_input=false;cancel_performance(a);if(a->patch.external){nf_sync_rearm(&a->sync);a->running=false;zero(a);}else if(a->running){reset(a,a->patch.bpm);reset_now=true;}
    }
    nf_sync_enable(&a->sync,enabled(a));unsigned increment=a->patch.external?nf_sync_tick(&a->sync,us):a->patch.bpm;
    bool run=enabled(a) && (!a->patch.external || a->sync.active),started=reset_now;
    if(run!=a->running || (run && a->sync.restart)) {a->sync.restart=false;a->running=run;if(run){reset(a,increment);started=true;}else zero(a);}
    if(a->running && !started) {
        ++a->age;a->phase+=increment;
        while(a->phase>=span(a)) {a->phase-=span(a);a->index=(a->index+1u)%16u;step_event(a,increment);}
    } else if(a->auditioning) {
        /* Keep audition CV held until Stop or another audition; only one pulse. */
        if(a->age<2000u)++a->age;
    }
    if(a->running || a->auditioning){
        if(a->event && a->age>=a->lead){unsigned elapsed=a->age-a->lead;
            if((a->retrigger && elapsed/a->spacing<a->ratchets && elapsed%a->spacing==0) || elapsed==0)a->envelope_age=0;
            else if(a->envelope_age<500u)++a->envelope_age;
        } else if(a->age>=a->lead && a->envelope_age<500u)++a->envelope_age;
    }
    publish(a);
}
bool bia_refresh(BiaApp *a) {
    unsigned status=a->patch.external?(a->sync.locked?2u:a->sync.lost?3u:1u):0;
    if(a->requested && a->patch.run_mode==NF_RUN_GATE && !a->run_high)status=4;
    unsigned bpm=a->patch.external?nf_sync_bpm(&a->sync):0;
    bool dirty=a->ui.active!=a->running || a->ui.auditioning!=a->auditioning || a->ui.playing_step!=a->index || a->ui.sync_status!=status || a->ui.effective_ratchets!=a->ratchets || a->ui.external_bpm!=bpm;
    a->ui.active=a->running;a->ui.auditioning=a->auditioning;a->ui.playing_step=a->index;a->ui.sync_status=status;a->ui.effective_ratchets=a->ratchets;a->ui.external_bpm=bpm;
    for(unsigned i=0;i<2;++i)a->ui.cv_raw[i]=a->cv.raw[i];a->ui.cv_valid=a->cv.raw_valid;
    return dirty;
}
