#ifndef _DEFRAG_H
#define _DEFRAG_H

#include <config.h>
#include <hdr80211.h>
#include <common.h>
#include <ssv_pktdef.h>
#include <pbuf.h>
#include <driver/mbox/drv_mbox.h>
#include <driver/dma/drv_pkt_dma.h>
#include <log.h>

	
#define _DEFRAG_ENABLE_TIMER		0		// 0 or 1 are both availabe

/* =================== debug options ===================*/
#define _DEFRAG_ENABLE_DUMP		0

/* ==================== macros         =====================*/
#define MAX_DEFRAG_SEQ				WLAN_MAX_STA
#define	MAX_DEFRAG_FRAG				16
#define MAX_DEFRAG_TIMEOUT			500			// ms


/* ==================== Debug Functions =================== */
#define DEFRAG_TRACE(fmt, ...)		LOG_TRACE_M(LOG_MODULE_DEFRAG, fmt, ##__VA_ARGS__)
#define DEFRAG_DEBUG(fmt, ...)   	LOG_DEBUG_M(LOG_MODULE_DEFRAG, fmt, ##__VA_ARGS__)
#define DEFRAG_INFO(fmt, ...)   	LOG_INFO_M (LOG_MODULE_DEFRAG, fmt, ##__VA_ARGS__)
#define DEFRAG_WARN(fmt, ...)   	LOG_WARN_M (LOG_MODULE_DEFRAG, fmt, ##__VA_ARGS__)
#define DEFRAG_FAIL(fmt, ...)   	LOG_FAIL_M (LOG_MODULE_DEFRAG, fmt, ##__VA_ARGS__)
#define DEFRAG_ERROR(fmt, ...)   	LOG_ERROR_M(LOG_MODULE_DEFRAG, fmt, ##__VA_ARGS__)
#define DEFRAG_FATAL(fmt, ...)   	LOG_FATAL_M(LOG_MODULE_DEFRAG, fmt, ##__VA_ARGS__)

#pragma pack(1)
typedef struct DEFRAG_KEY_st
{
	u8		is_qos;			// 'qos' field in 'RXInfo'. must be 0 or 1
	u16		sc;				// sequence control field 	from MAC hdr
	u16		qc;				// QoS control field 		from MAC hdr. '0' if 'is_qos = 0'
	u8		sa[6];			// from 'Address X' field 	in 	 MAC hdr
	u8		bssid[6];		// from 'Address X' field 	in   MAC hdr
} DEFRAG_KEY;
#pragma pack()


#define		DEFRAG_KEY_FMT				" {qos(%d),sc(0x%04x),qc(0x%04x),sa(%02X:%02X:%02X:%02X:%02X:%02X),bssid(%02X:%02X:%02X:%02X:%02X:%02X)} "
#define		DEFRAG_KEY_VA_LIST(k)		(k).is_qos, (k).sc, (k).qc, (k).sa[0], (k).sa[1], (k).sa[2], (k).sa[3], (k).sa[4], (k).sa[5], (k).bssid[0], (k).bssid[1], (k).bssid[2], (k).bssid[3], (k).bssid[4], (k).bssid[5]
#define 	DEFRAG_KEY_IS_EQUAL(a, b)	(((a).is_qos == (b).is_qos) && ((a).sc == (b).sc) && ((a).qc == (b).qc) && (__memcmp((a).sa, (b).sa, 6) == 0) && (__memcmp((a).bssid, (b).bssid, 6)) == 0)

/* example of defrag seq_array in some moment:
   (idx)  (key)		(frag_cnt)		(frag_array)
	[0] : aaa...		6			[0][1][2][3][4][][]		(maybe waiting to recieve [5] & [6])
	[1] : bbb...		3			[0][1][2]
	[2] : empty		
	[3] : xxx...		4			[0][1][][3]				(maybe [2] is lost, this seq should be flush later)

note : 
	- the "frag_array idx = frag_num". 
	  (eg: frag_num 2 occupy [2] & frag_num 5 occpy [5] ...,etc)
*/
typedef struct DEFRAG_SEQ_st
{
	DEFRAG_KEY	key;
	u32			frag_cnt;
	u8*			frag_array[MAX_DEFRAG_FRAG];
#if (_DEFRAG_ENABLE_TIMER)
	OsTimer		timer;
#else
	u32			tick_last_arrival;	// the arrival time(tick) of the last frag pkt, will update for each frag pkt.
#endif
	bool		bUsed;				// false : this array entry is in 'empty' status.
} DEFRAG_SEQ, *PDEFRAG_SEQ;

	
extern void Soft_DeFragHandler(PKT_Info *p, s32 reason);
extern s32	defrag_init(void);


#endif /* _DEFRAG_H */
