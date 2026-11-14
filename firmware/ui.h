/* SPDX-License-Identifier: MIT */
#ifndef ORIGINAL_UI_H
#define ORIGINAL_UI_H
#include <stdbool.h>
#include <stdint.h>
#include "voice.h"
#include "sync.h"
#include "cv.h"
#include "preset_format.h"
typedef struct {
    unsigned bpm,page;
    int encoder;
    uint8_t previous,candidate,stable;
    uint32_t changed_at,reset_count;
    bool editing,running,timing_fault;
    unsigned field;
    NfConfig outputs[8];
    NfRhythm rhythms[8];NfGroove grooves[8];
    bool selecting,program_consumed;
    uint32_t program_at;
    bool external,active,run_high;
    uint8_t ppqn_index,run_mode,sync_status;
    unsigned external_bpm;
    NfCvRoute routes[8][NF_FIELDS];
    NfCvCalibration calibration[2];
    NfConfig effective[8];
    uint16_t cv_raw[2];int32_t cv_normalized[2];
    uint8_t cv_valid,cv_raw_valid,cv_field,cv_target,cv_input;
    bool cv_menu,cv_monitor,cv_swapped;
    bool preset_menu;
    uint8_t preset_slot,preset_field,preset_command,preset_status,preset_mask;
} OriginalUi;
void original_ui_init(OriginalUi *ui,uint8_t raw,uint32_t now);
bool original_ui_update(OriginalUi *ui,uint8_t raw,uint32_t now);
void original_ui_render(const OriginalUi *ui);
#endif
