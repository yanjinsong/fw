#include <config.h>
#include <ssv_regs.h>
#include <log.h>
#include <pbuf.h>
#include <hwmac/drv_mac.h>
#include <idmanage/drv_idmanage.h>
#if(PACKET_TRACE==1)
#include <mbox/drv_mbox.h>
#endif
#if (CONFIG_SIM_PLATFORM == 1)
#include <wsimp/wsimp_lib.h>
#include <wsimp/wsimp_dump.h>



/**
 * Define Data structure and static global variables for simulation
 * and emulation use only.
 */
typedef struct HW_PBUF_Info_st
{
    u16         valid;
    u16         pbuf_id;
    u16         size;
    u32         pa;     /* physical address */
    u32         va;     /* virtual address */

    /* logging infomation: */
    u32         pktid;  /* Packet ID */
    u32         fragid;
    u8          parsed; /* 0: not parsed frame, 1: parsed frame */
    u8          drop;   /* 0: not drop, 1: drop */
    u32         rx2host; /* 0: not to host, 1: to host format */
    u8          flow_buffer[1024];

} HW_PBUF_Info, *PHW_PBUF_Info;

#define PBUF_MAX_NO                     128
#define PBUF_VA_BASE                    0x8000





static HW_PBUF_Info HW_PBUF_Pool[PBUF_MAX_NO];
static OsMutex PBUF_LogMutex;

#endif /* CONFIG_SIM_PLATFORM */



static OsMutex sg_pbuf_mutex;





/**
 *  Packet buffer driver for SSV6200 Hardware Packet Buffer Manipulation.
 *  Three APIs are implemented for this driver:
 *
 *      @ PBUF_Init()           -Initialize hardware packet buffer setting
 *                                      and a mutex.
 *      @ PBUF_MAlloc()       -Request a packet buffer from hardware.
 *      @ PBUF_MFree()        -Release a packet buffer to hardware.
 *
 */


s32 PBUF_Init(void)
{
#if (CONFIG_SIM_PLATFORM == 1)
    /* For simulation/emulation only */
    memset(&HW_PBUF_Pool, 0, sizeof(HW_PBUF_Info)*PBUF_MAX_NO);
    OS_MutexInit(&PBUF_LogMutex);
#endif
    OS_MutexInit(&sg_pbuf_mutex);
    return OS_SUCCESS;
}



static PKT_Info* __PBUF_MAlloc_0(u32 size, u32 need_header, PBuf_Type_E buf_type)
{
    PKT_Info *pkt_info;
	u32 extra_header=0;
    if (need_header)
    {
        extra_header = (GET_PB_OFFSET + (GET_TX_PKT_RSVD<<TX_PKT_RES_SHIFT));
	    size += extra_header;
    }

    /**
        * The following code is for simulation/emulation platform only.
        * In real chip, this code shall be replaced by manipulation of
        * hardware packet engine.
        */
    /* Request a packet buffer from hardware */
    pkt_info = (PKT_Info *)drv_id_mng_alloc(size, buf_type);
	if(extra_header)
		memset(pkt_info, 0, extra_header);

    return pkt_info;
}

void *PBUF_MAlloc_Raw(u32 size, u32 need_header, PBuf_Type_E buf_type)
{
    PKT_Info *pkt_info;
    if (gOsFromISR)
    {
        pkt_info = __PBUF_MAlloc_0(size,need_header,buf_type);
    }
    else
    {

        OS_MutexLock(sg_pbuf_mutex);

        pkt_info = __PBUF_MAlloc_0(size,need_header,buf_type);

        OS_MutexUnLock(sg_pbuf_mutex);
    }
    #if(PACKET_TRACE==1)
    if(NULL!=pkt_info)
    {
        if(1==is_pbuf_in_fw((void *)pkt_info))
        {
            printf("%s:WARN!!This pbuf is already in fw(0x%x)\n",__func__,(u32)pkt_info);
        }
        pt((void *)pkt_info,M_ENG_MAX,EN_LOC_ALLOCATE_PBUF);
    }
    #endif
    return (void *)pkt_info;
}



static void __PBUF_MFree_0(void *PKTMSG)
{
    /**
        * The following code is for simulation/emulation platform only.
        * In real chip, this code shall be replaced by manipulation of
        * hardware packet engine.
        */
#if (CONFIG_SIM_PLATFORM == 1)
    {
        s32 i;
        for(i=0; i<PBUF_MAX_NO; i++) {
            if (HW_PBUF_Pool[i].valid== 0)
                continue;
            if ((u32)HW_PBUF_Pool[i].pa != (u32)PKTMSG)
                continue;
            free(PKTMSG);
            HW_PBUF_Pool[i].valid = 0;
            break;
        }
        ASSERT(i < PBUF_MAX_NO);
    }
#else
    ASSERT(drv_id_mng_rls(PKTMSG)==TRUE);
#endif

}

static inline void __PBUF_MFree_1(void *PKTMSG)
{
    OS_MutexLock(sg_pbuf_mutex);

    __PBUF_MFree_0(PKTMSG);

    OS_MutexUnLock(sg_pbuf_mutex);
}

void _PBUF_MFree (void *PKTMSG)
{
	if (gOsFromISR)
		__PBUF_MFree_0(PKTMSG);
	else
		__PBUF_MFree_1(PKTMSG);
}



bool PBUF_is_reusable(void *old_packet, u32 new_packet_len)
{
    PKT_Info *p=(PKT_Info *)old_packet;
    u8 old_page_num=0,new_page_num=0;
    u32 _len=0;
    if(p->c_type==HOST_CMD)
    {
        _len=p->len;
        old_page_num=(_len/PBUF_PAGE_SIZE);
        old_page_num+=(_len%PBUF_PAGE_SIZE)?1:0;

        _len=new_packet_len+sizeof(HDR_HostCmd);
        new_page_num=(_len/PBUF_PAGE_SIZE);
        new_page_num+=(_len%PBUF_PAGE_SIZE)?1:0;

        //if(((p->len%PBUF_PAGE_SIZE)+1)>=(((new_packet_len+sizeof(HDR_HostCmd))%PBUF_PAGE_SIZE)+1))
        if(old_page_num>=new_page_num)
        {
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }
    else
    {
        if(IS_TX_PKT(p)||IS_RX_PKT(p))
        {
            _len=p->len+p->hdr_offset;
            old_page_num=(_len/PBUF_PAGE_SIZE);
            old_page_num+=(_len %PBUF_PAGE_SIZE)?1:0;

            _len=(new_packet_len+GET_TX_PKT_RSVD*TX_PKT_RES_BASE+drv_mac_get_pbuf_offset());
            new_page_num=(_len/PBUF_PAGE_SIZE);
            new_page_num+=(_len%PBUF_PAGE_SIZE)?1:0;

            //if((((p->len+p->hdr_offset)%PBUF_PAGE_SIZE)+1)>=
            //    (((new_packet_len+GET_TX_PKT_RSVD*TX_PKT_RES_BASE+drv_mac_get_pbuf_offset())%PBUF_PAGE_SIZE)+1))
            if(old_page_num>=new_page_num)
            {
                return TRUE;
            }
            else
            {
                return FALSE;
            }
        }
    }

    return FALSE;
}

void *_PBUF_MReAlloc(void **_pp, u32 new_size)
{
    void *_new=NULL;

    if(((*_pp)==NULL)||(!PBUF_isPkt((*_pp))))
    {
        _new=PBUF_MAlloc(new_size,SOC_PBUF);
    }
    else
    {
        if(TRUE==(PBUF_is_reusable((*_pp),new_size)))
        {
            _new=(*_pp);
            PBUF_SET_REUSE(_pp);
        }
        else
        {
            PBUF_MFree((*_pp));
            PBUF_SET_REUSE(_pp);
            _new=PBUF_MAlloc(new_size,SOC_PBUF);
        }
    }
    return _new;
}
#if (CONFIG_SIM_PLATFORM == 1)

u32 PBUF_GetPbufInfo(void *PKTMSG)
{
    s32 i;
    for(i=0; i<PBUF_MAX_NO; i++) {
        if (HW_PBUF_Pool[i].valid== 0)
            continue;
        if ((u32)HW_PBUF_Pool[i].pa != (u32)PKTMSG)
            continue;
        return (u32)&HW_PBUF_Pool[i];
    }
    ASSERT(0);
    return 0;
}




//Just copy pktid, fragid, flow_buffer
void PBUF_CopyPBUFInfo(void *DSTPKTMSG, void *SRCPKTMSG)
{
    /* In real chip, we need to add mutex */
    HW_PBUF_Info *dst, *src;

	dst = (HW_PBUF_Info *)PBUF_GetPbufInfo(DSTPKTMSG);
	src = (HW_PBUF_Info *)PBUF_GetPbufInfo(SRCPKTMSG);


	dst->pktid		= src->pktid;
	dst->fragid		= src->fragid;
	dst->rx2host	= src->rx2host;
	memcpy(dst->flow_buffer, src->flow_buffer, strlen(src->flow_buffer));
}



bool PBUF_AllUnUse(void)
{
	int i,use_count=0;
	for(i=0; i<PBUF_MAX_NO; i++)
    if (HW_PBUF_Pool[i].valid == 1)
	{
		use_count++;
	}
	//ap.c			== os_frame_alloc(AP_MGMT_PKT_LEN);
	//ap.c			== os_frame_alloc(AP_MGMT_BEACON_LEN);
	//ssv_drv.c		== os_frame_alloc(MAX_RECV_BUF)
	//drv_security  == (u32)PBUF_MAlloc_Raw(sizeof(struct SRAM_KEY_st), 0);
	if(use_count <= 4)
		return TRUE;
	else
		return FALSE;
}






u32 PBUF_GetEngineID(PKT_Info *PktInfo, enum fcmd_seek_type seek)
{
    u32 cmd_idx = PktInfo->fCmdIdx;
    switch(seek)
    {
    case FCMD_SEEK_PREV:
        if (cmd_idx > 0)
            cmd_idx --;
        break;
    case FCMD_SEEK_CUR:
        break;

    case FCMD_SEEK_NEXT:
        if (cmd_idx < 8)
            cmd_idx++;
        break;

    default: ASSERT(0);

    }
    return (PktInfo->fCmd>>(cmd_idx<<2))&0x0F;

}








void PBUF_HwDrop(void *PKTMSG)
{
    PBUF_MFree(PKTMSG);
}


u16 PBUF_GetPbufID(void *PKTMSG)
{
    s32 i;
    for(i=0; i<PBUF_MAX_NO; i++) {
        if (HW_PBUF_Pool[i].valid== 0)
            continue;
        if ((u32)HW_PBUF_Pool[i].pa != (u32)PKTMSG)
            continue;
        return HW_PBUF_Pool[i].pbuf_id;
    }
    ASSERT(0);
    return 0;
}


u8 PBUF_GetParsedStatus(void *PKTMSG)
{
    s32 i;
    for(i=0; i<PBUF_MAX_NO; i++) {
        if (HW_PBUF_Pool[i].valid== 0)
            continue;
        if ((u32)HW_PBUF_Pool[i].pa != (u32)PKTMSG)
            continue;
        return HW_PBUF_Pool[i].parsed;
    }
    ASSERT(0);
    return 0;
}

void PBUF_SetParsedStatus(void *PKTMSG)
{
    s32 i;
    for(i=0; i<PBUF_MAX_NO; i++) {
        if (HW_PBUF_Pool[i].valid== 0)
            continue;
        if ((u32)HW_PBUF_Pool[i].pa != (u32)PKTMSG)
            continue;
        HW_PBUF_Pool[i].parsed = 1;
        return;
    }
    ASSERT(0);
}


void PBUF_SetDropFlag(void *PKTMSG)
{
    s32 i;
    for(i=0; i<PBUF_MAX_NO; i++) {
        if (HW_PBUF_Pool[i].valid== 0)
            continue;
        if ((u32)HW_PBUF_Pool[i].pa != (u32)PKTMSG)
            continue;
        HW_PBUF_Pool[i].drop = 1;
        return;
    }
    ASSERT(0);
}


u8 PBUF_GetDropFlag(void *PKTMSG)
{
    s32 i;
    for(i=0; i<PBUF_MAX_NO; i++) {
        if (HW_PBUF_Pool[i].valid== 0)
            continue;
        if ((u32)HW_PBUF_Pool[i].pa != (u32)PKTMSG)
            continue;
        return HW_PBUF_Pool[i].drop;
    }
    ASSERT(0);
    return 0;
}


void PBUF_AddFlow(void *PKTMSG, u8 *fcmd, u8 *scmd)
{
    s32 i;
    for(i=0; i<PBUF_MAX_NO; i++) {
        if (HW_PBUF_Pool[i].valid== 0)
            continue;
        if ((u32)HW_PBUF_Pool[i].pa != (u32)PKTMSG)
            continue;
        strcat(HW_PBUF_Pool[i].flow_buffer, " ");
        strcat(HW_PBUF_Pool[i].flow_buffer, fcmd);
        strcat(HW_PBUF_Pool[i].flow_buffer, scmd);
        return;
    }
    ASSERT(0);
}


u8 *PBUF_GetFlow(void *PKTMSG)
{
    s32 i;
    for(i=0; i<PBUF_MAX_NO; i++) {
        if (HW_PBUF_Pool[i].valid== 0)
            continue;
        if ((u32)HW_PBUF_Pool[i].pa != (u32)PKTMSG)
            continue;
        return HW_PBUF_Pool[i].flow_buffer;
    }
    ASSERT(0);
    return (u8 *)NULL;
}

void PBUF_Logging(void *PKTMSG)
{
    if ((g_wsimp_flags & WSIMP_ENABLE) == 0)
        return ;
    OS_MutexLock(PBUF_LogMutex);
    if (g_wsimp_flags & WSIMP_RUN_RAW)
        wsimp_raw_dump(PKTMSG);
    else wsimp_dump(PKTMSG);
    OS_MutexUnLock(PBUF_LogMutex);
}

void PBUF_SetPktID(void *PKTMSG, u32 pktid)
{
    s32 i;
    for(i=0; i<PBUF_MAX_NO; i++) {
        if (HW_PBUF_Pool[i].valid== 0)
            continue;
        if ((u32)HW_PBUF_Pool[i].pa != (u32)PKTMSG)
            continue;
        HW_PBUF_Pool[i].pktid = pktid;
        return;
    }
    LOG_PRINTF("PKTMSG=0x%08x\n", (u32)PKTMSG);
    PBUF_Dump();
    ASSERT(0);
}


u32 PBUF_GetPktID(void *PKTMSG)
{
    s32 i;
    for(i=0; i<PBUF_MAX_NO; i++) {
        if (HW_PBUF_Pool[i].valid== 0)
            continue;
        if ((u32)HW_PBUF_Pool[i].pa != (u32)PKTMSG)
            continue;
        return HW_PBUF_Pool[i].pktid;
    }
    LOG_PRINTF("PKTMSG=0x%08x\n", (u32)PKTMSG);
    PBUF_Dump();
    ASSERT(0);
    return 0;
}

#if 0
u32 PBUF_GetPktByID(u32 PKTID)
{
    s32 i;
    for(i=0; i<PBUF_MAX_NO; i++) {
        if (HW_PBUF_Pool[i].valid== 0)
            continue;
        if ((u32)HW_PBUF_Pool[i].pbuf_id != (u32)PKTID)
            continue;
        return HW_PBUF_Pool[i].pa;
    }
    LOG_PRINTF("MISS PKTID=%d\n", (u32)PKTID);
    PBUF_Dump();
    ASSERT(0);
    return 0;
}
#endif



void PBUF_Set2HostFlag(void *PKTMSG)
{
    s32 i;
    for(i=0; i<PBUF_MAX_NO; i++) {
        if (HW_PBUF_Pool[i].valid== 0)
            continue;
        if ((u32)HW_PBUF_Pool[i].pa != (u32)PKTMSG)
            continue;
        HW_PBUF_Pool[i].rx2host = 1;
        return;
    }
    ASSERT(0);
}

u8 PBUF_Get2HostFlag(void *PKTMSG)
{
    s32 i;
    for(i=0; i<PBUF_MAX_NO; i++) {
        if (HW_PBUF_Pool[i].valid== 0)
            continue;
        if ((u32)HW_PBUF_Pool[i].pa != (u32)PKTMSG)
            continue;
        return HW_PBUF_Pool[i].rx2host;
    }
    ASSERT(0);
    return 0;
}


void PBUF_Dump(void)
{
    s32 i, in_use=0;
    for(i=0; i<PBUF_MAX_NO; i++) {
        if (HW_PBUF_Pool[i].valid == 1) {
            in_use ++;
            LOG_PRINTF(" -->pa=0x%08x\n", (u32) HW_PBUF_Pool[i].pa);
        }
    }
    LOG_PRINTF("PBUF total in use: %d\n", in_use);
    LOG_PRINTF("PBUF in freelist: %d\n", PBUF_MAX_NO-in_use);
}


#if 0

u8 PBUF_IsPBufAllFree(void)
{
    s32 i, free_pbuf;
    for(i=0, free_pbuf=0; i<PBUF_MAX_NO; i++) {
        if (HW_PBUF_Pool[i].valid==1)
            continue;
       free_pbuf ++;
    }
    if (free_pbuf != PBUF_MAX_NO)
        return false;
    return true;
}

#endif


u8 PBUF_MapPkttoID(void *PKTMSG)
{

	s32 i;
    for(i=0; i<PBUF_MAX_NO; i++) {
        if (HW_PBUF_Pool[i].valid== 0)
            continue;
        if ((u32)HW_PBUF_Pool[i].pa != (u32)PKTMSG)
            continue;
        return i;
    }
    ASSERT(0);
    return 0;
}
void* PBUF_MapIDtoPkt(u32 pktid)
{
   	if (HW_PBUF_Pool[pktid].valid== 1)
   		return (void *)HW_PBUF_Pool[pktid].pa;
	else
		ASSERT(0);
    return 0;
}




#endif






