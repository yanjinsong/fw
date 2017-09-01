// #define __SFILE__ "edca.c"

#include <common.h>
#include <config.h>
#include <pbuf.h>
#include "edca_queue.h"
#include "edca.h"
#include "pide.h"

#include <rtos.h>

#include <pbuf.h>
#include <log.h>

#include "hdr80211.h"
#include <edca/drv_edca.h>
#include <mbox/drv_mbox.h>
#include <nav/drv_nav.h>
#include <hwmac/drv_mac.h>

#include <types.h>

#include <task.h>
#include "sta_info.h"
#if(PACKET_TRACE==1)
#include <lib/ssv_lib.h>
#endif
struct ecda_info *gEDCAInfo;
extern SSV6XXX_STA	*g6200StaInfo;


#if (CONFIG_SIM_PLATFORM == 0)
#define CONFIG_EDCA_HW_CALCULATE_NAV
#define _FPGA_
#endif
//#define LOG_TRACE(...)
s32 EDCA_init()
{
	enum edca_tx_queue_subtype i;
	u16 size = sizeof(struct ecda_info);

	gEDCAInfo = (struct ecda_info *)OS_MemAlloc(size);
	ASSERT_RET(gEDCAInfo != NULL, OS_FAILED);
	memset(gEDCAInfo, 0, size);

	for(i=EDCA_WME_AC_FIRST; i<EDCA_WME_AC_MAX; i++)
		EDCA_QUE_Setup(i);






#if(TX_INT_ENABLE == 1)
	//Enable interrupt
	drv_edca_enable_interrupt(EDCA_TX_INT_BY_Q_EMPTY);
#endif
    //rate_control_fix_rate(14);
    //rate_control_jump_rate(TRUE);
	return OS_SUCCESS;
}








//----------------------------------------------------





//------------------------------------------------------------------
#if 0
//test code
#include <time.h>
#include <stdlib.h>

u32 EDCA_GetRandomLen()
{
	int r;

	srand(xTaskGetTickCount());
	r =  rand()% 1300+50;
	return r;
}


u32 EDCA_GetRandomT()
{
	int r;

	if (!GHW_REGS.WMM_CFG.bRdmIpt)
	{

		srand(xTaskGetTickCount());
		r =  rand()% 20+1;
		GHW_REGS.WMM_CFG.RdmVal[GHW_REGS.WMM_CFG.RdmIdx] = r;
		GHW_REGS.WMM_CFG.RdmIdx++;
	}
	else
	{
		r = GHW_REGS.WMM_CFG.RdmVal[GHW_REGS.WMM_CFG.RdmIdx++];
	}


	return r;
}
#endif

//---------------------------------------------------------------



#define HT_SIGNAL_EXT				6		//For 2.4G
#define HT_SIFS_TIME        		10
#define ACK_LEN						(14)	//include fcs
#define BA_LEN						(34)	//include fcs
#define RTS_LEN						(20)	//include fcs
#define CTS_LEN						(14)	//include fcs








#ifndef CONFIG_EDCA_HW_CALCULATE_NAV



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



#define CCK_SIFS_TIME        		HT_SIFS_TIME

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


static u32 EDCA_DurationHT(enum WLAN_RC_PHY_TYPE phy, u8 rateidx, u32 frameLen, bool bShortGI);



#endif// CONFIG_EDCA_HW_CALCULATE_NAV



#ifdef ENABLE_TX_QUEUE_STATS
u8      tx_queue_stats_trigger = 0;
u32	tx_queue_len_acc;
u32     tx_queue_len_sq_acc;
u32     tx_queue_samples;
u32     tx_queue_total_samples;
u32     tx_queue_zero_count;

void _init_tx_q_stats (void)
{
    tx_queue_len_acc = 0;
    tx_queue_len_sq_acc = 0;
    tx_queue_samples = 0;
    tx_queue_zero_count = 0;
} // end of - _init_tx_q_stats -


void start_tx_q_stats (u32 count)
{
    _init_tx_q_stats();
    tx_queue_total_samples = count;
    tx_queue_stats_trigger = 1;
} // end of - start_tx_q_stats -


void show_tx_q_stats (void)
{
    LOG_PRINTF("Zero: %u\n", tx_queue_zero_count);
    LOG_PRINTF("Sum: %u\n", tx_queue_len_acc);
    LOG_PRINTF("Sq Sum: %u\n", tx_queue_len_sq_acc);
    LOG_PRINTF("Samples: %u", tx_queue_samples);
} // end of - show_tx_q_stats -


void acc_tx_q_stats (void)
{
    u32	queue_len;

    if (tx_queue_stats_trigger == 0)
        return;

    queue_len = GET_FFO7_CNT;
    if (queue_len == 0)
        tx_queue_zero_count++;
    tx_queue_len_acc += queue_len;
    tx_queue_len_sq_acc += (queue_len * queue_len);

    if (++tx_queue_samples >= tx_queue_total_samples)
    	tx_queue_stats_trigger = 0;
}

#endif // ENABLE_TX_QUEUE_STATS




//=============================================================================================================================================================================================================================================================





void inline EDCA_get_cfg(u8 phy_idx, enum WLAN_RC_PHY_TYPE *phy, int *kbps, bool *shortPreamble, u8 *rateidx, bool *bShortGI)
{


	struct phy_rate_info *p_phy_rate_info;


	p_phy_rate_info = &ssv6200_phy_rate[phy_idx];
	*phy = p_phy_rate_info->type;
	*rateidx = *kbps = p_phy_rate_info->bitrate;
	*bShortGI = *shortPreamble = (u8)p_phy_rate_info->flag;

}

//EDCA_Duration adds SIFS time will add automatiocally.
u16 EDCA_Duration(enum WLAN_RC_PHY_TYPE phy, int kbps,
			   u32 frameLen, bool shortPreamble, u8 rateidx, bool bShortGI)
{
	u32 txTime;

#ifndef CONFIG_EDCA_HW_CALCULATE_NAV
	u32 bitsPerSymbol, numBits, numSymbols, phyTime;

#endif

#if 0
	if(2 == kbps || 4 == kbps || 11 == kbps || 22 == kbps ) // 1, 2, 5.5, 11
		phy = WLAN_RC_PHY_CCK;
	else if(12 == kbps || 18 == kbps || 24 == kbps || 36 == kbps || 48 == kbps || 72 == kbps || 96 == kbps || 108 == kbps)//6, 9, 12, 18, 24, 36, 48, 54
		phy = WLAN_RC_PHY_OFDM;
	else
	{;}
#endif

#ifdef CONFIG_EDCA_HW_CALCULATE_NAV

	txTime = drv_get_duration((NAV_PHY_MODE)phy,
	                          shortPreamble ? NAV_PREAMBLE_SHORT : NAV_PREAMBLE_LONG,
	                          bShortGI ? NAV_GI_SHORT : NAV_GI_LONG,
	                          (NAV_DATA_RATE)kbps,
	                          (NAV_MCS)rateidx,
	                          frameLen);
#else


	kbps = kbps*500;

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
		txTime = CCK_SIFS_TIME + phyTime + DIV_ROUND_UP((numBits * 1000) , kbps);//kbps=

		break;
	case WLAN_RC_PHY_OFDM:	//802.11g, 5g

		/*
		 * OFDM:
		 *
		 * N_DBPS = DATARATE x 4
		 * N_SYM = Ceiling((16+8xLENGTH+6) / N_DBPS)
		 *	(16 = SIGNAL time, 6 = tail bits)
		 *
		 * TXTIME = TPREAMBLE + TSIGNAL + TSYM  �� Ceiling((16 + 8 �� LENGTH + 6)/NDBPS)

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

	default:
		//txTime = EDCA_DurationHT(phy, rateidx, frameLen,
		//	    b40MHZ, bShortGI, shortPreamble);

		txTime = EDCA_DurationHT((enum WLAN_RC_PHY_TYPE)phy, rateidx, frameLen, bShortGI);
		break;
	}


#endif//#ifdef CONFIG_EDCA_HW_CALCULATE_NAV

	return txTime;
}



#ifndef CONFIG_EDCA_HW_CALCULATE_NAV


static u32 EDCA_DurationHT(enum WLAN_RC_PHY_TYPE phy, u8 rateidx, u32 frameLen, bool bShortGI)
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
		if(WLAN_RC_PHY_HT_MF == phy)
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

#endif

//=============================================================================================================================================================================================================================================================

#if 0
static u16 EDCA_FrameTxTimetoLlength(u16 txtime)
{
	u16 val = (txtime-6-20)>>2;
	return (val+val+val)-3;
}
#endif // 0

static u16 EDCA_FrameDuration(PKT_TxInfo *pTxInfo, u16 ack_dur, u16 fc)
{
	//----------------------------------------------------------------------------------------------
	//Config
	//get info from somewhere



	enum WLAN_RC_PHY_TYPE phy;
	int kbps;
	//int basic_kbps;

	bool shortPreamble = TRUE;
	u8 rateidx = 0;
	bool bShortGI = FALSE;



//----------------------------------------------------------------------------------------------
	int next_frag_len;
	int dur;
	//u16 fc;
	const u8 *raw;





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
	raw = GET_ADDR_HDR80211(pTxInfo, 0);
	//fc = (raw[1]<<8) | raw[0];

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

		//No need to modify duration
		if (IS_PSPOLL(fc))
			return 65535;
		return 0;
	}

	/* data/mgmt */
#if 0
	if (0 /* FIX: data/mgmt during CFP */)
		return (le16)(32768);
#endif
	if ((raw[4]&0x01)) /* Group address as the destination - no ACK */
		return 0;

	/* Don't calculate ACKs for QoS Frames with NoAck Policy set */
	if ((pTxInfo->ack_policy == 0x00) || (pTxInfo->aggregation == 1)) /* 0x00 = normal ack */
	{
        #if 1
        dur=ack_dur;
        #else
		EDCA_get_cfg((u8)ssv6200_phy_rate[pTxInfo->drate_idx].ackIndex, &phy, &basic_kbps, &shortPreamble, &rateidx, &bShortGI);
	/* Time needed to transmit ACK
	 * (10 bytes + 4-byte FCS = 112 bits) plus SIFS; rounded up
	 * to closest integer */
		dur = EDCA_Duration(phy, basic_kbps, ACK_LEN/*10+FCS_LEN*/, shortPreamble, rateidx, bShortGI);
        #endif
	}
	else
		dur = 0;

	if (pTxInfo->next_frag_pid) {
		const PKT_TxInfo *pFragTxInfo;

		EDCA_get_cfg(pTxInfo->drate_idx, &phy, &kbps, &shortPreamble, &rateidx, &bShortGI);

		/* Frame is fragmented: duration increases with time needed to
		 * transmit next fragment plus ACK and 2 x SIFS. */
		dur *= 2; /* ACK + SIFS */
		/* next fragment */
		pFragTxInfo = (const PKT_TxInfo *)PBUF_MapIDtoPkt(pTxInfo->next_frag_pid);
		next_frag_len = pFragTxInfo->len+FCS_LEN;
		dur += EDCA_Duration(phy, kbps, (u32)next_frag_len, shortPreamble, rateidx, bShortGI);
	}

	return (dur);
}





u16 EDCA_generic_frame_duration(enum WLAN_RC_PHY_TYPE phy, int kbps, u32 frameLen, bool shortPreamble, u8 rateidx, bool bShortGI)
{
	u16 dur = 0;

	dur += EDCA_Duration(phy, kbps, frameLen, shortPreamble, rateidx, bShortGI);
	dur -= HT_SIFS_TIME;

	return dur;
}



u16 EDCA_rts_ctstoself_duration(PKT_TxInfo *pTxInfo, u16 *dframe_txtime, u16 ack_dur)
{

	u16 dur=0;

//----------------------------------------------------------------------------------------------
//Config
	enum WLAN_RC_PHY_TYPE phy;
	int kbps;
	//int basic_kbps;
	int cts_rts_kbps;
	bool shortPreamble;
	u8 rateidx;
	bool bShortGI;

//----------------------------------------------------------------------------------------------

	//RTS/CTS   = CTS duration(+SIFS)+Data frame duration(+SIFS)+(ACK duration(+SIFS))
	//CTS to self = 					Data frame duration(+SIFS)+(ACK duration(+SIFS))


	if(pTxInfo->do_rts_cts == 0)
		return 0;
	else if(pTxInfo->do_rts_cts == 1)//1: need RTS/CTS
	{
    EDCA_get_cfg(pTxInfo->crate_idx, &phy, &cts_rts_kbps, &shortPreamble, &rateidx, &bShortGI);
	/* CTS duration */
		dur = EDCA_Duration(phy, cts_rts_kbps, CTS_LEN, shortPreamble, rateidx, bShortGI);
	}
    else
    {}



	EDCA_get_cfg(pTxInfo->drate_idx, &phy, &kbps, &shortPreamble, &rateidx, &bShortGI);
	/* Data frame duration */
	*dframe_txtime = EDCA_Duration(phy, kbps, pTxInfo->len+FCS_LEN, shortPreamble, rateidx, bShortGI);
	dur+=*dframe_txtime;



	/* ACK duration */
	if (pTxInfo->ack_policy == 0x00) /* 0x00 = normal ack */
	{
        #if 1
        dur+=ack_dur;
        #else
		EDCA_get_cfg((u8)ssv6200_phy_rate[pTxInfo->drate_idx].ackIndex, &phy, &basic_kbps, &shortPreamble, &rateidx, &bShortGI);
		dur += EDCA_Duration(phy, basic_kbps, ACK_LEN, shortPreamble, rateidx, bShortGI);
        #endif
	}
	return (dur);



}


void EDCA_FillRelatedDuration(PKT_TxInfo *pTxInfo, u16 fc)
{

	struct HDR80211_Data_st * hdr = (struct HDR80211_Data_st * )GET_ADDR_HDR80211(pTxInfo, 0);
	u16 duration;
    u16 ack_dur=0;

    enum WLAN_RC_PHY_TYPE d_phy, c_phy;
	int kbps;
	int basic_kbps;
	//int cts_rts_kbps;
    bool shortPreamble;
	u8 d_rateidx, c_rateidx;
	bool bShortGI;
	u16 frame_consume_time = 0;
	u16 cts_txtime = 0, rts_txtime = 0, df_txtime = 0;
    SSV6XXX_STA		*p_sta;
    pTxInfo->dl_length = 0;
    pTxInfo->frame_consume_time = 0;
    pTxInfo->rts_cts_nav = 0;

//	{

			//hdr = (struct HDR80211_Data_st *)((u8*)pTxInfo+pTxInfo->hdr_offset); //(struct ieee80211_hdr *)skb->data;
//		u8* raw = (const u8 *)pTxInfo+pTxInfo->hdr_offset;
//		u16 fc = (raw[1]<<8) | raw[0];

// 		if (IS_DATA_FRAME(fc)) {
// 			LOG_PRINTF("data rate\n");
// 			pTxInfo->drate_idx = 3;
// 			pTxInfo->crate_idx = 3;
// 		}


//	}
    #if(PACKET_TRACE==1)
    pt((void *)pTxInfo,M_ENG_MAX,EN_LOC_FILL_DUR);
    #endif
    //Step 0: Calculate ACK duration
    p_sta = &g6200StaInfo[pTxInfo->wsid];
    EDCA_get_cfg((u8)ssv6200_phy_rate[pTxInfo->drate_idx].ackIndex, &d_phy, &basic_kbps, &shortPreamble, &d_rateidx, &bShortGI);
    if((p_sta->ampdu_tx == TRUE)&&(pTxInfo->aggregation==1))
    {
        ack_dur = EDCA_Duration(d_phy, basic_kbps, BA_LEN, shortPreamble, d_rateidx, bShortGI);
        /*
            Our durations of AMPDU frames is more than other devices 4us, we are not sure the reason now,
            so we add this patch.
        */
        ack_dur -= 4;
    }
    else
    {
        ack_dur = EDCA_Duration(d_phy, basic_kbps, ACK_LEN, shortPreamble, d_rateidx, bShortGI);
    }

	//Step 1. Frame Duration
	duration = EDCA_FrameDuration(pTxInfo, ack_dur, fc);

	//65535 is an invalid value. we should skip to update duration
	if (65535 != duration)
		hdr->dur = duration;

//------------------------------

	//Step2.Calculate Frame Xmit Time

	EDCA_get_cfg(pTxInfo->drate_idx, &d_phy, &kbps, &shortPreamble, &d_rateidx, &bShortGI);

	//txtime = EDCA_generic_frame_duration( phy,  kbps,  framelen,  shortPreamble,  rateidx,  bShortGI);
    df_txtime = EDCA_Duration(d_phy, kbps, (pTxInfo->len+FCS_LEN), shortPreamble, d_rateidx, bShortGI);
    //printf("d_phy=%d, kbps=%d, len=%d, shortPreamble=%d, d_rateidx=%d,bShortGI=%d,df_txtime=%d\r\n",d_phy,kbps,(pTxInfo->len+FCS_LEN),shortPreamble,d_rateidx,bShortGI,df_txtime);
//------------------------------

	//Step 3. RTS/CTS-to-Self NAV
	if (pTxInfo->do_rts_cts != 0)//RTS/CTS, CTS-to-Self
	{
        EDCA_get_cfg(pTxInfo->crate_idx, &c_phy, &kbps, &shortPreamble, &c_rateidx, &bShortGI);
        cts_txtime = EDCA_Duration(c_phy, kbps, CTS_LEN, shortPreamble, c_rateidx, bShortGI);
        if(pTxInfo->do_rts_cts == 1)//1: need RTS/CTS
            rts_txtime = EDCA_Duration(c_phy, kbps, RTS_LEN, shortPreamble, c_rateidx, bShortGI);

		pTxInfo->rts_cts_nav = df_txtime;
        pTxInfo->rts_cts_nav += ack_dur;

        //Add cts time to rts_cts_nav for RTS/CTS
        if(pTxInfo->do_rts_cts == 1)
            pTxInfo->rts_cts_nav += cts_txtime;

        //frame_consume_time must be more than rts_cts_nav with actual frame activity
		frame_consume_time = pTxInfo->rts_cts_nav;
        if (pTxInfo->do_rts_cts == 1)
            frame_consume_time += rts_txtime;
        else
            frame_consume_time += cts_txtime;
        //EDCA_Duration include SIFS_TIME, need to exclude one SIFS_TIME for frame_consume_time
        frame_consume_time -= HT_SIFS_TIME;
    }

//------------------------------

	pTxInfo->frame_consume_time = ((frame_consume_time>>5) + 1);

//------------------------------
	//check if need to caculate L-Length
	if(d_phy >= WLAN_RC_PHY_HT_MF)
	{
		u16 temp;	//ceil[ (TXTIME-T_SIGEXT-20)/4 ], plus 3 cause need to get ceil

		temp = ((df_txtime-(HT_SIFS_TIME+HT_SIGNAL_EXT+20))+3)>>2;	//ceil[ (TXTIME-T_SIGEXT-20)/4 ], plus 3 cause need to get ceil
		pTxInfo->dl_length = temp*3 - 3;						//ceil[ (TXTIME-T_SIGEXT-20)/4 ]*3-3
	}

    ack_dur=0;
}


//======================================================================================================================================================================================================

#if (EDCA_DBG == 1)

extern u8 TxFrameCnt;


void inline EDCA_NotifyHost(void)
{

	if(EDCA_IS_ANY_ONE_WAIT_TX_DONE || TxFrameCnt<GET_TX_SEG)
		return;

	TxFrameCnt = 0;

#ifdef __EDCA_NOTIFY_HOST__
	//Send event to notify host all data queue are empty.
	{
		HDR_HostEvent *ev=NULL;
		HOST_EVENT_ALLOC(ev,SOC_EVT_TX_ALL_DONE , 0);
		HOST_EVENT_SET_LEN(ev,0);
		HOST_EVENT_SEND(ev);
	}
#endif//__EDCA_NOTIFY_HOST__

}




#if (EDCA_DBG == 1)
void inline EDCA_GetXmit_Result(enum edca_tx_queue_subtype queuetype, u16 *frm_success, u16 *frm_fail, u16 *ack_success,
																				u16 *ack_fail, u32 *id_map)
{


	switch(queuetype)
	{
		case EDCA_WME_AC_BK:
			*frm_success 	= GET_DBG_Q0_SUCC;
			*frm_fail 		= GET_DBG_Q0_FAIL;
			*ack_success 	= GET_DBG_Q0_ACK_SUCC;
			*ack_fail 		= GET_DBG_Q0_ACK_FAIL;
			*id_map			= GET_TXQ0_MTX_Q_ID_MAP_L;

			break;

		case EDCA_WME_AC_BE:
			*frm_success 	= GET_DBG_Q1_SUCC;
			*frm_fail 		= GET_DBG_Q1_FAIL;
			*ack_success 	= GET_DBG_Q1_ACK_SUCC;
			*ack_fail 		= GET_DBG_Q1_ACK_FAIL;
			*id_map			= GET_TXQ1_MTX_Q_ID_MAP_L;

			break;

		case EDCA_WME_AC_VI:
			*frm_success 	= GET_DBG_Q2_SUCC;
			*frm_fail 		= GET_DBG_Q2_FAIL;
			*ack_success 	= GET_DBG_Q2_ACK_SUCC;
			*ack_fail 		= GET_DBG_Q2_ACK_FAIL;
			*id_map			= GET_TXQ2_MTX_Q_ID_MAP_L;

			break;

		case EDCA_WME_AC_VO:
			*frm_success 	= GET_DBG_Q3_SUCC;
			*frm_fail 		= GET_DBG_Q3_FAIL;
			*ack_success 	= GET_DBG_Q3_ACK_SUCC;
			*ack_fail 		= GET_DBG_Q3_ACK_FAIL;
			*id_map			= GET_TXQ3_MTX_Q_ID_MAP_L;

			break;

		default:
			ASSERT(0);
			break;

	}






}




void inline EDCA_Clear_Mib(enum edca_tx_queue_subtype queuetype)
{
	switch(queuetype)
	{
		case EDCA_WME_AC_BK:
			SET_Q0_PKT_CLS_MIB_EN(0);
			SET_Q0_PKT_CLS_MIB_EN(1);
			break;
		case EDCA_WME_AC_BE:
			SET_Q1_PKT_CLS_MIB_EN(0);
			SET_Q1_PKT_CLS_MIB_EN(1);
			break;
		case EDCA_WME_AC_VI:
			SET_Q2_PKT_CLS_MIB_EN(0);
			SET_Q2_PKT_CLS_MIB_EN(1);
			break;

		case EDCA_WME_AC_VO:
			SET_Q3_PKT_CLS_MIB_EN(0);
			SET_Q3_PKT_CLS_MIB_EN(1);
			break;
		default:
			ASSERT(0);
			break;
	}
}






#endif//#if (EDCA_DBG == 1)




















#endif//EDCA_DBG




#ifndef _FPGA_
//Simulator

bool EDCA_SendFrametoMailbox(enum edca_tx_queue_subtype queuetype, enum edca_xmit_type xmit_type)
{
	PKT_TxInfo *pTxInfo;
	u8 xmit_num=0;

	if (EDCA_XMIT_ALL != xmit_type)
		ASSERT(FALSE);


	LOG_PRINTF("EDCA_SendFrametoMailbox--->%d\n", queuetype);

	//Pop All from queue
	while (pTxInfo=EDCA_RemoveFromQueue(queuetype))
	{
		PBUF_AddFlow(pTxInfo,"ME_EDCA","out");


#if (EDCA_DBG == 1)
		gEDCAInfo->txq[queuetype].mib_info[xmit_num].ack_policy = pTxInfo->ack_policy;
#endif
		xmit_num++;

		ENG_MBOX_NEXT((u32)pTxInfo);
	}

	gEDCAInfo->txq[queuetype].xmit_num = xmit_num;


	EDCA_SET_Q_WAIT_TX_DONE(queuetype, TRUE);

	return FALSE;
}





void EDCA_MacTxIntNotify(enum edca_tx_queue_subtype queuetype)
{
	LOG_PRINTF("EDCA_MacTxIntNotify:Queue %d\n", queuetype);

	EDCA_PrintQueueInfo();

	if(!EDCA_QueueEmpty(queuetype))
		EDCA_SendFrametoMailbox(queuetype, EDCA_XMIT_ALL);
	else
	{

		EDCA_SET_Q_WAIT_TX_DONE(queuetype, FALSE);

#if (EDCA_DBG == 1)
		EDCA_NotifyHost();
#endif


	}

	EDCA_PrintQueueInfo();
	LOG_PRINTF("info_flag:%8x\n",gEDCAInfo->info_flag);

};


#else//_FPGA_
//-----------------------------------------------------------------------------------------
//EDCA FPGA


bool EDCA_SendFrametoMailbox(enum edca_tx_queue_subtype queuetype, enum edca_xmit_type xmit_type)////edca_tx_queue_type
{
	PKT_TxInfo *pTxInfo;
	u8 xmit_num=0;

	ASSERT_RET(EDCA_XMIT_ALL == xmit_type, FALSE);

	LOG_PRINTF("EDCA_SendFrametoMailbox--->\n");
	EDCA_PrintQueueInfo();

	drv_edca_queue_halt((edca_tx_queue_type)queuetype, TRUE);


	//Pop All from sw queue and put in hw queue
	while ((pTxInfo=(PKT_TxInfo *)EDCA_RemoveFromQueue(queuetype)) != NULL)
	{

#if (EDCA_DBG == 1)
		gEDCAInfo->txq[queuetype].mib_info[xmit_num].ack_policy = pTxInfo->ack_policy;
#endif

		xmit_num++;
		ENG_MBOX_SEND((u32)(queuetype+M_ENG_TX_EDCA1), (u32)pTxInfo);
	}
	gEDCAInfo->txq[queuetype].xmit_num = xmit_num;



	drv_edca_queue_halt((edca_tx_queue_type)queuetype, FALSE);

	EDCA_PrintQueueInfo();
	EDCA_SET_Q_WAIT_TX_DONE(queuetype, TRUE);

	return TRUE;
}





void EDCA_MacTxIntNotify(enum edca_tx_queue_subtype queuetype)
{
	LOG_PRINTF("EDCA_MacTxIntNotify:Queue %d\n", queuetype);


#if (EDCA_DBG == 1)
	//Check Tx ID map
  	//only check this in mac normal mode
	if(drv_mac_get_mac_loopback()== 0 && drv_mac_get_mac_m2m() == 0 && drv_mac_get_phy_loopback()== 0 )
	{

		int i;

		u16 frm_success = 0;
		u16 frm_fail = 0;
		u16 ack_success = 0;
		u16 ack_fail = 0;
		u32 id_map = 0;

		u16 bitmap_frm_success = 0;
		u16 bitmap_frm_fail = 0;
		u16 exptext_ack_num = 0;


		EDCA_GetXmit_Result(queuetype, &frm_success, &frm_fail, &ack_success, &ack_fail, &id_map);


		for(i=0; i<gEDCAInfo->txq[queuetype].xmit_num; i++)
		{
			if(id_map&BIT(i))
			{
				//Success
				bitmap_frm_success++;
				if(gEDCAInfo->txq[queuetype].mib_info[i].ack_policy == 0x00)//normal ack
					exptext_ack_num++;
			}
			else
			{
				//Fail
				bitmap_frm_fail++;
			}

		}

		//
		LOG_PRINTF("Int Notify Tx queue:%d\n");
		LOG_PRINTF("frm_success:%d frm_fail:%d ack_success:%d ack_fail:%d id_map:0x%08x\n", frm_success, frm_fail, ack_success, ack_fail, id_map);
		LOG_PRINTF("frm_success:%d frm_fail:%d expect_ack_num:%d\n", bitmap_frm_success, bitmap_frm_fail, exptext_ack_num);



		//Check id map and conter is match or not
		if(exptext_ack_num != (ack_fail+ack_success) ||
			bitmap_frm_success != frm_success ||
			bitmap_frm_fail != frm_fail)
			ASSERT(FALSE);


		//Clear TX MIB counter
		EDCA_Clear_Mib(queuetype);

	}
#endif


	gEDCAInfo->txq[queuetype].xmit_num = 0;


	if(!EDCA_QueueEmpty(queuetype))
		EDCA_SendFrametoMailbox(queuetype, EDCA_XMIT_ALL);
	else
	{

		EDCA_SET_Q_WAIT_TX_DONE(queuetype, FALSE);

#if (EDCA_DBG == 1)
		EDCA_NotifyHost();
#endif


	}

}

//-----------------------------------------------------------------------------------------
#endif//_FPGA_



void _packetdump(const char *title, const u8 *buf,
                             size_t len);



#if (MACTXRX_CONTROL_DURATION_SIM == 0)


#ifdef THROUGHPUT_TEST
extern u8	throughputFixRate;
extern u8	throughputDutMode;
#endif

u8 scrambler_seed=1;
extern u8 g_txQueueMapping[MAX_TX_DATA_QUEUE];
#if (N_MODE_AMPDU_TX == 1)
extern void ampdu_reform_mpdu_to_apmdu(PKT_TxInfo *pPktInfo, u16 fc);
extern void *rate_control_pid_prb_frame_gen(u8 wsid);
#endif
void EDCA_Handler(PKT_Info *pPktInfo, s32 reason)
{
    /*lint -save -e831 -e740 */
    PKT_TxInfo *pTxInfo=(PKT_TxInfo *)pPktInfo;
    u16 fc = GET_HDR80211_FC(pTxInfo);
    u16 QC=0;
    u8 wrong_ack_policy=0;
    /*lint -restore */

    if(pTxInfo->c_type ==M0_TXREQ && pTxInfo->f80211!=1)
    {
        printf("EDCA_Handler free packet addr:%x\n",pPktInfo);

        PBUF_MFree((void *)pPktInfo);
        return;
    }

//	LOG_PRINTF("EDCA_Handler edca.c\n");
#ifdef ENABLE_TX_QUEUE_STATS
    acc_tx_q_stats();
#endif // ENABLE_TX_QUEUE_STATS

    //Particular data will be saturated at PHY, the patch will avoid it.
    SET_RG_SMB_DEF(scrambler_seed);
    scrambler_seed++;
    if(scrambler_seed == 0x80)
    {
        scrambler_seed = 1;
    }

#ifdef THROUGHPUT_TEST
    if(throughputDutMode)
    {
        //By pass packet
        ;
    }
    else
    {
        pTxInfo->drate_idx = throughputFixRate;
        if(pTxInfo->do_rts_cts)
            pTxInfo->crate_idx= ssv6200_phy_rate[pTxInfo->drate_idx].ackIndex;
    }
#else
    //Rate control
    //06052015: mgmt frame & control frame duration = 0, data rate = 1M
    if((IS_DATA_FRAME(fc)))
    {
        //issue:RB-272
        //check ether type frame send with 1M

        if(GET_HDR80211_Ether_Type(pPktInfo) == 0x888E)
        {
            pTxInfo->drate_idx = 0;
            pTxInfo->crate_idx = 0;
        }
        else
        {
            //rate_control_fix_rate(14);
            pTxInfo->drate_idx = rate_control_pid_get_rate((void*)pTxInfo, IS_MGMT_FRAME(fc), GET_HDR80211_SC_SEQNUM(pTxInfo));

            //Need to use NON-OFDM rates to send protect frame, so far using 1M for sending protect frame
            if ((pTxInfo->do_rts_cts != 0) && (rate_control_get_erp_protect() == FALSE))
                pTxInfo->crate_idx= ssv6200_phy_rate[pTxInfo->drate_idx].ackIndex;
            else
                pTxInfo->crate_idx = 0;
        }
    }
    else
    {
        pTxInfo->drate_idx = 0;
        pTxInfo->crate_idx = 0;
    }
#endif
    //RB-340. check the ack_policy of every tx packets
    if(IS_CTRL_FRAME(fc)){
        if(1!=pTxInfo->ack_policy){
            pTxInfo->ack_policy=1;
            wrong_ack_policy=1;
        }
    }

    if(IS_MGMT_FRAME(fc)){
        if(0!=pTxInfo->unicast){ //unicast frames
            if(0!=pTxInfo->ack_policy) {
                pTxInfo->ack_policy=0;
                wrong_ack_policy=1;
            }
        }
        else{ //multicast/broadcast frames
            if(1!=pTxInfo->ack_policy){
                pTxInfo->ack_policy=1;
                wrong_ack_policy=1;
            }
        }
    }

    if(IS_DATA_FRAME(fc)){
        if(0!=pTxInfo->unicast){ //unicast frames
            //RB-240
            //This is just a work around solution, we still don't know who dirty the QoS ctrl field and ack_policy
            if(0!=pTxInfo->ack_policy){
                pTxInfo->ack_policy=0;
                wrong_ack_policy=1;
                if(pTxInfo->qos==1){
                    QC=GET_HDR80211_QC(pTxInfo);
                    QC&=(~0x60);//clear bit5-6
                    SET_HDR80211_QC(pTxInfo,QC);
                }
            }
        }else{ //multicast/broadcast frames
            if(1!=pTxInfo->ack_policy) {
                pTxInfo->ack_policy=1;
                wrong_ack_policy=1;
            }
        }
    }

    if(1==wrong_ack_policy){
        gAckPolicyErrorCount++;
        //printf("\33[31m ack_policy(%d),QC(0x%x) seq(%d) \33[0m\r\n",pTxInfo->ack_policy,QC,GET_HDR80211_SC_SEQNUM(pPktInfo));
    }

#if (N_MODE_AMPDU_TX == 1)
    //Reform to AMPDU Sharp
    if ((IS_QOS_DATA(fc)) && !(IS_QOS_NULL_DATA(fc)) && (ssv6200_phy_rate[pTxInfo->drate_idx].type >= WLAN_RC_PHY_HT_MF))
        ampdu_reform_mpdu_to_apmdu(pTxInfo, fc);
        if((pTxInfo->aggregation==1)&&(pTxInfo->do_rts_cts!=0))
        {

            //we change the settoing of SSV6051_TX_PKT_RSVD_SETTING from 3 to 5,
            //it can guartnee that we have enough spaces for moving
            // The detail is on RB-450
            #define AMPDU_HW_HDR_OFFSET 0x50
            u8 shift=0;
            u8 *pS=NULL,*pD=NULL;
            u32 len=0;
            shift=AMPDU_HW_HDR_OFFSET-pTxInfo->hdr_offset;
            pS=(u8 *)((u32)pTxInfo+pTxInfo->hdr_offset+pTxInfo->len-1);
            pD=(u8 *)((u32)pTxInfo+pTxInfo->hdr_offset+pTxInfo->len+shift-1);
            len=pTxInfo->len;
            do{ *pD--=*pS--;}while(--len);
            pTxInfo->hdr_offset=AMPDU_HW_HDR_OFFSET;
            pTxInfo->payload_offset+=shift;
        }

#endif
    //Step0.Calculate ACK duration
    //Step1.Calculate RTS/CST-to-Self Duration
    //Step2.Calculate Frame Duration
    //Step3.Calculate Frame Xmit Time
    EDCA_FillRelatedDuration(pTxInfo, fc);


#if ( MACTXRX_CONTROL_ENABLE == 0)
    if(IS_DATA_FRAME(fc)){
#if (N_MODE_AMPDU_TX == 1)
        bool IsPrbPktNeeded = FALSE;
        u8 wsid = WLAN_MAX_STA;
#endif
        /*
        In auto scanning, STA must send the null data to AP before changging the channel,
        in order to decrease the probability of AP miss the null data,
        we send the null data to air through 5th queue because 5th queue has the highest priority.

        I didn't pick up this soultion in sw data queue becuase the auto scanning solution doesn't down now.

        if(IS_NULL_DATA(fc))
	        ENG_MBOX_SEND((u32)M_ENG_TX_MNG,(u32)pTxInfo);
        else
            ENG_MBOX_SEND((u32)M_ENG_TX_EDCA0+queuetype,(u32)pTxInfo);
        */
        s32 ret=FALSE;

#if (N_MODE_AMPDU_TX == 1)
        if ((0==pTxInfo->unicast) && (pTxInfo->aggregation == 0)) //g_txQueueMapping
#else
        if (0==pTxInfo->unicast)
#endif
        {
            //ret=ENG_MBOX_SEND((u32)M_ENG_TX_EDCA3,(u32)pTxInfo);
            ret=ENG_MBOX_SEND((u32)g_txQueueMapping[LAST_TX_DATA_QUEUE]+M_ENG_TX_EDCA0,(u32)pTxInfo);
        }
        else
        {
#if (N_MODE_AMPDU_TX == 1)
            if((pTxInfo->RSVD_0 == RC_SAMPLE) && (rate_control_pid_is_probe_needed(pTxInfo->wsid) == TRUE))
            {
                IsPrbPktNeeded = TRUE;
                wsid = pTxInfo->wsid;
            }
#endif
            ret=ENG_MBOX_SEND((u32)g_txQueueMapping[pTxInfo->wsid]+M_ENG_TX_EDCA0,(u32)pTxInfo);
        }

        if(FALSE==ret)
        {
            printf("drop this packet\n");
            PBUF_MFree((void *)pTxInfo);
        }
#if (N_MODE_AMPDU_TX == 1)
        else
        {
            if (IsPrbPktNeeded == TRUE)
            {
                u8 prb_cnt = MAXPROBES;
                //printf("[EDCA]: Gen probing frame\n");
                pTxInfo = NULL;
                while ((ret != FALSE) && (prb_cnt > 0))
                {
                    pTxInfo = rate_control_pid_prb_frame_gen(wsid);
                    if (pTxInfo == NULL)
                        break;
                    ret=ENG_MBOX_SEND((u32)g_txQueueMapping[pTxInfo->wsid]+M_ENG_TX_EDCA0,(u32)pTxInfo);
                    prb_cnt--;
                }
                if(ret == FALSE)
                {
                    rate_control_pid_return_probe_frame(pTxInfo);
                    printf("[RC]: pre allocate probe frame failed to send\n");
                }
            }
        }
#endif
    }else{
        //Avoid no one anwser porbe resp(jump to other channel), it will cause throughput low
        //we modify it as broadcast frame
        if (IS_PROBE_RESP(fc))
            pTxInfo->ack_policy =1;

        //send mgmt/ctl frame out ASAP.
        //if(FALSE==ENG_MBOX_SEND((u32)M_ENG_TX_EDCA3,(u32)pTxInfo)){
        if(FALSE==ENG_MBOX_SEND((u32)g_txQueueMapping[LAST_TX_DATA_QUEUE]+M_ENG_TX_EDCA0,(u32)pTxInfo))
        {
            printf("drop this packet\n");
            PBUF_MFree((void *)pTxInfo);
        }
    }


#endif//#if ( MACTXRX_CONTROL_ENABLE == 0)

}

#endif//MACTXRX_CONTROL_DURATION_SIM



//---------------------------------
//
//
// #define RATE_TABLE_SIZE 72
//
//
//
// #define WLAN_RC_DS_FLAG  0x01
// #define WLAN_RC_40_FLAG  0x02
// #define WLAN_RC_SGI_FLAG 0x04
// #define WLAN_RC_HT_FLAG  0x08
//
//
//
// /**
//  * struct ath_rate_table - Rate Control table
//  * @rate_cnt: total number of rates for the given wireless mode
//  * @mcs_start: MCS rate index offset
//  * @rate_flags: Rate Control flags
//  * @phy: CCK/OFDM/HT20/HT40
//  * @ratekbps: rate in Kbits per second
//  * @user_ratekbps: user rate in Kbits per second
//  * @ratecode: rate that goes into HW descriptors
//  * @dot11rate: value that goes into supported
//  * 	rates info element of MLME
//  * @ctrl_rate: Index of next lower basic rate, used for duration computation
//  * @cw40index: Index of rates having 40MHz channel width
//  * @sgi_index: Index of rates having Short Guard Interval
//  * @ht_index: high throughput rates having 40MHz channel width and
//  * 	Short Guard Interval
//  * @probe_interval: interval for rate control to probe for other rates
//  * @initial_ratemax: initial ratemax value
//  */
// struct ath_rate_table {
// 	int rate_cnt;
// 	int mcs_start;
// 	struct {
// 		u16 rate_flags;
// 		u8 phy;
// 		u32 ratekbps;
// 		u32 user_ratekbps;
// 		u8 ratecode;
// 		u8 dot11rate;
// 		u8 ctrl_rate;
// 		u8 cw40index;
// 		u8 sgi_index;
// 		u8 ht_index;
// 	} info[RATE_TABLE_SIZE];
// 	u32 probe_interval;
// 	u8 initial_ratemax;
// };
//
//
//
// #define RC_INVALID	0x0000
// #define RC_LEGACY	0x0001
// #define RC_SS		0x0002
// #define RC_DS		0x0004
// #define RC_TS		0x0008
// #define RC_HT_20	0x0010
// #define RC_HT_40	0x0020
//
// #define RC_STREAM_MASK	0xe
/*
#define RC_DS_OR_LATER(f)	((((f) & RC_STREAM_MASK) == RC_DS) || \
 				(((f) & RC_STREAM_MASK) == (RC_DS | RC_TS)))
*/
// #define RC_TS_ONLY(f)		(((f) & RC_STREAM_MASK) == RC_TS)
// #define RC_SS_OR_LEGACY(f)	((f) & (RC_SS | RC_LEGACY))
//
// #define RC_HT_2040		(RC_HT_20 | RC_HT_40)
// #define RC_ALL_STREAM		(RC_SS | RC_DS | RC_TS)
// //#define RC_L_SD			(RC_LEGACY | RC_SS | RC_DS)
// #define RC_L_SDT		(RC_LEGACY | RC_SS | RC_DS | RC_TS)
// #define RC_HT_S_20		(RC_HT_20 | RC_SS)
// //#define RC_HT_D_20		(RC_HT_20 | RC_DS)
// //#define RC_HT_T_20		(RC_HT_20 | RC_TS)
// //#define RC_HT_S_40		(RC_HT_40 | RC_SS)
// //#define RC_HT_D_40		(RC_HT_40 | RC_DS)
// //#define RC_HT_T_40		(RC_HT_40 | RC_TS)
//
// #define RC_HT_SD_20		(RC_HT_20 | RC_SS | RC_DS)
// #define RC_HT_DT_20		(RC_HT_20 | RC_DS | RC_TS)
// #define RC_HT_SD_40		(RC_HT_40 | RC_SS | RC_DS)
// #define RC_HT_DT_40		(RC_HT_40 | RC_DS | RC_TS)
//
// //#define RC_HT_SD_2040		(RC_HT_2040 | RC_SS | RC_DS)
// //#define RC_HT_SDT_2040		(RC_HT_2040 | RC_SS | RC_DS | RC_TS)
//
// #define RC_HT_SDT_20		(RC_HT_20 | RC_SS | RC_DS | RC_TS)
// #define RC_HT_SDT_40		(RC_HT_40 | RC_SS | RC_DS | RC_TS)
//
// #define RC_ALL			(RC_LEGACY | RC_HT_2040 | RC_ALL_STREAM)
//
//
//
//
//
// /* 4ms frame limit not used for NG mode.  The values filled
//  * for HT are the 64K max aggregate limit */
//
// static const struct ath_rate_table ssv_11ng_ratetable = {
// 	72,
// 	12, /* MCS start */
// 	{
// 		{ RC_ALL, WLAN_RC_PHY_CCK, 1000,
// 			900, 0, 2, 0, 0, 0, 0 }, /* 1 Mb */
// 		/*[1] =*/ { RC_ALL, WLAN_RC_PHY_CCK, 2000,
// 			1900, 1, 4, 1, 1, 1, 1 }, /* 2 Mb */
// 		/*[2] =*/ { RC_ALL, WLAN_RC_PHY_CCK, 5500,
// 			4900, 2, 11, 2, 2, 2, 2 }, /* 5.5 Mb */
// 		/*[3] =*/ { RC_ALL, WLAN_RC_PHY_CCK, 11000,
// 			8100, 3, 22, 3, 3, 3, 3 }, /* 11 Mb */
// 		/*[4] =*/ { RC_INVALID, WLAN_RC_PHY_OFDM, 6000,
// 			5400, 4, 12, 4, 4, 4, 4 }, /* 6 Mb */
// 		/*[5] =*/ { RC_INVALID, WLAN_RC_PHY_OFDM, 9000,
// 			7800, 5, 18, 4, 5, 5, 5 }, /* 9 Mb */
// 		/*[6] =*/ { RC_L_SDT, WLAN_RC_PHY_OFDM, 12000,
// 			10100, 6, 24, 6, 6, 6, 6 }, /* 12 Mb */
// 		/*[7] =*/ { RC_L_SDT, WLAN_RC_PHY_OFDM, 18000,
// 			14100, 7, 36, 6, 7, 7, 7 }, /* 18 Mb */
// 		/*[8] =*/ { RC_L_SDT, WLAN_RC_PHY_OFDM, 24000,
// 			17700, 8, 48, 8, 8, 8, 8 }, /* 24 Mb */
// 		/*[9] =*/ { RC_L_SDT, WLAN_RC_PHY_OFDM, 36000,
// 			23700, 9, 72, 8, 9, 9, 9 }, /* 36 Mb */
// 		/*[10] =*/ { RC_L_SDT, WLAN_RC_PHY_OFDM, 48000,
// 			27400, 10, 96, 8, 10, 10, 10 }, /* 48 Mb */
// 		/*[11] =*/ { RC_L_SDT, WLAN_RC_PHY_OFDM, 54000,
// 			30900, 11, 108, 8, 11, 11, 11 }, /* 54 Mb */
//
// 		/*[12] =*/ { RC_INVALID, WLAN_RC_PHY_HT_20_SS, 6500,
// 			6400, 0, 0, 4, 42, 12, 42 }, /* 6.5 Mb */
// 		/*[13] =*/ { RC_HT_SDT_20, WLAN_RC_PHY_HT_20_SS, 13000,
// 			12700, 1, 1, 6, 43, 13, 43 }, /* 13 Mb */
// 		/*[14] =*/ { RC_HT_SDT_20, WLAN_RC_PHY_HT_20_SS, 19500,
// 			18800, 2, 2, 6, 44, 14, 44 }, /* 19.5 Mb*/
// 		/*[15] =*/ { RC_HT_SD_20, WLAN_RC_PHY_HT_20_SS, 26000,
// 			25000, 3, 3, 8, 45, 15, 45 }, /* 26 Mb */
// 		/*[16] =*/ { RC_HT_SD_20, WLAN_RC_PHY_HT_20_SS, 39000,
// 			36700, 4, 4, 8, 46, 16, 46 }, /* 39 Mb */
// 		/*[17] =*/ { RC_HT_S_20, WLAN_RC_PHY_HT_20_SS, 52000,
// 			48100, 5, 5, 8, 47, 17, 47 }, /* 52 Mb */
// 		/*[18] =*/ { RC_HT_S_20, WLAN_RC_PHY_HT_20_SS, 58500,
// 			53500, 6, 6, 8, 48, 18, 48 }, /* 58.5 Mb */
// 		/*[19] =*/ { RC_HT_S_20, WLAN_RC_PHY_HT_20_SS, 65000,
// 			59000, 7, 7, 8, 49, 20, 50 }, /* 65 Mb */
// 		/*[20] =*/ { RC_HT_S_20, WLAN_RC_PHY_HT_20_SS_HGI, 72200,//Half GI
// 			65400, 7, 7, 8, 49, 20, 50 }, /* 65 Mb*/
// // 		[21] = { RC_INVALID, WLAN_RC_PHY_HT_20_DS, 13000,
// // 			12700, 8, 8, 4, 51, 21, 51 }, /* 13 Mb */
// // 		[22] = { RC_HT_T_20, WLAN_RC_PHY_HT_20_DS, 26000,
// // 			24800, 9, 9, 6, 52, 22, 52 }, /* 26 Mb */
// // 		[23] = { RC_HT_T_20, WLAN_RC_PHY_HT_20_DS, 39000,
// // 			36600, 10, 10, 6, 53, 23, 53 }, /* 39 Mb */
// // 		[24] = { RC_HT_DT_20, WLAN_RC_PHY_HT_20_DS, 52000,
// // 			48100, 11, 11, 8, 54, 24, 54 }, /* 52 Mb */
// // 		[25] = { RC_HT_DT_20, WLAN_RC_PHY_HT_20_DS, 78000,
// // 			69500, 12, 12, 8, 55, 25, 55 }, /* 78 Mb */
// // 		[26] = { RC_HT_DT_20, WLAN_RC_PHY_HT_20_DS, 104000,
// // 			89500, 13, 13, 8, 56, 26, 56 }, /* 104 Mb */
// // 		[27] = { RC_HT_DT_20, WLAN_RC_PHY_HT_20_DS, 117000,
// // 			98900, 14, 14, 8, 57, 27, 57 }, /* 117 Mb */
// // 		[28] = { RC_HT_DT_20, WLAN_RC_PHY_HT_20_DS, 130000,
// // 			108300, 15, 15, 8, 58, 29, 59 }, /* 130 Mb */
// // 		[29] = { RC_HT_DT_20, WLAN_RC_PHY_HT_20_DS_HGI, 144400,
// // 			120000, 15, 15, 8, 58, 29, 59 }, /* 144.4 Mb */
// // 		[30] = {  RC_INVALID, WLAN_RC_PHY_HT_20_TS, 19500,
// // 			17400, 16, 16, 4, 60, 30, 60 }, /* 19.5 Mb */
// // 		[31] = {  RC_INVALID, WLAN_RC_PHY_HT_20_TS, 39000,
// // 			35100, 17, 17, 6, 61, 31, 61 }, /* 39 Mb */
// // 		[32] = {  RC_INVALID, WLAN_RC_PHY_HT_20_TS, 58500,
// // 			52600, 18, 18, 6, 62, 32, 62 }, /* 58.5 Mb */
// // 		[33] = {  RC_INVALID, WLAN_RC_PHY_HT_20_TS, 78000,
// // 			70400, 19, 19, 8, 63, 33, 63 }, /* 78 Mb */
// // 		[34] = {  RC_INVALID, WLAN_RC_PHY_HT_20_TS, 117000,
// // 			104900, 20, 20, 8, 64, 35, 65 }, /* 117 Mb */
// // 		[35] = {  RC_INVALID, WLAN_RC_PHY_HT_20_TS_HGI, 130000,
// // 			115800, 20, 20, 8, 64, 35, 65 }, /* 130 Mb */
// // 		[36] = {  RC_HT_T_20, WLAN_RC_PHY_HT_20_TS, 156000,
// // 			137200, 21, 21, 8, 66, 37, 67 }, /* 156 Mb */
// // 		[37] = {  RC_HT_T_20, WLAN_RC_PHY_HT_20_TS_HGI, 173300,
// // 			151100, 21, 21, 8, 66, 37, 67 }, /* 173.3 Mb */
// // 		[38] = {  RC_HT_T_20, WLAN_RC_PHY_HT_20_TS, 175500,
// // 			152800, 22, 22, 8, 68, 39, 69 }, /* 175.5 Mb */
// // 		[39] = {  RC_HT_T_20, WLAN_RC_PHY_HT_20_TS_HGI, 195000,
// // 			168400, 22, 22, 8, 68, 39, 69 }, /* 195 Mb */
// // 		[40] = {  RC_HT_T_20, WLAN_RC_PHY_HT_20_TS, 195000,
// // 			168400, 23, 23, 8, 70, 41, 71 }, /* 195 Mb */
// // 		[41] = {  RC_HT_T_20, WLAN_RC_PHY_HT_20_TS_HGI, 216700,
// // 			185000, 23, 23, 8, 70, 41, 71 }, /* 216.7 Mb */
// //		/*[42] =*/ { RC_HT_SDT_40, WLAN_RC_PHY_HT_40_SS, 13500,
// //			13200, 0, 0, 8, 42, 42, 42 }, /* 13.5 Mb */
// //		/*[43] =*/ { RC_HT_SDT_40, WLAN_RC_PHY_HT_40_SS, 27500,
// //			25900, 1, 1, 8, 43, 43, 43 }, /* 27.0 Mb */
// //		/*[44] =*/ { RC_HT_SDT_40, WLAN_RC_PHY_HT_40_SS, 40500,
// //			38600, 2, 2, 8, 44, 44, 44 }, /* 40.5 Mb */
// //		/*[45] =*/ { RC_HT_SD_40, WLAN_RC_PHY_HT_40_SS, 54000,
// //			49800, 3, 3, 8, 45, 45, 45 }, /* 54 Mb */
// //		/*[46] =*/ { RC_HT_SD_40, WLAN_RC_PHY_HT_40_SS, 81500,
// //			72200, 4, 4, 8, 46, 46, 46 }, /* 81 Mb */
// //		/*[47] =*/ { RC_HT_S_40 , WLAN_RC_PHY_HT_40_SS, 108000,
// //			92900, 5, 5, 8, 47, 47, 47 }, /* 108 Mb */
// //		/*[48] =*/ { RC_HT_S_40, WLAN_RC_PHY_HT_40_SS, 121500,
// //			102700, 6, 6, 8, 48, 48, 48 }, /* 121.5 Mb */
// //		/*[49] =*/ { RC_HT_S_40, WLAN_RC_PHY_HT_40_SS, 135000,
// //			112000, 7, 7, 8, 49, 50, 50 }, /* 135 Mb */
// //		/*[50] =*/ { RC_HT_S_40, WLAN_RC_PHY_HT_40_SS_HGI, 150000,
// //			122000, 7, 7, 8, 49, 50, 50 }, /* 150 Mb */
// // 		/*[51] =*/ { RC_INVALID, WLAN_RC_PHY_HT_40_DS, 27000,
// // 			25800, 8, 8, 8, 51, 51, 51 }, /* 27 Mb */
// // 		[52] = { RC_HT_T_40, WLAN_RC_PHY_HT_40_DS, 54000,
// // 			49800, 9, 9, 8, 52, 52, 52 }, /* 54 Mb */
// // 		[53] = { RC_HT_T_40, WLAN_RC_PHY_HT_40_DS, 81000,
// // 			71900, 10, 10, 8, 53, 53, 53 }, /* 81 Mb */
// // 		[54] = { RC_HT_DT_40, WLAN_RC_PHY_HT_40_DS, 108000,
// // 			92500, 11, 11, 8, 54, 54, 54 }, /* 108 Mb */
// // 		[55] = { RC_HT_DT_40, WLAN_RC_PHY_HT_40_DS, 162000,
// // 			130300, 12, 12, 8, 55, 55, 55 }, /* 162 Mb */
// // 		[56] = { RC_HT_DT_40, WLAN_RC_PHY_HT_40_DS, 216000,
// // 			162800, 13, 13, 8, 56, 56, 56 }, /* 216 Mb */
// // 		[57] = { RC_HT_DT_40, WLAN_RC_PHY_HT_40_DS, 243000,
// // 			178200, 14, 14, 8, 57, 57, 57 }, /* 243 Mb */
// // 		[58] = { RC_HT_DT_40, WLAN_RC_PHY_HT_40_DS, 270000,
// // 			192100, 15, 15, 8, 58, 59, 59 }, /* 270 Mb */
// // 		[59] = { RC_HT_DT_40, WLAN_RC_PHY_HT_40_DS_HGI, 300000,
// // 			207000, 15, 15, 8, 58, 59, 59 }, /* 300 Mb */
// // 		[60] = {  RC_INVALID, WLAN_RC_PHY_HT_40_TS, 40500,
// // 			36100, 16, 16, 8, 60, 60, 60 }, /* 40.5 Mb */
// // 		[61] = {  RC_INVALID, WLAN_RC_PHY_HT_40_TS, 81000,
// // 			72900, 17, 17, 8, 61, 61, 61 }, /* 81 Mb */
// // 		[62] = {  RC_INVALID, WLAN_RC_PHY_HT_40_TS, 121500,
// // 			108300, 18, 18, 8, 62, 62, 62 }, /* 121.5 Mb */
// // 		[63] = {  RC_INVALID, WLAN_RC_PHY_HT_40_TS, 162000,
// // 			142000, 19, 19, 8, 63, 63, 63 }, /* 162 Mb */
// // 		[64] = {  RC_INVALID, WLAN_RC_PHY_HT_40_TS, 243000,
// // 			205100, 20, 20, 8, 64, 65, 65 }, /* 243 Mb */
// // 		[65] = {  RC_INVALID, WLAN_RC_PHY_HT_40_TS_HGI, 270000,
// // 			224700, 20, 20, 8, 64, 65, 65 }, /* 270 Mb */
// // 		[66] = {  RC_HT_T_40, WLAN_RC_PHY_HT_40_TS, 324000,
// // 			263100, 21, 21, 8, 66, 67, 67 }, /* 324 Mb */
// // 		[67] = {  RC_HT_T_40, WLAN_RC_PHY_HT_40_TS_HGI, 360000,
// // 			288000, 21, 21, 8, 66, 67, 67 }, /* 360 Mb */
// // 		[68] = {  RC_HT_T_40, WLAN_RC_PHY_HT_40_TS, 364500,
// // 			290700, 22, 22, 8, 68, 69, 69 }, /* 364.5 Mb */
// // 		[69] = {  RC_HT_T_40, WLAN_RC_PHY_HT_40_TS_HGI, 405000,
// // 			317200, 22, 22, 8, 68, 69, 69 }, /* 405 Mb */
// // 		[70] = {  RC_HT_T_40, WLAN_RC_PHY_HT_40_TS, 405000,
// // 			317200, 23, 23, 8, 70, 71, 71 }, /* 405 Mb */
// // 		[71] = {  RC_HT_T_40, WLAN_RC_PHY_HT_40_TS_HGI, 450000,
// // 			346400, 23, 23, 8, 70, 71, 71 }, /* 450 Mb */
// 	},
// 	50,  /* probe interval */
// 	WLAN_RC_HT_FLAG,  /* Phy rates allowed initially */
// };
//
//
//
//
// static const struct ath_rate_table ssv_11g_ratetable = {
// 	12,
// 	0,
// 	{
// 		{ RC_L_SDT, WLAN_RC_PHY_CCK, 1000, /* 1 Mb */
// 		900, 0, 2, 0},
// 		{ RC_L_SDT, WLAN_RC_PHY_CCK, 2000, /* 2 Mb */
// 		1900, 1, 4, 1},
// 		{ RC_L_SDT, WLAN_RC_PHY_CCK, 5500, /* 5.5 Mb */
// 		4900, 2, 11, 2},
// 		{ RC_L_SDT, WLAN_RC_PHY_CCK, 11000, /* 11 Mb */
// 		8100, 3, 22, 3},
// 		{ RC_INVALID, WLAN_RC_PHY_OFDM, 6000, /* 6 Mb */
// 		5400, 4, 12, 4},
// 		{ RC_INVALID, WLAN_RC_PHY_OFDM, 9000, /* 9 Mb */
// 		7800, 5, 18, 4},
// 		{ RC_L_SDT, WLAN_RC_PHY_OFDM, 12000, /* 12 Mb */
// 		10000, 6, 24, 6},
// 		{ RC_L_SDT, WLAN_RC_PHY_OFDM, 18000, /* 18 Mb */
// 		13900, 7, 36, 6},
// 		{ RC_L_SDT, WLAN_RC_PHY_OFDM, 24000, /* 24 Mb */
// 		17300, 8, 48, 8},
// 		{ RC_L_SDT, WLAN_RC_PHY_OFDM, 36000, /* 36 Mb */
// 		23000, 9, 72, 8},
// 		{ RC_L_SDT, WLAN_RC_PHY_OFDM, 48000, /* 48 Mb */
// 		27400, 10, 96, 8},
// 		{ RC_L_SDT, WLAN_RC_PHY_OFDM, 54000, /* 54 Mb */
// 		29300, 11, 108, 8},
// 	},
// 	50,  /* probe interval */
// 	0,   /* Phy rates allowed initially */
// };
