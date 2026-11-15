/* SPDX-License-Identifier: MIT */
#ifndef BIA_APPLICATION_H
#define BIA_APPLICATION_H
#include <stdbool.h>
#include <stdint.h>
#include "../sync.h"
#include "../cv.h"
#define BIA_STEPS 16u
#define BIA_SCENES 8u
#define BIA_LANES 7u
/* OUT1 trigger; OUT2..8 Pitch, Decay, Harm, Fold, Morph, Spread, Attack. */
typedef struct {uint8_t scene,probability,ratchets,enabled;} BiaStep;
enum {BIA_FILL,BIA_ACCENT,BIA_VARIATION,BIA_EFFECTS};
typedef struct {int8_t envelope[2];uint8_t decay[2];int8_t accent[3];uint8_t variation[6];bool retrigger;} BiaMotion;
typedef struct {
    uint16_t bpm,scene[8][7];BiaStep steps[16];
    uint8_t variation,delay_ms,pulse_ms,swing,external,ppqn_index,run_mode,cv_macro;
    NfCvCalibration calibration[2];uint8_t swapped;
    uint8_t step_accent[16];uint16_t fill_mask;BiaMotion motion[8];
    uint8_t effect,strength[3];
} BiaPatch;
typedef struct {
    BiaPatch patch;unsigned page,field;bool editing,selecting,running,active,auditioning,timing_fault;
    unsigned playing_step,effective_ratchets,sync_status,external_bpm;
    uint8_t previous,candidate,stable;int encoder;uint32_t changed_at,program_at;
    bool consumed,performance,performance_page,start_pending,start_consumed,start_playback,start_audition;
    uint32_t start_at;
    uint32_t reset_count,audition_count,cancel_count;uint8_t audition_scene;
    bool preset_menu,initialize_confirm,initialize_yes;
    uint8_t preset_slot,preset_field,preset_command,preset_status,preset_mask;
    uint16_t cv_raw[2];uint8_t cv_valid;
} BiaUi;
typedef struct {
    BiaUi ui;BiaPatch patch;NfSync sync;NfCv cv;
    uint16_t output[8],held[7],adc[2];uint8_t adc_valid;
    uint32_t now_us,phase,seed,reset_seen,audition_seen,cancel_seen;
    unsigned index,age,duration,lead,pulse,spacing,ratchets;
    bool running,requested,run_high,reset_input,event,auditioning;
    bool performance,fill_event,fill_boost,retrigger;uint8_t effect,amount,scene,accent,variation;
    uint8_t limits[6];int16_t deviation[6],accent_delta[3],envelope[2];uint16_t decay[2];
    unsigned base_ratchets,envelope_age;
} BiaApp;
extern const char *const bia_lane_names[7],*const bia_scene_names[8];
void bia_defaults(BiaPatch *p);
void bia_legacy_extras(BiaPatch *p);
bool bia_patch_valid(const BiaPatch *p);
void bia_ui_init(BiaUi *u,uint8_t raw,uint32_t now);
bool bia_ui_update(BiaUi *u,uint8_t raw,uint32_t now);
void bia_ui_render(const BiaUi *u);
void bia_init(BiaApp *a,uint8_t raw,uint32_t now);
void bia_sync(BiaApp *a);
void bia_tick(BiaApp *a,uint32_t now_us);
void bia_input(BiaApp *a,unsigned kind,uint32_t us);
void bia_cv(BiaApp *a,unsigned adc0,unsigned adc1,unsigned valid);
bool bia_refresh(BiaApp *a);

/* Keep the saved percentage encoding compatible; expose six musical levels. */
unsigned bia_fill_level(unsigned strength);
unsigned bia_fill_strength(unsigned level);

#endif
