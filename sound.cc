#include "sound.h"

#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

// typedef uint16_t u16;
// typedef uint32_t u32;

const uint16_t duty = 50;  // duty cycle, in percent
// static uint slice_num;
// static uint channel;

// 16x 64kHz, which is pokey sound high frequency clock
// #define MAX_PWM_FREQ 1'024'000UL
#define MAX_PWM_FREQ 1024'000UL

uint32_t slice_enable_mask = 0;

void on_pwm_wrap() {
    // clear the interrupt
    uint32_t irq = pwm_get_irq_status_mask();

    // audio uses first 3 pwm slices
    for (int slice = 0; slice < 4; slice++) {
        if (irq & (1 << slice)) {
            pwm_clear_irq(slice);
            // toggle audio output pin
            gpio_xor_mask((1ul << 20));
        }
    }

    // if (irq & (1 << pwm_gpio_to_slice_num(AUDIO_CHANNEL0))) {
    //     pwm_clear_irq(pwm_gpio_to_slice_num(AUDIO_CHANNEL0));
    //     // toggle gpio
    //     gpio_xor_mask((1 << 20));
    // }
    // if (irq & (1 << pwm_gpio_to_slice_num(AUDIO_CHANNEL1))) {
    //     pwm_clear_irq(pwm_gpio_to_slice_num(AUDIO_CHANNEL1));
    //     // toggle gpio
    //     gpio_xor_mask((1 << 20));
    // }
    // if (irq & (1 << pwm_gpio_to_slice_num(AUDIO_CHANNEL2))) {
    //     pwm_clear_irq(pwm_gpio_to_slice_num(AUDIO_CHANNEL2));
    //     // toggle gpio
    //     gpio_xor_mask((1 << 20));
    // }
}

void init_pwm_slice(uint gpio) {
    // set frequency
    // determine top given Hz - assumes free-running counter rather than
    // phase-correct
    uint32_t f_sys = clock_get_hz(clk_sys);  // typically 125'000'000 Hz
    float divider = f_sys / MAX_PWM_FREQ;

    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_clear_irq(slice_num);
    pwm_set_irq_enabled(slice_num, false);
    irq_set_exclusive_handler(PWM_DEFAULT_IRQ_NUM(), on_pwm_wrap);
    irq_set_enabled(PWM_DEFAULT_IRQ_NUM(), true);
    pwm_set_clkdiv(slice_num, divider);
    // pwm_set_enabled(slice_num, false);
}

void init_sound(uint8_t gpio) {
    // gpio_set_function(gpio,
    //                   GPIO_FUNC_PWM);  // Tell GPIO 0 it is allocated to the
    //                   PWM

    // initialize GPIO20 as audio output
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, 0);

    init_pwm_slice(AUDIO_CHANNEL0);
    init_pwm_slice(AUDIO_CHANNEL1);
    init_pwm_slice(AUDIO_CHANNEL2);
    init_pwm_slice(AUDIO_CHANNEL4);

    // enable all pwm channels
    pwm_set_mask_enabled(0x0000000F);
}

void __not_in_flash_func(play_sound)(uint8_t gpio, uint32_t freq) {
    // TOP is u16 has a max of 65535, being 65536 cycles
    uint top = MAX_PWM_FREQ / (freq)-1;
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_set_wrap(slice_num, top);

    pwm_set_irq_enabled(slice_num, true);
    // pwm_set_enabled(slice_num, true);
    slice_enable_mask |= (1 << slice_num);
    // pwm_set_mask_enabled(slice_enable_mask);
}

void __not_in_flash_func(stop_sound)(uint8_t gpio) {
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_set_irq_enabled(slice_num, false);
    // pwm_set_enabled(slice_num, false);
    slice_enable_mask &= ~(1 << slice_num);
    // pwm_set_mask_enabled(slice_enable_mask);
}

void __not_in_flash_func(play_pokey_sound)(uint8_t gpio, uint8_t pokey_freq) {
    // translate pokey frequency to audio frequency
    // assume pokey audio uses 64KHz clock for audio
    if (pokey_freq)
        play_sound(gpio, 64000 / pokey_freq);
    else
        stop_sound(gpio);

    // uint slice_num = pwm_gpio_to_slice_num(gpio);
    // uint freq = pokey_freq * 16;
    // pwm_set_wrap(slice_num, freq);

    // pwm_set_enabled(slice_num, true);  // let's go!
}
