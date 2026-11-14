/* SPDX-License-Identifier: MIT */
#ifndef ORIGINAL_APP_H
#define ORIGINAL_APP_H
#include "clock.h"
#include "ui.h"
typedef struct {OriginalUi ui;OriginalClock clock;uint32_t reset_seen;NfVoice voices[8];uint16_t output[8];
    NfSync sync;
    uint32_t now_us;
    bool external,requested,run_high,reset_input;
    unsigned ppqn,run_mode,step;
} OriginalApp;
void original_app_init(OriginalApp *app,uint8_t raw,uint32_t now);
/* Clock access must be serialized with the tick interrupt on MCU. */
void original_app_sync(OriginalApp *app);
void original_app_tick(OriginalApp *app);
void original_app_tick_at(OriginalApp *app,uint32_t now_us);
void original_app_input(OriginalApp *app,unsigned kind,uint32_t us);
/* Foreground-only UI snapshot; serialize with the tick interrupt. */
bool original_app_refresh_ui(OriginalApp *app);
#endif
