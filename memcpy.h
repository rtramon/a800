#ifndef _MEMCPY_DMA_H_
#define _MEMCPY_DMA_H_
#include <cstddef>
#include <cstdint>

// support function
// memset for 16bit values
void inline memset16(uint16_t *dst, uint16_t val, size_t cnt) {
    do {
        *(dst++) = val;
    } while (--cnt);
}

void inline memset32(uint32_t *dst, uint32_t val, size_t cnt) {
    do {
        *(dst++) = val;
    } while (--cnt);
}

void memcpy_dma_init();
// void memcpy_32(uint32_t *dst, const uint32_t *src, size_t cnt);
void memset32_dma(uint32_t *dst, uint32_t val, size_t cnt);
void memset_dma_ready();

#endif
