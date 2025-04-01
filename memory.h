#ifndef __MEMORY_H__
#define __MEMORY_H__
#include <stddef.h>
#include <stdint.h>

#define __m6502_func(f) __scratch_y(__STRING(f)) f

constexpr int SIZE_8K = 8192;
constexpr int SIZE_16K = 16384;
constexpr int SIZE_64K = 65536;

extern uint8_t mem[SIZE_64K];
// extern uint8_t basic_rom[SIZE_8K];
extern uint8_t* basic_rom;
extern uint8_t* cart_rom;
extern uint16_t cart_start;

void mem_reset();
void mem_write(uint16_t address, uint8_t data);
uint8_t mem_read(uint16_t address);
uint8_t* mem_read_ptr(uint16_t address);

void poke(uint16_t addr, uint8_t data);
void poke_n(uint16_t addr, uint8_t* data, size_t n);
void poke_str(uint16_t addr, char* str);
uint8_t peek(uint16_t addr);
#endif