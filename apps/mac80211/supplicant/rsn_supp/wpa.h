/*
 * wpa_supplicant - WPA definitions
 * Copyright (c) 2003-2007, Jouni Malinen <j@w1.fi>
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

#ifndef WPA_H
#define WPA_H

#include "../common/defs.h"
#include "../common/eapol_common.h"
#include "../common/wpa_common.h"

struct wpa_sm_ctx {
	void *ctx; /* pointer to arbitrary upper level context */
	void *msg_ctx; /* upper level context for wpa_msg() calls */
};


enum wpa_sm_conf_params {
	WPA_PARAM_PROTO,
	WPA_PARAM_PAIRWISE,
	WPA_PARAM_GROUP,
	WPA_PARAM_KEY_MGMT,
	#ifdef CONFIG_IEEE80211W
	WPA_PARAM_MGMT_GROUP,
	#endif // CONFIG_IEEE80211W
	WPA_PARAM_RSN_ENABLED,
	WPA_PARAM_MFP
};


#ifndef CONFIG_NO_WPA

struct wpa_sm * wpa_sm_init(struct wpa_sm_ctx *ctx);
void wpa_sm_deinit(struct wpa_sm *sm);
void wpa_sm_notify_assoc(struct wpa_sm *sm, const u8 *bssid);
void wpa_sm_set_pmk(struct wpa_sm *sm, const u8 *pmk, size_t pmk_len);

void wpa_sm_set_own_addr(struct wpa_sm *sm, const u8 *addr);

//void wpa_sm_set_eapol(struct wpa_sm *sm, struct eapol_sm *eapol);
int wpa_sm_set_assoc_wpa_ie(struct wpa_sm *sm, const u8 *ie, size_t len);
int wpa_sm_set_assoc_wpa_ie_default(struct wpa_sm *sm, u8 *wpa_ie,
				    size_t *wpa_ie_len);
int wpa_sm_set_ap_wpa_ie(struct wpa_sm *sm, const u8 *ie, size_t len);
int wpa_sm_set_ap_rsn_ie(struct wpa_sm *sm, const u8 *ie, size_t len);

int wpa_sm_set_param(struct wpa_sm *sm, enum wpa_sm_conf_params param,
		     unsigned int value);
unsigned int wpa_sm_get_param(struct wpa_sm *sm,
			      enum wpa_sm_conf_params param);

void wpa_sm_key_request(struct wpa_sm *sm, int error, int pairwise);

int wpa_parse_wpa_ie(const u8 *wpa_ie, size_t wpa_ie_len,
		     struct wpa_ie_data *data);

int wpa_sm_rx_eapol(struct wpa_sm *sm, const u8 *src_addr,
		    u8 *buf, size_t len);
int wpa_sm_parse_own_wpa_ie(struct wpa_sm *sm, struct wpa_ie_data *data);

int wpa_sm_has_ptk(struct wpa_sm *sm);

void wpa_sm_update_replay_ctr(struct wpa_sm *sm, const u8 *replay_ctr);

#else /* CONFIG_NO_WPA */
#endif /* CONFIG_NO_WPA */


#endif /* WPA_H */
