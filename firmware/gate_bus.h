/* SPDX-License-Identifier: MIT */
#ifndef PNW_GATE_BUS_H
#define PNW_GATE_BUS_H
#include <stdbool.h>
#include <stdint.h>
/* Call these operations in a critical section on a single-core MCU. Only the
 * SPI transfer itself runs with interrupts enabled. A queued level replaces
 * an undelivered level; overruns make that loss visible rather than emitting
 * stale edges in a catch-up burst. */
typedef struct {volatile uint32_t busy,pending,level,overruns;} PnwGateBus;
bool pnw_bus_acquire(PnwGateBus *bus);
void pnw_bus_release(PnwGateBus *bus);
void pnw_bus_request(PnwGateBus *bus,bool high);
bool pnw_bus_take(PnwGateBus *bus,bool *high);
#endif
