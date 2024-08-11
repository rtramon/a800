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

// external declarations
void load_rom(int);

void system_init() {
    antic_reset();
    gtia_reset();
    pokey_reset();
    m6520_reset();

    mem_reset();
    load_rom(0);

#if defined(EMU_M6502)
    puts("EMU 6502 Core");
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
    puts("EMU 6502 Core");
    m6502_init(&cpu);
    cpu.read_byte = mem_read;
    cpu.write_byte = mem_write;
#else
    mos65c02_reset();
#endif
    GLreset = false;
}

void __m6502_func(system_run)() {
    while (!GLreset) {
        pokey_tick();
#ifdef EMU_M6502
        m6502_step(&cpu);
#else
        mos65c02_tick();
#endif
    }
    GLreset = false;
}
