/*
 * WPA Supplicant - Glue code to setup EAPOL and RSN modules
 * Copyright (c) 2003-2008, Jouni Malinen <j@w1.fi>
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
#include "l2_packet/l2_packet.h"
#include "common/wpa_common.h"
#include <wpa_supplicant_i.h>
#include "drivers/driver_cabrio.h"
#include "common/ieee802_11_defs.h"
#include "wpas_glue.h"


#if defined(IEEE8021X_EAPOL) || !defined(CONFIG_NO_WPA)
u8 * wpa_alloc_eapol(const struct wpa_supplicant *wpa_s, u8 type,
			    const void *data, u16 data_len,
			    size_t *msg_len, void **data_pos)
{
	struct ieee802_1x_hdr *hdr;

	*msg_len = sizeof(*hdr) + data_len;
	hdr = (struct ieee802_1x_hdr *)os_malloc(*msg_len);
	if (hdr == NULL)
		return (u8 *)NULL;

	hdr->version = EAPOL_VERSION;
	hdr->type = type;
	hdr->length = host_to_be16(data_len);

	if (data)
		os_memcpy(hdr + 1, data, data_len);
	else
		os_memset(hdr + 1, 0, data_len);

	if (data_pos)
		*data_pos = hdr + 1;

	return (u8 *) hdr;
}


/**
 * wpa_ether_send - Send Ethernet frame
 * @wpa_s: Pointer to wpa_supplicant data
 * @dest: Destination MAC address
 * @proto: Ethertype in host byte order
 * @buf: Frame payload starting from IEEE 802.1X header
 * @len: Frame payload length
 * Returns: >=0 on success, <0 on failure
 */
int wpa_ether_send(struct wpa_supplicant *wpa_s, const u8 *dest,
			  u16 proto, const u8 *buf, size_t len, u8 security)
{
	//return l2_packet_send(NULL, dest, proto, buf, len);

	return wpa_drv_send_eapol(wpa_s, dest, proto, buf, len, security);
}
#endif /* IEEE8021X_EAPOL || !CONFIG_NO_WPA */

#ifndef CONFIG_NO_WPA
/**
 * wpa_supplicant_get_state - Get the connection state
 * @wpa_s: Pointer to wpa_supplicant data
 * Returns: The current connection state (WPA_*)
 */
enum wpa_states wpa_supplicant_get_state(struct wpa_supplicant *wpa_s)
{
	return wpa_s->wpa_state;
}

int wpa_supplicant_set_key(void *_wpa_s, enum wpa_alg alg,
				  const u8 *addr, int key_idx, int set_tx,
				  const u8 *seq, size_t seq_len,
				  const u8 *key, size_t key_len)
{
	struct wpa_supplicant *wpa_s = (struct wpa_supplicant *)_wpa_s;
	if (alg == WPA_ALG_TKIP && key_idx == 0 && key_len == 32) {
		/* Clear the MIC error counter when setting a new PTK. */
		//wpa_s->mic_errors_seen = 0;
		;
	}
	return wpa_drv_set_key(wpa_s, alg, addr, key_idx, set_tx, seq, seq_len,
			       key, key_len);
}

#endif /* CONFIG_NO_WPA */

void wpa_supplicant_set_rekey_offload(void *ctx, const u8 *kek,
					     const u8 *kck,
					     const u8 *replay_ctr)
{
	//struct wpa_supplicant *wpa_s = ctx;

	//wpa_drv_set_rekey_info(wpa_s, kek, kck, replay_ctr);
}

static struct wpa_sm_ctx ctx_st;
static struct wpa_sm_ctx *_ctx;
int wpa_supplicant_init_wpa(struct wpa_supplicant *wpa_s)
{
#ifndef CONFIG_NO_WPA
	_ctx = &ctx_st;

	_ctx->ctx = wpa_s;
	_ctx->msg_ctx = wpa_s;

	wpa_s->wpa = wpa_sm_init(_ctx);
	if (wpa_s->wpa == NULL) {
		wpa_printf(MSG_ERROR, "Failed to initialize WPA state "
			   "machine");
		return -1;
	}
#endif /* CONFIG_NO_WPA */

	return 0;
}

