#include <config.h>
#include <hdr80211.h>
#include <errno.h>
#include <pbuf.h>
#include <msgevt.h>
#include <common.h>
#include <cmd_def.h>
#include <mbox/drv_mbox.h>
#include <hw_regs_api.h>
#include "defrag.h"
#include "edca.h"
#include <edca/drv_edca.h>
#include <hwmac/drv_mac.h>
#include <phy/drv_phy.h>
#include <nav/drv_nav.h>
#include <security/drv_security.h>
#include <beacon/drv_beacon.h>
#include <mbox/drv_mbox.h>
#include "l34cs.h"
#include "mlme.h"
#include <log.h>
#include <ssv_pktdef.h>
#include "cmd_engine.h"
#include "soft-main.h"
#include "pide.h"
#include <dma/drv_dma.h>
#include <dbg_timer/dbg_timer.h>
#include "sta_info.h"
#include <ampdu/drv_ampdu.h>


#ifdef THROUGHPUT_TEST
#include <rtos.h>

OsTimer sdio_countTimer;
bool isCreateTimer = false;
u16 countTimer=1000;

u16 rxTimeStampOffset	= 0;
u8	throughputFixRate = 0;
u8	throughputDutMode = 0;

PKT_TxInfo *data_pkt;

extern  u32 soc_tx_speed;
extern  u32 soc_rx_speed;
extern  u32 soc_rx_rssi;
extern  u32 soc_rx_snr;
u32 rx_snr_total = 0;
u32 rx_rssi_total = 0;


#endif



#define MAIN_ERROR(fmt, ...)     LOG_ERROR(fmt, ##__VA_ARGS__)

extern void hex_dump(const void *data, u32 size);

struct tx_edca_queue_info{
    u16 shift;
    u16 mask;
    u8  max_len;
};

static struct tx_edca_queue_info tx_hw_queue_info[5];

#if (EDCA_DBG == 1)

int g_tx_report_cnt=0;
int g_tx_report_test_time=0;


#if (TX_REPORT_NOTIFY_HOST== 1)
void inline TxReport_NotifyHost(void)
{
	//Send event to notify host all data queue are empty.
	{
		HDR_HostEvent  *ev=NULL;
		HOST_EVENT_ALLOC(ev,SOC_EVT_TX_ALL_DONE , 0);
		HOST_EVENT_SET_LEN(ev,0);
		HOST_EVENT_SEND(ev);
	}
}
#endif//(TX_REPORT_NOTIFY_HOST== 1)


#endif

extern u8 srame_get_rx_pn(u8 wsid, bool unicast, u8 *rx_pn);
static void SoftMac_RxReplayFrameHandler( PKT_Info *pPktInfo, s32 reason )
{
    u64 pn=0;
    srame_get_rx_pn(pPktInfo->wsid,pPktInfo->unicast,(u8 *)&pn);
    printf("TrapReason=36: unicast=%d, seq=%d. ",pPktInfo->unicast,GET_HDR80211_SC_SEQNUM(pPktInfo));
    if(0==pn){
        //This is a compatible issue with MTK solution.
        //The default setting of PN code is 1, the MTK's default setting is 0, our hw compare the value of rx data and the internal value of PN,
        //if the value is larger than internal, hw set the trap reason 36 to this data frame.
        printf("pass to next module \n");
        //If the loca pn code is 0, it means it's the first secure data frame that we received
        pPktInfo->reason=0;
        if(FALSE==ENG_MBOX_NEXT((u32)pPktInfo))
        {
            printf("drop this packet\n");
            PBUF_MFree(pPktInfo);
        }
    }else{
        printf("drop this packet\n");
        PBUF_MFree((void *)pPktInfo);
    }
    return;
}

#if (AUTO_BEACON == 1)
extern void check_bcn(void);
#else
extern void* last_bcn_ptr;
#endif

#if (N_MODE_AMPDU_TX == 1)
extern bool ampdu_tx_report_handler (u32 ampdu_tx_pkt);
#endif
static void SoftMac_TxReportFrameHandler( PKT_Info *pPktInfo, s32 reason )
{

    PKT_TxInfo *pTxInfo = (PKT_TxInfo *)pPktInfo;
    u16 pktid = ((u32)pTxInfo & 0x0FFF0000)>>16;
#if (EDCA_DBG == 1)

	g_tx_report_cnt++;
	//Check content info
	if(((PKT_TxInfo*)pPktInfo)->frame_consume_time != g_tx_report_cnt*10 )
		ASSERT(FALSE);

//	LOG_PRINTF("tx report (reason=%u) !!\n", pPktInfo->reason);

	if(g_tx_report_cnt == GET_TX_SEG)
	{
		g_tx_report_test_time++;
		LOG_PRINTF("tx report test %d!!\n", g_tx_report_test_time);
		g_tx_report_cnt = 0;	//Reset

#if (TX_REPORT_NOTIFY_HOST== 1)
		TxReport_NotifyHost();
#endif

	}


#else// (EDCA_DBG == 1)
    if (   (reason == ID_TRAP_TX_XMIT_SUCCESS)
        || (reason  == ID_TRAP_TX_XMIT_FAIL)
        || pTxInfo->RSVD_0)
    {
        u32 txCounter = 0;
        u16 frameTx = 0, frameSuccess = 0, frameTotal = 0, frameRetry = 0;
        u8 resend = 0, wsid = 0, drate = 0;
#if 1
        if (reason != ID_TRAP_TX_XMIT_SUCCESS && reason != ID_TRAP_TX_XMIT_FAIL
            && pTxInfo->RSVD_0 != 0x2)
            printf("rc-%d %d\n", reason, pTxInfo->RSVD_0);
#endif
#if (AUTO_BEACON == 1)
        check_bcn();
#endif
#if (N_MODE_AMPDU_TX == 1)
        if(pTxInfo->aggregation == 1)
        {
            printf("[RC]:Warning!! Aggregation pkt get!! %08x\n", pTxInfo);
            ampdu_tx_report_handler((u32)pTxInfo);
            return;
        }
#endif

        if (pTxInfo->RSVD_0 == 0)
        {
/*
            printf("%s: buggy frame , %08x!!, global map [0x%08X], TX map [0x%08X], RX map [0x%08X], reason = %d \n", __FUNCTION__ ,pTxInfo, GET_CPU_ID_TB0, GET_TX_ID_TB0, GET_RX_ID_TB0, reason);
#if (AUTO_BEACON != 1)
            printf("last bcn = %08x\n", last_bcn_ptr);
#endif
 * */
            PBUF_MFree((void *)pTxInfo);
            return;
        }

        wsid = pTxInfo->wsid;
        drate = pTxInfo->drate_idx;

        if(wsid == 0)
            txCounter = GET_MTX_MIB_CNT0;
        else if(wsid == 1)
            txCounter = GET_MTX_MIB_CNT1;

        if ((reason == ID_TRAP_TX_XMIT_FAIL)
             && (pTxInfo->RSVD_0 != RC_RESEND)
             && (rate_control_is_resend_failreport((void *)pTxInfo) == TRUE))
        {
            resend = 1;
        }
        else
        {
            u32 frm_st = (pktid < 32)?(1<<pktid & GET_TX_ID_TB0):(1<<(pktid-32) & GET_TX_ID_TB1);
            u32 gbl_frm_st = (pktid < 32)?(1<<pktid & GET_ID_TB0):(1<<(pktid-32) & GET_ID_TB1);

#if (N_MODE_AMPDU_TX == 1)
            if ((pTxInfo->RSVD_0 == RC_PROBE) && (wsid == 0))
                rate_control_pid_return_probe_frame(pTxInfo);
            else
#endif
            {
                if (frm_st != 0) // Free it if it is a tx frame
                    PBUF_MFree((void *)pTxInfo);
                else if(gbl_frm_st != 0) // show and free it if it is not a tx frame but global
                {
                    if (pktid > 31)
                        printf("%s: why %08x is gbl frame gbl map[0x%08X], rx map[0x%08X]\n",__func__ ,pTxInfo, GET_ID_TB1,GET_RX_ID_TB1);
                    else
                        printf("%s: why %08x is gbl frame gbl map[0x%08X], rx map[0x%08X]\n",__func__ ,pTxInfo, GET_ID_TB0,GET_RX_ID_TB0);
                    PBUF_MFree((void *)pTxInfo);
                }
                else // show it if it is not a tx frame neither global frame
                {
                    if (pktid > 31)
                        printf("%s: disappear frame , %08x!! global map [0x%08X], TX map [0x%08X], RX map [0x%08X], reason = %d, RSVD_0 =%d \n",
                            __func__ ,pTxInfo, GET_ID_TB1, GET_TX_ID_TB1,GET_RX_ID_TB1, reason, pTxInfo->RSVD_0);
                    else
                        printf("%s: disappear frame , %08x!! global map [0x%08X], TX map [0x%08X], RX map [0x%08X], reason = %d, RSVD_0 =%d \n",
                           __func__ ,pTxInfo, GET_ID_TB0, GET_TX_ID_TB0,GET_RX_ID_TB0, reason, pTxInfo->RSVD_0);
#if (AUTO_BEACON == 0)
                    printf("last bcn = %08x\n", last_bcn_ptr);
#endif
                    hex_dump(pTxInfo, 64);
                }
            }
            pTxInfo = NULL;
        }


        frameTx = (u16)(txCounter & 0x000003ff);
        frameRetry = (u16)((txCounter >> 10) & 0x000003ff);
        frameSuccess = (u16)((txCounter >> 20) & 0x000003ff);

        if(wsid == 1)
        {
            frameRetry = (frameTx - frameSuccess)*7;
        }


        if ((frameTx < frameSuccess) || (frameRetry < (frameTx - frameSuccess)))
        {
            printf("[RC]:Warning!!\n");
            printf("mpduFrames[%d] mpduFrameRetry[%d] mpduFrameSuccess[%d]\n",frameTx,frameRetry,frameSuccess);
            frameTx = 0; //use frameTx = 0 to decide if do not update statistics in rate_control_pid_tx_status
            goto update_rc;
        }

        if(wsid == 0)
        {
            //Fix hardware issue
            //(1,7,0) shoubld be (1,6,0)
            frameRetry -= (frameTx - frameSuccess);
        }

        if((frameTx + frameRetry) > 255 )
        {
            printf("[RC]:Warning - 3!!\n");
            printf("mpduFrames[%d] mpduFrameRetry[%d] mpduFrameSuccess[%d]\n",frameTx,frameRetry,frameSuccess);
            frameTx = 0; //use frameTx = 0 to decide if do not update statistics in rate_control_pid_tx_status
            goto update_rc;
        }

        frameTotal = (frameTx + frameRetry);
update_rc:

        rate_control_pid_tx_status(wsid, drate, frameTx, frameTotal, frameSuccess, frameTx);
        //printf("frameTx = %d, frameTotal =%d, frameSuccess = %d\n",frameTx, frameTotal, frameSuccess);
        if (resend == 1)
        {
            //printf("!!! seq = %d to be resent\n", GET_HDR80211_SC_SEQNUM(pTxInfo));
            pTxInfo->drate_idx = 0;
            pTxInfo->crate_idx = 0;
            pTxInfo->RSVD_0 = RC_RESEND;
            pTxInfo->frame_consume_time = 0;
            pTxInfo->tx_report = 0;
            pTxInfo->dl_length = 0;
            pTxInfo->reason = 0;
            EDCA_Handler(pPktInfo, pTxInfo->reason);
            return ;
        }
        /*else if ((reason == ID_TRAP_TX_XMIT_FAIL) && (pTxInfo->RSVD_0 == 0x2))
        {
            printf("*** seq = %d has been sent once, just release report\n", GET_HDR80211_SC_SEQNUM(pTxInfo));
        }*/

        //_firmware_rate_control_report((u32)pPktInfo, pPktInfo->RSVD_0);
    }
	 LOG_PRINTF("%s(): drop frame (reason=%u) !!\n",
               __FUNCTION__, reason);

#endif// (EDCA_DBG == 1)

    if (pTxInfo != NULL)
    {
        u32 frm_st = (pktid < 32)?(1<<pktid & GET_TX_ID_TB0):(1<<((pktid-32)) & GET_TX_ID_TB1);
        if (frm_st != 0)
            PBUF_MFree((void *)pTxInfo);
/*
        else
        {
            if (pktid > 31)
                printf("%s: final disappear frame , %08x!! global map [0x%08X], TX map [0x%08X], RX map [0x%08X], reason = %d\n",
                   __FUNCTION__ ,pTxInfo, GET_ID_TB1, GET_TX_ID_TB1, GET_RX_ID_TB0, reason);
            else
                printf("%s: final disappear frame , %08x!! global map [0x%08X], TX map [0x%08X], RX map [0x%08X], reason = %d\n",
                   __FUNCTION__ ,pTxInfo, GET_ID_TB0, GET_TX_ID_TB0, GET_RX_ID_TB0, reason);
#if (AUTO_BEACON == 0)
            printf("last bcn = %08x\n", last_bcn_ptr);
#endif
            hex_dump(pTxInfo, 64);
        }
        */
    }

}


extern u16 null_data_status [WLAN_MAX_STA];
void SoftMac_NullFrameHandler( PKT_Info *pPktInfo, s32 reason )
{

    do
    {
        if (1 == drv_mac_get_op_mode())///* 0: STA, 1: AP, 2: IBSS, 3: WDS */
        {
            u16 fc = GET_HDR80211_FC(pPktInfo);
            HDR_HostEvent               *ev=NULL;
            struct cfg_null_data_info 	data_info;
            bool need_wake_up_task = false;
            memset(&data_info,0,sizeof(data_info));

            if(pPktInfo->wsid==0||pPktInfo->wsid==1)
            {
                if (IS_PM_SET(null_data_status[pPktInfo->wsid]) == IS_PM_SET(fc) )
                {
                    //printf("Null:wsid=%d,seq=%d,PM=%d,c=%d\n ",pPktInfo->wsid,GET_HDR80211_SC_SEQNUM(pPktInfo),IS_PM_SET(fc),GET_RO_MRX_LEN_CNT_0);
                    PBUF_MFree((void *)pPktInfo);
                    break;
                }
                else
                {
                    null_data_status[pPktInfo->wsid] = IS_PM_SET(fc)?0xFFFF:0x0000;
                }


                if(TRUE==IsSoftMacTaskSleeping())
                    need_wake_up_task =true;

                if(IS_PM_SET(fc)){

                    //drv_edca_queue_halt(EDCA_AC_BK+pPktInfo->wsid, TRUE);
                    ssv6xxx_SWQOutLock(pPktInfo->wsid,TX_SW_QUEUE);
                    //printf("Null:wsid=%d,seq=%d,c=%d,Lock\n ",pPktInfo->wsid,GET_HDR80211_SC_SEQNUM(pPktInfo),GET_RO_MRX_LEN_CNT_0);

                }else{

                    //drv_edca_queue_halt(EDCA_AC_BK+pPktInfo->wsid, FALSE);
                    ssv6xxx_SWQOutUnLock(pPktInfo->wsid,TX_SW_QUEUE);
                    //printf("Null:wsid=%d,seq=%d,c=%d,UnLock\n ",pPktInfo->wsid,GET_HDR80211_SC_SEQNUM(pPktInfo),GET_RO_MRX_LEN_CNT_0);

                }
                if (need_wake_up_task)
                {
                    //printf(" wake_up_task \n");
                    wake_up_task(MBOX_SOFT_MAC);
                }
            }
            //get data info
            data_info.Flags=IS_PM_SET(fc)?HOST_EVT_NULL_DATA_PM:0;

            if (IS_QOS_DATA(fc))
            {
                data_info.Flags |= HOST_EVT_NULL_DATA_QOS;
                data_info.Priority = GET_HDR80211_QC(pPktInfo)&0x000f;
            }

            GET_HDR80211_ADDRESS_2(&data_info.SAAddr, pPktInfo);
            data_info.q_len = ssv6xxx_SWQWaittingSize(pPktInfo->wsid,TX_SW_QUEUE);
            /*
                We don't need to care abot the hw queue status because we don't block the hw tx queue now,
                the length of hw queue is alwyays zero finally.
            */
            #if 0
            if(((pPktInfo->wsid == 0) && (GET_FFO7_CNT > 0)) ||
                ((pPktInfo->wsid == 1) && (GET_FFO8_CNT > 0)))
            {
                    data_info.q_len += 1;
            }
            #endif
            //set pPktInfo to HDR_HostEvent

            ev =(HDR_HostEvent *) pPktInfo;
            ev->c_type  = HOST_EVENT;
            ev->h_event = SOC_EVT_NULL_DATA;
            ev->len     = sizeof(struct cfg_null_data_info)+sizeof(HDR_HostEvent);
            //printf(" M_box_size:%d\n",OS_MsgQWaitingSize(MBOX_SOFT_MAC));
            memcpy(&ev->dat,&data_info, sizeof(struct cfg_null_data_info));

            //send to command engine. for central dispatch.
            HOST_EVENT_SEND(ev);

        }
        else
        {
            printf("%s(): Rx Null-data frame !!\n", __FUNCTION__);
            PBUF_MFree((void *)pPktInfo);
            LOG_PRINTF("%s(): Rx Null-data frame !!\n", __FUNCTION__);
        }
    } while (0);

	//PBUF_MFree((void *)pPktInfo);
}

#ifdef THROUGHPUT_TEST
u32 soc_sdio_rx_count=0;
u32 soc_mac_rx_count=0,soc_rx_snr=0,soc_rx_srssi = 0;
u8	soc_rx_verify_mode=0;
#ifdef TIMESTAMP_VERIFY
static u32 timeStampOffset=0,pretimestamp=0,stampTemp;
static u32 beaconCount=0;
#endif
#ifdef ENABLE_DEBUG_TIMER
    Time_T rx_begin_t;
#endif


void Sdio_CountHandler( void *args)
{
	if(soc_sdio_rx_count)
	{
		//LOG_PRINTF("sdio tx speed =%f (KB/sec) \n",(double)((1480/1024)*(soc_sdio_rx_count/(countTimer/1000))));
	}
	soc_sdio_rx_count = 0;
}
#endif


#if (AMPDU_TEST == 1)

u32 ampdu_rx_cnt = 0;
u32 AMPDU_ARY[64] = {0};
u32 g_ampdu_static = 0;
u32 g_ampdu_static_cnt = 0;

static void SoftMac_AMPDU_TrapHandler( PKT_Info *pPktInfo )
{
	if(g_ampdu_static)
	{
		g_ampdu_static_cnt++;
		////LOG_PRINTF("Recv APMDU Statistic - total cnt:%d\n", g_ampdu_static_cnt);
		PBUF_MFree(pPktInfo);
	}
	else
	{

		LOG_PRINTF("Recv Aggregrated PKT %08x - total cnt:%d\n",pPktInfo, ampdu_rx_cnt);
		taskENTER_CRITICAL();
		AMPDU_ARY[ampdu_rx_cnt]=(u32)pPktInfo;
		ampdu_rx_cnt++;
		taskEXIT_CRITICAL();
	}
}
#endif//AMPDU_TEST



static void SoftMac_NullTrapHandler( PKT_Info *pPktInfo, s32 reason )
{
#ifdef THROUGHPUT_TEST
#ifdef ENABLE_DEBUG_TIMER
	u32 time_offset,throughput;
	u16 fc;
	u8 *raw,*pointer;
	u32 rcpi_pos=0;
#endif
	if(IS_TX_PKT(pPktInfo))
	{
		extern u32 soc_sdio_tx_count;
		soc_sdio_rx_count ++;
		if(soc_sdio_rx_count % 1000 == 0)
		{
			//SOC rx == SDIO tx
			printf("SDIO tx/rx=%d/%d\n", soc_sdio_rx_count, soc_sdio_tx_count);
		}
	}
	else
	{
#ifdef ENABLE_DEBUG_TIMER
		if(soc_rx_verify_mode)
		{
			pointer=(u8*)pPktInfo;
			raw = (u8 *)pPktInfo+pPktInfo->hdr_offset;
			fc = (raw[1]<<8) | raw[0];

			if(IS_DATA_FRAME(fc))
			{

				if(soc_mac_rx_count)
				{
					if((soc_mac_rx_count%1000) == 0)
					{
						time_offset = dbg_get_elapsed(&rx_begin_t);

						throughput = pPktInfo->len;
						throughput *= soc_mac_rx_count;
						if(throughput < 0x1fffff)
							throughput = ((throughput * 1000)/(time_offset/1000))/1000;
						else
							throughput = throughput/(time_offset/1000);
						    printf("Rx [%d]Mb per 1000 secnonds \n",throughput<<3);
                            soc_rx_speed = throughput;
                            soc_rx_rssi  = (rx_rssi_total/1000);
                            soc_rx_snr = (rx_snr_total /1000);
							printf("Rx RSSI=[%x]SNR=[%x]\n",soc_rx_rssi,soc_rx_snr);
                            rx_rssi_total = 0;
                            rx_snr_total = 0;
					}

				}
				else
				{
					dbg_timer_init();
					dbg_get_time(&rx_begin_t);
				}
				rcpi_pos = pPktInfo->len+20;
				soc_mac_rx_count++;
                rx_rssi_total += *(pointer+rcpi_pos);
                rx_snr_total += *(pointer+rcpi_pos+1);
                //hex_dump(pointer,40);
                //printf("Rx RSSI=[%x]SNR=[%x]\n",*(pointer+rcpi_pos),*(pointer+rcpi_pos+1));

			}

		}
#endif

#ifdef TIMESTAMP_VERIFY
		u16 fc;
		u8 *raw,*pointer;

		pointer=(u8*)pPktInfo;
		raw = (u8 *)pPktInfo+pPktInfo->hdr_offset;
		fc = (raw[1]<<8) | raw[0];
		if(IS_BEACON(fc))
		{
			//LOG_PRINTF("time[%d]\n",*(u32 *)&pointer[rxTimeStampOffset]);
			if(pretimestamp)
			{
				beaconCount++;
				stampTemp = *(u32 *)&pointer[rxTimeStampOffset];
				timeStampOffset += (stampTemp - pretimestamp);
				pretimestamp = stampTemp;
				if((beaconCount % 10) == 0)
				{
					printf("T-[%d]\n",timeStampOffset/beaconCount);
					timeStampOffset = 0;
					beaconCount = 0;
				}
			}
			else
			{
				pretimestamp = *(u32 *)&pointer[rxTimeStampOffset];
			}
		}
#endif
#if 0
		printf("RX rate[%d]!\n",pPktInfo->drate_idx);

		u8 *pointer;
		u16 *seq_tag;

		pointer =(u8*)pPktInfo+pPktInfo->payload_offset+8;
		seq_tag = (u16 *)pointer;
		printf("s-%x\n",*seq_tag);
#endif
	}

    PBUF_MFree((void *)pPktInfo);
#else
#ifdef MODULE_TEST
	PBUF_MFree((void *)pPktInfo);
#else
#if (N_MODE_AMPDU_TX == 1)
    if ((pPktInfo->c_type == M0_TXREQ) || (pPktInfo->c_type == M2_TXREQ))
    {
        PKT_TxInfo *pTxInfo = (PKT_TxInfo *)pPktInfo;
        if (pTxInfo->aggregation == 1)
        {
            printf("%s: pTxInfo = %08x\n", __FUNCTION__, pTxInfo);
            ampdu_tx_report_handler((u32)pPktInfo);
            return;
        }
    }
#endif
    if(FALSE==ENG_MBOX_NEXT((u32)pPktInfo))
    {
        printf("drop this packet\n");
        PBUF_MFree(pPktInfo);
    }
#endif
#endif
}



static void SoftHCI_TxEtherTrapHandler( PKT_Info *pPktInfo, s32 reason )
{
    LOG_PRINTF("%s(): drop frame (reason=%u) !!\n",
               __FUNCTION__, pPktInfo->reason);
    PBUF_MFree((void *)pPktInfo);
}


static void SoftHCI_RxEtherTrapHandler( PKT_Info *pPktInfo, s32 reason )
{
	const u8 *dat;
	u16 eth_type;
	const PKT_RxInfo *PktRxInfo = (PKT_RxInfo *)pPktInfo;

	dat =(const u8 *)PktRxInfo + PktRxInfo->payload_offset + ETHER_MAC_LEN;
	eth_type = (dat[0]<<8) | dat[1];
	dat += ETHER_TYPE_LEN;

	switch(eth_type){
		case EAPOL_ETHER_TYPPE:
			{
				//ETHER_ADDR peer_mac;
            	//drv_mac_get_wsid_peer_mac(PktRxInfo->wsid, &peer_mac);
				if(pPktInfo->f80211)
				{
					//802.11
					;
                    PBUF_MFree((void *)pPktInfo);
				}
				else
				{
					//802.3
					//l2_packet_receive(dat,peer_mac.addr,eth_type, pPktInfo->len-ETHER_HDR_LEN);
                    MsgEvent *MsgEv=(MsgEvent *)msg_evt_alloc();
                    ASSERT_RET(MsgEv != NULL, EMPTY);
                    MsgEv->MsgType = MEVT_EAPOL;
                    MsgEv->MsgData = (u32)pPktInfo;
                    msg_evt_post(MBOX_MLME, MsgEv);
                    g_soc_mib.mlme.sw_mlme_rx_cnt ++;
				}
			}
			break;
	default:
			PBUF_MFree((void *)pPktInfo);
			break;
	}
}



typedef enum{
	EXTRA_NEXT_ENG           =     0,		//Contine to next hw module.
	EXTRA_STOP    	 		 =	   -1,		//No

}extra_result;


struct soft_mac_extra_info_tbl
{

	u8 				id;	//ssv6xxx_tx_extra_type
	extra_result             (*func)(struct soft_mac_extra_info_tbl *extra_info, PKT_TxInfo *pTxInfo, u8 *data);

};


extra_result softmac_generic_extrainfo(struct soft_mac_extra_info_tbl *extra_info, PKT_TxInfo *pTxInfo, u8 *u8_data)
{
	switch (extra_info->id)
	{
		case SSV6XXX_TX_FRAME_TIME:

			pTxInfo->frame_consume_time = *(u16*)u8_data;
			break;
		case SSV6XXX_TX_RTS_CTS_NAV:
			pTxInfo->rts_cts_nav = *(u16*)u8_data;
			break;
		case SSV6XXX_TX_DO_RTS_CTS:
			pTxInfo->do_rts_cts = *(u8*)u8_data;
			break;
		case SSV6XXX_TX_ACK_POLICY:
			pTxInfo->ack_policy= *(u8*)u8_data;
			break;
		case SSV6XXX_TX_TX_BRUST:
			pTxInfo->tx_burst= *(u8*)u8_data;
			break;
		case SSV6XXX_TX_TX_REPORT:
			pTxInfo->tx_report= *(u8*)u8_data;
			break;
		case SSV6XXX_TX_CRATE_IDX:
			pTxInfo->crate_idx= *(u8*)u8_data;
			break;
		case SSV6XXX_TX_DRATE_IDX:
			pTxInfo->drate_idx= *(u8*)u8_data;
			break;
		case SSV6XXX_TX_DL_LENGTH_IDX:
			pTxInfo->dl_length= *(u16*)u8_data;
			break;


		default:
			ASSERT(FALSE);
			break;
	}

	return EXTRA_NEXT_ENG;
}


//extern void cmd_eng_set_beacon(u32 *bcn, struct cfg_bcn_info *info);

#if (CONFIG_SIM_PLATFORM == 1)
void cmd_eng_bcn_enable_SIM(bool enable, u8 dtim_cnt, u16 bcn_itv);
#endif



#if (BEACON_DBG == 1)

extern PKT_TxInfo * test_bcn_addr;
extern struct cfg_bcn_info test_bcn_info;
extern PKT_TxInfo * cmd_eng_gen_test_bcn(PKT_TxInfo * pTxBcnInfo);

#endif//#if (BEACON_DBG == 1)


#if 0
extra_result softmac_bcn_extrainfo(struct soft_mac_extra_info_tbl *extra_info, PKT_TxInfo *pTxInfo, u8 *u8_data)
{

#if (CONFIG_SIM_PLATFORM == 0)
	taskENTER_CRITICAL();
#endif

 	if(extra_info->id == SSV6XXX_SET_INIT_BEACON)
 	{
//--------------------------------------------------------------------
//INIT_BEACON
//--------------------------------------------------------------------
		struct cfg_set_init_bcn *init_bcn = (struct cfg_set_init_bcn *)u8_data;



#if (BEACON_DBG == 1)

		if(test_bcn_addr)
		{
			LOG_PRINTF("free g_test_bcn_addr:%08x\n",test_bcn_addr);
			PBUF_MFree(test_bcn_addr);
		}
		test_bcn_info = init_bcn->bcn_info;
		test_bcn_addr = pTxInfo;


		LOG_PRINTF("set g_test_bcn_addr:%08x\n",test_bcn_addr);
		pTxInfo = cmd_eng_gen_test_bcn(test_bcn_addr);

#endif//#if (BEACON_DBG == 1)


		cmd_eng_set_beacon((u32*)pTxInfo, &init_bcn->bcn_info);

#if (CONFIG_SIM_PLATFORM == 0)
		drv_bcn_enable(TRUE, init_bcn->param.dtim_cnt, init_bcn->param.bcn_itv);
#else
		cmd_eng_bcn_enable_SIM(TRUE, init_bcn->param.dtim_cnt, init_bcn->param.bcn_itv);
#endif//CONFIG_SIM_PLATFORM




	}
	else
	{
//--------------------------------------------------------------------
//SET_BEACON
//--------------------------------------------------------------------
		struct cfg_bcn_info *bcn_info = (struct cfg_bcn_info *)u8_data;

#if (BEACON_DBG == 1)

		if(test_bcn_addr)
		{
			LOG_PRINTF("free g_test_bcn_addr:%08x\n",test_bcn_addr);
			PBUF_MFree(test_bcn_addr);
		}

		test_bcn_info = *bcn_info;
		test_bcn_addr = pTxInfo;

		LOG_PRINTF("set g_test_bcn_addr:%08x\n",test_bcn_addr);
		pTxInfo = cmd_eng_gen_test_bcn(test_bcn_addr);
#endif//#if (BEACON_DBG == 1)


		cmd_eng_set_beacon((u32*)pTxInfo, bcn_info);
	}


#if (CONFIG_SIM_PLATFORM == 0)
	taskEXIT_CRITICAL();
#endif


	return EXTRA_STOP;
}
#endif

//----------------------------------------
//			!!!IMPORTANT!!!
//Need to Add by Order
//----------------------------------------
static const struct soft_mac_extra_info_tbl extrainfo[]=
{
	{SSV6XXX_TX_FRAME_TIME	,softmac_generic_extrainfo},
	{SSV6XXX_TX_DO_RTS_CTS	,softmac_generic_extrainfo},
	{SSV6XXX_TX_ACK_POLICY	,softmac_generic_extrainfo},
	{SSV6XXX_TX_TX_BRUST	,softmac_generic_extrainfo},
	{SSV6XXX_TX_TX_REPORT	,softmac_generic_extrainfo},
	{SSV6XXX_TX_RTS_CTS_NAV	,softmac_generic_extrainfo},
	{SSV6XXX_TX_CRATE_IDX	,softmac_generic_extrainfo},
	{SSV6XXX_TX_DRATE_IDX	,softmac_generic_extrainfo},
	{SSV6XXX_TX_DL_LENGTH_IDX,softmac_generic_extrainfo},

	//{SSV6XXX_SET_INIT_BEACON,softmac_bcn_extrainfo},
	//{SSV6XXX_SET_BEACON,     softmac_bcn_extrainfo},

};


//Extra info
static void SoftMac_SWTrapHandler( PKT_Info *pPktInfo, s32 reason )
{
	u8 *end, *pos =(u8*)pPktInfo+pPktInfo->hdr_offset+pPktInfo->len-SSV_EXTRA_TOTAL_LEN_SIZE;
	u16 extra_len = *(u16*)pos;
	extra_result ret;
	bool bNextEng = TRUE;

	end=pos;
	//move to head of extra info
	pos-=extra_len;

	//Remove extra_info length.
	pPktInfo->len-=(extra_len+SSV_EXTRA_TOTAL_LEN_SIZE);

	//Reset to extra info;
	pPktInfo->reason = 0;
	pPktInfo->extra_info = 0;

	do
	{
		u8 len, idx=*pos;

		if (idx >= ARRAY_SIZE(extrainfo))
		{
			//ASSERT(FALSE);
			LOG_PRINTF("%s: Invalid packet.", __FUNCTION__);
			return;
		}

		pos++;
		len =*pos;
		pos++;
		ret = extrainfo[idx].func((struct soft_mac_extra_info_tbl *)&extrainfo[idx], (PKT_TxInfo *)pPktInfo, pos);

		if(ret == EXTRA_STOP)
			bNextEng=FALSE;

		pos+=len;

	} while (end>pos);

	if(bNextEng)
	{

            if(FALSE==ENG_MBOX_NEXT((u32)pPktInfo))
            {
                printf("drop this packet\n");
                PBUF_MFree(pPktInfo);
            }
	}
}


#if 0
static void SoftSec_DropPkt( PKT_Info *pPktInfo )
{
    LOG_PRINTF("%s(): drop frame (reason=%u) !!\n",
        __FUNCTION__, pPktInfo->reason);
    PBUF_MFree((void *)pPktInfo);
}

static void SoftSec_ErrorIndexHandler( PKT_Info *pPktInfo )
{
    LOG_PRINTF("%s(): drop frame (reason=%u) !!\n",
        __FUNCTION__, pPktInfo->reason);
	PBUF_MFree((void *)pPktInfo);
}

static void SoftSec_DecodeFailHandler( PKT_Info *pPktInfo )
{
    LOG_PRINTF("%s(): drop frame (reason=%u) !!\n",
        __FUNCTION__, pPktInfo->reason);

//	SoftSec_DropPkt(pPktInfo);
    PBUF_MFree((void *)pPktInfo);
}

static void SoftSec_ReplayerHandler( PKT_Info *pPktInfo )
{
    LOG_PRINTF("%s(): drop frame (reason=%u) !!\n",
        __FUNCTION__, pPktInfo->reason);
    //HW_ENG_NEXT(pPktInfo);
    PBUF_MFree((void *)pPktInfo);
}

static void SoftSec_BadLenHandler( PKT_Info *pPktInfo )
{
    LOG_PRINTF("%s(): drop frame (reason=%u) !!\n",
        __FUNCTION__, pPktInfo->reason);
//	SoftSec_DropPkt(pPktInfo);
    PBUF_MFree((void *)pPktInfo);
}

static void SoftSec_MICHandler( PKT_Info *pPktInfo )
{
    LOG_PRINTF("%s(): drop frame (reason=%u) !!\n",
        __FUNCTION__, pPktInfo->reason);

	//Trigger supplicant to heanle MIC error. Notify supplicant

    PBUF_MFree((void *)pPktInfo);
}

static void SoftSec_35Handler( PKT_Info *pPktInfo )
{
    LOG_PRINTF("%s(): drop frame (reason=%u) !!\n",
        __FUNCTION__, pPktInfo->reason);
	//Index error & decode fail.
	//SoftSec_DecodeFailHandler(pPktInfo);
    PBUF_MFree((void *)pPktInfo);
}

static void SoftSec_37Handler( PKT_Info *pPktInfo )
{
    LOG_PRINTF("%s(): drop frame (reason=%u) !!\n",
        __FUNCTION__, pPktInfo->reason);
	//Index error & Replay
	//SoftSec_ErrorIndexHandler(pPktInfo);
    PBUF_MFree((void *)pPktInfo);
}
static void SoftSec_38Handler( PKT_Info *pPktInfo )
{
    LOG_PRINTF("%s(): drop frame (reason=%u) !!\n",
        __FUNCTION__, pPktInfo->reason);
	//Replay & Decode fail
	//SoftSec_DecodeFailHandler(pPktInfo);
    PBUF_MFree((void *)pPktInfo);
}

static void SoftSec_39Handler( PKT_Info *pPktInfo )
{
    LOG_PRINTF("%s(): drop frame (reason=%u) !!\n",
        __FUNCTION__, pPktInfo->reason);
	//Index error & Replay & Decode fail
	//SoftSec_DecodeFailHandler(pPktInfo);
    PBUF_MFree((void *)pPktInfo);
}
#endif

static void SoftFrag_RejectHandler( PKT_Info *pPktInfo, s32 reason )
{
    LOG_PRINTF("%s(): drop frame (reason=%u) !!\n",
        __FUNCTION__, pPktInfo->reason);
    //HW_ENG_NEXT(pPktInfo);
    PBUF_MFree((void *)pPktInfo);
}

static void SoftFrag_FragHandler( PKT_Info *pPktInfo, s32 reason )
{
	// Pass NULL data frame
    if (pPktInfo->len == HDR80211_MGMT_LEN)
    {
    	if(FALSE==ENG_MBOX_NEXT((u32)pPktInfo))
    	{
            printf("drop this packet\n");
            PBUF_MFree(pPktInfo);
    	}
    }
    else
    {
        LOG_PRINTF("%s(): drop frame (reason=%u) !!\n",
                   __FUNCTION__, pPktInfo->reason);
    	PBUF_MFree((void *)pPktInfo);
    }
    //ssv6xxx_raw_dump((s8 *)pPktInfo, drv_mac_get_pbuf_offset() + HDR80211_MGMT_LEN);
    //HW_ENG_NEXT(pPktInfo);

}

#if 0
static void Soft_L34TxHandler( PKT_Info *pPktInfo, s32 reason )
{
    L34_Handler(pPktInfo, true);
}


static void Soft_L34RxHandler( PKT_Info *pPktInfo, s32 reason )
{
    L34_Handler(pPktInfo, false);
}
#endif

static void Soft_RxMgmtHandler( PKT_Info *pPktInfo, s32 reason )
{
    #if(PACKET_TRACE==1)
    pt((void *)pPktInfo,M_ENG_MAX,EN_LOC_RX_MGMT_HANDLE);
    #endif
    if((drv_mac_get_op_mode()==WLAN_AP))
    {
        if(FALSE==ENG_MBOX_SEND(M_ENG_HWHCI, (u32)pPktInfo))
        {
            printf("drop this packet\n");
            PBUF_MFree(pPktInfo);
        }
    }
    else
    {
        MsgEvent              *MsgEv;
        //const HDR80211_Mgmt   *Mgmt80211=HDR80211_MGMT(pPktInfo);
        //u32                    subtype = (u32)Mgmt80211->fc.subtype;
        MsgEv=(MsgEvent *)msg_evt_alloc();
        ASSERT_RET(MsgEv != NULL, EMPTY);
        MsgEv->MsgType = MEVT_PKT_BUF;
        MsgEv->MsgData = (u32)pPktInfo;
        msg_evt_post(MBOX_MLME, MsgEv);
        g_soc_mib.mlme.sw_mlme_rx_cnt ++;
    }
}


static void Soft_RxDataHandler( PKT_Info *pPktInfo, s32 reason )
{

#if 0
	HDR_HostEvent  *ev=NULL;
	u8 *ptr,*pos;
#endif


     /*LOG_PRINTF("%s(): drop frame (reason=%u) !!\n",
        __FUNCTION__, pPktInfo->reason);
     PBUF_MFree((void *)pPktInfo);*/
//LOG_PRINTF("===============================================================\n");
//LOG_PRINTF("*****Soft_RxDataHandler ****************************\n");
//LOG_PRINTF("*****pktinfo->fCmdIdx=%u,Length=%u ****************************\n",pPktInfo->fCmdIdx,pPktInfo->len);
//LOG_PRINTF("===============================================================\n");

#if 0

pos=(u8 *)pPktInfo + pPktInfo->hdr_offset;



LOG_PRINTF("*****pktinfo->fCmdIdx=%d,Length=%d *****************************\n",pPktInfo->fCmdIdx,pPktInfo->len);

HOST_EVENT_ALLOC(ev,SOC_EVT_LEAVE_RESULT , pPktInfo->hdr_len+4);
HOST_EVENT_DATA_PTR(ev, ptr);
*(u32 *)ptr=pPktInfo->len;
ptr+=4;
memcpy(ptr,pos,pPktInfo->hdr_len);
HOST_EVENT_SET_LEN(ev,pPktInfo->hdr_len+4);
HOST_EVENT_SEND(ev);
PBUF_MFree(pPktInfo);
#endif


#if (AMPDU_TEST == 1)
{
	PKT_RxInfo *pRxInfo = (PKT_RxInfo *)pPktInfo;
	if(pRxInfo->rx_phy_info.aggregation)
	{



		if(g_ampdu_static)
		{
			g_ampdu_static_cnt++;
			//LOG_PRINTF("Recv APMDU Statistic - total cnt:%d\n", g_ampdu_static_cnt);

			LOG_PRINTF("Recv MPDU SEQ %d\n", GET_HDR80211_SC_SEQNUM(pPktInfo));
			PBUF_MFree(pPktInfo);
			return;
		}
		else
		{
			taskENTER_CRITICAL();
			AMPDU_ARY[ampdu_rx_cnt]=(u32)pPktInfo;
			ampdu_rx_cnt++;
			taskEXIT_CRITICAL();
			LOG_PRINTF("Recv APMDU PKT %08x - total cnt:%d\n",pPktInfo, ampdu_rx_cnt);
			return;
		}



	}
}

#endif
        if(FALSE==ENG_MBOX_NEXT((u32)pPktInfo))
        {
            printf("drop this packet\n");
            PBUF_MFree(pPktInfo);
        }

}


static void Soft_RxCtrlHandler( PKT_Info *pPktInfo, s32 reason )
{

    u16 fc = GET_HDR80211_FC(pPktInfo);

    //Assume just AP mode will have PS-Poll Frame trap to MCU. Therefore no need to check if in AP mode.
    if(IS_PSPOLL(fc)&&!IS_RETRY_SET(fc))
    {
        //Send event to host
        HDR_HostEvent               *ev=NULL;
        struct cfg_ps_poll_info     ps_poll_info;
        s32 sw_q_len = ssv6xxx_SWQWaittingSize(pPktInfo->wsid,TX_SW_QUEUE);

        ssv6xxx_SWQOutConUnLock(pPktInfo->wsid,TX_SW_QUEUE,1);
        wake_up_task(MBOX_SOFT_MAC);

        GET_HDR80211_ADDRESS_2((u8*)&ps_poll_info.SAAddr, pPktInfo);
        ps_poll_info.q_len=sw_q_len-1;

        //set pPktInfo to HDR_HostEvent
        ev =(HDR_HostEvent *) pPktInfo;
        ev->c_type  = HOST_EVENT;
        ev->h_event = SOC_EVT_PS_POLL;
        ev->len     = sizeof(struct cfg_ps_poll_info)+sizeof(HDR_HostEvent);
        memcpy(&ev->dat,&ps_poll_info, sizeof(struct cfg_ps_poll_info));

        //send to command engine. for central dispatch.
        HOST_EVENT_SEND(ev);

    }
    else if (IS_RETRY_SET(fc))
    {
        //printf("IS_RETRY_SET\n");
        PBUF_MFree((void *)pPktInfo);
    }
    else if(IS_BAR(fc))
    {
        if(FALSE==ENG_MBOX_SEND(M_ENG_HWHCI, (u32)pPktInfo))
        {
            printf("drop BAR packet\n");
            PBUF_MFree(pPktInfo);
        }
    }
    else
    {
        LOG_PRINTF("%s(): drop frame (reason=%d) !!\n",
        __FUNCTION__, pPktInfo->reason);
        PBUF_MFree((void *)pPktInfo);
    }
}


static void Soft_FragmentHandler( PKT_Info *pPktInfo, s32 reason )
{
	/*
		Need to do something.
	*/
        if(FALSE==ENG_MBOX_NEXT((u32)pPktInfo))
        {
            printf("drop this packet\n");
            PBUF_MFree(pPktInfo);
        }
}

static void Soft_RxMgmtHandler_ACS( PKT_Info *pPktInfo, s32 reason )
{
    MsgEvent              *MsgEv;
    //const HDR80211_Mgmt   *Mgmt80211=HDR80211_MGMT(pPktInfo);
    //u32                    subtype = (u32)Mgmt80211->fc.subtype;
    MsgEv=(MsgEvent *)msg_evt_alloc();
    ASSERT_RET(MsgEv != NULL, EMPTY);
    MsgEv->MsgType = MEVT_PKT_BUF;
    MsgEv->MsgData = (u32)pPktInfo;
    msg_evt_post(MBOX_MLME, MsgEv);
    g_soc_mib.mlme.sw_mlme_rx_cnt ++;

}

bool send_deauth_to_sta (struct ETHER_ADDR_st *sta_mac)
{
    bool result = TRUE;
    gDevInfo->sta_deauth_info.counter ++;

    // check send deauth count; Example: 1,2,3->send deauth; 4,5,6->Not to send deauth
    if (!memcmp(&gDevInfo->sta_deauth_info.self_mac,sta_mac,sizeof(struct ETHER_ADDR_st)))
    {
        if(gDevInfo->sta_deauth_info.counter > SEND_DEAUTH_COUNT+NO_NEED_TO_SEND_DEAUTH_COUNT)
        {
            gDevInfo->sta_deauth_info.counter =1;
        }
        else if(gDevInfo->sta_deauth_info.counter > SEND_DEAUTH_COUNT)
        {
            result = FALSE;
        }
    }
    else
    {
        //For different station ; send deauth
        memcpy(&gDevInfo->sta_deauth_info.self_mac,sta_mac,sizeof(struct ETHER_ADDR_st));
        gDevInfo->sta_deauth_info.counter=1;
    }

    return result;
}

#ifdef AIRKISS_ENABLE
extern bool sconfig_user_mode;
//u32 Trap7Count=0;
void SoftMac_WsidNotFoundHandler( PKT_Info *pPktInfo, s32 reason  )
{

    if(drv_mac_get_op_mode()==WLAN_AP)
    {
        u16 fc=GET_HDR80211_FC(pPktInfo);
        if((IS_DATA_FRAME(fc)||IS_PSPOLL(fc))&&GET_HDR80211_FC_TODS(pPktInfo))
        {
            struct ETHER_ADDR_st sa;
            struct ETHER_ADDR_st da;
            GET_HDR80211_ADDRESS_1(&da,pPktInfo);
            GET_HDR80211_ADDRESS_2(&sa,pPktInfo);
            PBUF_MFree(pPktInfo);
            if(send_deauth_to_sta(&sa))
                mlme_send_deauth(6,&da,&sa,&da,FALSE);
        }else{
            //Add error handle to avoid exception cases
            PBUF_MFree(pPktInfo);
        }
    }
    else
    {

        if(sconfig_user_mode==TRUE){
            struct cfg_host_rxpkt *rxpkt = (struct cfg_host_rxpkt *)pPktInfo;
            //u16 fc = GET_HDR80211_FC(pPktInfo);
            //Trap7Count++;
            //printf("c_type=%d,seq=0x%x, len=%d,%s %s,unicast=%d,Trap7Count=%d\n",
            //    rxpkt->c_type,GET_HDR80211_SC_SEQNUM(pPktInfo),rxpkt->len,
            //    IS_RX_PKT(pPktInfo)?"RX":(IS_TX_PKT(pPktInfo)?"TX":"OTHER"),
            //    IS_DATA_FRAME(fc)?"DATA":(IS_MGMT_FRAME(fc)?"MGMT":"OTHER"),
            //    rxpkt->unicast,
            //    Trap7Count
            //    );
            rxpkt->fCmd=0x1;
            rxpkt->fCmdIdx=0;
            rxpkt->RSVD = gDevInfo->current_channel;
            if(-1==TX_FRAME((u32)pPktInfo))
            {
                PBUF_MFree(pPktInfo);
            }
        }else{
            MsgEvent *MsgEv = (MsgEvent *)(gOsFromISR ? msg_evt_alloc_0() : msg_evt_alloc());
            if(MsgEv!=NULL){
                MsgEv->MsgType = MEVT_SMART_CFG;
                MsgEv->MsgData = (u32)pPktInfo;
                msg_evt_post(MBOX_MLME, MsgEv);
            }else{
                PBUF_MFree(pPktInfo);
            }

        }
    }
}
#endif
/*---------------------------------------------------------------------------*/


typedef void (*SoftMac_Handler)(PKT_Info *, s32 reason);


/**
 *  Software MAC Hanlder Table:
 *
 *  Hardware traps tx/rx frames according to some pre-define reasons.
 *  If software interests in a reason, a callback function shall be
 *  registered in this table.
 */
const SoftMac_Handler sgSoftMac_Func[] = { /*lint -save -e611 */
    /* Reason ID=00: */ SoftMac_NullTrapHandler,
#if (AMPDU_TEST == 1)
	/* Reason ID=01: */ (SoftMac_Handler)SoftMac_AMPDU_TrapHandler,
#else//#if (AMPDU_TEST == 1)
	/* Reason ID=01: */ (SoftMac_Handler)NULL,
#endif//#if (AMPDU_TEST == 1)
    /* Reason ID=02: */ (SoftMac_Handler)NULL,
    /* Reason ID=03: */ (SoftMac_Handler)NULL,
    /* Reason ID=04: */ (SoftMac_Handler)NULL,
    /* Reason ID=05: */ (SoftMac_Handler)NULL,
    /* Reason ID=06: */ (SoftMac_Handler)NULL,
    /* Reason ID=07: */ (SoftMac_Handler)NULL,          //ID_TRAP_NO_WSID_FOUND, This kind of packets are handled in irq_mac_handle
    /* Reason ID=08: */ (SoftMac_Handler)NULL,
    /* Reason ID=09: */ SoftHCI_TxEtherTrapHandler,     //ID_TRAP_TX_ETHER_TRAP
    /* Reason ID=10: */ SoftHCI_RxEtherTrapHandler,     //ID_TRAP_RX_ETHER_TRAP
    /* Reason ID=11: */ (SoftMac_Handler)NULL,
    /* Reason ID=12: */ (SoftMac_Handler)NULL,
    /* Reason ID=13: */ SoftFrag_RejectHandler,         //ID_TRAP_MODULE_REJECT
    /* Reason ID=14: */ (SoftMac_Handler)NULL,          //ID_TRAP_NULL_DATA, This kind of packets are handled in irq_mac_handle
    /* Reason ID=15: */ (SoftMac_Handler)NULL,
    /* Reason ID=16: */ (SoftMac_Handler)SoftMac_TxReportFrameHandler,
    /* Reason ID=17: */ (SoftMac_Handler)SoftMac_TxReportFrameHandler,
    /* Reason ID=18: */ (SoftMac_Handler)NULL,
    /* Reason ID=19: */ (SoftMac_Handler)NULL,
    /* Reason ID=20: */ SoftFrag_FragHandler,           //ID_TRAP_FRAG_OUT_OF_ENTRY
    /* Reason ID=21: */ (SoftMac_Handler)NULL,
    /* Reason ID=22: */ (SoftMac_Handler)NULL,
    /* Reason ID=23: */ (SoftMac_Handler)NULL,
    /* Reason ID=24: */ (SoftMac_Handler)NULL,
    /* Reason ID=25: */ (SoftMac_Handler)NULL,
    /* Reason ID=26: */ (SoftMac_Handler)NULL,
    /* Reason ID=27: */ (SoftMac_Handler)NULL,
    /* Reason ID=28: */ (SoftMac_Handler)NULL,
    /* Reason ID=29: */ SoftMac_SWTrapHandler,          //ID_TRAP_SW_TRAP
    /* Reason ID=30: */ (SoftMac_Handler)NULL,
    /* Reason ID=31: */ (SoftMac_Handler)NULL,
	//For security module
    /* Reason ID=32: */ (SoftMac_Handler)NULL,          //ID_TRAP_SECURITY
    /* Reason ID=33: */ (SoftMac_Handler)NULL,          //ID_TRAP_KEY_IDX_MISMATCH
    /* Reason ID=34: */ (SoftMac_Handler)NULL,          //ID_TRAP_DECODE_FAIL
    /* Reason ID=35: */ (SoftMac_Handler)NULL,
    /* Reason ID=36: */ (SoftMac_Handler)SoftMac_RxReplayFrameHandler,  //ID_TRAP_REPLAY
    /* Reason ID=37: */ (SoftMac_Handler)NULL,
    /* Reason ID=38: */ (SoftMac_Handler)NULL,
    /* Reason ID=39: */ (SoftMac_Handler)NULL,
    /* Reason ID=40: */ (SoftMac_Handler)NULL,          //ID_TRAP_SECURITY_BAD_LEN
    /* Reason ID=41: */ (SoftMac_Handler)NULL,          //ID_TRAP_TKIP_MIC_ERROR

    /* SW pre-defined trapping: */
    /* Reason ID=42: */ (SoftMac_Handler)NULL,          //Soft_L34TxHandler,
    /* Reason ID=43: */ (SoftMac_Handler)NULL,          //Soft_L34RxHandler,
    /* Reason ID=44: */ (SoftMac_Handler)NULL,          //Soft_DeFragHandler,
    /* Reason ID=45: */ EDCA_Handler,
    /* Reason ID=46: */ Soft_RxDataHandler,
    /* Reason ID=47: */ Soft_RxMgmtHandler,
    /* Reason ID=48: */ Soft_RxCtrlHandler,
    /* Reason ID=49: */ Soft_FragmentHandler,               //ID_TRAP_SW_FRAGMENT
    /* Reason ID=50: */ Soft_RxMgmtHandler_ACS,
}; /*lint -restore */



#ifdef __LOOP_BACK_TEST__
#include <hwmac/drv_mac.h>
bool SoftMac_IsFirstTimeTraptoCPU(PKT_Info *PktInfo)
{
	int i;
	u32 shiftbit, ENG_ID;

	for(i=0;i<=PktInfo->fCmdIdx;i++)
	{
		shiftbit = i << 2;
		ENG_ID = ((PktInfo->fCmd & (0x0F<<shiftbit))>>shiftbit);

		if(ENG_ID == M_ENG_CPU)
			return FALSE;
	}

	return TRUE;
}

#endif//#ifdef __LOOP_BACK_TEST__

s32 SoftMac_GetSWReason( PKT_Info *PktInfo)
{
    s32 reason;
    u32 flows;
    u16 fc;
    fc = GET_HDR80211_FC(PktInfo);

    /* Trap due to software pre-defined trapping */
    if (IS_RX_PKT(PktInfo)) {
        if (IS_MGMT_FRAME(fc))
            flows = gRxFlowMgmtReason;
        else if (IS_CTRL_FRAME(fc))
            flows = gRxFlowCtrlReason;
        else
            flows = gRxFlowDataReason;
    }
    else {
        if (IS_MGMT_FRAME(fc))
            flows = gTxFlowMgmtReason;
        else
            flows = gTxFlowDataReason;
    }
    fc = PktInfo->fCmdIdx<<2;
    reason = (flows>>(((s32)PktInfo->fCmdIdx)<<2))&0x0F;

    return reason;
}
/**
 * s32 SoftMac_GetTrapReason() - Get the trapping reason.
 *
 * The trapping reason can be dividied into following 3 catalogs:
 *
 *      @ Hardware Module Error:
 *      @ Extra-Info set:
 *      @ Software Pre-defined error:
 */
static s32 SoftMac_GetTrapReason( PKT_Info *PktInfo )
{
    s32 reason;

#if (CONFIG_SIM_PLATFORM == 0)
    //ASSERT_PKT(!(IS_MGMT_FRAME(fc)&&IS_TX_PKT(PktInfo)), PktInfo);

    //AP mode will trap frame to EDCA handler....this code should be here
    //
    //if (IS_MGMT_FRAME(fc) && IS_TX_PKT(PktInfo))
    //{
    //   extern void hex_dump(const void *addr, u32 size);
    //    LOG_ERROR("TX MGMT Trap!\n");
    //    //hex_dump(PktInfo, 8);
    //    return 3;
    //}

#endif

#ifdef __LOOP_BACK_TEST__
    //If the second stop is MCU, we jsut got this frame from MAC RX.
    //Therefore we need to add fCmdIdx.(MAC RX Bug)
    if(PktInfo->c_type == M0_RXEVENT &&
    SoftMac_IsFirstTimeTraptoCPU(PktInfo))
    {
        PktInfo->reason = 0;
        PktInfo->RSVD_1 = 0;
        PktInfo->fCmdIdx++;
    }

#endif//__LOOP_BACK_TEST__

    /* Trap due to hardware module error */
    if (PktInfo->reason != 0)
        return PktInfo->reason;

    /* Trap due to extra_info is set */
    /*if (PktInfo->RSVD_2 == 1)
        return PktInfo->reason;*/
    reason = SoftMac_GetSWReason(PktInfo);

    if (reason > 0) {
        reason += (ID_TRAP_SW_START-1);
    }

    return reason;

}


#ifdef _AMPDU_SOC_
extern void agg_rx_release_reorder_timeout(u32 data);
#endif

inline int _is_hw_queue_ready_for_rx(void);
inline int _is_hw_queue_ready_for_tx(u32 qID);
inline s32 ssv6xxx_PushRxPacketToNextHW(void);
inline s32 ssv6xxx_PushTxPacketToNextHW(void);
bool IsSoftMacTaskSleeping(void)
{
    if((IS_TX_DATA_QUEUE_OUTPUT_LOCK(TX_DATA_QUEUE_0)||IS_TX_DATA_QUEUE_EMPTY(TX_DATA_QUEUE_0))&& //Don't need to service tx dq[0]
        (IS_TX_DATA_QUEUE_OUTPUT_LOCK(TX_DATA_QUEUE_1)||IS_TX_DATA_QUEUE_EMPTY(TX_DATA_QUEUE_1))&& //Don't need to service tx dq[1]
        IS_TX_DATA_QUEUE_EMPTY(LAST_TX_DATA_QUEUE)&& //Don't need to service last tx queue, this queue is for managerment and broadcast frames
        IS_RX_DATA_QUEUE_EMPTY()&& //Don't need to service rx dq[1]
        msg_evt_empty(MBOX_SOFT_MAC)){
        return TRUE;
    }else{
        return FALSE;
    }
}
/*
     0: OK
    -1: FAIL
*/
u8 g_highPriorityQueue=0;
s32 ssv6xxx_TxPacketToNextHW(void *p, s32 next_hw)
{


    PKT_TxInfo *pTxInfo = (PKT_TxInfo *)p;
    s32 qID=0,ret=0;
    u16 fc = GET_HDR80211_FC(pTxInfo);
    bool need_wake_up_task = FALSE;
    bool bSentDirectly=FALSE;

    if(pTxInfo->c_type == HOST_CMD){
        printf("HOST_CMD should not enqueue to sw queue\n");
    }

    if((0==pTxInfo->unicast)||!IS_DATA_FRAME(fc)){
        qID=LAST_TX_DATA_QUEUE;
    }else{
        qID=pTxInfo->wsid;
    }


    if(IS_TX_DATA_QUEUE_EMPTY(qID)&&IS_TX_DATA_QUEUE_OUTPUT_UNLOCK(qID)){

        if(!gOsFromISR) taskENTER_CRITICAL();

        if(_is_hw_queue_ready_for_tx(qID)){
            drv_mailbox_send(next_hw,(u32)p);
            bSentDirectly=TRUE;
        }

        if(!gOsFromISR) taskEXIT_CRITICAL();
    }


    if(TRUE==bSentDirectly) return 0;

    if(TRUE==IsSoftMacTaskSleeping())
        need_wake_up_task=TRUE;

    if(IS_AMPDU_RETRY_PACKET(pTxInfo)){
        ret=ssv6xxx_SWQInsert(qID,TX_SW_QUEUE,p,next_hw);
        g_highPriorityQueue|=(1<<qID);
    }else{
        ret=ssv6xxx_SWQEnqueue(qID,TX_SW_QUEUE,p,next_hw);
    }

    if(0==ret){

        ssv6xxx_PushTxPacketToNextHW();

        if(TRUE==need_wake_up_task)
            wake_up_task(MBOX_SOFT_MAC);

        return 0;

    }else{
        printf("TX packet enqueue fail (qID=%d) \n",qID);
        return -1;
    }
}

/*
     0: OK
    -1: FAIL
*/

s32 ssv6xxx_RxPacketToNextHW(void *p, s32 next_hw)
{
    bool need_wake_up_task = FALSE;

    bool bSentDirectly=FALSE;

    if(IS_RX_DATA_QUEUE_EMPTY()){

        if(!gOsFromISR) taskENTER_CRITICAL();

        if((next_hw==M_ENG_HWHCI)&&_is_hw_queue_ready_for_rx()){
            drv_mailbox_send(M_ENG_HWHCI,(u32)p);
            bSentDirectly=TRUE;
        }

        if(!gOsFromISR) taskEXIT_CRITICAL();
    }

    if(TRUE==bSentDirectly) return 0;

    if(TRUE==IsSoftMacTaskSleeping())
        need_wake_up_task=TRUE;

    if(0==ssv6xxx_SWQEnqueue(0,RX_SW_QUEUE,p, next_hw)){

        ssv6xxx_PushRxPacketToNextHW();

        if(TRUE==need_wake_up_task) wake_up_task(MBOX_SOFT_MAC);

        return 0;

    }else{
        printf("RX packet enqueue fail\n");
        return -1;
    }


}
/*
     0: OK
    -1: FAIL
*/
s32 ssv6xxx_PacketToNextHW(void *p, s32 next_hw)
{
    PKT_Info *pPktInfo = (PKT_Info *)p;

    if(IS_TX_PKT(pPktInfo)||(pPktInfo->c_type == HOST_CMD)){
        /*
            This is a special case for fw sending the mgmt frame.
            The flow for mgmt frame is "HCI"->"MCU"->HW TXQUEUE
        */
        if(next_hw==M_ENG_HWHCI)
        {
            return ssv6xxx_RxPacketToNextHW(p, next_hw);
        }
        else
        {
            return ssv6xxx_TxPacketToNextHW(p, next_hw);
        }
    }else{ //RX packet and HOST_EVT
        return ssv6xxx_RxPacketToNextHW(p, next_hw);
    }
}

extern u8 recover_enable;
extern u8 g_txQueueMapping[MAX_TX_DATA_QUEUE];
static int _is_hw_queue_ready(u32 qID, bool IsforTx)
{
    bool tx_queue_full=0;
    u8 maxQueueDepth=0;
    //No matter tx or rx case, we must always keep the input queue of CPU is empty.
    if(IsforTx==TRUE){

        tx_queue_full=(REG32(ADR_RD_FFOUT_CNT2)>>tx_hw_queue_info[g_txQueueMapping[qID]].shift)&tx_hw_queue_info[g_txQueueMapping[qID]].mask;
        maxQueueDepth=tx_hw_queue_info[g_txQueueMapping[qID]].max_len;

        //We found that If TX4 is in the full status, this hw queue will be blocked
        //Although we can't make sure that every queue has this problem, we avoid the any tx queue is in the full status
        tx_queue_full= (tx_queue_full <= (maxQueueDepth-2))?FALSE:TRUE;

        //The input queue of CPU must be empty, and the output queue of tx is not over than "MAX_DEPATH-1"
        if(GET_FF0_EMPTY&&(!tx_queue_full)) return TRUE;
    }else{

        //The input queue of CPU must be empty, and the output queue of HCI is not full
        if(GET_FF0_EMPTY&&(!GET_CH1_FFO_FULL))return TRUE;
    }

    return FALSE;
}

inline int _is_hw_queue_ready_for_tx(u32 qID)
{
    return _is_hw_queue_ready(qID,TRUE);
}

inline int _is_hw_queue_ready_for_rx(void)
{
    return _is_hw_queue_ready(0,FALSE);
}

inline s32 ssv6xxx_PushRxPacketToNextHW(void)
{
    PKT_RxInfo *pRxInfo=NULL;
    s32 next_hw=0;
    if(!IS_RX_DATA_QUEUE_EMPTY()){

        if(!gOsFromISR) taskENTER_CRITICAL();

KEEP_SENDING:

        if(TRUE==_is_hw_queue_ready_for_rx()){

            if(0==ssv6xxx_SWQDequeue(0, RX_SW_QUEUE,(void **)&pRxInfo,&next_hw)){
                if((pRxInfo!=0)&&(PBUF_isPkt((u32)pRxInfo))){
                    drv_mailbox_send(next_hw,(u32)pRxInfo);
                }else{
                    printf("\33[31mGet a wrong rx packet(0x%x)\33[0m\n",(u32)pRxInfo);
                }
            }
            //In ISR, we wish we can send all data in the showest time.
            if(gOsFromISR&&(!IS_RX_DATA_QUEUE_EMPTY())) goto KEEP_SENDING;
        }


        if(!gOsFromISR) taskEXIT_CRITICAL();

    }

    return 0;
}

#define MAX_OUT_QUEUE_LOCK_TIME 10 //unit is 10ms
inline s32 ssv6xxx_PushTxPacketToNextHW(void)
{
    int i=0;
    bool response_ps_pull=FALSE;
    PKT_TxInfo *pTxInfo=NULL;
    u16 fc;
    s32 next_hw=0;
    u32 diff_time, curr_time;
    u8 hw_queue_ready=0,highPriorityReady=0;

KEEP_SENDING:
    if(!gOsFromISR) taskENTER_CRITICAL();

    hw_queue_ready=0;
    highPriorityReady=0;

    if(!gOsFromISR)
        curr_time=xTaskGetTickCount();
    else
        curr_time=xTaskGetTickCountFromISR();

    for(i=0;i<MAX_TX_DATA_QUEUE;i++)
    {
        if((g_highPriorityQueue&(1<<i))&&IS_TX_DATA_QUEUE_OUTPUT_UNLOCK(i))
        {
            highPriorityReady++;
        }
    }


    for(i=0;i<MAX_TX_DATA_QUEUE;i++){

        if(highPriorityReady){
            if(0==GET_NUMBER_OF_HIGH_PRIORITY_PACKET(i))
                //It means that this queue doesn't have the high priority packets(retry packets are the high priority packets), so skip this queue
                continue;
        }

        //Wehn the rx resources is not enouth to receiving the next null datas or other rx data frames,
        //no input data frames can unlock the tx queue, it will cause the resources faill into the dead lock.
        //Here, we monitor the time, if this tx queue is in the lock status over 100ms, we force to unlock this tx queue.
        if(IS_TX_DATA_QUEUE_OUTPUT_LOCK(i)&&!IS_TX_DATA_QUEUE_EMPTY(i)){

            diff_time=(curr_time<IS_TX_DATA_QUEUE_OUTPUT_LAST_LOCK_TIME(i))?
                    ((curr_time+0xFFFFFFFF)-IS_TX_DATA_QUEUE_OUTPUT_LAST_LOCK_TIME(i)):
                    (curr_time-IS_TX_DATA_QUEUE_OUTPUT_LAST_LOCK_TIME(i));

            if(diff_time>MAX_OUT_QUEUE_LOCK_TIME){
                printf("force to unlock tx[%d]\n",i);

                ssv6xxx_SWQOutUnLock(i,TX_SW_QUEUE);
            }
        }

        if(IS_TX_DATA_QUEUE_OUTPUT_UNLOCK(i)&&!IS_TX_DATA_QUEUE_EMPTY(i)){

            if(TRUE==_is_hw_queue_ready_for_tx(i)){

                if(TRUE==IS_TX_DATA_QUEUE_OUTPUT_AUTO_LOCK(i)) response_ps_pull = TRUE;

                if(0==ssv6xxx_SWQDequeue(i,TX_SW_QUEUE, (void **)&pTxInfo,&next_hw)){

                    if((pTxInfo!=NULL)&&(PBUF_isPkt((u32)pTxInfo))){

                        fc = GET_HDR80211_FC(pTxInfo);
                        //If there are not any STA's data in AP, AP will send a null(or QOS null) data frame to STA after AP getting the PS_PULL
                        //We check the output data, if this data is a null(or QoS null) data, it means that this is the last data frame,
                        // if this data is not a null(or Qos null) data, it means that there are some data frames in AP, we must set MOREDATA to 1
                        if((response_ps_pull==TRUE)&&(!(IS_NULL_DATA(fc)||IS_QOS_NULL_DATA(fc)))) {
                            SET_HDR80211_FC_MOREDATA(pTxInfo);
                        }
                        drv_mailbox_send(next_hw,(u32)pTxInfo);
                    }else{
                        printf("\33[31mGet a wrong tx packet(0x%x)\33[0m\n",(u32)pTxInfo);
                    }

                }
                hw_queue_ready|=(1<<i);
            }

            }

        if(0==GET_NUMBER_OF_HIGH_PRIORITY_PACKET(i)){
            g_highPriorityQueue&=(~(1<<i));
        }

    }

    if(gOsFromISR&&(g_highPriorityQueue&hw_queue_ready)) goto KEEP_SENDING;

    if(!gOsFromISR) taskEXIT_CRITICAL();

    return 0;
}

#define IsSoftMacTaskNeedToSleep() IsSoftMacTaskSleeping()
void soft_mac_task( void *args )
{

    PKT_Info *pPktInfo;
    MsgEvent *MsgEv;
    s32 res, TrapReason;
    u8 dest[6]={0};
    u16 fc;

    //log_printf("%s() Task started.\n", __FUNCTION__);
    //drv_mac_init() ;

    /*lint -save -e716 */
    while(1)
    { /*lint -restore */

#if (WLAN_MAX_STA!=2)
#pragma message("========================================================================")
#pragma message("You should modify this condiction checking beacuse WLAN_MAX_STA is not 2")
#pragma message("========================================================================")
error
#endif
        if(IsSoftMacTaskNeedToSleep())
        {
            res = msg_evt_fetch(MBOX_SOFT_MAC,MAX_DELAY, &MsgEv); //If msg queue is empty, soft_mac_task fall sleeping.
        }
        else{
            res = msg_evt_fetch(MBOX_SOFT_MAC,0, &MsgEv);
        }


        if(res == OS_SUCCESS){
            gSoftTaskCount++;
            switch(MsgEv->MsgType)
            { /*lint -save -e788 */
                /**
                *  Message from CPU HW message queue. Dispatch the message
                *  to corresponding function for processing according to the
                *  reason field of PKT_Info. The reason info is assiged by HW
                *  MAC.
                */
                case MEVT_PKT_BUF:
                    pPktInfo = (PKT_Info *)MsgEv->MsgData;
                    ASSERT_PKT(PBUF_isPkt(pPktInfo), pPktInfo);

                    if ((pPktInfo->c_type == HOST_CMD)||(pPktInfo->c_type == HOST_EVENT))
                    {
                        /* Host Command, pass the event to the cmd_engine */
                        g_soc_mib.sw_cmd_rx_cnt ++;
                        CmdEngine_handle(MsgEv);
                        msg_evt_free(MsgEv);
                    }
                    else
                    {
                        msg_evt_free(MsgEv);

                        TrapReason = SoftMac_GetTrapReason(pPktInfo);

                        if ((TrapReason >= ID_TRAP_MAX) || sgSoftMac_Func[TrapReason] == NULL)
                        {
                        	GET_HDR80211_ADDRESS_1(dest,pPktInfo);
                        	fc = GET_HDR80211_FC(pPktInfo);
                        	/* Fatal Error: Shall not happend !!! */
                        	if (TrapReason < ID_TRAP_MAX)
                        	{
                                printf("WSID:%d, Trap %d, %s:%s,seq=%d,d=%2x:%2x:%2x:%2x:%2x:%2x\n",
                                    pPktInfo->wsid,
                                    TrapReason,
                                    (IS_RX_PKT(pPktInfo)?"R":"T"),
                                    (IS_DATA_FRAME(fc)?"D":(IS_MGMT_FRAME(fc)?"M":"C")),
                                    GET_HDR80211_SC_SEQNUM(pPktInfo),
                                    dest[0],dest[1],dest[2],dest[3],dest[4],dest[5]);
                                //hex_dump(pPktInfo, 64);
                        	}
                        	else
                        	{
                                printf("WSID:%d, Exceptional trap %d,%s:%s, seq=%d,d=%2x:%2x:%2x:%2x:%2x:%2x\n",
                                    pPktInfo->wsid,
                                    TrapReason,
                                    (IS_RX_PKT(pPktInfo)?"R":"T"),
                                    (IS_DATA_FRAME(fc)?"D":(IS_MGMT_FRAME(fc)?"M":"C")),
                                    GET_HDR80211_SC_SEQNUM(pPktInfo),
                                    dest[0],dest[1],dest[2],dest[3],dest[4],dest[5]);
                        		//hex_dump(pPktInfo, 64);
                        	}
                            PBUF_MFree((void *)pPktInfo);
                        }
                        else{
                            sgSoftMac_Func[TrapReason](pPktInfo, TrapReason);
                        }
                    }
                    break;

                /**
                *  Message from software timer timeout event.
                */
#ifdef _AMPDU_SOC_
                case MEVT_SOFT_TIMER:
                    switch(MsgEv->MsgData)
                    {
                        case SOFT_MAC_TIMER_REORDER_BUFFER:
                        	agg_rx_release_reorder_timeout(MsgEv->MsgData1);
                        	break;
                        default:
                        	break;
                    }
                    LOG_PRINTF("MsgType  = %d\n", MsgEv->MsgType);
                    LOG_PRINTF("MsgData  = %x\n", MsgEv->MsgData);
                    LOG_PRINTF("MsgData1  = %x\n", MsgEv->MsgData1);
                    LOG_PRINTF("MsgData2  = %x\n", MsgEv->MsgData2);
                    LOG_PRINTF("MsgData3  = %x\n", MsgEv->MsgData3);
                    msg_evt_free(MsgEv);
                    break;
#endif
                case MEVT_TX_DONE:

                    if(MsgEv->MsgData == 6)
                    {
                        //IRQ_EDCA_PREDTIM
                        //HDR_HostEvent   *ev=NULL;
                        //u8 *msg;

                        //HOST_EVENT_ALLOC(ev, SOC_EVT_DTIM_EXPIRED, sizeof(u32));
                        //msg = HOST_EVENT_DATA_PTR(MsgEv);

                        //HOST_EVENT_SET_LEN(ev, sizeof(u32));
                        //HOST_EVENT_SEND(ev);
                    }
                    else
                    {
                        //IRQ_EDCA_BK:
                        //IRQ_EDCA_BE:
                        //IRQ_EDCA_VI:
                        //IRQ_EDCA_VO:
                        enum edca_tx_queue_subtype queuetype;
                        queuetype = (enum edca_tx_queue_subtype)MsgEv->MsgData;
                        msg_evt_free(MsgEv);
                        EDCA_MacTxIntNotify(queuetype);
                    }

                    break;
                case MEVT_WAKEUP_TASK:
                    //printf("soft_make wakeup\n");
                    msg_evt_free(MsgEv);
                break;
                default:
                    LOG_PRINTF("%s(): Unknown MsgEvent type(%02x) !!\n",
                    __FUNCTION__, MsgEv->MsgType);
                    PBUF_MFree((void *)MsgEv->MsgData);
                    msg_evt_free(MsgEv);
                break;
            }; /*lint -restore */
        }
        ssv6xxx_PushTxPacketToNextHW();
        ssv6xxx_PushRxPacketToNextHW();
    }
}

#if (N_MODE_AMPDU_TX == 1)
extern void amdpu_init(void);
#endif

s32 soft_mac_init(void)
{
    /**
        * Allocate one packet buffer from hardware for the purpose of
        * storing the following information:
        *
        * @ Security Key Storage
        * @ PHY Info Table
        */
    u32 regval = 0 ;
    regval = drv_mac_get_security_pkt_id();
    regval = regval << 16;
    regval= regval | 0x80000000;
    g_soc_table = (struct soc_table_st *)regval;

    //g_soc_table = (struct soc_table_st *)PBUF_MAlloc_Raw(sizeof(struct soc_table_st), 0, SOC_PBUF);
    //ASSERT_RET(g_soc_table != NULL, 0);
    //drv_dma_memset((void *)g_soc_table, 0, (u16)sizeof(struct soc_table_st));

	wpa_drv_init();

    EDCA_init();
#if (CONFIG_SIM_PLATFORM == 0)
    //defrag_init();
#endif


   /**
       *  Initialize IEEE 802.11 MLME (MAC Layer Management Entity)
       *  This also initializes bss, sta and ap.
       */
    ASSERT_RET( mlme_init() == OS_SUCCESS, 0 );

    /* MIB Counter Init: */

    memset((void *)&g_soc_mib, 0, sizeof(struct soc_mib_st));

    drv_phy_init();

#if (N_MODE_AMPDU_TX == 1)
    amdpu_init();
#endif
/*
#define GET_FFO6_CNT  (((REG32(ADR_RD_FFOUT_CNT2))& 0x0000000f ) >> 0)
#define GET_FFO7_CNT  (((REG32(ADR_RD_FFOUT_CNT2))& 0x000003e0 ) >> 5)
#define GET_FFO8_CNT  (((REG32(ADR_RD_FFOUT_CNT2))& 0x00007c00 ) >> 10)
#define GET_FFO9_CNT  (((REG32(ADR_RD_FFOUT_CNT2))& 0x000f8000 ) >> 15)
#define GET_FFO10_CNT  (((REG32(ADR_RD_FFOUT_CNT2))& 0x00f00000 ) >> 20)

*/
    tx_hw_queue_info[0].max_len=M_ENG_TX_EDCA0_OUT;
    tx_hw_queue_info[0].shift=0;
    tx_hw_queue_info[0].mask=0xF;

    tx_hw_queue_info[1].max_len=M_ENG_TX_EDCA1_OUT;
    tx_hw_queue_info[1].shift=5;
    tx_hw_queue_info[1].mask=0x1F;

    tx_hw_queue_info[2].max_len=M_ENG_TX_EDCA2_OUT;
    tx_hw_queue_info[2].shift=10;
    tx_hw_queue_info[2].mask=0x1F;

    tx_hw_queue_info[3].max_len=M_ENG_TX_EDCA3_OUT;
    tx_hw_queue_info[3].shift=15;
    tx_hw_queue_info[3].mask=0x1F;

    tx_hw_queue_info[4].max_len=M_ENG_TX_MNG_OUT;
    tx_hw_queue_info[4].shift=20;
    tx_hw_queue_info[4].mask=0xF;
    #if(PACKET_TRACE==1)
    pt_init();
    #endif
    return 0;
}


#if (EDCA_DBG == 1)
u8 TxFrameCnt= 0;
#endif


//----------------------------------------------------
#if (MACTXRX_CONTROL_ENABLE == 1)
#if (MACTXRX_CONTROL_DURATION_SIM == 1)

//------------------------------------------------------------------


#if 0
u32 SoftMac_GetRandomT()
{
	int r = 1;

 	r = g_soc_table->rdm[RdmIdx];
 	RdmIdx++;

	//LOG_PRINTF("frame:%d random no:%d\n",RdmIdx, r);

	return r;
}
#endif

//---------------------------------------------------------------



#define OFDM_PLCP_BITS          22			//Service(16)+tail bits(6)
#define HT_RC_2_STREAMS(_rc)    ((((_rc) & 0x78) >> 3) + 1)
#define L_STF                   8
#define L_LTF                   8
#define L_SIG                   4
#define HT_SIG                  8
#define HT_STF                  4
#define HT_LTF(_ns)             (4 * (_ns))
#define SYMBOL_TIME(_ns)        ((_ns) << 2) /* ns * 4 us */
#define SYMBOL_TIME_HALFGI(_ns) (((_ns) * 18 + 4) / 5)  /* ns * 3.6 us */
static const u16 bits_per_symbol[][2] = {
	/* 20MHz 40MHz */
	{    26,   54 },     /*  0: BPSK */
	{    52,  108 },     /*  1: QPSK 1/2 */
	{    78,  162 },     /*  2: QPSK 3/4 */
	{   104,  216 },     /*  3: 16-QAM 1/2 */
	{   156,  324 },     /*  4: 16-QAM 3/4 */
	{   208,  432 },     /*  5: 64-QAM 2/3 */
	{   234,  486 },     /*  6: 64-QAM 3/4 */
	{   260,  540 },     /*  7: 64-QAM 5/6 */
};


#define HT_SIFS_TIME        		10

#define CCK_SIFS_TIME        		10
#define CCK_PREAMBLE_BITS   		144
#define CCK_PLCP_BITS        		48

#define OFDM_SIFS_TIME        		16	//a SIFS+ Signal extension
#define OFDM_PREAMBLE_TIME    		20	//TPreamble+TSignal
#define OFDM_PLCP_BITS        		22
#define OFDM_SYMBOL_TIME      		4

#define OFDM_SIFS_TIME_HALF     	32
#define OFDM_PREAMBLE_TIME_HALF 	40
#define OFDM_PLCP_BITS_HALF     	22
#define OFDM_SYMBOL_TIME_HALF   	8

#define OFDM_SIFS_TIME_QUARTER      64
#define OFDM_PREAMBLE_TIME_QUARTER  80
#define OFDM_PLCP_BITS_QUARTER      22
#define OFDM_SYMBOL_TIME_QUARTER    16


#define HT_SIGNAL_EXT				6		//For 2.4G



enum WLAN_RC_PHY_TYPE {
	WLAN_RC_PHY_CCK,
	WLAN_RC_PHY_OFDM,

//	WLAN_RC_PHY_HT_20_SS,
//	WLAN_RC_PHY_HT_20_SS_HGI,

	WLAN_RC_PHY_HT_MIX,
	WLAN_RC_PHY_HT_GF,
};



static u32 SoftMac_FrameDurationHT(enum WLAN_RC_PHY_TYPE phy, u8 rateidx, u32 frameLen, bool bShortGI);




void SoftMac_get_cfg( enum WLAN_RC_PHY_TYPE *phy, int *kbps, int *basic_kbps, int *cts_rts_kbps, bool *shortPreamble, u8 *rateidx, bool *bShortGI)
{
	 *phy = WLAN_RC_PHY_OFDM;
	 *kbps			= 54000;
	 *basic_kbps	= 24000;
	 *cts_rts_kbps	= 24000;
	 *shortPreamble = TRUE;
	 *rateidx= 0;
	 *bShortGI = FALSE;
}


u16 SoftMac_FrameDuration(enum WLAN_RC_PHY_TYPE phy, int kbps,
			   u32 frameLen, bool shortPreamble, u8 rateidx, bool bShortGI)
{
	u32 bitsPerSymbol, numBits, numSymbols, phyTime, txTime;

	//if (kbps == 0)
	//	return 0;

	switch (phy) {
	case WLAN_RC_PHY_CCK:	//802.11b
		/*
		 * 802.11b or 802.11g with 802.11b compatibility:
		 * 18.3.4: TXTIME = PreambleLength + PLCPHeaderTime +
		 * Ceiling(((LENGTH+PBCC)x8)/DATARATE). PBCC=0.
		 *
		 * 802.11 (DS): 15.3.3, 802.11b: 18.3.4
		 * aSIFSTime = 10 usec
		 * aPreambleLength = 144 usec or 72 usec with short preamble
		 * aPLCPHeaderLength = 48 usec or 24 usec with short preamble
		 */
		phyTime = CCK_PREAMBLE_BITS + CCK_PLCP_BITS;
		if (shortPreamble)
			phyTime >>= 1;
		numBits = frameLen << 3;
		txTime = CCK_SIFS_TIME + phyTime + ((numBits * 1000) / kbps);//kbps=

		break;
	case WLAN_RC_PHY_OFDM:	//802.11g, 5g

		/*
		 * OFDM:
		 *
		 * N_DBPS = DATARATE x 4
		 * N_SYM = Ceiling((16+8xLENGTH+6) / N_DBPS)
		 *	(16 = SIGNAL time, 6 = tail bits)
		 *
		 * TXTIME = TPREAMBLE + TSIGNAL + TSYM  !N Ceiling((16 + 8 !N LENGTH + 6)/NDBPS)

		 * 19.8.3.1 TXTIME = T_PREAMBLE + T_SIGNAL + T_SYM x N_SYM + Signal Ext
		 *(table 17.4)
		 * T_SYM = 4 usec
		 * 802.11a - 17.5.2: aSIFSTime = 16 usec
		 * 802.11g - 19.8.4: aSIFSTime = 10 usec +
		 *	signal ext = 6 usec
		 */

		//20MHz
		{
			bitsPerSymbol = (kbps * OFDM_SYMBOL_TIME) / 1000;		//DBPS

			numBits = OFDM_PLCP_BITS + (frameLen << 3);

			numSymbols = DIV_ROUND_UP(numBits, bitsPerSymbol);		//NSYM

			txTime = OFDM_SIFS_TIME + OFDM_PREAMBLE_TIME
				+ (numSymbols * OFDM_SYMBOL_TIME);
		}
		break;

#if 0
	case WLAN_RC_PHY_HT_MIX:
	case WLAN_RC_PHY_HT_GF:

		txTime = SoftMac_FrameDurationHT(phy, rateidx, frameLen,
			    b40MHZ, bShortGI, shortPreamble);
		break;
#endif

	default:
		//txTime = SoftMac_FrameDurationHT(phy, rateidx, frameLen,
		//	    b40MHZ, bShortGI, shortPreamble);

		txTime = SoftMac_FrameDurationHT(phy, rateidx, frameLen, bShortGI);
		break;
	}

	return txTime;
}




/*
 * rix 	-(MCS index) rate index
 * pktlen 	- total bytes(802.11 payload+FCS)
 * width  	- (20MHz/40MHz)0 for 20 MHz, 1 for 40 MHz
 * half_gi - (GI)to use 4us v/s 3.6 us for symbol time
 *
 *shortPreamble
 *
 *  according to spec 20.4.3
 *
 *
 */
static u32 SoftMac_FrameDurationHT(enum WLAN_RC_PHY_TYPE phy, u8 rateidx, u32 frameLen, bool bShortGI)
{
	u32 nbits, nsymbits, duration, nsymbols;
	int streams;
	//u8 n_es;
	/* find number of symbols: PLCP + data */
	streams = 1;//HT_RC_2_STREAMS(rateidx);

	nbits = (frameLen << 3) + OFDM_PLCP_BITS;
	nsymbits = bits_per_symbol[rateidx % 8][0] * streams;			//NDBPS					a symbol own data bits

	nsymbols = (nbits + nsymbits - 1) / nsymbits;					//NSYM					how many symbol

	if (!bShortGI)
		duration = SYMBOL_TIME(nsymbols);							//TSYM					how many time per symbol
	else
	{
		if(WLAN_RC_PHY_HT_MIX == phy)
			duration = DIV_ROUND_UP(SYMBOL_TIME_HALFGI(nsymbols), 4)<<2;		//20-91
		else
			duration = SYMBOL_TIME_HALFGI(nsymbols);							//TSYM 20-93
	}







	/* add up duration for legacy/ht training and signal fields */

	duration += L_STF + L_LTF + L_SIG + HT_SIG + HT_STF + HT_LTF(streams)+HT_SIGNAL_EXT;	//HT Mixed mode



	//HT_GF = HT_MIX-12us(Legacy L_STF+L_LTF+L_SIG(20us)- 8us)
	//HT_SFT(4us)  v.s. HT_GF_STF(8us)
	//HT_LTF1(4us) v.s. HT_LTF1_GF(8us)


	if (phy == WLAN_RC_PHY_HT_GF)
		duration -=12;


	duration += HT_SIFS_TIME;
	//?Plus SIFS
	return duration;
}

//static __le16 ieee80211_duration(PKT_TxInfo *pTxInfo, int group_addr)
static u16 SoftMac_frame_duration(PKT_TxInfo *pTxInfo)
{
	//----------------------------------------------------------------------------------------------
	//Config
	//get info from somewhere
	//bool use_short_preamble = TRUE;

	enum WLAN_RC_PHY_TYPE phy = WLAN_RC_PHY_OFDM;
	int kbps = 54000;
	int basic_kbps = 24000;
	int cts_rts_kbps = 24000;
	bool shortPreamble = TRUE;
	u8 rateidx = 0;
	//bool b40MHZ = FALSE;
	bool bShortGI = FALSE;
	//bool bGF = FALSE;


//----------------------------------------------------------------------------------------------
	int next_frag_len;
	int dur;
//	int rate, mrate, erp, dur, i;
	u16 fc;
	u8 *raw;
	SoftMac_get_cfg( &phy, &kbps, &basic_kbps, &cts_rts_kbps, &shortPreamble, &rateidx, &bShortGI);
	/*
	 * data and mgmt (except PS Poll):
	 * - during CFP: 32768
	 * - during contention period:
	 *   if addr1 is group address: 0
	 *   if more fragments = 0 and addr1 is individual address: time to
	 *      transmit one ACK plus SIFS
	 *   if more fragments = 1 and addr1 is individual address: time to
	 *      transmit next fragment plus 2 x ACK plus 3 x SIFS
	 *
	 * IEEE 802.11, 9.6:
	 * - control response frame (CTS or ACK) shall be transmitted using the
	 *   same rate as the immediately previous frame in the frame exchange
	 *   sequence, if this rate belongs to the PHY mandatory rates, or else
	 *   at the highest possible rate belonging to the PHY rates in the
	 *   BSSBasicRateSet
	 */
	//hdr = (struct HDR80211_Data_st *)((u8*)pTxInfo+pTxInfo->hdr_offset); //(struct ieee80211_hdr *)skb->data;
	raw = (u8*)pTxInfo+pTxInfo->hdr_offset;
	fc = (raw[1]<<8) | raw[0];

	if (IS_CTRL_FRAME(fc)) {
		/* TODO: These control frames are not currently sent by
		 * mac80211, but should they be implemented, this function
		 * needs to be updated to support duration field calculation.
		 *
		 * RTS: time needed to transmit pending data/mgmt frame plus
		 *    one CTS frame plus one ACK frame plus 3 x SIFS
		 * CTS: duration of immediately previous RTS minus time
		 *    required to transmit CTS and its SIFS
		 * ACK: 0 if immediately previous directed data/mgmt had
		 *    more=0, with more=1 duration in ACK frame is duration
		 *    from previous frame minus time needed to transmit ACK
		 *    and its SIFS
		 * PS Poll: BIT(15) | BIT(14) | aid
		 */
		return 0;
	}

	/* data/mgmt */
	if (0 /* FIX: data/mgmt during CFP */)
		return (le16)(32768);

	if ((raw[4]&0x01)) /* Group address as the destination - no ACK */
		return 0;

// 	/* Individual destination address:
// 	 * IEEE 802.11, Ch. 9.6 (after IEEE 802.11g changes)
// 	 * CTS and ACK frames shall be transmitted using the highest rate in
// 	 * basic rate set that is less than or equal to the rate of the
// 	 * immediately previous frame and that is using the same modulation
// 	 * (CCK or OFDM). If no basic rate set matches with these requirements,
// 	 * the highest mandatory rate of the PHY that is less than or equal to
// 	 * the rate of the previous frame is used.
// 	 * Mandatory rates for IEEE 802.11g PHY: 1, 2, 5.5, 11, 6, 12, 24 Mbps
// 	 */
// 	rate = -1;
// 	/* use lowest available if everything fails */
// 	mrate = sband->bitrates[0].bitrate;
// 	for (i = 0; i < sband->n_bitrates; i++) {
// 		struct ieee80211_rate *r = &sband->bitrates[i];
//
// 		if (r->bitrate > txrate->bitrate)
// 			break;
//
// 		if (tx->sdata->vif.bss_conf.basic_rates & BIT(i))
// 			rate = r->bitrate;
//
// 		switch (sband->band) {
// 		case IEEE80211_BAND_2GHZ: {
// 			u32 flag;
// 			if (tx->sdata->flags & IEEE80211_SDATA_OPERATING_GMODE)
// 				flag = IEEE80211_RATE_MANDATORY_G;
// 			else
// 				flag = IEEE80211_RATE_MANDATORY_B;
// 			if (r->flags & flag)
// 				mrate = r->bitrate;
// 			break;
// 		}
// // 		case IEEE80211_BAND_5GHZ:
// // 			if (r->flags & IEEE80211_RATE_MANDATORY_A)
// // 				mrate = r->bitrate;
// // 			break;
// 		case IEEE80211_NUM_BANDS:
// 			WARN_ON(1);
// 			break;
// 		}
// 	}
//
//
// 	if (rate == -1) {
// 		/* No matching basic rate found; use highest suitable mandatory
// 		 * PHY rate */
// 		rate = mrate;
// 	}


	/* Don't calculate ACKs for QoS Frames with NoAck Policy set */
	if (pTxInfo->ack_policy == 0x00) /* 0x00 = normal ack */
	/* Time needed to transmit ACK
	 * (10 bytes + 4-byte FCS = 112 bits) plus SIFS; rounded up
	 * to closest integer */
		dur = SoftMac_FrameDuration(phy, basic_kbps, 10+FCS_LEN, shortPreamble, rateidx, bShortGI);
	else
		dur = 0;


	if (pTxInfo->next_frag_pid) {
		PKT_TxInfo *pFragTxInfo;

		/* Frame is fragmented: duration increases with time needed to
		 * transmit next fragment plus ACK and 2 x SIFS. */
		dur *= 2; /* ACK + SIFS */
		/* next fragment */
		pFragTxInfo = (PKT_TxInfo *)PBUF_MapIDtoPkt(pTxInfo->next_frag_pid);
		next_frag_len = pFragTxInfo->len+FCS_LEN;
		dur += SoftMac_FrameDuration(phy, kbps, next_frag_len, shortPreamble, rateidx, bShortGI);
	}

	return (dur);
}

//__le16 ieee80211_generic_frame_duration(struct ieee80211_hw *hw,
//struct ieee80211_vif *vif,
//	size_t frame_len,
//struct ieee80211_rate *rate)



u16 SoftMac_generic_frame_duration(enum WLAN_RC_PHY_TYPE phy, int kbps, u32 frameLen, bool shortPreamble, u8 rateidx, bool bShortGI)
{
	u16 dur = 0;


	//----------------------------------------------------------------------------------------------
	//Config
// 	enum WLAN_RC_PHY_TYPE phy = WLAN_RC_PHY_OFDM;
// 	int kbps = 54000;
// 	int basic_kbps = 24000;
// 	bool shortPreamble = TRUE;
// 	u8 rateidx = 0;
// 	bool b40MHZ = FALSE;
// 	bool bShortGI = FALSE;
// 	bool bGF = FALSE;
	//----------------------------------------------------------------------------------------------

	dur += SoftMac_FrameDuration(phy, kbps, frameLen, shortPreamble, rateidx, bShortGI);
	dur -= HT_SIFS_TIME;


	return dur;
}


//__le16 ieee80211_rts_ctstoself_duration(PKT_TxInfo *pTxInfo)
u16 SoftMac_rts_ctstoself_duration(PKT_TxInfo *pTxInfo)
{

	u16 dur = 0;

//----------------------------------------------------------------------------------------------
//Config

	enum WLAN_RC_PHY_TYPE phy = WLAN_RC_PHY_OFDM;
	int kbps = 54000;
	int basic_kbps = 24000;
	int cts_rts_kbps = 24000;
	bool shortPreamble = TRUE;
	u8 rateidx = 0;
	//bool b40MHZ = FALSE;
	bool bShortGI = FALSE;
	//bool bGF = FALSE;

	SoftMac_get_cfg( &phy, &kbps, &basic_kbps, &cts_rts_kbps, &shortPreamble, &rateidx, &bShortGI);
//----------------------------------------------------------------------------------------------

	//RTS/CTS   = CTS duration+Data frame duration+(ACK duration)
	//CTS to self = Data frame duration+(ACK duration)


	if(pTxInfo->do_rts_cts == 0)
		return 0;
	else if(pTxInfo->do_rts_cts == 1)//1: need RTS/CTS
	{
		/* CTS duration */
		dur += SoftMac_FrameDuration(phy, cts_rts_kbps, 10+FCS_LEN, shortPreamble, rateidx, bShortGI);
	}
	else
	{}


	// do_rts_cts:2;   /* 1: need RTS/CTS, 2: CTS protection*/
	/* Data frame duration */
	dur += SoftMac_FrameDuration(phy, kbps, pTxInfo->len+FCS_LEN, shortPreamble, rateidx, bShortGI);

	/* ACK duration */
	if (pTxInfo->ack_policy == 0x00) /* 0x00 = normal ack */
		dur += SoftMac_FrameDuration(phy, basic_kbps, 10+FCS_LEN, shortPreamble, rateidx, bShortGI);

	return (dur);
}







u8 *GET_QOS_CTRL(HDR80211_Data_4addr *hdr)
{
	if (hdr->fc.fromDS && hdr->fc.toDS)
		return (u8 *)hdr + 30;
	else
		return (u8 *)hdr + 24;
}


#if(CONFIG_SIM_PLATFORM==0)

extern struct ecda_info *gEDCAInfo;
#endif

//---------------------------------





#endif//MACTXRX_CONTROL_DURATION_SIM
#endif//MACTXRX_CONTROL_ENABLE
