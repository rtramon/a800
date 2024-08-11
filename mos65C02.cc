#include "mos65C02.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#include "memory.h"
#include "pico/stdlib.h"

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

// mux bus enable pins
//                                2         1         0
//                              21098765432109876543210
// 8 - 10
constexpr uint32_t en_MASK = 0b00000000000011100000000;
constexpr uint32_t en_NONE = 0b00000000000011100000000;

// mask used for the mux address/data bus: GP0-7
constexpr uint32_t BUS_MASK = 0xFF;

constexpr uint32_t A0_7_OE = 1ul << 8;   // gpio for a0-7 output enable
constexpr uint32_t A8_15_OE = 1ul << 9;  // gpio for a8-15 output enable
constexpr uint32_t D0_7_OE = 1ul << 10;  // gpio for D0-7 output enable
constexpr uint32_t CLOCK_MASK = 1ul << GP_CLOCK;

// GPIO direction DATA in
constexpr uint32_t GPIO_DIR_DATA_IN_MASK =
    (1ul << GP_RESET) | (1ul << GP_CLOCK) | (1ul << GP_U5_OE) |
    (1ul << GP_U6_OE) | (1ul << GP_U7_OE);

// GPIO direction DATA out
constexpr uint32_t GPIO_DIR_DATA_OUT_MASK =
    (1ul << GP_RESET) | (1ul << GP_CLOCK) | (1ul << GP_U5_OE) |
    (1ul << GP_U6_OE) | (1ul << GP_U7_OE) | 0xFF;

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

#define DELAY_FACTOR_SHORT() asm volatile("nop\nnop\nnop\nnop\n");

#define DELAY_FACTOR_LONG() asm volatile("nop\nnop\nnop\nnop\nnop\nnop\n");

// # of clock cycles to keep reset pin low
#define RESET_COUNT 4
// external declarations
void video_putchar(uint8_t);

//
// GLOBALS
uint ticks;

void mos65c02_init() {
    ticks = 0;

    // CLOCK
    // gpio_init(GP_CLOCK);
    // gpio_set_dir(GP_CLOCK, GPIO_OUT);
    // gpio_put(GP_CLOCK, true);
    // // RESET
    // gpio_init(GP_RESET);
    // gpio_set_dir(GP_RESET, GPIO_OUT);
    // gpio_put(GP_RESET, false);
    // // RW
    // gpio_init(GP_RW);
    // gpio_set_dir(GP_RW, GPIO_IN);

    gpio_init_mask((1 << GP_CLOCK) | (1 << GP_RESET) | (1 << GP_RW) |
                   (1 << GP_NMIB) | (1 << GP_IRQB));
    gpio_set_dir_out_masked((1 << GP_CLOCK) | (1 << GP_RESET) | (1 << GP_NMIB) |
                            (1 << GP_IRQB));

    gpio_set_mask((1 << GP_CLOCK) | (1 << GP_NMIB) | (1 << GP_IRQB));
    gpio_clr_mask((1 << GP_RESET));

    gpio_set_dir_in_masked((1 << GP_RW));

    // // NMI
    // gpio_init(GP_NMIB);
    // gpio_set_dir(GP_NMIB, GPIO_OUT);

    // // IRQ
    // gpio_init(GP_IRQB);
    // gpio_set_dir(GP_IRQB, GPIO_OUT);

    // BUS ENABLE
    gpio_init_mask(en_MASK);
    gpio_set_dir_out_masked(en_MASK);  // enable as output
    gpio_set_mask(D0_7_OE | A0_7_OE |
                  A8_15_OE);  // disable latch d0-7 and a0-a7 and a8-a15

    // ADDRESS
    // DATA
    gpio_init_mask(BUS_MASK);
    // initialize databus for input
    gpio_set_dir_in_masked(BUS_MASK);
}

void __m6502_func(mos65c02_nmi)() {
    // 6502 NMI is edge triggered
    gpio_clr_mask(1ul << GP_NMIB);
    asm volatile("nop\nnop\nnop\nnop\n");
    gpio_set_mask(1ul << GP_NMIB);
}

void __m6502_func(mos65c02_reset)() {
    mos65c02_init();

    gpio_set_mask(D0_7_OE | A0_7_OE | A8_15_OE);  // disable all latch
    gpio_set_dir_in_masked(BUS_MASK);             // data gpio input

    gpio_set_mask((1ul << GP_CLOCK) | (1ul << GP_NMIB));

    gpio_clr_mask(1ul << GP_CLOCK);

    for (int i = 0; i < RESET_COUNT; i++) {
        DELAY_FACTOR_LONG();
        gpio_clr_mask(1ul << GP_CLOCK);
        DELAY_FACTOR_LONG();
        gpio_set_mask(1ul << GP_CLOCK);
    }
    DELAY_FACTOR_LONG();
    gpio_set_mask(1ul << GP_RESET);
    // puts("RESET released");
}

uint16_t prev_address;

void __m6502_func(mos65c02_tick)() {
    // uint8_t data;
    uint data;
    // set output enable of required latch first, to use
    // clocks for usefull work
    gpio_clr_mask(A8_15_OE | CLOCK_MASK);  // enable a0-a7 latch
    gpio_set_mask(D0_7_OE | A0_7_OE);      // disable latch d0-d7 and a8-a15

    // set databus gpios as inputs
    gpio_set_dir_in_masked(BUS_MASK);

    // increment tick counter here, avoiding useless nop's
    ticks = ticks + 1;

    // read A8 - A15
    uint32_t allbits = gpio_get_all();

    // prepare to read A0-7
    // disable latch d0-d7 and a0-a7 and set clock high
    gpio_set_mask(D0_7_OE | A8_15_OE | CLOCK_MASK);
    gpio_clr_mask(A0_7_OE);  // enable a8-a15 latch

    uint32_t address = (allbits & BUS_MASK) << 8;

    // optimization to enable d0-d7 latch now, latch takes 10ns to enable
    // in that time the address bus is read
    gpio_clr_mask(D0_7_OE);

    // read A7-15
    allbits = gpio_get_all();
    address |= (allbits & BUS_MASK);
    const bool rw = allbits & (1ul << GP_RW);

    // setup latches for databus operation
    gpio_set_mask(A0_7_OE | A8_15_OE);  // disable latch a0-a7 and a8-a15

    // do RW action
    if (rw) {
        // Read from memory
        data = mem_read(address);

        // set databus direction to output
        gpio_set_dir_out_masked(BUS_MASK);
        gpio_put_masked(BUS_MASK, (uint32_t)data);

    } else {
        // Write to Memory
        uint8_t data = (uint8_t)gpio_get_all();

        mem_write(address, data);
    }

    // prev_address = address;
}
