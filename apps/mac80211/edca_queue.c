//#define __SFILE__ "edca_queue.c"

#include <config.h>
#include <pbuf.h>
#include "edca_queue.h"
#include "rtos.h"
#include "edca.h"
#include <log.h>
//#define LOG_TRACE(...)
//#define LOG_FATAL(...)


extern struct ecda_info *gEDCAInfo;

s32 EDCA_QUE_Setup(enum edca_tx_queue_subtype queuetype)
{
	struct edca_txq *txq = &gEDCAInfo->txq[queuetype];
	//int queueIndex = tqi_subtype;

	txq->data = (u32 *)OS_MemAlloc(sizeof(u32 *)*EDCA_TXQ_LEN_MAX);

	txq->txq_max = EDCA_TXQ_LEN_MAX;

	return 0;
}


s32 EDCA_addToQueue(void *data, enum edca_tx_queue_subtype queuetype)
{
	struct edca_txq *txq = &gEDCAInfo->txq[queuetype];
	int index;
	//struct edca_txq *txq = &gEDCAInfo->txq[tqi_subtype];

	if ( txq->txq_len >= txq->txq_max )
	{
		LOG_PRINTF("Queue Full\n");
		return -1;
	}


	//index = txq->txq_len%txq->txq_max;
	txq->txq_len++;
	index = txq->txq_len-1;
	txq->data[index] = (u32)data;

	return 0;
}



void* EDCA_RemoveFromQueue(enum edca_tx_queue_subtype queuetype)
{
	struct edca_txq *txq = &gEDCAInfo->txq[queuetype];
	u32 data = txq->data[0];

	if (0 == txq->txq_len)
		return NULL;

	txq->txq_len--;
	memcpy(&txq->data[0], &txq->data[1], txq->txq_len*sizeof(u32));

	//reset last for fun
	//txq->data[txq->txq_len]=0;

	return (void*)data;
}



void* EDCA_PeekFromQueue(enum edca_tx_queue_subtype queuetype, int qidx)
{
	const struct edca_txq *txq = &gEDCAInfo->txq[queuetype];
	if (qidx >= txq->txq_len)
		return NULL;

	return (void*)txq->data[qidx];
}

bool EDCA_QueueEmpty(enum edca_tx_queue_subtype queuetype)
{
	const struct edca_txq *txq = &gEDCAInfo->txq[queuetype];
	if (0 == txq->txq_len)
		return TRUE;

	return FALSE;
}


//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//for debug


struct tx_queue_info_tbl
{
	const char *queuename;
};


static const struct tx_queue_info_tbl queueinfo[EDCA_NUM_TX_QUEUES]=
{
	//{"QUEUE_BEACON"},
	//{"QUEUE_BROADCAST"},
	{"QUEUE_BK"},
	{"QUEUE_BE"},
	{"QUEUE_VI"},
	{"QUEUE_VO"},

};



void EDCA_PrintQueueInfo()
{
	s32 i;



	for (i=0;i<(s32)EDCA_NUM_TX_QUEUES;i++)
	{
		LOG_PRINTF("=============================================================\n");
		LOG_PRINTF("%s-->len:%u wait tx done:%d\n", queueinfo[i].queuename, gEDCAInfo->txq[i].txq_len, EDCA_IS_WAIT_TX_DONE(i));
	}
	LOG_PRINTF("\n\n");

}
