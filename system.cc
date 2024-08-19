#include "system.h"

#include "antic.h"
#include "gtia.h"
#include "m6520.h"
#include "m6821.h"
#include "memory.h"
#include "pokey.h"
#ifdef EMU_M6502
#include "emu_m6502.h"
#else
#include "mos65C02.h"
#endif

#include "tusb.h"
#include "tusb_config.h"

// globals
bool GLreset = false;

#ifdef EMU_M6502
M6502 cpu;
#endif

uint runticks;

// external declarations
void load_rom(int);

void system_set_runticks(uint t) { runticks = t; }

void system_init() {
    antic_reset();
    gtia_reset();
    pokey_reset();
    m6520_reset();

    mem_reset();
    load_rom(0);

    // init 6502
    runticks = 100;  // allow some startup ticks
#if defined(EMU_M6502)
    puts("EMU 6502 Init");
    m6502_init(&cpu);
    cpu.read_byte = mem_read;
    cpu.write_byte = mem_write;
#else
    puts("NEO 65C02 Core");

    mos65c02_init();
#endif
}

void __m6502_func(system_reset)() {
    extern int rom;
    m6520_reset();
    gtia_reset();
    antic_reset();
    pokey_reset();

    mem_reset();
    load_rom(rom);

#if defined(EMU_M6502)
    puts("EMU 6502 Reset");
    m6502_gen_res(&cpu);
#else
    mos65c02_reset();
#endif
    GLreset = false;
}

void __m6502_func(system_run)() {
    while (!GLreset) {
        pokey_tick();

        if (runticks > 0) {
            runticks--;
#ifdef EMU_M6502
            m6502_step(&cpu);
#else
            mos65c02_tick();
#endif
        }
    }
    GLreset = false;
}
#if 0
void __m6502_func(system_run)() {
    while (!GLreset) {
        uint32_t start = timer_hw->timerawl;
        for (uint i = 0; i < 100; i++) {
            pokey_tick();
#ifdef EMU_M6502
            m6502_step(&cpu);
#else
            mos65c02_tick();
#endif
        }
        while (timer_hw->timerawl - start < 100'000 / 1789);
    }
    GLreset = false;
}
#endif