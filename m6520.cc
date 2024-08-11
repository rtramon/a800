#include "m6520.h"

#include <stdio.h>

#include "console.h"
#include "mos65C02.h"

////////////////////////////////////////////////////////////////////
// 6520 PIA
//
////////////////////////////////////////////////////////////////////
//
volatile uint8_t regPORTA;
uint8_t regDDRA;  // Dir register when KBDCR.bit2 == 0
uint8_t regPACTL;
uint8_t regPORTB;
uint8_t regDDRB;  // Dir register when DSPCR.bit2 == 0
uint8_t regPBCTL;
bool select_selftestrom;
bool select_basicrom;
enum PIA_REG_t { PORTA = 0x0, PORTB = 0x1, PACTL = 0x2, PBCTL = 0x3 };

void m6520_reset() {
    regPORTA = 0xFF;
    regPACTL = 0x00;
    regDDRA = 0x00;
    regPORTB = 0x83;  // external pullups on bit 0,1,7
    regPBCTL = 0x00;
    regDDRB = 0x00;

    select_selftestrom = false;
    select_basicrom = true;
}

uint8_t __m6502_func(m6520_read)(uint8_t reg) {
    uint8_t data;
    // printf("R PIA $%02x\n", reg);

    switch (reg) {
        case PORTA:
            // reading PORTA clears interupt A Status
            regPACTL &= (~0xC0);

            if (regPACTL & (1 << 2)) {
                data = regPORTA;
                // printf("PIA R PORTA %02x\n", data);
            } else {
                data = regDDRA;
            }
            break;

        case PACTL:
            data = regPACTL;
            // printf("PIA R PACTL %02x\n", data);

            break;

        case PORTB:
            // reading portb clear IRQ status
            regPBCTL &= (~0xC0);

            if (regPBCTL & (1 << 2)) {
                data = regPORTB;

                // printf("PIA R PORTB %02x\n", data);
            } else
                data = regDDRB;

            break;

        case PBCTL:
            data = regPBCTL;
            // printf("PIA R PBCTL %02x\n", data);

            break;

        default:
            printf("PANIC! M6520 read unknown register $%02x\n", reg);
            break;
    }

    return data;
}

void __m6502_func(m6520_write)(uint8_t reg, uint8_t data) {
    switch (reg) {
        case PORTA:
            if (regPACTL & (1 << 2)) {
                regPORTA = data;
                // printf("W PORTA %02x\n", data);

            } else
                regDDRA = data;
            break;

        case PACTL:
            regPACTL = data & 0x3F;
            break;

        case PORTB:
            if (regPBCTL & (1 << 2)) {
                regPORTB = data;
                // printf("W PIA PORTB %02x\n", data);
                select_selftestrom = (regPORTB & (1 << 7)) == 0 ? true : false;
                select_basicrom = (regPORTB & (1 << 1)) == 0 ? true : false;

                // if (regPORTB & (1 << 0))
                //     puts("PIA $C000 OS-ROM");
                // else
                //     puts("PIA $C000 RAM");

                // if (regPORTB & (1 << 1))
                //     puts("PIA $A000 RAM");
                // else
                //     puts("PIA $A000 BASIC-ROM");

                // if (regPORTB & (1 << 7))
                //     puts("PIA $5000 RAM");
                // else
                //     puts("PIA $5000 selftest ROM");

            } else {
                // printf("PIA W DDRB $%02x\n", data);
                regDDRB = data;
            }
            break;

        case PBCTL:
            // printf("PIA W PBCTL $%02x\n", data);
            regPBCTL = data & 0x3F;
            break;

        default:
            printf("PANIC! M6520 write unknown register $%02x : $%02x\n", reg,
                   data);
            break;
    }
}

void m6520_joy0(uint8_t val) {
    regPORTA = val & 0x0F;
    // if (regPORTA != 0x0F) {
    //   // trigger interupt
    //   regPACTL |= (1 << 7);
    //   mos65c02_irq_on();
    // }
}