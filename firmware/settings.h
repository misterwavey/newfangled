/* SPDX-License-Identifier: MIT */
#ifndef NEWFANGLED_SETTINGS_H
#define NEWFANGLED_SETTINGS_H
#include "ui.h"
#include "preset_format.h"
typedef struct {
    uint16_t bpm;uint8_t external,ppqn_index,run_mode,swapped,muted;
    NfCvCalibration calibration[2];NfConfig outputs[8];NfRhythm rhythms[8];NfGroove grooves[8];NfTone tones[8];NfCvRoute routes[8][NF_FIELDS];
} NfSettings;
void nf_settings_capture(const OriginalUi *ui,NfSettings *settings);
void nf_settings_apply(OriginalUi *ui,const NfSettings *settings);
bool nf_settings_encode(const NfSettings *settings,uint8_t bytes[NF_SETTINGS_BYTES]);
bool nf_settings_decode_size(NfSettings *settings,const uint8_t *bytes,unsigned length);
bool nf_settings_decode(NfSettings *settings,const uint8_t bytes[NF_SETTINGS_BYTES]);
#endif
