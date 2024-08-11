#include "console.h"
#include "pico/stdlib.h"

#include <cstring>

#define CHAR_COLS 40
#define CHAR_ROWS 30
extern char charbuf[CHAR_ROWS * CHAR_COLS];
uint8_t cursor_x = 0;
uint8_t cursor_y = 0;

uint8_t cursor_blink_on = false;

void __not_in_flash_func(blink_cursor)(uint32_t frames) {
  if (frames % 10 == 0) {
    if (cursor_blink_on) {
      cursor_blink_on = false;
    } else {
      cursor_blink_on = true;
    }
  }
}

void __not_in_flash_func(display_cursor)(uint8_t line) {}

void __not_in_flash_func(video_putchar)(uint8_t data) {
  if (data > 31) {
    charbuf[cursor_y * CHAR_COLS + cursor_x] = data;
    cursor_x++;
  } else {
    // special character handling
    if (data == 8) { // backspace
      charbuf[cursor_y * CHAR_COLS + cursor_x] = ' ';
      if (cursor_x > 0)
        cursor_x--;
    } else if ((data == '\r') || (data == '\n')) {
      // remove cursor

      cursor_y++;
      cursor_x = 0;
    }
  }
  if (cursor_x >= CHAR_COLS) {

    cursor_y++;
    cursor_x = 0;
  }

  if (cursor_y >= CHAR_ROWS) {
    // copy all characters one line up
    memmove(charbuf, charbuf + CHAR_COLS, CHAR_COLS * (CHAR_ROWS - 1));

    // clear bottom line
    memset(charbuf + CHAR_COLS * (CHAR_ROWS - 1), 32,
           CHAR_COLS); // 32 == space

    cursor_y--;
  }
}

void video_cls() {
  cursor_x = 0;
  cursor_y = 0;

  memset(charbuf, 32, CHAR_COLS * CHAR_ROWS); // fill with 32 (space)
}
