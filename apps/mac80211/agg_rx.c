#include <lib/ssv_lib.h>
#include <config.h>
#include <msgevt.h>
#include <mbox/drv_mbox.h>
#include <ssv_pktdef.h>
#include <pbuf.h>
#include <ssv_pktdef.h>


#include "agg_rx.h"
#include "soft-main.h"


#ifdef _AMPDU_SOC_
//--------------------------------------------------------------------------------------
//	Global data
//--------------------------------------------------------------------------------------

static u8 g_ba_rx_session_cnt;
struct rx_ba_session_desc g_ba_rx_session_desc[RX_AGG_RX_BA_MAX_SESSIONS];

#define HT_RX_REORDER_BUF_TIMEOUT 5//(HZ / 10)


//--------------------------------------------------------------------------------------
//	For RX AGG General
//--------------------------------------------------------------------------------------
void agg_rx_ba_session_desc_init(void);
struct rx_ba_session_desc *agg_rx_get_rx_ba_session_desc(void);
void agg_rx_release_rx_ba_session_desc(struct rx_ba_session_desc *ba_session_desc);





void agg_rx_reorder_timer( void *args )
{

	struct rx_ba_session_desc *rx_ba_session_desc = OS_TimerGetData((OsTimer)args);
    MsgEvent *pMsgEv = msg_evt_alloc();

	pMsgEv->MsgType 	= MEVT_SOFT_TIMER;
	pMsgEv->MsgData 	= SOFT_MAC_TIMER_REORDER_BUFFER;
	pMsgEv->MsgData1	= (u32)rx_ba_session_desc;
	pMsgEv->MsgData2	= 0;
	pMsgEv->MsgData3	= 0;
	msg_evt_post(MBOX_SOFT_MAC, pMsgEv);

}






void agg_rx_ba_session_desc_init(void)
{
	g_ba_rx_session_cnt = 0;
	memset(&g_ba_rx_session_desc, 0, sizeof(struct rx_ba_session_desc)*RX_AGG_RX_BA_MAX_SESSIONS);

	OS_TimerCreate( &g_ba_rx_session_desc[0].reorder_timer, HT_RX_REORDER_BUF_TIMEOUT, FALSE, (void*)&g_ba_rx_session_desc[0], agg_rx_reorder_timer );

}


struct rx_ba_session_desc *agg_rx_get_rx_ba_session_desc(void)
{
	int i;
	//if(g_ba_rx_session_desc)
	for(i=0;i<RX_AGG_RX_BA_MAX_SESSIONS;i++)
	{
		if(g_ba_rx_session_desc[i].vaild == FALSE)
			return &g_ba_rx_session_desc[i];
	}

	return NULL;
}


void agg_rx_release_rx_ba_session_desc(struct rx_ba_session_desc *ba_session_desc)
{
	ba_session_desc->vaild = FALSE;
}



//--------------------------------------------------------------------------------------
//	For RX BA session
//--------------------------------------------------------------------------------------







//--------------------------------------------------------------------------------------
//	For RX reorder buffer
//--------------------------------------------------------------------------------------
#define SEQ_MODULO 0x1000
#define SEQ_MASK   0xfff

static inline int seq_less(u16 sq1, u16 sq2)
{
	return ((sq1 - sq2) & SEQ_MASK) > (SEQ_MODULO >> 1);//2048
}

static inline u16 seq_inc(u16 sq)
{
	return (sq + 1) & SEQ_MASK;
}

static inline u16 seq_sub(u16 sq1, u16 sq2)
{
	return (sq1 - sq2) & SEQ_MASK;
}



static void ieee80211_release_reorder_frame(struct rx_ba_session_desc *tid_agg_rx, int index)
{
	//struct ieee80211_local *local = hw_to_local(hw);
	//struct sk_buff *skb = tid_agg_rx->reorder_buf[index];
	//struct ieee80211_rx_status *status;
	u32 *pktid = tid_agg_rx->reorder_buf[index];


	//lockdep_assert_held(&tid_agg_rx->reorder_lock);

	if (!pktid)
		goto no_frame;

	/* release the frame from the reorder ring buffer */
	tid_agg_rx->stored_mpdu_num--;
	tid_agg_rx->reorder_buf[index] = NULL;


	ENG_MBOX_NEXT((u32)pktid);

no_frame:
	tid_agg_rx->head_seq_num = seq_inc(tid_agg_rx->head_seq_num);
}


static void ieee80211_release_reorder_frames(struct rx_ba_session_desc *tid_agg_rx,
	u16 head_seq_num)
{
	int index;

	//lockdep_assert_held(&tid_agg_rx->reorder_lock);

	while (seq_less(tid_agg_rx->head_seq_num, head_seq_num)) {
		index = seq_sub(tid_agg_rx->head_seq_num, tid_agg_rx->ssn) %
			tid_agg_rx->buf_size;
		ieee80211_release_reorder_frame(tid_agg_rx, index);
	}
}





/*
 * Timeout (in jiffies) for skb's that are waiting in the RX reorder buffer. If
 * the skb was added to the buffer longer than this time ago, the earlier
 * frames that have not yet been received are assumed to be lost and the skb
 * can be released for processing. This may also release other skb's from the
 * reorder buffer if there are no additional gaps between the frames.
 *
 * Callers must hold tid_agg_rx->reorder_lock.
 */


static void ieee80211_sta_reorder_release( struct rx_ba_session_desc *tid_agg_rx)
{
	int index, j;

	//lockdep_assert_held(&tid_agg_rx->reorder_lock);

	/* release the buffer until next missing frame */
	index = seq_sub(tid_agg_rx->head_seq_num, tid_agg_rx->ssn) %
						tid_agg_rx->buf_size;

//---------------------------------------------------------------------------------------------------------------
	if (!tid_agg_rx->reorder_buf[index] &&
	    tid_agg_rx->stored_mpdu_num) {
		/*
		 * No buffers ready to be released, but check whether any
		 * frames in the reorder buffer have timed out.
		 */
		int skipped = 1;
		for (j = (index + 1) % tid_agg_rx->buf_size; j != index;
		     j = (j + 1) % tid_agg_rx->buf_size) {
			if (!tid_agg_rx->reorder_buf[j]) {
				skipped++;
				continue;
			}

			if (skipped &&
			    !time_after(xTaskGetTickCount(), tid_agg_rx->reorder_time[j] + HT_RX_REORDER_BUF_TIMEOUT))
				goto set_release_timer;


			ieee80211_release_reorder_frame(tid_agg_rx, j);

			/*
			 * Increment the head seq# also for the skipped slots.
			 */
			tid_agg_rx->head_seq_num =
				(tid_agg_rx->head_seq_num + skipped) & SEQ_MASK;
			skipped = 0;
		}
	} else while (tid_agg_rx->reorder_buf[index]) {
		//Release frame until next missing frame
		ieee80211_release_reorder_frame(tid_agg_rx, index);
		index =	seq_sub(tid_agg_rx->head_seq_num, tid_agg_rx->ssn) %
							tid_agg_rx->buf_size;
	}

//---------------------------------------------------------------------------------------------------------------
	if (tid_agg_rx->stored_mpdu_num) {
		j = index = seq_sub(tid_agg_rx->head_seq_num,
				    tid_agg_rx->ssn) % tid_agg_rx->buf_size;

		for (; j != (index - 1) % tid_agg_rx->buf_size;
		     j = (j + 1) % tid_agg_rx->buf_size) {
			if (tid_agg_rx->reorder_buf[j])
				break;
		}

set_release_timer:
		index = 0;

		OS_TimerStart(tid_agg_rx->reorder_timer);
//
//		mod_timer(&tid_agg_rx->reorder_timer,//??????????????????????????????????????????????????????????????????????????????????????????????????????????
//			  tid_agg_rx->reorder_time[j] + 1 +
//			  HT_RX_REORDER_BUF_TIMEOUT);
	}
	else {
		OS_TimerStop(tid_agg_rx->reorder_timer);
//		del_timer(&tid_agg_rx->reorder_timer);//??????????????????????????????????????????????????????????????????????????????????????????????????????????
	}
}





void agg_rx_release_reorder_timeout(u32 data)
{
	struct rx_ba_session_desc *tid_agg_rx = (struct rx_ba_session_desc *)data;
	//struct ieee80211_rx_data rx = {
	//	.sta = sta,
	//	.sdata = sta->sdata,
	//	.local = sta->local,
	//	/* This is OK -- must be QoS data frame */
	//	.security_idx = tid,
	//	.seqno_idx = tid,
	//	.flags = 0,
	//};
	//struct tid_ampdu_rx *tid_agg_rx;

	//tid_agg_rx = rcu_dereference(sta->ampdu_mlme.tid_rx[tid]);
	//if (!tid_agg_rx)
	//	return;

	//spin_lock(&tid_agg_rx->reorder_lock);
	ieee80211_sta_reorder_release( tid_agg_rx);
	//spin_unlock(&tid_agg_rx->reorder_lock);


}







/*
 * As this function belongs to the RX path it must be under
 * rcu_read_lock protection. It returns false if the frame
 * can be processed immediately, true if it was consumed.
 */
static bool ieee80211_sta_manage_reorder_buf(struct rx_ba_session_desc *tid_agg_rx,
					     void *pkt_id)
{
// 	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *) skb->data;
// 	u16 sc = //le16_to_cpu(hdr->seq_ctrl);
 	u16 mpdu_seq_num = GET_HDR80211_SC_SEQNUM((PKT_TxInfo *)pkt_id);//(sc & IEEE80211_SCTL_SEQ) >> 4;
 	u16 head_seq_num, buf_size;
 	int index;
 	bool ret = true;
//
// 	spin_lock(&tid_agg_rx->reorder_lock);
//
 	buf_size = tid_agg_rx->buf_size;
 	head_seq_num = tid_agg_rx->head_seq_num;
//--------------------------------------------------------------------------------------------------------------------------------
//c
//	Wstart+2048  <= 	SN		<	  Wsart

 	/* frame with out of date sequence number */
 	if (seq_less(mpdu_seq_num, head_seq_num)) {
		//dev_kfree_skb(skb);
		PBUF_MFree(pkt_id);
 		goto out;
	}

//--------------------------------------------------------------------------------------------------------------------------------
//b
//	Wend  < 	   SN	   <	 Wsart+2048
//head_seq_num + buf_size--->Wend+1


	/*
	 * If frame the sequence number exceeds our buffering window
	 * size release some previous frames to make room for this one.
	 */
	if (!seq_less(mpdu_seq_num, head_seq_num + buf_size)) {						//mpdu_seq_num >= Wend+1
		head_seq_num = seq_inc(seq_sub(mpdu_seq_num, buf_size));				//b2
		/* release stored frames up to new head to stack */
		ieee80211_release_reorder_frames(tid_agg_rx, head_seq_num);			//b4
	}

//--------------------------------------------------------------------------------------------------------------------------------
//a
//	Wstart	<=	   SN	   <=	  Wend
//

	/* Now the new frame is always in the range of the reordering buffer */
	index = seq_sub(mpdu_seq_num, tid_agg_rx->ssn) % tid_agg_rx->buf_size;

	/* check if we already stored this frame */
	if (tid_agg_rx->reorder_buf[index]) {
		//dev_kfree_skb(skb);
		PBUF_MFree(pkt_id);
		goto out;
	}

	/*
	 * If the current MPDU is in the right order and nothing else
	 * is stored we can process it directly, no need to buffer it.
	 * If it is first but there's something stored, we may be able
	 * to release frames after this one.
	 */
	if (mpdu_seq_num == tid_agg_rx->head_seq_num &&
	    tid_agg_rx->stored_mpdu_num == 0) {
		tid_agg_rx->head_seq_num = seq_inc(tid_agg_rx->head_seq_num);
		ret = false;
		goto out;
	}
//
// 	/* put the frame in the reordering buffer */
	tid_agg_rx->reorder_buf[index] = pkt_id;
	tid_agg_rx->reorder_time[index] = xTaskGetTickCount();
	tid_agg_rx->stored_mpdu_num++;
	ieee80211_sta_reorder_release(tid_agg_rx);

 out:
//	spin_unlock(&tid_agg_rx->reorder_lock);
	return ret;
}


//
// /*
//  * Reorder MPDUs from A-MPDUs, keeping them on a buffer. Returns
//  * true if the MPDU was buffered, false if it should be processed.
//  */
//  static void ieee80211_rx_reorder_ampdu(PKT_RxInfo *pkt_id)
// {
// // 	struct sk_buff *skb = rx->skb;
// // 	struct ieee80211_local *local = rx->local;
// // 	struct ieee80211_hw *hw = &local->hw;
// // 	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *) skb->data;
// // 	struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(skb);
// // 	struct sta_info *sta = rx->sta;
//  	struct rx_ba_session_desc *tid_agg_rx;
//  	u16 sc;
//  	u8 tid, ack_policy;
// //
// // 	if (!ieee80211_is_data_qos(hdr->frame_control))
// // 		goto dont_reorder;
//
// 	if(!pkt_id->qos)
// 		goto dont_reorder;
//
// //
// // 	/*
// // 	 * filter the QoS data rx stream according to
// // 	 * STA/TID and check if this STA/TID is on aggregation
// // 	 */
// //
// 	if (!sta)
// 		goto dont_reorder;
//
//
// 	tid = GET_HDR80211_QC(pkt_id) & IEEE80211_QOS_CTL_TID_MASK;
//
// 	tid_agg_rx = rcu_dereference(sta->ampdu_mlme.tid_rx[tid]);
// 	if (!tid_agg_rx)
// 		goto dont_reorder;
//
// 	ack_policy = GET_HDR80211_QC(pkt_id) &
// 		IEEE80211_QOS_CTL_ACK_POLICY_MASK;
//
//
// 	/* qos null data frames are excluded */
// 	if ((hdr->frame_control & cpu_to_le16(IEEE80211_STYPE_NULLFUNC)))
// 		goto dont_reorder;
//
// 	/* not part of a BA session */
// 	if (ack_policy != IEEE80211_QOS_CTL_ACK_POLICY_BLOCKACK &&
// 	    ack_policy != IEEE80211_QOS_CTL_ACK_POLICY_NORMAL)
// 		goto dont_reorder;
// //
// // 	/* not actually part of this BA session */
// // 	if (!(status->rx_flags & IEEE80211_RX_RA_MATCH))
// // 		goto dont_reorder;
// //
// // 	/* new, potentially un-ordered, ampdu frame - process it */
// //
// // 	/* reset session timer */
// // 	if (tid_agg_rx->timeout)
// // 		mod_timer(&tid_agg_rx->session_timer,
// // 			  TU_TO_EXP_TIME(tid_agg_rx->timeout));
// //
// // 	/* if this mpdu is fragmented - terminate rx aggregation session */
// // 	sc = le16_to_cpu(hdr->seq_ctrl);
// // 	if (sc & IEEE80211_SCTL_FRAG) {
// // 		skb->pkt_type = IEEE80211_SDATA_QUEUE_TYPE_FRAME;
// // 		skb_queue_tail(&rx->sdata->skb_queue, skb);
// // 		ieee80211_queue_work(&local->hw, &rx->sdata->work);
// // 		return;
// // 	}
// //
// // 	/*
// // 	 * No locking needed -- we will only ever process one
// // 	 * RX packet at a time, and thus own tid_agg_rx. All
// // 	 * other code manipulating it needs to (and does) make
// // 	 * sure that we cannot get to it any more before doing
// // 	 * anything with it.
// // 	 */
// // 	if (ieee80211_sta_manage_reorder_buf(hw, tid_agg_rx, skb))
// // 		return;
// //
//   dont_reorder:
//  	//skb_queue_tail(&local->rx_skb_queue, skb);
// 	ENG_MBOX_NEXT((u32)pkt_id);
//
//  }
#endif//#ifdef _AMPDU_SOC_
