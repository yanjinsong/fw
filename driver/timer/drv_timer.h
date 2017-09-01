#ifndef _DRV_TIMER_H_
#define _DRV_TIMER_H_

#include <regs.h>
#include <bsp/irq.h>


#define US_TIMER0                       (REG_US_TIMER + 0)
#define US_TIMER1                       (REG_US_TIMER + 1)
#define US_TIMER2                       (REG_US_TIMER + 2)
#define US_TIMER3                       (REG_US_TIMER + 3) //use for send beacon from firmware
//#define MS_TIMER0                       (REG_MS_TIMER + 0)//use for OS tick 10ms
//REG_MS_TIMER[0].TMR_CTRL = (OS_CONTEXT_SWITCHING_INTERVAL | (1<<16));
#define MS_TIMER1                       (REG_MS_TIMER + 1)
#define MS_TIMER2                       (REG_MS_TIMER + 2)
//#define MS_TIMER3                       (REG_MS_TIMER + 3)//use for recover interrupt
//REG_MS_TIMER[3].TMR_CTRL =1;
#define MAX_HW_TIMER                    8

#define HW_TIMER_MAX_VALUE_MS          (0xffff*1000)  //16bits    //65535000
#define HW_TIMER_MAX_VALUE_US          0xffff       //16bits    //65535

#define BCN_TIMER                       MS_TIMER1 //use for send beacon from firmware


/**
 * Hardware Timer Operation Mode
 *
 * @ HTMR_ONESHOT: Timer counts down to 0 and then stop.
 * @ HTMR_PERIODIC: Timer counts down to 0 and the reload.
 */
enum hwtmr_op_mode {
    HTMR_ONESHOT = 0,
    HTMR_PERIODIC = 1,
};


#define HTMR_COUNT_VAL(x)               ((x)&0xFFFF)
#define HTMR_OP_MODE(x)                 (((x)&0x01)<<16)
#define HTMR_GET_IRQ(x)                 (((HW_TIMER*)(x)-REG_TIMER)+ \
                                         IRQ_US_TIMER0)
/*HW just supports 16 bits timer for now*/
void hwtmr_init(void);
s32 hwtmr_stop(HW_TIMER *tmr);
s32 hwtmr_start(HW_TIMER *tmr, u16 count, irq_handler tmr_handle,
                   void *m_data, enum hwtmr_op_mode mode);

void irq_timer_handler (void *data);
#endif /* _DRV_TIMER_H_ */

