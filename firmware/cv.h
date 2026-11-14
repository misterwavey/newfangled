/* SPDX-License-Identifier: MIT */
#ifndef NEWFANGLED_CV_H
#define NEWFANGLED_CV_H
#include <stdint.h>
#include <stdbool.h>
#define NF_CV_Q 32768
#define NF_CV_CONTROLS 5u
#define NF_GLOBAL_FIELDS 10u
#define NF_OUTPUT_FIELDS 22u
#define NF_MUTE_FIELD 19u
#define NF_COPY_FIELD 20u
#define NF_CLEAR_FIELD 21u
typedef struct {uint8_t source;int8_t depth,bias;} NfCvRoute;
typedef struct {uint16_t zero,positive;} NfCvCalibration;
typedef struct {NfCvRoute config;int32_t gain,offset;} NfCvPlan;
typedef struct {
    uint16_t raw[2];int32_t filtered[2],normalized[2];
    uint8_t raw_valid,valid,primed;
    bool swapped;
    NfCvCalibration calibration[2];
} NfCv;
void nf_cv_init(NfCv *cv);
void nf_cv_sample(NfCv *cv,unsigned adc0,unsigned adc1,unsigned valid);
void nf_cv_configure(NfCv *cv,bool swapped,const NfCvCalibration calibration[2]);
void nf_cv_normalize(NfCv *cv);
void nf_cv_plan(NfCvPlan *plan,NfCvRoute route);
unsigned nf_cv_apply(const NfCv *cv,const NfCvPlan *plan,unsigned base,unsigned current,unsigned maximum,unsigned minimum,bool discrete);
#endif
