#include "antic.h"

#include "antic_impl.h"
#include "colortable.h"
#include "gtia.h"
#include "memory.h"
#include "pokey.h"
#ifdef EMU_M6502
#include "emu_m6502.h"
#else
#include "mos65C02.h"
#endif

#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "memcpy.h"
#include "pico/platform.h"

#define __dvi_func_y(f) __scratch_y(__STRING(f)) f
#define __dvi_func_x(f) __scratch_x(__STRING(f)) f

////////////////////////////////////////////////////////////////////
// Atari Custom ANTIC
//
////////////////////////////////////////////////////////////////////

// forward declaration
void antic_set_dlist(uint16_t);
void pm_missile_scanline(uint16_t* pixbuf);
void pm_player_scanline(uint16_t* pixbuf);

void mode_0_prepare_scanbuf(uint16_t*);
void mode_2_prepare_scanbuf(uint16_t*);
void mode_4_prepare_scanbuf(uint16_t*);
void mode_6_prepare_scanbuf(uint16_t*);
void mode_8_prepare_scanbuf(uint16_t*);
void mode_B_prepare_scanbuf(uint16_t*);
void mode_D_prepare_scanbuf(uint16_t*);
void mode_E_prepare_scanbuf(uint16_t*);
void mode_F_prepare_scanbuf(uint16_t*);
void blankline_prepare_scanbuf(uint16_t*);

enum ANTIC_READ_REG_t {
    REG06 = 0x6,
    VCOUNT = 0xB,
    PENH = 0xC,
    PENV = 0xD,
    NMIST = 0xF
};

enum ANTIC_WRITE_REG_t {
    DMACTL = 0x0,
    CHACTL = 0x1,
    DLISTL = 0x2,
    DLISTH = 0x3,
    HSCROL = 0x4,
    VSCROL = 0x5,
    PMBASE = 0x7,
    CHBASE = 0x9,
    WSYNC = 0xA,
    NMIEN = 0xE,
    NMIRES = 0xF
};
volatile uint8_t nmien_;
volatile uint8_t nmist_ = 0;
static volatile uint8_t dmactl_;
static volatile uint8_t chactl_;
static volatile uint8_t pmbase_;
static volatile uint32_t vcount_ = 0;
static volatile uint8_t dlistl_ = 0;
static volatile uint8_t dlisth_ = 0;
static volatile uint8_t vscrol_ = 0;
static volatile uint8_t hscrol_ = 0;
static volatile bool wsync_ = false;

volatile uint16_t antic_screen_base;
volatile uint8_t antic_chbase;

// rendering variables
static int chars_scanline;
static int bytes_scanline;  // number of bytes for scanline
static int double_y;

static uint32_t dli_ts;

static uint8_t*
    screendata_ptr;  // pointer to graphics or character data in a screen
static uint16_t dl_counter;

struct display_list_t {
    int scanline;
    int lastline;
    uint8_t mode;
    // uint8_t hscroll_offset;
    // uint8_t vscroll_offset;
    bool dli;
    bool lms;
    bool hscroll;
    bool vscroll;
    bool prev_vscroll;
    bool hires;
} dl_data;

struct Mode_data {
    int lastline;
    int double_y;
    int col_div;  // scanline width divider
    bool hires;
    void (*render_scanline)(uint16_t*);
};

Mode_data mode_data[16] = {
    {0, 0, 1, false, mode_0_prepare_scanbuf},     // not a display mode
    {0, 0, 1, false, blankline_prepare_scanbuf},  // not a display mode
    {7, 1, 1, true, mode_2_prepare_scanbuf},  // Character ANTIC 2, Basic mode 0
    {9, 1, 1, true, mode_2_prepare_scanbuf},  // Character ANTIC 3
    {7, 1, 1, false,
     mode_4_prepare_scanbuf},  // character mode 4, Basic mode 12
    {15, 2, 1, false,
     mode_4_prepare_scanbuf},  // character mode 5, Basic mode 13
    {7, 1, 2, false, mode_6_prepare_scanbuf},  // Character mode 6, Basic mode 1
    {15, 2, 2, false,
     mode_6_prepare_scanbuf},                  // Character mode 7, Basic mode 2
    {7, 1, 4, false, mode_8_prepare_scanbuf},  // graphics mode 8, Basic mode 3
    {0, 0, 0, false,
     blankline_prepare_scanbuf},  // not implemented Antic mode 9, Basic mode 4
    {0, 0, 0, false,
     blankline_prepare_scanbuf},  // not implemented Antic mode A, Basic mode 5
    {1, 1, 2, false, mode_B_prepare_scanbuf},  // Graphics mode B, Basic 6
    {0, 1, 1, false, mode_B_prepare_scanbuf},  // Graphics mode C, Basic mode 14
    {1, 2, 1, false, mode_E_prepare_scanbuf},  // Graphics mode D, Basic mode 7
    {0, 1, 1, false, mode_E_prepare_scanbuf},  // Graphics mode E, Basic mode 15
    {0, 1, 1, true,
     mode_F_prepare_scanbuf},  // Graphics mode F, Basic mode 8,9,10,11
};

uint32_t timestamp_32;
// uint32_t end_32;

extern uint16_t lines;
extern void (*render_scanline)(uint16_t*);
#ifdef EMU_M6502
extern M6502 cpu;
#endif

void __always_inline antic_set_dlist(uint16_t dlist) { dl_counter = dlist; }

void antic_reset() {
    nmien_ = 0;
    dmactl_ = 0;
    chactl_ = 0;
    pmbase_ = 0;
    vscrol_ = 0;
    dlistl_ = 0;
    dlisth_ = 0;
    hscrol_ = 0;
    vscrol_ = 0;
    antic_chbase = 0;

    // initialize renderer
    render_scanline = blankline_prepare_scanbuf;
    double_y = 0;
    dl_counter = 0;
    screendata_ptr = nullptr;
    chars_scanline = 0;
    bytes_scanline = 0;
    memset(&dl_data, 0, sizeof(dl_data));
}

uint8_t __not_in_flash_func(antic_read)(uint8_t reg) {
    uint8_t data;
    switch (reg) {
        case VCOUNT:
            // printf("ANTIC read VCOUNT $%02x\n", vcount_);
            data = vcount_ / 2;
            break;

        case NMIST:
            data = nmist_;
            break;

        default:
            data = 0xFF;
            break;
    }

    return data;
}

void __not_in_flash_func(antic_write)(uint8_t reg, uint8_t data) {
    // printf("W ANTIC $%02x : $%02x\n", reg, data);

    switch (reg) {
        case DMACTL:
            dmactl_ = data;
            // printf("ANTIC write DMACTL $%02x\n", data);
            break;

        case CHACTL:
            chactl_ = data;
            // printf("ANTIC write CHACTL $%02x\n", data);
            break;

        case HSCROL:
            hscrol_ = data & 0x0F;
            // printf("ANTIC write HSCROL 0x%02x\n", hscrol_);
            break;

        case VSCROL:
            vscrol_ = data & 0x0F;
            break;

        case REG06:  // undefined register
        case PENH:
        case PENV:
            break;

        case PMBASE:
            pmbase_ = data;
            // printf("pmbase $%02x  addr $%04X\n", pmbase_, pmbase_ << 8);
            break;

        case WSYNC:
            // used to synchronize cpu to horizontal sync
            // experiment to wait till render function updates the
            // vcount line counter
            if ((dmactl_ & 0x03) != 0) {
                wsync_ = false;
                while (!wsync_);
            }
            break;

        case DLISTL:
            // printf("ANTIC write DLISTL $%02x\n", data);
            dlistl_ = data;
            antic_set_dlist((dlisth_ << 8) + dlistl_);
            break;

        case DLISTH:
            dlisth_ = data;
            antic_set_dlist((dlisth_ << 8) + dlistl_);
            // printf("ANTIC write DLISTH $%04x\n", dl_counter);

            break;

        case CHBASE:
            antic_chbase = data;
            // printf("%d W chbase %d\n", timer_hw->timerawl - timestamp_32,
            // lines);
            // printf("W ANTIC CHBASE $%02x ($%04x) %d\n", data, (data << 8),
            //    lines);
            break;

        case NMIEN:
            nmien_ = data;
            // printf("ANTIC write NMIEN $%02x\n", data);
            break;

        case NMIRES:
            // reset NMIST, clear top 3 active interrupt status
            nmist_ = 0x1F;
            break;

        default:
            break;
    }
}

void __not_in_flash_func(antic_gen_vbi)() {
    // generate nmi for 6502
    // vbi clears dli
    nmist_ = 0x40;
    if (nmien_ & 0x40) {
        // vbi is enabled, set vbi in NMIST and generate nmi
#ifdef EMU_M6502
        cpu.nmi = true;
#else
        mos65c02_nmi();
#endif
    }
}

void __always_inline antic_gen_dli() {
    // dli clears vbi (ref Altirra HW guide pg72)
    nmist_ = 0x80;
    if (nmien_ & 0x80) {
// dli is enabled, set vbi in NMIST and generate nmi
#ifdef EMU_M6502
        cpu.nmi = true;
#else
        mos65c02_nmi();
#endif
    }
}

constexpr uint CHAR_COLS[4] = {0, 32, 40, 48};
constexpr uint CHAR_COLS_HSCROL[4] = {0, 40, 48, 48};

void __not_in_flash_func(antic_render_scanline)(uint16_t* scanbuf, int line) {
    // antic lines go from 8 to 248 (ref: Altirra HW guide)
    antic_dl_start(line);

#if !defined(OPTION_TMDS)
    if (dl_data.hires) {
        uint16_t col0 = colpf[2];

        // clear scanline buffer with background color
        // memset16(scanbuf, col0, 320);
        memset32_dma((uint32_t*)scanbuf, col0 | col0 << 16, 160);
        // memset32((uint32_t*)scanbuf, (col0 << 16) | col0, 160);

        pm_missile_scanline(scanbuf);

        // update scanline with rendered player graphics
        pm_player_scanline(scanbuf);
    }

    // render playfield scanline
    (*render_scanline)(scanbuf);

    if (!dl_data.hires) {
        pm_missile_scanline(scanbuf);

        // // update scanline with rendered player graphics
        pm_player_scanline(scanbuf);
    }
#endif
}

void __not_in_flash_func(execute_dlist)(uint line) {
    // called at end of each scanline to execute one displaylist instruction
    static bool dl_suspend = false;

    // fetch display list instruction when the last line of the mode line
    // has been rendered
    if (dl_data.scanline < dl_data.lastline + 1) {
        return;
    }

    // finished rendering one complete modeline
    // reset scanline for the next line of the selected screenmode
    dl_data.scanline = 0;

    // increment pointer to screen data , note possibly reset when new lms
    // is done
    screendata_ptr += bytes_scanline;

    // antic_screen_base = antic_screen_base + bytes_scanline;

    // printf("mode: %1X window_y: %d scanline: %d lastline: %d\n",
    // dl_data.mode,
    //        window_mode_y_, dl_data.scanline, dl_data.lastline);

    if (dl_suspend) {
        if (line < 240) return;
        dl_suspend = false;
    }

    // ANTIC does not proces more then 240 instructions (240+8)
    if (line > 239) return;

    // is displaylist dma on? ref altirra hardware manual
    if (!(dmactl_ & (1 << 5))) return;

    // on the last scanline of the scanline the display list
    // executes an instruction
    const uint8_t instr = mem_read(dl_counter);

    dl_data.dli = instr & (1 << 7);
    dl_data.lms = instr & (1 << 6);
    dl_data.mode = instr & 0x0F;
    dl_data.hscroll = instr & (1 << 4);
    dl_data.prev_vscroll = dl_data.vscroll;
    dl_data.vscroll = instr & (1 << 5);

    // static uint8_t last_mode = 0;
    // if (dl_data.mode != last_mode) {
    //     printf(" DL mode %02x %1x %3d\n", dl_data.mode, dl_data.lms, lines);
    //     last_mode = dl_data.mode;
    // }

    //
    switch (dl_data.mode) {
        case 0:
            dl_data.lastline = ((instr >> 4) & 0x07);
            dl_data.lms = false;
            dl_data.hires = false;
            dl_data.hscroll = dl_data.vscroll = dl_data.prev_vscroll = false;
            render_scanline = blankline_prepare_scanbuf;
            bytes_scanline = 0;
            break;

        case 1:
            dl_data.lastline =
                0;  // render blank lines till the end of the frame
            dl_data.hires = false;
            render_scanline = blankline_prepare_scanbuf;
            bytes_scanline = 0;
            // load new display list
            dl_counter =
                mem_read(dl_counter + 1) + (mem_read(dl_counter + 2) << 8);
            if (dl_data.lms) {
                // suspend displaylist processing until next vblank
                dl_suspend = true;
            }
            return;

        default:
            dl_data.lastline = mode_data[dl_data.mode].lastline;
            double_y = mode_data[dl_data.mode].double_y;
            render_scanline = mode_data[dl_data.mode].render_scanline;
            dl_data.hires = mode_data[dl_data.mode].hires;
            chars_scanline =
                CHAR_COLS[dmactl_ & 0x03] / mode_data[dl_data.mode].col_div;
            if (dl_data.hscroll)
                bytes_scanline = CHAR_COLS_HSCROL[dmactl_ & 0x03] /
                                 mode_data[dl_data.mode].col_div;
            else
                bytes_scanline = chars_scanline;

            break;
    }

    if (dl_data.lms) {
        antic_screen_base =
            mem_read(dl_counter + 1) + (mem_read(dl_counter + 2) << 8);
        screendata_ptr = mem_read_ptr(antic_screen_base);
        dl_counter += 2;
    }
    dl_counter++;

    // vertical scrolling messes with start and end of scanlines
    if (dl_data.vscroll && (!dl_data.prev_vscroll)) {
        dl_data.scanline = vscrol_;
    } else if ((dl_data.prev_vscroll == 1) && (dl_data.vscroll == 0)) {
        dl_data.lastline = vscrol_;
    }
}

void __not_in_flash_func(antic_dl_start)(uint line) {
    // fetch player missile data
    const uint16_t pmbase_addr = (pmbase_ & 0xF8) << 8;
    // const uint8_t *pm_data = mem_read_ptr(pmbase_addr);
    // #if 0

    if (dmactl_ & (1 << 3)) {
        // Fetch Players bitmap
        for (int i = 3; i >= 0; i--) {
            grafp_[i] = mem_read(pmbase_addr + line + 8 + 0x400 + 256 * i);
        }
    }
    // #endif

    if (dmactl_ & (1 << 2)) {
        // grafm_ = *(pm_data + line + 768);
        grafm_ = mem_read(pmbase_addr + line + 8 + 768);
    }

    // fetch new display list instruction
    execute_dlist(line);

    // Altirra hardware reference section 4.8
    // "To trigger a DLI, bit 7 should be set on a display list instruction.
    // This causes ANTIC to fire an NMI at the start of
    // the last scan line for that mode line."
    if ((dl_data.scanline == dl_data.lastline) && dl_data.dli) {
        antic_gen_dli();
    }

    return;
}

void __not_in_flash_func(antic_dl_end)(uint line) {
    wsync_ = true;

    // update vount
    // ANTIC visible lines are from 8-248, screen display is from 0-240
    // so add 8 to the vcount'er
    vcount_ = line + 8;

    dl_data.scanline++;
}

bool __always_inline has_overlap(uint8_t x1, uint8_t w1, uint8_t x2,
                                 uint8_t w2) {
    if (((x1 >= x2) && (x1 <= x2 + w2)) &&
        ((x1 + w1 >= x2) && (x1 + w1) <= (x2 + w2)))
        return true;

    return false;
}

void __not_in_flash_func(pm_missile_scanline)(uint16_t* pixbuf) {
#ifdef PLAYERMISSILE

    // Is GTIA Missiles DMA activated
    if (!(gractl_ & (1 << 0))) return;

    const uint8_t graf = grafm_;
    if (graf != 0) {
        for (int i = 3; i >= 0; i--) {
            if ((hposm_[i] < 0x30) || (hposm_[i] > 0xCF))
                // Missile position is outside Normal size playfield
                continue;

            int xpos = (hposm_[i] - 0x30) * 2;
            uint8_t missile = (graf >> (2 * i)) & 0x03;
            if (missile) {
                uint16_t color;
                if (prior_ & (1 << 4))
                    color = colpf[3];
                else
                    color = colpm_[i];

                // render missile with correct size
                const int sizep = sizem_ + 1;
                for (int bit = 1; bit >= 0; bit--) {
                    // for (int size = 0; size < sizep; size++) {
                    if (missile & (1 << bit)) {
                        // perform playfield to missile collision detection
                        for (int pf = 0; pf < 4; pf++) {
                            if (*(pixbuf + xpos) == colpf[pf]) {
                                mxpf_[i] |= (1 << pf);
                            }
                        }

                        // paint missile
                        memset16(pixbuf + xpos, color, 2 * sizep);
                    }
                    xpos += 2 * sizep;
                }
                // perform missile to player collision detection
                for (int player = 0; player < 4; player++) {
                    if (grafp_[player] != 0)
                        if (has_overlap(hposm_[i], 2, hposp_[player], 8))
                            mxpl_[i] |= (1 << player);
                }
            }
        }
    }
}

void __not_in_flash_func(pm_player_scanline)(uint16_t* pixbuf) {
    // is GTIA Player DMA actived
    if (!(gractl_ & (1 << 1))) return;

    // Display Players
    for (int_fast8_t i = 3; i >= 0; i--) {
        if ((hposp_[i] < 0x30) || (hposp_[i] > 0xCF))
            // player position is outside Normal playfield size
            continue;

        uint8_t graf = grafp_[i];
        // for now only support DMA filled grafp_
        if (graf != 0) {
            int xpos = (hposp_[i] - 0x30) * 2;
            const int sizep = sizep_[i] + 1;
            uint16_t color = colpm_[i];

            // multicolor player?
            if (prior_ & (1 << 5)) {
                if ((i == 0) && (grafp_[1] != 0))
                    color = color2rgb(reg_colpm_[0] | reg_colpm_[1]);
                if ((i == 2) && (grafp_[3] != 0))
                    color = color2rgb(reg_colpm_[2] | reg_colpm_[3]);
            }

            for (int8_t bit = 7; bit >= 0; bit--) {
                // for (int size = 0; size < sizep; size++) {
                if (graf & (1 << bit)) {
                    // perform playfield to player collision detection
                    for (int pf = 0; pf < 4; pf++) {
                        if (*(pixbuf + xpos) == colpf[pf]) {
                            pxpf_[i] |= (1 << pf);
                        }
                    }

                    memset16(pixbuf + xpos, color, 2 * sizep);
                }
                xpos += 2 * sizep;
            }

            // perform player to player collision detection
            for (int_fast8_t player = 0; player < 4; player++) {
                if (player != i) {
                    if (grafp_[player] != 0)
                        if (has_overlap(hposp_[i], 8, hposp_[player], 8))
                            pxpl_[i] |= (1 << player);
                }
            }
        }
    }

#endif
}

// antic text mode 2, basic mode 0, 40x24 hires text, 1,5 colors
void __not_in_flash_func(mode_2_prepare_scanbuf)(uint16_t* pixbuf) {
    // handle small wide playfield
    if ((dmactl_ & 0x03) == 1) {
        // handle small playfield
        pixbuf += (40 - 32) * 8 / 2;
    }

    const uint8_t* font =
        mem_read_ptr(((antic_chbase & 0xFE) << 8) + (dl_data.scanline));

    const uint16_t col1 =
        color2rgb((reg_colpf[2] & 0xF0) | (reg_colpf[1] & 0x0F));

    // draw playfield
    for (int i = 0; i < chars_scanline; ++i) {
        const uint8_t c = *(screendata_ptr + i);

        uint8_t pixels = *(font + (c & 0x7F) * 8);
        if ((c & 0x80) && (chactl_ & 0x03)) pixels ^= 0xFF;

        for (int bit = 7; bit >= 0; bit--) {
            if (pixels & (1 << bit)) {
                // color from PF2, luminance from PF1
                *(pixbuf) = col1;
            }
            pixbuf++;
        }
    }
}

#if defined(OPTION_TMDS)
// antic text mode 2, basic mode 0, 40x24 hires text, 1,5 colors
void __dvi_func_x(mode_2_prepare_tmdsbuf)(uint32_t* tmdsbuf) {
    // TMDS data for RGB channel for a double pixel (a perfectly bit balanced
    // pixel)
    constexpr int DVI_WORDS_PER_CHANNEL = 320;

    int bk = (reg_colpf[2] & 0xF0) * 3;
    int col = ((reg_colpf[2] & 0xF0) | (reg_colpf[1] & 0xF)) * 3;

    // dvi_get_scanline(tmdsbuf);
    // dvi_scanline_rgb(tmdsbuf, tmdsbuf_red, tmdsbuf_green, tmdsbuf_blue);
    uint32_t* tmdsbuf_blue = tmdsbuf;
    uint32_t* tmdsbuf_green = tmdsbuf_blue + DVI_WORDS_PER_CHANNEL;
    uint32_t* tmdsbuf_red = tmdsbuf_green + DVI_WORDS_PER_CHANNEL;

    // // handle small wide playfield
    // if ((dmactl_ & 0x03) == 1) {
    //     // handle small playfield
    //     pixbuf += (40 - 32) * 8 / 2;
    // }

    const uint8_t* font =
        mem_read_ptr(((antic_chbase & 0xFE) << 8) + (dl_data.scanline));

    // draw playfield
    for (int i = 0; i < chars_scanline; ++i) {
        const uint8_t c = *(screendata_ptr + i);

        uint8_t pixels = *(font + (c & 0x7F) * 8);
        if ((c & 0x80) && (chactl_ & 0x03)) pixels ^= 0xFF;
        uint color_index = 0;
        for (int bit = 7; bit >= 0; bit--) {
            if (pixels & (1 << bit))
                color_index = col;
            else
                color_index = bk;

            // *(tmdsbuf_blue++) = color2tmds(color_index + 2);
            // *(tmdsbuf_green++) = color2tmds(color_index + 1);
            // *(tmdsbuf_red++) = color2tmds(color_index);
            *(tmdsbuf_blue++) = tmds_palette[color_index + 2];
            *(tmdsbuf_green++) = tmds_palette[color_index + 1];
            *(tmdsbuf_red++) = tmds_palette[color_index];
        }
    }
}
#endif

// mode 4
// text mode 4; 40x24; 5 color 4x8 character cell
#if defined(OPTION_MODE4_EXPERIMENTAL)
void __not_in_flash_func(mode_4_prepare_scanbuf)(uint16_t* scanbuf) {
    uint16_t pal[4] = {
        colbk_,
        colpf[0],
        colpf[1],
        colpf[2],
    };

    uint16_t* pixbuf = scanbuf;
    pixbuf += ((40 - CHAR_COLS[dmactl_ & 0x03]) * 8 / 2);

    uint8_t* font =
        mem_read_ptr((antic_chbase << 8) + (dl_data.scanline / double_y));

    uint8_t chdata;
    uint8_t pixels;
    int start = 0;
    int scroll_offset = 0;
    int end = chars_scanline;
    if (dl_data.hscroll) {
        start = 4 - (hscrol_ / 4);
        end = chars_scanline + start;

        if (hscrol_) {
            scroll_offset = (hscrol_ & 0x03) * 2;

            if (scroll_offset) {
                start--;
                end--;
                // display shifted first character
                chdata = *(screendata_ptr + start++);
                pixels = *(font + (chdata & 0x7F) * 8);
                if (chdata & 0x80)
                    pal[3] = colpf[3];
                else
                    pal[3] = colpf[2];
                for (int j = scroll_offset - 2; j >= 0; j -= 2) {
                    uint8_t color = (pixels >> j) & 0x03;
                    *(pixbuf++) = pal[color];
                    *(pixbuf++) = pal[color];
                }
            }
        }
    }

    // draw middle part of view
    for (int i = start; i < end; i++) {
        chdata = *(screendata_ptr + i);
        pixels = *(font + (chdata & 0x7F) * 8);
        if (chdata & 0x80)
            pal[3] = colpf[3];
        else
            pal[3] = colpf[2];

        for (int j = 6; j >= 0; j -= 2) {
            uint8_t color = (pixels >> j) & 0x03;
            *(pixbuf++) = pal[color];
            *(pixbuf++) = pal[color];
        }
    }

    // draw the last scrolled character
    if (scroll_offset) {
        chdata = *(screendata_ptr + end);
        pixels = *(font + (chdata & 0x7F) * 8);
        if (chdata & 0x80)
            pal[3] = colpf[3];
        else
            pal[3] = colpf[2];

        for (int j = 6; j >= (scroll_offset); j -= 2) {
            uint8_t color = (pixels >> j) & 0x03;
            *(pixbuf++) = pal[color];
            *(pixbuf++) = pal[color];
        }
    }
}

#else

void __not_in_flash_func(mode_4_prepare_scanbuf)(uint16_t* scanbuf) {
    uint16_t pal[4] = {
        colbk_,
        colpf[0],
        colpf[1],
        colpf[2],
    };

    uint16_t* pixbuf = scanbuf;
    pixbuf += ((40 - CHAR_COLS[dmactl_ & 0x03]) * 8 / 2);

    uint8_t* font =
        mem_read_ptr((antic_chbase << 8) + (dl_data.scanline / double_y));

    int offset = 0;
    int scroll_offset = 0;
    int end = chars_scanline;
    if (dl_data.hscroll) {
        // from a800 emulator github antic.c#3956
        // but for boulderdash '4' is used iso '2'???
        // default hscrol_ in boulderdash is 3
        offset = 4 - (hscrol_ / 4);
        end += offset;

        if (hscrol_ & 0x03) {
            scroll_offset = (hscrol_ & 0x03) * 2;
            offset--;
        }
    }

    for (int i = offset; i < end; i++) {
        uint8_t c = *(screendata_ptr + i);
        uint8_t pixels = *(font + (c & 0x7F) * 8);
        uint16_t rgbcolor;
        if (c & 0x80)
            pal[3] = colpf[3];
        else
            pal[3] = colpf[2];

        if (scroll_offset) {
            // read to character bit and shift the horizontal scoll
            // use the low 8 bit as the pixel bits
            uint16_t pix16 = ((uint16_t)pixels << 8) |
                             *(font + (*(screendata_ptr + i + 1) & 0x7F) * 8);

            pixels = (pix16 >> (scroll_offset));
        }

        for (int j = 6; j >= 0; j -= 2) {
            uint8_t color = (pixels >> j) & 0x03;
            *(pixbuf++) = pal[color];
            *(pixbuf++) = pal[color];
        }
    }
}
#endif

//
// text mode 6, basic mode 1, 8x8 chars , 20x24, 5 colors; each pixel is a color
// clock text mode 7, basic mode 2, 8x16 chars, 20x12, 5 colors
//
#if defined(OPTION_MODE6_EXPERIMENTAL)
void __not_in_flash_func(mode_6_prepare_scanbuf)(uint16_t* pixbuf) {
    // printf("dmaclt_: %x, hscroll: %x\n", dmactl_ & 0x03,
    // dl_data.hscroll);
    uint16_t* scanbuf = pixbuf;

    pixbuf += ((40 - CHAR_COLS[dmactl_ & 0x03]) * 8 / 2);

    uint8_t* font = mem_read_ptr(((antic_chbase & 0xFC) << 8) +
                                 ((dl_data.scanline / double_y)));
    uint8_t chdata;
    uint8_t pixels;

    int start = 0;
    int scroll_offset = 0;
    int end = chars_scanline;

    if (dl_data.hscroll) {
        // from a800 emulator github antic.c#3956
        // but here i is '2', otherwise spaceinvaders does not look and
        // align correctly
        start = 2 - (hscrol_ / 8);
        end = start + chars_scanline;

        if (hscrol_) {
            scroll_offset = hscrol_ & 0x07;

            // draw scrolled first character
            if (scroll_offset) {
                start--;
                end--;
                // experiment, display shifted first character
                chdata = *(screendata_ptr + start++);
                pixels = *(font + (chdata & 0x3F) * 8);

                for (int bit = (scroll_offset - 1); bit >= 0; bit--) {
                    if (pixels & (1 << bit)) {
                        *(pixbuf++) = colpf[(chdata >> 6)];
                        *(pixbuf++) = colpf[(chdata >> 6)];
                    } else {
                        *(pixbuf++) = colbk_;
                        *(pixbuf++) = colbk_;
                    }
                }
            }
        }
    }

    // Draw middle part of view
    for (int i = start; i < end; i++) {
        chdata = *(screendata_ptr + i);
        pixels = *(font + (chdata & 0x3F) * 8);

        for (int bit = 7; bit >= 0; bit--) {
            if (pixels & (1 << bit)) {
                *(pixbuf++) = colpf[(chdata >> 6)];
                *(pixbuf++) = colpf[(chdata >> 6)];
            } else {
                *(pixbuf++) = colbk_;
                *(pixbuf++) = colbk_;
            }
        }
    }

    // draw scrolled last character
    if (scroll_offset) {
        chdata = *(screendata_ptr + end);
        pixels = *(font + (chdata & 0x3F) * 8);

        for (int bit = 7; bit >= scroll_offset; bit--) {
            if (pixels & (1 << bit)) {
                *(pixbuf++) = colpf[(chdata >> 6)];
                *(pixbuf++) = colpf[(chdata >> 6)];
            } else {
                *(pixbuf++) = colbk_;
                *(pixbuf++) = colbk_;
            }
        }
    }
}

#else

void __not_in_flash_func(mode_6_prepare_scanbuf)(uint16_t* pixbuf) {
    // printf("dmaclt_: %x, hscroll: %x\n", dmactl_ & 0x03,
    // dl_data.hscroll);

    pixbuf += ((40 - CHAR_COLS[dmactl_ & 0x03]) * 8 / 2);

    uint8_t* font = mem_read_ptr(((antic_chbase & 0xFC) << 8) +
                                 ((dl_data.scanline / double_y)));
    uint8_t chdata;
    uint8_t pixels;

    int start = 0;
    int scroll_offset = 0;
    int end = chars_scanline;
    if (dl_data.hscroll) {
        // from a800 emulator github antic.c#3956
        // but here i is '2', otherwise spaceinvaders does not look and
        // align correctly
        start = 2 - (hscrol_ / 8);

        if (hscrol_) {
            scroll_offset = hscrol_ & 0x07;
            if (scroll_offset) start--;
        }
        end = start + chars_scanline;
    }

    for (int i = start; i < end; i++) {
        uint8_t chdata = *(screendata_ptr + i);
        uint8_t pixels = *(font + (chdata & 0x3F) * 8);

        if (scroll_offset) {
            // read to character bit and shift the horizontal scroll
            // use the low 8 bit as the pixel bits
            uint16_t pix16 = (pixels << 8) |
                             *(font + (*(screendata_ptr + i + 1) & 0x3F) * 8);

            pixels = pix16 >> scroll_offset;
        }

        for (int bit = 7; bit >= 0; bit--) {
            if (pixels & (1 << bit)) {
                *(pixbuf++) = colpf[(chdata >> 6)];
                *(pixbuf++) = colpf[(chdata >> 6)];
            } else {
                *(pixbuf++) = colbk_;
                *(pixbuf++) = colbk_;
            }
        }
    }
}
#endif

//
// Mode 8 Grafic mode , basic mode 3, 4 color 40x24
// 10 bytes per scanline
// 8 dl_data.scanlines per pixel
void __not_in_flash_func(mode_8_prepare_scanbuf)(uint16_t* pixbuf) {
    // handle small wide playfield
    if ((dmactl_ & 0x03) == 1) {
        // handle small playfield
        pixbuf += (40 - 32) * 8 / 2;
    }

    palette[0] = colbk_;
    palette[1] = colpf[0];
    palette[2] = colpf[1];
    palette[3] = colpf[2];

    uint8_t* scrn = screendata_ptr;

    // 4 pixels per byte, 2 bits/4colors per pixel
    for (uint i = 0; i < chars_scanline; ++i) {
        uint8_t c = scrn[i];
        constexpr int WIDE = 8;
        uint8_t color = (c >> 6) & 0x03;
        memset16(pixbuf, palette[color], WIDE);
        pixbuf += WIDE;

        color = (c >> 4) & 0x03;
        memset16(pixbuf, palette[color], WIDE);
        pixbuf += WIDE;

        color = (c >> 2) & 0x03;
        memset16(pixbuf, palette[color], WIDE);
        pixbuf += WIDE;

        color = (c) & 0x03;
        memset16(pixbuf, palette[color], WIDE);
        pixbuf += WIDE;
    }
}

void __not_in_flash_func(mode_B_prepare_scanbuf)(uint16_t* pixbuf) {
    uint8_t* scrn = screendata_ptr;
    palette[0] = colbk_;
    palette[1] = colpf[0];

    // 8 pixels per byte, write pixels twice
    for (uint i = 0; i < chars_scanline; ++i) {
        uint8_t c = scrn[i];

        for (int i = 7; i >= 0; i--) {
            uint8_t color = (c >> i) & 0x01;
            *(pixbuf++) = palette[color];
            *(pixbuf++) = palette[color];
        }
    }
}

void __not_in_flash_func(mode_E_prepare_scanbuf)(uint16_t* scanbuf) {
    uint16_t* pixbuf = scanbuf;
    uint8_t* scrn = screendata_ptr;
    palette[0] = colbk_;
    palette[1] = colpf[0];
    palette[2] = colpf[1];
    palette[3] = colpf[2];

    for (int i = 0; i < chars_scanline; ++i) {
        uint8_t c = scrn[i];

        uint8_t color = (c >> 6) & 0x03;
        *(pixbuf) = palette[color];
        *(pixbuf + 1) = palette[color];
        color = (c >> 4) & 0x03;
        *(pixbuf + 2) = palette[color];
        *(pixbuf + 3) = palette[color];
        color = (c >> 2) & 0x03;
        *(pixbuf + 4) = palette[color];
        *(pixbuf + 5) = palette[color];
        color = (c) & 0x03;
        *(pixbuf + 6) = palette[color];
        *(pixbuf + 7) = palette[color];
        pixbuf += 8;
    }

    // //  display pm
    // pm_missile_scanline(scanbuf, line);

    // // update scanline with rendered player graphics
    // pm_player_scanline(scanbuf, line);
}

void __not_in_flash_func(mode_F_prepare_scanbuf)(uint16_t* scanbuf) {
    uint16_t* pixbuf = scanbuf;

    uint8_t* scrn = screendata_ptr;

    // mode f operates in multiple GTIA modes
    switch (prior_ >> 6) {
        case 0:

            palette[0] = colpf[2];
            palette[1] =
                color2rgb((reg_colpf[2] & 0xF0) | (reg_colpf[1] & 0x0F));

            // 8 pixels per byte, write pixels
            for (uint i = 0; i < chars_scanline; ++i) {
                uint8_t c = scrn[i];

                for (int bit = 7; bit >= 0; bit--) {
                    uint8_t color = (c >> bit) & 0x01;
                    *(pixbuf++) = palette[color];
                }
            }
            break;

        case 0x01:  // 1 color 16 luma mode
            for (uint i = 0; i < chars_scanline; ++i) {
                // 2 pixels per byte, 4 bit pixel data is used as luma for
                // background color each pixels is 4 screen pixels
                uint8_t c = scrn[i];
                uint16_t rgbcolor = color2rgb(reg_colbk_ | (c >> 4));
                // memset16(pixbuf, rgbcolor, 4);
                *(pixbuf++) = rgbcolor;
                *(pixbuf++) = rgbcolor;
                *(pixbuf++) = rgbcolor;
                *(pixbuf++) = rgbcolor;

                rgbcolor = color2rgb(reg_colbk_ | (c & 0x0F));
                // memset16(pixbuf + 4, rgbcolor, 4);
                // pixbuf += 8;
                *(pixbuf++) = rgbcolor;
                *(pixbuf++) = rgbcolor;

                *(pixbuf++) = rgbcolor;
                *(pixbuf++) = rgbcolor;
            }
            break;

        case 0x02:  // 9 color mode
            // pixel values are used to select color registers
            // 0000-0011 : P0-P3
            // x100-x111 : PF0-PF3
            // 10xx : background
            palette[0] = colpm_[0];
            palette[1] = colpm_[1];
            palette[2] = colpm_[2];
            palette[3] = colpm_[3];
            palette[4] = colpf[0];
            palette[5] = colpf[1];
            palette[6] = colpf[2];
            palette[7] = colpf[3];
            // TODO handle background color pixel value 10xx

            for (uint i = 0; i < chars_scanline; ++i) {
                // 2 pixels per byte, 4 bit pixel data is used as luma for
                // background color each pixels is 4 screen pixels
                uint8_t c = scrn[i];
                uint16_t rgbcolor = palette[c >> 4];
                // memset16(pixbuf, rgbcolor, 4);
                *(pixbuf++) = rgbcolor;
                *(pixbuf++) = rgbcolor;
                *(pixbuf++) = rgbcolor;
                *(pixbuf++) = rgbcolor;

                rgbcolor = palette[c & 0x0F];
                // memset16(pixbuf + 4, rgbcolor, 4);
                // pixbuf += 8;
                *(pixbuf++) = rgbcolor;
                *(pixbuf++) = rgbcolor;
                *(pixbuf++) = rgbcolor;
                *(pixbuf++) = rgbcolor;
            }
            break;

        case 0x03:  // 16 color 1 luma mode
            for (uint i = 0; i < chars_scanline; ++i) {
                // 2 pixels per byte, 4 bit pixel data is used as luma for
                // background color each pixels is 4 screen pixels
                uint8_t c = scrn[i];
                uint16_t rgbcolor = color2rgb((c & 0x0F) | reg_colbk_);
                // memset16(pixbuf, rgbcolor, 4);
                *(pixbuf++) = rgbcolor;
                *(pixbuf++) = rgbcolor;
                *(pixbuf++) = rgbcolor;
                *(pixbuf++) = rgbcolor;

                rgbcolor = color2rgb((c << 4) | reg_colbk_);
                // memset16(pixbuf + 4, rgbcolor, 4);
                // pixbuf += 8;
                *(pixbuf++) = rgbcolor;
                *(pixbuf++) = rgbcolor;
                *(pixbuf++) = rgbcolor;
                *(pixbuf++) = rgbcolor;
            }
            break;
    }
    // increment line counter
    // window_mode_y_++;
}

void __not_in_flash_func(mode_0_prepare_scanbuf)(uint16_t* scanbuf) {
    const uint32_t col = (colbk_ << 16) | colbk_;
    memset32_dma((uint32_t*)scanbuf, col, 160);
}

void __not_in_flash_func(blankline_prepare_scanbuf)(uint16_t* scanbuf) {
    const uint32_t col = (colbk_ << 16) | colbk_;
    memset32_dma((uint32_t*)scanbuf, col, 160);
}
