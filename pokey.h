#ifndef _POKEY_H__
#define _POKEY_H__
#include <stdint.h>

enum AUDCTL {
    CLK_SELECT = 0,
    CH2_HPF = 1,
    CH1_LFP = 2,
    CH34 = 3,
    CH12 = 4,
    CH3_FAST_CLK = 5,
    CH1_FAST_CLK = 6,
    PLY = 7
};

enum SKSTAT {
    KEY = 2,
    SHIFT = 3,
    SERIAL_INP = 4,
    SERIAL_INP_OVR = 5,
    KEYB_OVR = 6,
    SERIAL_INP_FRM_ERR = 7
};

enum SKCTL {
    KEYBD_DEBOUND = 1,
    KEYBD_SCAN = 2,
    FAST_POT_SCAN = 3,
    TWOTONE = 4,
    ASYNC = 5,
    FORCEBREAK = 7
};

enum KBCODE {
    SHIFT_KEY = 6,
    CNTRL_KEY = 7,
};

enum IRQST {
    TIMER1 = 0,
    TIMER2 = 1,
    TIMER4 = 2,
    SERIAL_TX_COMPLETE = 3,
    SERIAL_OUT_READY = 4,
    SERIAL_IN_READY = 5,
    KEYBD = 6,
    BREAK = 7
};

extern uint8_t audf[4];
extern uint8_t audc[4];
extern uint8_t audctl_;

void pokey_reset();
uint8_t pokey_read(uint8_t reg);
void pokey_write(uint8_t reg, uint8_t data);
void pokey_tick();

void pokey_report_keycode(uint8_t code);
void pokey_keyb_event(uint8_t code);
void pokey_report_break();

extern volatile uint8_t irqst_, irqen_, skstat_, kbcode_;

#endif
