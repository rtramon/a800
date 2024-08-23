
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "gtia.h"
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

struct rom_info {
    uint8_t* addr;
    uint16_t start;
};

rom_info rom_table[] = {{nullptr, 0},
                        {miner2049_rom, 0x8000},
                        {pacman_rom, 0xA000},
                        {spaceinvaders_rom, 0xA000},
                        {boulderdash_rom, 0x8000},
                        {donkeykong_rom, 0x8000},
                        {pengo_rom, 0x8000},
                        {mspacman_rom, 0x8000},
                        {starraiders_rom, 0xA000},
                        {salt205_rom, 0xA000}};

void load_rom(int rom) {
    printf("load_rom(%d)\n", rom);

    memcpy(mem + ATARI_OS_START, atari_xl_rom, ATARI_OS_LENGTH);
    // memcpy(mem + ATARI_OS_START, atari_xe_pal_rom, ATARI_OS_LENGTH);

    // memcpy(basic_rom, ataribas_rom, BASIC_ROM_LENGTH);
    memcpy(basic_rom, altirra_basic_rom, BASIC_ROM_LENGTH);
    // basic_rom = altirra_basic_rom;
    // basic_rom = ataribas_rom;

#if 0 
    switch (rom) {
        case 1:  // MINER 2049
            cart_start = 0x8000;
            cart_rom = miner2049_rom;
            break;

        case 2:  // pacman
            // printf("copy pacman rom to ram\n");
            cart_start = 0xA000;
            cart_rom = pacman_rom;
            break;

        case 3:  // space invaders
            // printf("copy spaceinvader rom to ram\n");
            cart_start = 0xA000;
            cart_rom = spaceinvaders_rom;
            // memcpy(cart_rom, spaceinvaders_rom, ROM_8K);
            break;

        case 4:  // boulder dash
            cart_start = 0x8000;
            cart_rom = boulderdash_rom;
            break;

        case 5:  // donkey kong
            cart_start = 0x8000;
            cart_rom = donkeykong_rom;
            break;

        case 6:  // Pengo
            cart_start = 0x8000;
            cart_rom = pengo_rom;
            break;

        case 0:  // BASIC
        default:
            cart_start = 0xFFFF;
            cart_rom = nullptr;
            break;
    }
#endif
    cart_start = rom_table[rom].start;
    cart_rom = rom_table[rom].addr;

    // indicate ROM cartridge is inserted
    if (rom != 0)
        trig3_ = 1;
    else
        trig3_ = 0;
}
