#ifndef _EDCA_H_
#define _EDCA_H_

#include <msgevt.h>
#include "edca_queue.h"



typedef enum{
	EDCA_INFOFLAG_WAIT_BK_TX_DONE 	= 0,
	EDCA_INFOFLAG_WAIT_BE_TX_DONE	,
	EDCA_INFOFLAG_WAIT_VI_TX_DONE	, 
	EDCA_INFOFLAG_WAIT_VO_TX_DONE	, 
	
}EDCA_INFOFLAG;



#define EDCA_IS_ANY_ONE_WAIT_TX_DONE  (gEDCAInfo->info_flag)

#define EDCA_IS_WAIT_TX_DONE(_q) !!(gEDCAInfo->info_flag&(1<<_q))
#define EDCA_SET_Q_WAIT_TX_DONE(_q, _tf) \
    /*lint -save -e845 */ \
    do { \
        gEDCAInfo->info_flag = ((gEDCAInfo->info_flag & ~(1<<(_q)))| ((_tf)<<(_q))); \
    } while (0) /*lint -save -e845 */ \




struct ecda_info {

	u32 info_flag;
	struct edca_txq   txq[EDCA_NUM_TX_QUEUES];

	
	

};


s32 EDCA_init(void);

void EDCA_MacTxIntNotify(enum edca_tx_queue_subtype queuetype);

void EDCA_Handler(PKT_Info *pPktInfo, s32 reason);
 



#endif /* _EDCA_H_ */

