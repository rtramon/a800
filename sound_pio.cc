#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "pokey.h"
#include "sound.h"
#include "sound_pio.pio.h"

// pokey fast audio clock is 64kHhz
// #define PIO_SM_FREQ 1'024'000UL
// pokey cpu clock 1.789Mhz
#define PIO_SM_FREQ 1'789'000UL

#define AUDIO_GPIO (20)
#define AUDIO_PIO (pio1)
// #define AUDIO_PIO_IRQ (PIO1_IRQ_0)  // asserts on PIO1_IRQ_0
#define AUDIO_PIO_IRQ (0)
#define MAX_AUDIO_CHANNEL (4)
constexpr float clk_fast = 252'000'000.0 / 1'789'000.0;
constexpr float clk_64k = 252'000'000.0 / 1'024'000.0;

uint audio_pio_offset;

void on_pio_irq(void) {
    printf("pio irq: %x\n", AUDIO_PIO->irq);

    if (AUDIO_PIO->irq & 1) AUDIO_PIO->irq = 1;

    if (AUDIO_PIO->irq & 2) AUDIO_PIO->irq = 2;
    if (AUDIO_PIO->irq & 4) AUDIO_PIO->irq = 4;
    if (AUDIO_PIO->irq & 8) AUDIO_PIO->irq = 8;
}

void init_sound(uint8_t chan) {
    chan;

    audio_pio_offset = pio_add_program(AUDIO_PIO, &sound_pio_program);
    for (chan = 0; chan < MAX_AUDIO_CHANNEL; chan++) {
        sound_pio_program_init(AUDIO_PIO, chan, audio_pio_offset, AUDIO_GPIO);
        pio_sm_set_clkdiv(AUDIO_PIO, chan, clk_64k);
        pio_sm_set_enabled(AUDIO_PIO, chan, true);
    }

    // setup PIO interrupt
    irq_set_exclusive_handler(AUDIO_PIO_IRQ, on_pio_irq);
    irq_set_enabled(AUDIO_PIO_IRQ, true);
    AUDIO_PIO->inte0 = PIO_IRQ0_INTE_SM0_BITS | PIO_IRQ0_INTE_SM1_BITS |
                       PIO_IRQ0_INTE_SM2_BITS | PIO_IRQ0_INTE_SM3_BITS;
}

void __not_in_flash_func(play_sound)(uint8_t chan, uint32_t freq) {
    if (chan < MAX_AUDIO_CHANNEL) {
        uint top = (PIO_SM_FREQ / freq) / 2;

        pio_sm_exec(AUDIO_PIO, chan, pio_encode_jmp(audio_pio_offset));
        pio_sm_put_blocking(AUDIO_PIO, chan, top);
    }
}

void __not_in_flash_func(stop_sound)(uint8_t chan) {
    if (chan < MAX_AUDIO_CHANNEL) {
        pio_sm_exec(AUDIO_PIO, chan, pio_encode_jmp(audio_pio_offset));
    }
}

void __not_in_flash_func(play_pokey_sound)(uint8_t chan, uint8_t pokey_freq) {
    // translate pokey frequency to audio frequency

    if (audctl_ & (1 << 3)) {
        if ((chan == AUDIO_CHANNEL4) || (chan == AUDIO_CHANNEL3)) {
            // channel 3 and 4 are combined, only use channel 4
            // calculate 16 bit delay
            uint cnt = audf3_ + (audf4_) * 256;

            pio_sm_exec(AUDIO_PIO, AUDIO_CHANNEL3,
                        pio_encode_jmp(audio_pio_offset));
            pio_sm_put_blocking(AUDIO_PIO, AUDIO_CHANNEL3, 0);

            pio_sm_exec(AUDIO_PIO, AUDIO_CHANNEL4,
                        pio_encode_jmp(audio_pio_offset));
            pio_sm_put_blocking(AUDIO_PIO, AUDIO_CHANNEL4, cnt / 2);

            return;
        }
    }

    if (audctl_ & (1 << 4)) {
        if ((chan == AUDIO_CHANNEL1) || (chan == AUDIO_CHANNEL2)) {
            // calculate 16 bit delay
            uint cnt = audf1_ + (audf2_) * 256;

            // setup pio sm
            pio_sm_exec(AUDIO_PIO, AUDIO_CHANNEL1,
                        pio_encode_jmp(audio_pio_offset));
            pio_sm_put_blocking(AUDIO_PIO, AUDIO_CHANNEL1, 0);

            pio_sm_exec(AUDIO_PIO, AUDIO_CHANNEL2,
                        pio_encode_jmp(audio_pio_offset));
            pio_sm_put_blocking(AUDIO_PIO, AUDIO_CHANNEL2, cnt / 2);

            return;
        }
    }

    if (pokey_freq) {
        uint cnt = (pokey_freq + 1) * 28;
        pio_sm_exec(AUDIO_PIO, chan, pio_encode_jmp(audio_pio_offset));
        pio_sm_put_blocking(AUDIO_PIO, chan, cnt);
        // play_sound(chan, 64000 / (pokey_freq + 1));
        // play_sound(chan, pokey_freq);
    } else
        stop_sound(chan);
}

void __not_in_flash_func(set_timer_int)(uint chan, bool enabled) {
    pio_set_irqn_source_enabled(AUDIO_PIO, AUDIO_PIO_IRQ,
                                (pio_interrupt_source_t)(pis_interrupt0 + chan),
                                enabled);

    // printf("PIO Int %d set to %d\n", pis_interrupt0 + chan, enabled);
}

void __not_in_flash_func(audctl_update)(uint8_t audctl) {
    printf("audctl: %02X\n", audctl);
    if (audctl & 1) printf("\t15kHz\n");
    if (audctl & (1 << 4)) printf("\tch1&2 combined\n");
    if (audctl & (1 << 3)) printf("\tch3&4 combined\n");
    if (audctl & (1 << 5)) printf("\tch3 1.79Mhz\n");
    if (audctl & (1 << 6)) printf("\tch1 1.79Mhz\n");
}