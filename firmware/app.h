/* SPDX-License-Identifier: MIT */
#ifndef ORIGINAL_APP_H
#define ORIGINAL_APP_H
#include "clock.h"
#include "ui.h"
typedef struct {OriginalUi ui;OriginalClock clock;uint32_t reset_seen,output_reset_seen[8];NfVoice voices[8];uint16_t output[8];
    NfSync sync;
    uint32_t now_us;
    bool external,requested,run_high,reset_input;
    unsigned ppqn,run_mode,step;
    uint8_t muted;
    NfConfig base[8];
    NfCv cv;NfCvPlan cv_plans[8][NF_FIELDS];
    uint32_t cv_display_us;
    uint16_t cv_adc[2];uint8_t cv_adc_valid,cv_divider;
} OriginalApp;
void original_app_init(OriginalApp *app,uint8_t raw,uint32_t now);
/* Clock access must be serialized with the tick interrupt on MCU. */
void original_app_sync(OriginalApp *app);
void original_app_tick(OriginalApp *app);
void original_app_tick_at(OriginalApp *app,uint32_t now_us);
void original_app_input(OriginalApp *app,unsigned kind,uint32_t us);
void original_app_cv(OriginalApp *app,unsigned adc0,unsigned adc1,unsigned valid);
/* Foreground-only UI snapshot; serialize with the tick interrupt. */
bool original_app_refresh_ui(OriginalApp *app);
#endif
