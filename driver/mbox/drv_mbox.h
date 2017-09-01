#ifndef _DRV_MBOX_H_
#define _DRV_MBOX_H_

#include <hctrl.h>

/* Hardware Offload Engine ID */
#define M_ENG_CPU                       0x00
#define M_ENG_HWHCI                     0x01
//#define M_ENG_FRAG                    0x02
#define M_ENG_EMPTY	                    0x02
#define M_ENG_ENCRYPT                   0x03
#define M_ENG_MACRX                     0x04
#define M_ENG_MIC	                    0x05
#define M_ENG_TX_EDCA0                  0x06
#define M_ENG_TX_EDCA1                  0x07
#define M_ENG_TX_EDCA2                  0x08
#define M_ENG_TX_EDCA3                  0x09
#define M_ENG_TX_MNG                    0x0A
#define M_ENG_ENCRYPT_SEC               0x0B
#define M_ENG_MIC_SEC                   0x0C
#define M_ENG_RESERVED_1                0x0D
#define M_ENG_RESERVED_2                0x0E
#define M_ENG_TRASH_CAN                 0x0F
#define M_ENG_MAX                      (M_ENG_TRASH_CAN+1)

/*The queue's depth of HW */
#define M_ENG_CPU_INT               16
#define M_ENG_CPU_OUT               16
#define M_ENG_HWHCI_INT             8
#define M_ENG_HWHCI_OUT             16
#define M_ENG_EMPTY_INT             0
#define M_ENG_EMPTY_OUT             0
#define M_ENG_ENCRYPT_INT           4
#define M_ENG_ENCRYPT_OUT           16
#define M_ENG_MACRX_INT             16
#define M_ENG_MACRX_OUT             0
#define M_ENG_MIC_INT               4
#define M_ENG_MIC_OUT               4
#define M_ENG_TX_EDCA0_INT          4
#define M_ENG_TX_EDCA0_OUT          8
#define M_ENG_TX_EDCA1_INT          4
#define M_ENG_TX_EDCA1_OUT          16
#define M_ENG_TX_EDCA2_INT          4
#define M_ENG_TX_EDCA2_OUT          16
#define M_ENG_TX_EDCA3_INT          4
#define M_ENG_TX_EDCA3_OUT          16
#define M_ENG_TX_MNG_INT            4
#define M_ENG_TX_MNG_OUT            8
#define M_ENG_ENCRYPT_SEC_INT       4
#define M_ENG_ENCRYPT_SEC_OUT       16
#define M_ENG_MIC_SEC_INT           4
#define M_ENG_MIC_SEC_OUT           4
#define M_ENG_RESERVED_1_INT        0
#define M_ENG_RESERVED_1_OUT        0
#define M_ENG_RESERVED_2_INT        0
#define M_ENG_RESERVED_2_OUT        0
#define M_ENG_TRASH_CAN_INT         0
#define M_ENG_TRASH_CAN_OUT         32


/* Software Engine ID: */
#define M_CPU_HWENG                     0x00
#define M_CPU_TXL34CS                   0x01
#define M_CPU_RXL34CS                   0x02
#define M_CPU_DEFRAG                    0x03
#define M_CPU_EDCATX                    0x04
#define M_CPU_RXDATA                    0x05
#define M_CPU_RXMGMT                    0x06
#define M_CPU_RXCTRL                    0x07
#define M_CPU_FRAG                      0x08


#define HW_ID_OFFSET                    7

#if 0
#define ENG_MBOX_NEXT(m)                drv_mailbox_cpu_next((u32)(m))
#define ENG_MBOX_SEND(e, m)             drv_mailbox_cpu_ff((u32)(m), e)
#define TX_FRAME(m)                     ENG_MBOX_SEND(M_ENG_HWHCI, (m))
#else
//extern inline s32 ENG_MBOX_NEXT(u32 pktmsg);
//extern inline s32 ENG_MBOX_SEND(u32 hw_number,u32 pktmsg);
inline s32 drv_mailbox_next(u32 pktmsg);
inline s32 drv_mailbox_send(u32 hw_number,u32 pktmsg);
#endif



/**
* The following definitions are only for simulation/emulation platform. These
* definitions are used for simulation of the real chip mailbox behaviors.
*/
#if (CONFIG_SIM_PLATFORM == 1)
static char sg_me_name[50];

#define HW_CHANGE_MOD_NAME(_s)                              \
{                                                           \
    sg_me_name[0] = 0x00;                                   \
    strcpy(sg_me_name, _s);								    \
}

#ifdef EDCA_PATTERN_TEST
#define HW_ENG_END(p)                                       \
{                                                           \
    PBUF_AddFlow((void *)p, sg_me_name, "(end)");           \

}

#define HW_ENG_ENTER(p)                                     \
{                                                           \
    sg_me_name[0] = 0x00;                                   \
    strcpy(sg_me_name, __FUNCTION__);                       \
    PBUF_AddFlow((void *)p, sg_me_name, "(in)");            \
}
#else

#define HW_ENG_ENTER(p)                                     \
{                                                           \
    sg_me_name[0] = 0x00;                                   \
    strcpy(sg_me_name, __FUNCTION__);                       \
    PBUF_AddFlow((void *)p, sg_me_name, "(in)");            \
    PBUF_Logging(p);                                        \
}

#define HW_ENG_END(p)                                       \
{                                                           \
    PBUF_AddFlow((void *)p, sg_me_name, "(end)");           \
    PBUF_Logging(p);                                        \
}
#endif


#define HW_ENG_NEXT(p)                                      \
{                                                           \
    PBUF_AddFlow((void *)p, sg_me_name, "(out)");                   \
    drv_mailbox_next(p);                                       \
}

#define HW_ENG_2HOST(p)                                     \
{                                                           \
    PBUF_AddFlow((void *)p, sg_me_name, "(2host)");                 \
    PBUF_Set2HostFlag(p);                                   \
    PBUF_Logging(p);                                        \
}

#define HW_ENG_DROP(p)                                      \
{                                                           \
    PBUF_AddFlow((void *)p, sg_me_name, "(drop)");                  \
    PBUF_SetDropFlag(p);                                    \
    PBUF_Logging(p);                                        \
    PBUF_HwDrop(p);                                         \
}
#define HW_ENG_TRAP(p, r)                                   \
{                                                           \
    drv_mailbox_sim_trap_to_cpu((void*)p, r,                \
    sg_me_name, "(trap-cpu)", "(trap-drop)");               \
}



void drv_mailbox_sim_trap_to_cpu(void *msg, u16 reason, u8 *fcmd, u8 *trapStr, u8* dropStr);

#endif /* CONFIG_SIM_PLATFORM */




#endif /* _DRV_MBOX_H_ */

