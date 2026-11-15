/* Test board: real EEPROM adapter/journal, injected shared 64 KiB device. */
#include "store.h"
#include "pnw_board.h"
#include <string.h>
static uint8_t *device;static int budget=-1;static bool damage_page;
void test_device(uint8_t *p){device=p;budget=-1;damage_page=false;}
void test_budget(int n,unsigned damage){budget=n;damage_page=damage!=0;}
bool pnw_eeprom_read(uint16_t address,void *p,size_t n){if(!p || n>65536u-address)return false;memcpy(p,device+address,n);return true;}
bool pnw_eeprom_write(uint16_t address,const void *p,size_t n){
 if((!p && n) || n>65536u-address)return false;const uint8_t *bytes=p;
 for(unsigned i=0;i<n;++i){
  if(budget==0){if(damage_page)memset(device+((address+i)&~127u),0,128);return false;}
  if(budget>0)--budget;device[address+i]=bytes[i];
 }
 return true;
}
static NfStoreIo io={0,pnw_preset_read,pnw_preset_erase,pnw_preset_program};
int test_save(unsigned slot,unsigned bpm){
 OriginalUi u;
#ifdef BIA_BUILD
 bia_ui_init(&u,15,0);u.patch.bpm=(uint16_t)bpm;
#else
 original_ui_init(&u,15,0);u.bpm=bpm;
#endif
 NfSettings settings;nf_settings_capture(&u,&settings);NfStore s;(void)nf_store_open(&s,io);return nf_store_save(&s,slot,&settings);
}
int test_load(unsigned slot){NfStore s;NfSettings settings;if(nf_store_open(&s,io)==NF_STORE_IO || nf_store_load(&s,slot,&settings)!=NF_STORE_OK)return -1;return settings.bpm;}
int test_boot(void){NfStore s;OriginalUi u={0};if(nf_store_open(&s,io)==NF_STORE_IO || !nf_presets_boot(&s,&u))return -1;
#ifdef BIA_BUILD
 return u.patch.bpm;
#else
 return (int)u.bpm;
#endif
}
int test_clear(void){return pnw_preset_erase(0,0) && pnw_preset_erase(0,1);}
int test_ranges(void){uint8_t byte=1;return !pnw_preset_read(0,8192,&byte,1) && !pnw_preset_program(0,8191,&byte,2) && !pnw_preset_program(0,0,0,1) && !pnw_preset_erase(0,2);}
