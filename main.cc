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

static inline void __dvi_func_x(my_dvi_prepare_scanline_16bpp)(
    struct dvi_inst* inst, uint32_t* scanbuf) {
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
static uint16_t __aligned(4) scanbuf[FRAME_WIDTH + 16];
// int lines;

void mode_2_prepare_tmdsbuf(uint32_t*);

void __dvi_func_x(core1_main)() {
    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);

    sem_acquire_blocking(&dvi_start_sem);
    printf("core1: start dvi\n");

    // acquired semaphore, start dvi
    dvi_start(&dvi0);

    uint32_t start = timer_hw->timerawl;
    while (true) {
        // display list starts at line 8 and ends no later than scanline 248
        for (int lines = 0; lines < FRAME_HEIGHT; lines++) {
            antic_render_scanline(scanbuf, lines);

#if !defined(OPTION_TMDS)
            my_dvi_prepare_scanline_16bpp(&dvi0, (uint32_t*)&scanbuf);
#endif
#if defined(OPTION_TMDS)
            uint32_t* tmdsbuf;
            constexpr uint pixwidth = 640;
            constexpr uint words_per_channel = pixwidth / DVI_SYMBOLS_PER_WORD;
            queue_remove_blocking_u32(&dvi0.q_tmds_free, &tmdsbuf);

            // uint32_t line_ts = timer_hw->timerawl;
            mode_2_prepare_tmdsbuf(tmdsbuf);
            // printf("l: %d t: %d\n", lines, timer_hw->timerawl - line_ts);

            queue_add_blocking_u32(&dvi0.q_tmds_valid, &tmdsbuf);
#endif
            antic_dl_end(lines);
            // ensure some time for 6502 DLI routine to execute
            // before the next scanline is drawn
            // 240810: delay > 10 causes redlines in pacman
            uint32_t line_ts = timer_hw->timerawl;
            while (timer_hw->timerawl - line_ts < 8);
        }

        // puts("vbi");
        uint32_t vbi_ts = timer_hw->timerawl;
        // handle once per frame /vsync stuff
        antic_dl_start(240);
        antic_dl_end(240);

        antic_gen_vbi();

        // tinyusb host task
        if (check_tuh) tuh_task();

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

                    // extern uint read_cnt, write_cnt;
                    // printf("reads: %d, writes: %d read%: %d\n", read_cnt,
                    //        write_cnt,
                    //        (100 * read_cnt) / (read_cnt + write_cnt));
                    // read_cnt = 0;
                    // write_cnt = 0;

#endif
                    start = timer_hw->timerawl;
                    break;

                default:
                    // pass key to pokey
                    pokey_keyb_event(key);

                    break;
            }
        }

        // ensure minimum vblank period
        while (timer_hw->timerawl - vbi_ts < 200);
        // printf("vbi: %d\n", timer_hw->timerawl - vbi_ts);
    }
}

int main() {
    // vreg_set_voltage(VREG_VSEL);
    // sleep_ms(10);

    set_sys_clock_khz(DVI_TIMING.bit_clk_khz, true);

    board_init();

    stdio_init_all();

    puts("\n\nNEO6502 Memory Emulator v0.02");
    printf("CPU Freq: %d Mhz\n", DVI_TIMING.bit_clk_khz / 1000);

    tuh_init(BOARD_TUH_RHPORT);

    // init DMA supported memcopy
    memcpy_dma_init();

    for (int i = 0; i < 150; i++) {
        tuh_task();
        busy_wait_ms(10);
    }

    init_sound(20);
    play_sound(0, 440);
    sleep_ms(200);
    stop_sound(0);
    play_sound(1, 380);
    sleep_ms(200);
    stop_sound(1);
    play_sound(2, 340);
    sleep_ms(200);
    stop_sound(2);
    play_sound(3, 300);
    sleep_ms(200);
    stop_sound(3);

    // Reset Atari & 6502 core
    system_init();

    // initialize dvipico
    dvi0.timing = &DVI_TIMING;
    dvi0.ser_cfg = pico_neo6502_cfg;
    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());

    // start service core
    puts("Starting Display Core 1");
    sem_init(&dvi_start_sem, 0, 1);
    hw_set_bits(&bus_ctrl_hw->priority, BUSCTRL_BUS_PRIORITY_PROC1_BITS);
    multicore_launch_core1(core1_main);

    // removing the following lines affects the correct generation
    // of the dvi output
    // not sure why
    // for (int i = 0; i < 100; i++) {
    //     tuh_task();
    //     busy_wait_ms(10);
    // }

    sem_release(&dvi_start_sem);

    // puts("Starting 6502");
    while (1) {
        for (int i = 0; i < 4; i++) stop_sound(i);
        menu();
        play_sound(0, 400);
        sleep_ms(100);
        //  play start beep and let usb discover devices (if any)
        // init_sound(20);
        // play_sound(20, 400);
        // for (int i = 0; i < 20; i++) {
        //     tuh_task();
        //     busy_wait_ms(10);
        // }
        // stop_sound(20);

        // ensure core1 check usb host task
        check_tuh = true;

        system_reset();
        system_run();

        check_tuh = false;
    }
}
