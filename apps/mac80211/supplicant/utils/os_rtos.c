/*
 * wpa_supplicant/hostapd / Empty OS specific functions
 * Copyright (c) 2005-2006, Jouni Malinen <j@w1.fi>
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
 * This file can be used as a starting point when adding a new OS target. The
 * functions here do not really work as-is since they are just empty or only
 * return an error value. os_internal.c can be used as another starting point
 * or reference since it has example implementation of many of these functions.
 */

#include "../supplicant_config.h"
#include "os.h"
#include <log.h>
#include <pbuf.h>
#include <random/drv_random.h>

#ifdef OS_NO_C_LIB_DEFINES

/**
 * os_malloc - Allocate dynamic memory
 * @size: Size of the buffer to allocate
 * Returns: Allocated buffer or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer with os_free().
 */
void * os_malloc(size_t size)
{
	return (void*)PBUF_MAlloc_Raw((u16)size,0, SOC_PBUF);
}

/**
 * os_free - Free dynamic memory
 * @ptr: Old buffer from os_malloc() or os_realloc(); can be %NULL
 */
void os_free(void *ptr)
{
	if(ptr)
	{
		PBUF_MFree(ptr);
	}
}
#endif

//Word access
int os_get_random(unsigned char *buf, int len)
{
	return drv_get_random(buf,len);
}

