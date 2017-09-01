#include <config.h>
#include <stdarg.h>
#include <hdr80211.h>
#include <ssv_pktdef.h>
#include <mbox/drv_mbox.h>


#if (CONFIG_SIM_PLATFORM == 0)
#include <uart/drv_uart.h>
#endif

#include <log.h>
#include "lib-impl.h"
#include "ssv_lib.h"
#include <pbuf.h>
#include <msgevt.h>

#if(PACKET_TRACE==1)
#define MAX_PT_MAP_SIZE 64
struct pbuf_trace pt_map[MAX_PT_MAP_SIZE];
#endif

u8 g_txQueueMapping[MAX_TX_DATA_QUEUE]={0};

#define CONSOLE_UART   SSV6XXX_UART0
#define MAX_DQ_DEPTH (31+31) //The value of this macro depend on  SSV6200_ID_TX_THRESHOLD and SSV6200_ID_TX_THRESHOLD, those are defined in iot-host

//Now WLAN_MAX_STA=2
//tx_dq_head[0] is for wsid 0
//tx_dq_head[1] is for wsid 1
//tx_dq_head[2] is for all managerment and broadcast frames
struct data_queue tx_dq_head[MAX_TX_DATA_QUEUE];
struct data_queue rx_dq_head[MAX_RX_DATA_QUEUE];
struct data_queue fdq_head;
struct data_packet dp[MAX_DQ_DEPTH];

void list_q_init(struct list_q *qhd)
{
    qhd->prev = (struct list_q *)qhd;
    qhd->next = (struct list_q *)qhd;
    qhd->qlen = 0;
}

void list_q_qtail(struct list_q *qhd, struct list_q *newq)
{
    struct list_q *next = qhd;
    struct list_q* prev = qhd->prev;

    newq->next = next;
    newq->prev = prev;
    next->prev = newq;
    prev->next = newq;
    qhd->qlen++;
}

void list_q_insert(struct list_q *qhd, struct list_q *prev, struct list_q *newq)
{
    struct list_q *next = prev->next;

    newq->next = next;
    newq->prev = prev;
    next->prev = newq;
    prev->next = newq;
    qhd->qlen++;
}

void list_q_remove(struct list_q *qhd,struct list_q *curt)
{
    struct list_q *next = curt->next;
    struct list_q *prev = curt->prev;

    prev->next = next;
    next->prev = prev;
    qhd->qlen--;
}

struct list_q * list_q_deq(struct list_q *qhd)
{
    struct list_q *next, *prev;
    struct list_q *elm = qhd->next;

    if((qhd->qlen > 0) && (elm != NULL))
    {
        qhd->qlen--;
        next        = elm->next;
        prev        = elm->prev;
        elm->next   = NULL;
        elm->prev   = NULL;
        next->prev  = prev;
        prev->next  = next;

        return elm;
    }else{
        return NULL;
    }
}
unsigned int list_q_len(struct list_q *qhd)
{
    return qhd->qlen;
}

u32 list_q_len_safe(struct list_q *q, OsMutex *pmtx)
{
    u32 len = 0;
    OS_MutexLock(*pmtx);
    len = q->qlen;
    OS_MutexUnLock(*pmtx);
    return len;
}

void list_q_qtail_safe(struct list_q *qhd, struct list_q *newq, OsMutex *pmtx)
{
    OS_MutexLock(*pmtx);
    list_q_qtail(qhd, newq);
    OS_MutexUnLock(*pmtx);
}

struct list_q * list_q_deq_safe(struct list_q *qhd, OsMutex *pmtx)
{
    struct list_q *_list = NULL;
    OS_MutexLock(*pmtx);
    _list = list_q_deq(qhd);
    OS_MutexUnLock(*pmtx);
    return _list;
}

void list_q_insert_safe(struct list_q *qhd, struct list_q *prev, struct list_q *newq, OsMutex *pmtx)
{
    OS_MutexLock(*pmtx);
    list_q_insert(qhd, prev, newq);
    OS_MutexUnLock(*pmtx);
}

void list_q_remove_safe(struct list_q *qhd,struct list_q *curt, OsMutex *pmtx)
{
    OS_MutexLock(*pmtx);
    list_q_remove(qhd, curt);
    OS_MutexUnLock(*pmtx);
}
LIB_APIs static s32 _ssv6xxx_SWQInit(s32 qID, EN_SW_QUEUE q)
{
    struct data_queue *dq_head=NULL;
    dq_head=(q==TX_SW_QUEUE)?tx_dq_head:rx_dq_head;
    if(q==RX_SW_QUEUE) qID=0; // No matter rx data or event, every client share the same rx queue

    memset((void *)&dq_head[qID],0,sizeof(struct data_queue));
    list_q_init(&dq_head[qID].qHead);
    return 0;
}

LIB_APIs static s32 _ssv6xxx_SWFreeQInit(void)
{
    int i=0;
    //Init free data queue head
    memset((void *)&fdq_head,0,sizeof(struct data_queue));
    list_q_init(&fdq_head.qHead);

    //Reserve enough data_pack in free queue
    if(MAX_DQ_DEPTH < (GET_TX_ID_THOLD+GET_RX_ID_THOLD)){
        printf("\33[31mWARNING:The MAX_DQ_DEPTH and (SSV6200_ID_TX_THRESHOLD+SSV6200_ID_RX_THRESHOLD) are not equal \33[0m\n");
        while(1){;}
    }

    for(i=0;i<MAX_DQ_DEPTH;i++){

        memset((void *)&dp[i],0,sizeof(struct data_packet));
#if(DATA_QUEUE_DEBUG==1)
        dp[i].idx=i;
#endif
        list_q_init(&dp[i].q);
        list_q_qtail(&fdq_head.qHead,&dp[i].q);
    }

    return 0;
}
LIB_APIs s32 ssv6xxx_SWQInit(void)
{
    u8 i=0;

    //Init data queue head
    for(i=0;i<MAX_TX_DATA_QUEUE;i++){
        _ssv6xxx_SWQInit(i,TX_SW_QUEUE);
    }

    for(i=0;i<MAX_RX_DATA_QUEUE;i++){
        _ssv6xxx_SWQInit(i,RX_SW_QUEUE);
    }
    _ssv6xxx_SWFreeQInit();

    g_txQueueMapping[TX_DATA_QUEUE_0]=1;//M_ENG_TX_EDCA1
    g_txQueueMapping[TX_DATA_QUEUE_1]=2;//M_ENG_TX_EDCA2
    g_txQueueMapping[TX_DATA_QUEUE_2]=3;//M_ENG_TX_EDCA3

    return 0;
}

LIB_APIs s32 ssv6xxx_SWQWaittingSize(s32 qID, EN_SW_QUEUE q)
{
    struct data_queue *dq_head=NULL;
    dq_head=(q==TX_SW_QUEUE)?tx_dq_head:rx_dq_head;
    if(q==RX_SW_QUEUE) qID=0; // No matter rx data or event, every client share the same rx queue

    if(((qID >= MAX_TX_DATA_QUEUE)&&(q==TX_SW_QUEUE))||
        ((qID >= MAX_RX_DATA_QUEUE)&&(q==RX_SW_QUEUE))){
        DQ_LOG("Inavlid qID:%d\n",qID);
        return -1;
    }

    return (s32)list_q_len(&dq_head[qID].qHead);
}

LIB_APIs inline s32 ssv6xxx_SWFreeQWaittingSize(void)
{
    return (s32)list_q_len(&fdq_head.qHead);
}
LIB_APIs s32 ssv6xxx_SWQIntLock(s32 qID, EN_SW_QUEUE q)
{
    struct data_queue *dq_head=NULL;
    //u32 time=0;

    dq_head=(q==TX_SW_QUEUE)?tx_dq_head:rx_dq_head;
    if(q==RX_SW_QUEUE) qID=0; // No matter rx data or event, every client share the same rx queue

    if(((qID >= MAX_TX_DATA_QUEUE)&&(q==TX_SW_QUEUE))||
        ((qID >= MAX_RX_DATA_QUEUE)&&(q==RX_SW_QUEUE))){
        DQ_LOG("Inavlid qID:%d\n",qID);
        return -1;
    }

    DQ_LOG("lock input of dq[%d]\n",qID);

    //if(!gOsFromISR)
    //    time=xTaskGetTickCount();
    //else
    //    time=xTaskGetTickCountFromISR();

    if(!gOsFromISR) taskENTER_CRITICAL();

    dq_head[qID].int_lock = TRUE;
    //dq_head[qID].int_last_lock_time=time;
    if(!gOsFromISR) taskEXIT_CRITICAL();

    return 0;

}

LIB_APIs s32 ssv6xxx_SWQIntUnLock(s32 qID, EN_SW_QUEUE q)
{
    struct data_queue *dq_head=NULL;

    dq_head=(q==TX_SW_QUEUE)?tx_dq_head:rx_dq_head;
    if(q==RX_SW_QUEUE) qID=0; // No matter rx data or event, every client share the same rx queue

    if(((qID >= MAX_TX_DATA_QUEUE)&&(q==TX_SW_QUEUE))||
        ((qID >= MAX_RX_DATA_QUEUE)&&(q==RX_SW_QUEUE))){
        DQ_LOG("Inavlid qID:%d\n",qID);
        return -1;
    }

    DQ_LOG("unlock input of dq[%d]\n",qID);

    if(!gOsFromISR) taskENTER_CRITICAL();

    dq_head[qID].int_lock = FALSE;

    if(!gOsFromISR) taskEXIT_CRITICAL();

    return 0;
}

LIB_APIs s32 ssv6xxx_SWQOutLock(s32 qID, EN_SW_QUEUE q)
{
    struct data_queue *dq_head=NULL;
    u32 time=0;
    dq_head=(q==TX_SW_QUEUE)?tx_dq_head:rx_dq_head;
    if(q==RX_SW_QUEUE) qID=0; // No matter rx data or event, every client share the same rx queue

    if(((qID >= MAX_TX_DATA_QUEUE)&&(q==TX_SW_QUEUE))||
        ((qID >= MAX_RX_DATA_QUEUE)&&(q==RX_SW_QUEUE))){
        DQ_LOG("Inavlid qID:%d\n",qID);
        return -1;
    }

    DQ_LOG("lock output of dq[%d]\n",qID);

    if(!gOsFromISR)
        time=xTaskGetTickCount();
    else
        time=xTaskGetTickCountFromISR();

    if(!gOsFromISR) taskENTER_CRITICAL();

    dq_head[qID].out_lock = TRUE;
    dq_head[qID].out_auto_lock = FALSE;
    dq_head[qID].out_auto_lock_count=0;
    dq_head[qID].out_last_lock_time=time;
    if(!gOsFromISR) taskEXIT_CRITICAL();

    return 0;

}

LIB_APIs s32 ssv6xxx_SWQOutUnLock(s32 qID, EN_SW_QUEUE q)
{
    struct data_queue *dq_head=NULL;

    dq_head=(q==TX_SW_QUEUE)?tx_dq_head:rx_dq_head;
    if(q==RX_SW_QUEUE) qID=0; // No matter rx data or event, every client share the same rx queue

    if(((qID >= MAX_TX_DATA_QUEUE)&&(q==TX_SW_QUEUE))||
        ((qID >= MAX_RX_DATA_QUEUE)&&(q==RX_SW_QUEUE))){
        DQ_LOG("Inavlid qID:%d\n",qID);
        return -1;
    }

    DQ_LOG("unlock output of dq[%d]\n",qID);

    if(!gOsFromISR) taskENTER_CRITICAL();

    dq_head[qID].out_lock = FALSE;
    dq_head[qID].out_auto_lock = FALSE;
    dq_head[qID].out_auto_lock_count=0;

    if(!gOsFromISR) taskEXIT_CRITICAL();

    return 0;
}
/*
This function is for PS_PULL handler.  It will cause the data queu lock automatically after it output the packets.
qID: queue number
count : how much packe that we want to output before the queue lock automatically
*/
LIB_APIs inline s32 ssv6xxx_SWQOutConUnLock(s32 qID, EN_SW_QUEUE q, u8 count)
{
    struct data_queue *dq_head=NULL;
    dq_head=tx_dq_head;

    if(q!=TX_SW_QUEUE){
        DQ_LOG("This function only support on tx now\n");
        return -1;
    }

    if(qID >= MAX_TX_DATA_QUEUE){
        DQ_LOG("Inavlid qID:%d\n",qID);
        return -1;
    }

    DQ_LOG("condictional unlcok dq[%d]. count=%d\n",qID,count);

    if(!gOsFromISR) taskENTER_CRITICAL();

    dq_head[qID].out_lock = FALSE;
    dq_head[qID].out_auto_lock = TRUE;
    dq_head[qID].out_auto_lock_count+=count;

    if(!gOsFromISR) taskEXIT_CRITICAL();

    return 0;
}
static s32 _ssv6xxx_SWQAdd(s32 qID, EN_SW_QUEUE q, bool bAddToTail, void *pbuf, s32 next_hw)
{
    s32 ret = 0;
    struct data_queue *dq_head=NULL;
    struct data_packet *pdp=NULL;
    struct list_q *_q=NULL;
    u8 i=0;
    dq_head=(q==TX_SW_QUEUE)?tx_dq_head:rx_dq_head;
    if(q==RX_SW_QUEUE) qID=0; // No matter rx data or event, every client share the same rx queue

    if(((qID >= MAX_TX_DATA_QUEUE)&&(q==TX_SW_QUEUE))||
        ((qID >= MAX_RX_DATA_QUEUE)&&(q==RX_SW_QUEUE))){
        DQ_LOG("Inavlid qID:%d\n",qID);
        return -1;
    }

    if(pbuf==NULL) {
        DQ_LOG("pbuf is a null pointer\n");
        return -1;
    }

    if(!gOsFromISR)
        taskENTER_CRITICAL();

    if(dq_head[qID].int_lock==TRUE){
        DQ_LOG("The intput of dq[%d] is lock\n",qID);
        ret = -1;
        goto END;
    }

    if(0==list_q_len(&fdq_head.qHead)){
        DQ_LOG("no available data_packet\n");
        ret = -1;
        goto END;
    }

    pdp=(struct data_packet *)list_q_deq(&fdq_head.qHead);

    if(pdp==NULL) {
        DQ_LOG("no valid data_packet\n");
        ret = -1;
        goto END;
    }

    pdp->pbuf=pbuf;
    pdp->next_hw=next_hw;
    if(TRUE==bAddToTail){
    list_q_qtail(&dq_head[qID].qHead,&pdp->q);
    }else{
        dq_head[qID].hpPackets++;
        for(i=0;i<dq_head[qID].hpPackets;i++){
            if(i==0)
                _q=&dq_head[qID].qHead;
            else
                _q=_q->next;
        }
        list_q_insert(&dq_head[qID].qHead,_q,&pdp->q);
    }


    if(list_q_len(&dq_head[qID].qHead) > dq_head[qID].max_queue_len)
        dq_head[qID].max_queue_len=list_q_len(&dq_head[qID].qHead);

    dq_head[qID].count++;
    DQ_LOG("qID=%d, enqueue,idx=%d\n",qID,pdp->idx);
END:

    if(!gOsFromISR)
        taskEXIT_CRITICAL();

    return ret;
}
LIB_APIs s32 ssv6xxx_SWQEnqueue(s32 qID, EN_SW_QUEUE q, void *pbuf, s32 next_hw)
{
    #if(PACKET_TRACE==1)
    pt((void *)pbuf,M_ENG_MAX,EN_LOC_ENQUE);
    #endif
    return _ssv6xxx_SWQAdd(qID,q,TRUE,pbuf,next_hw);
}

LIB_APIs s32 ssv6xxx_SWQInsert(s32 qID, EN_SW_QUEUE q, void *pbuf, s32 next_hw)
{
    #if(PACKET_TRACE==1)
    pt((void *)pbuf,M_ENG_MAX,EN_LOC_ENQUE);
    #endif
    return _ssv6xxx_SWQAdd(qID,q,FALSE,pbuf,next_hw);
}
LIB_APIs s32 ssv6xxx_SWQDequeue(s32 qID, EN_SW_QUEUE q, void **pbuf, s32 *next_hw)
{
    s32 ret = 0;
    struct data_queue *dq_head=NULL;
    struct data_packet *pdp=NULL;

    dq_head=(q==TX_SW_QUEUE)?tx_dq_head:rx_dq_head;
    if(q==RX_SW_QUEUE) qID=0; // No matter rx data or event, every client share the same rx queue

    if(((qID >= MAX_TX_DATA_QUEUE)&&(q==TX_SW_QUEUE))||
        ((qID >= MAX_RX_DATA_QUEUE)&&(q==RX_SW_QUEUE))){
        DQ_LOG("Inavlid qID:%d\n",qID);
        return -1;
    }

    if(!gOsFromISR)
        taskENTER_CRITICAL();

    if(dq_head[qID].out_lock==TRUE){
        DQ_LOG("The output of dq[%d] is lock\n",qID);
        ret = -1;
        goto END;
    }

    if(0==list_q_len(&dq_head[qID].qHead)) {
        DQ_LOG("dq[%d] length is zero\n",qID);
        ret = -1;
        goto END;
    }

    pdp=(struct data_packet *)list_q_deq(&dq_head[qID].qHead);
    if(pdp==NULL) {
        *pbuf=NULL;
        DQ_LOG("dq[%d] dequeue fail\n",qID);
        ret = -1;
        goto END;
    }

    *pbuf=pdp->pbuf;
    *next_hw=pdp->next_hw;
    pdp->pbuf=NULL;

    list_q_qtail(&fdq_head.qHead,&pdp->q);

    if(TRUE==dq_head[qID].out_auto_lock){
        dq_head[qID].out_auto_lock_count--;
        if(dq_head[qID].out_auto_lock_count==0){
            ssv6xxx_SWQOutLock(qID,TX_SW_QUEUE);
            dq_head[qID].out_auto_lock=FALSE;
        }
    }

    if(dq_head[qID].hpPackets!=0)
        dq_head[qID].hpPackets--;

    #if(DATA_QUEUE_DEBUG==1)
    DQ_LOG("qID=%d, dequeue,idx=%d,qlen=%d,a.count=%d,out_lock=%d\n",qID,pdp->idx,list_q_len(&dq_head[qID].qHead),dq_head[qID].out_auto_lock_count,dq_head[qID].out_lock);
    #endif

END:
    if(!gOsFromISR)
        taskEXIT_CRITICAL();

    return ret;
}
LIB_APIs s32 ssv6xxx_SWQFlush(s32 qID, EN_SW_QUEUE q, void (*flush_fun)(void *p))
{
    s32 ret = 0;
    struct data_queue *dq_head=NULL;
    struct data_packet *pdp=NULL;
    u32 qlen=0,i=0;

    dq_head=(q==TX_SW_QUEUE)?tx_dq_head:rx_dq_head;
    if(q==RX_SW_QUEUE) qID=0; // No matter rx data or event, every client share the same rx queue

    if(((qID >= MAX_TX_DATA_QUEUE)&&(q==TX_SW_QUEUE))||
        ((qID >= MAX_RX_DATA_QUEUE)&&(q==RX_SW_QUEUE))){
        DQ_LOG("Inavlid qID:%d\n",qID);
        return -1;
    }

    if(!gOsFromISR)
        taskENTER_CRITICAL();

    if((dq_head[qID].out_lock==FALSE)||(dq_head[qID].int_lock==FALSE)){
        DQ_LOG("dq[%d] is not lock\n",qID);
        ret = -1;
        goto END;
    }

    if(flush_fun==NULL){
        DQ_LOG("No available flush function\n");
        ret = -1;
        goto END;
    }
    qlen=list_q_len(&dq_head[qID].qHead);

    if(0==qlen) {
        DQ_LOG("dq[%d] length is zero\n",qID);
        ret = 0;
        goto END;
    }

    for(i=0;i<qlen;i++){
        pdp=(struct data_packet *)list_q_deq(&dq_head[qID].qHead);
        if(pdp!=NULL) {
            flush_fun(pdp->pbuf);
        }
        pdp->pbuf=NULL;
        list_q_qtail(&fdq_head.qHead,&pdp->q);
    }

END:
    if(!gOsFromISR)
        taskEXIT_CRITICAL();

    return ret;
}
LIB_APIs static void _free_packet(void *p)
{
    if(p!=NULL){
        printf("%s\n",__FUNCTION__);
        PBUF_MFree((void *)p);
    }
}
LIB_APIs s32 flush_sw_queue(s32 qID, EN_SW_QUEUE q)
{
   printf("%s\n",__FUNCTION__);
   ssv6xxx_SWQFlush(qID,q,_free_packet);
   //_ssv6xxx_SWQInit(qID);
   return 0;
}

LIB_APIs s32 clear_sw_queue(s32 qID, EN_SW_QUEUE q)
{
    s32 i = 0;
    s32 qlen=0;
    void *pbuf=NULL;
    s32 next_hw=0;
    //printf("clear_data_queue, qID=%d\n",qID);

    ssv6xxx_SWQOutUnLock(qID,q);
    qlen=ssv6xxx_SWQWaittingSize(qID,q);

    if(qlen>=0){
        for(i=0;i<qlen;i++){
            ssv6xxx_SWQDequeue(qID,q,&pbuf,&next_hw);
            if(pbuf!=NULL) PBUF_MFree((void *)pbuf);
        }
        _ssv6xxx_SWQInit(qID,q);
        //ssv6xxx_DataQDestory(qID);
        return 0;
    }else{
        return -1;
    }
}

LIB_APIs s32 ENG_MBOX_NEXT(u32 pktmsg)
{
    PKT_Info *pkt_info=(PKT_Info *)pktmsg;
    u32 shiftbit, eng_id;
    s32 ret;

    // shiftbit = pkt_info->fCmdIdx << 2;
    pkt_info->fCmdIdx ++;
    shiftbit = pkt_info->fCmdIdx << 2;
    eng_id = ((pkt_info->fCmd & (0x0F<<shiftbit))>>shiftbit);
    ret=(0==ssv6xxx_PacketToNextHW((void *)pktmsg,eng_id))?TRUE:FALSE;

    if(ret==FALSE)
    {
        printf("MBOX NEXT fail\n");
    }

    return ret;
}

LIB_APIs s32 ENG_MBOX_SEND(u32 hw_number,u32 pktmsg)
{
    s32 ret=(0==ssv6xxx_PacketToNextHW((void *)pktmsg,hw_number))?TRUE:FALSE;
    if(ret==FALSE)
    {
        printf("MBOX SEND fail\n");
    }

    return ret;
}

LIB_APIs void ssv6xxx_sw_queue_status(void)
{
    int i=0;
    printf("\n---SW Queue Status ---\n");
    printf("freelist: length=%u\n",ssv6xxx_SWFreeQWaittingSize());
    for(i=0;i<MAX_TX_DATA_QUEUE;i++){
        #if 1
        if(i==LAST_TX_DATA_QUEUE)
            printf("mng->TX%d\r\n",g_txQueueMapping[i]);
        else
            printf("wsid(%d)->TX%d\r\n",i,g_txQueueMapping[i]);
        #else
        printf("tx[%d]->TX%d\r\n",i,g_txQueueMapping[i]);
        #endif
        printf("  queue length=%u\n",ssv6xxx_SWQWaittingSize(i,TX_SW_QUEUE));
        printf("  max queue length=%u\n",MAX_TX_DATA_QUEUE_LEN(i));
        printf("  output lock:%s\n",(IS_TX_DATA_QUEUE_OUTPUT_LOCK(i))?"TRUE":"FALSE");
        printf("  intput lock:%s\n",(IS_TX_DATA_QUEUE_INTPUT_LOCK(i))?"TRUE":"FALSE");
        printf("  count:%d\n",IS_TX_DATA_QUEUE_COUNT(i));
    }
    printf("rx[0]:\r\n");
    printf("  queue length=%u\n",ssv6xxx_SWQWaittingSize(0,RX_SW_QUEUE));
    printf("  max queue length=%u\n",MAX_RX_DATA_QUEUE_LEN());
    printf("  output lock:%s\n",(IS_RX_DATA_QUEUE_OUTPUT_LOCK())?"TRUE":"FALSE");
    printf("  intput lock:%s\n",(IS_RX_DATA_QUEUE_INTPUT_LOCK())?"TRUE":"FALSE");
    printf("  count:%d\n",IS_RX_DATA_QUEUE_COUNT());
}

LIB_APIs void ssv6xxx_msg_queue_status(void)
{
    printf("soft-mac msg queue lenth=%d\n",msg_evt_count(MBOX_SOFT_MAC));
    printf("mlme msg queue lenth=%d\n",msg_evt_count(MBOX_MLME));
}
LIB_APIs inline s32 wake_up_task(OsMsgQ msgevq)
{
    u8 res=OS_FAILED;
    //printf("Send wakeup event\n");
    res = msg_evt_post_data1(msgevq, MEVT_WAKEUP_TASK, 0,0,0,0);

    if(res != OS_SUCCESS){
        printf("post wake up event fail. (msg queue len = %d) \n",OS_MsgQWaitingSize(msgevq));
    }
    return res;
}

#if(PACKET_TRACE==1)
s32 pt_init(void)
{
    u8 i=0;
    for(i=0;i<MAX_PT_MAP_SIZE;i++)
    {
        pt_map[i].InFW=0;
        pt_map[i].NextHW=M_ENG_MAX;
        pt_map[i].loc=EN_LOC_NONE;
    }
    return 0;
}

s32 pt(void *_p, u8 next_hw, enum pbuf_location loc)
{
    PKT_Info *p=(PKT_Info *)_p;
    u8 id=PBUF_MapPkttoID(p);

    if(id>MAX_PT_MAP_SIZE)
    {
        printf("%s:id is not valid\n",__func__);
        return -1;
    }

    if((loc==EN_LOC_RECEIVE)||(loc==EN_LOC_ALLOCATE_PBUF))
    {
        pt_map[id].InFW=1;
    }
    else if(loc==EN_LOC_LEAVE_FW)
    {
        pt_map[id].InFW=0;
    }

    pt_map[id].NextHW=next_hw;
    pt_map[id].loc=loc;
    //printf("id=%2d, InFW=%d, NextHW=%d, Location=%d\n",id,footprint[id].InFW,footprint[id].NextHW,footprint[id].loc);



    return 0;
}

int is_pbuf_in_fw(void *_p)
{
    PKT_Info *p=(PKT_Info *)_p;
    u8 id=PBUF_MapPkttoID(p);

    if(id>MAX_PT_MAP_SIZE)
    {
        printf("%s:id is not valid\n",__func__);
        return -1;
    }

    if(pt_map[id].InFW==1)
    {
        return 1;
    }

    return 0;
}

void dump_pt(void)
{
    u8 i=0;
    for(i=0;i<MAX_PT_MAP_SIZE;i++)
    {
        printf("id=%2d, InFW=%d, NextHW=%d, Location=%d\n",
            i,pt_map[i].InFW,pt_map[i].NextHW,pt_map[i].loc);
    }
}
#endif

#define CONSOLE_UART   SSV6XXX_UART0

// return  -1 : fail
//        >=0 : ok
LIB_APIs s32 ssv6xxx_strrpos(const char *str, char delimiter)
{
	const char *p;

	for (p = (str + strlen(str)) - 1; (s32)p>=(s32)str; p--)
	{
		if (*p == delimiter)
			return ((s32)p - (s32)str);
	}

	return -1;	// find no matching delimiter

}

LIB_APIs s32	ssv6xxx_isalpha(s32 c)
{
	if (('A' <= c) && (c <= 'Z'))
		return 1;
	if (('a' <= c) && (c <= 'z'))
		return 1;
	return 0;
}

LIB_APIs s32 ssv6xxx_str_toupper(char *s)
{
	while (*s)
	{
		*s = toupper(*s);
		s++;
	}
	return 0;
}

LIB_APIs s32 ssv6xxx_str_tolower(char *s)
{
	while (*s)
	{
		*s = tolower(*s);
		s++;
	}
	return 0;
}

LIB_APIs u32 ssv6xxx_atoi_base( const char *s, u32 base )
{
    u32  index, upbound=base-1;
    u32  value = 0, v;

    while( (v = (u8)*s) != 0 ) {
        index = v - '0';
        if ( index > 10 && base==16 ) {
            index = (v >= 'a') ? (v - 'a') : (v - 'A');
            index += 10;
        }
        if ( index > upbound )
            break;
        value = value * base + index;
        s++;
    }

    return value;
}

LIB_APIs s32 ssv6xxx_atoi( const char *s )
{
    u32 neg=0, value, base=10;

    if ( *s=='0' ) {
        switch (*++s) {
        case 'x':
        case 'X': base = 16; break;
        case 'b':
        case 'B': base = 2; break;
        default: return 0;
        }
        s++;
    }
    else if ( *s=='-' ) {
        neg = 1;
        s++;
    }

    value = ssv6xxx_atoi_base(s, base);

    if ( neg==1 )
        return -(s32)value;
    return (s32)value;

}


#if (CONFIG_SIM_PLATFORM == 1)
u64 ssv6xxx_64atoi( char *s )
{
    u8 bchar='A', index, upbound=9;
    u32 neg=0, value=0, base=10;

    if ( *s=='0' ) {
        switch (*++s) {
                case 'x': bchar = 'a';
                case 'X': base = 16; upbound = 15; break;
                case 'b':
                case 'B': base = 2; upbound = 1; break;
                default: return 0;
        }
        s++;
    }
    else if ( *s=='-' ) {
        neg = 1;
        s++;
    }

    while( *s ) {
        index = *s - '0';
        if ( base==16 && (*s>=bchar) && (*s<=(bchar+5)) )
        {
                index = 10 + *s - bchar;
        }
        if ( index > upbound )
        {
                break;
        }
        value = value * base + index;
        s++;
    }

    if ( neg==1 )
        return -(s32)value;
    return (u64)value;

}
#endif



#if (CONFIG_SIM_PLATFORM == 0)
LIB_APIs char toupper(char ch)
{
	if (('a' <= ch) && (ch <= 'z'))
		return ('A' + (ch - 'a'));

	// else, make the original ch unchanged
	return ch;
}

LIB_APIs char tolower(char ch)
{
	if (('A' <= ch) && (ch <= 'Z'))
		return ('a' + (ch - 'A'));

	// else, make the original ch unchanged
	return ch;
}


LIB_APIs s32 strcmp( const char *s0, const char *s1 )
{
    s32 c1, c2;

    do {
        c1 = (u8) *s0++;
        c2 = (u8) *s1++;
        if (c1 == '\0')
            return c1 - c2;
    } while (c1 == c2);

    return c1 - c2;
}



LIB_APIs char *strcat(char *s, const char *append)
{
    char *save = s;

    while (*s) { s++; }
    while ((*s++ = *append++) != 0) { }
    return(save);
}



LIB_APIs char *strncpy(char *dst, const char *src, size_t n)
{
    register char *d = dst;
    register const char *s = src;

    if (n != 0) {
        do {
            if ((*d++ = *s++) == 0) {
                /* NUL pad the remaining n-1 bytes */
                while (--n != 0)
                *d++ = 0;
                break;
            }
        } while (--n != 0);
    }
    return (dst);
}


LIB_APIs size_t strlen(const char *s)
{
    const char *ptr = s;
    while (*ptr) ptr++;
    return (size_t)ptr-(size_t)s;
}

LIB_APIs void *memset(void *s, s32 c, size_t n)
{
    if ( NULL != s ) {
		u8 * ps= (u8 *)s;
		const u8 * pes= ps+n;
        while( ps != pes )
			*(ps++) = (u8) c;
    }
    return s;
}


LIB_APIs void *memcpy(void *dest, const void *src, size_t n)
{
    char *d = dest;
    char const *s = src;

    while (n-- > 0)
      *d++ = *s++;
    return dest;
}


LIB_APIs s32 memcmp(const void *s1, const void *s2, size_t n)
{
    const u8 *u1 = (const u8 *)s1, *u2 = (const u8 *)s2;

    while (n--) {
        s32 d = *u1++ - *u2++;
        if (d != 0)
            return d;
    }
    /*
    for ( ; n-- ; s1++, s2++) {
        u1 = *(u8 *)s1;
        u2 = *(u8 *)s2;
        if (u1 != u2)
            return (u1-u2);
    } */
    return 0;
}


LIB_APIs void *malloc(size_t size)
{
    extern u32 g_heap_start;
    extern u32 g_heap_size;
    void *mem=(void *)0;

    /* Make sure the size is the multiple of 4-byte */
    if (size & 0x03)
        size = ((size>>2)+1) << 2;
    if (size <= g_heap_size) {
        mem = (void *)g_heap_start;
        g_heap_start += size;
        g_heap_size  -= size;
#ifdef	BOOT_MALLOC_DEBUG
		sys_init_prnf("Mem Alloc: 0x%08x, %u byte(s)\n", (u32)mem, size);
#endif
    }
    else {
        sys_init_prnf("Out of memory !!\n");
        /*lint -save -e716 */
        while(1) {}
        /*lint -restore */
    }
    return mem;
}



LIB_APIs void free(void *ptr)
{
    sys_init_prnf("Please DO NOT use free() !!\n");
}


extern s32 gOsInFatal;
LIB_APIs s32 fatal_printf(const char *format, ...)
{
   va_list args;
   s32     ret;

   gOsInFatal = 1;
   /*lint -save -e530 */
   va_start( args, format );
   /*lint -restore */
   ret = print( 0, 0, format, args );
   va_end(args);
   return ret;
}
LIB_APIs s32 printf(const char *format, ...)
{
   va_list args;
   s32     ret;
   /*lint -save -e530 */
   va_start( args, format );
   /*lint -restore */
   ret = print( 0, 0, format, args );
   va_end(args);
   return ret;
}

LIB_APIs s32 snprintf(char *out, size_t size, const char *format, ...)
{
    va_list args;
    s32     ret;
    /*lint -save -e530 */
    va_start( args, format ); /*lint -restore */
    ret = print( &out, (out + size - 1), format, args );
    va_end(args);
    return ret;
}

LIB_APIs s32 vsnprintf(char *out, size_t size, const char *format, va_list args)
{
	return print( &out, (out + size - 1), format, args );
}


LIB_APIs s32 putstr (const char *s, size_t len)
{
    return  printstr(0, 0, s, len);
}


LIB_APIs s32 snputstr (char *out, size_t size, const char *s, size_t len)
{
    return  printstr( &out, (out + size - 1), s, len);
}


#endif


#if (CLI_ENABLE==1 && CONFIG_SIM_PLATFORM==0)
LIB_APIs s32 kbhit(void)
{
    return drv_uart_rx_ready(CONSOLE_UART);
}


LIB_APIs s32 getch(void)
{
    return drv_uart_rx(CONSOLE_UART);
}


LIB_APIs s32 putchar(s32 ch)
{
    return drv_uart_tx(CONSOLE_UART, ch);
}
#endif

LIB_APIs void ssv6xxx_hw_queue_status(void)
{
    // 0 - MCU0
	// 1 - HCI
	// 2 -
	// 3 - Security
	// 4 - MRX
	// 5 - MIC
	// 6 - TX 0
	// 7 - TX 1
	// 8 - TX 2
	// 9 - TX 3
	// A - TX MNG
	// B - Security Sec
	// C - MIC Sec
	// D -
	// E -
	// F - Trash Can
    printf("\n---HW Queue Status ---\n");
    printf("[TAG]  MCU - HCI - SEC -  RX - MIC - TX0 - TX1 - TX2 - TX3 - TX4 - RX SEC - RX MIC - TSH\n");
    printf("OUTPUT %3d - %3d - %3d - %3d - %3d - %3d - %3d - %3d - %3d - %3d - %3d   -   %3d -  %3d\n",
            GET_FFO0_CNT, GET_FFO1_CNT, GET_FFO3_CNT, GET_FFO4_CNT, GET_FFO5_CNT, GET_FFO6_CNT,
            GET_FFO7_CNT, GET_FFO8_CNT, GET_FFO9_CNT, GET_FFO10_CNT, GET_FFO11_CNT, GET_FFO12_CNT, GET_FFO15_CNT);
    printf("INPUT  %3d - %3d - %3d - %3d - %3d - %3d - %3d - %3d - %3d - %3d - %3d   -   %3d -  %3d\n",
            GET_FF0_CNT, GET_FF1_CNT, GET_FF3_CNT, GET_FF4_CNT, GET_FF5_CNT, GET_FF6_CNT,
            GET_FF7_CNT, GET_FF8_CNT, GET_FF9_CNT, GET_FF10_CNT, GET_FF11_CNT, GET_FF12_CNT, GET_FF15_CNT);

    printf("PAUSE                                %3d - %3d - %3d - %3d - %3d\n", GET_MTX_HALT_Q_MB&BIT(0),
                                                   (GET_MTX_HALT_Q_MB&BIT(1))>>1,
                                                   (GET_MTX_HALT_Q_MB&BIT(2))>>2,
                                                   (GET_MTX_HALT_Q_MB&BIT(3))>>3,
                                                   (GET_MTX_HALT_Q_MB&BIT(4))>>4);

    //used page
    printf("TX used[%d], RX used[%d], AVA[%d],TX1=[%d]\n",GET_TX_ID_ALC_LEN,GET_RX_ID_ALC_LEN,GET_AVA_TAG,GET_FFO7_CNT);

    //used id
    //printf("TxID[%d]\n", GET_TX_ID_USE_5_0);
    printf("ID  MAP[0x%08x%08x]\n", GET_ID_TB1, GET_ID_TB0);
    printf("TX_ID MAP[0x%08x%08x]\n", GET_TX_ID_TB1, GET_TX_ID_TB0);
    printf("RX_ID MAP[0x%08x%08x]\n", GET_RX_ID_TB1,GET_RX_ID_TB0);



}

LIB_APIs void ssv6xxx_queue_status(void)
{
    ssv6xxx_hw_queue_status();
    ssv6xxx_sw_queue_status();
    ssv6xxx_msg_queue_status();
}

LIB_APIs void ssv6xxx_raw_dump(char *data, s32 len)
{
	ssv6xxx_raw_dump_ex(data, len, true, 10, 10, 16, LOG_LEVEL_ON, LOG_MODULE_EMPTY);
	return;
}


LIB_APIs bool ssv6xxx_raw_dump_ex(char *data, s32 len, bool with_addr, u8 addr_radix, char line_cols, u8 radix, u32 log_level, u32 log_module)
{
    s32 i;

	// check input parameters
	if ((addr_radix != 10) && (addr_radix != 16))
	{
		LOG_ERROR("%s(): invalid value 'addr_radix' = %d\n\r", __FUNCTION__, addr_radix);
		return false;
	}
	if ((line_cols != 8) && (line_cols != 10) && (line_cols != 16) && (line_cols != -1))
	{
		LOG_ERROR("%s(): invalid value 'line_cols' = %d\n\r", __FUNCTION__, line_cols);
		return false;
	}
	if ((radix != 10) && (radix != 16))
	{
		LOG_ERROR("%s(): invalid value 'radix' = %d\n\r", __FUNCTION__, radix);
		return false;
	}

	if (len == 0)	return true;

	// if ONLY have one line
	if (line_cols == -1)
	{
		LOG_TAG_SUPPRESS_ON();
		// only print addr heading at one time
		if ((with_addr == true))
		{
			if      (addr_radix == 10)	printf("%08d: ", 0);
			else if (addr_radix == 16)	printf("0x%08x: ", 0);
		}

		for (i=0; i<len; i++)
		{
			// print data
			if	    (radix == 10)	printf("%4d ",  (u8)data[i]);
			else if (radix == 16)	printf("%02x ", (u8)data[i]);
		}
		printf("\n\r");
		LOG_TAG_SUPPRESS_OFF();
		return true;
	}

	// normal case
	LOG_TAG_SUPPRESS_ON();
    for (i=0; i<len; i++)
	{
		// print addr heading
		if ((with_addr == true) && (i % line_cols) == 0)
		{
			if      (addr_radix == 10)	printf("%08d: ", i);
			else if (addr_radix == 16)	printf("0x%08x: ", i);
		}
		// print data
		if	    (radix == 10)	printf("%4d ",  (u8)data[i]);
		else if (radix == 16)	printf("%02x ", (u8)data[i]);
		// print newline
        if (((i+1) % line_cols) == 0)
            printf("\n\r");
    }
    printf("\n\r");
	LOG_TAG_SUPPRESS_OFF();
	return true;
}

#define ONE_RAW 16
void _packetdump(const char *title, const u8 *buf,
                             size_t len)
{
    size_t i;
    printf("%s - hexdump(len=%d):\n    ", title, (unsigned long) len);
    if (buf == NULL) {
        printf(" [NULL]");
    }else{


        for (i = 0; i < ONE_RAW; i++)
            printf("%02X ", i);

        printf("\n---\n00|");


        for (i = 0; i < len; i++){
            printf(" %02x", buf[i]);
            if((i+1)%ONE_RAW ==0)
                printf("\n%02x|", (i+1));
        }
    }
    printf("\n-----------------------------\n");
}



void pkt_dump_txinfo(PKT_TxInfo *p)
{
	LOG_PRINTF("========= TxInfo =========\n\r");
	LOG_PRINTF("%20s : %d\n\r", "len", 		p->len);
	LOG_PRINTF("%20s : %d\n\r", "c_type",		p->c_type);
	LOG_PRINTF("%20s : %d\n\r", "f80211",		p->f80211);
	LOG_PRINTF("%20s : %d\n\r", "qos",			p->qos);
	LOG_PRINTF("%20s : %d\n\r", "ht",			p->ht);
	LOG_PRINTF("%20s : %d\n\r", "use_4addr",	p->use_4addr);
	LOG_PRINTF("%20s : %d\n\r", "RSVD_0",		p->RSVD_0);
	LOG_PRINTF("%20s : %d\n\r", "bc_que",		p->bc_que);
	LOG_PRINTF("%20s : %d\n\r", "security",	p->security);
	LOG_PRINTF("%20s : %d\n\r", "more_data",	p->more_data);
	LOG_PRINTF("%20s : %d\n\r", "stype_b5b4",	p->stype_b5b4);
	LOG_PRINTF("%20s : %d\n\r", "extra_info",	p->extra_info);
	LOG_PRINTF("%20s : 0x%08x\n\r", "fCmd",	p->fCmd);
	LOG_PRINTF("%20s : %d\n\r", "hdr_offset",	p->hdr_offset);
	LOG_PRINTF("%20s : %d\n\r", "frag",		p->frag);
	LOG_PRINTF("%20s : %d\n\r", "unicast",		p->unicast);
	LOG_PRINTF("%20s : %d\n\r", "hdr_len",		p->hdr_len);
	LOG_PRINTF("%20s : %d\n\r", "tx_report",	p->tx_report);
	LOG_PRINTF("%20s : %d\n\r", "tx_burst",	p->tx_burst);
	LOG_PRINTF("%20s : %d\n\r", "ack_policy",  p->ack_policy);
	LOG_PRINTF("%20s : %d\n\r", "RSVD_1",		p->RSVD_1);
	LOG_PRINTF("%20s : %d\n\r", "do_rts_cts",  p->do_rts_cts);
	LOG_PRINTF("%20s : %d\n\r", "reason",		p->reason);
	LOG_PRINTF("%20s : %d\n\r", "payload_offset",	p->payload_offset);
	LOG_PRINTF("%20s : %d\n\r", "next_frag_pid",	p->next_frag_pid);
	LOG_PRINTF("%20s : %d\n\r", "RSVD_2",		p->RSVD_2);
	LOG_PRINTF("%20s : %d\n\r", "fCmdIdx",		p->fCmdIdx);
	LOG_PRINTF("%20s : %d\n\r", "wsid",		p->wsid);
	LOG_PRINTF("%20s : %d\n\r", "txq_idx",		p->txq_idx);
	LOG_PRINTF("%20s : %d\n\r", "TxF_ID",		p->TxF_ID);
	LOG_PRINTF("%20s : %d\n\r", "rts_cts_nav",	p->rts_cts_nav);
	LOG_PRINTF("%20s : %d\n\r", "frame_consume_time",	p->frame_consume_time);
	LOG_PRINTF("%20s : %d\n\r", "RSVD_3",		p->RSVD_3);
	// LOG_PRINTF("%20s : %d\n\r", "RSVD_5",		p->RSVD_5);
	LOG_PRINTF("============================\n\r");
	LOG_PRINTF("%20s : %d\n\r", "payload_len", p->len - p->hdr_len);
    #undef dat
	#define dat ((u8 *)p + p->hdr_offset)
	LOG_PRINTF("========== hdr     ==========\n\r");
	LOG_PRINTF("frame ctl     : 0x%04x\n\r", (((u16)dat[1] << 8)|dat[0]));
	LOG_PRINTF("  - more_frag : %d\n\r", GET_HDR80211_FC_MOREFRAG(p));

    #undef a
	#define a   ((u8*)p + p->hdr_offset +  4)
	LOG_PRINTF("address 1     : %02x:%02x:%02x:%02x:%02x:%02x\n\r", a[0], a[1], a[2], a[3], a[4], a[5]);
	LOG_PRINTF("address 2     : %02x:%02x:%02x:%02x:%02x:%02x\n\r", a[6], a[7], a[8], a[9], a[10], a[11]);
	LOG_PRINTF("address 3     : %02x:%02x:%02x:%02x:%02x:%02x\n\r", a[12], a[13], a[14], a[15], a[16], a[17]);

	LOG_PRINTF("seq ctl       : 0x%04x\n\r", (((u16)dat[23] << 8)|dat[22]));
	LOG_PRINTF("  - seq num   : %d\n\r", GET_HDR80211_SC_SEQNUM(p));
	LOG_PRINTF("  - frag num  : %d\n\r", GET_HDR80211_SC_FRAGNUM(p));
    #undef data
    #undef a
	return;
}

void pkt_dump_rxinfo(PKT_RxInfo *p)
{
    LOG_PRINTF("========= RxInfo =========\n\r");
	LOG_PRINTF("%20s : %d\n\r", "len",	        p->len);
	LOG_PRINTF("%20s : %d\n\r", "c_type",	    p->c_type);
	LOG_PRINTF("%20s : %d\n\r", "f80211",	    p->f80211);
	LOG_PRINTF("%20s : %d\n\r", "qos",	        p->qos);
	LOG_PRINTF("%20s : %d\n\r", "ht",		    p->ht);
	LOG_PRINTF("%20s : %d\n\r", "l3cs_err",	p->l3cs_err);
	LOG_PRINTF("%20s : %d\n\r", "l4cs_err",	p->l4cs_err);
	LOG_PRINTF("%20s : %d\n\r", "use_4addr",	p->use_4addr);
	LOG_PRINTF("%20s : %d\n\r", "RSVD_0",		p->RSVD_0);
	LOG_PRINTF("%20s : %d\n\r", "psm",	        p->psm);
	LOG_PRINTF("%20s : %d\n\r", "stype_b5b4",	p->stype_b5b4);
	LOG_PRINTF("%20s : %d\n\r", "extra_info",	p->extra_info);
	LOG_PRINTF("%20s : 0x%08x\n\r", "fCmd",	p->fCmd);
	LOG_PRINTF("%20s : %d\n\r", "hdr_offset",	p->hdr_offset);
	LOG_PRINTF("%20s : %d\n\r", "frag",	    p->frag);
	LOG_PRINTF("%20s : %d\n\r", "unicast",	    p->unicast);
	LOG_PRINTF("%20s : %d\n\r", "hdr_len",	    p->hdr_len);
	LOG_PRINTF("%20s : 0x%x\n\r", "RxResult",	p->RxResult);
	LOG_PRINTF("%20s : %d\n\r", "wildcard_bssid",	p->wildcard_bssid);
	LOG_PRINTF("%20s : %d\n\r", "RSVD_1",	    p->RSVD_1);
	LOG_PRINTF("%20s : %d\n\r", "reason",	    p->reason);
	LOG_PRINTF("%20s : %d\n\r", "payload_offset",	p->payload_offset);
	LOG_PRINTF("%20s : %d\n\r", "next_frag_pid",	p->next_frag_pid);
	LOG_PRINTF("%20s : %d\n\r", "RSVD_2",	    p->RSVD_2);
	LOG_PRINTF("%20s : %d\n\r", "fCmdIdx",	    p->fCmdIdx);
	LOG_PRINTF("%20s : %d\n\r", "wsid",	    p->wsid);
	LOG_PRINTF("%20s : %d\n\r", "RSVD_3",	    p->RSVD_3);
//	LOG_PRINTF("%20s : %d\n\r", "RxF_ID",	    p->RxF_ID);
	LOG_PRINTF("============================\n\r");

	LOG_PRINTF("%20s : %d\n\r", "payload_len", p->len - p->hdr_len);
    #undef data
    #define data ((u8 *)p + p->hdr_offset)
	LOG_PRINTF("========== hdr     ==========\n\r");
	LOG_PRINTF("frame ctl     : 0x%04x\n\r", (((u16)dat[1] << 8)|dat[0]));
	LOG_PRINTF("  - more_frag : %d\n\r", GET_HDR80211_FC_MOREFRAG(p));
    #undef a
    #define a ((u8*)p + p->hdr_offset +  4)
	LOG_PRINTF("address 1     : %02x:%02x:%02x:%02x:%02x:%02x\n\r", a[0], a[1], a[2], a[3], a[4], a[5]);
	LOG_PRINTF("address 2     : %02x:%02x:%02x:%02x:%02x:%02x\n\r", a[6], a[7], a[8], a[9], a[10], a[11]);
	LOG_PRINTF("address 3     : %02x:%02x:%02x:%02x:%02x:%02x\n\r", a[12], a[13], a[14], a[15], a[16], a[17]);

	LOG_PRINTF("seq ctl       : 0x%04x\n\r", (((u16)dat[23] << 8)|dat[22]));
	LOG_PRINTF("  - seq num   : %d\n\r", GET_HDR80211_SC_SEQNUM(p));
	LOG_PRINTF("  - frag num  : %d\n\r", GET_HDR80211_SC_FRAGNUM(p));
    #undef data
    #undef a
	return;

}





LIB_APIs void hex_dump (const void *addr, u32 size)
{

    u32 i, j;
    const u32 *data = (const u32 *)addr;

    LOG_TAG_SUPPRESS_ON();
    printf("        ");
    for (i = 0; i < 8; i++)
        printf("       %02X", i*sizeof(u32));

    printf("\n--------");
    for (i = 0; i < 8; i++)
        printf("+--------");

    for (i = 0; i < size; i+= 8)
    {
        printf("\n%08X:%08X", (s32)data, data[0]);
        for (j = 1; j < 8; j++)
        {
            printf(" %08X", data[j]);
        }
        data = &data[8];
    }
    printf("\n");
    LOG_TAG_SUPPRESS_OFF();

    return;
}

LIB_APIs void halt(void)
{
#if (CONFIG_SIM_PLATFORM)
	system("pause");
	exit(EXIT_FAILURE);
#else
	/*lint -save -e716 */
    while (1) ;
	/*lint -restore */
#endif
}


