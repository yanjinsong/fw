/*
 * Copyright 2002-2005, Instant802 Networks, Inc.
 * Copyright 2005, Devicescape Software, Inc.
 * Copyright 2007, Mattias Nissler <mattias.nissler@gmx.de>
 * Copyright 2007-2008, Stefano Brivio <stefano.brivio@polimi.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "pide.h"
#include "sta_info.h"
#include <hw_regs_api.h>
#include "ssv_pktdef.h"
#include <dbg_timer/dbg_timer.h>
#include "hdr80211.h"
#include <driver/ampdu/drv_ampdu.h>
#include <pbuf.h>
#if(PACKET_TRACE==1)
#include <mbox/drv_mbox.h>
#endif

//static OsTimer pid_timer;
//static struct ssv62xx_supported_band ssv6200_band;
static u8 pideFixRateEnable=0,pideFixRate=0;
static u8 pideJumpRateEnable=0, pideJumpRateIdx=0;
static u8 pideJumpRate[]={14,13};
static u8 pideFixPreambleEnable=0,pideFixPreamble=0;
static u8 pideFixNetworkEnable=0,pideFixNetwork=0;
static u8 rcdbg = 0;
//Upgrade rate PER and downgrade rate PER in simple algorithm
static u8 rc_up_pf = RC_DEFAULT_UP_PF;
static u8 rc_down_pf = RC_DEFAULT_DOWN_PF;
// For fastest b mode rate to do probe up at specific PER
static u8 rc_up_pf_fastest_b = RC_DEFAULT_UP_PF - 3;
//For sending frame in greenfield
static u8 gf_enable;
//For STAs to identify preamble type
static u8 long_preamble_idx = 0;
static bool erp_protect = 0;
static u16 defaultpideRateMsk = 0x0fff;
bool force_up_down =FALSE;
static u32 rcreportIntval = 10000;
static u32 rcsampleIntval = 10000;
//Features related variable and macros
static u32 rcFeature = 0;
//Pre-allocated frame for probing rate
PKT_TxInfo *probe_pkt[MAXPROBES];
static u8 prb_pkt_st = 0;
static bool prb_pkt_inited = FALSE;
u8 g_prb_pkt_enable = 0;
//u16 latest_wsid0_seq;
u8 g_force_rtscts = 0;
u8 g_drate_endian=0;

#define RC_IS_FEATURED(x) (rcFeature&BIT(x))
#define RC_FEATURE(x) (rcFeature|=BIT(x))
#define RC_NOT_FEATURE(x) (rcFeature&=(~BIT(x)))
//Minimum and maximum of moving continued probe up window
#define RC_MOVINGCPU_MIN 32
#define RC_MOVINGCPU_MAX 128
#define RC_PRB_PKT_SIZE 1536
#define RC_PROBED_TIMEOUT 250000
#define RC_DFT_AGED 8
#define RC_DFT_NEW_AGED_RATIO 1
#define RC_DFT_OLD_AGED_RATIO (RC_DFT_AGED - RC_DFT_NEW_AGED_RATIO)

//Customized
#define SET_PHY_IDX_WSID0(_VAL_) ((REG32(ADR_DBG_SPI_TO_PHY_PARAM2)) = (((_VAL_) << 0) | ((REG32(ADR_DBG_SPI_TO_PHY_PARAM2)) & 0xffff0000)))
#define RESET_ALLCNT_WSID0 ((REG32(ADR_DBG_SPI_TO_PHY_PARAM1)) = 0x00000000)
#define UPDATE_ALLCNT_WSID0(_TX_ ,_RETRY_) ((REG32(ADR_DBG_SPI_TO_PHY_PARAM1)) = ((((REG32(ADR_DBG_SPI_TO_PHY_PARAM1)) & 0x0000ffff ) + _TX_) & 0x0000ffff) | (((((REG32(ADR_DBG_SPI_TO_PHY_PARAM1)) & 0xffff0000 ) >> 16) + _RETRY_) << 16))

//Hardware rate table, mapping to hardware rate index
struct phy_rate_info ssv6200_phy_rate[PHY_SUPPORT_RATE_NUMBER] = {
	{2,		WLAN_RC_PHY_CCK		,PHY_MODE_LONG_PREAMBLE	, 0x0,  10},
	{4,		WLAN_RC_PHY_CCK		,PHY_MODE_LONG_PREAMBLE	, 0x1,  20},
	{11,	WLAN_RC_PHY_CCK		,PHY_MODE_LONG_PREAMBLE	, 0x1,  55},
	{22,	WLAN_RC_PHY_CCK		,PHY_MODE_LONG_PREAMBLE	, 0x1,  110},
	{4,		WLAN_RC_PHY_CCK		,PHY_MODE_SHORT_PREAMBLE, 0x4,  20},
	{11,	WLAN_RC_PHY_CCK 	,PHY_MODE_SHORT_PREAMBLE, 0x4,  55},
	{22,	WLAN_RC_PHY_CCK		,PHY_MODE_SHORT_PREAMBLE, 0x4,  110},
	{12,	WLAN_RC_PHY_OFDM	,0						, 0x7,  60},//G-7
	{18,	WLAN_RC_PHY_OFDM	,0						, 0x7,  90},
	{24,	WLAN_RC_PHY_OFDM	,0						, 0x9,  120},
	{36,	WLAN_RC_PHY_OFDM	,0						, 0x9,  180},
	{48,	WLAN_RC_PHY_OFDM	,0						, 0xb,  240},
	{72,	WLAN_RC_PHY_OFDM	,0						, 0xb,  360},
	{96,	WLAN_RC_PHY_OFDM	,0						, 0xb,  480},
	{108,	WLAN_RC_PHY_OFDM	,0						, 0xb,  540},
	{0,		WLAN_RC_PHY_HT_MF	,PHY_MODE_LONG_GI		, 0x7,  65},//N-LONG-15
	{1,		WLAN_RC_PHY_HT_MF	,PHY_MODE_LONG_GI		, 0x9,  130},
	{2,		WLAN_RC_PHY_HT_MF	,PHY_MODE_LONG_GI		, 0x9,  195},
	{3,		WLAN_RC_PHY_HT_MF	,PHY_MODE_LONG_GI		, 0xb,  260},
	{4,		WLAN_RC_PHY_HT_MF	,PHY_MODE_LONG_GI		, 0xb,  390},
	{5,		WLAN_RC_PHY_HT_MF	,PHY_MODE_LONG_GI		, 0xb,  520},
	{6,		WLAN_RC_PHY_HT_MF	,PHY_MODE_LONG_GI		, 0xb,  585},
	{7,		WLAN_RC_PHY_HT_MF	,PHY_MODE_LONG_GI		, 0xb,  650},
	{0,		WLAN_RC_PHY_HT_MF	,PHY_MODE_SHORT_GI		, 0x7,  72},//N-SHORT-23
	{1,		WLAN_RC_PHY_HT_MF	,PHY_MODE_SHORT_GI		, 0x9,  144},
	{2,		WLAN_RC_PHY_HT_MF	,PHY_MODE_SHORT_GI		, 0x9,  217},
	{3,		WLAN_RC_PHY_HT_MF	,PHY_MODE_SHORT_GI		, 0xb,  289},
	{4,		WLAN_RC_PHY_HT_MF	,PHY_MODE_SHORT_GI		, 0xb,  433},
	{5,		WLAN_RC_PHY_HT_MF	,PHY_MODE_SHORT_GI		, 0xb,  578},
	{6,		WLAN_RC_PHY_HT_MF	,PHY_MODE_SHORT_GI		, 0xb,  650},
	{7,		WLAN_RC_PHY_HT_MF	,PHY_MODE_SHORT_GI		, 0xb,  722},
	{0,		WLAN_RC_PHY_HT_GF	,PHY_MODE_LONG_GI		, 0x7,  65},//Greenfield-31
	{1,		WLAN_RC_PHY_HT_GF	,PHY_MODE_LONG_GI		, 0x9,  130},
	{2,		WLAN_RC_PHY_HT_GF	,PHY_MODE_LONG_GI		, 0x9,  195},
	{3,		WLAN_RC_PHY_HT_GF	,PHY_MODE_LONG_GI		, 0xb,  260},
	{4,		WLAN_RC_PHY_HT_GF	,PHY_MODE_LONG_GI		, 0xb,  390},
	{5,		WLAN_RC_PHY_HT_GF	,PHY_MODE_LONG_GI		, 0xb,  520},
	{6,		WLAN_RC_PHY_HT_GF	,PHY_MODE_LONG_GI		, 0xb,  585},
	{7,		WLAN_RC_PHY_HT_GF	,PHY_MODE_LONG_GI		, 0xb,  650}
};


/* SSV6200 hardware rate */
#define RATE(_init_hw_rate, _flags) {	\
	(_flags),		\
	(_init_hw_rate)	\
}

static struct ssv62xx_rate ssv6200_rate[SSV6200_SUPPORT_RATE_NUMBER] = {
	RATE(0x0, IEEE80211_NO_TYPE),
	RATE(0x1, IEEE80211_PREAMBLE_MODE),
	RATE(0x2, IEEE80211_PREAMBLE_MODE),
	RATE(0x3, IEEE80211_PREAMBLE_MODE),
	RATE(0x7, IEEE80211_NETWORK_MODE),
	RATE(0x8, IEEE80211_NETWORK_MODE),
	RATE(0x9, IEEE80211_NETWORK_MODE),
	RATE(0xa, IEEE80211_NETWORK_MODE),
	RATE(0xb, IEEE80211_NETWORK_MODE),
	RATE(0xc, IEEE80211_NETWORK_MODE),
	RATE(0xd, IEEE80211_NETWORK_MODE),
	RATE(0xe, IEEE80211_NETWORK_MODE)
};

#if (N_MODE_AMPDU_TX == 1)
extern u8 AMPDU_TX_ENABLE;
#endif

// count number of rates from rate status
static u16 ratebit_count(u16 supp_rate)
{
   u16 count = 0 ;
   while (supp_rate != 0)
   {
      count++ ;
      supp_rate &= (supp_rate - 1);
   }

   return count ;
}

/* @rate: rateinfo of current rate
 * @spinfo: station info
 * @new_pf: new pf
 * Calculate aged PER and throughput of current rate
 */
inline void _calculate_tp(struct rc_pid_rateinfo *rate, struct rc_pid_sta_info *spinfo, u8 new_pf)
{
    unsigned int perfect_time = 0;
    u8 ori_aged_ratio = RC_DFT_OLD_AGED_RATIO;
    u8 new_aged_ratio = RC_DFT_NEW_AGED_RATIO;

    perfect_time = rate->perfect_tx_time;
    if (!perfect_time)
        perfect_time = 1000000;

    //Aged PER for ease burst error
    spinfo->agedpf = ((spinfo->agedpf*ori_aged_ratio) + (new_pf*new_aged_ratio)) >> 3;
    //TP = (100 -PER)* 1000000us/perfect time, perfect time =  sending time of a packet which its length is 1530 byte
    rate->throughput = (100 - spinfo->agedpf)*(1000000 / perfect_time);
}


/* By copy data in actual frame, to probe frames
 * The copied data includes TxInfo, 802.11 header, and LLC header.
 * With the copy action, a lot preparation of probe frames can be avoid
 * @pTxInfo: The data packet to copy to pre-allocated probe frame
*/
static void _update_prb_pkt(PKT_TxInfo *pTxInfo)
{
    u8 i;
    u8 cpy_len = (pTxInfo->payload_offset + 8); //Copy TXInfo, 802.11 header, and  LLC header
    u32 seqno = GET_HDR80211_SC_SEQNUM(pTxInfo);

    if (pTxInfo->len < cpy_len)
        return ;

    if (seqno > 2045)
        seqno = 2045;

    // Using different sequence numbers for these probe frames
    for(i = 0 ; i < 3; i++)
    {
        memcpy((void *)probe_pkt[i], (void *)pTxInfo, cpy_len);
        probe_pkt[i]->len = (RC_PRB_PKT_SIZE - pTxInfo->hdr_offset);
        SET_HDR80211_SC_SEQNUM(probe_pkt[i], seqno);
        seqno++;
        printf("[RC]: Update probe frame %d for %08x, seq = %d\n", i, probe_pkt[i], seqno);
    }
    prb_pkt_inited = TRUE;
}

/* Allocate or free the pre-allocated probe frame
 * @IsAllocate: True for allocating probe frames, false for freeing probe frame
 */
static void _process_prb_pkt(bool IsAllocate)
{
    u8 i, process_cnt = 0;

    for(i = 0 ; i < MAXPROBES; i++)
    {
        if(IsAllocate == TRUE)
        {
            PKT_TxInfo *prb_pkt = NULL;
            prb_pkt = (PKT_TxInfo *)PBUF_MAlloc_Raw(RC_PRB_PKT_SIZE, 0 , NOTYPE_BUF);
            if (prb_pkt == NULL)
            {
                printf("[RC]: Failed to pre-allocate probe frame\n");
                break;
            }
            if (probe_pkt[i] != NULL)
            {
                printf("[RC]: pre-allocate probe frames are not NULL :%08x\n", probe_pkt[i]);
                PBUF_MFree(probe_pkt[i]);
                probe_pkt[i] = NULL;
            }
            probe_pkt[i] = prb_pkt;
            prb_pkt_st |= BIT(i);
            process_cnt++;
            //prepare pattern in probe packet here.
            //probe_pkt[i]->RSVD_3 = i;
        }
        else
        {
            if (probe_pkt[i] != NULL)
            {
                PBUF_MFree(probe_pkt[i]);
                probe_pkt[i] = NULL;
                prb_pkt_st &= ~BIT(i);
                process_cnt++;
            }
        }
    }

    prb_pkt_inited = FALSE;
    if(process_cnt != MAXPROBES)
        printf("[RC]: Probe frame less!! %d for %s\n", process_cnt, (IsAllocate == TRUE)?"allocate":"release");
}

/* Check the rate if the STA supports
 * @sta: Target sta
 * @index: rate index
 */
static u32 ssv62xx_rate_supported(SSV6XXX_STA *sta, u32 index)
{
	return (sta->supp_rates & BIT(index));
}
/* Check the rate if the STA available
 * curr_rate = supp_rates & defaultpideRateMsk
 * dftratemsk can control the available rate of each sta
 * @sta: Target sta
 * @index: rate index
 */
static u32 ssv62xx_rate_available(SSV6XXX_STA *sta, u32 index)
{
	return (sta->curr_rates & BIT(index));
}

/* Get lowest available rates
 * @sta: Target sta
 */
static u8 ssv62xx_rate_lowest_index(SSV6XXX_STA *sta)
{
	int i;

	for (i = 0; i < SSV6200_SUPPORT_RATE_NUMBER; i++)
		if (ssv62xx_rate_available(sta, i))
			return i;

	return 0;
}

/* Get highest available rates
 * @sta: Target sta
 */
static u8 ssv62xx_rate_highest_index(SSV6XXX_STA *sta)
{
	int i;

	for (i = SSV6200_SUPPORT_RATE_NUMBER-1; i >= 0; i--)
		if (ssv62xx_rate_available(sta, i))
			return i;

	return 0;
}

#if (N_MODE_AMPDU_TX == 1)
/* Get highest available rates according to wifi modulation
 * @sta: Target sta
 * @wifi_mode: Specific modulation for the highest rate
 */
static u8 ssv62xx_rate_highest_index_by_mode(SSV6XXX_STA *sta, enum WLAN_RC_PHY_TYPE wifi_mode)
{
	int i = SSV6200_SUPPORT_RATE_NUMBER-1;

    if (wifi_mode == WLAN_RC_PHY_CCK)
        i = SSV6200_SUPPORT_BMODE_RATE_NUMBER - 1;
	for (; i >= 0; i--)
    {
		if ((ssv62xx_rate_available(sta, i)) && (ssv6200_phy_rate[sta->hwratetbl[i]].type == wifi_mode))
			return i;
    }

	return 0;
}

#endif


/* setting the support rates and the rate mode for the sta
 * @sta: Target sta
 */
static void _decide_networkMode(SSV6XXX_STA *sta)
{
    if (sta->mcs_0 != 0)
    {
        u16 nrates = (u16)sta->mcs_0;
        sta->networkMode = PHY_MODE_LONG_GI;

        if ((sta->ht_capa_b6b4 & 0x1) && (gf_enable != 0))
            sta->networkMode = PHY_MODE_GREENFIELD;

        if(sta->networkMode != PHY_MODE_G_ONLY)
            sta->supp_rates = (sta->non_ht_rates | nrates<<4);
    }

    if (sta->networkMode == PHY_MODE_G_ONLY)
        sta->supp_rates = sta->non_ht_rates;

    if(rcdbg & DBG_LV_GENERAL)
        printf("sta->networkMode = %d\n", sta->networkMode);

    if ((gf_enable != 0) && (sta->networkMode != PHY_MODE_GREENFIELD))
        printf("[RC]: Warning!! Greenfield is on but sta is not the same mode\n");
}

#if (N_MODE_AMPDU_TX == 1)
#if 0
u8 nrate_mod_type[] =
{
    RC_QAM_BPSK,
    RC_QAM_QPSK,
    RC_QAM_QPSK,
    RC_QAM_16,
    RC_QAM_16,
    RC_QAM_64,
    RC_QAM_64,
    RC_QAM_64
};

/* _multi_rate_shift: degrade the rate by shift to get different rate
 *
 */
static void _multi_rate_remap(SSV6XXX_STA *sta)
{
    s8 cur_mod, i, j, nrate, target_rate, rate_not_found, diff_mod;
    u8 biggest_next_rate;
    struct rc_pid_sta_info *spinfo = &sta->spinfo;

    if (ssv6200_phy_rate[sta->hwratetbl[spinfo->txrate_idx]].type < WLAN_RC_PHY_HT_MF)
        return ;

    nrate = spinfo->txrate_idx - 4;
    if (spinfo->txrate_idx < 7 )
        diff_mod = 0;
    else
        diff_mod = 1;

    if((sta->pinfo.rate_sum > 1) && (pideFixRateEnable ==0))
    {
        for(i = RC_MULTI_RATE_TRY_CNT, j = 0; (j<3);j++)
        {
            spinfo->multi_rate[j].drate_idx = sta->hwratetbl[(nrate + 4)];
            if ((i < AMPDU_FW_MAX_RETRY) && (j<2))
                spinfo->multi_rate[j].accu_try_limit = i;
            else
            {
                spinfo->multi_rate[j].accu_try_limit = AMPDU_FW_MAX_RETRY;
                break;
            }

            cur_mod = nrate_mod_type[nrate];
            rate_not_found = 1;
            biggest_next_rate = 0;
            for(target_rate = nrate;(target_rate >= 0);target_rate--)
            {
                //biggest_next_rate = target_rate;
                if ((ssv62xx_rate_available(sta, (target_rate + 4)) != 0) && (target_rate !=  nrate) && (biggest_next_rate < target_rate))
                    biggest_next_rate = target_rate;

                if (diff_mod == 1)
                {
                    if (cur_mod != nrate_mod_type[target_rate])
                    {
                        nrate = target_rate;
                        rate_not_found = 0;
                        break;
                    }
                }
                else
                {
                    if(nrate != biggest_next_rate)
                    {
                        nrate = biggest_next_rate;
                        rate_not_found = 0;
                        break;
                    }
                }
            }
            if (rate_not_found == 1)
                nrate = biggest_next_rate;
            i+= RC_MULTI_RATE_TRY_CNT;
        }
    }
    else
    {
        memset(spinfo->multi_rate, 0, 3*sizeof(struct rates_param));
        spinfo->multi_rate[0].drate_idx = sta->hwratetbl[spinfo->txrate_idx];
        spinfo->multi_rate[0].accu_try_limit = AMPDU_FW_MAX_RETRY;
    }

    printf("[RC]: Multi rate: %d/%d, %d/%d, %d/%d\n",
        spinfo->multi_rate[0].drate_idx, spinfo->multi_rate[0].accu_try_limit,
        spinfo->multi_rate[1].drate_idx, spinfo->multi_rate[1].accu_try_limit,
        spinfo->multi_rate[2].drate_idx, spinfo->multi_rate[2].accu_try_limit);
}
#endif //#if 0

extern u8 AMPDU_TX_MaxRetry;
extern u8 AMPDU_TX_Maintry;
/* Generate multi rate table
 * @sta: Target sta
 * @IsProbeRate : is @rate_idx probe rate
 * @rate_idx: star rate of the multi rate table
 */
static void _multi_rate_remap(SSV6XXX_STA *sta, u8 rate_idx, bool IsProbeRate)
{
    s8 i, j, nrate, mr_idx = 0, main_try_cnt, avg_try_cnt;
    u16 retail_nrate_sum = 0;
    struct rc_pid_sta_info *spinfo = &sta->spinfo;
    struct mr_params *mrate_param;

    // avoid non n mode rate
    if (ssv6200_phy_rate[sta->hwratetbl[rate_idx]].type < WLAN_RC_PHY_HT_MF)
        return ;
    // avoid non n mode rate from fix rate command
    if ((pideFixRateEnable ==1) && (pideFixRate < 15))
    {
        printf("[RC]: buggy fixrate setting for multi-rate\n");
        return ;
    }

    if (IsProbeRate != TRUE)
        mrate_param = &spinfo->mr_cur_trycnt;
    else
        mrate_param = &spinfo->mr_prb_trycnt;

    memset((void *)mrate_param, 0, MR_PARAM_LEN);

    //Get current rate idx in n mode
    nrate = rate_idx - 4;
    // How many n mode rates are available except current rate
    retail_nrate_sum = ratebit_count(sta->curr_rates&(0x0ff0 & ((1<<rate_idx) -1)));

    // For the other n mode rates available case
    if((retail_nrate_sum > 0) && (pideFixRateEnable == 0))
    {
        u8 last_try_cnt = 0;
        // target rate has 3 try time, then separate retail times on retail n mode rate
        main_try_cnt = AMPDU_TX_Maintry;//3;
        avg_try_cnt = (AMPDU_TX_MaxRetry - main_try_cnt)/retail_nrate_sum;

        //Avoid to separate try times on last n mode rate
        if (avg_try_cnt == 0)
            avg_try_cnt = 1;

        for(i= nrate,j=0; ((i >= 0) && (j<= retail_nrate_sum)); i--)
        {
            mr_idx = 7 - i;
            if(sta->curr_rates & BIT((i+4)))
            {
                // The try_cnt for each rate is cumulative try count
                // The default try count table when current rate is MCS7 would be (3,4,5,6,7,7,7,7)
                if(last_try_cnt < AMPDU_TX_MaxRetry)
                    last_try_cnt = (i==nrate)?main_try_cnt:(main_try_cnt+ j*avg_try_cnt);
                if (j != retail_nrate_sum)
                    mrate_param->mr_tbl[mr_idx] = last_try_cnt;
                else
                    mrate_param->mr_tbl[mr_idx] = AMPDU_TX_MaxRetry;
                j++;
            }
            else
                mrate_param->mr_tbl[mr_idx] = last_try_cnt;
        }
        mrate_param->mr_retry_max = AMPDU_TX_MaxRetry;
    }
    else
    {
        //just one n mode rate in multi-rate
        u16 nrate_sum = ratebit_count((sta->curr_rates&0x0ff0));
        if (pideFixRateEnable == 0)
        {
            mr_idx = 7 - nrate;
            retail_nrate_sum = 0;
        }
        else
        {
            mr_idx = pideFixRate-15;
            while(mr_idx > 7)
                mr_idx -= 8;
            mr_idx = 7-mr_idx;
        }

        //Shorten the retry count for n mode AMPDU when lowest n mode rate available
        if ((pideFixRateEnable != 1) && (nrate_sum > 1) && (g_prb_pkt_enable == 0))
            mrate_param->mr_tbl[mr_idx] = AMPDU_FW_MAX_LESS_RETRY;
        else
            mrate_param->mr_tbl[mr_idx] = AMPDU_TX_MaxRetry;
        mrate_param->mr_retry_max = mrate_param->mr_tbl[mr_idx];
    }

    mrate_param->mr_rate_num = (retail_nrate_sum+1);
    if (rcdbg & DBG_LV_GENERAL)
    {
        printf("[RC]: Multi rate tbl generating for %s rate: \n", (IsProbeRate == FALSE)?"current":"probing");
        for(i = nrate; i >= 0; i --)
            printf("MCS%d: %d \n", i, mrate_param->mr_tbl[(7-i)]);
        printf("rate num: %d \nretry max: %d \n", mrate_param->mr_rate_num, mrate_param->mr_retry_max);
    }
}

#endif
/* Return string of rate according to hardware rate index
 * @rateIndex: hardware rate index
 */
char *rateControlGetRate(u8 rateIndex)
{
	switch(rateIndex)
	{
        case 0:
			return "1Mbps";
        case 1:
		case 4:
			return "2Mbps";
		case 2:
		case 5:
			return "5.5Mbps";
		case 3:
		case 6:
			return "11Mbps";
		case 7:
			return "6Mbps";
		case 8:
			return "9Mbps";
		case 9:
			return "12Mbps";;
		case 10:
			return "18Mbps";;
		case 11:
			return "24Mbps";
		case 12:
			return "36Mbps";;
		case 13:
			return "48Mbps";;
		case 14:
			return "54Mbps";
		case 15:
		case 31:
			return "6.5Mbps";
		case 16:
		case 32:
			return "13Mbps";
		case 17:
		case 33:
			return "19.5Mbps";
		case 18:
		case 34:
			return "26Mbps";
		case 19:
		case 35:
			return "39Mbps";
		case 20:
		case 36:
			return "52Mbps";
		case 21:
		case 37:
			return "58.5Mbps";
		case 22:
		case 38:
			return "65Mbps";
		case 23:
			return "7.2Mbps";
		case 24:
			return "14.4Mbps";
		case 25:
			return "21.7Mbps";
		case 26:
			return "28.9Mbps";
		case 27:
			return "43.3Mbps";
		case 28:
			return "57.8Mbps";
		case 29:
			return "65Mbps";
		case 30:
			return "72.2Mbps";
		default:
			return "0Mbps";
    };
}

#ifdef PROBE
/* This is an implementation of a TX rate control algorithm that uses a PID
 * controller. Given a target failed frames rate, the controller decides about
 * TX rate changes to meet the target failed frames rate.
 *
 * The controller basically computes the following:
 *
 * adj = CP * err + CI * err_avg + CD * (err - last_err) * (1 + sharpening)
 *
 * where
 * 	adj	adjustment value that is used to switch TX rate (see below)
 * 	err	current error: target vs. current failed frames percentage
 * 	last_err	last error
 * 	err_avg	average (i.e. poor man's integral) of recent errors
 *	sharpening	non-zero when fast response is needed (i.e. right after
 *			association or no frames sent for a long time), heading
 * 			to zero over time
 * 	CP	Proportional coefficient
 * 	CI	Integral coefficient
 * 	CD	Derivative coefficient
 *
 * CP, CI, CD are subject to careful tuning.
 *
 * The integral component uses a exponential moving average approach instead of
 * an actual sliding window. The advantage is that we don't need to keep an
 * array of the last N error values and computation is easier.
 *
 * Once we have the adj value, we map it to a rate by means of a learning
 * algorithm. This algorithm keeps the state of the percentual failed frames
 * difference between rates. The behaviour of the lowest available rate is kept
 * as a reference value, and every time we switch between two rates, we compute
 * the difference between the failed frames each rate exhibited. By doing so,
 * we compare behaviours which different rates exhibited in adjacent timeslices,
 * thus the comparison is minimally affected by external conditions. This
 * difference gets propagated to the whole set of measurements, so that the
 * reference is always the same. Periodically, we normalize this set so that
 * recent events weigh the most. By comparing the adj value with this set, we
 * avoid pejorative switches to lower rates and allow for switches to higher
 * rates if they behaved well.
 *
 * Note that for the computations we use a fixed-point representation to avoid
 * floating point arithmetic. Hence, all values are shifted left by
 * RC_PID_ARITH_SHIFT.
 */

/* Calculate perfect tx time
 * @len: The length of transferred data
 * @rate: speed in hkbps
 * @short_preamble: preamble for b mode rate
 * @flags: the modulation of the rate
 */
static int pide_frame_duration(size_t len,
                 int rate, int short_preamble, int flags)
{
	int dur=0;

	/* calculate duration (in microseconds, rounded up to next higher
	 * integer if it includes a fractional microsecond) to send frame of
	 * len bytes (does not include FCS) at the given rate. Duration will
	 * also include SIFS.
	 *
	 * rate is in 100 kbps, so divident is multiplied by 10 in the
	 * DIV_ROUND_UP() operations.
	 */
    if (flags == WLAN_RC_PHY_CCK)
    {
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
		dur = 10; /* aSIFSTime = 10 usec */
		dur += (short_preamble == 1) ? (72 + 24) : (144 + 48);

		dur += DIV_ROUND_UP(8 * (len + 4) * 10, rate);
	}
    else
    {
		/*
		 * OFDM:
		 *
		 * N_DBPS = DATARATE x 4
		 * N_SYM = Ceiling((16+8xLENGTH+6) / N_DBPS)
		 *	(16 = SIGNAL time, 6 = tail bits)
		 * TXTIME = T_PREAMBLE + T_SIGNAL + T_SYM x N_SYM + Signal Ext
		 *
		 * T_SYM = 4 usec
		 * 802.11a - 17.5.2: aSIFSTime = 16 usec
		 * 802.11g - 19.8.4: aSIFSTime = 10 usec +
		 *	signal ext = 6 usec
		 */
		dur = 16; /* SIFS + signal ext */
		dur += 16; /* 17.3.2.3: T_PREAMBLE = 16 usec */
		dur += 4; /* 17.3.2.3: T_SIGNAL = 4 usec */
		dur += 4 * DIV_ROUND_UP((16 + 8 * (len + 4) + 6) * 10,
					4 * rate); /* T_SYM x N_SYM */
	}

	return dur;
}
#endif

extern SSV6XXX_STA	*g6200StaInfo;
/* Calculate b mode tx time due to one long preamble STA connected, it is need
 * to use long preamble with other connected STA
 * @IsInverse=true is designed to update rest connected STAs if one long preamble
 * only STA is connecting for AP mode
 * By @IsInverse, you can select to process specific sta or not to process specific sta
 * The scenario is of one long preamble sta connects/disconnects, how to update
 * other sta to use long preamble. The connecting STA is set as long preamble at
 * ssv62xx_rateinfo_init stage, use long_preamble_idx to record the index of long preamble STA.
 */
inline void _calculate_bmode_txtime(u8 wsid, bool IsInverse)
{
    u8 i = 0, j;
    SSV6XXX_STA *sta;
    struct rc_pid_rateinfo *rinfo;
    int preamble = 0;

    for (;i < WLAN_MAX_STA;i++)
    {
        sta = &g6200StaInfo[i];
        if ((sta->valid == TRUE) && ( ((i != wsid) && (IsInverse == true)) || ((i == wsid) && (IsInverse == false))))
        {
            /* Recover and update all STAs to use short preamble once no connected sta uses long preamble*/
            if (IsInverse == true)
                sta->preamble = (long_preamble_idx == 0)?PHY_MODE_SHORT_PREAMBLE:PHY_MODE_LONG_PREAMBLE;

            rinfo = sta->pinfo.rinfo;

            for (j = 1; ((j < SSV6200_SUPPORT_BMODE_RATE_NUMBER)&&(ssv6200_rate[j].flags == IEEE80211_PREAMBLE_MODE)); j++)
            {
                //avoid 1M to use short preamble. Use short preamble to calculate most rates.
                preamble = (sta->hwratetbl[i] == 0)?0:1;
                sta->hwratetbl[i] = ssv6200_rate[j].init_hw_value + (HW_PREABBLE_OFFSET * sta->preamble);
                rinfo[j].perfect_tx_time = TDIFS + (TSLOT * 15 >> 1) + pide_frame_duration (1530,
                    ssv6200_phy_rate[sta->hwratetbl[i]].hkbps, preamble, ssv6200_phy_rate[sta->hwratetbl[i]].type) +
                    pide_frame_duration (10, ssv6200_phy_rate[sta->hwratetbl[i]].hkbps, preamble, ssv6200_phy_rate[sta->hwratetbl[i]].type);
                if (rcdbg & DBG_LV_GENERAL)
                    printf ("STA %d bmode rate %d pt = %d\n", i, j,rinfo[j].perfect_tx_time);
            }
            if ((i == wsid) && (IsInverse == false))
                break;
        }
    }
}

/* Generate hardware rate table for sta by rate index
 * @sta: target sta
 */
static void ssv62xx_rateinfo_init(SSV6XXX_STA *sta)
{
    u16 i;
    struct rc_pid_rateinfo *rinfo = sta->pinfo.rinfo;
    int preamble = 0;

	/* Sort the rates. This is optimized for the most common case (i.e.
	 * almost-sorted CCK+OFDM rates). Kind of bubble-sort with reversed
	 * mapping too. */
	for (i = 0; i < SSV6200_SUPPORT_RATE_NUMBER; i++)
    {
		rinfo[i].index = i;
		rinfo[i].rev_index = i;

		switch(ssv6200_rate[i].flags)
        {
            case IEEE80211_NO_TYPE:
                //1Mbps
                sta->hwratetbl[i] = ssv6200_rate[i].init_hw_value;
                break;
            case IEEE80211_PREAMBLE_MODE:
                sta->hwratetbl[i] = ssv6200_rate[i].init_hw_value + (HW_PREABBLE_OFFSET*sta->preamble);
                break;
            case IEEE80211_NETWORK_MODE:
                //G - N-LGI - N-SGI - GREENFIELD
                if(sta->networkMode == PHY_MODE_G_ONLY)
                    sta->hwratetbl[i] = ssv6200_rate[i].init_hw_value;
                else
                    sta->hwratetbl[i] = ssv6200_rate[i].init_hw_value + (HW_HT_OFFSET*(1+sta->networkMode));
                break;
            default:
#ifdef RATE_CONTROL_DEBUG
                    printf("[RATE control]flags[%d]ERROR!!\n",i);
#endif
                break;
		}

        if (!ssv62xx_rate_supported(sta, i))
			continue;
		//n++;

        //Only for 2GHz (Please read register)
#ifdef PROBE
        //avoid 1M to use short preamble
        preamble = (sta->hwratetbl[i] == 0)?0:1;
        rinfo[i].perfect_tx_time = TDIFS + (TSLOT * 15 >> 1) + pide_frame_duration(1530,
				ssv6200_phy_rate[sta->hwratetbl[i]].hkbps, preamble, ssv6200_phy_rate[sta->hwratetbl[i]].type) +
				pide_frame_duration(10, ssv6200_phy_rate[sta->hwratetbl[i]].hkbps, preamble, ssv6200_phy_rate[sta->hwratetbl[i]].type);
        if(rcdbg & DBG_LV_GENERAL)
            printf("default pt = %d\n", rinfo[i].perfect_tx_time );
		rinfo[i].throughput = 0;
#endif
#ifdef NO_REDUCTION
		rinfo[i].this_attempt = 0;
		rinfo[i].this_success = 0;
		rinfo[i].this_fail = 0;
#endif
		rinfo[i].attempt = 0;
		rinfo[i].success = 0;
		rinfo[i].fail = 0;
#ifdef RATE_CONTROL_DEBUG
		printf("[RATE control]support rate[%s]\n",rateControlGetRate(sta->hwratetbl[i]));
#endif
	}
}

/* After initializing rate info, do the rates ordering by speed
 * @sta: target sta
 */
static void ssv62xx_rateinfo_remap(SSV6XXX_STA *sta)
{
    u16 i, j, pre_bitrate, next_bitrate, tmp;
    bool stop_order;
    struct rc_pid_rateinfo *rinfo = sta->pinfo.rinfo;

    ssv62xx_rateinfo_init(sta);
	//Please make sure ssv6200_rate table order.
	for (i = 1; i < SSV6200_SUPPORT_RATE_NUMBER; i++) {
		stop_order = false;
		for (j = 0; j < SSV6200_SUPPORT_RATE_NUMBER - i; j++)
		{
            pre_bitrate = ssv6200_phy_rate[sta->hwratetbl[rinfo[j].index]].hkbps;
            next_bitrate = ssv6200_phy_rate[sta->hwratetbl[rinfo[j+1].index]].hkbps;

			if (pre_bitrate > next_bitrate) {
				tmp = rinfo[j].index;
				rinfo[j].index = rinfo[j + 1].index;
				rinfo[j + 1].index = tmp;
				rinfo[rinfo[j].index].rev_index = j;
				rinfo[rinfo[j + 1].index].rev_index = j + 1;
				stop_order = true;
			}
		}
		if (!stop_order)
			break;
	}
}

/* Adjust the rate while ensuring that we won't switch to a lower rate if it
 * exhibited a worse failed frames behaviour and we'll choose the highest rate
 * whose failed frames behaviour is not worse than the one of the original rate
 * target. While at it, check that the new rate is valid.
 * @adj: adjust level of rate
 */
static void rate_control_pid_adjust_rate(SSV6XXX_STA *sta,
					 struct rc_pid_sta_info *spinfo, int adj,
					 struct rc_pid_rateinfo *rinfo, u8 wsid)
{
	int cur_sorted, new_sorted, probe, tmp, n_bitrates;
	int cur = spinfo->txrate_idx;
#ifdef SELF_GEN_PROBE_PKT
    int i;
#endif

	n_bitrates = SSV6200_SUPPORT_RATE_NUMBER;

	/* Map passed arguments to sorted values. */
	cur_sorted = rinfo[cur].rev_index;
	new_sorted = cur_sorted + adj;

	/* Check limits. */
	if (new_sorted < 0)
		new_sorted = rinfo[0].rev_index;
	else if (new_sorted >= n_bitrates)
		new_sorted = rinfo[n_bitrates - 1].rev_index;

	tmp = new_sorted;

	if (adj < 0) {
		/* Ensure that the rate decrease isn't disadvantageous. */
		for (probe = cur_sorted; probe >= new_sorted; probe--)
        {
            if(ssv62xx_rate_available(sta, rinfo[probe].index))
                tmp = probe;
            else if(new_sorted > 0) // moving edge
                new_sorted--;
        }
	} else {
		/* Look for rate increase with zero (or below) cost. */
		for (probe = new_sorted; probe <= new_sorted; probe++)
        {
            if(ssv62xx_rate_available(sta, rinfo[probe].index))
                tmp = probe;
            else if(new_sorted < (SSV6200_SUPPORT_RATE_NUMBER-1))
                new_sorted++;
        }
	}

	/* Fit the rate found to the nearest supported rate. */
	do {
		if (ssv62xx_rate_available(sta, rinfo[tmp].index)) {
#ifdef PROBE
			spinfo->tmp_rate_idx = rinfo[tmp].index;
#else
			spinfo->txrate_idx = rinfo[tmp].index;
#endif//#ifdef PROBE
			break;
		}
		if (adj < 0)
			tmp--;
		else
			tmp++;
	} while (tmp < n_bitrates && tmp >= 0);


#ifdef PROBE
	if (spinfo->tmp_rate_idx != spinfo->txrate_idx)
    {
#ifdef RC_ENHANCE
        if (adj < 0)
        {
            //For every rate in simple algorithm when adjusting down the rate
            {
                //Comparison of throughput, if current tp bigger than 90% ideal tp of probe rate
                //The code is designed of divison avoidance
                if (((rinfo[spinfo->txrate_idx].throughput*rinfo[spinfo->tmp_rate_idx].perfect_tx_time) > 90000000)&&(!force_up_down))
                {
                    if(rcdbg & DBG_LV_GENERAL)
                        printf("%s predict TP is bigger, no adjusting down\n", rateControlGetRate(sta->hwratetbl[spinfo->txrate_idx]));

                    spinfo->tmp_rate_idx = spinfo->txrate_idx;
                    return;
                }
                else
                {
                    if ((rcdbg & DBG_LV_GENERAL) || (rcdbg & DBG_LV_SAMPLE))
                        printf("%s change down at TP %d\n", rateControlGetRate(sta->hwratetbl[spinfo->txrate_idx]), rinfo[spinfo->txrate_idx].throughput);
                }
            }
            //Reset related parameters after changing rate
            spinfo->txrate_idx = spinfo->tmp_rate_idx;
            spinfo->agedpf = 0;
            spinfo->last_pf = 0;
            spinfo->ConPrbUpSuc = RC_MOVINGCPU_MIN;

            if (wsid == 0)
            {
                SET_MTX_MIB_CNT0 (0);
                //Customized
                SET_PHY_IDX_WSID0(sta->hwratetbl[spinfo->txrate_idx]);
            }
            else
                SET_MTX_MIB_CNT1 (0);
#if (N_MODE_AMPDU_TX == 1)
            _multi_rate_remap(sta, spinfo->txrate_idx, FALSE);
#endif

            if(rcdbg & DBG_LV_GENERAL)
                printf("No probe and change rate to %s\n", rateControlGetRate(sta->hwratetbl[spinfo->tmp_rate_idx]));
        }
        else
#endif //#if RC_ENHANCE
        {
            //Start to do probe and set the probe frame count
            spinfo->monitoring = 1;
            spinfo->probe_cnt = MAXPROBES;
            spinfo->feedback_probes = 0;
#if (N_MODE_AMPDU_TX == 1)
            _multi_rate_remap(sta, spinfo->tmp_rate_idx, TRUE);
#endif
            if ((rcdbg & DBG_LV_GENERAL) || (rcdbg & DBG_LV_SAMPLE))
                printf("Probe %s\n", rateControlGetRate(sta->hwratetbl[spinfo->tmp_rate_idx]));
        }
	}

#endif//#ifdef PROBE
}

/* conclude the statistics of current rate and probe rate
 * If aged_pf (PRE) is bigger than downgrade PER or smaller than upgrade PER,
 * Start to downgrade rate or upgrade rate procedure in rate_control_pid_adjust_rate
 */
static void rate_control_pid_sample(struct rc_pid_info *pinfo,
				    SSV6XXX_STA *sta,
				    struct rc_pid_sta_info *spinfo, u8 wsid)
{
	struct rc_pid_rateinfo *rinfo = pinfo->rinfo;
	u8 pf, tmp_up_pf = rc_up_pf;
	int adj;
	//unsigned long period;
#ifdef PROBE
	unsigned int dlr;
	unsigned int perfect_time = 0;
#ifndef RC_ENHANCE
	unsigned int this_thp, ewma_thp;
#endif
#endif
	struct rc_pid_rateinfo *rate;
#ifdef RATE_CONTROL_DEBUG
    printf("******%s\n", __FUNCTION__);
#endif

	/* In case nothing happened during the previous control interval, turn
	 * the sharpening factor on. */
#ifdef PROBE
	if (!spinfo->monitoring)
#endif//#ifdef PROBE
	{
        dbg_get_time(&spinfo->last_sample);
		/* This should never happen, but in case, assume old sample is
		* still a good measurement and copy it. */
		if (spinfo->tx_num_xmit == 0)
			return;
        // pf = PER
        pf = spinfo->tx_num_failed * 100 / spinfo->tx_num_xmit;
        if ((rcdbg & DBG_LV_GENERAL) && ((pf > 20) || (spinfo->agedpf > 20)))
            printf("fail/tx/pf/agedpf = %d/%d/%d/%d\n", spinfo->tx_num_failed, spinfo->tx_num_xmit, pf, spinfo->agedpf);
#ifdef NO_REDUCTION
		if (pinfo->rinfo[spinfo->txrate_idx].this_attempt > 0)
#else
		if (spinfo->tx_num_xmit> 0)
#endif
		{
			rate = &pinfo->rinfo[spinfo->txrate_idx];
#ifdef PROBE
#ifdef NO_REDUCTION
			dlr = 100 - rate->this_fail * 100 / rate->this_attempt;
#else
			dlr = 100 - spinfo->tx_num_failed * 100 / spinfo->tx_num_xmit;
#endif
			_calculate_tp(rate, spinfo, pf);
#endif//#ifdef PROBE
#ifdef NO_REDUCTION
			rate->attempt += rate->this_attempt;
			rate->success += rate->this_success;
			rate->fail += rate->this_fail;
#else
			rate->attempt += spinfo->tx_num_xmit;
			rate->success += (spinfo->tx_num_xmit - spinfo->tx_num_failed);
			rate->fail += spinfo->tx_num_failed;
#endif
			spinfo->tx_num_xmit = 0;
			spinfo->tx_num_failed = 0;
#ifdef NO_REDUCTION
			rate->this_fail = 0;
			rate->this_success = 0;
			rate->this_attempt = 0;
#endif
            {
#if (N_MODE_AMPDU_TX == 1)
                if ((sta->ampdu_tx == TRUE) && (AMPDU_TX_ENABLE == 1) && (rc_up_pf_fastest_b != rc_up_pf) &&
                    (spinfo->txrate_idx == ssv62xx_rate_highest_index_by_mode(sta, WLAN_RC_PHY_CCK)))
                    tmp_up_pf = rc_up_pf_fastest_b;
#endif
                //Simply use specific upgrade PER and downgrade PER to decide rate adjustment
                if(spinfo->agedpf < tmp_up_pf)
                    adj = 1;
                else if(spinfo->agedpf > rc_down_pf)
                    adj = -1;
                else
                    adj = 0;
            }
#ifdef RC_ENHANCE
            if (adj > 0)
            {
                spinfo->ConPrbUpCnt++;
                if ((spinfo->ConPrbUpCnt) < spinfo->ConPrbUpSuc)
                    return;
                else
                    spinfo->ConPrbUpCnt = 0; // reset cnt here
            }
            else if (adj < 0)//(adj <= 0)
            {
                //Reset continued probe up counter due to downgrading rate occurs
                spinfo->ConPrbUpCnt = 0;
                if (rcdbg & DBG_LV_GENERAL)
                    printf ("ConPrbUpCnt:0, no adj up\n");
            }
#endif //#ifdef RC_ENHANCE
			/* if at highest rate or lowest rate, don't need to up grade or down grade  */
			if (((adj > 0)&&(spinfo->txrate_idx != ssv62xx_rate_highest_index(sta))) ||
             ((adj < 0 ) && (spinfo->txrate_idx != ssv62xx_rate_lowest_index(sta))))//(adj != 0)
            {
#ifdef RATE_CONTROL_DEBUG
                if((spinfo->txrate_idx!=11) || ((spinfo->txrate_idx==11)&&(adj < 0)))
                    printf("[RC]Probe adjust[%d] dlr[%d%%] this_thp[%d] ewma_thp[%d] index[%d]\n",adj ,dlr,this_thp,ewma_thp,spinfo->txrate_idx);
#endif
#ifdef RC_ENHANCE
                //adjusting rate only one scale at once
                adj = (adj > 0)?1:-1;
#endif //#ifdef RC_ENHANCE

				rate_control_pid_adjust_rate(sta, spinfo, adj, rinfo, wsid);
			}
		}
	}
#ifdef PROBE
	else
    {
        // At probe procedure, if more than one probe packet is received, start to do sample of probe rate
        if((spinfo->feedback_probes >= MAXPROBES) || ((spinfo->feedback_probes != 0) && (spinfo->probe_cnt == 0)))
        {
            rate = &pinfo->rinfo[spinfo->txrate_idx];
            dbg_get_time(&spinfo->last_sample);
            if (rate->this_attempt > 0)
            {
                dlr = 100 - rate->this_fail * 100 / rate->this_attempt;

                spinfo->last_dlr = dlr;
                pf = 100 - dlr;
                _calculate_tp(rate, spinfo, pf);
                rate->attempt += rate->this_attempt;
                rate->success += rate->this_success;
                rinfo[spinfo->txrate_idx].fail += rate->this_fail;
                rate->this_fail = 0;
                rate->this_success = 0;
                rate->this_attempt = 0;
            }

            rate = &pinfo->rinfo[spinfo->tmp_rate_idx];

            if (rate->this_attempt > 0)
            {
                dlr = 100 - rate->this_fail * 100 / rate->this_attempt;
                perfect_time = rate->perfect_tx_time;
                if (!perfect_time)
                    perfect_time = 1000000;

                rate->throughput = dlr * (1000000 / perfect_time);

                if (rcdbg & DBG_LV_SAMPLE)
                    printf("Probe PER/TP = %d/%d, Origin TP=%d\n", (100-dlr),rate->throughput, pinfo->rinfo[spinfo->txrate_idx].throughput);

                /* adopt proposed rate if it is better. */
                if (rate->throughput > pinfo->rinfo[spinfo->txrate_idx].throughput)
                {
                    if ((rcdbg & DBG_LV_GENERAL) || (rcdbg & DBG_LV_SAMPLE))
                        printf ("Change rate!!!!, %s -> %s\n",
                            rateControlGetRate (sta->hwratetbl[spinfo->txrate_idx]),
                            rateControlGetRate (sta->hwratetbl[spinfo->tmp_rate_idx]));

                    spinfo->txrate_idx = spinfo->tmp_rate_idx;
#ifdef RC_ENHANCE
                    //Reset related parameters after changing rate
                    spinfo->agedpf = 0;
                    spinfo->ConPrbUpSuc = RC_MOVINGCPU_MIN;
                    spinfo->ConPrbUpCnt = 0;
                    spinfo->last_pf = 0;
#endif //#ifdef RC_ENHANCE
                    //Customized
                    SET_PHY_IDX_WSID0(sta->hwratetbl[spinfo->txrate_idx]);
#if (N_MODE_AMPDU_TX == 1)
                    _multi_rate_remap(sta, spinfo->txrate_idx, FALSE);
#endif
                }
#ifdef RC_ENHANCE
                else
                {
                    spinfo->tmp_rate_idx = spinfo->txrate_idx;
                    spinfo->ConPrbUpSuc =(spinfo->ConPrbUpSuc < RC_MOVINGCPU_MAX)?(spinfo->ConPrbUpSuc<<1):RC_MOVINGCPU_MAX;
                    spinfo->ConPrbUpCnt = 0;
                    if(rcdbg & DBG_LV_GENERAL)
                        printf("Fail to probe up, ConPrbUpSuc = %d\n",spinfo->ConPrbUpSuc);
                }
#endif //#ifdef RC_ENHANCE

                rate->attempt += rate->this_attempt;
                rate->success += rate->this_success;
                rate->fail += rate->this_fail;

                rate->this_fail = 0;
                rate->this_success = 0;
                rate->this_attempt = 0;

            }
            else
            {
                spinfo->tmp_rate_idx = spinfo->txrate_idx;
                if(rcdbg & DBG_LV_GENERAL)
                    printf("[RC]: No tx report update for probe packets!!!!\n");
            }

            spinfo->feedback_probes = 0;
            //spinfo->fail_probes = 0;
            spinfo->tx_num_xmit = 0;
            spinfo->tx_num_failed = 0;
            spinfo->monitoring = 0;
            spinfo->probe_report_flag = 0;
            spinfo->probe_cnt = 0;
        }
        else if (spinfo->probe_cnt == 0)
        {
            if(dbg_get_elapsed(&spinfo->last_report) > RC_PROBED_TIMEOUT)
            {
#ifdef RATE_CONTROL_DEBUG
                printf("[RC]@@@@@ PROBE LOSE @@@@@ feedback=[%d] need=[%d] probe_cnt=[%d], last report[%d]\n",
                    spinfo->feedback_probes,MAXPROBES,spinfo->probe_cnt,, dbg_get_elapsed (&spinfo->last_report));
#endif
                spinfo->feedback_probes = 0;
                //spinfo->fail_probes = 0;
                spinfo->tx_num_xmit = 0;
                spinfo->tx_num_failed = 0;
                spinfo->monitoring = 0;
                spinfo->probe_report_flag = 0;
                spinfo->tmp_rate_idx = spinfo->txrate_idx;
            }
        }
	}
#endif//#ifdef PROBE
}

#ifdef PIDE_TEST
//125ms seconds to execute
u32 MTX_WSID_TOTAL=0;
u32 MTX_WSID_SUCC=0;
#endif

#ifdef RATE_CONTROL_PERFORMANCE_MONITOR
u32 rangeTable[10];
u32 rateCounter=0;
#endif


/*Update counter of tx frames
 * @wsid: the wsid of reported STA
 * @hw_rates_idx: the report hardware rate index
 * @frameTx: The count of Tx frames
 * @frameTotal: The total tx counts of these TX frames
 * @frameSuccess: The successful count of TX frames
 * @pkt_complete decribe if this is a probe frame
 */
void rate_control_pid_tx_status(u8 wsid, u8 hw_rates_idx, u16 frameTx, u16 frameTotal, u16 frameSuccess, u8 pkt_complete)
{
	SSV6XXX_STA *sta;
	struct rc_pid_info *pinfo;
	struct rc_pid_sta_info *spinfo;
#ifdef NO_REDUCTION
	struct rc_pid_rateinfo * pidrate;
	struct rc_pid_rateinfo *rate;
#endif
	u16 frameRetry=0;
#ifdef RATE_CONTROL_PERFORMANCE_MONITOR
	u8 x;
#endif

#if (CONFIG_SIM_PLATFORM == 1)
#ifndef PIDE_TEST
	return;
#endif
#endif

	sta = &g6200StaInfo[wsid];

	//for(i=0;i<WLAN_MAX_STA;i++)
    if(sta->valid != FALSE)
	{
        pinfo = &sta->pinfo;
        spinfo = &sta->spinfo;
#ifdef NO_REDUCTION
        pidrate = sta->pinfo.rinfo;
#endif

        if (frameTx != 0)
        {
            frameRetry = frameTotal - frameSuccess;
#ifdef PROBE
            //AMPDU may complete the frame at other rate but probing rate and current rate.
            //collect feedback_probes if AMPDU is enable when current rate is n mode rate or it is probing rate
            if (((spinfo->txrate_idx != spinfo->tmp_rate_idx ) && (hw_rates_idx == sta->hwratetbl[spinfo->tmp_rate_idx]))
#if (N_MODE_AMPDU_TX == 1)
                || ((sta->ampdu_tx == TRUE) && (AMPDU_TX_ENABLE == 1) && (ssv6200_phy_rate[sta->hwratetbl[spinfo->txrate_idx]].type >= WLAN_RC_PHY_HT_MF))
#endif
                )

                spinfo->feedback_probes += pkt_complete;


            if (hw_rates_idx == sta->hwratetbl[spinfo->txrate_idx])
#endif//#ifdef PROBE
            {
                spinfo->tx_num_xmit += frameTotal;
                spinfo->tx_num_failed += frameRetry;
#ifdef NO_REDUCTION
                rate = &pidrate[spinfo->txrate_idx];
                rate->this_fail += frameRetry;
                rate->this_attempt += frameTotal;
                rate->this_success += frameSuccess;
#endif
                //Customized
                if(wsid == 0)
                    UPDATE_ALLCNT_WSID0(frameTotal, frameRetry);
            }
#ifdef PROBE
            else if (hw_rates_idx == sta->hwratetbl[spinfo->tmp_rate_idx])
            {
                //printk("Match tmp_rate_idx\n");
#if (N_MODE_AMPDU_TX == 0)
                if(frameTx == frameSuccess)
#else
                // using (frameTx == frameSuccess) the condition to stop probing action for MPDU tx
                // At AMPDU TX case, don't stop probing because probe frames may be not sent out yet,
                // the real statistics of probing rate may not be returned
                if (((sta->ampdu_tx == TRUE) && (AMPDU_TX_ENABLE == 1) && (ssv6200_phy_rate[hw_rates_idx].type >= WLAN_RC_PHY_HT_MF)) ||
                    ((frameTx == frameSuccess) && ((sta->ampdu_tx == FALSE) || (AMPDU_TX_ENABLE == 0) || (ssv6200_phy_rate[hw_rates_idx].type < WLAN_RC_PHY_HT_MF))))
#endif
                {

                    rate = &pidrate[spinfo->tmp_rate_idx];

                    rate->this_fail += frameRetry;
                    rate->this_attempt += frameTotal;
                    rate->this_success += frameSuccess;
                }
                else
                {
                    spinfo->feedback_probes = 0;
                    //spinfo->fail_probes = 0;
                    spinfo->tx_num_xmit = 0;
                    spinfo->tx_num_failed = 0;
                    spinfo->monitoring = 0;
                    spinfo->probe_report_flag = 0;
                    spinfo->probe_cnt = 0;
                    spinfo->tmp_rate_idx = spinfo->txrate_idx;
#ifdef RC_ENHANCE
                    //Shifting continued probe up window to next scale due to probe frames are failed to be send.
                    if (hw_rates_idx > sta->hwratetbl[spinfo->txrate_idx])
                    {
                        spinfo->ConPrbUpSuc =(spinfo->ConPrbUpSuc < RC_MOVINGCPU_MAX)?(spinfo->ConPrbUpSuc<<1):RC_MOVINGCPU_MAX;
                        spinfo->ConPrbUpCnt = 0;
                    }
#endif //#ifdef RC_ENHANCE
                    if(rcdbg & DBG_LV_GENERAL)
                        printf("Fail at probing frame, stop probing, frameTx/frameSuccess/ConPrbUpSuc = %d/%d/%d\n", frameTx, frameSuccess, spinfo->ConPrbUpSuc);
                }
            }
#endif//#ifdef PROBE
            else //if((report_data->hw_rates_idx != spinfo->tmp_rate_idx) && (report_data->hw_rates_idx != spinfo->txrate_idx))
            {
#ifdef RATE_CONTROL_DEBUG
                printf ("Rate control report mismatch report_rate_idx[%d] tmp_rate_idx[%d] txrate_idx[%d]!!\n",
                        hw_rates_idx, sta->bitrates[spinfo->tmp_rate_idx].hw_value,
                        sta->bitrates[spinfo->txrate_idx].hw_value);
#endif
                return;
            }


        }//if (frameTx != 0)
        //When timeout or probe packets are sent out
        //Even though counter is buggy, we still need to sample
#ifdef RC_ENHANCE
        //When report-and-sample feature is turned on, it means sampling should be executed when report is received
        if ( (!(RC_IS_FEATURED(RC_FEAT_REPORTSAMPLE)) && (dbg_get_elapsed(&spinfo->last_sample) > rcsampleIntval)) ||
            ((spinfo->monitoring == 1) && (spinfo->probe_cnt == 0)) ||
            ((RC_IS_FEATURED(RC_FEAT_REPORTSAMPLE)) && (spinfo->monitoring == 0)) )
#else
        if ((dbg_get_elapsed(&spinfo->last_sample) > 125000) || ((spinfo->monitoring == 1) && (spinfo->probe_cnt == 0)))
#endif //#ifdef RC_ENHANCE
        {
            if ((rcdbg & DBG_LV_GENERAL) && (frameTx == 0))
                printf("frameTx/frameSuccess/frameRetry = %d/%d/%d\n", frameTx, frameSuccess, frameRetry);
            rate_control_pid_sample(pinfo, sta, spinfo, wsid);
        }
	}


    //if(sta_bitmask == 0)
    //    printf("There is no sta available\n");
}

#if (AUTO_BEACON == 1)
extern void check_bcn(void);
#endif
#ifdef RATE_CONTROL_DEBUG
	char preGetRate=0;
	#define RATE_CONTROL_DEBUG_STRING_LEN	24
#endif
u8 show_probepkt_seq = 0;
/*Get rate and do report or probe frame for rate control
 * @pInfo: target frame
 * @management: is this frame MGMT frame
 * @seqno: the sequence number of @pInfo
 */

u8 rate_control_pid_get_rate(void *pInfo, u8 management, u32 seqno)
{
	SSV6XXX_STA *sta;
	struct rc_pid_sta_info *spinfo;
	int rateidx = 0;
    PKT_TxInfo *pTxInfo=(PKT_TxInfo *)pInfo;
    u8 staIndex = pTxInfo->wsid,  unicast = pTxInfo->unicast, jumpRate=0;
    u16 fc = GET_HDR80211_FC(pTxInfo);

#ifdef RATE_CONTROL_DEBUG
	char string[RATE_CONTROL_DEBUG_STRING_LEN];
#endif
    #if(PACKET_TRACE==1)
    pt((void *)pInfo,M_ENG_MAX,EN_LOC_GET_RATE);
    #endif
    if(pideFixRateEnable)
    {
        if ((erp_protect == TRUE) && (pideFixRate > 6))
        {
            pTxInfo->do_rts_cts = 1; //TX queue probably stop by CTS to self. So we need to use RTS/CTS
        }
        else
            pTxInfo->do_rts_cts = 0;

        return pideFixRate;
    }
    if(pideJumpRateEnable){
        jumpRate=pideJumpRate[pideJumpRateIdx];
        pideJumpRateIdx++;
        pideJumpRateIdx=pideJumpRateIdx%sizeof(pideJumpRate);
        return jumpRate;
    }

    //staIndex >= WLAN_MAX_STA includes wsid = 0xf(Not found) and wsid = 0xe(BC/MC data packet)
    //Todo: Should consider all rates of connected STAs for BC/MC data packets?
    if(staIndex >= WLAN_MAX_STA)
    {
        if ((staIndex != 0xe) && (staIndex != 0xf))
            printf("[RC]Wrong WSID %d!!\n", staIndex);
        return DEFAULT_RATE_HW_VALUE;
    }
    else if(g6200StaInfo[staIndex].valid == FALSE)
        return DEFAULT_RATE_HW_VALUE;

    sta=&g6200StaInfo[staIndex];
	spinfo = &sta->spinfo;

#if 0
	if (txrc->rts)
		info->control.rates[0].count =
			txrc->hw->conf.long_frame_max_tx_count;
	else
		info->control.rates[0].count = 2;
#endif

	/* Send management frames and NO_ACK data using lowest rate. */
	if((!unicast) || (management))
        return (u8)sta->hwratetbl[ssv62xx_rate_lowest_index(sta)];

    if ((IS_NULL_DATA(fc)) || (IS_QOS_NULL_DATA(fc)))
    {
        rateidx = spinfo->txrate_idx;
        goto get_rate_exit;
    }

    // If one rate, or AMPDU feature is not available for STA whose wsid is not 0, don't need to do report/probe
    if ((sta->pinfo.rate_sum == 1)
#if (N_MODE_AMPDU_TX == 1)
         && (((sta->ampdu_tx == FALSE) || (AMPDU_TX_ENABLE == 0)))
#endif
             )
    {
        //printf("[RC]: Check\n");
        rateidx = spinfo->txrate_idx;
        goto get_rate_exit;
    }

    if ((staIndex == 0) && (prb_pkt_inited == FALSE) && (g_prb_pkt_enable == 1))
        _update_prb_pkt(pTxInfo);
#ifdef PROBE
    show_probepkt_seq = 0;
    if (spinfo->monitoring)
    {
        //before probe, get the statistics of current rate
        if (spinfo->probe_report_flag == 0)
        {
            spinfo->probe_report_flag = 1;
            rateidx = spinfo->txrate_idx;
            pTxInfo->tx_report = 1;
#ifdef RATE_CONTROL_DEBUG
            printf("packet before probe ");
#endif
        }
        else if ((spinfo->probe_report_flag == 1) && (spinfo->probe_cnt > 0))
        {
            spinfo->probe_cnt--;
            rateidx = spinfo->tmp_rate_idx;
            pTxInfo->tx_report = 1;
#ifdef RATE_CONTROL_DEBUG
            printf("probe packet ");
#endif
            show_probepkt_seq = 1;
        }
        else //After sending out probe packets
        {
            // post probing, check if last probing is time out or not
#ifdef RATE_CONTROL_DEBUG
            printf("after probe packet %d:%d\n", seqno, spinfo->probe_cnt);
#endif
            if(dbg_get_elapsed(&spinfo->last_report) > RC_PROBED_TIMEOUT)
            {
#ifdef RATE_CONTROL_DEBUG
                printf("after probe packet, timeout for last report frame\n");
#endif
                spinfo->feedback_probes = 0;
                //spinfo->fail_probes = 0;
                spinfo->tx_num_xmit = 0;
                spinfo->tx_num_failed = 0;
                spinfo->monitoring = 0;
                spinfo->probe_report_flag = 0;
                spinfo->tmp_rate_idx = spinfo->txrate_idx;
#if (AUTO_BEACON == 1)
                // Avoid collision of last report frame and beacon
                // wish that incoming frame would be faster than bcn_check timer
                check_bcn();
#endif
            }
            rateidx = spinfo->txrate_idx; //use original rate
        }

    }else
#endif//#ifdef PROBE
    {
        rateidx = spinfo->txrate_idx;
        //report tx counter back to avoid counter overflow
        if(dbg_get_elapsed(&spinfo->last_report) > rcreportIntval)
        {
            pTxInfo->tx_report = 1;
#ifdef RATE_CONTROL_DEBUG
            printf("report packet ");
#endif
        }
    }

    if(pTxInfo->tx_report == 1)
    {
        if (pTxInfo->RSVD_0 == 0)
            pTxInfo->RSVD_0 = (rateidx == spinfo->txrate_idx)?RC_SAMPLE:RC_PROBE;

        pTxInfo->RSVD_3 = 0;
        dbg_get_time(&spinfo->last_report);
        if ((show_probepkt_seq == 1) && ((rcdbg & DBG_LV_GENERAL)))
            printf("%d:%d\n", seqno, spinfo->probe_cnt);

        if ((IS_NULL_DATA(fc)) || IS_QOS_NULL_DATA(fc))
            printf("[RC]: buggy!! Why NULL data get report?\n");
    }
    else
    {
        if (pTxInfo->RSVD_0 != RC_RESEND)
            pTxInfo->RSVD_0 = 0;
    }


	if (rateidx >= SSV6200_SUPPORT_RATE_NUMBER)
		rateidx = SSV6200_SUPPORT_RATE_NUMBER - 1;

#ifdef RATE_CONTROL_DEBUG
	if(preGetRate == sta->hwratetbl[rateidx])
		;
	else
	{
		printf("[RATE control]get rate[%s]\n",rateControlGetRate(sta->hwratetbl[rateidx]);
		preGetRate = sta->hwratetbl[rateidx];
	}
#endif
get_rate_exit:
    //If erp_protect is enable, need to do rtstoself for non b mode rates.
    if (((erp_protect == TRUE) && (sta->hwratetbl[rateidx] > 6))||(g_force_rtscts==TRUE))
        pTxInfo->do_rts_cts = 1; //TX queue probably stop by CTS to self. So we need to use RTS/CTS
    else
        pTxInfo->do_rts_cts = 0;

    //printf("r[%d],rateidx=%d\r\n", sta->hwratetbl[rateidx]);
	return (u8)sta->hwratetbl[rateidx];
}

/*Procedure once a sta connect to DUT
 * @staIndex: the wsid of the newly connected STA
 * @sta_info: the info of the newly connected STA
 */
void rate_control_pid_sta_connect(u8 staIndex, struct cfg_set_sta *sta_info)
{
	SSV6XXX_STA *sta;
	//int n = 0;
	struct rc_pid_sta_info *spinfo;
	struct rc_pid_info *pinfo;
    bool update_all_bmode = FALSE;
#ifdef RATE_CONTROL_DEBUG
	char string[RATE_CONTROL_DEBUG_STRING_LEN];
#endif
	sta=&g6200StaInfo[staIndex];

	if(pideFixPreambleEnable)
		sta->preamble = pideFixPreamble;
	if(pideFixNetworkEnable)
		sta->networkMode = pideFixNetwork;

    //TO-DO: set networkMode = PHY_MODE_G_ONLY
    sta->networkMode = PHY_MODE_G_ONLY;

    //Get nmode rates and B/G mode rates. If phy supports more attenna or other MCS, need to modify.
    if(sta_info != NULL)
    {
        sta->non_ht_rates = sta_info->non_ht_supp_rates;
        sta->mcs_0 = sta_info->ht_capabilities.supported_mcs_set[0];
        sta->preamble = (long_preamble_idx == 0)?sta_info->short_preamble:PHY_MODE_LONG_PREAMBLE;
        sta->ht_capa_b6b4 = (sta_info->ht_capabilities.ht_capabilities_info&0x30)>>4;
        _decide_networkMode(sta);
        /* Record long preamble station, update all STAs to use long preamble once new added sta uses long preamble only*/
        if (sta_info->short_preamble == PHY_MODE_LONG_PREAMBLE)
        {
            if(long_preamble_idx == 0)
                update_all_bmode = TRUE;
            long_preamble_idx |= BIT(staIndex);
            if (rcdbg & DBG_LV_GENERAL)
                printf("update all bmode = %d, long_preamble_idx = %x\n",update_all_bmode, long_preamble_idx);
        }
    }

#ifdef RATE_CONTROL_DEBUG
	printf("[RATE control]rate_control_pid_sta_connect\n");
	printf("[RATE control]curr_rates[%08x]networkMode[%d]preamble[%d]\n",sta->curr_rates,sta->networkMode,sta->preamble);
#endif

	spinfo = &sta->spinfo;
	pinfo = &sta->pinfo;
	sta->valid = TRUE;
    sta->ampdu_tx = FALSE;
    sta->wsid = staIndex;

#ifdef RATE_CONTROL_DEBUG
    printf("STA=%2x:%2x:%2x:%2x:%2x:%2x\r\n",sta->addr.addr[0],sta->addr.addr[1],sta->addr.addr[2],
                                             sta->addr.addr[3],sta->addr.addr[4],sta->addr.addr[5]);
#endif
	if(sta->supp_rates == 0)
	{
		sta->supp_rates = 0x0fff;
#ifdef RATE_CONTROL_DEBUG
		printf("[RATE control]sta->supp_rates is not initialized!!\n");
#endif
	}
    sta->curr_rates = sta->supp_rates&defaultpideRateMsk;

	memset(spinfo,0x00,sizeof(struct rc_pid_sta_info));
	memset(pinfo,0x00,sizeof(struct rc_pid_info));
    pinfo->rate_sum = ratebit_count(sta->curr_rates);

//#ifdef PROBE
//	spinfo->probe_cnt = MAXPROBES;
//#endif
    if(g_drate_endian==1)
    {
        spinfo->txrate_idx = ssv62xx_rate_lowest_index(sta);
    }
    else
    {
        spinfo->txrate_idx = ssv62xx_rate_highest_index(sta);
    }
    
#ifdef PROBE
    spinfo->tmp_rate_idx = spinfo->txrate_idx;
    pinfo->oldrate = spinfo->txrate_idx;
#endif


	/* TODO: This routine should consider using RSSI from previous packets
	 * as we need to have IEEE 802.1X auth succeed immediately after assoc..
	 * Until that method is implemented, we will use the lowest supported
	 * rate as a workaround. */

    ssv62xx_rateinfo_remap(sta);
    if (update_all_bmode == TRUE)
        _calculate_bmode_txtime(staIndex, true);
#if (N_MODE_AMPDU_TX == 1)
    if(sta->networkMode >= PHY_MODE_LONG_GI)
        _multi_rate_remap(sta, spinfo->txrate_idx, FALSE);
#endif

    if(rcdbg & DBG_LV_GENERAL)
        printf("long_preamble_idx = %x, staIndex = %d\n", long_preamble_idx, staIndex);

	if(staIndex == 0)
    {
        SET_MTX_MIB_EN0 (1);
        //Customized
        SET_PHY_IDX_WSID0 (sta->hwratetbl[spinfo->txrate_idx]);
        RESET_ALLCNT_WSID0;
        prb_pkt_inited = FALSE;
    }
    else
    {   
		SET_MTX_MIB_EN1(1);
        SET_TXQ2_MTX_Q_SRC_LIMIT(4); //Retry 3 time
    }
    dbg_get_time(&spinfo->last_report);
    dbg_get_time(&spinfo->last_sample);

}

/*Procedure once a sta disconnect to DUT
 * @staIndex: wsid of target sta
 */
void rate_control_pid_sta_disconnect(u8 staIndex)
{
	SSV6XXX_STA *sta;
#ifdef RATE_CONTROL_DEBUG
	printf("[RATE control]rate_control_pid_sta_disconnect[%d]!!\n",staIndex);
#endif
    if(long_preamble_idx & BIT(staIndex))
    {
        long_preamble_idx &= ~BIT(staIndex);
        //
        if (long_preamble_idx == 0)
            _calculate_bmode_txtime(staIndex, true);
    }

    if(rcdbg & DBG_LV_GENERAL)
        printf("long_preamble_idx = %x, staIndex = %d\n", long_preamble_idx, staIndex);

	sta=&g6200StaInfo[staIndex];
	sta->valid = FALSE;
    sta->ampdu_tx = FALSE;


    memset(sta, 0, sizeof(SSV6XXX_STA));

	if(staIndex == 0)
    {
 		SET_MTX_MIB_EN0(0);
        SET_PHY_IDX_WSID0(0xffff);
        RESET_ALLCNT_WSID0;
        prb_pkt_inited = FALSE;
    }
	else
		SET_MTX_MIB_EN1(0);
}


s32 rate_control_pid_init()
{
#ifdef RATE_CONTROL_PERFORMANCE_MONITOR
	memset(rangeTable,0x00,40);
	rateCounter = 0;
#endif
    dbg_timer_init();
#ifdef GREENFIELD_ENABLE
    gf_enable = 1;
#else
    gf_enable = 0;
#endif

#ifdef THROUGHPUT_TEST
	extern void txThroughputHandler();
	//Init 200ms timer
	if( OS_TimerCreate(&pid_timer, 200, (u8)TRUE, NULL, (OsTimerHandler)txThroughputHandler) == OS_FAILED)
		return OS_FAILED;

	OS_TimerStart(pid_timer);
	return OS_SUCCESS;
#endif

#ifdef RATE_CONTROL_DEBUG
	printf("[RATE control]rate_control_pid_init!!");
#endif
	//ssv6200_band.bitrates = NULL;

	//Init 125ms timer
	//if( OS_TimerCreate(&pid_timer, RC_PID_INTERVAL, (u8)TRUE, NULL, (OsTimerHandler)rate_control_pid_tx_status) == OS_FAILED)
	//	return OS_FAILED;

    //Customized
    SET_PHY_IDX_WSID0(0xffff);
	return OS_SUCCESS;
}

void rate_control_pid_deinit()
{
#ifdef THROUGHPUT_TEST
		return;
#endif

#ifdef RATE_CONTROL_DEBUG
		printf("[RATE control]rate_control_pid_deinit!!\n");
#endif
    _process_prb_pkt(FALSE);
	return;
}

void rate_control_jump_rate(bool enable)
{
    if(enable == TRUE){
        pideJumpRateEnable = 1;
    }

}

/* Set hw rate index to use the rate only
 * @rate: target hardware rate index
 */
void rate_control_fix_rate(u32 rate)
{
#if (N_MODE_AMPDU_TX == 1)
    u8 i;
#endif
    if(rate < PHY_SUPPORT_RATE_NUMBER)
    {
        pideFixRateEnable = 1;
        pideFixRate = (u8)rate;
    }
    else
    {
        pideFixRateEnable = 0;
    }

#if (N_MODE_AMPDU_TX == 1)
    for(i = 0; i< WLAN_MAX_STA;i++)
    {
        SSV6XXX_STA *sta = NULL;

        sta=&g6200StaInfo[i];
        if ((sta->valid == TRUE) && (sta->ampdu_tx == TRUE))
            _multi_rate_remap(sta, sta->spinfo.txrate_idx, FALSE);
    }
#endif
}

void rate_control_fix_preamble(u32 preamble)
{
    if(preamble <= PHY_MODE_SHORT_PREAMBLE)
    {
        pideFixPreambleEnable = 1;
        pideFixPreamble = (u8)preamble;
    }
    else
    {
        pideFixPreambleEnable = 0;
    }
}

void rate_control_fix_network(u32 network)
{
    if(network <= PHY_MODE_G_ONLY)
    {
        pideFixNetworkEnable = 1;
        pideFixNetwork = (u8)network;
    }
    else
    {
        pideFixNetworkEnable = 0;
    }
}

//Clear statistics of specific STA
void rate_control_reset_tbl(SSV6XXX_STA *sta)
{
    struct rc_pid_sta_info *spinfo;
	struct rc_pid_rateinfo *rinfo;
    int i;

    spinfo = &sta->spinfo;
	rinfo = sta->pinfo.rinfo;

    memset(spinfo,0x00,sizeof(struct rc_pid_sta_info));

    if(g_drate_endian==1)
        spinfo->txrate_idx = ssv62xx_rate_lowest_index(sta);
    else
        spinfo->txrate_idx = ssv62xx_rate_highest_index(sta);
    
    spinfo->tmp_rate_idx = spinfo->txrate_idx;

    for(i = 0 ; i< (SSV6200_SUPPORT_RATE_NUMBER);i++)
    {
        rinfo[i].throughput = 0;
        rinfo[i].this_attempt = 0;
        rinfo[i].this_success = 0;
        rinfo[i].this_fail = 0;
        rinfo[i].attempt = 0;
        rinfo[i].success = 0;
        rinfo[i].fail = 0;
    }

    dbg_get_time(&spinfo->last_report);
    dbg_get_time(&spinfo->last_sample);
}

/* Set the specific connect STA with default rate mask
 * Remove 11M for STA 0 and remove b mode rate for wsid is not 0 when AMPDU is on
 */
void rate_control_set_defaul_rates_by_sta(u8 wsid)
{
    SSV6XXX_STA *sta = NULL;
    u16 target_rates = 0;

    sta=&g6200StaInfo[wsid];
    if(sta->valid == false)
        return;

    //TODO: need to know the default support rate for future development
    target_rates = sta->supp_rates&(defaultpideRateMsk);
#if (N_MODE_AMPDU_TX == 1)
    //If non wsid 0 sta uses ampdu_tx, b mode rate no use.
    //If wsid 1 sta uses ampdu_tx
    if ((sta->ampdu_tx == TRUE) && (AMPDU_TX_ENABLE == 1))
    {
        // Remove 11M or b mode rate if n mode rate is available
        if ((target_rates & 0x0ff0) != 0)
        {
            if (wsid == 0)
                target_rates &= (~0x0008);
            else
                target_rates &= (~0x000f);
        }
    }
#endif

    //If target_rate mask match current rates, don't need to update
    if (target_rates == sta->curr_rates)
        return ;

    sta->curr_rates = target_rates;
    sta->pinfo.rate_sum = ratebit_count(sta->curr_rates);

    rate_control_reset_tbl(sta);
    if (wsid == 0)
        SET_MTX_MIB_CNT0(0);
    else
        SET_MTX_MIB_CNT1(0);

#if (N_MODE_AMPDU_TX == 1)
    _multi_rate_remap(sta, sta->spinfo.txrate_idx, FALSE);
#endif
}

/* Set the connect STA with default rate mask
 * @dftratemsk: The new value for defaultpideRateMsk
 */
void rate_control_set_default_rates_mask(u16 dftratemsk)
{
    u8 i = 0;

    defaultpideRateMsk = dftratemsk;
    for(;i < WLAN_MAX_STA; i++)
        rate_control_set_defaul_rates_by_sta(i);
}

/* Reset the rate table if rate features of connect STA change
 * @wsid: wsid of target sta
 */
void rate_control_set_rate_capa_by_sta(u8 wsid)
{
    SSV6XXX_STA *sta = NULL;

    sta=&g6200StaInfo[wsid];
    if(sta->valid == false)
        return;

    _decide_networkMode(sta);
    ssv62xx_rateinfo_remap(sta);
}

void rate_control_set_gf(u8 gf_st)
{
    printf("%s:gf_st = %d\n", __FUNCTION__, gf_st);
#ifdef GREENFIELD_ENABLE
    if (gf_enable != gf_st)
    {
        u8 i = 0;

        gf_enable = gf_st;
        for(;i < WLAN_MAX_STA; i++)
            rate_control_set_rate_capa_by_sta(i);
    }
#endif
    }
/* set erp protect*/
void rate_control_set_erp_protect(bool param)
{
    erp_protect = param;
    printf("%s:param = %d\n", __FUNCTION__, param);
}

bool rate_control_get_erp_protect()
{
    return erp_protect;
}

/*This API is designed to NON-AP STA to update preamble type
 * @wsid: wsid of target sta
 * @preamble: preamble type. 0 for short preamble, 1 for long preamble
 */
void rate_control_set_preamble(u8 wsid, u8 preamble)
{
    SSV6XXX_STA *sta = NULL;

    sta=&g6200StaInfo[wsid];
    if(sta->valid == false)
        return;

    if(preamble != sta->preamble)
    {
        sta->preamble = preamble;
        _calculate_bmode_txtime(wsid, false);
    }
}

void rate_control_show_info (u8 staIndex, u8 mode)
{
    SSV6XXX_STA *sta = NULL;
    struct rc_pid_sta_info *spinfo = NULL;
    struct rc_pid_rateinfo *pidrate = NULL;
    Time_T now;

    if (mode < RCINFO_DBG_GENMSG)
    {
        sta = &g6200StaInfo[staIndex];
        spinfo = &sta->spinfo;
        if (sta->valid == FALSE)
        {
            printf ("STA %d is invalid\n", staIndex);
            return;
        }
        spinfo = &sta->spinfo;
        pidrate = sta->pinfo.rinfo;
        if (mode == RCINFO_DBG_SHOW_ST)
        {
            printf ("------------STA %d RC status------------\n", staIndex);

            printf ("current rc status: ");
            if (spinfo->monitoring != 0)
            {
                if (spinfo->probe_report_flag != 0)
                    printf ("%s\n", "probing");
                else
                    printf ("%s\n","monitoring");
            }
            else
                printf ("%s\n","normal");

            printf ("current rate: ");
            if (pideFixRateEnable == 0)
                printf ("auto, %s\n", rateControlGetRate(sta->hwratetbl[spinfo->txrate_idx]));
            else
                printf ("fixrate, %s\n", rateControlGetRate(pideFixRate));

            if (pideFixRateEnable == 0)
            {
                printf ("preamble: %d, network mode: %d\n", sta->preamble, sta->networkMode);
                printf ("probing rate: %s\n", rateControlGetRate (sta->hwratetbl[spinfo->tmp_rate_idx]));
                printf ("available rates: %04x, rate mask: %04x\n", sta->curr_rates, defaultpideRateMsk);
                printf ("Features: %08x\n", rcFeature);
                if ((RC_IS_FEATURED(RC_FEAT_REPORTSAMPLE)))
                     printf("    report then sample\n");
                if ((RC_IS_FEATURED(RC_FEAT_RESEND_FAILREPORT)))
                     printf("    resent fail tx report\n");
                if (g_prb_pkt_enable == 1)
                     printf("    pre-allocated probe frames\n");
                printf ("continued probe up window = %d\n", spinfo->ConPrbUpSuc);
                printf ("Up Per:%d, Down PER:%d, Up PER for fastest b mode: %d\n", rc_up_pf, rc_down_pf, rc_up_pf_fastest_b);
                printf ("last sample time: %8d us, last report time: %8d us\n",
                        dbg_get_elapsed (&spinfo->last_sample), dbg_get_elapsed (&spinfo->last_report));
            }

            printf("AMPDU TX:%d Token:%d\n", sta->ampdu_tx, sta->ampdu_token);

        }
        else if (mode == RCINFO_DBG_SHOW_TBL)
        {
            int i = 0;
            if (pideFixRateEnable == 0)
            {
                printf ("------------STA %d RC table info------------\n", staIndex);
                dbg_get_time (&now);
                printf ("Last sample time: %8d us, last report time: %8d us\n",
                        dbg_get_elapsed (&spinfo->last_sample), dbg_get_elapsed (&spinfo->last_report));
                printf ("------------STA %d RC rate info------------\n", staIndex);
                printf ("sel|rate info|predict TP|curr att|curr suc|all  att|all suc\n");
                for (i = (SSV6200_SUPPORT_RATE_NUMBER - 1); i >= 0; i--)
                {
                    pidrate = &sta->pinfo.rinfo[i];
                    if (i == spinfo->txrate_idx)
                        printf (" * |");
                    else if (i == spinfo->tmp_rate_idx)
                        printf (" **|");
                    else
                        printf ("   |");
                    printf ("%9s|%10d|%8d|%8d|%8d|%8d\n", rateControlGetRate (sta->hwratetbl[i]), pidrate->throughput
                            , pidrate->this_attempt, pidrate->this_success, pidrate->attempt, pidrate->success);
                }
            }
            else
                printf ("Not available for fix rate\n");
        }
        else
            printf ("no mode %d\n", mode);
    }
    else
    {
        u8 param = 0;
        if(mode == RCINFO_DBG_GENMSG)
            param = DBG_LV_GENERAL;
        else if(mode == RCINFO_DBG_SAMPLEMSG)
            param = DBG_LV_SAMPLE;

        rcdbg = (rcdbg&param)?(rcdbg&(~param)):(rcdbg|param);
        printf ("RC debug control is %x\n",rcdbg);
    }

}

void rate_control_set_feature(u32 feature, bool IsActive)
{
    if(IsActive == TRUE)
        RC_FEATURE(feature);
    else
        RC_NOT_FEATURE(feature);
}

//Api for turn on/off the features
void rate_control_set_variables (u32 mode, u32 param)
{
    if(mode == RC_OPT_SET_FEATURE)
    {
        bool IsActive = TRUE;
        if(param > RC_FEAT_RESEND_FAILREPORT)
        {
            printf("Invalid param\n");
            return;
        }
        if (RC_IS_FEATURED(param))
            IsActive = FALSE;

        rate_control_set_feature(param, IsActive);
        printf("Current feature: %x\n", rcFeature);
    }
    else if(mode == RC_OPT_SET_PER)
    {
        if (param != 0 )
        {
            rc_down_pf = (u8)(param&0xff);
            rc_up_pf = (u8)((param>>16)&0xff);
        }
        else
        {
            rc_down_pf = RC_DEFAULT_DOWN_PF;
            rc_up_pf = RC_DEFAULT_UP_PF;
        }

        printf("(up_pf,down_pf) = (%d, %d)\n", rc_up_pf, rc_down_pf);
    }
    else if(mode == RC_OPT_SET_RPTINT)
    {
        rcreportIntval = param;
        printf("rcreportIntval (%d)us\n", rcreportIntval);
    }
    else if(mode == RC_OPT_SET_SAMPLEINT)
    {
        rcsampleIntval = param;
        printf("rcsampleIntval (%d)us\n", rcsampleIntval);
    }
    else if(mode == RC_OPT_ENA_PREALLOCATED_PRB_FRM)
    {
        if (g_prb_pkt_enable == param)
            return ;

        g_prb_pkt_enable = param;

        if (g_prb_pkt_enable == 1)
            _process_prb_pkt(TRUE);
        else
            _process_prb_pkt(FALSE);

        printf("g_prb_pkt_enable is %d\n", g_prb_pkt_enable);
    }
    else if (mode == RC_OPT_SET_UP_PER_FASTEST_B)
    {
        rc_up_pf_fastest_b = param;
        printf("rc_up_pf_fastest_b is %d\n", rc_up_pf_fastest_b);
    }
    else if (mode == RC_OPT_DIRECT_DWGRADE_RATE)
    {
        if(param==1)
            force_up_down= TRUE;
        else
            force_up_down= FALSE;
        
        printf("force_up_down:%d\n",force_up_down);
    }
    else if (mode == RC_OPT_FORCE_RTS_CTS)
    {
        if(param==1)
            g_force_rtscts= TRUE;
        else
            g_force_rtscts= FALSE;
        
        printf("g_force_rtscts:%d\n",g_force_rtscts);
    }
    else if (mode ==RC_OPT_DRATE_ENDIAN)
    {
        if(param==1)
            g_drate_endian= TRUE;
        else
            g_drate_endian= FALSE;
        
        printf("g_drate_endian:%d\n",g_drate_endian);        
    }
}

/* Update rate that CONNECT STA uses, and use the rate to send data frame
 * @wsid: wsid of target sta
 * @hw_idx: the hardware index of the rate that is used for data frame
 */
void rate_control_update_drate_by_hwidx(u8 wsid, u8 hw_idx)
{
    SSV6XXX_STA *sta;
    struct rc_pid_sta_info *spinfo;

    sta=&g6200StaInfo[wsid];
    if(sta->valid == false)
        return;

    spinfo = &sta->spinfo;
    // If not select one rate, follow the rate from hw_idx
    // Use report interval to eliminate the effort of updating rate
#if (N_MODE_AMPDU_TX == 1)
    if ((sta->pinfo.rate_sum > 1) && (dbg_get_elapsed(&spinfo->last_report) > rcreportIntval) && ((sta->ampdu_tx == FALSE) || (AMPDU_TX_ENABLE == FALSE)))
#else
    if ((sta->pinfo.rate_sum > 1) && (dbg_get_elapsed(&spinfo->last_report) > rcreportIntval) && (sta->ampdu_tx == FALSE))
#endif
    {
        if ( (sta->preamble == PHY_MODE_LONG_PREAMBLE) && ((hw_idx > 3) && (hw_idx < 7)))
        {
            hw_idx -= 3;
            if(rcdbg & DBG_LV_GENERAL)
                printf("[RC]:shifting for short preamble\n");
        }

        if(hw_idx == 0)
            spinfo->txrate_idx = 0;
        else if(hw_idx < 7) //if the rate are b mode rates
            spinfo->txrate_idx = hw_idx - sta->hwratetbl[1] + 1;
        else if(hw_idx <= sta->hwratetbl[(SSV6200_SUPPORT_RATE_NUMBER-1)])
            spinfo->txrate_idx = hw_idx - sta->hwratetbl[4] + 4;
        else
        {
            if(rcdbg & DBG_LV_GENERAL)
                printf("[RC]: Warning!! No match rate from STA\n");
        }

        dbg_get_time(&spinfo->last_report);
    }
}

#if (N_MODE_AMPDU_TX == 1)
/*Copy the multi rate table to packet, drv_ampdu references the multi rate table
 * to do retry at specified rates.
 * @p_sta: target sta
 * @pTxInfo: The tx AMPDU frame
 * @IsProbePkt: If the frame is probe frame or not
 */
bool rate_control_pid_get_multi_rates(void *p_sta, void *pTxInfo, bool IsProbePkt)
{
    struct rc_pid_sta_info *spinfo;
    struct mr_params *pTryCnt = NULL, *src_mr_tbl = NULL;

    spinfo = &((SSV6XXX_STA *)p_sta)->spinfo;
    if (IsProbePkt != TRUE)
        src_mr_tbl = &spinfo->mr_cur_trycnt;
    else
        src_mr_tbl = &spinfo->mr_prb_trycnt;
    if ((src_mr_tbl->mr_rate_num < 1) || (src_mr_tbl->mr_retry_max ==0))
    {
        printf("[RC]: Buggy multi rate table for %s rate\n", (IsProbePkt != TRUE)?"current":"probing");
        return FALSE;
    }
    //PhyInfo is not used at Cabrio, use last 8 bytes of PhyInfo to store multi rate parameters
    pTryCnt = (struct mr_params *)((u32)pTxInfo + sizeof(PKT_TxInfo) - MR_PARAM_LEN);
    memcpy(pTryCnt, src_mr_tbl, MR_PARAM_LEN);

    return TRUE;
}
#endif

bool rate_control_is_resend_failreport(void *pTxInfo)
{
#if (N_MODE_AMPDU_TX == 1)
    u8 i;
    for(i = 0; i<MAXPROBES; i++)
    {
        if (probe_pkt[i] == pTxInfo)
            return FALSE;
    }
#endif
    if (RC_IS_FEATURED(RC_FEAT_RESEND_FAILREPORT))
        return TRUE;
    else
        return FALSE;
}

#if (N_MODE_AMPDU_TX == 1)
/* Return the preallocate probe frame in order to reuse them
 * @pTxInfo: The being sent probe frame
 */
void rate_control_pid_return_probe_frame(void *pTxInfo)
{
    u8 i;

    if ((prb_pkt_st != ((1<<MAXPROBES) - 1)) && (g_prb_pkt_enable == 1))
    {
        for(i = 0; i<MAXPROBES; i++)
        {
            if (probe_pkt[i] == pTxInfo)
            {
                prb_pkt_st |= (BIT(i));
                return;
            }
        }
    }

    if(rcdbg & DBG_LV_GENERAL)
        printf("[RC]: return pkt %08x doesn't match, prb_pkt_st = %x\n", pTxInfo, prb_pkt_st);

    PBUF_MFree((void *)pTxInfo);
}

// Judge if the pre-allocated probe frames are needed
// They are needed for
//  1. Been enable pre-allocated probe frames feature
//  2. Current rate is not n mode rate
//  3. Need to send probe frames out
bool rate_control_pid_is_probe_needed(u8 wsid)
{
    SSV6XXX_STA *sta;
    struct rc_pid_sta_info *spinfo;
    bool ret = FALSE;

    if ((wsid > 0) || (g_prb_pkt_enable == 0) || (prb_pkt_inited == FALSE))
        return ret;

    sta = &g6200StaInfo[wsid];
    spinfo = &sta->spinfo;

    //Don't need to do that if AMPUD enable and current rate is one of n mode rates
    if ((sta->ampdu_tx == TRUE) && (AMPDU_TX_ENABLE == 1) &&
        (ssv6200_phy_rate[sta->hwratetbl[spinfo->txrate_idx]].type >= WLAN_RC_PHY_HT_MF))
        return ret;

    if ((spinfo->monitoring == 1) && (spinfo->probe_report_flag == 1) && (spinfo->probe_cnt > 0))
        ret = TRUE;

    return ret;
}

extern void EDCA_FillRelatedDuration(PKT_TxInfo *pTxInfo, u16 fc);
extern void ampdu_reform_mpdu_to_apmdu(PKT_TxInfo *pPktInfo, u16 fc);
extern void ampdu_reform_apmdu_to_mpdu(PKT_TxInfo *pPktInfo);
extern bool stainfo_is_ampdu_ready(u8 wsid);

/* Generate probe frame and setting up the related values of the pre-allocated probe frames
 * @wsid: wsid of  target sta
 */
void *rate_control_pid_prb_frame_gen(u8 wsid)
{
    PKT_TxInfo *prbpkt = NULL;
    u16 fc;
    u8 i;
    s32 seqno;

    if (wsid > 0)
        return NULL;

    if (prb_pkt_st == 0)
        return NULL;

    if (g_prb_pkt_enable == 0)
        return NULL;

    for(i = 0; i < MAXPROBES; i++)
    {
        if ((prb_pkt_st &BIT(i)))
        {
            prbpkt = (PKT_TxInfo *)probe_pkt[i];
            prb_pkt_st &= ~BIT(i);
            break;
        }
    }

    if(prbpkt == NULL)
    {
        printf("[RC]: Preallocated probe frame is not available \n");
        return NULL;
    }

    fc = GET_HDR80211_FC(prbpkt);
    seqno = GET_HDR80211_SC_SEQNUM(prbpkt);

    if (rcdbg & DBG_LV_SAMPLE)
        printf("[RC]: gen probe frame seq = %d for %08x\n", seqno, prbpkt);

    prbpkt->RSVD_0 = 0;
    prbpkt->RSVD_3 = 0;
    prbpkt->unicast = 1;
    prbpkt->reason = 0;
    prbpkt->TxF_ID = 0x10;
    //Still get rate here and the probing procedure is complete
    prbpkt->drate_idx = rate_control_pid_get_rate((void*)prbpkt, FALSE, seqno);

    //Need to use NON-OFDM rates to send protect frame, so far using 1M for sending protect frame
    if ((prbpkt->do_rts_cts != 0) && (rate_control_get_erp_protect() == FALSE))
        prbpkt->crate_idx= ssv6200_phy_rate[prbpkt->drate_idx].ackIndex;
    else
        prbpkt->crate_idx = 0;

    if(prbpkt->qos==1)
    {
        u16 QC=GET_HDR80211_QC(prbpkt);
        QC&=(~0x60);//clear bit5-6
        QC |= 0x02; //Modify probe packet has wrong qos tid
        SET_HDR80211_QC(prbpkt,QC);
    }

    //Reform MPDU to AMPDU or inverse, depends on rate of packet and the format of the packet
    if((prbpkt->aggregation == 0) && (ssv6200_phy_rate[prbpkt->drate_idx].type >= WLAN_RC_PHY_HT_MF)/* && (stainfo_is_ampdu_ready(wsid) == TRUE)*/)
    {
        ampdu_reform_mpdu_to_apmdu(prbpkt, fc);
        if (rcdbg & DBG_LV_SAMPLE)
            printf("[RC]: reform to AMPDU\n");
    }
    else if ((prbpkt->aggregation == 1) && ((ssv6200_phy_rate[prbpkt->drate_idx].type < WLAN_RC_PHY_HT_MF) /*|| (stainfo_is_ampdu_ready(wsid) == FALSE)*/))
    {
        ampdu_reform_apmdu_to_mpdu(prbpkt);
        prbpkt->tx_report = 1; // Keep the frame
        if (rcdbg & DBG_LV_SAMPLE)
            printf("[RC]: reform to MPDU\n");
    }

    //Additional checking due to this value may be modify after report
    if (prbpkt->aggregation == 1)
        prbpkt->ack_policy = 1;
    else
        prbpkt->ack_policy = 0;


    if(prbpkt->RSVD_0 != RC_PROBE)
    {
        prbpkt->RSVD_0 = RC_PROBE;
        if (rcdbg & DBG_LV_SAMPLE)
            printf("[RC]: probe pkt is not label as probe frame\n");
    }

    if(prbpkt->tx_report == 0)
    {
        prbpkt->tx_report = 1;
        if (rcdbg & DBG_LV_SAMPLE)
            printf("[RC]: probe pkt is not report\n");
    }

    EDCA_FillRelatedDuration(prbpkt, fc);
    //printf("[RC]: Popup %08x, seq:%d\n", prbpkt, seqno);
    return (void*)prbpkt;
}

// Check if current rate is n mode rate
bool rate_control_pid_is_nmode_rate(u8 wsid)
{
    SSV6XXX_STA *sta;
    struct rc_pid_sta_info *spinfo;

    if (wsid >= WLAN_MAX_STA)
        return FALSE;

    sta = &g6200StaInfo[wsid];
    spinfo = &sta->spinfo;
    if((ssv6200_phy_rate[sta->hwratetbl[spinfo->txrate_idx]].type >= WLAN_RC_PHY_HT_MF))
        return TRUE;

    return FALSE;
}
#endif
