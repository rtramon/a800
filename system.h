#ifndef _SYSTEM_H_
#define _SYSTEM_H_
#include <stdint.h>
#include <sys/types.h>

extern volatile uint runticks;

void system_reset();
void system_init();
void system_run();
void system_wait_cputicks(uint);

void __always_inline system_set_cputicks(uint t) { runticks = t; }
void __always_inline system_add_cputicks(uint t) { runticks += t; }

#endif