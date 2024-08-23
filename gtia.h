#ifndef _GTIA_H
#define _GTIA_H

#include <stdint.h>

enum PRIOR {
    PF_PM_PRIORITY = 0x0F,
    FIFTH_PLAYER = 4,
    MULTICOLOR_PLAYER = 5,
    GTIA_MODE = 0x03,
};

void gtia_reset();
uint8_t gtia_read(uint8_t reg);
void gtia_write(uint8_t reg, uint8_t data);
void gtia_set_consol(uint8_t);

#define CONSOL_START 0b00000110
#define CONSOL_SELECT 0b00000101
#define CONSOL_OPTION 0b00000011

extern uint8_t prior_;
extern uint8_t reg_colbk_;
extern uint16_t colbk_;
extern uint16_t palette[9];
extern uint8_t trig0_, trig3_;
extern uint8_t consol_;
extern uint8_t gractl_;
extern uint8_t reg_colpf[4];
extern uint16_t colpf[4];
extern uint8_t reg_colpm_[4];
extern uint16_t colpm_[4];
extern uint8_t grafp_[4];
extern uint8_t grafm_;
extern uint8_t hposp_[4];
extern uint8_t sizep_[4];
extern uint8_t sizem_;
extern uint8_t hposm_[4];
extern uint8_t mxpl_[4];
extern uint8_t pxpf_[4];
extern uint8_t mxpf_[4];
extern uint8_t pxpl_[4];

#endif
