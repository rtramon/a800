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
extern int rom;
void load_rom(int);

void system_init() {
    antic_reset();
    gtia_reset();
    pokey_reset();
    m6520_reset();

    mem_reset();
    load_rom(0);

    // init 6502
    // system_set_cputicks(100000000);  // allow some startup ticks
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
#if defined(EMU_M6502)
    puts("EMU 6502 Reset");
    m6502_gen_res(&cpu);
#else
    mos65c02_reset();
#endif

    m6520_reset();
    gtia_reset();
    antic_reset();
    pokey_reset();

    mem_reset();
    load_rom(rom);

    GLreset = false;
}

void __m6502_func(system_wait_cputicks)(uint ticks) {
    // if (runticks < ticks) runticks = ticks;

    busy_wait_at_least_cycles(ticks * 252 / 1.79);
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
