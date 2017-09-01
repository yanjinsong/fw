#include <config.h>
#include <regs.h>
#include <bsp/irq.h>
#include <msgevt.h>
#include <mbox/drv_mbox.h>
#include <hdr80211.h>
#if (CONFIG_SIM_PLATFORM==1)
#include <sim_regs.h>
#endif
#include "drv_mac.h"
#if (N_MODE_AMPDU_TX == 1)
#include <driver/ampdu/drv_ampdu.h>
#include <apps/mac80211/sta_info.h>
#endif


#include <log.h>
#include <pbuf.h>
#ifdef UART_DATA_TEST
#include <lib/ssv_lib.h>
#include "uart/drv_uart.h"
#define fflush(x)
#endif

#include <edca/drv_edca.h>
#include <dbg_timer/dbg_timer.h>
#include <apps/mac80211/mlme.h>





//static OsMutex sg_mac_mutex;
static u32 drop_all_frame = false;


extern void cmd_eng_prebcn_int_handler (void);
extern void SoftMac_NullFrameHandler( PKT_Info *pPktInfo );
extern void SoftMac_WsidNotFoundHandler( PKT_Info *pPktInfo, s32 reason  );
extern void EDCA_Handler(PKT_Info *pPktInfo, s32 reason);
extern bool MLME_is_ps_doze(void);
extern u32 MLME_ps_wow(PKT_Info *pPktInfo);
extern inline bool IsSoftMacTaskSleeping(void);
extern void rate_control_update_drate_by_hwidx(u8 wsid, u8 hw_idx);
inline void ap_mode_set_rcpi_value(struct cfg_host_rxpkt *pCfgHostRxPkt)
{
    #if 1
    u8 type=0;
    u8 offset[]={   0,  //SECURITY_NONE
                    8,  //SECURITY_WEP40
                    8,  //SECURITY_WEP104
                    16,  //SECURITY_TKIP
                    16   //SECURITY_CCMP
    };

    if(pCfgHostRxPkt->unicast==0)
        type=drv_mac_get_group_security();
    else
        type=drv_mac_get_pair_security();

    pCfgHostRxPkt->RCPI=*((u8 *)((u32)pCfgHostRxPkt+pCfgHostRxPkt->hdr_offset+pCfgHostRxPkt->len+offset[type]));
    pCfgHostRxPkt->SNR =*((u8 *)((u32)pCfgHostRxPkt+pCfgHostRxPkt->hdr_offset+pCfgHostRxPkt->len+1+offset[type]));
    #else
    pCfgHostRxPkt->RCPI=*((u8 *)((u32)pCfgHostRxPkt+pCfgHostRxPkt->hdr_offset+pCfgHostRxPkt->len));
    pCfgHostRxPkt->SNR =*((u8 *)((u32)pCfgHostRxPkt+pCfgHostRxPkt->hdr_offset+pCfgHostRxPkt->len+1));
    #endif
}

#if (N_MODE_AMPDU_TX == 1)
extern SSV6XXX_STA *g6200StaInfo;
#endif
void ap_mode_update_rate_from_sta(PKT_Info *pRxPkt)
{
#if (N_MODE_AMPDU_TX == 1)
    SSV6XXX_STA *sta = NULL;

    sta=&g6200StaInfo[pRxPkt->wsid];
    if ((pRxPkt->wsid == 0) || (pRxPkt->wsid >= WLAN_MAX_STA) || (pRxPkt->unicast == 0) || (sta->ampdu_tx == TRUE))
#else
    if ((pRxPkt->wsid == 0) || (pRxPkt->wsid >= WLAN_MAX_STA) || (pRxPkt->unicast == 0))
#endif
        return ;
    rate_control_update_drate_by_hwidx(pRxPkt->wsid, pRxPkt->drate_idx);
}

extern u16 null_data_status [WLAN_MAX_STA];
Time_T last_tx_rx_time;
extern u8 recover_mechanism;
extern u8 recover_enable;

s32 ap_mode_monitor_pm_status(PKT_Info *pPktInfo)
{
    u16 fc = GET_HDR80211_FC(pPktInfo);
    struct cfg_host_rxpkt *pCfgHostRxPkt = (struct cfg_host_rxpkt *)pPktInfo;
    bool need_wake_up_task = false;
    s32  result =OS_SUCCESS;

    if(IS_NULL_DATA(fc)||IS_QOS_NULL_DATA(fc)){
        SoftMac_NullFrameHandler(pPktInfo);
        return result;
    }
    else
    {
        if((pPktInfo->wsid==0||pPktInfo->wsid==1))
        {
            if((!IS_PM_SET(fc))&&IS_PM_SET(null_data_status[pPktInfo->wsid])){

                if(TRUE==IsSoftMacTaskSleeping())
                    need_wake_up_task =true;

                //In this case, STA is in the power saving mode, and it send a data packet with powe active to AP
                //drv_edca_queue_halt(EDCA_AC_BK+pPktInfo->wsid, FALSE);
                ssv6xxx_SWQOutUnLock(pPktInfo->wsid,TX_SW_QUEUE);

                if (need_wake_up_task)
                     wake_up_task(MBOX_SOFT_MAC);

                null_data_status[pPktInfo->wsid] = 0x0000;

                //printf("data:wsid=%d, seq=%d,c=%d,UnLock\n ",pPktInfo->wsid,GET_HDR80211_SC_SEQNUM(pPktInfo),GET_RO_MRX_LEN_CNT_0);

            }else if((IS_PM_SET(fc))&&(!IS_PM_SET(null_data_status[pPktInfo->wsid]))){
                //In this case, STA is in the power active mode, and it send a data packet with powe saving to AP
                //drv_edca_queue_halt(EDCA_AC_BK+pPktInfo->wsid, TRUE);
                ssv6xxx_SWQOutLock(pPktInfo->wsid,TX_SW_QUEUE);
                null_data_status[pPktInfo->wsid] = 0xFFFF;
                //printf("data:wsid=%d,seq=%d,c=%d,Lock\n ",pPktInfo->wsid,GET_HDR80211_SC_SEQNUM(pPktInfo),GET_RO_MRX_LEN_CNT_0);
            }
            else{
                //printf("data:wsid=%d,seq=%d,PM=%d,c=%d \n ",pPktInfo->wsid,GET_HDR80211_SC_SEQNUM(pPktInfo),IS_PM_SET(fc),GET_RO_MRX_LEN_CNT_0);
            }
        }

        ap_mode_set_rcpi_value(pCfgHostRxPkt);
        //ap_mode_update_rate_from_sta(pPktInfo);

        result=(TRUE==ENG_MBOX_NEXT((u32)pPktInfo))?OS_SUCCESS:OS_FAILED;
        if(result == OS_SUCCESS){
            if(recover_mechanism ==1 && recover_enable >0)
                dbg_get_time(&last_tx_rx_time);
        }
        return result;

    }//if(IS_NULL_DATA(fc)||IS_QOS_NULL_DATA(fc)){
}


extern inline s32 ssv6xxx_PushRxPacketToNextHW(void);
extern inline s32 ssv6xxx_PushTxPacketToNextHW(void);
extern bool IsSTAActReady(void);
inline void update_heartbeat(PKT_Info *pPktInfo)
{
    if(IS_TX_PKT(pPktInfo) && IS_DATA_FRAME(GET_HDR80211_FC(pPktInfo))){
        if(recover_mechanism ==1 && recover_enable > 0){
            if(drv_mac_get_op_mode()==WLAN_AP)
                dbg_get_time(&last_tx_rx_time);
            else{
                if(TRUE==IsSTAActReady())
                    dbg_get_time(&last_tx_rx_time);
            }
        }
    }
}

static void irq_mac_handler(void *m_data)
{
//    MsgEvent *MsgEv;
#if 0
    PKT_Info *pPktInfo;
#endif
    u32 irq = (u32)m_data;
    u8  count;

    if (drop_all_frame)
    {
        u32     rx_data = (u32)GET_CH0_INT_ADDR;
        PBUF_MFree((void *)rx_data);
        return;
    }

    ASSERT(gOsFromISR > 0);
    switch(irq)
    {
        case IRQ_MCU_MBOX:
            count = GET_FFO0_CNT;
            while(count)
            {
                u32     rx_data = (u32)GET_CH0_INT_ADDR;
                s32     result=OS_FAILED,TrapReason=0;
                PKT_Info *pPktInfo = (PKT_Info *)rx_data;
                struct cfg_host_rxpkt *rxpkt = (struct cfg_host_rxpkt *)rx_data; 
                u16 eth_type;
                const u8 *dat;

                ASSERT_PKT(PBUF_isPkt(rx_data), rx_data);
                #if(PACKET_TRACE==1)
                if(1==is_pbuf_in_fw((void *)rx_data))
                {
                    printf("%s:WARN!!This pbuf is already in fw(0x%x)\n",__func__,(u32)rx_data);
                }
                pt((void *)rx_data,M_ENG_MAX,EN_LOC_RECEIVE);
                #endif

                
                if(recover_enable > 0)// Enable watchdog & reset watchdog
                    SET_SYS_WDT_TIME_CNT(WATCHDOG_TIMER);

                switch(pPktInfo->c_type)
                {
                    case HOST_CMD:
                        result = msg_evt_post_data1(MBOX_SOFT_MAC, MEVT_PKT_BUF, rx_data, 0,0,0);
                        break;
                    case M0_TXREQ:
                    case M0_RXEVENT:
                    case M2_TXREQ:
                    //case M2_RXEVENT:
                        TrapReason = pPktInfo->reason;
                        //printf("rsn=%d,T=%d,R=%d,g=%d,s=%d,rp=%d\r\n",pPktInfo->reason,IS_TX_PKT(pPktInfo),IS_RX_PKT(pPktInfo),
                            //((PKT_TxInfo*)pPktInfo)->aggregation,GET_HDR80211_SC_SEQNUM(((PKT_TxInfo*)pPktInfo)),((PKT_TxInfo*)pPktInfo)->tx_report);
                        switch(TrapReason)
                        {
                            case ID_TRAP_NONE:
                                if(IS_RX_PKT(pPktInfo) && IS_DATA_FRAME(GET_HDR80211_FC(pPktInfo))){
                                    
                                    //Fill seq_number for AMPDU RX
                                    rxpkt->seq_num = GET_HDR80211_SC_SEQNUM(pPktInfo);
                                    rxpkt->tid = GET_HDR80211_QC(pPktInfo)& 0x000F; //IEEE80211_QOS_CTL_TID_MASK
                                    
                                    if(drv_mac_get_op_mode()==WLAN_AP){
                                        result = ap_mode_monitor_pm_status(pPktInfo);
                                    }
                                    else if(drv_mac_get_op_mode()==WLAN_STA && MLME_is_ps_doze()){
                                        //printf("rx\r\n");
                                        result = msg_evt_post_data1(MBOX_MLME,MEVT_POWER_SAVING,PS_EVT_WOW, rx_data, 0,0);//MLME_ps_wow(pPktInfo);
                                    }
                                    else{
                                        result=(TRUE==ENG_MBOX_NEXT((u32)pPktInfo))?OS_SUCCESS:OS_FAILED;
                                        if(result == OS_SUCCESS){
                                            if(recover_mechanism ==1 && recover_enable >0){
                                                if(TRUE==IsSTAActReady())
                                                    dbg_get_time(&last_tx_rx_time);                                                
                                            }
                                        }

                                            result = OS_SUCCESS;
                                    }
                                }
                                else if( (drv_mac_get_op_mode()==WLAN_AP) && IS_RX_PKT(pPktInfo) && IS_MGMT_FRAME(GET_HDR80211_FC(pPktInfo)))
                                {
                                    HDR80211_Mgmt *Mgmt80211=HDR80211_MGMT(pPktInfo);
                                    //Pass ADDBA request frames to MLME task, others continues next side
                                    if((IS_ACTION(GET_HDR80211_FC(pPktInfo))&&(Mgmt80211->u.action.category == WLAN_CATEGORY_BACK)))
                                    {
                                        result = msg_evt_post_data1(MBOX_MLME, MEVT_PKT_BUF, rx_data,0,0,0);
                                    }
                                    else
                                    {
                                        result = msg_evt_post_data1(MBOX_SOFT_MAC, MEVT_PKT_BUF, rx_data,0,0,0);
                                        if(result == OS_SUCCESS){
                                           update_heartbeat(pPktInfo);
                                        }
                                    }

                                }
#if (N_MODE_AMPDU_TX == 1)
                                else if (IS_RX_PKT(pPktInfo) && IS_BA(GET_HDR80211_FC(pPktInfo)))
                                {
                                    ampdu_rx_ctrl_handler((u32)pPktInfo);
                                    PBUF_MFree((void *)rx_data);
                                    result = OS_SUCCESS;
                                }
                                else if (IS_TX_PKT(pPktInfo) && (((PKT_TxInfo*)pPktInfo)->aggregation == 1))
                                {
                                    ampdu_tx_report_handler((u32)pPktInfo);
                                    result = OS_SUCCESS;
                                }
#endif
                                else if (IS_TX_PKT(pPktInfo) && (((PKT_TxInfo*)pPktInfo)->tx_report == 1))
                                {
                                    //printf("Err txRp %d\r\n",GET_HDR80211_SC_SEQNUM(((PKT_TxInfo*)pPktInfo)));
                                    memset((void*)pPktInfo,0,sizeof(PKT_TxInfo));
                                    PBUF_MFree((void *)rx_data);
                                    result = OS_SUCCESS;
                                }
                                else{

                                    result = msg_evt_post_data1(MBOX_SOFT_MAC, MEVT_PKT_BUF, rx_data,0,0,0);
                                    if(result == OS_SUCCESS){
                                        update_heartbeat(pPktInfo);

                                    }
                                }//if(IS_RX_PKT(pPktInfo) && IS_DATA_FRAME(fc)){
                                break;
                            case ID_TRAP_RX_ETHER_TRAP:
                                dat =(const u8 *)pPktInfo + pPktInfo->payload_offset + ETHER_MAC_LEN;
                                eth_type = (dat[0]<<8) | dat[1];
                                if((EAPOL_ETHER_TYPPE==eth_type)&&(0==pPktInfo->f80211)){
                                    result = msg_evt_post_data1(MBOX_MLME, MEVT_EAPOL, rx_data,0,0,0);
                                }
                                else{
                                    printf("eth_type=%d,pPktInfo->f80211=%d\n",eth_type,pPktInfo->f80211);
                                    ssv6xxx_raw_dump((char *)((u32)pPktInfo + pPktInfo->hdr_offset),(s32)pPktInfo->len);
                                }
                                break;
                            #ifdef AIRKISS_ENABLE
                            //Move this handle from soft_mac_task to here, it's in oreder to avoid pbuf allocate fail in AIRKISS
                            //Please refer to the RB-249 to get the detail
                            case ID_TRAP_NO_WSID_FOUND:
                                SoftMac_WsidNotFoundHandler(pPktInfo,TrapReason);
                                result = OS_SUCCESS;
                                break;
                            #endif
                            default:
                            {
                                result = msg_evt_post_data1(MBOX_SOFT_MAC, MEVT_PKT_BUF, rx_data,0,0,0);
                            }
                        }
                        break;

                    default:
                        printf("\n!!! Unexpect cType=%d!!!\n",pPktInfo->c_type);
                        result=OS_FAILED;
                }

                ssv6xxx_PushRxPacketToNextHW();
                ssv6xxx_PushTxPacketToNextHW();

                if (result != OS_SUCCESS)
                {
                    printf("\n!!! MAC IRQ ERROR: c_type=%d",pPktInfo->c_type);
                    if(pPktInfo->c_type!=HOST_CMD){
                        printf(" ,Trapreason=%d",TrapReason);
                    }
                    printf("!!!\n");
                    memset((void*)pPktInfo,0,sizeof(PKT_Info));
                    PBUF_MFree((void *)rx_data);
                }

                //Write 1 to POP
                SET_CPU_QUE_POP(1);
                count--;

                g_soc_mib.sw_mac_rx_cnt ++;
            }
            break;

        default: /*lint -save -e774 */
            assert(0); /*lint -restore */
    }
}



s32 drv_mac_set_sta_mac(const u8 *mac)
{
    u8 temp[8];
    const u32   *mac_31_0 = (u32 *)(mac+0);
    const u16   *mac_47_32 = (u16 *)(mac+4);
    memcpy(temp,mac,MAC_ADDR_LEN);
    mac_31_0 = (u32 *)(mac+0);
    mac_47_32 = (u16 *)(mac+4);

//    OS_MutexLock(sg_mac_mutex);
    SET_STA_MAC_31_0(*mac_31_0);
    SET_STA_MAC_47_32(*mac_47_32);

//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}

s32 drv_mac_get_sta_mac(u8 *mac)
{
    u8 temp[8];

    u32   *mac_31_0 = (u32 *)(temp+0);
    u16   *mac_47_32 = (u16 *)(temp+4);

    *mac_31_0 = GET_STA_MAC_31_0;
    *mac_47_32 = (u16)GET_STA_MAC_47_32;

    memcpy(mac,temp,MAC_ADDR_LEN);

    return 0;
}


s32 drv_mac_set_ba_ta(u8 *mac)
{
    u8 temp[8];
    const u32   *mac_31_0 = (u32 *)(mac+0);
    const u16   *mac_47_32 = (u16 *)(mac+4);
    memcpy(temp,mac,MAC_ADDR_LEN);
    mac_31_0 = (u32 *)(mac+0);
    mac_47_32 = (u16 *)(mac+4);

//    OS_MutexLock(sg_mac_mutex);
    SET_BA_TA_31_0(*mac_31_0);
    SET_BA_TA_47_32(*mac_47_32);

//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}

s32 drv_mac_get_ba_ta(u8 *mac)
{
//    OS_MutexLock(sg_mac_mutex);
    *(u32*)(mac+0) = GET_BA_TA_31_0;
    *(u16*)(mac+4) =(u16)GET_BA_TA_47_32;

//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}

s32 drv_mac_set_ba_switch(u8 enable)
{
    volatile u32 control = GET_BA_CTRL;
    control = (control & 0xfffffffc) | enable;
    SET_BA_CTRL(control);
    return 0;
}

s32 drv_mac_get_ba_switch()
{
    return (GET_BA_CTRL&0x01);
}

s32 drv_mac_set_ba_winsize(u8 size)
{
    volatile u32 control = GET_BA_CTRL;
    control = (control & 0x3) | (size << 2);
    SET_BA_CTRL(control);
    return 0;
}


u16 drv_mac_get_ba_winsize()
{
    return (u16)((GET_BA_CTRL>>2)&0x1f);
}


s32 drv_mac_set_bssid(const u8 *mac)
{
    u8 temp[8];
    const u32   *mac_31_0 ;
    const u16   *mac_47_32 ;
    memcpy(temp,mac,MAC_ADDR_LEN);
    mac_31_0 = (u32 *)(temp+0);
    mac_47_32 = (u16 *)(temp+4);
//    OS_MutexLock(sg_mac_mutex);
    SET_BSSID_31_0(*mac_31_0);
    SET_BSSID_47_32((u32)*mac_47_32);

//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}


s32 drv_mac_get_bssid(u8 *mac)
{
    u8 temp[8];

    u32   *mac_31_0 = (u32 *)(temp+0);
    u16   *mac_47_32 = (u16 *)(temp+4);
    *mac_31_0 = (u32)GET_BSSID_31_0;
    *mac_47_32 = (u16)GET_BSSID_47_32;

    memcpy(mac,temp,MAC_ADDR_LEN);
    return 0;
}


s32 drv_mac_get_wsid_peer_mac (s32 index, ETHER_ADDR *mac)
{
    u32 mac32[2];

    //*(u32 *)(mac->addr+0) = REG_WSID_ENTRY[index].MAC00_31;
    //*(u32 *)(mac->addr+4) = REG_WSID_ENTRY[index].MAC32_47;
    mac32[0] = REG_WSID_ENTRY[index].MAC00_31;
    mac32[1] = REG_WSID_ENTRY[index].MAC32_47;
    memcpy(mac, mac32, sizeof(ETHER_ADDR));
    return 0;
} // end of - drv_mac_get_wsid_peer_mac -


s32 drv_mac_set_wsid_table(s32 index, const mwsid_entry *wen)
{
    s32 i;
    u32 mac32[2] = {0};

//    OS_MutexLock(sg_mac_mutex);
    REG_WSID_ENTRY[index].WINFO = wen->info;
    memcpy(mac32, wen->sta_mac.addr, sizeof(ETHER_ADDR));
    REG_WSID_ENTRY[index].MAC00_31 = mac32[0];
    REG_WSID_ENTRY[index].MAC32_47 = mac32[1];

    for(i=0; i<8; i++) {
        REG_WSID_ENTRY[index].s[i].UP_ACK_POLICY = (wen->tx_ack_policy>>(i<<1));
        REG_WSID_ENTRY[index].s[i].UP_TX_SEQCTL = wen->tx_seq_ctrl[i];
        REG_WSID_RX_ENTRY[index].UP_RX_SEQCTL[i] = wen->rx_seq_ctrl[i];
    }

//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}


s32 drv_mac_get_wsid_table(s32 index, mwsid_entry *wen)
{
    u32 i;
    u32 mac32[2] = {0};
//    OS_MutexLock(sg_mac_mutex);
    if (index < WLAN_MAX_STA) {
        wen->info = REG_WSID_ENTRY[index].WINFO;
        mac32[0] = REG_WSID_ENTRY[index].MAC00_31;
        mac32[1] = REG_WSID_ENTRY[index].MAC32_47;
        memcpy(wen->sta_mac.addr, mac32, sizeof(ETHER_ADDR));
        wen->tx_ack_policy = 0;
        for(i=0, wen->tx_ack_policy=0; i<(sizeof(wen->tx_seq_ctrl)/sizeof(wen->tx_seq_ctrl[0])); i++) {
            wen->tx_ack_policy |= (REG_WSID_ENTRY[index].s[i].UP_ACK_POLICY & 0x03) << (i*2);
            wen->tx_seq_ctrl[i] = (u16)REG_WSID_ENTRY[index].s[i].UP_TX_SEQCTL;
            wen->rx_seq_ctrl[i] = (u16)REG_WSID_RX_ENTRY[index].UP_RX_SEQCTL[i];
        }
    }
//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}






/**
* s32 drv_mac_set_gmfilter() - group address (BC/MC) filtering.
*
* @ mac: ether mac address.
* @ mask: mask for ether mac.
* @ single: single mode for single!=0, mask mode for single=0.
*/
s32 drv_mac_set_gmfilter(s32 index, const gmfilter *gm_entry)
{
    u32 mac32[2] = {0};
    //u32 gm_ctrl;
//    OS_MutexLock(sg_mac_mutex);
    //REG_MRX_GMFLT[index].TB_MAC00_31 = *(u32 *)(gm_entry->mac.addr+0);
    //REG_MRX_GMFLT[index].TB_MAC32_47 = *(u16 *)(gm_entry->mac.addr+4);
    //REG_MRX_GMFLT[index].MK_MAC00_31 = *(u32 *)(gm_entry->msk.addr+0);
    //REG_MRX_GMFLT[index].MK_MAC32_47 = *(u16 *)(gm_entry->msk.addr+4);
    memcpy(mac32, gm_entry->mac.addr, sizeof(ETHER_ADDR));
    REG_MRX_GMFLT[index].TB_MAC00_31 = mac32[0];
    REG_MRX_GMFLT[index].TB_MAC32_47 = mac32[1];
    memcpy(mac32, gm_entry->msk.addr, sizeof(ETHER_ADDR));
    REG_MRX_GMFLT[index].MK_MAC00_31 = mac32[0];
    REG_MRX_GMFLT[index].MK_MAC32_47 = mac32[1];
    REG_MRX_GMFLT[index].control = gm_entry->ctrl;
   // gm_ctrl = GET_MRX_MCAST_CTRL;
    //gm_ctrl =
    //GET_MRX_MCAST_CTRL_3<<24|GET_MRX_MCAST_CTRL_2<<16|GET_MRX_MCAST_CTRL_1<<8|GET_MRX_MCAST_CTRL_0;
    //gm_ctrl &= ~(0x03<<(index*2));
    //gm_ctrl |= ((gm_entry->ctrl&0x03)<<(index*2));
    //SET_MRX_MCAST_CTRL(gm_ctrl);
    //SET_MRX_MCAST_CTRL_0(gm_ctrl&0x000000ff);
    //SET_MRX_MCAST_CTRL_1((gm_ctrl>>8)&0x000000ff);
    //SET_MRX_MCAST_CTRL_2((gm_ctrl>>16)&0x000000ff);
    //SET_MRX_MCAST_CTRL_3((gm_ctrl>>24)&0x000000ff);

//    OS_MutexUnLock(sg_mac_mutex);

    return 0;
}


/**
* s32 drv_mac_get_gmfilter() - group address (BC/MC) filtering.
*
* @ mac: ether mac address.
* @ mask: mask for ether mac.
* @ single: single mode for single!=0, mask mode for single=0.
*/
s32 drv_mac_get_gmfilter(s32 index, gmfilter *gm_entry)
{

//    OS_MutexLock(sg_mac_mutex);
//    u8 gm_filter;
    u32 mac32[2];
    //*(u32 *)(gm_entry->mac.addr+0) = (u32)REG_MRX_GMFLT[index].TB_MAC00_31;
    //*(u16 *)(gm_entry->mac.addr+4) = (u16)REG_MRX_GMFLT[index].TB_MAC32_47;
    //*(u32 *)(gm_entry->msk.addr+0) = (u32)REG_MRX_GMFLT[index].MK_MAC00_31;
    //*(u16 *)(gm_entry->msk.addr+4) = (u16)REG_MRX_GMFLT[index].MK_MAC32_47;
    mac32[0] = REG_MRX_GMFLT[index].TB_MAC00_31;
    mac32[1] = REG_MRX_GMFLT[index].TB_MAC32_47;
    memcpy(gm_entry->mac.addr, mac32, sizeof(ETHER_ADDR));
    mac32[0] = REG_MRX_GMFLT[index].MK_MAC00_31;
    mac32[1] = REG_MRX_GMFLT[index].MK_MAC32_47;
    memcpy(gm_entry->msk.addr, mac32, sizeof(ETHER_ADDR));

    //gm_filter=((GET_MRX_MCAST_CTRL_3<<24)|(GET_MRX_MCAST_CTRL_2<<16)|(GET_MRX_MCAST_CTRL_1)<<8|(GET_MRX_MCAST_CTRL_0));
    gm_entry->ctrl = REG_MRX_GMFLT[index].control;
//    OS_MutexUnLock(sg_mac_mutex);

    return 0;
}





/**
* s32 drv_mac_set_tx_ether_trap() - ASIC table manipulation for tx-ether-trap
*       to/from hardware.
*
* @ eth_type: specify the ether type of ether frame.
*/
s32 drv_mac_set_tx_ether_trap(s32 index, u16 eth_type)
{
//    OS_MutexLock(sg_mac_mutex);
    if (index == 0)
        SET_TXTRAP_ETHTYPE0(eth_type);
    else if (index == 1)
        SET_TXTRAP_ETHTYPE1(eth_type);

//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}



/**
* s32 drv_mac_get_tx_ether_trap() - ASIC table manipulation for tx-ether-trap
*       to/from hardware.
*
* @ eth_type: specify the ether type of ether frame.
*/
s32 drv_mac_get_tx_ether_trap(s32 index, u16 *eth_type)
{
//    OS_MutexLock(sg_mac_mutex);
    if (index == 0)
        *eth_type = GET_TXTRAP_ETHTYPE0;
    else if (index == 1)
        *eth_type = GET_TXTRAP_ETHTYPE1;
//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}




/**
* s32 drv_mac_set_rx_ether_trap() - ASIC table manipulation for rx-ether-trap
*       to/from hardware.
*
* @ eth_type: specify the ether type of ether frame.
*/
s32 drv_mac_set_rx_ether_trap(s32 index, u16 eth_type)
{
//    OS_MutexLock(sg_mac_mutex);
    if (index == 0)
        SET_RXTRAP_ETHTYPE0(eth_type);
    else if (index == 1)
        SET_RXTRAP_ETHTYPE1(eth_type);

//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}


s32 drv_mac_get_rx_ether_trap(s32 index, u16 *eth_type)
{
//    OS_MutexLock(sg_mac_mutex);
    if (index == 0)
        *eth_type = GET_RXTRAP_ETHTYPE0;
    else if (index == 1)
        *eth_type = GET_RXTRAP_ETHTYPE1;
//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}





/**
* s32 drv_mac_set_decsion_table() - Set MAC Rx decision table.
*
* @
*/
s32 drv_mac_set_decision_table(const u16 *raw_dat)
{
    s32 i;
//    OS_MutexLock(sg_mac_mutex);
    for(i=0; i<16; i++)
        REG_MRX_DECTBL->DECI_VAL[i] = raw_dat[i];
    for(i=0; i<9; i++)
        REG_MRX_DECMSK->DECI_MSK[i] = raw_dat[i+16];

//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}

/**
* s32 drv_mac_get_decsion_able() - Get MAC Rx decision table.
*
* @
*/
s32 drv_mac_get_decision_table(u16 *raw_dat)
{
    s32 i;
//    OS_MutexLock(sg_mac_mutex);
    for (i=0; i<16; i++)
        raw_dat[i] = REG_MRX_DECTBL->DECI_VAL[i];
    for(i=0; i<9; i++)
        raw_dat[i+16] = REG_MRX_DECMSK->DECI_MSK[i];
//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}



/**
* s32 drv_mac_get_decsion_able() - Get MAC Rx decision table.
*
* @
*/
s32 drv_mac_get_decision_table_value(u16 *raw_dat)
{
    s32 i;
//    OS_MutexLock(sg_mac_mutex);
    for (i=0; i<16; i++)
        raw_dat[i] = REG_MRX_DECTBL->DECI_VAL[i];
//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}

/**
* s32 drv_mac_get_decsion_able() - Get MAC Rx decision table.
*
* @
*/
s32 drv_mac_get_decision_table_mask(u16 *raw_dat)
{
    s32 i;
//    OS_MutexLock(sg_mac_mutex);
    for(i=0; i<9; i++)
        raw_dat[i] = (u16)REG_MRX_DECMSK->DECI_MSK[i];
//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}


s32 drv_mac_set_tx_flow_data(const u32 *cflow)
{
//    OS_MutexLock(sg_mac_mutex);
    SET_TX_FLOW_DATA(cflow[0]);
    gTxFlowDataReason = cflow[1];

//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}


s32 drv_mac_get_tx_flow_data(u32 *cflow)
{
//    OS_MutexLock(sg_mac_mutex);
    cflow[0] = GET_TX_FLOW_DATA;
    cflow[1] = (u32)gTxFlowDataReason;
//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}


s32 drv_mac_set_tx_flow_mgmt(const u32 *cflow)
{
//    OS_MutexLock(sg_mac_mutex);
    SET_TX_FLOW_MGMT(cflow[0]);
    gTxFlowMgmtReason = cflow[1];

//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}



s32 drv_mac_get_tx_flow_mgmt(u32 *cflow)
{
//    OS_MutexLock(sg_mac_mutex);
    cflow[0] = GET_TX_FLOW_MGMT;
    cflow[1] = (u32)gTxFlowMgmtReason;
//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}


s32 drv_mac_set_tx_flow_ctrl(const u32 *cflow)
{
//    OS_MutexLock(sg_mac_mutex);
    SET_TX_FLOW_CTRL(cflow[0]);

//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}


s32 drv_mac_get_tx_flow_ctrl(u32 *cflow)
{
//    OS_MutexLock(sg_mac_mutex);
    cflow[0] = GET_TX_FLOW_CTRL;
//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}


s32 drv_mac_set_rx_flow_data(const u32 *cflow)
{
//    OS_MutexLock(sg_mac_mutex);
    SET_RX_FLOW_DATA(cflow[0]);
    gRxFlowDataReason = cflow[1];
    //LOG_DEBUG("set gRxFlowDataReason =%x\n",gRxFlowDataReason);

//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}


s32 drv_mac_get_rx_flow_data(u32 *cflow)
{
//    OS_MutexLock(sg_mac_mutex);
    cflow[0] = GET_RX_FLOW_DATA;
    cflow[1] = (u32)gRxFlowDataReason;
//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}



s32 drv_mac_set_rx_flow_mgmt(const u32 *cflow)
{
//    OS_MutexLock(sg_mac_mutex);
    SET_RX_FLOW_MNG(cflow[0]);
    gRxFlowMgmtReason = cflow[1];

//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}


s32 drv_mac_get_rx_flow_mgmt(u32 *cflow)
{
//    OS_MutexLock(sg_mac_mutex);
    cflow[0] = GET_RX_FLOW_MNG;
    cflow[1] = (u32)gRxFlowMgmtReason;
//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}



s32 drv_mac_set_rx_flow_ctrl(const u32 *cflow)
{
//    OS_MutexLock(sg_mac_mutex);
    SET_RX_FLOW_CTRL(cflow[0]);
    gRxFlowCtrlReason = cflow[1];

//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}



s32 drv_mac_get_rx_flow_ctrl(u32 *cflow)
{
//    OS_MutexLock(sg_mac_mutex);
    cflow[0] = GET_RX_FLOW_CTRL;
    cflow[1] = (u32)gRxFlowCtrlReason;
//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}


s32 drv_mac_set_trap_mask(const u32 *trap_mask)
{
//    OS_MutexLock(sg_mac_mutex);
    SET_REASON_TRAP0(trap_mask[0]);
    SET_REASON_TRAP1(trap_mask[1]);
//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}



s32 drv_mac_get_trap_mask(u32 *trap_mask)
{
//    OS_MutexLock(sg_mac_mutex);
    trap_mask[0] = GET_REASON_TRAP0;
    trap_mask[1] = GET_REASON_TRAP1;
//    OS_MutexUnLock(sg_mac_mutex);
    return 0;
}




#if (FPGA_DEBUG == 1)


inline void show_DMN_status(u32 adr_dmn_status)
{
    u8 NHIT_ID=(u16)(adr_dmn_status>>8)&0x7f;
    u32 NHIT_ADDR=(u16)(adr_dmn_status>>16)&0xffff;
    u32 addr=0;
    /*please go to JIRA (ssv6200_reg.xml) to check details*/
    printf("NOHIT_FLAG:%d\n",(u16)(adr_dmn_status)&0x1);
    printf("FLAG:%d\n",(u16)(adr_dmn_status>>1)&0x1);
    printf("WR:%d\n",(u16)(adr_dmn_status>>3)&0x1);
    printf("PORT:%d\n",(u16)(adr_dmn_status>>4)&0xf);
    printf("NHIT_ID:%d\n",NHIT_ID);
    printf("NHIT_ADDR:%d\n",NHIT_ADDR);

    addr=PBUF_MapIDtoPkt(NHIT_ID);

    ssv6xxx_raw_dump((char *)addr,256);


    printf("ID TB0=0x%x\n",GET_ID_TB0);
    printf("ID TB1=0x%x\n",GET_ID_TB1);

    printf("TX ID TB0=0x%x\n",GET_TX_ID_TB0);
    printf("TX ID TB1=0x%x\n",GET_TX_ID_TB1);
    printf("RX ID TB0=0x%x\n",GET_RX_ID_TB0);
    printf("RX ID TB1=0x%x\n",GET_RX_ID_TB1);

}



extern void dump_mailbox_dbg (int num);

static void irq_mac_int_debug_handler(void *m_data)
{

    u32 irq = (u32)m_data;

    SET_RX_QUOTA((u16)irq);

    if(irq==IRQ_ID_MNG_DMN_NOHIT)
    {
		printf("Interrupt Exception Erro number(%d)\n",(u16)irq);
        printf("0xC10000A0: %08X\n", *(volatile u32 *)(0xc10000A0));
        printf("0xC10000A4: %08X\n", *(volatile u32 *)(0xc10000A4));
        printf("0xC10000A8: %08X\n", *(volatile u32 *)(0xc10000A8));
        printf("0xC10000AC: %08X\n", *(volatile u32 *)(0xc10000AC));
        printf("0xC10000B0: %08X\n", *(volatile u32 *)(0xc10000B0));


        REG32(ADR_MBOX_HALT_CFG) = 0x300000;
        //SET_MB_EXCEPT_CLR(1);
        //SET_STOP_MBOX(0);
        SET_DMN_FLAG_CLR(1);
        return;
    }

    //Disable interrupt mask in addition to timer.
    SET_MASK_TOP(0xf807ffff);
    SET_INT_PERI_MASK(0xffffffff);
    printf("****************************\n");
    printf("*                          *\n");
    printf("*   Interrupt Exception    *\n");
    printf("*   Erro number(%d)        *\n",(u16)irq);
    printf("*                          *\n");
    printf("****************************\n");
    switch (irq)
    {
    case IRQ_ID_MNG_DMN_NOHIT:
        {
        	u32 adr_dmn_status = REG32(ADR_DMN_STATUS);
        	printf("DMN Status: %08X\n", adr_dmn_status);
            show_DMN_status(adr_dmn_status);
        	dump_mailbox_dbg(0);
        	hex_dump(((void *)(0x80000000 | ((adr_dmn_status & 0x7F00) << 8))), 64);
        }
        break;
    case IRQ_DMN_MCU_INT:
        {
            u32 mcu_dmn_status = REG32(ADR_DMN_MCU_STATUS);
            printf("MCU DMN Status: %08X\n", mcu_dmn_status);
            show_DMN_status(mcu_dmn_status);
            dump_mailbox_dbg(0);
        }
        break;
    case IRQ_ID_DOUBLE_RLS_INT:
        {
            u32 dls_status = REG32(0xcd010034);
            dls_status = (dls_status&0x7f00)>>8;
            printf("double release occurs, ID: %d\n", dls_status);
            dump_mailbox_dbg(0);
        }
        break;
    default:
        break;
    }
    //ASSERT(FALSE);
    //Reset chip, it's inorder to trigger the host to do the recovery
    if(recover_enable>0)
        SET_SYS_ALL_RST(1);
}






void drv_mac_int_debug()
{
   irq_request(IRQ_MMU_ALC_ERR,                 irq_mac_int_debug_handler, (void *)IRQ_MMU_ALC_ERR);
   irq_request(IRQ_MMU_RLS_ERR,                 irq_mac_int_debug_handler, (void *)IRQ_MMU_RLS_ERR);
   irq_request(IRQ_ID_MNG_ALLOCATE_TIMEOUT,     irq_mac_int_debug_handler, (void *)IRQ_ID_MNG_ALLOCATE_TIMEOUT);
   irq_request(IRQ_MBOX_RELSASE_TIMEOUT,        irq_mac_int_debug_handler, (void *)IRQ_MBOX_RELSASE_TIMEOUT);
   irq_request(IRQ_ID_MNG_DMN_NOHIT,            irq_mac_int_debug_handler, (void *)IRQ_ID_MNG_DMN_NOHIT);
   irq_request(IRQ_ID_DOUBLE_RLS_INT,           irq_mac_int_debug_handler, (void *)IRQ_ID_DOUBLE_RLS_INT);
   irq_request(IRQ_DMN_MCU_INT,                 irq_mac_int_debug_handler, (void *)IRQ_DMN_MCU_INT);


}

#endif//FPGA_DEBUG









/**
* s32 drv_mac_init() - do hardware mac initialization.
*/
s32 drv_mac_init(void)
{
    // Prevent from RX hang because of receiving AMPDU
    //SET_AMPDU_SNIFFER(1);
    // Prevent from RX hang because of receiving large frame
    //SET_MRX_LEN_FLT(0x900);

    //Packet buffer resource control
//    SET_TX_ID_THOLD(SSV6200_ID_TX_THRESHOLD);
//    SET_RX_ID_THOLD(SSV6200_ID_RX_THRESHOLD);
//	SET_ID_TX_LEN_THOLD(SSV6200_PAGE_TX_THRESHOLD);
//	SET_ID_RX_LEN_THOLD(SSV6200_PAGE_RX_THRESHOLD);

    //OS_MutexInit(&sg_mac_mutex);

    /* Register MAC interrupt handler here: */
    irq_request(IRQ_MAC_RX, irq_mac_handler, (void *)IRQ_MAC_RX);
//    irq_request(IRQ_EDCA_BK, irq_mac_handler, (void *)IRQ_EDCA_BK);
//    irq_request(IRQ_EDCA_BE, irq_mac_handler, (void *)IRQ_EDCA_BE);
//    irq_request(IRQ_EDCA_VI, irq_mac_handler, (void *)IRQ_EDCA_VI);
//    irq_request(IRQ_EDCA_VO, irq_mac_handler, (void *)IRQ_EDCA_VO);
#ifdef UART_DATA_TEST
    irq_request(IRQ_UART1_RX, irq_mac_handler, (void *)IRQ_UART1_RX);
    irq_request(IRQ_UART1_RX_TIMEOUT, irq_mac_handler, (void *)IRQ_UART1_RX_TIMEOUT);
    irq_request(IRQ_UART1_MLTI, irq_mac_handler, (void *)IRQ_UART1_MLTI);
#endif
    //irq_request(IRQ_EDCA_MNT, irq_mac_handler, (void *)IRQ_EDCA_MNT);

    //irq_request(IRQ_PREBEACON, irq_mac_handler, (void *)IRQ_PREBEACON);
    //irq_request(IRQ_BEACON_DONE, irq_mac_handler, (void *)IRQ_BEACON_DONE);


#if (FPGA_DEBUG == 1)
    drv_mac_int_debug();
    SET_TRX_DEBUG_CNT_ENA(1);                  //enable HCI Tx/RX cnt
#endif

#ifdef __LOOP_BACK_TEST__


#if (TX_EDCA_QUEUE_TEST == 1)
	gTxFlowDataReason=0x00004400;
   	gTxFlowMgmtReason=0x00000400;
   	gRxFlowDataReason=0x00000030;
   	gRxFlowMgmtReason=0x00000630;
#else
	gTxFlowDataReason=0x00040000;
   	gTxFlowMgmtReason=0x00000400;
   	gRxFlowDataReason=0x00000300;
   	gRxFlowMgmtReason=0x00000630;
#endif//(TX_EDCA_QUEUE_TEST == 1)
#endif




#if (TX_EDCA_QUEUE_TEST == 1)

	SET_TX_ID_THOLD(0);
	SET_RX_ID_THOLD(0);
	SET_ID_TX_LEN_THOLD(0);
	SET_ID_RX_LEN_THOLD(0);
	//set minimun response  time out
	SET_MTX_DUR_RSP_TOUT_B(1);
	SET_MTX_DUR_RSP_TOUT_G(1);
	SET_PRESCALER_USTIMER(0x1ff);

#endif//TX_EDCA_QUEUE_TEST


#if(MAILBOX_TXRX_NO_LIMIT == 1)
    SET_TX_ID_THOLD(0);
    SET_RX_ID_THOLD(0);
    SET_ID_TX_LEN_THOLD(0);
    SET_ID_RX_LEN_THOLD(0);
#endif//MAILBOX_TXRX_NO_LIMIT

#if (TX_MIB_ENABLE == 1)
    //SET_TIM_CLS_MIB_EN(1);
    //SET_TIM_CLS_ONGOING(1);
    SET_G0_PKT_CLS_MIB_EN(1);
    SET_G0_PKT_CLS_ONGOING(1);
    SET_G1_PKT_CLS_MIB_EN(1);
    SET_G1_PKT_CLS_ONGOING(1);
    SET_Q0_PKT_CLS_MIB_EN(1);
    SET_Q0_PKT_CLS_ONGOING(1);
    SET_Q1_PKT_CLS_MIB_EN(1);
    SET_Q1_PKT_CLS_ONGOING(1);
    SET_Q2_PKT_CLS_MIB_EN(1);
    SET_Q2_PKT_CLS_ONGOING(1);
    SET_Q3_PKT_CLS_MIB_EN(1);
    SET_Q3_PKT_CLS_ONGOING(1);
#endif//TX_MIB_ENABLE

#if(TX_IGNORE_CCA_ENGINE == 1)
    SET_MTX_FORCE_CS_IDLE(1);
#endif//TX_IGNORE_CCA_ENGINE


 //Set Rx desciptor length of host
#if (MACTXRX_CONTROL_DURATION_SIM == 1)
    drv_mac_set_rx_info_size(0x04);
#endif

    return 0;
}


#include "../phy/drv_phy.h"
void block_all_traffic (void)
{
    // stop PHY RX
	drv_phy_off();
    // redirect all TX frame to CPU and drop them all
    drop_all_frame = true;
    drv_mac_set_tx_flow_data(0);
    drv_mac_set_tx_flow_ctrl(0);
    drv_mac_set_tx_flow_mgmt(0);
    drv_mac_set_rx_flow_data(0);
    drv_mac_set_rx_flow_ctrl(0);
    drv_mac_set_rx_flow_mgmt(0);
} // end of - block_all_traffic -

void stop_and_halt (void)
{
    block_all_traffic();
    /*lint -save -e716 */
    //Reset chip, it's inorder to trigger the host to do the recovery
    if(recover_enable>0){
        SET_SYS_ALL_RST(1);
    }
    else{
        while (1) {}
    }
    /*lint -restore */
} // end of - stop_and_halt -

extern void hex_dump (const void *addr, u32 size);

void stop_and_dump_and_halt (const void *addr, u32 size)
{
    block_all_traffic();
    hex_dump(addr, size);
    /*lint -save -e716 */
    //Reset chip, it's inorder to trigger the host to do the recovery
    if(recover_enable>0){
        SET_SYS_ALL_RST(1);
    }
    else{
        while (1) {}
    }
    /*lint -restore */
} // end of - stop_and_halt -

void stop_and_dump (const u32 *addr, u32 size)
{
    block_all_traffic();
    hex_dump(addr, size);
} // end of - stop_and_halt -








