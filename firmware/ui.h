/* SPDX-License-Identifier: MIT */
#ifndef ORIGINAL_UI_H
#define ORIGINAL_UI_H
#include <stdbool.h>
#include <stdint.h>
#include "voice.h"
typedef struct {
    unsigned bpm,page;
    int encoder;
    uint8_t previous,candidate,stable;
    uint32_t changed_at,reset_count;
    bool editing,running,timing_fault;
    unsigned field;
    NfConfig outputs[8];
    bool selecting,program_consumed;
    uint32_t program_at;
} OriginalUi;
void original_ui_init(OriginalUi *ui,uint8_t raw,uint32_t now);
bool original_ui_update(OriginalUi *ui,uint8_t raw,uint32_t now);
void original_ui_render(const OriginalUi *ui);
#endif
