#ifndef _ANTIC_H__
#define _ANTIC_H__
#include <stdint.h>

extern uint8_t GLtrace;
extern volatile uint8_t antic_chbase;
extern volatile uint8_t nmist_;

void antic_reset();
uint8_t antic_read(uint8_t reg);
void antic_write(uint8_t reg, uint8_t data);

void antic_dli();
void antic_gen_vbi();

void antic_render_scanline(uint16_t* scanbuf, int line);

void antic_dl_start(unsigned int);
void antic_dl_end(unsigned int);

// support functions
inline uint8_t atascii_to_iv(uint8_t c) {
    // strip off top bit
    // c &= 0x7F;
    if (c <= 31) return (c + 64) * 8;
    if (c <= 95) return (c - 32) * 8;
    return c * 8;
}

inline uint8_t iv_to_ascii(uint8_t iv) {
    if (iv <= 63) return iv + 32;
    if (iv <= 95) return iv - 64;

    return iv;
}

#endif
