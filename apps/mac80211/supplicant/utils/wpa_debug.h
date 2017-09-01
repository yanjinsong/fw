/*
 * wpa_supplicant/hostapd / Debug prints
 * Copyright (c) 2002-2007, Jouni Malinen <j@w1.fi>
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

#ifndef WPA_DEBUG_H
#define WPA_DEBUG_H

#include <log.h>

#define     MSG_EXCESSIVE       LOG_LEVEL_ON
#define     MSG_MSGDUMP         LOG_LEVEL_TRACE
#define     MSG_DEBUG           LOG_LEVEL_DEBUG
#define     MSG_INFO            LOG_LEVEL_INFO
#define     MSG_WARNING         LOG_LEVEL_WARN
#define     MSG_ERROR           LOG_LEVEL_ERROR


#define wpa_debug_print_timestamp() do { } while (0)

#ifdef CONFIG_NO_WPA_MSG
#define wpa_printf(level, args, ...)       do { } while (0)
#else
#define wpa_printf(level, args, ...)       LOG_PRINTF_LM(level, LOG_MODULE_WPA, args, ##__VA_ARGS__)
#endif

/**
 * wpa_hexdump - conditional hex dump
 * @level: priority level (MSG_*) of the message
 * @title: title of for the message
 * @buf: data buffer to be dumped
 * @len: length of the buf
 *
 * This function is used to print conditional debugging and error messages. The
 * output may be directed to stdout, stderr, and/or syslog based on
 * configuration. The contents of buf is printed out has hex dump.
 */
#ifdef CONFIG_NO_WPA_HEXDUMP
#define wpa_hexdump(l,t,b,le) do { } while (0)
#define wpa_hexdump_buf(l,t,b) do { } while (0)
#define wpa_hexdump_key(l,t,b,le) do { } while (0)
#define wpa_hexdump_buf_key(l,t,b) do { } while (0)
#define wpa_hexdump_ascii(l,t,b,le) do { } while (0)
#define wpa_hexdump_ascii_key(l,t,b,le) do { } while (0)
#else // CONFIG_NO_WPA_HEXDUMP
void wpa_hexdump(int level, const char *title, const u8 *buf, size_t len);

/**
 * wpa_hexdump_key - conditional hex dump, hide keys
 * @level: priority level (MSG_*) of the message
 * @title: title of for the message
 * @buf: data buffer to be dumped
 * @len: length of the buf
 *
 * This function is used to print conditional debugging and error messages. The
 * output may be directed to stdout, stderr, and/or syslog based on
 * configuration. The contents of buf is printed out has hex dump. This works
 * like wpa_hexdump(), but by default, does not include secret keys (passwords,
 * etc.) in debug output.
 */
void wpa_hexdump_key(int level, const char *title, const u8 *buf, size_t len);

/**
 * wpa_hexdump_ascii - conditional hex dump
 * @level: priority level (MSG_*) of the message
 * @title: title of for the message
 * @buf: data buffer to be dumped
 * @len: length of the buf
 *
 * This function is used to print conditional debugging and error messages. The
 * output may be directed to stdout, stderr, and/or syslog based on
 * configuration. The contents of buf is printed out has hex dump with both
 * the hex numbers and ASCII characters (for printable range) are shown. 16
 * bytes per line will be shown.
 */
void wpa_hexdump_ascii(int level, const char *title, const u8 *buf,
		       size_t len);

/**
 * wpa_hexdump_ascii_key - conditional hex dump, hide keys
 * @level: priority level (MSG_*) of the message
 * @title: title of for the message
 * @buf: data buffer to be dumped
 * @len: length of the buf
 *
 * This function is used to print conditional debugging and error messages. The
 * output may be directed to stdout, stderr, and/or syslog based on
 * configuration. The contents of buf is printed out has hex dump with both
 * the hex numbers and ASCII characters (for printable range) are shown. 16
 * bytes per line will be shown. This works like wpa_hexdump_ascii(), but by
 * default, does not include secret keys (passwords, etc.) in debug output.
 */
void wpa_hexdump_ascii_key(int level, const char *title, const u8 *buf,
			   size_t len);
#endif // CONFIG_NO_WPA_HEXDUMP

/*
 * wpa_dbg() behaves like wpa_msg(), but it can be removed from build to reduce
 * binary size. As such, it should be used with debugging messages that are not
 * needed in the control interface while wpa_msg() has to be used for anything
 * that needs to shown to control interface monitors.
 */
#define wpa_dbg wpa_msg


#define wpa_debug_open_file(p) do { } while (0)
#define wpa_debug_close_file() do { } while (0)
static inline int wpa_debug_reopen_file(void)
{
	return 0;
}

#ifdef CONFIG_NO_WPA_MSG
#define wpa_msg(ctx, level, fmt, ...) do { } while (0)
#else
#define wpa_msg(ctx, level, fmt, ...) LOG_PRINTF_LM(level, LOG_MODULE_WPA, fmt "\n", ##__VA_ARGS__)
#endif


#ifdef EAPOL_TEST
#if 0
#define WPA_ASSERT(a)						       \
	do {							       \
		if (!(a)) {					       \
			wpa_printf(MSG_ERROR, "WPA_ASSERT FAILED '" #a "' "	       \
			       "%s %s:%d\n",			       \
			       __FUNCTION__, __FILE__, __LINE__);      \
			while (1) ;				       \
		}						       \
	} while (0)
#endif
#define WPA_ASSERT(a) ASSERT_RET(a, (-1))
#else
#define WPA_ASSERT(a) do { } while (0)
#endif // EAPOL_TEST

#endif /* WPA_DEBUG_H */
