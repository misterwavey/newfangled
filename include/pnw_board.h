#ifndef PNW_BOARD_H
#define PNW_BOARD_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* General SPI APIs are foreground-only. The dedicated gate service below
   arbitrates short clock writes with foreground transactions. */
void pnw_board_init(void);
uint32_t pnw_millis(void);
uint32_t pnw_micros(void); /* Free-running 32-bit 1 MHz TIM2; wraps in ~71 minutes. */
/* Events: 1 clock rising at jack, 2 Run high, 3 Run low. Captured by EXTI. */
bool pnw_input_take(uint32_t *kind,uint32_t *us);
uint32_t pnw_input_overruns(void);
void pnw_delay(uint32_t ms);
bool pnw_dac_frame(unsigned channel, unsigned code, uint16_t *frame);
bool pnw_dac_set(unsigned channel, unsigned code);
void pnw_dac_commit(void);
void pnw_dac_zero(void);
uint32_t pnw_interrupt_lock(void);
void pnw_interrupt_restore(uint32_t saved);
void pnw_gate_request(bool high);
/* Eight 10-bit codes, queued atomically and latched together by gate_service.
 * request: one scheduled sample per tick; control: intentional immediate edit.
 * Only an undelivered scheduled sample at the next sample counts as an overrun. */
void pnw_outputs_request(const uint16_t codes[8]);
void pnw_outputs_control(const uint16_t codes[8]); /* output 1 only; post desired level */
void pnw_gate_service(void); /* nonblocking if foreground owns SPI */
uint32_t pnw_gate_overruns(void);
void pnw_tick_hook(void); /* weak, called once per 1 ms interrupt */
void pnw_oled_clear(void);
void pnw_oled_pixel(unsigned x, unsigned y, bool on);
void pnw_oled_flush(void);
void pnw_oled_flush_page(unsigned page);
void pnw_oled_number(unsigned x, unsigned value);
uint8_t pnw_controls_raw(void); /* b0 PB6, b1 PB7 unchanged; b2 Program/PB5, b3 Start/PB4 normalized active low */
/* Cached ADC8/PB0 and ADC9/PB1 samples, not calibrated volts. service is
 * nonblocking, called once per tick; each physical ADC channel samples at 500 Hz. */
uint16_t pnw_cv_raw(unsigned input);
unsigned pnw_cv_valid(void);
void pnw_cv_service(void);
bool pnw_clock_input(void); /* PA10 inverted: true means jack high */
bool pnw_run_input(void); /* PA8 inverted: true means jack high */
/* Conservative UART-free foreground expander signals, PA0 and PA2. */
void pnw_expander(bool run, bool clock);
/* Optional expander UART: PA9 / USART1, 31250 baud. Foreground only. */
bool pnw_midi_send(uint8_t byte);
bool pnw_eeprom_read(uint16_t address, void *data, size_t length);
/* Explicitly destructive to saved module data. No demo calls this. */
bool pnw_eeprom_write(uint16_t address, const void *data, size_t length);
#endif
