#include <config.h>
#include <msgevt.h>
#include <hdr80211.h>
#include <pbuf.h>
#include <log.h>

#include "mlme.h"
#include "MLME_Task.h"
#include "MLME_Scan.h"
#include "MLME_Join.h"
#include "MLME_Leave.h"
#include "MLME_PwrSaving.h"
#include "sta_info.h"
#include "supplicant/drivers/driver_cabrio.h"
#include <hwmac/drv_mac.h>
#include <edca/drv_edca.h>
#include <mbox/drv_mbox.h>
#include <dbg_timer/dbg_timer.h>
#include <beacon/drv_beacon.h>
#include "SmartConfig.h"

u32 mlme_cmd_seq_no;
bool mlme_cmd_running=FALSE;

#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
extern u8 supplicant_bypass;
#endif

bool gBeaconListening=TRUE;
void MLME_TimeOutHandle(u32 subType);
s32 MLME_add_background_task (void (*task_func)(void *), void *task_data);
s32 MLME_TxManageFrame(struct cfg_host_cmd **_HostCmd );
//s32 MLME_ADDBA_RESP(struct cfg_addba_resp  *cfg_addba_resp, u32 cmd_seq_no );
//s32 MLME_DELBA(struct cfg_delba  *cfg_delba, u32 cmd_seq_no );
s32 MLME_RxManageFrame( PKT_RxInfo **PktInfo );
#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
s32 MLME_RxEapolFrame( PKT_RxInfo *pPktInfo );
#endif
void Supplicant_Notify_MLME_4way(u8 is_sucessful);
extern s32 Join_action(u8 work_type, void *data, u32 data_len);

void Supplicant_Notify_MLME_4way(u8 is_sucessful)
{

    if(is_sucessful)
    {
        MLME_TRACE("WPA 4-way handshake is success.\n");
        gDevInfo->State=STA_STATE_ASSOCED_4way;
        MLME_STOP_TIMER(gDevInfo->MTimer.MTimer);
        Join_process_result(EN_JOIN_RESULT_SUCCESS);
    }
    else
    {
        MLME_TRACE("WPA 4-way handshake is failed.\n");
        Join_process_result(EN_JOIN_RESULT_WRONG_PASSWORD);
    }
}

void MLME_Cmd_Done(void **ppMlmeReqParams)
{
    HDR_HostEvent    *ev = NULL;
    if(TRUE==mlme_cmd_running)
    {
        HOST_EVENT_REALLOC_0(ppMlmeReqParams,ev,SOC_EVT_MLME_CMD_DONE,0);
        HOST_EVENT_ASSIGN_EVT_NO(ev, mlme_cmd_seq_no);
        HOST_EVENT_SET_LEN(ev,0);
        HOST_EVENT_SEND(ev);
        mlme_cmd_running=FALSE;
        printf("\33[35m MLME_Cmd_Done():%d \33[0m\r\n",mlme_cmd_seq_no);
    }
}


s32 MLME_TxManageFrame(struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    mlme_cmd_seq_no=HostCmd->cmd_seq_no;
    if(TRUE==mlme_cmd_running)
    {
        printf("\33[31m MLME cmd is still running \33[0m\r\n");
        ASSERT(TRUE);
    }
    mlme_cmd_running=TRUE;
    printf("\33[35m MLME_TxManageFrame():%d \33[0m\r\n",mlme_cmd_seq_no);
    switch(HostCmd->h_cmd)
    {
        case SSV6XXX_HOST_CMD_SCAN:
            MLME_Scan(_HostCmd);
            break;
        case SSV6XXX_HOST_CMD_JOIN:
            MLME_Join(_HostCmd);
            break;
        case SSV6XXX_HOST_CMD_LEAVE:
            MLME_Leave(_HostCmd);
            break;
#ifdef AIRKISS_ENABLE
        case SSV6XXX_HOST_CMD_SMART_CONFIG:
            MLME_SConfig(_HostCmd);
            break;
#endif
        case SSV6XXX_HOST_CMD_SET_PWR_SAVING:
            mlme_cmd_running=false;
            MLME_PwrSaving((void*)HostCmd->dat8);
            break;
        default:
            break;
    }

#if 0
    if(HostCmd->h_cmd == SSV6XXX_HOST_CMD_ADDBA_RESP)
        MLME_ADDBA_RESP((struct cfg_addba_resp *)HostCmd->dat8, HostCmd->cmd_seq_no);
    if(HostCmd->h_cmd == SSV6XXX_HOST_CMD_DELBA)
        MLME_DELBA((struct cfg_delba *)HostCmd->dat8, HostCmd->cmd_seq_no);
#endif
    return OS_SUCCESS;
}

void MLME_TimeOutHandle(u32 subType)
{
    if (subType==MLME_WORK_ACTIVE_SCANING_TIMEOUT||subType==MLME_WORK_PASSIVE_SCANING_TIMEOUT)
    {
        //?????if the sta's state is connected,
        Scan_action(MLME_WORK_CHANNEL_SETING,NULL,0);
    }

    if ((subType==MLME_WORK_AUTH_TIMEOUT)&&(gDevInfo->State!=STA_STATE_AUTHED_UNASSOC))
    {
        printf("Authenticate time out ... \n");
        Join_action(MLME_WORK_JOIN_AUTH_FAIL,NULL,0);
    }

    //20141028 EricYu: For Join timer
    if ((subType==MLME_WORK_ASSOC_TIMEOUT)&&(gDevInfo->State!=STA_STATE_AUTHED_ASSOCED))
    {
        printf("Associate time out ... \n");
        Join_action(MLME_WORK_JOIN_ASSOC_FAIL,NULL,0);
    }


    if ((subType==MLME_WORK_4way_TIMEOUT)&&(gDevInfo->State!=STA_STATE_ASSOCED_4way))
    {
        printf("4way time out ... \n");
        Join_action(MLME_WORK_JOIN_4WAY_FAIL,NULL,0);
    }
#ifdef AIRKISS_ENABLE
    if (subType==MLME_WORK_SMART_CONFIG_SCAN_TIMEOUT){
        SConfig_action(MLME_WORK_SMART_CONFIG_CHANNEL_SETTING,NULL,0);
    }
    else if (subType==MLME_WORK_SMART_CONFIG_LOCK_TIMEOUT){
        SConfig_action(MLME_WORK_SMART_CONFIG_LOCK,NULL,0);
    }
    else if(subType==MLME_WORK_SMART_CONFIG_PROBE_TIMEOUT){
        SConfig_action(MLME_WORK_SMART_CONFIG_DONE,NULL,0);
    }
#endif
	return;
}

s32 MLME_RxManageFrame( PKT_RxInfo **PktInfo )
{
    const HDR80211_Mgmt *Mgmt80211=HDR80211_MGMT((*PktInfo));
    u16 subtype=Mgmt80211->fc.subtype;

    switch(drv_mac_get_op_mode())
    {
    case WLAN_STA:
    case WLAN_AP:

        if (MGMT_RxHandler[subtype] == NULL) {
            return OS_SUCCESS;
        }
        MGMT_RxHandler[subtype]((PktInfo));

        break;
#if 0
    case WLAN_AP:

        {
        DevInfo *StaInfo;
        StaInfo = STAInfo_FindSTAByAddr(&Mgmt80211->sa);
        ...
        }
#endif
    case WLAN_IBSS:
        /*lint -fallthrough */
    case WLAN_WDS:
        /*lint -fallthrough */
    default:
        /*lint -save -e774 */
        ASSERT_RET(0, OS_FAILED);
        /*lint -restore */
    }

    return OS_SUCCESS;
}


#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
s32 MLME_RxEapolFrame( PKT_RxInfo *pPktInfo )
{
    u8              *dat;
    PKT_RxInfo      *PktRxInfo=(PKT_RxInfo *)pPktInfo;
    ETHER_ADDR      peer_mac;
    u16             eapol_data_len;

    GET_BSSID(&peer_mac);
    //ssv6xxx_raw_dump((u8 *)PktRxInfo,pPktInfo->len + PktRxInfo->payload_offset);

    dat =(u8 *)PktRxInfo + PktRxInfo->payload_offset + ETHER_HDR_LEN;

    if(PktRxInfo->qos)
    {
        eapol_data_len = pPktInfo->len - ETHER_HDR_LEN - IEEE80211_QOS_CTL_LEN;
    }
    else
    {
        eapol_data_len = pPktInfo->len - ETHER_HDR_LEN;
    }

    wpa_supplicant_rx_eapol(gDevInfo->pSupplicant/*sta_info->pSupplicant*/,
                            peer_mac.addr, dat,eapol_data_len);

    return OS_SUCCESS;
}
#endif // _INTERNAL_MCU_SUPPLICANT_SUPPORT_



s32 MLME_add_background_task (void (*task_func)(void *), void *task_data)
{
    MsgEvent *msg = msg_evt_alloc();
    if (msg == NULL)
        return MLME_FAILED;

    msg->MsgType = MEVT_BKG_TASK;
    msg->MsgData = (u32)task_func;
    msg->MsgData1 = (u32)task_data;

    return msg_evt_post(MBOX_MLME, msg);
} // end of - MLME_add_background_task -


extern s32 MLME_Leave_Resp (void **_PktInfo, bool timeout, s16 reason);
void MLME_DetectAP(void)
{
    #define POKE_AP_MAX_CNT 40
    
    SSV6XXX_STA		*sta=gDevInfo->connected_sta_ptr;
    PKT_Info *pKtInfo=NULL;

    if(sta!=NULL){
        if((sta->ap_heartbeat_cnt==sta->last_ap_heartbeat_cnt)&&(sta->poke_ap==POKE_AP_MAX_CNT))
        {
            printf("AP is not alive...(HB=%d,Last HB=%d)\n",sta->ap_heartbeat_cnt,sta->last_ap_heartbeat_cnt);
            ssv6xxx_queue_status();
            MLME_Leave_Resp((void **)&pKtInfo, 1,4); //#define WLAN_REASON_DISASSOC_DUE_TO_INACTIVITY 4
        }
        else
        {
            sta->poke_ap++;
            printf("poke ap by probe reques (%d)\n",sta->poke_ap);
            if(MLME_FAILED==mlme_send_probe_req(sta->addr.addr,1,&sta->ssid))
            {
                ssv6xxx_queue_status();
            }
        }

    }
}

extern void MLME_ps_beacon_sync_timeout(u32 tdata);
extern void MLME_host_wakeup_timeout(void);
void MLME_Task( void *args )
{
    PKT_RxInfo *PktInfo;
    MsgEvent *MsgEv;
    s32 res;

    SetStaAction(STA_ACTION_READY);
    /*lint -save -e716 */
    while(1)
    { /*lint -restore */
        /* Wait Message: */
        res = msg_evt_fetch(MBOX_MLME, MAX_DELAY, &MsgEv);
        ASSERT(res == OS_SUCCESS);
        PktInfo = (PKT_RxInfo *)MsgEv->MsgData;
        /*lint -save -e788 */
        gMLMETaskCount++;
        switch(MsgEv->MsgType)
        {
            case MEVT_PKT_BUF:
                msg_evt_free(MsgEv);
                if (PktInfo->c_type == HOST_CMD)
                {
                    MLME_TxManageFrame((struct cfg_host_cmd **)&PktInfo);
                }
                else
                {
                    MLME_RxManageFrame(&PktInfo);
                }

                if(PktInfo!=NULL)
                {
                    PBUF_MFree(PktInfo);
                }

                break;
#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
            case MEVT_EAPOL:
                msg_evt_free(MsgEv);
                MLME_RxEapolFrame(PktInfo);
                PBUF_MFree(PktInfo);
                break;
#endif // _INTERNAL_MCU_SUPPLICANT_SUPPORT_
            case MEVT_SOFT_TIMER:
                MLME_TimeOutHandle(MsgEv->MsgData);
                break;
            case MEVT_BKG_TASK:
                {
                    void (*task_func)(void *) = (void (*)(void *))MsgEv->MsgData;
                    void *task_data = (void *)MsgEv->MsgData1;
                    msg_evt_free(MsgEv);
                    if (task_func != NULL)
                    {
                        (*task_func)(task_data);
                    }
                }
                break;
            case MEVT_SMART_CFG:
                msg_evt_free(MsgEv);
#ifdef AIRKISS_ENABLE
                SConfig_Rx_Handler(PktInfo);
#endif
                PBUF_MFree(PktInfo);
                break;
            case MEVT_WAKEUP_TASK:
                //printf("mlme wakeup\n");
                msg_evt_free(MsgEv);
                break;
            case MEVT_DETECT_AP:
                msg_evt_free(MsgEv);
                MLME_DetectAP();
                break;                
            case MEVT_POWER_SAVING:
                switch((u32)PktInfo)
                {
                    case PS_EVT_BCN_SYNC_HTMR:
                    case PS_EVT_BCN_SYNC_STMR:
                        MLME_ps_beacon_sync_timeout((u32)PktInfo);
                        break;                        
                    case PS_EVT_WOW:
                        MLME_ps_wow((PKT_Info *)MsgEv->MsgData1);
                        break;
                    case PS_EVT_HOST_WAKEUP_TIMOUT:
                        MLME_host_wakeup_timeout();
                        break;
                }
                msg_evt_free(MsgEv);
                break;
            default:
                /* Invalid Message Type, drop the frame ! */
                msg_evt_free(MsgEv);
                PBUF_MFree(PktInfo);
                break;
        }
        /*lint -restore */
    }
}


