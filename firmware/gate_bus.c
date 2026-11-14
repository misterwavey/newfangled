/* SPDX-License-Identifier: MIT */
#include "gate_bus.h"
bool pnw_bus_acquire(PnwGateBus *bus) {
    if(bus->busy)return false;
    bus->busy=1;return true;
}
void pnw_bus_release(PnwGateBus *bus) {bus->busy=0;}
void pnw_bus_request(PnwGateBus *bus,bool high) {
    if(bus->pending && bus->level!=(uint32_t)high)++bus->overruns;
    bus->level=high;bus->pending=1;
}
bool pnw_bus_take(PnwGateBus *bus,bool *high) {
    if(!bus->pending || !pnw_bus_acquire(bus))return false;
    *high=bus->level!=0;bus->pending=0;return true;
}
