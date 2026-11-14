/* Behavioral checks for the new UI; no MCU or vendor image needed. */
#include "pnw_board.h"
#include "ui.h"
#include "font.h"
#include <assert.h>
#include <limits.h>
#include <string.h>
static unsigned pixels,clears;
static unsigned char framebuffer[128*32];
void pnw_oled_clear(void) {memset(framebuffer,0,sizeof framebuffer);pixels=0;++clears;}
void pnw_oled_pixel(unsigned x,unsigned y,bool on) {
    assert(x<128 && y<32);framebuffer[y*128+x]=on;++pixels;
}
static void detent(OriginalUi *ui,int direction) {
    const uint8_t forward[]={2,0,1,3},reverse[]={1,0,2,3};
    const uint8_t *values=direction>0?forward:reverse;
    for(unsigned i=0;i<4;++i)(void)original_ui_update(ui,12u|values[i],0);
}
static void click(OriginalUi *ui,uint8_t mask,uint32_t *now) {
    *now+=30;(void)original_ui_update(ui,(uint8_t)(15u&~mask),*now);
    assert(!original_ui_update(ui,(uint8_t)(15u&~mask),*now+19));
    bool pressed=original_ui_update(ui,(uint8_t)(15u&~mask),*now+20);assert(pressed==(mask==8));
    assert(!original_ui_update(ui,(uint8_t)(15u&~mask),*now+29));
    *now+=60;(void)original_ui_update(ui,15,*now);bool released=original_ui_update(ui,15,*now+20);assert(released==(mask==4));
}
int main(void) {
    OriginalUi ui;uint32_t now=UINT_MAX-200;
    original_ui_init(&ui,15,now);assert(ui.bpm==120 && !ui.editing && ui.page==0);
    /* Rotation with released buttons may browse, but never enter parameter
       modes or alter any output configuration, in either direction. */
    NfConfig boot_outputs[8];memcpy(boot_outputs,ui.outputs,sizeof boot_outputs);
    for(unsigned i=0;i<27;++i) {
        detent(&ui,1);assert(ui.page==(i+1)%9u && !ui.selecting && !ui.editing);
        assert(!memcmp(ui.outputs,boot_outputs,sizeof boot_outputs));
    }
    for(unsigned i=0;i<27;++i) {
        detent(&ui,-1);assert(ui.page==(8u+9u-i%9u)%9u && !ui.selecting && !ui.editing);
        assert(!memcmp(ui.outputs,boot_outputs,sizeof boot_outputs));
    }
    detent(&ui,1);assert(ui.page==1); /* physical clockwise: Home -> OUT1 */
    detent(&ui,-1);assert(ui.page==0);
    for(unsigned i=0;i<9;++i)detent(&ui,1);assert(ui.page==0);
    detent(&ui,-1);assert(ui.page==8);
    click(&ui,4,&now);assert(ui.page==8 && ui.selecting && !ui.editing && ui.field==0);
    detent(&ui,1);assert(ui.field==1 && ui.outputs[7].ratio==5);
    click(&ui,4,&now);assert(ui.editing);
    detent(&ui,1);assert(ui.outputs[7].shape==3 && ui.field==1);
    click(&ui,4,&now);assert(!ui.editing && ui.selecting && ui.field==1);
    /* Every parameter can be left immediately, without walking other fields. */
    for(unsigned field=0;field<NF_FIELDS;++field) {
        ui.field=field;click(&ui,4,&now);assert(ui.editing);
        now+=30;(void)original_ui_update(&ui,11,now);(void)original_ui_update(&ui,11,now+20);
        assert(!original_ui_update(&ui,11,now+519));
        assert(original_ui_update(&ui,11,now+520));
        assert(!ui.editing && !ui.selecting && ui.page==8);
        now+=550;(void)original_ui_update(&ui,15,now);assert(!original_ui_update(&ui,15,now+20));
        click(&ui,4,&now);assert(ui.selecting && !ui.editing);
    }
    /* Hold from parameter browsing, then rotate directly to another output. */
    now+=30;(void)original_ui_update(&ui,11,now);(void)original_ui_update(&ui,11,now+20);
    (void)original_ui_update(&ui,11,now+520);now+=550;
    (void)original_ui_update(&ui,15,now);(void)original_ui_update(&ui,15,now+20);
    assert(!ui.editing && !ui.selecting);detent(&ui,1);assert(ui.page==0);
    click(&ui,4,&now);assert(ui.editing);
    for(unsigned i=0;i<300;++i)detent(&ui,1);assert(ui.bpm==300);
    for(unsigned i=0;i<400;++i)detent(&ui,-1);assert(ui.bpm==10);
    click(&ui,8,&now);assert(ui.running && ui.editing && ui.page==0);
    now+=30;(void)original_ui_update(&ui,11,now);(void)original_ui_update(&ui,11,now+20);
    now+=30;(void)original_ui_update(&ui,3,now);(void)original_ui_update(&ui,3,now+20);
    assert(ui.running && !ui.editing && ui.reset_count==1);
    now+=30;(void)original_ui_update(&ui,15,now);(void)original_ui_update(&ui,15,now+20);
    assert(!ui.editing && !ui.selecting);
    click(&ui,8,&now);assert(!ui.running);
    (void)original_ui_update(&ui,12,now);assert(ui.encoder==0); /* illegal quadrature jump */
    original_ui_init(&ui,15,0);
    for(unsigned page=0;page<9;++page) {ui.page=page;original_ui_render(&ui);assert(pixels>0);}
    assert(clears>=9);
    /* Hi-fi symbols occupy the left edge on Home and right edge on outputs. */
    for(unsigned page=0;page<=1;++page)for(unsigned running=0;running<=1;++running) {
        ui.page=page;ui.running=running!=0;original_ui_render(&ui);
        unsigned x=page?119u:0u;
        for(unsigned y=0;y<9;++y)for(unsigned col=0;col<9;++col) {
            unsigned distance=y>4?y-4:4-y;
            assert((framebuffer[y*128+x+col]!=0)==(!running || col<9u-2u*distance));
        }
    }
    ui.page=1;ui.selecting=true;
    for(unsigned field=0;field<NF_FIELDS;++field) {
        ui.field=field;ui.editing=true;original_ui_render(&ui);assert(pixels>0);
    }
    for(unsigned shape=0;shape<NF_SHAPES;++shape) {
        ui.field=1;ui.outputs[0].shape=(uint8_t)shape;original_ui_render(&ui);
    }
    /* A delayed polling interval must still classify a released hold correctly. */
    original_ui_init(&ui,15,UINT_MAX-400);ui.page=1;ui.selecting=true;ui.editing=true;
    (void)original_ui_update(&ui,11,UINT_MAX-350);(void)original_ui_update(&ui,11,UINT_MAX-330);
    (void)original_ui_update(&ui,15,200);(void)original_ui_update(&ui,15,220);
    assert(!ui.selecting && !ui.editing && ui.page==1);

    pnw_oled_clear();assert(original_text(127,31,"W",3)>127);assert(pixels<=1);
    unsigned count=pixels;assert(original_text(UINT_MAX,0,"A",1)==UINT_MAX);assert(pixels==count);
    original_text(0,0,"A",0);assert(pixels==count);
    pnw_oled_clear();original_text(0,0,"abc",1);unsigned char lower[sizeof framebuffer];memcpy(lower,framebuffer,sizeof lower);
    pnw_oled_clear();original_text(0,0,"ABC",1);assert(!memcmp(lower,framebuffer,sizeof lower));
    pnw_oled_clear();original_text(0,0,"%",1);memcpy(lower,framebuffer,sizeof lower);
    pnw_oled_clear();original_text(0,0,"?",1);assert(memcmp(lower,framebuffer,sizeof lower));
    pnw_oled_clear();original_text(0,0,"@",1);memcpy(lower,framebuffer,sizeof lower);
    pnw_oled_clear();original_text(0,0,"?",1);assert(!memcmp(lower,framebuffer,sizeof lower));
    return 0;
}
