#include <ctype.h>
#include <stdio.h>
#include <tusb.h>

#include <cstring>
#include <queue>

#include "antic.h"
#include "bsp/board.h"
#include "colortable.h"
#include "console.h"
#include "gtia.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "hardware/vreg.h"
#include "m6821.h"
#include "memory.h"
#include "menu.h"
#include "pico/binary_info.h"
#include "pico/mem_ops.h"
#include "pico/multicore.h"
#include "pico/stdio_uart.h"
#include "pico/stdlib.h"
#include "pokey.h"
#include "system.h"
#include "tusb_config.h"

#ifdef EMU_M6502
#include "emu_m6502.h"
extern M6502 cpu;
#else
#include "mos65C02.h"
#endif

#include "memcpy.h"
#include "sound.h"

// PicoDVI definitions
#include "PicoDVI/software/include/common_dvi_pin_configs.h"
#include "dvi.h"
extern "C" {
#include "dvi_serialiser.h"
#include "tmds_encode.h"
}

// DVDD 1.2V (1.1V seems ok too)
#define FRAME_WIDTH 320
#if DVI_VERTICAL_REPEAT == 2
#define FRAME_HEIGHT 240
#else
#define FRAME_HEIGHT (480)
#endif
#define VREG_VSEL VREG_VOLTAGE_1_20
#define DVI_TIMING dvi_timing_640x480p_60hz

// GLOBALS
struct dvi_inst dvi0;
semaphore_t dvi_start_sem;
bool check_tuh = false;

// Support functions
constexpr uint16_t RGB565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r >> 3) << 11) | ((g >> 2) << 5) | ((b >> 3));
}

#define __dvi_func_x(f) __scratch_x(__STRING(f)) f
#define __dvi_func_y(f) __scratch_y(__STRING(f)) f
void (*render_scanline)(uint16_t*, uint16_t);

//
// functions
//

static __always_inline void my_dvi_prepare_scanline_16bpp(struct dvi_inst* inst,
                                                          uint32_t* scanbuf) {
    uint32_t* tmdsbuf;
    constexpr uint pixwidth = 640;
    constexpr uint words_per_channel = pixwidth / DVI_SYMBOLS_PER_WORD;
    queue_remove_blocking_u32(&inst->q_tmds_free, &tmdsbuf);

    tmds_encode_data_channel_16bpp(
        (const uint32_t*)scanbuf, tmdsbuf + 0 * words_per_channel, pixwidth / 2,
        DVI_16BPP_BLUE_MSB, DVI_16BPP_BLUE_LSB);
    tmds_encode_data_channel_16bpp(
        (const uint32_t*)scanbuf, tmdsbuf + 1 * words_per_channel, pixwidth / 2,
        DVI_16BPP_GREEN_MSB, DVI_16BPP_GREEN_LSB);
    tmds_encode_data_channel_16bpp(
        (const uint32_t*)scanbuf, tmdsbuf + 2 * words_per_channel, pixwidth / 2,
        DVI_16BPP_RED_MSB, DVI_16BPP_RED_LSB);

    queue_add_blocking_u32(&inst->q_tmds_valid, &tmdsbuf);
}

// buffer to hold 320 16bit color pixels representing one scanline
// note: buffer is 16 pixels larger to cope with writing past the end
// instead of checking (optimization)
static uint16_t __aligned(4) __scratch_x("scanbuf") scanbuf[FRAME_WIDTH + 16];

void __dvi_func_x(core1_main)() {
    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);

    sem_acquire_blocking(&dvi_start_sem);
    printf("core1: start dvi\n");

    // acquired semaphore, start dvi
    dvi_start(&dvi0);

    uint32_t start = timer_hw->timerawl;
    while (true) {
        // first 8 lines are not visible, but execute instruction and are
        // counted in vcount
        uint32_t vbi_ts = timer_hw->timerawl;

        for (int lines = 0; lines < 8; lines++) {
            update_vcount(lines);
            system_add_cputicks(105);
            system_wait_cputicks(105);
        }

        // display list starts at line 8 and ends no later than scanline 248
        for (int lines = 8; lines < FRAME_HEIGHT + 8; lines++) {
            antic_render_scanline(scanbuf, lines);
            my_dvi_prepare_scanline_16bpp(&dvi0, (uint32_t*)&scanbuf);
            antic_dl_end(lines);
        }

        // puts("vbi");
        // handle once per frame /vsync stuff

        // antic_dl_start(248);
        // antic_dl_end(248);

        antic_gen_vbi();

        //  run the system for 22 more lines (ntsc) or 72 more lines in pal
        for (int i = 248; i < 262; i++) {
            update_vcount(i);
            // system_wait_cputicks(90);
            system_add_cputicks(105);
            system_wait_cputicks(105);
        }

        // prevent 6502 from running to many instructions
        system_set_cputicks(0);

        // tinyusb host task
        tuh_task();

        // blink_cursor(frames);
        int key = getchar_timeout_us(0);
        if (key > 0) {
            // check for emulator commands
            switch (key & 0xff) {
                // case 0x12: // control-R
                //   GLreset = true;
                //   break;
                // case 0x13: // control-S
                //   break;
                case 0x14:  // control-T
#ifdef EMU_M6502
                    printf("Freq: %1.3f Mhz\n",
                           (float)cpu.cyc / (timer_hw->timerawl - start));
                    cpu.cyc = 0;

#else
                    printf("Freq: %1.3f Mhz\n",
                           (float)ticks / (timer_hw->timerawl - start));
                    ticks = 0;

#endif
                    start = timer_hw->timerawl;
                    break;

                default:
                    // pass key to pokey
                    pokey_keyb_event(key);

                    break;
            }
        }

        // ensure minimum frame time
        // 640x480 at 60Hz is approx 16.68 ms.
        while (timer_hw->timerawl - vbi_ts < 16000);
    }
}

int main() {
    vreg_set_voltage(VREG_VSEL);
    sleep_ms(10);

    set_sys_clock_khz(DVI_TIMING.bit_clk_khz, true);

    board_init();

    stdio_init_all();

    puts("\n\nNEO6502 Memory Emulator v0.3");
    printf("CPU Freq: %d Mhz\n", DVI_TIMING.bit_clk_khz / 1000);

    tuh_init(BOARD_TUH_RHPORT);

    // init DMA supported memcopy
    memcpy_dma_init();

    for (int i = 0; i < 1500; i++) {
        tuh_task();
        busy_wait_ms(1);
    }

    init_sound();

    // Reset Atari & 6502 core
    system_init();

    // initialize dvipico
    dvi0.timing = &DVI_TIMING;
    dvi0.ser_cfg = olimex_rp2040_cfg;  // old name: pico_neo6502_cfg;
    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());

    // start service core
    puts("Starting Display Core 1");
    sem_init(&dvi_start_sem, 0, 1);
    hw_set_bits(&bus_ctrl_hw->priority, BUSCTRL_BUS_PRIORITY_PROC1_BITS);
    multicore_launch_core1(core1_main);

    sem_release(&dvi_start_sem);

    puts("Starting Atari 6502 Emulation");
    while (1) {
        // show menu
        system_reset();
        menu();
        play_sound(0, 400);
        sleep_ms(100);
        stop_sound(0);

        // run the system
        system_reset();
        system_run();
    }
}
