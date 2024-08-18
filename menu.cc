#include "menu.h"

#include "gtia.h"
#include "memory.h"
#include "sound.h"
#include "tusb.h"
#include "tusb_config.h"

#define MAXROMS 9
const char *roms[MAXROMS] = {"BASIC",          "Miner 2049",   "PACMAN",
                             "SPACE INVADERS", "Boulder Dash", "Donkey Kong",
                             "Pengo",          "MS Pacman",    "Star Raiders"};
const char hello[] = "NEO6502 ATARI 8BIT";
const char games[] = "games";
const char press[] = "press start/fire to continue";

// function prototypes
void init_display();
void print_banner();
void print_sound_option();
void print(uint16_t, const char *);

int rom = 0;

uint8_t atascii_to_iv(uint8_t c) {
    // strip off top bit
    c &= 0x7F;
    if (c <= 31) return (c + 64);
    if (c <= 95) return (c - 32);
    return c;
}

void menu() {
    printf("Display start menu\n");

    extern uint8_t regPORTA;
    extern uint8_t trig0_;

    init_display();
    print_banner();
    print_sound_option();

    print(0x4000 + 10 + rom * 40, "*");

    // while (peek(0xD010)) {
    // wait for fire button of start pressed
    // while ((trig0_) || (consol_ != CONSOL_START)) {
    bool done = false;
    while (!done) {
        // check input to end loop
        if (consol_ == CONSOL_START) done = true;

        if (!trig0_) done = true;

        if (consol_ == CONSOL_SELECT) {
            enable_sound(!sound_enabled());
            print_sound_option();
        }

        if (regPORTA != 0xFF) {
            print(0x4000 + 10 + rom * 40, " ");

            if (!(regPORTA & (1)))  // UP
                if (rom > 0)
                    rom--;
                else
                    rom = MAXROMS - 1;

            if (!(regPORTA & (1 << 1)))  // DOWN
                if (rom < MAXROMS)
                    rom++;
                else
                    rom = 0;

            print(0x4000 + 10 + rom * 40, "*");
        }

        // slow down a bit
        for (auto i = 0; i < 6; i++) {
            tuh_task();
            sleep_ms(20);
        }
    }

    // for test force mspacman
    // rom = 7;
}

void init_display() {
    // setup display list
    uint8_t dlist[] = {
        0x70, 0x70, 0x70,  // 24 blank lines
        0x46, 0x00, 0x30,  // mode 6 + LMS to 0x3000
        0x70,              // 8 blank lines
        0x06,              // mode 6 lines
        0x70, 0x70, 0x70,  // 24 blank lines
        0x42, 0x00, 0x40,  // mode 2 + LMS 0x4000
        0x02, 0x02, 0x02,  // mode 2
        0x02, 0x02, 0x02,  // mode 2
        0x02, 0x02, 0x02,  // mode 2
        0x02, 0x02, 0x02,  // mode 2
        0x02, 0x02, 0x02,  // mode 2
        0x02, 0x02, 0x02,  // mode 2

        0x41, 0x00, 0x10  // JVB, restart display list (@ 0x1000)
    };

    poke_n(0x1000, dlist, sizeof dlist);
    poke(0xD402, 0);
    poke(0xD403, 0x10);

    // initialize dmactl: enable dlist dma and normal wide
    poke(0xD400, (1 << 5) | (2));
    poke(0xD401, 0);

    // setup color registers
    uint8_t colors[]{0x84, 0x8C, 0x00, 0x34, 0x0};
    poke_n(0xD016, colors, sizeof colors);

    // point chbase to default ROM character set @ 0xE000
    poke(0xD409, 0xCC);

    // clear screen ram
    memset(mem + 0x3000, 0, 40);
    memset(mem + 0x4000, 0, 400);
}

void print_banner() {
    // mode 6 at 0x3000
    print(0x3000 + (20 - strlen(hello)) / 2, hello);
    print(0x3014 + (20 - strlen(games)) / 2, games);

    // mode 2 text
    for (int i = 0; i < MAXROMS; i++) print(0x4000 + 12 + i * 40, roms[i]);

    print(0x4000 + 16 * 40 + (40 - strlen(press)) / 2, press);
}

void print_sound_option() {
    const char *snd_off = "press option to disable sound";
    const char *snd_on = "press option to enable sound";

    if (sound_enabled())
        print(0x4000 + 15 * 40 + (40 - strlen(snd_off)) / 2, snd_off);
    else
        print(0x4000 + 15 * 40 + (40 - strlen(snd_on)) / 2, snd_on);
}

void print(uint16_t addr, const char *str) {
    while (*str) {
        poke(addr++, atascii_to_iv(*(str++)));
    }
}
