#include <config.h>
#include <msgevt.h>
#include <hdr80211.h>
#include <pbuf.h>
#include <log.h>
#include "SmartConfig.h"
#include "mlme.h"
#include "MLME_Task.h"
#include "sta_info.h"
#include <driver/phy/drv_phy.h>
#include <mbox/drv_mbox.h>

#ifdef AIRKISS_ENABLE
#include <airkiss.h>
#define AIRKISS_SCAN_TIME_OUT 100
#define MAX_RETRY_COUNT 3
u8 g_retryCountAfterLock=0;

//airkiss_result_t result;
const airkiss_config_t g_airkscfg = {
    (airkiss_memset_fn)&memset,
    (airkiss_memcpy_fn)&memcpy,
    (airkiss_memcmp_fn)&memcmp,
    (airkiss_printf_fn)&printf
};

typedef struct{
    airkiss_result_t ares;
    ETHER_ADDR	bssid;
}my_airkiss_result_t;

typedef enum{
    EN_SCONFIG_NOT_READY=0,
    EN_SCONFIG_GOING,
    EN_SCONFIG_LOCK,
    EN_SCONFIG_DONE,
}EN_SCONFIG_STATUS;

EN_SCONFIG_STATUS enSCstat=EN_SCONFIG_NOT_READY;
my_airkiss_result_t my_result;
airkiss_context_t g_smtcfg;
struct cfg_sconfig_request sreq;



#define ACCEPT_NONE_WSID_FRAME()           \
        do{                         \
            u32 val;                \
            val=GET_MRX_FLT_EN3;    \
            val|=0x0800;              \
            SET_MRX_FLT_EN3(val);   \
        }while(0)

#define DROP_NONE_WSID_FRAME()           \
        do{                         \
            u32 val;                \
            val=GET_MRX_FLT_EN3;    \
            val&=~0x0800;              \
            SET_MRX_FLT_EN3(val);   \
        }while(0)


s32 start_airkiss(void)
{
    int ret=0;
    printf("\33[32mAirKiss version:%s\33[0m\n",airkiss_version());
    ret=airkiss_init(&g_smtcfg,&g_airkscfg);
    if(ret<0){
        printf("AirKiss init fail\r\n");
        return -1;
    }
    enSCstat=EN_SCONFIG_GOING;
    return 0;
}

s32 set_next_channel(void)
{
    Mlme_WorkJobReq *WorkJobReq;
    s32 i=0,j=0;
    MLME_WORKJOB_STRUCT(WorkJobReq,gDevInfo);

    //start from the current channel
    i=gDevInfo->current_channel;
    j=0;
    for(;(i<(MAX_CHANNEL_NUM))&&(j<(MAX_CHANNEL_NUM));i=((i+1)%(MAX_CHANNEL_NUM)),j++)
    {
        if((WorkJobReq->u.sconfig.channel_mask&(1<<i))==0)
        {
            continue;
        }
        WorkJobReq->u.sconfig.channel_mask=WorkJobReq->u.sconfig.channel_mask&~(1<<i);
        WorkJobReq->u.sconfig.chidx=i;
        break;
    }

    return 0;
}

//u8 bssid[]={0xc8,0xd3,0xa3,0x51,0xdb,0xad};
s32 SConfig_Rx_Handler(PKT_RxInfo *pPktInfo)
{
    int ret=0;
    u8 source[6]={0};
    u8 broadcast_mac[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    Mlme_WorkJobReq *WorkJobReq;
    MLME_WORKJOB_STRUCT(WorkJobReq,gDevInfo);

    if((enSCstat==EN_SCONFIG_NOT_READY)||(enSCstat==EN_SCONFIG_DONE))
    {
        return 0;
    }

    if(GET_HDR80211_FC_FROMDS(pPktInfo))
    { //From DS
        GET_HDR80211_ADDRESS_1(source,pPktInfo);

    }
    else
    { //To DS
         GET_HDR80211_ADDRESS_3(source,pPktInfo);
    }

    if(0!=memcmp(source,broadcast_mac,sizeof(broadcast_mac)))
    {   //unicast frame
        return 0;
    }

    if(GET_HDR80211_FC_FROMDS(pPktInfo))
    {
        printf("From DS =>");
    }
    else
    {
        printf("ToDS =>");
    }

    GET_HDR80211_ADDRESS_2(source,pPktInfo);
    printf("ADDR2:%x:%x:%x:%x:%x:%x\r\n",
        source[0],source[1],source[2],source[3],source[4],source[5]);
    #if 0
    if(0==memcmp(my_result.bssid.addr,bssid,sizeof(bssid))){
        //printf("type=%d\r\n",GET_HDR80211_FC_TYPE(pPktInfo));
        //printf("sub type=%d\r\n",GET_HDR80211_FC_SUBTYPE(pPktInfo));
        printf("seq num=%d\r\n",GET_HDR80211_SC_SEQNUM(pPktInfo));
        printf("pPktInfo->len=%u\n",pPktInfo->len);
        //_packetdump("......",(const u8 *)((u32)pPktInfo + pPktInfo->hdr_offset),pPktInfo->len);//pPktInfo->len-pPktInfo->hdr_offset);
    }
    #endif
    ret=airkiss_recv(&g_smtcfg,(const void *)((u32)pPktInfo + pPktInfo->hdr_offset),pPktInfo->len);
    if(AIRKISS_STATUS_CHANNEL_LOCKED==ret)
    {
        enSCstat=EN_SCONFIG_LOCK;
        MLME_STOP_TIMER(gDevInfo->MTimer.MTimer);
        printf("\33[32mThe Airkiss lock on channel %d\33[0m\n",WorkJobReq->u.sconfig.chidx);

         /*
        Sometimes we can't get the complete status after airkiss return the lock.
        The state machine will restart the scanning, I think we don't need to scan whole channel  in this case.
        we just need to scan the previos and after channel of locking channel
        */
        WorkJobReq->u.sconfig.original_channel_mask=0;

        if(WorkJobReq->u.sconfig.chidx>1)
            WorkJobReq->u.sconfig.original_channel_mask|=(1<<(WorkJobReq->u.sconfig.chidx-1));

        if(WorkJobReq->u.sconfig.chidx>2)
            WorkJobReq->u.sconfig.original_channel_mask|=(1<<(WorkJobReq->u.sconfig.chidx-2));

        WorkJobReq->u.sconfig.original_channel_mask |= (1<<WorkJobReq->u.sconfig.chidx);

        if(WorkJobReq->u.sconfig.chidx<10)
            WorkJobReq->u.sconfig.original_channel_mask|=(1<<(WorkJobReq->u.sconfig.chidx+2));

        if(WorkJobReq->u.sconfig.chidx<11)
            WorkJobReq->u.sconfig.original_channel_mask|=(1<<(WorkJobReq->u.sconfig.chidx+1));

        WorkJobReq->u.sconfig.waitting_time=0;

        SConfig_action(MLME_WORK_SMART_CONFIG_LOCK,NULL,0);
    }
    else if(AIRKISS_STATUS_COMPLETE==ret)
    {
        enSCstat=EN_SCONFIG_DONE;
        printf("\33[32mThe Airkiss is completed\33[0m\n");
        MLME_STOP_TIMER(gDevInfo->MTimer.MTimer);
        GET_HDR80211_ADDRESS_2(my_result.bssid.addr,pPktInfo);
        airkiss_get_result(&g_smtcfg,&my_result.ares);
        #if 1
        u8 ssid_buf[MAX_SSID_LEN+1]={0};
        memcpy((void*)ssid_buf,(void*)my_result.ares.ssid,my_result.ares.ssid_length);
        printf("ssid=%s\r\n",ssid_buf);
        printf("ssid_length=%u\r\n",my_result.ares.ssid_length);
        printf("pwd=%s\r\n",my_result.ares.pwd);
        printf("pwd_length=%u\r\n",my_result.ares.pwd_length);
        printf("random=%u\r\n",my_result.ares.random);
        printf("reserved=%u\r\n",my_result.ares.reserved);
        #endif
        //DROP_NONE_WSID_FRAME();
        SConfig_action(MLME_WORK_SMART_CONFIG_DONE,NULL,0);
    }

    return 0;
}
s32 Sconfig_Done(void **ppMlmeReqParams, u8 channel)
{
    u32 size=0;
    HDR_HostEvent   *ev=NULL;
    struct resp_evt_result *SonfigDone=NULL;

    size=SCONFIG_DONE_EVT_HEADER_SIZE;
    HOST_EVENT_REALLOC_0(ppMlmeReqParams,ev,SOC_EVT_SCONFIG_SCAN_DONE,size);
    HOST_EVENT_ASSIGN_EVT_NO(ev,mlme_cmd_seq_no);
    SonfigDone = (struct resp_evt_result *)HOST_EVENT_DATA_PTR(ev);
    memset((void *)SonfigDone,0,sizeof(struct resp_evt_result));
    if((my_result.ares.ssid_length<=MAX_SSID_LEN)&&((my_result.ares.pwd_length<=MAX_PASSWD_LEN)))
    {
        SonfigDone->u.sconfig_done.result_code=0;
        memcpy(SonfigDone->u.sconfig_done.ssid,my_result.ares.ssid,my_result.ares.ssid_length);
        SonfigDone->u.sconfig_done.ssid_len=my_result.ares.ssid_length;
        memcpy(SonfigDone->u.sconfig_done.pwd,my_result.ares.pwd,my_result.ares.pwd_length);
        SonfigDone->u.sconfig_done.rand=my_result.ares.random;
        SonfigDone->u.sconfig_done.channel=channel;
    }
    else
    {
        SonfigDone->u.sconfig_done.result_code=1;
    }
    HOST_EVENT_SET_LEN(ev, size);
    HOST_EVENT_SEND(ev);
    printf("Sconfig Done\n");
    return MLME_OK;

}
s32 SConfig_action(u8 work_type, void *data, u32 data_len)
{
    Mlme_WorkJobReq *WorkJobReq;
    s32 ret = MLME_OK;
  //  u32 val=0;
    MLME_WORKJOB_STRUCT(WorkJobReq,gDevInfo);
    switch(work_type){
        case MLME_WORK_SMART_CONFIG_INIT:
            WorkJobReq->u.sconfig.waitting_time=0;

            if(WorkJobReq->u.sconfig.original_channel_mask!=WorkJobReq->u.sconfig.backup_original_channel_mask){
                //In this case, SmartConfig only scan the neighbor channel of lock_channel
                g_retryCountAfterLock++;
            }

            if(g_retryCountAfterLock%MAX_RETRY_COUNT==0){
                //reset original_channel_mask
                g_retryCountAfterLock=0;
                WorkJobReq->u.sconfig.original_channel_mask=WorkJobReq->u.sconfig.backup_original_channel_mask;
            }

            WorkJobReq->u.sconfig.channel_mask = WorkJobReq->u.sconfig.original_channel_mask;
            if(enSCstat==EN_SCONFIG_NOT_READY){
                //Force to start from the current channel in the first time
                WorkJobReq->u.sconfig.channel_mask |= (1<<gDevInfo->current_channel);
            }

            if(-1==start_airkiss()) return -1;
            SetStaAction(STA_ACTION_SCANING);
            //ACCEPT_NONE_WSID_FRAME();
        case MLME_WORK_SMART_CONFIG_CHANNEL_SETTING:
            if(enSCstat==EN_SCONFIG_GOING){
                if(WorkJobReq->u.sconfig.waitting_time>40000){ //over 40s
                    SConfig_action(MLME_WORK_SMART_CONFIG_INIT,NULL,0);
                }

                WorkJobReq->work_type=MLME_WORK_SMART_CONFIG_CHANNEL_SETTING;
                if(WorkJobReq->u.sconfig.channel_mask){
                    set_next_channel();
                    drv_phy_channel_change(WorkJobReq->u.sconfig.chidx,TRUE);
                    airkiss_change_channel(&g_smtcfg);
                    MLME_START_TIMER(&gDevInfo->MTimer, WorkJobReq->u.sconfig.dwell_time, MLME_WORK_SMART_CONFIG_SCAN_TIMEOUT , NULL, NULL);
                    WorkJobReq->u.sconfig.waitting_time+=WorkJobReq->u.sconfig.dwell_time;
                }else{
                    WorkJobReq->u.sconfig.channel_mask = WorkJobReq->u.sconfig.original_channel_mask;
                    WorkJobReq->u.sconfig.chidx=1;

                    SConfig_action(MLME_WORK_SMART_CONFIG_CHANNEL_SETTING,NULL,0);
                }
            }
            break;
        case MLME_WORK_SMART_CONFIG_LOCK:
            if(WorkJobReq->u.sconfig.waitting_time>=10000){ //over 10s
                //Lock Time Out
                MLME_STOP_TIMER(gDevInfo->MTimer.MTimer);
                SConfig_action(MLME_WORK_SMART_CONFIG_INIT,NULL,0);
            }else{
                //Start a timer for lock status.
                WorkJobReq->u.sconfig.waitting_time+=10000;
                MLME_START_TIMER(&gDevInfo->MTimer, 10000, MLME_WORK_SMART_CONFIG_LOCK_TIMEOUT , NULL, NULL);
            }
            break;
        case MLME_WORK_SMART_CONFIG_COMPLETE:
            WorkJobReq->work_type=MLME_WORK_SMART_CONFIG_COMPLETE;
            break;
        case MLME_WORK_SMART_CONFIG_PROBE_REQ:
            WorkJobReq->work_type=MLME_WORK_SMART_CONFIG_PROBE_REQ;
            mlme_send_probe_req(my_result.bssid.addr,0,NULL);

        case MLME_WORK_SMART_CONFIG_PROBE_RSP:
            WorkJobReq->work_type=MLME_WORK_SMART_CONFIG_PROBE_RSP;
            MLME_START_TIMER(&gDevInfo->MTimer, WorkJobReq->u.sconfig.dwell_time, MLME_WORK_SMART_CONFIG_PROBE_TIMEOUT , NULL, NULL);
            break;
        case MLME_WORK_SMART_CONFIG_DONE:
            WorkJobReq->work_type=MLME_WORK_SMART_CONFIG_DONE;
            Sconfig_Done(&gDevInfo->MlmeReqParams,WorkJobReq->u.sconfig.chidx);
            MLME_Cmd_Done(&gDevInfo->MlmeReqParams);
            SetStaAction(STA_ACTION_READY);
            break;
        default:
            break;
    }
    return ret;

}

void SConfig_FillWorkJobReq(Mlme_WorkJobReq *WorkJobReq, u32 size, u32 cmd_seq_no, struct cfg_sconfig_request *SconfigReq)
{

   u16     dwell_time;
    u16     channel_mask;

    dwell_time=SconfigReq->dwell_time;
    channel_mask=SconfigReq->channel_mask;

    /**
     * cmd.c_type and cmd.len are for reuse the pbuf of WorkJobReq.
     */
    WorkJobReq->cmd.c_type=HOST_CMD;
    WorkJobReq->cmd.len=size;

    WorkJobReq->cmd_seq_no = cmd_seq_no;
    WorkJobReq->u.sconfig.dwell_time = (dwell_time == 0)? AIRKISS_SCAN_TIME_OUT: (dwell_time*10);
    WorkJobReq->u.sconfig.original_channel_mask = channel_mask;
    WorkJobReq->u.sconfig.original_channel_mask = WorkJobReq->u.sconfig.original_channel_mask&~(1<<0);
    WorkJobReq->u.sconfig.original_channel_mask = WorkJobReq->u.sconfig.original_channel_mask&~(1<<15);
    WorkJobReq->u.sconfig.channel_mask = WorkJobReq->u.sconfig.original_channel_mask;
    WorkJobReq->u.sconfig.backup_original_channel_mask = WorkJobReq->u.sconfig.original_channel_mask;
    WorkJobReq->u.sconfig.chidx = 1;
    WorkJobReq->u.sconfig.waitting_time=0;
    WorkJobReq->work_type=MLME_WORK_SMART_CONFIG_INIT;
    printf("channel mask=%x\r\n",WorkJobReq->u.sconfig.channel_mask);
    printf("dwell_time=0x%x\r\n",WorkJobReq->u.sconfig.dwell_time);
    return ;
}


s32 MLME_SConfig( struct cfg_host_cmd **_HostCmd)
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    struct cfg_sconfig_request SconfigReq;
    u32 cmd_seq_no;
    Mlme_WorkJobReq *WorkJobReq=NULL;
    size_t size;

    gDevInfo->MlmeReqParams=NULL;

    //backup the data for reusing the pbuf
    cmd_seq_no=HostCmd->cmd_seq_no;
    memcpy((void *)&SconfigReq,(const void *)HostCmd->dat8,sizeof(struct cfg_sconfig_request));

    /* Allocate buffer for storing WorkJobReq */
    size = sizeof(Mlme_WorkJobReq);

    WorkJobReq = (Mlme_WorkJobReq * )PBUF_MReAlloc(_HostCmd,size);
    if(WorkJobReq==NULL)
    {
        MLME_Cmd_Done((void **)_HostCmd);
        return -1;
    }

    SConfig_FillWorkJobReq(WorkJobReq,size,cmd_seq_no,&SconfigReq);

    gDevInfo->MlmeReqParams = WorkJobReq; //This pbuf will be reused in SConfig_Done
    g_retryCountAfterLock=0;
    SConfig_action(MLME_WORK_SMART_CONFIG_INIT,NULL,0);
    return 0;
}

#endif

