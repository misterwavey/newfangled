/* Actual font pixels must not collide when composing independent UI elements. */
#include "bia/bia.h"
#include "preset_format.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t pixels[128*32];
void pnw_oled_clear(void){memset(pixels,0,sizeof pixels);}
void pnw_oled_pixel(unsigned x,unsigned y,bool on){assert(x<128 && y<32);if(on){assert(!pixels[y*128+x]);pixels[y*128+x]=1;}}
int main(void){
 BiaUi u;bia_ui_init(&u,15,0);
 for(unsigned state=0;state<5;++state)for(unsigned flags=0;flags<16;++flags){
  u.sync_status=state;u.active=(flags&1)!=0;u.auditioning=(flags&2)!=0;u.timing_fault=(flags&4)!=0;u.editing=(flags&8)!=0;
  for(unsigned step=0;step<16;++step){u.playing_step=step;bia_ui_render(&u);}
 }
 u.timing_fault=false;u.auditioning=false;
 for(unsigned page=0;page<25;++page)for(unsigned editing=0;editing<2;++editing)for(unsigned selected=0;selected<2;++selected){
  u.page=page;u.editing=editing!=0;u.selecting=selected!=0;
  for(unsigned field=0;field<(page==0?16u:page<=16?6u:22u);++field){u.field=field;bia_ui_render(&u);}
 }
 for(unsigned effect=0;effect<3;++effect)for(unsigned active=0;active<2;++active){u.performance_page=true;u.performance=active!=0;u.patch.effect=(uint8_t)effect;u.patch.strength[effect]=100;bia_ui_render(&u);}u.performance=false;u.performance_page=false;
 u.preset_menu=true;
 for(unsigned state=0;state<9;++state)for(unsigned field=0;field<3;++field)for(unsigned editing=0;editing<2;++editing){u.preset_status=(uint8_t)state;u.preset_field=(uint8_t)field;u.editing=editing!=0;bia_ui_render(&u);}
 u.initialize_confirm=true;u.initialize_yes=false;bia_ui_render(&u);u.initialize_yes=true;bia_ui_render(&u);
 puts("BIA OLED: Home/status, presets, all pages/fields and transport/edit modes compose without overlapping pixels.");
}
