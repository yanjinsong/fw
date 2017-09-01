#ifndef __PSK_CACHE_H__
#define __PSK_CACHE_H__

struct wpa_supplicant;

void init_psk_cache (void);
void add_psk_to_cache (struct wpa_supplicant *supplicant);
u8 *find_psk_in_cache (const u8 *SSID, const u8 *password);

#endif // __PSK_CACHE_H__

