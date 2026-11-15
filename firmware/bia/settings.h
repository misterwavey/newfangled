/* SPDX-License-Identifier: MIT */
#ifndef BIA_SETTINGS_H
#define BIA_SETTINGS_H
#include "bia.h"
#include "../preset_format.h"
typedef BiaPatch NfSettings;
typedef BiaUi OriginalUi;
void nf_settings_capture(const OriginalUi *u,NfSettings *s);
void nf_settings_apply(OriginalUi *u,const NfSettings *s);
bool nf_settings_encode(const NfSettings *s,uint8_t *p);
bool nf_settings_decode_size(NfSettings *s,const uint8_t *p,unsigned n);
bool nf_settings_decode(NfSettings *s,const uint8_t *p);
#endif
