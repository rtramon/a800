#ifndef _SYSTEM_H_
#define _SYSTEM_H_
#include <stdint.h>
#include <sys/types.h>

void system_reset();
void system_init();
void system_run();
void system_set_runticks(uint);

#endif