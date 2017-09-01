#include <config.h>
#include <msgevt.h>
#include <hdr80211.h>
#include <pbuf.h>
#include <hwmac/drv_mac.h>
#include <mbox/drv_mbox.h>
#include <edca/drv_edca.h>
#include <ampdu/drv_ampdu.h>
#include <log.h>

#include "util.h"
#include "mlme.h"
#include "MLME_Task.h"
#include "MLME_Join.h"
#include "MLME_PwrSaving.h"
#include "sta_info.h"
#include "pide.h"
#include "util.h"
#include "cmd_engine.h"

#include <common.h>

#include <hw_regs_api.h>
#include <security/drv_security.h>




#include <wpa_supplicant_i.h>
#include "supplicant/utils/common.h"
#include "supplicant/common/defs.h"
#include "supplicant/common/wpa_common.h"
#include "supplicant/drivers/driver_cabrio.h"
#include "supplicant/common/ieee802_11_defs.h"
#include "psk_cache.h"
#include "cmd_def.h"

#include <dbg_timer/dbg_timer.h>
#include <beacon/drv_beacon.h>
#include <regs/ssv6200_aux.h>

#define ADR_TXQ0_MTX_Q      0xc6002104
#define MLME_BEACON_LISTENING_PERIOD 2000

#define ADD_VENDOR_IE_IN_PROBE_REQUEST 0
#define STA_MODE_WSID 0

static s32 mlme_rx_reassoc_response( PKT_RxInfo **_PktInfo );
static s32 mlme_rx_beacon( PKT_RxInfo **_PktInfo );
static s32 mlme_rx_probe_response( PKT_RxInfo **_PktInfo );
static s32 mlme_rx_atim( PKT_RxInfo **_PktInfo );
static s32 mlme_rx_disassoc( PKT_RxInfo **_PktInfo );
static s32 mlme_rx_auth( PKT_RxInfo **_PktInfo );
static s32 mlme_rx_deauth( PKT_RxInfo **_PktInfo );
static s32 mlme_rx_action( PKT_RxInfo **_PktInfo );
static s32 mlme_rx_assoc_response( PKT_RxInfo **_PktInfo );
static s32 mlme_rx_probe_request( PKT_RxInfo **_PktInfo );


#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
u8 supplicant_bypass = 0;
extern struct wpa_supplicant * wpa_supplicant_init(void);
#endif
u8 gBSSID[ETHER_ADDR_LEN]={0};
static u16 sg_mgmt_tx_seqno;
extern u32 sg_soc_evt_seq_no;
extern SSV6XXX_STA	*g6200StaInfo;

/**
 *  IEEE 802.11 Management Frame Handler:
 *
 *  @ MLME_RXFUNC_MGMT_STYPE_00: Association Request Handler (subtype=00)
 *  @ MLME_RXFUNC_MGMT_STYPE_01: Association Response Handler (subtype=01)
 *  @ MLME_RXFUNC_MGMT_STYPE_02: Reassociation Request Handler (subtype=02)
 *  @ MLME_RXFUNC_MGMT_STYPE_03: Reassociation Response Handler (subtype=03)
 *  @ MLME_RXFUNC_MGMT_STYPE_04: Probe Request Handler (subtype=04)
 *  @ MLME_RXFUNC_MGMT_STYPE_05: Probe Response Handler (subtype=05)
 *  @ MLME_RXFUNC_MGMT_STYPE_06: Reserved (subtype=06)
 *  @ MLME_RXFUNC_MGMT_STYPE_07: Reserved (subtype=07)
 *  @ MLME_RXFUNC_MGMT_STYPE_08: Beacon Handler (subtype=08)
 *  @ MLME_RXFUNC_MGMT_STYPE_09: ATIM Handler (subtype=09)
 *  @ MLME_RXFUNC_MGMT_STYPE_10: Disassociation Handler (subtype=10)
 *  @ MLME_RXFUNC_MGMT_STYPE_11: Authentication Handler (subtype=11)
 *  @ MLME_RXFUNC_MGMT_STYPE_12: Deauthentication Handler (subtype=12)
 *  @ MLME_RXFUNC_MGMT_STYPE_13: Action Handler (subtype=13)
 *  @ MLME_RXFUNC_MGMT_STYPE_14: Reserved (subtype=14)
 *  @ MLME_RXFUNC_MGMT_STYPE_15: Reserved (subtype=15)
 */
#define MLME_RXFUNC_MGMT_STYPE_00           (MGMT80211_RxHandler)NULL//mlme_rx_assoc_request
#define MLME_RXFUNC_MGMT_STYPE_01           mlme_rx_assoc_response
#define MLME_RXFUNC_MGMT_STYPE_02           (MGMT80211_RxHandler)NULL//mlme_rx_reassoc_request
#define MLME_RXFUNC_MGMT_STYPE_03           mlme_rx_reassoc_response
#define MLME_RXFUNC_MGMT_STYPE_04           mlme_rx_probe_request
#define MLME_RXFUNC_MGMT_STYPE_05           mlme_rx_probe_response
#define MLME_RXFUNC_MGMT_STYPE_06           (MGMT80211_RxHandler)NULL
#define MLME_RXFUNC_MGMT_STYPE_07           (MGMT80211_RxHandler)NULL
#define MLME_RXFUNC_MGMT_STYPE_08           mlme_rx_beacon
#define MLME_RXFUNC_MGMT_STYPE_09           mlme_rx_atim
#define MLME_RXFUNC_MGMT_STYPE_10           mlme_rx_disassoc
#define MLME_RXFUNC_MGMT_STYPE_11           mlme_rx_auth
#define MLME_RXFUNC_MGMT_STYPE_12           mlme_rx_deauth
#define MLME_RXFUNC_MGMT_STYPE_13           mlme_rx_action
#define MLME_RXFUNC_MGMT_STYPE_14           (MGMT80211_RxHandler)NULL
#define MLME_RXFUNC_MGMT_STYPE_15           (MGMT80211_RxHandler)NULL

typedef s32 (*MGMT80211_RxHandler)(PKT_RxInfo **);

MGMT80211_RxHandler const MGMT_RxHandler[] =
{   /*lint -save -e611 */
    /* stype=00: */ MLME_RXFUNC_MGMT_STYPE_00,
    /* stype=01: */ MLME_RXFUNC_MGMT_STYPE_01,
    /* stype=02: */ MLME_RXFUNC_MGMT_STYPE_02,
    /* stype=03: */ MLME_RXFUNC_MGMT_STYPE_03,
    /* stype=04: */ MLME_RXFUNC_MGMT_STYPE_04,
    /* stype=05: */ MLME_RXFUNC_MGMT_STYPE_05,
    /* stype=06: */ MLME_RXFUNC_MGMT_STYPE_06,
    /* stype=07: */ MLME_RXFUNC_MGMT_STYPE_07,
    /* stype=08: */ MLME_RXFUNC_MGMT_STYPE_08,
    /* stype=09: */ MLME_RXFUNC_MGMT_STYPE_09,
    /* stype=10: */ MLME_RXFUNC_MGMT_STYPE_10,
    /* stype=11: */ MLME_RXFUNC_MGMT_STYPE_11,
    /* stype=12: */ MLME_RXFUNC_MGMT_STYPE_12,
    /* stype=13: */ MLME_RXFUNC_MGMT_STYPE_13,
    /* stype=14: */ MLME_RXFUNC_MGMT_STYPE_14,
    /* stype=15: */ MLME_RXFUNC_MGMT_STYPE_15,
};  /*lint -restore */

#define IS_StaInfo_NULL(sta) { \
    if(sta==NULL)              \
        return MLME_FAILED;    \
}

//#define IEEE80211_ADDBA_PARAM_POLICY_MASK 0x0002
//#define IEEE80211_ADDBA_PARAM_TID_MASK 0x003C
//#define IEEE80211_ADDBA_PARAM_BUF_SIZE_MASK 0xFFA0
//#define IEEE80211_DELBA_PARAM_TID_MASK 0xF000
//#define IEEE80211_DELBA_PARAM_INITIATOR_MASK 0x0800

#define BLOCKACKPARAM_POLICY_POS    1
#define BLOCKACKPARAM_TID_POS		2
#define BLOCKACKPARAM_WINSIZE_POS	6
#define DELBA_TID_POS			   12
#define DELBA_INITIATOR_POS		    11

extern s32 MLME_Leave_Resp (void **_PktInfo, bool timeout, s16 reason);
extern s32 Scan_process_result(void *data, u32 data_len,struct ieee802_11_elems *elems, u32 cmd_seq_no);
extern s32 Join_process_result(s32 status_code);

#if 0
static s32 mlme_rx_assoc_request( PKT_RxInfo *PktInfo )
{

    PBUF_MFree(PktInfo);
    return MLME_OK;
}
#endif


#if 0
struct resp_evt_result* Pack_CMD_SEQ_NO(CMD_SEQ_CTRL *cmd_seq_ctrl,u32 cmd_seq_no)
{
    cmd_seq_ctrl->cmd_seq_no=cmd_seq_no;
    cmd_seq_ctrl=(CMD_SEQ_CTRL *)((u8*)cmd_seq_ctrl+sizeof(CMD_SEQ_CTRL));
    return (struct resp_evt_result *) cmd_seq_ctrl;
}
#endif

u32 ieee80211_hdrlen(u16 fc)
{
	u32 hdrlen = 24;

	if (IS_DATA_FRAME(fc)) {
		if (IS_4ADDR_FORMAT(fc))
			hdrlen = 30;
		if (IS_QOS_DATA(fc)) {
			hdrlen += IEEE80211_QOS_CTL_LEN;
			if (IS_ORDER_SET(fc))
				hdrlen += IEEE80211_HT_CTL_LEN;
		}
		goto out;
	}

	if (IS_CTRL_FRAME(fc)) {
		/*
		 * ACK and CTS are 10 bytes, all others 16. To see how
		 * to get this condition consider
		 *   subtype mask:   0b0000000011110000 (0x00F0)
		 *   ACK subtype:    0b0000000011010000 (0x00D0)
		 *   CTS subtype:    0b0000000011000000 (0x00C0)
		 *   bits that matter:         ^^^      (0x00E0)
		 *   value of those: 0b0000000011000000 (0x00C0)
		 */
		if ((fc & 0x00E0) == 0x00C0)
			hdrlen = 10;
		else
			hdrlen = 16;
	}
out:
	return hdrlen;
}

static int ieee802_11_parse_vendor_specific(const u8 *pos, size_t elen,
					    struct ieee802_11_elems *elems)
{
	unsigned int oui;

	/* first 3 bytes in vendor specific information element are the IEEE
	 * OUI of the vendor. The following byte is used a vendor specific
	 * sub-type. */
	if (elen < 4) {
		LOG_ERROR("short vendor specific information element ignored (len=%lu)\n",(unsigned long) elen);

		return -1;
	}


	oui = ((((u32) pos[0]) << 16) | (((u32) pos[1]) << 8) | ((u32) (pos)[2]));
	switch (oui) {
	case OUI_MICROSOFT:
		/* Microsoft/Wi-Fi information elements are further typed and
		 * subtyped */
		switch (pos[3]) {
		case 1:
			/* Microsoft OUI (00:50:F2) with OUI Type 1:
			 * real WPA information element */
			elems->wpa_ie = pos;
			elems->wpa_ie_len = elen;
			break;
		case WMM_OUI_TYPE:
			/* WMM information element */
			if (elen < 5) {
				LOG_ERROR( "short WMM "
					   "information element ignored "
					   "(len=%lu)\n",
					   (unsigned long) elen);
				return -1;
			}
			switch (pos[4]) {
			case WMM_OUI_SUBTYPE_INFORMATION_ELEMENT:
				elems->wmm_info = pos;
				elems->wmm_info_len = elen;
				break;
			case WMM_OUI_SUBTYPE_PARAMETER_ELEMENT:
				elems->wmm_parameter = pos;
				elems->wmm_parameter_len = elen;
				break;
			case WMM_OUI_SUBTYPE_TSPEC_ELEMENT:
				elems->wmm_tspec = pos;
				elems->wmm_tspec_len = elen;
				break;
			default:
				LOG_WARN(  "unknown WMM "
					   "information element ignored "
					   "(subtype=%d len=%lu)\n",
					   pos[4], (unsigned long) elen);
				return -1;
			}
			break;
		case 4:
			/* Wi-Fi Protected Setup (WPS) IE */
			elems->wps_ie = pos;
			elems->wps_ie_len = elen;
			break;
		default:
#if 0
			LOG_WARN(  "Unknown Microsoft "
				   "information element ignored "
				   "(type=%d len=%lu)\n",
				   pos[3], (unsigned long) elen);
#endif
			return -1;
		}
		break;

	case OUI_BROADCOM:
		switch (pos[3]) {
		case VENDOR_HT_CAPAB_OUI_TYPE:
			elems->vendor_ht_cap = pos;
			elems->vendor_ht_cap_len = elen;
			break;
		default:
			/*LOG_WARN( "Unknown Broadcom "
				   "information element ignored "
				   "(type=%d len=%lu)\n",
				   pos[3], (unsigned long) elen);*/
			return -1;
		}
		break;

	default:
#if 0
		LOG_WARN( "unknown vendor specific information "
			   "element ignored (vendor OUI %02x:%02x:%02x "
			   "len=%lu)\n",
			   pos[0], pos[1], pos[2], (unsigned long) elen);
#endif
		return -1;
	}

	return 0;
}


/**
 * mlme_parse_elems - Parse information elements in management frames
 * @start: Pointer to the start of IEs
 * @len: Length of IE buffer in octets
 * Returns: Parsing result
 */
ParseRes mlme_parse_elems(const u8 *start, size_t len,
				struct ieee802_11_elems *elems)
{
	size_t left = len;
	const u8 *pos = start;
	int unknown = 0;

	os_memset(elems, 0x00, sizeof(struct ieee802_11_elems));

	while (left >= 2) {
		u8 id, elen;

		id = *pos++;
		elen = *pos++;
		left -= 2;

		if (elen > left) {
			LOG_PRINTF( "IEEE 802.11 element "
				   "parse failed (id=%d elen=%d "
				   "left=%lu)",
				   id, elen, (unsigned long) left);

			return ParseFailed;
		}
		//LOG_PRINTF("id=%d\n",id);
		switch (id) {
		case WLAN_EID_SSID:
			elems->ssid = pos;
			elems->ssid_len = elen;
			break;
		case WLAN_EID_SUPP_RATES:
			elems->supp_rates = pos;
			elems->supp_rates_len = elen;
			break;
		case WLAN_EID_FH_PARAMS:
			elems->fh_params = pos;
			elems->fh_params_len = elen;
			break;
		case WLAN_EID_DS_PARAMS:
			elems->ds_params = pos;
			elems->ds_params_len = elen;
			break;
		case WLAN_EID_CF_PARAMS:
			elems->cf_params = pos;
			elems->cf_params_len = elen;
			break;
		case WLAN_EID_TIM:
			elems->tim = pos;
			elems->tim_len = elen;
			break;
		case WLAN_EID_IBSS_PARAMS:
			elems->ibss_params = pos;
			elems->ibss_params_len = elen;
			break;
		case WLAN_EID_CHALLENGE:
			elems->challenge = pos;
			elems->challenge_len = elen;
			break;
		case WLAN_EID_ERP_INFO:
			elems->erp_info = pos;
			elems->erp_info_len = elen;
			break;
		case WLAN_EID_EXT_SUPP_RATES:
			elems->ext_supp_rates = pos;
			elems->ext_supp_rates_len = elen;
			break;
		case WLAN_EID_VENDOR_SPECIFIC:
			if (ieee802_11_parse_vendor_specific(pos, elen,
				elems))
				unknown++;
			break;
		case WLAN_EID_RSN:
			elems->rsn_ie = pos;
			elems->rsn_ie_len = elen;
			break;
		case WLAN_EID_PWR_CAPABILITY:
			elems->power_cap = pos;
			elems->power_cap_len = elen;
			break;
		case WLAN_EID_SUPPORTED_CHANNELS:
			elems->supp_channels = pos;
			elems->supp_channels_len = elen;
			break;
		case WLAN_EID_MOBILITY_DOMAIN:
			elems->mdie = pos;
			elems->mdie_len = elen;
			break;
		case WLAN_EID_FAST_BSS_TRANSITION:
			elems->ftie = pos;
			elems->ftie_len = elen;
			break;
		case WLAN_EID_TIMEOUT_INTERVAL:
			elems->timeout_int = pos;
			elems->timeout_int_len = elen;
			break;
		case WLAN_EID_HT_CAP:
			elems->ht_capabilities = pos;
			elems->ht_capabilities_len = elen;
			break;
		case WLAN_EID_HT_OPERATION:
			elems->ht_operation = pos;
			elems->ht_operation_len = elen;
			break;
#if 0
		case WLAN_EID_VHT_CAP:
			elems->vht_capabilities = pos;
			elems->vht_capabilities_len = elen;
			break;
		case WLAN_EID_VHT_OPERATION:
			elems->vht_operation = pos;
			elems->vht_operation_len = elen;
			break;
#endif
		case WLAN_EID_LINK_ID:
			if (elen < 18)
				break;
			elems->link_id = pos;
			break;
		case WLAN_EID_INTERWORKING:
			elems->interworking = pos;
			elems->interworking_len = elen;
			break;
#if 0
		case WLAN_EID_EXT_CAPAB:
			elems->ext_capab = pos;
			elems->ext_capab_len = elen;
			break;
		case WLAN_EID_BSS_MAX_IDLE_PERIOD:
			if (elen < 3)
				break;
			elems->bss_max_idle_period = pos;
			break;

		case WLAN_EID_SSID_LIST:
			elems->ssid_list = pos;
			elems->ssid_list_len = elen;
			break;

#endif
		default:
			unknown++;
			LOG_PRINTF("IEEE 802.11 element parse "
				   "ignored unknown element (id=%d elen=%d)\n",
				   id, elen);
			break;
		}

		left -= elen;
		pos += elen;
	}

	if (left)
		return ParseFailed;

	return unknown ? ParseUnknown : ParseOK;
}

u16 supportRateParser(u8 supp_rates)
{
	switch(supp_rates)
	{
		case 2:
			return BIT(0);
		case 4:
			return BIT(1);
		case 11:
			return BIT(2);
		case 22:
			return BIT(3);
		case 12:
			return BIT(4);
		case 18:
			return BIT(5);
		case 24:
			return BIT(6);
		case 36:
			return BIT(7);
		case 48:
			return BIT(8);
		case 72:
			return BIT(9);
		case 96:
			return BIT(10);
		case 108:
			return BIT(11);
		default:
			return 0;
	}
}

void _set_short_slot_time(bool enable)
{
    u32 regval=0;

    if (enable)
    {

        regval = REG32(ADR_MTX_DUR_SIFS_G);
        regval = regval & MTX_DUR_BURST_SIFS_G_I_MSK;
        regval |= 0xa << MTX_DUR_BURST_SIFS_G_SFT;
        regval = regval & MTX_DUR_SLOT_G_I_MSK;
        regval |= 9 << MTX_DUR_SLOT_G_SFT;
        REG32(ADR_MTX_DUR_SIFS_G)=regval;

        regval = REG32(ADR_MTX_DUR_IFS);
        regval = regval & MTX_DUR_BURST_SIFS_I_MSK;
        regval |= 0xa << MTX_DUR_BURST_SIFS_SFT;
        regval = regval & MTX_DUR_SLOT_I_MSK;
        regval |= 9 << MTX_DUR_SLOT_SFT;
        REG32(ADR_MTX_DUR_IFS)=regval;
        //slottime = 9;
    }
    else
    {

        regval = REG32(ADR_MTX_DUR_SIFS_G);
        regval = regval & MTX_DUR_BURST_SIFS_G_I_MSK;
        regval |= 0xa << MTX_DUR_BURST_SIFS_G_SFT;
        regval = regval & MTX_DUR_SLOT_G_I_MSK;
        regval |= 20 << MTX_DUR_SLOT_G_SFT;
        REG32(ADR_MTX_DUR_SIFS_G)=regval;

        regval = REG32(ADR_MTX_DUR_IFS);
        regval = regval & MTX_DUR_BURST_SIFS_I_MSK;
        regval |= 0xa << MTX_DUR_BURST_SIFS_SFT;
        regval = regval & MTX_DUR_SLOT_I_MSK;
        regval |= 20 << MTX_DUR_SLOT_SFT;
        REG32(ADR_MTX_DUR_IFS)=regval;
        //slottime = 20;
    }
}

#define CAPABILITY_IE_SHORT_SLOT_TIME_MASK ((u16)BIT(10)) /*B10*/
#define POSITION_SHORT_SLOT_TIME (10) /*B10*/

//The size of capability is 16 bits, and we still use the data type "u8" on purpose,
//because we are worried about the address is not 2 bytes alignment.
void set_short_slot_time(const u8 *capab_info)
{
    u16 _capab_info=0;
    if((!capab_info)||(gDevInfo->connected_sta_ptr==NULL))
    {
        printf("error : set_short_slot_time. capab_info(0x%x) connected_sta_ptr(0x%x)\r\n",(u32)capab_info,gDevInfo->connected_sta_ptr);
        return;
    }
    _capab_info=(capab_info[0]|(capab_info[1]<<8));

    u8 old_short_slot_time = gDevInfo->short_slot_time ;
    u8 new_short_slot_time = (_capab_info& CAPABILITY_IE_SHORT_SLOT_TIME_MASK)>>POSITION_SHORT_SLOT_TIME;

    if(old_short_slot_time != new_short_slot_time)
    {

        printf("BSS_CHANGED_ERP_SLOT: use_short_slot[%d]\n", new_short_slot_time);
        _set_short_slot_time(new_short_slot_time);

        gDevInfo->short_slot_time = new_short_slot_time;
    }
}

#define HT_OPER_OP_MODE_HT_PROT_MASK ((BIT(0) | BIT(1))) /* B8..B9 */
#define ERP_IE_USE_PROTECTION_MASK ((u8)(BIT(1))) /*B1*/
#define POSITION_USE_PROTECTION (1) /*position is Bit 1*/
#define ERP_IE_BARKER_PREAMBLE_MODE_MASK ((u8)(BIT(2))) /*B2*/
#define POSITION_BARKER_PREAMBLE_MODE (2) /*position is Bit 2*/

void set_ht_protection(const u8 *ht_operation)
{
    if(!ht_operation)
    {
        printf("error : set_ht_protection \r\n");
        return;
    }

    u8 old_ht_prot = gDevInfo->connected_sta_ptr->ht_protect ;
    u8 new_ht_prot = (ht_operation[2] & HT_OPER_OP_MODE_HT_PROT_MASK);

    if(memcmp(&old_ht_prot,&new_ht_prot,sizeof(u8)))
    {
        //set HT protection
        printf("send to RC for ht protection %d\n",new_ht_prot);
        gDevInfo->connected_sta_ptr->ht_protect = new_ht_prot;
    }
}

void set_erp_param(const u8 *erp_info)
{
    if((!erp_info)||(gDevInfo->connected_sta_ptr==NULL))
    {
        printf("error : set_erp_param. erp_info(0x%x) connected_sta_ptr(0x%x)\r\n",erp_info,gDevInfo->connected_sta_ptr);
        return;
    }

    //ERP bit 1 : ERP_IE_USE_PROTECTION_MASK
    bool new_erp_protect = (erp_info[0] & ERP_IE_USE_PROTECTION_MASK)>>1 ;

    if(rate_control_get_erp_protect() != new_erp_protect)
    {
        printf("erp_protect =%d \n", new_erp_protect);
        rate_control_set_erp_protect(new_erp_protect);
    }

    //ERP bit 2 : ERP_IE_BARKER_PREAMBLE_MODE_MASK
    u8 new_barker_preamble = (erp_info[0] & ERP_IE_BARKER_PREAMBLE_MODE_MASK)>>2;

    if(gDevInfo->connected_sta_ptr->preamble != new_barker_preamble)
    {
        printf("erp_barker_preamble =%d \n",new_barker_preamble);
        rate_control_set_preamble(0, new_barker_preamble);
    }

}


int ieee80211_get_rates(
				u8 *supp_rates, unsigned int supp_rates_len,
				u16 *rates, u32 *basic_rates,
				bool *have_higher_than_11mbit,
				int *min_rate, int *min_rate_index)
{
	int i, j;
    u32 count=0;
	for (i = 0; i < supp_rates_len; i++) {
		int rate = (supp_rates[i] & 0x7f) * 5;
		bool is_basic = !!(supp_rates[i] & 0x80);

		if (rate > 110)
			*have_higher_than_11mbit = true;

		/*
		 * BSS_MEMBERSHIP_SELECTOR_HT_PHY is defined in 802.11n-2009
		 * 7.3.2.2 as a magic value instead of a rate. Hence, skip it.
		 *
		 * Note: Even through the membership selector and the basic
		 *	 rate flag share the same bit, they are not exactly
		 *	 the same.
		 */
		if (!!(supp_rates[i] & 0x80) &&
		    (supp_rates[i] & 0x7f) == BSS_MEMBERSHIP_SELECTOR_HT_PHY)
			continue;

		for (j = 0; j < sizeof(gBitRates)/sizeof(u16); j++) {
			if (gBitRates[j] == rate) {
				*rates |= BIT(j);
                count++;
				if (is_basic)
					*basic_rates |= BIT(j);
				if (rate < *min_rate) {
					*min_rate = rate;
					*min_rate_index = j;
				}
				break;
			}
		}
	}
    return count;
}

#define CAPABILITY_IE_SHORT_PREAMBLE_MASK ((u16)(BIT(5)))/*B5*/
#define SIZE_OF_SUPPORTED_MCS_SET_ELEMENT 16
#define SIZE_OF_HT_CAPABILITIES_INFO_ELEMENT 2
#define EDCA_DEFINE_NUM_OF_TX_Q 4
#define ACI_OF_BEST_EFFORT 0x00
#define POSITION_OF_ACI_IN_AC 5
#define TOTAL_BYTE_OF_EACH_AC 4
s32 get_sta_info_from_assoc_resp(const HDR80211_Mgmt *Mgmt80211,u16 pkt_len)
{
	u16 aid,status_code;
	struct ieee802_11_elems elems;
	u32 i;
    const Mlme_WorkJobReq *WorkJobReq=(Mlme_WorkJobReq *)(gDevInfo)->MlmeReqParams;
    
    
    //for rc setting
    struct cfg_set_sta ap_rc_info;
    memset(&ap_rc_info,0,sizeof(struct cfg_set_sta));
    ap_rc_info.sta_oper=CFG_STA_ADD;

	//Check Assoc_resp packet
	status_code=Mgmt80211->u.assoc_resp.status_code;

	if(!(status_code==0))
	{
		MLME_TRACE("Associate response is failure with code %04X.", status_code);
		return MLME_FAILED;
	}

    gDevInfo->connected_sta_ptr=&g6200StaInfo[0];
	aid=(Mgmt80211->u.assoc_resp.aid)^(1<<14)^(1<<15);

	gDevInfo->connected_sta_ptr->aid = gDevInfo->aid = aid;
	LOG_PRINTF("%s:gDevInfo->connected_sta_ptr->aid=%d\n",__FUNCTION__,gDevInfo->connected_sta_ptr->aid);
	memcpy(gDevInfo->connected_sta_ptr->addr.addr,Mgmt80211->bssid.addr,ETHER_ADDR_LEN);
	//Setting preamble
	gDevInfo->connected_sta_ptr->preamble=Mgmt80211->u.assoc_resp.capab_info & CAPABILITY_IE_SHORT_PREAMBLE_MASK; // change "0x20"

    //short slot time setting
    if(Mgmt80211->u.assoc_resp.capab_info)
    {
        if (Mgmt80211->u.assoc_resp.capab_info & WLAN_CAPABILITY_SHORT_PREAMBLE) {
            ap_rc_info.short_preamble=1;
        }
        else{
            ap_rc_info.short_preamble=0;
        }
        set_short_slot_time((const u8*)&Mgmt80211->u.assoc_resp.capab_info);
    }
	//Element parser
	mlme_parse_elems((u8 *) Mgmt80211->u.assoc_resp.variable,pkt_len-BASE_LEN(Mgmt80211->u.assoc_resp.variable,Mgmt80211),&elems);

	if (!elems.supp_rates)
		return MLME_FAILED;

	//Parser WLAN_EID_SUPP_RATES
	gDevInfo->connected_sta_ptr->supp_rates=0;
	gDevInfo->connected_sta_ptr->networkMode = PHY_MODE_G_ONLY;
	for(i=0;i<elems.supp_rates_len;i++)
	{
		LOG_PRINTF("elems.supp_rates[%d]=%d\n",i,elems.supp_rates[i]&0x7f);
		gDevInfo->connected_sta_ptr->supp_rates |= supportRateParser(elems.supp_rates[i]&0x7f);
	}
	//Parser WLAN_EID_EXT_SUPP_RATES
	if(((u8 *)elems.ext_supp_rates) != NULL)
	{
		for(i=0;i<elems.ext_supp_rates_len;i++)
		{
			LOG_PRINTF("elems.supp_rates[%d]=%d\n",i,elems.ext_supp_rates[i]&0x7f);
			gDevInfo->connected_sta_ptr->supp_rates |= supportRateParser(elems.ext_supp_rates[i]&0x7f);
		}
	}
    ap_rc_info.non_ht_supp_rates=gDevInfo->connected_sta_ptr->supp_rates;

	/*
		Parser WLAN_EID_HT_OPERATION
	*/
	if(elems.ht_operation)
	{
		//gDevInfo->connected_sta_ptr->supp_rates=0;
		//gDevInfo->connected_sta_ptr->networkMode = PHY_MODE_LONG_GI;
		//Force support rate
		//gDevInfo->connected_sta_ptr->supp_rates=0xfff;

        //set HT protectction
        set_ht_protection(elems.ht_operation); /*bit 8 & bit 9*/
	}

    if(elems.ht_capabilities)
    {
        //rc :get ht capabilities info
        memcpy((void*)&ap_rc_info.ht_capabilities.ht_capabilities_info,(void*)elems.ht_capabilities,SIZE_OF_HT_CAPABILITIES_INFO_ELEMENT);

        //rc :get supported mcs set
        memcpy((void*)ap_rc_info.ht_capabilities.supported_mcs_set,(void*)&elems.ht_capabilities[3],SIZE_OF_SUPPORTED_MCS_SET_ELEMENT);
    }

    if(elems.wmm_parameter && elems.wmm_parameter_len>0)
    {
        drv_mac_set_qos_en(1);
        gDevInfo->connected_sta_ptr->parameter_set_count=(elems.wmm_parameter[6]) & 0x07;
#if (CONFIG_SIM_PLATFORM == 0)
#if 0 //for AMPDU, don't flow the AP's setting
        for(i=0;i<EDCA_DEFINE_NUM_OF_TX_Q;i++)
        {
            //We give the same priority to the all edca queues, these tx queues are not for edca handle, are for individual STAs in our design now.
            //So we set the EDCA(AC_BE) value into all queues.
            // i=0 is for tx queue 0 (AC_BK), the lowest priority queue
            // i=3 is for tx queue 3 (AC_VO), the highest priority queue
            //memcpy(	(u32 *) (ADR_TXQ0_MTX_Q+i*0x100),&elems.wmm_parameter[8+i*4],4);

            aci = (elems.wmm_parameter[8+i*TOTAL_BYTE_OF_EACH_AC]&0x00000060)>>POSITION_OF_ACI_IN_AC;

            if(aci == ACI_OF_BEST_EFFORT)
            {
                for(j=0;j<EDCA_DEFINE_NUM_OF_TX_Q;j++)
                {
                    memcpy((u32 *) (ADR_TXQ0_MTX_Q+j*0x100),&elems.wmm_parameter[8+i*TOTAL_BYTE_OF_EACH_AC],4);
                    // cabrio hw designing for AIFS is need to -1;
                    REG32(ADR_TXQ0_MTX_Q+j*0x100)-=1 ;
                }
                break;
            }
        }
#endif
#endif
	}
	else{
		drv_mac_set_qos_en(0);
    }
    /*
        Parser WLAN_EID_ERP_INFO
        */
    if(elems.erp_info)
    {
        set_erp_param(elems.erp_info);
    }

    if(WorkJobReq!=NULL)
    {
        memcpy((void *)gDevInfo->connected_sta_ptr->ht_cap_ie, (void *)WorkJobReq->u.join.ht_cap_ie, sizeof(struct ssv_ht_cap));
        memcpy((void *)&gDevInfo->connected_sta_ptr->ssid, (void *)&WorkJobReq->u.join.bss.ssid, sizeof(struct cfg_80211_ssid));       
    }
    

    rate_control_pid_sta_connect(STA_MODE_WSID,&ap_rc_info);

	return MLME_OK;

}


static s32 mlme_rx_assoc_response( PKT_RxInfo **_PktInfo )
{
    PKT_RxInfo *PktInfo=(PKT_RxInfo *)(*_PktInfo);
    const HDR80211_Mgmt *Mgmt80211=HDR80211_MGMT(PktInfo);
    printf("Received assoc reponse.\n");

    //wrong BSSID. error handling
    drv_mac_get_bssid(gBSSID);
    if(memcmp(gBSSID,Mgmt80211->bssid.addr,ETHER_ADDR_LEN))
    {
        printf("received other AP's assoc response !! wsid = %d ,seq number =%d ,BSSID = %x:%x:%x:%x:%x:%x\n",PktInfo->wsid,GET_HDR80211_SC_SEQNUM(PktInfo),
            Mgmt80211->bssid.addr[0],Mgmt80211->bssid.addr[1],
            Mgmt80211->bssid.addr[2],Mgmt80211->bssid.addr[3],
            Mgmt80211->bssid.addr[4],Mgmt80211->bssid.addr[5]);
        return MLME_OK;
    }

    //wrong da. error handling
    if(memcmp(&Mgmt80211->da, &gDevInfo->info.addr, ETHER_ADDR_LEN))
    {
        printf("The assoc_response frame is not to me !! wsid = %d ,seq number =%d ,DA = %x:%x:%x:%x:%x:%x\n",
                PktInfo->wsid,GET_HDR80211_SC_SEQNUM(PktInfo),
                Mgmt80211->da.addr[0],Mgmt80211->da.addr[1],
                Mgmt80211->da.addr[2],Mgmt80211->da.addr[3],
                Mgmt80211->da.addr[4],Mgmt80211->da.addr[5]);
        return MLME_OK;
    }       

    // MLME_WORKJOB_STRUCT(WorkJobReq,sta_info);

    if (gDevInfo->State!=STA_STATE_AUTHED_UNASSOC)
    {
        MLME_WARN("Invalid state when receiving associate response.");
        //PBUF_MFree(PktInfo);
        return MLME_FAILED;
    }

    //get_sta_info_from_assoc_resp(Mgmt80211,PktInfo->len);
    if((get_sta_info_from_assoc_resp(Mgmt80211,PktInfo->len))==MLME_OK)
    {
        MLME_TRACE("Associate response is success. (AID=%d)\n", gDevInfo->connected_sta_ptr->aid);

        // _apply_join_req_wsid(0, WorkJobReq);

        //Add station to rate control handler
        //rate_control_pid_sta_connect(0);
#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
        if(supplicant_bypass)
        {
            Join_process_result(EN_JOIN_RESULT_SUCCESS);
        }
        else
        {
            //tmp-mark-supplicant
            wpa_supplicant_event(gDevInfo->pSupplicant, EVENT_ASSOC, (union wpa_event_data *)NULL);
        }
#endif
        //PBUF_MFree(PktInfo);
        //20141028 EricYu: For join timer
        MLME_STOP_TIMER(gDevInfo->MTimer.MTimer);
        Join_action(MLME_WORK_JOIN_SUPPLICIANT,NULL,0);
        return MLME_OK;
    }
    else
    {
        return MLME_FAILED;
    }
}


#if 0
static s32 mlme_rx_reassoc_request( PKT_RxInfo *PktInfo )
{

    return MLME_OK;
}
#endif

static s32 mlme_rx_reassoc_response( PKT_RxInfo **_PktInfo )
{
    MLME_TRACE("Reassociation response received.\n");
    PKT_RxInfo *PktInfo=(PKT_RxInfo *)(*_PktInfo);
    const HDR80211_Mgmt *Mgmt80211=HDR80211_MGMT(PktInfo);
    printf("Received Reassociation response.\n");

    //wrong BSSID. error handling
    drv_mac_get_bssid(gBSSID);
    if(memcmp(gBSSID,Mgmt80211->bssid.addr,ETHER_ADDR_LEN))
    {
        printf("received other AP's Reassociation response !! wsid = %d ,seq number =%d ,BSSID = %x:%x:%x:%x:%x:%x\n",PktInfo->wsid,GET_HDR80211_SC_SEQNUM(PktInfo),
                Mgmt80211->bssid.addr[0],Mgmt80211->bssid.addr[1],
                Mgmt80211->bssid.addr[2],Mgmt80211->bssid.addr[3],
                Mgmt80211->bssid.addr[4],Mgmt80211->bssid.addr[5]);
        return MLME_OK;
    }

    //wrong da. error handling
    if(memcmp(&Mgmt80211->da, &gDevInfo->info.addr, ETHER_ADDR_LEN))
    {
        printf("The reassoc_response frame is not to me !! wsid = %d ,seq number =%d ,DA = %x:%x:%x:%x:%x:%x\n",
                PktInfo->wsid,GET_HDR80211_SC_SEQNUM(PktInfo),
                Mgmt80211->da.addr[0],Mgmt80211->da.addr[1],
                Mgmt80211->da.addr[2],Mgmt80211->da.addr[3],
                Mgmt80211->da.addr[4],Mgmt80211->da.addr[5]);
        return MLME_OK;
    }    

    if (gDevInfo->State!=STA_STATE_AUTHED_UNASSOC)
    {
        MLME_WARN("Invalid state when receiving associate response.");
        return MLME_FAILED;
    }
#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
    if(supplicant_bypass)
    {
        ;
    }
    else
    {
        //tmp-mark-supplicant
        wpa_supplicant_event(gDevInfo->pSupplicant, EVENT_ASSOC, (union wpa_event_data *)NULL);
    }
#endif
    //PBUF_MFree(PktInfo);
    return MLME_OK;
}


#if 0
static s32 mlme_rx_probe_request( PKT_RxInfo *PktInfo )
{

    return MLME_OK;
}
#endif
static s32 mlme_rx_probe_request( PKT_RxInfo **_PktInfo )
{
    #if 0
    const HDR80211_Mgmt *Mgmt80211=HDR80211_MGMT(PktInfo);
    LOG_PRINTF("\33[35m mlme_rx_probe_resquest():da=%x:%x:%x:%x:%x:%x \33[0m\r\n",Mgmt80211->da.addr[0],Mgmt80211->da.addr[1],Mgmt80211->da.addr[2],
                                                        Mgmt80211->da.addr[3],Mgmt80211->da.addr[4],Mgmt80211->da.addr[5]);
    LOG_PRINTF("\33[35m mlme_rx_probe_resquest():sa=%x:%x:%x:%x:%x:%x \33[0m\r\n",Mgmt80211->sa.addr[0],Mgmt80211->sa.addr[1],Mgmt80211->sa.addr[2],
                                                        Mgmt80211->sa.addr[3],Mgmt80211->sa.addr[4],Mgmt80211->sa.addr[5]);
    LOG_PRINTF("\33[35m mlme_rx_probe_resquest():bssid=%x:%x:%x:%x:%x:%x \33[0m\r\n",Mgmt80211->bssid.addr[0],Mgmt80211->bssid.addr[1],Mgmt80211->bssid.addr[2],
                                                           Mgmt80211->bssid.addr[3],Mgmt80211->bssid.addr[4],Mgmt80211->bssid.addr[5]);
    #endif
    return 0;
}
static s32 mlme_rx_probe_response( PKT_RxInfo **_PktInfo )
{
    PKT_RxInfo *PktInfo=(PKT_RxInfo *)(*_PktInfo);
    const HDR80211_Mgmt *Mgmt80211=HDR80211_MGMT(PktInfo);
    const Mlme_WorkJobReq *WorkJobReq=(Mlme_WorkJobReq *)(gDevInfo)->MlmeReqParams;
    struct ieee802_11_elems elems;
    SSV6XXX_STA		*sta=gDevInfo->connected_sta_ptr;
    #if 0
    printf("\33[35m mlme_rx_probe_response():da=%x:%x:%x:%x:%x:%x \33[0m\r\n",Mgmt80211->da.addr[0],Mgmt80211->da.addr[1],Mgmt80211->da.addr[2],
                                                        Mgmt80211->da.addr[3],Mgmt80211->da.addr[4],Mgmt80211->da.addr[5]);
    printf("\33[35m mlme_rx_probe_response():sa=%x:%x:%x:%x:%x:%x \33[0m\r\n",Mgmt80211->sa.addr[0],Mgmt80211->sa.addr[1],Mgmt80211->sa.addr[2],
                                                        Mgmt80211->sa.addr[3],Mgmt80211->sa.addr[4],Mgmt80211->sa.addr[5]);
    printf("\33[35m mlme_rx_probe_response():bssid=%x:%x:%x:%x:%x:%x \33[0m\r\n",Mgmt80211->bssid.addr[0],Mgmt80211->bssid.addr[1],Mgmt80211->bssid.addr[2],
                                                        Mgmt80211->bssid.addr[3],Mgmt80211->bssid.addr[4],Mgmt80211->bssid.addr[5]);
    #endif

    if((sta==NULL)&&(WorkJobReq==NULL))
    {
        return MLME_FAILED;
    }

    //check da, this frame is for me
    if (memcmp(&Mgmt80211->da, &gDevInfo->info.addr, ETHER_ADDR_LEN))
    {
        printf("The probe_response frame is not to me !! wsid = %d ,seq number =%d ,DA = %x:%x:%x:%x:%x:%x\n",
                PktInfo->wsid,GET_HDR80211_SC_SEQNUM(PktInfo),
                Mgmt80211->da.addr[0],Mgmt80211->da.addr[1],
                Mgmt80211->da.addr[2],Mgmt80211->da.addr[3],
                Mgmt80211->da.addr[4],Mgmt80211->da.addr[5]);
        return MLME_FAILED;
     }

      
    if(sta!=NULL)
    {
        sta->ap_heartbeat_cnt++;
    }

       
    //check PktInfo->len
    if (PktInfo->len < IEEE80211_HDRLEN + sizeof(Mgmt80211->u.probe_resp)) {
        //PBUF_MFree(PktInfo);
        return MLME_FAILED;
    }

    memset((void *)&elems,0,sizeof(struct ieee802_11_elems));

    // Parse scan result.
    if (   mlme_parse_elems((u8 *)Mgmt80211->u.probe_resp.variable,
                            PktInfo->len - BASE_LEN(Mgmt80211->u.probe_resp.variable,Mgmt80211),
                            &elems)
        == ParseFailed)
    {
        MLME_WARN("Scan result parse error.\n");
        return MLME_FAILED;
    }

    if(WorkJobReq!=NULL)
    {
    Scan_process_result(PktInfo,PktInfo->len,&elems,WorkJobReq->cmd_seq_no);
    }
    else
    {
        Scan_process_result(PktInfo,PktInfo->len,&elems,0);    
    }
    
#if 0
    if(WorkJobReq->work_type==MLME_WORK_PROBE_RESP)
    {
        MLME_STOP_TIMER(sta_info->MTimer.MTimer);
        Scan_action(MLME_WORK_SCAN_RESULT,NULL,0);
    }
#endif
    return MLME_OK;
}

#define SET_RSSI_TO_REG(_VAL_) ((REG32(ADR_DBG_SPI_TO_PHY_PARAM2)) = (((_VAL_) << 16) | ((REG32(ADR_DBG_SPI_TO_PHY_PARAM2)) & 0x0000ffff)))
inline void sta_mode_set_rcpi_value(void* data)
{
    u32 rcpi_value;

    const PKT_RxInfo *PktInfo=(PKT_RxInfo *)data;
    rcpi_value=*((u8 *)((u32)PktInfo+PktInfo->hdr_offset+PktInfo->len));
    SET_RSSI_TO_REG(rcpi_value);

    //printf("!!! %s : rcpi value = %d \r\n",__FUNCTION__,rcpi_value);
}
#if(ENABLE_DYNAMIC_RX_SENSITIVE==1)
u16 rcpi_update_count=8;
u32 rcpi_counter=0;
u16 rcpi_sum=0;
#define STA_GET_RCPI_FROM_PKT(p) *((u8 *)((u32)p+((PKT_RxInfo *)p)->hdr_offset+((PKT_RxInfo *)p)->len))
#define STA_SET_RCPI_FROM_PKT(p,val) *((u8 *)((u32)p+((PKT_RxInfo *)p)->hdr_offset+((PKT_RxInfo *)p)->len))=val
#define RSSI_SMOOTHING_SHIFT        5
#define RSSI_DECIMAL_POINT_SHIFT    6
u32 sta_mode_fine_tune_rcpi(SSV6XXX_STA		*sta, PKT_RxInfo * pRxInfo)
{

    //ScanResp->u.scan.bss_info.rxphypad.rpci=*((u8 *)((u32)PktInfo+PktInfo->hdr_offset+PktInfo->len));
    u16 _rcpi=STA_GET_RCPI_FROM_PKT(pRxInfo);//*((u8 *)((u32)pRxInfo+pRxInfo->hdr_offset+pRxInfo->len));
    if(sta->rxphypad.rpci)
    {
        sta->prev_rcpi = ((((u32)_rcpi) << RSSI_DECIMAL_POINT_SHIFT)
            + ((sta->prev_rcpi<<RSSI_SMOOTHING_SHIFT) - sta->prev_rcpi)) >> RSSI_SMOOTHING_SHIFT;
        sta->rxphypad.rpci = ((sta->prev_rcpi) >> RSSI_DECIMAL_POINT_SHIFT);
    }
    else
    {
        sta->prev_rcpi=_rcpi<<RSSI_DECIMAL_POINT_SHIFT;
        sta->rxphypad.rpci=_rcpi;
    }
    return sta->rxphypad.rpci;
}
#endif

#define BKN_UPDATE_TIME (500000) //5 Sec, unit 10us.
u32 bkn_update_time = 0;
u32 current_bcn,last_bcn;
extern bool ieee80211_check_tim(struct ieee80211_tim_ie *tim,u8 tim_len, u32 aid);
extern u32 enterSystemSleep(u16 beacon_int, u8 dtim_count, u8 dtim_period,u32 diff_t_10us,u32 default_ltc_10us);
s32 mlme_null_send(DevInfo *dev, bool ps_on, bool edca_direct);
extern void MLME_PwrSaving(void* pdata);
extern bool ps_send_null_frame_mib(bool on);
extern void ps_stop_all_tmr(void);
extern bool ieee80211_check_tim(struct ieee80211_tim_ie *tim,u8 tim_len, u32 aid);
//extern bool display_beacon;
//Assume that this function is available in STA mode
static s32 mlme_rx_beacon( PKT_RxInfo **_PktInfo )
{
    PKT_RxInfo *PktInfo=(PKT_RxInfo *)(*_PktInfo);
    const Mlme_WorkJobReq *WorkJobReq;
    WorkJobReq = (Mlme_WorkJobReq *)(gDevInfo)->MlmeReqParams;
    SSV6XXX_STA		*sta=gDevInfo->connected_sta_ptr;
    struct ieee802_11_elems elems;
    const HDR80211_Mgmt *Mgmt80211=HDR80211_MGMT((void *)PktInfo);
    int i=0;
    #if 0
    //const HDR80211_Mgmt *Mgmt80211=HDR80211_MGMT(PktInfo);
    //printf("\33[35m mlme_rx_beacon():da=%x:%x:%x:%x:%x:%x \33[0m\r\n",Mgmt80211->da.addr[0],Mgmt80211->da.addr[1],Mgmt80211->da.addr[2],
    //                                                    Mgmt80211->da.addr[3],Mgmt80211->da.addr[4],Mgmt80211->da.addr[5]);
    //printf("\33[35m mlme_rx_beacon():sa=%x:%x:%x:%x:%x:%x \33[0m\r\n",Mgmt80211->sa.addr[0],Mgmt80211->sa.addr[1],Mgmt80211->sa.addr[2],
    //                                                    Mgmt80211->sa.addr[3],Mgmt80211->sa.addr[4],Mgmt80211->sa.addr[5]);
    if(TRUE==display_beacon){
        printf("\33[35m mlme_rx_beacon():seq num=%d\n",GET_HDR80211_SC_SEQNUM(PktInfo));
        printf("\33[35m mlme_rx_beacon():bssid=%x:%x:%x:%x:%x:%x \33[0m\r\n",Mgmt80211->bssid.addr[0],Mgmt80211->bssid.addr[1],Mgmt80211->bssid.addr[2],
                                                        Mgmt80211->bssid.addr[3],Mgmt80211->bssid.addr[4],Mgmt80211->bssid.addr[5]);
    }
    #endif
    if((gDevInfo->ps_state == PWR_DOZE)||(gDevInfo->ps_state == PWR_BCN_SYNC)||
       (gDevInfo->ps_state == PWR_BCN_SYNC_TIM))
    {
        ps_stop_all_tmr();
    }
    /*
        The purpose of ap_heartbeat_cnt is let us know this AP still exists.
    */
    if(sta!=NULL){
        sta->ap_heartbeat_cnt++;
        //update rssi by using register
        sta_mode_set_rcpi_value((void*)PktInfo);
    }

    //do only frame is from associate AP
    // Parse IE result.
    memset((void *)&elems,0,sizeof(struct ieee802_11_elems));
    if (mlme_parse_elems((u8 *)Mgmt80211->u.beacon.variable,PktInfo->len - BASE_LEN(Mgmt80211->u.beacon.variable,Mgmt80211),&elems)
            == ParseFailed){
        MLME_WARN("IE parse error.\n");
        return MLME_FAILED;
    }

    if(sta!=NULL){
        //do only frame is from associate AP
        if(!memcmp(&Mgmt80211->bssid.addr,&sta->addr,ETHER_ADDR_LEN)){
            //do this when it has any changed
            if(elems.erp_info)
                set_erp_param(elems.erp_info);
            if(Mgmt80211->u.probe_resp.capab_info)
                set_short_slot_time((const u8 *)&Mgmt80211->u.probe_resp.capab_info);
            if(elems.ht_operation)
                set_ht_protection(elems.ht_operation);
        }
    }

    if(WorkJobReq == NULL)
    {
        //HDR80211_Mgmt *Mgmt80211=HDR80211_MGMT(PktInfo);
        u32 diff_t;
        u8* ptr =(u8*)&(Mgmt80211->u.beacon.timestamp);
        //dbg_get_time(&current_bcn);

        if((gDevInfo->ps_state == PWR_DOZE)||(gDevInfo->ps_state == PWR_BCN_SYNC)||
           (gDevInfo->ps_state == PWR_BCN_SYNC_TIM))
        {
            current_bcn = (ptr[0]|(ptr[1]<<8)|(ptr[2]<<16)|(ptr[3]<<24));
            diff_t = current_bcn -last_bcn;
            //printf("st =%d\r\n",current_bcn -last_bcn);
            if((gDevInfo->ps_state == PWR_BCN_SYNC)||(gDevInfo->ps_state == PWR_BCN_SYNC_TIM))
            {
                last_bcn = current_bcn;
                diff_t=0;
            }

#if 0 //Nerver wakeup self. Sleep till wakeup by AP.
            if(bkn_update_time > BKN_UPDATE_TIME*36) //180 Sec
            {
                MLME_ps_exit();
                printf("PS T=%d\r\n",bkn_update_time);
                Scan_process_result(PktInfo, PktInfo->len, &elems, WorkJobReq->cmd_seq_no);
                bkn_update_time = 0;
            }
            else
#endif
            {
                bkn_update_time += MLME_ps_bcn_handling(PktInfo,&elems,bkn_update_time,diff_t);
                //printf("%x,%x,%x,%x,%x,%x,%x,%x\r\n",ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7]);
                last_bcn = current_bcn;
#if 1                
                if(gDevInfo->ps_state == PWR_DOZE)
                {
                    if(bkn_update_time > (BKN_UPDATE_TIME*7)) //35 Sec
                    {   
                        //Send ps null frame to keep alive with AP                        
                        if(ps_send_null_frame_mib(true)== true)
                            bkn_update_time=0;
                        else
                            MLME_ps_exit();
                    }
                }
#endif
            }

        }
        else
        {
            if(sta!=NULL)
            {
                bool directed_tim = false;
                if(gDevInfo->aid)
                {
                    directed_tim = ieee80211_check_tim(((struct ieee80211_tim_ie *)(elems.tim)),
                                                       elems.tim_len,gDevInfo->aid);
                }

                if((sta->ap_heartbeat_cnt%3 == 0)||(directed_tim))
                    mlme_null_send(gDevInfo, false, true);

#if(ENABLE_DYNAMIC_RX_SENSITIVE==1)
                //u16 rcpi=0;
                rcpi_sum += sta_mode_fine_tune_rcpi(sta,PktInfo);
                //*((u8 *)((u32)PktInfo+PktInfo->hdr_offset+PktInfo->len))=sta_mode_fine_tune_rcpi(sta,PktInfo);
                rcpi_counter++;
                if(rcpi_counter==rcpi_update_count)
                {
                    STA_SET_RCPI_FROM_PKT(PktInfo,(rcpi_sum/rcpi_update_count));
                    Scan_process_result(PktInfo, PktInfo->len, &elems, WorkJobReq->cmd_seq_no);
                    rcpi_counter=0;
                    rcpi_sum = 0;
                }
#endif
            }
        }
    }
    else
    {
        if (WorkJobReq->work_type <= MLME_WORK_END_SCANNING){
            if(WorkJobReq->u.scan.n_ssids!=0){
                for(i=0;i<WorkJobReq->u.scan.n_ssids;i++){
                    //If the scan request bring the SSID informaction, fw filter the dismatch bss information, fw only pass the mach bss information to host
                    //It's in order to avoid the AP list doesn't have enough space to store the AP inforamtion that we want.
                    if(0==memcmp(&WorkJobReq->u.scan.ssids[i],elems.ssid,elems.ssid_len)){
                        Scan_process_result(PktInfo, PktInfo->len, &elems,WorkJobReq->cmd_seq_no);
                    }
                }
            }else{
                Scan_process_result(PktInfo, PktInfo->len, &elems,WorkJobReq->cmd_seq_no);
            }
        }
    }
    return MLME_OK;
}



static s32 mlme_rx_atim( PKT_RxInfo **_PktInfo )
{
    MLME_TRACE("%s\n", __FUNCTION__);
    return MLME_OK;
}


static s32 mlme_disconnected(PKT_RxInfo **_PktInfo)
{
    PKT_RxInfo *PktInfo=(PKT_RxInfo *)(*_PktInfo);
    const u8 *reason_code_ie;
    s16       reason_code;
    const HDR80211_Mgmt *Mgmt80211;

    Mgmt80211=HDR80211_MGMT(PktInfo);
    reason_code_ie=(const u8 *)Mgmt80211+24;//Mac Header
    reason_code = ((reason_code_ie[1]<<8)+reason_code_ie[0]);
    MLME_Leave_Resp((void **)_PktInfo, 1,reason_code);

    return MLME_OK;
}

static s32 mlme_rx_disassoc( PKT_RxInfo **_PktInfo )
{
    MLME_TRACE("%s\n", __FUNCTION__);
    PKT_RxInfo *PktInfo=(PKT_RxInfo *)(*_PktInfo);
    const HDR80211_Mgmt *Mgmt80211=HDR80211_MGMT(PktInfo);
    printf("Received diassoc.\n");

    //wrong BSSID. error handling
    drv_mac_get_bssid(gBSSID);
    if(memcmp(gBSSID,Mgmt80211->bssid.addr,ETHER_ADDR_LEN))
    {
        printf("received other AP's disassoc !! wsid = %d ,seq number =%d ,BSSID = %x:%x:%x:%x:%x:%x\n",PktInfo->wsid,GET_HDR80211_SC_SEQNUM(PktInfo),
            Mgmt80211->bssid.addr[0],Mgmt80211->bssid.addr[1],
            Mgmt80211->bssid.addr[2],Mgmt80211->bssid.addr[3],
            Mgmt80211->bssid.addr[4],Mgmt80211->bssid.addr[5]);
        return MLME_OK;
    }

    //wrong da. error handling
    if(memcmp(&Mgmt80211->da, &gDevInfo->info.addr, ETHER_ADDR_LEN))
    {
        printf("The disassoc frame is not to me !! wsid = %d ,seq number =%d ,DA = %x:%x:%x:%x:%x:%x\n",
                PktInfo->wsid,GET_HDR80211_SC_SEQNUM(PktInfo),
                Mgmt80211->da.addr[0],Mgmt80211->da.addr[1],
                Mgmt80211->da.addr[2],Mgmt80211->da.addr[3],
                Mgmt80211->da.addr[4],Mgmt80211->da.addr[5]);
        return MLME_OK;
    }  

    if ((gDevInfo->State!=STA_STATE_AUTHED_ASSOCED) && (gDevInfo->State!=STA_STATE_ASSOCED_4way))
    {
        MLME_WARN("Dissociation received when state in %d\n", gDevInfo->State);
        return MLME_OK;
    }
#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
    if(supplicant_bypass)
    {
        ;
    }
    else
    {
        //tmp-mark-supplicant
        wpa_supplicant_event(gDevInfo->pSupplicant, EVENT_DISASSOC, (union wpa_event_data *)NULL);
    }
#endif
    return mlme_disconnected(_PktInfo);
}

static s32 mlme_rx_auth( PKT_RxInfo **_PktInfo )
{
    PKT_RxInfo *PktInfo=(PKT_RxInfo *)(*_PktInfo);
    HDR80211_Mgmt *Mgmt80211=HDR80211_MGMT(PktInfo);
    const Mlme_WorkJobReq *WorkJobReq;
    u16 auth_alg; //, status_code;

    MLME_WORKJOB_STRUCT(WorkJobReq,gDevInfo);

    printf("Authentication response received. trans_id=%d, status_code=%d \n",Mgmt80211->u.auth.trans_id,Mgmt80211->u.auth.status_code);

    //wrong BSSID. error handling
    drv_mac_get_bssid(gBSSID);    
    if(memcmp(gBSSID,Mgmt80211->bssid.addr,ETHER_ADDR_LEN))
    {
        printf("received other AP's auth !! wsid = %d ,seq number =%d ,BSSID = %x:%x:%x:%x:%x:%x\n",PktInfo->wsid,GET_HDR80211_SC_SEQNUM(PktInfo),
            Mgmt80211->bssid.addr[0],Mgmt80211->bssid.addr[1],
            Mgmt80211->bssid.addr[2],Mgmt80211->bssid.addr[3],
            Mgmt80211->bssid.addr[4],Mgmt80211->bssid.addr[5]);
        return MLME_OK;
    }

    //wrong da. error handling
    if(memcmp(&Mgmt80211->da, &gDevInfo->info.addr, ETHER_ADDR_LEN))
    {
        printf("The disassoc frame is not to me !! wsid = %d ,seq number =%d ,DA = %x:%x:%x:%x:%x:%x\n",
                PktInfo->wsid,GET_HDR80211_SC_SEQNUM(PktInfo),
                Mgmt80211->da.addr[0],Mgmt80211->da.addr[1],
                Mgmt80211->da.addr[2],Mgmt80211->da.addr[3],
                Mgmt80211->da.addr[4],Mgmt80211->da.addr[5]);
        return MLME_OK;
    } 


    if (gDevInfo->State!=STA_STATE_UNAUTH_UNASSOC)
    {
        MLME_WARN("Authentication response received invalid state %u.\n", WorkJobReq->work_type);
        return MLME_FAILED;
    }

    do
    {
        if (WorkJobReq->work_type != MLME_WORK_JOIN_RX_AUTH_2 &&
            WorkJobReq->work_type != MLME_WORK_JOIN_RX_AUTH_4 )
        {
            MLME_ERROR("Reveive rx auth but in worng work type %d\n", WorkJobReq->work_type);
            break;
        }

        auth_alg = Mgmt80211->u.auth.auth_algo;
        /*
        The default algorithm of WEP is "share",  if we get the "13" of status code, we try "open"
        */
        if(Mgmt80211->u.auth.status_code == WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG)
        {
            gDevInfo->wpa_ssid.auth_alg=WPA_AUTH_ALG_OPEN;
            Join_action(MLME_WORK_JOIN_TX_AUTH_1,NULL,0);
            break;
        }

        if( !( ((gDevInfo->wpa_ssid.auth_alg == WPA_AUTH_ALG_OPEN) && auth_alg == WLAN_AUTH_OPEN) ||((gDevInfo->wpa_ssid.auth_alg == WPA_AUTH_ALG_SHARED) && auth_alg == WLAN_AUTH_SHARED_KEY)) ||
            Mgmt80211->u.auth.trans_id != WorkJobReq->u.join.auth_transaction ||
            Mgmt80211->u.auth.status_code != WLAN_STATUS_SUCCESS )
        {
            MLME_ERROR( "Error auth_alg:%d gDevInfo->wpa_ssid.auth_alg:%d \n"
                "Mgmt80211->u.auth.trans_id:%d WorkJobReq->u.probe_auth.auth_transaction:%d\n"
                "Mgmt80211->u.auth.status_code:%d \n",
                auth_alg,
                gDevInfo->wpa_ssid.auth_alg,
                Mgmt80211->u.auth.trans_id,
                WorkJobReq->u.join.auth_transaction,
                Mgmt80211->u.auth.status_code);
            break;
        }

        //20141028 EricYu: For join timer
        MLME_STOP_TIMER(gDevInfo->MTimer.MTimer);
        if(Mgmt80211->u.auth.trans_id == 2)
        {
            //MLME_STOP_TIMER(gDevInfo->MTimer.MTimer);

            if(Mgmt80211->u.auth.auth_algo == WLAN_AUTH_OPEN)
            {
                Join_action(MLME_WORK_JOIN_ASSOC_REQ,NULL,0);
            }
            else
            {
                u8 *pos;
                u32 len, left;
                u32 eid;
                //Shared key
                pos = &Mgmt80211->u.auth.variable[0];

                left = PktInfo->len - (PktInfo->hdr_len+ sizeof(Mgmt80211->u.auth));
                // Checking challenge text
                eid = *pos++;
                len = *pos++;
                if( left < 2 ||
                    WLAN_EID_CHALLENGE != eid ||
                    (left-2) < len )
                {
                    MLME_ERROR("Challenge text format is incorrect. (EID=%u, len = %u)\n",
                        eid, len);
                    break;
                }
                Join_action(MLME_WORK_JOIN_TX_AUTH_3, (void*)(pos-2), len+2);
            }
         }
        else
        {
            Join_action(MLME_WORK_JOIN_ASSOC_REQ,NULL,0);
        }
    } while(0);

    return MLME_OK;
}


static s32 mlme_rx_deauth( PKT_RxInfo **_PktInfo )
{
    MLME_TRACE("%s\n", __FUNCTION__);
    PKT_RxInfo *PktInfo=(PKT_RxInfo *)(*_PktInfo);
    const HDR80211_Mgmt *Mgmt80211=HDR80211_MGMT(PktInfo);
    printf("Received deauth.\n");

    //wrong BSSID. error handling
    drv_mac_get_bssid(gBSSID);

    if(memcmp(gBSSID,Mgmt80211->bssid.addr,ETHER_ADDR_LEN))
    {
        printf("received other AP's deauth !! wsid = %d ,seq number =%d ,BSSID = %x:%x:%x:%x:%x:%x\n",PktInfo->wsid,GET_HDR80211_SC_SEQNUM(PktInfo),
            Mgmt80211->bssid.addr[0],Mgmt80211->bssid.addr[1],
            Mgmt80211->bssid.addr[2],Mgmt80211->bssid.addr[3],
            Mgmt80211->bssid.addr[4],Mgmt80211->bssid.addr[5]);
        return MLME_OK;
    }

    //wrong da. error handling
    if(memcmp(&Mgmt80211->da, &gDevInfo->info.addr, ETHER_ADDR_LEN))
    {
        printf("The deauth frame is not to me !! wsid = %d ,seq number =%d ,DA = %x:%x:%x:%x:%x:%x\n",
                PktInfo->wsid,GET_HDR80211_SC_SEQNUM(PktInfo),
                Mgmt80211->da.addr[0],Mgmt80211->da.addr[1],
                Mgmt80211->da.addr[2],Mgmt80211->da.addr[3],
                Mgmt80211->da.addr[4],Mgmt80211->da.addr[5]);
        return MLME_OK;
    }

    if(gDevInfo->State==STA_STATE_UNAUTH_UNASSOC)
    {
        return MLME_OK;
    }
#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
    if(supplicant_bypass)
    {
        ;
    }
    else
    {
        //tmp-mark-supplicant
        wpa_supplicant_event(gDevInfo->pSupplicant, EVENT_DEAUTH, (union wpa_event_data *)NULL);
    }
#endif


    return mlme_disconnected(_PktInfo);

}



#if (N_MODE_AMPDU_TX == 1)
extern void ampdu_addba_req(SSV6XXX_STA *sta, u8 *p8_da, u8 *p8_sa, u8 *p8_bssid);
extern void ampdu_send_bar(u8 *ta, u8 *ra, u16 ssn);

extern void rate_control_set_defaul_rates_by_sta(u8 wsid);
extern bool rate_control_pid_is_nmode_rate(u8 wsid);
extern u8 AMPDU_TX_BlockSessionAtNotNmodeRate;
extern u8 AMPDU_TX_ENABLE;
extern u8 AMPDU_RX_ENABLE;
#endif
//handle action frame for AP/STA mode
static s32 mlme_rx_action( PKT_RxInfo **_PktInfo )
{
    PKT_RxInfo *p_pkt_info=(PKT_RxInfo *)(*_PktInfo);
    HDR80211_Mgmt *p_mgmt_hdr=HDR80211_MGMT(p_pkt_info);
    u8 u8_wsid = p_pkt_info->wsid;
#if (N_MODE_AMPDU_TX == 1)
    SSV6XXX_STA *p_sta;
#endif

    if(u8_wsid >= WLAN_MAX_STA)
    {
     printf("received action frame DA[%02x:%02x:%02x:%02x:%02x:%02x] SA[%02x:%02x:%02x:%02x:%02x:%02x] BSSID[%02x:%02x:%02x:%02x:%02x:%02x] catetory[%d]\n",
                p_mgmt_hdr->da.addr[0],p_mgmt_hdr->da.addr[1],
                p_mgmt_hdr->da.addr[2],p_mgmt_hdr->da.addr[3],
                p_mgmt_hdr->da.addr[4],p_mgmt_hdr->da.addr[5],
                p_mgmt_hdr->sa.addr[0],p_mgmt_hdr->sa.addr[1],
                p_mgmt_hdr->sa.addr[2],p_mgmt_hdr->sa.addr[3],
                p_mgmt_hdr->sa.addr[4],p_mgmt_hdr->sa.addr[5],
                p_mgmt_hdr->bssid.addr[0],p_mgmt_hdr->bssid.addr[1],
                p_mgmt_hdr->bssid.addr[2],p_mgmt_hdr->bssid.addr[3],
                p_mgmt_hdr->bssid.addr[4],p_mgmt_hdr->bssid.addr[5],
                p_mgmt_hdr->u.action.category);

        printf("received action frame unknow wsid = %d\n",
                p_pkt_info->wsid);
        return MLME_OK;
    }
#if (N_MODE_AMPDU_TX == 1)
    p_sta = &g6200StaInfo[u8_wsid];
#endif


	switch (p_mgmt_hdr->u.action.category) {

    case WLAN_CATEGORY_BACK:
        switch (p_mgmt_hdr->u.action.u.addba_req.action_code) {
        case WLAN_ACTION_ADDBA_REQ://for AMPDU RX

            printf("ADDBA_REQ for RX=%d\n", AMPDU_RX_ENABLE);
            if(AMPDU_RX_ENABLE == TRUE)
                mlme_addba_setting(_PktInfo,WLAN_STATUS_SUCCESS);
            else
                mlme_addba_setting(_PktInfo,WLAN_STATUS_UNSPECIFIED_FAILURE);//WLAN_STATUS_SUCCESS);
            break;

        case WLAN_ACTION_ADDBA_RESP://for AMPDU TX
            {
#if (N_MODE_AMPDU_TX == 1)
                u16 u16_capab = p_mgmt_hdr->u.action.u.addba_resp.capab;
                u16 u16_ba_policy, u16_tid;
                printf("ADDBA_RESP for TX\n");

                //Recving addba resp to let us know if amodu tx seesion is created.
                if(p_mgmt_hdr->u.action.u.addba_resp.dialog_token < p_sta->ampdu_token){
                   printf("got small token:%d\n", p_mgmt_hdr->u.action.u.addba_resp.dialog_token);
                   break;
                }

                u16_ba_policy = (u16_capab & IEEE80211_ADDBA_PARAM_POLICY_MASK) >> 1;
	            u16_tid = (u16_capab & IEEE80211_ADDBA_PARAM_TID_MASK) >> 2;


                if(u16_tid != 0 ||
                    u16_ba_policy != BA_POLICY_IMMEDIATE){

                    printf("wrong tid:%d or ba policy:%d\n", u16_tid, u16_ba_policy);
                    break;
                }

                if(WLAN_STATUS_SUCCESS != p_mgmt_hdr->u.action.u.addba_resp.status){
                   printf("status code:%dsend req again\n",
                        p_mgmt_hdr->u.action.u.addba_resp.status);
                   //launch another request
                    if (AMPDU_TX_ENABLE == 1)
                    {
                        if ((AMPDU_TX_BlockSessionAtNotNmodeRate == 1) && (rate_control_pid_is_nmode_rate(u8_wsid) == FALSE))
                            break;

                        ampdu_addba_req(p_sta, (u8 *)&p_mgmt_hdr->sa,
                                (u8 *)&p_mgmt_hdr->da, (u8 *)&p_mgmt_hdr->bssid);
                    }

                   break;
                }

                //Finally, this station can use ammpdu frames to send.
                p_sta->ampdu_tx = TRUE;
                rate_control_set_defaul_rates_by_sta(u8_wsid);

                //Send BAR to get BA
                ampdu_send_bar((u8 *)&p_mgmt_hdr->sa, (u8 *)&p_mgmt_hdr->da, p_sta->ampdu_tx_ssn);
#endif //#if (N_MODE_AMPDU_TX == 1)
            }
			break;

		case WLAN_ACTION_DELBA://for AMPDU TX/RX
            {
                u16 u16_tid, u16_params;
    	        u16 u16_initiator;


                u16_params = (p_mgmt_hdr->u.action.u.delba.params);
    	        u16_tid = (u16_params & IEEE80211_DELBA_PARAM_TID_MASK) >> 12;
    	        u16_initiator = (u16_params & IEEE80211_DELBA_PARAM_INITIATOR_MASK) >> 11;

                printf("DELBA for %s\n", (u16_initiator == WLAN_BACK_INITIATOR)?"RX":"TX");

                //For now we just support tid0
                if(u16_tid != 0)
                    break;

                //ignore ampdu rx belba
                if(u16_initiator == WLAN_BACK_INITIATOR)
                    break;

                //opps, ampdutx tx session decline by station, reconnect again.

#if (N_MODE_AMPDU_TX == 1)
                p_sta->ampdu_tx = FALSE;
                rate_control_set_defaul_rates_by_sta(u8_wsid);

                printf("session tear down, reconnect again\n");

                //launch another request
                if (AMPDU_TX_ENABLE == 1)
                {
                    if ((AMPDU_TX_BlockSessionAtNotNmodeRate == 1) && (rate_control_pid_is_nmode_rate(u8_wsid) == FALSE))
                        break;

                    ampdu_addba_req(p_sta, (u8 *)&p_mgmt_hdr->sa,
                                (u8 *)&p_mgmt_hdr->da, (u8 *)&p_mgmt_hdr->bssid);
                }
#endif

              }
            break;

		default:
			break;
		}
		break;
	case WLAN_CATEGORY_SPECTRUM_MGMT:
		break;
	case WLAN_CATEGORY_SA_QUERY:
		break;
	case WLAN_CATEGORY_SELF_PROTECTED:
		break;
	case WLAN_CATEGORY_MESH_ACTION:
		break;
	}

    return MLME_OK;
}


void Fill_TxDescriptor(PKT_TxInfo *TxPkt,s32 fixed_field_length)
{
    /* Fill TxPkt descriptor */
    memset((void *)TxPkt, 0, sizeof(PKT_TxInfo));
    TxPkt->len          = HDR80211_MGMT_LEN+(u32)fixed_field_length;
    TxPkt->c_type       = M0_TXREQ;
    TxPkt->f80211       = 1;
    TxPkt->qos          = 0;
    TxPkt->ht           = 0;
    TxPkt->use_4addr    = 0;
    TxPkt->RSVD_0       = 0;
    TxPkt->more_data    = 0;
    TxPkt->crate_idx    = 0;
    TxPkt->drate_idx    = 0;
}
void Fill_Mgmt_FC(HDR80211_Mgmt *mgmt,u32 type,u32 stype)
{
    /*lint -save -e740 */
    *(u32 *)mgmt = 0;
    mgmt->fc.type =  _LE16(type>>4);
    mgmt->fc.subtype = _LE16(stype>>4);
    /*lint -restore */
}


u8* Fill_SSID( u8 *pos, const struct cfg_80211_ssid *ssid)
{
    *pos++ = ssid->ssid_len;
    memcpy(pos,ssid->ssid,ssid->ssid_len);
    pos += ssid->ssid_len;
    return pos;
}
#if(ADD_VENDOR_IE_IN_PROBE_REQUEST==1)
u8* Fill_VendorIE( u8 *pos, u32 len, u8 *data)
{
    *pos++ = len;
    memcpy(pos,data,len);
    pos += len;
    return pos;
}
#endif
u8* Fill_RSN( u8 *pos, DevInfo *Dev)
{
    //option: add RSN
    if((Dev->wpa_ssid.pairwise_cipher&WPA_CIPHER_TKIP) == WPA_CIPHER_TKIP ||
        (Dev->wpa_ssid.pairwise_cipher&WPA_CIPHER_CCMP) == WPA_CIPHER_CCMP)
    {
        //mgmt->u.auth.variable[0];
    	if (Dev->wpa_ie_len > 0)
            memcpy(pos, Dev->wpa_ie, (size_t)Dev->wpa_ie_len);
        pos += Dev->wpa_ie_len;
    }

    return pos;
}
u8* Fill_WMM_INFO(u8 *pos,struct ssv6xxx_ieee80211_bss *bss)
{
    struct wmm_information_element wmm_info_elem;

    *pos++ = WLAN_EID_VENDOR_SPECIFIC;
    *pos++ = sizeof(struct wmm_information_element);

    memset(&wmm_info_elem,0,sizeof(struct wmm_information_element));
    wmm_info_elem.oui[0]=0x00;
    wmm_info_elem.oui[1]=0x50;
    wmm_info_elem.oui[2]=0xF2;
    wmm_info_elem.oui_type=WMM_OUI_TYPE;
    wmm_info_elem.oui_subtype=WMM_OUI_SUBTYPE_INFORMATION_ELEMENT;
    wmm_info_elem.version=WMM_VERSION;

    if(bss->uapsd_supported)
    {
        //need to modify
        wmm_info_elem.qos_info=0x0f;
    }
    else
    {
        wmm_info_elem.qos_info=0;
    }
    memcpy(pos,&wmm_info_elem,sizeof(struct wmm_information_element));
    pos+=sizeof(struct wmm_information_element);


    return pos;

}
u8* Fill_HT_IE(u8 *pos,struct ssv6xxx_ieee80211_bss *bss)
{

    u16 cap;
    *pos++ = WLAN_EID_HT_CAP;
    *pos++ = sizeof(struct ieee80211_ht_cap);
    memset(pos, 0, sizeof(struct ieee80211_ht_cap));
    /* capability flags */
    cap=0x003c;
    memcpy(pos, &cap, sizeof(u16));
    pos += sizeof(u16);
    /* AMPDU parameters */
    //tempoary,need to move,consider that get the configuration from host
    gDevInfo->ampdu_factor=3;
    gDevInfo->ampdu_density=6;
    *pos++ = gDevInfo->ampdu_factor |
            (gDevInfo->ampdu_density <<
            IEEE80211_HT_AMPDU_PARM_DENSITY_SHIFT);
    /* MCS set */
    memset(&gDevInfo->mcs,0x00,sizeof(struct ieee80211_mcs_info));
    //tempoary,need to move
     gDevInfo->mcs.rx_mask[0]=0xFF;
    memcpy(pos, &gDevInfo->mcs, sizeof(gDevInfo->mcs));
    pos += sizeof(gDevInfo->mcs);

    /* extended capabilities */
    pos += sizeof(u16);

    /* BF capabilities */
    pos += sizeof(u32);

    /* antenna selection */
    pos += sizeof(u8);

    return pos;
}

inline u8* Fill_ht_cap_ie(u8 *pos,u8 *ht_cap_ie,u32 ie_len)
{
    *pos++ = WLAN_EID_HT_CAP ;
    *pos++ = ie_len;
    memcpy(pos,(const void *)ht_cap_ie,ie_len);
    pos += ie_len;
    return pos;
}
void mlme_mgmt_send(PKT_TxInfo *TxPkt)
{
    HDR80211_Mgmt *mgmt;
    mgmt = (HDR80211_Mgmt *)((u8 *)TxPkt + drv_mac_get_pbuf_offset());
    //mgmt->fc.toDS = 1;
    mgmt->seq_ctrl = sg_mgmt_tx_seqno;
    sg_mgmt_tx_seqno += 0x10;
    if(-1==TX_FRAME((u32)TxPkt))
    {
        PBUF_MFree((void *)TxPkt);
        printf("mgmt send fail\n");
    }
}


extern s32 drv_mailbox_cpu_ff(u32 pktmsg, u32 hw_number);
u16 g_null_frame_seq=1;
s32 mlme_null_send(DevInfo *dev, bool ps_on, bool edca_direct)
{
#if 1
    //u32 size = sizeof(HDR80211_Data)+sizeof(PKT_TxInfo);
    PKT_TxInfo *TxPkt;
    HDR80211_Data *data;
    u16 *fc;
    ETHER_ADDR	bssid;

    if (dev->State == STA_STATE_UNAUTH_UNASSOC)
    	return -1;
    GET_BSSID(&bssid);
    TxPkt = (PKT_TxInfo *)PBUF_MAlloc((sizeof(HDR80211_Data)+sizeof(PKT_TxInfo)) ,SOC_PBUF);
    if (TxPkt == NULL){
        //printf("\33[31m%s:allocate pbuf fail\33[0m\r\n",__FUNCTION__);
        return -1;
    }
    data = (HDR80211_Data *)((u8 *)TxPkt + drv_mac_get_pbuf_offset());
    memcpy(&data->addr1, bssid.addr, ETHER_ADDR_LEN);
    memcpy(&data->addr2, dev->info.addr.addr, ETHER_ADDR_LEN);
    memcpy(&data->addr3, bssid.addr, ETHER_ADDR_LEN);

    fc = (u16 *)&data->fc;
    *fc = M_FC_TODS | FT_DATA | FST_NULLFUNC;//FST_QOS_NULLFUNC;

    data->fc.PwrMgmt=(ps_on)?1:0;
    data->seq_ctrl = (g_null_frame_seq++) <<4;

     Fill_TxDescriptor(TxPkt, 0);

    if(!edca_direct)
    {
        if(-1==TX_FRAME((u32)TxPkt))
        {
            PBUF_MFree((void *)TxPkt);
            //printf("mlme_null_send fail\n");
            return -1;
        }
        //return 0;
    }
    else
    {
        data->dur = 314;
    	TxPkt->f80211 	  = 1;
        TxPkt->hdr_offset = drv_mac_get_pbuf_offset();
        TxPkt->unicast    = 1;
        TxPkt->hdr_len    = 24;
		TxPkt->fCmd       = M_ENG_TX_MNG;
		TxPkt->fCmdIdx    = 0;
        TxPkt->payload_offset = TxPkt->hdr_offset+ TxPkt->hdr_len;
        //hex_dump((void *)TxPkt,24);
        //if(FALSE==ENG_MBOX_SEND(M_ENG_TX_MNG,(u32)TxPkt))
        if(FALSE==drv_mailbox_send(M_ENG_TX_MNG,(u32)TxPkt))
        {
            PBUF_MFree(TxPkt);
            //printf("xx\n");
            return -1;
        }
    }
    return 0;
#endif
}

#if(ADD_VENDOR_IE_IN_PROBE_REQUEST==1)
struct vendor_info{
    u8 info[10];
};
#endif
s32 mlme_send_probe_req(u8 *da,u8 n_ssids,const struct cfg_80211_ssid *ssid)
{
    const Mlme_WorkJobReq *WorkJobReq =(Mlme_WorkJobReq *)(gDevInfo)->MlmeReqParams;
    SSV6XXX_STA		*sta=gDevInfo->connected_sta_ptr;
    PKT_TxInfo *TxPkt;
    HDR80211_Mgmt *mgmt;
    u8 *pos;
    const u8 *tmp_pos;
    size_t size; //,i,increased_len=0;
    s32 ret = MLME_OK;
#if(ADD_VENDOR_IE_IN_PROBE_REQUEST==1)
    struct vendor_info vinfo;
#endif

  
    /*
      *@SSID:
      *  -------------------------------------------
      * |1byte               |1byte              |sizeof(SSID_1)   |
      * --------------------------------------------
      * |WLAN_EID_SSID|Length_1         |SSID                 |=>IE_ID_FIELD_LEN+IE_LEN_FIELD_LEN+sizeof(struct cfg_80211_ssid)
      *  -------------------------------------------
      * @Support rate
      *  -------------------------------------------------------------------------
      * |1byte               |1byte              |1byte               |1byte              |1byte                 |
      * --------------------------------------------------------------------------
      * |WLAN_EID_SUPP
          _RATES             |Length             |Rate1               |Rate2              |Rate3               |...=>IE_ID_FIELD_LEN+IE_LEN_FIELD_LEN+sizeof(gBitRates)
      *  -------------------------------------------------------------------------
      */

#if(ADD_VENDOR_IE_IN_PROBE_REQUEST==1)
    size = sizeof(HDR80211_Mgmt) +(sizeof(struct cfg_80211_ssid))+sizeof(gBitRates)+sizeof(struct vendor_info)+3*(IE_ID_FIELD_LEN+IE_LEN_FIELD_LEN)+IE_LEN_FIELD_LEN;
#else
    size = sizeof(HDR80211_Mgmt) +(sizeof(struct cfg_80211_ssid))+sizeof(gBitRates)+3*(IE_ID_FIELD_LEN+IE_LEN_FIELD_LEN);
#endif
    TxPkt = (PKT_TxInfo *)PBUF_MAlloc(size, SOC_PBUF);
    if (TxPkt == NULL){
        printf("\33[31m%s:allocate pbuf fail\33[0m\r\n",__FUNCTION__);
        ret = MLME_FAILED;
        goto out;
    }

    //ASSERT_RET(TxPkt, MLME_FAILED);

    // IEEE 802.11 MGMT frame header
    mgmt = (HDR80211_Mgmt *)((u8 *)TxPkt + drv_mac_get_pbuf_offset());
    Fill_Mgmt_FC(mgmt,FT_MGMT,FST_PROBE_REQ);

    if(da==NULL){
        memset(&mgmt->da, 0xFF, ETHER_ADDR_LEN);
        memset(&mgmt->bssid, 0xFF, ETHER_ADDR_LEN);
    }else{
          
        memcpy((void *)&mgmt->da, (void *)da, ETHER_ADDR_LEN);
        memcpy((void *)&mgmt->bssid, (void *)da, ETHER_ADDR_LEN);
    }
    // Ensure MAC is available in dev
    drv_mac_get_sta_mac(gDevInfo->info.addr.addr);
    memcpy(&mgmt->sa, &gDevInfo->info.addr, ETHER_ADDR_LEN);
    


    pos = (u8 *)mgmt + HDR80211_MGMT_LEN;
    tmp_pos=pos;

#if(ADD_VENDOR_IE_IN_PROBE_REQUEST==1)
    *pos++ = WLAN_EID_VENDOR_SPECIFIC;
    vinfo.info[0]=0;
    vinfo.info[1]=0;
    vinfo.info[2]=0;
    vinfo.info[3]=WorkJobReq->u.scan.chidx;
    vinfo.info[4]=0;
    vinfo.info[5]=0;
    vinfo.info[6]=0;
    vinfo.info[7]=0;
    vinfo.info[8]=0;
    vinfo.info[9]=0;

    pos=Fill_VendorIE(pos,sizeof(struct vendor_info),(u8 *)&vinfo);
#endif

    //Fill SSID
    *pos++ = WLAN_EID_SSID;
    if(n_ssids==0)
    {
        *pos++ =0;
    }
    else
    {
        pos=Fill_SSID(pos, ssid);
    }

    //Fill support rate
    //BitRate_BitMask2IE(0x00000FFF, 0x0000015F, pos);//1,2,5,5,6,11,12, and 24 are basic rates
    BitRate_BitMask2IE(0x0000000F, 0x00000000, pos,WLAN_EID_SUPP_RATES);
    pos += (pos[1]+2);

    //Fill extend support rate
    BitRate_BitMask2IE(0x00000FF0, 0x00000000, pos,WLAN_EID_EXT_SUPP_RATES);
    pos += (pos[1]+2);

    //Fill HT capability IE
    if(WorkJobReq!=NULL)
    {
    pos=Fill_ht_cap_ie(pos, (u8 *)WorkJobReq->u.scan.ht_cap_ie,sizeof(WorkJobReq->u.scan.ht_cap_ie));
    }
    else
    {
        if(sta!=NULL)
        {
            pos=Fill_ht_cap_ie(pos, (u8 *)sta->ht_cap_ie,sizeof(sta->ht_cap_ie));
        }
    }

    //Fill TxPkt descriptor
    Fill_TxDescriptor(TxPkt,(pos-tmp_pos));

    // Extended Supported Rate
    /*BitRate_BitMask2IE(sta->info.bit_rates&0xFFFFFF00, false, pos);
    TxPkt->len += (pos[1]+2);
    pos += (pos[1]+2);*/


    //TX_FRAME(TxPkt);
    mlme_mgmt_send(TxPkt);

out:

    return ret;
}

inline u8* Fill_support_rate(u8 *pos,u32 rates,u32 rates_len)
{
    printf("fill_support_rate: rates=0x%x ,rates_len=%d \n",rates,rates_len);

    u32 count,i;
    u32 temp_rates_len;
    //Fill support rate
    /* add all rates which were marked to be used above */
    temp_rates_len = rates_len;
    if (temp_rates_len > 8)
        temp_rates_len = 8;

    //pos = skb_put(skb, supp_rates_len + 2);
    *pos++ = WLAN_EID_SUPP_RATES;
    *pos++ = temp_rates_len;

    count = 0;
    for (i = 0; i < sizeof(gBitRates)/sizeof(u16); i++) {
        if (BIT(i) & rates) { // 4 byte aliment problem?
            int rate = gBitRates[i];
            *pos++ = (u8) (rate / 5); //units = 500 kb/s.
            if (++count == 8)   //only can set 8 number of support rate
                break;
        }
    }

    //Fill extend support rate
    if (rates_len > count) {
        //pos = skb_put(skb, rates_len - count + 2);
        *pos++ = WLAN_EID_EXT_SUPP_RATES;
        *pos++ = rates_len - count;

        for (i++; i < sizeof(gBitRates)/sizeof(u16); i++) {
            if (BIT(i) & rates) {
                int rate = gBitRates[i];
                *pos++ = (u8) (rate / 5); //units = 500 kb/s.
            }
        }
    }
    return pos;
}

#define SIZE_OF_SUPPORTED_MCS_SET_ELEMENT 16
#define SIZE_OF_HT_CAPABILITIES_INFO_ELEMENT 2
s32 mlme_send_assoc_req(void)
{
    const Mlme_WorkJobReq *WorkJobReq;
    PKT_TxInfo *TxPkt;
    HDR80211_Mgmt *mgmt;

    size_t size;
    u8  *pos;
    const u8 *tmp_pos;

    //for rc setting
    //struct cfg_set_sta ap_rc_info;
    //memset(&ap_rc_info,0,sizeof(struct cfg_set_sta));
    //ap_rc_info.sta_oper=CFG_STA_ADD;

    MLME_WORKJOB_STRUCT(WorkJobReq,gDevInfo);

#if 1   //collect support rate
    u32 rates_len;
    u16 rates = 0;

    if (WorkJobReq->u.join.bss.supp_rates_len) {
		/*
		 * Get all rates supported by the device and the AP as
		 * some APs don't like getting a superset of their rates
		 * in the association request (e.g. D-Link DAP 1353 in
		 * b-only mode)...
		 */
        rates=assoc_req_rate.rate_set;
        rates_len=assoc_req_rate.rate_set_len;

        //rates=gDevInfo->connected_sta_ptr->rate_set;
        //rates_len =gDevInfo->connected_sta_ptr->rate_set_len;


	} else {
		/*
		 * In case AP not provide any supported rates information
		 * before association, we send information element(s) with
		 * all rates that we support.
		 */
		rates = ~0;
		rates_len = sizeof(gBitRates)/sizeof(u16);
	}

    //ap_rc_info.non_ht_supp_rates=rates;
    size = sizeof(HDR80211_Mgmt) +sizeof(struct cfg_80211_ssid)+rates_len+3*(IE_ID_FIELD_LEN+IE_LEN_FIELD_LEN);
#else
    size = sizeof(HDR80211_Mgmt) +sizeof(struct cfg_80211_ssid)+sizeof(gBitRates)+3*(IE_ID_FIELD_LEN+IE_LEN_FIELD_LEN);
#endif

    //option size for RSN IE
    if((gDevInfo->wpa_ssid.pairwise_cipher&WPA_CIPHER_TKIP) == WPA_CIPHER_TKIP ||
        (gDevInfo->wpa_ssid.pairwise_cipher&WPA_CIPHER_CCMP) == WPA_CIPHER_CCMP)
    {
        size += gDevInfo->wpa_ie_len;
    }
    //option size for WMM IE
    if(WorkJobReq->u.join.bss.wmm_used)
    {

        size+=sizeof(struct wmm_information_element)+IE_ID_FIELD_LEN+IE_LEN_FIELD_LEN;
    }
	//option size for HT
	if(WorkJobReq->u.join.bss.ht_used)
    {

        size+=sizeof(struct ssv_ht_info)+IE_ID_FIELD_LEN+IE_LEN_FIELD_LEN;
    }
    size += WorkJobReq->u.join.assoc_ie_len;

    TxPkt = (PKT_TxInfo *)PBUF_MAlloc(size ,SOC_PBUF);
    //ASSERT_RET(TxPkt, MLME_FAILED);
    if(TxPkt==NULL){
        printf("\33[31m%s:allocate pbuf fail\33[0m\r\n",__FUNCTION__);
        Join_action(MLME_WORK_JOIN_ASSOC_RESP,NULL,0);
        return MLME_OK;
    }

    // IEEE 802.11 MGMT frame header
    mgmt = (HDR80211_Mgmt *)((u8 *)TxPkt + drv_mac_get_pbuf_offset());
    Fill_Mgmt_FC(mgmt,FT_MGMT,FST_ASSOC_REQ);

    memcpy(&mgmt->da, WorkJobReq->u.join.bss.bssid.addr, ETHER_ADDR_LEN);
    memcpy(&mgmt->sa, &gDevInfo->info.addr, ETHER_ADDR_LEN);
    memcpy(&mgmt->bssid,WorkJobReq->u.join.bss.bssid.addr , ETHER_ADDR_LEN);

    //Fill capability IE
    if(WorkJobReq->u.join.bss.capab_info&BIT(4))
    {   //check AP's privacy
        mgmt->u.assoc_req.capab_info=_LE16(0x031);
    }
    else
    {
        mgmt->u.assoc_req.capab_info=_LE16(0x021);
    }

    mgmt->u.assoc_req.listen_interval=_LE16(0x0064);


    //Fill SSID
    pos = (u8 *)mgmt + HDR80211_MGMT_LEN+4;
    tmp_pos=pos;
    *pos++ = WLAN_EID_SSID;

    pos=Fill_SSID(pos, &WorkJobReq->u.join.bss.ssid);

#if 1
    //Fill support rate
    pos = Fill_support_rate(pos,rates,rates_len);

#else
    //support rate
    //BitRate_BitMask2IE(0x00000FFF, 0x0000015F, pos);//1,2,5,5,6,11,12, and 24 are basic rates
    BitRate_BitMask2IE(0x00000B8F, 0x0000010f, pos, WLAN_EID_SUPP_RATES);//1,2,5,5,6,11,12, and 24 are basic rates
    pos += (pos[1]+2);
    //extend support rate
    BitRate_BitMask2IE(0x00000470, 0x00000050, pos, WLAN_EID_EXT_SUPP_RATES);//1,2,5,5,6,11,12, and 24 are basic rates
    pos += (pos[1]+2);
#endif

    // RSN
    pos = Fill_RSN(pos, gDevInfo);

    //Fill HT capability IE
    if(WorkJobReq->u.join.bss.ht_used && WorkJobReq->u.join.ht_supported) {
       pos=Fill_ht_cap_ie(pos, (u8 *)WorkJobReq->u.join.ht_cap_ie,sizeof(WorkJobReq->u.join.ht_cap_ie));

       //rc :get ht capabilities info
       //memcpy((void*)&ap_rc_info.ht_capabilities.ht_capabilities_info,(void*)WorkJobReq->u.join.ht_cap_ie,SIZE_OF_HT_CAPABILITIES_INFO_ELEMENT);
       //memcpy((void*)&ap_rc_info.ht_capabilities.ht_capabilities_info,(void*)&WorkJobReq->u.join.bss.ht_cap.cap_info,SIZE_OF_HT_CAPABILITIES_INFO_ELEMENT);

       //rc :get supported mcs set
       //memcpy((void*)ap_rc_info.ht_capabilities.supported_mcs_set,(void*)&WorkJobReq->u.join.ht_cap_ie[3],SIZE_OF_SUPPORTED_MCS_SET_ELEMENT);
       //memcpy((void*)ap_rc_info.ht_capabilities.supported_mcs_set,(void*)&WorkJobReq->u.join.bss.ht_cap.mcs,SIZE_OF_SUPPORTED_MCS_SET_ELEMENT);
    }

    //Fill WMM
    if(WorkJobReq->u.join.bss.wmm_used){
        pos = Fill_WMM_INFO(pos, (struct ssv6xxx_ieee80211_bss *)&WorkJobReq->u.join.bss);
    }

    // Any IE from join request.
    if (WorkJobReq->u.join.assoc_ie_len > 0){
    	memcpy(pos, WorkJobReq->u.join.assoc_ie, WorkJobReq->u.join.assoc_ie_len);
    	pos += WorkJobReq->u.join.assoc_ie_len;
    }

    //set ap rc_info short_preamble
 //   if (WorkJobReq->u.join.bss.capab_info & WLAN_CAPABILITY_SHORT_PREAMBLE) {
//			ap_rc_info.short_preamble=1;
//	}
//    else{
//            ap_rc_info.short_preamble=0;
//    }

    //Fill TxPkt descriptor
    Fill_TxDescriptor(TxPkt,4+(pos-tmp_pos));//4:Capab_info(2 bytes)+Listen_interval(2 bytes)
    //TX_FRAME(TxPkt);

    mlme_mgmt_send(TxPkt);

    //WorkJobReq->work_type=MLME_WORK_JOIN_ASSOC_RESP;
    Join_action(MLME_WORK_JOIN_ASSOC_RESP,NULL,0);
    return MLME_OK;
}

#if 0
static s32 mlme_send_disassoc( DevInfo *Dev )
{
    const Mlme_WorkJobReq *WorkJobReq;
    PKT_TxInfo *TxPkt;
    HDR80211_Mgmt *mgmt;

    size_t size;

    MLME_WORKJOB_STRUCT(WorkJobReq,Dev);
    size = sizeof(HDR80211_Mgmt);
    TxPkt = (PKT_TxInfo *)PBUF_MAlloc(size ,TX_BUF);
    ASSERT_RET(TxPkt, MLME_FAILED);

    // IEEE 802.11 MGMT frame header
    mgmt = (HDR80211_Mgmt *)((u8 *)TxPkt + drv_mac_get_pbuf_offset());
    Fill_Mgmt_FC(mgmt,FT_MGMT,FST_DISASSOC);

    memcpy(&mgmt->da, WorkJobReq->u.disassoc.bssidx, ETHER_ADDR_LEN);
    memcpy(&mgmt->sa, &Dev->info.addr, ETHER_ADDR_LEN);
    memcpy(&mgmt->bssid,WorkJobReq->u.disassoc.bssidx , ETHER_ADDR_LEN);
    mgmt->u.disassoc.reason_code=_LE16(1);//�D??ucw

    // Fill TxPkt descriptor
    Fill_TxDescriptor(TxPkt,2);//2:reason_code(2 bytes)
    //TX_FRAME(TxPkt);
    mlme_mgmt_send(TxPkt);

    return MLME_OK;
}
#endif
s32 mlme_send_auth(u16 trans, u32 *extra_data, u32 extra_data_len)
{
    Mlme_WorkJobReq *WorkJobReq;
    PKT_TxInfo *TxPkt=NULL;
    HDR80211_Mgmt *mgmt;
    size_t size;
    MLME_WORKJOB_STRUCT(WorkJobReq,gDevInfo);
    //????size
    size = sizeof(HDR80211_Mgmt) + extra_data_len;

    TxPkt = (PKT_TxInfo *)PBUF_MAlloc(size ,SOC_PBUF);
    //ASSERT_RET(TxPkt, MLME_FAILED);
    if(TxPkt==NULL){
        printf("\33[31m%s:allocate pbuf fail.trans=%d\33[0m\r\n",__FUNCTION__,trans);
	WorkJobReq->u.join.auth_transaction = trans+1;
        Join_action((trans ==1)?MLME_WORK_JOIN_RX_AUTH_2:MLME_WORK_JOIN_RX_AUTH_4,NULL,0);
        return MLME_OK;
    }

    // IEEE 802.11 MGMT frame header
    mgmt = (HDR80211_Mgmt *)((u8 *)TxPkt + drv_mac_get_pbuf_offset());
    Fill_Mgmt_FC(mgmt,FT_MGMT,FST_AUTH);

    memcpy(&mgmt->da, WorkJobReq->u.join.bss.bssid.addr, ETHER_ADDR_LEN);
    memcpy(&mgmt->sa, &gDevInfo->info.addr, ETHER_ADDR_LEN);
    memcpy(&mgmt->bssid, WorkJobReq->u.join.bss.bssid.addr , ETHER_ADDR_LEN);

    mgmt->u.auth.auth_algo = _LE16( gDevInfo->wpa_ssid.auth_alg==WPA_AUTH_ALG_OPEN? WLAN_AUTH_OPEN:WLAN_AUTH_SHARED_KEY);
    mgmt->u.auth.trans_id = _LE16(trans);
    mgmt->u.auth.status_code = _LE16(0);

    WorkJobReq->u.join.auth_transaction = trans+1;


    if( trans == 3)
    {
        mgmt->fc.Protected = 1;//Allow into security module
        TxPkt->security = 1;

        // TxPkt->extra_info = 1; //Trap to cpu to modify tx mgmt flow

        //if(extra_data)
        memcpy(&mgmt->u.auth.variable[0], extra_data, extra_data_len);
    }

    // Fill TxPkt descriptor
    Fill_TxDescriptor(TxPkt,6+extra_data_len);//6:Auth

    mlme_mgmt_send(TxPkt);


    Join_action((trans ==1)?MLME_WORK_JOIN_RX_AUTH_2:MLME_WORK_JOIN_RX_AUTH_4,NULL,0);
    return MLME_OK;
}

s32 mlme_send_deauth(u16 reason ,struct ETHER_ADDR_st* sa,struct ETHER_ADDR_st* da,struct ETHER_ADDR_st* bssid, bool send_event)
{
    PKT_TxInfo *TxPkt;
    HDR80211_Mgmt *mgmt;
    size_t size;
    printf("Send deauth ... \n");
    //????size
    size = sizeof(HDR80211_Mgmt);
    TxPkt = (PKT_TxInfo *)PBUF_MAlloc(size ,SOC_PBUF);
    if(TxPkt==NULL)
    {
        printf("\33[31m%s:allocate pbuf fail\33[0m\r\n",__FUNCTION__);
        return MLME_FAILED;
    }

    // IEEE 802.11 MGMT frame header
    mgmt = (HDR80211_Mgmt *)((u8 *)TxPkt + drv_mac_get_pbuf_offset());
    Fill_Mgmt_FC(mgmt,FT_MGMT,FST_DEAUTH);

    //GET_BSSID(&mgmt->da);
    memcpy(&mgmt->da, da, ETHER_ADDR_LEN);
    //memcpy(&mgmt->da, WorkJobReq->u.deauth.bssidx, ETHER_ADDR_LEN);
    //memcpy(&mgmt->sa, &gDevInfo->info.addr, ETHER_ADDR_LEN);
    memcpy(&mgmt->sa, sa, ETHER_ADDR_LEN);
    //GET_BSSID(&mgmt->bssid);
    memcpy(&mgmt->bssid, bssid, ETHER_ADDR_LEN);
    //memcpy(&mgmt->bssid,WorkJobReq->u.deauth.bssidx , ETHER_ADDR_LEN);

    mgmt->u.deauth.reason_code=reason;

    // Fill TxPkt descriptor
    Fill_TxDescriptor(TxPkt,2);//2:reason_code(2 bytes)
    //TX_FRAME(TxPkt);
    mlme_mgmt_send(TxPkt);
 #ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
    if(supplicant_bypass)
    {
        ;
    }
    else
    {
        //tmp-mark-supplicant
        if(send_event)
            wpa_supplicant_event(gDevInfo->pSupplicant, EVENT_DEAUTH, (union wpa_event_data *)NULL);
    }
#endif


    return MLME_OK;
}



void mlme_timer_handler( void *args )
{
    /**
        *  MLME Timer timeout. Reschedule a message.
        */
    MsgEvent *MsgEv=(MsgEvent *)OS_TimerGetData((OsTimer)args);
    ASSERT(OS_SUCCESS == msg_evt_post(MBOX_MLME, MsgEv));
}


s32 mlme_send_null_frame(bool ps_on)
{
#define REPEAT_TIME 3
    u8 total=0;
    u8 count=0;
    u8 fail=0;
    //mlme_null_send(gDevInfo, ps_on);
    //mlme_null_send(gDevInfo, ps_on);
    //mlme_null_send(gDevInfo, ps_on);
    //OS_MsDelay(1);
    total=(ps_on)?(REPEAT_TIME+2):REPEAT_TIME;
    for(count=0;count<total;count++){
        if(0!=mlme_null_send(gDevInfo, ps_on, 0)){
            fail++;
        }
    }


    if(fail==total){
        return -1;
    }else{
        OS_MsDelay(1);
        return 0;
    }
}

void get_bss_info(struct ieee802_11_elems *elems,struct resp_evt_result *Scan_Resp)
{
    struct ssv6xxx_ieee80211_bss *bss = &Scan_Resp->u.scan.bss_info;
    struct wpa_ie_data wpa_ie_data;
	struct ssv_ht_info ht_info;
	memset(&wpa_ie_data,0x00,sizeof(struct wpa_ie_data));
	memset(&ht_info,0x00,sizeof(struct ssv_ht_info));

    if(elems->ssid && elems->ssid_len>0)
    {
        if (elems->ssid_len > MAX_SSID_LEN)
        {
            MLME_WARN("%s: Invalid SSID length %d.\n", __FUNCTION__, elems->ssid_len);
            return;
        }
        memcpy(bss->ssid.ssid,elems->ssid, elems->ssid_len);
        //bss->ssid.ssid[elems->ssid_len] = 0;
        bss->ssid.ssid_len=elems->ssid_len;
    }
    if (elems->ds_params && elems->ds_params_len == 1)
        Scan_Resp->u.scan.bss_info.channel_id = elems->ds_params[0];
    /* save the ERP value so that it is available at association time */
    if (elems->erp_info && elems->erp_info_len >= 1) {
        bss->erp_value = elems->erp_info[0];
        bss->has_erp_value = 1;
    }
    if (elems->tim) {
        bss->dtim_period = elems->tim[1];
    }
    /* If the beacon had no TIM IE, or it was invalid, use 1 */
    if (!bss->dtim_period)
        bss->dtim_period = 1;
    if(elems->wmm_parameter && elems->wmm_parameter_len>0)
    {
        bss->wmm_used=1;
        bss->uapsd_supported=(elems->wmm_parameter[6]) >> 7;
        bss->parameter_set_count=(elems->wmm_parameter[6]) & 0x07;
    }
#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
	//RSN
	if(elems->rsn_ie!=NULL)
		wpa_parse_wpa_ie_rsn(elems->rsn_ie,elems->rsn_ie_len,&wpa_ie_data);
	else
	{
		if(elems->wpa_ie!=NULL)
			wpa_parse_wpa_ie_wpa(elems->wpa_ie,elems->wpa_ie_len,&wpa_ie_data);
	}
#endif
	//printf("pairwise_cipher=[%x] group_cipher=[%x]\n",wpa_ie_data.pairwise_cipher,wpa_ie_data.group_cipher);
	bss->group_cipher=wpa_ie_data.group_cipher;
	bss->pairwise_cipher[0]=wpa_ie_data.pairwise_cipher;
    bss->proto=wpa_ie_data.proto;

/*
	Avoid Get DHCP fail(WRT610)...Joy
	Some bug here
*/

    /*record HT operation IE for host using*/
	 if (elems->ht_capabilities!=NULL) {
	 	 bss->ht_used=1;
		 memcpy(&bss->ht_cap,elems->ht_capabilities,sizeof(struct ssv_ht_cap));
    }


    /*record support rates */
    u32 clen, srlen;
    srlen = 0;
    if (elems->supp_rates) {
        clen = IEEE80211_MAX_SUPP_RATES;
        if (clen > elems->supp_rates_len)
            clen = elems->supp_rates_len;

        memcpy(bss->supp_rates, elems->supp_rates, clen);
        srlen += clen;
    }

    if (elems->ext_supp_rates) {
        clen = IEEE80211_MAX_SUPP_RATES - srlen;
        if (clen > elems->ext_supp_rates_len)
            clen = elems->ext_supp_rates_len;

        memcpy(bss->supp_rates + srlen, elems->ext_supp_rates,clen);
        srlen += clen;
    }

    if (srlen) {
        bss->supp_rates_len = srlen;

    }

}

#if 0
s32 MLME_ADDBA_RESP( struct cfg_addba_resp *cfg_addba_resp, u32 cmd_seq_no)
{

	u32 size;
	PKT_TxInfo *TxPkt;
	HDR80211_Mgmt *mgmt;
	u16 BA_param_set,tid;
	const u32   *ba_ta_31_0 ;
	const u16   *ba_ta_47_32;
	size = sizeof(HDR80211_Mgmt) ;


	TxPkt = (PKT_TxInfo *)PBUF_MAlloc(size ,SOC_PBUF);
    ASSERT_RET(TxPkt, MLME_FAILED);

	//Setting Reg for BA
	ba_ta_31_0 = (u32 *)(gDevInfo->connected_sta_ptr->addr.addr+0);
	ba_ta_47_32 = (u16 *)(gDevInfo->connected_sta_ptr->addr.addr+4);
	SET_BA_TA_31_0(*ba_ta_31_0);
	SET_BA_TA_47_32(*ba_ta_47_32);
	tid=cfg_addba_resp->tid;
	SET_BA_TID(tid);
  	SET_BA_ST_SEQ(cfg_addba_resp->start_seq_num);
	printf("cfg_addba_resp->tid=%d\n",cfg_addba_resp->tid);
	SET_BA_SB0(0);
	SET_BA_SB1(0);
	SET_BA_CTRL(3);
	SET_BA_AGRE_EN(1);
    // IEEE 802.11 MGMT frame header
    mgmt = (HDR80211_Mgmt *)((u8 *)TxPkt + drv_mac_get_pbuf_offset());
    Fill_Mgmt_FC(mgmt,FT_MGMT,FST_ACTION);



    // Fill SA,DA,BSSID
    drv_mac_get_sta_mac(gDevInfo->info.addr.addr);
    memcpy(&mgmt->sa, gDevInfo->info.addr.addr, ETHER_ADDR_LEN);
	memcpy(&mgmt->da, gDevInfo->connected_sta_ptr->addr.addr, ETHER_ADDR_LEN);
    memcpy(&mgmt->bssid, gDevInfo->connected_sta_ptr->addr.addr, ETHER_ADDR_LEN);
	//Category
	mgmt->u.action.category = WLAN_CATEGORY_BACK;
	//Block Action
	mgmt->u.action.u.addba_resp.action_code = WLAN_ACTION_ADDBA_RESP;
	mgmt->u.action.u.addba_resp.dialog_token=cfg_addba_resp->dialog_token;
	//Block_Parameter_Set
	BA_param_set = (u16)(cfg_addba_resp->policy << 1);/* bit 1 aggregation policy */
	BA_param_set |= (u16)(tid << 2); /* bit 5:2 TID number */
	BA_param_set |= (u16)(cfg_addba_resp->buf_size<< 6);/* bit 15:6 max size of aggregation */
	printf("cfg_addba_resp->buf_size=%d\n",cfg_addba_resp->buf_size);
	mgmt->u.action.u.addba_resp.capab=BA_param_set;
	//Timeout
	mgmt->u.action.u.addba_resp.timeout=cfg_addba_resp->timeout;
	//Status_code
	mgmt->u.action.u.addba_resp.status=cfg_addba_resp->status;

	Fill_TxDescriptor(TxPkt,8);
	mlme_mgmt_send(TxPkt);
    return MLME_OK;
}

s32 MLME_DELBA( struct cfg_delba *cfg_delba, u32 cmd_seq_no)
{

	u32 size;
	PKT_TxInfo *TxPkt;
	HDR80211_Mgmt *mgmt;
	u16 params;
	size = sizeof(HDR80211_Mgmt) ;
	TxPkt = (PKT_TxInfo *)PBUF_MAlloc(size ,SOC_PBUF);
    ASSERT_RET(TxPkt, MLME_FAILED);

	// IEEE 802.11 MGMT frame header
    mgmt = (HDR80211_Mgmt *)((u8 *)TxPkt + drv_mac_get_pbuf_offset());
    Fill_Mgmt_FC(mgmt,FT_MGMT,FST_ACTION);



    // Fill SA,DA,BSSID
    drv_mac_get_sta_mac(gDevInfo->info.addr.addr);
    memcpy(&mgmt->sa, gDevInfo->info.addr.addr, ETHER_ADDR_LEN);
	memcpy(&mgmt->da, gDevInfo->connected_sta_ptr->addr.addr, ETHER_ADDR_LEN);
    memcpy(&mgmt->bssid, gDevInfo->connected_sta_ptr->addr.addr, ETHER_ADDR_LEN);




	mgmt->u.action.category = WLAN_CATEGORY_BACK;
	mgmt->u.action.u.delba.action_code = WLAN_ACTION_DELBA;
	params = (u16)(cfg_delba->initiator << 11);	/* bit 11 initiator */
	params|= (u16)(cfg_delba->tid << 12); 		/* bit 15:12 TID number */

	mgmt->u.action.u.delba.params = params;
	mgmt->u.action.u.delba.reason_code = cfg_delba->reason_code;
	Fill_TxDescriptor(TxPkt,5);
	mlme_mgmt_send(TxPkt);
    return MLME_OK;
}
#endif

#include "pide.h"
#ifdef PIDE_TEST
//125ms seconds to execute
extern u32 MTX_WSID_TOTAL;
extern u32 MTX_WSID_SUCC;
#endif
s32 mlme_init( void )
{
#ifdef PIDE_TEST
	int x,y;
	SSV6XXX_STA *sta;
	u8 tempRate,packetRate;
#endif

    //BSSInfo_Init();
    STAInfo_Init();
    sg_mgmt_tx_seqno = 0;
    init_psk_cache();
    rate_control_pid_init();


#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
	if(supplicant_bypass)
		;
	else
	{
    	//tmp-mark-supplicant
    	//gDevInfo[0].pSupplicant = NULL;
    	gDevInfo->pSupplicant = wpa_supplicant_init();
	}
#endif
#ifdef PIDE_TEST
	sta=&g6200StaInfo[0];
	sta->supp_rates = 0xffff;
	sta->preamble = PHY_MODE_SHORT_PREAMBLE;
	sta->networkMode = PHY_MODE_G_ONLY;
	rate_control_pid_sta_connect(0);
	{
		printf("****Trend down****\n");
		tempRate = 99;
		MTX_WSID_TOTAL = 100;
		MTX_WSID_SUCC = 100;
		for(x=0;x<120;x++)
		{
			for(y=0;y<20;y++)
			{
				rate_control_pid_tx_status();
				packetRate = rate_control_pid_get_rate(0,1,0);
				if(tempRate != packetRate)
				{
					tempRate = packetRate;
					printf("R[%d]Total[%d]Success[%d]\n",packetRate,MTX_WSID_TOTAL,MTX_WSID_SUCC);
				}
			}
			MTX_WSID_TOTAL++;
		}
	}
	rate_control_pid_sta_connect(0);
	{
		printf("****Upward trend****\n");
		tempRate = 99;
		MTX_WSID_TOTAL = 100;
		MTX_WSID_SUCC = 50;
		for(x=0;x<50;x++)
		{
			for(y=0;y<20;y++)
			{
				rate_control_pid_tx_status();
				packetRate = rate_control_pid_get_rate(0,1,0);
				if(tempRate != packetRate)
				{
					tempRate = packetRate;
					printf("R[%d]Total[%d]Success[%d]\n",packetRate,MTX_WSID_TOTAL,MTX_WSID_SUCC);
				}
			}
			MTX_WSID_SUCC++;
		}
	}
	while(1);
#endif

    return OS_SUCCESS;
}

void _mlme_sta_disconnect(u8 wsid)
{
    u8 invalid_bssid[MAC_ADDR_LEN]={0};

    StopStaMonitorTimer();

    //reset env setting
    /*
    We must check the "connected_sta_ptr" before using it, because we set 0 to "connected_sta_ptr" in the end of this function.
    If we don't check it first, it cause the system crash when we receive the two deauth frames continuelly.
    */
    if(gDevInfo->connected_sta_ptr!=NULL){
        gDevInfo->connected_sta_ptr->aid = 0;
        gDevInfo->aid=0;
        memset(&gDevInfo->connected_sta_ptr->addr,0,ETHER_ADDR_LEN);
        rate_control_set_erp_protect(FALSE);
        gDevInfo->connected_sta_ptr->ht_protect =0;
        gDevInfo->erp_protect = FALSE;
        gDevInfo->erp_barker_preamble = FALSE;
        gDevInfo->short_slot_time = 0;
        _set_short_slot_time(FALSE);
    }

    drv_mac_set_bssid(invalid_bssid);


    /*
    Release the data packets that are in software data queue.
    */
    ssv6xxx_SWQOutLock(0,TX_SW_QUEUE);
    ssv6xxx_SWQIntLock(0,TX_SW_QUEUE);
    flush_sw_queue(0,TX_SW_QUEUE);

    gDevInfo->State = STA_STATE_UNAUTH_UNASSOC;
    rate_control_pid_sta_disconnect(0);
    gDevInfo->connected_sta_ptr = NULL;

}


// PKT_TxInfo *TxPkt->need to perpare the memory of this packet firstly.
s32 mlme_send_addba_response(u8 *da, u16 tid, u8 dialog_token, u16 status, u16 policy, u16 fixed_field_length, u16 agg_buf_size, u16 timeout,PKT_TxInfo *TxPkt)
{
    HDR80211_Mgmt *mgmt;
    u16 capab;


//    if(FRMAE_COULD_BE_REUSED(TxPkt, HDR80211_MGMT_LEN+))
//    (sizeof(mgmt->u.action.u.addba_req)+IEEE80211_ACTION_CATEGORY_SIZE)


    // IEEE 802.11 MGMT frame header
    mgmt = (HDR80211_Mgmt *)((u8 *)TxPkt + drv_mac_get_pbuf_offset());


    Fill_Mgmt_FC(mgmt,FT_MGMT,FST_ACTION);

    memcpy(&mgmt->da, da, ETHER_ADDR_LEN);
    memcpy(&mgmt->sa, &gDevInfo->info.addr, ETHER_ADDR_LEN);
    if(drv_mac_get_op_mode()==WLAN_AP)
        memcpy(&mgmt->bssid, &gDevInfo->info.addr, ETHER_ADDR_LEN);
    else
        memcpy(&mgmt->bssid, da, ETHER_ADDR_LEN);

    mgmt->u.action.category = WLAN_CATEGORY_BACK;
    mgmt->u.action.u.addba_resp.action_code = WLAN_ACTION_ADDBA_RESP;
    mgmt->u.action.u.addba_resp.dialog_token = dialog_token;

    capab = (u16) (policy << 1); /* bit 1 aggregation policy */
    capab |= (u16) (tid << 2); /* bit 5:2 TID number */
    capab |= (u16) (agg_buf_size << 6); /* bit 15:6 max size of aggregation */

    mgmt->u.action.u.addba_resp.capab = _LE16 (capab);
    mgmt->u.action.u.addba_resp.timeout = _LE16 (timeout);
    mgmt->u.action.u.addba_resp.status = _LE16 (status);

    Fill_TxDescriptor(TxPkt, fixed_field_length);
    mlme_mgmt_send(TxPkt);    
    return MLME_OK;
}

void send_addba_response_to_host(u32 wsid, u16 tid, u16 ssn, u16 buf_size)
{
    // Send to cmd_engine
    HDR_HostEvent   *ev=NULL;
    u32 size = sizeof(struct cfg_addba_info);
    //HOST_EVENT_ALLOC(ev,SOC_EVT_GET_PMK , size);
    HOST_EVENT_ALLOC_0(ev,SOC_EVT_ADD_BA , size);

    struct cfg_addba_info *addbainfo = (struct cfg_addba_info *)HOST_EVENT_DATA_PTR(ev);
    memset((void *)addbainfo,0,size);
    addbainfo->wsid = wsid;
    addbainfo->tid = tid;
    addbainfo->ssn = ssn;
    addbainfo->buf_size = buf_size;
    HOST_EVENT_SET_LEN(ev, size);
    HOST_EVENT_SEND(ev);

}
extern u8 AMPDU_RX_BUF_SIZE;
void mlme_addba_setting(PKT_RxInfo **_PktInfo,u32 status)
{
#define FIXED_FIELD_LEN 9
        PKT_RxInfo *PktInfo=(PKT_RxInfo *)(*_PktInfo);
        u16 tid, policy, timeout,ssn,agg_size;
        u8 dialog_token;
        u8 da[ETHER_ADDR_LEN];
        u32 wsid = PktInfo->wsid;
        HDR80211_Mgmt* rxmgmt = HDR80211_MGMT(PktInfo);
        dialog_token = rxmgmt->u.action.u.addba_req.dialog_token;
        policy = (rxmgmt->u.action.u.addba_req.capab & IEEE80211_ADDBA_PARAM_POLICY_MASK) >> BLOCKACKPARAM_POLICY_POS;
        tid = (rxmgmt->u.action.u.addba_req.capab & IEEE80211_ADDBA_PARAM_TID_MASK) >> BLOCKACKPARAM_TID_POS;
        agg_size=(rxmgmt->u.action.u.addba_req.capab & IEEE80211_ADDBA_PARAM_BUF_SIZE_MASK) >> BLOCKACKPARAM_WINSIZE_POS;
        agg_size = AMPDU_RX_BUF_SIZE;
    
        timeout= rxmgmt->u.action.u.addba_req.timeout;
        ssn= rxmgmt->u.action.u.addba_req.start_seq_num>> 4;
        memcpy(da, rxmgmt->sa.addr, ETHER_ADDR_LEN);   
    
        PktInfo=PBUF_MReAlloc(_PktInfo,(HDR80211_MGMT_LEN+FIXED_FIELD_LEN));
    
        if(PktInfo==NULL)
        {
            return;
        }
        if((wsid < RX_AGG_RX_BA_MAX_STATION)&&(tid < RX_AGG_RX_BA_MAX_SESSIONS)&&(status == WLAN_STATUS_SUCCESS))
        {
            //if (status == WLAN_STATUS_SUCCESS)
            {
                u32 u32ta;
                u32ta = 0;
                u32ta |= (da[0] & 0xff) << (8 * 0);
                u32ta |= (da[1] & 0xff) << (8 * 1);
                u32ta |= (da[2] & 0xff) << (8 * 2);
                u32ta |= (da[3] & 0xff) << (8 * 3);
                SET_BA_TA_31_0(u32ta);
        
                u32ta = 0;
                u32ta |= (da[4] & 0xff) << (8 * 0);
                u32ta |= (da[5] & 0xff) << (8 * 1);
                SET_BA_TA_47_32(u32ta);
        
                REG32(ADR_BA_TID)= tid;
                REG32(ADR_BA_ST_SEQ)= ssn;
                REG32(ADR_BA_SB0)= 0;
                REG32(ADR_BA_SB1)= 0;
                REG32(ADR_BA_CTRL)= (1 << BA_AGRE_EN_SFT)| (3 << BA_CTRL_SFT);
                printf("tid%d, ssn:%d\n",tid,ssn);
                send_addba_response_to_host(PktInfo->wsid,tid,ssn,agg_size);
            } 
            //else {
                //turn off ba session
            //    REG32(ADR_BA_CTRL)= 0;
            //}
        }
        else
        {
            status = WLAN_STATUS_UNSPECIFIED_FAILURE;
            //turn off ba session
            REG32(ADR_BA_CTRL)= 0;
        }
        mlme_send_addba_response(da, tid, dialog_token, status,
                            policy,FIXED_FIELD_LEN,agg_size, timeout,(PKT_TxInfo *)PktInfo);


}

