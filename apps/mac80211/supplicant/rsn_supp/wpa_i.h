/*
 * Internal WPA/RSN supplicant state machine definitions
 * Copyright (c) 2004-2010, Jouni Malinen <j@w1.fi>
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

#ifndef WPA_I_H
#define WPA_I_H

#include "../wpa_supplicant.h"
#include "../wpas_glue.h"

/**
 * struct wpa_sm - Internal WPA state machine data
 */
struct wpa_sm {
	u8 pmk[PMK_LEN];
	size_t pmk_len;
	struct wpa_ptk ptk, tptk;
	int ptk_set, tptk_set;
	u8 snonce[WPA_NONCE_LEN];
	u8 anonce[WPA_NONCE_LEN]; /* ANonce from the last 1/4 msg */
	int renew_snonce;
	u8 rx_replay_counter[WPA_REPLAY_COUNTER_LEN];
	int rx_replay_counter_set;
	u8 request_counter[WPA_REPLAY_COUNTER_LEN];

	//struct eapol_sm *eapol; /* EAPOL state machine from upper level code */



	struct wpa_sm_ctx *ctx;



	//void *network_ctx;
	int allowed_pairwise_cipher; /* bitfield of WPA_CIPHER_* */
	//int proactive_key_caching;
	//int eap_workaround;
	//void *eap_conf_ctx;
	u8 ssid[32];
	size_t ssid_len;
	int wpa_ptk_rekey;

	u8 own_addr[ETH_ALEN];
	//const char *ifname;
	//const char *bridge_ifname;
	u8 bssid[ETH_ALEN];

	//unsigned int dot11RSNAConfigPMKLifetime;
	//unsigned int dot11RSNAConfigPMKReauthThreshold;
	//unsigned int dot11RSNAConfigSATimeout;

	//unsigned int dot11RSNA4WayHandshakeFailures;

	/* Selected configuration (based on Beacon/ProbeResp WPA IE) */
	unsigned int proto;
	unsigned int pairwise_cipher;
	unsigned int group_cipher;
	unsigned int key_mgmt;
	unsigned int mgmt_group_cipher;

	int rsn_enabled; /* Whether RSN is enabled in configuration */
	//int mfp; /* 0 = disabled, 1 = optional, 2 = mandatory */

	u8 *assoc_wpa_ie; /* Own WPA/RSN IE from (Re)AssocReq */
	size_t assoc_wpa_ie_len;
	u8 *ap_wpa_ie, *ap_rsn_ie;
	size_t ap_wpa_ie_len, ap_rsn_ie_len;
};


static inline void wpa_sm_set_state(struct wpa_sm *sm, enum wpa_states state)
{
	//sm->ctx->set_state(sm->ctx->ctx, state);
	wpa_supplicant_set_state((struct wpa_supplicant *)sm->ctx->ctx, state);
}

static inline enum wpa_states wpa_sm_get_state(struct wpa_sm *sm)
{
	//return sm->ctx->get_state(sm->ctx->ctx);
	return wpa_supplicant_get_state((struct wpa_supplicant *)sm->ctx->ctx);
}

static inline void wpa_sm_deauthenticate(struct wpa_sm *sm, int reason_code)
{
	//sm->ctx->deauthenticate(sm->ctx->ctx, reason_code);
	wpa_supplicant_deauthenticate((struct wpa_supplicant *)sm->ctx->ctx, reason_code);
}

static inline void wpa_sm_disassociate(struct wpa_sm *sm, int reason_code)
{
	//sm->ctx->disassociate(sm->ctx->ctx, reason_code);
	wpa_supplicant_disassociate((struct wpa_supplicant *)sm->ctx->ctx, reason_code);
}

static inline int wpa_sm_set_key(struct wpa_sm *sm, enum wpa_alg alg,
				 const u8 *addr, int key_idx, int set_tx,
				 const u8 *seq, size_t seq_len,
				 const u8 *key, size_t key_len)
{
	//return sm->ctx->set_key(sm->ctx->ctx, alg, addr, key_idx, set_tx,seq, seq_len, key, key_len);
	return wpa_supplicant_set_key((struct wpa_supplicant *)sm->ctx->ctx, alg, addr, key_idx, set_tx,seq, seq_len, key, key_len);
}

static inline void * wpa_sm_get_network_ctx(struct wpa_sm *sm)
{
	//return sm->ctx->get_network_ctx(sm->ctx->ctx);
	return (void *)wpa_supplicant_get_ssid((struct wpa_supplicant *)sm->ctx->ctx);
}

static inline int wpa_sm_get_bssid(struct wpa_sm *sm, u8 *bssid)
{
	//return sm->ctx->get_bssid(sm->ctx->ctx, bssid);
	//return wpa_drv_get_bssid(wpa_s, bssid);
	return 0;
}

static inline int wpa_sm_ether_send(struct wpa_sm *sm, const u8 *dest,
				    u16 proto, const u8 *buf, size_t len, u8 security)
{
	//return sm->ctx->ether_send(sm->ctx->ctx, dest, proto, buf, len);
	return wpa_ether_send((struct wpa_supplicant *)sm->ctx->ctx, dest, proto, buf, len, security);
}

static inline void wpa_sm_cancel_auth_timeout(struct wpa_sm *sm)
{
	//sm->ctx->cancel_auth_timeout(sm->ctx->ctx);
	wpa_supplicant_cancel_auth_timeout((struct wpa_supplicant *)sm->ctx->ctx);
}

static inline u8 * wpa_sm_alloc_eapol(struct wpa_sm *sm, u8 type,
				      const void *data, u16 data_len,
				      size_t *msg_len, void **data_pos)
{
	//return sm->ctx->alloc_eapol(sm->ctx->ctx, type, data, data_len,msg_len, data_pos);
	return (u8 *)wpa_alloc_eapol((struct wpa_supplicant *)sm->ctx->ctx, type, data, data_len, msg_len, data_pos);
}

static inline void wpa_sm_set_rekey_offload(struct wpa_sm *sm)
{
	//if (!sm->ctx->set_rekey_offload)
		//return;
	//sm->ctx->set_rekey_offload(sm->ctx->ctx, sm->ptk.kek,sm->ptk.kck, sm->rx_replay_counter);
	wpa_supplicant_set_rekey_offload((struct wpa_supplicant *)sm->ctx->ctx, sm->ptk.kek,sm->ptk.kck, sm->rx_replay_counter);
}


void wpa_eapol_key_send(struct wpa_sm *sm, const u8 *kck,
			int ver, const u8 *dest, u16 proto,
			u8 *msg, size_t msg_len, u8 *key_mic,u8 security);
int wpa_supplicant_send_2_of_4(struct wpa_sm *sm, const unsigned char *dst,
			       const struct wpa_eapol_key *key,
			       int ver, const u8 *nonce,
			       const u8 *wpa_ie, size_t wpa_ie_len,
			       struct wpa_ptk *ptk);
int wpa_supplicant_send_4_of_4(struct wpa_sm *sm, const unsigned char *dst,
			       const struct wpa_eapol_key *key,
			       u16 ver, u16 key_info,
			       const u8 *kde, size_t kde_len,
			       struct wpa_ptk *ptk);

#endif /* WPA_I_H */
