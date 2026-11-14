/* SPDX-License-Identifier: MIT
 * Explicit little-endian wire format: never persist a padded C structure.
 */
#include "settings.h"
static void put16(uint8_t *p,unsigned value) {p[0]=(uint8_t)value;p[1]=(uint8_t)(value>>8);}
static unsigned get16(const uint8_t *p) {return p[0]|((unsigned)p[1]<<8);}
void nf_settings_capture(const OriginalUi *u,NfSettings *s) {
    s->bpm=(uint16_t)u->bpm;s->external=u->external;s->ppqn_index=u->ppqn_index;s->run_mode=u->run_mode;s->swapped=u->cv_swapped;
    for(unsigned i=0;i<2;++i)s->calibration[i]=u->calibration[i];
    for(unsigned i=0;i<8;++i) {s->outputs[i]=u->outputs[i];for(unsigned f=0;f<NF_FIELDS;++f)s->routes[i][f]=u->routes[i][f];}
}
void nf_settings_apply(OriginalUi *u,const NfSettings *s) {
    u->bpm=s->bpm;u->external=s->external!=0;u->ppqn_index=s->ppqn_index;u->run_mode=s->run_mode;u->cv_swapped=s->swapped!=0;
    for(unsigned i=0;i<2;++i)u->calibration[i]=s->calibration[i];
    for(unsigned i=0;i<8;++i) {u->outputs[i]=s->outputs[i];for(unsigned f=0;f<NF_FIELDS;++f)u->routes[i][f]=s->routes[i][f];}
    u->running=false;u->active=false;u->editing=false;++u->reset_count;
}
bool nf_settings_decode(NfSettings *s,const uint8_t *p) {
    unsigned bpm=get16(p);
    if(bpm<10 || bpm>300 || p[2]>1 || p[3]>=NF_PPQN_COUNT || p[4]>NF_RUN_RESET || p[5]>1 || p[6] || p[7])return false;
    s->bpm=(uint16_t)bpm;s->external=p[2];s->ppqn_index=p[3];s->run_mode=p[4];s->swapped=p[5];
    for(unsigned i=0;i<2;++i) {
        unsigned zero=get16(p+8+i*4),positive=get16(p+10+i*4);
        if(zero>4095 || positive>4095)return false;
        s->calibration[i]=(NfCvCalibration){(uint16_t)zero,(uint16_t)positive};
    }
    for(unsigned i=0;i<8;++i) {
        const uint8_t *v=p+16+i*6;
        if(v[0]>=NF_RATIOS || v[1]>=NF_SHAPES || v[2]<1 || v[2]>99 || v[3]>99 || v[4]>100 || v[5]>100)return false;
        s->outputs[i]=(NfConfig){v[0],v[1],v[2],v[3],v[4],v[5]};
        for(unsigned f=0;f<NF_FIELDS;++f) {
            const uint8_t *r=p+64+(i*NF_FIELDS+f)*3;
            int depth=r[1]<128?r[1]:(int)r[1]-256,bias=r[2]<128?r[2]:(int)r[2]-256;
            if(r[0]>2 || depth< -100 || depth>100 || bias< -100 || bias>100)return false;
            s->routes[i][f]=(NfCvRoute){r[0],(int8_t)depth,(int8_t)bias};
        }
    }
    return true;
}
bool nf_settings_encode(const NfSettings *s,uint8_t *p) {
    for(unsigned i=0;i<NF_SETTINGS_BYTES;++i)p[i]=0;
    put16(p,s->bpm);p[2]=s->external;p[3]=s->ppqn_index;p[4]=s->run_mode;p[5]=s->swapped;
    for(unsigned i=0;i<2;++i) {put16(p+8+i*4,s->calibration[i].zero);put16(p+10+i*4,s->calibration[i].positive);}
    for(unsigned i=0;i<8;++i) {
        const NfConfig *c=&s->outputs[i];unsigned offset=16+i*6;
        p[offset]=c->ratio;p[offset+1]=c->shape;p[offset+2]=c->width;p[offset+3]=c->phase;p[offset+4]=c->level;p[offset+5]=c->offset;
        for(unsigned f=0;f<NF_FIELDS;++f) {unsigned r=64+(i*NF_FIELDS+f)*3;p[r]=s->routes[i][f].source;p[r+1]=(uint8_t)s->routes[i][f].depth;p[r+2]=(uint8_t)s->routes[i][f].bias;}
    }
    NfSettings check;return nf_settings_decode(&check,p);
}
