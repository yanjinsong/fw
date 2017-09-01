#ifndef __DBG_TIMER_H__
#define __DBG_TIMER_H__

#include <types.h>

void dbg_timer_init (void);
void dbg_timer_switch(int sw);
void dbg_get_time (Time_T *time);
u32 dbg_get_elapsed (Time_T *begin);
void dbg_timer_push (void);
void dbg_timer_pop (const char *label);

#endif // __DBG_TIMER_H__

