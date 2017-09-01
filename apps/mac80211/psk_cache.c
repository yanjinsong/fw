#include <config.h>
#include <common.h>
#include "psk_cache.h"

#include <wpa_supplicant_i.h>
#include "supplicant/utils/common.h"
#include "supplicant/common/defs.h"
#include "supplicant/drivers/driver_cabrio.h"
#include "supplicant/common/ieee802_11_defs.h"
#include "supplicant/rsn_supp/wpa.h"
#include "supplicant/rsn_supp/wpa_i.h"

#ifdef ENABLE_PSK_CACHE

#define PSK_NUM          (4)

#define MAX_PSK_LEN      (32)

typedef struct __PskCacheElem_S {
    u8      ssid[MAX_SSID_LEN]; //ssid[] didnot include '\0' at end of matrix
    u8      password[MAX_PASSWD_LEN];
    u8      psk[MAX_PSK_LEN];
    u32     age;
} PskCacheElem_S;

static PskCacheElem_S       _psk_cache[PSK_NUM];

void init_psk_cache (void)
{
    #if 0
	const u8 pmk[] =
   {0x72, 0x7C, 0xCD, 0xB0, 0xB6, 0xD8, 0xAB, 0x60,
    0x6A, 0x88, 0xC0, 0x4A, 0x73, 0xA8, 0xEF, 0xEB,
    0x5D, 0x1C, 0xE1, 0x7D, 0xD8, 0xE5, 0xC2, 0x17,
	0xE1, 0xEB, 0x6E, 0xEF, 0xAD, 0x4F, 0xD1, 0x67};
    #endif // 0

	int i;
    for (i = 0; i < PSK_NUM; i++)
        _psk_cache[i].age = 0;
    #if 0
    memcpy(_psk_cache[0].ssid, "cisco", MAX_SSID_LEN);
    memcpy(_psk_cache[0].password, "secret00", MAX_PASSWD_LEN);
    memcpy(_psk_cache[0].psk, pmk, MAX_PSK_LEN);
	_psk_cache[0].age = 0x80000000;
    #endif // 0

 //SSID: "cisco"
 //password: "secret00"
 //PSK:
 //    9E 3D 00 EA 14 F0 9F E5 14 F0 9F E5 14 F0 9F E5
 //    14 F0 9F E5 00 00 A0 E1 10 F0 9F E5 10 F0 9F E5
} // end of - init_psk_cache -

static int _find_psk (const u8 *SSID, const u8 *password)
{
    int i;
    for (i = 0; i < PSK_NUM; i++) {
        // Valid PSK entry?
        if (_psk_cache[i].age == 0)
            continue;
        // SSID match?
        if (memcmp((void *)SSID, (void *)_psk_cache[i].ssid,MAX_SSID_LEN))
            continue;
        // PASSWORD match?
        if (memcmp((void *)password, (void *)_psk_cache[i].password,MAX_PASSWD_LEN))
            continue;
        return i;
    }
    return (-1);
} // end of - find_psk -


void add_psk_to_cache (struct wpa_supplicant *supplicant)
{
    int i;
    int empty_entry = (-1);
    int eldest_entry = (-1);
    u32 eldest_age = (u32)(-1);
    int ssid_entry = (-1);
    const u8 *ssid;
    const u8 *pmk;
    const u8 *password;

	if(supplicant == NULL)
		return;

    // Only keep WPA2/PSK's PMK
    if (   (supplicant->current_ssid->key_mgmt != WPA_KEY_MGMT_PSK)
        || (supplicant->current_ssid->pairwise_cipher != WPA_CIPHER_CCMP))
        return;

    ssid = supplicant->current_ssid->ssid;
    pmk = supplicant->wpa->pmk;
    password = (const u8 *)supplicant->current_ssid->passphrase;

    for (i = 0; i < PSK_NUM; i++) {
        // Valid PSK entry?
        if (_psk_cache[i].age == 0) {
            empty_entry = i;
            continue;
        }
        // SSID match?
        if (memcmp((const char *)ssid, (const char *)_psk_cache[i].ssid, MAX_SSID_LEN)) {
            if (_psk_cache[i].age > 1)
                _psk_cache[i].age >>= 1;
            if (eldest_age < _psk_cache[i].age)
                continue;
            eldest_age = _psk_cache[i].age;
            eldest_entry = i;
            continue;
        }
        ssid_entry = i;
    }

    if (ssid_entry == (-1)) {
        if (empty_entry == (-1))
            ssid_entry = eldest_entry;
        else
            ssid_entry = empty_entry;
    }
    if (password[0] != 0) {
        memcpy(_psk_cache[ssid_entry].ssid, ssid, MAX_SSID_LEN);
        memcpy(_psk_cache[ssid_entry].password, password, MAX_PASSWD_LEN);
        memcpy(_psk_cache[ssid_entry].psk, pmk, MAX_PSK_LEN);
        #if 0
		printf("Cache \n"
			   "\tSSID: \"%s\"\n"
			   "\tpassword: \"%s\"\n"
			   "\tPSK: \n"
			   "\t\t%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n"
			   "\t\t%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
			   _psk_cache[ssid_entry].ssid,
			   _psk_cache[ssid_entry].password,
			   _psk_cache[ssid_entry].psk[ 0], _psk_cache[ssid_entry].psk[ 1],
			   _psk_cache[ssid_entry].psk[ 2], _psk_cache[ssid_entry].psk[ 3],
			   _psk_cache[ssid_entry].psk[ 4], _psk_cache[ssid_entry].psk[ 5],
			   _psk_cache[ssid_entry].psk[ 6], _psk_cache[ssid_entry].psk[ 7],
			   _psk_cache[ssid_entry].psk[ 8], _psk_cache[ssid_entry].psk[ 9],
			   _psk_cache[ssid_entry].psk[10], _psk_cache[ssid_entry].psk[11],
			   _psk_cache[ssid_entry].psk[12], _psk_cache[ssid_entry].psk[13],
			   _psk_cache[ssid_entry].psk[14], _psk_cache[ssid_entry].psk[15],
			   _psk_cache[ssid_entry].psk[16], _psk_cache[ssid_entry].psk[17],
			   _psk_cache[ssid_entry].psk[18], _psk_cache[ssid_entry].psk[19],
			   _psk_cache[ssid_entry].psk[20], _psk_cache[ssid_entry].psk[21],
			   _psk_cache[ssid_entry].psk[22], _psk_cache[ssid_entry].psk[23],
			   _psk_cache[ssid_entry].psk[24], _psk_cache[ssid_entry].psk[25],
			   _psk_cache[ssid_entry].psk[26], _psk_cache[ssid_entry].psk[27],
			   _psk_cache[ssid_entry].psk[28], _psk_cache[ssid_entry].psk[29],
			   _psk_cache[ssid_entry].psk[30], _psk_cache[ssid_entry].psk[31]
			   );
        #endif
    }
    _psk_cache[ssid_entry].age = 0x80000000;
} // end of - add_psk_cache -


u8 *find_psk_in_cache (const u8 *SSID, const u8 *password)
{
    int ssid_entry = _find_psk(SSID, password);
    if (ssid_entry == (-1))
        return NULL;
    #if 0
		printf("Cache \n"
			   "SSID: \"%s\"\n"
			   "password: \"%s\"\n"
			   "PSK: \n"
			   "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n"
			   "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
			   _psk_cache[ssid_entry].ssid,
			   _psk_cache[ssid_entry].password,
			   _psk_cache[ssid_entry].psk[ 0], _psk_cache[ssid_entry].psk[ 1],
			   _psk_cache[ssid_entry].psk[ 2], _psk_cache[ssid_entry].psk[ 3],
			   _psk_cache[ssid_entry].psk[ 4], _psk_cache[ssid_entry].psk[ 5],
			   _psk_cache[ssid_entry].psk[ 6], _psk_cache[ssid_entry].psk[ 7],
			   _psk_cache[ssid_entry].psk[ 8], _psk_cache[ssid_entry].psk[ 9],
			   _psk_cache[ssid_entry].psk[10], _psk_cache[ssid_entry].psk[11],
			   _psk_cache[ssid_entry].psk[12], _psk_cache[ssid_entry].psk[13],
			   _psk_cache[ssid_entry].psk[14], _psk_cache[ssid_entry].psk[15],
			   _psk_cache[ssid_entry].psk[16], _psk_cache[ssid_entry].psk[17],
			   _psk_cache[ssid_entry].psk[18], _psk_cache[ssid_entry].psk[19],
			   _psk_cache[ssid_entry].psk[20], _psk_cache[ssid_entry].psk[21],
			   _psk_cache[ssid_entry].psk[22], _psk_cache[ssid_entry].psk[23],
			   _psk_cache[ssid_entry].psk[24], _psk_cache[ssid_entry].psk[25],
			   _psk_cache[ssid_entry].psk[26], _psk_cache[ssid_entry].psk[27],
			   _psk_cache[ssid_entry].psk[28], _psk_cache[ssid_entry].psk[29],
			   _psk_cache[ssid_entry].psk[30], _psk_cache[ssid_entry].psk[31]
			   );
    #endif
	return _psk_cache[ssid_entry].psk;
} // end of - find_psk -

#else // ENABLE_PSK_CACHE

void init_psk_cache (void)
{
} // end of - init_psk_cache -


void add_psk_to_cache (struct wpa_supplicant *supplicant)
{
} // end of - add_psk_cache -


u8 *find_psk_in_cache (const u8 *SSID, const u8 *password)
{
    return NULL;
} // end of - find_psk -

#endif // ENABLE_PSK_CACHE

