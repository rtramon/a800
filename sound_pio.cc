#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "pokey.h"
#include "sound.h"
#include "sound_pio.pio.h"

// pokey slow audio clock is 15khz
// pokey fast audio clock is 64khz
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
bool sound_is_enabled;

#if defined(OPTION_SOUND_INTERRUPTS)
void on_pio_irq(void) {
    printf("pio irq: %x\n", AUDIO_PIO->irq);

    if (AUDIO_PIO->irq & 1) AUDIO_PIO->irq = 1;

    if (AUDIO_PIO->irq & 2) AUDIO_PIO->irq = 2;
    if (AUDIO_PIO->irq & 4) AUDIO_PIO->irq = 4;
    if (AUDIO_PIO->irq & 8) AUDIO_PIO->irq = 8;
}
#endif

void init_sound(uint8_t chan) {
    chan;

    audio_pio_offset = pio_add_program(AUDIO_PIO, &sound_pio_program);
    for (chan = 0; chan < MAX_AUDIO_CHANNEL; chan++) {
        sound_pio_program_init(AUDIO_PIO, chan, audio_pio_offset, AUDIO_GPIO);
        pio_sm_set_enabled(AUDIO_PIO, chan, true);
    }

    enable_sound(true);

#if defined(OPTION_SOUND_INTERRUPTS)
    // setup PIO interrupt
    irq_set_exclusive_handler(AUDIO_PIO_IRQ, on_pio_irq);
    irq_set_enabled(AUDIO_PIO_IRQ, true);
    AUDIO_PIO->inte0 = PIO_IRQ0_INTE_SM0_BITS | PIO_IRQ0_INTE_SM1_BITS |
                       PIO_IRQ0_INTE_SM2_BITS | PIO_IRQ0_INTE_SM3_BITS;
#endif
}
bool sound_enabled() { return sound_is_enabled; };
extern void enable_sound(bool enable) { sound_is_enabled = enable; };

static void inline set_sound_output(uint8_t chan, uint32_t value) {
    pio_sm_put_blocking(AUDIO_PIO, chan, value);
    pio_sm_put_blocking(AUDIO_PIO, chan, value);
    pio_sm_exec(AUDIO_PIO, chan, pio_encode_jmp(audio_pio_offset));
}

void __not_in_flash_func(play_sound)(uint8_t chan, uint32_t freq) {
    if (!sound_is_enabled) return;

    if (chan < MAX_AUDIO_CHANNEL) {
        uint top = (PIO_SM_FREQ / freq) / 2;

        set_sound_output(chan, top);
    }
}

void __not_in_flash_func(stop_sound)(uint8_t chan) {
    if (!sound_is_enabled) return;

    if (chan < MAX_AUDIO_CHANNEL) {
        pio_sm_exec(AUDIO_PIO, chan, pio_encode_jmp(audio_pio_offset));
    }
}

void __not_in_flash_func(play_pokey_sound)(uint8_t chan) {
    if (!sound_is_enabled) return;

    static uint prev_top[4];

    // translate pokey frequency to audio frequency
    if (audctl_ & (1 << 3)) {
        if ((chan == AUDIO_CHANNEL4) || (chan == AUDIO_CHANNEL3)) {
            // channel 3 and 4 are combined, only use channel 4
            // calculate 16 bit delay
            uint top = 0;
            if (audc[3] & 0x0F)
                if (audctl_ & (1 << 5))
                    // channel 1.79 Mhz clock
                    top = (7 + audf[2] + audf[3] * 256) / 2;
                else
                    // regular audio clock
                    top = (4 + audf[2] + audf[3] * 256) / 2;

            if (top != prev_top[3]) {
                prev_top[3] = top;
                set_sound_output(AUDIO_CHANNEL3, 0);
                set_sound_output(AUDIO_CHANNEL4, top);
            }
            return;
        }
    }

    if (audctl_ & (1 << 4)) {
        if ((chan == AUDIO_CHANNEL1) || (chan == AUDIO_CHANNEL2)) {
            // calculate 16 bit delay
            uint top = 0;
            if (audc[1] & 0x0F)
                if (audctl_ & (1 << 6))
                    // channel 1.79 Mhz clock
                    top = (7 + audf[0] + audf[1] * 256) / 2;
                else
                    // regular audio clock
                    top = (4 + audf[0] + audf[1] * 256) / 2;

            if (top != prev_top[1]) {
                prev_top[1] = top;

                // setup pio sm
                set_sound_output(AUDIO_CHANNEL1, 0);
                set_sound_output(AUDIO_CHANNEL2, top);
            }
            return;
        }
    }

    // if (pokey_freq) {
    uint top = 0;
    // printf("audio chan:%d audf:%d audc:%d\n", chan, audf[chan], audc[chan]);

    if (audc[chan] & 0x0F) top = (((1 + audf[chan])) * 28) / 2;
    // printf("audio chan:%d top:%d\n", chan, top);
    if (prev_top[chan] != top) {
        prev_top[chan] = top;
        set_sound_output(chan, top);
    }
}

void __not_in_flash_func(set_timer_int)(uint chan, bool enabled) {
#if defined(OPTION_SOUND_INTERRUPTS)
    pio_set_irqn_source_enabled(AUDIO_PIO, AUDIO_PIO_IRQ,
                                (pio_interrupt_source_t)(pis_interrupt0 + chan),
                                enabled);

// printf("PIO Int %d set to %d\n", pis_interrupt0 + chan, enabled);
#endif
}

void __not_in_flash_func(audctl_update)(uint8_t audctl) {
    printf("audctl: %02X\n", audctl);
    if (audctl & 1) printf("\t15kHz\n");
    if (audctl & (1 << 4)) printf("\tch1&2 combined\n");
    if (audctl & (1 << 3)) printf("\tch3&4 combined\n");
    if (audctl & (1 << 5)) printf("\tch3 1.79Mhz\n");
    if (audctl & (1 << 6)) printf("\tch1 1.79Mhz\n");
}