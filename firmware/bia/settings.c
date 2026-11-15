/* SPDX-License-Identifier: MIT. Independent BIA scene wire format. */
#include "settings.h"
static void put16(uint8_t *p,unsigned x){p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);}
static unsigned get16(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
void nf_settings_capture(const OriginalUi *u,NfSettings *s){*s=u->patch;}
void nf_settings_apply(OriginalUi *u,const NfSettings *s){u->patch=*s;u->running=false;u->active=false;u->auditioning=false;u->editing=false;u->performance=false;u->performance_page=false;u->start_pending=false;u->start_consumed=true;++u->cancel_count;++u->reset_count;}
bool nf_settings_encode(const NfSettings *s,uint8_t *p) {
    if(!bia_patch_valid(s))return false;for(unsigned i=0;i<NF_SETTINGS_BYTES;++i)p[i]=0;
    p[0]='B';p[1]='I';p[2]='A';p[3]=2;put16(p+4,s->bpm);
    unsigned at=6;for(unsigned i=0;i<8;++i)for(unsigned f=0;f<7;++f){put16(p+at,s->scene[i][f]);at+=2;}
    for(unsigned i=0;i<16;++i){BiaStep v=s->steps[i];p[at++]=v.scene;p[at++]=v.probability;p[at++]=v.ratchets;p[at++]=v.enabled;}
    const uint8_t config[]={s->variation,s->delay_ms,s->pulse_ms,s->swing,s->external,s->ppqn_index,s->run_mode,s->cv_macro};
    for(unsigned i=0;i<8;++i)p[at++]=config[i];
    for(unsigned i=0;i<2;++i){put16(p+at,s->calibration[i].zero);put16(p+at+2,s->calibration[i].positive);at+=4;}p[at++]=s->swapped;
    for(unsigned i=0;i<16;++i)p[at++]=s->step_accent[i];put16(p+at,s->fill_mask);at+=2;
    unsigned bits=0;
    for(unsigned i=0;i<8;++i) {
        const BiaMotion *m=&s->motion[i];
        for(unsigned f=0;f<2;++f){p[at++]=(uint8_t)m->envelope[f];p[at++]=m->decay[f];}
        for(unsigned f=0;f<3;++f)p[at++]=(uint8_t)m->accent[f];
        for(unsigned f=0;f<6;++f)p[at++]=m->variation[f];
        if(m->retrigger)bits|=1u<<i;
    }
    p[at++]=(uint8_t)bits;p[at++]=s->effect;for(unsigned i=0;i<3;++i)p[at++]=s->strength[i];return true;
}
bool nf_settings_decode_size(NfSettings *s,const uint8_t *p,unsigned n) {
    if(n!=NF_SETTINGS_BYTES || p[0]!='B' || p[1]!='I' || p[2]!='A' || (p[3]!=1 && p[3]!=2))return false;
    *s=(BiaPatch){.bpm=(uint16_t)get16(p+4)};unsigned at=6;
    for(unsigned i=0;i<8;++i)for(unsigned f=0;f<7;++f){s->scene[i][f]=(uint16_t)get16(p+at);at+=2;}
    for(unsigned i=0;i<16;++i){s->steps[i]=(BiaStep){p[at],p[at+1],p[at+2],p[at+3]};at+=4;}
    s->variation=p[at++];s->delay_ms=p[at++];s->pulse_ms=p[at++];s->swing=p[at++];s->external=p[at++];s->ppqn_index=p[at++];s->run_mode=p[at++];s->cv_macro=p[at++];
    for(unsigned i=0;i<2;++i){s->calibration[i]=(NfCvCalibration){(uint16_t)get16(p+at),(uint16_t)get16(p+at+2)};at+=4;}s->swapped=p[at++];
    bia_legacy_extras(s);
    if(p[3]==2) {
        for(unsigned i=0;i<16;++i)s->step_accent[i]=p[at++];s->fill_mask=(uint16_t)get16(p+at);at+=2;
        for(unsigned i=0;i<8;++i) {
            BiaMotion *m=&s->motion[i];
            for(unsigned f=0;f<2;++f){m->envelope[f]=(int8_t)p[at++];m->decay[f]=p[at++];}
            for(unsigned f=0;f<3;++f)m->accent[f]=(int8_t)p[at++];
            for(unsigned f=0;f<6;++f)m->variation[f]=p[at++];
        }
        unsigned bits=p[at++];for(unsigned i=0;i<8;++i)s->motion[i].retrigger=(bits&(1u<<i))!=0;
        s->effect=p[at++];for(unsigned i=0;i<3;++i)s->strength[i]=p[at++];
    }
    for(;at<n;++at)if(p[at])return false;return bia_patch_valid(s);
}
bool nf_settings_decode(NfSettings *s,const uint8_t *p){return nf_settings_decode_size(s,p,NF_SETTINGS_BYTES);}
