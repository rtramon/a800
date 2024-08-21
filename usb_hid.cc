#include "antic.h"
#include "bsp/board_api.h"
#include "gtia.h"
#include "m6520.h"
#include "pokey.h"
#include "tusb.h"
#include "usb_hid_keys.h"
#ifdef EMU_M6502
#include "emu_m6502.h"
#else
#include "mos65C02.h"
#endif

// externals
extern bool GLreset;

#define MAX_REPORT 4

// Each HID instance can has multiple reports
static struct {
    uint8_t report_count;
    tuh_hid_report_info_t report_info[MAX_REPORT];
} hid_info[CFG_TUH_HID];

// function declaration
void process_kbd_report(hid_keyboard_report_t const* report);
void process_generic_report(uint8_t dev_addr, uint8_t instance,
                            uint8_t const* report, uint16_t len);
void process_gamepad_report(hid_gamepad_report_t const* report);

static uint8_t conv_table[128] = {
    0,  // KEY_NONE 0x00 // No key pressed
    0,  // KEY_ERR_OVF 0x01 //  Keyboard Error Roll Over - used for all slots if
        // too many keys are
        //  pressed ("Phantom key")
    0,            // 0x02 //  Keyboard POST Fail
    0,            // 0x03 //  Keyboard Error Undefined
    0x3F,         // KEY_A 0x04 // Keyboard a and A
    0x15,         // KEY_B 0x05 // Keyboard b and B
    0x12,         // KEY_C 0x06 // Keyboard c and C
    0x3A,         // KEY_D 0x07 // Keyboard d and D
    0x2A,         // KEY_E 0x08 // Keyboard e and E
    0x38,         // KEY_F 0x09 // Keyboard f and F
    0x3D,         // KEY_G 0x0a // Keyboard g and G
    0x39,         // KEY_H 0x0b // Keyboard h and H
    0x0D,         // KEY_I 0x0c // Keyboard i and I
    0x01,         // KEY_J 0x0d // Keyboard j and J
    0x05,         // KEY_K 0x0e // Keyboard k and K
    0x00,         // KEY_L 0x0f // Keyboard l and L
    0x25,         // KEY_M 0x10 // Keyboard m and M
    0x23,         // KEY_N 0x11 // Keyboard n and N
    0x08,         // KEY_O 0x12 // Keyboard o and O
    0x0A,         // KEY_P 0x13 // Keyboard p and P
    0x2F,         // KEY_Q 0x14 // Keyboard q and Q
    0x28,         // KEY_R 0x15 // Keyboard r and R
    0x3E,         // KEY_S 0x16 // Keyboard s and S
    0x2D,         // KEY_T 0x17 // Keyboard t and T
    0x0B,         // KEY_U 0x18 // Keyboard u and U
    0x10,         // KEY_V 0x19 // Keyboard v and V
    0x2E,         // KEY_W 0x1a // Keyboard w and W
    0x16,         // KEY_X 0x1b // Keyboard x and X
    0x2B,         // KEY_Y 0x1c // Keyboard y and Y
    0x17,         // KEY_Z 0x1d // Keyboard z and Z
    0x1F,         // KEY_1 0x1e // Keyboard 1 and !
    0x1E,         // KEY_2 0x1f // Keyboard 2 and @
    0x1A,         // KEY_3 0x20 // Keyboard 3 and #
    0x18,         // KEY_4 0x21 // Keyboard 4 and $
    0x1D,         // KEY_5 0x22 // Keyboard 5 and %
    0x1B,         // KEY_6 0x23 // Keyboard 6 and ^
    0x33,         // KEY_7 0x24 // Keyboard 7 and &
    0x35,         // KEY_8 0x25 // Keyboard 8 and *
    0x30,         // KEY_9 0x26 // Keyboard 9 and (
    0x32,         // KEY_0 0x27 // Keyboard 0 and )
    0x0C,         // KEY_ENTER 0x28      // Keyboard Return (ENTER)
    0x1C,         // KEY_ESC 0x29        // Keyboard ESCAPE
    0x34,         // KEY_BACKSPACE 0x2a  // Keyboard DELETE (Backspace)
    0x2C,         // KEY_TAB 0x2b        // Keyboard Tab
    0x21,         // KEY_SPACE 0x2c      // Keyboard Spacebar
    0x0E,         // KEY_MINUS 0x2d      // Keyboard - and _
    0x0F,         // KEY_EQUAL 0x2e      // Keyboard = and +
    0x20 + 0x40,  // KEY_LEFTBRACE 0x2f  // Keyboard [ and {
    0x22 + 0x40,  // KEY_RIGHTBRACE 0x30 // Keyboard ] and }
    0x06 + 0x40,  // KEY_BACKSLASH 0x31  // Keyboard \ and |
    0x1A + 0x40,  // KEY_HASHTILDE 0x32  // Keyboard Non-US # and ~
    0x02,         // KEY_SEMICOLON 0x33  // Keyboard ; and :
    0x33 + 0x40,  // KEY_APOSTROPHE 0x34 // Keyboard ' and "
    0,            // KEY_GRAVE 0x35      // Keyboard ` and ~
    0x20,         // KEY_COMMA 0x36      // Keyboard , and <
    0x22,         // KEY_DOT 0x37        // Keyboard . and >
    0x26,         // KEY_SLASH 0x38      // Keyboard / and ?
    0x3C,         // KEY_CAPSLOCK 0x39   // Keyboard Caps Lock
    0,            // KEY_F1 0x3a  // Keyboard F1
    0,            // KEY_F2 0x3b  // Keyboard F2
    0,            // KEY_F3 0x3c  // Keyboard F3
    0,            // KEY_F4 0x3d  // Keyboard F4
    0,            // KEY_F5 0x3e  // Keyboard F5
    0,            // KEY_F6 0x3f  // Keyboard F6
    0,            // KEY_F7 0x40  // Keyboard F7
    0,            // KEY_F8 0x41  // Keyboard F8
    0,            // KEY_F9 0x42  // Keyboard F9
    0,            // KEY_F10 0x43 // Keyboard F10
    0,            // KEY_F11 0x44 // Keyboard F11
    0,            // KEY_F12 0x45 // Keyboard F12
};

static uint8_t shift_conv_table[128] = {
    0,  // KEY_NONE 0x00 // No key pressed
    0,  // KEY_ERR_OVF 0x01 //  Keyboard Error Roll Over - used for all slots if
        // too many keys are
        //  pressed ("Phantom key")
    0,            // 0x02 //  Keyboard POST Fail
    0,            // 0x03 //  Keyboard Error Undefined
    0x3F + 0x40,  // KEY_A 0x04 // Keyboard a and A
    0x15 + 0x40,  // KEY_B 0x05 // Keyboard b and B
    0x12 + 0x40,  // KEY_C 0x06 // Keyboard c and C
    0x3A,         // KEY_D 0x07 // Keyboard d and D
    0x2A,         // KEY_E 0x08 // Keyboard e and E
    0x38,         // KEY_F 0x09 // Keyboard f and F
    0x3D,         // KEY_G 0x0a // Keyboard g and G
    0x39,         // KEY_H 0x0b // Keyboard h and H
    0x0D,         // KEY_I 0x0c // Keyboard i and I
    0x01,         // KEY_J 0x0d // Keyboard j and J
    0x05,         // KEY_K 0x0e // Keyboard k and K
    0x00,         // KEY_L 0x0f // Keyboard l and L
    0x25,         // KEY_M 0x10 // Keyboard m and M
    0x23,         // KEY_N 0x11 // Keyboard n and N
    0x08,         // KEY_O 0x12 // Keyboard o and O
    0x0A,         // KEY_P 0x13 // Keyboard p and P
    0x2F,         // KEY_Q 0x14 // Keyboard q and Q
    0x28,         // KEY_R 0x15 // Keyboard r and R
    0x3E,         // KEY_S 0x16 // Keyboard s and S
    0x2D,         // KEY_T 0x17 // Keyboard t and T
    0x0B,         // KEY_U 0x18 // Keyboard u and U
    0x10,         // KEY_V 0x19 // Keyboard v and V
    0x2E,         // KEY_W 0x1a // Keyboard w and W
    0x16,         // KEY_X 0x1b // Keyboard x and X
    0x2B,         // KEY_Y 0x1c // Keyboard y and Y
    0x17,         // KEY_Z 0x1d // Keyboard z and Z
    0x1F + 0x40,  // KEY_1 0x1e // Keyboard 1 and !
    0x35 + 0x40,  // KEY_2 0x1f // Keyboard 2 and @
    0x1A + 0x40,  // KEY_3 0x20 // Keyboard 3 and #
    0x18 + 0x40,  // KEY_4 0x21 // Keyboard 4 and $
    0x1D + 0x40,  // KEY_5 0x22 // Keyboard 5 and %
    0x07 + 0x40,  // KEY_6 0x23 // Keyboard 6 and ^
    0x1b + 0x40,  // KEY_7 0x24 // Keyboard 7 and &
    0x07,         // KEY_8 0x25 // Keyboard 8 and *
    0x30 + 0x40,  // KEY_9 0x26 // Keyboard 9 and (
    0x32 + 0x40,  // KEY_0 0x27 // Keyboard 0 and )
    0x0C,         // KEY_ENTER 0x28      // Keyboard Return (ENTER)
    0x1C,         // KEY_ESC 0x29        // Keyboard ESCAPE
    0x34,         // KEY_BACKSPACE 0x2a  // Keyboard DELETE (Backspace)
    0x2C,         // KEY_TAB 0x2b        // Keyboard Tab
    0x21,         // KEY_SPACE 0x2c      // Keyboard Spacebar
    0x0E + 0x40,  // KEY_MINUS 0x2d      // Keyboard - and _
    0x06,         // KEY_EQUAL 0x2e      // Keyboard = and +
    0,            // KEY_LEFTBRACE 0x2f  // Keyboard [ and {
    0,            // KEY_RIGHTBRACE 0x30 // Keyboard ] and }
    0x0F + 0x40,  // KEY_BACKSLASH 0x31  // Keyboard \ and |
    0x1A + 0x40,  // KEY_HASHTILDE 0x32  // Keyboard Non-US # and ~
    0x02 + 0x40,  // KEY_SEMICOLON 0x33  // Keyboard ; and :
    0x1E + 0x40,  // KEY_APOSTROPHE 0x34 // Keyboard ' and "
    0,            // KEY_GRAVE 0x35      // Keyboard ` and ~
    0x36,         // KEY_COMMA 0x36      // Keyboard , and <
    0x37,         // KEY_DOT 0x37        // Keyboard . and >
    0x26 + 0x40,  // KEY_SLASH 0x38      // Keyboard / and ?
    0x3C,         // KEY_CAPSLOCK 0x39   // Keyboard Caps Lock
    0,            // KEY_F1 0x3a  // Keyboard F1
    0,            // KEY_F2 0x3b  // Keyboard F2
    0,            // KEY_F3 0x3c  // Keyboard F3
    0,            // KEY_F4 0x3d  // Keyboard F4
    0,            // KEY_F5 0x3e  // Keyboard F5
    0,            // KEY_F6 0x3f  // Keyboard F6
    0,            // KEY_F7 0x40  // Keyboard F7
    0,            // KEY_F8 0x41  // Keyboard F8
    0,            // KEY_F9 0x42  // Keyboard F9
    0,            // KEY_F10 0x43 // Keyboard F10
    0,            // KEY_F11 0x44 // Keyboard F11
    0,            // KEY_F12 0x45 // Keyboard F12
};
//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

void tuh_mount_cb(uint8_t dev_addr) {
    // application set-up
    // printf("A device with address %d is mounted\r\n", dev_addr);
}

void tuh_umount_cb(uint8_t dev_addr) {
    // application tear-down
    // printf("A device with address %d is unmounted \r\n", dev_addr);
}

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use.
// tuh_hid_parse_report_descriptor() can be used to parse common/simple enough
// descriptor. Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE,
// it will be skipped therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                      uint8_t const* desc_report, uint16_t desc_len) {
    printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr,
           instance);

    // Interface protocol (hid_interface_protocol_enum_t)
    const char* protocol_str[] = {"None", "Keyboard", "Mouse"};
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    // printf("HID Interface Protocol = %s\r\n", protocol_str[itf_protocol]);

    // By default host stack will use activate boot protocol on supported
    // interface. Therefore for this simple example, we only need to parse
    // generic report descriptor (with built-in parser)
    if (itf_protocol == HID_ITF_PROTOCOL_NONE) {
        hid_info[instance].report_count = tuh_hid_parse_report_descriptor(
            hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
        // printf("HID has %u reports \r\n", hid_info[instance].report_count);
    }

    // request to receive report
    // tuh_hid_report_received_cb() will be invoked when report is available
    if (!tuh_hid_receive_report(dev_addr, instance)) {
        printf("Error: cannot request to receive report\r\n");
    }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr,
           instance);
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                uint8_t const* report, uint16_t len) {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    switch (itf_protocol) {
        case HID_ITF_PROTOCOL_KEYBOARD:
            TU_LOG2("HID receive boot keyboard report\r\n");
            // printf("HID receive boot keyboard report\r\n");
            process_kbd_report((hid_keyboard_report_t const*)report);
            break;

        case HID_ITF_PROTOCOL_MOUSE:
            TU_LOG2("HID receive boot mouse report\r\n");
            // process_mouse_report((hid_mouse_report_t const *)report);
            break;

        default:
            // Generic report requires matching ReportID and contents with
            // previous parsed report info printf("dev: %d instance: %d \n",
            // dev_addr, instance);
            process_generic_report(dev_addr, instance, report, len);
            break;
    }

    // continue to request to receive report
    if (!tuh_hid_receive_report(dev_addr, instance)) {
        printf("Error: cannot request to receive report\r\n");
    }
}

static inline bool is_key_pressed(hid_keyboard_report_t const* report) {
    for (int i = 0; i < 6; i++)
        if (report->keycode[i] != 0) return true;
    return false;
}

static inline bool is_key_held(hid_keyboard_report_t const* report,
                               uint8_t keycode) {
    for (uint8_t i = 0; i < 6; i++) {
        if (report->keycode[i] == keycode) return true;
    }
    return false;
}

void process_kbd_report(hid_keyboard_report_t const* report) {
    static hid_keyboard_report_t prev_report = {0, 0, {0}};

    if (!is_key_pressed(report)) {
        // reset SKSTAT key pressed status bits
        skstat_ |= ((1 << 3) | (1 << 2));
        kbcode_ &= ~((1 << 7) | (1 << 6));
    }

    for (uint8_t i = 0; i < 6; i++) {
        if (report->keycode[i]) {
            if (!is_key_held(&prev_report, report->keycode[i])) {
                bool const is_shift_pressed =
                    report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT |
                                        KEYBOARD_MODIFIER_RIGHTSHIFT);

                bool const is_ctrl_pressed =
                    report->modifier &
                    (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL);

                // no use for alt modifier, but keep around just in case
                bool const is_alt_pressed =
                    report->modifier &
                    (KEYBOARD_MODIFIER_LEFTALT | KEYBOARD_MODIFIER_RIGHTALT);

                // handle special atari xl keys
                if (is_ctrl_pressed) {
                    // printf("usb kbcode: %02X\n", report->keycode[i]);

                    switch (report->keycode[i]) {
                        case KEY_DELETE:
                            if (is_alt_pressed)
                                GLreset = true;
                            else
                                pokey_report_break();
                            break;
                        case KEY_F1:
                            gtia_set_consol(CONSOL_START);
                            break;
                        case KEY_F2:
                            gtia_set_consol(CONSOL_SELECT);
                            break;
                        case KEY_F3:
                            gtia_set_consol(CONSOL_OPTION);
                            break;
                    }
                } else {
                    // handle regular keyboards presses
                    int kbcode;
                    skstat_ &= ~(1 << 2);

                    if (is_shift_pressed) {
                        kbcode = shift_conv_table[report->keycode[i]];
                        // set bit 6 to indicate shift is pressed
                        // kbcode |= (1 << 6);
                    } else
                        kbcode = conv_table[report->keycode[i]];

                    // if atari kbcode is accessed using shift, indicate shift
                    // is pressed
                    if (kbcode & (1 << 6)) skstat_ &= ~(1 << 3);

                    pokey_report_keycode(kbcode);
                }
            }
        }
    }
    prev_report = *report;
}

void process_generic_report(uint8_t dev_addr, uint8_t instance,
                            uint8_t const* report, uint16_t len) {
    (void)dev_addr;
    (void)instance;

    // Dump report
    // for (int i = 0; i < len; i++) printf("%02X ", report[i]);
    // printf("\n");

    if (report[0] == 1) {
        // reset all event input to off
        trig0_ = 1;
        consol_ = 0b00000111;
        regPORTA = 0xFF;

        // set reported event input to active
        switch (report[6]) {
            case 0x01:  // left fire
            case 0x02:  // right fire
                trig0_ = 0;
                break;
            case 0x20:           // START
                consol_ = 0x06;  // 0b00000110;
                break;

            case 0x10:           // SELECT
                consol_ = 0x05;  // 0b00000101;
                break;
        }

        switch (report[5]) {
            case 0x1F:  // X
                break;

            case 0x8F:                 // Y
                consol_ = 0b00000011;  // used for OPTION
                break;

            case 0x2F:  // A
                GLreset = true;
                break;

            case 0x4F:  // B
                        // used for break key
                if (irqen_ & (1 << 7)) {
                    irqst_ &= (~(1 << 7));
                    // printf("BRKKEY IRQST $%02x\n", irqst_);
#ifdef EMU_M6502
                    cpu.irq = 1;
#else
                    mos65c02_irq_on();
#endif
                }
                break;
        }

        if (report[3] == 0)  // LEFT
            regPORTA &= ~(1 << 2);
        else if (report[3] == 0xFF)  // RIGHT
            regPORTA &= ~(1 << 3);
        if (report[4] == 0)  // UP
            regPORTA &= ~(1 << 0);
        else if (report[4] == 0xFF)
            regPORTA &= ~(1 << 1);
    }
}

#if 0
//
// https://forums.raspberrypi.com/viewtopic.php?t=339681&hilit=robot
void process_generic_report(uint8_t dev_addr, uint8_t instance,
                            uint8_t const *report, uint16_t len) {
  (void)dev_addr;

  uint8_t const rpt_count = hid_info[instance].report_count;
  tuh_hid_report_info_t *rpt_info_arr = hid_info[instance].report_info;
  tuh_hid_report_info_t *rpt_info = NULL;
  int loop;

  for (int i = 0; i < len; i++)
    printf("%02X ", report[i]);
  printf("\n");
  if (rpt_info_arr->report_id == 3) {
    process_gamepad_report((const hid_gamepad_report_t *)report);
    return;
  }
}
void process_gamepad_report(hid_gamepad_report_t const *report) {
  int sizeR = sizeof(hid_gamepad_report_t);
  int loop;
  uint8_t *pt;

  pt = (uint8_t *)report;

  for (loop = 0; loop < sizeR; loop++)
    printf("%02X ", (int)pt[loop]);
  printf("\n");
  // #my gamepad doesnt have z axis so hat is on ry
  printf("hat:0x%02X\n", report->ry);
}

static void process_generic_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) dev_addr;

  uint8_t const rpt_count = hid_info[instance].report_count;
  tuh_hid_report_info_t* rpt_info_arr = hid_info[instance].report_info;
  tuh_hid_report_info_t* rpt_info = NULL;
  int loop;

  if(rpt_info_arr->report_id==3)
     {
       process_gamepad_report(report);
       return;
     }

#endif