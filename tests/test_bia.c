/* SPDX-License-Identifier: MIT. Event/transport oracles independent of UI. */
#include "bia/bia.h"
#include "store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void tick(BiaApp *a,unsigned ms){for(unsigned i=0;i<ms;++i)bia_tick(a,a->now_us+1000u);}
static void setup(BiaApp *a){bia_init(a,15,0);for(unsigned i=0;i<16;++i)a->ui.patch.steps[i]=(BiaStep){(uint8_t)(i%8),100,1,1};a->ui.running=true;bia_sync(a);}
static void scheduling(void){
    BiaApp a;setup(&a);assert(a.running && a.output[0]==0);for(unsigned f=0;f<7;++f)assert(a.output[f+1]==a.patch.scene[0][f]);
    tick(&a,2);assert(!a.output[0]);tick(&a,1);assert(a.output[0]==1023);tick(&a,2);assert(a.output[0]==1023);tick(&a,1);assert(!a.output[0]);
    tick(&a,119);assert(a.index==1 && a.age==0 && !a.output[0]);for(unsigned f=0;f<7;++f)assert(a.output[f+1]==a.patch.scene[1][f]);
    unsigned delay=a.patch.delay_ms;for(unsigned i=1;i<=delay;++i){tick(&a,1);assert(a.output[0]==(i==delay?1023:0));}
    /* UI refresh is timeless: does not age, reroll, or overwrite the snapshot. */
    uint32_t seed=a.seed;unsigned age=a.age,held=a.output[2];a.ui.patch.scene[1][1]=1023;
    for(unsigned i=0;i<100;++i)bia_sync(&a);assert(a.seed==seed && a.age==age && a.output[2]==held);
    a.ui.running=false;bia_sync(&a);for(unsigned i=0;i<8;++i)assert(a.output[i]==0);tick(&a,1000);for(unsigned i=0;i<8;++i)assert(a.output[i]==0);
    a.ui.running=true;bia_sync(&a);assert(!a.output[0]);++a.ui.reset_count;bia_sync(&a);assert(!a.output[0]);tick(&a,2);assert(!a.output[0]);
    a.ui.running=false;bia_sync(&a);tick(&a,100);assert(!a.output[0]);
}
static void ratchets(void){
    for(unsigned bpm=10;bpm<=300;bpm+=29)for(unsigned swing=0;swing<=50;swing+=25)for(unsigned lead=1;lead<=20;lead+=19)for(unsigned width=1;width<=10;width+=9)for(unsigned count=1;count<=8;++count){
        BiaApp a;bia_init(&a,15,0);a.ui.patch.bpm=(uint16_t)bpm;a.ui.patch.swing=(uint8_t)swing;a.ui.patch.delay_ms=(uint8_t)lead;a.ui.patch.pulse_ms=(uint8_t)width;
        a.ui.patch.steps[0]=(BiaStep){0,100,(uint8_t)count,1};a.ui.running=true;bia_sync(&a);
        unsigned expected=a.ratchets,rises=0,previous=0,previous_rise=0;assert(expected>=1 && expected<=count && !a.output[0]);
        assert(a.lead>=1 && a.pulse>=1 && a.spacing>a.pulse);
        while(a.index==0){tick(&a,1);if(a.output[0] && !previous){++rises;assert(a.age>=a.lead);if(rises>1)assert(a.age-previous_rise>=a.pulse+1);previous_rise=a.age;}previous=a.output[0];}
        assert(rises==expected && !a.output[0]);
    }
}
static void probability_and_variation(void){
    BiaApp a;setup(&a);uint16_t held[7];memcpy(held,a.held,sizeof held);a.ui.patch.steps[1].probability=0;bia_sync(&a);tick(&a,125);
    assert(a.index==1 && !a.event && !a.output[0]);for(unsigned i=0;i<7;++i)assert(a.output[i+1]==held[i]);
    a.ui.patch.steps[2].enabled=0;bia_sync(&a);tick(&a,125);for(unsigned i=0;i<7;++i)assert(a.output[i+1]==held[i]);
    /* Repeatable sequence on Start, bounded variation, pitch never perturbed. */
    a.ui.patch.variation=100;a.ui.running=false;bia_sync(&a);a.ui.running=true;bia_sync(&a);uint16_t varied[7];memcpy(varied,a.output+1,sizeof varied);
    assert(a.output[1]==a.patch.scene[0][0]);for(unsigned i=1;i<7;++i){int error=(int)a.output[i+1]-(int)a.patch.scene[0][i];assert(error>=-205 && error<=205);}
    tick(&a,1000);a.ui.running=false;bia_sync(&a);a.ui.running=true;bia_sync(&a);assert(!memcmp(varied,a.output+1,sizeof varied));
    a.ui.running=false;bia_sync(&a);a.ui.patch.variation=0;a.ui.patch.cv_macro=1;bia_sync(&a);bia_cv(&a,4095,2048,3);tick(&a,100);a.ui.running=true;bia_sync(&a);
    assert(!memcmp(varied,a.output+1,sizeof varied));
}
static void external(void){
    for(unsigned choice=0;choice<NF_PPQN_COUNT;++choice){
        BiaApp a;bia_init(&a,15,0);a.ui.patch.external=1;a.ui.patch.ppqn_index=(uint8_t)choice;a.ui.running=true;bia_sync(&a);assert(!a.running);
        unsigned ppqn=nf_ppqn_values[choice],next=0,n=0;
        for(unsigned ms=0;ms<2000;++ms){if(ms*1000u>=next){bia_input(&a,NF_INPUT_CLOCK,ms*1000u);next=(++n)*500000u/ppqn;}tick(&a,1);}
        assert(a.running && a.sync.locked && a.ui.running);tick(&a,2000);assert(!a.running && a.sync.lost);for(unsigned i=0;i<8;++i)assert(!a.output[i]);
        bia_input(&a,NF_INPUT_CLOCK,a.now_us);tick(&a,1);assert(!a.running);tick(&a,500/ppqn);bia_input(&a,NF_INPUT_CLOCK,a.now_us);tick(&a,1);assert(a.running && a.index==0 && a.age==0 && !a.output[0]);
    }
    BiaApp a;bia_init(&a,15,0);a.ui.patch.run_mode=NF_RUN_GATE;a.ui.running=true;bia_sync(&a);assert(!a.running);bia_input(&a,NF_INPUT_RUN_HIGH,0);tick(&a,1);assert(a.running && !a.output[0]);bia_input(&a,NF_INPUT_RUN_LOW,a.now_us);tick(&a,1);assert(!a.running);for(unsigned i=0;i<8;++i)assert(!a.output[i]);
    setup(&a);a.ui.patch.run_mode=NF_RUN_RESET;bia_sync(&a);tick(&a,300);bia_input(&a,NF_INPUT_RUN_HIGH,a.now_us);tick(&a,1);assert(a.index==0 && a.age==0 && !a.output[0]);tick(&a,2);assert(!a.output[0]);tick(&a,1);assert(a.output[0]);
}
static void audition(void){
    BiaApp a;bia_init(&a,15,0);a.ui.audition_scene=7;++a.ui.audition_count;bia_sync(&a);assert(a.auditioning && !a.running && !a.output[0]);
    unsigned rises=0,prev=0;for(unsigned i=0;i<5000;++i){tick(&a,1);if(a.output[0] && !prev)++rises;prev=a.output[0];}assert(rises==1 && a.auditioning);for(unsigned i=0;i<7;++i)assert(a.output[i+1]==a.patch.scene[7][i]);
    ++a.ui.cancel_count;bia_sync(&a);assert(!a.auditioning);for(unsigned i=0;i<8;++i)assert(!a.output[i]);
    setup(&a);uint32_t seed=a.seed;++a.ui.audition_count;bia_sync(&a);assert(!a.auditioning && a.seed==seed);
}
static uint8_t media[8192];static int budget=-1;static unsigned writes;
static bool spend(void){if(!budget)return false;if(budget>0)--budget;return true;}
static bool rd(void *c,unsigned at,void *p,unsigned n){(void)c;assert(at<=8192 && n<=8192-at);memcpy(p,media+at,n);return true;}
static bool er(void *c,unsigned bank){(void)c;for(unsigned i=0;i<4096;++i){if(!spend())return false;media[bank*4096+i]=255;}return true;}
static bool wr(void *c,unsigned at,const void *p,unsigned n){(void)c;const uint8_t *q=p;++writes;for(unsigned i=0;i<n;++i){if(!spend())return false;assert((media[at+i]&q[i])==q[i]);media[at+i]=q[i];}return true;}
static NfStoreIo io={0,rd,er,wr};
static void presets(void){
    BiaPatch p,q;bia_defaults(&p);uint8_t b[NF_SETTINGS_BYTES],again[NF_SETTINGS_BYTES];assert(nf_settings_encode(&p,b));assert(nf_settings_decode(&q,b));assert(nf_settings_encode(&q,again));assert(!memcmp(b,again,sizeof b));b[3]=3;assert(!nf_settings_decode(&q,b));b[3]=2;b[333]=1;assert(!nf_settings_decode(&q,b));
    memset(media,255,sizeof media);NfStore s;assert(nf_store_open(&s,io)==NF_STORE_EMPTY);for(unsigned i=0;i<8;++i){p.scene[i][6]=(uint16_t)(700+i);assert(nf_store_save(&s,i,&p)==NF_STORE_OK);}assert(s.mask==255);
    for(unsigned n=0;n<20;++n){p.variation=(uint8_t)n;assert(nf_store_save(&s,n%8,&p)==NF_STORE_OK);}for(unsigned i=0;i<8;++i)assert(nf_store_load(&s,i,&q)==NF_STORE_OK);
    uint8_t snapshot[8192];memcpy(snapshot,media,8192);p.variation=77;
    for(unsigned cut=0;cut<=7194;++cut){memcpy(media,snapshot,8192);budget=-1;assert(nf_store_open(&s,io)==NF_STORE_OK);budget=(int)cut;(void)nf_store_save(&s,0,&p);budget=-1;assert(nf_store_open(&s,io)==NF_STORE_OK);for(unsigned i=0;i<8;++i)assert(nf_store_load(&s,i,&q)==NF_STORE_OK);}
    /* A committed Newfangled bank blocks BIA writes, without changing media. */
    unsigned base=(unsigned)s.bank*4096u;memcpy(media+base,"NFB1",4);uint32_t crc=nf_store_crc32(media+base,20);for(unsigned i=0;i<4;++i)media[base+20+i]=(uint8_t)(crc>>(i*8));
    assert(nf_store_open(&s,io)==NF_STORE_FUTURE);unsigned before=writes;assert(nf_store_save(&s,0,&p)==NF_STORE_FUTURE && writes==before);
}
static void put16(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i));}
static void legacy_storage(void){
    memset(media,255,sizeof media);BiaPatch p,q;bia_defaults(&p);uint8_t b[NF_SETTINGS_BYTES];
    for(unsigned i=0;i<8;++i){p.bpm=(uint16_t)(100+i);p.variation=37;assert(nf_settings_encode(&p,b));b[3]=1;memset(b+199,0,NF_SETTINGS_BYTES-199);
        uint8_t *r=media+384*(i+1);memcpy(r,"BIP1",4);put16(r+4,6);put16(r+6,334);r[8]=(uint8_t)i;put32(r+12,i+1);memcpy(r+16,b,334);put32(r+376,nf_store_crc32(r,376));put16(r+382,NF_RECORD_COMMIT);
    }
    memcpy(media,"BIB1",4);put16(media+4,2);put16(media+6,384);put32(media+8,1);put16(media+12,8);put32(media+16,nf_store_crc32(media+384,8*384));put32(media+20,nf_store_crc32(media,20));put16(media+4094,NF_BANK_COMMIT);
    NfStore s;assert(nf_store_open(&s,io)==NF_STORE_OK && s.mask==255);
    for(unsigned i=0;i<8;++i){assert(nf_store_load(&s,i,&q)==NF_STORE_OK && q.bpm==100+i && q.variation==37 && !q.motion[0].envelope[0] && q.motion[0].variation[5]==20);}
    BiaUi u;bia_ui_init(&u,15,0);assert(nf_presets_boot(&s,&u) && u.patch.bpm==107 && !u.running && !u.performance);
    p.bpm=179;p.motion[0].envelope[0]=50;p.step_accent[0]=73;assert(nf_store_save(&s,3,&p)==NF_STORE_OK);assert(nf_store_save(&s,3,&p)==NF_STORE_OK); /* compaction migrates remaining v0.1 records */
    for(unsigned i=0;i<8;++i){assert(nf_store_load(&s,i,&q)==NF_STORE_OK);if(i==3)assert(q.bpm==179 && q.motion[0].envelope[0]==50 && q.step_accent[0]==73);else assert(q.bpm==100+i && q.motion[0].variation[5]==20 && !q.motion[0].envelope[0]);}
}
static void ui(void){
    BiaUi u;bia_ui_init(&u,15,0);assert(bia_patch_valid(&u.patch));
    for(unsigned page=0;page<25;++page){u.page=page;u.selecting=true;for(unsigned field=0;field<(page==0?16u:page<=16?6u:22u);++field){u.field=field;bia_ui_render(&u);}}
}
int main(void){scheduling();ratchets();probability_and_variation();external();audition();presets();legacy_storage();ui();puts("BIA: scene-before-trigger, ratchet bounds, shared decisions, variation, sync/Run, audition, 8-slot storage/power cuts and OLED passed.");}
