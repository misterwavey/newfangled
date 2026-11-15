/* SPDX-License-Identifier: MIT */
#ifndef NEWFANGLED_VOICE_H
#define NEWFANGLED_VOICE_H
#include <stdint.h>
#include <stdbool.h>
#define NF_RATIOS 30u
#define NF_SHAPES 7u
#define NF_FIELDS 6u
#define NF_CV_FIELDS 10u
typedef struct {uint16_t loop_beats;uint8_t logic,partner;} NfExtra;
typedef struct {uint8_t ratio,shape,width,phase,level,offset;} NfConfig;
typedef struct {uint8_t steps,hits,rotate,probability,loop;} NfRhythm;
#define NF_RHYTHM_DEFAULT ((NfRhythm){1,1,0,100,0})
typedef struct {uint8_t swing,ratchet;} NfGroove;
#define NF_GROOVE_DEFAULT ((NfGroove){0,1})
#define NF_SCALES 7u
typedef struct {uint8_t scale,root,range,slew;} NfTone;
#define NF_TONE_DEFAULT ((NfTone){0,0,1,0})
extern const char *const nf_scale_names[NF_SCALES],*const nf_note_names[12];
unsigned nf_pitch_note(NfTone tone,unsigned code);
typedef struct {NfConfig config;uint32_t phase,random;NfRhythm rhythm;
    NfExtra extra;uint32_t loop_phase;uint16_t held_pitch;bool pitch_held;
    uint32_t seed,chance,mask;uint8_t index;bool allowed,swing_odd;NfGroove groove;NfTone tone;
    int32_t slew_position;uint16_t slew_target,slew_remaining;bool slew_primed;} NfVoice;
unsigned nf_voice_note(const NfVoice *voice);
void nf_voice_tone(NfVoice *voice,NfTone tone);
uint16_t nf_voice_output(NfVoice *voice,bool running,unsigned bpm,bool tick);
uint32_t nf_rhythm_mask(NfRhythm rhythm);
unsigned nf_voice_ratchets(const NfVoice *voice,unsigned step);
void nf_voice_groove(NfVoice *voice,NfGroove groove);
void nf_voice_rhythm(NfVoice *voice,NfRhythm rhythm);
extern const uint8_t nf_multipliers[NF_RATIOS];
extern const uint16_t nf_divisors[NF_RATIOS];
extern const char *const nf_ratio_names[NF_RATIOS],*const nf_shape_names[NF_SHAPES];
void nf_voice_reset(NfVoice *voice,unsigned channel);
void nf_voice_retime(NfVoice *voice,unsigned ratio);
uint16_t nf_voice_value(const NfVoice *voice,bool running,unsigned bpm);
void nf_voice_tick(NfVoice *voice,unsigned bpm);
void nf_voice_advance(NfVoice *voice,unsigned step);
#endif
