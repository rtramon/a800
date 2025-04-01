#pragma once
#include <cstdint>

#define MAXROMS 12

struct rom_info {
    char title[16];
    uint8_t* addr;
    uint16_t start;
    uint16_t size;
};

extern rom_info romtable[MAXROMS];
