/* SPDX-License-Identifier: MIT
 * Application-local journal in Rev62's 64 KiB SPI EEPROM. Logical erase
 * writes FF; every write is read back. Never accesses the other app's region.
 */
#include "pnw_board.h"
#include "preset_format.h"
#ifdef __arm__
__attribute__((section(".nf_identity"),used))
#endif
const struct {uint8_t magic[8];uint32_t base,size;} nf_storage_identity={
#ifdef BIA_BUILD
    {'B','I','E','E','P','R','0','1'},NF_STORE_EEPROM_BASE,NF_STORE_BYTES
#else
    {'N','F','E','E','P','R','0','1'},NF_STORE_EEPROM_BASE,NF_STORE_BYTES
#endif
};
static bool within(unsigned offset,unsigned length) {
    return offset<=NF_STORE_BYTES && length<=NF_STORE_BYTES-offset;
}
bool pnw_preset_read(void *context,unsigned offset,void *data,unsigned length) {
    (void)context;
    return within(offset,length) && pnw_eeprom_read((uint16_t)(NF_STORE_EEPROM_BASE+offset),data,length);
}
bool pnw_preset_program(void *context,unsigned offset,const void *data,unsigned length) {
    (void)context;if(!within(offset,length) || (!data && length))return false;
    const uint8_t *p=data;uint8_t check[16];
    /* Page splitting and WREN/WIP handling belong to the SPI driver. */
    if(!pnw_eeprom_write((uint16_t)(NF_STORE_EEPROM_BASE+offset),data,length))return false;
    for(unsigned done=0;done<length;){
        unsigned n=length-done;if(n>sizeof check)n=sizeof check;
        if(!pnw_eeprom_read((uint16_t)(NF_STORE_EEPROM_BASE+offset+done),check,n))return false;
        for(unsigned i=0;i<n;++i)if(check[i]!=p[done+i])return false;
        done+=n;
    }
    return true;
}
bool pnw_preset_erase(void *context,unsigned bank) {
    if(bank>1)return false;
    uint8_t blank[128];for(unsigned i=0;i<sizeof blank;++i)blank[i]=255;
    for(unsigned offset=0;offset<NF_BANK_BYTES;offset+=sizeof blank)
        if(!pnw_preset_program(context,bank*NF_BANK_BYTES+offset,blank,sizeof blank))return false;
    return true;
}
