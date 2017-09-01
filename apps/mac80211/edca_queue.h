#ifndef _EDCA_QUEUE_
#define _EDCA_QUEUE_


#include "types.h"



/**
 * enum mactxrx_tx_queue_subtype - Queue sub-types to classify normal data queues
 * @MACTXRX_WME_AC_BK: Background traffic
 * @MACTXRX_WME_AC_BE: Best-effort (normal) traffic
 * @MACTXRX_WME_AC_VI: Video traffic
 * @MACTXRX_WME_AC_VO: Voice traffic
 *
 * These are the 4 Access Categories as defined in
 * WME spec. 4 is the lowest priority and 0 is the
 * highest. Normal data that hasn't been classified
 * goes to the Best Effort AC.
 */
enum edca_tx_queue_subtype {
	EDCA_WME_AC_FIRST	= 0						, 	
	EDCA_WME_AC_BK 		= EDCA_WME_AC_FIRST		,
	EDCA_WME_AC_BE								,
	EDCA_WME_AC_VI								,
	EDCA_WME_AC_VO								,
	EDCA_WME_AC_BC								,
	EDCA_WME_AC_LAST	= EDCA_WME_AC_BC		, 	
	EDCA_WME_AC_MAX								,
	EDCA_WME_AC_ALL		= EDCA_WME_AC_MAX		,
	EDCA_WME_QUE_MAX	= EDCA_WME_AC_MAX		,
};



#define	EDCA_NUM_TX_QUEUES	EDCA_WME_AC_MAX




#define	EDCA_TXBUF	80		/* number of TX buffers */

#define EDCA_TXQ_LEN_MAX	(EDCA_TXBUF / 4)		/* bufs per queue */
#define EDCA_TXQ_LEN_LOW	(EDCA_TXQ_LEN_MAX / 2)	/* low mark */



struct edca_txq_mib_info{	

	u8 ack_policy;
};

/**
 * struct mactxrx_txq - Transmit queue state
 * @qnum: Hardware q number
 * @link: Link ptr in last TX desc
 * @q: Transmit queue (&struct list_head)
 * @lock: Lock on q and link
 * @setup: Is the queue configured
 * @txq_len:Number of queued buffers
 * @txq_max: Max allowed num of queued buffers
 * @txq_poll_mark: Used to check if queue got stuck
 * @txq_stuck: Queue stuck counter
 *
 * One of these exists for each hardware transmit queue.
 * Packets sent to us from above are assigned to queues based
 * on their priority.  Not all devices support a complete set
 * of hardware transmit queues. For those devices the array
 * sc_ac2q will map multiple priorities to fewer hardware queues
 * (typically all to one hardware queue).
 */
struct edca_txq {	
	u32			*data;
	u16				txq_len;
	u16				txq_max;


	u16				xmit_num;	
#if (EDCA_DBG == 1)
	struct edca_txq_mib_info	mib_info[EDCA_TXQ_LEN_MAX];	
#endif

	
};






enum edca_xmit_type {
	EDCA_XMIT_ALL,	
	EDCA_XMIT_TXOP,
};






s32 EDCA_QueueInit(void);
s32 EDCA_QUE_Setup(enum edca_tx_queue_subtype queuetype);
s32 EDCA_addToQueue(void *data, enum edca_tx_queue_subtype queuetype);
void* EDCA_RemoveFromQueue(enum edca_tx_queue_subtype queuetype);
void* EDCA_PeekFromQueue( enum edca_tx_queue_subtype queuetype, int qidx);
bool EDCA_QueueEmpty(enum edca_tx_queue_subtype queuetype);
//#define EDCA_QueueEmpty(_txq) (_txq->len==0) 

#define EDCA_QueueElements(_txq_type) (gEDCAInfo->txq[_txq_type]->len)



//----------------------------------------------


void EDCA_PrintQueueInfo(void);



#endif /* _EDCA_QUEUE_ */
