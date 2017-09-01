/*
 * Event loop - empty template (basic structure, but no OS specific operations)
 * Copyright (c) 2002-2005, Jouni Malinen <j@w1.fi>
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

#include <config.h>
#include "../supplicant_config.h"
//#include "../utils/common.h"
#include "eloop.h"
#include <rtos.h>


struct eloop_timeout {
	OsTimer timer;
	void *eloop_data;
	void *user_data;
	void (*handler)(void *eloop_ctx, void *sock_ctx);
	u8 vaild;
};

#define	MAX_ELOOP	WLAN_MAX_STA
struct eloop_timeout timeout[MAX_ELOOP];
static u8 eloop_count = 0;

int eloop_init(void)
{
    memset((void *)timeout,0,sizeof(timeout));
    return 0;
}
static void eloop_run(OsTimer timer)
{
	u8 id;

	id=*((u8*)OS_TimerGetData(timer));

	if(id < eloop_count)
		timeout[id].handler(timeout[id].eloop_data,timeout[id].user_data);
}

int eloop_register_timeout(unsigned int secs, unsigned int usecs,
			   void (*handler)(void *eloop_ctx, void *timeout_ctx),
			   void *eloop_data, void *user_data)
{
	u8 x,free_st=0;
	if(eloop_count == MAX_ELOOP)
		return -1;

	for(x=0;x<MAX_ELOOP;x++)
	{
		if(timeout[x].vaild == 0)
        {
			free_st = x;
            break;
        }
	}

	timeout[free_st].eloop_data = eloop_data;
	timeout[free_st].user_data = user_data;
	timeout[free_st].handler = handler;
    if(timeout[free_st].timer ==0){
        printf("Create the timer for eloop_run. (%x)\n",free_st);
    if( OS_TimerCreate(&(timeout[free_st].timer), secs*1000, (u8)FALSE, (void *)&free_st, eloop_run) == OS_FAILED)
        return -1;
    timeout[free_st].vaild = 1;
        printf("Start the timer for eloop_run. (%x)\n",free_st);
        OS_TimerStart(timeout[free_st].timer);
    }else{
        printf("Re-start the timer for eloop_run. (%x)\n",free_st);
        OS_TimerSet(timeout[free_st].timer,secs*1000, (u8)FALSE,NULL);
        OS_TimerStart(timeout[free_st].timer);
    }
	eloop_count++;
	return 0;
}

int eloop_cancel_timeout(void (*handler)(void *eloop_ctx, void *sock_ctx),
			 void *eloop_data, void *user_data)
{
	u8 removed=0,x;

	if(eloop_count == 0)
		return removed;
	
	for(x=0;x<MAX_ELOOP;x++)
	{
		if(timeout[x].vaild == 1)
		{
			if (timeout[x].handler == handler && (timeout[x].eloop_data == eloop_data || eloop_data == ELOOP_ALL_CTX) && (timeout[x].user_data == user_data || user_data == ELOOP_ALL_CTX))
			{
				removed++;
				eloop_count--;
				timeout[x].vaild = 0;
                printf("Stop the timer for eloop_run (%x)\n",x);
				OS_TimerStop( timeout[x].timer);
				//xTimerDelete(timeout[x].timer,0);
                break;
			}
		}
	}

	return removed;
}

