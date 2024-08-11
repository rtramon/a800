#ifndef _CONSOLE_H
#define _CONSOLE_H
#include <stdint.h>

void cursor_on();
void cursor_off();

void blink_cursor(uint32_t frames);
void display_cursor(uint8_t line);
void video_putchar(uint8_t data);
void video_cls();

#endif
