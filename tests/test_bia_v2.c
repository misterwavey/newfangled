/* BIA v0.2 musical and button-order oracles; no board register emulation. */
#include "bia/bia.h"
#include "bia/settings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void tick(BiaApp *a,unsigned n){for(unsigned i=0;i<n;++i)bia_tick(a,a->now_us+1000u);}
static void flat(BiaApp *a){bia_init(a,15,0);for(unsigned s=0;s<8;++s){for(unsigned f=0;f<7;++f)a->ui.patch.scene[s][f]=500;a->ui.patch.motion[s]=(BiaMotion){.decay={2,2},.retrigger=true};}for(unsigned i=0;i<16;++i)a->ui.patch.steps[i]=(BiaStep){0,100,1,0};a->ui.patch.steps[0].enabled=1;}
static void envelopes(void){
 BiaApp a;flat(&a);a.ui.patch.motion[0].envelope[0]=50;a.ui.patch.motion[0].envelope[1]=-20;a.ui.running=true;bia_sync(&a);
 assert(a.output[4]==1011 && a.output[3]==296 && !a.output[0]);tick(&a,2);assert(a.output[4]==1011 && !a.output[0]);tick(&a,1);assert(a.output[0] && a.output[4]==1011);
 tick(&a,5);assert(a.envelope_age==5 && a.output[4]==755 && a.output[3]==398);tick(&a,5);assert(a.output[4]==500 && a.output[3]==500);
 tick(&a,120);assert(!a.event && a.output[4]==500);
 flat(&a);a.ui.patch.steps[0].ratchets=3;a.ui.patch.motion[0].envelope[0]=50;a.ui.running=true;bia_sync(&a);assert(a.spacing==40);
 tick(&a,3);assert(a.output[4]==1011);tick(&a,10);assert(a.output[4]==500);tick(&a,30);assert(a.output[0] && a.output[4]==1011);
 a.ui.patch.motion[0].envelope[0]=-100;bia_sync(&a);assert(a.output[4]==1011); /* live edits do not rewrite captured envelopes */
 flat(&a);a.ui.patch.steps[0].ratchets=3;a.ui.patch.motion[0].envelope[0]=50;a.ui.patch.motion[0].retrigger=false;a.ui.running=true;bia_sync(&a);tick(&a,43);assert(a.output[0] && a.output[4]==500);
 a.ui.running=false;bia_sync(&a);for(unsigned i=0;i<8;++i)assert(!a.output[i]);tick(&a,1000);for(unsigned i=0;i<8;++i)assert(!a.output[i]);
 a.ui.patch.steps[0].enabled=0;a.ui.running=true;bia_sync(&a);for(unsigned i=0;i<8;++i)assert(!a.output[i]);tick(&a,10);for(unsigned i=0;i<8;++i)assert(!a.output[i]);
}
static void accents(void){
 BiaApp a;flat(&a);a.ui.patch.motion[0].accent[0]=50;a.ui.patch.motion[0].accent[1]=-50;a.ui.patch.motion[0].accent[2]=20;a.ui.patch.step_accent[0]=40;a.ui.running=true;bia_sync(&a);
 assert(a.output[4]==704 && a.output[3]==296 && a.output[2]==581 && a.output[1]==500);
 a.ui.patch.effect=BIA_ACCENT;a.ui.patch.strength[BIA_ACCENT]=60;a.ui.performance=true;unsigned phase=a.phase;bia_sync(&a);
 assert(a.output[4]==1011 && a.output[3]==0 && a.output[2]==704 && a.phase==phase && a.output[1]==500);
 a.ui.performance=false;bia_sync(&a);assert(a.output[4]==704 && a.output[3]==296 && a.output[2]==581 && a.phase==phase);
}
static uint32_t next(uint32_t x){x^=x<<13;x^=x>>17;x^=x<<5;return x;}
static void variation_and_legacy(void){
 BiaApp a;flat(&a);a.ui.patch.variation=37;for(unsigned f=0;f<6;++f)a.ui.patch.motion[0].variation[f]=20;
 a.ui.running=true;bia_sync(&a);uint32_t r=next(0x6b8b4567u);
 for(unsigned f=0;f<6;++f){r=next(r);int d=((int)(r%411u)-205)*37/100;assert(a.output[f+2]==500+d);}assert(a.seed==r);
 a.ui.running=false;bia_sync(&a);a.ui.patch.variation=0;a.ui.patch.motion[0].variation[5]=0;a.ui.running=true;bia_sync(&a);r=a.seed;
 a.ui.patch.effect=BIA_VARIATION;a.ui.patch.strength[BIA_VARIATION]=100;a.ui.performance=true;bia_sync(&a);assert(a.output[1]==500 && a.output[7]==500 && a.seed==r);
 for(unsigned i=2;i<7;++i)assert(a.output[i]>=295 && a.output[i]<=705);
 a.ui.performance=false;bia_sync(&a);for(unsigned i=1;i<8;++i)assert(a.output[i]==500);assert(a.seed==r);
 BiaPatch p;bia_defaults(&p);p.variation=37;uint8_t bytes[NF_SETTINGS_BYTES];assert(nf_settings_encode(&p,bytes));bytes[3]=1;memset(bytes+199,0,NF_SETTINGS_BYTES-199);BiaPatch migrated;
 assert(nf_settings_decode(&migrated,bytes));assert(migrated.variation==37 && !migrated.step_accent[0] && migrated.motion[0].variation[5]==20 && !migrated.motion[0].envelope[0] && !migrated.motion[0].accent[0]);
 assert(nf_settings_encode(&migrated,bytes) && bytes[3]==2);bytes[217+1]=0;assert(!nf_settings_decode(&migrated,bytes));
}
static void fill_levels(void){
 /* Every default-pattern level has a distinct trigger count over one loop. */
 const unsigned expected[6]={8,9,10,12,14,16};
 for(unsigned level=0;level<=5;++level){
  BiaApp a;bia_init(&a,15,0);a.ui.running=true;bia_sync(&a);
  a.ui.patch.strength[0]=(uint8_t)bia_fill_strength(level);a.ui.performance=true;bia_sync(&a);
  unsigned rises=0;bool high=false;
  for(unsigned ms=0;ms<2000;++ms){bool gate=a.output[0]!=0;if(gate && !high)++rises;high=gate;tick(&a,1);}
  assert(rises==expected[level]);assert(bia_fill_level(bia_fill_strength(level))==level);
 }
 for(unsigned strength=0;strength<=100;++strength)assert(bia_fill_level(strength)<=5);
}
static void fill(void){
 BiaApp a,b;flat(&a);for(unsigned i=0;i<16;++i){a.ui.patch.steps[i]=(BiaStep){(uint8_t)(i%8),100,1,(uint8_t)(!(i&1u))};a.ui.patch.motion[i%8].variation[0]=20;}
 a.ui.patch.variation=37;b=a;a.ui.running=true;b.ui.running=true;bia_sync(&a);bia_sync(&b);
 uint8_t baseline[NF_SETTINGS_BYTES],after[NF_SETTINGS_BYTES];assert(nf_settings_encode(&a.ui.patch,baseline));a.ui.performance=true;a.ui.patch.effect=BIA_FILL;a.ui.patch.strength[0]=100;bia_sync(&a);
 for(unsigned ms=0;ms<2000;++ms){tick(&a,1);tick(&b,1);assert(a.phase==b.phase && a.index==b.index && a.seed==b.seed);
  if(a.index==13 || a.index==15){assert(a.ratchets==4);}
  else assert(a.ratchets==b.ratchets);
 }
 a.ui.performance=false;bia_sync(&a);a.ui.patch.strength[0]=50;assert(nf_settings_encode(&a.ui.patch,after));assert(!memcmp(baseline,after,sizeof after));
 /* Added fill struck at last beat: release cancels every remaining trigger. */
 flat(&a);a.ui.patch.fill_mask=1;a.ui.patch.steps[0].enabled=0;a.ui.patch.strength[0]=100;a.ui.running=true;bia_sync(&a);a.ui.performance=true;bia_sync(&a);
 tick(&a,2000);assert(a.fill_event);tick(&a,3);assert(a.output[0]);a.ui.performance=false;unsigned phase=a.phase;bia_sync(&a);assert(!a.output[0] && a.phase==phase);tick(&a,100);assert(!a.output[0]);
}
static uint32_t now;static uint8_t raw;
static void wait_ui(BiaUi *u,unsigned ms){for(unsigned i=0;i<ms;++i)bia_ui_update(u,raw,++now);}
static void button(BiaUi *u,unsigned bit,bool down){if(down)raw&=(uint8_t)~bit;else raw|=(uint8_t)bit;wait_ui(u,21);}
static void turn(BiaUi *u){const unsigned sequence[]={2,0,1,3};for(unsigned i=0;i<4;++i){raw=(uint8_t)((raw&12u)|sequence[i]);wait_ui(u,1);}}
static void gestures(void){
 BiaUi u;now=0;raw=15;bia_ui_init(&u,raw,now);u.running=true;u.active=true;
 button(&u,8,true);assert(u.running && !u.performance);wait_ui(&u,50);button(&u,8,false);assert(!u.running && !u.performance);
 u.running=true;u.active=true;button(&u,8,true);wait_ui(&u,298);assert(!u.performance);wait_ui(&u,2);assert(u.performance && u.running);
 unsigned page=u.page,amount=u.patch.strength[0];turn(&u);assert(bia_fill_level(u.patch.strength[0])==bia_fill_level(amount)+1 && u.page==page);
 button(&u,4,true);button(&u,4,false);assert(u.patch.effect==BIA_ACCENT && !u.reset_count);
 button(&u,8,false);assert(u.running && !u.performance);button(&u,8,true);wait_ui(&u,301);assert(u.performance && u.patch.effect==BIA_ACCENT);button(&u,8,false);
 button(&u,4,true);button(&u,8,true);assert(u.reset_count==1 && !u.performance && u.running);button(&u,8,false);button(&u,4,false);assert(u.running && u.reset_count==1);
 /* Stop tap on release, and bounce shorter than debounce, remain distinct. */
 raw&=(uint8_t)~8u;wait_ui(&u,5);raw|=8u;wait_ui(&u,25);assert(u.running);
 now=UINT32_MAX-150u;bia_ui_init(&u,15,now);raw=15;u.running=true;button(&u,8,true);wait_ui(&u,301);assert(u.performance);button(&u,8,false);assert(u.running && !u.performance);
}
static void cancellation(void){
 BiaApp a;flat(&a);a.ui.running=true;bia_sync(&a);a.ui.performance=true;a.ui.start_pending=true;a.ui.start_playback=true;bia_sync(&a);assert(a.performance);
 ++a.ui.reset_count;bia_sync(&a);assert(!a.performance && !a.ui.performance && a.ui.start_consumed && !a.output[0]);
 a.ui.performance=true;bia_sync(&a);a.ui.running=false;bia_sync(&a);assert(!a.performance);for(unsigned i=0;i<8;++i)assert(!a.output[i]);
 flat(&a);a.ui.patch.external=1;a.ui.patch.ppqn_index=0;a.ui.running=true;bia_sync(&a);bia_input(&a,NF_INPUT_CLOCK,0);tick(&a,500);bia_input(&a,NF_INPUT_CLOCK,500000);tick(&a,1);assert(a.running);
 a.ui.performance=true;a.ui.start_pending=true;a.ui.start_playback=true;bia_sync(&a);assert(a.performance);tick(&a,1100);assert(!a.running && !a.performance && !a.ui.performance && a.ui.start_consumed);for(unsigned i=0;i<8;++i)assert(!a.output[i]);
}
int main(void){envelopes();accents();variation_and_legacy();fill_levels();fill();gestures();cancellation();puts("BIA v0.2: envelopes/retriggers, signed accents, protected/legacy variation, reversible fills, button ordering and cancellation passed.");}
