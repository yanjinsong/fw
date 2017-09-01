#include <config.h>
#include <msgevt.h>

#ifdef CHECK_MSG_EVT_RANGE
#include <log.h>
static u32      min_msg_evt_addr;
static u32      max_msg_evt_addr;
#endif // CHECK_MSG_EVT_RANGE

#define MSG_EVT_DEBUG	0
#if (MSG_EVT_DEBUG)
#include <log.h>
#define MSGQ_STR(q)		(((q) == MBOX_SOFT_MAC)    ? "SOFT_MAC"    : \
						 ((q) == MBOX_MLME)        ? "MLME"        : \
						 ((q) == MBOX_CMD_ENGINE)  ? "CMD_ENGINE"  : \
						 ((q) == MBOX_HCMD_ENGINE) ? "HCMD_ENGINE" : \
						 ((q) == MBOX_SIM_DRIVER)  ? "SIM_DRIVER"  : "UNKNOWN")
#endif	/* MSG_EVT_DEBUG */

static MsgEvent * volatile sg_msgevt_free_list;
static OsMutex sg_msgevt_mutex;
u32 msgevt_addr_beg;
u32 msgevt_addr_end;
void *msg_evt_alloc_0(void)
{
    MsgEvent *msgev;
    if (sg_msgevt_free_list==NULL)
        return NULL;
    msgev = sg_msgevt_free_list;
	#ifdef CHECK_MSG_EVT_RANGE
    if (((u32)msgev < min_msg_evt_addr) || ((u32)msgev > max_msg_evt_addr))
    {
        LOG_PRINTF("A MSG: 0x%08X - 0x%08X ~ 0x%08X\n", (u32)msgev,
                    min_msg_evt_addr, max_msg_evt_addr);
    }
    ASSERT_RET((((u32)msgev >= min_msg_evt_addr) && ((u32)msgev <= max_msg_evt_addr)),
               OS_FAILED);
	#endif // CHECK_MSG_EVT_RANGE
    sg_msgevt_free_list = (MsgEvent *)msgev->MsgData;
    g_free_msgevt_cnt --;
    return msgev;
}


void *msg_evt_alloc(void)
{
    void *msgev;
    //OS_MutexLock(sg_msgevt_mutex);
    taskENTER_CRITICAL();
    msgev = msg_evt_alloc_0();
    //OS_MutexUnLock(sg_msgevt_mutex);
    taskEXIT_CRITICAL();
    return msgev;
}

void msg_evt_free_0(MsgEvent *msgev)
{
	#ifdef CHECK_MSG_EVT_RANGE
    if (((u32)msgev < min_msg_evt_addr) || ((u32)msgev > max_msg_evt_addr))
    {
        LOG_PRINTF("F MSG: 0x%08X - 0x%08X ~ 0x%08X\n", (u32)msgev,
                    min_msg_evt_addr, max_msg_evt_addr);
        return;
    }
    //ASSERT_RET((((u32)msgev >= min_msg_evt_addr) && ((u32)msgev <= max_msg_evt_addr)),
    //           OS_FAILED);
	#endif // CHECK_MSG_EVT_RANGE
    msgev->MsgData = (u32)sg_msgevt_free_list;
    sg_msgevt_free_list = msgev;
    g_free_msgevt_cnt ++;
}

void msg_evt_free(MsgEvent *msgev)
{
    //OS_MutexLock(sg_msgevt_mutex);
    taskENTER_CRITICAL();
    msg_evt_free_0(msgev);
    taskEXIT_CRITICAL();
    //OS_MutexUnLock(sg_msgevt_mutex);
}

s32 msg_evt_post(OsMsgQ msgevq, MsgEvent *msgev)
{
    OsMsgQEntry MsgEntry;

#if (MSG_EVT_DEBUG)
	log_printf("msg_evt_post (%12s): 0x%08x\n\r", MSGQ_STR(msgevq), msgev);
#endif

	ASSERT_RET(msgevq != NULL, OS_FAILED);
    MsgEntry.MsgCmd  = 0;
    MsgEntry.MsgData = (void *)msgev;
	return OS_MsgQEnqueue(msgevq, &MsgEntry, gOsFromISR);
}

#ifdef CHECK_MSG_EVT_RANGE
extern u32 is_sta_timer (u32 addr);
#endif // CHECK_MSG_EVT_RANGE

int msg_evt_count(OsMsgQ msgevq)
{
    ASSERT(msgevq != NULL);
    return OS_MsgQWaitingSize(msgevq);
}

bool msg_evt_empty(OsMsgQ msgevq)
{
    ASSERT(msgevq != NULL);
    return (0==OS_MsgQWaitingSize(msgevq))?TRUE:FALSE;
}

s32 msg_evt_fetch(OsMsgQ msgevq, u32 TickToWait,MsgEvent **msgev)
{
    OsMsgQEntry MsgEntry;
    s32 res;
    MsgEvent *msg_ret = (MsgEvent *)NULL;

    ASSERT_RET(msgevq != NULL, OS_FAILED);
    res = OS_MsgQDequeue(msgevq, &MsgEntry, TickToWait, gOsFromISR);
    ASSERT_RET(((MsgEntry.MsgCmd == 0)|(TickToWait== 0)), res);


    if (res == OS_SUCCESS)
        msg_ret = (MsgEvent *)MsgEntry.MsgData;
	#ifdef CHECK_MSG_EVT_RANGE
    if (((u32)msg_ret < min_msg_evt_addr) || ((u32)msg_ret > max_msg_evt_addr))
    {
        if (!is_sta_timer((u32)msg_ret))
        {
            LOG_PRINTF("T MSG: 0x%08X - 0x%08X ~ 0x%08X\n", (u32)msg_ret,
                        min_msg_evt_addr, max_msg_evt_addr);
            ASSERT_RET((((u32)msg_ret >= min_msg_evt_addr) && ((u32)msg_ret <= max_msg_evt_addr)),
                       OS_FAILED);
        }
    }
	#endif // CHECK_MSG_EVT_RANGE
	#if (MSG_EVT_DEBUG)
	log_printf("msg_evt_fetch(%12s): 0x%08x\n\r", MSGQ_STR(msgevq), msg_ret);
	#endif

	*msgev = msg_ret;
	return res;
}


s32 msg_evt_init(void)
{
    u32 index, size;

    OS_MutexInit(&sg_msgevt_mutex);

    /**
        *  Forbid allocation/free of memory through OS_MemAlloc() and
        *  OS_MemFree() APIs, pre-allocate enough MsgEvent here.
        */
    size = sizeof(MsgEvent)*MBOX_MAX_MSG_EVENT;

    taskENTER_CRITICAL();

    sg_msgevt_free_list = (MsgEvent *)OS_MemAlloc(size);
    msgevt_addr_beg=(u32)sg_msgevt_free_list;
    msgevt_addr_end=(u32)(sg_msgevt_free_list)+(sizeof(MsgEvent)*MBOX_MAX_MSG_EVENT)-1;
    ASSERT_RET(sg_msgevt_free_list, OS_FAILED);
    for(index=0; index<MBOX_MAX_MSG_EVENT-1; index++)
    {
        sg_msgevt_free_list[index].MsgData =
        (u32)&sg_msgevt_free_list[index+1];
    }
    sg_msgevt_free_list[index].MsgData = 0;
    g_free_msgevt_cnt = MBOX_MAX_MSG_EVENT;
	#ifdef CHECK_MSG_EVT_RANGE
    min_msg_evt_addr = (u32)sg_msgevt_free_list;
    max_msg_evt_addr = ((u32)&sg_msgevt_free_list[MBOX_MAX_MSG_EVENT]) - 1;
	#endif // CHECK_MSG_EVT_RANGE
    taskEXIT_CRITICAL();

    return OS_SUCCESS;
}

s32 msg_evt_post_data1 (OsMsgQ msgevq, msgevt_type msg_type, u32 msg_data, u32 msg_data1, u32 msg_data2, u32 msg_data3)
{
    MsgEvent *msg_ev = (MsgEvent *)(gOsFromISR ? msg_evt_alloc_0() : msg_evt_alloc());
    //ASSERT_RET(msg_ev, OS_FAILED);

    if(msg_ev==NULL)
    {
        printf("msg_evt alloc fail\n");
        return OS_FAILED;
    }
    
    msg_ev->MsgType  = msg_type;
    msg_ev->MsgData  = msg_data;
    msg_ev->MsgData1  = msg_data1;
    msg_ev->MsgData2  = msg_data2;
    msg_ev->MsgData3  = msg_data3;

    if (OS_SUCCESS == msg_evt_post(msgevq, msg_ev))
        return OS_SUCCESS;

    if (gOsFromISR)
        msg_evt_free_0(msg_ev);
    else
        msg_evt_free(msg_ev);

    return OS_FAILED;
} // end of - msg_evt_post_14o -




