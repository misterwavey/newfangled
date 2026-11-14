#include "pnw_board.h"
#include "gate_bus.h"
#define REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define A 0x48000000u
#define B 0x48000400u
#define RCC 0x40021000u
#define ADC 0x40012400u
static volatile uint32_t ticks;
/* Debugger-visible timeout stage; a halt cannot guarantee that latched DAC
   outputs return to zero when the bus itself has failed. */
volatile uint32_t pnw_fault_stage;
static void await_bits(uint32_t address,uint32_t mask,uint32_t value,unsigned stage) {
    for(unsigned limit=0;limit<1000000;limit++) if((REG(address)&mask)==value) return;
    pnw_fault_stage=stage;
    for(;;) __asm volatile("wfi");
}

/* EXTI handlers only capture events; synthesis and SPI stay in SysTick. */
static struct {uint32_t kind,us;} input_events[16];
static volatile uint32_t input_head,input_tail,input_overruns;
uint32_t pnw_micros(void) {return REG(0x40000024u);}
uint32_t pnw_input_overruns(void) {return input_overruns;}
static void input_push(uint32_t kind,uint32_t us) {
    uint32_t next=(input_head+1u)&15u;
    if(next==input_tail) {++input_overruns;return;}
    input_events[input_head].kind=kind;input_events[input_head].us=us;
    __asm volatile("dmb":::"memory");input_head=next;
}
bool pnw_input_take(uint32_t *kind,uint32_t *us) {
    uint32_t saved=pnw_interrupt_lock();bool ready=input_tail!=input_head;
    if(ready) {*kind=input_events[input_tail].kind;*us=input_events[input_tail].us;input_tail=(input_tail+1u)&15u;}
    pnw_interrupt_restore(saved);return ready;
}
void pnw_inputs_discard(void) {
    uint32_t saved=pnw_interrupt_lock();input_tail=input_head;
    REG(0x40010414u)=(1u<<8)|(1u<<10);
    input_push(pnw_run_input()?2u:3u,pnw_micros());pnw_interrupt_restore(saved);
}
void EXTI4_15_IRQHandler(void) {
    uint32_t pending=REG(0x40010414u)&((1u<<8)|(1u<<10));
    uint32_t us=pnw_micros();REG(0x40010414u)=pending;
    /* Run first when both arrive together, so a coincident gate can arm clock. */
    if(pending&(1u<<8))input_push(pnw_run_input()?2u:3u,us);
    if(pending&(1u<<10))input_push(1u,us);
}
static uint8_t pixels[512];
static PnwGateBus gate_bus;
static uint16_t output_pending[8];
static volatile bool outputs_waiting;
static volatile uint32_t output_overruns;
static bool output_timed;
static void outputs_queue(const uint16_t codes[8],bool timed) {
    uint32_t saved=pnw_interrupt_lock();bool changed=false;
    for(unsigned i=0;i<8;++i)if(output_pending[i]!=codes[i])changed=true;
    /* Only two successive scheduled samples imply a missed delivery deadline.
       Foreground parameter/transport changes deliberately supersede old values. */
    if(timed && outputs_waiting && output_timed)++output_overruns;
    if(changed) {
        for(unsigned i=0;i<8;++i)output_pending[i]=codes[i];
        outputs_waiting=true;
    }
    if(outputs_waiting && (timed || changed))output_timed=timed;
    pnw_interrupt_restore(saved);
}
void pnw_outputs_request(const uint16_t codes[8]) {outputs_queue(codes,true);}
void pnw_outputs_control(const uint16_t codes[8]) {outputs_queue(codes,false);}
uint32_t pnw_interrupt_lock(void) {
    uint32_t saved;__asm volatile("mrs %0, primask\n cpsid i":"=r"(saved)::"memory");return saved;
}
void pnw_interrupt_restore(uint32_t saved) {__asm volatile("msr primask, %0"::"r"(saved):"memory");}
__attribute__((weak)) void pnw_tick_hook(void) {}
void SysTick_Handler(void) {++ticks;pnw_tick_hook();pnw_gate_service();}
static bool bus_try_begin(void) {
    uint32_t saved=pnw_interrupt_lock();bool acquired=pnw_bus_acquire(&gate_bus);pnw_interrupt_restore(saved);return acquired;
}
static void bus_begin(void) {
    if(bus_try_begin())return;
    pnw_fault_stage=13;for(;;)__asm volatile("wfi");
}
static void bus_unlock(void) {
    uint32_t saved=pnw_interrupt_lock();pnw_bus_release(&gate_bus);pnw_interrupt_restore(saved);
}
static void bus_end(void) {bus_unlock();pnw_gate_service();}
void pnw_gate_request(bool high) {
    uint32_t saved=pnw_interrupt_lock();pnw_bus_request(&gate_bus,high);pnw_interrupt_restore(saved);
}
uint32_t pnw_gate_overruns(void) {return gate_bus.overruns+output_overruns;}
uint32_t pnw_millis(void) { return ticks; }
void pnw_delay(uint32_t ms) { uint32_t start=ticks; while((uint32_t)(ticks-start)<ms) __asm volatile("nop"); }
static void pin(uint32_t port,unsigned n,bool high) { REG(port+0x18)=1u<<(n+(high?0:16)); }
static void mode(uint32_t port,unsigned n,unsigned m,unsigned pull) {
    REG(port)=(REG(port)&~(3u<<(2*n)))|(m<<(2*n));
    REG(port+0xc)=(REG(port+0xc)&~(3u<<(2*n)))|(pull<<(2*n));
}
static void deselect(void) { pin(A,4,true); pin(A,15,true); pin(B,3,true); }
static void spi_mode(bool falling_sample) {
    await_bits(0x40013008,128,0,10);
    REG(0x40013000)=0x31c | (falling_sample?1u:0u); /* master, /16 = 3 MHz at 48 MHz, software NSS */
    REG(0x40013004)=0x1700; /* 8-bit FIFO threshold and data size */
    REG(0x40013000)|=64;
}
static uint8_t transfer(uint8_t value) {
    await_bits(0x40013008,2,2,11);
    *(volatile uint8_t *)(uintptr_t)0x4001300c=value;
    await_bits(0x40013008,1,1,12);
    uint8_t received=*(volatile uint8_t *)(uintptr_t)0x4001300c;
    await_bits(0x40013008,128,0,10);
    return received;
}
static void dac_word_locked(uint16_t value) {
    deselect(); spi_mode(true); pin(B,3,false);
    transfer((uint8_t)(value>>8)); transfer((uint8_t)value);
    pin(B,3,true);
}
static void dac_word(uint16_t value) {bus_begin();dac_word_locked(value);bus_end();}
void pnw_gate_service(void) {
    /* A tick can queue a newer frame while foreground SPI owns the bus.
       Drain it after unlock, before returning to potentially lengthy rendering.
       The interrupt-side service returns immediately when another owner is busy. */
    for(;;) {
        uint16_t snapshot[8];uint32_t guard=pnw_interrupt_lock();
        bool frame=outputs_waiting && pnw_bus_acquire(&gate_bus);
        if(frame) {for(unsigned i=0;i<8;++i)snapshot[i]=output_pending[i];outputs_waiting=false;}
        pnw_interrupt_restore(guard);
        if(frame) {
            /* One ownership interval and one common latch for the complete frame. */
            for(unsigned i=0;i<8;++i)dac_word_locked((uint16_t)((i<<12)|(snapshot[i]<<2)));
            dac_word_locked(0xa0ff);bus_unlock();continue;
        }
        bool high;uint32_t saved=pnw_interrupt_lock();bool ready=pnw_bus_take(&gate_bus,&high);pnw_interrupt_restore(saved);
        if(!ready)return;
        /* Own the bus across both the channel write and DAC latch. Interrupts can
           still advance the clock and queue the next level during these writes. */
        dac_word_locked((uint16_t)(high?1023u<<2:0));dac_word_locked(0xa0ff);
        bus_unlock();
    }
}
bool pnw_dac_set(unsigned channel,unsigned code) {
    uint16_t frame;
    if(!pnw_dac_frame(channel,code,&frame)) return false;
    dac_word(frame); return true;
}
void pnw_dac_commit(void) { dac_word(0xa0ff); }
void pnw_dac_zero(void) { for(unsigned i=0;i<8;i++) (void)pnw_dac_set(i,0); pnw_dac_commit(); }
static void command(uint8_t value) {
    bus_begin();deselect(); spi_mode(false); pin(A,3,false); pin(A,4,false); transfer(value); pin(A,4,true);bus_end();
}
void pnw_oled_clear(void) { for(unsigned i=0;i<512;i++) pixels[i]=0; }
void pnw_oled_pixel(unsigned x,unsigned y,bool on) {
    if(x>=128 || y>=32) return;
    unsigned pos=(y>>3)*128+x; uint8_t mask=(uint8_t)(1u<<(y&7));
    if(on) pixels[pos]|=mask; else pixels[pos]&=(uint8_t)~mask;
}
void pnw_oled_flush_page(unsigned page) {
    if(page>=4) return;
    command(0x21); command(0); command(127); command(0x22); command((uint8_t)page); command((uint8_t)page);
    /* Release shared SPI every 16 bytes, not after an entire frame. */
    for(unsigned start=0;start<128;start+=16) {
        bus_begin();deselect();spi_mode(false);pin(A,3,true);pin(A,4,false);
        for(unsigned i=0;i<16;++i)transfer(pixels[page*128+start+i]);
        pin(A,4,true);bus_end();
    }
}
void pnw_oled_flush(void) { for(unsigned page=0;page<4;page++) pnw_oled_flush_page(page); }
void pnw_oled_number(unsigned x,unsigned value) {
    static const uint8_t digits[10][3]={{31,17,31},{18,31,16},{29,21,23},{21,21,31},{7,4,31},{23,21,29},{31,21,29},{1,1,31},{31,21,31},{23,21,31}};
    unsigned place=100;
    for(unsigned d=0;d<3;d++) {
        unsigned n=0; while(value>=place && n<9) {value-=place;n++;}
        for(unsigned col=0;col<3;col++) for(unsigned row=0;row<5;row++)
            for(unsigned dx=0;dx<3;dx++) for(unsigned dy=0;dy<3;dy++)
                pnw_oled_pixel(x+d*12+col*3+dx,8+row*3+dy,(digits[n][col]&(1u<<row))!=0);
        place=(d==0)?10:1;
    }
}
uint8_t pnw_controls_raw(void) {
    uint32_t v=REG(B+0x10);
    /* Rev 62 buttons are active high; present active-low logical controls to
       the portable UI. Encoder quadrature levels remain unchanged. */
    return (uint8_t)((((v>>6)&1)|(((v>>7)&1)<<1)|(((v>>5)&1)<<2)|(((v>>4)&1)<<3))^12u);
}
bool pnw_clock_input(void) { return (REG(A+0x10)&(1u<<10))==0; }
bool pnw_run_input(void) { return (REG(A+0x10)&(1u<<8))==0; }
void pnw_expander(bool run,bool clock) { pin(A,0,run); pin(A,2,clock); }
bool pnw_midi_send(uint8_t byte) {
    static bool initialized;
    if(!initialized) {
        REG(RCC+0x18)|=1u<<14;
        REG(A+0x24)=(REG(A+0x24)&~0xf0u)|0x10u; /* PA9 AF1 */
        mode(A,9,2,0);
        REG(0x40013800)=0;REG(0x4001380c)=1536;REG(0x40013800)=9;
        initialized=true;
    }
    unsigned timeout=100000;
    while(!(REG(0x4001381c)&128) && --timeout) {}
    if(!timeout) return false;
    REG(0x40013828)=byte;return true;
}
static volatile uint16_t cv_samples[2];
static volatile uint8_t cv_valid;
static volatile bool cv_ready;
static bool cv_waiting;
static unsigned cv_channel,cv_wait_ticks;
uint16_t pnw_cv_raw(unsigned input) {return input<2?cv_samples[input]:0;}
unsigned pnw_cv_valid(void) {return cv_valid;}
void pnw_cv_service(void) {
    if(!cv_ready)return;
    if(cv_waiting) {
        if(!(REG(ADC)&4u)) {if(++cv_wait_ticks>20)cv_valid=0;return;}
        cv_samples[cv_channel]=(uint16_t)(REG(ADC+0x40)&4095u);cv_valid|=(uint8_t)(1u<<cv_channel);
        cv_channel^=1u;cv_waiting=false;
    }
    if(REG(ADC+8)&4u)return;
    REG(ADC+0x28)=1u<<(8u+cv_channel);REG(ADC)=4u|8u|16u;REG(ADC+8)|=4u;
    cv_waiting=true;cv_wait_ticks=0;
}
static uint8_t status(void) {
    bus_begin();deselect();spi_mode(false);pin(A,15,false);transfer(5);
    uint8_t result=transfer(0);pin(A,15,true);bus_end();return result;
}
static bool ready(void) {
    uint32_t start=ticks;
    while(status()&1) if((uint32_t)(ticks-start)>20) return false;
    return true;
}
bool pnw_eeprom_read(uint16_t address,void *data,size_t length) {
    if((!data && length) || length>65536u-address || !ready()) return false;
    uint8_t *p=data;
    while(length) {
        unsigned count=length>16?16:(unsigned)length;
        bus_begin();deselect();spi_mode(false);pin(A,15,false);transfer(3);transfer((uint8_t)(address>>8));transfer((uint8_t)address);
        for(unsigned i=0;i<count;++i)*p++=transfer(0);
        pin(A,15,true);bus_end();address=(uint16_t)(address+count);length-=count;
    }
    return true;
}
bool pnw_eeprom_write(uint16_t address,const void *data,size_t length) {
    if((!data && length) || length>65536u-address) return false;
    const uint8_t *p=data;
    while(length) {
        if(!ready()) return false;
        bus_begin();deselect();spi_mode(false);pin(A,15,false);transfer(6);pin(A,15,true);bus_end();
        if(!(status()&2) || (status()&0x0c)) return false;
        unsigned count=128u-(address&127u);if(count>length) count=(unsigned)length;
        bus_begin();deselect();spi_mode(false);pin(A,15,false);transfer(2);transfer((uint8_t)(address>>8));transfer((uint8_t)address);
        for(unsigned i=0;i<count;i++) transfer(*p++);
        pin(A,15,true);bus_end();address=(uint16_t)(address+count);length-=count;
    }
    return ready();
}
void pnw_board_init(void) {
    /* Deterministic internal HSI8 / 2 * 12 = 48 MHz; external HSE unused. */
    REG(RCC)|=1; await_bits(RCC,2,2,1);
    /* Normalize inherited clock state before changing PLL fields. */
    REG(RCC+4)&=~3u;await_bits(RCC+4,12,0,2);
    REG(RCC)&=~(1u<<24);await_bits(RCC,1u<<25,0,3);
    REG(0x40022000)=0x11;
    REG(RCC+4)=(REG(RCC+4)&~0x003fffffu)|(10u<<18);
    REG(RCC)|=1u<<24;await_bits(RCC,1u<<25,1u<<25,4);
    REG(RCC+4)=(REG(RCC+4)&~3u)|2;await_bits(RCC+4,12,8,5);
    REG(RCC+0x14)|=(1u<<17)|(1u<<18);
    /* Establish idle latch values before switching to output. */
    deselect();pin(A,5,false);pin(A,7,false);pin(A,1,true);pin(A,3,false);
    pin(A,0,false);pin(A,2,false);
    const unsigned outputs[]={0,1,2,3,4,5,7,15};
    for(unsigned i=0;i<sizeof(outputs)/sizeof(outputs[0]);i++) mode(A,outputs[i],1,0);
    mode(B,3,1,0);
    for(unsigned i=5;i<=7;i++) mode(A,i,2,0);
    REG(A+0x20)&=~0xfff00000u; /* AF0 SPI1 on PA5/6/7 */
    REG(RCC+0x18)|=1u<<12;spi_mode(false);
    for(unsigned i=4;i<=7;i++) mode(B,i,0,1);
    mode(A,8,0,0);mode(A,10,0,0);
    mode(B,0,3,0);mode(B,1,3,0);
    REG(0xe000e014)=47999;REG(0xe000e018)=0;REG(0xe000e010)=7;
    dac_word(0x8000);pnw_dac_zero();
    REG(RCC+0x18)|=1u<<9; /* ADC clock: HSI14 */
    REG(RCC+0x34)|=1;await_bits(RCC+0x34,2,2,6);
    REG(ADC+8)=1u<<31;await_bits(ADC+8,1u<<31,0,7);
    for(volatile unsigned settle=0;settle<64;settle++) __asm volatile("nop");
    REG(ADC+0xc)=0;REG(ADC+0x14)=7;REG(ADC)=1;REG(ADC+8)=1;
    await_bits(ADC,1,1,8);cv_ready=true;
    pin(A,1,false);pnw_delay(2);pin(A,1,true);pnw_delay(2);
    static const uint8_t init[]={0xae,0xa8,31,0xd3,0,0x40,0xda,2,0x20,0,0xa1,0xc8,0x81,0x8f,0xa6,0xdb,0x40,0xa4,0xd5,0x80,0x8d,0x14,0x2e,0xaf};
    for(unsigned i=0;i<sizeof(init);i++) command(init[i]);
    pnw_oled_clear();pnw_oled_flush();
    REG(RCC+0x1c)|=1u; /* APB1 TIM2, 48 MHz on the configured APB clock. */
    REG(RCC+0x10)|=1u;REG(RCC+0x10)&=~1u;
    REG(0x40000028)=47;REG(0x4000002c)=0xffffffffu;REG(0x40000014)=1;REG(0x40000024)=0;REG(0x40000000)=1;
    REG(RCC+0x18)|=1u; /* SYSCFG clock; EXTI8/10 route to port A. */
    REG(0x40010010)&=~0x0f0fu;
    REG(0x40010400)&=~((1u<<8)|(1u<<10));
    REG(0x40010408)=(REG(0x40010408)&~(1u<<10))|(1u<<8);
    REG(0x4001040c)|=(1u<<8)|(1u<<10);
    REG(0x40010414)=(1u<<8)|(1u<<10);
    /* EXTI priority 0 may preempt synthesis; the event ring is its only output. */
    *(volatile uint8_t *)(uintptr_t)0xe000e407=0;
    *(volatile uint8_t *)(uintptr_t)0xe000ed23=0x80;
    input_push(pnw_run_input()?2u:3u,pnw_micros());
    REG(0x40010400)|=(1u<<8)|(1u<<10);REG(0xe000e100)=1u<<7;
}
