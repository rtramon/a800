

#ifndef _M6821_H
#define _M6821_H
#include "pico/stdlib.h"
#include <stdint.h>

#include "console.h"

////////////////////////////////////////////////////////////////////
// 6821 Peripheral
// emulate just enough so keyboard/display works thru serial port.
////////////////////////////////////////////////////////////////////
//

#define KBD 0x10
#define KBDCR 0x11
#define DSP 0x12
#define DSPCR 0x13

class M6821 {
public:
  M6821() {
    regKBD = 0x00;
    regKBDDIR = 0x00;
    regKBDCR = 0x00;
    regDSP = 0x00;
    regDSPDIR = 0x00;
    regDSPCR = 0x00;
  };

  uint8_t read(uint8_t reg) {
    uint8_t data;
    switch (reg) {
    case DSP:
      if (regDSPCR & 0x02) {
        data = regDSP;
        regDSPCR &= 0x7F;
      } else {
        data = regDSPDIR;
      }

      break;

    case DSPCR:
      data = regDSPCR;
      break;

    case KBD:
      if (regKBDCR & 0x02) {
        data = regKBD;
        regKBDCR &= 0x7F; // clear IRQA
      } else
        data = regKBDDIR;

      break;

    case KBDCR:
      data = regKBDCR;
      break;

    default:
      panic("M6821 unknown register");
      break;
    }
    return data;
  }

  void write(uint8_t reg, uint8_t data) {
    switch (reg) {
    case DSP:
      if (regDSPCR & 0x02) {
        // display output
        video_putchar(data & 0x7F);
      } else
        regDSPDIR = data;
      break;

    case DSPCR:
      regDSPCR = data;
      break;

    case KBD:
      if (regKBDCR & 0x02) {
        regKBD = data;
      } else {
        regKBDDIR = data;
      }
      break;

    case KBDCR:
      regKBDCR = data & 0x7F;
      break;
    }
  };

  void store_key(uint8_t key) {
    if (!(regKBDCR & 0x80)) {
      regKBD = key;     // apple 1 expect upper bit set on incoming
      regKBDCR |= 0x80; // set IRQA
    }
  };

private:
  uint8_t regKBD;
  uint8_t regKBDDIR; // Dir register when KBDCR.bit2 == 0
  uint8_t regKBDCR;
  uint8_t regDSP;
  uint8_t regDSPDIR; // Dir register when DSPCR.bit2 == 0
  uint8_t regDSPCR;

  uint8_t flagDSP; // indicates that regDSPhas been updated
};

#endif
