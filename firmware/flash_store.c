/* LEGACY ONLY: not linked by current EEPROM builds.
 * SPDX-License-Identifier: MIT
 * Only 0x08006000..0x08007fff is writable. No EEPROM/option-byte access.
 */
#include "pnw_board.h"
#include "preset_format.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
__attribute__((section(".nf_identity"),used))
const struct {uint8_t magic[8];uint32_t base,size;} nf_storage_identity={
    #ifdef BIA_BUILD
    {'B','I','P','R','E','S','0','1'},NF_STORE_FLASH_BASE,NF_STORE_BYTES
#else
    {'N','F','P','R','E','S','0','1'},NF_STORE_FLASH_BASE,NF_STORE_BYTES
#endif
};
/* Executed from SRAM: all literals and waits remain available while flash is
 * busy. The linker includes this section in the startup data copy. */
__attribute__((section(".ramfunc"),noinline))
static bool flash_operation(uint32_t address,uint16_t value,bool erase) {
    uint32_t saved;__asm volatile("mrs %0, primask\n cpsid i":"=r"(saved)::"memory");
    volatile uint32_t *sr=(volatile uint32_t *)(uintptr_t)0x4002200c;
    volatile uint32_t *cr=(volatile uint32_t *)(uintptr_t)0x40022010;
    uint32_t start=WORD(0x40000024);unsigned guard=1000000;bool ok=true;
    while((*sr&1u) && --guard && (uint32_t)(WORD(0x40000024)-start)<100000u) {}
    if(*sr&1u)ok=false;
    if(ok && (*cr&128u)) {WORD(0x40022004)=0x45670123u;WORD(0x40022004)=0xcdef89abu;}
    if(*cr&128u)ok=false;
    if(ok) {
        *sr=0x34u;
        if(erase) {*cr=2u;WORD(0x40022014)=address;*cr=2u|64u;}
        else {*cr=1u;*(volatile uint16_t *)(uintptr_t)address=value;}
        __asm volatile("dsb":::"memory");start=WORD(0x40000024);guard=1000000;
        while((*sr&1u) && --guard && (uint32_t)(WORD(0x40000024)-start)<100000u) {}
        if(*sr&(1u|4u|16u))ok=false;
    }
    *cr=128u;__asm volatile("dsb\n msr primask, %0"::"r"(saved):"memory");return ok;
}
bool pnw_preset_read(void *context,unsigned offset,void *data,unsigned length) {
    (void)context;if((!data && length) || offset>NF_STORE_BYTES || length>NF_STORE_BYTES-offset)return false;
    uint8_t *out=data;const volatile uint8_t *in=(const volatile uint8_t *)(uintptr_t)(NF_STORE_FLASH_BASE+offset);
    for(unsigned i=0;i<length;++i)out[i]=in[i];return true;
}
bool pnw_preset_program(void *context,unsigned offset,const void *data,unsigned length) {
    (void)context;
    if((!data && length) || (offset&1u) || (length&1u) || offset>NF_STORE_BYTES || length>NF_STORE_BYTES-offset)return false;
    const uint8_t *p=data;
    for(unsigned i=0;i<length;i+=2) {
        uint32_t address=NF_STORE_FLASH_BASE+offset+i;uint16_t value=(uint16_t)(p[i]|((unsigned)p[i+1]<<8));
        uint16_t old=*(const volatile uint16_t *)(uintptr_t)address;
        /* STM32F0 requires an erased halfword before programming (apart from
           the special all-zero case, which this driver does not need). */
        if(old!=value && old!=0xffffu)return false;
        if(old!=value && !flash_operation(address,value,false))return false;
        if(*(const volatile uint16_t *)(uintptr_t)address!=value)return false;
    }
    return true;
}
bool pnw_preset_erase(void *context,unsigned bank) {
    (void)context;if(bank>1)return false;
    for(unsigned page=0;page<4;++page) {
        uint32_t address=NF_STORE_FLASH_BASE+bank*NF_BANK_BYTES+page*1024u;
        if(!flash_operation(address,0xffff,true))return false;
        for(unsigned i=0;i<1024;i+=2)if(*(const volatile uint16_t *)(uintptr_t)(address+i)!=0xffffu)return false;
    }
    return true;
}
