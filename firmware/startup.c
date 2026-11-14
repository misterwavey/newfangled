#include <stdint.h>
extern uint32_t _estack,_sidata,_sdata,_edata,_sbss,_ebss;
int main(void);
void SysTick_Handler(void);
void EXTI4_15_IRQHandler(void);
void Default_Handler(void) { for(;;) __asm volatile("wfi"); }
void Reset_Handler(void) {
    uint32_t *src=&_sidata;
    for(uint32_t *p=&_sdata;p<&_edata;) *p++=*src++;
    for(uint32_t *p=&_sbss;p<&_ebss;) *p++=0;
    (void)main();Default_Handler();
}
__attribute__((section(".vectors"),used))
const uintptr_t vectors[48]={
    (uintptr_t)&_estack,(uintptr_t)Reset_Handler,
    (uintptr_t)Default_Handler,(uintptr_t)Default_Handler,
    0,0,0,0,0,0,0,(uintptr_t)Default_Handler,0,0,
    (uintptr_t)Default_Handler,(uintptr_t)SysTick_Handler,
    [16 ... 22]=(uintptr_t)Default_Handler,
    [23]=(uintptr_t)EXTI4_15_IRQHandler,
    [24 ... 47]=(uintptr_t)Default_Handler
};
