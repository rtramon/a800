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

#define AUDIO_PWM_STEPS (400)

#define AUDIO_GPIO (20)
#define AUDIO_PIO (pio1)
// #define AUDIO_PIO_IRQ (PIO1_IRQ_0)  // asserts on PIO1_IRQ_0
#define AUDIO_PIO_IRQ (0)
#define MAX_AUDIO_CHANNEL (4)
constexpr float clk_fast = 252'000'000.0 / PIO_SM_FREQ;
// constexpr float clk_64k = 252'000'000.0 / 1'024'000.0;

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

void init_sound() {
    audio_pio_offset = pio_add_program(AUDIO_PIO, &sound_pio_program);
    for (uint chan = 0; chan < MAX_AUDIO_CHANNEL; chan++) {
        sound_pio_program_init(AUDIO_PIO, chan, audio_pio_offset, AUDIO_GPIO);
        pio_sm_set_enabled(AUDIO_PIO, chan, true);
        // set audio output off
        pio_sm_put(AUDIO_PIO, chan, 0);
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

uint vol_table[16] = {0,   100, 130, 160, 190, 220, 250, 280,
                      310, 350, 380, 410, 440, 470, 500, 530};

// uint abs_table[16] = {0,   50,  93,  137, 181, 225, 268, 312,
//                       356, 400, 443, 487, 531, 575, 618, 662};

static void __always_inline set_sound_output(uint8_t chan, uint32_t value,
                                             uint vol) {
    if (value > 0x0FFFF) {
        printf("%d\n", value);
        panic("input exceeded");
    }

    if ((value == 0) || (vol == 0)) {
        pio_sm_put(AUDIO_PIO, chan, 0);
        return;
    }

    // uint16_t high_period = abs_table[vol];
    uint16_t high_period = (vol_table[vol] * value) / 1024;
    uint16_t low_period = value - high_period;
    pio_sm_put(AUDIO_PIO, chan, (high_period << 16) | low_period);
}

void __not_in_flash_func(play_sound)(uint8_t chan, uint32_t freq, uint8_t vol) {
    if (!sound_is_enabled) return;

    if (chan < MAX_AUDIO_CHANNEL) {
        uint top = (PIO_SM_FREQ / freq);

        set_sound_output(chan, top, vol);
    }
}

void __not_in_flash_func(play_sound_p)(uint8_t chan, uint16_t h_period,
                                       uint16_t l_period) {
    if (!sound_is_enabled) return;

    if (chan < MAX_AUDIO_CHANNEL) {
        // pio_sm_put_blocking(AUDIO_PIO, chan, (h_period << 16) | l_period);
        // pio_sm_put_blocking(AUDIO_PIO, chan, l_period);
        // pio_sm_exec(AUDIO_PIO, chan, pio_encode_jmp(audio_pio_offset));
    }
}

void __not_in_flash_func(stop_sound)(uint8_t chan) {
    if (!sound_is_enabled) return;

    if (chan < MAX_AUDIO_CHANNEL) {
        // pio_sm_put(AUDIO_PIO, chan, 0);
        pio_sm_put(AUDIO_PIO, chan, 0);
        // pio_sm_exec(AUDIO_PIO, chan, pio_encode_jmp(audio_pio_offset));
    }
}

void __not_in_flash_func(play_pokey_sound)(uint8_t chan) {
    if (!sound_is_enabled) return;

    // printf("chan %d f:$%02X c:$%02X\n", chan, audf[chan], audc[chan]);
    if (audctl_ & (1 << AUDCTL::CH12)) {
        if (chan == AUDIO_CHANNEL2) {
            // channel 1 and channel 2 combined
            // calculate 16 bit delay, if channel 2 volume is enabled
            uint top;
            if (audctl_ & (1 << AUDCTL::CH1_FAST_CLK))
                // channel 1.79 Mhz clock
                top = (7 + audf[0] + audf[1] * 256);
            else
                // regular audio clock
                top = (4 + audf[0] + audf[1] * 256) * 28;

            // setup pio sm
            set_sound_output(AUDIO_CHANNEL2, top, audc[AUDIO_CHANNEL2] & 0x0F);

            return;
        }
        if (chan = AUDIO_CHANNEL1) {
            uint top;
            if (audctl_ & (1 << AUDCTL::CH1_FAST_CLK))
                // channel 1.79 Mhz clock
                top = (audf[0]);
            else
                // regular audio clock
                top = (audf[0]) * 28;

            set_sound_output(AUDIO_CHANNEL1, top, audc[AUDIO_CHANNEL1] & 0x0F);

            return;
        }
    }
    if (audctl_ & (1 << AUDCTL::CH34)) {
        if (chan == AUDIO_CHANNEL4) {
            // channel 3 and channel 4 are combined

            // calculate 16 bit delay, if channel 4 volume is enabled
            uint top;
            if (audctl_ & (1 << AUDCTL::CH3_FAST_CLK))
                // channel 3 fast clock  1.79 Mhz clock
                top = (7 + audf[2] + audf[3] * 256);
            else
                // regular audio clock
                top = (4 + audf[2] + audf[3] * 256) * 28;

            // setup pio sm
            set_sound_output(AUDIO_CHANNEL4, top, audc[AUDIO_CHANNEL4] & 0x0F);

            return;
        }

        if (chan == AUDIO_CHANNEL3) {
            uint top;
            if (audctl_ & (1 << AUDCTL::CH3_FAST_CLK))
                // channel 1.79 Mhz clock
                top = (audf[AUDIO_CHANNEL3]);
            else
                // regular audio clock
                top = (audf[AUDIO_CHANNEL3]) * 28;

            set_sound_output(AUDIO_CHANNEL3, top, audc[AUDIO_CHANNEL3] & 0x0F);

            return;
        }
    }

    // single channel, assumes 64khz clock
    if ((audc[chan] & 0x0F) || (audf[chan] != 0)) {
        uint top;
        if ((audctl_ & (1 << AUDCTL::CH1_FAST_CLK)) && (chan == AUDIO_CHANNEL1))
            top = (1 + audf[chan]);
        else if ((audctl_ & (1 << AUDCTL::CH3_FAST_CLK)) &&
                 (chan == AUDIO_CHANNEL3))
            top = (1 + audf[chan]);
        else
            top = (1 + audf[chan]) * 28;

        set_sound_output(chan, top, audc[chan] & 0x0F);
    } else {
        stop_sound(chan);
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