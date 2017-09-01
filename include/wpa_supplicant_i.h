/*
 * wpa_supplicant - Internal definitions
 * Copyright (c) 2003-2010, Jouni Malinen <j@w1.fi>
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

#ifndef WPA_SUPPLICANT_I_H
#define WPA_SUPPLICANT_I_H

#include <config_ssid.h>
#include "apps/mac80211/supplicant/common/defs.h"


//struct wpa_sm;
//struct wpa_supplicant;
//struct scan_info;
//struct wpa_bss;
//struct wpa_scan_results;


/**
 * struct wpa_supplicant - Internal data for wpa_supplicant interface
 *
 * This structure contains the internal data for core wpa_supplicant code. This
 * should be only used directly from the core code. However, a pointer to this
 * data is used from other files as an arbitrary context pointer in calls to
 * core functions.
 */
struct wpa_supplicant {
	unsigned char own_addr[ETH_ALEN];

	int countermeasures;
	long last_michael_mic_error;
	u8 bssid[ETH_ALEN];
	//u8 pending_bssid[ETH_ALEN]; /* If wpa_state == WPA_ASSOCIATING, this* field contains the targer BSSID. */
	//int reassociate; /* reassociation requested */
	//int disconnected; /* all connections disabled; i.e., do no reassociate* before this has been cleared */
	struct wpa_ssid *current_ssid;

	/* Selected configuration (based on Beacon/ProbeResp WPA IE) */
	int pairwise_cipher;
	int group_cipher;
	int key_mgmt;
	int wpa_proto;
	int mgmt_group_cipher;

	struct wpa_sm *wpa;

	enum wpa_states wpa_state;
	int eapol_received; /* number of EAPOL packets received after the* previous association event */
    #ifdef ENABLE_BACKGROUND_PMK_CALC
    int (*pmk_calc_task) (void *data);
    void *pmk_calc_data;
    #endif // ENABLE_BACKGROUND_PMK_CALC
	//unsigned int drv_flags;
	//int pending_mic_error_report;
	//int pending_mic_error_pairwise;
	//int mic_errors_seen; /* Michael MIC errors with the current PTK */

	//struct wpabuf *pending_eapol_rx;
	//struct os_time pending_eapol_rx_time;
	//u8 pending_eapol_rx_src[ETH_ALEN];
};


/* wpa_supplicant.c */
const char * wpa_supplicant_state_txt(enum wpa_states state);
int wpa_supplicant_set_suites(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid,
			      u8 *wpa_ie, size_t *wpa_ie_len);
void wpa_supplicant_associate(struct wpa_supplicant *wpa_s,
			      struct wpa_ssid *ssid);

void wpa_clear_keys(struct wpa_supplicant *wpa_s, const u8 *addr);
void wpa_supplicant_req_auth_timeout(struct wpa_supplicant *wpa_s,
				     u32 sec, u32 usec);

struct wpa_ssid * wpa_supplicant_get_ssid(struct wpa_supplicant *wpa_s);
void wpa_supplicant_cancel_auth_timeout(struct wpa_supplicant *wpa_s);
void wpa_supplicant_deauthenticate(struct wpa_supplicant *wpa_s,
				   int reason_code);
void wpa_supplicant_disassociate(struct wpa_supplicant *wpa_s,
				 int reason_code);

struct wpa_supplicant * wpa_supplicant_init();
void wpa_supplicant_deinit(struct wpa_supplicant *wpa_s);


void wpa_supplicant_rx_eapol(void *ctx, const u8 *src_addr,
			     u8 *buf, size_t len);

void wpa_supplicant_clear_status(struct wpa_supplicant *wpa_s);

/* events.c */
void wpa_supplicant_mark_disassoc(struct wpa_supplicant *wpa_s);

void wpa_supplicant_stop_countermeasures(void *eloop_ctx, void *sock_ctx);

int wpa_supplicant_set_key(void *_wpa_s, enum wpa_alg alg,
				  const u8 *addr, int key_idx, int set_tx,
				  const u8 *seq, size_t seq_len,
				  const u8 *key, size_t key_len);


#endif /* WPA_SUPPLICANT_I_H */
