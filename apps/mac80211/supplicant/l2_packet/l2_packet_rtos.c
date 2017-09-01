/*
 * WPA Supplicant - Layer2 packet handling example with dummy functions
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
 *
 * This file can be used as a starting point for layer2 packet implementation.
 */

#include "../supplicant_config.h"

#include "../utils/common.h"
#include "../utils/eloop.h"
#include "../drivers/driver_cabrio.h"
#include "l2_packet.h"

struct l2_packet_data {
	//char ifname[17];
	//u8 own_addr[ETH_ALEN];
	void (*rx_callback)(void *ctx, const u8 *src_addr, u8 *buf, size_t len);
	unsigned short	protocol;
	void *rx_callback_ctx;
	u8 vaild;
};

#define MAX_L2_PACKET	5
struct l2_packet_data l2_st[MAX_L2_PACKET];
static u8 l2_packet_count = 0;

void l2_packet_receive(void *buffer,u8 *src_addr, unsigned short protocol, size_t len)
{
	s32 x;

	for(x=0;x<MAX_L2_PACKET;x++)
	{
		if(l2_st[x].protocol == protocol)
		{
			if(l2_st[x].rx_callback_ctx)
				l2_st[x].rx_callback(l2_st[x].rx_callback_ctx, src_addr, (u8 *)buffer, len);
			break;
		}
	}
}

struct l2_packet_data * l2_packet_init(
	const char *ifname, const u8 *own_addr, unsigned short protocol,
	void (*rx_callback)(void *ctx, const u8 *src_addr,
			    u8 *buf, size_t len),
	void *rx_callback_ctx, int l2_hdr)
{
	struct l2_packet_data *l2;
	u8 x,free_l2=0;
	if(l2_packet_count == MAX_L2_PACKET)
		return (struct l2_packet_data *)NULL;

	for(x=0;x<MAX_L2_PACKET;x++)
	{
		if(l2_st[x].vaild == 0)
		{
			free_l2 = x;
			break;
		}
	}
	l2 = &l2_st[free_l2];

	l2->rx_callback = rx_callback;
	l2->vaild = 1;
	l2->protocol = protocol;
	l2->rx_callback_ctx = rx_callback_ctx;

	l2_packet_count++;

	return l2;
}

void l2_packet_deinit(struct l2_packet_data *l2)
{
	u8 x;

	for(x=0;x<MAX_L2_PACKET;x++)
	{
		if((l2_st[x].vaild == 1) && (l2_st[x].protocol == l2->protocol))
		{
			l2_st[x].vaild = 0;
			l2_packet_count --;
			break;
		}
	}	
}


