/*
 * wpa_supplicant - Internal driver interface wrappers
 * Copyright (c) 2003-2009, Jouni Malinen <j@w1.fi>
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

#ifndef DRV_SECURITY_I_H
#define DRV_SECURITY_I_H

#include <config.h>
#include <regs.h>
#include <pbuf.h>
#include <soc_global.h>
#include "drv_security.h"
#include <hwmac/drv_mac.h>


SRAM_KEY *pGSRAM_KEY;

s32 wpa_drv_init(void)
{
    #define GET_PKID_OFFSET	16
    u32 sec_pbuf_addr;
//    u32 test;
    if (g_soc_table == NULL)
        return -1;
    sec_pbuf_addr = (u32)g_soc_table->sec_key_tbl;
	printf("Security address[%08x]\n",sec_pbuf_addr);
#if (CONFIG_SIM_PLATFORM == 0 )	
	drv_mac_set_security_pkt_id((sec_pbuf_addr>>GET_PKID_OFFSET));
#endif
	pGSRAM_KEY = (SRAM_KEY *)sec_pbuf_addr;
    return 0;
}


#endif /* DRV_SECURITY_I_H */
