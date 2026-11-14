#include <stddef.h>
#include <stdint.h>
unsigned __aeabi_uidiv(unsigned n,unsigned d) {
    if(!d) return 0;
    unsigned q=0,bit=1;
    while(d<=n/2u && !(d&0x80000000u)) {d<<=1;bit<<=1;}
    while(bit) {if(n>=d) {n-=d;q|=bit;} d>>=1;bit>>=1;}
    return q;
}
void *memcpy(void *dest,const void *src,size_t n) {
    unsigned char *d=dest;const unsigned char *s=src;while(n--) *d++=*s++;return dest;
}
void *memset(void *dest,int v,size_t n) {unsigned char *d=dest;while(n--) *d++=(unsigned char)v;return dest;}
void __aeabi_memcpy(void *d,const void *s,size_t n) {(void)memcpy(d,s,n);}
void __aeabi_memcpy4(void *d,const void *s,size_t n) {(void)memcpy(d,s,n);}
void __aeabi_memclr(void *d,size_t n) {(void)memset(d,0,n);}
void __aeabi_memclr4(void *d,size_t n) {(void)memset(d,0,n);}
