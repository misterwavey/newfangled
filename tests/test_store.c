/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "store.h"
static unsigned writes,erases;static int budget=-1;
static uint8_t media[NF_STORE_BYTES],snapshot[NF_STORE_BYTES];
static bool spend(void) {if(budget==0)return false;if(budget>0)--budget;return true;}
static bool read_media(void *c,unsigned at,void *p,unsigned n) {(void)c;assert(at<=sizeof media && n<=sizeof media-at);memcpy(p,media+at,n);return true;}
static bool erase_media(void *c,unsigned bank) {(void)c;assert(bank<2);++erases;for(unsigned i=0;i<NF_BANK_BYTES;++i){if(!spend())return false;media[bank*NF_BANK_BYTES+i]=255;}return true;}
static bool program_media(void *c,unsigned at,const void *p,unsigned n) {(void)c;assert(!(at&1) && !(n&1));assert(at<=sizeof media && n<=sizeof media-at);const uint8_t *b=p;++writes;for(unsigned i=0;i<n;++i){if(!spend())return false;assert((media[at+i]&b[i])==b[i]);media[at+i]=b[i];}return true;}
static NfStoreIo io={0,read_media,erase_media,program_media};
static void blank(void) {memset(media,255,sizeof media);budget=-1;writes=erases=0;}
static NfSettings settings(unsigned bpm) {
    OriginalUi u={.bpm=bpm,.ppqn_index=6,.calibration={{83,3900},{2100,127}},.cv_swapped=true};
    for(unsigned i=0;i<8;++i){u.outputs[i]=(NfConfig){i,4,1,99,100,100};for(unsigned f=0;f<NF_FIELDS;++f)u.routes[i][f]=(NfCvRoute){2,-100,100};}
    NfSettings s;nf_settings_capture(&u,&s);return s;
}
static void codec(void) {
    assert(nf_store_crc32("123456789",9)==0xcbf43926u);
    NfSettings s=settings(173),loaded;uint8_t bytes[NF_SETTINGS_BYTES],again[NF_SETTINGS_BYTES];
    assert(nf_settings_encode(&s,bytes));assert(nf_settings_decode(&loaded,bytes));assert(nf_settings_encode(&loaded,again));assert(!memcmp(bytes,again,sizeof bytes));
    assert(loaded.routes[7][5].depth==-100 && loaded.calibration[1].positive==127);
    bytes[2]=2;assert(!nf_settings_decode(&loaded,bytes));bytes[2]=0;bytes[18]=0;assert(!nf_settings_decode(&loaded,bytes));
}
static void lifecycle(void) {
    blank();NfStore s;assert(nf_store_open(&s,io)==NF_STORE_EMPTY);OriginalUi u={.bpm=120};assert(!nf_presets_boot(&s,&u));assert(!writes && !erases);
    for(unsigned slot=0;slot<8;++slot){NfSettings p=settings(100+slot);assert(nf_store_save(&s,slot,&p)==NF_STORE_OK);}
    assert(s.mask==255 && s.last_slot==7);NfSettings p;
    for(unsigned slot=0;slot<8;++slot){assert(nf_store_load(&s,slot,&p)==NF_STORE_OK && p.bpm==100+slot);}
    u.running=true;u.preset_command=NF_PRESET_SAVE;unsigned before=writes;
    assert(!nf_presets_execute(&s,&u,true));assert(u.preset_status==NF_PRESET_STOP && writes==before);
    u.running=false;u.active=true;u.preset_command=NF_PRESET_LOAD;assert(!nf_presets_execute(&s,&u,true));assert(u.bpm==120);
    u.active=false;u.preset_slot=1;u.preset_command=NF_PRESET_LOAD;assert(nf_presets_execute(&s,&u,true));assert(u.bpm==101 && !u.running);
    u.running=true;assert(nf_presets_boot(&s,&u));assert(!u.running && u.bpm==107 && u.preset_slot==7);
    for(unsigned i=0;i<60;++i){p=settings(120+i);assert(nf_store_save(&s,i%8,&p)==NF_STORE_OK);}
    assert(erases>2);assert(nf_store_open(&s,io)==NF_STORE_OK && s.mask==255);
    for(unsigned slot=0;slot<8;++slot){assert(nf_store_load(&s,slot,&p)==NF_STORE_OK);unsigned expected=slot<4?176+slot:168+slot;assert(p.bpm==expected);}
    /* A valid unknown schema blocks writes instead of destroying newer data. */
    unsigned at=(unsigned)s.bank*NF_BANK_BYTES;media[at+4]=2;uint32_t crc=nf_store_crc32(media+at,20);for(unsigned i=0;i<4;++i)media[at+20+i]=(uint8_t)(crc>>(8*i));
    assert(nf_store_open(&s,io)==NF_STORE_FUTURE);before=writes;assert(nf_store_save(&s,0,&p)==NF_STORE_FUTURE && writes==before);
}
static void cuts(bool compact) {
    blank();NfStore s;assert(nf_store_open(&s,io)==NF_STORE_EMPTY);NfSettings old=settings(120),next=settings(179),got;
    assert(nf_store_save(&s,0,&old)==NF_STORE_OK);
    if(compact){for(unsigned i=1;i<NF_BANK_RECORDS;++i)assert(nf_store_save(&s,i%8,&old)==NF_STORE_OK);}
    memcpy(snapshot,media,sizeof media);unsigned limit=compact?NF_BANK_BYTES+8*NF_RECORD_BYTES+26:NF_RECORD_BYTES;
    /* Every byte boundary, including a torn erase and torn commit halfword. */
    for(unsigned cut=0;cut<=limit;++cut){
        memcpy(media,snapshot,sizeof media);budget=-1;assert(nf_store_open(&s,io)==NF_STORE_OK);budget=(int)cut;
        NfStoreResult r=nf_store_save(&s,0,&next);budget=-1;assert(nf_store_open(&s,io)==NF_STORE_OK);assert(nf_store_load(&s,0,&got)==NF_STORE_OK);
        assert(got.bpm==old.bpm || got.bpm==next.bpm);if(r==NF_STORE_OK)assert(got.bpm==next.bpm);
        /* Recovery must remain writable; a torn append occupies its record. */
        assert(nf_store_save(&s,1,&next)==NF_STORE_OK);assert(nf_store_load(&s,0,&got)==NF_STORE_OK);
        if(compact)for(unsigned slot=2;slot<8;++slot){assert(nf_store_load(&s,slot,&got)==NF_STORE_OK && got.bpm==120);}
    }
}
int main(void) {codec();lifecycle();cuts(false);cuts(true);puts("Preset codec, 8 slots, stopped-only lifecycle and 6428 power-cut boundaries passed.");}
