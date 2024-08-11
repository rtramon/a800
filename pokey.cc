#include "pokey.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#include <cstdlib>

#include "gtia.h"
#include "hardware/timer.h"
#include "m6520.h"
#include "mos65C02.h"
#include "pico/platform.h"
#include "pico/stdio_uart.h"
#include "sound.h"

using namespace std;

////////////////////////////////////////////////////////////////////
// Atari Custom POKEY
//
// ref: atariwiki.org/wiki
////////////////////////////////////////////////////////////////////
//

volatile uint8_t skctl_;
volatile uint8_t skstat_;
volatile uint8_t irqen_;
volatile uint8_t irqst_;
volatile uint8_t kbcode_;
uint8_t random_;
uint8_t audf1_, audf2_, audf3_, audf4_;
uint8_t audc1_, audc2_, audc3_, audc4_;
uint8_t audctl_;
int cnt1_, cnt2_, cnt3_, cnt4_;
uint8_t aud_out;
uint32_t start;

static int serout_delay;
static int serxmt_delay;

enum POKEY_READ_REG_t {
    POT0 = 0x0,
    POT1 = 0x01,
    POT2 = 0x02,
    POT5 = 0x05,
    ALLPOT = 0x8,
    KBCODE = 0x09,
    RANDOM = 0x0a,
    SERIN = 0x0d,
    IRQST = 0xe,
    SKSTAT = 0xf
};
enum POKEY_WRITE_REG_t {
    AUDF1 = 0x0,
    AUDC1 = 0x1,
    AUDF2 = 0x2,
    AUDC2 = 0x3,
    AUDF3 = 0x4,
    AUDC3 = 0x5,
    AUDF4 = 0x6,
    AUDC4 = 0x7,
    AUDCTL = 0x8,
    STIMER = 0x9,
    SKRES = 0x0a,
    POTGO = 0x0b,
    NONAME = 0x0c,
    SEROUT = 0xd,
    IRQEN = 0xe,
    SKCTL = 0xf
};

// function prototype
uint8_t ascii_keycode(uint8_t);

#if defined(EMU_M6502)
#include "emu_m6502.h"
extern m6502 cpu;

void __always_inline handle_irq() {
    if (irqen_ & (~irqst_)) {
        // printf("IRQST $%02X\n", irqst_);
        cpu.irq = true;
    } else
        cpu.irq = false;
}
#else
void __always_inline handle_irq() {
    // Handle pending IRQ
    if (irqen_ & (~irqst_)) {
        mos65c02_irq_on();
    } else {
        mos65c02_irq_off();
    }
}
#endif

// Functions
void pokey_reset() {
    skctl_ = 0;
    skstat_ = 0xFF;
    irqen_ = 0;
    irqst_ = 0xF7;  // bit 3 is always 0 - serial transmit complete
    random_ = 0;

    audctl_ = 0;
    audc1_ = audc2_ = audc3_ = audc4_ = 0;

    // invisible registers
    cnt1_ = cnt2_ = cnt3_ = cnt4_ = 0xFF;
    aud_out = 0;

    serout_delay = 0;
    serxmt_delay = 0;

    // initialize GPIO20 as audio output
    // gpio_init(20);
    // gpio_set_dir(20, GPIO_OUT);
    // gpio_put(20, 0);
    // gpio_set_dir_out_masked((1ul << 20));
    // gpio_clr_mask(1ul << 20);
}

uint8_t __not_in_flash_func(pokey_read)(uint8_t reg) {
    uint8_t data;
    // printf("R POKEY : $%02x\n", reg);

    switch (reg) {
        case POT0:  // value of paddle
        case POT1:
        case POT2:
        case POT5:
            // fallthrough
        case ALLPOT:  // indicate paddle values are ready
            data = 0;
            break;

        case KBCODE:  // value of last pressed key
            // printf("POKEY read to KBCODE : $%02x\n", kbcode_);
            data = kbcode_;
            break;

        case SERIN:
            data = 0xff;
            break;

        case SKSTAT:
            data = skstat_;
            break;

        case IRQST:
            // printf("R POKEY IRQST: $%02x  = IRQEN: $%02x\n", irqst_, irqen_);
            data = irqst_;
            break;

        case RANDOM:
            // data = random_++;
            data = rand() % 256;
            break;

        default:
            // printf("PANIC! POKEY read not implemented register $%02x\n",
            // reg);

            data = 0xFF;
            break;
    }

    return data;
}

void __not_in_flash_func(pokey_write)(uint8_t reg, uint8_t data) {
    // printf("POKEY write $%02x : $%02x\n", reg, data);
    switch (reg) {
        case AUDF1:
            audf1_ = data;
// printf("W POKEY AUDF1 %02X\n", data);
#if defined(OPTION_AUDIO)
            play_pokey_sound(20, audf1_);
#endif
            break;
        case AUDC1:
            // printf("W POKEY AUDC1 %02X\n", data);
            audc1_ = data;
#if defined(OPTION_AUDIO)
            if ((audc1_ & 0x0F) == 0) stop_sound(20);
#endif
            break;

        case AUDF2:
            audf2_ = data;
            break;
        case AUDC2:
            // if (data & 0x0F)
            //   printf("W POKEY AUDC2 %02X\n", data);

            audc2_ = data;
            break;
        case AUDF3:
            audf3_ = data;
            break;
        case AUDC3:
            // if (data & 0x0F)
            //   printf("W POKEY AUDC3 %02X\n", data);

            audc3_ = data;
            break;
        case AUDF4:
            audf4_ = data;
            break;
        case AUDC4:
            // if (data & 0x0F)
            //   printf("W POKEY AUDC4 %02X\n", data);

            audc4_ = data;
            break;

        case AUDCTL:
            audctl_ = data;
            // printf("POKEY AUDCTL: $%02x\n", audctl_);
            break;

        case SKRES:  // resets bits 5,6,7 of SKSTAT to 1
            // skstat_ |= 0b11100000;
            skstat_ = 0xFF;
            break;

        case SKCTL:
            // printf("W POKEY SKCTL $%02x\n", data);
            skctl_ = data;
            if ((skctl_ & 0x03) == 0) {
                // special case pokey initialize
                pokey_reset();
            }
            break;

        case SEROUT:
            // serial output complete is always set
            // printf("W POKEY SEROUT: $%02x\n", data);
            // printf("  \tIRQEN $%02x\n", irqen_);

            serout_delay = 100;
            serxmt_delay = 100;

            // disable Serial output transmission completed interrupt
            irqst_ |= (1 << 3);
            break;

        case STIMER:
            // set counter registers to their audf*_ value;
            cnt1_ = audf1_;
            cnt2_ = audf2_;
            cnt3_ = audf3_;
            cnt4_ = audf4_;
            // printf("POKEY Write to STIMER : $%02x\n", data);
            break;

        case IRQEN:  // interupt status
            irqen_ = data;
            // printf("W IRQEN: $%02x  IRQST $%02x\n", irqen_, irqst_);
            irqst_ |= (~irqen_ & 0xF7);
            // printf("  after IRQST $%02x\n", irqst_);
            handle_irq();
            break;

        case POTGO:
        case NONAME:
            // not implemented; ignore
            break;

        default:
            printf("PANIC POKEY write not implemented register $%02x : $%02x\n",
                   reg, data);
            break;
    }
}

void __not_in_flash_func(pokey_tick)() {
    constexpr int clock_step = 1;

    if ((skctl_ & 0x03) == 0)
        // pokey is in init
        return;

    // emulate stupid serial output transmission
    if (serout_delay > 0) {
        serout_delay -= clock_step;
        if (serout_delay <= 0) {
            // emulated Serial output data needed ready interrupt

            if (irqen_ & (1 << 4)) {
                irqst_ &= (~(1 << 4));
                // irqst_ &= 0xEF;
                // puts("POKEY serial output data ready");

#ifdef EMU_M6502
                cpu.irq = true;
#else
                mos65c02_irq_on();
#endif
            }
        }
    }
    if (serxmt_delay > 0) {
        serxmt_delay -= clock_step;
        if (serxmt_delay <= 0) {
            irqst_ &= (~(1 << 3));
            // puts("POKEY serial transmit done");
            if (irqen_ & (1 << 3))
#ifdef EMU_M6502
                cpu.irq = true;
#else
                mos65c02_irq_on();
#endif
        }
    }

    // return;

    // #ifdef AUDIO
    // timers
    // timers count down to zero
    // timers (audio channels) 1, 2 and 4 can generate an IRQ
    // IRQST D0 timer1, D1 timer2, D2 timer 4
    // implement setting $28:
    // chan 1 64 KHz
    // chan3 fast clock 1.79 MHz
    // chan 3 + 4 linked mode

    // slow clock frequency select audctl bit 0 ; not supported
    // for now only the slow 15kHZ audio clock is emulated
    // static uint clock_tick = 0;
    // if ((clock_tick++ % 114) == 0) {
    //   cnt1_ -= clock_step;
    //   cnt2_ -= clock_step;

    //   if (cnt1_ <= 0) {
    //     cnt1_ = audf1_;
    //     if (audc1_ & 0x0F > 0)
    //       aud_out ^= (1 << 1);

    //     if (irqen_ & (1 << 0)) {
    //       irqst_ &= (~(1 << 0));
    //       mos65c02_irq_on();
    //     }
    //   }
    // if (cnt2_ <= 0) {
    //   cnt2_ = audf2_;
    //   if (audc2_ & 0x0F > 0)
    //     aud_out ^= (1 << 2);

    //   if (irqen_ & (1 << 1)) {
    //     irqst_ &= (~(1 << 1));
    //     mos65c02_irq_on();
    //   }
    // }
    //   // create audio output
    //   if (aud_out)
    //     gpio_set_mask(1 << 20);
    //   else
    //     gpio_clr_mask(1 << 20);
    // }

    cnt3_ -= clock_step;

    if (cnt3_ <= 0) {
        // reload counter 3
        cnt3_ = audf3_;
        // if (audc3_ & 0x0F > 0)
        //   aud_out ^= (1 << 3);

        // checked for linked mode with tim4
        if (audctl_ & (1 << 3)) {
            cnt4_--;

            if (cnt4_ <= 0) {
                // linked timer 3/4 zero, generate irq if enabled
                // and reload counters
                if (irqen_ & (1 << 2)) {
                    irqst_ &= (~(1 << 2));  // timer 4 expired
                    // printf("TIM4 IRQST $%02x\n", irqst_);
                    mos65c02_irq_on();
                }

                // reload counter 4
                cnt4_ = audf4_;
            }
        }
    }

    // #endif
}

void pokey_report_break() {
    if (irqen_ & (1 << 7)) {
        irqst_ &= (~(1 << 7));
        // printf("BRKKEY IRQST $%02x\n", irqst_ & (1 << 7));

#ifdef EMU_M6502
        cpu.irq = true;
#else
        mos65c02_irq_on();
#endif
    }
}

void pokey_report_keycode(uint8_t code) {
    // printf("pokey_report_keycode: $%x\n", code);
    kbcode_ = code;
    if (irqen_ & (1 << 6)) {
        irqst_ &= (~(1 << 6));  // other key pressed
                                // printf("IRQST $%02x\n", irqst_ & (1 << 6));

        // skstat_ &= (~(1 << 2)); // used for software auto repeat
#ifdef EMU_M6502
        cpu.irq = 1;
#else
        mos65c02_irq_on();
#endif
    }
}

// void __not_in_flash_func(pokey_keyb_event)(uint8_t code) {
void pokey_keyb_event(uint8_t code) {
    static bool escape = false;
    printf("keybd code: $%02x\n", code);

    // intercept escape key
    if (code == 0x1B) {
        escape = true;
        return;
    }

    if (escape) {
        switch (code) {
            case 0x50:
                // F1
                puts("START");
                gtia_set_consol(CONSOL_START);
                break;

            case 0x51:
                // F2
                puts("SELECT");
                gtia_set_consol(CONSOL_SELECT);
                break;

            case 0x52:
                // F3
                puts("OPTION");
                gtia_set_consol(CONSOL_OPTION);
                break;

            case 0x53:
                // F4 - BREAK key
                pokey_report_break();
                break;

            case 0x41:  // cursor up
                m6520_joy0(14);
                break;
            case 0x42:  // cursor down
                m6520_joy0(13);
                break;
            case 0x43:  // right
                m6520_joy0(0x07);
                break;
            case 0x44:  // left
                m6520_joy0(11);
                break;
            case 0x46:  // end to simulate neutral joystick
                m6520_joy0(0x0F);
                break;

            case 0x4f:  // function keys
            case 0x5b:  // cursor keys

                // second byte after escape, just ignore
                return;
        }

        escape = false;
        return;
    }

    kbcode_ = ascii_keycode(code);
    if (irqen_ & (1 << 6)) {
        irqst_ &= (~(1 << 6));  // other key pressed
                                // printf("IRQST $%02x\n", irqst_);

        // skstat_ &= (~(1 << 2)); // used for software auto repeat
#ifdef EMU_M6502
        cpu.irq = true;
#else
        mos65c02_irq_on();
#endif
    }
}

uint8_t __not_in_flash_func(ascii_keycode)(uint8_t a) {
    // convert ascii code to an atari keyboard scan code
    uint8_t kbcode;
    uint8_t shift = false;

    switch (toupper(a)) {
        case 'A':
            kbcode = 0x3F;
            break;
        case 'B':
            kbcode = 0x15;
            break;
        case 'C':
            kbcode = 0x12;
            break;
        case 'D':
            kbcode = 0x3A;
            break;
        case 'E':
            kbcode = 0x2A;
            break;
        case 'F':
            kbcode = 0x38;
            break;
        case 'G':
            kbcode = 0x3D;
            break;
        case 'H':
            kbcode = 0x39;
            break;
        case 'I':
            kbcode = 0x0D;
            break;
        case 'J':
            kbcode = 0x01;
            break;
        case 'K':
            kbcode = 0x05;
            break;
        case 'L':
            kbcode = 0x00;
            break;
        case 'M':
            kbcode = 0x25;
            break;
        case 'N':
            kbcode = 0x23;
            break;
        case 'O':
            kbcode = 0x08;
            break;
        case 'P':
            kbcode = 0x0A;
            break;
        case 'Q':
            kbcode = 0x2F;
            break;
        case 'R':
            kbcode = 0x28;
            break;

        case 'S':
            kbcode = 0x3E;
            break;
        case 'T':
            kbcode = 0x2D;
            break;
        case 'U':
            kbcode = 0x0B;
            break;
        case 'V':
            kbcode = 0x10;
            break;
        case 'W':
            kbcode = 0x2E;
            break;
        case 'X':
            kbcode = 0x16;
            break;
        case 'Y':
            kbcode = 0x2B;
            break;
        case 'Z':
            kbcode = 0x17;
            break;
        case 0x0D:  // enter/return
            kbcode = 0x0C;
            break;
        case ' ':
            kbcode = 0x21;
            break;

        case '1':
            kbcode = 0x1F;
            break;
        case '2':
            kbcode = 0x1E;
            break;
        case '3':
            kbcode = 0x1A;
            break;
        case '4':
            kbcode = 0x18;
            break;
        case '5':
            kbcode = 0x1D;
            break;
        case '6':
            kbcode = 0x1B;
            break;
        case '7':
            kbcode = 0x33;
            break;
        case '8':
            kbcode = 0x35;
            break;
        case '9':
            kbcode = 0x30;
            break;
        case '0':
            kbcode = 0x32;
            break;

        case '<':
            kbcode = 0x36;
            break;
        case '>':
            kbcode = 0x37;
            break;
        case ',':
            kbcode = 0x20;
            break;
        case '.':
            kbcode = 0x22;
            break;
        case '=':
            kbcode = 0x0F;
            break;
        case '*':
            kbcode = 0x07;
            break;
        case '"':
            kbcode = 0x1E | (1 << 6);
            shift = true;
            break;
        case '(':
            kbcode = 0x30 | (1 << 6);
            break;
        case ')':
            kbcode = 0x32 | (1 << 6);
            break;
        case '#':
            kbcode = 0x1A | (1 << 6);
            break;
        case '!':
            kbcode = 1 | (1 << 6);
            break;

        case '/':
            kbcode = 0x26;
            break;
        case '?':
            kbcode = 0x26 | (1 << 6);
            break;
        case ';':
            kbcode = 0x02;
            break;
        case '&':
            kbcode = 0x1B | (1 << 6);
            break;
        case '+':
            kbcode = 0x06;
            break;
        case '%':
            kbcode = 0x1D | (1 << 6);
            break;

        case 0x2D:  // -
            kbcode = 0x0E;
            break;
        case 0x3A:  // :
            kbcode = 0x02 | (1 << 6);
            break;
        case 0x24:  // $
            kbcode = 0x18 | (1 << 6);
            break;

        case 8:  // backspace
            kbcode = 0x34;
            break;
        case 9:  // TAB
            kbcode = 0x2C;
            break;

        case 27:  // escape
            kbcode = 0x1C;
            break;
        case 17:  // F1
            kbcode = 0x03;
            break;
        case 18:  // F2
            kbcode = 0x04;
            break;
        case 19:  // F3
            kbcode = 0x13;
            break;
        case 20:  // F4
            kbcode = 0x14;
            break;

        default:
            printf("KB $%02x not mapped (%c)\n", a, a);
            kbcode = 0x16;
            break;
    }

    // if (shift)
    //   skstat_ &= (~(1 << 3));
    // else
    //   skstat_ |= (1 << 3);

    return kbcode;
}
