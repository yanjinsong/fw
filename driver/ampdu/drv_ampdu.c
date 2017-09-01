#include <config.h>
#include <mbox/drv_mbox.h>
#include <timer/drv_timer.h>
//#include <bsp/irq.h>
#include <log.h>
#include <pbuf.h>
#include <ssv_pktdef.h>
#include <hdr80211.h>
#include "drv_ampdu.h"
#include <hwmac/drv_mac.h>
#include <regs/ssv6200_aux.h>
#include <apps/mac80211/pide.h>
#include <lib/ssv_lib.h>
#include <apps/mac80211/sta_info.h>

#if (N_MODE_AMPDU_TX == 1)
//#define DUMP_AMPDU_TX_DESC
//#define AMPDU_HAS_LEADING_FRAME
#define MAX_AMPDU_RCD_NUM        16
#define skb_ssn(hdr)        (((hdr)->seq_ctrl)>>4)
#define BITMAP_NUM 64
static volatile u8 current_ampdu_idx;
extern bool stainfo_is_ampdu_ready(u8 wsid);
extern u8 recover_enable;

#define AMPDU_DBG 0
#if (AMPDU_DBG == 1)
#define PRINTF(fmt, ...) \
    do { \
        printf(fmt, ##__VA_ARGS__); \
    } while (0)
#else
#define PRINTF(fmt, ...)
#endif

#ifdef DUMP_AMPDU_TX_DESC
bool to_dump_first_retry_tx_desc = true;
#endif // DUMP_AMPDU_TX_DESC

// IEEE802.11 frame header of block ack.
typedef struct AMPDU_BLOCKACK_st
{
	u16			frame_control;
	u16			duration;
	u8			ra_addr[6];
	u8			ta_addr[6];

    u16         BA_ack_ploicy:1;
    u16         multi_tid:1;
    u16         compress_bitmap:1;
    u16         reserved:9;
    u16         tid_info:4;

    u16         BA_fragment_sn:4;
    u16         BA_ssn:12;
    u8         BA_sn_bit_map[8];
} __attribute__((packed)) AMPDU_BLOCKACK, *p_AMPDU_BLOCKACK;

#define AMPDU_ST_PEND 0x01
#define AMPDU_ST_RESEND 0x02
#define AMPDU_ST_BA_RECIEVED 0x04
#define AMPDU_ST_TIMEOUT 0x08
#define AMPDU_ST_JAMOUT 0x10
#define AMPDU_ST_DROP 0x20
#define AMPDU_ST_VALID 0x80

struct AMPDU_TX_Data_S
{
    PKT_TxInfo* tx_pkt;
    u8 retry_count;
};

struct AMPDU_TX_RCD
{
    u16 ssn;
    u8 retry_cnt_timeout:4;
    u8 retry_cnt_jam:4;
    u8 retry_cnt_flag; //0: it didn't count to ampdu_tx_redundance_count. 1: it has already counted to ampdu_tx_redundance_count
    u8 tid;
    u8 wsid;
    u8 st;
};

struct BA_Data_S
{
    u8 wsid;
    u8 tid;
    s16 ssn;
    u8 bitmap[8];
};


static struct AMPDU_TX_Data_S last_ampdu;
static struct BA_Data_S ba_window;
static struct AMPDU_TX_RCD ampdu_tx_data[MAX_AMPDU_RCD_NUM];

u8 AMPDU_RX_ENABLE = 0;
u8 AMPDU_RX_BUF_SIZE = AMPDU_RX_BUF_SIZE_DEFAULT;
u8 AMPDU_TX_ENABLE = 0;
u8 AMPDU_TX_Maintry = AMPDU_MAIN_TRY;
u8 AMPDU_TX_MaxRetry = AMPDU_FW_MAX_RETRY;
#if (N_MODE_AMPDU_RETRY_TO_B_MODE == 1)
u8 AMPDU_TX_LastRetryInBMode = 1;
u8 AMPDU_TX_LastRetryBModeRate = 2; //5.5M
u8 AMPDU_TX_LastRetryBModeOnce = 1;
u8 AMPDU_TX_BlockSessionAtNotNmodeRate = 1;
#endif

// AMPDU TX statistics
/* BA related */
u32    ampdu_tx_ba_count = 0;
u32    ampdu_tx_miss_ba_count = 0;
u32    ampdu_tx_ba_match_last_ampdu_new_count = 0;
u32    ampdu_tx_ba_match_last_ampdu_retried_count = 0;
//u32    ampdu_tx_lose_ba_count = 0;

/* AMPDU related */
u32    ampdu_tx_retry_count[(AMPDU_FW_MAX_RETRY+1)];
//0 for each packet record, once packet retry at specific time store at specific index
u32    ampdu_tx_drop_count = 0;         //Once pakcet is DROP
u32    ampdu_tx_timeout_count = 0;      //Once pakcet is timeout
u32    ampdu_tx_jamout_count = 0;       //Once pakcet is jamout
u32    ampdu_tx_txreport_received_count = 0;
u32    ampdu_tx_timeout_miss_count = 0; // timeout but seq doesn't match seq of last ampdu

/* AMPDU match BA*/
u32    ampdu_tx_new_txreport_match_BA_count = 0;
u32    ampdu_tx_retry_txreport_match_BA_count = 0;
u32    ampdu_tx_redundance_count=0;


#define RC_SUC_TX_ST(_wsid, _hw_rate_idx, _prb_pkt_complete) rate_control_pid_tx_status(_wsid, _hw_rate_idx, 1, 1, 1, _prb_pkt_complete)
#define RC_FAIL_TX_ST(_wsid, _hw_rate_idx, _prb_pkt_complete) rate_control_pid_tx_status(_wsid, _hw_rate_idx, 1, 1, 0, _prb_pkt_complete)
#define RC_GET_MR_PARAMS(_p) (struct mr_params *)((u32)_p + sizeof(PKT_TxInfo) - MR_PARAM_LEN)

extern u8 g_prb_pkt_enable;

/*
 *     c0  c1  c2  c3  c4  c5  c6  c7
 *             a6  a7                             (a << 4) & 0x30
 *     a0  a1  a2  a3  a4  a5  a6  a7             (a)
 *     a7  a0  a1  a2  a3  a4  a5  a6             (a >> 1) | (a << 7)
 *     a6  a6  a0  a1  a2  a3  a4  a5             (a >> 2) | ((a & 2) ? 0xC0 : 0)
 *     b6  b6  b0  b1  b2  b3  b4  b5             (b >> 2) | ((b & 2) ? 0xC0 : 0)
 *     b7  b0  b1  b2  b3  b4  b5  b6             (b >> 1) | (b << 7)
 *     b0  b1  b2  b3  b4  b5  b6  b7             (b)
 *             b6  b7                             (b << 4) & 0x30
 *
 */

inline bool _tx_memory_check(void* pTxInfo)
{
    u16 pktid = ((u32)pTxInfo & 0x0FFF0000)>>16;
    u32 frm_st = (pktid < 32)?(1<<pktid & GET_TX_ID_TB0):(1<<(pktid-32) & GET_TX_ID_TB1);
    u32 gbl_frm_st = (pktid < 32)?(1<<pktid & GET_ID_TB0):(1<<(pktid-32) & GET_ID_TB1);

    if (gbl_frm_st == 0)
    {
        printf("[AMPDU]: pkt %08x can not found in global map !! tx result = %08x\n", (u32)pTxInfo, frm_st);
        return FALSE;
    }
    return TRUE;
}


static u8 _cal_ampdu_delm_half_crc (u8 value)
{
    u32 c32 = value, v32 = value;
    c32 ^= (v32 >> 1) | (v32 << 7);
    c32 ^= (v32 >> 2);
    if (v32 & 2)
        c32 ^= (0xC0);
    c32 ^= ((v32 << 4) & 0x30);
    return (u8) c32;
} // end of - _cal_ampdu_delm_half_crc -

static u8 _cal_ampdu_delm_crc (u8 *pointer)
{
    u8 crc = 0xCF;
    crc ^= _cal_ampdu_delm_half_crc(*pointer++);
    crc = _cal_ampdu_delm_half_crc(crc) ^ _cal_ampdu_delm_half_crc(*pointer);
    return ~crc;
} // end of - _cal_ampdu_delm_crc -

static inline void us_timer_start(HW_TIMER *tmr, u32 count, u32 data, enum hwtmr_op_mode mode)
{
    //ampdu_fw_timer_data = data;
    //tmr->TMR_CTRL = HTMR_COUNT_VAL(count)|HTMR_OP_MODE(mode);
    hwtmr_start(tmr, count, irq_us_timer1_handler, (void *)data, HTMR_ONESHOT);
}


static void us_timer_stop (HW_TIMER *tmr)
{
    //tmr->TMR_CTRL = TU1_TM_INT_STS_DONE_MSK;
    hwtmr_stop(tmr);
}


static inline AMPDU_DELIMITER *get_ampdu_delim_from_ssv_pkt (PKT_TxInfo *pTxInfo)
{
    //u32 TX_PB_offset = GET_TX_PBOFFSET;
//    struct ssv6200_tx_desc *tx_desc = (struct ssv6200_tx_desc*)rx_data;
    AMPDU_DELIMITER *tx_delimiter = (AMPDU_DELIMITER *)((u32)pTxInfo + pTxInfo->hdr_offset);
    return tx_delimiter;
}


static inline HDR80211_Data *get_80211hdr_from_delim (AMPDU_DELIMITER *tx_delimiter)
{
    HDR80211_Data *hdr = (HDR80211_Data*)((u32)tx_delimiter + AMPDU_DELIMITER_LEN);
    return hdr;
}

static bool __check_ba_window(u16 seq, u8 wsid, u8 tid, u8 ba_wsid, u8 ba_tid, s16 ba_ssn, u8 *bitmap)
{

    if (!(wsid < WLAN_MAX_STA))
        return FALSE;

    if ((ba_wsid == wsid) && (ba_tid == tid))
    {
        if (seq > 4095)
            return FALSE;
        else
        {
            s16 gap = 0;
            //check if seq is reset due to reach maximum
            if (seq < ba_ssn)
            {
                if((ba_ssn+BITMAP_NUM) > 4095)
                    seq += 4096;
                else
                    return FALSE;
            }

            gap = seq - ba_ssn;
            if ((gap >= BITMAP_NUM) || (gap < 0))
                return FALSE;

            if (((1<<(gap&0x07))&bitmap[(gap>>3)]) != 0)
                return TRUE;
        }
    }

    return FALSE;
}

static bool _check_ba_window(PKT_TxInfo *pTxInfo, u8 ba_wsid, u8 ba_tid, s16 ba_ssn, u8 *bitmap)
{
#if 1
    u16 qc = GET_HDR80211_QC(pTxInfo);
    u8 tid = (u8)qc&0xf;
    s16 seq = GET_HDR80211_SC_SEQNUM(pTxInfo);

    return __check_ba_window(seq,pTxInfo->wsid,tid,ba_wsid,ba_tid,ba_ssn,bitmap);
#else
    u16 qc = GET_HDR80211_QC(pTxInfo);
    u8 tid = (u8)qc&0xf;
    if (!(pTxInfo->wsid < WLAN_MAX_STA))
        return FALSE;

    if ((ba_wsid == pTxInfo->wsid) && (ba_tid == tid))
    {
        s16 seq = GET_HDR80211_SC_SEQNUM(pTxInfo);
        if (seq > 4095)
            return FALSE;
        else
        {
            s16 gap = 0;
            //check if seq is reset due to reach maximum
            if (seq < ba_ssn)
            {
                if((ba_ssn+BITMAP_NUM) > 4095)
                    seq += 4096;
                else
                    return FALSE;
            }

            gap = seq - ba_ssn;
            if ((gap >= BITMAP_NUM) || (gap < 0))
                return FALSE;

            if (((1<<(gap&0x07))&bitmap[(gap>>3)]) != 0)
                return TRUE;
        }
    }

    return FALSE;
#endif
}


static inline void ssv6200_ampdu_set_retry_bit(PKT_TxInfo *pTxInfo)
{
    AMPDU_DELIMITER *tx_delimiter;
    HDR80211_Data *hdr;

    tx_delimiter = (AMPDU_DELIMITER *)((u8 *)pTxInfo +pTxInfo->hdr_offset);
    hdr = (HDR80211_Data *)((u32)tx_delimiter + AMPDU_DELIMITER_LEN);

    // set retry bit
    if(hdr->fc.Retry != 1)
        hdr->fc.Retry = 1;
}

extern u8 g_txQueueMapping[MAX_TX_DATA_QUEUE];
extern struct phy_rate_info ssv6200_phy_rate[PHY_SUPPORT_RATE_NUMBER];
extern void EDCA_FillRelatedDuration(PKT_TxInfo *pTxInfo, u16 fc);
#define MR_IDX(p) (7-p)
void _retry_ampdu_frame (struct AMPDU_TX_Data_S *p_ampdu_tx_data)
{
    u8 next_drate_idx = p_ampdu_tx_data->tx_pkt->drate_idx, wsid = p_ampdu_tx_data->tx_pkt->wsid;
    u8 retry_times = p_ampdu_tx_data->tx_pkt->RSVD_3;
    s8 rate_gap = p_ampdu_tx_data->tx_pkt->drate_idx - 15; // MF_LG
    //PhyInfo is not used at Cabrio, use last 10 bytes of PhyInfo to store multi rate parameters
    struct mr_params* mr= RC_GET_MR_PARAMS(p_ampdu_tx_data->tx_pkt);
    struct AMPDU_TX_RCD *p_ampdu_rcd = &ampdu_tx_data[p_ampdu_tx_data->tx_pkt->TxF_ID];

    if(retry_times == 0)
        ssv6200_ampdu_set_retry_bit(p_ampdu_tx_data->tx_pkt);

    while(rate_gap > 7)
        rate_gap -=8;

    retry_times++;

    while ((retry_times < mr->mr_retry_max) && (retry_times >= mr->mr_tbl[MR_IDX(rate_gap)]) && (rate_gap >0))
        rate_gap--;

    if (retry_times > mr->mr_tbl[MR_IDX(rate_gap)])
        printf("[AMPDU]: Buggy!!, retry_times %d for rate %d and its accu_try_limit = %d \n", retry_times, p_ampdu_tx_data->tx_pkt->drate_idx, mr->mr_tbl[MR_IDX(rate_gap)]);

    next_drate_idx = (rate_gap + 15);
    p_ampdu_tx_data->tx_pkt->RSVD_0 = (p_ampdu_tx_data->tx_pkt->RSVD_0 == RC_PROBE)?RC_PROBE:RC_RESEND;
    p_ampdu_tx_data->tx_pkt->tx_report = 1;
    p_ampdu_tx_data->tx_pkt->reason = 0;
    p_ampdu_tx_data->tx_pkt->unicast = 1;
    p_ampdu_tx_data->tx_pkt->RSVD_3 = retry_times;
    p_ampdu_tx_data->retry_count = retry_times;
    if(p_ampdu_tx_data->tx_pkt->drate_idx != next_drate_idx)
    {
        u16 fc = GET_HDR80211_FC(p_ampdu_tx_data->tx_pkt);
        p_ampdu_tx_data->tx_pkt->drate_idx = next_drate_idx;
        if (p_ampdu_tx_data->tx_pkt->crate_idx != 0)
            p_ampdu_tx_data->tx_pkt->crate_idx = ssv6200_phy_rate[next_drate_idx].ackIndex;

        EDCA_FillRelatedDuration(p_ampdu_tx_data->tx_pkt, fc);
    }

    p_ampdu_rcd->st |= AMPDU_ST_RESEND;

    if(TRUE==ENG_MBOX_SEND((u32)g_txQueueMapping[wsid]+M_ENG_TX_EDCA0, (u32)p_ampdu_tx_data->tx_pkt))
    {
        ampdu_tx_retry_count[retry_times]++;
    }
    else
    {
        memset(p_ampdu_rcd, 0, sizeof(struct AMPDU_TX_RCD));
        PBUF_MFree(p_ampdu_tx_data->tx_pkt);
        ampdu_tx_drop_count++;
    }
}

#if (N_MODE_AMPDU_RETRY_TO_B_MODE == 1)
void _retry_last_in_BMode(struct AMPDU_TX_Data_S *p_ampdu_tx_data){

    u16 fc = GET_HDR80211_FC(p_ampdu_tx_data->tx_pkt);
    struct mr_params *mr = RC_GET_MR_PARAMS(p_ampdu_tx_data->tx_pkt);
    u8 LastRetryOnce = AMPDU_TX_LastRetryBModeOnce;

    //If the pkt is a probe frame, and only one rate for multi rates,
    //this frame must be sent in unicast mode
    if ((p_ampdu_tx_data->tx_pkt->RSVD_0 == RC_PROBE) && (mr->mr_rate_num == 1) &&
        (AMPDU_TX_LastRetryBModeOnce == 1))
        LastRetryOnce = 0;

    p_ampdu_tx_data->tx_pkt->drate_idx = AMPDU_TX_LastRetryBModeRate;
    //it's last retry just sent it. so sent it as no ack frame.
    p_ampdu_tx_data->tx_pkt->ack_policy = LastRetryOnce;
    p_ampdu_tx_data->tx_pkt->tx_report = 0;
    if (p_ampdu_tx_data->tx_pkt->crate_idx != 0)
        p_ampdu_tx_data->tx_pkt->crate_idx = ssv6200_phy_rate[AMPDU_TX_LastRetryBModeRate].ackIndex;
    EDCA_FillRelatedDuration(p_ampdu_tx_data->tx_pkt, fc);
    if(FALSE==ENG_MBOX_SEND((u32)g_txQueueMapping[p_ampdu_tx_data->tx_pkt->wsid]+M_ENG_TX_EDCA0, (u32)p_ampdu_tx_data->tx_pkt))
    {
        PBUF_MFree(p_ampdu_tx_data->tx_pkt);
    }
}
#endif//(N_MODE_AMPDU_RETRY_TO_B_MODE == 1)

void amdpu_init(void){
    //enable hw to calculate fcs of mpdu.
    SET_MTX_AMPDU_CRC_AUTO(1);
    AMPDU_TX_ENABLE = 1;
    memset(ampdu_tx_data, 0, sizeof(ampdu_tx_data));
    memset(&last_ampdu, 0 , sizeof(last_ampdu));
    memset(&ba_window, 0 , sizeof(ba_window));
    ba_window.ssn = -1;
}

extern SSV6XXX_STA	*g6200StaInfo;
extern void ampdu_addba_req(SSV6XXX_STA *sta, u8 *p8_da, u8 *p8_sa, u8 *p8_bssid);

#define ETH_ALEN 6
#define AMPDU_DIALOG_TOKEN  (15)
#define AMPDU_TID           (0)
#define AMPDU_AGG_SIZE      (64)
#define AMPDU_START_SEQ_NUM (0)
#define AMPDU_TIMEOUT       (0)


extern void Fill_Mgmt_FC(HDR80211_Mgmt *mgmt,u32 type,u32 stype);
extern void Fill_TxDescriptor(PKT_TxInfo *TxPkt,s32 fixed_field_length);
extern void mlme_mgmt_send(PKT_TxInfo *TxPkt);

//0 ->success , 1 fail to allocated
s8 _alloc_tx_frame(u32 frm_len, PKT_TxInfo **pp_tx_pkt, void **pp_frm_hdr){

    *pp_tx_pkt = (PKT_TxInfo *)PBUF_MAlloc(frm_len, SOC_PBUF);
    if(NULL == *pp_tx_pkt)
        return 1;
    *pp_frm_hdr = (void *)((u8 *)*pp_tx_pkt + drv_mac_get_pbuf_offset());

    //clear frm
    memset(*pp_frm_hdr, 0, frm_len);

    return 0;
}

void ampdu_addba_req(SSV6XXX_STA *sta, u8 *p8_da, u8 *p8_sa, u8 *p8_bssid){


    PKT_TxInfo *p_tx_pkt;
    HDR80211_Mgmt *p_mgmt_hdr;
    u16 u16_capab;

    if(_alloc_tx_frame(sizeof(HDR80211_Mgmt), &p_tx_pkt, (void**)&p_mgmt_hdr) != 0)
        return;


    Fill_Mgmt_FC(p_mgmt_hdr, FT_MGMT, FST_ACTION);

    memcpy((void *)&p_mgmt_hdr->da, (void*)p8_da, ETH_ALEN);
    memcpy((void *)&p_mgmt_hdr->sa, (void*)p8_sa, ETH_ALEN);
    memcpy((void *)&p_mgmt_hdr->bssid, (void*)p8_bssid, ETH_ALEN);


    p_mgmt_hdr->u.action.category = WLAN_CATEGORY_BACK;
    p_mgmt_hdr->u.action.u.addba_req.action_code = WLAN_ACTION_ADDBA_REQ;

    p_mgmt_hdr->u.action.u.addba_req.dialog_token = ++sta->ampdu_token;
    u16_capab = (u16)(1 << 1);      /* bit 1 aggregation policy */
    u16_capab |= (u16)(AMPDU_TID << 2);   /* bit 5:2 TID number */
    u16_capab |= (u16)(AMPDU_AGG_SIZE << 6);  /* bit 15:6 max size of aggergation */

    p_mgmt_hdr->u.action.u.addba_req.capab = u16_capab;

    p_mgmt_hdr->u.action.u.addba_req.timeout = AMPDU_TIMEOUT;
    sta->ampdu_tx_ssn = p_mgmt_hdr->u.action.u.addba_req.start_seq_num = (DRV_MAC_GET_STA_TX_SEQ(sta->wsid, 0)<<4);//AMPDU_START_SEQ_NUM << 4;


    printf("sending addba_req to [%02x:%02x:%02x:%02x:%02x:%02x]\n",
        p8_da[0],
        p8_da[1],
        p8_da[2],
        p8_da[3],
        p8_da[4],
        p8_da[5]);
    Fill_TxDescriptor(p_tx_pkt, (sizeof(p_mgmt_hdr->u.action.u.addba_req)+IEEE80211_ACTION_CATEGORY_SIZE));
    mlme_mgmt_send(p_tx_pkt);

    return;
}



void ampdu_send_bar(u8 *ta, u8 *ra, u16 ssn)
{
	struct ieee80211_bar *bar;
	u16 bar_control = 0;
    PKT_TxInfo *p_tx_pkt;

    if(_alloc_tx_frame((u32)sizeof(struct ieee80211_bar), &p_tx_pkt, (void**)&bar) != 0)
        return;

    //clear bar
    bar->frame_control = (FT_MGMT | FST_BA_REQ);
	memcpy(bar->ra, ra, ETH_ALEN);
	memcpy(bar->ta, ta, ETH_ALEN);

	bar_control |= (u16)IEEE80211_BAR_CTRL_ACK_POLICY_NORMAL;
	bar_control |= (u16)IEEE80211_BAR_CTRL_CBMTID_COMPRESSED_BA;
	bar_control |= (u16)(AMPDU_TID << IEEE80211_BAR_CTRL_TID_INFO_SHIFT);
	bar->control = (bar_control);
	bar->start_seq_num = (ssn);


    memset((void *)p_tx_pkt, 0, sizeof(PKT_TxInfo));
    p_tx_pkt->len          = sizeof(struct ieee80211_bar);
    p_tx_pkt->c_type       = M0_TXREQ;
    p_tx_pkt->f80211       = 1;
    p_tx_pkt->qos          = 0;
    p_tx_pkt->ht           = 0;
    p_tx_pkt->use_4addr    = 0;
    p_tx_pkt->RSVD_0       = 0;
    p_tx_pkt->more_data    = 0;
    p_tx_pkt->crate_idx    = 0;
    p_tx_pkt->drate_idx    = 0;

    TX_FRAME((u32)p_tx_pkt);

    return;
}

#if (N_MODE_AMPDU_RETRY_TO_B_MODE == 1)
void ampdu_reform_apmdu_to_mpdu(PKT_TxInfo *pPktInfo){

    pPktInfo->aggregation = 0;
    pPktInfo->ack_policy= 0;
    pPktInfo->unicast = 1;
    pPktInfo->tx_report = 0;

    pPktInfo->len -= (AMPDU_DELIMITER_LEN+AMPDU_FCS_LEN+pPktInfo->txq_idx);
    //Remove delimeter
    pPktInfo->hdr_offset += AMPDU_DELIMITER_LEN;
    pPktInfo->hdr_len -= AMPDU_DELIMITER_LEN;

}
#endif//(N_MODE_AMPDU_RETRY_TO_B_MODE == 1)


void ampdu_reform_mpdu_to_apmdu(PKT_TxInfo *pPktInfo, u16 fc){

    AMPDU_DELIMITER *pDelimiter;
    u32 u32Pad;
    u16 u16OrgPktLen;
    u8 u8_wsid  = pPktInfo->wsid;
    SSV6XXX_STA		*p_sta;

    if(AMPDU_TX_ENABLE == 0)
        return;

    if(!IS_DATA_FRAME(fc))
        return;

    if(!pPktInfo->unicast)
        return;

    if(pPktInfo->aggregation)
        return;

    if(u8_wsid >= WLAN_MAX_STA)
        return;


    p_sta = &g6200StaInfo[u8_wsid];

    if(p_sta->ampdu_tx != TRUE){
        //It need to add ba session againg
        HDR80211_Data *p_data_hdr = (HDR80211_Data *)((u8*)pPktInfo+pPktInfo->hdr_offset);
        u8 *bssid = (drv_mac_get_op_mode() == OP_MODE_STA)?(u8*)&p_data_hdr->addr1:(u8*)&p_data_hdr->addr2;
        ampdu_addba_req(p_sta, (u8 *)&p_data_hdr->addr1, (u8 *)&p_data_hdr->addr2, bssid);
        return;
    }

    //If fail to get multi rate param, return
    if (rate_control_pid_get_multi_rates((void *)p_sta, (void *)pPktInfo, ((pPktInfo->RSVD_0 == RC_PROBE)?TRUE:FALSE)) == FALSE)
        return ;

    //Start Sharp to AMPDU
    u16OrgPktLen = pPktInfo->len;
    //u32Pad = (4 - (u16OrgPktLen % 4)) % 4;
    u32Pad = (4 - (u16OrgPktLen & 0x0003)) & 0x0003;


    //Modify header to fit AMPDU
    pPktInfo->aggregation = 1;
    pPktInfo->ack_policy= 1;
    pPktInfo->RSVD_3 = 0;			//Reset ampdu retry cnt.
    pPktInfo->RSVD_1 = 0;
    pPktInfo->unicast = 1;
    pPktInfo->tx_report = 1;
    pPktInfo->txq_idx = u32Pad;		//Save pad length here.
    pPktInfo->TxF_ID = 0x10; //Reset TxF_ID due to store AMPDU list index

    pPktInfo->len += AMPDU_DELIMITER_LEN+AMPDU_FCS_LEN+u32Pad;
    //Move space for delimeter
    pPktInfo->hdr_offset -= AMPDU_DELIMITER_LEN;
    pPktInfo->hdr_len += AMPDU_DELIMITER_LEN;

    pDelimiter = (AMPDU_DELIMITER *)((u8*)pPktInfo+pPktInfo->hdr_offset);
    pDelimiter->reserved = 0;
    pDelimiter->length = u16OrgPktLen + AMPDU_FCS_LEN;
    pDelimiter->signature = AMPDU_SIGNATURE;
    pDelimiter->crc = _cal_ampdu_delm_crc((u8*) (pDelimiter));

}

void _process_failed_ampdu(struct AMPDU_TX_Data_S *p_ampdu_tx_data, struct AMPDU_TX_RCD *p_ampdu_rcd, bool IsSuspend, bool IsTimeout)
{
    u8 wsid = p_ampdu_tx_data->tx_pkt->wsid;
    u8 hw_rate_idx = p_ampdu_tx_data->tx_pkt->drate_idx;
    u32 prb_pkt_complete = 0;
    struct mr_params *mr = RC_GET_MR_PARAMS(last_ampdu.tx_pkt);

    if ((IsSuspend == FALSE) && (p_ampdu_tx_data->retry_count < mr->mr_retry_max))
    {
        PRINTF("%st: %d, max = %d\n", (IsTimeout==TRUE)?:"T":"A",ssn, mr->mr_retry_max);
        if(IsTimeout==TRUE)
        {
            p_ampdu_rcd->st |= AMPDU_ST_TIMEOUT;
            p_ampdu_rcd->retry_cnt_timeout++;
            ampdu_tx_timeout_count++;
        }
        else
        {
            p_ampdu_rcd->st |= AMPDU_ST_JAMOUT;
            p_ampdu_rcd->retry_cnt_jam++;
            ampdu_tx_jamout_count++;
        }
        _retry_ampdu_frame(p_ampdu_tx_data);
    }
    else
    {
        if(p_ampdu_tx_data->tx_pkt->RSVD_0 == RC_PROBE)
            prb_pkt_complete = 1;
#if (N_MODE_AMPDU_RETRY_TO_B_MODE == 1)
        if ((IsSuspend == TRUE) || (AMPDU_TX_LastRetryInBMode == 0) ||
            ((g_prb_pkt_enable == 1) && (p_ampdu_tx_data->tx_pkt->RSVD_0 == RC_PROBE)))
        {
            if(p_ampdu_tx_data->tx_pkt->RSVD_0 == RC_PROBE)
                rate_control_pid_return_probe_frame(p_ampdu_tx_data->tx_pkt);
            else
                PBUF_MFree(p_ampdu_tx_data->tx_pkt);
        }
        else
        {
            ampdu_reform_apmdu_to_mpdu(p_ampdu_tx_data->tx_pkt);
            _retry_last_in_BMode(p_ampdu_tx_data);
        }
#else//#if (N_MODE_AMPDU_RETRY_TO_B_MODE == 1)
        if (p_ampdu_tx_data->tx_pkt->RSVD_0 == RC_PROBE)
            rate_control_pid_return_probe_frame(p_ampdu_tx_data->tx_pkt);
        else
            PBUF_MFree(p_ampdu_tx_data->tx_pkt);
#endif//#if (N_MODE_AMPDU_RETRY_TO_B_MODE == 1)


        //p_ampdu_rcd->st |= AMPDU_ST_DROP;
        memset(p_ampdu_rcd, 0, sizeof(struct AMPDU_TX_RCD));
        ++ampdu_tx_drop_count;
        PRINTF("%se: %d:%d, max = %d\n", (IsTimeout==TRUE)?:"T":"A",ssn, p_ampdu_rcd->ssn, mr->mr_retry_max);
    }

    memset((void*)p_ampdu_tx_data, 0 , sizeof(struct AMPDU_TX_Data_S));
    if(hw_rate_idx < 7)
        printf("Warning: got hw rate idx = %d\n", hw_rate_idx);

    if(recover_enable > 0)// Enable watchdog & reset watchdog
        SET_SYS_WDT_TIME_CNT(WATCHDOG_TIMER);
    
    RC_FAIL_TX_ST(wsid, hw_rate_idx, prb_pkt_complete);
}

void _process_success_ampdu(PKT_TxInfo *pTxInfo, struct AMPDU_TX_RCD *p_ampdu_rcd)
{
    u8 wsid = pTxInfo->wsid;
    u8 drate_idx = pTxInfo->drate_idx;
    u8 retry_count = (u8)pTxInfo->RSVD_3;
    u32 prb_pkt_complete = 0;
    PRINTF("f - %d\n", GET_HDR80211_SC_SEQNUM(pTxInfo));
    if(pTxInfo->RSVD_0 == RC_PROBE)
    {
        prb_pkt_complete = 1;
        rate_control_pid_return_probe_frame(pTxInfo);
    }
    else
        PBUF_MFree(pTxInfo);

    if(retry_count == 0)
        ++ampdu_tx_ba_match_last_ampdu_new_count;
    else
        ++ampdu_tx_ba_match_last_ampdu_retried_count;

    p_ampdu_rcd->st |= AMPDU_ST_BA_RECIEVED;
    //memset(p_ampdu_rcd, 0, sizeof(struct AMPDU_TX_RCD));

    if(drate_idx < 7)
        printf("Warning: got hw rate idx = %d\n", drate_idx);

    
    if(recover_enable > 0)// Enable watchdog & reset watchdog
        SET_SYS_WDT_TIME_CNT(WATCHDOG_TIMER);

    RC_SUC_TX_ST(wsid, drate_idx, prb_pkt_complete);
}

//#if (N_MODE_AMPDU_RETRY_TO_B_MODE == 1)
//extern void ampdu_reform_apmdu_to_mpdu(PKT_TxInfo *pPktInfo);
//void _retry_last_time(struct AMPDU_TX_Data_S *p_ampdu_tx_data);
//#endif//#if (N_MODE_AMPDU_RETRY_TO_B_MODE == 1)

void irq_us_timer1_handler(void *m_data)
{
    struct AMPDU_TX_RCD *p_ampdu_rcd;
    u16 ssn;
    u32 timeout_param;
    bool suspend_retry = FALSE;

    if (last_ampdu.tx_pkt == NULL)
        return;

    if(FALSE==stainfo_is_ampdu_ready(last_ampdu.tx_pkt->wsid))
        suspend_retry = TRUE;

    p_ampdu_rcd = &ampdu_tx_data[last_ampdu.tx_pkt->TxF_ID];
    ssn = GET_HDR80211_SC_SEQNUM(last_ampdu.tx_pkt);
    timeout_param = (u32)m_data;

    if(ssn != timeout_param)
    {
        ++ampdu_tx_timeout_miss_count;
        PRINTF("Tm: %d:%d\n", ssn, timeout_param);
    }

    _process_failed_ampdu(&last_ampdu, p_ampdu_rcd, suspend_retry, TRUE);
}


bool ampdu_rx_ctrl_handler (u32 rx_data)
{
    PKT_RxInfo *rxBApkt = (PKT_RxInfo *)rx_data;
    bool is_BA = IS_BA(GET_HDR80211_FC(rxBApkt));
    p_AMPDU_BLOCKACK MAC_header = (p_AMPDU_BLOCKACK)((u8 *)(rxBApkt)+(rxBApkt)->hdr_offset);
    bool replace_ba = TRUE, miss_ba = TRUE;
    u8 i=0;

    if (is_BA)
    {
        ++ampdu_tx_ba_count;
        PRINTF("ba\n");
        if (last_ampdu.tx_pkt != NULL)
        {
            // Check if the Received BA matches is from the destination of the waiting AMPDU.
            PKT_TxInfo *pTxInfo = last_ampdu.tx_pkt;
            struct AMPDU_TX_RCD *p_ampdu_rcd = &ampdu_tx_data[pTxInfo->TxF_ID];
            AMPDU_DELIMITER *delim = get_ampdu_delim_from_ssv_pkt(pTxInfo);
            HDR80211_Data *hdr = get_80211hdr_from_delim(delim);
            //      BA TA                  != AMPDU RA
            if (!((MAC_header->ta_addr[0] != hdr->addr1.addr[0])
                || (MAC_header->ta_addr[1] != hdr->addr1.addr[1])
                || (MAC_header->ta_addr[2] != hdr->addr1.addr[2])
                || (MAC_header->ta_addr[3] != hdr->addr1.addr[3])
                || (MAC_header->ta_addr[4] != hdr->addr1.addr[4])
                || (MAC_header->ta_addr[5] != hdr->addr1.addr[5])))
            {
                if(_check_ba_window(pTxInfo, rxBApkt->wsid, MAC_header->tid_info, MAC_header->BA_ssn, MAC_header->BA_sn_bit_map) == TRUE)
                {
                    us_timer_stop(US_TIMER1);
                    miss_ba = FALSE;
                    PRINTF("b");
                    _process_success_ampdu(last_ampdu.tx_pkt, p_ampdu_rcd);
                    memset(&last_ampdu, 0 , sizeof(struct AMPDU_TX_Data_S));
                }
                else
                    PRINTF("ba No hit - %d, 0x%02x%02x%02x%02x%02x%02x%02x%02x\n"
                                    , MAC_header->BA_ssn
                                    , MAC_header->BA_sn_bit_map[0]
                                    , MAC_header->BA_sn_bit_map[1]
                                    , MAC_header->BA_sn_bit_map[2]
                                    , MAC_header->BA_sn_bit_map[3]
                                    , MAC_header->BA_sn_bit_map[4]
                                    , MAC_header->BA_sn_bit_map[5]
                                    , MAC_header->BA_sn_bit_map[6]
                                    , MAC_header->BA_sn_bit_map[7]
                                    );
            }
            else
                PRINTF("ba Not match - %d, 0x%02x%02x%02x%02x%02x%02x%02x%02x\n"
                                    , MAC_header->BA_ssn
                                    , MAC_header->BA_sn_bit_map[0]
                                    , MAC_header->BA_sn_bit_map[1]
                                    , MAC_header->BA_sn_bit_map[2]
                                    , MAC_header->BA_sn_bit_map[3]
                                    , MAC_header->BA_sn_bit_map[4]
                                    , MAC_header->BA_sn_bit_map[5]
                                    , MAC_header->BA_sn_bit_map[6]
                                    , MAC_header->BA_sn_bit_map[7]
                                    );
        }
        else
            PRINTF("ba N/A - %d, 0x%02x%02x%02x%02x%02x%02x%02x%02x\n"
                                    , MAC_header->BA_ssn
                                    , MAC_header->BA_sn_bit_map[0]
                                    , MAC_header->BA_sn_bit_map[1]
                                    , MAC_header->BA_sn_bit_map[2]
                                    , MAC_header->BA_sn_bit_map[3]
                                    , MAC_header->BA_sn_bit_map[4]
                                    , MAC_header->BA_sn_bit_map[5]
                                    , MAC_header->BA_sn_bit_map[6]
                                    , MAC_header->BA_sn_bit_map[7]
                                    );

        for(i=0;i<MAX_AMPDU_RCD_NUM;i++)
        {
            struct AMPDU_TX_RCD *p_ampdu_rc=NULL;
            p_ampdu_rc=&ampdu_tx_data[i];
            if((p_ampdu_rc->st&AMPDU_ST_VALID)
                &&(p_ampdu_rc->st&AMPDU_ST_RESEND)
                &&(p_ampdu_rc->retry_cnt_flag==0))
            {
                if(TRUE==__check_ba_window(p_ampdu_rc->ssn,p_ampdu_rc->wsid,p_ampdu_rc->tid,rxBApkt->wsid,
                        MAC_header->tid_info, MAC_header->BA_ssn, MAC_header->BA_sn_bit_map))
                {
                    ampdu_tx_redundance_count++;
                    p_ampdu_rc->retry_cnt_flag=1;
                }
            }
        }

        if(replace_ba == TRUE)
        {
            ba_window.wsid = rxBApkt->wsid;
            ba_window.tid = MAC_header->tid_info;
            ba_window.ssn = (s16)MAC_header->BA_ssn;
            memcpy(&ba_window.bitmap, &MAC_header->BA_sn_bit_map, 8);
            PRINTF("ba renew - %d, 0x%02x%02x%02x%02x%02x%02x%02x%02x\n"
                                    , ba_window.ssn
                                    , ba_window.bitmap[0]
                                    , ba_window.bitmap[1]
                                    , ba_window.bitmap[2]
                                    , ba_window.bitmap[3]
                                    , ba_window.bitmap[4]
                                    , ba_window.bitmap[5]
                                    , ba_window.bitmap[6]
                                    , ba_window.bitmap[7]
                                    );
        }

        if(miss_ba == TRUE)
            ++ampdu_tx_miss_ba_count;
    }

    return true;
} // end of - ampdu_rx_ctrl_handler -

bool ampdu_tx_report_handler (u32 ampdu_tx_pkt)
{
    struct AMPDU_TX_RCD *tx_rcd = NULL;
    PKT_TxInfo *pTxInfo = (PKT_TxInfo*)ampdu_tx_pkt;
    u16 seq = GET_HDR80211_SC_SEQNUM(pTxInfo);
    bool is_ba_found = FALSE;
    u8 retry_cnt = pTxInfo->RSVD_3, wsid, hw_rate_idx, prb_pkt_complete = 0;
    u16 qc = GET_HDR80211_QC(pTxInfo);
    u8 tid = (u8)qc&0xf;
    struct mr_params *mr;

    int match_entry = (-1);

    if(FALSE==stainfo_is_ampdu_ready(pTxInfo->wsid))
    {
        wsid = pTxInfo->wsid;
        hw_rate_idx = pTxInfo->drate_idx;
        // Update record
        if ((pTxInfo->TxF_ID < MAX_AMPDU_RCD_NUM) && (ampdu_tx_data[pTxInfo->TxF_ID].ssn == seq))
            memset(&ampdu_tx_data[pTxInfo->TxF_ID], 0 , sizeof(struct AMPDU_TX_RCD));

        printf("[AMPDU]: Failed at begin, (pkt, seq) = (%08x, %d)\n", ampdu_tx_pkt, seq);
        if (pTxInfo->RSVD_0 == RC_PROBE)
        {
            rate_control_pid_return_probe_frame(pTxInfo);
            prb_pkt_complete = 1;
        }
        else
            PBUF_MFree(pTxInfo);

        ++ampdu_tx_drop_count;
        
        if(recover_enable > 0)// Enable watchdog & reset watchdog
            SET_SYS_WDT_TIME_CNT(WATCHDOG_TIMER);

        RC_FAIL_TX_ST(wsid, hw_rate_idx, prb_pkt_complete);
        return TRUE;
    }

    is_ba_found = _check_ba_window(pTxInfo, ba_window.wsid, ba_window.tid, ba_window.ssn, ba_window.bitmap);
    if (is_ba_found == TRUE)
    {
        PRINTF("A");
        _process_success_ampdu(pTxInfo, tx_rcd);
    }
    else
    {
        if ((pTxInfo->TxF_ID < MAX_AMPDU_RCD_NUM) && (ampdu_tx_data[pTxInfo->TxF_ID].ssn == seq))
        {
            match_entry = pTxInfo->TxF_ID;
            tx_rcd = &ampdu_tx_data[match_entry];
            tx_rcd->retry_cnt_flag=0;
            PRINTF("Ao - %d\n", seq);
        }
        else
        {
            bool pkt_check = _tx_memory_check(pTxInfo);
            current_ampdu_idx = (current_ampdu_idx+1)&0x0F;
            tx_rcd = &ampdu_tx_data[current_ampdu_idx];
            pTxInfo->TxF_ID = current_ampdu_idx;
            if(pkt_check == FALSE)
                printf("[AMPDU]: print for test\n");
            memset(tx_rcd, 0, sizeof(struct AMPDU_TX_RCD));
            tx_rcd->ssn = seq;
            tx_rcd->tid = tid;
            tx_rcd->wsid = pTxInfo->wsid;
            tx_rcd->retry_cnt_flag=0;
            PRINTF("An - %d\n", seq);
        }
        mr = RC_GET_MR_PARAMS(pTxInfo);
        //recodr the first received AMPDU here, other retried AMPDU count record at _retry_ampdu_frame
        if(retry_cnt == 0)
            ampdu_tx_retry_count[retry_cnt]++;
        else if(retry_cnt > mr->mr_retry_max)
            printf("Error !! retry cnt = %d, retry max = %d\n",retry_cnt, mr->mr_retry_max);
    }

    ++ampdu_tx_txreport_received_count;

    //Stop timer first and resent last ampdu
    if(last_ampdu.tx_pkt != NULL)
    {
        us_timer_stop(US_TIMER1);
        tx_rcd = &ampdu_tx_data[last_ampdu.tx_pkt->TxF_ID];
        _process_failed_ampdu(&last_ampdu, tx_rcd, FALSE, FALSE);
    }

    //Update last_ampdu
    if (is_ba_found != TRUE)
    {
        last_ampdu.tx_pkt = pTxInfo;
        last_ampdu.retry_count = pTxInfo->RSVD_3;
        us_timer_start(US_TIMER1, AMPDU_FW_retry_time_us, GET_HDR80211_SC_SEQNUM(last_ampdu.tx_pkt), HTMR_ONESHOT);
        tx_rcd->ssn = seq;
        tx_rcd->st |= (AMPDU_ST_PEND|AMPDU_ST_VALID);
        #if(PACKET_TRACE==1)
        pt((void *)pTxInfo,M_ENG_MAX,EN_LOC_WAIT_BA);
        #endif
    }

    return true;
}

void ampdu_reset_ampdu_mib (void)
{
    ampdu_tx_ba_count = 0;
    ampdu_tx_miss_ba_count = 0;
    ampdu_tx_ba_match_last_ampdu_new_count = 0;
    ampdu_tx_ba_match_last_ampdu_retried_count = 0;
    //ampdu_tx_lose_ba_count = 0;

    /* AMPDU related */
    memset(ampdu_tx_retry_count, 0 , sizeof(ampdu_tx_retry_count));

    ampdu_tx_drop_count = 0;         //Once pakcet is DROP
    ampdu_tx_timeout_count = 0;      //Once pakcet is timeout
    ampdu_tx_jamout_count = 0;       //Once pakcet is jamout
    ampdu_tx_txreport_received_count = 0;
    ampdu_tx_timeout_miss_count = 0;

    ampdu_tx_new_txreport_match_BA_count = 0;
    ampdu_tx_retry_txreport_match_BA_count = 0;
    ampdu_tx_redundance_count=0;
}

char *bmode[] = {"1", "2", "5.5", "11"};
void ampdu_show_ampdu_mib (void)
{
    u8 i;
    printf("Last retry in BMode: %s\n", (AMPDU_TX_LastRetryInBMode == 1)?"enable":"disable");
    if (AMPDU_TX_LastRetryInBMode == 1)
    {
        printf("Last retry rate in BMode: %sM\n", bmode[AMPDU_TX_LastRetryBModeRate]);
        printf("Last retry rate in BMode once: %s\n", (AMPDU_TX_LastRetryBModeOnce == 1)?"enable":"disable");
    }
    printf("AMPDU_TX_MaxRetry = %d\n", AMPDU_TX_MaxRetry);
    printf("Block session at non-nmode rate is %s\n", (AMPDU_TX_BlockSessionAtNotNmodeRate!=0)?"enable":"disable");
    printf("BA count: %d\n", ampdu_tx_ba_count);
    printf("BA miss count: %d\n", ampdu_tx_miss_ba_count);
    printf("BA match last ampdu which has no retry: %d\n", ampdu_tx_ba_match_last_ampdu_new_count);
    printf("BA match last ampdu which been retried: %d\n", ampdu_tx_ba_match_last_ampdu_retried_count);
    //printf("BA lose count: %d\n",ampdu_tx_lose_ba_count);

    /* AMPDU related */
    for(i = 0; i<= AMPDU_FW_MAX_RETRY;i++)
        printf("AMPDUs sent at %d try count : %d\n", i, ampdu_tx_retry_count[i]);
    printf("AMPDU received count : %d (count once ampdu report is received)\n", ampdu_tx_txreport_received_count);
    printf("AMPDU new report match BA : %d\n", ampdu_tx_new_txreport_match_BA_count);
    printf("AMPDU retried report match BA : %d\n", ampdu_tx_retry_txreport_match_BA_count);
    printf("AMPDU timeout miss : %d\n", ampdu_tx_timeout_miss_count);

    printf("AMPDU drop count : %d\n", ampdu_tx_drop_count);         //Once pakcet is DROP
    printf("AMPDU timeout count : %d\n", ampdu_tx_timeout_count);      //Once pakcet is timeout
    printf("AMPDU jamout count : %d\n", ampdu_tx_jamout_count);       //Once pakcet is jamout
    printf("AMPDU redundance count : %d\n", ampdu_tx_redundance_count);

}

void ampdu_dump_retry_list(void)
{
    int i;

    printf("Current ampdu index: %d\n", current_ampdu_idx);
    for (i = 0; i < MAX_AMPDU_RCD_NUM; i++)
        printf("\t%d: seq: %4d st: %08x, jam:time = (%d:%d)\n", i, ampdu_tx_data[i].ssn, ampdu_tx_data[i].st, ampdu_tx_data[i].retry_cnt_jam, ampdu_tx_data[i].retry_cnt_timeout);
}

extern void rate_control_set_defaul_rates_by_sta(u8 wsid);
void ampdu_tx_set_enable(u8 IsEnable)
{
    u8 i = 0;
    if(AMPDU_TX_ENABLE != IsEnable)
    {
        AMPDU_TX_ENABLE = IsEnable;
        for(;i < WLAN_MAX_STA; i++)
            rate_control_set_defaul_rates_by_sta(i);
    }

    printf("[AMPDU]: AMPDU_TX_ENABLE = %d\n", AMPDU_TX_ENABLE);
}

void ampdu_tx_set_options(u16 mode, u16 value)
{
    if (mode ==AMPDU_TX_OPT_ENABLE)
        ampdu_tx_set_enable((u8) value);
#if (N_MODE_AMPDU_RETRY_TO_B_MODE == 1)
    else if (mode == AMPDU_TX_OPT_SET_LAST_TRY_BMODE)
    {
        AMPDU_TX_LastRetryInBMode = (u8)value;
        printf("[AMPDU]: Last Retry In BMode is %s\n", (AMPDU_TX_LastRetryInBMode!=0)?"enable":"disable");
    }
    else if (mode == AMPDU_TX_OPT_SET_BLOCK_NON_NMODE)
    {
        AMPDU_TX_BlockSessionAtNotNmodeRate = (u8)value;
        printf("[AMPDU]: Block session at non-nmode rate is %s\n", (AMPDU_TX_BlockSessionAtNotNmodeRate!=0)?"enable":"disable");
    }
    else if(mode == AMPDU_TX_OPT_SET_RETRY_MAX)
    {
        if (value > AMPDU_FW_MAX_RETRY)
        {
            printf("[AMPDU]: Failed to set, please set the value less than %d\n", AMPDU_FW_MAX_RETRY);
            return;
        }

        AMPDU_TX_MaxRetry = (u8) value;
        printf("[AMPDU]: AMPDU_TX_MaxRetry = %d\n", AMPDU_TX_MaxRetry);
    }
    else if(mode == AMPDU_TX_OPT_SET_MAIN_TRY)
    {
        if (value > AMPDU_MAIN_TRY)
        {
            printf("[AMPDU]: Failed to set, please set the value less than %d\n", AMPDU_MAIN_TRY);
            return;
        }

        AMPDU_TX_Maintry = (u8) value;
        printf("[AMPDU]: AMPDU_TX_Maintry = %d\n", AMPDU_TX_Maintry);
    }
    else if(mode == AMPDU_RX_OPT_ENABLE)
    {
        AMPDU_RX_ENABLE= (u8) value;
        printf("[AMPDU]: AMPDU_RX_ENABLE = %d\n", AMPDU_RX_ENABLE);
    }
    else if(mode == AMPDU_RX_OPT_BUF_SIZE)
    {
        AMPDU_RX_BUF_SIZE = (u8) value;
        printf("[AMPDU]: AMPDU_RX_BUF_SIZE = %d\n", AMPDU_RX_BUF_SIZE);
    }
    else if (AMPDU_TX_LastRetryInBMode == 1)
    {
        if (mode == AMPDU_TX_OPT_SET_LAST_BMODE_RATE)
        {
            AMPDU_TX_LastRetryBModeRate = (u8) value;
            printf("[AMPDU]: Last Retry In BMode rate = %d\n", AMPDU_TX_LastRetryBModeRate);
        }
        else if(mode == AMPDU_TX_OPT_SET_LAST_BMODE_RETRY)
        {
            AMPDU_TX_LastRetryBModeOnce = (u8) value;
            printf("[AMPDU]: Last Retry In BMode Once is %s\n", (AMPDU_TX_LastRetryBModeOnce!=0)?"enable":"disable");
        }
    }
    else
        printf("[AMPDU]: Last retry in B is not enable\n");
#endif
}


#endif //#if (N_MODE_AMPDU_TX == 1)
