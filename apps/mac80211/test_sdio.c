#include <config.h>
#include <msgevt.h>
#include <pbuf.h>
#include <hdr80211.h>
#include <cmd_def.h>
#include <rtos.h>
#include <lib/ssv_lib.h>
#include <log.h>
#include <driver/mbox/drv_mbox.h>
#include <driver/dma/drv_pkt_dma.h>
#include <dbg_timer/dbg_timer.h>
#include <hwmac/drv_mac.h>
#include <ethertype.h>
#include <random/drv_random.h>


#ifdef THROUGHPUT_TEST
    u32 soc_tx_speed =0;
    u32 soc_rx_speed = 0;
    
    u16 soc_qos = 0;
    u8 soc_noack = 0;
    bool soc_tx_done = false;
#ifdef ENABLE_DEBUG_TIMER
	u32 throughput;
#endif

extern s32 drv_mailbox_cpu_ff(u32 pktmsg, u32 hw_number);

u32 soc_sdio_tx_count = 0;
volatile u8 txTestingMode=0;

void send_rx_data_frame(u32 size,u32 loopTimes,u32 moduleId)
{
	u32 i = 0,rawIndex = 0,allocate_fail=0,sent_fail=0;
	PKT_RxInfo *NewPkt=NULL,*TempPkt=NULL;	
	u8	randomLengthMode = 0;

	printf("SDIO testing RX Frame size[%d] loopTimes[%d]\n",size,loopTimes);

	if(size == 9999)
		randomLengthMode = 1;

	TempPkt = (PKT_RxInfo *)PBUF_MAlloc(140, SOC_PBUF);
	{
		u8 raw[]={	
		0x3c, 0x00,0x0b,0x00, 0x34, 0x50, 0x01, 0x00, 0x50, 0x60, 0x2b ,0x00, 0x68, 0x00, 0x03, 0x00, 
		0x50, 0x00,0x00,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 ,0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00,0x00,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 ,0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00,0x00,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 ,0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00,0x00,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 ,0x00, 0x00, 0x00, 0x00, 0x00, 
		0x08, 0x42,0x00,0x00, 0x84 ,0xc9, 0xb2, 0x05, 0xbd, 0xde, 0x84 ,0xc9, 0xb2, 0x05, 0xbd, 0xde, 
		0x84, 0xc9,0xb2,0x05, 0xbd, 0xdc, 0x60, 0xa1, 0xaa, 0xaa, 0x03 ,0x00, 0x00, 0x00, 0x08, 0x06, 
		0x00, 0x01,0x08,0x00, 0x06, 0x04, 0x00, 0x01, 0x84, 0xc9, 0xb2 ,0x05, 0xbd, 0xdc, 0xc0, 0xa8, 
		0x0a, 0x01,0x00,0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xa8, 0x0a ,0x7e 	 };
		memcpy(TempPkt,raw,sizeof(raw));
		rawIndex = sizeof(raw);
	}

	TempPkt->len = size;

	soc_sdio_tx_count = 0;
	for(i=1;;i++)
	{
		if(randomLengthMode)
		{
			drv_get_random((u8*)&size,4);
			size = size % 2240 + 64;
			TempPkt->len = size;
		}

		do
		{	
			NewPkt = (PKT_RxInfo *)PBUF_MAlloc(size, SOC_PBUF);
			if(NewPkt == NULL)
            {
                OS_MsDelay(10);
				allocate_fail++;
            }
		}
		while(NewPkt == NULL);

		drv_pkt_memcpy(NewPkt,TempPkt,rawIndex);

		{
			u8* TempPtr = (u8 *)NewPkt;
			if(moduleId == M_ENG_MIC)
			{
				TempPtr[14] =0x03;
			}
			else if(moduleId == M_ENG_ENCRYPT)
			{
				TempPtr[14] =0x01;	
			}
			else if(moduleId == M_ENG_HWHCI)
			{
				TempPtr[14] =0x04;
			}
		}
		
		while(drv_mailbox_cpu_ff((u32)NewPkt,moduleId) == FALSE)
		{
			OS_MsDelay(10);
			sent_fail++; 
		}

		soc_sdio_tx_count = (u32)i;
		NewPkt = NULL;
		if(loopTimes!=0)
		{
			if(i==loopTimes)
			{
				LOG_DEBUG( "[Allocate fail=%d Send fail=%d]\n",allocate_fail,sent_fail);
				if(TempPkt != NULL)
					PBUF_MFree(TempPkt);
				return;
			} 
		}
		else
		{
			LOG_DEBUG( "Error loopTimes=0\n");
			PBUF_MFree(TempPkt);
			return;
		}
	}
}

static void throughputGetRate(u8 rateIndex ,size_t buffer_size, char * pointer)
{
	switch(rateIndex)
	{
		case 0:
			snprintf(pointer, buffer_size, "1Mbps");
			return;
		case 1:
		case 4:
			snprintf(pointer, buffer_size, "2Mbps");
			return;
		case 2:
		case 5:
			snprintf(pointer, buffer_size, "5.5Mbps");
			return;
		case 3:
		case 6:
			snprintf(pointer, buffer_size, "11Mbps");
			return;
		case 7:
			snprintf(pointer, buffer_size, "6Mbps");
			return;
		case 8:
			snprintf(pointer, buffer_size, "9Mbps");
			return;
		case 9:
			snprintf(pointer, buffer_size, "12Mbps");
			return;
		case 10:
			snprintf(pointer, buffer_size, "18Mbps");
			return;
		case 11:
			snprintf(pointer, buffer_size, "24Mbps");
			return;
		case 12:
			snprintf(pointer, buffer_size, "36Mbps");
			return;
		case 13:
			snprintf(pointer, buffer_size, "48Mbps");
			return;
		case 14:
			snprintf(pointer, buffer_size, "54Mbps");
			return;
		case 15:
		case 31:
			snprintf(pointer, buffer_size, "6.5Mbps");
			return;
		case 16:
		case 32:
			snprintf(pointer, buffer_size, "13Mbps");
			return;
		case 17:
		case 33:
			snprintf(pointer, buffer_size, "19.5Mbps");
			return;
		case 18:
		case 34:
			snprintf(pointer, buffer_size, "26Mbps");
			return;
		case 19:
		case 35:
			snprintf(pointer, buffer_size, "39Mbps");
			return;
		case 20:
		case 36:
			snprintf(pointer, buffer_size, "52Mbps");
			return;
		case 21:
		case 37:
			snprintf(pointer, buffer_size, "58.5Mbps");
			return;
		case 22:
		case 38:
			snprintf(pointer, buffer_size, "65Mbps");
			return;
		case 23:
			snprintf(pointer, buffer_size, "7.2Mbps");
			return;
		case 24:
			snprintf(pointer, buffer_size, "14.4Mbps");
			return;
		case 25:
			snprintf(pointer, buffer_size, "21.7Mbps");
			return;
		case 26:
			snprintf(pointer, buffer_size, "28.9Mbps");
			return;
		case 27:
			snprintf(pointer, buffer_size, "43.3Mbps");
			return;
		case 28:
			snprintf(pointer, buffer_size, "57.8Mbps");
			return;
		case 29:
			snprintf(pointer, buffer_size, "65Mbps");
			return;
		case 30:
			snprintf(pointer, buffer_size, "72.2Mbps");
			return;
		default:
			snprintf(pointer, buffer_size, "0Mbps");
			return;
	};
}

#ifdef ENABLE_DEBUG_TIMER
	Time_T pre_t;
#endif
static u32 reportLength = 0;
static u32 preFrame = 0;
extern u8 on_fire_flag;
void txThroughputHandler()
{
    u32 time_offset,throughput,temp,currr;

    if(txTestingMode)
    {
#ifdef ENABLE_DEBUG_TIMER
        time_offset = dbg_get_elapsed(&pre_t);
        dbg_get_time(&pre_t);
        throughput = reportLength;
		if(on_fire_flag == 0)
        	currr = GET_MTX_WSID0_SUCC;
		else
		{
        	currr = GET_MTX_WSID0_FRM;
        	printf("[%d][%d][%d]",(u32)GET_FFO6_CNT,(u32)GET_FFO7_CNT,(u32)GET_AVA_TAG);
		}

        temp = currr - preFrame;
        throughput *= temp;
        preFrame = currr;
        if(throughput < 0x1fffff)
            throughput = ((throughput * 1000)/(time_offset/1000))/1000;
        else
            throughput = throughput/(time_offset/1000);
        printf("%d\n",throughput<<3);
#endif
    }

    return;
}

static PKT_TxInfo *sourcePkt=NULL;
static u8 reportMode=0;

void throughput_tx_handle(THROUGHPUT_COMMAND *command)
{
#ifdef ENABLE_DEBUG_TIMER
	Time_T begin_t;
	u32 time_offset,throughput;
#endif
	PKT_TxInfo *TxPkt;
	HDR8023_Data *p8023data;
	u32 allocate_fail=0,sent_fail=0,copyLen,repeatCount;
	u16 proto = ETHERTYPE_IP;
	char string[24];

	//Reset counter
	SET_G0_PKT_CLS_MIB_EN(0);
	SET_G0_PKT_CLS_MIB_EN(1);
	if(reportMode == 0)
	{
		printf("*************************************************\n");
		printf("transferCount[%d] transferLength[%d] rateIndex[%d]\n",
				command->transferCount,command->transferLength,command->rateIndex);
	}
	if((command->transferCount == 0) || (command->transferLength <50) || (command->transferLength >65535))
	{
		LOG_DEBUG("Error parameters !!\n");
		return;
	}

	if(sourcePkt == NULL)
	{
		sourcePkt = (PKT_TxInfo *)PBUF_MAlloc(140 ,SOC_PBUF);
		p8023data = (HDR8023_Data *)((u8 *)sourcePkt + drv_mac_get_pbuf_offset());
		{
			drv_mac_get_sta_mac(p8023data->src.addr);
            if(command->noack == 0)
			{
    			drv_mac_get_wsid_peer_mac(0,&(p8023data->dest));
			}
            else
            {
                int i=0;
                for(i=0;i<ETHER_ADDR_LEN;i++)
                    p8023data->dest.addr[i] =0xff;
            }
		}
		p8023data->protocol = (proto>>8) | (proto<<8);
	}
	if(reportMode == 0)
	{
		printf("MMU avabible page number[%d]\n",(u32)GET_AVA_TAG);
		printf("RX miss count[%d]\n",(u32)GET_MRX_MISS);
		printf("GET_ID_TB0[%08x] GET_ID_TB1[%08x]\n\n",(u32)GET_ID_TB0,(u32)GET_ID_TB1);
	}
	//size = ETHER_HDR_LEN + command->transferLength - 50;
	/* Fill TxPkt descriptor */
	sourcePkt->len		= 
	command->transferLength-(sizeof(HDR80211_Data)+LLC_HEADER_LEN+ETHER_TYPE_LEN-ETHER_HDR_LEN-FCS_LEN);
	sourcePkt->c_type	= M0_TXREQ;
	sourcePkt->f80211	= 0;
	sourcePkt->qos		= (command->qos==0?0:1);
	sourcePkt->ht		= 0;
	sourcePkt->use_4addr = 0;
	sourcePkt->more_data = 0;
	sourcePkt->security = 0;
	
	sourcePkt->drate_idx = command->rateIndex;
	sourcePkt->crate_idx = command->rateIndex;

    *((u8 *)sourcePkt+(drv_mac_get_pbuf_offset()-2)) = (command->qos>>8);
    *((u8 *)sourcePkt+(drv_mac_get_pbuf_offset()-1)) = (command->qos&0x0ff);
    copyLen = drv_mac_get_pbuf_offset() + ETHER_HDR_LEN;
    
    

#ifdef ENABLE_DEBUG_TIMER
    dbg_timer_init();
    dbg_get_time(&begin_t);
	dbg_get_time(&pre_t);
#endif

	reportLength = command->transferLength;
	txTestingMode = 1;
	preFrame=0;
	
	repeatCount = command->transferCount;
    soc_tx_done = false;
	while(repeatCount--)
	{
		do
		{	
			TxPkt = (PKT_TxInfo *)PBUF_MAlloc(sourcePkt->len, SOC_PBUF);
			if(TxPkt == NULL)
            {
                OS_MsDelay(10);
				allocate_fail++;
            }
		}
		while(TxPkt == NULL);

		drv_pkt_memcpy(TxPkt,sourcePkt,copyLen);
#if 0
		p8023data = (HDR8023_Data *)((u8 *)TxPkt + drv_mac_get_pbuf_offset());
		*((u16 *)&p8023data[1]) = repeatCount;
#endif
		while(drv_mailbox_cpu_ff((u32)TxPkt,M_ENG_HWHCI) == FALSE)
		{
			sent_fail++; 
		}
	}
    soc_tx_done = true;
	do{
		;
	}while((GET_TX_ID_COUNT==0) && (GET_TX_ID_ALC_LEN == 0));

	txTestingMode = 0;
#ifdef ENABLE_DEBUG_TIMER
	time_offset = dbg_get_elapsed(&begin_t);
	throughputGetRate(command->rateIndex,24,string);
	//50 hot code (802.3 transfer to 802.11)
	//throughput = command->transferLength;
	//throughput *= command->transferCount;
	//LOG_PRINTF("[%s]rate [%d]microseconds send [%d]Bytes\n",string,time_offset,throughput);
	OS_MsDelay(100);
	throughput = command->transferLength;
	throughput *= GET_MTX_WSID0_SUCC;
	//throughput *= 8;
	if(throughput < 0x1fffff)
		throughput = ((throughput * 1000)/(time_offset/1000))/1000;
	else
		throughput = throughput/(time_offset/1000);
	if(reportMode == 0)
	{	
	 	soc_tx_speed = (throughput<<3); 
	    printf("[%s]rate throughput [%d]Mb per 1000 secnonds \n\n",string,soc_tx_speed);
       
    }
#else
	OS_MsDelay(100);
#endif

	if(reportMode == 0)
	{
		printf("Tx Done -- Reallocate[%d] Resend-HCI[%d] \n",allocate_fail,sent_fail);
		printf("MMU avabible page number[%d]\n",(u32)GET_AVA_TAG);
		printf("RX miss count[%d]\n",(u32)GET_MRX_MISS);
		printf("GET_ID_TB0[%08x] GET_ID_TB1[%08x]\n",(u32)GET_ID_TB0,(u32)GET_ID_TB1);
		printf("	Tx WSID-0 success	:%d\n", GET_MTX_WSID0_SUCC);
		printf("	Tx WSID-0 frame 	:%d\n", GET_MTX_WSID0_FRM);
		printf("	Tx WSID-0 retry 	:%d\n", GET_MTX_WSID0_RETRY);
		printf("	Tx WSID-0 Total 	:%d\n\n", GET_MTX_WSID0_TOTAL);
	}
	else
	{
		copyLen = GET_MTX_WSID0_TOTAL;
		repeatCount = GET_MTX_WSID0_SUCC*1000;

		throughputGetRate(command->rateIndex,24,string);
#ifdef ENABLE_DEBUG_TIMER
		printf("%02d-%08s %5d %5d %5d %5d %5d %5d %d\n",command->rateIndex,
			string,GET_MTX_WSID0_SUCC,GET_MTX_WSID0_FRM,GET_MTX_WSID0_RETRY,GET_MTX_WSID0_TOTAL,(repeatCount/copyLen)/10,throughput<<3,time_offset/1000);
#endif
	}

	return ;
}

extern u16	rxTimeStampOffset;
extern u8	throughputFixRate;
extern u8	throughputDutMode;
extern u32	soc_mac_rx_count;
u32  soc_rx_rssi;
u32  soc_rx_snr;
extern u8	soc_rx_verify_mode;



void Testing_Task( void *args )
{
	MsgEvent *MsgEv=NULL;
	THROUGHPUT_COMMAND *command;
	SDIO_THROUGHPUT_COMMAND *sdioCommand;
	s32 res;
	u32 temp,count,lenth[3],start,end,repeat;
	char string[24];

	//LOG_DEBUG( "%s() Task started.\n", __FUNCTION__);
    /*lint -save -e716 */
	SET_MRX_STP_EN(1);
	rxTimeStampOffset = GET_MRX_STP_OFST;
    while(1)
    { /*lint -restore */
        /* Wait Message: */
        res = msg_evt_fetch(MBOX_TESTINGS,MAX_DELAY, &MsgEv);
        ASSERT(res == OS_SUCCESS);
		switch(MsgEv->MsgType)            
		{
			case MEVT_SDIO_TEST_CMD:
				{
					sdioCommand = (SDIO_THROUGHPUT_COMMAND *)&MsgEv->MsgData;
					send_rx_data_frame(sdioCommand->transferLength,sdioCommand->transferCount,MsgEv->MsgData3);
				}
				break;
			case MEVT_THROUGHPUT:
				command = (THROUGHPUT_COMMAND *)&MsgEv->MsgData;
				if(command->mode == 1)
				{
					throughputDutMode = 1;
					if(command->rateIndex > 39)
					{
						lenth[0]=64;
						lenth[1]=1536;
						lenth[2]=2303;
						switch(command->rateIndex)
						{
							case 40:
								start = 0;
								end = 39;
								repeat = 3;
								break;
							case 41:
								start = 0;
								end = 7;
								repeat = 3;
								break;
							case 42:
								start = 0;
								end = 15;
								repeat = 3;
								break;
							case 43:
								start = 0;
								end = 31;
								repeat = 3;
								break;
							case 44:
								start = 7;
								end = 15;
								repeat = 3;
								break;
							case 45:
								start = 31;
								end = 39;
								repeat = 3;
								break;
							case 46:
								start = 0;
								end = 7;
								repeat = 1;
								break;
							case 47:
								start = 7;
								end = 15;
								repeat = 1;
								break;
							case 48:
								start = 15;
								end = 39;
								repeat = 1;
								break;
							default:
								start = 0;
								end = 0;
								repeat = 0;
								LOG_PRINTF("Error parameter\n");
								break;
						}
						for(count=0;count<repeat;count++)
						{
							if(repeat == 3)
								command->transferLength = lenth[count];
							else
								command->transferLength = command->transferLength;

							LOG_PRINTF("[%d]bytes Success Frames Retry Total Percentage Throughput TimeOffset\n",
								command->transferLength);
							for(temp=start;temp<end;temp++)
							{
								reportMode = 1;
								command->rateIndex=temp;
								throughput_tx_handle(command);
							}
						}
						LOG_PRINTF("[Report Done!!]\n");
					}
					else if(command->rateIndex == 39)
					{
						count = command->transferCount;
						for(temp=0;temp<count;temp++)
						{
							reportMode = 1;
							command->rateIndex=temp % 39;
							command->transferCount=1;
							throughput_tx_handle(command);
						}
					}
					else
					{
						reportMode = 0;
						throughput_tx_handle(command);
					}
					throughputDutMode = 0;
				}
				else if(command->mode == 0)
				{
					throughputGetRate(command->rateIndex,24,string);
					printf("Setting fix rate[%s]\n",string);
					throughputFixRate = command->rateIndex;
				}
				else if(command->mode == 2)
				{
					//Backoff off
					if(command->rateIndex == 0)
					{
						printf("Backoff off!!\n");
						SET_TXQ0_MTX_Q_BKF_CNT_FIXED(1);
						SET_TXQ0_MTX_Q_ECWMIN(0);
						SET_TXQ0_MTX_Q_ECWMAX(0);
						SET_TXQ1_MTX_Q_BKF_CNT_FIXED(1);
						SET_TXQ1_MTX_Q_ECWMIN(0);
						SET_TXQ1_MTX_Q_ECWMAX(0);
						SET_TXQ2_MTX_Q_BKF_CNT_FIXED(1);
						SET_TXQ2_MTX_Q_ECWMIN(0);
						SET_TXQ2_MTX_Q_ECWMAX(0);
						SET_TXQ3_MTX_Q_BKF_CNT_FIXED(1);
						SET_TXQ3_MTX_Q_ECWMIN(0);
						SET_TXQ3_MTX_Q_ECWMAX(0);
						SET_TXQ4_MTX_Q_BKF_CNT_FIXED(1);
						SET_TXQ4_MTX_Q_ECWMIN(0);
						SET_TXQ4_MTX_Q_ECWMAX(0);
					}
					//Backoff on
					else
					{
						printf("Backoff on!!\n");
						SET_TXQ0_MTX_Q_BKF_CNT_FIXED(0);
						SET_TXQ0_MTX_Q_ECWMIN(5);
						SET_TXQ0_MTX_Q_ECWMAX(0xa);
						SET_TXQ1_MTX_Q_BKF_CNT_FIXED(0);
						SET_TXQ1_MTX_Q_ECWMIN(5);
						SET_TXQ1_MTX_Q_ECWMAX(0xa);
						SET_TXQ2_MTX_Q_BKF_CNT_FIXED(0);
						SET_TXQ2_MTX_Q_ECWMIN(5);
						SET_TXQ2_MTX_Q_ECWMAX(0xa);
						SET_TXQ3_MTX_Q_BKF_CNT_FIXED(0);
						SET_TXQ3_MTX_Q_ECWMIN(5);
						SET_TXQ3_MTX_Q_ECWMAX(0xa);
						SET_TXQ4_MTX_Q_BKF_CNT_FIXED(0);
						SET_TXQ4_MTX_Q_ECWMIN(5);
						SET_TXQ4_MTX_Q_ECWMAX(0xa);
					}
				}
				else if(command->mode == 3)
				{
					//turn off
					if(command->rateIndex == 0)
					{
						printf("RX off!!\n");                        
						soc_rx_verify_mode = 0;
					}
					//turn on
					else
					{
						printf("RX on!!\n");
						soc_rx_verify_mode = 1;
						soc_mac_rx_count = 0;
                        soc_rx_rssi=0;
                        soc_rx_snr = 0;
					}
				}
				else if(command->mode == 4) //for qos
				{
					soc_qos = command->qos;
                    soc_noack = command->noack;
				}                
               
				break;
			default:
				LOG_DEBUG( "%s() default\n", __FUNCTION__);
				break;
		}
        /*lint -restore */
		msg_evt_free(MsgEv);
    }
	return;

}

#endif

