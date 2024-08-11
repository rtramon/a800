#ifndef _POKEY_H__
#define _POKEY_H__
#include <stdint.h>

// uncomment to enable sound (sort of) generation
// #define OPTION_AUDIO

void pokey_reset();
uint8_t pokey_read(uint8_t reg);
void pokey_write(uint8_t reg, uint8_t data);
void pokey_tick();

void pokey_report_keycode(uint8_t code);
void pokey_keyb_event(uint8_t code);
void pokey_report_break();

extern volatile uint8_t irqst_, irqen_, skstat_, kbcode_;

#endif
