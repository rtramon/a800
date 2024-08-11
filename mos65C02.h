#ifndef _MOS65C02_H_
#define _MOS65C02_H_
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/timer.h"

#define __m6502_func(f) __scratch_y(__STRING(f)) f

#define GP_NMIB 27  // NMI
#define GP_IRQB 25  // IRQ

extern uint ticks;

//
// Function declarations
void mos65c02_init();
void mos65c02_reset();
void mos65c02_tick();
void mos65c02_nmi();
void mos65c02_irq_on();
void mos65c02_irq_off();

//
// inline functions to avoid expensive trampoline/veneer
// function call stuff
void inline mos65c02_irq_on() { gpio_clr_mask(1ul << GP_IRQB); }

void inline mos65c02_irq_off() { gpio_set_mask(1ul << GP_IRQB); }

#endif
