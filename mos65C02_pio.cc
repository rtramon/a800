#if defined(OPTION_6502_PIO)

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#include "memory.h"
#include "mos65C02.h"
#include "pico/stdlib.h"
#include "sm0_memory_emulation_with_clock.pio.h"
#include "system.h"

// Externals

// NEO6502 board v1.0
#define GP_RESET 26  // RESB(40) <-- UEXT pin 3
#define GP_CLOCK 21  // PHI2
#define GP_RW 11     // RW#
#define GP_NMIB 27   // NMI
#define GP_IRQB 25   // IRQ
#define GP_BUZZ 20
#define GP_U5_OE 8
#define GP_U6_OE 9
#define GP_U7_OE 10

#define RESET_LOW false
#define RESET_HIGH true

#define ENABLE_LOW false
#define ENABLE_HIGH true

#define CLOCK_LOW false
#define CLOCK_HIGH true

#define RW_READ true
#define RW_WRITE false

#define DATA_OUTPUT 0xff
#define DATA_INPUT 0

// # of clock cycles to keep reset pin low
#define RESET_COUNT 100

#define PIO_M65C02 pio0
#define PIO_SM_M65C02 3
//
// GLOBALS
uint ticks;
uint pio_prog_offset = 0;

void mos65c02_init() {
    ticks = 0;

    // addressbus, databus  and rw are controlled by pio
    pio_prog_offset =
        pio_add_program(PIO_M65C02, &memory_emulation_with_clock_program);
    memory_emulation_with_clock_program_init(PIO_M65C02, PIO_SM_M65C02,
                                             pio_prog_offset);
    pio_sm_set_enabled(PIO_M65C02, PIO_SM_M65C02, true);

    // reset, nmi and irq are controlled by sw
    gpio_init_mask((1ul << GP_RESET) | (1ul << GP_NMIB) | (1ul << GP_IRQB));
    gpio_set_dir_out_masked((1ul << GP_RESET) | (1ul << GP_NMIB) |
                            (1ul << GP_IRQB));

    gpio_set_mask((1ul << GP_NMIB) | (1ul << GP_IRQB));
    gpio_clr_mask((1ul << GP_RESET));
}

bool nmi = false;
void __m6502_func(mos65c02_nmi)() {
    nmi = true;
    // 6502 NMI is edge triggered
    gpio_clr_mask(1ul << GP_NMIB);
    // delay a bit so that 6502 will recognize NMI
    // pico @ 252Mhz take 4 ns per cycle,
    // 6502 @ 2Mhz  requires > 60ns to detect NMI
    busy_wait_at_least_cycles(60 / 4);
    gpio_set_mask(1ul << GP_NMIB);
}

void __m6502_func(mos65c02_reset)() {
    gpio_set_mask((1ul << GP_NMIB) | (1ul << GP_IRQB));

    gpio_clr_mask(1ul << GP_RESET);

    sleep_us(RESET_COUNT);
    gpio_set_mask(1ul << GP_RESET);
    puts("RESET released");
}

void __m6502_func(mos65c02_tick)() {
    union u32 {
        uint32_t value;
        struct {
            uint16_t address;
            uint8_t flags;
        } data;
    } value;

    ticks = ticks + 1;

    value.value = pio_sm_get_blocking(PIO_M65C02, PIO_SM_M65C02);
    if (value.data.flags & 0x8) {  // 65C02 read
        pio_sm_put(PIO_M65C02, 3, mem_read(value.data.address));
    } else {
        uint8_t data = pio_sm_get_blocking(PIO_M65C02, PIO_SM_M65C02);
        mem_write(value.data.address, data);
    }
}
#endif