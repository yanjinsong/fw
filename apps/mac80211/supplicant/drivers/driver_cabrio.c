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

#ifndef DRIVER_CABRIO_I_H
#define DRIVER_CABRIO_I_H

#include <config.h>
#include "../utils/common.h"

#include "driver_cabrio.h"
#include <hw_regs_api.h>
#include <hdr80211.h>
#include <log.h>

#include <pbuf.h>
#include <msgevt.h>
#include <mbox/drv_mbox.h>
#include <dma/drv_dma.h>
#include <hwmac/drv_mac.h>
#include <security/drv_security.h>
#include "../../sta_info.h"
#include <cmd_def.h>

/* driver_ops */
int wpa_drv_set_countermeasures(struct wpa_supplicant *wpa_s,
					      int enabled)
{
	return -1;
}

int wpa_drv_get_bssid(struct wpa_supplicant *wpa_s, u8 *bssid)
{
    drv_mac_get_bssid(wpa_s->bssid);
	if(bssid)
		memcpy(bssid, wpa_s->bssid, ETH_ALEN);
	return 1;
}

int wpa_drv_get_ssid(struct wpa_supplicant *wpa_s, u8 *ssid)
{
	if(wpa_s->current_ssid)
	{
		if(ssid)
			memcpy(ssid,wpa_s->current_ssid->ssid, wpa_s->current_ssid->ssid_len);
		return wpa_s->current_ssid->ssid_len;
	}
	return 0;
}

u8 sram_del_key(u8 wsid,u8 key_idx)
{
	if(key_idx)
	{
		memset(pGSRAM_KEY->group[key_idx-1].key,0x00,SECURITY_KEY_LEN);
		pGSRAM_KEY->group[key_idx-1].tx_pn = 0;
		pGSRAM_KEY->group[key_idx-1].rx_pn = 0;
		pGSRAM_KEY->wsid[wsid].group_key_idx = 0;
	}
	else
	{
		memset(pGSRAM_KEY->wsid[wsid].pair.key,0x00,SECURITY_KEY_LEN);
		pGSRAM_KEY->wsid[wsid].pair.tx_pn = 0;
		pGSRAM_KEY->wsid[wsid].pair.rx_pn = 0;
		pGSRAM_KEY->wsid[wsid].pair_key_idx = 0;
	}
	return 1;
}

u8 sram_set_key(u8 wsid,int key_idx,const u8 *key,size_t key_len)
{
	if(key_idx)
	{
		//printf("key_len %d key_idx %d\n",key_len,key_idx);
		memcpy((void *)pGSRAM_KEY->group[key_idx-1].key,key,(u16)key_len);
		pGSRAM_KEY->group[key_idx-1].tx_pn = 1;
		pGSRAM_KEY->group[key_idx-1].rx_pn = 0;
		pGSRAM_KEY->wsid[wsid].group_key_idx = key_idx;
	}
	else
	{
		//printf("key_len %d key_idx %d\n",key_len,key_idx);
		memcpy((void *)pGSRAM_KEY->wsid[wsid].pair.key,key,(u16)key_len);
		pGSRAM_KEY->wsid[wsid].pair.tx_pn = 1;
		pGSRAM_KEY->wsid[wsid].pair.rx_pn = 0;
		pGSRAM_KEY->wsid[wsid].pair_key_idx = key_idx;
	}
	return 1;
}

u8 srame_get_rx_pn(u8 wsid, bool unicast, u8 *rx_pn)
{
    u8 keyidx=0;
    if(unicast)
    {
    	keyidx = pGSRAM_KEY->wsid[wsid].pair_key_idx;
    	if(keyidx)
    	{
    	    rx_pn = (u8 *)&pGSRAM_KEY->group[keyidx-1].rx_pn;
    	}
    	else
    	{
    	    rx_pn = (u8 *)&pGSRAM_KEY->wsid[wsid].pair.rx_pn;
    	}
    }
    else
    {
    	keyidx = pGSRAM_KEY->wsid[wsid].group_key_idx;
    	if(keyidx)
    	{
    	    rx_pn = (u8 *)&pGSRAM_KEY->group[keyidx-1].rx_pn;
    	}
    	else
    	{
    	    rx_pn = (u8 *)&pGSRAM_KEY->wsid[wsid].pair.rx_pn;
    	}
    }
    return 1;
}

u8 sram_set_wep(u8 wsid,int key_idx,const u8 *key,size_t key_len)
{

    pGSRAM_KEY->wsid[wsid].group_key_idx = key_idx;
    pGSRAM_KEY->wsid[wsid].pair_key_idx = key_idx;

    if(key_idx)
    {
        memcpy(pGSRAM_KEY->group[key_idx-1].key,key,key_len);
    }
    else
    {
        memcpy(pGSRAM_KEY->wsid[wsid].pair.key,key,key_len);
    }

    return 1;
}


int wpa_drv_set_key(struct wpa_supplicant *wpa_s,
				  enum wpa_alg alg, const u8 *addr,
				  int key_idx, int set_tx,
				   const u8 *seq, size_t seq_len,
				   const u8 *key, size_t key_len)
{

	//u8 wsid = wsid_lookup(wpa_s->wpa.bssid);
	s8 wsid = WLAN_MAX_STA;
	u8 wen_index;


    mwsid_entry wen;
	u8 cipher, key_type = (u8)SECURITY_NONE;

	if(addr == NULL)
		key_type = WSID_NOT_FOUND;
	else if(is_broadcast_ether_addr(addr))
		key_type = WSID_GROUP_ADDR;
	else
		key_type = 0;


	if (alg == WPA_ALG_NONE) {
		if(key_type == WSID_NOT_FOUND)
		{
		    printf("clear group and pair secuirty\n");
            drv_mac_set_group_security(SECURITY_NONE);
            drv_mac_set_pair_security(SECURITY_NONE);
			if(addr == NULL)
			{
				//only clear pairwise key
				//sram_del_key(0, 0);
				//sram_del_key(1, 0);
				printf("clear all key\r\n");
			}

			return 0;
		}
		else
		{
			for( wen_index=0;wen_index<WLAN_MAX_STA;wen_index++)
			{
				drv_mac_get_wsid_table(wen_index,&wen);
				if(memcmp(wen.sta_mac.addr,addr,ETHER_ADDR_LEN)==0)
				{
					wsid = wen_index;
					printf("=====wsid:%d,key_index:%d====\n\r",wen_index,key_idx);
					break;
				}
			}
			if(wsid < WLAN_MAX_STA)
			{
				//only clear pairwise key
				//sram_del_key(wsid, 0);
				printf("clear key: wsid=%d\r\n",wsid);
			}
			return 0;
		}
    }

	switch(alg)
	{
		case WPA_ALG_WEP:
			if(key_len == 5)
				cipher = SECURITY_WEP40;
			else
				cipher = SECURITY_WEP104;
			break;
		case WPA_ALG_TKIP:
			cipher = SECURITY_TKIP;
			break;
		case WPA_ALG_CCMP:
			cipher = SECURITY_CCMP;
			break;
        case WPA_ALG_NONE:
        case WPA_ALG_IGTK:
        case WPA_ALG_PMK:
		default:
			cipher = SECURITY_NONE;
			break;
	}

	//Just for WEP setting
	if(set_tx == 2)
	{
		//For WEP mode -- we must setting group and pair register to same mode.
		printf("set the wep \n\r");
        drv_mac_set_group_security(cipher);
        drv_mac_set_pair_security(cipher);
		sram_set_wep(0,key_idx,key,key_len);
        sram_set_wep(1,key_idx,key,key_len);
		return 0;
	}
	//WPA & WPA2
	else
	{
		switch(key_type)
		{
			case WSID_GROUP_ADDR:
#if 0
				printf("WSID_GROUP_ADDR key");
				for(i=0;i<key_len;i++)
					printf("%x",key[i]);
				printf("\n");
				printf("WSID_GROUP_ADDR key_len %d key_idx %d\n",key_len,key_idx);
#endif
				/* Group Key */
                drv_mac_set_group_security(cipher);
                printf("=====group ,key_index:%d====\n\r",key_idx);
                /*
                  This is a special case on Apple AP, its WEP mode is a special combiation:GROUP CYPHER is WEP and PAIRWISE CYPYER is CCK or TKIP.
                  In general case, the key index for GROUP KEY is 1, and the index of PAIRWISE KEY is 0, but they are both zero in this case.
                  The api of sram_set_wep can't support the key index of GROUP KEY is 0.                                                                                                                                                      
                */
                if(alg==WPA_ALG_WEP)
                {
                    if(key_idx==0)
                    {
                        key_idx=1;
                    }

                }
		//sram_set_key(wsid,key_idx,key,key_len);
                sram_set_key(0,key_idx,key,key_len);
                sram_set_key(1,key_idx,key,key_len); //group key

				break;
			case WSID_NOT_FOUND:
				return -1;
			default:
#if 0
				printf("Pairwise key");
				for(i=0;i<key_len;i++)
					printf("%x",key[i]);
				printf("\n");
				printf("Pairwise key_len %d key_idx %d\n",key_len,key_idx);
#endif
				/* Pairwise Key */
                 for( wen_index=0;wen_index<WLAN_MAX_STA;wen_index++)
                 {
                     drv_mac_get_wsid_table(wen_index,&wen);
                     if(memcmp(wen.sta_mac.addr,addr,ETHER_ADDR_LEN)==0)
                     {
                         wsid = wen_index;
                         printf("=====pair key =>wsid:%d,key_index:%d====\n\r",wen_index,key_idx);
                         break;
                     }
                 }
                drv_mac_set_pair_security(cipher);

                if(wsid < WLAN_MAX_STA)
				    sram_set_key(wsid,key_idx,key,key_len);
				break;
		}
	}
	return 0;
}

const u8 * wpa_drv_get_mac_addr(struct wpa_supplicant *wpa_s)
{
    drv_mac_get_sta_mac(wpa_s->own_addr);
	return (const u8 *)NULL;
}

int wpa_drv_send_eapol(struct wpa_supplicant *wpa_s, const u8 *dest,
			  u16 proto, const u8 *buf, size_t len, u8 security)
{
	HDR8023_Data *p8023data;
	PKT_TxInfo *TxPkt;
	u32 size;
	u16 encap_offset=0;
	ETHER_ADDR	a4;
	u16 		qos_value=0x07;
	u32			ht;
	u8			*payload,*pointer;

	if(drv_mac_get_op_mode()==3)
		encap_offset += ETHER_ADDR_LEN;

	if(drv_mac_get_qos_en()==1)
		encap_offset += IEEE80211_QOS_CTL_LEN;

	if(drv_mac_get_ht_mode()!=0)
		encap_offset += IEEE80211_HT_CTL_LEN;

	size = encap_offset + ETHER_HDR_LEN + len;
	TxPkt = (PKT_TxInfo *)PBUF_MAlloc(size ,SOC_PBUF);
	ASSERT_RET(TxPkt != NULL, 0);

	/* Fill TxPkt descriptor */
	TxPkt->len		= encap_offset + ETHER_HDR_LEN + len; //MAC header+reason_code
	TxPkt->c_type	= M0_TXREQ;
	TxPkt->f80211	= 0;
	TxPkt->qos		= (drv_mac_get_qos_en()==1)?1:0;
	TxPkt->ht		= (drv_mac_get_ht_mode()!=0)?1:0;
	TxPkt->use_4addr = (drv_mac_get_op_mode()==3)?1:0;
	TxPkt->RSVD_0 = 0;
	TxPkt->more_data = 0;
	TxPkt->security = security;
	/* IEEE 802.11 MGMT frame header */
	pointer = (u8 *)TxPkt + drv_mac_get_pbuf_offset();
	if(TxPkt->use_4addr)
	{
		//Write address 4
		memcpy(pointer, a4.addr, ETHER_ADDR_LEN);
		pointer += ETHER_ADDR_LEN;
	}
	if(TxPkt->qos)
	{
		//Write Qos 2 bytes
		memcpy(pointer, &qos_value, IEEE80211_QOS_CTL_LEN);
		pointer += IEEE80211_QOS_CTL_LEN;
	}
	if(TxPkt->ht)
	{
		//Write HT 4 bytes
		memcpy(pointer, &ht, IEEE80211_HT_CTL_LEN);
		pointer += IEEE80211_HT_CTL_LEN;
	}
	p8023data = (HDR8023_Data *)pointer;
	payload = pointer + sizeof(HDR8023_Data);

	memcpy(p8023data->dest.addr, dest, ETHER_ADDR_LEN);
	drv_mac_get_sta_mac(p8023data->src.addr);
	p8023data->protocol = (proto>>8) | (proto<<8);

	memcpy((void*)payload, (void*)buf, (u16)len);

	if(-1==TX_FRAME((u32)TxPkt))
	{
        PBUF_MFree((void *)TxPkt);
        printf("send eapol fail\n");
        return -1;
	}

	return len;
}


int wpa_drv_set_operstate(struct wpa_supplicant *wpa_s,enum wpa_states state)
{
    extern void Supplicant_Notify_MLME_4way(u8 is_sucessful);
	if(state==WPA_COMPLETED)
	{
//        printf("\33[35mwpa_drv_set_operstate():WPA_COMPLETED \33[0m\r\n");
        Supplicant_Notify_MLME_4way(1);
        SetStaAction(STA_ACTION_READY);
	}else if((state==WPA_4WAY_HANDSHAKE)||(state==WPA_GROUP_HANDSHAKE)){
//	    printf("\33[35mwpa_drv_set_operstate():WPA_GROUP_HANDSHAKE \33[0m\r\n");
	    SetStaAction(STA_ACTION_JOING_4WAY);
	}
    #if 0
    else if (state == WPA_DISCONNECTED || state == WPA_ASSOCIATING ||
		   state == WPA_ASSOCIATED){
        Supplicant_Notify_MLME_4way(0);
	}
    #endif

	return 0;
}

#endif /* DRIVER_CABRIO_I_H */
