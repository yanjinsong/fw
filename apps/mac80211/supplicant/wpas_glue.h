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

#ifndef WPAS_GLUE_H
#define WPAS_GLUE_H

#if defined(IEEE8021X_EAPOL) || !defined(CONFIG_NO_WPA)
u8 * wpa_alloc_eapol(const struct wpa_supplicant *wpa_s, u8 type,
			    const void *data, u16 data_len,
			    size_t *msg_len, void **data_pos);

int wpa_ether_send(struct wpa_supplicant *wpa_s, const u8 *dest,
			  u16 proto, const u8 *buf, size_t len, u8 security);
#endif /* IEEE8021X_EAPOL || !CONFIG_NO_WPA */

#ifndef CONFIG_NO_WPA
enum wpa_states wpa_supplicant_get_state(struct wpa_supplicant *wpa_s);
int wpa_supplicant_set_key(void *_wpa_s, enum wpa_alg alg,
				  const u8 *addr, int key_idx, int set_tx,
				  const u8 *seq, size_t seq_len,
				  const u8 *key, size_t key_len);
#endif /* CONFIG_NO_WPA */

void wpa_supplicant_set_rekey_offload(void *ctx, const u8 *kek,const u8 *kck,const u8 *replay_ctr);
int wpa_supplicant_init_wpa(struct wpa_supplicant *wpa_s);


#endif /* WPAS_GLUE_H */
