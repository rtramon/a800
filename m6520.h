#ifndef _M6520_H
#define _M6520_H
#include <stdint.h>

void m6520_reset();
uint8_t m6520_read(uint8_t reg);
void m6520_write(uint8_t reg, uint8_t data);

void m6520_joy0(uint8_t);

extern uint8_t regPORTB;
extern volatile uint8_t regPORTA;
extern bool select_selftestrom;
extern bool select_basicrom;

#endif