#ifndef _SYSTEM_H_
#define _SYSTEM_H_
#include <stdint.h>
#include <sys/types.h>

extern uint runticks;

void system_reset();
void system_init();
void system_run();
void system_wait_cputicks(uint);

#ifdef EMU_M6502
void __always_inline system_set_cputicks(uint t) { runticks = t / 3; }
void __always_inline system_add_cputicks(uint t) { runticks += t / 3; }

#else
void __always_inline system_set_cputicks(uint t) { runticks = t; }
void __always_inline system_add_cputicks(uint t) { runticks += t; }
#endif
#endif