
#include "rom.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "gtia.h"
#include "m6520.h"
#include "memory.h"

#define ATARI_OS_START 0xC000
#define ATARI_OS_LENGTH 16384

#define ATARIBAS_START 0xA000
#define BASIC_ROM_LENGTH 8192

#define CARTRIDGE_START 0xA000
#define CARTRIDGE_ROM_LENGTH 16384

#define ROM_8K 8192
#define ROM_16K 16384

extern const uint8_t atari_xl_rom[];
extern const uint8_t atari_xe_pal_rom[];
extern uint8_t ataribas_rom[];
extern uint8_t altirra_basic_rom[];
extern uint8_t donkeykong_rom[];
extern uint8_t pengo_rom[];
extern uint8_t pacman_rom[];
extern uint8_t salt205_rom[];
extern uint8_t miner2049_rom[];
extern uint8_t spaceinvaders_rom[];
extern uint8_t boulderdash_rom[];
extern uint8_t mspacman_rom[];
extern uint8_t starraiders_rom[];
extern uint8_t qbert_rom[];
extern uint8_t Galaxian_rom[];

rom_info romtable[MAXROMS] = {
    {"BASIC", altirra_basic_rom, 0xA000, ROM_8K},
    {"Miner 2049", miner2049_rom, 0x8000, 16384},
    {"PacMan", pacman_rom, 0xA000, ROM_8K},
    {"Space Invaders", spaceinvaders_rom, 0xA000, 8192},
    {"BoulderDash", boulderdash_rom, 0x8000, 16384},
    {"Donkey Kong", donkeykong_rom, 0x8000, 16384},
    {"Pengo", pengo_rom, 0x8000, 16384},
    {"Ms PacMan", mspacman_rom, 0x8000, 16384},
    {"Star Raiders", starraiders_rom, 0xA000, ROM_8K},
    {"SALT 2.05", salt205_rom, 0xA000, ROM_8K},
    {"Qbert", qbert_rom, 0xA000, ROM_8K},
    {"Galaxian", Galaxian_rom, 0xA000, ROM_8K}};

void load_rom(int rom) {
    printf("load_rom %s\n", romtable[rom].title);

    memset(mem, 0, SIZE_64K);

    memcpy(mem + ATARI_OS_START, atari_xl_rom, ATARI_OS_LENGTH);
    // memcpy(mem + ATARI_OS_START, atari_xe_pal_rom, ATARI_OS_LENGTH);

    // memcpy(basic_rom, ataribas_rom, BASIC_ROM_LENGTH);
    // memcpy(basic_rom, altirra_basic_rom, BASIC_ROM_LENGTH);
    basic_rom = altirra_basic_rom;
    // basic_rom = ataribas_rom;

    cart_start = romtable[rom].start;
    cart_rom = romtable[rom].addr;

    // indicate ROM cartridge is inserted
    if (rom != 0) {
        select_rom_cartridge = true;
        select_basicrom = false;
        // copy rom code to memory
        // memcpy(mem + romtable[rom].start, romtable[rom].addr,
        //        romtable[rom].size);
    } else {
        select_basicrom = true;
        select_rom_cartridge = false;
    }
}
