#ifndef _MLME_H_
#define _MLME_H_

#include <common.h>
#include <cmd_def.h>
#include <log.h>
#include <ssv_pktdef.h>

#define MLME_JOIN_MAX_TRIES                    3

/**
 *  Define MLME error numbers:
 */
#define MLME_OK                                  0
#define MLME_FAILED                             -1
#define MLME_TIMEOUT                            -2



/**
 *  Define constants value for InfoFlags of DevInfo &
 *  WLAN_BSSInfo structure.
 */
#define INFO_FLAG_VALID                         1
#define INFO_FLAG_STAINFO                       2
#define INFO_FLAG_BSSINFO                       4
#define INFO_FLAG_HWTBL                         8



/**
 *  MLME Timer Interfaces:
 */
#define MTIMER_MSG_XXX                          0

typedef struct MLME_Timer_st
{
    msgevt_type     MsgType;
    u32             MsgSubType;
    OsTimer         MTimer;
    u32             MTData0;
    u32             MTData1;

} MLME_Timer, *PMLME_Timer;

#define MLME_INIT_TIMER(MT)                     \
{                                               \
    s32 res;                                    \
    res = OS_TimerCreate(&((MT)->MTimer), 100,  \
    false, NULL, mlme_timer_handler);            \
    ASSERT(res == OS_SUCCESS);                  \
}
#define MLME_START_TIMER(MT, ms, type, d0, d1)  \
{                                               \
    (MT)->MsgType    = MEVT_SOFT_TIMER;         \
    (MT)->MsgSubType = (type);                  \
    (MT)->MTData0    = (u32)(d0);               \
    (MT)->MTData1    = (u32)(d1);               \
    OS_TimerSet((MT)->MTimer, ms, false, (MT)); \
    OS_TimerStart((MT)->MTimer);                \
}
#define MLME_STOP_TIMER(MT) OS_TimerStop((MT))




/**
 *  work_type of Mlme_WorkJobReq struct - work_type description
 *
 *  @ MLME_WORK_SCAN: Do Active/Passive Scan
 *  @ MLME_WORK_AUTH: Do authentication
 *  @ MLME_WORK_ASSOC: Do association
 *  @ MLME_WORK_DIRECT_PROBE:
 *  @ MLME_WORK_JOIN:
 *  @ MLME_WORK_DEAUTH:
 *  @ MLME_WORK_DISASSOC:
 *  @
 */

enum MLME_WORK{
    MLME_WORK_START_SCANNING=0x00,
    MLME_WORK_CHANNEL_SETING,
    MLME_WORK_PROBE_REQ,
    MLME_WORK_PROBE_RESP,
    MLME_WORK_BEACON_LISTEN,
    MLME_WORK_END_SCANNING,
    MLME_WORK_JOIN_ACTIVITY,
    MLME_WORK_JOIN_TX_AUTH_1,
    MLME_WORK_JOIN_RX_AUTH_2,
    MLME_WORK_JOIN_TX_AUTH_3,
    MLME_WORK_JOIN_RX_AUTH_4,
    MLME_WORK_JOIN_ASSOC_REQ,
    MLME_WORK_JOIN_ASSOC_RESP,
    MLME_WORK_JOIN_SUPPLICIANT,
    MLME_WORK_JOIN_RESULT,
    MLME_WORK_JOIN_AUTH_FAIL,
    MLME_WORK_JOIN_ASSOC_FAIL,
    MLME_WORK_JOIN_4WAY_FAIL,
    MLME_WORK_LEAVE ,
#ifdef AIRKISS_ENABLE
    MLME_WORK_SMART_CONFIG_INIT,
    MLME_WORK_SMART_CONFIG_CHANNEL_SETTING,
    MLME_WORK_SMART_CONFIG_LOCK,
    MLME_WORK_SMART_CONFIG_COMPLETE,
    MLME_WORK_SMART_CONFIG_PROBE_REQ,
    MLME_WORK_SMART_CONFIG_PROBE_RSP,
    MLME_WORK_SMART_CONFIG_DONE,
#endif
};

enum POWER_STATE{
    PWR_WAKE,
    PWR_BCN_SYNC,
    PWR_BCN_SYNC_TIM,
    PWR_DOZE,
    PWR_SLEEP,
    PWR_WAIT_HOST_WAKEUP,
};

enum PS_EVT_TPY{
    PS_EVT_BCN_SYNC_HTMR        , // 30 ms
    PS_EVT_BCN_SYNC_STMR        , // 3 second
    PS_EVT_WOW                  ,
    PS_EVT_HOST_WAKEUP_TIMOUT   ,
};

enum PS_PHY_TPY{
    PS_PHY_B,
    PS_PHY_BGN,
};

#define IE_ID_FIELD_LEN	     					1
#define IE_LEN_FIELD_LEN                        1


typedef struct Mlme_WorkJobReq_st
{
    HDR_HostCmd     cmd;
    u8              work_type;
    u32 			cmd_seq_no;
    union {
        /**
                * Data Structure for Scan Request (Scan State Machine). The
                * content of the data structure is copied from cfg_scan_requst
                * structure from host command.
                *
                * @ chidx: the bit number of channel_mask
                * @ staidx:
                * @ is_active: true for active scan (SM), false for passive
                *     scan (SM)
                * @ n_ssids: the number of ssid to scan
                * @ channel_mask: channel to scan
                */
        struct {
            u8      chidx;
            u8      staidx;
            bool    is_active;
            u8      n_ssids;
            u16     dwell_time;
            u16     channel_mask;
            s32     last_scan_result;
            bool    is_scaning_phase;
            u8      retry_times;
            //bool    ht_supported;
            u8      ht_cap_ie[sizeof(struct ssv_ht_cap)];
            struct cfg_80211_ssid ssids[0];
        } scan;
#ifdef AIRKISS_ENABLE
        struct {
            u16     dwell_time;
            u16     original_channel_mask;
            u16     backup_original_channel_mask;
            u16     channel_mask;
            u32     waitting_time;
            u8      chidx;
        } sconfig;
#endif
         struct {
            u8      tries;
            u8      auth_transaction;
            struct  ssv6xxx_ieee80211_bss bss;
            bool    ht_supported;
            u8      ht_cap_ie[sizeof(struct ssv_ht_cap)];
            u8      assoc_ie_len;
            u8      assoc_ie[0];
        } join ;

#if 0
        /*Data Structure for Join Request (Join State Machine).  */
        struct {
            u8      bssidx[6];
            u8      tries;
            u8      auth_transaction;
			struct cfg_80211_ssid ssids[0];
            /*u8      dat[0];*/
        } probe_auth;

        struct {
            u8      bssidx[6];
            struct cfg_80211_ssid ssids[0];
            /*u8      dat[0];*/
        } assoc;
#endif
        /**
            * Data Structure for Leave Request (Leave State Machine).
        */
        struct {
            u8      bssidx[6];
            u8      dat[0];
        } deauth;
        struct {
            u8      bssidx[6];
            u8      rsvd[10];
        } disassoc;
    } u;
} Mlme_WorkJobReq, *PMlme_WorkJobReq;


#define IEEE80211_HT_MCS_MASK_LEN		10
#define	IEEE80211_HT_AMPDU_PARM_DENSITY_SHIFT	2
/**
 * struct ieee80211_mcs_info - MCS information
 * @rx_mask: RX mask
 * @rx_highest: highest supported RX rate. If set represents
 *	the highest supported RX data rate in units of 1 Mbps.
 *	If this field is 0 this value should not be used to
 *	consider the highest RX data rate supported.
 * @tx_params: TX parameters
 */
struct ieee80211_mcs_info {
	u8 rx_mask[IEEE80211_HT_MCS_MASK_LEN];
	__le16 rx_highest;
	u8 tx_params;
	u8 reserved[3];
} ;

/**
 * struct ieee80211_ht_cap - HT capabilities
 *
 */
struct ieee80211_ht_cap {
	u16 cap_info;
	u8 ampdu_params_info;

	/* 16 bytes MCS information */
	struct ieee80211_mcs_info mcs;

	u16 extended_ht_cap_info;
	u32 tx_BF_cap_info;
	u8 antenna_selection_info;
} ;

struct ieee80211_ht_info {
	u8 control_chan;
	u8 ht_param;
	u16 operation_mode;
	u16 stbc_param;
	u8 basic_set[16];
} ;






struct queeed_probe_resp{
	u8 *next;
};

/* Parsed Information Elements */
struct ieee802_11_elems {
	const u8 *ssid;
	const u8 *supp_rates;
	const u8 *fh_params;
	const u8 *ds_params;
	const u8 *cf_params;
	const u8 *tim;
	const u8 *ibss_params;
	const u8 *challenge;
	const u8 *erp_info;
	const u8 *ext_supp_rates;
	const u8 *wpa_ie;
	const u8 *rsn_ie;
	const u8 *wmm_info; /* WMM Information or Parameter Element */
	const u8 *wmm_parameter;
	const u8 *wmm_tspec;
	const u8 *wps_ie;
	const u8 *power_cap;
	const u8 *supp_channels;
	const u8 *mdie;
	const u8 *ftie;
	const u8 *timeout_int;
	const u8 *ht_capabilities;
	const u8 *ht_operation;
	const u8 *vendor_ht_cap;
	const u8 *p2p;
	const u8 *link_id;
	const u8 *interworking;

	u8 ssid_len;
	u8 supp_rates_len;
	u8 fh_params_len;
	u8 ds_params_len;
	u8 cf_params_len;
	u8 tim_len;
	u8 ibss_params_len;
	u8 challenge_len;
	u8 erp_info_len;
	u8 ext_supp_rates_len;
	u8 wpa_ie_len;
	u8 rsn_ie_len;
	u8 wmm_info_len; /* 7 = WMM Information; 24 = WMM Parameter */
	u8 wmm_parameter_len;
	u8 wmm_tspec_len;
	u8 wps_ie_len;
	u8 power_cap_len;
	u8 supp_channels_len;
	u8 mdie_len;
	u8 ftie_len;
	u8 timeout_int_len;
	u8 ht_capabilities_len;
	u8 ht_operation_len;
	u8 vendor_ht_cap_len;
	u8 p2p_len;
	u8 interworking_len;
};

struct ieee80211_tim_ie {
	u8 dtim_count;
	u8 dtim_period;
	u8 bitmap_ctrl;
	/* variable size: 1 - 251 bytes */
	u8 virtual_map[1];
} STRUCT_PACKED;

typedef enum { ParseOK = 0, ParseUnknown = 1, ParseFailed = -1 } ParseRes;


/* Information Element IDs */
/*lint -save -e849 */
enum mlme_eid {
	WLAN_EID_SUPP_RATES=1,
	WLAN_EID_HP_PARAMS=8,
	WLAN_EID_HP_TABLE=9,
	WLAN_EID_QUIET = 40,
	WLAN_EID_QBSS_LOAD=11,
	WLAN_EID_EDCA_PARAM_SET=12,
	WLAN_EID_QOS_CAPA = 46,
        WLAN_EID_EXT_SUPP_RATES = 50,
	/*WLAN_EID_VHT_CAP=191,
	WLAN_EID_VHT_OPERATION=192,
	WLAN_EID_BSS_MAX_IDLE_PERIOD=90,
	WLAN_EID_SSID_LIST=84,*/
}; /*lint -restore */

#if 0
enum ieee80211_category {
	WLAN_CATEGORY_SPECTRUM_MGMT = 0,
	WLAN_CATEGORY_QOS = 1,
	WLAN_CATEGORY_DLS = 2,
	WLAN_CATEGORY_BACK = 3,
	WLAN_CATEGORY_PUBLIC = 4,
	WLAN_CATEGORY_HT = 7,
	WLAN_CATEGORY_SA_QUERY = 8,
	WLAN_CATEGORY_PROTECTED_DUAL_OF_ACTION = 9,
	WLAN_CATEGORY_TDLS = 12,
	WLAN_CATEGORY_MESH_ACTION = 13,
	WLAN_CATEGORY_MULTIHOP_ACTION = 14,
	WLAN_CATEGORY_SELF_PROTECTED = 15,
	WLAN_CATEGORY_WMM = 17,
	WLAN_CATEGORY_VENDOR_SPECIFIC_PROTECTED = 126,
	WLAN_CATEGORY_VENDOR_SPECIFIC = 127,
};
/* BACK action code */
enum ieee80211_back_actioncode {
	WLAN_ACTION_ADDBA_REQ = 0,
	WLAN_ACTION_ADDBA_RESP = 1,
	WLAN_ACTION_DELBA = 2,
};
#endif


#if 0

typedef struct Mlme_AuthReqParams_st
{
    u8              auth_type;
    u8              tries;              /* How many auth has been tried */
    u8              bssidx;
    u8              auth_transaction;

    u16             ie_len;
    u8              ie[0];


} Mlme_AuthReqParams, *PMlme_AuthReqParams;



typedef struct Mlme_DeauthReqParams_st
{
    u8              tmp;

} Mlme_DeauthReqParams, *PMlme_DeauthReqParams;



typedef struct Mlme_AssocReqParams_st
{
    u8              tmp;

} Mlme_AssocReqParams, *PMlme_AssocReqParams;


#endif

/* ==================== Debug Functions =================== */
#define MLME_TRACE(fmt, ...)		LOG_TRACE_M(LOG_MODULE_MLME, fmt, ##__VA_ARGS__)
#define MLME_DEBUG(fmt, ...)   		LOG_DEBUG_M(LOG_MODULE_MLME, fmt, ##__VA_ARGS__)
#define MLME_INFO(fmt, ...)   		LOG_INFO_M (LOG_MODULE_MLME, fmt, ##__VA_ARGS__)
#define MLME_WARN(fmt, ...)   		LOG_WARN_M (LOG_MODULE_MLME, fmt, ##__VA_ARGS__)
#define MLME_FAIL(fmt, ...)   		LOG_FAIL_M (LOG_MODULE_MLME, fmt, ##__VA_ARGS__)
#define MLME_ERROR(fmt, ...)   		LOG_ERROR_M(LOG_MODULE_MLME, fmt, ##__VA_ARGS__)
#define MLME_FATAL(fmt, ...)   		LOG_FATAL_M(LOG_MODULE_MLME, fmt, ##__VA_ARGS__)


#define MLME_WORKJOB_STRUCT(wj,sta) {   \
	if ((Mlme_WorkJobReq *)(sta)->MlmeReqParams == NULL) \
	{ \
	    MLME_ERROR("%s got NULL work request.\n", __FUNCTION__); \
	    return MLME_FAILED; \
	} \
    (wj) = (Mlme_WorkJobReq *)(sta)->MlmeReqParams; \
}


#define GET_BSSID(x) memcpy(x,gDevInfo->connected_sta_ptr->addr.addr,sizeof(ETHER_ADDR))

typedef s32 (*MGMT80211_RxHandler)(PKT_RxInfo **);
extern MGMT80211_RxHandler const MGMT_RxHandler[];

void mlme_timer_handler( void *args );
s32 mlme_init( void );
s32 mlme_send_probe_req(u8 *da,u8 n_ssids,const struct cfg_80211_ssid *ssid);
s32 mlme_send_assoc_req( void);
s32 mlme_send_deauth(u16 reason ,struct ETHER_ADDR_st *sa,struct ETHER_ADDR_st *da,struct ETHER_ADDR_st *bssid, bool send_event);
s32 mlme_send_auth(u16 trans, u32 *extra_data, u32 extra_data_len);
s32 mlme_send_null_frame(bool ps_on);
ParseRes mlme_parse_elems(const u8 *start, size_t len, struct ieee802_11_elems *elems);
void get_bss_info(struct ieee802_11_elems *elems,struct resp_evt_result *Scan_Resp);
void _mlme_sta_disconnect(u8 wsid);
void set_short_slot_time(const u8 *capab_info);
void set_ht_protection(const u8 *ht_operation);
void set_erp_param(const u8 *erp_info);
int ieee80211_get_rates(
				u8 *supp_rates, unsigned int supp_rates_len,
				u16 *rates, u32 *basic_rates,
				bool *have_higher_than_11mbit,
				int *min_rate, int *min_rate_index);

s32 mlme_send_addba_response(u8 *da, u16 tid, u8 dialog_token, u16 status, u16 policy, u16 agg_buf_size, u16 timeout,u16 fixed_field_length,PKT_TxInfo *TxPkt);
void mlme_addba_setting(PKT_RxInfo **_PktInfo,u32 status);


#endif /* _MLME_H_ */

