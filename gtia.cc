#include <stdint.h>
#include <stdio.h>

#include <cstring>

#include "antic_impl.h"
#include "colortable.h"
#include "mos65C02.h"
#include "pico/stdlib.h"

////////////////////////////////////////////////////////////////////
// Atari Custom GTIA
//
////////////////////////////////////////////////////////////////////
//
enum GTIA_READ_REG_t {
    M0PF = 0x00,
    M1PF = 0x01,
    M2PF = 0x02,
    M3PF = 0x03,
    P0PF = 0x04,
    P1PF = 0x05,
    P2PF = 0x06,
    P3PF = 0x07,
    M0PL = 0x08,
    M1PL = 0x09,
    M2PL = 0x0A,
    M3PL = 0x0B,
    P0PL = 0x0C,
    P1PL = 0x0D,
    P2PL = 0x0E,
    P3PL = 0x0F,
    TRIG0 = 0x10,
    TRIG1 = 0x11,
    TRIG2 = 0x12,
    TRIG3 = 0x13,
    PAL = 0x14,
    CONSOL = 0x1F
};

enum GTIA_WRITE_REG_t {
    HPOSP0 = 0x0,
    HPOSP1 = 0x1,
    HPOSP2 = 0x2,
    HPOSP3 = 0x3,
    HPOSM0 = 0x4,
    HPOSM1 = 0x5,
    HPOSM2 = 0x6,
    HPOSM3 = 0x7,
    SIZEP0 = 0x8,
    SIZEP1 = 0x9,
    SIZEP2 = 0xa,
    SIZEP3 = 0xb,
    SIZEM = 0xc,
    GRAFP0 = 0xd,
    GRAFP1 = 0xe,
    GRAFP2 = 0xf,
    GRAFP3 = 0x10,
    GRAFM = 0x11,
    COLPM0 = 0x12,
    COLPM1 = 0x13,
    COLPM2 = 0x14,
    COLPM3 = 0x15,
    COLPF0 = 0x16,
    COLPF1 = 0x17,
    COLPF2 = 0x18,
    COLPF3 = 0x19,
    COLBK = 0x1A,
    PRIOR = 0x1B,
    VDELAY = 0x1C,
    GRACTL = 0x1D,
    HITCLR = 0x1E,
    // CONSOL = 0x1F
};

volatile uint8_t consol_;
volatile uint8_t prior_;
volatile uint16_t colbk_;
volatile uint8_t reg_colbk_;
uint8_t trig0_, select_rom_cartridge;
// uint16_t palette[9];
uint8_t gractl_;
uint8_t reg_colpf[4];
uint16_t colpf[4];
uint8_t reg_colpm_[4];
uint16_t colpm_[4];
uint8_t grafp_[4];  // player bitmap for one scanline
uint8_t grafm_;     // missiles bitmap for one scanline
uint8_t hposp_[4];
uint8_t hposm_[4];
uint8_t sizep_[4];
uint8_t sizem_;
uint8_t mxpl_[4];
uint8_t pxpf_[4];
uint8_t mxpf_[4];
uint8_t pxpl_[4];

void gtia_reset() {
    consol_ = 0x07;
    prior_ = 0;
    trig0_ = 0xFF;
    gractl_ = 0;
    grafm_ = 0;
    memset(grafp_, 0, 4);
    memset(hposm_, 0, 4);
    memset(hposp_, 0, 4);
    memset(sizep_, 0, 4);
    sizem_ = 0;

    // #if defined(PACMAN) || defined(DONKEY_KONG) || defined(MINER2049) ||    \
//     defined(BOULDERDASH) || defined(PENGO) || defined(SPACEINVADERS) || \
//     defined(MSPACMAN)
    //     trig3_ = 1;  // to enable cartridge
    // #else
    //     trig3_ = 0;
    // #endif
    select_rom_cartridge = false;

    colbk_ = 0;
    memset((void*)colpf, 0, sizeof(colpf));
    memset((void*)colpm_, 0, sizeof(colpm_));
    memset(mxpl_, 0, 4);
    memset(mxpf_, 0, 4);
    memset(pxpf_, 0, 4);
    memset(pxpl_, 0, 4);
}

uint8_t __m6502_func(gtia_read)(uint8_t reg) {
    uint8_t data;
    switch ((GTIA_READ_REG_t)reg) {
        case M0PF:  // collision missile - player
        case M1PF:
        case M2PF:
        case M3PF:
            return mxpf_[reg - M0PF];
            break;
        case P0PF:  // collision player 0
        case P1PF:
        case P2PF:
        case P3PF:
            return pxpf_[reg - P0PF];
            break;
        case M0PL:
        case M1PL:
        case M2PL:
        case M3PL:
            return mxpl_[reg - M0PL];
            break;
        case P0PL:
        case P1PL:
        case P2PL:
        case P3PL:
            return pxpl_[reg - P0PL];
            break;

        case PAL:  // low 4bits: 0001=PAL, 1111=NTSC
            data = 0x0F;
            break;

        case TRIG0:  // trigger registers
            data = trig0_;
            trig0_ = 0xFF;
            break;

        case TRIG1:  // paddle 1 trigger button
            data = 0xFF;
            break;
        case TRIG2:  // indicates XEGS keyboard, reads 1 on XL/XE
            data = 1;
            break;

        case TRIG3:  // indicates cartridge is inserted. not affected by basic
                     // rom
            data = select_rom_cartridge;
            break;

        case CONSOL:  // used to read status of the console keys START, SELECT,
                      // OPTION
            // printf("R CONSOL $%02x\n", consol_);
            data = consol_;
            break;

        default:
            // printf("GTIA read reg 0x%2X\n", reg);
            data = 0xFF;
            break;
    }

    return data;
}

void __m6502_func(gtia_write)(uint8_t reg, uint8_t data) {
    // printf("W GTIA $%02x : $%02x\n", reg, data);

    switch (reg) {
        case HPOSP0:  // Player 0 horizontal position
            // printf("HPOSP0: %d\n", data);
            hposp_[0] = data;
            break;
        case HPOSP1:
            // printf("HPOSP1: %d\n", data);
            hposp_[1] = data;
            break;
        case HPOSP2:
            // printf("HPOSP2: %d\n", data);
            hposp_[2] = data;
            break;
        case HPOSP3:
            // printf("HPOSP3: %d\n", data);
            hposp_[3] = data;
            break;

        case HPOSM0:
        case HPOSM1:
        case HPOSM2:
        case HPOSM3:
            // printf("HPOSM%1d: %d\n", reg - HPOSM0, data);
            hposm_[reg - HPOSM0] = data;
            break;

        case COLPM0:
        case COLPM1:
        case COLPM2:
        case COLPM3:
            reg_colpm_[reg - COLPM0] = data;
            colpm_[reg - COLPM0] = color2rgb(data);
            break;

        case SIZEP0:
            sizep_[0] = data & 0x03;
            break;
        case SIZEP1:
            sizep_[1] = data & 0x03;
            break;
        case SIZEP2:
            sizep_[2] = data & 0x03;
            break;
        case SIZEP3:
            sizep_[3] = data & 0x03;
            break;

        case SIZEM:
            // printf("GTIA sizem: %02X\n", data & 0x03);
            sizem_ = data & 0x03;
            break;

        case COLPF0:
        case COLPF1:
        case COLPF2:
        case COLPF3:

            // printf("W GTIA COLPF0 $%02x\n", data);
            // store both the colr value as well as the rgb value of the color
            reg_colpf[reg - COLPF0] = data;
            colpf[reg - COLPF0] = color2rgb(data);
            break;

        case COLBK:
            // printf("W GTIA COLBK $%02x\n", data);
            reg_colbk_ = data;
            colbk_ = color2rgb(data);
            break;

        case GRAFP0:
        case GRAFP1:
        case GRAFP2:
        case GRAFP3:
            // printf("W GTIA GRAFP%1d  $%02x\n", reg - 0xD, data);
            grafp_[reg - GRAFP0] = data;
            break;
        case GRAFM:
            // printf("W GTIA GRAFM  $%02x\n", data);
            grafm_ = data;
            break;

        case GRACTL:
            gractl_ = data;
            // printf("GTIA write to GRACTL $%02x\n", data);
            break;

        case CONSOL:
            // bit 0-2 when written zet direction: 0 is input, which results in
            // reading back 1 meaning key not pressed!! consol_ = data; if (data
            // & 0x07 == 0x07)
            //   consol_ = 0x00;
            // else
            //   consol_ = 0x07;
            break;

        case PRIOR:
            // printf("W GTIA PRIOR $%02x\n", data);
            prior_ = data;
            break;

        case HITCLR:
            memset(mxpl_, 0, 4);
            memset(mxpf_, 0, 4);
            memset(pxpf_, 0, 4);
            memset(pxpl_, 0, 4);
            break;

        case VDELAY:
            // printf("W GTIA VDELAY $%02x\n", data);
            break;

        default:
            break;
    }
}

void gtia_set_consol(uint8_t val) { consol_ = val; }
