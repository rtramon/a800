#if defined(OPTION_6502_PIO)

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#include "memory.h"
#include "mos65C02.h"
#include "pico/stdlib.h"
#include "sm0_memory_emulation_with_clock.pio.h"

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

//
// GLOBALS
uint ticks;
uint pio1_offset = 0;

void mos65c02_init() {
    ticks = 0;

    // addressbus, databus  and rw are controlled by pio
    pio1_offset = pio_add_program(pio1, &memory_emulation_with_clock_program);
    memory_emulation_with_clock_program_init(pio1, 0, pio1_offset);
    pio_sm_set_enabled(pio1, 0, true);

    // reset, nmi and irq are controlled by sw
    gpio_init_mask((1ul << GP_RESET) | (1ul << GP_NMIB) | (1ul << GP_IRQB));
    gpio_set_dir_out_masked((1ul << GP_RESET) | (1ul << GP_NMIB) |
                            (1ul << GP_IRQB));

    gpio_set_mask((1ul << GP_NMIB) | (1ul << GP_IRQB));
    gpio_clr_mask((1ul << GP_RESET));
}

void __m6502_func(mos65c02_nmi)() {
    // 6502 NMI is edge triggered
    gpio_clr_mask(1ul << GP_NMIB);
    asm volatile("nop\nnop\nnop\nnop\n");
    gpio_set_mask(1ul << GP_NMIB);
}

void __m6502_func(mos65c02_reset)() {
    gpio_set_mask((1ul << GP_NMIB) | (1ul << GP_IRQB));

    gpio_clr_mask(1ul << GP_RESET);

    sleep_us(100);
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
    value.value = pio_sm_get_blocking(pio1, 0);
    // printf("a:%04x\n", value.value & 0x0000FFFF);
    if (value.data.flags & 0x8) {  // 65C02 read
        pio_sm_put(pio1, 0, mem_read(value.data.address));
    } else {
        uint8_t data = pio_sm_get_blocking(pio1, 0);
        mem_write(value.data.address, data);
    }
}
#endif