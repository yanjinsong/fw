#ifndef _STA_INFO_H_
#define _STA_INFO_H_

#include <common.h>
#include <config_ssid.h>
#include <cmd_def.h>
//for mutex
#include <rtos.h>
#include "mlme.h"
#include "pide.h"


extern OsMutex staInfoMutex;

void staInfo_mutex_init(void);
void staInfo_mutex_lock(void);
void staInfo_mutex_unlock(void);
void SetStaAction(s32 flag);
/**
 *  State Flags for State field of DevInfo structure:
 *  (Std. IEEE 802.11, 2007, Figure 11-6)
 *
 *  @ STA_STATE_UNAUTH_UNASSOC: State 1
 *  @ STA_STATE_AUTHED_UNASSOC: State 2
 *  @ STA_STATE_AUTHED_ASSOCED: State 3
 *
 */
typedef enum{
	STA_STATE_UNAUTH_UNASSOC=0,
	STA_STATE_AUTHED_UNASSOC,
	STA_STATE_AUTHED_ASSOCED,
	STA_STATE_ASSOCED_4way,
}EN_STA_STATE;

typedef enum{
	STA_ACTION_INIT=0,
	STA_ACTION_IDLE=1,
	STA_ACTION_READY=2,
	STA_ACTION_SCANING=3,
	STA_ACTION_JOING=4,
	STA_ACTION_JOING_4WAY=5,
	STA_ACTION_LEAVING=6,
}EN_STA_ACTION;


#define STA_WPA_DEFAULT_IE_LEN						255
//struct wpa_supplicant;
//struct wpa_ssid;

typedef struct SSV6XXX_STA_ST
{
	u16			supp_rates;
    u16			curr_rates;
    u16			aid;
	ETHER_ADDR	addr;
	/*
		802.11b(preamble)
			Long(1-2-5.5-11)
			Short(2-5.5-11)
		802.11g 12 rate options
			(6-9-12-18-24-36-48-54)
		802.11n rate options(Guard interval)
			Mix long	  (6.5-13-19.5-26-39-52-58.5-65)
			Mix short	  (7.2-14.4-21.7-28.9-43.3-57.8-65-72.2)
			Green long (7.2-14.4-21.7-28.9-43.3-57.8-65-72.2)

		bit 0-3	:
				1-2-5.5-11
		bit 4-11	:
				(6-9-12-18-24-36-48-54)
				(6.5-13-19.5-26-39-52-58.5-65)
				(7.2-14.4-21.7-28.9-43.3-57.8-65-72.2)
				(7.2-14.4-21.7-28.9-43.3-57.8-65-72.2)
	*/
	u16 	valid:1;
	u16 	preamble:1;
	u16 	networkMode:2;
	u16		ampdu_tx:1;

    u16     ampdu_tx_ssn;
    u8      ampdu_token;
    u8      wsid;

	struct rc_pid_info pinfo;
	struct rc_pid_sta_info spinfo;
	u16 parameter_set_count;
    u8 hwratetbl[SSV6200_SUPPORT_RATE_NUMBER];

    /*
    For STA monitor
    */
    u32 ap_heartbeat_cnt:8;
    u32 last_ap_heartbeat_cnt:8;
    u32 poke_ap:8;
    u32 RSVD0:8;   

    //Record the rates
    u16 non_ht_rates;
    u8 mcs_0;
    //Record info
    u8 ht_capa_b6b4;//record ht_capabilities b4~b6
    u8 ht_protect; //bit 8 & bit9 in ht operation it

    
    #if(ENABLE_DYNAMIC_RX_SENSITIVE==1)
    u32 prev_rcpi;
    struct ssv6xxx_rxphy_info_padding rxphypad;
    #endif

    struct cfg_80211_ssid ssid;
    u8  ht_cap_ie[sizeof(struct ssv_ht_cap)];  

    

} SSV6XXX_STA, *PSSV6XXX_STA;
struct last_deauth_info
{
    struct ETHER_ADDR_st self_mac;
    u32 counter;

};

typedef struct DevInfo_st
{
    u32                 Action;//record mlme_task state
    void                *MlmeReqParams;//for mlme
    MLME_Timer          MTimer;//for mlme
    u16                 aid;
    u8                  State;//record AUTH ASSOC State

    //power saving related
    u8      ps_state;
    u16     beacon_int;
    u8      dtim_count;
    u8      dtim_period;
    struct cfg_ps_request wow_req;
    //connected deviced related (STA/AP)
    bool erp_protect;
    bool erp_barker_preamble;
    u8 short_slot_time;

    /**
        *  User configuration parameters. This structure
        *  is shared with sta.cfg file. So to init STA, sta.cfg
        *  is directly copied to this structure.
        */
    struct cfg_sta_info info;

    u16                 tid_seq[8]; /* Only support 0~7 */

	struct wpa_supplicant 	*pSupplicant;
	struct wpa_ssid 		wpa_ssid;
	u8 wpa_ie[STA_WPA_DEFAULT_IE_LEN];
	int wpa_ie_len;
    /* MIB Counters: */
	s32 current_channel;
	u16 capability_info;
	struct ieee80211_mcs_info mcs;
	u8 ampdu_factor;
	u8 ampdu_density;
	SSV6XXX_STA    *connected_sta_ptr;
    u32 channel_edcca_count[MAX_CHANNEL_NUM];
    u32 channel_packet_count[MAX_CHANNEL_NUM];
    u32 final_channel_edcca_count[MAX_CHANNEL_NUM];
    u32 final_channel_packet_count[MAX_CHANNEL_NUM];
    struct last_deauth_info sta_deauth_info;
    OsTimer         STAMonitorTimer;
} DevInfo, *PDevInfo;




extern DevInfo *gDevInfo;



int StartStaMonitorTimer(void);
int StopStaMonitorTimer(void);

SSV6XXX_STA *STAInfo_FindSTAByAddr( const ETHER_ADDR *mac );
void STAInfo_Init( void );

SSV6XXX_STA *stainfo_add_sta();
s8 stainfo_del_sta(const ETHER_ADDR *mac);
s8 stainfo_reset_sta();
s8 stainfo_set_sec_key();//????
EN_STA_ACTION querySTAAction(void);
EN_STA_STATE querySTAState(void);
bool IsSTAActReady(void);

#endif /* _STA_INFO_H_ */

