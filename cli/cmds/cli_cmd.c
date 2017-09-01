//#define __SFILE__ "cli_cmd.c"
#include <config.h>
#include <log.h>
#include <cli/cli.h>
#include <soc_global.h>
#include <pbuf.h>

//add by zoe
//will be removed
#include "phy/drv_phy.h"
// extern syn_channel_select VT_Tbl[];

#include <apps/mac80211/defrag.h>
#include <apps/mac80211/mlme.h>
#include <flash/drv_flash.h>
#include "cli_cmd_monitor.h"
// #define __TEMPERATURE_VERIFY__
//#define __PMU_VERIFY__

#ifdef  __PMU_VERIFY__
#include <rtc/drv_rtc.h>
#include <pmu/drv_pmu.h>
#endif
#include <apps/mac80211/sta_info.h>
#include <regs/ssv6200_reg_sim.h>
#if (N_MODE_AMPDU_TX == 1)
#include <ampdu/drv_ampdu.h>
#endif


/*---------------------------------- CMDs -----------------------------------*/
#if 0
static void Cmd_McuSleep(s32 argc, char *argv[])
{
    SET_MCU_CLK_EN(0);
    LOG_DEBUG("mcu wake up");
}
#endif
#if 0
static void Cmd_Exit( s32 argc, char *argv[] )
{
	OS_Terminate();
}
#endif

#ifdef ENABLE_CLI_CMD_MIB
static void Cmd_Mib(s32 argc, char  *argv[])
{
    volatile u32 * pointer;
    if (argc==2 && (strcmp(argv[1], "reset")==0 || strcmp(argv[1], "on")==0))
    {
        g_soc_mib.sw_mac_rx_cnt=0;
        g_soc_mib.sw_mac_handle_cnt=0;
        g_soc_mib.hdl.hdl_defrag_rx_cnt=0;
        g_soc_mib.hdl.hdl_defrag_accept=0;
        g_soc_mib.sw_mac_drop_cnt=0;
        g_soc_mib.sw_cmd_rx_cnt=0;
        g_soc_mib.mlme.sw_mlme_rx_cnt=0;

        SET_TRX_DEBUG_CNT_ENA(1);

        //Reset MAC MIB
        pointer = (u32 *)MIB_REG_BASE;
        *pointer = 0x00;
        *pointer = 0xffffffff;

        //Reset PHY MIB
        SET_RG_PACKET_STAT_EN_11B(0);
        SET_RG_PACKET_STAT_EN_11B(1);

        SET_RG_PACKET_STAT_EN_11GN(0);
        SET_RG_PACKET_STAT_EN_11GN(1);
        
        SET_RG_MRX_EN_CNT_RST_N(0);
        SET_RG_MRX_EN_CNT_RST_N(1);

        printf("MIB reset success!!\n");
        return;
    }
    else if (argc==2 && strcmp(argv[1], "off")==0)
    {
        SET_TRX_DEBUG_CNT_ENA(0);

        pointer = (u32 *)MIB_REG_BASE;
        *pointer = 0x00;

        return;
    }
    else if(argc==2)
    {
#if 0
        if (strcmp(argv[1], "sw") == 0)
        {
            printf("\nSW MIB Statistic:\n");
            printf("    sw_mac_rx_cnt       :%d\n", g_soc_mib.sw_mac_rx_cnt);
            printf("    sw_mac_handle_cnt   :%d\n", g_soc_mib.sw_mac_handle_cnt);
            printf("    sw_defrag_rx_cnt    :%d\n",g_soc_mib.hdl.hdl_defrag_rx_cnt);
            printf("    sw_defrag_accept    :%d\n",g_soc_mib.hdl.hdl_defrag_accept);
            printf("    sw_mac_drop_cnt     :%d\n", g_soc_mib.sw_mac_drop_cnt);
            printf("    sw_cmd_rx_cnt       :%d\n", g_soc_mib.sw_cmd_rx_cnt);
            printf("    sw_mlme_rx_cnt      :%d\n", g_soc_mib.mlme.sw_mlme_rx_cnt);
        }
        else if (strcmp(argv[1], "hci") == 0)
        {
            printf("\nHW-HCI Statistic:\n");
            printf("    Tx Pkt Count        :%d\n", GET_TX_PKT_COUNTER);
            printf("    Rx Pkt Count        :%d\n", GET_RX_PKT_COUNTER);
            printf("    Host CMD Count      :%d\n", GET_HOST_CMD_COUNTER);
            printf("    Host Event Count    :%d\n", GET_HOST_EVENT_COUNTER);
            printf("    Tx Drop Count       :%d\n", GET_TX_PKT_DROP_COUNTER);
            printf("    Rx Drop Count       :%d\n", GET_RX_PKT_DROP_COUNTER);
            printf("    Tx Pkt Trap Count   :%d\n", GET_TX_PKT_TRAP_COUNTER);
            printf("    Rx Pkt Trap Count   :%d\n", GET_RX_PKT_TRAP_COUNTER);
            printf("    Tx Fail Count       :%d\n", GET_HOST_TX_FAIL_COUNTER);
            printf("    Rx Fail Count       :%d\n", GET_HOST_RX_FAIL_COUNTER);
        }
        else if (strcmp(argv[1], "tx") == 0)
        {
            printf("\nTX Statistic:\n");

            printf("    Tx Group            :%08d\n", GET_MTX_GRP);
            printf("    Tx Fial             :%08d\n", GET_MTX_FAIL);
            printf("    Tx Retry            :%08d\n", GET_MTX_RETRY);
            printf("    Tx Multi Retry      :%08d\n", GET_MTX_MULTI_RETRY);
            printf("    Tx RTS success      :%08d\n", GET_MTX_RTS_SUCC);
            printf("    Tx RTS Fail         :%08d\n", GET_MTX_RTS_FAIL);
            printf("    Tx ACK Fail         :%08d\n", GET_MTX_ACK_FAIL);
            printf("    Tx total frame count:%08d\n", GET_MTX_FRM);
            printf("    Tx ack frame count  :%08d\n", GET_MTX_ACK_TX);
            printf("    Tx WSID-0 success   :%08d\n", GET_MTX_WSID0_SUCC);
            printf("    Tx WSID-0 frame     :%08d\n", GET_MTX_WSID0_FRM);
            printf("    Tx WSID-0 retry     :%08d\n", GET_MTX_WSID0_RETRY);
            printf("    Tx WSID-0 Total     :%08d\n", GET_MTX_WSID0_TOTAL);
        }
        else
#endif
        if (strcmp(argv[1], "rx") == 0)
        {
            printf("\nRX Statistic:\n");

            printf("PHY RX status:\n");
            printf("    B mode CRC error      :%08d\n", GET_B_PACKET_ERR_CNT);
            printf("    B mode CCA CNT        :%08d\n", GET_B_CCA_CNT);
            printf("    B mode PKT CNT        :%08d\n", GET_B_PACKET_CNT);

            printf("    GN mode CRC error     :%08d\n", GET_GN_PACKET_ERR_CNT);
            printf("    GN mode CCA CNT       :%08d\n", GET_GN_CCA_CNT);
            printf("    GN mode PKT CNT       :%08d\n", GET_GN_PACKET_CNT);

            printf("MAC RX status:\n");
            printf("    RX duplicated frame                 :%08d\n", GET_MRX_DUP);
            printf("    RX fragment frame                   :%08d\n", GET_MRX_FRG);
            printf("    RX group frame                      :%08d\n", GET_MRX_GRP);
            printf("    RX fcs error frame                  :%08d\n", GET_MRX_FCS_ERR);
            printf("    RX fcs success frame                :%08d\n", GET_MRX_FCS_SUC);
            printf("    RX miss a pcaket from PHY frame     :%08d\n", GET_MRX_MISS);
            printf("    Allocation failure                  :%08d\n", GET_MRX_ALC_FAIL);
            printf("    RX ACK notify                       :%08d\n", GET_MRX_DAT_NTF);
            printf("    RX rts frame notify                 :%08d\n", GET_MRX_RTS_NTF);
            printf("    RX cts frame notify                 :%08d\n", GET_MRX_CTS_NTF);
            printf("    RX receive ACK frames               :%08d\n", GET_MRX_ACK_NTF);
            printf("    RX BA frame notify                  :%08d\n", GET_MRX_BA_NTF);
            printf("    RX data frame notify                :%08d\n", GET_MRX_DATA_NTF);
            printf("    RX manage frame notify              :%08d\n", GET_MRX_MNG_NTF);
            printf("    RX ACK notify but crc error         :%08d\n", GET_MRX_DAT_CRC_NTF);
            printf("    RX BAR frame notify                 :%08d\n", GET_MRX_BAR_NTF);
            printf("    RX MBOX miss                        :%08d\n", GET_MRX_MB_MISS);
            printf("    Not-in-IDLE                         :%08d\n", GET_MRX_NIDLE_MISS);
            printf("    CSR notify                          :%08d\n", GET_MRX_CSR_NTF);

        }
        #if(ENABLE_DYNAMIC_RX_SENSITIVE==1)
        else if (strcmp(argv[1], "tx") == 0)
        {
            printf("\nTX Statistic:\n");

            printf("    Tx Group            :%08d\n", GET_MTX_GRP);
            printf("    Tx Fial             :%08d\n", GET_MTX_FAIL);
            printf("    Tx Retry            :%08d\n", GET_MTX_RETRY);
            printf("    Tx Multi Retry      :%08d\n", GET_MTX_MULTI_RETRY);
            printf("    Tx RTS success      :%08d\n", GET_MTX_RTS_SUCC);
            printf("    Tx RTS Fail         :%08d\n", GET_MTX_RTS_FAIL);
            printf("    Tx ACK Fail         :%08d\n", GET_MTX_ACK_FAIL);
            printf("    Tx total frame count:%08d\n", GET_MTX_FRM);
            printf("    Tx ack frame count  :%08d\n", GET_MTX_ACK_TX);
            printf("    Tx WSID-0 success   :%08d\n", GET_MTX_WSID0_SUCC);
            printf("    Tx WSID-0 frame     :%08d\n", GET_MTX_WSID0_FRM);
            printf("    Tx WSID-0 retry     :%08d\n", GET_MTX_WSID0_RETRY);
            printf("    Tx WSID-0 Total     :%08d\n", GET_MTX_WSID0_TOTAL);
        }
        #endif
        else
        {
            //printf("mib hci/tx/rx/sw \n");
            #if(ENABLE_DYNAMIC_RX_SENSITIVE==1)
            printf("mib rx/tx \n");
            #else
            printf("mib rx \n");
            #endif
            printf("mib on/off/reset \n");
        }
        return;
    }
    else
    {
        //printf("mib hci/tx/rx/sw \n");
        #if(ENABLE_DYNAMIC_RX_SENSITIVE==1)
        printf("mib rx/tx \n");
        #else
        printf("mib rx \n");
        #endif
        printf("mib on/off/reset \n");
    }
}
#endif
#if 0
static void Cmd_Cal(s32 argc, char *argv[])
{

	s32 index;
	index=ssv6xxx_atoi(argv[1]);

	//Prepare setting for this version sof
	/*(*(volatile u32 *) 0xc0000a18)= 0x000e0006;
	(*(volatile u32 *) 0xc0000a1c)= 0x000e000e;*/

	drv_phy_channel_change(index,TRUE);

}
#endif


// r addr [size]
static void Cmd_Read(s32 argc, char *argv[])
{
    static u32 read_size = 1;
    static u32 read_addr = 0xCD010000;

    if (argc < 2)
    {
        //LOG_PRINTF("Usage: r addr [size]\n");
        hex_dump((const u32 *)read_addr, read_size);
        return;
    }

    read_addr = (u32)ssv6xxx_atoi_base(argv[1], 16);
    if (argc > 2)
        read_size = (u32)ssv6xxx_atoi(argv[2]);
    else
        read_size = 1;

    read_addr &= 0xFFFFFFE0;
    read_size = (read_size+7)&0xFFFFFFF8;
    hex_dump((const u32 *)read_addr, read_size);
} // end of - Cmd_Read -

// w addr value
static void Cmd_Write(s32 argc, char *argv[])
{
    volatile u32 *write_addr;
    u32           value;

    if (argc != 3)
    {
        LOG_PRINTF("Usage: w addr value\n");
        return;
    }

    write_addr = (volatile u32 *)ssv6xxx_atoi_base(argv[1], 16);
    value = (u32)ssv6xxx_atoi_base(argv[2], 16);
    *write_addr = value;
    printf("\n    %08X => %08X : %08X\n", value, (u32)write_addr, *write_addr);
} // end of - Cmd_Write

#ifdef ENABLE_LOG_TO_HOST


#define FRANK_DEBUG(fmt, ...)			t_printf(LOG_LEVEL_DEBUG, LOG_MODULE_DEFRAG, fmt, ##__VA_ARGS__)
static void Cmd_Log(s32 argc, char *argv[])
{
	// char	buf[256];
	//log_hcmd_st		hcmd;

	if ((argc == 2) && strcmp(argv[1], "t") == 0)
	{
		// t_printf(LOG_LEVEL_ON, LOG_MODULE_EMPTY, T("Cmd_Log : %x, %s\n"), 0x1122, T("south"));
		// t_printf(LOG_LEVEL_ON, LOG_MODULE_EMPTY, T("Cmd_Log : %d, %s\n"), 1234,   T("test 123"));
		//t_printf(LOG_LEVEL_ON,    LOG_MODULE_EMPTY,		T("0 args\n"));
		t_printf(LOG_LEVEL_TRACE, LOG_MODULE_MRX,		T("1 char args  - %c\n"), 'a');
		//t_printf(LOG_LEVEL_DEBUG, LOG_MODULE_MTX,		T("1 str args   - %s\n"), T("hello world"));
#if 1
		t_printf(LOG_LEVEL_INFO,  LOG_MODULE_EDCA,		T("3 digit args - %d, %3d, %03d\n"), 9, 9, 9);
		t_printf(LOG_LEVEL_WARN,  LOG_MODULE_PBUF,		T("3 hex args   - %x, %X, %08x\n"), 0xab, 0xab, 0xab);
		t_printf(LOG_LEVEL_FAIL,  LOG_MODULE_L3L4,		T("mix args     - %10s, %d, %s\n"), T("string"), 10, T("hello world"));
		t_printf(LOG_LEVEL_ERROR, LOG_MODULE_MGMT,		T("mix args     - %-3d, %-10s, %-08x\n"), 10, "string", 0xab);
		t_printf(LOG_LEVEL_ON,    LOG_MODULE_FRAG,		T("0 args\n"));
		t_printf(LOG_LEVEL_TRACE, LOG_MODULE_DEFRAG,	T("1 char args  - %c\n"), 'a');
		t_printf(LOG_LEVEL_DEBUG, LOG_MODULE_MLME,		T("1 str args   - %s\n"), T("hello world"));
		t_printf(LOG_LEVEL_INFO,  LOG_MODULE_CMD,		T("3 digit args - %d, %3d, %03d\n"), 9, 9, 9);
		t_printf(LOG_LEVEL_WARN,  LOG_MODULE_WPA,		T("3 hex args   - %x, %X, %08x\n"), 0xab, 0xab, 0xab);
		t_printf(LOG_LEVEL_FAIL,  LOG_MODULE_MAIN,		T("mix args     - %10s, %d, %s\n"), T("string"), 10, T("hello world"));

		//FRANK_DEBUG(T("frank debug ========================== %d\n"), __LINE__);
#endif
		return;
		/*
		memset(buf, 0, 256);
		log_printf("%s()<=\n\r", __FUNCTION__);

		LOG_PRINTF("test : LOG_PRINTF() %d\n\r", LOG_LEVEL_ON);
		LOG_TRACE ("test : LOG_TRACE () %d\n\r", LOG_LEVEL_TRACE);

		LOG_PRINTF("12345678\n\r");
		sprintf(buf, "hello %d %s %c\n\r", 0xaa, "abcdefg", 'f');
		log_printf(T(buf));
		LOG_PRINTF(buf);
		return;
		*/
	}

	if ((argc == 2) && strcmp(argv[1], "load") == 0)
	{
		LOG_out_dst_open(LOG_OUT_SOC_TERM, NULL);
		LOG_out_dst_turn_on(LOG_OUT_SOC_TERM);

		LOG_out_dst_open(LOG_OUT_SOC_HOST_TERM, NULL);
		LOG_out_dst_turn_on(LOG_OUT_SOC_HOST_TERM);
		/*
		LOG_out_dst_open(LOG_OUT_SOC_HOST_FILE,	(const u8 *)"c:\\soc_log.txt");
		LOG_out_dst_turn_on(LOG_OUT_SOC_HOST_FILE);
		*/
		LOG_out_desc_dump();
	}
	if (argc == 2 && (strcmp(argv[1], "test") == 0))
	{

		log_printf("\n\r");
		log_printf("test : log_printf() %s\n\r", "Hello~ I am handsome smart Eric Chen!!");
		log_printf("test : to turn off LOG_OUT_SOC_TERM ...\n\r");
		LOG_out_dst_turn_off(LOG_OUT_SOC_TERM);
		LOG_PRINTF("test : LOG_PRINTF() %d\n\r", LOG_LEVEL_ON);
		LOG_TRACE ("test : LOG_TRACE () %d\n\r", LOG_LEVEL_TRACE);
		LOG_DEBUG ("test : LOG_DEBUG () %d\n\r", LOG_LEVEL_DEBUG);
		LOG_INFO  ("test : LOG_INFO  () %d\n\r", LOG_LEVEL_INFO);
		LOG_WARN  ("test : LOG_WARN  () %d\n\r", LOG_LEVEL_WARN);
		LOG_FAIL  ("test : LOG_FAIL  () %d\n\r", LOG_LEVEL_FAIL);
		LOG_ERROR ("test : LOG_ERROR () %d\n\r", LOG_LEVEL_ERROR);
		// LOG_FATAL ("test : LOG_FATAL () %d\n\r", LOG_LEVEL_FATAL);
		DEFRAG_DEBUG("defrag : DEFRAG_DEBUG() %d\n\r", LOG_LEVEL_DEBUG);
		DEFRAG_INFO ("defrag : DEFRAG_INFO () %d\n\r", LOG_LEVEL_INFO);
		DEFRAG_WARN ("defrag : DEFRAG_WARN () %d\n\r", LOG_LEVEL_WARN);
		DEFRAG_FAIL ("defrag : DEFRAG_FAIL () %d\n\r", LOG_LEVEL_FAIL);
		DEFRAG_ERROR("defrag : DEFRAG_ERROR() %d\n\r", LOG_LEVEL_ERROR);
		// DEFRAG_FATAL("defrag : DEFRAG_FATAL() %d\n\r", LOG_LEVEL_FATAL);
		MLME_TRACE("mlme : MLME_TRACE() %d\n\r", LOG_LEVEL_TRACE);
		MLME_DEBUG("mlme : MLME_DEBUG() %d\n\r", LOG_LEVEL_DEBUG);
		MLME_INFO ("mlme : MLME_INFO () %d\n\r", LOG_LEVEL_INFO);
		MLME_WARN ("mlme : MLME_WARN () %d\n\r", LOG_LEVEL_WARN);
		MLME_FAIL ("mlme : MLME_FAIL () %d\n\r", LOG_LEVEL_FAIL);
		MLME_ERROR("mlme : MLME_ERROR() %d\n\r", LOG_LEVEL_ERROR);
		// MLME_FATAL("mlme : MLME_FATAL() %d\n\r", LOG_LEVEL_FATAL);
		LOG_out_dst_turn_on(LOG_OUT_SOC_TERM);

		return;
	}

	return;
}
#endif


#ifdef __MCU_CLK_EN_VERIFY__
static void Cmd_Clk(s32 argc, char *argv[])
{
    s32 i;

    if (strcmp(argv[1], "0") == 0) {
        for(i=4; i>=0; i--) {
            LOG_PRINTF("%d lines to set MCU clock off \n", i);
        }
        SET_MCU_CLK_EN(0);
    }
    else if (strcmp(argv[1], "1") == 0) {
        LOG_PRINTF("set MCU clock on \n");
        SET_MCU_CLK_EN(1);
    }
    else {
        LOG_PRINTF("invalid argv to Cmd_Clk\n");
    }

    for (i=0; i<10; i++) {
        LOG_PRINTF("%d lines after write csr_MCU_CLK_EN \n", i);
    }

}
#endif

#if 0
static void Cmd_Dump(s32 argc, char *argv[])
{
    s32 i;

    if (argc==2 && strcmp(argv[1], "decision") == 0) {
        LOG_PRINTF("\n  >> Decision Table:\n      ");
        for(i=0; i<16; i++) {
            LOG_PRINTF("0x%04x ", REG_MRX_DECTBL->DECI_VAL[i]);
            if (((i+1)&0x03) == 0) {
                LOG_PRINTF("\n      ");
            }
        }
        LOG_PRINTF("\n      ");
        for(i=0; i<9; i++) {
            LOG_PRINTF("0x%04x ", REG_MRX_DECMSK->DECI_MSK[i]);
            if (((i+1)&0x03) == 0) {
                LOG_PRINTF("\n      ");
            }
        }
        LOG_PRINTF("\n");
    }
    LOG_PRINTF("Invalid parameter !\n");

}
#endif
#if 0
static void Cmd_Phy(s32 argc, char *argv[])
{
    u32 *b32, i;

    if (argc >= 2 && strcmp(argv[1], "show")==0) {
        if (argc==3 && strcmp(argv[2], "phyinfo")==0) {
            LOG_PRINTF("\n  >> PHY-INFO:\n      ");
            for(i=0; i<39; i++) {
                /* Dump PHY-Info Table: */
                LOG_PRINTF("0x%08x ", REG_PHYINFO_ENTRY[i].INFO);
                if (((i+1)%4) == 0) {
                    LOG_PRINTF("\n      ");
                }
            }
            LOG_PRINTF("\n\n      ");

            /* Dump PHY Index Table: */
            b32 = (u32 *)g_soc_table->phy_idx_tbl;
            for(i=0; i<39; i++, b32++) {
                LOG_PRINTF("0x%08x ", *b32);
                if (((i+1)%4) == 0) {
                    LOG_PRINTF("\n      ");
                }
            }
            LOG_PRINTF("\n\n");

            /* Dump PHY L-LENGTH Table: */
            b32 = (u32 *)g_soc_table->phy_llen_tbl;
            LOG_PRINTF("      0x%08x 0x%08x 0x%08x 0x%08x\n",
                b32[0], b32[1], b32[2], b32[3]
            );
            LOG_PRINTF("      0x%08x 0x%08x 0x%08x 0x%08x\n\n",
                b32[4], b32[5], b32[6], b32[7]
            );

            LOG_PRINTF("      ADR_INFO_IDX_ADDR: 0x%08x\n", GET_INFO_IDX_TBL_ADDR);
            LOG_PRINTF("      ADR_INFO_LEN_ADDR: 0x%08x\n", GET_INFO_LEN_TBL_ADDR);
            LOG_PRINTF("      ADR_INFO_MASK: 0x%08x\n", GET_INFO_MASK);
            LOG_PRINTF("      ADR_INFO_DEF_RATE: 0x%08x\n", GET_INFO_DEF_RATE);
            LOG_PRINTF("      ADR_INFO_MRX_OFFSET: 0x%08x\n", GET_INFO_MRX_OFFSET);
            return;
        }

    }
    LOG_PRINTF("Invalid paramters !\n");

}
#endif
#ifdef __TEMPERATURE_VERIFY__
static void Cmd_Temperature (s32 argc, char *argv[])
{
    drv_phy_temperature();
}
#endif

#ifdef __PMU_VERIFY__
static void Cmd_Rtc(s32 argc, char *argv[])
{

    u32 ntime;

    if (strcmp(argv[1], "cali") == 0) {
        drv_rtc_cali();
    }
    else if (strcmp(argv[1], "en") == 0) {
        drv_rtc_set_source(0);          // 0: inside clock , 1: external clock
        drv_rtc_en(1);
    }
    else if (strcmp(argv[1], "now") == 0) {
        drv_rtc_getrealtime(&ntime);
        LOG_PRINTF("RTC time now is %d\n", ntime);
    }
    else {
        LOG_PRINTF("%s(): invalid argv\n", __FUNCTION__);
    }
}

static void Cmd_tm2 (u32 t_ms) {
    volatile u32 tm2_rdy = 0;

    if(t_ms == 0)
        return;

    // set timer
    SET_TM2_TM_INIT_VALUE(t_ms);

    // wait ready
    while(tm2_rdy == 0) {
        tm2_rdy = GET_TM2_TM_INT_STS_DONE;
    }

    // clear timer
    SET_TM2_TM_INT_STS_DONE(1);
}

static void Cmd_Pmu (s32 argc, char *argv[]){

    u32 wake_cnt;
    u64 tmp_long;

    u32 loop_i;
    u32 loop_time;
    u32 loop_on_duty;
    u32 loop_on_time;
    u32 loop_target;

    u32 irq_cfg;

    if (argc == 3 && (strcmp(argv[1], "wake-time") == 0)) {
        wake_cnt = ssv6xxx_atoi(argv[2]);

        drv_pmu_setwake_cnt(wake_cnt);

    }
    else if ((strcmp(argv[1], "init") == 0) && (argc == 2)) {
        drv_pmu_init();
    }
    else if (strcmp(argv[1], "sleep") == 0) {
        drv_pmu_sleep();
    }
    else if ((strcmp(argv[1], "loop") == 0) && (argc == 5)) {

        LOG_PRINTF("Turn off irq\n");
        irq_cfg = REG32(ADR_INT_MASK);
        REG32(ADR_INFO_IDX_ADDR) = 0xffffffff;

        loop_time    = ssv6xxx_atoi(argv[2]);
        loop_on_duty = ssv6xxx_atoi(argv[3]);
        if (loop_on_duty >= 99) {
            loop_on_duty = 99;
        }

        loop_target  = ssv6xxx_atoi(argv[4]);
        if (loop_target == 0) {
            loop_target = 1;
        }

        // ## off-time caliculation
        tmp_long = (u64) loop_time * (100-loop_on_duty) * 67109 * 82;
        tmp_long = tmp_long >> (11 + 13);

        wake_cnt = (u32)tmp_long;
        drv_pmu_setwake_cnt(wake_cnt);

        // ## on-time cali.
        tmp_long = (u64)loop_time * loop_on_duty * 82;
        tmp_long = tmp_long >> 13;

        loop_on_time = (u32)tmp_long;

        LOG_PRINTF("dbg: {loop_on_time, wake_cnt} = {%d, %d}\n", loop_on_time, wake_cnt);
        Cmd_tm2(10);

        for(loop_i = 0; loop_i < loop_target; loop_i++) {
            // LOG_PRINTF("# %4d sleep: \n", loop_i);
            drv_pmu_sleep();
            Cmd_tm2(loop_on_time);
        }
        LOG_PRINTF("# %4d pmu loop ok\n", loop_i);

        LOG_PRINTF("Turn on irq as %08x\n", irq_cfg);
        REG32(ADR_INFO_IDX_ADDR) = irq_cfg;

    }
    else if (strcmp(argv[1], "check") == 0) {
        drv_pmu_chk();
    }
    else {
        printf("%s(): Usage: [wake-time time(x1ms)]/[init]/[sleep]/[loop prd(ms) on_duty(percent) target]/[check]\n");
    }
}
#endif


#if 0
extern void EDCA_PrintQueueInfo();

static void Cmd_EDCA(s32 argc, char *argv[])
{


    if (argc >= 2 && strcmp(argv[1], "sq")==0) {
		EDCA_PrintQueueInfo();
    }
    LOG_PRINTF("Invalid paramters !\n");

}
#endif
extern void rate_control_fix_rate(u32 rate);
extern void rate_control_fix_preamble(u32 preamble);
extern void rate_control_fix_network(u32 network);
extern void rate_control_show_info(u8 staIndex, u8 mode);
extern void rate_control_set_default_rates_mask(u16 dftratemsk);
extern void rate_control_set_variables (u32 mode, u32 param);

static void Cmd_FIXRATE(s32 argc, char *argv[])
{
    u32 input;
     if (argc == 2 )
     {
        input = (u32)ssv6xxx_atoi(argv[1]);
        rate_control_fix_rate(input);
        printf("fix rate[%d]\n",input);
    }
}

static void Cmd_PREAMBLE(s32 argc, char *argv[])
{
    u32 input;
     if (argc == 2 )
     {
        input = (u32)ssv6xxx_atoi(argv[1]);
        rate_control_fix_preamble(input);
        LOG_PRINTF("Preamble[%d]\n",input);
    }
}

static void Cmd_NETWORK(s32 argc, char  *argv[])
{
    u32 input;
     if (argc == 2 )
     {
        input = (u32)ssv6xxx_atoi(argv[1]);
        rate_control_fix_network(input);
        LOG_PRINTF("Network mode[%d]\n",input);
    }
}

static void Cmd_RCINFO(s32 argc, char  *argv[])
{
    u8 sta, info_mode;
    if (argc == 3 )
    {

        if ((strcmp (argv[1], "st") == 0) || (strcmp (argv[1], "tbl") == 0))
        {
            sta = (u8) ssv6xxx_atoi (argv[2]);
            if (sta >= WLAN_MAX_STA)
            {
                printf ("Invalid sta wsid, should be 0~%d\n", (WLAN_MAX_STA - 1));
                return;
            }
            info_mode = (strcmp (argv[1], "st") == 0)?0:1;
            rate_control_show_info (sta, info_mode);
            return;
        }
        else if(strcmp (argv[1], "mask") == 0)
        {
            u16 msk = (u16)ssv6xxx_atoi(argv[2]);
            msk = msk&0x0FFF;
            rate_control_set_default_rates_mask(msk);
            return;
        }
        else if(strcmp (argv[1], "report") == 0)
        {
            u32 val = (u32)ssv6xxx_atoi(argv[2]);
            rate_control_set_variables(RC_OPT_SET_RPTINT, val);
            return;
        }
        else if(strcmp (argv[1], "sample") == 0)
        {
            u32 val = (u32)ssv6xxx_atoi(argv[2]);
            rate_control_set_variables(RC_OPT_SET_SAMPLEINT, val);
            return;
        }
        else if(strcmp (argv[1], "upperfastestb") == 0)
        {
            u32 val = (u32)ssv6xxx_atoi(argv[2]);
            rate_control_set_variables(RC_OPT_SET_UP_PER_FASTEST_B, val);
            return;
        }
        else if(strcmp (argv[1], "direct") == 0)
        {            
            u32 val = (u32)ssv6xxx_atoi(argv[2]);
            rate_control_set_variables(RC_OPT_DIRECT_DWGRADE_RATE, val);
            return;

        }
        else if(strcmp (argv[1], "rtscts") == 0)
        {            
            u32 val = (u32)ssv6xxx_atoi(argv[2]);
            rate_control_set_variables(RC_OPT_FORCE_RTS_CTS,val);
            return;

        }
    }
    else if(argc == 2)
    {
        if (strcmp(argv[1], "msg") == 0)
        {
            rate_control_show_info(0, 2);
            return;
        }
        else if (strcmp(argv[1], "samplemsg") == 0)
        {
            rate_control_show_info(0, 3);
            return;
        }
        else if(strcmp (argv[1], "reportnsample") == 0)
        {
            rate_control_set_variables(RC_OPT_SET_FEATURE, RC_FEAT_REPORTSAMPLE);
            return;
        }
        else if(strcmp (argv[1], "resent") == 0)
        {
            rate_control_set_variables(RC_OPT_SET_FEATURE, RC_FEAT_RESEND_FAILREPORT);
            return;
        }
    }
    else if(argc == 4)
    {
        if(strcmp (argv[1], "per") == 0)
        {
            u8 up_pf = (u8)ssv6xxx_atoi(argv[2]);
            u8 down_pf = (u8)ssv6xxx_atoi(argv[3]);
            u32 val = up_pf<<16|down_pf;
            if(up_pf < down_pf)
                rate_control_set_variables(RC_OPT_SET_PER, val);
            else if ((up_pf == 0) && (down_pf == 0))
                rate_control_set_variables(RC_OPT_SET_PER, 0);
            else
                printf("buggy parameters, up_pf must be smaller than down_pf\n");
            return;
        }
    }

    printf("Invalid param, rcinfo msg/rcinfo samplemsg/rcinfo st [wsid]/rcinfo tbl [wsid]/rcinfo probcnt [cnt]/rcinfo resent/rcinfo simplealg/rcinfo per [UP_PER] [DOWN_PER]/rcinfo upperfastestb [PER]\n");
}

static void Cmd_ReadQueue (s32 argc, char  *argv[])
{
    ssv6xxx_queue_status();
}

#if 0
#ifndef BIT
#define BIT(x) (1 << (x))
#endif

static void Cmd_ReadResource(s32 argc, char  *argv[]){

    //printf("TX[%d]RX[%d]AVA[%d]\n",GET_TX_ID_ALC_LEN,GET_RX_ID_ALC_LEN,GET_AVA_TAG);
    printf("\n[TAG]  TX0 - TX1 - TX2 - TX3 - TX4\n");
    printf("PAUSE  %3d - %3d - %3d - %3d - %3d\n", GET_MTX_HALT_Q_MB&BIT(0),
                                                   GET_MTX_HALT_Q_MB&BIT(1),
                                                   GET_MTX_HALT_Q_MB&BIT(2),
                                                   GET_MTX_HALT_Q_MB&BIT(3),
                                                   GET_MTX_HALT_Q_MB&BIT(4));
}
#endif

void DumpSecureSetting(void)
{
    /*
        typedef enum sec_type_en {
        SECURITY_NONE,
        SECURITY_WEP40,
        SECURITY_WEP104,
        SECURITY_TKIP,
        SECURITY_CCMP,
        } sec_type;
    */
    const char *str[]={"NONE","WEP40","WEP104","TKIP","CCMP"};

    printf("PAIR=%s, GROUP=%s \r\n",str[GET_PAIR_SCRT],str[GET_GRP_SCRT]); //GET_PAIR_SCRT
    //hex_dump((s8 *)pGSRAM_KEY,(s32)sizeof(SRAM_KEY));

}
#if 0
static void Cmd_Monitor (s32 argc, char  *argv[])
{
    u32 period;
    u32 repeat;

    if((argc!=2)&&(argc!=4)){
        goto usage;
    }

    if(argc==4){
        repeat = (u32)ssv6xxx_atoi_base(argv[2], 16);
        period = (u32)ssv6xxx_atoi_base(argv[3], 16);
        period=(repeat==0)?100:period;
        mac_monitor_start(repeat,period);
        return;
    }

    if(argc==2){
        mac_monitor_stop();
        return;
    }


usage:
    printf("usage: \r\n");
    printf("     monitor on [repeat] [period(ms)] \r\n");
    printf("     monitor off \r\n");
    printf("     repeat=0 means 'never stop' \r\n");
    return;
}
#endif

#ifdef ENABLE_TX_QUEUE_STATS
void start_tx_q_stats (u32 count);
void show_tx_q_stats (void);

static void Cmd_StartTxQueueSt (s32 argc, char  *argv[])
{
	u32 sample_count = 10000;

    if (argc > 1) {
    	sample_count = ssv6xxx_atoi(argv[1]);
    }

	LOG_PRINTF("Starting TX queue statistics with %d samples.\n", sample_count);

	start_tx_q_stats(sample_count);
} // end of - Cmd_StartTxQueueSt -


static void Cmd_ShowTxQueueSt (s32 argc, char  *argv[])
{
	show_tx_q_stats();
} // end of - Cmd_StartTxQueueSt -

#endif // ENABLE_TX_QUEUE_STATS

#ifdef FLASH_MCU

static void Cmd_FlashReadStatus(s32 argc, char  *argv[])
{
    printf("status=%x\n",drv_flash_readSatus());
}

static void Cmd_FlashWriteEn(s32 argc, char  *argv[])
{
    drv_flash_writeEn();
}


static void Cmd_FlashRead(s32 argc, char  *argv[])
{
    u32 read_addr=0x000000,read_size=4;
    int i=0;
    u8 rbuffer[8]={0};

    if (argc < 2)
    {
        return;
    }

    read_addr = (u32)ssv6xxx_atoi_base(argv[1], 16);
    if (argc > 2)
        read_size = (u32)ssv6xxx_atoi(argv[2]);
    else
        read_size = 4;
    drv_flash_read(read_addr,&rbuffer[0],read_size);

    for(i=0;i<read_size;i++)
    {
        printf("addr[%06x]=%x\n",read_addr+i,rbuffer[i]);
    }
} // end of - Cmd_Read -

static void Cmd_FlashWrite(s32 argc, char  *argv[])
{
    volatile u32 write_addr;
    u8 wdata[4]={0};
    u32 value;

    write_addr = (volatile u32)ssv6xxx_atoi_base(argv[1],16);
    value = (volatile u32)ssv6xxx_atoi_base(argv[2],16);

    wdata[0] = value & 0xff;
    wdata[1] = value>>8 & 0xff;
    wdata[2] = value>>16 & 0xff;
    wdata[3] = value>>24 & 0xff;

    //printf("addrs=%x\n",write_addr);
    //printf("value=%x\n",value);
    drv_flash_pageProgram(write_addr,wdata,4);
}


static void Cmd_FlashSErase(s32 argc, char  *argv[])
{
    static u32 erase_addr = 0x000000;

    erase_addr = (u32)ssv6xxx_atoi_base(argv[1], 16);

    drv_flash_sectorErase(erase_addr);
}
static void Cmd_FlashBErase(s32 argc, char  *argv[])
{
    static u32 erase_addr = 0x000000;

    erase_addr = (u32)ssv6xxx_atoi_base(argv[1], 16);

    drv_flash_sectorErase(erase_addr);
}
#endif


#if (AMPDU_TEST == 1)
extern u32 ampdu_rx_cnt;
extern u32 AMPDU_ARY[64];
extern u32 g_ampdu_static;
extern u32 g_ampdu_static_cnt;

void print_ampdu_reg()
{
		LOG_PRINTF("ampdu sniffer [%d]\n", GET_AMPDU_SNIFFER);

		LOG_PRINTF("ampdu stat [%d] cnt[%d]\n", g_ampdu_static, g_ampdu_static_cnt);
		LOG_PRINTF("ampdu stat reg AMPDU[%d] ampdu_pass[%d] ampdu_fail[%d]\n", GET_MIB_AMPDU, GET_DBG_AMPDU_PASS, GET_DBG_AMPDU_FAIL);
		LOG_PRINTF("phytotxerr [%d] GET_MRX_ALC_FAIL[%d]\n", GET_MRX_MISS, GET_MRX_ALC_FAIL);

		LOG_PRINTF("phy2rx fifo full [%d] rx to pkt buffer fifo full[%d]\n", GET_MTX_WSID0_SUCC, GET_MTX_WSID1_SUCC);
		LOG_PRINTF("mail box  full [%d] \n", GET_MTX_WSID2_SUCC);

		LOG_PRINTF("ampdu ampdu_rx_cnt [%d]\n\n", ampdu_rx_cnt);
}

static void Cmd_ampdu(s32 argc, char  *argv[])
{
    u32 i;

    if (argc == 1)
    {
        LOG_PRINTF("ampdu sniffer [1|0]\n");
        LOG_PRINTF("ampdu stat [on|off|reset]\n");
        LOG_PRINTF("ampdu del\n");
        LOG_PRINTF("ampdu list\n");
        LOG_PRINTF("\n");
        return;
    }

	if(argc == 3 && strcmp(argv[1], "sniffer")==0)
	{
		LOG_PRINTF("Set AMPDU Sniffer to %d\n", ssv6xxx_atoi(argv[2]));
		SET_AMPDU_SNIFFER(ssv6xxx_atoi(argv[2]));
		return;
	}
	else if(argc == 3 && strcmp(argv[1], "stat")==0)
	{
		if(strcmp(argv[2], "on")==0) {

			LOG_PRINTF("enable AMPDU statistic\n");

			g_ampdu_static = 1;

			SET_MIB_LEN_FAIL(0xffff);

			g_ampdu_static_cnt = 0;
			SET_MISC_PKT_CLS_MIB_EN(0);
			SET_MISC_PKT_CLS_MIB_EN(1);

			SET_G0_PKT_CLS_MIB_EN(0);
			SET_G0_PKT_CLS_MIB_EN(1);

			SET_G1_PKT_CLS_MIB_EN(0);
			SET_G1_PKT_CLS_MIB_EN(1);

			SET_MIB_AMPDU(GET_MIB_AMPDU);
			print_ampdu_reg();


		}else if(strcmp(argv[2], "off")==0) {

			LOG_PRINTF("disable AMPDU statistic\n");
			g_ampdu_static = 0;
		}else if(strcmp(argv[2], "reset")==0) {
			g_ampdu_static_cnt = 0;

			SET_MISC_PKT_CLS_MIB_EN(0);
			SET_MISC_PKT_CLS_MIB_EN(1);

			SET_G0_PKT_CLS_MIB_EN(0);
			SET_G0_PKT_CLS_MIB_EN(1);

			SET_G1_PKT_CLS_MIB_EN(0);
			SET_G1_PKT_CLS_MIB_EN(1);

			SET_MIB_AMPDU(GET_MIB_AMPDU);

			LOG_PRINTF("reset AMPDU statistic cnt to 0\n");
			print_ampdu_reg();
		}
	}
	else
	{
	}

    if (argc != 2)
		return;

	if(strcmp(argv[1], "del")==0) {
		LOG_PRINTF("=============APMDU Del=======================\n");
		taskENTER_CRITICAL();
		for(i=0;i<ampdu_rx_cnt; i++)
		{
			PBUF_MFree((void*)AMPDU_ARY[i]);
			AMPDU_ARY[i]=0;
		}
		ampdu_rx_cnt = 0;
		taskEXIT_CRITICAL();

    }
	else if(strcmp(argv[1], "list")==0) {
		LOG_PRINTF("=============APMDU Info=======================\n");

		print_ampdu_reg();

		LOG_PRINTF("=============APMDU List=======================\n");
		for(i=0;i<ampdu_rx_cnt; i++)
		{
			LOG_PRINTF("%d:%08x\n",i, AMPDU_ARY[i]);
		}
	}
	else
	{
		;
	}
}



//-----------------------------------------------------------------------------
//BAR Frame
#define IEEE80211_FTYPE_MGMT		0x0000
#define IEEE80211_FTYPE_CTL			0x0004
#define IEEE80211_FTYPE_DATA		0x0008

#define IEEE80211_QOS_CTL_LEN		2
#define IEEE80211_QOS_CTL_TID_MASK	0x000F
#define IEEE80211_QOS_CTL_TAG1D_MASK	0x0007

#define IEEE80211_STYPE_QOS_DATA		0x0080


/* data */
#define IEEE80211_STYPE_DATA			0x0000
#define IEEE80211_STYPE_DATA_CFACK		0x0010
#define IEEE80211_STYPE_DATA_CFPOLL		0x0020
#define IEEE80211_STYPE_DATA_CFACKPOLL		0x0030
#define IEEE80211_STYPE_NULLFUNC		0x0040
#define IEEE80211_STYPE_CFACK			0x0050
#define IEEE80211_STYPE_CFPOLL			0x0060
#define IEEE80211_STYPE_CFACKPOLL		0x0070
#define IEEE80211_STYPE_QOS_DATA		0x0080
#define IEEE80211_STYPE_QOS_DATA_CFACK		0x0090
#define IEEE80211_STYPE_QOS_DATA_CFPOLL		0x00A0
#define IEEE80211_STYPE_QOS_DATA_CFACKPOLL	0x00B0
#define IEEE80211_STYPE_QOS_NULLFUNC		0x00C0
#define IEEE80211_STYPE_QOS_CFACK		0x00D0
#define IEEE80211_STYPE_QOS_CFPOLL		0x00E0
#define IEEE80211_STYPE_QOS_CFACKPOLL		0x00F0

#define IEEE80211_FCTL_VERS		0x0003
#define IEEE80211_FCTL_FTYPE		0x000c
#define IEEE80211_FCTL_STYPE		0x00f0
#define IEEE80211_FCTL_TODS		0x0100
#define IEEE80211_FCTL_FROMDS		0x0200
#define IEEE80211_FCTL_WDS			0x0300
#define IEEE80211_FCTL_MOREFRAGS	0x0400
#define IEEE80211_FCTL_RETRY		0x0800
#define IEEE80211_FCTL_PM		0x1000
#define IEEE80211_FCTL_MOREDATA		0x2000
#define IEEE80211_FCTL_PROTECTED	0x4000
#define IEEE80211_FCTL_ORDER		0x8000




/* control */
#define IEEE80211_STYPE_BACK_REQ	0x0080
#define IEEE80211_STYPE_BACK		0x0090
#define IEEE80211_STYPE_PSPOLL		0x00A0
#define IEEE80211_STYPE_RTS			0x00B0
#define IEEE80211_STYPE_CTS			0x00C0
#define IEEE80211_STYPE_ACK			0x00D0
#define IEEE80211_STYPE_CFEND		0x00E0
#define IEEE80211_STYPE_CFENDACK	0x00F0

/* 802.11 BAR control masks */
#define IEEE80211_BAR_CTRL_ACK_POLICY_NORMAL	0x0000
#define IEEE80211_BAR_CTRL_MULTI_TID		0x0002
#define IEEE80211_BAR_CTRL_CBMTID_COMPRESSED_BA	0x0004
#define IEEE80211_BAR_CTRL_TID_INFO_MASK	0xf000
#define IEEE80211_BAR_CTRL_TID_INFO_SHIFT	12

#define IEEE80211_SN_SHIFT							4


#define ETH_ALEN 6

#include <mbox/drv_mbox.h>
#include <hwmac/drv_mac.h>



struct ieee80211_bar {
	__le16 frame_control;
	__le16 duration;
	__u8 ra[6];
	__u8 ta[6];
	__le16 control;
	__le16 start_seq_num;
} ;


void _send_bar(u8 *ra, u16 tid, u16 ssn)
{
	struct ieee80211_bar *bar;
	u16 bar_control = 0;
	PKT_TxInfo *pTxInfo = PBUF_MAlloc(sizeof(struct ieee80211_bar), SOC_PBUF);


	if (!pTxInfo)
		ASSERT(FALSE);


	memset((void *)pTxInfo, 0, sizeof(PKT_TxInfo));
	pTxInfo->len          = sizeof(struct ieee80211_bar);
	pTxInfo->c_type       = M0_TXREQ;
	pTxInfo->txq_idx	  = 3; //VO
	pTxInfo->f80211 	  = 1;



	bar = ( struct ieee80211_bar*)((u8 *)pTxInfo + drv_mac_get_pbuf_offset());

	memset(bar, 0, sizeof(*bar));
	bar->frame_control = (IEEE80211_FTYPE_CTL | IEEE80211_STYPE_BACK_REQ | IEEE80211_FCTL_FROMDS);

	memcpy(bar->ra, ra, ETH_ALEN);
	drv_mac_get_sta_mac(bar->ta);//

	bar_control |= (u16)IEEE80211_BAR_CTRL_ACK_POLICY_NORMAL;
	bar_control |= (u16)IEEE80211_BAR_CTRL_CBMTID_COMPRESSED_BA;
	bar_control |= (u16)(tid << IEEE80211_BAR_CTRL_TID_INFO_SHIFT);
	bar->control = (bar_control);
	bar->start_seq_num = (u16)(ssn)<<IEEE80211_SN_SHIFT;

	TX_FRAME((u32)pTxInfo);
}


//It is for Ap mode used
static void Cmd_BAR(s32 argc, char  *argv[])
{
	u8 ra[6];

	if(argc < 8 )
	{
		LOG_PRINTF("bar RA(mac_address) tid ssn\n");
		LOG_PRINTF("bar 0x10 0x11 0x22 0x33 0x44 0x55 tid ssn\n");
		return;
	}

	ra[0] = ssv6xxx_atoi(argv[1]);
	ra[1] = ssv6xxx_atoi(argv[2]);
	ra[2] = ssv6xxx_atoi(argv[3]);
	ra[3] = ssv6xxx_atoi(argv[4]);
	ra[4] = ssv6xxx_atoi(argv[5]);
	ra[5] = ssv6xxx_atoi(argv[6]);

	_send_bar(ra, ssv6xxx_atoi(argv[7]), ssv6xxx_atoi(argv[8]));
}








static void Cmd_BA(s32 argc, char  *argv[])
{





	if(strcmp(argv[1], "ta")==0) {

		const u32   *mac_31_0;
		const u16   *mac_47_32;
		u8 addr[6];

		drv_mac_get_bssid(addr);

		//drv_mac_get_wsid_peer_mac(0, addr);

		//addr[0] = ssv6xxx_atoi(argv[2]);
		//addr[1] = ssv6xxx_atoi(argv[3]);
		//addr[2] = ssv6xxx_atoi(argv[4]);
		//addr[3] = ssv6xxx_atoi(argv[5]);
		//addr[4] = ssv6xxx_atoi(argv[6]);
		//addr[5] = ssv6xxx_atoi(argv[7]);


		mac_31_0 = (u32 *)(addr+0);
		mac_47_32 = (u16 *)(addr+4);

		SET_BA_TA_31_0(*mac_31_0);
		SET_BA_TA_47_32(*mac_47_32);


		LOG_PRINTF("Set BA reg TA Addr %02x:%02x:%02x:%02x:%02x:%02x\n",addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);
		return;
    }else if(strcmp(argv[1], "tid")==0 && argc == 3) {

		SET_BA_TID(ssv6xxx_atoi(argv[2]));
		LOG_PRINTF("Set BA reg tid %02d\n", ssv6xxx_atoi(argv[2]));
		return;
	}else if(strcmp(argv[1], "ssn")==0 && argc == 3) {

		SET_BA_ST_SEQ(ssv6xxx_atoi(argv[2]));
		LOG_PRINTF("Set BA reg SSN %04d\n", ssv6xxx_atoi(argv[2]));
		return;
	}else if(strcmp(argv[1], "ra")==0) {

		const u32   *mac_31_0;
		const u16   *mac_47_32;
		u8 addr[6];

		drv_mac_get_sta_mac(addr);

		//addr[0] = ssv6xxx_atoi(argv[2]);
		//addr[1] = ssv6xxx_atoi(argv[3]);
		//addr[2] = ssv6xxx_atoi(argv[4]);
		//addr[3] = ssv6xxx_atoi(argv[5]);
		//addr[4] = ssv6xxx_atoi(argv[6]);
		//addr[5] = ssv6xxx_atoi(argv[7]);


		mac_31_0 = (u32 *)(addr+0);
		mac_47_32 = (u16 *)(addr+4);

		SET_ACK_GEN_RA_31_0(*mac_31_0);
		SET_ACK_GEN_RA_47_32(*mac_47_32);


		LOG_PRINTF("Set BA reg ACK_GEN_RA Addr %02x:%02x:%02x:%02x:%02x:%02x\n",addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);
		return;

	}else if(strcmp(argv[1], "gen")==0) {

		u8 enable = ssv6xxx_atoi(argv[2]);

		//(1 == enable)?SET_ACK_GEN_EN(2):SET_ACK_GEN_EN(0);
		if(enable)
			SET_ACK_GEN_EN(2);
		else
			SET_ACK_GEN_EN(0);

		LOG_PRINTF("Set BA Gen %d\n", ssv6xxx_atoi(argv[2]));
		return;

	}else if(strcmp(argv[1], "info")==0 && argc == 2) {


		u8 temp[6];
		u32   *mac_31_0 = (u32 *)(temp+0);
		u16   *mac_47_32 = (u16 *)(temp+4);

		u32 sb0 = GET_BA_SB0;
		u32 sb1 = GET_BA_SB1;
		u16 tid = GET_BA_TID;
		u16 ssn = GET_BA_ST_SEQ;


	    *mac_31_0 = GET_BA_TA_31_0;
    	*mac_47_32 = (u16)GET_BA_TA_47_32;


		LOG_PRINTF("BA Info TA Addr:%02x:%02x:%02x:%02x:%02x:%02x  SSN:%d  TID:%d  SB:%08x %08x BA_CTRL:%d\n",temp[0],temp[1],temp[2],temp[3],temp[4],temp[5],
			ssn, tid, sb1, sb0, GET_BA_CTRL);

		return;

	}else if(strcmp(argv[1], "ctrl")==0 && argc == 3) {

		u8 ba_ctrl = ssv6xxx_atoi(argv[2]);
		SET_BA_CTRL(ba_ctrl&0x3);
		LOG_PRINTF("BA_CTRL %d\n", GET_BA_CTRL);

		return;
	}else
	{;}




	//if (argc < 2)
	{
		LOG_PRINTF("1. ba ta [mac_address]\n");
		LOG_PRINTF("2. ba tid [value]	\n");
		LOG_PRINTF("3. ba ssn [value]	\n");
		LOG_PRINTF("4. ba ra [mac_address]\n");
		LOG_PRINTF("5. ba gen [0|1]\n");
		LOG_PRINTF("6. ba ctrl [0|1|2|3]\n");
		LOG_PRINTF("7. ba info\n");



		//LOG_PRINTF("bar 0x10 0x11 0x22 0x33 0x44 0x55 tid ssn\n");

		return;
	}




}






struct ieee80211_qos_hdr {
	__le16 frame_control;
	__le16 duration_id;
	u8 addr1[6];
	u8 addr2[6];
	u8 addr3[6];
	__le16 seq_ctrl;
	__le16 qos_ctrl;
} ;



#include <mbox/drv_mbox.h>


//It is for Ap mode used
void _null_qos_func(u8 *da, u16 tid, u16 sn)//ap mode
{

	struct ieee80211_qos_hdr* hdr;
	PKT_TxInfo *pTxInfo = (PKT_TxInfo *)PBUF_MAlloc(sizeof(struct ieee80211_qos_hdr), SOC_PBUF);


	if (!pTxInfo)
		ASSERT(FALSE);

	memset((void *)pTxInfo, 0, sizeof(PKT_TxInfo));
	pTxInfo->len          = sizeof(struct ieee80211_qos_hdr);
	pTxInfo->c_type       = M0_TXREQ;
	pTxInfo->txq_idx	  = 3; //VO
	pTxInfo->f80211 	  = 1;


    pTxInfo->unicast  	= 1;
    pTxInfo->hdr_len	= 26;
    pTxInfo->hdr_offset = drv_mac_get_pbuf_offset();

	hdr = ( struct ieee80211_qos_hdr*)((u8 *)pTxInfo + drv_mac_get_pbuf_offset());

	memcpy(hdr->addr1, da, ETH_ALEN);	//RA
	drv_mac_get_sta_mac(hdr->addr2);	//TA
	drv_mac_get_sta_mac(hdr->addr3);	//SA

	hdr->frame_control = (IEEE80211_FTYPE_DATA |
		IEEE80211_STYPE_NULLFUNC | IEEE80211_FCTL_FROMDS );	//AP to STA

	hdr->duration_id = 216;

	hdr->seq_ctrl = (u16)(sn)<<IEEE80211_SN_SHIFT;
	hdr->qos_ctrl = tid;//Normal Ack

	//ENG_MBOX_SEND((u32)M_ENG_TX_EDCA2,(u32)pTxInfo);
	//msg_evt_post(MBOX_SOFT_MAC, MsgEv);
	{
		MsgEvent *MsgEv;
		MsgEv = msg_evt_alloc();
    	ASSERT(MsgEv != NULL);

	    /* Fill MsgEvent Content: */
    	MsgEv->MsgType  = MEVT_PKT_BUF;
	    MsgEv->MsgData  = (u32)pTxInfo;
	    MsgEv->MsgData1 = 0;
    	MsgEv->MsgData2 = 0;
	    MsgEv->MsgData3 = 0;
    	msg_evt_post(MBOX_SOFT_MAC, MsgEv);
	}
}

static void Cmd_NULL(s32 argc, char  *argv[])
{
	u8 da[6];

	if(argc < 8 )
	{
		LOG_PRINTF("null RA(mac_address) tid sn\n");
		LOG_PRINTF("bar 0x10 0x11 0x22 0x33 0x44 0x55 tid sn\n");
		return;
	}

	da[0] = ssv6xxx_atoi(argv[1]);
	da[1] = ssv6xxx_atoi(argv[2]);
	da[2] = ssv6xxx_atoi(argv[3]);
	da[3] = ssv6xxx_atoi(argv[4]);
	da[4] = ssv6xxx_atoi(argv[5]);
	da[5] = ssv6xxx_atoi(argv[6]);

	_null_qos_func(da, ssv6xxx_atoi(argv[7]), ssv6xxx_atoi(argv[8]));
}

//-----------------------------------------------------------------------------
//BAR Frame

#endif//#if (AMPDU_TEST == 1)

#if 0
static void Cmd_BACKOFF(s32 argc, char  *argv[])
{
    if (argc==2 &&  strcmp(argv[1], "on")==0)
    {
        LOG_PRINTF("Backoff on!!\n");
        SET_TXQ0_MTX_Q_ECWMIN(5);
        SET_TXQ0_MTX_Q_ECWMAX(0xa);
        SET_TXQ1_MTX_Q_ECWMIN(5);
        SET_TXQ1_MTX_Q_ECWMAX(0xa);
        SET_TXQ2_MTX_Q_ECWMIN(5);
        SET_TXQ2_MTX_Q_ECWMAX(0xa);
        SET_TXQ3_MTX_Q_ECWMIN(5);
        SET_TXQ3_MTX_Q_ECWMAX(0xa);

        return;
    }
    else if (argc==2 && strcmp(argv[1], "off")==0)
    {
        LOG_PRINTF("backoff off!!\n");
        SET_TXQ0_MTX_Q_ECWMIN(0);
        SET_TXQ0_MTX_Q_ECWMAX(0);
        SET_TXQ1_MTX_Q_ECWMIN(0);
        SET_TXQ1_MTX_Q_ECWMAX(0);
        SET_TXQ2_MTX_Q_ECWMIN(0);
        SET_TXQ2_MTX_Q_ECWMAX(0);
        SET_TXQ3_MTX_Q_ECWMIN(0);
        SET_TXQ3_MTX_Q_ECWMAX(0);

        return;
    }
    else
    {
        printf("backoff on/off \n");
    }
}
#endif
#if 0
u8 on_fire_flag = 0;
static void Cmd_ONFIRE(s32 argc, char  *argv[])
{
        if (argc==2 &&  strcmp(argv[1], "on")==0)
        {
            LOG_PRINTF("fire on!!\n");
			on_fire_flag = 1;
            SET_TXQ0_MTX_Q_ECWMIN(0);
            SET_TXQ0_MTX_Q_ECWMAX(0);
            SET_TXQ1_MTX_Q_ECWMIN(0);
            SET_TXQ1_MTX_Q_ECWMAX(0);
            SET_TXQ2_MTX_Q_ECWMIN(0);
            SET_TXQ2_MTX_Q_ECWMAX(0);
            SET_TXQ3_MTX_Q_ECWMIN(0);
            SET_TXQ3_MTX_Q_ECWMAX(0);

            SET_MTX_FORCE_CS_IDLE(1);
            SET_MTX_FORCE_BKF_RXEN0(1);
            SET_MTX_FORCE_DMA_RXEN0(1);
            SET_MTX_FORCE_RXEN0(1);

            SET_TXQ0_MTX_Q_SRC_LIMIT(1);
            SET_TXQ0_MTX_Q_LRC_LIMIT(1);
            SET_TXQ1_MTX_Q_SRC_LIMIT(1);
            SET_TXQ1_MTX_Q_LRC_LIMIT(1);
            SET_TXQ2_MTX_Q_SRC_LIMIT(1);
            SET_TXQ2_MTX_Q_LRC_LIMIT(1);
            SET_TXQ3_MTX_Q_SRC_LIMIT(1);
            SET_TXQ3_MTX_Q_LRC_LIMIT(1);
            return;
        }
        else if (argc==2 && strcmp(argv[1], "off")==0)
        {
            LOG_PRINTF("fire off!!\n");
			on_fire_flag = 0;
            SET_TXQ0_MTX_Q_ECWMIN(5);
            SET_TXQ0_MTX_Q_ECWMAX(0xa);
            SET_TXQ1_MTX_Q_ECWMIN(5);
            SET_TXQ1_MTX_Q_ECWMAX(0xa);
            SET_TXQ2_MTX_Q_ECWMIN(5);
            SET_TXQ2_MTX_Q_ECWMAX(0xa);
            SET_TXQ3_MTX_Q_ECWMIN(5);
            SET_TXQ3_MTX_Q_ECWMAX(0xa);

            SET_MTX_FORCE_CS_IDLE(0);
            SET_MTX_FORCE_BKF_RXEN0(0);
            SET_MTX_FORCE_DMA_RXEN0(0);
            SET_MTX_FORCE_RXEN0(0);

            SET_TXQ0_MTX_Q_SRC_LIMIT(7);
            SET_TXQ0_MTX_Q_LRC_LIMIT(4);
            SET_TXQ1_MTX_Q_SRC_LIMIT(7);
            SET_TXQ1_MTX_Q_LRC_LIMIT(4);
            SET_TXQ2_MTX_Q_SRC_LIMIT(7);
            SET_TXQ2_MTX_Q_LRC_LIMIT(4);
            SET_TXQ3_MTX_Q_SRC_LIMIT(7);
            SET_TXQ3_MTX_Q_LRC_LIMIT(4);
            return;
        }
        else
        {
            printf("fire on/off \n");
        }
    }
#endif

#if 0
static void Cmd_BUFFER(s32 argc, char  *argv[])
{
    u32 tx_page,rx_page;
    if (argc==3)
    {
        tx_page = ssv6xxx_atoi(argv[1]);
        rx_page = ssv6xxx_atoi(argv[2]);
        SET_ID_TX_LEN_THOLD(tx_page);
        SET_ID_RX_LEN_THOLD(rx_page);
        printf("TX buffer[%d] RX buffer[%d]!\n",tx_page,rx_page);
        return;
    }
    else
    {
        printf("buffer [tx page number] [rx page number]\n");
    }
}
#endif
#if 0
void dump_mailbox_dbg (int num);
static void Cmd_DumpMboxDbg (s32 argc, char  *argv[])
{
	dump_mailbox_dbg(20);
}
#endif
/* ---------------------- Registered CMDs to CMD Table ---------------------- */
#ifdef ENABLE_CLI_CMD_DUMP_REG
void reg_dump_bank()
{
    s32 i,n;

    for (n=0; n< BANK_COUNT; n++)
    {
        LOG_PRINTF("bank [%d] (%s) : size = %d \n", n, STR_BANK_SSV6200[n], SIZE_BANK_SSV6200[n]);
        if((BASE_BANK_SSV6200[n]==HCI_REG_BASE) || (BASE_BANK_SSV6200[n]==MRX_REG_BASE) ||
        (BASE_BANK_SSV6200[n]==HIF_INFO_BASE) || (BASE_BANK_SSV6200[n]==PHY_RATE_INFO_BASE) ||
        (BASE_BANK_SSV6200[n]==AMPDU_REG_BASE) || (BASE_BANK_SSV6200[n]==MAC_GLB_SET_BASE) ||
        (BASE_BANK_SSV6200[n]==TXQ0_MT_Q_REG_CSR_BASE) || (BASE_BANK_SSV6200[n]==TXQ1_MT_Q_REG_CSR_BASE) ||
        (BASE_BANK_SSV6200[n]==TXQ2_MT_Q_REG_CSR_BASE) || (BASE_BANK_SSV6200[n]==TXQ3_MT_Q_REG_CSR_BASE) ||
        (BASE_BANK_SSV6200[n]==TXQ4_MT_Q_REG_CSR_BASE) ||
        (BASE_BANK_SSV6200[n]==MT_REG_CSR_BASE))
        {
            for (i=0; i< (s32)SIZE_BANK_SSV6200[n]; i+=4)
            {
                LOG_PRINTF("%08x %08x\n", BASE_BANK_SSV6200[n]+i, *(u32 *)(BASE_BANK_SSV6200[n]+i));
            }
        }
    }
}
#endif

#if 0
static void Cmd_history(s32 argc, s8 *argv[])
{
    u8 i=0;
    for(i=0;i<CLI_HISTORY_NUM;i++){
        printf("%s\r\n",&gCmdHistoryBuffer[i][0]);
    }
}
#endif
#if 0
static void Cmd_SWQueue(s32 argc, char *argv[])
{
    s8 i=0;
//    s32 *p=NULL;
//    s8 err=0;


    if(argc==2){
        if(0==strcmp("list",argv[1])){
            ssv6xxx_sw_queue_status();
        }

    }

    else if(argc==3){
        if(0==strcmp("lock-out",argv[1])){
            ssv6xxx_SWQOutLock(ssv6xxx_atoi(argv[2]),TX_SW_QUEUE);
            printf("lock the output of dq[%d]\n",ssv6xxx_atoi(argv[2]));
            return;
        }else if(0==strcmp("unlock-out",argv[1])){
            ssv6xxx_SWQOutUnLock(ssv6xxx_atoi(argv[2]),TX_SW_QUEUE);
            printf("unlock the output of dq[%d]\n",ssv6xxx_atoi(argv[2]));
            return;
        }else if(0==strcmp("lock-int",argv[1])){
            ssv6xxx_SWQIntLock(ssv6xxx_atoi(argv[2]),TX_SW_QUEUE);
            printf("lock the intput of dq[%d]\n",ssv6xxx_atoi(argv[2]));
            return;
        }else if(0==strcmp("unlock-int",argv[1])){
            ssv6xxx_SWQIntUnLock(ssv6xxx_atoi(argv[2]),TX_SW_QUEUE);
            printf("unlock the intput of dq[%d]\n",ssv6xxx_atoi(argv[2]));
            return;
        }else if(0==strcmp("deq",argv[1])){ //dequeue
            if(-1 == ssv6xxx_SWQDequeue(ssv6xxx_atoi(argv[2]), TX_SW_QUEUE, (void **)&p)) err++;

            if(err!=0){
               printf("dq[%d] dequeue fail\n",ssv6xxx_atoi(argv[2]));
            }else{
               printf("dq[%d] dequeue: %d\n",ssv6xxx_atoi(argv[2]),(u32)*p);
               if(p!=NULL) PBUF_MFree(p);
            }

            return;
        }

    }else if(argc==4){
        if(0==strcmp("add",argv[1])){
            p=PBUF_MAlloc(sizeof(int) ,SOC_PBUF);

            if(p==NULL) err++;

            if(err==0){
                *p=ssv6xxx_atoi(argv[3]);
                if(-1 == ssv6xxx_SWQEnqueue(ssv6xxx_atoi(argv[2]),TX_SW_QUEUE,p)) err++;
            }

            if(err!=0){
                printf("dq[%d] enqueue fail\n",ssv6xxx_atoi(argv[2]));
            }else{
                printf("dq[%d] enqueue: %d\n",ssv6xxx_atoi(argv[2]),*p);
            }
            return;
        }else if(0==strcmp("unlock2",argv[1])){
            ssv6xxx_SWQOutConUnLock(ssv6xxx_atoi(argv[2]),TX_SW_QUEUE,ssv6xxx_atoi(argv[3]));
            printf("condictional unlock dq[%d] % packets\n",ssv6xxx_atoi(argv[2]),ssv6xxx_atoi(argv[3]));
            return;
        }

    }
    printf("wrong usage \n");
    return;
}
#endif
extern u32 g_heap_start;
extern u32 g_heap_size;
extern u32 __mem_start__;
extern u32 __mem_end__;
extern u32 __end_of_text__;
extern u32 __data_beg__;
extern u32 __data_end__;
extern u32 __dmsg_beg__;
extern u32 __dmsg_end__;
extern u32 __bss_beg__;
extern u32 _bss_end__;
extern u32 _g_heap_start;
extern u32 _g_heap_size;
extern u32 msgevt_addr_beg;
extern u32 msgevt_addr_end;
extern struct task_info_st g_soc_task_info[];
static void Cmd_mem(s32 argc, char *argv[])
{
    printf("mem:  %08x - %08x\n", (u32)&__mem_start__, (u32)&__mem_end__);
    printf(" -- %s:  %08x - %08x\n", "text",(u32)&__mem_start__, (u32)&__end_of_text__);
    printf(" -- %s:  %08x - %08x\n", "data",(u32)&__data_beg__, (u32)&__data_end__);
    //printf(" -- %s:  %08x - %08x\n", "dmsg",(u32)&__dmsg_beg__, (u32)&__dmsg_end__);
    printf(" -- %s:  %08x - %08x\n", "bss ",(u32)&__bss_beg__, (u32)&_bss_end__);
    printf(" -- %s:  %08x - %08x\n", "heap",(u32)_g_heap_start, (u32)(_g_heap_start+_g_heap_size-1));
    //printf("  -- %s:  %08x - %08x\n", "msgevt",(u32)msgevt_addr_beg, (u32)msgevt_addr_end);
    printf("  -- %s:\n", "task  ");
    #if 1
    printf("      -- %s:maximum=%d\n", g_soc_task_info[0].task_name
        ,((g_soc_task_info[0].stack_size<<4)-uxTaskGetStackHighWaterMark(g_soc_task_info[0].task_handle)*4));

    printf("      -- %s:maximum=%d\n", g_soc_task_info[1].task_name
        ,((g_soc_task_info[1].stack_size<<4)-uxTaskGetStackHighWaterMark(g_soc_task_info[1].task_handle)*4));

    printf("      -- %s:maximum=%d\n", g_soc_task_info[2].task_name
        ,((g_soc_task_info[2].stack_size<<4)-uxTaskGetStackHighWaterMark(g_soc_task_info[2].task_handle)*4));

    #else
    printf("      -- %s:stack size=%d, max usage amount=%d\n", g_soc_task_info[0].task_name
        ,g_soc_task_info[0].stack_size<<4
        ,((g_soc_task_info[0].stack_size<<4)-uxTaskGetStackHighWaterMark(g_soc_task_info[0].task_handle)*4));

    printf("      -- %s:stack size=%d, max usage amount=%d\n", g_soc_task_info[1].task_name
        ,g_soc_task_info[1].stack_size<<4
        ,((g_soc_task_info[1].stack_size<<4)-uxTaskGetStackHighWaterMark(g_soc_task_info[1].task_handle)*4));

    printf("      -- %s:stack size=%d, max usage amount=%d\n", g_soc_task_info[2].task_name
        ,g_soc_task_info[2].stack_size<<4
        ,((g_soc_task_info[2].stack_size<<4)-uxTaskGetStackHighWaterMark(g_soc_task_info[2].task_handle)*4));
    #endif
    /*
     In boot.S
    .set  UND_STACK_SIZE,   0x00000040
    .set  ABT_STACK_SIZE,   0x00000040
    .set  FIQ_STACK_SIZE,   0x00000040
    .set  IRQ_STACK_SIZE,   0X00000400
    .set  SVC_STACK_SIZE,   0x00000400
    */
    #if 0
    printf("  -- UND_STACK_SIZE:  64 bytes\n");
    printf("  -- ABT_STACK_SIZE:  64 bytes\n");
    printf("  -- FIQ_STACK_SIZE:  64 bytes\n");
    printf("  -- IRQ_STACK_SIZE:  1024 bytes\n");
    printf("  -- SVC_STACK_SIZE:  1024 bytes\n");
    #endif
    printf("  -- %s:  %d bytes\n", "Avail ",g_heap_size);

}
#if 0
static void Cmd_channel_status(s32 argc, char *argv[])
{
    int i=0;
    printf("Channel  edcca count  packet count\r\n");
    printf("----------------------------------------\r\n");
    for(i=0;i<MAX_CHANNEL_NUM;i++){
        printf("  %2d      %7d       %5d    \r\n",i,gDevInfo->final_channel_edcca_count[i],gDevInfo->final_channel_packet_count[i]);
    }
}
#endif
#if 0
//extern volatile u8 g_numeric;
static void Cmd_swi(s32 argc, char *argv[])
{
    u32 loop=1, count=0;
    if (argc==2){
        loop=ssv6xxx_atoi(argv[1]);
    }

    for(count=0;count<loop;count++){
        portENTER_CRITICAL();
        portEXIT_CRITICAL();
        printf("swi 1 done. count=%d\n",count+1);
    }
}
#endif
#if 0
bool display_beacon=0;
static void Cmd_display_beacon_receiving(s32 argc, char *argv[])
{
    display_beacon^=1;
    return;
}
#endif
extern u8 g_txQueueMapping[MAX_TX_DATA_QUEUE];
static void Cmd_QueueMapping(s32 argc, char *argv[])
{
    portENTER_CRITICAL();
    if (argc!=3){
        g_txQueueMapping[0]=ssv6xxx_atoi_base(argv[1],16);
        g_txQueueMapping[1]=ssv6xxx_atoi_base(argv[2],16);
        g_txQueueMapping[2]=ssv6xxx_atoi_base(argv[3],16);
        printf("%d %d %d\n ",g_txQueueMapping[0],g_txQueueMapping[1],g_txQueueMapping[2]);
    }else{
        printf("wrong usage\n");
    }
    portEXIT_CRITICAL();
    return;
}
static void Cmd_status(s32 argc, char *argv[])
{
    #if(PACKET_TRACE==1)
    dump_pt();
    #endif
    printf("error count of ack_policy:%d\n",gAckPolicyErrorCount);
    printf("soft_mac_task:%d\n",gSoftTaskCount);
    printf("mlme_Task:%d\n",gMLMETaskCount);
    return;
}

static void Cmd_wakeup(s32 argc, char *argv[])
{
    if (argc!=2){
        goto USAGE;
    }

    if(0==strcmp(argv[1],"soft_mac"))
    {
        wake_up_task(MBOX_SOFT_MAC);
        return;
    }
    else if(0==strcmp(argv[1],"mlme"))
    {
        wake_up_task(MBOX_MLME);
        return;
    }
USAGE:
    printf("usage:\n");
    printf("     wakeup mlme\n");
    printf("     wakeup soft_mac\n");
    return;

}

#if (N_MODE_AMPDU_TX == 1)
static void Cmd_ampdu(s32 argc, char *argv[])
{
    if(argc == 2)
    {
        if (strcmp (argv[1], "st") == 0)
        {
            ampdu_show_ampdu_mib();
            return;
        }
        else if (strcmp (argv[1], "reset") == 0)
        {
            ampdu_reset_ampdu_mib();
            return;
        }
        else if(strcmp (argv[1], "list") == 0)
        {
            ampdu_dump_retry_list();
            return;
        }
    }
    else if(argc == 3)
    {
        if(strcmp (argv[1], "tx") == 0)
        {
            if(strcmp (argv[2], "on") == 0)
            {
                ampdu_tx_set_options(AMPDU_TX_OPT_ENABLE, 1);
                return;
            }
            else if(strcmp (argv[2], "off") == 0)
            {
                ampdu_tx_set_options(AMPDU_TX_OPT_ENABLE, 0);
                return;
            }
        }
        else if(strcmp (argv[1], "lastinb") == 0)
        {
            ampdu_tx_set_options(AMPDU_TX_OPT_SET_LAST_TRY_BMODE, (u8) ssv6xxx_atoi (argv[2]));
            return;
        }
        else if(strcmp (argv[1], "lastbrate") == 0)
        {
            ampdu_tx_set_options(AMPDU_TX_OPT_SET_LAST_BMODE_RATE, (u8) ssv6xxx_atoi (argv[2]));
            return;
        }
        else if(strcmp (argv[1], "lastbonce") == 0)
        {
            ampdu_tx_set_options(AMPDU_TX_OPT_SET_LAST_BMODE_RETRY, (u8) ssv6xxx_atoi (argv[2]));
            return;
        }
        else if(strcmp (argv[1], "checksession") == 0)
        {
            ampdu_tx_set_options(AMPDU_TX_OPT_SET_BLOCK_NON_NMODE, (u8) ssv6xxx_atoi (argv[2]));
            return;
        }
        else if(strcmp (argv[1], "maxretry") == 0)
        {
            ampdu_tx_set_options(AMPDU_TX_OPT_SET_RETRY_MAX, (u8) ssv6xxx_atoi (argv[2]));
            return;
        }

    }

    printf("Error!! ampdu st/reset/list, ampdu tx on/off, ampdu lastinb 0/1, "
        "ampdu lastbrate [0-3], ampdu lastbonce 0/1, ampdu checksession 0/1, ampdu maxretry 0/ \n");

}
#endif

//static void Cmd_ipc(s32 argc, char *argv[])
//{
//    SET_SPI_IPC_ADDR(1); // write c0000e34 to 1
//    return;
//}

#if(ENABLE_DYNAMIC_RX_SENSITIVE==1)
extern u16 rcpi_update_count;
extern u32 rcpi_counter;
static void Cmd_RCPI(s32 argc, char *argv[])
{
    u16 value;
    if(argc!=2)
    {
        goto USAGE;
    }
    value=(u16)ssv6xxx_atoi(argv[1]);

    taskENTER_CRITICAL();
    rcpi_update_count=value;
    rcpi_counter=0;
    taskEXIT_CRITICAL();
    return;
USAGE:
    printf("usage:\n");
    printf("      rcpi [update period]\n");
    printf("update period: unit is count of beacon\n");
    printf("update period=1: update every time\n");
    return;
}
#endif
const CLICmds gCliCmdTable[] =
{
#if 0
    { "exit",             Cmd_Exit, 		"Terminate Wifi system."		},
#endif
#ifdef ENABLE_CLI_CMD_MIB
    { "mib",             Cmd_Mib,           "SoC MIB Counter"				},
#endif
#if 0
    { "cal",              Cmd_Cal,		"Calibration Process"			},
#endif
    { "r",                 Cmd_Read,   	"Read memory or register."  	},
    { "w",                Cmd_Write, 		"Write memory or register." 	},
#if 0
    { "phy",             Cmd_Phy,           "PHY commands."    				},
#endif
#ifdef __TEMPERATURE_VERIFY__
    { "temperature",    Cmd_Temperature,   "Got temperature"                  },
#endif
#ifdef __PMU_VERIFY__
    { "rtc",            Cmd_Rtc,           "rtc control"                      },
    { "pmu",            Cmd_Pmu,           "Pmu control"                      },
#endif
#if 0
    { "dump",          Cmd_Dump,  	"Dump HW Mac Table." 			},
#endif
#if 0
    { "edca",           Cmd_EDCA, 		"EDCA stat. info."    			},
#endif
    { "fix-rate",       Cmd_FIXRATE, 	"[0-38]enable fix rate  [39]disable fix rate"},
	{ "preamble",		Cmd_PREAMBLE,	"[0]Short [1]Long"},
    { "networkMode",    Cmd_NETWORK,	"[0]L-GI [1]S-GI [2]Green [3]G-ONLY"},
    { "rcinfo",    Cmd_RCINFO,	"st/tbl [0-1]sta_wsid or msg/samplemsg"},
#if 0
    { "backoff",        Cmd_BACKOFF, 	"backoff on/off"},
#endif
#if 0
    { "buffer",         Cmd_BUFFER, 	"Setting TX RX buffer"},
#endif
#if 0
    { "onfire",         Cmd_ONFIRE,        "[on/off] Ignore PHY and disable backoff"},
#endif
    { "q",              Cmd_ReadQueue,    "Show sizes of queues"},
#if 0
    { "res",              Cmd_ReadResource,    "Show pbuf resource"},
#endif
#if 0
    { "mb",              Cmd_DumpMboxDbg,  "Dump MBOX log"},
#endif
#if 0
    { "mcusleep",       Cmd_McuSleep,  "Mcu clock disable"},
#endif
#ifdef ENABLE_CLI_CMD_DUMP_REG
    { "reg",            reg_dump_bank,      "Register dump"},
#endif
#ifdef ENABLE_TX_QUEUE_STATS
    { "st_tx_st",      Cmd_StartTxQueueSt,  "Start TX queue statistics"},
    { "sw_tx_st",      Cmd_ShowTxQueueSt,   "Show TX queue statistics"},
#endif // ENABLE_TX_QUEUE_STATS


    // add by Frank
#ifdef ENABLE_LOG_TO_HOST
    { "log",             Cmd_Log,           "SSV LOG command"      },
#endif

#ifdef __MCU_CLK_EN_VERIFY__
    { "clk",             Cmd_Clk,             "MCU clock enable"},
#endif
#ifdef FLASH_MCU
     { "fr",          	  Cmd_FlashRead, 		"Read flash"    		},
     { "serase",         Cmd_FlashSErase, 		"secotr erase"    		},
     { "fw",          	  Cmd_FlashWrite, 		"Write Flash"    		},
     { "fs",          	  Cmd_FlashReadStatus, 	"Write Flash"    	},
     { "fwen",            Cmd_FlashWriteEn, 		"Write Flash"    	},
#endif


#if (AMPDU_TEST == 1)
    {"ampdu",           Cmd_ampdu,       "AMPDU test"	},
    {"bar",                Cmd_BAR,           "send bar frame"	},
    {"null",               Cmd_NULL,         "send bar frame"	},
    {"ba",                 Cmd_BA,             "set ba address"	},
#endif
#if 0
    {"m",         Cmd_Monitor,        "monitor system status" },
#endif
#if 0
    {"dq",         Cmd_SWQueue,        "SW queue utility" },
#endif
    {"mem",         Cmd_mem,        "memory status" },
#if 0
    {"cs",          Cmd_channel_status, "channel status"},
#endif
#if 0
    {"swi",         Cmd_swi,            "swi instruction testing"},
#endif
#if 0
    {"br",      Cmd_display_beacon_receiving,         "display the status of receiving beacon"},
#endif
    {"qm",      Cmd_QueueMapping, "Map the sw tx queue and hw tx queue"},
    {"s",       Cmd_status, "display system information"},
#if (N_MODE_AMPDU_TX == 1)
    {"ampdu",       Cmd_ampdu, "ampdu mib, ampdu list/st/reset"},
#endif
    {"wakeup",  Cmd_wakeup, "wakeup the tasks"},
    #if(ENABLE_DYNAMIC_RX_SENSITIVE==1)
    {"rcpi",Cmd_RCPI,"modify the rcpi update period in STA mode"},
    #endif
//    {"ipc",      Cmd_ipc,                              "Trigger ipc interrupt to Host and FW"},
//    {"h",         Cmd_history,         "list history record"},
    /*lint -save -e611 */
    { (const char *)NULL, (CliCmdFunc)NULL,   (const char *)NULL },
    /*lint -restore */
};


