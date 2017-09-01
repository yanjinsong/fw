#include <config.h>
#include <msgevt.h>
#include <hdr80211.h>
#include <pbuf.h>
#include <log.h>

#include "mlme.h"
#include "MLME_Task.h"
#include "sta_info.h"
#include "supplicant/drivers/driver_cabrio.h"
#include <mbox/drv_mbox.h>
#include <edca/drv_edca.h>
#include <dbg_timer/dbg_timer.h>
#include <beacon/drv_beacon.h>
#include <timer/drv_timer.h>

#define DEFAULT_CHANNEL_DWELL_TIME		(150)
#define CCA_SAMPLE_PERIOD 128
#if 0
#define ENABLE_TX_QUEUE()
#define DISABLE_TX_QUEUE()
#else
#define ENABLE_TX_QUEUE() \
    do{ \
        drv_edca_queue_halt(EDCA_AC_BK, FALSE); \
        drv_edca_queue_halt(EDCA_AC_BE, FALSE);  \
        drv_edca_queue_halt(EDCA_AC_VI, FALSE);  \
        drv_edca_queue_halt(EDCA_AC_VO, FALSE);  \
    }while(0)

#define DISABLE_TX_QUEUE() \
    do{ \
        drv_edca_queue_halt(EDCA_AC_BK, TRUE); \
        drv_edca_queue_halt(EDCA_AC_BE, TRUE);  \
        drv_edca_queue_halt(EDCA_AC_VI, TRUE);  \
        drv_edca_queue_halt(EDCA_AC_VO, TRUE);  \
    }while(0)
#endif

s32 MLME_Scan(struct cfg_host_cmd **_HostCmd);
s32 Scan_action(u8 work_type, void *data, u32 data_len);
s32 Scan_set_next_channel(void);
s32 Scan_process_result(void *data, u32 data_len,struct ieee802_11_elems *elems, u32 cmd_seq_no);
inline void Scan_PreProcess(void);
inline void Scan_PostProcess(void);

void Scan_FillWorkJobReq(Mlme_WorkJobReq *WorkJobReq, u32 size, struct cfg_host_cmd *hostCmd)
{
    struct cfg_scan_request *ScanReq=(struct cfg_scan_request *)(hostCmd->dat8);
    u32 cmd_seq_no=hostCmd->cmd_seq_no;

    /**
     * cmd.c_type and cmd.len are for reuse the pbuf of WorkJobReq.
     */
    WorkJobReq->cmd.c_type=HOST_CMD;
    WorkJobReq->cmd.len=size;

    WorkJobReq->cmd_seq_no = cmd_seq_no;
    WorkJobReq->u.scan.chidx = 1;     //scan from channel 1
    WorkJobReq->u.scan.staidx = 0  ;   //?????removeit
    WorkJobReq->u.scan.is_active = ScanReq->is_active;
    WorkJobReq->u.scan.n_ssids = ScanReq->n_ssids; //?????removeit
    WorkJobReq->u.scan.dwell_time = (ScanReq->dwell_time == 0)
                                    ? DEFAULT_CHANNEL_DWELL_TIME
                                    : (ScanReq->dwell_time * 10);
    WorkJobReq->u.scan.channel_mask = ScanReq->channel_mask;
    WorkJobReq->u.scan.last_scan_result = MLME_OK;
    WorkJobReq->u.scan.retry_times=0;
    //Avoid channel 0&15
    WorkJobReq->u.scan.channel_mask = WorkJobReq->u.scan.channel_mask&~(1<<0);
    WorkJobReq->u.scan.channel_mask = WorkJobReq->u.scan.channel_mask&~(1<<15);
    //copy ht capability ie
    //WorkJobReq->u.scan.ht_supported= ScanReq->ht_supported;
    memcpy(WorkJobReq->u.scan.ht_cap_ie,ScanReq->ht_cap_ie,sizeof(ScanReq->ht_cap_ie));

    //copy ssid
    if ((ScanReq->n_ssids > 0) && (ScanReq->ssids[0].ssid_len != 0))
    {
        if (ScanReq->ssids[0].ssid_len > MAX_SSID_LEN)
        {
            MLME_ERROR("Scan request has SSID too long %d.", ScanReq->ssids[0].ssid_len);
            ScanReq->n_ssids = 0;
            WorkJobReq->u.scan.n_ssids=0;
        }
        else
        {
            WorkJobReq->u.scan.n_ssids=ScanReq->n_ssids;
            WorkJobReq->u.scan.ssids[0].ssid_len = ScanReq->ssids[0].ssid_len;
            memcpy(WorkJobReq->u.scan.ssids[0].ssid, ScanReq->ssids[0].ssid, ScanReq->ssids[0].ssid_len);
        }
    }
    return;
}
void Scan_MonitorCCAStatus(void *data)
{
    Mlme_WorkJobReq *WorkJobReq;
    if(gDevInfo->MlmeReqParams == NULL){
        printf("Get a null pointer\n");
        return;
    }

    WorkJobReq = gDevInfo->MlmeReqParams;

    if(1==GET_RO_ED_STATE){
        gDevInfo->channel_edcca_count[ WorkJobReq->u.scan.chidx]++;
    }
    return;
}

inline void Scan_PreProcess(void)
{
    SetStaAction(STA_ACTION_SCANING);
}

s32 Scan_Done(void **ppMlmeReqParams)
{
    u32 size=0;
    HDR_HostEvent   *ev=NULL;
    u8 i=0;
    struct resp_evt_result *ScanDone=NULL;

    size=SCAN_DONE_EVT_HEADER_SIZE;

    HOST_EVENT_REALLOC_0(ppMlmeReqParams,ev,SOC_EVT_SCAN_DONE,size);

    HOST_EVENT_ASSIGN_EVT_NO(ev,mlme_cmd_seq_no);

    ScanDone = (struct resp_evt_result *)HOST_EVENT_DATA_PTR(ev);
    ScanDone->u.scan_done.result_code = 0;
    memcpy(ScanDone->u.scan_done.channel_edcca_count,gDevInfo->channel_edcca_count,MAX_CHANNEL_NUM*sizeof(u32));
    memcpy(ScanDone->u.scan_done.channel_packet_count,gDevInfo->channel_packet_count,MAX_CHANNEL_NUM*sizeof(u32));
    for(i=0;i<MAX_CHANNEL_NUM;i++)
    {
        gDevInfo->final_channel_edcca_count[i]+=gDevInfo->channel_edcca_count[i];
        gDevInfo->final_channel_packet_count[i]+=gDevInfo->channel_packet_count[i];
    }
    HOST_EVENT_SET_LEN(ev, size);
    HOST_EVENT_SEND(ev);
    printf("Scan Done\n");
    return MLME_OK;

}
inline void Scan_PostProcess(void)
{
    SetStaAction(STA_ACTION_READY);
    Scan_Done(&gDevInfo->MlmeReqParams);
    MLME_Cmd_Done(&gDevInfo->MlmeReqParams);
}

s32 Scan_set_next_channel(void)
{
    Mlme_WorkJobReq *WorkJobReq;
    s32 i;
    MLME_WORKJOB_STRUCT(WorkJobReq,gDevInfo);

    for(i=WorkJobReq->u.scan.chidx;i<MAX_CHANNEL_NUM;i++)
    {
        if((WorkJobReq->u.scan.channel_mask&(1<<i))==0)
        {
            continue;
        }
        WorkJobReq->u.scan.channel_mask=WorkJobReq->u.scan.channel_mask&~(1<<i);
        WorkJobReq->u.scan.chidx=i;
        break;
    }

    return 0;
}

/*-------------------------------
 *|HostEvent                        |
 *-------------------------------
 *|ScanResp    |Mgmt80211                |
 *------------------------------- */
s32 Scan_process_result(void *data, u32 data_len,struct ieee802_11_elems *elems, u32 cmd_seq_no)
{
    HDR_HostEvent   *ev=NULL;
    const PKT_RxInfo *PktInfo=(PKT_RxInfo *)data;
    const HDR80211_Mgmt *Mgmt80211=HDR80211_MGMT(data);
    struct resp_evt_result *ScanResp;
    u32 size;

    if(elems==NULL){
        MLME_WARN("Scan result parse error.\n");
        return MLME_FAILED;
    }

    // Send to cmd_engine
    size=SCAN_RESP_EVT_HEADER_SIZE+PktInfo->len;
    HOST_EVENT_ALLOC_1(ev,SOC_EVT_SCAN_RESULT , size);
    if(ev==NULL){
        printf("\33[31m%s:allocate pbuf fail\33[0m\r\n",__FUNCTION__);
        return MLME_FAILED;
    }
    HOST_EVENT_ASSIGN_EVT_NO(ev,cmd_seq_no);
    ScanResp = (struct resp_evt_result *)HOST_EVENT_DATA_PTR(ev);
    memset((void *)ScanResp,0,sizeof(struct resp_evt_result));
    //ScanResp=Pack_CMD_SEQ_NO((CMD_SEQ_CTRL *)ScanResp,cmd_seq_no);
    ScanResp->u.scan.result_code=CMD_PASS_FRAME;
    memcpy(ScanResp->u.scan.bss_info.bssid.addr,Mgmt80211->bssid.addr,ETHER_ADDR_LEN);
    ScanResp->u.scan.bss_info.capab_info=Mgmt80211->u.probe_resp.capab_info;
    ScanResp->u.scan.bss_info.rxphypad.rpci=*((u8 *)((u32)PktInfo+PktInfo->hdr_offset+PktInfo->len));
    ScanResp->u.scan.bss_info.rxphypad.snr=*((u8 *)((u32)PktInfo+PktInfo->hdr_offset+PktInfo->len+1));
    //printf("rpci=%d, snr=%d\r\n",ScanResp->u.scan.bss_info.rxphypad.rpci,ScanResp->u.scan.bss_info.rxphypad.snr);

    get_bss_info(elems, ScanResp);

    // Copy beacon/probe response frame for host.
    ScanResp->u.scan.dat_size = PktInfo->len;
    memcpy(&ScanResp->u.scan.dat[0], Mgmt80211, PktInfo->len);
    HOST_EVENT_SET_LEN(ev, size);
    HOST_EVENT_SEND(ev);

#if(CONFIG_LOG_ENABLE==1)
    {
    struct cfg_80211_ssid ssid;
    int    ssid_len = (elems->ssid_len > MAX_SSID_LEN) ? (MAX_SSID_LEN ) : elems->ssid_len;
    memcpy(ssid.ssid, elems->ssid, ssid_len);
    //ssid.ssid[ssid_len] = 0;
    }
#endif
    //SetStaInfoFlag(STA_INFOFLAG_READY);
    //PBUF_MFree(data);
    return MLME_OK;
}

s32 Scan_Channel_Change(s32 channel_id)
{
    s32 ret=0;
    if(TRUE==IS_STA_CONNECTED()){
        if(channel_id==gDevInfo->current_channel){
            ret=drv_phy_channel_change(channel_id,TRUE);
            mlme_send_null_frame(0);
            ENABLE_TX_QUEUE();
        }else{
            DISABLE_TX_QUEUE();
            if(-1==mlme_send_null_frame(1)){
                ENABLE_TX_QUEUE();
                return -1;
            }
            ret=drv_phy_channel_change(channel_id,TRUE);
        }
    }else{
        ret=drv_phy_channel_change(channel_id,TRUE);
    }

    return ret;

}

inline void Scan_Stop_Phy_Counter(void)
{
    SET_RG_MRX_LEN_LOWER_TH_0(0);
    SET_RG_MRX_LEN_UPPER_TH_0(0);
    SET_RG_MRX_LEN_CNT_EN_0(0); //clear RX counter 0
}

inline void Scan_Start_Phy_Counter(void)
{
    SET_RG_MRX_LEN_LOWER_TH_0(0);
    SET_RG_MRX_LEN_UPPER_TH_0(0x1FFF);
    SET_RG_MRX_LEN_CNT_EN_0(1); //clear RX counter 0
}

s32 Scan_action(u8 work_type, void *data, u32 data_len)
{
    Mlme_WorkJobReq *WorkJobReq;
    s32 ret = MLME_OK;
    MLME_WORKJOB_STRUCT(WorkJobReq,gDevInfo);
    WorkJobReq->work_type = work_type;

    switch (work_type) {
        case MLME_WORK_START_SCANNING:
            Scan_PreProcess();
            WorkJobReq->u.scan.is_scaning_phase=TRUE;
            WorkJobReq->work_type=MLME_WORK_CHANNEL_SETING;
        case MLME_WORK_CHANNEL_SETING:
            hwtmr_stop(US_TIMER0);
            gDevInfo->channel_packet_count[ WorkJobReq->u.scan.chidx]+=GET_RO_MRX_LEN_CNT_0;
            Scan_Stop_Phy_Counter();
            if((WorkJobReq->u.scan.channel_mask)&&(STA_ACTION_JOING_4WAY!=querySTAAction()))
            {
                //In connect status, scanning must fllow the interleave mode.
                //ch1 -> current channel -> ch2 ->current channel -> ch3 -> current channel .....
                //This mode is for sneding the data from TX queue.
                if((TRUE==IS_STA_CONNECTED())&&(FALSE==WorkJobReq->u.scan.is_scaning_phase)){
                    WorkJobReq->u.scan.is_scaning_phase=TRUE;
                    Scan_Channel_Change(gDevInfo->current_channel);
                    MLME_START_TIMER(&gDevInfo->MTimer, WorkJobReq->u.scan.dwell_time, MLME_WORK_ACTIVE_SCANING_TIMEOUT , NULL, NULL);
                }
                else{

                    WorkJobReq->u.scan.is_scaning_phase=FALSE;
                    if ((WorkJobReq->u.scan.last_scan_result == MLME_OK)||(WorkJobReq->u.scan.retry_times==5)){
                        WorkJobReq->u.scan.retry_times=0;
                        Scan_set_next_channel();
                    }else{
                        WorkJobReq->u.scan.retry_times++;
                        //printf("WorkJobReq->u.scan.retry_times=%d\r\n",WorkJobReq->u.scan.retry_times);
                    }
                    if(0==Scan_Channel_Change(WorkJobReq->u.scan.chidx)){
                        hwtmr_start(US_TIMER0,CCA_SAMPLE_PERIOD,Scan_MonitorCCAStatus,NULL,HTMR_PERIODIC);
                        Scan_Start_Phy_Counter();
                        Scan_action((WorkJobReq->u.scan.is_active)?MLME_WORK_PROBE_REQ:MLME_WORK_BEACON_LISTEN,NULL,0);
                    }
                    else{
                        WorkJobReq->u.scan.last_scan_result=MLME_FAILED;
                        Scan_action(MLME_WORK_CHANNEL_SETING,NULL,0);
                    }
                }
            }
            else{
                Scan_action(MLME_WORK_END_SCANNING,NULL,0);
            }

            break;
       case MLME_WORK_PROBE_REQ:
            ret = mlme_send_probe_req(NULL,WorkJobReq->u.scan.n_ssids,&WorkJobReq->u.scan.ssids[0]);
            WorkJobReq->u.scan.last_scan_result = ret;
            Scan_action(MLME_WORK_PROBE_RESP,NULL,0);
            break;
       case MLME_WORK_PROBE_RESP:
            MLME_START_TIMER(&gDevInfo->MTimer, WorkJobReq->u.scan.dwell_time, MLME_WORK_ACTIVE_SCANING_TIMEOUT , NULL, NULL);
            break;
       case MLME_WORK_BEACON_LISTEN:
            MLME_START_TIMER(&gDevInfo->MTimer, WorkJobReq->u.scan.dwell_time, MLME_WORK_PASSIVE_SCANING_TIMEOUT , NULL, NULL);
            break;
        case MLME_WORK_END_SCANNING:
            Scan_Channel_Change(gDevInfo->current_channel);
            Scan_PostProcess();
            break;
        default:
            /*lint -save -e774 */
            ASSERT_RET(0, MLME_FAILED);
            /*lint -restore */
    }
    return ret;
}


s32 MLME_Scan( struct cfg_host_cmd **_HostCmd)
{
    Mlme_WorkJobReq *WorkJobReq=NULL;
    size_t size;
    u8 err=0;

    gDevInfo->MlmeReqParams=NULL;

    /* Allocate buffer for storing WorkJobReq */
    size = sizeof(Mlme_WorkJobReq) + sizeof(struct cfg_80211_ssid);

    WorkJobReq = (Mlme_WorkJobReq * )PBUF_MAlloc(size,SOC_PBUF);
    if (WorkJobReq == NULL)
    {
        printf("\33[31m%s:allocate pbuf fail\33[0m\r\n",__FUNCTION__);
        err++;
    }

    if(STA_STATE_UNAUTH_UNASSOC!=querySTAState())
    {
        printf("\33[31m%s:STA is not on the leave status \33[0m\r\n",__FUNCTION__);
        err++;
    }

    if (err!=0)
    {
        if(WorkJobReq!=NULL)
        {
            PBUF_MFree(WorkJobReq);
        }

        memset(gDevInfo->channel_edcca_count,0,sizeof(gDevInfo->channel_edcca_count));
        memset(gDevInfo->channel_packet_count,0,sizeof(gDevInfo->channel_packet_count));

        Scan_Done(&gDevInfo->MlmeReqParams);
        MLME_Cmd_Done(&gDevInfo->MlmeReqParams);
        return MLME_FAILED;
    }

    Scan_FillWorkJobReq(WorkJobReq,size,*_HostCmd);

    gDevInfo->MlmeReqParams = WorkJobReq;
    memset(gDevInfo->channel_edcca_count,0,sizeof(gDevInfo->channel_edcca_count));
    memset(gDevInfo->channel_packet_count,0,sizeof(gDevInfo->channel_packet_count));

    Scan_action(MLME_WORK_START_SCANNING, NULL, 0);

    return MLME_OK;
}

