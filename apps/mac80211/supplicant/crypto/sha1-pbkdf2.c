/*
 * SHA1-based key derivation function (PBKDF2) for IEEE 802.11i
 * Copyright (c) 2003-2005, Jouni Malinen <j@w1.fi>
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

#include "../supplicant_config.h"

#include "../utils/common.h"
#include "sha1.h"
#include "md5.h"
#include "crypto.h"
#include <dbg_timer/dbg_timer.h>
#ifdef ENABLE_DYNAMIC_HMAC_SHA1_BUF
#include <pbuf.h>
#endif // ENABLE_DYNAMIC_HMAC_SHA1_BUF

#ifndef ENABLE_BACKGROUND_PMK_CALC

static int pbkdf2_sha1_f(const char *passphrase, const char *ssid,
                         size_t ssid_len, int iterations, unsigned int count,
                         u8 *digest)
{
    int i, j;
    unsigned char count_buf[4];
    const u8 *addr[2];
    size_t len[2];
    size_t passphrase_len;
    u32 pass_buf[32]; // Use U32 buffer to ensure word-alignment process.
    const char *pass;
    HMAC_SHA1_LOOP_DATA_S *hmac_sha1_loop_data;
    u32                   *loop_mac;
    u32                    digest_u32[SHA1_MAC_LEN/sizeof(u32)];

	#ifdef ENABLE_DYNAMIC_HMAC_SHA1_BUF
    hmac_sha1_loop_data = (HMAC_SHA1_LOOP_DATA_S *)PBUF_MAlloc_Raw(get_hmac_sha1_loop_data_size(), 0, SOC_PBUF);
    if (hmac_sha1_loop_data == NULL)
        return (-1);
	#endif

    // If passphrase is not 4-byte aligned, use aligned buffer to make later process 
    // could apply word arithmatic.
    if (((u32)passphrase % 4))
    {
        char *pass_data = (char *)pass_buf;
        for (passphrase_len = 0; passphrase_len < sizeof(pass_buf); passphrase_len++)
            if ((pass_data[passphrase_len] = passphrase[passphrase_len]) == 0)
                break;
        ASSERT(passphrase_len < sizeof(pass_buf));
        pass = pass_data;
    }
    else
    {
        passphrase_len = os_strlen(passphrase);
        pass = passphrase;
    }

    addr[0] = (const u8 *) ssid;
    len[0] = ssid_len;
    addr[1] = count_buf;
    len[1] = 4;

    /* F(P, S, c, i) = U1 xor U2 xor ... Uc
     * U1 = PRF(P, S || i)
     * U2 = PRF(P, U1)
     * Uc = PRF(P, Uc-1)
     */

    count_buf[0] = (count >> 24) & 0xff;
    count_buf[1] = (count >> 16) & 0xff;
    count_buf[2] = (count >> 8) & 0xff;
    count_buf[3] = count & 0xff;

	#ifdef ENABLE_DYNAMIC_HMAC_SHA1_BUF
    init_hmac_sha1_loop(hmac_sha1_loop_data, pass, passphrase_len, 2, addr, len);
	#else
    hmac_sha1_loop_data = init_hmac_sha1_loop(pass, passphrase_len, 2, addr, len);
	#endif // ENABLE_DYNAMIC_HMAC_SHA1_BUF
    loop_mac = (u32 *)hmac_sha1_loop_data->mac;
    os_memcpy((void *)digest_u32, (const u8 *)loop_mac, SHA1_MAC_LEN);
        
    for (i = 1; i < iterations; i++) {
        loop_hmac_sha1(hmac_sha1_loop_data);

        for (j = 0; j < (SHA1_MAC_LEN / 4); j++) {      
            digest_u32[j] ^= loop_mac[j];
        }
    }
    os_memcpy((void *)digest, (const u8 *)digest_u32, SHA1_MAC_LEN);
    stop_hmac_sha1_loop(hmac_sha1_loop_data);    
	#ifdef ENABLE_DYNAMIC_HMAC_SHA1_BUF
    PBUF_MFree(hmac_sha1_loop_data);
	#endif // ENABLE_DYNAMIC_HMAC_SHA1_BUF
    return 0;
}


/**
 * pbkdf2_sha1 - SHA1-based key derivation function (PBKDF2) for IEEE 802.11i
 * @passphrase: ASCII passphrase
 * @ssid: SSID
 * @ssid_len: SSID length in bytes
 * @iterations: Number of iterations to run
 * @buf: Buffer for the generated key
 * @buflen: Length of the buffer in bytes
 * Returns: 0 on success, -1 of failure
 *
 * This function is used to derive PSK for WPA-PSK. For this protocol,
 * iterations is set to 4096 and buflen to 32. This function is described in
 * IEEE Std 802.11-2004, Clause H.4. The main construction is from PKCS#5 v2.0.
 */
int pbkdf2_sha1(const char *passphrase, const char *ssid, size_t ssid_len,
                int iterations, u8 *buf, size_t buflen)
{
    unsigned int count = 0;
    unsigned char *pos = buf;
    size_t left = buflen, plen;
    unsigned char digest[SHA1_MAC_LEN];

    while (left > 0) {
        count++;
        if (pbkdf2_sha1_f(passphrase, ssid, ssid_len, iterations,
                  count, digest))
            return -1;
        plen = left > SHA1_MAC_LEN ? SHA1_MAC_LEN : left;
        os_memcpy((void *)pos, (const u8 *)digest, plen);
        pos += plen;
        left -= plen;
    }

    return 0;
}


int pbkdf2_sha1_flatten (const char *passphrase, const char *ssid, size_t ssid_len,
                         int iterations, u8 *buf, size_t buflen)
{
    u32                     digest_u32[SHA1_MAC_LEN/4];
    u8                      count_buf[4] = {0, 0, 0, 1};
    int                     i, j;
    size_t                  passphrase_len = os_strlen(passphrase);
    const u8               *addr[2] = {(const u8 *) ssid, count_buf};
    size_t                  len[2] = {ssid_len, 4};
    HMAC_SHA1_LOOP_DATA_S  *hmac_sha1_loop_data;
    u32                    *loop_mac;

	#ifdef ENABLE_DYNAMIC_HMAC_SHA1_BUF
    hmac_sha1_loop_data = (HMAC_SHA1_LOOP_DATA_S *)PBUF_MAlloc_Raw(get_hmac_sha1_loop_data_size(), 0, SOC_PBUF);
    if (hmac_sha1_loop_data == NULL)
        return (-1);
    
    init_hmac_sha1_loop(hmac_sha1_loop_data, passphrase, passphrase_len, 2, addr, len);
	#else // ENABLE_DYNAMIC_HMAC_SHA1_BUF
	hmac_sha1_loop_data = init_hmac_sha1_loop(passphrase, passphrase_len, 2, addr, len);
	#endif // ENABLE_DYNAMIC_HMAC_SHA1_BUF
    loop_mac = (u32 *)hmac_sha1_loop_data->mac;
    for (i = 0; i < (SHA1_MAC_LEN/4); i++)
        digest_u32[i] = loop_mac[i];
        
    for (i = 1; i < iterations; i++) {
        loop_hmac_sha1(hmac_sha1_loop_data);
        for (j = 0; j < (SHA1_MAC_LEN / 4); j++) {      
            digest_u32[j] ^= loop_mac[j];
        }
    }
    os_memcpy((void *)buf, (const u8 *)digest_u32, SHA1_MAC_LEN);

    count_buf[3] = 2;

    reinit_hmac_sha1_loop(hmac_sha1_loop_data, 2, addr, len);
    for (i = 0; i < (SHA1_MAC_LEN/4); i++)
        digest_u32[i] = loop_mac[i];
        
    for (i = 1; i < iterations; i++) {
        loop_hmac_sha1(hmac_sha1_loop_data);
        for (j = 0; j < (SHA1_MAC_LEN / 4); j++) {      
            digest_u32[j] ^= loop_mac[j];
        }
    }
    os_memcpy((void *)(buf+SHA1_MAC_LEN), (const u8 *)digest_u32, buflen - SHA1_MAC_LEN);
    stop_hmac_sha1_loop(hmac_sha1_loop_data);

	#ifdef ENABLE_DYNAMIC_HMAC_SHA1_BUF
    PBUF_MFree(hmac_sha1_loop_data);
	#endif // ENABLE_DYNAMIC_HMAC_SHA1_BUF
    return 0;
}

#else // ! ENABLE_BACKGROUND_PMK_CALC

typedef struct _PMK_CALC_DATA_PRIV_S {
    union {
    u8                      count_buf[4];
    u32                     count;
    };
    const u8               *addr[2];
    size_t                  len[2];
    HMAC_SHA1_LOOP_DATA_S  *hmac_sha1_loop_data;
    u32                     total_iterations;
    u32                     max_block;
    u32                     cur_iterations;
    u32                     cur_sub_iter;
    u32                     cur_block;
    u32                     digest_u32[SHA1_MAC_LEN/4];
} PMK_CALC_DATA_PRIV_S;

#ifndef ENABLE_DYNAMIC_HMAC_SHA1_BUF
static PMK_CALC_DATA_S         _pmk_calc_data;
static PMK_CALC_DATA_PRIV_S    _pmk_calc_data_priv;
#else
#define PMK_CALC_DATA_SIZE         ARRAY_ELEM_SIZE(PMK_CALC_DATA_S)
#define PMK_CALC_DATA_PRIV_SIZE    ARRAY_ELEM_SIZE(PMK_CALC_DATA_PRIV_S)
#endif // ENABLE_DYNAMIC_HMAC_SHA1_BUF

PMK_CALC_DATA_S *init_pbkdf2_sha1_calc (const u8 *passphrase, const u8 *ssid,
                                        u32 ssid_len, u32 iterations)
{
    size_t   passphrase_len = os_strlen((const char *)passphrase);
    u32     *loop_mac;
    int      i;

    PMK_CALC_DATA_S        *p_pmk_calc_data;
    PMK_CALC_DATA_PRIV_S   *p_pmk_calc_data_priv;
    HMAC_SHA1_LOOP_DATA_S  *hmac_sha1_loop_data;
    
	#ifdef ENABLE_DYNAMIC_HMAC_SHA1_BUF
    size_t   buf_size =   get_hmac_sha1_loop_data_size()
                        + PMK_CALC_DATA_SIZE + PMK_CALC_DATA_PRIV_SIZE;
    
    p_pmk_calc_data = (PMK_CALC_DATA_S *)PBUF_MAlloc_Raw(buf_size, 0, SOC_PBUF);

    if (p_pmk_calc_data == NULL)
        return NULL;
    p_pmk_calc_data_priv    = (PMK_CALC_DATA_PRIV_S *)(&p_pmk_calc_data[1]);
    hmac_sha1_loop_data     = (HMAC_SHA1_LOOP_DATA_S *)(&p_pmk_calc_data_priv[1]);
    #else
    p_pmk_calc_data         = &_pmk_calc_data;
    p_pmk_calc_data_priv    = &_pmk_calc_data_priv;
	#endif // ENABLE_DYNAMIC_HMAC_SHA1_BUF
    

    #ifdef PROFILE_SUPPLICANT
    dbg_get_time(&p_pmk_calc_data->pmk_begin_time);
    #endif // PROFILE_SUPPLICANT
    
    p_pmk_calc_data_priv->count                = 0;
    p_pmk_calc_data_priv->count_buf[3]         = 1;
    p_pmk_calc_data_priv->addr[0]              = ssid;
    p_pmk_calc_data_priv->addr[1]              = p_pmk_calc_data_priv->count_buf;
    p_pmk_calc_data_priv->len[0]               = ssid_len;
    p_pmk_calc_data_priv->len[1]               = 4;
    p_pmk_calc_data_priv->total_iterations     = iterations;
    p_pmk_calc_data_priv->max_block            = 1; // Two blocks for PMK.
    p_pmk_calc_data_priv->cur_iterations       = 1;
    p_pmk_calc_data_priv->cur_sub_iter         = 1; // init_hmac_sha1_loop takes 1 iteration.
    p_pmk_calc_data_priv->cur_block            = 0;
	#ifdef ENABLE_DYNAMIC_HMAC_SHA1_BUF
	init_hmac_sha1_loop(hmac_sha1_loop_data, passphrase, passphrase_len, 2, 
                        p_pmk_calc_data_priv->addr, p_pmk_calc_data_priv->len);
	#else // ENABLE_DYNAMIC_HMAC_SHA1_BUF
    hmac_sha1_loop_data  = init_hmac_sha1_loop(passphrase, passphrase_len, 
                                               2, 
                                               p_pmk_calc_data_priv->addr, 
                                               p_pmk_calc_data_priv->len);
	#endif // ENABLE_DYNAMIC_HMAC_SHA1_BUF
    p_pmk_calc_data_priv->hmac_sha1_loop_data = hmac_sha1_loop_data;
    loop_mac = (u32 *)hmac_sha1_loop_data->mac;
    for (i = 0; i < (SHA1_MAC_LEN/4); i++)
        p_pmk_calc_data->pmk_u32_block[0][i] = loop_mac[i];

    p_pmk_calc_data->pbkdf2_sha1_data = p_pmk_calc_data_priv;

    return p_pmk_calc_data;
} // end of - init_pbkdf2_sha1_calc -


int pbkdf2_sha1_calc_func (PMK_CALC_DATA_S * pmk_calc_data, u32 iterations)
{
    int                     i, j, k;
    PMK_CALC_DATA_PRIV_S   *pbkdf2_sha1_data = (PMK_CALC_DATA_PRIV_S *)pmk_calc_data->pbkdf2_sha1_data;
    HMAC_SHA1_LOOP_DATA_S  *hmac_sha1_loop_data = pbkdf2_sha1_data->hmac_sha1_loop_data;
    u32                    *loop_mac = (u32 *)hmac_sha1_loop_data->mac;
    u32                    *digest_u32 = pmk_calc_data->pmk_u32_block[pbkdf2_sha1_data->cur_block];

    for (i = pbkdf2_sha1_data->cur_iterations, k = pbkdf2_sha1_data->cur_sub_iter; 
         i < pbkdf2_sha1_data->total_iterations && k < iterations; 
         i++, k++) {
        loop_hmac_sha1(hmac_sha1_loop_data);
        for (j = 0; j < (SHA1_MAC_LEN / 4); j++) {      
            digest_u32[j] ^= loop_mac[j];
        }
    }

    // Not finish a MAC block yet?
    if (i < pbkdf2_sha1_data->total_iterations) {
        pbkdf2_sha1_data->cur_iterations = i;
        pbkdf2_sha1_data->cur_sub_iter = 0;
        return 1; 
    }
    // No more MAC block?
    if (pbkdf2_sha1_data->cur_block == pbkdf2_sha1_data->max_block)  {
	    stop_hmac_sha1_loop(hmac_sha1_loop_data);    
        #ifdef PROFILE_SUPPLICANT
        pmk_calc_data->pmk_begin_time.ts.lt = dbg_get_elapsed(&pmk_calc_data->pmk_begin_time);
        pmk_calc_data->pmk_begin_time.ts.ut = 0;
        #endif // PROFILE_SUPPLICANT
        return 0; 
    }
    // Next block.
    pbkdf2_sha1_data->cur_block++; 
    pbkdf2_sha1_data->count_buf[3] = 2;
    pbkdf2_sha1_data->cur_iterations = 1;
    reinit_hmac_sha1_loop(hmac_sha1_loop_data, 2, 
                          pbkdf2_sha1_data->addr, pbkdf2_sha1_data->len);
    digest_u32 = pmk_calc_data->pmk_u32_block[pbkdf2_sha1_data->cur_block];
    k++;
    for (i = 0; i < (SHA1_MAC_LEN/4); i++)
        digest_u32[i] = loop_mac[i];
    if (k == iterations) {
        pbkdf2_sha1_data->cur_sub_iter = 0;
        return 1;
    }
    // Continue remain iterations to next block.
    pbkdf2_sha1_data->cur_sub_iter = k;
    return pbkdf2_sha1_calc_func(pmk_calc_data, iterations);
} // end of - pbkdf2_sha1_calc_func -


void deinit_pbkdf2_sha1_calc (PMK_CALC_DATA_S *pmk_calc_data)
{
	#ifdef ENABLE_DYNAMIC_HMAC_SHA1_BUF
    if (pmk_calc_data)
        PBUF_MFree(pmk_calc_data);
	#endif // ENABLE_DYNAMIC_HMAC_SHA1_BUF
} // end of - deinit_pbkdf2_sha1_calc -

#endif // ENABLE_BACKGROUND_PMK_CALC

