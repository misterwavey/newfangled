/* SPDX-License-Identifier: MIT
 * Two-bank append journal. Checkpoint CRC + bank footer make bank replacement
 * transactional; record CRC + final halfword make each append transactional.
 */
#include "store.h"
static unsigned u16(const uint8_t *p) {return p[0]|((unsigned)p[1]<<8);}
static uint32_t u32(const uint8_t *p) {return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void p16(uint8_t *p,unsigned v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void p32(uint8_t *p,uint32_t v) {for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(i*8));}
static bool magic(const uint8_t *p,const char *s) {for(unsigned i=0;i<4;++i)if(p[i]!=(uint8_t)s[i])return false;return true;}
static void fill(uint8_t *p,unsigned n) {for(unsigned i=0;i<n;++i)p[i]=255;}
static bool newer(uint32_t a,uint32_t b) {uint32_t d=a-b;return d && d<0x80000000u;}
static uint32_t crc_update(uint32_t crc,const uint8_t *p,unsigned n) {
    while(n--) {crc^=*p++;for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1u)?0xedb88320u:0u);}
    return crc;
}
uint32_t nf_store_crc32(const void *p,unsigned n) {return ~crc_update(0xffffffffu,p,n);}
static bool read_at(NfStore *s,unsigned offset,void *p,unsigned n) {return offset<=NF_STORE_BYTES && n<=NF_STORE_BYTES-offset && s->io.read(s->io.context,offset,p,n);}
static bool program_at(NfStore *s,unsigned offset,const void *p,unsigned n) {
    return !(offset&1u) && !(n&1u) && offset<=NF_STORE_BYTES && n<=NF_STORE_BYTES-offset && s->io.program(s->io.context,offset,p,n);
}
static unsigned address(unsigned bank,unsigned record) {return bank*NF_BANK_BYTES+(record+1u)*NF_RECORD_BYTES;}
static bool record_valid(NfStore *s,NfSettings *settings) {
    uint8_t *p=s->buffer;
    if(!magic(p,"NFP1") || u16(p+254)!=NF_RECORD_COMMIT || u32(p+248)!=nf_store_crc32(p,248))return false;
    if(u16(p+4)!=NF_STORE_SCHEMA) {s->future=true;return false;}
    if(u16(p+6)!=NF_SETTINGS_BYTES || p[8]>=NF_PRESET_SLOTS)return false;
    return nf_settings_decode(settings,p+16);
}
static int bank_valid(NfStore *s,unsigned bank,uint32_t *generation) {
    unsigned base=bank*NF_BANK_BYTES;uint8_t header[24],footer[2];
    if(!read_at(s,base,header,24) || !read_at(s,base+NF_BANK_BYTES-2,footer,2))return -1;
    if(!magic(header,"NFB1") || u16(footer)!=NF_BANK_COMMIT || u32(header+20)!=nf_store_crc32(header,20))return 0;
    if(u16(header+4)!=NF_STORE_SCHEMA) {s->future=true;return 0;}
    unsigned count=u16(header+12);
    if(u16(header+6)!=NF_RECORD_BYTES || count>NF_PRESET_SLOTS)return 0;
    uint32_t crc=0xffffffffu;
    for(unsigned i=0;i<count;++i) {if(!read_at(s,address(bank,i),s->buffer,NF_RECORD_BYTES))return -1;crc=crc_update(crc,s->buffer,NF_RECORD_BYTES);}
    if(~crc!=u32(header+16))return 0;
    *generation=u32(header+8);return 1;
}
NfStoreResult nf_store_open(NfStore *s,NfStoreIo io) {
    *s=(NfStore){.io=io,.bank=-1};
    if(!io.read || !io.erase || !io.program)return NF_STORE_IO;
    for(unsigned bank=0;bank<2;++bank) {
        uint32_t generation=0;int valid=bank_valid(s,bank,&generation);
        if(valid<0)return NF_STORE_IO;
        if(valid && (s->bank<0 || newer(generation,s->generation))) {s->bank=(int8_t)bank;s->generation=generation;}
    }
    if(s->bank<0)return s->future?NF_STORE_FUTURE:NF_STORE_EMPTY;
    NfSettings scratch;bool found=false;
    for(unsigned i=0;i<NF_BANK_RECORDS;++i) {
        unsigned at=address((unsigned)s->bank,i);
        if(!read_at(s,at,s->buffer,NF_RECORD_BYTES))return NF_STORE_IO;
        if(!record_valid(s,&scratch))continue;
        unsigned slot=s->buffer[8];uint32_t sequence=u32(s->buffer+12);
        if(!(s->mask&(1u<<slot)) || newer(sequence,s->slot_sequence[slot])) {
            s->slot_sequence[slot]=sequence;s->slot_address[slot]=(uint16_t)at;s->mask|=(uint8_t)(1u<<slot);
        }
        if(!found || newer(sequence,s->sequence)) {s->sequence=sequence;s->last_slot=(uint8_t)slot;found=true;}
    }
    return s->future?NF_STORE_FUTURE:s->mask?NF_STORE_OK:NF_STORE_EMPTY;
}
NfStoreResult nf_store_load(NfStore *s,unsigned slot,NfSettings *settings) {
    if(slot>=NF_PRESET_SLOTS)return NF_STORE_INVALID;
    if(!(s->mask&(1u<<slot)))return NF_STORE_EMPTY;
    if(!read_at(s,s->slot_address[slot],s->buffer,NF_RECORD_BYTES))return NF_STORE_IO;
    return record_valid(s,settings) && s->buffer[8]==slot?NF_STORE_OK:NF_STORE_CORRUPT;
}
static bool make_record(NfStore *s,unsigned slot,uint32_t sequence,const NfSettings *settings) {
    fill(s->buffer,NF_RECORD_BYTES);uint8_t *p=s->buffer;
    p[0]='N';p[1]='F';p[2]='P';p[3]='1';p16(p+4,NF_STORE_SCHEMA);p16(p+6,NF_SETTINGS_BYTES);p[8]=(uint8_t)slot;p32(p+12,sequence);
    if(!nf_settings_encode(settings,p+16))return false;
    p32(p+248,nf_store_crc32(p,248));p16(p+254,NF_RECORD_COMMIT);return true;
}
static bool write_record(NfStore *s,unsigned at) {
    if(!program_at(s,at,s->buffer,254))return false;
    return program_at(s,at+254,s->buffer+254,2);
}
NfStoreResult nf_store_save(NfStore *s,unsigned slot,const NfSettings *settings) {
    if(slot>=NF_PRESET_SLOTS)return NF_STORE_INVALID;
    NfStoreIo io=s->io;NfStoreResult opened=nf_store_open(s,io);
    if(opened==NF_STORE_IO || s->future)return s->future?NF_STORE_FUTURE:NF_STORE_IO;
    if(!nf_settings_encode(settings,s->buffer))return NF_STORE_INVALID;
    uint32_t sequence=s->sequence+1u;
    if(s->bank>=0) {
        for(unsigned i=0;i<NF_BANK_RECORDS;++i) {
            unsigned at=address((unsigned)s->bank,i);bool erased=true;
            if(!read_at(s,at,s->buffer,NF_RECORD_BYTES))return NF_STORE_IO;
            for(unsigned j=0;j<NF_RECORD_BYTES;++j)if(s->buffer[j]!=255) {erased=false;break;}
            if(!erased)continue;
            if(!make_record(s,slot,sequence,settings) || !write_record(s,at))return NF_STORE_IO;
            NfStoreResult scan=nf_store_open(s,io);
            return scan==NF_STORE_OK && (s->mask&(1u<<slot)) && s->slot_sequence[slot]==sequence?NF_STORE_OK:NF_STORE_IO;
        }
    }
    /* Build a complete replacement checkpoint in the inactive bank. The old
       bank stays intact until the new header, checkpoint and footer validate. */
    unsigned target=s->bank<0?0u:((unsigned)s->bank^1u),count=0;uint32_t crc=0xffffffffu,generation=s->generation+1u;
    if(!s->io.erase(s->io.context,target))return NF_STORE_IO;
    for(unsigned index=0;index<NF_PRESET_SLOTS;++index) {
        if(index==slot) {if(!make_record(s,slot,sequence,settings))return NF_STORE_INVALID;}
        else {
            if(!(s->mask&(1u<<index)))continue;
            if(!read_at(s,s->slot_address[index],s->buffer,NF_RECORD_BYTES))return NF_STORE_IO;
            NfSettings check;if(!record_valid(s,&check) || s->buffer[8]!=index)return NF_STORE_CORRUPT;
        }
        crc=crc_update(crc,s->buffer,NF_RECORD_BYTES);
        if(!write_record(s,address(target,count++)))return NF_STORE_IO;
    }
    uint8_t header[24];fill(header,24);header[0]='N';header[1]='F';header[2]='B';header[3]='1';
    p16(header+4,NF_STORE_SCHEMA);p16(header+6,NF_RECORD_BYTES);p32(header+8,generation);p16(header+12,count);
    p32(header+16,~crc);p32(header+20,nf_store_crc32(header,20));
    if(!program_at(s,target*NF_BANK_BYTES,header,24))return NF_STORE_IO;
    uint8_t seal[2];p16(seal,NF_BANK_COMMIT);
    if(!program_at(s,target*NF_BANK_BYTES+NF_BANK_BYTES-2,seal,2))return NF_STORE_IO;
    NfStoreResult scan=nf_store_open(s,io);
    return scan==NF_STORE_OK && (s->mask&(1u<<slot)) && s->slot_sequence[slot]==sequence?NF_STORE_OK:NF_STORE_IO;
}
static unsigned ui_status(NfStoreResult result) {
    return result==NF_STORE_EMPTY?NF_PRESET_EMPTY:result==NF_STORE_FUTURE?NF_PRESET_NEWER:result==NF_STORE_IO?NF_PRESET_IO:NF_PRESET_BAD;
}
bool nf_presets_boot(NfStore *s,OriginalUi *u) {
    u->preset_mask=s->mask;if(!s->mask)return false;
    NfSettings settings;
    if(nf_store_load(s,s->last_slot,&settings)!=NF_STORE_OK)return false;
    nf_settings_apply(u,&settings);u->preset_slot=s->last_slot;u->preset_status=NF_PRESET_LOADED;return true;
}
bool nf_presets_execute(NfStore *s,OriginalUi *u,bool disarmed) {
    unsigned command=u->preset_command;u->preset_command=NF_PRESET_NONE;
    if(!command)return false;
    if(!disarmed || u->running || u->active) {u->preset_status=NF_PRESET_STOP;return false;}
    NfSettings settings;NfStoreResult result;bool loaded=false;
    if(command==NF_PRESET_SAVE) {nf_settings_capture(u,&settings);result=nf_store_save(s,u->preset_slot,&settings);}
    else if(command==NF_PRESET_LOAD) {
        result=nf_store_open(s,s->io);
        if(result!=NF_STORE_IO)result=nf_store_load(s,u->preset_slot,&settings);
        if(result==NF_STORE_OK) {nf_settings_apply(u,&settings);loaded=true;}
    } else result=NF_STORE_INVALID;
    u->preset_mask=s->mask;u->preset_status=(uint8_t)(result==NF_STORE_OK?(loaded?NF_PRESET_LOADED:NF_PRESET_SAVED):ui_status(result));
    return loaded;
}
