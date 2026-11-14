/* SPDX-License-Identifier: MIT
 * Explicit little-endian wire format: never persist a padded C structure.
 */
#include "settings.h"
static void put16(uint8_t *p,unsigned value) {p[0]=(uint8_t)value;p[1]=(uint8_t)(value>>8);}
static unsigned get16(const uint8_t *p) {return p[0]|((unsigned)p[1]<<8);}
void nf_settings_capture(const OriginalUi *u,NfSettings *s) {
    s->bpm=(uint16_t)u->bpm;s->external=u->external;s->ppqn_index=u->ppqn_index;s->run_mode=u->run_mode;s->swapped=u->cv_swapped;s->muted=u->muted;
    for(unsigned i=0;i<2;++i)s->calibration[i]=u->calibration[i];
    for(unsigned i=0;i<8;++i) {s->outputs[i]=u->outputs[i];s->rhythms[i]=u->rhythms[i];s->grooves[i]=u->grooves[i];s->tones[i]=u->tones[i];for(unsigned f=0;f<NF_FIELDS;++f)s->routes[i][f]=u->routes[i][f];}
}
void nf_settings_apply(OriginalUi *u,const NfSettings *s) {
    u->bpm=s->bpm;u->external=s->external!=0;u->ppqn_index=s->ppqn_index;u->run_mode=s->run_mode;u->cv_swapped=s->swapped!=0;u->muted=s->muted;
    for(unsigned i=0;i<2;++i)u->calibration[i]=s->calibration[i];
    for(unsigned i=0;i<8;++i) {u->outputs[i]=s->outputs[i];u->rhythms[i]=s->rhythms[i];u->grooves[i]=s->grooves[i];u->tones[i]=s->tones[i];for(unsigned f=0;f<NF_FIELDS;++f)u->routes[i][f]=s->routes[i][f];}
    u->running=false;u->active=false;u->editing=false;++u->reset_count;
}
bool nf_settings_decode_size(NfSettings *s,const uint8_t *p,unsigned length) {
    if(length!=NF_SETTINGS_BYTES && length!=NF_LEGACY_SETTINGS_BYTES && length!=NF_V5_SETTINGS_BYTES && length!=NF_V6_SETTINGS_BYTES && length!=NF_V7_SETTINGS_BYTES)return false;
    if(length==NF_SETTINGS_BYTES && p[229])return false;
    s->muted=length==NF_SETTINGS_BYTES?p[228]:0;
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
        s->rhythms[i]=NF_RHYTHM_DEFAULT;s->grooves[i]=NF_GROOVE_DEFAULT;s->tones[i]=NF_TONE_DEFAULT;
        if(length!=NF_LEGACY_SETTINGS_BYTES) {
            const uint8_t *r=length==NF_V5_SETTINGS_BYTES?p+208+i*3:p+172+i*5;uint32_t bits=r[0]|((uint32_t)r[1]<<8)|((uint32_t)r[2]<<16);
            NfRhythm rhythm={(uint8_t)((bits&31u)+1u),(uint8_t)((bits>>5)&63u),(uint8_t)((bits>>11)&31u),(uint8_t)((bits>>16)&127u),(uint8_t)(bits>>23)};
            if(rhythm.hits>rhythm.steps || rhythm.rotate>=rhythm.steps || rhythm.probability>100)return false;
            s->rhythms[i]=rhythm;
            if(length==NF_SETTINGS_BYTES || length==NF_V7_SETTINGS_BYTES || length==NF_V6_SETTINGS_BYTES) {
                if(r[3]>50 || r[4]<1 || r[4]>8)return false;
                s->grooves[i]=(NfGroove){r[3],r[4]};
            }
        }
        if(length==NF_SETTINGS_BYTES || length==NF_V7_SETTINGS_BYTES) {
            unsigned bits=get16(p+212+i*2);
            NfTone t={(uint8_t)(bits&7u),(uint8_t)((bits>>3)&15u),(uint8_t)(((bits>>7)&3u)+1u),(uint8_t)(bits>>9)};
            if(t.scale>=NF_SCALES || t.root>11 || t.slew>100)return false;s->tones[i]=t;
        }
        for(unsigned f=0;f<NF_FIELDS;++f) {
            unsigned source,a,b;
            if(length==NF_SETTINGS_BYTES || length==NF_V7_SETTINGS_BYTES || length==NF_V6_SETTINGS_BYTES) {
                unsigned bit=(i*NF_FIELDS+f)*18,at=64+bit/8,shift=bit&7u;
                uint32_t v=((uint32_t)p[at]|((uint32_t)p[at+1]<<8)|((uint32_t)p[at+2]<<16))>>shift;
                source=v&3u;a=(v>>2)&255u;b=(v>>10)&255u;
            } else {const uint8_t *r=p+64+(i*NF_FIELDS+f)*3;source=r[0];a=r[1];b=r[2];}
            int depth=a<128?(int)a:(int)a-256,bias=b<128?(int)b:(int)b-256;
            if(source>2 || depth< -100 || depth>100 || bias< -100 || bias>100)return false;
            s->routes[i][f]=(NfCvRoute){(uint8_t)source,(int8_t)depth,(int8_t)bias};
        }
    }
    return true;
}
bool nf_settings_decode(NfSettings *s,const uint8_t *p) {return nf_settings_decode_size(s,p,NF_SETTINGS_BYTES);}
bool nf_settings_encode(const NfSettings *s,uint8_t *p) {
    for(unsigned i=0;i<NF_SETTINGS_BYTES;++i)p[i]=0;
    p[228]=s->muted;put16(p,s->bpm);p[2]=s->external;p[3]=s->ppqn_index;p[4]=s->run_mode;p[5]=s->swapped;
    for(unsigned i=0;i<2;++i) {put16(p+8+i*4,s->calibration[i].zero);put16(p+10+i*4,s->calibration[i].positive);}
    for(unsigned i=0;i<8;++i) {
        NfRhythm rhythm=s->rhythms[i];if(!rhythm.steps)rhythm=NF_RHYTHM_DEFAULT;
        if(rhythm.steps>32 || rhythm.hits>rhythm.steps || rhythm.rotate>=rhythm.steps || rhythm.probability>100 || rhythm.loop>1)return false;
        uint32_t bits=(rhythm.steps-1u)|((uint32_t)rhythm.hits<<5)|((uint32_t)rhythm.rotate<<11)|((uint32_t)rhythm.probability<<16)|((uint32_t)rhythm.loop<<23);
        for(unsigned j=0;j<3;++j)p[172+i*5+j]=(uint8_t)(bits>>(8*j));
        NfGroove g=s->grooves[i];if(!g.ratchet)g=NF_GROOVE_DEFAULT;
        if(g.swing>50 || g.ratchet>8)return false;
        p[175+i*5]=g.swing;p[176+i*5]=g.ratchet;
        NfTone t=s->tones[i];if(!t.range)t=NF_TONE_DEFAULT;
        if(t.scale>=NF_SCALES || t.root>11 || t.range>4 || t.slew>100)return false;
        put16(p+212+i*2,t.scale|((unsigned)t.root<<3)|((unsigned)(t.range-1u)<<7)|((unsigned)t.slew<<9));
        const NfConfig *c=&s->outputs[i];unsigned offset=16+i*6;
        p[offset]=c->ratio;p[offset+1]=c->shape;p[offset+2]=c->width;p[offset+3]=c->phase;p[offset+4]=c->level;p[offset+5]=c->offset;
        for(unsigned f=0;f<NF_FIELDS;++f) {
            NfCvRoute r=s->routes[i][f];if(r.source>2 || r.depth< -100 || r.depth>100 || r.bias< -100 || r.bias>100)return false;
            unsigned bit=(i*NF_FIELDS+f)*18,at=64+bit/8;
            uint32_t v=((uint32_t)r.source|((uint32_t)(uint8_t)r.depth<<2)|((uint32_t)(uint8_t)r.bias<<10))<<(bit&7u);
            for(unsigned j=0;j<3;++j)p[at+j]|=(uint8_t)(v>>(j*8));
        }
    }
    NfSettings check;return nf_settings_decode(&check,p);
}
