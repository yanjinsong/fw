/*
 * Driver interface definition
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
 *
 * This file defines a driver interface used by both %wpa_supplicant and
 * hostapd. The first part of the file defines data structures used in various
 * driver operations. This is followed by the struct wpa_driver_ops that each
 * driver wrapper will beed to define with callback functions for requesting
 * driver operations. After this, there are definitions for driver event
 * reporting with wpa_supplicant_event() and some convenience helper functions
 * that can be used to report events.
 */

#ifndef _DRV_SECURITY_H_
#define _DRV_SECURITY_H_

#define HW_SECURITY_SUPPORT_WSID		8

#define SECURITY_KEY_LEN	32
typedef struct SECURITY_KEY_st
{
	//TK(0-15)MICRX(16-23)MICTX(24-31)
    u8          key[SECURITY_KEY_LEN];
	u64			tx_pn;
	u64        	rx_pn;
} PACKED SECURITY_KEY, *PSECURITY_KEY;


typedef struct WSID_SECURITY_st
{    
	u8         	pair_key_idx:4;		//0-3
	u8         	group_key_idx:4;	//0-3
	u8			reserve[3];
	//[0]	-pairwise key
	//[1-3]	-group key
	SECURITY_KEY	pair;
}  PACKED  WSID_SECURITY, *PWSID_SECURITY ;

typedef struct SRAM_KEY_st
{    
	SECURITY_KEY	group[3]; 	// 3*48
	WSID_SECURITY	wsid[HW_SECURITY_SUPPORT_WSID];	// (48+4)*HW_SECURITY_SUPPORT_WSID
} PACKED SRAM_KEY, *SRAM_PKEY;

extern SRAM_KEY *pGSRAM_KEY;

s32 wpa_drv_init(void);


#endif /* _DRV_SECURITY_H_ */

