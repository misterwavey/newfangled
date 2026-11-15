/* Exercise the shared ISR/foreground boundary with both real UIs. */
#include "controls.h"
#include "ui.h"
#include "bia/bia.h"
#include <assert.h>
#include <stdio.h>
static OriginalUi normal;static BiaUi bia;static NfControlQueue queue;
static uint32_t now;static uint8_t raw;
static void drain(void){uint8_t r;uint32_t t;while(nf_controls_take(&queue,&r,&t)){original_ui_update(&normal,r,t);bia_ui_update(&bia,r,t);}}
static void sample(uint8_t r,bool busy){raw=r;nf_controls_push(&queue,raw,++now);if(!busy)drain();}
static void steady(uint8_t r,unsigned n){while(n--)sample(r,false);}
static void turn(int direction,bool busy){const uint8_t cw[]={2,0,1,3},ccw[]={1,0,2,3};const uint8_t *s=direction>0?cw:ccw;for(unsigned i=0;i<4;++i)sample((uint8_t)((raw&12u)|s[i]),busy);if(busy)drain();}
static void init(void){queue=(NfControlQueue){0};now=UINT32_MAX-40u;raw=15;original_ui_init(&normal,raw,now);bia_ui_init(&bia,raw,now);}
static void click_turn(int direction){
 init();
 /* Both start on a numeric parameter, with room in either direction. */
 normal.page=1;normal.selecting=true;normal.field=2;
 bia.page=1;bia.selecting=true;bia.field=2;bia.patch.steps[0].probability=50;
 steady(11,25);steady(15,25);assert(normal.editing && bia.editing);
 unsigned value=normal.outputs[0].width;
 /* Foreground is busy for the entire first notch after the click. */
 turn(direction,true);assert(normal.outputs[0].width==(unsigned)((int)value+direction));
 assert(bia.patch.steps[0].probability==50+direction);
 /* Click out, then the very first notch must select another parameter. */
 steady(11,25);steady(15,25);assert(!normal.editing && !bia.editing);
 turn(direction,true);assert(normal.field==(direction>0?3u:1u) && bia.field==normal.field);
 assert(queue.overruns==0);
}
static int path(const uint8_t *raws,unsigned count){int progress=0,events=0;uint8_t previous=3;for(unsigned i=0;i<count;++i){events+=nf_encoder_step(&progress,previous,raws[i]);previous=raws[i];}return events;}
static void bounce(void){
 const uint8_t cw[]={2,3,2,0,2,0,1,0,1,3};
 const uint8_t ccw[]={1,3,1,0,1,0,2,0,2,3};
 assert(path(cw,sizeof cw)==1);assert(path(ccw,sizeof ccw)==-1);
 /* Incomplete push-induced movement, then first complete notch/reversal. */
 const uint8_t noisy_cw[]={0,1,3,2,0,1,3};
 const uint8_t noisy_ccw[]={0,2,3,1,0,2,3};
 assert(path(noisy_cw,sizeof noisy_cw)==1);assert(path(noisy_ccw,sizeof noisy_ccw)==-1);
 const uint8_t reversal[]={2,0,2,3,1,0,2,3};assert(path(reversal,sizeof reversal)==-1);
 init();steady(11,25);sample(8,false);sample(9,false);sample(11,false);steady(15,25);
 assert(!normal.encoder && !bia.encoder);turn(1,true);assert(normal.bpm==121 && bia.patch.effect==BIA_ACCENT);
}
static void queue_limits(void){
 NfControlQueue q={0};uint8_t raw;uint32_t time;
 for(unsigned i=0;i<1000;++i){nf_controls_push(&q,(uint8_t)i,i);assert(nf_controls_take(&q,&raw,&time));assert(raw==(uint8_t)i && time==i);}
 for(unsigned i=0;i<40;++i)nf_controls_push(&q,(uint8_t)i,i);
 assert(q.overruns==40-(NF_CONTROL_SAMPLES-1));
 for(unsigned i=0;i<NF_CONTROL_SAMPLES-1;++i){assert(nf_controls_take(&q,&raw,&time));assert(raw==i && time==i);}
 assert(!nf_controls_take(&q,&raw,&time));
}
int main(void){click_turn(1);click_turn(-1);bounce();queue_limits();puts("Both UIs: first turn after click, queued display stalls, contact bounce, invalid paths, reversal and timestamp wrap passed.");}
