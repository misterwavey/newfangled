/* SPDX-License-Identifier: MIT
 * Additive, bounded modulation. No inferred voltage calibration or vendor data.
 */
#include "cv.h"
static int32_t signed_divide(int32_t n,uint32_t d) {
    return n<0?-(int32_t)((uint32_t)(-n)/d):(int32_t)((uint32_t)n/d);
}
void nf_cv_init(NfCv *cv) {
    *cv=(NfCv){.calibration={{0,4095},{2048,4095}}};
}
void nf_cv_configure(NfCv *cv,bool swapped,const NfCvCalibration calibration[2]) {
    if(cv->swapped!=swapped) {cv->primed=0;cv->raw_valid=0;cv->valid=0;}
    cv->swapped=swapped;
    for(unsigned i=0;i<2;++i)cv->calibration[i]=calibration[i];
}
void nf_cv_sample(NfCv *cv,unsigned adc0,unsigned adc1,unsigned valid) {
    unsigned physical[2]={adc0>4095?4095:adc0,adc1>4095?4095:adc1};
    cv->raw_valid=0;
    for(unsigned i=0;i<2;++i) {
        unsigned pin=i^(unsigned)cv->swapped,bit=1u<<i;
        cv->raw[i]=(uint16_t)physical[pin];
        if(!(valid&(1u<<pin))) {cv->primed&=(uint8_t)~bit;continue;}
        cv->raw_valid|=(uint8_t)bit;
        int32_t target=(int32_t)cv->raw[i]*256;
        if(!(cv->primed&bit)) {cv->filtered[i]=target;cv->primed|=(uint8_t)bit;}
        else cv->filtered[i]+=(target-cv->filtered[i])/8;
    }
}
void nf_cv_normalize(NfCv *cv) {
    cv->valid=0;
    for(unsigned i=0;i<2;++i) {
        int32_t span=(int32_t)cv->calibration[i].positive-cv->calibration[i].zero;
        cv->normalized[i]=0;
        if(!(cv->raw_valid&(1u<<i)) || (span>-64 && span<64))continue;
        int32_t n=((cv->filtered[i]+128)/256-(int32_t)cv->calibration[i].zero)*NF_CV_Q;
        if(span<0) {span=-span;n=-n;}
        int32_t value=signed_divide(n,(uint32_t)span),minimum=i?-NF_CV_Q:0;
        if(value<minimum)value=minimum;
        if(value>NF_CV_Q)value=NF_CV_Q;
        cv->normalized[i]=value;cv->valid|=(uint8_t)(1u<<i);
    }
}
void nf_cv_plan(NfCvPlan *plan,NfCvRoute route) {
    if(route.source>2)route.source=0;
    if(route.depth>100)route.depth=100;
    if(route.depth< -100)route.depth=-100;
    if(route.bias>100)route.bias=100;
    if(route.bias< -100)route.bias=-100;
    if(plan->config.source==route.source && plan->config.depth==route.depth && plan->config.bias==route.bias)return;
    plan->config=route;
    plan->gain=signed_divide((int32_t)route.depth*NF_CV_Q,100);
    plan->offset=signed_divide((int32_t)route.bias*NF_CV_Q,100);
}
unsigned nf_cv_apply(const NfCv *cv,const NfCvPlan *p,unsigned base,unsigned current,unsigned maximum,unsigned minimum,bool discrete) {
    unsigned source=p->config.source;
    if(!source || !(cv->valid&(1u<<(source-1))))return base;
    int32_t modulation=(cv->normalized[source-1]*p->gain)/NF_CV_Q+p->offset;
    int32_t value=(int32_t)base*NF_CV_Q+modulation*(int32_t)(maximum-minimum);
    int32_t low=(int32_t)minimum*NF_CV_Q,high=(int32_t)maximum*NF_CV_Q;
    if(value<low)value=low;
    if(value>high)value=high;
    unsigned rounded=(unsigned)(value+NF_CV_Q/2)/NF_CV_Q;
    if(discrete) {
        /* 0.1 bin hysteresis around the usual half-bin rounding threshold. */
        int32_t position=(int32_t)current*NF_CV_Q;
        if(rounded>current && value<position+NF_CV_Q/2+NF_CV_Q/10)return current;
        if(rounded<current && value>position-NF_CV_Q/2-NF_CV_Q/10)return current;
    }
    return rounded;
}
