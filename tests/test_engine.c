/* Independent requirements for the v0.1 multichannel engine. */
#include "app.h"
#include <assert.h>
#include <stdint.h>
int main(void) {
    const unsigned bpms[]={10,120,137,300};
    for(unsigned ratio=0;ratio<NF_RATIOS;++ratio)for(unsigned b=0;b<4;++b) {
        unsigned bpm=bpms[b],span=60000u*nf_divisors[ratio],step=bpm*nf_multipliers[ratio];
        NfVoice v={.config={ratio,0,50,0,100,0}};nf_voice_reset(&v,0);
        for(unsigned ms=0;ms<60000;++ms) {
            assert(v.phase==(uint64_t)ms*step%span);
            assert(nf_voice_value(&v,true,bpm)==(v.phase<span/2?1023:0));
            assert(!nf_voice_value(&v,false,bpm));nf_voice_tick(&v,bpm);
        }
    }
    NfVoice v={.config={5,2,50,0,100,0}};nf_voice_reset(&v,0);
    assert(nf_voice_value(&v,true,120)==0);v.phase=30000;assert(nf_voice_value(&v,true,120)==512);
    v.config.shape=3;assert(nf_voice_value(&v,true,120)==511);
    v.config.shape=1;assert(nf_voice_value(&v,true,120)==1022);
    v.config.shape=0;v.phase=0;v.config.phase=50;assert(!nf_voice_value(&v,true,120));
    v.config.phase=0;v.config.level=50;v.config.offset=25;assert(nf_voice_value(&v,true,120)==768);
    v.config.offset=100;assert(nf_voice_value(&v,true,120)==1023);
    v.config.level=0;v.config.offset=0;assert(!nf_voice_value(&v,true,120));
    /* Extreme widths at x16/300 BPM must still produce both levels per cycle. */
    for(unsigned width=1;width<=99;width+=98) {
        v.config=(NfConfig){10,0,width,0,100,0};nf_voice_reset(&v,0);
        unsigned highs=0,lows=0;
        for(unsigned t=0;t<1000;++t) {if(nf_voice_value(&v,true,300))++highs;else ++lows;nf_voice_tick(&v,300);}
        assert(highs>=80 && lows>=80);
    }
    v.config=(NfConfig){5,4,50,0,100,0};nf_voice_reset(&v,2);uint32_t random=v.random;
    for(unsigned t=0;t<499;++t)nf_voice_tick(&v,120);assert(v.random==random);
    nf_voice_tick(&v,120);assert(v.random!=random);
    OriginalApp app;original_app_init(&app,15,0);
    for(unsigned i=0;i<8;++i)assert(!app.output[i]);
    app.ui.running=true;original_app_sync(&app);
    for(unsigned i=0;i<8;++i) {app.ui.outputs[i].shape=i%NF_SHAPES;app.ui.outputs[i].ratio=i;}
    original_app_sync(&app);
    for(unsigned t=0;t<10000;++t) {
        original_app_tick(&app);
        for(unsigned i=0;i<8;++i)assert(app.output[i]<=1023);
    }
    uint32_t other=app.voices[1].phase;
    app.ui.outputs[0].ratio=10;original_app_sync(&app);
    assert(!app.voices[0].phase && app.voices[1].phase==other);
    ++app.ui.reset_count;original_app_sync(&app);
    for(unsigned i=0;i<8;++i)assert(!app.voices[i].phase);
    app.ui.running=false;original_app_sync(&app);
    for(unsigned i=0;i<8;++i)assert(!app.output[i]);
    return 0;
}
