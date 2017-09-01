#include <config.h>
#include <regs.h>
#include <pbuf.h>
#include "drv_mbox.h"
#if (CONFIG_SIM_PLATFORM == 1)
#include <msgevt.h>
#include <sim_global.h>
#endif
#include <log.h>
#include <bsp/irq.h>


#if (CONFIG_SIM_PLATFORM == 1)

static void drv_mailbox_sim_enqueue(u32 hw_no, u32 pktmsg)
{
	OsMsgQEntry MsgEntry;
	OsMsgQ MsgQ;
	s32 res;
	ASSERT(hw_no < M_ENG_MAX);
	if (hw_no==M_ENG_TX_EDCA0 || hw_no==M_ENG_TX_EDCA1 ||
        hw_no==M_ENG_TX_EDCA2 || hw_no==M_ENG_TX_EDCA3 ||
	    hw_no==M_ENG_TX_MNG )
		hw_no=M_ENG_MACRX;
	MsgEntry.MsgCmd  = 0;
	MsgEntry.MsgData = (void *)pktmsg;
	MsgQ = HW_ModuleInfo[hw_no].MsgBoxQ;
	res = OS_MsgQEnqueue(MsgQ, &MsgEntry, false);
	ASSERT(res == OS_SUCCESS);
}


void drv_mailbox_sim_trap_to_cpu(void *msg, u16 reason, u8 *fcmd, u8 *trapStr, u8* dropStr)
{
    PKT_Info *PktInfo=(PKT_Info *)msg;
    u32 err_no, shift;

	err_no = (reason>31)? GET_REASON_TRAP1:GET_REASON_TRAP0;
    PktInfo->reason = reason;
    if (reason > 31) {
        err_no = GET_REASON_TRAP1;
        shift = reason - 32;
    }
    else {
        err_no = GET_REASON_TRAP0;
        shift = reason;
    }
    if (err_no & (1<<shift)) {
        PBUF_AddFlow(PktInfo, fcmd, trapStr);
        PBUF_Logging(PktInfo);
        ENG_MBOX_SEND((u32)M_ENG_CPU, (u32)PktInfo);
    }
    else {
        PBUF_SetDropFlag(PktInfo);
        PBUF_AddFlow(PktInfo, fcmd, dropStr);
        PBUF_Logging(PktInfo);
        PBUF_HwDrop(PktInfo);
    }

}



#endif








//Mount mailbox FIFO for HW module(low priority)
/*
	default CPU(0)
	Command Flow => 87654321   drv_mailbox_cpu_ff(0x80000000,1)  => 1-2-3-4-5-6-7-8-9-0
	Command Flow => 87654321   drv_mailbox_cpu_ff(0x80000000,4)  => 4-5-6-7-8-9-0
	Command Flow => 87654321   drv_mailbox_cpu_ff(0x80000000,0)  => 0
	Command Flow => 87654321   drv_mailbox_cpu_ff(0x80000000,9)  => 0
*/

#define ADDRESS_OFFSET	16
extern u8 recover_mechanism;
extern u8 recover_enable;
s32 drv_mailbox_cpu_ff(u32 pktmsg, u32 hw_number)
{
	u8 failCount=0;
#ifdef MAILBOX_DEBUG
	LOG_PRINTF("mailbox 0x%08x\n\r",(u32)((pktmsg >> ADDRESS_OFFSET) | (hw_number << HW_ID_OFFSET))));
#endif
    #if(PACKET_TRACE==1)
    if(0==is_pbuf_in_fw((void *)pktmsg))
    {
        printf("%s:WARN!!This pbuf is not in fw(0x%x)\n",__func__,(u32)pktmsg);
    }
    pt((void *)pktmsg,hw_number,EN_LOC_LEAVE_FW);
    #endif
	while (GET_CH0_FULL)
	{
        if(recover_enable > 0)
        {
            if(failCount%100 ==0)
                SET_SYS_WDT_TIME_CNT(WATCHDOG_TIMER);
        }
	    if (failCount++ < 1000)
            continue;

        printf("ERROR!!MAILBOX Block[%d]\n", failCount);

        if(recover_enable > 0)
            SET_SYS_WDT_TIME_CNT(WATCHDOG_TIMER);

        return FALSE;
	} //Wait until input queue of cho is not full.

	{
		SET_HW_PKTID((u32)((pktmsg >> ADDRESS_OFFSET) | (hw_number << HW_ID_OFFSET)));
#if (CONFIG_SIM_PLATFORM)
        drv_mailbox_sim_enqueue(hw_number, pktmsg);
#endif
        if(recover_enable >0)
            SET_SYS_WDT_TIME_CNT(WATCHDOG_TIMER);

		return TRUE;
	}
}

//Mount mailbox FIFO for HW module(high priority)
s32 drv_mailbox_cpu_pri_ff(u32 pktmsg, u32 hw_number)
{
#if(CONFIG_LOG_ENABLE==1)
    u8 failCount=0;
#endif
#ifdef MAILBOX_DEBUG
    LOG_PRINTF("mailbox 0x%08x\n\r",(u32)((pktmsg >> ADDRESS_OFFSET) | (hw_number << HW_ID_OFFSET)));
#endif

    while(GET_CH0_FULL)
    {
#if(CONFIG_LOG_ENABLE==1)
        LOG_PRINTF("ERROR!!MAILBOX Block[%d]\n",failCount++);
#endif
    };//Wait until input queue of cho is not full.


    {
        SET_PRI_HW_PKTID((u32)((pktmsg >> ADDRESS_OFFSET) | (hw_number << HW_ID_OFFSET)));
        return TRUE;
    }
}


s32 drv_mailbox_cpu_next(u32 pktmsg)
{
    PKT_Info *pkt_info=(PKT_Info *)pktmsg;
    u32 shiftbit, eng_id;

    // shiftbit = pkt_info->fCmdIdx << 2;
    pkt_info->fCmdIdx ++;
    shiftbit = pkt_info->fCmdIdx << 2;
    eng_id = ((pkt_info->fCmd & (0x0F<<shiftbit))>>shiftbit);
#if (CONFIG_SIM_PLATFORM == 1)
#ifndef EDCA_PATTERN_TEST
    PBUF_Logging((void *)pktmsg);
#endif
#endif
    return drv_mailbox_cpu_ff(pktmsg, eng_id);
}


void drv_mailbox_arb_ff(void)
{
#ifdef MAILBOX_DEBUG
    LOG_PRINTF("ARB FIFO=(%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x)\n\r",
        GET_FF0_CNT,GET_FF1_CNT,GET_FF2_CNT,GET_FF3_CNT,GET_FF4_CNT,
        GET_FF5_CNT,GET_FF6_CNT,GET_FF7_CNT,GET_FF8_CNT,GET_FF9_CNT);
#endif
}

inline s32 drv_mailbox_next(u32 pktmsg)
{
    return drv_mailbox_cpu_next(pktmsg);
}

inline s32 drv_mailbox_send(u32 hw_number,u32 pktmsg)
{
    return drv_mailbox_cpu_ff(pktmsg,hw_number);
}
