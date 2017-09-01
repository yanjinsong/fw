/*
 * WPA Supplicant - Driver event processing
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
 */

#include "supplicant_config.h"

#include "utils/common.h"
#include "rsn_supp/wpa.h"
#include "utils/eloop.h"
//#include "l2_packet/l2_packet.h"
#include <wpa_supplicant_i.h>
#include "drivers/driver_cabrio.h"
#include "common/ieee802_11_defs.h"
#include "wpas_glue.h"
#include "wpa_supplicant.h"




void wpa_supplicant_stop_countermeasures(void *eloop_ctx, void *sock_ctx)
{
	struct wpa_supplicant *wpa_s = (struct wpa_supplicant *)eloop_ctx;

	if (wpa_s->countermeasures) {
		wpa_s->countermeasures = 0;
		wpa_drv_set_countermeasures(wpa_s, 0);
		wpa_msg(wpa_s, MSG_INFO, "WPA: TKIP countermeasures stopped");
		//wpa_supplicant_req_scan(wpa_s, 0, 0);
	}
}


void wpa_supplicant_mark_disassoc(struct wpa_supplicant *wpa_s)
{
	//int bssid_changed;

	wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
	//bssid_changed = !is_zero_ether_addr(wpa_s->bssid);
	os_memset(wpa_s->bssid, 0, ETH_ALEN);
	//os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);
	//wpa_s->current_bss = NULL;

	//if (bssid_changed)
		//wpas_notify_bssid_changed(wpa_s);
#if 0//Joy eapol
	eapol_sm_notify_portEnabled(wpa_s->eapol, FALSE);
	eapol_sm_notify_portValid(wpa_s->eapol, FALSE);
	if (wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt))
		eapol_sm_notify_eap_success(wpa_s->eapol, FALSE);
#endif
	//wpa_s->ap_ies_from_associnfo = 0;
}

static int wpa_supplicant_dynamic_keys(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_NONE ||
	    wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE)
		return 0;

#ifdef IEEE8021X_EAPOL
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA &&
	    wpa_s->current_ssid &&
	    !(wpa_s->current_ssid->eapol_flags &
	      (EAPOL_FLAG_REQUIRE_KEY_UNICAST |
	       EAPOL_FLAG_REQUIRE_KEY_BROADCAST))) {
		/* IEEE 802.1X, but not using dynamic WEP keys (i.e., either
		 * plaintext or static WEP keys). */
		return 0;
	}
#endif /* IEEE8021X_EAPOL */

	return 1;
}


static int wpa_supplicant_event_associnfo(struct wpa_supplicant *wpa_s,
					  union wpa_event_data *data)
{
	int l, len, found = 0, wpa_found, rsn_found;
	const u8 *p;

	wpa_dbg(wpa_s, MSG_DEBUG, "Association info event");
	if (data->assoc_info.req_ies)
		wpa_hexdump(MSG_DEBUG, "req_ies", data->assoc_info.req_ies,
			    data->assoc_info.req_ies_len);
	if (data->assoc_info.resp_ies) {
		wpa_hexdump(MSG_DEBUG, "resp_ies", data->assoc_info.resp_ies,
			    data->assoc_info.resp_ies_len);
	}
	if (data->assoc_info.beacon_ies)
		wpa_hexdump(MSG_DEBUG, "beacon_ies",
			    data->assoc_info.beacon_ies,
			    data->assoc_info.beacon_ies_len);
	if (data->assoc_info.freq)
		wpa_dbg(wpa_s, MSG_DEBUG, "freq=%u MHz",
			data->assoc_info.freq);

	p = data->assoc_info.req_ies;
	l = data->assoc_info.req_ies_len;

	/* Go through the IEs and make a copy of the WPA/RSN IE, if present. */
	while (p && l >= 2) {
		len = p[1] + 2;
		if (len > l) {
			wpa_hexdump(MSG_DEBUG, "Truncated IE in assoc_info",
				    p, (size_t)l);
			break;
		}
		if ((p[0] == WLAN_EID_VENDOR_SPECIFIC && p[1] >= 6 &&
		     (os_memcmp(&p[2], "\x00\x50\xF2\x01\x01\x00", 6) == 0)) ||
		    (p[0] == WLAN_EID_RSN && p[1] >= 2)) {
			if (wpa_sm_set_assoc_wpa_ie(wpa_s->wpa, p, (size_t)len))
				break;
			found = 1;
			//wpa_find_assoc_pmkid(wpa_s);
			break;
		}
		l -= len;
		p += len;
	}
	if (!found && data->assoc_info.req_ies)
		wpa_sm_set_assoc_wpa_ie(wpa_s->wpa, (const u8 *)NULL, 0);

	/* WPA/RSN IE from Beacon/ProbeResp */
	p = data->assoc_info.beacon_ies;
	l = data->assoc_info.beacon_ies_len;

	/* Go through the IEs and make a copy of the WPA/RSN IEs, if present.
	 */
	wpa_found = rsn_found = 0;
	while (p && l >= 2) {
		len = p[1] + 2;
		if (len > l) {
			wpa_hexdump(MSG_DEBUG, "Truncated IE in beacon_ies",
				    p, (size_t)l);
			break;
		}
		if (!wpa_found &&
		    p[0] == WLAN_EID_VENDOR_SPECIFIC && p[1] >= 6 &&
		    os_memcmp(&p[2], "\x00\x50\xF2\x01\x01\x00", 6) == 0) {
			wpa_found = 1;
			wpa_sm_set_ap_wpa_ie(wpa_s->wpa, p, (size_t)len);
		}

		if (!rsn_found &&
		    p[0] == WLAN_EID_RSN && p[1] >= 2) {
			rsn_found = 1;
			wpa_sm_set_ap_rsn_ie(wpa_s->wpa, p, (size_t)len);
		}

		l -= len;
		p += len;
	}

	if (!wpa_found && data->assoc_info.beacon_ies)
		wpa_sm_set_ap_wpa_ie(wpa_s->wpa, (const u8 *)NULL, 0);
	if (!rsn_found && data->assoc_info.beacon_ies)
		wpa_sm_set_ap_rsn_ie(wpa_s->wpa, (const u8 *)NULL, 0);
	//if (wpa_found || rsn_found)
		//wpa_s->ap_ies_from_associnfo = 1;

	return 0;
}


static void wpa_supplicant_event_assoc(struct wpa_supplicant *wpa_s,
				       union wpa_event_data *data)
{
	u8 bssid[ETH_ALEN];
	//struct wpa_driver_capa capa;

	if (data && wpa_supplicant_event_associnfo(wpa_s, data) < 0)
		return;

	wpa_supplicant_set_state(wpa_s, WPA_ASSOCIATED);
    #if 0
	if (wpa_drv_get_bssid(wpa_s, bssid) >= 0 &&os_memcmp(bssid, wpa_s->bssid, ETH_ALEN) != 0)
		;
    #else
    wpa_drv_get_bssid(wpa_s, bssid);
    #endif

	wpa_msg(wpa_s, MSG_INFO, "Associated with " MACSTR, MAC2STR(bssid));

	wpa_sm_notify_assoc(wpa_s->wpa, bssid);
	//if (wpa_s->l2)
		//l2_packet_notify_auth_start(wpa_s->l2);

	wpa_s->eapol_received = 0;
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_NONE ||
	    wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE ) {
		wpa_supplicant_cancel_auth_timeout(wpa_s);
		wpa_supplicant_set_state(wpa_s, WPA_COMPLETED);
	}
	//wpa_supplicant_cancel_scan(wpa_s);

	//if ((wpa_s->drv_flags & WPA_DRIVER_FLAGS_4WAY_HANDSHAKE) && wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt)) {
	if ((wpa_s->wpa_state == WPA_4WAY_HANDSHAKE) && (wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt))) {
		/*
		 * We are done; the driver will take care of RSN 4-way
		 * handshake.
		 */
		wpa_supplicant_cancel_auth_timeout(wpa_s);
		wpa_supplicant_set_state(wpa_s, WPA_COMPLETED);
#if 0//Joy eapol
		eapol_sm_notify_portValid(wpa_s->eapol, TRUE);
		eapol_sm_notify_eap_success(wpa_s->eapol, TRUE);
#endif
	//} else if ((wpa_s->drv_flags & WPA_DRIVER_FLAGS_4WAY_HANDSHAKE) && wpa_key_mgmt_wpa_ieee8021x(wpa_s->key_mgmt)) {
	} else if (wpa_key_mgmt_wpa_ieee8021x(wpa_s->key_mgmt)) {

		/*
		 * The driver will take care of RSN 4-way handshake, so we need
		 * to allow EAPOL supplicant to complete its work without
		 * waiting for WPA supplicant.
		 */
#if 0//Joy eapol
		eapol_sm_notify_portValid(wpa_s->eapol, TRUE);
#endif
	}
#if 0
	if (wpa_s->pending_eapol_rx) {
		struct os_time now, age;
		os_get_time(&now);
		os_time_sub(&now, &wpa_s->pending_eapol_rx_time, &age);
		if (age.sec == 0 && age.usec < 100000 &&
		    os_memcmp(wpa_s->pending_eapol_rx_src, bssid, ETH_ALEN) ==
		    0) {
			wpa_dbg(wpa_s, MSG_DEBUG, "Process pending EAPOL "
				"frame that was received just before "
				"association notification");
			wpa_supplicant_rx_eapol(
				wpa_s, wpa_s->pending_eapol_rx_src,
				wpabuf_head(wpa_s->pending_eapol_rx),
				wpabuf_len(wpa_s->pending_eapol_rx));
		}
		wpabuf_free(wpa_s->pending_eapol_rx);
		wpa_s->pending_eapol_rx = NULL;
	}
#endif
}


static void wpa_supplicant_event_disassoc(struct wpa_supplicant *wpa_s,
					  u16 reason_code)
{
	//const u8 *bssid;
	//int authenticating;

	//authenticating = wpa_s->wpa_state == WPA_AUTHENTICATING;
	//os_memcpy(prev_pending_bssid, wpa_s->pending_bssid, ETH_ALEN);

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE) {
		/*
		 * At least Host AP driver and a Prism3 card seemed to be
		 * generating streams of disconnected events when configuring
		 * IBSS for WPA-None. Ignore them for now.
		 */
		wpa_dbg(wpa_s, MSG_DEBUG, "Disconnect event - ignore in "
			"IBSS/WPA-None mode");
		return;
	}

	if (wpa_s->wpa_state == WPA_4WAY_HANDSHAKE &&
	    wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt)) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: 4-Way Handshake failed - "
			"pre-shared key may be incorrect");
	}
#if 0
	if (!wpa_s->auto_reconnect_disabled ||
	    wpa_s->key_mgmt == WPA_KEY_MGMT_WPS) {
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: Auto connect enabled: try to "
			"reconnect (wps=%d)",
			wpa_s->key_mgmt == WPA_KEY_MGMT_WPS);
		if (wpa_s->wpa_state >= WPA_ASSOCIATING) {
            //wpa_supplicant_req_scan(wpa_s, 0, 100000);
        }
	} else 
#endif
	{
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: Auto connect disabled: do not "
			"try to re-connect");
		//wpa_s->reassociate = 0;
		//wpa_s->disconnected = 1;
	}
	//bssid = wpa_s->bssid;
	//if (is_zero_ether_addr(bssid))
		//bssid = wpa_s->pending_bssid;
	//wpas_connection_failed(wpa_s, bssid);
	//wpa_sm_notify_disassoc(wpa_s->wpa);
	#if 0
	wpa_msg(wpa_s, MSG_INFO, "CTRL-EVENT-DISCONNECTED " "bssid=" MACSTR
		" reason=%d",
		MAC2STR(bssid), reason_code);
	#endif	
	if (wpa_supplicant_dynamic_keys(wpa_s)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Disconnect event - remove keys");
		//wpa_s->keys_cleared = 0;
		wpa_clear_keys(wpa_s, wpa_s->bssid);
	}
	wpa_supplicant_mark_disassoc(wpa_s);
#if 0
	if (authenticating && (wpa_s->drv_flags & WPA_DRIVER_FLAGS_SME))
		sme_disassoc_while_authenticating(wpa_s, prev_pending_bssid);
#endif
}


static void
wpa_supplicant_event_michael_mic_failure(struct wpa_supplicant *wpa_s,
					 union wpa_event_data *data)
{
	int pairwise;
	//struct os_time t;

	wpa_msg(wpa_s, MSG_WARNING, "Michael MIC failure detected");
	pairwise = (data && data->michael_mic_failure.unicast);
/*
	os_get_time(&t);
	if (wpa_s->last_michael_mic_error &&
	     t.sec - wpa_s->last_michael_mic_error <= 60){
*/
	if(1){
		/* Send the new MIC error report immediately since we are going
		 * to start countermeasures and AP better do the same.
		 */
		wpa_sm_key_request(wpa_s->wpa, 1, pairwise);

		/* initialize countermeasures */
		wpa_s->countermeasures = 1;
		wpa_msg(wpa_s, MSG_WARNING, "TKIP countermeasures started");

		/*
		 * Need to wait for completion of request frame. We do not get
		 * any callback for the message completion, so just wait a
		 * short while and hope for the best. */
		//os_sleep(0, 10000);

		wpa_drv_set_countermeasures(wpa_s, 1);
		wpa_supplicant_deauthenticate(wpa_s,
					      WLAN_REASON_MICHAEL_MIC_FAILURE);
		eloop_cancel_timeout(wpa_supplicant_stop_countermeasures,
				     wpa_s, NULL);
		eloop_register_timeout(60, 0,
				       wpa_supplicant_stop_countermeasures,
				       wpa_s, NULL);
		/* TODO: mark the AP rejected for 60 second. STA is
		 * allowed to associate with another AP.. */
	} else {
		wpa_sm_key_request(wpa_s->wpa, 1, pairwise);
	}
	//wpa_s->last_michael_mic_error = t.sec;
	//wpa_s->mic_errors_seen++;
}
#if 0
static void wpa_supplicant_event_unprot_deauth(struct wpa_supplicant *wpa_s,
					       struct unprot_deauth *e)
{
#ifdef CONFIG_IEEE80211W
	wpa_printf(MSG_DEBUG, "Unprotected Deauthentication frame "
		   "dropped: " MACSTR " -> " MACSTR
		   " (reason code %u)",
		   MAC2STR(e->sa), MAC2STR(e->da), e->reason_code);
	sme_event_unprot_disconnect(wpa_s, e->sa, e->da, e->reason_code);
#endif /* CONFIG_IEEE80211W */
}


static void wpa_supplicant_event_unprot_disassoc(struct wpa_supplicant *wpa_s,
						 struct unprot_disassoc *e)
{
#ifdef CONFIG_IEEE80211W
	wpa_printf(MSG_DEBUG, "Unprotected Disassociation frame "
		   "dropped: " MACSTR " -> " MACSTR
		   " (reason code %u)",
		   MAC2STR(e->sa), MAC2STR(e->da), e->reason_code);
	sme_event_unprot_disconnect(wpa_s, e->sa, e->da, e->reason_code);
#endif /* CONFIG_IEEE80211W */
}
#endif
void supplicant_event(void *ctx, enum wpa_event_type event,
		      union wpa_event_data *data)
{
	struct wpa_supplicant *wpa_s = (struct wpa_supplicant *)ctx;
	u16 reason_code = 0;

	wpa_dbg(wpa_s, MSG_DEBUG, "Event %d received on interface",
		event);

	switch (event) {
	case EVENT_ASSOC:
		wpa_supplicant_event_assoc(wpa_s, data);
		break;
	case EVENT_DISASSOC:
		wpa_dbg(wpa_s, MSG_DEBUG, "Disassociation notification");
		if (data) {
			wpa_dbg(wpa_s, MSG_DEBUG, " * reason %u",
				data->disassoc_info.reason_code);
			if (data->disassoc_info.addr)
				wpa_dbg(wpa_s, MSG_DEBUG, " * address " MACSTR,
					MAC2STR(data->disassoc_info.addr));
		}
		if (data) {
			reason_code = data->disassoc_info.reason_code;
			wpa_hexdump(MSG_DEBUG, "Disassociation frame IE(s)",
				    data->disassoc_info.ie,
				    data->disassoc_info.ie_len);
		}
#if 0
		if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_SME)
			sme_event_disassoc(wpa_s, data);
#endif
		/* fall through */
    /*lint -fallthrough */
	case EVENT_DEAUTH:
		if (event == EVENT_DEAUTH) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"Deauthentication notification");
			if (data) {
				reason_code = data->deauth_info.reason_code;
				wpa_dbg(wpa_s, MSG_DEBUG, " * reason %u",
					data->deauth_info.reason_code);
				if (data->deauth_info.addr) {
					wpa_dbg(wpa_s, MSG_DEBUG, " * address "
						MACSTR,
						MAC2STR(data->deauth_info.
							addr));
				}
				wpa_hexdump(MSG_DEBUG,
					    "Deauthentication frame IE(s)",
					    data->deauth_info.ie,
					    data->deauth_info.ie_len);
			}
		}
		wpa_supplicant_event_disassoc(wpa_s, reason_code);
		break;
	case EVENT_MICHAEL_MIC_FAILURE:
		wpa_supplicant_event_michael_mic_failure(wpa_s, data);
		break;
	case EVENT_EAPOL_RX:
		wpa_supplicant_rx_eapol(wpa_s, data->eapol_rx.src,
					            data->eapol_rx.data,
                                data->eapol_rx.data_len);
		break;
    case EVENT_AUTH:
    case EVENT_DRIVER_GTK_REKEY:
	default:
		wpa_msg(wpa_s, MSG_INFO, "Unknown event %d", event);
		break;
	}
}
