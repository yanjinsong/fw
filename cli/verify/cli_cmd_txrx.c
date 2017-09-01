#include <config.h>
#include <bsp/irq.h>
#include <rtc/drv_rtc.h>
#include <mbox/drv_mbox.h>
#include <timer/drv_timer.h>


#include <lib/ssv_lib.h>
#include "types.h"
#include "cli.h"

#include <pbuf.h>


#define TXRX_FPGA_TEST 0

#if ( TXRX_FPGA_TEST==1 )
#define TXRX_DAT_OFFSET 0x40
#define TXRX_DAT_COUNT 180
#define TXRX_DAT_SIZE 1792
#define TXRX_DAT_BLOCK 6



#define IRQ_RX				                      19



typedef u32 (*CliCmdTxRxTestFunc)(s32, char **);
typedef struct CLICmdTxRxTest_st {
	char *Cmd;
	CliCmdTxRxTestFunc CmdTxRxTestHandler;
} CLICmdTxRxTest, *PCLICmdTxRxTest;
static const CLICmdTxRxTest gCliCmdTxRxTestTable[];

u32 testCmdTableSize;
u8 TxRxSecIntStatus;
u8 TxRxAlarmIntStatus;

bool isCancelTx;
u32 txSeq;
u32 txDataCount;
u32 txDataDateRate;

bool isCancelRx;
u32 rxSeq;
u32 rxDataCount;



static void rx_int_handler(void *m_data)
{
    while(REG_MAILBOX->CPU_FLG&CPU_INT)
    {

        u32* pPktAddress;
        u32* pPktHeader;
        //u8* pPktData;
        u32 rxSeqTmp;

        pPktAddress = (u32*)REG_MAILBOX->CPU_FF;
#if 1

        pPktHeader = (u32*)(((u8 *)pPktAddress)+TXRX_DAT_OFFSET);
        //pPktData = (u8*)(((u8 *)pPktAddress)+TXRX_DAT_OFFSET+24);

        rxSeqTmp = (*(pPktHeader+5) >> (16 + 4))&0xffff;

        //log_printf("rx_int_handler  [%08x] [%08x] [%d]\n",pPktHeader+5,*(pPktHeader+5),rxSeqTmp);


        if ( rxSeqTmp < rxSeq )
        {
            log_printf("rx_int_handler duplicate --> drop frame [%d]\n",rxSeqTmp);
        }
        else
        {
            log_printf("rx_int_handler get frame [%d/%d]\n",rxSeqTmp,rxDataCount);
            if ( rxSeqTmp == 1 )
            {
                MS_TIMER1->TMR_CTRL = 0xffff;
            }
            else if ( rxSeqTmp == rxDataCount)
            {
                u32 time = 0xffff-(MS_TIMER1->TMR_CTRL&0xffff);
                u32 through = (rxDataCount*(TXRX_DAT_SIZE+24))*1000/time;
                log_printf("rx_int_handler Finish time[%d] through[%d]\n",time,through*8);
            }

            rxSeq++;
        }
#else
        if ( rxSeq == 1 )
            {
                MS_TIMER1->TMR_CTRL = 0xffff;
            }
            else
        if ( rxSeq == rxDataCount)
            {
                u32 time = 0xffff-MS_TIMER1->TMR_CTRL;
                u32 through = (rxDataCount*(TXRX_DAT_SIZE+24))*1000/time;
                log_printf("rx_int_handler Finish time[%d] through[%d]\n",time,through);
            }
        rxSeq++;
#endif
        REG_MAILBOX->CPU_FLG = CPU_INT;
        mailbox_mng_rls(pPktAddress);
    }

}


#endif

#if ( TXRX_FPGA_TEST==1 )
static u32 initTx()
{
    //init PHY
    *((u32 *)(PHY_BASE+0x10b0))= 0x00000000;
    *((u32 *)(PHY_BASE+0x0000))= 0x0000000a;
    *((u32 *)(PHY_BASE+0x208c))= 0x00000993;
    *((u32 *)(PHY_BASE+0x23f8))= 0x21140000;
    *((u32 *)(PHY_BASE+0x2004))= 0x001f1e0a;
    *((u32 *)(PHY_BASE+0x2008))= 0x7f202043;
    *((u32 *)(PHY_BASE+0x10A4))= 0x0002182C;

    *((u32 *)(PHY_BASE+0x2084))= 0x1F30CFFF;
    *((u32 *)(PHY_BASE+0x2090))= 0x91607004;
    *((u32 *)(PHY_BASE+0x2030))= 0x06081989;

    //GENERAL_MAC_TMODE
    //NORMAL
    *((u32 *)(MAC_GENERAL+0x0010))= 0;
    //MAC_LOOPBACK
    //*((u32 *)(MAC_GENERAL+0x0010))= 1 << 0;
    //DUAL_MAC
    //*((u32 *)(MAC_GENERAL+0x0010))= 1 << 1;
    //PHY_LOOPBACK
    //*((u32 *)(MAC_GENERAL+0x0010))= 1 << 2;
    //*((u32 *)(PHY_BASE+0x0000))= 0x00000003;

    //filter wildcard
    //MRX_FLT_EN2
    *((u32 *)(MAC_RX+0x00088))= 0x1008;

    //reset rx cmd seq
    //MRX_FCMD_DAT
    *((u32 *)(MAC_RX+0x00100))= 0;
    //MRX_FCMD_MNG
    *((u32 *)(MAC_RX+0x00104))= 0;
    //MRX_FCMD_CTRL
    *((u32 *)(MAC_RX+0x00108))= 0;

	//enable tx count
	//MTX_CNT_EN
    *((u32 *)(MAC_TX+0x00020))= 0x7fffffff;
    //SIFS
    //MTX_SIFS_TIME
    *((u32 *)(MAC_TX+0x00094))= 5000;//5 ms

    //set data offset
    //GENERAL_GLBLE_SET
    *((u32 *)(MAC_GENERAL+0x0300))= (6 << 16)| (TXRX_DAT_OFFSET << 8);

    //set send mac address
    //GENERAL_STA_MAC0
    *((u32 *)(MAC_GENERAL+0x0314))= 0x11111110;
    //GENERAL_STA_MAC1
    *((u32 *)(MAC_GENERAL+0x0318))= 0x00001111;

    //set send SSID
    //GENERAL_BSSID0
    *((u32 *)(MAC_GENERAL+0x030C))= 0x11111110;
    //GENERAL_BSSID1
    *((u32 *)(MAC_GENERAL+0x0310))= 0x00001111;

    //GENERAL_WSID0
    *((u32 *)(MAC_GENERAL+0x0400))= 1;
    //GENERAL_PEER_MAC00
    *((u32 *)(MAC_GENERAL+0x0404))= 0x22222222;
    //GENERAL_PEER_MAC01
    *((u32 *)(MAC_GENERAL+0x0408))= 0x00002222;

    //reset count
    isCancelTx = FALSE;
    txSeq = 1;
    //send 100 data to rx
    txDataCount = TXRX_DAT_COUNT;


    return 0;
}

static u32* packDataAndSend(u8 *pData, u32 length,u32 seq) {
    u32* pSendAddress = mailbox_mng_alloc(2048);
    PKT_TxInfo* pPKT_TxInfo = (PKT_TxInfo*)pSendAddress;
    u32* pPhy = (u32*)((u8 *)pSendAddress+16);
    u32* pPktHeader = (u32*)(((u8 *)pSendAddress)+TXRX_DAT_OFFSET);
    //u8* pPktData = (u8*)(((u8 *)pSendAddress)+TXRX_DAT_OFFSET+24);

		pPKT_TxInfo->c_type = M2_TXREQ;
		pPKT_TxInfo->f80211 = 1;
		pPKT_TxInfo->qos = 0;
		pPKT_TxInfo->ht = 0;
		pPKT_TxInfo->use_4addr = 0;
		pPKT_TxInfo->RSVD_0 = 0;
		pPKT_TxInfo->more_data = 0;
		pPKT_TxInfo->stype_b5b4 = 0;
		pPKT_TxInfo->extra_info = 0;

		/* The definition of WORD_2: */
		pPKT_TxInfo->fCmd = 0;

		/* The definition of WORD_3: */
		pPKT_TxInfo->hdr_offset = TXRX_DAT_OFFSET;//:8;
		pPKT_TxInfo->frag = 0;//:1;
		pPKT_TxInfo->unicast = 1;//:1;
		pPKT_TxInfo->hdr_len = 24;
		pPKT_TxInfo->frag_offload = 0;//:1; /* 0: no frag hw offload, 1: need frag hw offload */
		pPKT_TxInfo->tx_burst = 0;//:1;     /* 0: normal, 1: burst tx */
		pPKT_TxInfo->ack_policy = 0;//:2;   /* See Table 8-6, IEEE 802.11 Spec. 2012 */
		pPKT_TxInfo->RSVD_1 = 0;//:4;
		pPKT_TxInfo->do_rts_cts = 0;//:2;   /* 0: no RTS/CTS, 1: need RTS/CTS , 2: CTS protection, 3: RSVD */
		pPKT_TxInfo->reason = 0;//:6;

		/* The definition of WORD_4: */
		pPKT_TxInfo->payload_offset = pPKT_TxInfo->hdr_offset
				+ pPKT_TxInfo->hdr_len;//:8;
		pPKT_TxInfo->next_frag_pid = 0;//:7;
		pPKT_TxInfo->RSVD_2 = 0;//:1;
		pPKT_TxInfo->fCmdIdx = 0;//:3;
		pPKT_TxInfo->wsid = 0;//:3;
		pPKT_TxInfo->RSVD_3 = 0;//:4;
		pPKT_TxInfo->TxF_ID = 0;//:6;
        //length
		pPKT_TxInfo->len = pPKT_TxInfo->hdr_len + length;

        // 0 : 1M
		// 1 : 2M
		// 2 : 5.5M
		// 3 : 11M
        *pPhy = txDataDateRate << 24;

        //fill header
		*(pPktHeader+0) = 2 << 2; //Data
		*(pPktHeader+0) = *(pPktHeader+0) | (1 << 9);//From DS
		*(pPktHeader+1) = 0x22222222; // address1
		*(pPktHeader+2) = 0x11102222; // address1+address2
		*(pPktHeader+3) = 0x11111111; // address2
		*(pPktHeader+4) = 0x11111110; // address3
		*(pPktHeader+5) = 0x00001111; // address3
		*(pPktHeader+5) = *(pPktHeader+5) | (seq << (16 + 4));////Sequence Number

        //memcpy(pPktData,pData,length);

        //send data
		//put to CPU Fifo
		ENG_MBOX_SEND(HW_MAC,pSendAddress);
		return pSendAddress;

}

static u32 sendDataOut(u8 *pData, u32 length)
{
    u8 isCont = TRUE;
    u32 ret = 0;
    u32 i = 0;
    u32 txCount = TXRX_DAT_BLOCK;
    if ( (txDataCount - txSeq+1) < TXRX_DAT_BLOCK )
    {
        txCount = txDataCount - txSeq+1;
    }

    for ( i=0;i<txCount;i++ )
    {
        packDataAndSend(pData,length,txSeq+i);
    }

    //read tx status ,check if send ok
    //MTX_CNT_FRAG
	u32 mtx_cnt_frag = *((u32 *)(MAC_TX+0x0024));
    //MTX_CNT_FRM
	u32 mtx_cnt_frm = *((u32 *)(MAC_TX+0x0028));
    //MTX_CNT_FAIL
	u32 mtx_cnt_fail = *((u32 *)(MAC_TX+0x0030));
	//MTX_CNT_RETRY
	//u32 mtx_cnt_retry = *((u32 *)(MAC_TX+0x0034));
    //MTX_CNT_MULTI_RETRY
	//u32 mtx_cnt_m_retry = *((u32 *)(MAC_TX+0x0038));

    log_printf("sendDataOut mtx_cnt_frag[%d] mtx_cnt_frm[%d] mtx_cnt_fail[%d]\n",mtx_cnt_frag,mtx_cnt_frm,mtx_cnt_fail);

    while(isCont)
    {
        if ( (mtx_cnt_frm + mtx_cnt_fail) >= txCount)
        {
            isCont = FALSE;
            ret = 0;
            *((u32 *)(MAC_TX+0x0024)) = mtx_cnt_frag;
            *((u32 *)(MAC_TX+0x0028)) = mtx_cnt_frm;
        }

        if ( mtx_cnt_fail != 0)
        {
            isCont = FALSE;
            ret = -1;
        }

         mtx_cnt_frag = *((u32 *)(MAC_TX+0x0024));
        //MTX_CNT_FRM
	    mtx_cnt_frm = *((u32 *)(MAC_TX+0x0028));
        //MTX_CNT_FAIL
	     mtx_cnt_fail = *((u32 *)(MAC_TX+0x0030));
	    //MTX_CNT_RETRY
	    // mtx_cnt_retry = *((u32 *)(MAC_TX+0x0034));
        //MTX_CNT_MULTI_RETRY
	    // mtx_cnt_m_retry = *((u32 *)(MAC_TX+0x0038));
	    //log_printf("sendDataOut2 mtx_cnt_frag[%d] mtx_cnt_frm[%d] mtx_cnt_fail[%d]\n",mtx_cnt_frag,mtx_cnt_frm,mtx_cnt_fail);
    }

    txSeq = txSeq + txCount;

    return ret;

}

static u32 initRx()
{
    //init PHY
    *((u32 *)(PHY_BASE+0x10b0))= 0x00000000;
    *((u32 *)(PHY_BASE+0x0000))= 0x0000000a;
    *((u32 *)(PHY_BASE+0x208c))= 0x00000993;
    *((u32 *)(PHY_BASE+0x23f8))= 0x21140000;
    *((u32 *)(PHY_BASE+0x2004))= 0x001f1e0a;
    *((u32 *)(PHY_BASE+0x2008))= 0x7f202043;
    *((u32 *)(PHY_BASE+0x10A4))= 0x0002182C;

    *((u32 *)(PHY_BASE+0x2084))= 0x1F30CFFF;
    *((u32 *)(PHY_BASE+0x2090))= 0x91607004;
    *((u32 *)(PHY_BASE+0x2030))= 0x06081989;

    //GENERAL_MAC_TMODE
    //NORMAL
    *((u32 *)(MAC_GENERAL+0x0010))= 0;
    //MAC_LOOPBACK
    //*((u32 *)(MAC_GENERAL+0x0010))= 1 << 0;
    //DUAL_MAC
    //*((u32 *)(MAC_GENERAL+0x0010))= 1 << 1;
    //PHY_LOOPBACK
    //*((u32 *)(MAC_GENERAL+0x0010))= 1 << 2;
    //*((u32 *)(PHY_BASE+0x0000))= 0x00000003;

    //filter wildcard
    //MRX_FLT_EN2
    *((u32 *)(MAC_RX+0x00088))= 0x1008;

    //reset rx cmd seq
    //MRX_FCMD_DAT
    *((u32 *)(MAC_RX+0x00100))= 0;
    //MRX_FCMD_MNG
    *((u32 *)(MAC_RX+0x00104))= 0;
    //MRX_FCMD_CTRL
    *((u32 *)(MAC_RX+0x00108))= 0;

	//enable tx count
	//MTX_CNT_EN
    *((u32 *)(MAC_TX+0x00020))= 0x7fffffff;
    //SIFS
    //MTX_SIFS_TIME
    //*((u32 *)(MAC_TX+0x00094))= 5000;//5 ms

    //set data offset
    //GENERAL_GLBLE_SET
    *((u32 *)(MAC_GENERAL+0x0300))= (6 << 16)| (TXRX_DAT_OFFSET << 8);

    //set send mac address
    //GENERAL_STA_MAC0
    *((u32 *)(MAC_GENERAL+0x0314))= 0x22222222;
    //GENERAL_STA_MAC1
    *((u32 *)(MAC_GENERAL+0x0318))= 0x00002222;

    //set send SSID
    //GENERAL_BSSID0
    *((u32 *)(MAC_GENERAL+0x030C))= 0x11111110;
    //GENERAL_BSSID1
    *((u32 *)(MAC_GENERAL+0x0310))= 0x00001111;

    //GENERAL_WSID0
    *((u32 *)(MAC_GENERAL+0x0400))= 1;
    //GENERAL_PEER_MAC00
    *((u32 *)(MAC_GENERAL+0x0404))= 0x11111110;
    //GENERAL_PEER_MAC01
    *((u32 *)(MAC_GENERAL+0x0408))= 0x00001111;

    //reset count
    rxDataCount = TXRX_DAT_COUNT;
    rxSeq = 1;
    isCancelRx = FALSE;

    //register irq handle
	irq_request(IRQ_RX, rx_int_handler, NULL);

    return 0;
}

static u32 CLICmdTxRxTestRandomDataTx(s32 argc, char *argv[])
{
    u32 status;

    // 0 : 1M
	// 1 : 2M
    // 2 : 5.5M
	// 3 : 11M
    txDataDateRate = 0;

    if ( argc > 3 )
    {
        txDataDateRate = (u32) ssv6xxx_atoi(argv[3]);
    }

    initTx();

    MS_TIMER1->TMR_CTRL = 0xffff;

    while(!isCancelTx&&txSeq<=txDataCount)
    {
        //log_printf("CLICmdTxRxTestRandomDataTx start [%d/%d]\n",txSeq-1,txDataCount);
        if ((status = sendDataOut(NULL,TXRX_DAT_SIZE))!=0)
        {
            isCancelTx = TRUE;
            log_printf("CLICmdTxRxTestRandomDataTx Error [%d/%d]\n",txSeq-1,txDataCount);
        }
        else
        {
            log_printf("CLICmdTxRxTestRandomDataTx [%d/%d]\n",txSeq-1,txDataCount);
        }
    }

    log_printf("CLICmdTxRxTestRandomDataTx Finish [%d]\n",0xffff-(MS_TIMER1->TMR_CTRL&0xffff));

	return 0;

}

static u32 CLICmdTxRxTestRandomDataRx(s32 argc, char *argv[])
{
	//u32 status;
    initRx();


	return 0;

}


static u32 CLICmdTxRxTestAll(s32 argc, char *argv[]) {
	int i = 0, res = 0;

	for (i = 1; i < testCmdTableSize; i++) {
		if (gCliCmdTxRxTestTable[i].CmdTxRxTestHandler != NULL) {
			res = gCliCmdTxRxTestTable[i].CmdTxRxTestHandler(argc, argv);
			if (res == 0) {
				log_printf("Execute Test [%s] Success\n",
						gCliCmdTxRxTestTable[i].Cmd);
			} else {
				log_printf("Execute Test [%s] Fail\n", gCliCmdTxRxTestTable[i].Cmd);
				return res;
			}
		}
	}
	return res;
}

#endif

/*---------------------------------- CMDs -----------------------------------*/
#if ( TXRX_FPGA_TEST==1 )
static const CLICmdTxRxTest gCliCmdTxRxTestTable[] =
{
		//{"All TxRx test",CLICmdTxRxTestAll },
		{"TxRx random data (Tx)" ,CLICmdTxRxTestRandomDataTx},
        {"TxRx random data (Rx)" ,CLICmdTxRxTestRandomDataRx},
};
#endif

void _Cmd_HwTXRX(s32 argc, char *argv[]) {
#if ( TXRX_FPGA_TEST==1 )
	u32 commandIndex;
#endif
	s32 res;
	//void *m_data;

	/**
	 * Usage:  hw-txrx test [?]
	 */

	if (argc < 3) {
		log_printf("Invalid hw-txrx command!!\n");
		return;
	}

#if ( TXRX_FPGA_TEST==1 )
	if (strcmp(argv[1], "test") == 0) {
		commandIndex = (u32) ssv6xxx_atoi(argv[2]);

		testCmdTableSize = (sizeof(gCliCmdTxRxTestTable)
				/ sizeof(gCliCmdTxRxTestTable[0]));

		/* Dispatch command */
		if (commandIndex < testCmdTableSize) {
			if (gCliCmdTxRxTestTable[commandIndex].Cmd != NULL) {
				res = gCliCmdTxRxTestTable[commandIndex].CmdTxRxTestHandler(argc,
						argv);
				if (res == 0) {
					log_printf("Execute Test [%s] Success\n",
							gCliCmdTxRxTestTable[commandIndex].Cmd);
				} else {
					log_printf("Execute Test [%s] Fail\n",
							gCliCmdTxRxTestTable[commandIndex].Cmd);
				}
			} else {
				log_printf("Command not found!!\n");
			}
		} else {
			log_printf("\nCommand index out of boundary [%d]!!\n",
					testCmdTableSize);
		}
	} else
#endif
	{
		log_printf("Invalid dma cli command !!\n");
		return;
	}
}
