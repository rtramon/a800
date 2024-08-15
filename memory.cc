#include "memory.h"

#include <stdio.h>

#include <cstring>

#include "antic.h"
#include "gtia.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "m6520.h"
#include "pico/platform.h"
#include "pokey.h"

// atari memory map info
// 0000 - BFFF  RAM
// 5000 - 57FF  SelfTest ROM bank switched
// D000 - D7FF reserved for custom chips
// 8000 - 9FFF  right cartridge
// A000 - BFFF  left cartridge
// D000 - D01F  GTIA
// D200 - D20F  Pokey
// D210 - D2FF  Pokey shadow
// D400 - D40F  ANTIC
// D410 - D41F  ANTIC Shadow
// C000 - FFFF  OS ROM (16k)

// Externals
extern uint8_t ataribas_rom[];
extern uint8_t altirra_basic_rom[];
extern uint8_t boulderdash_rom[];
extern uint8_t donkeykong_rom[];
extern uint8_t pengo_rom[];
extern uint8_t miner2049_rom[];
extern uint8_t spaceinvaders_rom[];
extern uint8_t pacman_rom[];
extern uint8_t mspacman_rom[];

//  64k RAM
uint8_t mem[SIZE_64K];
uint8_t basic_rom[SIZE_8K];
// uint8_t* basic_rom = nullptr;

uint8_t* cart_rom = nullptr;
uint16_t cart_start = 0xFFFF;

// statistics
uint read_cnt = 0;
uint write_cnt = 0;

void inline NOP_LOOP(uint x) {
    while (x--) asm volatile("nop\n");
}

void mem_reset() { memset(mem, 0, sizeof(mem)); }

uint8_t* __m6502_func(mem_read_ptr)(uint16_t address) {
    if (address < 0xC000) {
        if (trig3_ == 1) {
            if (address >= cart_start)
                return &cart_rom[address - cart_start];  // mem[address];
        }

        if (address >= 0xA000) {
            if ((regPORTB & (1 << 1)) == 0) {
                // printf("R BASIC ROM $%04x\n", address);
                return &basic_rom[address - 0xA000];
            }
        }
        if ((address >= 0x5000) && (address <= 0x57FF)) {
            if ((regPORTB & (1 << 7)) == 0) {
                // puts("MEM Region $5000 access");
                // selfTest ROM is paged in
                // printf("R SELF-TEST  %04x\n", address);
                return &mem[address + 0x8000];
            }
        }
    }
    return &mem[address];
}

uint8_t __m6502_func(mem_read)(uint16_t address) {
    // statistic number of read counter
    // read_cnt++;
    // printf("MEM R $%04X\n", address);

    // if (address < 0x5000) return mem[address];

    if (address > 0xD7FF) return mem[address];

    if (address < 0xC000) {
        // if (address < 0xD000) {
        if (trig3_ == 1) {
            if ((address >= cart_start)) return cart_rom[address - cart_start];
        }

        if ((regPORTB & (1 << 1)) == 0) {
            // if (select_basicrom) {
            if ((address >= 0xA000)) {
                // printf("R BASIC ROM $%04x\n", address);
                return basic_rom[address - 0xA000];
            }
        }

        if (select_selftestrom) {
            if ((address >= 0x5000) && (address <= 0x57FF)) {
                // puts("MEM Region $5000 access");
                // selfTest ROM is paged in
                // printf("R SELF-TEST  %04x\n", address);
                return mem[address + 0x8000];
            }
        }

        return mem[address];
    }

    // check for custom chip access
    // if (address >= 0xD000) {
    // custom chip address range

    // GTIA custom chip
    if ((address >> 8) == 0xD0) {
        return gtia_read(address & 0x1F);
    }
    // Pokey custom chip
    if ((address >> 8) == 0xD2) {
        return pokey_read(address & 0x0F);
    }
    // PIA 6520
    if ((address >> 8) == 0xD3) {
        return m6520_read(address & 0x03);
    }
    // Antic custom chip
    if ((address >> 8) == 0xD4) {
        return antic_read(address & 0x0F);
    }
    // Reserved for future use
    // else if ((address >= 0xD500) && (address <= 0xD7FF)) {
    //   // ignore for future enhancement
    // }
    // }

    return mem[address];
}

void __m6502_func(mem_write)(uint16_t address, uint8_t data) {
    // statistics number of write counter
    // write_cnt++;

    if ((address >= 0x5000) && (address < 0x5800)) {
        // puts("MEM Region $5000 access");
        if (regPORTB & (1 << 7)) {
            // selfTest ROM is not paged in
            mem[address] = data;
        } else {
            printf("SELF-TEST write %04x\n", address);
        }
        return;
    }

    if ((address >= 0x8000) && (address <= 0x9FFF)) {
        if (trig3_ == 0) mem[address] = data;
        // else
        // printf("W CARTRIDGE ROM  $%04x: $%02x\n", address, data);
        return;
    }

    if ((address >= 0xA000) && (address <= 0xBFFF)) {
        if ((trig3_ == 0) || (regPORTB & (1 << 1))) {
            // BASIC ROM disabled
            mem[address] = data;
        }  // else {
           //  printf("W BASIC ROM  $%04x: $%02x\n", address, data);
        // }
        return;
    }

    // custom chip address range
    // if ((address >= 0xD000) && (address <= 0xD7FF)) {
    if (address >= 0xD000) {
        if ((address >= 0xD000) && (address <= 0xD0FF)) {
            gtia_write(address & 0x1F, data);
            return;
        }

        if ((address >= 0xD200) && (address <= 0xD2FF)) {
            pokey_write(address & 0x0F, data);
            return;
        }

        if ((address >= 0xD300) && (address <= 0xD3FF)) {
            m6520_write(address & 0x03, data);
            return;
        }

        if ((address >= 0xD400) && (address <= 0xD4FF)) {
            antic_write(address & 0x0F, data);
            return;
        }

        return;
        // Reserved for future use
        // else if ((address >= 0xD500) && (address <= 0xD7FF)) {
        //   // ignore for future enhancement
        // } else {
        //   // printf("PANIC! write to undefined Chip address $%04x :
        //   %02x\n",
        //   // address,
        //   //  data);
        // }
    }

    mem[address] = data;

    // printf("PANIC WRITE TO ROM $%04x\n", address);
    return;
}

void poke(uint16_t addr, uint8_t data) { mem_write(addr, data); }

void poke_n(uint16_t addr, uint8_t* data, size_t n) {
    while (n--) mem_write(addr++, *(data++));
}

void poke_str(uint16_t addr, char* str) {
    while (*str) mem_write(addr++, *(str++));
}

uint8_t peek(uint16_t addr) { return mem_read(addr); }