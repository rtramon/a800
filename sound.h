#ifndef __SOUND_H_
#define __SOUND_H_

#include <cstddef>
#include <cstdint>

#define AUDIO_CHANNEL1 0  // 16
#define AUDIO_CHANNEL2 1  // 18
#define AUDIO_CHANNEL3 2  // 20
#define AUDIO_CHANNEL4 3

extern void init_sound();
extern void play_sound(uint8_t chan, uint32_t freq, uint8_t vol = 15);
// extern void play_sound(uint8_t chan, uint32_t freq, uint32_t volume);
extern void play_sound_p(uint8_t chan, uint32_t h_period, uint32_t l_period);
extern void stop_sound(uint8_t chan);
extern void play_pokey_sound(uint8_t chan);

extern bool sound_enabled();
extern void enable_sound(bool);

#if defined(OPTION_SOUND_INTERRUPTS)
extern void set_timer_int(unsigned int chan, bool enabled);
#endif
extern void audctl_update(uint8_t audctl);

#endif