#include <config.h>
#include <msgevt.h>
#include <hdr80211.h>
#include <pbuf.h>
#include <log.h>

#include "mlme.h"
#include "MLME_Task.h"
#include "MLME_Join.h"
#include "psk_cache.h"
#include "sta_info.h"
#include "supplicant/drivers/driver_cabrio.h"
#include <hwmac/drv_mac.h>
#include <edca/drv_edca.h>
#include <dbg_timer/dbg_timer.h>
#include <beacon/drv_beacon.h>
#include <mbox/drv_mbox.h>

#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
extern u8 supplicant_bypass;
#endif


s32 MLME_Join( struct cfg_host_cmd **_HostCmd);
void _apply_join_req_wsid (u32 wsid_index, const Mlme_WorkJobReq *WorkJobReq);
s32 _Join_process_result(s32 status_code);
s32 Join_process_result(s32 status_code);
s32 Join_action(u8 work_type, void *data, u32 data_len);
s32 mlme_config_wpa_suppliciant(struct cfg_join_request *JoinReq);

void Join_FillWorkJobReq(Mlme_WorkJobReq *WorkJobReq, u32 size,struct cfg_host_cmd *HostCmd)
{
    struct cfg_join_request *JoinReq=(struct cfg_join_request *)(HostCmd->dat8);

    /**
     * cmd.c_type and cmd.len are for reuse the pbuf of WorkJobReq.
     */
    WorkJobReq->cmd.c_type=HOST_CMD;
    WorkJobReq->cmd.len=size;

    WorkJobReq->cmd_seq_no=HostCmd->cmd_seq_no;
    WorkJobReq->u.join.tries= 0;
    WorkJobReq->u.join.auth_transaction=1;
    WorkJobReq->u.join.assoc_ie_len = JoinReq->assoc_ie_len;
    if (JoinReq->assoc_ie_len > 0)
    {
        memcpy(WorkJobReq->u.join.assoc_ie, JoinReq->assoc_ie, JoinReq->assoc_ie_len);
    }
    memcpy(&WorkJobReq->u.join.bss, &JoinReq->bss, sizeof(struct ssv6xxx_ieee80211_bss));

    WorkJobReq->u.join.ht_supported= JoinReq->ht_supported;
    memcpy(WorkJobReq->u.join.ht_cap_ie, JoinReq->ht_cap_ie, sizeof(JoinReq->ht_cap_ie));


    //printf("pairwise_cipher=[%x] group_cipher=[%x]\n",JoinReq->bss.pairwise_cipher[0],JoinReq->bss.group_cipher);
    LOG_PRINTF("\tdtim_period: %d\n",JoinReq->bss.dtim_period);
    LOG_PRINTF("\twmm_used: %d\n",JoinReq->bss.wmm_used);
    LOG_PRINTF("\tBSSID: %02X:%02X:%02X:%02X:%02X:%02X\n",
               WorkJobReq->u.join.bss.bssid.addr[0], WorkJobReq->u.join.bss.bssid.addr[1],
               WorkJobReq->u.join.bss.bssid.addr[2], WorkJobReq->u.join.bss.bssid.addr[3],
               WorkJobReq->u.join.bss.bssid.addr[4], WorkJobReq->u.join.bss.bssid.addr[5]);
    LOG_PRINTF("\tie_len: %d\n",JoinReq->assoc_ie_len);

    return;
}
void _apply_join_req_wsid (u32 wsid_index, const Mlme_WorkJobReq *WorkJobReq)
{
    mwsid_entry wen;
    //set
    memcpy(&wen.sta_mac.addr[0],WorkJobReq->u.join.bss.bssid.addr, ETHER_ADDR_LEN);
    wen.info=5;
    drv_mac_set_wsid_table(wsid_index, &wen);
} // end of - apply_join_req_wsid -

#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
#ifdef ENABLE_BACKGROUND_PMK_CALC
void mlme_supplicant_key_calc_task (void *data)
{
    struct wpa_supplicant *wpa_s = (struct wpa_supplicant *)data;
    if (wpa_s->pmk_calc_task == NULL)
        return;

    if ((*wpa_s->pmk_calc_task)(wpa_s->pmk_calc_data))
        MLME_add_background_task(mlme_supplicant_key_calc_task, data);
}
#endif // ENABLE_BACKGROUND_PMK_CALC
#endif

s32 mlme_config_wpa_suppliciant(struct cfg_join_request *JoinReq)
{

    struct wpa_ssid *pWPASSID = &gDevInfo->wpa_ssid;
#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
    int cipher = WPA_CIPHER_NONE;
#endif
    int key_mgmt = WPA_KEY_MGMT_NONE;
    int proto = WPA_PROTO_WPA;
    //int auth_alg = WPA_AUTH_ALG_OPEN;
    //Reset
    memset(pWPASSID, 0, sizeof(struct wpa_ssid));
    //Fill config info

    memcpy(&pWPASSID->bssid[0], JoinReq->bss.bssid.addr, ETH_ALEN);
    memset((void*)&pWPASSID->ssid[0],0,MAX_SSID_LEN);
    memcpy(&pWPASSID->ssid[0], JoinReq->bss.ssid.ssid, JoinReq->bss.ssid.ssid_len);
    //memset((void*)pWPASSID->passphrase,0,MAX_PASSWD_LEN+1);
    pWPASSID->ssid_len = JoinReq->bss.ssid.ssid_len;
    printf("sec_type=[%d] pairwise_cipher=[%x] group_cipher=[%x]\n",JoinReq->sec_type,JoinReq->bss.pairwise_cipher[0],JoinReq->bss.group_cipher);
    printf("ssid=%s\n",pWPASSID->ssid);
    printf("password=%s\n",JoinReq->password);
    
    switch(JoinReq->sec_type)
    {
        case SSV6XXX_SEC_NONE:
            //key_mgmt = WPA_KEY_MGMT_NONE;
            //cipher = WPA_CIPHER_NONE;
            drv_mac_set_rx_ether_trap(0, 0);
            break;
        case SSV6XXX_SEC_WEP_40:
            memcpy(&pWPASSID->wep_key[JoinReq->wep_keyidx][0], JoinReq->password, 5);//Wep40 password
            pWPASSID->wep_key_len[JoinReq->wep_keyidx] = 5;
            pWPASSID->wep_tx_keyidx = JoinReq->wep_keyidx;
            //key_mgmt = WPA_KEY_MGMT_NONE;
#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
            cipher = WPA_CIPHER_WEP40;
#endif
            break;

        case SSV6XXX_SEC_WEP_104:
            memcpy(&pWPASSID->wep_key[JoinReq->wep_keyidx][0], JoinReq->password, 13);//Wep104 password length
            pWPASSID->wep_key_len[JoinReq->wep_keyidx] = 13;
            pWPASSID->wep_tx_keyidx = JoinReq->wep_keyidx;
            //key_mgmt = WPA_KEY_MGMT_NONE;
#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
            cipher = WPA_CIPHER_WEP104;
#endif
            break;

        case SSV6XXX_SEC_WPA_PSK:
            memcpy(&pWPASSID->passphrase[0], &JoinReq->password[0], strlen((char *)JoinReq->password));
            key_mgmt = WPA_KEY_MGMT_PSK;
            proto = WPA_PROTO_WPA;
#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
            cipher = WPA_CIPHER_TKIP;
            if(supplicant_bypass)
                ;
            else
            {
                //felix: disable this code due to that the ether-trapping may set when system init
                drv_mac_set_rx_ether_trap(0,EAPOL_ETHER_TYPPE);
            }
#endif
            break;
        case SSV6XXX_SEC_WPA2_PSK:
            {
                const u8 *psk = find_psk_in_cache(&pWPASSID->ssid[0], JoinReq->password);
                if (psk != NULL) {
                    memcpy(pWPASSID->psk, psk, sizeof(pWPASSID->psk));
                     // Pass NULL passphrase to make pWPASSID->psk be used.
                    pWPASSID->passphrase[0] = 0;
                } else
                    memcpy(pWPASSID->passphrase, JoinReq->password, strlen((char *)JoinReq->password));
                key_mgmt = WPA_KEY_MGMT_PSK;
                proto = WPA_PROTO_RSN;
#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
                cipher = WPA_CIPHER_CCMP;
                if(supplicant_bypass)
                    ;
                else
                {
                    //felix: disable this code due to that the ether-trapping may set when system init
                    drv_mac_set_rx_ether_trap(0,EAPOL_ETHER_TYPPE);
                }
#endif
            }
            break;
        case SSV6XXX_SEC_WPS:
        //    break;
        default:
            MLME_ERROR("Unsuppored security type %d\n", JoinReq->sec_type);
            return MLME_FAILED;
    }


	//20141028 EricYu: for WPA & WPA2 join issue
    /*
        pWPASSID->pairwise_cipher = cipher;
        pWPASSID->group_cipher = cipher;
    */
    pWPASSID->pairwise_cipher = JoinReq->bss.pairwise_cipher[0];
    pWPASSID->group_cipher = JoinReq->bss.group_cipher;
    pWPASSID->key_mgmt = key_mgmt;
    pWPASSID->proto = proto;
    pWPASSID->auth_alg = JoinReq->auth_alg;

    //int wpa_supplicant_set_suites(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid, u8 *wpa_ie, size_t *wpa_ie_len)
    gDevInfo->wpa_ie_len = STA_WPA_DEFAULT_IE_LEN;

#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
	if(supplicant_bypass)
	{
		gDevInfo->wpa_ie_len = 0;
	}
	else
	{
	    //tmp-mark-supplicant
	    wpa_supplicant_clear_status(gDevInfo->pSupplicant);
	    switch(cipher)
	    {
	        case WPA_CIPHER_WEP40:
	        case WPA_CIPHER_WEP104:
	            if(wpa_supplicant_set_key(gDevInfo->pSupplicant, WPA_ALG_WEP,
	                                      (const u8*)&pWPASSID->bssid[0],
	                                       pWPASSID->wep_tx_keyidx, 2,
	                                       (const u8 *)NULL, 0,
	                                       pWPASSID->wep_key[pWPASSID->wep_tx_keyidx],
	                                       pWPASSID->wep_key_len[pWPASSID->wep_tx_keyidx]))
	            {
	                MLME_ERROR( "wpa_supplicant_set_key failed\n");
	            }

	            break;
	        case WPA_CIPHER_TKIP:
	        case WPA_CIPHER_CCMP:
	            if(wpa_supplicant_set_suites(gDevInfo->pSupplicant, pWPASSID,
	                                         gDevInfo->wpa_ie,
	                                         (size_t *)&gDevInfo->wpa_ie_len))
	            {
	                MLME_ERROR( "wpa_supplicant_set_suites failed  sta_info->wpa_ie_len:%d\n", gDevInfo->wpa_ie_len);
	            }
	            #ifdef ENABLE_BACKGROUND_PMK_CALC
	            if (gDevInfo->pSupplicant->pmk_calc_task != NULL)
	                MLME_add_background_task(mlme_supplicant_key_calc_task,
	                                         gDevInfo->pSupplicant);
	            #endif // ENABLE_BACKGROUND_PMK_CALC
	            break;
	        case WPA_CIPHER_NONE:
	        	gDevInfo->wpa_ie_len = 0;
	        default:
	            break;
	    }
	}
#endif
    MLME_TRACE("Config WPA supplicant done.\n");
    return 0;
}

#define INT_MAX ((int)(~0U>>1))
extern SSV6XXX_STA *g6200StaInfo;
inline void get_basic_rate(Mlme_WorkJobReq *WorkJobReq)
{
    //BSS_CHANGED_BASIC_RATES

    int min_rate_index = -1;
    bool have_higher_than_11mbit;
    int min_rate = INT_MAX;
#if 1
    assoc_req_rate.rate_set_len=
        ieee80211_get_rates((u8*)&WorkJobReq->u.join.bss.supp_rates,
                            WorkJobReq->u.join.bss.supp_rates_len,
                            &assoc_req_rate.rate_set,
                            &assoc_req_rate.basic_rate_set,
                            &have_higher_than_11mbit,
                            &min_rate, &min_rate_index);

/*
     * This used to be a workaround for basic rates missing
     * in the association response frame. Now that we no
     * longer use the basic rates from there, it probably
     * doesn't happen any more, but keep the workaround so
     * in case some *other* APs are buggy in different ways
     * we can connect -- with a warning.
     */
    if (!assoc_req_rate.basic_rate_set && min_rate_index >= 0) {
        printf("Warrning: No basic rates, using min rate instead.\n");
        assoc_req_rate.basic_rate_set = BIT(min_rate_index);
    }

#else
    gDevInfo->connected_sta_ptr=&g6200StaInfo[0];

    gDevInfo->connected_sta_ptr->rate_set_len=
                   ieee80211_get_rates((u8*)&WorkJobReq->u.join.bss.supp_rates,
				    WorkJobReq->u.join.bss.supp_rates_len,
                    &gDevInfo->connected_sta_ptr->rate_set,
                    &gDevInfo->connected_sta_ptr->basic_rate_set,
				    &have_higher_than_11mbit,
				    &min_rate, &min_rate_index);

    /*
     * This used to be a workaround for basic rates missing
     * in the association response frame. Now that we no
     * longer use the basic rates from there, it probably
     * doesn't happen any more, but keep the workaround so
     * in case some *other* APs are buggy in different ways
     * we can connect -- with a warning.
     */
    if (!gDevInfo->connected_sta_ptr->basic_rate_set && min_rate_index >= 0) {
        printf("Warrning: No basic rates, using min rate instead.\n");
        gDevInfo->connected_sta_ptr->basic_rate_set = BIT(min_rate_index);
    }
#endif

}

inline void set_basic_rate(void)
{
    //printf("ssv6xxx_rc_update_basic_rate: basic rate =0x%x , rate = 0x%x, rate_len =%d \n",gDevInfo->connected_sta_ptr->basic_rate_set,gDevInfo->connected_sta_ptr->rate_set,gDevInfo->connected_sta_ptr->rate_set_len);
}

s32 MLME_Join( struct cfg_host_cmd **_HostCmd)
{
    struct cfg_join_request *JoinReq=(struct cfg_join_request *)(((struct cfg_host_cmd *)(*_HostCmd))->dat8);
    Mlme_WorkJobReq *WorkJobReq=NULL;
    size_t size;

    gDevInfo->MlmeReqParams=NULL;

    LOG_MODULE_TURN_ON(LOG_MODULE_MLME);
    LOG_LEVEL_SET(LOG_LEVEL_ON);

#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
	supplicant_bypass = JoinReq->request_supplicant_bypass;
#endif

    /* Allocate buffer for storing WorkJobReq */
    size = sizeof(Mlme_WorkJobReq) + JoinReq->assoc_ie_len;
    WorkJobReq = (Mlme_WorkJobReq * )PBUF_MAlloc(size, SOC_PBUF);
    if(WorkJobReq==NULL)
    {
        printf("\33[31m%s:allocate pbuf fail\33[0m\r\n",__FUNCTION__);
        _Join_process_result(1);
        SetStaAction(STA_ACTION_READY);
        MLME_Cmd_Done((void **)_HostCmd);
        return -1;
    }

    Join_FillWorkJobReq(WorkJobReq,size,*_HostCmd);

    drv_phy_channel_change(JoinReq->bss.channel_id,TRUE);
	gDevInfo->current_channel=JoinReq->bss.channel_id;

    SetStaAction(STA_ACTION_JOING);
    drv_mac_set_bssid(JoinReq->bss.bssid.addr);

    gDevInfo->State = STA_STATE_UNAUTH_UNASSOC;
    //sta_info->wpa_ie_len = STA_WPA_DEFAULT_IE_LEN;

    // WPA Supplicant config could take long time. Guard MLME from traffic.
    if (mlme_config_wpa_suppliciant(JoinReq) != MLME_OK)
    {
        return MLME_FAILED;
    }

    gDevInfo->MlmeReqParams=WorkJobReq;

    //set basic rate
    get_basic_rate(WorkJobReq); /*reference both AP & STA support rate*/
    set_basic_rate();/*mark this function will meet ack_policy=2 problem (RB-76)*/

    _apply_join_req_wsid(0, WorkJobReq);

    ssv6xxx_SWQIntUnLock(0,TX_SW_QUEUE);
    ssv6xxx_SWQOutUnLock(0,TX_SW_QUEUE);
    drv_edca_queue_halt(EDCA_AC_BK+0, FALSE);
    Join_action(MLME_WORK_JOIN_TX_AUTH_1, NULL, 0);

    return MLME_OK;
}

//Fill JoinResp,status_code=0 meas join sucess
s32 _Join_process_result(s32 status_code)
{
    HDR_HostEvent   *ev=NULL;
    struct resp_evt_result *JoinResp;
    u32 size;

    // Send to cmd_engine
    size = sizeof(struct resp_evt_result);

    HOST_EVENT_ALLOC_0(ev, SOC_EVT_JOIN_RESULT, size);

    JoinResp = (struct resp_evt_result *)HOST_EVENT_DATA_PTR(ev);

    HOST_EVENT_ASSIGN_EVT_NO(ev, mlme_cmd_seq_no);


    JoinResp->u.join.status_code = status_code;
    if(status_code==1)
        JoinResp->u.join.aid = 0;
    else
        JoinResp->u.join.aid = gDevInfo->connected_sta_ptr->aid;

    HOST_EVENT_SET_LEN(ev,size);
    HOST_EVENT_SEND(ev);

    return 0;
}
s32 Join_process_result(s32 status_code)
{
    //This condiction should not be removed, because wpa_supplicant call this function when 4-way and 2-way is finished.
    //If we revmoe this checking, fw will send the join sucess of "HOST_EVENT" to host when AP do the 2-way handshake on connecting phase.
    if (gDevInfo->MlmeReqParams == NULL)
    {
        return MLME_OK;
    }
    _Join_process_result(status_code);

    if(status_code != EN_JOIN_RESULT_SUCCESS)
    {
        _mlme_sta_disconnect(0);
    }
    else
    {
        add_psk_to_cache(gDevInfo->pSupplicant);
        StartStaMonitorTimer();
    }

    MLME_INFO("JoinResp->aid=%d, status: %d\n", JoinResp->u.join.aid, status_code);

    //Change CMD_ENGINE STATE
    SetStaAction(STA_ACTION_READY);
    MLME_Cmd_Done(&gDevInfo->MlmeReqParams);
    return MLME_OK;
}

s32 Join_action(u8 work_type, void *data, u32 data_len)
{
#define AUTH_TIME_OUT 1000
#define ASSOC_TIME_OUT 1000
#define SUPPLICANT_4WAY_TIME_OUT 5000

    Mlme_WorkJobReq *WorkJobReq;
    MLME_WORKJOB_STRUCT(WorkJobReq,gDevInfo);

    WorkJobReq->work_type = work_type;
    switch (WorkJobReq->work_type) {
    case MLME_WORK_JOIN_TX_AUTH_1:
        printf("Sending authentication request...\n");
        mlme_send_auth(1, (u32 *)data, data_len);
        break;
    case MLME_WORK_JOIN_RX_AUTH_2:
        MLME_TRACE("Waiting authentication reponse...\n");
        MLME_START_TIMER(&gDevInfo->MTimer, AUTH_TIME_OUT, MLME_WORK_AUTH_TIMEOUT , NULL, NULL);
        break;
    case MLME_WORK_JOIN_TX_AUTH_3:
        printf("Sending challenge response ...\n");
        //WorkJobReq->u.probe_auth.auth_transaction = 3;
        mlme_send_auth(3, (u32 *)data, data_len);
        //WorkJobReq->u.probe_auth.auth_transaction = 4;
        break;
    case MLME_WORK_JOIN_RX_AUTH_4:
        //20141028 EricYu: For join timer
        MLME_START_TIMER(&gDevInfo->MTimer, AUTH_TIME_OUT, MLME_WORK_AUTH_TIMEOUT , NULL, NULL);

        break;
    case MLME_WORK_JOIN_ASSOC_REQ:
        printf("Sending associate request...\n");
        gDevInfo->State=STA_STATE_AUTHED_UNASSOC;
        mlme_send_assoc_req();
        break;
    case MLME_WORK_JOIN_ASSOC_RESP:
        //20141028 EricYu: For join timer
        MLME_START_TIMER(&gDevInfo->MTimer, ASSOC_TIME_OUT, MLME_WORK_ASSOC_TIMEOUT, NULL, NULL);
        break;
    case MLME_WORK_JOIN_SUPPLICIANT:
        gDevInfo->State=STA_STATE_AUTHED_ASSOCED;
        if(gDevInfo->wpa_ssid.key_mgmt==WPA_KEY_MGMT_NONE)
        {
            Join_process_result(EN_JOIN_RESULT_SUCCESS);
            break;
        }
#if (CONFIG_SIM_PLATFORM == 1)
        MLME_START_TIMER(&gDevInfo->MTimer, SUPPLICANT_4WAY_TIME_OUT, MLME_WORK_4way_TIMEOUT, NULL, NULL);
#else
        MLME_START_TIMER(&gDevInfo->MTimer, SUPPLICANT_4WAY_TIME_OUT, MLME_WORK_4way_TIMEOUT, NULL, NULL);
#endif //
        break;
    case MLME_WORK_JOIN_RESULT:
        Join_process_result(0);
        break;
    case MLME_WORK_JOIN_AUTH_FAIL:
    case MLME_WORK_JOIN_ASSOC_FAIL:
        if (WorkJobReq->u.join.tries<MLME_JOIN_MAX_TRIES)
        {
            WorkJobReq->u.join.tries++;
            Join_action((gDevInfo->State==STA_STATE_UNAUTH_UNASSOC)?MLME_WORK_JOIN_TX_AUTH_1:MLME_WORK_JOIN_ASSOC_REQ, NULL, 0);
        }
        else
        {
            JOIN_FAIL_HANDLE(EN_JOIN_RESULT_FAIL);
        }
        break;
    case MLME_WORK_JOIN_4WAY_FAIL:
        mlme_send_deauth(1,(struct ETHER_ADDR_st*)&gDevInfo->info.addr,(struct ETHER_ADDR_st*)&gDevInfo->connected_sta_ptr->addr.addr,(struct ETHER_ADDR_st*)&gDevInfo->connected_sta_ptr->addr.addr,TRUE);
        JOIN_FAIL_HANDLE(EN_JOIN_RESULT_WRONG_PASSWORD);
        break;
     default:
         break;
    }
    return 0;
}



