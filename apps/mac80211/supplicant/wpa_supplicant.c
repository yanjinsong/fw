/*
 * WPA Supplicant
 * Copyright (c) 2003-2011, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 *
 * This file implements functions for registering and unregistering
 * %wpa_supplicant interfaces. In addition, this file contains number of
 * functions for managing network connections.
 */

#include "supplicant_config.h"
#include "utils/common.h"
#include "crypto/random.h"
#include "crypto/sha1.h"
#include "rsn_supp/wpa.h"
#include "utils/eloop.h"
#include "l2_packet/l2_packet.h"
#include <wpa_supplicant_i.h>
#include "drivers/driver_cabrio.h"
#include "common/ieee802_11_defs.h"
#include "wpas_glue.h"
#include "utils/wpa_debug.h"
#include <dbg_timer/dbg_timer.h>

static void wpa_supplicant_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = (struct wpa_supplicant *)eloop_ctx;
	//const u8 *bssid = wpa_s->bssid;
	//if (is_zero_ether_addr(bssid))
		//bssid = wpa_s->pending_bssid;
	wpa_msg(wpa_s, MSG_INFO, "Authentication with " MACSTR " timed out.",
		MAC2STR(wpa_s->bssid));
	//wpa_blacklist_add(wpa_s, bssid);
	wpa_supplicant_disassociate(wpa_s, WLAN_REASON_DEAUTH_LEAVING);
	//wpa_s->reassociate = 1;

	/*
	 * If we timed out, the AP or the local radio may be busy.
	 * So, wait a second until scanning again.
	 */
	//wpa_supplicant_req_scan(wpa_s, 1, 0);
}


/**
 * wpa_supplicant_req_auth_timeout - Schedule a timeout for authentication
 * @wpa_s: Pointer to wpa_supplicant data
 * @sec: Number of seconds after which to time out authentication
 * @usec: Number of microseconds after which to time out authentication
 *
 * This function is used to schedule a timeout for the current authentication
 * attempt.
 */
void wpa_supplicant_req_auth_timeout(struct wpa_supplicant *wpa_s,
                				     u32 sec, u32 usec)
{
	wpa_dbg(wpa_s, MSG_DEBUG, "Setting authentication timeout: %u sec "
		"%u usec", sec, usec);
	eloop_cancel_timeout(wpa_supplicant_timeout, wpa_s, NULL);
	eloop_register_timeout(sec, usec, wpa_supplicant_timeout, wpa_s, NULL);
}


/**
 * wpa_supplicant_cancel_auth_timeout - Cancel authentication timeout
 * @wpa_s: Pointer to wpa_supplicant data
 *
 * This function is used to cancel authentication timeout scheduled with
 * wpa_supplicant_req_auth_timeout() and it is called when authentication has
 * been completed.
 */
void wpa_supplicant_cancel_auth_timeout(struct wpa_supplicant *wpa_s)
{
	wpa_dbg(wpa_s, MSG_DEBUG, "Cancelling authentication timeout");
	eloop_cancel_timeout(wpa_supplicant_timeout, wpa_s, NULL);
}

static void wpa_supplicant_cleanup(struct wpa_supplicant *wpa_s)
{
	wpa_sm_deinit(wpa_s->wpa);
	wpa_s->wpa = (struct wpa_sm *)NULL;

	//wpa_supplicant_cancel_scan(wpa_s);
	wpa_supplicant_cancel_auth_timeout(wpa_s);
	eloop_cancel_timeout(wpa_supplicant_stop_countermeasures, wpa_s, NULL);
#ifdef CONFIG_DELAYED_MIC_ERROR_REPORT
	eloop_cancel_timeout(wpa_supplicant_delayed_mic_error_report,
			     wpa_s, NULL);
#endif /* CONFIG_DELAYED_MIC_ERROR_REPORT */

	//wpas_wps_deinit(wpa_s);

	//wpabuf_free(wpa_s->pending_eapol_rx);
	//wpa_s->pending_eapol_rx = NULL;

	//sme_deinit(wpa_s);
}

/**
 * wpa_clear_keys - Clear keys configured for the driver
 * @wpa_s: Pointer to wpa_supplicant data
 * @addr: Previously used BSSID or %NULL if not available
 *
 * This function clears the encryption keys that has been previously configured
 * for the driver.
 */
void wpa_clear_keys(struct wpa_supplicant *wpa_s, const u8 *addr)
{
	/* MLME-DELETEKEYS.request */
	wpa_drv_set_key(wpa_s, WPA_ALG_NONE, (const u8 *)NULL, 0, 0, (const u8 *)NULL, 0, (const u8 *)NULL, 0);
	//wpa_drv_set_key(wpa_s, WPA_ALG_NONE, (const u8 *)NULL, 1, 0, (const u8 *)NULL, 0, (const u8 *)NULL, 0);
	//wpa_drv_set_key(wpa_s, WPA_ALG_NONE, (const u8 *)NULL, 2, 0, (const u8 *)NULL, 0, (const u8 *)NULL, 0);
	//wpa_drv_set_key(wpa_s, WPA_ALG_NONE, (const u8 *)NULL, 3, 0, (const u8 *)NULL, 0, (const u8 *)NULL, 0);
#ifdef CONFIG_IEEE80211W
	wpa_drv_set_key(wpa_s, WPA_ALG_NONE, (const u8 *)NULL, 4, 0, (const u8 *)NULL, 0, (const u8 *)NULL, 0);
	wpa_drv_set_key(wpa_s, WPA_ALG_NONE, (const u8 *)NULL, 5, 0, (const u8 *)NULL, 0, (const u8 *)NULL, 0);
#endif /* CONFIG_IEEE80211W */
	if (addr) {
		wpa_drv_set_key(wpa_s, WPA_ALG_NONE, addr, 0, 0, (const u8 *)NULL, 0, (const u8 *)NULL,
				0);
	}
	//wpa_s->keys_cleared = 1;
    #ifdef ENABLE_BACKGROUND_PMK_CALC
    wpa_s->pmk_calc_data = NULL;
    wpa_s->pmk_calc_task = NULL;
	#endif // ENABLE_BACKGROUND_PMK_CALC
}


/**
 * wpa_supplicant_state_txt - Get the connection state name as a text string
 * @state: State (wpa_state; WPA_*)
 * Returns: The state name as a printable text string
 */
const char * wpa_supplicant_state_txt(enum wpa_states state)
{
	switch (state) {
	case WPA_DISCONNECTED:
		return "DISCONNECTED";
	case WPA_INACTIVE:
		return "INACTIVE";
	case WPA_SCANNING:
		return "SCANNING";
	case WPA_AUTHENTICATING:
		return "AUTHENTICATING";
	case WPA_ASSOCIATING:
		return "ASSOCIATING";
	case WPA_ASSOCIATED:
		return "ASSOCIATED";
	case WPA_4WAY_HANDSHAKE:
		return "4WAY_HANDSHAKE";
	case WPA_GROUP_HANDSHAKE:
		return "GROUP_HANDSHAKE";
	case WPA_COMPLETED:
		return "COMPLETED";
	default:
		return "UNKNOWN";
	}
}


/**
 * wpa_supplicant_set_state - Set current connection state
 * @wpa_s: Pointer to wpa_supplicant data
 * @state: The new connection state
 *
 * This function is called whenever the connection state changes, e.g.,
 * association is completed for WPA/WPA2 4-Way Handshake is started.
 */
void wpa_supplicant_set_state(struct wpa_supplicant *wpa_s,
			      enum wpa_states state)
{
    {
#ifndef CONFIG_NO_WPA_MSG
    const char *cur_state_name = wpa_supplicant_state_txt(wpa_s->wpa_state);
    const char *next_state_name = wpa_supplicant_state_txt(state);
#endif
	wpa_dbg(wpa_s, MSG_DEBUG, "State: %s -> %s",
            cur_state_name, next_state_name);
    }
#if 1
    wpa_drv_set_operstate(wpa_s, state);
#else
    //if (state != WPA_SCANNING)
		//wpa_supplicant_notify_scanning(wpa_s, 0);

	if (state == WPA_COMPLETED ) {
		//wpa_s->new_connection = 0;
		//wpa_s->reassociated_connection = 1;
		wpa_drv_set_operstate(wpa_s, 1);
#ifndef IEEE8021X_EAPOL
		//wpa_drv_set_supp_port(wpa_s, 1);
#endif /* IEEE8021X_EAPOL */
		//wpa_s->after_wps = 0;
	} else if (state == WPA_DISCONNECTED || state == WPA_ASSOCIATING ||
		   state == WPA_ASSOCIATED) {
		//wpa_s->new_connection = 1;
		wpa_drv_set_operstate(wpa_s, 0);
#ifndef IEEE8021X_EAPOL
		//wpa_drv_set_supp_port(wpa_s, 0);
#endif /* IEEE8021X_EAPOL */
	}
#endif
	wpa_s->wpa_state = state;


	//if (wpa_s->wpa_state == WPA_COMPLETED ||old_state == WPA_COMPLETED)
		//wpas_notify_auth_changed(wpa_s);
}

void wpa_supplicant_clear_status(struct wpa_supplicant *wpa_s)
{
	wpa_s->pairwise_cipher = 0;
	wpa_s->group_cipher = 0;
	wpa_s->mgmt_group_cipher = 0;
	wpa_s->key_mgmt = 0;
	wpa_s->wpa_state = WPA_DISCONNECTED;
    wpa_drv_get_mac_addr(wpa_s);
    wpa_sm_set_own_addr(wpa_s->wpa, wpa_s->own_addr);
    wpa_clear_keys(wpa_s, (const u8 *)NULL);
}
#if 0
static int wpa_supplicant_suites_from_ai(struct wpa_supplicant *wpa_s,
					 struct wpa_ssid *ssid,
					 struct wpa_ie_data *ie)
{
	int ret = wpa_sm_parse_own_wpa_ie(wpa_s->wpa, ie);
	if (ret) {
		if (ret == -2) {
			wpa_msg(wpa_s, MSG_INFO, "WPA: Failed to parse WPA IE "
				"from association info");
		}
		return -1;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "WPA: Using WPA IE from AssocReq to set "
		"cipher suites");
	if (!(ie->group_cipher & ssid->group_cipher)) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: Driver used disabled group "
			"cipher 0x%x (mask 0x%x) - reject",
			ie->group_cipher, ssid->group_cipher);
		return -1;
	}
	if (!(ie->pairwise_cipher & ssid->pairwise_cipher)) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: Driver used disabled pairwise "
			"cipher 0x%x (mask 0x%x) - reject",
			ie->pairwise_cipher, ssid->pairwise_cipher);
		return -1;
	}
	if (!(ie->key_mgmt & ssid->key_mgmt)) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: Driver used disabled key "
			"management 0x%x (mask 0x%x) - reject",
			ie->key_mgmt, ssid->key_mgmt);
		return -1;
	}

#ifdef CONFIG_IEEE80211W
	if (!(ie->capabilities & WPA_CAPABILITY_MFPC) &&
	    ssid->ieee80211w == MGMT_FRAME_PROTECTION_REQUIRED) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: Driver associated with an AP "
			"that does not support management frame protection - "
			"reject");
		return -1;
	}
#endif /* CONFIG_IEEE80211W */

	return 0;
}
#endif

#ifdef ENABLE_BACKGROUND_PMK_CALC
int pbkdf2_sha1_iter (const char *passphrase, const char *ssid, size_t ssid_len,
                      int iterations, u8 *buf, size_t buflen)
{
    PMK_CALC_DATA_S *pmk_calc_data = init_pbkdf2_sha1_calc((const u8 *)passphrase,
                                                           (const u8 *)ssid,
                                                           ssid_len, iterations);
    while (pbkdf2_sha1_calc_func(pmk_calc_data, 32) != 0)
        {}
    os_memcpy(buf, pmk_calc_data->pmk, buflen);
    deinit_pbkdf2_sha1_calc(pmk_calc_data);
    return 0;
} // end if - pbkdf2_sha1_iter -


static void *_init_pmk_in_background (struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid)
{
    PMK_CALC_DATA_S *pmk_calc_data = init_pbkdf2_sha1_calc((const u8 *)ssid->passphrase,
                                                           ssid->ssid,
                                                           ssid->ssid_len, 4096);
    pmk_calc_data->custom_data = (void *)wpa_s;
    return (void *)pmk_calc_data;
} // end of - _init_pmk_in_background -

static int _calc_pmk_in_background (void *data)
{
    int ret;
    struct wpa_supplicant *wpa_s;
    PMK_CALC_DATA_S *pmk_calc_data = (PMK_CALC_DATA_S *)data;
    ret = pbkdf2_sha1_calc_func(pmk_calc_data, 32);
    if (ret != 0)
        return 1; // More to go

    wpa_s = (struct wpa_supplicant *)pmk_calc_data->custom_data;
    wpa_hexdump_key(MSG_MSGDUMP, "PSK (from passphrase)",
                    pmk_calc_data->pmk, PMK_LEN);
    wpa_sm_set_pmk(wpa_s->wpa, pmk_calc_data->pmk, PMK_LEN);
    wpa_msg(wpa_s, MSG_INFO, "PMK Set .");
	#ifdef PROFILE_SUPPLICANT
    LOG_PRINTF("PMK finished with %uus.\n", pmk_calc_data->pmk_begin_time.ts.lt);
	#endif // PROFILE_SUPPLICANT
	#if 0
    printf("%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n"
	       "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
	       pmk_calc_data->pmk[ 0], pmk_calc_data->pmk[ 1],
	       pmk_calc_data->pmk[ 2], pmk_calc_data->pmk[ 3],
	       pmk_calc_data->pmk[ 4], pmk_calc_data->pmk[ 5],
	       pmk_calc_data->pmk[ 6], pmk_calc_data->pmk[ 7],
	       pmk_calc_data->pmk[ 8], pmk_calc_data->pmk[ 9],
	       pmk_calc_data->pmk[10], pmk_calc_data->pmk[11],
	       pmk_calc_data->pmk[12], pmk_calc_data->pmk[13],
	       pmk_calc_data->pmk[14], pmk_calc_data->pmk[15],
	       pmk_calc_data->pmk[16], pmk_calc_data->pmk[17],
	       pmk_calc_data->pmk[18], pmk_calc_data->pmk[19],
	       pmk_calc_data->pmk[20], pmk_calc_data->pmk[21],
	       pmk_calc_data->pmk[22], pmk_calc_data->pmk[23],
	       pmk_calc_data->pmk[24], pmk_calc_data->pmk[25],
	       pmk_calc_data->pmk[26], pmk_calc_data->pmk[27],
	       pmk_calc_data->pmk[28], pmk_calc_data->pmk[29],
	       pmk_calc_data->pmk[30], pmk_calc_data->pmk[31]);
	#endif
    deinit_pbkdf2_sha1_calc(pmk_calc_data);
    wpa_s->pmk_calc_data = NULL;
    wpa_s->pmk_calc_task = NULL;
    return 0;
}

static void _finish_pmk_calc (struct wpa_supplicant *wpa_s)
{
    while (wpa_s->pmk_calc_task != NULL)
        (*wpa_s->pmk_calc_task)(wpa_s->pmk_calc_data);
}
#endif // ENABLE_BACKGROUND_PMK_CALC

/**
 * wpa_supplicant_set_suites - Set authentication and encryption parameters
 * @wpa_s: Pointer to wpa_supplicant data
 * @bss: Scan results for the selected BSS, or %NULL if not available
 * @ssid: Configuration data for the selected network
 * @wpa_ie: Buffer for the WPA/RSN IE
 * @wpa_ie_len: Maximum wpa_ie buffer size on input. This is changed to be the
 * used buffer length in case the functions returns success.
 * Returns: 0 on success or -1 on failure
 *
 * This function is used to configure authentication and encryption parameters
 * based on the network configuration and scan result for the selected BSS (if
 * available).
 */
int wpa_supplicant_set_suites(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid,
			      u8 *wpa_ie, size_t *wpa_ie_len)
{
	struct wpa_ie_data ie;
	unsigned int sel, proto;

	//Assign to new config
	wpa_s->current_ssid = ssid;
	{
		if (ssid->proto & WPA_PROTO_RSN)
			proto = WPA_PROTO_RSN;
		else
			proto = WPA_PROTO_WPA;
		//if (wpa_supplicant_suites_from_ai(wpa_s, ssid, &ie) < 0)
		{
			os_memset(&ie, 0, sizeof(ie));
			ie.group_cipher = ssid->group_cipher;
			ie.pairwise_cipher = ssid->pairwise_cipher;
			ie.key_mgmt = ssid->key_mgmt;
#ifdef CONFIG_IEEE80211W
			ie.mgmt_group_cipher =
				ssid->ieee80211w != NO_MGMT_FRAME_PROTECTION ?
				WPA_CIPHER_AES_128_CMAC : 0;
#endif /* CONFIG_IEEE80211W */
			wpa_dbg(wpa_s, MSG_DEBUG, "WPA: Set cipher suites "
				"based on configuration");
		}
		//else
		//	proto = ie.proto;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "WPA: Selected cipher suites: group %d "
		"pairwise %d key_mgmt %d proto %u",
		ie.group_cipher, ie.pairwise_cipher, ie.key_mgmt, proto);
#ifdef CONFIG_IEEE80211W
	if (ssid->ieee80211w) {
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: Selected mgmt group cipher %d",
			ie.mgmt_group_cipher);
	}
#endif /* CONFIG_IEEE80211W */

	wpa_s->wpa_proto = proto;
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_PROTO, proto);
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_RSN_ENABLED,
			 (u32)!!(ssid->proto & WPA_PROTO_RSN));

	sel = ie.group_cipher & ssid->group_cipher;
	if (sel & WPA_CIPHER_CCMP) {
		wpa_s->group_cipher = WPA_CIPHER_CCMP;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using GTK CCMP");
	} else if (sel & WPA_CIPHER_TKIP) {
		wpa_s->group_cipher = WPA_CIPHER_TKIP;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using GTK TKIP");
	} else if (sel & WPA_CIPHER_WEP104) {
		wpa_s->group_cipher = WPA_CIPHER_WEP104;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using GTK WEP104");
	} else if (sel & WPA_CIPHER_WEP40) {
		wpa_s->group_cipher = WPA_CIPHER_WEP40;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using GTK WEP40");
	} else {
		wpa_msg(wpa_s, MSG_WARNING, "WPA: Failed to select group "
			"cipher");
		return -1;
	}

	sel = ie.pairwise_cipher & ssid->pairwise_cipher;
	if (sel & WPA_CIPHER_CCMP) {
		wpa_s->pairwise_cipher = WPA_CIPHER_CCMP;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using PTK CCMP");
	} else if (sel & WPA_CIPHER_TKIP) {
		wpa_s->pairwise_cipher = WPA_CIPHER_TKIP;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using PTK TKIP");
	} else if (sel & WPA_CIPHER_NONE) {
		wpa_s->pairwise_cipher = WPA_CIPHER_NONE;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using PTK NONE");
	} else {
		wpa_msg(wpa_s, MSG_WARNING, "WPA: Failed to select pairwise "
			"cipher");
		return -1;
	}

	sel = ie.key_mgmt & ssid->key_mgmt;
    /*lint -save -e774 */
	if (0) {
    /*lint -restore */
#ifdef CONFIG_IEEE80211W
	} else if (sel & WPA_KEY_MGMT_IEEE8021X_SHA256) {
		wpa_s->key_mgmt = WPA_KEY_MGMT_IEEE8021X_SHA256;
		wpa_dbg(wpa_s, MSG_DEBUG,
			"WPA: using KEY_MGMT 802.1X with SHA256");
	} else if (sel & WPA_KEY_MGMT_PSK_SHA256) {
		wpa_s->key_mgmt = WPA_KEY_MGMT_PSK_SHA256;
		wpa_dbg(wpa_s, MSG_DEBUG,
			"WPA: using KEY_MGMT PSK with SHA256");
#endif /* CONFIG_IEEE80211W */
	} else if (sel & WPA_KEY_MGMT_IEEE8021X) {
		wpa_s->key_mgmt = WPA_KEY_MGMT_IEEE8021X;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using KEY_MGMT 802.1X");
	} else if (sel & WPA_KEY_MGMT_PSK) {
		wpa_s->key_mgmt = WPA_KEY_MGMT_PSK;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using KEY_MGMT WPA-PSK");
	} else if (sel & WPA_KEY_MGMT_WPA_NONE) {
		wpa_s->key_mgmt = WPA_KEY_MGMT_WPA_NONE;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using KEY_MGMT WPA-NONE");
	} else {
		wpa_msg(wpa_s, MSG_WARNING, "WPA: Failed to select "
			"authenticated key management type");
		return -1;
	}

	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_KEY_MGMT, (u32)wpa_s->key_mgmt);
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_PAIRWISE,
			         (u32)wpa_s->pairwise_cipher);
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_GROUP, (u32)wpa_s->group_cipher);

#ifdef CONFIG_IEEE80211W
	sel = ie.mgmt_group_cipher;
	if (ssid->ieee80211w == NO_MGMT_FRAME_PROTECTION ||
	    !(ie.capabilities & WPA_CAPABILITY_MFPC))
		sel = 0;
	if (sel & WPA_CIPHER_AES_128_CMAC) {
		wpa_s->mgmt_group_cipher = WPA_CIPHER_AES_128_CMAC;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using MGMT group cipher "
			"AES-128-CMAC");
	} else {
		wpa_s->mgmt_group_cipher = 0;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: not using MGMT group cipher");
	}
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_MGMT_GROUP,
			 wpa_s->mgmt_group_cipher);
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_MFP, ssid->ieee80211w);
#endif /* CONFIG_IEEE80211W */

	if (wpa_sm_set_assoc_wpa_ie_default(wpa_s->wpa, wpa_ie, wpa_ie_len)) {
		wpa_msg(wpa_s, MSG_WARNING, "WPA: Failed to generate WPA IE");
		return -1;
	}

	if (ssid->key_mgmt &
	    (WPA_KEY_MGMT_PSK | WPA_KEY_MGMT_FT_PSK | WPA_KEY_MGMT_PSK_SHA256))
	{
#ifndef CONFIG_NO_PBKDF2
		if (ssid->passphrase[0] != '\0') {
#ifdef ENABLE_BACKGROUND_PMK_CALC
            wpa_s->pmk_calc_data = _init_pmk_in_background(wpa_s, ssid);
            wpa_s->pmk_calc_task = _calc_pmk_in_background;
#else // ENABLE_BACKGROUND_PMK_CALC
			u8 psk[PMK_LEN];
			wpa_msg(wpa_s, MSG_INFO, "Calculating PSK...\n");
            pbkdf2_sha1(ssid->passphrase, (const char *)ssid->ssid, ssid->ssid_len, 4096, psk, PMK_LEN);
            //pbkdf2_sha1_flatten(ssid->passphrase, (const char *)ssid->ssid, ssid->ssid_len, 4096, psk, PMK_LEN);
            //pbkdf2_sha1_iter(ssid->passphrase, (const char *)ssid->ssid, ssid->ssid_len, 4096, psk1, PMK_LEN);
            wpa_hexdump_key(MSG_MSGDUMP, "PSK (from passphrase)",
                            psk, PMK_LEN);
			wpa_sm_set_pmk(wpa_s->wpa, psk, PMK_LEN);
			wpa_msg(wpa_s, MSG_INFO, "PMK Set .");
#endif // ENABLE_BACKGROUND_PMK_CALC
		} else
#endif /* CONFIG_NO_PBKDF2 */
        {
            wpa_sm_set_pmk(wpa_s->wpa, ssid->psk, PMK_LEN);
		}
	} else {
		;//wpa_sm_set_pmk_from_pmksa(wpa_s->wpa);
    }
	return 0;
}


/**
 * wpa_supplicant_associate - Request association
 * @wpa_s: Pointer to wpa_supplicant data
 * @bss: Scan results for the selected BSS, or %NULL if not available
 * @ssid: Configuration data for the selected network
 *
 * This function is used to request %wpa_supplicant to associate with a BSS.
 */
void wpa_supplicant_associate(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid)
{
	{
#ifndef CONFIG_NO_WPA_MSG
    const char *ssid_txt = wpa_ssid_txt(ssid->ssid, ssid->ssid_len);
#endif
    wpa_msg(wpa_s, MSG_INFO, "Trying to associate with SSID '%s'",
            ssid_txt);
		//os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);
	}
	wpa_clear_keys(wpa_s,(const u8 *)NULL);
	wpa_supplicant_set_state(wpa_s, WPA_ASSOCIATING);
	//wpa_s->current_ssid = ssid;
}


static void wpa_supplicant_clear_connection(struct wpa_supplicant *wpa_s,
					    const u8 *addr)
{
	//struct wpa_ssid *old_ssid;

	wpa_clear_keys(wpa_s, addr);
	wpa_supplicant_mark_disassoc(wpa_s);
	//wpa_s->current_ssid = NULL;

	eloop_cancel_timeout(wpa_supplicant_timeout, wpa_s, NULL);
}


/**
 * wpa_supplicant_disassociate - Disassociate the current connection
 * @wpa_s: Pointer to wpa_supplicant data
 * @reason_code: IEEE 802.11 reason code for the disassociate frame
 *
 * This function is used to request %wpa_supplicant to disassociate with the
 * current AP.
 */
void wpa_supplicant_disassociate(struct wpa_supplicant *wpa_s,
				 int reason_code)
{
	const u8 *addr = (const u8 *)NULL;

	if (!is_zero_ether_addr(wpa_s->bssid)) {
		//wpa_drv_disassociate(wpa_s, wpa_s->bssid, reason_code);
		addr = wpa_s->bssid;
	}

	wpa_supplicant_clear_connection(wpa_s, addr);
}


/**
 * wpa_supplicant_deauthenticate - Deauthenticate the current connection
 * @wpa_s: Pointer to wpa_supplicant data
 * @reason_code: IEEE 802.11 reason code for the deauthenticate frame
 *
 * This function is used to request %wpa_supplicant to deauthenticate from the
 * current AP.
 */
void wpa_supplicant_deauthenticate(struct wpa_supplicant *wpa_s,
				   int reason_code)
{
	const u8 *addr = (const u8 *)NULL;

	if (!is_zero_ether_addr(wpa_s->bssid)) {
		//wpa_drv_deauthenticate(wpa_s, wpa_s->bssid, reason_code);
		addr = wpa_s->bssid;
	}

	wpa_supplicant_clear_connection(wpa_s, addr);
}

/**
 * wpa_supplicant_get_ssid - Get a pointer to the current network structure
 * @wpa_s: Pointer to wpa_supplicant data
 * Returns: A pointer to the current network structure or %NULL on failure
 */
struct wpa_ssid * wpa_supplicant_get_ssid(struct wpa_supplicant *wpa_s)
{
	int res;
	//size_t ssid_len;

	res = wpa_drv_get_ssid(wpa_s, (u8 *)NULL);
	if (res < 0) {
		wpa_msg(wpa_s, MSG_WARNING, "Could not read SSID from "
			"driver");
		return (struct wpa_ssid *)NULL;
	}
	//ssid_len = (size_t)res;

	if (wpa_drv_get_bssid(wpa_s, (u8 *)NULL) < 0) {
		wpa_msg(wpa_s, MSG_WARNING, "Could not read BSSID from "
			"driver");
		return (struct wpa_ssid *)NULL;
	}


	return wpa_s->current_ssid;
}

/**
 * wpa_supplicant_rx_eapol - Deliver a received EAPOL frame to wpa_supplicant
 * @ctx: Context pointer (wpa_s); this is the ctx variable registered
 *	with struct wpa_driver_ops::init()
 * @src_addr: Source address of the EAPOL frame
 * @buf: EAPOL data starting from the EAPOL header (i.e., no Ethernet header)
 * @len: Length of the EAPOL data
 *
 * This function is called for each received EAPOL frame. Most driver
 * interfaces rely on more generic OS mechanism for receiving frames through
 * l2_packet, but if such a mechanism is not available, the driver wrapper may
 * take care of received EAPOL frames and deliver them to the core supplicant
 * code by calling this function.
 */
void wpa_supplicant_rx_eapol(void *ctx, const u8 *src_addr,
			                 u8 *buf, size_t len)
{
	struct wpa_supplicant *wpa_s = (struct wpa_supplicant *)ctx;

	wpa_dbg(wpa_s, MSG_DEBUG, "RX EAPOL from " MACSTR, MAC2STR(src_addr));
	wpa_hexdump(MSG_MSGDUMP, "RX EAPOL", buf, len);

	if (wpa_s->wpa_state < WPA_ASSOCIATED) {
		/*
		 * There is possible race condition between receiving the
		 * association event and the EAPOL frame since they are coming
		 * through different paths from the driver. In order to avoid
		 * issues in trying to process the EAPOL frame before receiving
		 * association information, lets queue it for processing until
		 * the association event is received.
		 */
		wpa_dbg(wpa_s, MSG_DEBUG, "Not associated - Delay processing "
			"of received EAPOL frame");
#if 0
		wpabuf_free(wpa_s->pending_eapol_rx);
		wpa_s->pending_eapol_rx = wpabuf_alloc_copy(buf, len);
		if (wpa_s->pending_eapol_rx) {
			os_get_time(&wpa_s->pending_eapol_rx_time);
			os_memcpy(wpa_s->pending_eapol_rx_src, src_addr,
				  ETH_ALEN);
		}
#endif
		return;
	}

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_NONE) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Ignored received EAPOL frame since "
			"no key management is configured");
		return;
	}

	if (wpa_s->eapol_received == 0 &&
	    (!wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt) ||
	     wpa_s->wpa_state != WPA_COMPLETED) &&
	    wpa_s->current_ssid == NULL) {
		/* Timeout for completing IEEE 802.1X and WPA authentication */
		wpa_supplicant_req_auth_timeout(
			wpa_s,
			(wpa_key_mgmt_wpa_ieee8021x(wpa_s->key_mgmt) ||
			 wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA ||
			 wpa_s->key_mgmt == WPA_KEY_MGMT_WPS) ?
			70 : 10, 0);
	}
	wpa_s->eapol_received++;

	if (wpa_s->countermeasures) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: Countermeasures - dropped "
			"EAPOL packet");
		return;
	}

	/* Source address of the incoming EAPOL frame could be compared to the
	 * current BSSID. However, it is possible that a centralized
	 * Authenticator could be using another MAC address than the BSSID of
	 * an AP, so just allow any address to be used for now. The replies are
	 * still sent to the current BSSID (if available), though. */

	//os_memcpy(wpa_s->last_eapol_src, src_addr, ETH_ALEN);
#if 0//Joy eapol
	if (!wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt) &&
	    eapol_sm_rx_eapol(wpa_s->eapol, src_addr, buf, len) > 0)
		return;
#else
	if (!wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt))
		return;
#endif
#if 0
	wpa_drv_poll(wpa_s);
	if (!(wpa_s->drv_flags & WPA_DRIVER_FLAGS_4WAY_HANDSHAKE))
#else
	;//Please check WPA_DRIVER_FLAGS_4WAY_HANDSHAKE
#endif
    #ifdef ENABLE_BACKGROUND_PMK_CALC
    _finish_pmk_calc(wpa_s);
    #endif // ENABLE_BACKGROUND_PMK_CALC
	wpa_sm_rx_eapol(wpa_s->wpa, src_addr, buf, len);
}

/**
 * wpa_supplicant_driver_init - Initialize driver interface parameters
 * @wpa_s: Pointer to wpa_supplicant data
 * Returns: 0 on success, -1 on failure
 *
 * This function is called to initialize driver interface parameters.
 * wpa_drv_init() must have been called before this function to initialize the
 * driver interface.
 */
int wpa_supplicant_driver_init(struct wpa_supplicant *wpa_s)
{
//  MLME call wpa_supplicant_rx_eapol directly.
//	l2_packet_init((const char *)NULL,(const u8 *)NULL,ETH_P_EAPOL,wpa_supplicant_rx_eapol, wpa_s, 0);

	wpa_clear_keys(wpa_s, (const u8 *)NULL);

	/* Make sure that TKIP countermeasures are not left enabled (could
	 * happen if wpa_supplicant is killed during countermeasures. */
	wpa_drv_set_countermeasures(wpa_s, 0);

	wpa_supplicant_set_state(wpa_s, WPA_INACTIVE);

	return 0;
}

static int wpa_supplicant_init_iface(struct wpa_supplicant *wpa_s)
{
	if (wpa_supplicant_init_wpa(wpa_s) < 0)
		return -1;

	if (wpa_supplicant_driver_init(wpa_s) < 0)
		return -1;

	wpa_sm_set_own_addr(wpa_s->wpa, wpa_s->own_addr);

	return 0;
}

static void wpa_supplicant_deinit_iface(struct wpa_supplicant *wpa_s)
{
	//if (wpa_s->drv_priv)
	{
		wpa_supplicant_deauthenticate(wpa_s, WLAN_REASON_DEAUTH_LEAVING);

		wpa_drv_set_countermeasures(wpa_s, 0);
		wpa_clear_keys(wpa_s, (const u8 *)NULL);
	}

	wpa_supplicant_cleanup(wpa_s);

	//if (notify)
		//wpas_notify_iface_removed(wpa_s);

	//if (wpa_s->drv_priv)
		//wpa_drv_deinit(wpa_s);
}

extern void supplicant_event(void *ctx, enum wpa_event_type event,
			     union wpa_event_data *data);

/**
 * wpa_supplicant_remove_iface - Remove a network interface
 * @global: Pointer to global data from wpa_supplicant_init()
 * @wpa_s: Pointer to the network interface to be removed
 * Returns: 0 if interface was removed, -1 if interface was not found
 *
 * This function can be used to dynamically remove network interfaces from
 * %wpa_supplicant, e.g., when a hotplug network adapter is ejected. In
 * addition, this function is used to remove all remaining interfaces when
 * %wpa_supplicant is terminated.
 */
int wpa_supplicant_remove_iface(struct wpa_supplicant *wpa_s)
{
	wpa_dbg(wpa_s, MSG_DEBUG, "Removing interface");

	//if (global->p2p_group_formation == wpa_s)
		//global->p2p_group_formation = NULL;
	wpa_supplicant_deinit_iface(wpa_s);

	return 0;
}

void (*wpa_supplicant_event)(void *ctx, enum wpa_event_type event,
			     union wpa_event_data *data);
/**
 * wpa_supplicant_init - Initialize %wpa_supplicant
 * @params: Parameters for %wpa_supplicant
 * Returns: Pointer to global %wpa_supplicant data, or %NULL on failure
 *
 * This function is used to initialize %wpa_supplicant. After successful
 * initialization, the returned data pointer can be used to add and remove
 * network interfaces, and eventually, to deinitialize %wpa_supplicant.
 */
static struct wpa_supplicant wpa_st;
static struct wpa_supplicant *_wpa_s;
struct wpa_supplicant * wpa_supplicant_init()
{
	wpa_supplicant_event = supplicant_event;
#if 0
	random_init(params->entropy_file);
#endif
	_wpa_s = &wpa_st;

	wpa_drv_get_mac_addr(_wpa_s);

	if (wpa_supplicant_init_iface(_wpa_s)) {
		wpa_printf(MSG_DEBUG, "Failed to add interface");
		wpa_supplicant_deinit_iface(_wpa_s);
		return (struct wpa_supplicant *)NULL;
	}
    eloop_init();
	return _wpa_s;
}

void wpa_supplicant_deinit(struct wpa_supplicant *wpa_s)
{
	wpa_supplicant_deinit_iface(wpa_s);
}
