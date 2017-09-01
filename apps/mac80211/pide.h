/*
 * Copyright 2007, Mattias Nissler <mattias.nissler@gmx.de>
 * Copyright 2007, Stefano Brivio <stefano.brivio@polimi.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef RC80211_PID_H
#define RC80211_PID_H

#include <common.h>
#include <cmd_def.h>

#define DEFAULT_RATE_HW_VALUE			0x00

#define IEEE80211_NO_TYPE				0
#define IEEE80211_PREAMBLE_MODE			1
#define IEEE80211_NETWORK_MODE			2
//Base on PHY design
#define HW_PREABBLE_OFFSET				3
#define HW_HT_OFFSET					8

#define SSV6200_SUPPORT_RATE_NUMBER		12
#define SSV6200_SUPPORT_BMODE_RATE_NUMBER       4
#define PHY_SUPPORT_RATE_NUMBER			39


/* If enable PROBE please enable NO_REDUCTION */
#define PROBE
#define NO_REDUCTION
//Define for rate control enhancement
#define RC_ENHANCE
//Allowance for greenfield
//#define GREENFIELD_ENABLE


#define MAXPROBES 3
#define TDIFS 34
#define TSLOT 9

#define RATE_CONTROL_PARAMETER_DEBUG

struct mr_params
{
    u8 mr_tbl[8];       // cumulative retry count from highest n mode rate to lowest n mode rate, first element for MCS7 
    u8 mr_rate_num;     // How many rates use in multi-rate table    
    u8 mr_retry_max;    // Max retry count for whole multi-rate
};

#define MR_PARAM_LEN sizeof(struct mr_params)

struct rc_pid_sta_info {
    Time_T last_sample; //last time of sampling 
    Time_T last_report; //last time of reporting 

    u16 tx_num_failed;
    u16 tx_num_xmit;

    u8 txrate_idx;      //current rate index 

    /* Average failed frames percentage error (i.e. actual vs. target
     * percentage), scaled by RC_PID_SMOOTHING. This value is computed
     * using using an exponential weighted average technique:
     *
     *           (RC_PID_SMOOTHING - 1) * err_avg_old + err
     * err_avg = ------------------------------------------
     *                       RC_PID_SMOOTHING
     *
     * where err_avg is the new approximation, err_avg_old the previous one
     * and err is the error w.r.t. to the current failed frames percentage
     * sample. Note that the bigger RC_PID_SMOOTHING the more weight is
     * given to the previous estimate, resulting in smoother behavior (i.e.
     * corresponding to a longer integration window).
     *
     * For computation, we actually don't use the above formula, but this
     * one:
     *
     * err_avg_scaled = err_avg_old_scaled - err_avg_old + err
     *
     * where:
     * 	err_avg_scaled = err * RC_PID_SMOOTHING
     * 	err_avg_old_scaled = err_avg_old * RC_PID_SMOOTHING
     *
     * This avoids floating point numbers and the per_failed_old value can
     * easily be obtained by shifting per_failed_old_scaled right by
     * RC_PID_SMOOTHING_SHIFT.
     */
//	s32 err_avg_sc;

    /* Last framed failes percentage sample. */
    u8 last_pf; //Last PER 

    /* Sharpening needed. */
    //u8 sharp_cnt;
    u8 probe_report_flag;   //probe flag for using probe rate
#ifdef PROBE
    int last_dlr;           //(100 - last_pf)
    //int fail_probes;

    u8 feedback_probes;     // received probe frame counts
    u8 monitoring;          // Start the probing procedure
    u8 tmp_rate_idx;        // probe rate index
    u8 probe_cnt;           // probe frame counts
#endif
    u8 agedpf;              // aged PER
    u16 ConPrbUpSuc;        // Probe up limitation
    u16 ConPrbUpCnt;        // Probe up counts if agedpf < upgrade PER
//#if (N_MODE_AMPDU_TX == 1)  
    //struct rates_param multi_rate;
    struct mr_params mr_cur_trycnt;   //Multi rate table for current rate
    struct mr_params mr_prb_trycnt;   //Multi rate table for current rate
//#endif
    //int n_rates;
};

/* Algorithm parameters. We keep them on a per-algorithm approach, so they can
 * be tuned individually for each interface.
 */
struct rc_pid_rateinfo {
	/* Did we do any measurement on this rate? */
	//bool valid;

	/* Comparison with the lowest rate. */
//	s32 diff;

	/* Map sorted rates to rates in ieee80211_hw_mode. */
	u8 index;
	/* Map rates in ieee80211_hw_mode to sorted rates. */
	u8 rev_index;

	//int bitrate;
#ifdef PROBE
	u16 perfect_tx_time;
	u32 throughput;     //estimate from perfect tx time and PER
#endif
#ifdef NO_REDUCTION
	u16 this_attempt;
	u16 this_success;
	u16 this_fail;
#endif
	u32 attempt;
	u32 success;
	u32 fail;
};

struct rc_pid_info {

	/* The failed frames percentage target. */
	u8 target;

	/* Rate at which failed frames percentage is sampled in 0.001s. */
	//u8 sampling_period;
#if 0
	/* P, I and D coefficients. */
	u8 coeff_p;
	u8 coeff_i;
	u8 coeff_d;

	/* Exponential averaging shift. */
	u8 smoothing_shift;

	/* Sharpening factor and duration. */
	u8 sharpen_factor;
	u8 sharpen_duration;

	/* Normalization offset. */
	u8 norm_offset;
#endif
	/* Index of the last used rate. */
	u8 oldrate;

	/* Rates information. */
	struct rc_pid_rateinfo rinfo[SSV6200_SUPPORT_RATE_NUMBER];
        u8 rate_sum;        //number of available rates
};

/**
 * struct ieee80211_rate - bitrate definition
 *
 * This structure describes a bitrate that an 802.11 PHY can
 * operate with. The two values @hw_value and @init_hw_value
 * are only for driver use when pointers to this structure are
 * passed around.
 *
 * @flags: rate-specific flags
 * @init_hw_value: driver/hardware value for this rate
 *
 */
struct ssv62xx_rate {
	u8 flags:2;
	u8 init_hw_value:6;
};

/**
 * struct ieee80211_supported_band - frequency band definition
 *
 * This structure describes a frequency band a wiphy
 * is able to operate in.
 *
 * @channels: Array of channels the hardware can operate in
 *	in this band.
 * @band: the band this structure represents
 * @n_channels: Number of channels in @channels
 * @bitrates: Array of bitrates the hardware can operate with
 *	in this band. Must be sorted to give a valid "supported
 *	rates" IE, i.e. CCK rates first, then OFDM.
 * @n_bitrates: Number of bitrates in @bitrates
 * @ht_cap: HT capabilities in this band
 */
struct ssv62xx_supported_band {
	//struct ieee80211_channel *channels;
	struct ssv62xx_rate *bitrates;
	//Only support 2GHz
	//enum ieee80211_band band;
	//int n_channels;
	//int n_bitrates;
	//struct ieee80211_sta_ht_cap ht_cap;
};
#if 0
/**
 * struct ieee80211_tx_rate_control - rate control information for/from RC algo
 *
 * @hw: The hardware the algorithm is invoked for.
 * @sband: The band this frame is being transmitted on.
 * @bss_conf: the current BSS configuration
 * @reported_rate: The rate control algorithm can fill this in to indicate
 *	which rate should be reported to userspace as the current rate and
 *	used for rate calculations in the mesh network.
 * @rts: whether RTS will be used for this frame because it is longer than the
 *	RTS threshold
 * @short_preamble: whether mac80211 will request short-preamble transmission
 *	if the selected rate supports it
 * @max_rate_idx: user-requested maximum rate (not MCS for now)
 *	(deprecated; this will be removed once drivers get updated to use
 *	rate_idx_mask)
 * @rate_idx_mask: user-requested rate mask (not MCS for now)
 * @skb: the skb that will be transmitted, the control information in it needs
 *	to be filled in
 * @bss: whether this frame is sent out in AP or IBSS mode
 */
struct ieee80211_tx_rate_control {
	//struct ieee80211_hw *hw;
	//struct ieee80211_supported_band *sband;
	//struct ieee80211_bss_conf *bss_conf;
	//struct sk_buff *skb;
	//struct ieee80211_tx_rate reported_rate;
	//bool rts, short_preamble;
	u8 max_rate_idx;
	u32 rate_idx_mask;
	bool bss;
};
#endif

//void rate_control_pid_tx_status(DevInfo *sta);
//u8 rate_control_pid_get_rate(DevInfo *sta, u8 unicast, u8 management);
//void rate_control_pid_rate_init(DevInfo *sta);

// for different modulation
enum WLAN_RC_PHY_TYPE {
	WLAN_RC_PHY_CCK=0,
	WLAN_RC_PHY_OFDM,
	WLAN_RC_PHY_HT_MF,
	WLAN_RC_PHY_HT_GF
};


#define PHY_MODE_LONG_PREAMBLE		0
#define PHY_MODE_SHORT_PREAMBLE		1
#define PHY_MODE_LONG_GI			0
#define PHY_MODE_SHORT_GI			1
#define PHY_MODE_GREENFIELD			2
#define PHY_MODE_G_ONLY				3

// for debug messages
enum RCINFO_DBG {
    RCINFO_DBG_SHOW_ST=0,           //Show statistics of specific sta
    RCINFO_DBG_SHOW_TBL,            //Show rate table of specific sta
    RCINFO_DBG_GENMSG,         //Show general debug messages of rate control
    RCINFO_DBG_SAMPLEMSG       //Show sampling debug messages of rate control
};

// The feature option for rate control
enum RC_FEAT {
    RC_FEAT_REPORTSAMPLE,       //Sample once report occurs
    RC_FEAT_RESEND_FAILREPORT,  //Resend report if it is failed to be sent    
};

// Setting properties on rate control
enum RC_OPT {
    RC_OPT_SET_FEATURE,                 //Set feature values at above enum
    RC_OPT_SET_PER,                     //Set upgrade PER and downgrade PER
    RC_OPT_SET_RPTINT,                  //Setting report interval, not exclusive with RC_FUNC_REPORTSAMPLE
    RC_OPT_SET_SAMPLEINT,               //Setting sample interval, not exclusive with RC_FUNC_REPORTSAMPLE
    RC_OPT_ENA_PREALLOCATED_PRB_FRM,    //Enable preallocate probe frame
    RC_OPT_SET_UP_PER_FASTEST_B,        //Enable preallocate probe frame
    RC_OPT_DIRECT_DWGRADE_RATE,         //En/Disable direct downgarde rate feature
    RC_OPT_FORCE_RTS_CTS,               //En/Disable force rts cts feature
    RC_OPT_DRATE_ENDIAN,                //Set the default data rate from the lowest index of rate mask or highest index
    RC_OPT_TEST
};

#define RC_DEFAULT_UP_PF 10
#define RC_DEFAULT_DOWN_PF 27

// info for rate 
struct phy_rate_info {
    u16 bitrate:7;      // values in IE
	u16 type:2;     // modulation
	u16 flag:1;     // for long/short preamble or long short GI
	u16 ackIndex:6; // ack rate hareware index 
    u16 hkbps;          // Speed in hkbps, It is need to calculate perfect tx time
};


extern struct phy_rate_info ssv6200_phy_rate[PHY_SUPPORT_RATE_NUMBER];
u8 rate_control_pid_get_rate(void *pInfo, u8 management, u32 seqno);
void rate_control_pid_sta_disconnect(u8 staIndex);
void rate_control_pid_sta_connect(u8 staIndex, struct cfg_set_sta *sta_info);
s32 rate_control_pid_init();
void rate_control_pid_deinit();
void rate_control_fix_rate(u32 rate);
void rate_control_jump_rate(bool enable);
void rate_control_pid_tx_status(u8 wsid, u8 hw_rates_idx, u16 frameTx, u16 frameTotal, u16 frameSuccess, u8 pkt_complete);
void rate_control_set_erp_protect(bool param);
bool rate_control_get_erp_protect();
void rate_control_set_preamble(u8 wsid, u8 preamble);
bool rate_control_is_resend_failreport(void *pTxInfo);
#if (N_MODE_AMPDU_TX == 1) 
bool rate_control_pid_get_multi_rates(void *p_sta, void *pTxInfo, bool IsProbePkt);
void rate_control_pid_return_probe_frame(void *pTxInfo);
bool rate_control_pid_is_probe_needed(u8 wsid);
#endif

#define DBG_LV_GENERAL      (1<<0)
#define DBG_LV_SAMPLE       (1<<1)
#define DBG_LV_MSK          (0x03)

//#define RATE_CONTROL_DEBUG
//#define RATE_CONTROL_PERFORMANCE_MONITOR
//#define PIDE_TEST
#define RC_SAMPLE 0x1
#define RC_RESEND 0x2
#define RC_PROBE 0x3

#endif /* RC80211_PID_H */
