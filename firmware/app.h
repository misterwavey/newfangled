/* SPDX-License-Identifier: MIT */
#ifndef ORIGINAL_APP_H
#define ORIGINAL_APP_H
#include "clock.h"
#include "ui.h"
typedef struct {OriginalUi ui;OriginalClock clock;uint32_t reset_seen;NfVoice voices[8];uint16_t output[8];} OriginalApp;
void original_app_init(OriginalApp *app,uint8_t raw,uint32_t now);
/* Clock access must be serialized with the tick interrupt on MCU. */
void original_app_sync(OriginalApp *app);
void original_app_tick(OriginalApp *app);
#endif
