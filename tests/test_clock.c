/* Clock requirements, independent integer oracle and bus-arbitration scenarios. */
#include "clock.h"
#include "gate_bus.h"
#include <assert.h>
#include <limits.h>
int main(void) {
    OriginalClock clock;
    for(unsigned bpm=10;bpm<=300;++bpm) {
        original_clock_init(&clock,bpm);assert(!clock.gate && !clock.running);
        original_clock_running(&clock,true);assert(clock.gate);
        unsigned last_edge=0;bool previous=true;
        unsigned minimum=30000u/bpm,maximum=(30000u+bpm-1u)/bpm;
        for(unsigned ms=1;ms<=60000;++ms) {
            original_clock_tick(&clock);
            assert(clock.phase==(ms*bpm)%60000u);
            assert(clock.beats==(ms*bpm)/60000u);
            assert(clock.gate==((ms*bpm)%60000u<30000u));
            if(clock.gate!=previous) {
                unsigned width=ms-last_edge;assert(width>=minimum && width<=maximum);
                last_edge=ms;previous=clock.gate;
            }
        }
        assert(clock.beats==bpm && clock.phase==0 && clock.gate);
    }
    original_clock_init(&clock,120);original_clock_running(&clock,true);
    for(unsigned i=0;i<125;++i)original_clock_tick(&clock);
    assert(clock.phase==15000);original_clock_tempo(&clock,240);assert(clock.phase==15000);
    original_clock_running(&clock,true);assert(clock.phase==15000); /* idempotent */
    for(unsigned i=0;i<62;++i)original_clock_tick(&clock);assert(clock.gate);
    original_clock_tick(&clock);assert(!clock.gate);
    original_clock_reset(&clock);assert(clock.running && clock.gate && !clock.phase && !clock.beats);
    original_clock_running(&clock,false);assert(!clock.gate && !clock.phase);
    for(unsigned i=0;i<10000;++i)original_clock_tick(&clock);assert(!clock.phase && !clock.gate);
    original_clock_reset(&clock);assert(!clock.running && !clock.gate);
    original_clock_tempo(&clock,0);assert(clock.bpm==10);original_clock_tempo(&clock,UINT_MAX);assert(clock.bpm==300);
    original_clock_running(&clock,true);clock.beats=UINT_MAX;clock.phase=59900;original_clock_tick(&clock);assert(clock.beats==0);
    PnwGateBus bus={0};bool high=false;
    assert(pnw_bus_acquire(&bus));pnw_bus_request(&bus,true);
    assert(!pnw_bus_take(&bus,&high));assert(bus.pending && bus.busy && !bus.overruns);
    pnw_bus_release(&bus);assert(pnw_bus_take(&bus,&high) && high && bus.busy && !bus.pending);
    pnw_bus_request(&bus,false); /* interrupt arrives while previous level is transmitted */
    assert(!pnw_bus_take(&bus,&high));pnw_bus_release(&bus);
    assert(pnw_bus_take(&bus,&high) && !high);pnw_bus_release(&bus);
    assert(!pnw_bus_take(&bus,&high));
    assert(pnw_bus_acquire(&bus));pnw_bus_request(&bus,true);pnw_bus_request(&bus,true);assert(!bus.overruns);
    pnw_bus_request(&bus,false);assert(bus.overruns==1);pnw_bus_release(&bus);
    assert(pnw_bus_take(&bus,&high) && !high);pnw_bus_release(&bus);
    return 0;
}
