#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

// typedef uint16_t u16;
// typedef uint32_t u32;

const uint16_t duty = 50;  // duty cycle, in percent
static uint slice_num;
static uint channel;
// #define MAX_PWM_FREQ 1'000'000UL
#define MAX_PWM_FREQ 2000'000UL

void init_sound(uint8_t gpio) {
    gpio_set_function(gpio,
                      GPIO_FUNC_PWM);  // Tell GPIO 0 it is allocated to the PWM
    slice_num =
        pwm_gpio_to_slice_num(gpio);  // get PWM slice for GPIO 0 (it's slice 0)
    channel = pwm_gpio_to_channel(gpio);

    // set frequency
    // determine top given Hz - assumes free-running counter rather than
    // phase-correct
    uint32_t f_sys = clock_get_hz(clk_sys);  // typically 125'000'000 Hz
    float divider =
        f_sys /
        MAX_PWM_FREQ;  // let's arbitrarily choose to run pwm clock at 1MHz

    pwm_set_clkdiv(slice_num,
                   divider);  // pwm clock should now be running at 1MHz
}

void play_sound(uint8_t gpio, uint32_t freq) {
    uint32_t top = MAX_PWM_FREQ / freq -
                   1;  // TOP is u16 has a max of 65535, being 65536 cycles
    pwm_set_wrap(slice_num, top);

    // set duty cycle
    uint16_t level = (top + 1) * duty / 100 -
                     1;  // calculate channel level from given duty cycle in %
    pwm_set_chan_level(slice_num, channel, level);

    pwm_set_enabled(slice_num, true);  // let's go!
}

void stop_sound(uint8_t gpio) { pwm_set_enabled(slice_num, false); }

void play_pokey_sound(uint8_t gpio, uint8_t pokey_freq) {
    // translate pokey frequency to audio frequency
    // assume pokey audio uses 64KHz slow clock for audio
    // audctl d0 == 0
    play_sound(gpio, 64000 / pokey_freq);
}
