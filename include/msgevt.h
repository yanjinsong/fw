#ifndef _MSGEVT_H_
#define _MSGEVT_H_
#include <config.h>
#include <soc_global.h>
#if (CONFIG_SIM_PLATFORM == 1)
#include <sim_global.h>
#include <host_global.h>
#endif

#define MAX_DELAY portMAX_DELAY

/**
 *  Define Message Type for MsgType:
 *
 *  Note that we assign ID from 10 due to that ID 0-9 are reserved for
 *  lwip-tcpip message type.
 *
 *  @ MEVT_MSG_PKTBUFF
 *  @ MEVT_MSG_SOFT_TIMER
 *  @ MEVT_MSG_HOST_TIMER
 */
typedef enum msgevt_type_en
{
    MEVT_PKT_BUF                        = 10,
    MEVT_SOFT_TIMER                         ,
    MEVT_HOST_TIMER                         ,
    MEVT_HOST_CMD                           ,
    MEVT_TX_DONE                            ,
    MEVT_NET_APP_REQ                        ,
    MEVT_EAPOL                              ,
    MEVT_BKG_TASK                           , // Background task
#ifdef THROUGHPUT_TEST
    MEVT_SDIO_TEST_CMD                      ,
    MEVT_THROUGHPUT							,
#endif
	MEVT_BCN_CMD							,
	MEVT_SMART_CFG							,
	MEVT_WAKEUP_TASK						,
	MEVT_POWER_SAVING						,
	MEVT_DETECT_AP  						,	


} msgevt_type;


struct MsgEvent_st
{
	msgevt_type     MsgType;
    u32             MsgData;
    u32             MsgData1;
    u32             MsgData2;
	u32             MsgData3;
};

typedef struct MsgEvent_st MsgEvent;
typedef struct MsgEvent_st *PMsgEvent;

/**
* Define Message Event Queue:
*
* @ MBOX_SOFT_MAC
* @ MBOX_MLME
* @ MBOX_CMD_ENGINE
* @ MBOX_HCMD_ENGINE
* @ MBOX_TCPIP
*/
#define MBOX_SOFT_MAC           g_soc_task_info[0].qevt
#define MBOX_MLME               g_soc_task_info[1].qevt
#ifdef THROUGHPUT_TEST
#define MBOX_TESTINGS    		g_soc_task_info[3].qevt
#endif

#define MBOX_HCMD_ENGINE        g_host_task_info[0].qevt
#define MBOX_SIM_DRIVER         g_host_task_info[1].qevt
#define MBOX_SIM_TX_DRIVER      g_host_task_info[2].qevt

#define MBOX_TCPIP


void *msg_evt_alloc_0(void);
void *msg_evt_alloc(void);
void  msg_evt_free(MsgEvent *msgev);
s32   msg_evt_post(OsMsgQ msgevq, MsgEvent *msgev);
s32   msg_evt_fetch(OsMsgQ msgevq, u32 TickToWait,MsgEvent **msgev);
s32   msg_evt_init(void);
s32	  msg_evt_post_data1(OsMsgQ msgevq, msgevt_type msg_type, u32 msg_data, u32 msg_data1, u32 msg_data2, u32 msg_data3);
bool  msg_evt_empty(OsMsgQ msgevq);
int msg_evt_count(OsMsgQ msgevq);

#endif /* _MSGEVT_H_ */

