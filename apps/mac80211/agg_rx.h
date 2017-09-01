#ifndef _AGG_RX_H_
#define _AGG_RX_H_




#include <types.h>
#include <rtos.h>



#if defined(WIN32)

#define typecheck

#else
/*
 * Check at compile time that something is of a particular type.
 * Always evaluates to 1 so you may use it easily in comparisons.
 */
#define typecheck(type,x) \
({	type __dummy; \
	typeof(x) __dummy2; \
	(void)(&__dummy == &__dummy2); \
	1; \
})
#endif



#if defined(WIN32)
#define time_after(a,b)		\
	((long)(b) - (long)(a) < 0)
#define time_before(a,b)	time_after(b,a)

#define time_after_eq(a,b)	\
	((long)(a) - (long)(b) >= 0)

#else
#define time_after(a,b)		\
	(typecheck(unsigned long, a) && \
	typecheck(unsigned long, b) && \
	((long)(b) - (long)(a) < 0))
#define time_before(a,b)	time_after(b,a)

#define time_after_eq(a,b)	\
	(typecheck(unsigned long, a) && \
	typecheck(unsigned long, b) && \
	((long)(a) - (long)(b) >= 0))

#endif




/* ACK policy */
#define IEEE80211_QOS_CTL_ACK_POLICY_NORMAL	0x0000
#define IEEE80211_QOS_CTL_ACK_POLICY_NOACK	0x0020
#define IEEE80211_QOS_CTL_ACK_POLICY_NO_EXPL	0x0040
#define IEEE80211_QOS_CTL_ACK_POLICY_BLOCKACK	0x0060
#define IEEE80211_QOS_CTL_ACK_POLICY_MASK	0x0060



/**
 * struct tid_ampdu_rx - TID aggregation information (Rx).
 *
 * @reorder_buf: buffer to reorder incoming aggregated MPDUs
 * @reorder_time: jiffies when skb was added
 * @session_timer: check if peer keeps Tx-ing on the TID (by timeout value)
 * @reorder_timer: releases expired frames from the reorder buffer.
 * @head_seq_num: head sequence number in reordering buffer.
 * @stored_mpdu_num: number of MPDUs in reordering buffer
 * @ssn: Starting Sequence Number expected to be aggregated.
 * @buf_size: buffer size for incoming A-MPDUs
 * @timeout: reset timer value (in TUs).
 * @dialog_token: dialog token for aggregation session
 * @rcu_head: RCU head used for freeing this struct
 * @reorder_lock: serializes access to reorder buffer, see below.
 *
 * This structure's lifetime is managed by RCU, assignments to
 * the array holding it must hold the aggregation mutex.
 *
 * The @reorder_lock is used to protect the members of this
 * struct, except for @timeout, @buf_size and @dialog_token,
 * which are constant across the lifetime of the struct (the
 * dialog token being used only for debugging).
 */
struct rx_ba_session_desc {
	u32				vaild:1;
	u32             wsid:4;	
	u32				tid:4;
	u32             RSVD:23;

	OsMutex reorder_lock;

	//struct rcu_head rcu_head;
	//spinlock_t reorder_lock;		---------------
	//struct sk_buff **reorder_buf;	---------------
	//unsigned long *reorder_time;	---------------
	void *reorder_buf[RX_AGG_RX_BA_MAX_BUF_SIZE];
	unsigned long reorder_time[RX_AGG_RX_BA_MAX_BUF_SIZE];

	//struct timer_list session_timer;
	//struct timer_list reorder_timer;
	OsTimer   reorder_timer;
	

	u16 head_seq_num;
	u16 stored_mpdu_num;
	u16 ssn;
	u16 buf_size;
	u16 timeout;
	u8 dialog_token;
};


void agg_rx_ampdu_tid_desc_init(void);
struct rx_ba_session_desc *agg_rx_get_ampdu_tid_desc(void);










#endif// _AGG_RX_H_


