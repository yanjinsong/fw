#include <config.h>
#include <msgevt.h>
#include <pbuf.h>
#include <bsp/bsp.h>
#include <bsp/serial.h>
#include <apps/mac80211/soft-main.h>
#include <apps/mac80211/cmd_engine.h>
#include <apps/mac80211/MLME_Task.h>
#include <apps/mac80211/test_sdio.h>
#include <cmd_def.h>
#include <log.h>
#include <driver/phy/drv_phy.h>
#include <lib/ssv_lib.h>

#include <stdio.h>
#include <stdarg.h>

#if (CONFIG_SIM_PLATFORM==0 && CLI_ENABLE == 1)
#include <cli/cli.h>
#endif


/**
 *  Task Information Table:
 *
 *  Each Task registered here is created in system initial time. After a task
 *  is created, a Message Queue for the task is also created. Once the
 *  task starts, the task is blocked and waitting for message from the
 *  Message queue.
 */
struct task_info_st g_soc_task_info[] = {
    { "Soft-Mac",       (OsMsgQ)0, 40, OS_TASK_PRIO2, 128, NULL, soft_mac_task		}, // the stack size is 2K
    { "MLME",           (OsMsgQ)0, 31, OS_TASK_PRIO3, 160, NULL, MLME_Task			}, // the stack size is 2.5K
#ifdef THROUGHPUT_TEST
	{ "Testing",	   (OsMsgQ)0, 4, OS_TASK_PRIO2, 128, NULL, Testing_Task 		},
#endif
#if (CONFIG_SIM_PLATFORM==0 && CLI_ENABLE == 1)
    { "cli_task",       (OsMsgQ)0, 0, OS_TASK_PRIO2, 64, NULL, Cli_Task   		}, // the stack size is 1K
#endif

};

#if(MAILBOX_DBG == 1)

#define DEBUG_SIZE 1024
char debug_buffer[DEBUG_SIZE];

void dump_mailbox_dbg (int num)
{
	int i;
	int total_num = (num > 0) ? (num * 4) : sizeof(debug_buffer);
	//u32 a = *(volatile u32 *)(0xcd01003c);
	//u32 b = *(volatile u32 *)(0xc10000a8);
	//su32 c = *(volatile u32 *)(0xc10000b0);
    /*please go to JIRA (ssv6200_reg.xml) to check details*/
    u32 id_len_threadshold2 = *(volatile u32 *)(0xcd01003c);
    printf("ID_LEN_THREADSHOLD2(0xCD01003C):%08X\n", id_len_threadshold2);
    printf("ALL_ID_ALC_LEN:%02X\n", GET_ALL_ID_ALC_LEN);
    printf("TX_ID_ALC_LEN:%02X\n", GET_TX_ID_ALC_LEN);
    printf("RX_ID_ALC_LEN:%02X\n", GET_RX_ID_ALC_LEN);
    printf("0xC10000A0: %08X\n", *(volatile u32 *)(0xc10000A0));
    printf("0xC10000A4: %08X\n", *(volatile u32 *)(0xc10000A4));
    printf("0xC10000A8: %08X\n", *(volatile u32 *)(0xc10000A8));
    printf("0xC10000AC: %08X\n", *(volatile u32 *)(0xc10000AC));
    printf("0xC10000B0: %08X\n", *(volatile u32 *)(0xc10000B0));

	for (i = 0; i < total_num; i += 4)
	{
#if 0
		u32    log_type = (u32)(debug_buffer[0] >> 5);
		u32    time_tag = (((u32)(debug_buffer[0] & 0x1F)) << 7) + (u32)(debug_buffer[1] >> 1);
#endif
		u32    log = *(u32 *)&debug_buffer[i];
		u32    log_type = log >> 29;
		//u32    time_tag = (log >> 17) & 0x0FFF;

		printf("%d: %d - ", (log >> 17) & 0x0FFF, log_type);

		switch (log_type) {
		case 0: // Normal case
			{
            #if 0
			u32     r_w = (log >> 16) & 0x01;
			u32		port = (log >> 12) & 0x0F;
			u32		HID = (log >> 8) & 0x0F;
			u32		PID = log & 0x0FF;
            #endif
			printf("RW: %u, PRT: %u, HID: %u, PID: %u\n", (log >> 16) & 0x01, (log >> 12) & 0x0F, (log >> 8) & 0x0F, log & 0x0FF);
			}
			break;
			break;
		case 1: // Allocate ID failed
			{
            #if 0
			u32		ID = log & 0x03F;
            #endif
			printf("PID: %u\n", log & 0x03F);
			}
			break;
		case 2: // Release ID failed
			{
            #if 0
			u32		ID = log & 0x03F;
            #endif
			printf("PID: %u\n", log & 0x03F);
			}
			break;
		case 4: // Packet demand request
			{
            #if 0
			u32     port = (log >> 8) & 0x0F;
			u32		ID = log & 0x0FF;
            #endif
			printf("PRT: %u, HID: %u\n", (log >> 8) & 0x0F, log & 0x0FF);
			}
			break;
		case 5: // ID allocation request
			{
            #if 0
			u32     page_length = (log >> 7) & 0xFF;
			u32     chl = (log >> 15) & 0x03;
			u32		ID = log & 0x07F;
            #endif
			printf("LEN: %u, CHL: %u, ID: %u\n", (log >> 7) & 0xFF, (log >> 15) & 0x03, log & 0x07F);
			}
			break;
		case 6: // ID allocation ack
			{
            #if 0
			u32		nack = 0;
			u32     page_length = (log >> 7) & 0xFF;
			u32     chl = (log >> 15) & 0x03;
			u32		ID = log & 0x07F;
            #endif
			printf("NCK: %u, LEN: %u, CHL: %u, ID: %u\n", 0, (log >> 7) & 0xFF, (log >> 15) & 0x03, log & 0x07F);
			}
			break;
		case 7: // ID allocation nack
			{
            #if 0
			u32		nack = 1;
			u32     page_length = (log >> 7) & 0xFF;
			u32     chl = (log >> 15) & 0x03;
			u32		ID = log & 0x07F;
            #endif
			printf("NCK: %u, LEN: %u, CHL: %u, ID: %u\n", 1, (log >> 7) & 0xFF, (log >> 15) & 0x03, log & 0x07F);
			}
			break;
		}
#if 0
		LOG_PRINTF("%02X %02X %02X %02X\n",
				   debug_buffer[i + 0], debug_buffer[i + 1],
				   debug_buffer[i + 2], debug_buffer[i + 3]);
#endif
	}
}

void enable_mailbox_dbg()
{
	LOG_PRINTF("MAILBOX debug buffer = %08x\n", (int)debug_buffer);

	SET_MB_DBG_CFG_ADDR((int)debug_buffer);
	SET_MB_DBG_LENGTH(DEBUG_SIZE);
	SET_MB_DBG_EN(1);
	SET_MB_ERR_AUTO_HALT_EN(1);
}

#endif

#if (CONFIG_SIM_PLATFORM == 0)
#if defined(ENABLE_FW_SELF_CHECK)

#define FW_STATUS_REG       ADR_TX_SEG
#define BUF_SIZE            (FW_BLOCK_SIZE)


#define FW_BLOCK_CNT_SIZE   (0xFFF)                     //4096*FW_BLOCK_SIZE  support 4MB FW
#define FW_CHK_SUM_SIZE     (FW_BLOCK_CNT_SIZE)           //4096*FW_BLOCK_SIZE  support 4MB FW


#define FW_STATUS_SIZE      (0xF)


#define FW_MASK             (0xFFFF<<16)
#define FW_BLOCK_CNT_MASK   (FW_BLOCK_CNT_SIZE)<<16
#define FW_CHK_SUM_MASK     (FW_CHK_SUM_SIZE)<<16
#define FW_STATUS_MASK      (FW_STATUS_SIZE)<<28



#define FW_STATUS_FW_CHKSUM_BIT      (1<<31)
#define FW_STATUS_HOST_CONFIRM_BIT   (1<<30)



#include <ssv_regs.h>
void _calc_fw_checksum (void)
{

    volatile u32 *FW_RDY_REG = (volatile u32 *)(SPI_REG_BASE + 0x10);
    u32          block_count = (*FW_RDY_REG >> 16) & 0x0FF;
    u32          fw_checksum = FW_CHECKSUM_INIT;
    u32         *addr = 0;
    u32          total_words = block_count * FW_BLOCK_SIZE / sizeof(u32);

    // Don't calculate checksum if host does not provide block count.
    if (block_count == 0)
        return;

    while (total_words--) {
        fw_checksum += *addr++;
    }
    fw_checksum = ((fw_checksum >> 24) + (fw_checksum >> 16) + (fw_checksum >> 8) + fw_checksum) & FW_CHK_SUM_SIZE;
    fw_checksum = (fw_checksum << 16) & FW_CHK_SUM_MASK;

    *FW_RDY_REG = (*FW_RDY_REG & ~FW_MASK) | fw_checksum | FW_STATUS_FW_CHKSUM_BIT;


    //wait until host confirm recv chksum and it's oks.
	do {
        if ((*FW_RDY_REG & FW_STATUS_HOST_CONFIRM_BIT))
            break;
    } while (1);
}
#endif // ENABLE_FW_SELF_CHECK

static void _zero_bss (void)
{
	extern u32   __bss_beg__;
	extern u32   __bss_end__;
	u8          *p_bss = (u8 *)&__bss_beg__;
	u8          *p_bss_end = (u8 *)&__bss_end__;

	while (p_bss != p_bss_end)
		*p_bss++ = 0;
}
#endif


/**
 *  Entry point of the firmware code. After system booting, this is the
 *  first function to be called from boot code. This function need to
 *  initialize the chip register, software protoctol, RTOS and create
 *  tasks. Note that, all memory resource needed for each software
 *  module shall be pre-allocated in initialization time.
 *
 */
extern const char *rlsversion;
extern const char *version;
extern const char *date;
void APP_Main( void )
{
    register u32 i;

    /* Check integraty of downloaded firmware */
    #if (CONFIG_SIM_PLATFORM == 0)
    #ifdef ENABLE_FW_SELF_CHECK
    _calc_fw_checksum();
    #endif // ENABLE_FW_SELF_CHECK
	/* Fill BSS with 0 */
	_zero_bss();
    #endif
    /* Initialize hardware & device drivers */
    ASSERT(bsp_init() == OS_SUCCESS);

#if (CONFIG_SIM_PLATFORM == 0)
    /**
    * Initialize RTOS before starting use it. If in simulation/emulation platform,
    * we shall ignore this initialization because the simulation/emulation has
    * initialzed.
    */
    ASSERT( OS_Init() == OS_SUCCESS );

#ifdef USE_SERIAL_DRV
	// Initialize interrupt-driven UART driver
	xSerialPortInitMinimal(ser115200, 256);
#endif // USE_SERIAL_DRV

    LOG_init(true, true, LOG_LEVEL_ON, LOG_MODULE_MASK(LOG_MODULE_EMPTY) | LOG_MODULE_MASK(LOG_MODULE_ALL), false);
    LOG_OUT_DST_OPEN(LOG_OUT_SOC_TERM);
    LOG_OUT_DST_CUR_ON(LOG_OUT_SOC_TERM);
#endif
    LOG_PRINTF("OS version: %s\n", tskKERNEL_VERSION_NUMBER);
    //#if (CONFIG_SIM_PLATFORM == 0)
    LOG_PRINTF("%s FW built at %s %s\n",
               CONFIG_PRODUCT_STR, __DATE__, __TIME__);
    //#endif
    printf("\33[35mRELEASE VERSION:%s    SW VERSION: %s \r\nBUILD DATE: %s\33[0m\r\n",rlsversion,version,date);
    printf("Kick WDT for AMPDU TX\n");
	/**
     * Initialize software components.
    */
    ASSERT( msg_evt_init() == OS_SUCCESS );
    ASSERT( PBUF_Init() == OS_SUCCESS );
    ASSERT( soft_mac_init() == OS_SUCCESS );
    ASSERT( cmd_engine_init() == OS_SUCCESS);
    ASSERT( ssv6xxx_SWQInit() == OS_SUCCESS);
	/**
        * Create Tasks and Message Queues. Creating Message Queue or not
        * depends on the xxx.qevt field is zero or not. If zero, no need to
        * create a message queue for the task.
        */
    for(i=0; i<(sizeof(g_soc_task_info)/sizeof(g_soc_task_info[0])); i++) {
        /* Create Registered Task: */
        OS_TaskCreate(g_soc_task_info[i].task_func,
            g_soc_task_info[i].task_name,
            g_soc_task_info[i].stack_size<<4,
            g_soc_task_info[i].args,
            g_soc_task_info[i].prio,
            &g_soc_task_info[i].task_handle);
        //sys_init_prnf("task:%s, stack size=%dKBytes\n",g_soc_task_info[i].task_name,(g_soc_task_info[i].stack_size<<4)>>10);
        if ((u32)g_soc_task_info[i].qlength> 0)
		{
            ASSERT(OS_MsgQCreate(&g_soc_task_info[i].qevt,
            (u32)g_soc_task_info[i].qlength)==OS_SUCCESS);
        }
    }

	LOG_PRINTF("Initialzation done. \nStarting OS scheduler...\n");
#if (CONFIG_SIM_PLATFORM == 0)

#if(MAILBOX_DBG == 1)
	enable_mailbox_dbg();
#endif//__MAILBOX_DBG__

#if (WLAN_MAX_STA!=2)
#pragma message("=========================================================")
#pragma message("WLAN_MAX_STA is not 2, Please modify the RX decision Table")
#pragma message("=========================================================")
error
#endif
    /* Start the scheduler so our tasks start executing. */
    OS_StartScheduler();
#endif
}

