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

#ifndef WPA_SUPPLICANT_H
#define WPA_SUPPLICANT_H
#include <wpa_supplicant_i.h>

void wpa_supplicant_req_auth_timeout(struct wpa_supplicant *wpa_s, u32 sec, u32 usec);
void wpa_supplicant_cancel_auth_timeout(struct wpa_supplicant *wpa_s);
void wpa_clear_keys(struct wpa_supplicant *wpa_s, const u8 *addr);
const char * wpa_supplicant_state_txt(enum wpa_states state);
void wpa_supplicant_set_state(struct wpa_supplicant *wpa_s,enum wpa_states state);
void wpa_supplicant_clear_status(struct wpa_supplicant *wpa_s);
int wpa_supplicant_set_suites(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid,u8 *wpa_ie, size_t *wpa_ie_len);
void wpa_supplicant_associate(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid);
void wpa_supplicant_disassociate(struct wpa_supplicant *wpa_s,int reason_code);
void wpa_supplicant_deauthenticate(struct wpa_supplicant *wpa_s,int reason_code);
struct wpa_ssid * wpa_supplicant_get_ssid(struct wpa_supplicant *wpa_s);
void wpa_supplicant_rx_eapol(void *ctx, const u8 *src_addr,u8 *buf, size_t len);
int wpa_supplicant_driver_init(struct wpa_supplicant *wpa_s);
int wpa_supplicant_remove_iface(struct wpa_supplicant *wpa_s);
struct wpa_supplicant * wpa_supplicant_init(void);
void wpa_supplicant_deinit(struct wpa_supplicant *wpa_s);

#endif /* WPA_SUPPLICANT_H */
