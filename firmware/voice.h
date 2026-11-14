/* SPDX-License-Identifier: MIT */
#ifndef NEWFANGLED_VOICE_H
#define NEWFANGLED_VOICE_H
#include <stdint.h>
#include <stdbool.h>
#define NF_RATIOS 11u
#define NF_SHAPES 5u
#define NF_FIELDS 6u
typedef struct {uint8_t ratio,shape,width,phase,level,offset;} NfConfig;
typedef struct {NfConfig config;uint32_t phase,random;} NfVoice;
extern const uint8_t nf_multipliers[NF_RATIOS],nf_divisors[NF_RATIOS];
extern const char *const nf_ratio_names[NF_RATIOS],*const nf_shape_names[NF_SHAPES];
void nf_voice_reset(NfVoice *voice,unsigned channel);
uint16_t nf_voice_value(const NfVoice *voice,bool running,unsigned bpm);
void nf_voice_tick(NfVoice *voice,unsigned bpm);
void nf_voice_advance(NfVoice *voice,unsigned step);
#endif
