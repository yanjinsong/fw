#ifndef _SOFT_MAIN_H_
#define _SOFT_MAIN_H_


void soft_mac_task( void *args );
s32 soft_mac_init(void);
bool IsSoftMacTaskSleeping(void);
s32 ssv6xxx_PacketToNextHW(void *p, s32 next_hw);

typedef enum soft_mac_timer_type_en
{
    SOFT_MAC_TIMER_REORDER_BUFFER,

} soft_mac_timer_type;







#endif /* _SOFT_MAIN_H_ */

