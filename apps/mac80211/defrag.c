#include "defrag.h"
#include <soc_global.h>
#include <regs.h>

#define		STATUS_SEQ_OCCUPIED_SLOT		1
#define		STATUS_SEQ_EMPTY_SLOT			2

/* ==================== Static Variable ======================= */
u32				seq_cnt = 0;
DEFRAG_SEQ		seq_array[MAX_DEFRAG_SEQ];
u8				*rx_pkt;			// Rx output packet after reassemble

#if (_DEFRAG_ENABLE_TIMER)
static void		seq_timer_handler(void *args);
static void		seq_timer_init(DEFRAG_SEQ *pSeq);
static void		seq_timer_start(DEFRAG_SEQ *pSeq);
static void		seq_timer_stop(DEFRAG_SEQ *pSeq);
static bool		seq_free_by_key(const DEFRAG_KEY *key);
#endif

static bool		    DEFRAG_KEY_IS_SAME_SEQ(const DEFRAG_KEY *a, const DEFRAG_KEY *b);
static bool		    get_key(PKT_Info *p, DEFRAG_KEY *key);
static DEFRAG_SEQ*	get_seq(const DEFRAG_KEY *key, u8 *status);
static DEFRAG_SEQ*  seq_rls_timeout(void);
//static bool		seq_is_new_seq(const DEFRAG_KEY *key);
static void         seq_free(DEFRAG_SEQ *pSeq);
static void	        seq_add_new(DEFRAG_SEQ *pSeq, const DEFRAG_KEY *key);
static void	        frag_array_free(DEFRAG_SEQ *pSeq);
static u32	        frag_cal_total_payload_len(DEFRAG_SEQ *pSeq);
static bool	        frag_add(DEFRAG_SEQ *pSeq, u8 *p);
static bool	        frag_rollcall(DEFRAG_SEQ *pSeq); 
static bool	        defrag_reassemble(DEFRAG_SEQ *pSeq);

#if (_DEFRAG_ENABLE_DUMP)
static void		dump_seq_array(void);
static void		dump_frag_array(DEFRAG_SEQ *pSeq);

void dump_seq_array(void)
{
	u16		i;

	DEFRAG_INFO("<< seq_array >>\n\r");
	for (i = 0; i < MAX_DEFRAG_SEQ; i++)
	{
		DEFRAG_INFO("[%2d] : 0x%08x\n\r", i, &(seq_array[i]));
		dump_frag_array(&(seq_array[i]));
	}
}

void dump_frag_array(DEFRAG_SEQ *pSeq)
{
	u16		i;
	u16		fc;

	DEFRAG_INFO("<< frag_array >> : frag_cnt = %d\n\r", pSeq->frag_cnt);
	for (i = 0; i < MAX_DEFRAG_FRAG; i++)
	{
		if (pSeq->frag_array[i] != NULL)
		{
			fc = GET_HDR80211_FC((PKT_RxInfo *)(pSeq->frag_array[i]));
			DEFRAG_INFO("[%2d] : 0x%08x, more_frag = %d\n\r", i, pSeq->frag_array[i], IS_MOREFLAG_SET(fc));
		}
		else
		{
			DEFRAG_INFO("[%2d] : 0x%08x\n\r", i, pSeq->frag_array[i]);
		}
	}
}
#endif // _DEFRAG_ENABLE_DUMP

void dump_PKT_TxInfo(PKT_TxInfo *p)	
{
#if (_DEFRAG_ENABLE_DUMP)
	u32		payload_len;
	u32		i;
	u8		*dat;
	u8		*a;

	DEFRAG_INFO("========= TxInfo =========\n\r");
	DEFRAG_INFO("%20s : %d\n\r", "len", 		p->len);
	DEFRAG_INFO("%20s : %d\n\r", "c_type",		p->c_type);
	DEFRAG_INFO("%20s : %d\n\r", "f80211",		p->f80211);
	DEFRAG_INFO("%20s : %d\n\r", "qos",			p->qos);
	DEFRAG_INFO("%20s : %d\n\r", "ht",			p->ht);
	DEFRAG_INFO("%20s : %d\n\r", "use_4addr",	p->use_4addr);
	DEFRAG_INFO("%20s : %d\n\r", "RSVD_0",		p->RSVD_0);
	DEFRAG_INFO("%20s : %d\n\r", "bc_que",		p->bc_que);	
	DEFRAG_INFO("%20s : %d\n\r", "security",	p->security);
	DEFRAG_INFO("%20s : %d\n\r", "more_data",	p->more_data);
	DEFRAG_INFO("%20s : %d\n\r", "stype_b5b4",	p->stype_b5b4);
	DEFRAG_INFO("%20s : %d\n\r", "extra_info",	p->extra_info);
	DEFRAG_INFO("%20s : 0x%08x\n\r", "fCmd",	p->fCmd);
	DEFRAG_INFO("%20s : %d\n\r", "hdr_offset",	p->hdr_offset);
	DEFRAG_INFO("%20s : %d\n\r", "frag",		p->frag);
	DEFRAG_INFO("%20s : %d\n\r", "unicast",		p->unicast);
	DEFRAG_INFO("%20s : %d\n\r", "hdr_len",		p->hdr_len);
	DEFRAG_INFO("%20s : %d\n\r", "tx_report",	p->tx_report);
	DEFRAG_INFO("%20s : %d\n\r", "tx_burst",	p->tx_burst);
	DEFRAG_INFO("%20s : %d\n\r", "ack_policy",  p->ack_policy);
	DEFRAG_INFO("%20s : %d\n\r", "RSVD_1",		p->RSVD_1);
	DEFRAG_INFO("%20s : %d\n\r", "do_rts_cts",  p->do_rts_cts);
	DEFRAG_INFO("%20s : %d\n\r", "reason",		p->reason);
	DEFRAG_INFO("%20s : %d\n\r", "payload_offset",	p->payload_offset); 
	DEFRAG_INFO("%20s : %d\n\r", "next_frag_pid",	p->next_frag_pid);
	DEFRAG_INFO("%20s : %d\n\r", "RSVD_2",		p->RSVD_2); 
	DEFRAG_INFO("%20s : %d\n\r", "fCmdIdx",		p->fCmdIdx); 
	DEFRAG_INFO("%20s : %d\n\r", "wsid",		p->wsid); 
	DEFRAG_INFO("%20s : %d\n\r", "txq_idx",		p->txq_idx); 
	DEFRAG_INFO("%20s : %d\n\r", "TxF_ID",		p->TxF_ID); 
	DEFRAG_INFO("%20s : %d\n\r", "rts_cts_nav",	p->rts_cts_nav); 
	DEFRAG_INFO("%20s : %d\n\r", "frame_consume_time",	p->frame_consume_time); 
	DEFRAG_INFO("%20s : %d\n\r", "RSVD_3",		p->RSVD_3); 
	// DEFRAG_INFO("%20s : %d\n\r", "RSVD_5",		p->RSVD_5); 
	DEFRAG_INFO("============================\n\r");
	payload_len = p->len - p->hdr_len;
	DEFRAG_INFO("%20s : %d\n\r", "payload_len", payload_len); 

	dat = (u8 *)p + p->hdr_offset;
	DEFRAG_INFO("========== hdr     ==========\n\r");
	DEFRAG_INFO("frame ctl     : 0x%04x\n\r", (((u16)dat[1] << 8)|dat[0]));
	DEFRAG_INFO("  - more_frag : %d\n\r", GET_HDR80211_FC_MOREFRAG(p)); 

	a = p + p->hdr_offset +  4;
	DEFRAG_INFO("address 1     : %02x:%02x:%02x:%02x:%02x:%02x\n\r", a[0], a[6], a[2], a[3], a[4], a[5]);
	DEFRAG_INFO("address 2     : %02x:%02x:%02x:%02x:%02x:%02x\n\r", a[6], a[7], a[8], a[9], a[10], a[11]);
	DEFRAG_INFO("address 3     : %02x:%02x:%02x:%02x:%02x:%02x\n\r", a[12], a[13], a[14], a[15], a[16], a[17]);
	
	DEFRAG_INFO("seq ctl       : 0x%04x\n\r", (((u16)dat[23] << 8)|dat[22]));
	DEFRAG_INFO("  - seq num   : %d\n\r", GET_HDR80211_SC_SEQNUM(p));
	DEFRAG_INFO("  - frag num  : %d\n\r", GET_HDR80211_SC_FRAGNUM(p));

	ssv6xxx_raw_dump_ex(dat, p->hdr_len, true, 10, 10, 16, LOG_LEVEL_INFO, LOG_MODULE_DEFRAG);

	DEFRAG_INFO("========== payload ==========\n\r");
	dat = (u8 *)p + p->payload_offset;
	ssv6xxx_raw_dump_ex(dat, p->len - p->hdr_len, true, 10, 10, 16, LOG_LEVEL_INFO, LOG_MODULE_DEFRAG);

	return;
#endif	/* _DEFRAG_ENABLE_DUMP */
}

void dump_PKT_RxInfo(PKT_RxInfo *p)
{
#if (_DEFRAG_ENABLE_DUMP)
	u32		payload_len;
	u32		i;
	u8		*dat;
	u8		*a;

	DEFRAG_INFO("========= RxInfo =========\n\r");
	DEFRAG_INFO("%20s : %d\n\r", "len",	        p->len);
	DEFRAG_INFO("%20s : %d\n\r", "c_type",	    p->c_type);
	DEFRAG_INFO("%20s : %d\n\r", "f80211",	    p->f80211);
	DEFRAG_INFO("%20s : %d\n\r", "qos",	        p->qos);
	DEFRAG_INFO("%20s : %d\n\r", "ht",		    p->ht);
	DEFRAG_INFO("%20s : %d\n\r", "l3cs_err",	p->l3cs_err);
	DEFRAG_INFO("%20s : %d\n\r", "l4cs_err",	p->l4cs_err);
	DEFRAG_INFO("%20s : %d\n\r", "use_4addr",	p->use_4addr);
	DEFRAG_INFO("%20s : %d\n\r", "RSVD_0",		p->RSVD_0);
	DEFRAG_INFO("%20s : %d\n\r", "psm",	        p->psm);
	DEFRAG_INFO("%20s : %d\n\r", "stype_b5b4",	p->stype_b5b4);
	DEFRAG_INFO("%20s : %d\n\r", "extra_info",	p->extra_info);	
	DEFRAG_INFO("%20s : 0x%08x\n\r", "fCmd",	p->fCmd);
	DEFRAG_INFO("%20s : %d\n\r", "hdr_offset",	p->hdr_offset);
	DEFRAG_INFO("%20s : %d\n\r", "frag",	    p->frag);
	DEFRAG_INFO("%20s : %d\n\r", "unicast",	    p->unicast);
	DEFRAG_INFO("%20s : %d\n\r", "hdr_len",	    p->hdr_len);
	DEFRAG_INFO("%20s : %d\n\r", "RxResult",	p->RxResult);
	DEFRAG_INFO("%20s : %d\n\r", "wildcard_bssid",	p->wildcard_bssid);
	DEFRAG_INFO("%20s : %d\n\r", "RSVD_1",	    p->RSVD_1);
	DEFRAG_INFO("%20s : %d\n\r", "reason",	    p->reason);
	DEFRAG_INFO("%20s : %d\n\r", "payload_offset",	p->payload_offset);
	DEFRAG_INFO("%20s : %d\n\r", "next_frag_pid",	p->next_frag_pid);
	DEFRAG_INFO("%20s : %d\n\r", "RSVD_2",	    p->RSVD_2);
	DEFRAG_INFO("%20s : %d\n\r", "fCmdIdx",	    p->fCmdIdx);
	DEFRAG_INFO("%20s : %d\n\r", "wsid",	    p->wsid);
	DEFRAG_INFO("%20s : %d\n\r", "RSVD_3",	    p->RSVD_3);
//	DEFRAG_INFO("%20s : %d\n\r", "RxF_ID",	    p->RxF_ID);
	DEFRAG_INFO("============================\n\r");

	payload_len = p->len - p->hdr_len;
	DEFRAG_INFO("%20s : %d\n\r", "payload_len", payload_len);

	dat = (u8 *)p + p->hdr_offset;
	DEFRAG_INFO("========== hdr     ==========\n\r");
	DEFRAG_INFO("frame ctl     : 0x%04x\n\r", (((u16)dat[1] << 8)|dat[0]));
	DEFRAG_INFO("  - more_frag : %d\n\r", GET_HDR80211_FC_MOREFRAG(p));

	a = p + p->hdr_offset +  4;
	DEFRAG_INFO("address 1     : %02x:%02x:%02x:%02x:%02x:%02x\n\r", a[0], a[6], a[2], a[3], a[4], a[5]);
	DEFRAG_INFO("address 2     : %02x:%02x:%02x:%02x:%02x:%02x\n\r", a[6], a[7], a[8], a[9], a[10], a[11]);
	DEFRAG_INFO("address 3     : %02x:%02x:%02x:%02x:%02x:%02x\n\r", a[12], a[13], a[14], a[15], a[16], a[17]);

	DEFRAG_INFO("seq ctl       : 0x%04x\n\r", (((u16)dat[23] << 8)|dat[22]));
	DEFRAG_INFO("  - seq num   : %d\n\r", GET_HDR80211_SC_SEQNUM(p));
	DEFRAG_INFO("  - frag num  : %d\n\r", GET_HDR80211_SC_FRAGNUM(p));

	ssv6xxx_raw_dump_ex(dat, p->hdr_len, true, 10, 10, 16, LOG_LEVEL_INFO, LOG_MODULE_DEFRAG);

	DEFRAG_INFO("========== payload ==========\n\r");
	dat = (u8 *)p + p->payload_offset;
	ssv6xxx_raw_dump_ex(dat, p->len - p->hdr_len, true, 10, 10, 16, LOG_LEVEL_INFO, LOG_MODULE_DEFRAG);
	return;
#endif	/* _DEFRAG_ENABLE_DUMP */
}

/* =========================== Timer Func ================================== */
#if (_DEFRAG_ENABLE_TIMER)

static void seq_timer_start(DEFRAG_SEQ *pSeq)
{
	//DEFRAG_TRACE(TRACE_dbg, "%-20s<= : 0x%08x, &key = 0x%08x, timer = 0x%08x\n\r", __FUNCTION__, pSeq, &(pSeq->key), pSeq->timer);
	OS_TimerStart(pSeq->timer);
}

static void seq_timer_stop(DEFRAG_SEQ *pSeq)
{
	//DEFRAG_TRACE(TRACE_dbg, "%-20s<= : 0x%08x, &key = 0x%08x, timer = 0x%08x\n\r", __FUNCTION__, pSeq, &(pSeq->key), pSeq->timer);
	OS_TimerStop(pSeq->timer);    
}

static void seq_timer_init(DEFRAG_SEQ *pSeq)
{
	s32 res;
	//DEFRAG_TRACE(TRACE_dbg, "%-20s<= : 0x%08x, &key = 0x%08x\n\r", __FUNCTION__, pSeq, &(pSeq->key));
	res = OS_TimerCreate(&(pSeq->timer), MAX_DEFRAG_TIMEOUT, false, pSeq, seq_timer_handler);
	//DEFRAG_TRACE(TRACE_dbg, "%-20s=> : timer = 0x%08x\n\r", __FUNCTION__, pSeq->timer);
	ASSERT(res == OS_SUCCESS); 
}

static void seq_timer_handler(void *timer)
{
	DEFRAG_SEQ	*pSeq;

	pSeq = (DEFRAG_SEQ *)pvTimerGetTimerID(timer);	
	seq_timer_stop(pSeq);
	//DEFRAG_TRACE(TRACE_dbg, "%-20s<= : 0x%08x, &key = 0x%08x\n\r", __FUNCTION__, pSeq, &(pSeq->key));
	seq_free(pSeq->key);
}

#endif /* _DEFRAG_ENABLE_TIMER */

bool	DEFRAG_KEY_IS_SAME_SEQ(const DEFRAG_KEY *a, const DEFRAG_KEY *b)
{
	bool	ret = false;
	u16		a_sn, b_sn;

	//DEFRAG_TRACE(TRACE_sub_major, "%-20s<= : \n\r", __FUNCTION__);
	//DEFRAG_TRACE(TRACE_sub_major, "#1:" DEFRAG_KEY_FMT "\n\r", DEFRAG_KEY_VA_LIST(*a));
	//DEFRAG_TRACE(TRACE_sub_major, "#2:" DEFRAG_KEY_FMT "\n\r", DEFRAG_KEY_VA_LIST(*b));
	if (a->is_qos != b->is_qos)
	{
		ret = false; 
		goto label_exit;
	}
	a_sn = GET_SC_SEQNUM(a->sc);
	b_sn = GET_SC_SEQNUM(b->sc);
	
	ret = false;
	if (a->is_qos == 0)	// non QoS frame
	{
		if (a_sn != b_sn)
			goto label_exit;

		if (memcmp(a->sa, b->sa, 6) != 0)
			goto label_exit;

		// otherwise, the 2 keys are in the same seq
		ret = true;
	}
	else				// QoS frame
	{
		if (a_sn != b_sn)
			goto label_exit;

		if (memcmp(a->sa, b->sa, 6) != 0)
			goto label_exit;

		if (memcmp(a->bssid, b->bssid, 6) != 0)
			goto label_exit;

		if (GET_QC_AC(a->qc) != GET_QC_AC(b->qc))
			goto label_exit;

		ret = true;
	}

label_exit:
	//DEFRAG_TRACE(TRACE_sub_major, "%-20s=> : return %d\n\r", __FUNCTION__, ret);
	return ret;
}

bool	get_key(PKT_Info *p, DEFRAG_KEY *key)
{
	u8	ToDS, FromDS;
	
	ASSERT_RET(p != NULL, FALSE);
	//DEFRAG_TRACE(TRACE_sub_minor, "%-10s<= : \n\r", __FUNCTION__);

	memset(key, 0, sizeof(DEFRAG_KEY));
	key->sc		= GET_HDR80211_SC(p);
	key->is_qos = p->qos;
	key->qc		= GET_HDR80211_QC(p);	// automatically set 'key->qc = 0' if (p->qos == 0)
	//
	//	Function			ToDS	FromDS	Addr1	Addr2	Addr3	Addr4
	//	-------------------------------------------------------------------------
	//	IBSS				0		0		DA		SA		BSSID	Not_Used
	//	To AP (infra.)		1		0		BSSID	SA		DA		Not_Used
	//	From AP (infra.)	0		1		DA		BSSID	SA		Not_Used
	//	WDS (bridge)		1		1		RA		TA		DA		SA
	ToDS 	= GET_HDR80211_FC_TODS(p);
	FromDS	= GET_HDR80211_FC_FROMDS(p);
	if ((ToDS == 0 && FromDS == 0) || (ToDS == 1 && FromDS == 1))
	{
		DEFRAG_ERROR("%-10s() :(ToDS=%d)&&(FromDS=%d), NOT supported, return false!\n\r", __FUNCTION__, ToDS, FromDS);
		return false;
	}
	
	if ((ToDS == 1) && (FromDS == 0))
	{
		GET_HDR80211_ADDRESS_1(key->bssid,	p);
		GET_HDR80211_ADDRESS_2(key->sa,		p);
	}
	else if ((ToDS == 0) && (FromDS == 1))
	{
		GET_HDR80211_ADDRESS_2(key->bssid,	p);
		GET_HDR80211_ADDRESS_3(key->sa,		p);
	}
	//DEFRAG_TRACE(TRACE_dbg, "%-10s() : " DEFRAG_KEY_FMT "\n\r", __FUNCTION__, DEFRAG_KEY_VA_LIST((*key)));
	//DEFRAG_TRACE(TRACE_sub_minor, "%-10s<= : \n\r", __FUNCTION__);
	return true;
}

static u32 frag_cal_total_payload_len(DEFRAG_SEQ *pSeq)
{
	u32		total;
	u32		i;

	total = 0;
	for (i = 0; i < pSeq->frag_cnt; i++)
	{
		total += (((PKT_RxInfo *)(pSeq->frag_array[i]))->len - ((PKT_RxInfo *)(pSeq->frag_array[i]))->hdr_len);
	}

	return total;
}


static void		frag_array_free(DEFRAG_SEQ *pSeq)
{
	u32		i;
	// free all none-empty frag_array[i] entry in seq
	for (i = 0; i < MAX_DEFRAG_FRAG; i++)
	{
		if (pSeq->frag_array[i] != NULL)
		{
			PBUF_MFree((u32 *)(pSeq->frag_array[i]));
			//DEFRAG_TRACE(TRACE_sub_major, "PBUF_MFree() frag_array[%d] = 0x%08x\n\r", i, pSeq->frag_array[i]);
			pSeq->frag_array[i] = (u8 *)NULL;
		}
	}
	pSeq->frag_cnt	= 0;
}


// check "whether all fragment pkt are recieved?" 
// return : true  => all pkt are recieved.
//          false => otherwise.
static bool frag_rollcall(DEFRAG_SEQ *pSeq)
{
	u32		i;
	u16		fc;

	//DEFRAG_TRACE(TRACE_sub_major, "%-20s<= : " DEFRAG_KEY_FMT "frag_cnt = %d\n\r", __FUNCTION__, DEFRAG_KEY_VA_LIST(pSeq->key), pSeq->frag_cnt);
	#if 0
	dump_frag_array(pSeq);
	#endif
	
	for (i = 0; i < pSeq->frag_cnt; i++)
	{
		if (pSeq->frag_array[i] == NULL)
		{
			//DEFRAG_TRACE(TRACE_sub_major, "%-20s() : i = %d, a lost pkt is found\n\r", __FUNCTION__, i);
			return false;
		}
	}
	// make sure the last pkt has 'more fragment' UNSET in Frame Control field.
	i = pSeq->frag_cnt - 1;
	fc = GET_HDR80211_FC((PKT_RxInfo *)(pSeq->frag_array[i]));
	if (IS_MOREFLAG_SET(fc))
	{
		//DEFRAG_TRACE(TRACE_sub_major, "%-20s=> : the last pkt's more_frag > 0, return false\n\r", __FUNCTION__);
		return false;
	}
	
	//DEFRAG_TRACE(TRACE_sub_major, "%-20s=>\n\r", __FUNCTION__);
	return true;
}

// add PKT_RxInfo into 'frag_array' in a certain seq
static bool	frag_add(DEFRAG_SEQ *pSeq, u8 *p)
{
	u16		fn,fc;

	fn = GET_HDR80211_SC_FRAGNUM((PKT_RxInfo *)(p));
	//DEFRAG_TRACE(TRACE_sub_middle, "%-20s<= : " DEFRAG_KEY_FMT "fn = %d, 0x%08x\n\r", __FUNCTION__, DEFRAG_KEY_VA_LIST(pSeq->key), fn, p);
	if (fn >= MAX_DEFRAG_FRAG)
	{
		DEFRAG_FAIL("Max fragments reached (%d).\n\r", fn);
		return false;
	}
	if (pSeq->frag_array[fn] != NULL)
	{
		fc = GET_HDR80211_FC((PKT_RxInfo *)(p));
		if (IS_MOREFLAG_SET(fc))
			return false;
		else
			DEFRAG_FAIL("Fragment ID \"%d\" exists.\n\r", fn);
		return false;
	}
	pSeq->frag_array[fn] = p;
	pSeq->frag_cnt++;
	
	#if 0
	dump_frag_array(pSeq);
	#endif
	
	//DEFRAG_TRACE(TRACE_sub_middle, "%-20s=> : frag_cnt = %d\n\r", __FUNCTION__, pSeq->frag_cnt);
	return true;
}


// 1. rls all frag_array belongs to this seq
// 2. rls this seq
static void		seq_free(DEFRAG_SEQ *pSeq)
{
	//DEFRAG_TRACE(TRACE_sub_major, "%-20s=> : pSeq = 0x%08x, seq_cnt = %d\n\r", __FUNCTION__, pSeq, seq_cnt);
	frag_array_free(pSeq);
	// reset seq_array[] entry
	pSeq->bUsed		= false;
	pSeq->tick_last_arrival = 0;
	memset(&(pSeq->key), 0, sizeof(DEFRAG_KEY));

	seq_cnt--;

	//DEFRAG_TRACE(TRACE_sub_major, "%-20s=> : seq_cnt = %d\n\r", __FUNCTION__, seq_cnt);
	return;
}

// use empty array entry & add it into arrray
static void		seq_add_new(DEFRAG_SEQ *pSeq, const DEFRAG_KEY *key)
{
	pSeq->bUsed		= true;
	pSeq->key		= *key;
	seq_cnt++;
	return;
}

// release timeout seq
// return :
// 	0 : none of seq is released
// >0 : total # of released seq
DEFRAG_SEQ *seq_rls_timeout(void)
{
	u32		n, loop;
	DEFRAG_SEQ	*pSeq;
    DEFRAG_SEQ  *pFreedSeq = (DEFRAG_SEQ  *)NULL;
	
	//DEFRAG_TRACE(TRACE_sub_major, "%-20s<=\n\r", __FUNCTION__);
	loop = seq_cnt;
	for (n = 0; n < loop; n++)
	{
		pSeq = &(seq_array[n]);
		if ((xTaskGetTickCount() - pSeq->tick_last_arrival)*portTICK_RATE_MS > MAX_DEFRAG_TIMEOUT) 
		{
			seq_free(pSeq);
            pFreedSeq = pSeq;
		}
	}
	//DEFRAG_TRACE(TRACE_sub_major, "%-20s=> : rls seq\n\r", __FUNCTION__);	
	return pFreedSeq;
}


// return :
//	NULL -> fail! (seq_array is full & no such 'key' is found)
//  &seq_array[i] -> success! with 2 status :
//		1). STATUS_SEQ_OCCUPIED_SLOT
//		2). STATUS_SEQ_EMPTY_SLOT
static DEFRAG_SEQ*		get_seq(const DEFRAG_KEY *key, u8 *status)
{
	s16		i;

	//DEFRAG_TRACE(TRACE_sub_major, "%-20s<= : " DEFRAG_KEY_FMT "\n\r", __FUNCTION__, DEFRAG_KEY_VA_LIST(key));
	*status = 0;
	// scan whole seq_array & return if the matching key is found.
	for (i = 0; i < MAX_DEFRAG_SEQ; i++)
	{
		if (!seq_array[i].bUsed)
		{
			*status = STATUS_SEQ_EMPTY_SLOT;
			//DEFRAG_TRACE(TRACE_sub_major, "%-20s=> : seq_array[%d] = 0x%08x, STATUS_SEQ_EMPTY_SLOT\n\r", __FUNCTION__, i, seq_array[i]);
			return &(seq_array[i]);
		}

		if ((seq_array[i].bUsed) && (DEFRAG_KEY_IS_SAME_SEQ(key, &seq_array[i].key)))
		{
			*status = STATUS_SEQ_OCCUPIED_SLOT;
			//DEFRAG_TRACE(TRACE_sub_major, "%-20s=> : seq_array[%d] = 0x%08x, STATUS_SEQ_OCCUPIED_SLOT\n\r", __FUNCTION__, i, seq_array[i]);
			return &(seq_array[i]);
		}
	}

	return (DEFRAG_SEQ *)NULL;
}



// assumption : the frag_rollcall() should be called before this func.
//
// caution : inside this func,
//		1. it will call PBUF_MAlloc() for rx_pkt &
//		2. call seq_free() which will PBUF_MFree() all frag_array[] entry.
//
static bool	defrag_reassemble(DEFRAG_SEQ *pSeq)
{
	u32			i, loops;
	u32			pbuf_size;	// total size need to be alloced
	u8			*p_end;		// point to the end pos in rx_pkt's payload during copying operation
	u8            *p0;		// point to frag_array[0]	
	PKT_RxInfo    *p;			// for temp use
	u32			copy_len;
	u32			payload_len;
	u32			payload_offset;
	u32			hdr_len;
	u32			hdr_offset;
	
	//DEFRAG_TRACE(TRACE_sub_major, "%-20s<= : " DEFRAG_KEY_FMT "\n\r", __FUNCTION__, DEFRAG_KEY_VA_LIST(pSeq->key));
	
	// cal total payload len & hdr len
	payload_len		= frag_cal_total_payload_len(pSeq);
	payload_offset	= ((PKT_RxInfo *)(pSeq->frag_array[0]))->payload_offset;
	hdr_len			= ((PKT_RxInfo *)(pSeq->frag_array[0]))->hdr_len;
	hdr_offset		= ((PKT_RxInfo *)(pSeq->frag_array[0]))->hdr_offset;

	// cal total size & call PBUF_MALLOC() for rx_pkt
	pbuf_size = hdr_len + payload_len;
	rx_pkt = (u8 *)PBUF_MAlloc(pbuf_size,SOC_PBUF);
	if (rx_pkt == NULL)
	{
		// TODO: discuss with Felix : what to do if malloc fail.
		DEFRAG_FATAL("rx_pkt = PBUF_MAlloc(%u)\n\r", pbuf_size);
	}
	//DEFRAG_TRACE(TRACE_dbg, "%-20s() : malloc(%u) for 'rx_pkt' = 0x%08x\n\r", __FUNCTION__, pbuf_size, rx_pkt);
	
	// copy frag #0 RxInfo to rx_pkt
	p0 = (u8 *)(pSeq->frag_array[0]);
	drv_pkt_memcpy(rx_pkt, p0, sizeof(PKT_RxInfo));
	
	// copy frag #0's 802.11 header to rx_pkt
	drv_pkt_memcpy(rx_pkt + hdr_offset, p0 + hdr_offset, hdr_len);
	
	// do real reassemble job! copy all the payload into rx_pkt.
	p_end = (u8 *)rx_pkt + payload_offset;

    /* Simulation only: set new packet buffer's PID */
    #if (defined _DEFRAG_SIM_PLATFORM)
    PBUF_SetPktID(rx_pkt, PBUF_GetPbufID((u8 *)p0));
	#endif
	
	loops = pSeq->frag_cnt;
	for (i = 0; i < loops; i++)
	{
		p = (PKT_RxInfo *)(pSeq->frag_array[i]);
		copy_len = p->len - p->hdr_len;
		drv_pkt_memcpy(p_end, (u8 *)p + p->payload_offset, copy_len);
		p_end += copy_len;
	}
	// free all frag_array[] entry belongs this seq
	seq_free(pSeq);
	// update the fields in the output pkt.
	((PKT_RxInfo *)rx_pkt)->frag = 0;	
	((PKT_RxInfo *)rx_pkt)->next_frag_pid = 0;
	((PKT_RxInfo *)rx_pkt)->len	 = hdr_len + payload_len;
	UNSET_HDR80211_FC_MOREFRAG((PKT_RxInfo *)rx_pkt);

	//DEFRAG_TRACE(TRACE_sub_major, "%-20s=>\n\r", __FUNCTION__);
	return true;
}

s32 defrag_init(void)
{
	u32		i;

	//DEFRAG_TRACE(TRACE_sub_middle, "%-20s<=\n\r", __FUNCTION__);
	seq_cnt = 0;
	for (i = 0; i < MAX_DEFRAG_SEQ; i++)
		memset(&(seq_array[i]), 0, sizeof(DEFRAG_SEQ));
	
	rx_pkt = (u8 *)NULL;
	return OS_SUCCESS;
}




	
void Soft_FillRxPktInfo(PKT_RxInfo*p)
{

	struct cfg_host_rxpkt *hp = (struct cfg_host_rxpkt *)p;


	//memset((u8*)&hp->seq_num, 0, 8);

	hp->RCPI = 0;
	hp->SNR  = 0;

	
	hp->pkt_type = 0;
	

	hp->seq_num= GET_HDR80211_SC(p);

	if(p->qos)
		hp->tid = GET_QC_TID(GET_HDR80211_QC(p));
	else
		hp->tid = 0;

	





}



static u32 defrag_count = 0;
void Soft_DeFragHandler(PKT_Info *p, s32 reason)
{
	DEFRAG_SEQ	*pSeq;
	DEFRAG_KEY	key;
	u8			status;

    LOG_MODULE_TURN_ON(LOG_MODULE_DEFRAG);
#if (CONFIG_SIM_PLATFORM == 1)
	PBUF_AddFlow(p, "ME_DEFRAG", "(in)"); 
#endif



	

	Soft_FillRxPktInfo((PKT_RxInfo*)p);

    g_soc_mib.hdl.hdl_defrag_rx_cnt++;
	//DEFRAG_TRACE(TRACE_handler, "%-20s<= : 0x%08x\n\r", __FUNCTION__, p);
	if ((p->frag) == 0)					// not defragment packet
	{
		//DEFRAG_TRACE(TRACE_dbg, "%-20s=> : not defrag pkt -> ENG_MBOX_NEXT()\n\r", __FUNCTION__);
#if (CONFIG_SIM_PLATFORM == 1)		
		PBUF_AddFlow(p,"ME_DEFRAG","(out)");
#endif


		

		ENG_MBOX_NEXT((u32)p);
		return;
	}
	if (GET_HDR80211_FC_TYPE(p) == 0)	// management frame
	{
		//DEFRAG_TRACE(TRACE_dbg, "%-20s=> : mgmt frame -> ENG_MBOX_NEXT()\n\r", __FUNCTION__);
#if (CONFIG_SIM_PLATFORM == 1)		
		PBUF_AddFlow(p,"ME_DEFRAG","(out)");
#endif
		ENG_MBOX_NEXT((u32)p);
		return;
	}

	g_soc_mib.hdl.hdl_defrag_accept++;

	//DEFRAG_TRACE("DF <= 0x%08x\n\r", p);

	//DEFRAG_TRACE(TRACE_dbg, "frm ctl = 0x%04x [T=%s, MF=%d, MD=%d]\n\r", GET_HDR80211_FC(p), GET_HDR80211_FC_TYPE_STR(GET_HDR80211_FC_TYPE(p)), GET_HDR80211_FC_MOREFRAG(p), GET_HDR80211_FC_MOREDATA(p));
	//DEFRAG_TRACE(TRACE_dbg, "seq ctl = 0x%04x [sn=%d, fn=%d]\n\r", GET_HDR80211_SC(p), GET_HDR80211_SC_SEQNUM(p), GET_HDR80211_SC_FRAGNUM(p));
	#if 0
	dump_PKT_RxInfo((PKT_TxInfo *)p);
	#endif

	if (!get_key(p, &key))
	{
		PBUF_MFree(p);
		DEFRAG_INFO("get_key() fail, pkt drop & return!\n\r");
		return;
	}

	// 1st try to get_seq()
	pSeq = get_seq(&key, &status);
	if (pSeq == NULL)
	{	
		// note : there is ONLY 1 case that get_seq() return NULL on 1st try.
		//        => the seq_array is full & this is the NEW seq.
		ASSERT(seq_cnt == MAX_DEFRAG_SEQ);
		DEFRAG_INFO("since seq_array is full, try to rls timeout seq ...\n\r");
        pSeq = seq_rls_timeout();
		if (pSeq == NULL)
		{
			PBUF_MFree(p);
			DEFRAG_INFO("seq_rls_timeout() fail, pkt drop & return!\n\r");
			return;
		}
        status = STATUS_SEQ_EMPTY_SLOT;

		// while program go here, some seq_array slot must be clear
		ASSERT(seq_cnt < MAX_DEFRAG_SEQ);
		// 2nd try to get_seq()
		//pSeq = get_seq(&key, &status);
		// ASSERT(pSeq != NULL);
	}
	// if this is a new seq
	if (status == STATUS_SEQ_EMPTY_SLOT)
		seq_add_new(pSeq, &key);

	if (!frag_add(pSeq, (u8 *)p))
	{
		PBUF_MFree(p);
		//DEFRAG_FAIL("frag_add(), drop pkt! 0x%08x\n\r", (u32)p);
		// release seq if it is new
		if (status == STATUS_SEQ_EMPTY_SLOT)
			pSeq->bUsed = false;

		return;
	}

	pSeq->tick_last_arrival = xTaskGetTickCount();

	if (frag_rollcall(pSeq))
	{
		if (!defrag_reassemble(pSeq))
		{
			DEFRAG_FAIL("defrag_reassemble() -> free seq!\n\r");
			seq_free(pSeq);
			return;
		}
		#if 0
		dump_PKT_RxInfo((PKT_RxInfo *)rx_pkt);
		#endif
		
		// defrag success! throw rx_pkt to next engine.
#if (CONFIG_SIM_PLATFORM == 1)		
		PBUF_AddFlow(rx_pkt,"ME_DEFRAG","(out)");
#endif
		ENG_MBOX_NEXT((u32)rx_pkt);

        defrag_count++;
		//DEFRAG_TRACE("(%u) DF => %08X\n\r", defrag_count, rx_pkt);
	}

	//DEFRAG_TRACE(TRACE_handler, "%-20s=> \n\r", __FUNCTION__);
	return;
}

