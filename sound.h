#ifndef __SOUND_H_
#define __SOUND_H_

#include <cstdint>

extern void init_sound(uint8_t gpio);
extern void play_sound(uint8_t gpio, uint32_t freq);
extern void stop_sound(uint8_t gpio);

extern void play_pokey_sound(uint8_t gpio, uint8_t pokey_freq);

#endif