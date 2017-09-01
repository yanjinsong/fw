#include <msgevt.h>
#include <pbuf.h>

#include <cmd_def.h>
//#include <common.h>
//#include <host_apis.h>
#include <log.h>
#include "mlme.h"

#include "sta_info.h"
#include "cmd_engine.h"
#include <wpa_supplicant_i.h>
#include <hwmac/drv_mac.h>
#include <mbox/drv_mbox.h>
#include <edca/drv_edca.h>
#include <beacon/drv_beacon.h>
#include <driver/phy/drv_phy.h>

#include <rtos.h>
#include <lib/ssv_lib.h>
#include "MLME_Task.h"
#include "supplicant/crypto/sha1.h"
#include "supplicant/utils/wpa_debug.h"
#include <dbg_timer/dbg_timer.h>
#include "hdr80211.h"
#include <timer/drv_timer.h>
#include <ampdu/drv_ampdu.h>

//#include "supplicant/common/wpa_common.h"

#ifdef ENABLE_BACKGROUND_PMK_CALC
struct ap_calc_data_st
{
    int (*pmk_calc_task)(void *data);
    void *pmk_calc_data;
};


struct ap_calc_data_st ap_calc_pmk_data;
#endif

extern int pbkdf2_sha1(const char *passphrase, const char *ssid, size_t ssid_len,
		int iterations, u8 *buf, size_t buflen);

void wpa_pmk_to_ptk(const u8 *pmk, size_t pmk_len, const char *label,
		    const u8 *addr1, const u8 *addr2,
		    const u8 *nonce1, const u8 *nonce2,
		    u8 *ptk, size_t ptk_len, int use_sha256);


#if (CONFIG_SIM_PLATFORM == 1)
extern void dump_all();
#endif

#define CMD_PRINTF(fmt, ...) 		LOG_PRINTF_M(LOG_MODULE_CMD, fmt, ##__VA_ARGS__)
#define CMD_TRACE(fmt, ...)			LOG_TRACE_M(LOG_MODULE_CMD, fmt, ##__VA_ARGS__)
#define CMD_DEBUG(fmt, ...)			LOG_DEBUG_M(LOG_MODULE_CMD, fmt, ##__VA_ARGS__)
#define CMD_INFO(fmt, ...)			LOG_INFO_M(LOG_MODULE_CMD,  fmt, ##__VA_ARGS__)
#define CMD_WARN(fmt, ...)			LOG_WARN_M(LOG_MODULE_CMD,  fmt, ##__VA_ARGS__)
#define CMD_FAIL(fmt, ...)			LOG_FAIL_M(LOG_MODULE_CMD,  fmt, ##__VA_ARGS__)
#define CMD_ERROR(fmt, ...)			LOG_ERROR_M(LOG_MODULE_CMD, fmt, ##__VA_ARGS__)
#define CMD_FATAL(fmt, ...)			LOG_FATAL_M(LOG_MODULE_CMD, fmt, ##__VA_ARGS__)

#define CHECK_DATA_LEN(host_cmd, size) \
    do { \
        if ((host_cmd->len - HOST_CMD_HDR_LEN) != (size)) \
        { \
            CMD_ERROR("Host command %u with length %d does not match expected %u.\n", \
                      host_cmd->h_cmd, (s32)host_cmd->len - (s32)HOST_CMD_HDR_LEN, (u32)(size)); \
            return; \
        } \
    } while (0)

#ifdef	CMD_ENGINE_DEBUG
#define CMD_DUMP(p_host_cmd, fmt, ...) \
            CMD_INFO("%s(): len=%u, " fmt, __FUNCTION__, p_host_cmd->len, ##__VA_ARGS__)
#else
#define CMD_DUMP(p_host_cmd, fmt, ...) do{}while(0)
#endif

static OsMutex CmdEngineMutex;
static bool    _CmdEnginBatchMode = false;

struct cmd_bcn_info {
    bool bRegSet;            //Set packetbuf in hw reg.  need to free if not used



//-------------------------------------------
    struct cfg_bcn_info bcn_info;
    PKT_TxInfo *bcn;
};



struct cmd_bcn_info g_bcn_info;
u32 sg_soc_evt_seq_no;

#if (AUTO_BEACON == 1)
static OsTimer bcn_timer;
#else
#define BEACON_PERIOD GET_MTX_BCN_PERIOD
#define GET_FW_BCN_ST (((REG32(ADR_MTX_BCN_EN_MISC))                    & 0x00000300 ) >> 8)
#define SET_FW_BCN_ST(_VAL_)    ((REG32(ADR_MTX_BCN_EN_MISC))                    = (((_VAL_) << 8)  | ((REG32(ADR_MTX_BCN_EN_MISC))                    & 0xfffffcff)))
u8 self_bcn_st;
void* last_bcn_ptr = NULL;
#endif

#if (CONFIG_SIM_PLATFORM == 1 || BEACON_DBG == 1)

enum{

	BCN_GEN_BCN,
	BCN_TEST_RELEASE_BCN,
};


extern bool bReleaseTest;

extern  bool soc_tx_done;


//SIM beacon send function
//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------
#include <mbox/drv_mbox.h>
#define BCN_TIMER_INTERVAL 100


OsTimer BcnMTimer;

s8 g_dtim_cnt=0;
u8 g_dtim = 0;

u8 cmd_eng_bcn_get_dtim_cnt_SIM();


extern void  sim_global_bcn_lock();
extern void  sim_global_bcn_unlock();





void cmd_eng_bcn_timer_handler( void *args )
{
    /**
     *
     */
    //struct cfg_test_cmd *cmd =OS_TimerGetData((OsTimer)args);
    MsgEvent *MsgEv;


    MsgEv = msg_evt_alloc();
    ASSERT(MsgEv != NULL);



    /* Fill MsgEvent Content: */
    MsgEv->MsgType  = MEVT_BCN_CMD;
#if(BEACON_DBG==1)
    MsgEv->MsgData  = (bReleaseTest)?BCN_TEST_RELEASE_BCN:BCN_GEN_BCN;
#else
    MsgEv->MsgData  = BCN_GEN_BCN;
#endif


    MsgEv->MsgData1 = 0;
    MsgEv->MsgData2 = 0;
    MsgEv->MsgData3 = 0;

    msg_evt_post(MBOX_SOFT_MAC, MsgEv);


}


void cmd_eng_bcn_timer_init_SIM()
{
    s32 res;
    res = OS_TimerCreate(&(BcnMTimer), BCN_TIMER_INTERVAL, false, NULL, cmd_eng_bcn_timer_handler);
    ASSERT(res == OS_SUCCESS);
}



void cmd_eng_bcn_create_timer_SIM()
{

    OS_TimerSet(BcnMTimer, 1, false, NULL);
    OS_TimerStart(BcnMTimer);
}

void cmd_eng_bcn_cancel_timer_SIM(void)
{
    OS_TimerStop(BcnMTimer);
}

void cmd_eng_bcn_enable_SIM(bool enable, u8 dtim_cnt, u16 bcn_itv)
{
    if(enable)
    {
        cmd_eng_bcn_create_timer_SIM();
        g_dtim_cnt = dtim_cnt;
        g_dtim = dtim_cnt+1;
    }
    else
        cmd_eng_bcn_cancel_timer_SIM();
}


extern void cmd_eng_prebcn_int_handler();


//Generate beacon to MAC TX(for Simulator)
void cmd_eng_bcn_gen_bcn_to_mactx_SIM()
{


    u32 bcn_id, dtim_offset;
    PKT_TxInfo *pkt, *new_pkt;
//	u8 *ptr;

#if (CONFIG_SIM_PLATFORM == 1)
    sim_global_bcn_lock();
#endif

    bcn_id = GET_MTX_BCN_PKT_ID1;
    dtim_offset = GET_MTX_DTIM_OFST1;



    //copy beacon
    pkt = (PKT_TxInfo *)PBUF_MapIDtoPkt(bcn_id);
    new_pkt = (PKT_TxInfo *)PBUF_MAlloc(pkt->len, SOC_PBUF);


    //copy descriptor
    memcpy(new_pkt, pkt, drv_mac_get_pbuf_offset());
    //copy raw data
    memcpy((u8*)new_pkt+new_pkt->hdr_offset, (u8*)pkt+new_pkt->hdr_offset, pkt->len);

    ptr = (u8*)new_pkt+new_pkt->hdr_offset;

    //move to DTIM cnt and modify it;
    ptr+=dtim_offset;

    cmd_eng_bcn_get_dtim_cnt_SIM();
    *ptr = g_dtim_cnt;

    //Send to MACTX
    ENG_MBOX_SEND((u32)M_ENG_TX_EDCA0, (u32)new_pkt);


#if (CONFIG_SIM_PLATFORM == 1)
    sim_global_bcn_unlock();
#endif



}




u8 cmd_eng_bcn_get_dtim_cnt_SIM()
{
    u8 val = g_dtim_cnt;
    g_dtim_cnt--;

    if(g_dtim_cnt<0)
        g_dtim_cnt = g_dtim-1;


    return val;
}




//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------

#endif// (CONFIG_SIM_PLATFORM == 1)








/*---------------------------  Start of HostCmd  ---------------------------*/
HDR_HostEvent *host_cmd_resp_alloc (const struct cfg_host_cmd *host_cmd, CmdResult_E cmd_result, u32 resp_size, void **p_resp_data)
{
    HDR_HostEvent           *ev=NULL;
    struct  resp_evt_result *resp;
    u32						 event_size;

    event_size = resp_size;
    HOST_EVENT_ALLOC_RET(ev, (ssv6xxx_soc_event)0, ETHER_ADDR_LEN, NULL);
    resp = (struct resp_evt_result *)HOST_EVENT_DATA_PTR(ev);
    HOST_EVENT_SET_LEN(ev, event_size);
    if (p_resp_data != NULL)
    	*p_resp_data = (void *)&resp->u.dat[0];

    return ev;
}



/* Scan Command: */

static void _bypass_cmd_to_mlme_task(const struct cfg_host_cmd **_HostCmd)
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    MsgEvent *MsgEv=NULL;
    MsgEv=(MsgEvent *)msg_evt_alloc();
    ASSERT(MsgEv != NULL);

    MsgEv->MsgType = MEVT_PKT_BUF;
    MsgEv->MsgData = (u32)HostCmd;
    msg_evt_post(MBOX_MLME, MsgEv);
}

void CmdEngine_ScanRequest( const struct cfg_host_cmd **_HostCmd )
{
    printf("\33[35m ScanRequest\33[0m\r\n");
    _bypass_cmd_to_mlme_task(_HostCmd);
    PBUF_SET_REUSE(_HostCmd);
}
#ifdef AIRKISS_ENABLE
bool sconfig_user_mode=FALSE;
void CmdEngine_SconfigRequest( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_sconfig_request *request=(struct cfg_sconfig_request *)((*_HostCmd)->dat8);

    printf("\33[35m SonfigRequest:\33[0m");
    if(request->user_mode==TRUE)
    {
        printf("\33[35mUser mode\33[0m\n");
        sconfig_user_mode=TRUE;
    }
    else
    {
        printf("\33[35mNon-user mode\33[0m\n");
        sconfig_user_mode=FALSE;
        _bypass_cmd_to_mlme_task(_HostCmd);
        PBUF_SET_REUSE(_HostCmd);
    }
}
#endif

static void CmdEngine_JoinRequest( const struct cfg_host_cmd **_HostCmd )
{
    printf("\33[35m JoinRequest()\33[0m\r\n");
    _bypass_cmd_to_mlme_task(_HostCmd);
    PBUF_SET_REUSE(_HostCmd);
}



static void CmdEngine_LeaveRequest( const struct cfg_host_cmd **_HostCmd )
{
    printf("\33[35m LeaveRequests\33[0m\r\n");
    _bypass_cmd_to_mlme_task(_HostCmd);
    PBUF_SET_REUSE(_HostCmd);
}

#if 0
static void CmdEngine_ADDBA_RESP( const struct cfg_host_cmd *HostCmd )
{
	//MLME_ADDBA_RESP((struct cfg_addba_resp *)HostCmd->dat8, HostCmd->cmd_seq_no);
    _bypass_cmd_to_mlme_task(HostCmd);
}
static void CmdEngine_DELBA( const struct cfg_host_cmd *HostCmd )
{
	//MLME_DELBA((struct cfg_delba *)HostCmd->dat8, HostCmd->cmd_seq_no);
    _bypass_cmd_to_mlme_task(HostCmd);
}
#endif

#if 0
static void CmdEngine_SetReg( const struct cfg_host_cmd *HostCmd )
{
    u32 reg_addr = HostCmd->dat32[0];
    u32 reg_val  = HostCmd->dat32[1];
    CMD_DUMP(HostCmd, "addr=0x%08x, val=0x%08x\n", reg_addr, reg_val);
    REG32(reg_addr) = reg_val;
}


static void CmdEngine_GetReg( const struct cfg_host_cmd *HostCmd )
{
    HDR_HostEvent *ev=NULL;
    u32 reg_addr = HostCmd->dat32[0];
    u8 *msg;

    CMD_RESP_ALLOC(ev, HostCmd, CMD_OK, sizeof(u32), (void *)&msg);
    HOST_EVENT_ASSIGN_EVT(ev, SOC_EVT_GET_REG_RESP);
    *(u32 *)msg = REG32(reg_addr);
    HOST_EVENT_SEND(ev);
}


static void CmdEngine_SetStaMac( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "val=%02x:%02x:%02x:%02x:%02x:%02x\n",
             (u32)HostCmd->dat8[0], (u32)HostCmd->dat8[1], (u32)HostCmd->dat8[2],
             (u32)HostCmd->dat8[3], (u32)HostCmd->dat8[4], (u32)HostCmd->dat8[5]);
    drv_mac_set_sta_mac(HostCmd->dat8);
}


static void CmdEngine_GetStaMac( const struct cfg_host_cmd *HostCmd )
{
	ETHER_ADDR 	sta_mac;

	drv_mac_get_sta_mac(&sta_mac.addr[0]);


	CMD_DUMP(HostCmd, "val=%02x:%02x:%02x:%02x:%02x:%02x\n",
             sta_mac.addr[0], sta_mac.addr[1], sta_mac.addr[2],
             sta_mac.addr[3], sta_mac.addr[4], sta_mac.addr[5]);
} // end of - CmdEngine_GetStaMac -


static void CmdEngine_SetBSSID( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "val=%02x:%02x:%02x:%02x:%02x:%02x\n",
             HostCmd->dat8[0], HostCmd->dat8[1], HostCmd->dat8[2],
             HostCmd->dat8[3], HostCmd->dat8[4], HostCmd->dat8[5]);
    drv_mac_set_bssid(HostCmd->dat8);
}
#endif

#if 0

static void CmdEngine_GetBSSID( const struct cfg_host_cmd *HostCmd )
{
	ETHER_ADDR		bssid;

	drv_mac_get_bssid(&bssid.addr[0]);


	CMD_DUMP(HostCmd, "val=%02x:%02x:%02x:%02x:%02x:%02x\n",
             bssid.addr[0], bssid.addr[1], bssid.addr[2],
             bssid.addr[3], bssid.addr[4], bssid.addr[5]);
} // end of - CmdEngine_GetBSSID -


static void CmdEngine_SetDeciTable( const struct cfg_host_cmd *HostCmd )
{
#ifdef	CMD_ENGINE_DEBUG
    s32 i;

    CMD_DUMP(HostCmd, "\n");
    for(i=0; i<24; i+=8) {
        CMD_INFO("\t0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x\n",
                 HostCmd->dat16[i], HostCmd->dat16[i+1],
                 HostCmd->dat16[i+2], HostCmd->dat16[i+3],
                 HostCmd->dat16[i+4], HostCmd->dat16[i+5],
                 HostCmd->dat16[i+6], HostCmd->dat16[i+7]);
    }
    CMD_INFO("\t0x%04x\n", HostCmd->dat16[24]);
#endif
    CHECK_DATA_LEN(HostCmd, (sizeof(u16)*25+2)); /* padding 2 bytes */

    drv_mac_set_decision_table(HostCmd->dat16);
}


static void CmdEngine_GetDeciTable( const struct cfg_host_cmd *HostCmd )
{
    HDR_HostEvent *ev=NULL;
    u8       *msg;

    CMD_RESP_ALLOC(ev, HostCmd, CMD_OK, (2*25+2), (void *)&msg);
    HOST_EVENT_ASSIGN_EVT(ev, SOC_EVT_GET_DECI_TABLE_RESP);
    drv_mac_get_decision_table((u16 *)msg);
    HOST_EVENT_SEND(ev);

    {
    s32 i;
    CMD_DUMP(HostCmd, "\n");
    for(i=0; i<25; i+=8) {
        const u16 *msg16 = (const u16 *)&msg[i];
        CMD_INFO("\t%04x %04x %04x %04x\n",
                 (u32)msg16[0], (u32)msg16[1], (u32)msg16[2], (u32)msg16[3]);
    }
    }
} // end of - CmdEngine_GetStaMac -


static void CmdEngine_SetWSIDTable( const struct cfg_host_cmd *HostCmd )
{
    const struct mac_wsid_entry_st *wsid_entry;
    wsid_entry = (const struct mac_wsid_entry_st *)(void *)&HostCmd->dat32[1];
    CMD_DUMP(HostCmd, "\tmac=%02x:%02x:%02x:%02x:%02x:%02x\n",
             (u32)wsid_entry->sta_mac.addr[0], (u32)wsid_entry->sta_mac.addr[1],
             (u32)wsid_entry->sta_mac.addr[2], (u32)wsid_entry->sta_mac.addr[3],
             (u32)wsid_entry->sta_mac.addr[4], (u32)wsid_entry->sta_mac.addr[5]);
#ifdef	CMD_ENGINE_DEBUG
    CMD_INFO("\tinfo: 0x%04x\n", wsid_entry->info);
    CMD_INFO("\ttx_ack_policy: 0x%04x\n", wsid_entry->tx_ack_policy);
    CMD_INFO("\ttx_seq_ctrl: %d %d %d %d %d %d %d %d\n",
             wsid_entry->tx_seq_ctrl[0], wsid_entry->tx_seq_ctrl[1],
             wsid_entry->tx_seq_ctrl[2], wsid_entry->tx_seq_ctrl[3],
             wsid_entry->tx_seq_ctrl[4], wsid_entry->tx_seq_ctrl[5],
             wsid_entry->tx_seq_ctrl[6], wsid_entry->tx_seq_ctrl[7]);
    CMD_INFO("\trx_seq_ctrl: %d %d %d %d %d %d %d %d\n",
             wsid_entry->rx_seq_ctrl[0], wsid_entry->rx_seq_ctrl[1],
             wsid_entry->rx_seq_ctrl[2], wsid_entry->rx_seq_ctrl[3],
             wsid_entry->rx_seq_ctrl[4], wsid_entry->rx_seq_ctrl[5],
             wsid_entry->rx_seq_ctrl[6], wsid_entry->rx_seq_ctrl[7]);
#endif
    CHECK_DATA_LEN(HostCmd, (sizeof(mwsid_entry)+sizeof(s32)));

    drv_mac_set_wsid_table((s32)HostCmd->dat32[0], wsid_entry);
}

#endif
#if 0

static void CmdEngine_GetWSIDTable( const struct cfg_host_cmd *HostCmd )
{
    struct mac_wsid_entry_st    *wsid_entry;
    HDR_HostEvent               *ev=NULL;
    u8                          *msg;

    CMD_RESP_ALLOC(ev, HostCmd, CMD_OK, (sizeof(struct mac_wsid_entry_st)*4), (void *)&msg);
    HOST_EVENT_ASSIGN_EVT(ev, SOC_EVT_GET_WSID_TABLE_RESP);

    wsid_entry = (struct mac_wsid_entry_st *)msg;
    drv_mac_get_wsid_table(0, wsid_entry+0);
    drv_mac_get_wsid_table(1, wsid_entry+1);
    drv_mac_get_wsid_table(2, wsid_entry+2);
    drv_mac_get_wsid_table(3, wsid_entry+3);
    HOST_EVENT_SEND(ev);

    CMD_DUMP(HostCmd, "\n");

    CMD_INFO("\tmac=%02x:%02x:%02x:%02x:%02x:%02x\n",
        wsid_entry->sta_mac.addr[0], wsid_entry->sta_mac.addr[1],
        wsid_entry->sta_mac.addr[2], wsid_entry->sta_mac.addr[3],
        wsid_entry->sta_mac.addr[4], wsid_entry->sta_mac.addr[5]);
    CMD_INFO("\tinfo: 0x%04x\n\t", wsid_entry->info);
    CMD_INFO("\ttx_ack_policy: 0x%04x\n\t", wsid_entry->tx_ack_policy);
    CMD_INFO("\ttx_seq_ctrl: %d %d %d %d %d %d %d %d\n",
             wsid_entry->tx_seq_ctrl[0], wsid_entry->tx_seq_ctrl[1],
             wsid_entry->tx_seq_ctrl[2], wsid_entry->tx_seq_ctrl[3],
             wsid_entry->tx_seq_ctrl[4], wsid_entry->tx_seq_ctrl[5],
             wsid_entry->tx_seq_ctrl[6], wsid_entry->tx_seq_ctrl[7]);
    CMD_INFO("\trx_seq_ctrl: %d %d %d %d %d %d %d %d\n",
             wsid_entry->rx_seq_ctrl[0], wsid_entry->rx_seq_ctrl[1],
             wsid_entry->rx_seq_ctrl[2], wsid_entry->rx_seq_ctrl[3],
             wsid_entry->rx_seq_ctrl[4], wsid_entry->rx_seq_ctrl[5],
             wsid_entry->rx_seq_ctrl[6], wsid_entry->rx_seq_ctrl[7]);
} // end of - CmdEngine_GetStaMac -


static void CmdEngine_SetTxEthTrap( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "index=%d, ethtype=0x%04x\n",
             (s32)HostCmd->dat32[0],
             HostCmd->dat32[1]);

    CHECK_DATA_LEN(HostCmd, 8);

    drv_mac_set_tx_ether_trap((s32)HostCmd->dat32[0],      /* index */
                              HostCmd->dat32[1]            /* ether type */
    );
}



static void CmdEngine_GetTxEthTrap( const struct cfg_host_cmd *HostCmd )
{
    s32                          tx_trap_id = (s32)HostCmd->dat32[0];
    u16                         *p_tx_trap;
    HDR_HostEvent               *ev=NULL;
    u8                          *msg;

    HOST_EVENT_ALLOC(ev, SOC_EVT_GET_TX_ETH_TRAP_RESP, sizeof(u16));
    msg = HOST_EVENT_DATA_PTR(ev);
    p_tx_trap = (u16 *)msg;
    drv_mac_get_tx_ether_trap(tx_trap_id, p_tx_trap);
    HOST_EVENT_SET_LEN(ev, sizeof(u16));
    HOST_EVENT_SEND(ev);

    CMD_DUMP(HostCmd, "index=%d, ethtype=0x%04x\n",
             tx_trap_id, (u32)*p_tx_trap);
} // end of - CmdEngine_GetTxEthTrap -
static void CmdEngine_SetRxEthTrap( const struct cfg_host_cmd *HostCmd )
{
    s32     index = (s32)HostCmd->dat32[0];
    u32     ethtype = HostCmd->dat32[1];
    CMD_DUMP(HostCmd, "index=%d, ethtype=0x%04x\n",
             index, ethtype);

    CHECK_DATA_LEN(HostCmd, 8);

    drv_mac_set_rx_ether_trap(index, ethtype);
}



static void CmdEngine_GetRxEthTrap( const struct cfg_host_cmd *HostCmd )
{
    s32                          rx_trap_id = (s32)HostCmd->dat32[0];
    u16                         *p_rx_trap;
    HDR_HostEvent               *ev=NULL;
    u8                          *msg;

    HOST_EVENT_ALLOC(ev, SOC_EVT_GET_RX_ETH_TRAP_RESP, sizeof(u16));
    msg = HOST_EVENT_DATA_PTR(ev);
    p_rx_trap = (u16 *)msg;
    drv_mac_get_rx_ether_trap(rx_trap_id, p_rx_trap);
    HOST_EVENT_SET_LEN(ev, sizeof(u16));
    HOST_EVENT_SEND(ev);

    CMD_DUMP(HostCmd, "index=%d, ethtype=0x%04x\n",
             rx_trap_id, (u32)*p_rx_trap);
} // end of - CmdEngine_GetTxEthTrap -
#endif

#if 0
static void CmdEngine_GetEtherTrap( const struct cfg_host_cmd *HostCmd )
{
    u16                         *p_eth_trap;
    HDR_HostEvent               *ev=NULL;
    u8                          *msg;

    CMD_RESP_ALLOC(ev, HostCmd, CMD_OK, (sizeof(u16)*8), (void *)&msg);
    HOST_EVENT_ASSIGN_EVT(ev, SOC_EVT_GET_ETHER_TRAP_RESP);

    p_eth_trap = (u16 *)msg;
    drv_mac_get_tx_ether_trap(0, p_eth_trap+0);
    drv_mac_get_tx_ether_trap(1, p_eth_trap+1);
    drv_mac_get_tx_ether_trap(2, p_eth_trap+2);
    drv_mac_get_tx_ether_trap(3, p_eth_trap+3);
    drv_mac_get_rx_ether_trap(0, p_eth_trap+4);
    drv_mac_get_rx_ether_trap(1, p_eth_trap+5);
    drv_mac_get_rx_ether_trap(2, p_eth_trap+6);
    drv_mac_get_rx_ether_trap(3, p_eth_trap+7);
    HOST_EVENT_SEND(ev);

    CMD_DUMP(HostCmd, "0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x\n",
             p_eth_trap[0], p_eth_trap[1], p_eth_trap[2], p_eth_trap[3],
             p_eth_trap[4], p_eth_trap[5], p_eth_trap[6], p_eth_trap[7]);
} // end of - CmdEngine_GetEthTrap -
#endif

static void CmdEngine_SetFCmdTxData( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    CMD_DUMP(HostCmd, "val0=0x%08x, val1=0x%08x\n",
             HostCmd->dat32[0], HostCmd->dat32[1]);

    CHECK_DATA_LEN(HostCmd, 8);

    drv_mac_set_tx_flow_data(HostCmd->dat32);
}


#if 0
static void CmdEngine_GetFCmdTxData( const struct cfg_host_cmd *HostCmd )
{
    HDR_HostEvent               *ev=NULL;
    u8                          *msg;

    HOST_EVENT_ALLOC(ev, SOC_EVT_GET_FCMD_TX_DATA_RESP, sizeof(u32)*2);
    msg = HOST_EVENT_DATA_PTR(ev);

    drv_mac_get_tx_flow_data((u32 *)msg);

    HOST_EVENT_SET_LEN(ev, sizeof(u32)*2);
    HOST_EVENT_SEND(ev);

    CMD_DUMP(HostCmd, "val0=0x%08x, val1=0x%08x\n",
             *(u32 *)&msg[0], *(u32 *)&msg[4]);
} // end of - CmdEngine_GetFCmdTxData -
#endif



static void CmdEngine_SetFCmdTxMgmt( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    CMD_DUMP(HostCmd, "val0=0x%08x, val1=0x%08x\n",
             HostCmd->dat32[0], HostCmd->dat32[1]);

    CHECK_DATA_LEN(HostCmd, 8);

    drv_mac_set_tx_flow_mgmt(HostCmd->dat32);
}


#if 0
static void CmdEngine_GetFCmdTxMgmt( const struct cfg_host_cmd *HostCmd )
{
    //s32                          rx_trap_id = (s32)HostCmd->dat32[0];
    HDR_HostEvent               *ev=NULL;
    u8                          *msg;
    u32                         *msg32;

    HOST_EVENT_ALLOC(ev, SOC_EVT_GET_FCMD_TX_MGMT_RESP, sizeof(u32)*2);
    msg = HOST_EVENT_DATA_PTR(ev);
    msg32 = (u32 *)msg;

    drv_mac_get_tx_flow_mgmt(msg32);

    HOST_EVENT_SET_LEN(ev, sizeof(u32)*2);
    HOST_EVENT_SEND(ev);

    CMD_DUMP(HostCmd, "val0=0x%08x, val1=0x%08x\n", msg32[0], msg32[1]);
} // end of - CmdEngine_GetFCmdTxMgmt -
#endif


static void CmdEngine_SetFCmdTxCtrl( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    CMD_DUMP(HostCmd, "val=0x%08x\n", HostCmd->dat32[0]);

    CHECK_DATA_LEN(HostCmd, 4);

    drv_mac_set_tx_flow_ctrl(HostCmd->dat32);
}


#if 0
static void CmdEngine_GetFCmdTxCtrl( const struct cfg_host_cmd *HostCmd )
{
    // s32                          rx_trap_id = (s32)HostCmd->dat32[0];
    HDR_HostEvent               *ev=NULL;
    u8                          *msg;

    HOST_EVENT_ALLOC(ev, SOC_EVT_GET_FCMD_TX_CTRL_RESP, sizeof(u32)*2);
    msg = HOST_EVENT_DATA_PTR(ev);

    drv_mac_get_tx_flow_ctrl((u32 *)msg);

    HOST_EVENT_SET_LEN(ev, sizeof(u32)*2);
    HOST_EVENT_SEND(ev);

    CMD_DUMP(HostCmd, "val0=0x%08x, val1=0x%08x\n",
             *(u32 *)&msg[0], *(u32 *)&msg[4]);
} // end of - CmdEngine_GetFCmdTxCtrl -
#endif


static void CmdEngine_SetFCmdRxData( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    CMD_DUMP(HostCmd, "val0=0x%08x, val1=0x%08x\n",
             HostCmd->dat32[0], HostCmd->dat32[1]);

    CHECK_DATA_LEN(HostCmd, 8);

    drv_mac_set_rx_flow_data(HostCmd->dat32);
}


#if 0
static void CmdEngine_GetFCmdRxData( const struct cfg_host_cmd *HostCmd )
{
    //s32                          rx_trap_id = (s32)HostCmd->dat32;
    HDR_HostEvent               *ev=NULL;
    u8                          *msg;

    HOST_EVENT_ALLOC(ev, SOC_EVT_GET_FCMD_RX_DATA_RESP, sizeof(u32)*2);
    msg = HOST_EVENT_DATA_PTR(ev);

    drv_mac_get_rx_flow_data((u32 *)msg);

    HOST_EVENT_SET_LEN(ev, sizeof(u32)*2);
    HOST_EVENT_SEND(ev);

    CMD_DUMP(HostCmd, "val0=0x%08x, val1=0x%08x\n",
             *(u32 *)&msg[0], *(u32 *)&msg[4]);
} // end of - CmdEngine_GetFCmdRxData -
#endif


static void CmdEngine_SetFCmdRxMgmt( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    CMD_DUMP(HostCmd, "val0=0x%08x, val1=0x%08x\n",
             HostCmd->dat32[0], HostCmd->dat32[1]);

    CHECK_DATA_LEN(HostCmd, 8);

    drv_mac_set_rx_flow_mgmt(HostCmd->dat32);
}


#if 0
static void CmdEngine_GetFCmdRxMgmt( const struct cfg_host_cmd *HostCmd )
{
    //s32                          rx_trap_id = (s32)HostCmd->dat32[0];
    HDR_HostEvent               *ev=NULL;
    u8                          *msg;
    u32                         *msg32;

    HOST_EVENT_ALLOC(ev, SOC_EVT_GET_FCMD_RX_MGMT_RESP, sizeof(u32)*2);
    msg = HOST_EVENT_DATA_PTR(ev);
	msg32 = (u32 *)msg;
    drv_mac_get_rx_flow_mgmt(msg32);

    HOST_EVENT_SET_LEN(ev, sizeof(u32)*2);
    HOST_EVENT_SEND(ev);

    CMD_DUMP(HostCmd, "val0=0x%08x, val1=0x%08x\n", msg32[0], msg32[1]);
} // end of - CmdEngine_GetFCmdRxMgmt -
#endif


static void CmdEngine_SetFCmdRxCtrl( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    CMD_DUMP(HostCmd, "val0=0x%08x, val1=0x%08x\n",
            HostCmd->dat32[0], HostCmd->dat32[1]);


    CHECK_DATA_LEN(HostCmd, 8);


    drv_mac_set_rx_flow_ctrl(HostCmd->dat32);
}


#if 0
static void CmdEngine_GetFCmdRxCtrl( const struct cfg_host_cmd *HostCmd )
{
    //s32                          rx_trap_id = (s32)HostCmd->dat32[0];
    HDR_HostEvent               *ev;
    u32                         *data;

    HOST_EVENT_ALLOC(ev, SOC_EVT_GET_FCMD_RX_CTRL_RESP, sizeof(u32)*2);
    data = (u32 *)HOST_EVENT_DATA_PTR(ev);
    drv_mac_get_rx_flow_ctrl(data);

    HOST_EVENT_SET_LEN(ev, sizeof(u32)*2);
    HOST_EVENT_SEND(ev);

    CMD_DUMP(HostCmd, "val0=0x%08x, val1=0x%08x\n",
             data[0], data[1]);
} // end of - CmdEngine_GetFCmdRxCtrl -
#endif

#if 0
static void CmdEngine_GetFCmds( const struct cfg_host_cmd *HostCmd )
{
    HDR_HostEvent               *ev=NULL;
    u8                          *msg;
    u32                         *data;

    /**
        * Format: { TX_DATA, TX_SW_DATA, TX_MGMT, TX_SW_MGMT, TX_CTRL
        *                  RX_DATA, RX_SWDATA, RX_MGMT, RX_SW_MGMT, RX_CTRL
        *               }
        */
    CMD_RESP_ALLOC(ev, HostCmd, CMD_OK, (sizeof(u32)*10), (void *)&msg);
    HOST_EVENT_ASSIGN_EVT(ev, SOC_EVT_GET_FCMDS_RESP);

    data = (u32 *)msg;

    drv_mac_get_tx_flow_data(data+0);
    drv_mac_get_tx_flow_mgmt(data+2);
    drv_mac_get_tx_flow_ctrl(data+4);
    drv_mac_get_rx_flow_data(data+5);
    drv_mac_get_rx_flow_mgmt(data+7);
    drv_mac_get_rx_flow_ctrl(data+9);

    HOST_EVENT_SEND(ev);

    CMD_DUMP(HostCmd, "0x%08x 0x%08x 0x%08x 0x%08x 0x%08x "
             "0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
             data[0], data[1], data[2], data[3], data[4],
             data[5], data[6], data[7], data[8], data[9]);
} // end of - CmdEngine_GetFCmds -



static void CmdEngine_SetMib( const struct cfg_host_cmd *HostCmd )
{
    const struct cfg_set_mib *msg = (const struct cfg_set_mib *)&(HostCmd->dat8[0]);

    if(msg->mib_type == CFG_MIB_RTS)
        drv_mac_set_rts_threshold(msg->mib_value);
}


static void CmdEngine_Set_Rx_Info_Size( const struct cfg_host_cmd *HostCmd )
{
    const u8 value = HostCmd->dat8[0];
    drv_mac_set_rx_info_size(value);
}




static void CmdEngine_InitPhyTable( const struct cfg_host_cmd *HostCmd )
{
    if (drv_phy_info_tbl_init())
        CMD_INFO("%s(): success !\n\r", __FUNCTION__);
    else
        CMD_INFO("%s(): fail !!!\n\r", __FUNCTION__);
}
#endif

#if 0

static void CmdEngine_SetPhyInfoTable( const struct cfg_host_cmd *HostCmd )
{
    const u32 *dat = HostCmd->dat32;
    u32 *tblidx, i;

    //LOG_PRINTF("len=%d, dat=0x%08x (0x%08x, 0x%08x)\n", HostCmd->len, *dat, (u32)HostCmd, (u32)HostCmd->dat);
    ASSERT(HostCmd->len == (39*2*4+8*4+HOST_CMD_HDR_LEN));

    //ssv6xxx_raw_dump((s8 *)HostCmd, HostCmd->len);

    for(i=0; i<39; i++)
        REG_PHYINFO_ENTRY[i].INFO = *dat++;
    tblidx = (u32 *)g_soc_table->phy_idx_tbl;
    for(i=0; i<39; i++)
        tblidx[i] = *dat++;
    tblidx = (u32 *)g_soc_table->phy_llen_tbl;
    for(i=0; i<8; i++)
        tblidx[i] = *dat++;
}



static void CmdEngine_GetPhyInfoTable( const struct cfg_host_cmd *HostCmd )
{
    HDR_HostEvent               *ev;
    u8                          *msg;
    s32                         len, i;
    u32                         *b32pos, *tbl;

    /**
        * (1) phy-info table: 4*39
        * (2) phy-index table: 4*39
        * (3) phy-llen table: 4*8
        * (4) phy-index table address: 4
        * (5) phy-llen table address: 4
        * (6) phy rate mask: 4
        * (7) phy default rate: 4
        * (8) phy info offset: 4
        */
    ASSERT(HostCmd->len == 4);

    len = 4*39*2+4*8+4+4+4+4+4;

    CMD_RESP_ALLOC(ev, HostCmd, CMD_OK, len, (void *)&msg);
    HOST_EVENT_ASSIGN_EVT(ev, SOC_EVT_GET_PHY_INFO_TBL_RESP);

    b32pos = (u32 *)msg; /* make sure 4-byte aligned */
    for(i=0, tbl=(u32 *)g_soc_table->phy_idx_tbl; i<39; i++) {
        b32pos[i+0 ] = REG_PHYINFO_ENTRY[i].INFO;
        b32pos[i+39] = tbl[i];
    }
    for(i=0, tbl=(u32 *)g_soc_table->phy_llen_tbl; i<8; i++)
        b32pos[i+39*2] = tbl[i];
    b32pos[39*2+8+0] = GET_INFO_IDX_TBL_ADDR;
    b32pos[39*2+8+1] = GET_INFO_LEN_TBL_ADDR;
    b32pos[39*2+8+2] = GET_INFO_MASK;
    b32pos[39*2+8+3] = GET_INFO_DEF_RATE;
    b32pos[39*2+8+4] = GET_INFO_MRX_OFFSET;
    HOST_EVENT_SEND(ev);
}


static void CmdEngine_SetSifs( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "val=%u\n", HostCmd->dat32[0]);

    CHECK_DATA_LEN(HostCmd, 4);

}



static void CmdEngine_GetSifs( const struct cfg_host_cmd *HostCmd )
{
    HDR_HostEvent               *ev=NULL;
    u8                          *msg;

    HOST_EVENT_ALLOC(ev, SOC_EVT_GET_SIFS_RESP, sizeof(u32));
    msg = HOST_EVENT_DATA_PTR(ev);

    *(u32 *)msg = drv_mac_get_sifs();

    HOST_EVENT_SET_LEN(ev, sizeof(u32));
    HOST_EVENT_SEND(ev);

    CMD_DUMP(HostCmd, "val=%u\n", (*(u32 *)msg));
} // end of - CmdEngine_GetSifs -

static void CmdEngine_SetDifs( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "val=%u\n",
             HostCmd->dat32[0]);

    CHECK_DATA_LEN(HostCmd, 4);

}



static void CmdEngine_GetDifs( const struct cfg_host_cmd *HostCmd )
{
    HDR_HostEvent               *ev=NULL;
    u8                          *msg;

    HOST_EVENT_ALLOC(ev, SOC_EVT_GET_DIFS_RESP, sizeof(u32));
    msg = HOST_EVENT_DATA_PTR(ev);

    drv_mac_get_difs((u32 *)msg);

    HOST_EVENT_SET_LEN(ev, sizeof(u32));
    HOST_EVENT_SEND(ev);

    CMD_DUMP(HostCmd, "val=%d\n", (*(u32 *)msg));
} // end of - CmdEngine_GetDifs -


static void CmdEngine_SetEifs( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "val=%u\n", (HostCmd->dat32[0]));

    CHECK_DATA_LEN(HostCmd, 4);

}


static void CmdEngine_GetEifs( const struct cfg_host_cmd *HostCmd )
{
    HDR_HostEvent               *ev=NULL;
    u8                          *msg;

    HOST_EVENT_ALLOC(ev, SOC_EVT_GET_SIFS_RESP, sizeof(u32));
    msg = HOST_EVENT_DATA_PTR(ev);

    drv_mac_get_eifs((u32 *)msg);

    HOST_EVENT_SET_LEN(ev, sizeof(u32));
    HOST_EVENT_SEND(ev);

    CMD_DUMP(HostCmd, "val=%d\n", (*(u32 *)msg));
} // end of - CmdEngine_GetEifs -
#endif

static void CmdEngine_BCN_Timer_En( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    SET_MTX_BCN_TIMER_EN(HostCmd->dat32[0]);
    SET_MTX_TIME_STAMP_AUTO_FILL(0);
}



#ifdef ENABLE_LOG_TO_HOST
static void CmdEngine_LOG( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    // CMD_INFO("%s <= \n\r", __FUNCTION__);
    // LOG_hcmd_dump((log_hcmd_st *)(&(HostCmd->dat8)));
    LOG_soc_exec_hcmd((log_hcmd_st *)(HostCmd->dat8));
    // CMD_INFO("%s => \n\r", __FUNCTION__);
}
#endif



#if 0
#include <regs/ssv6200_configuration.h>

static void CmdEngine_PHY_ON( const struct cfg_host_cmd *HostCmd )
{
#if (CONFIG_SIM_PLATFORM == 0)
    int i;
    volatile u32 *write_addr;

    //PHY off
    drv_phy_off();

    for (i=0; i<(sizeof(phy_setting)/sizeof(struct ssv6xxx_reg_structure)); i++)
    {
        write_addr = (volatile u32 *)phy_setting[i].address;
        *write_addr = (u32)phy_setting[i].data;
    }
    //PHY on
    drv_phy_bgn();
    drv_phy_on();

#endif
}
#endif

static void CmdEngine_Cal( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);

    drv_phy_channel_change(HostCmd->dat8[0],TRUE);
    gDevInfo->current_channel=HostCmd->dat8[0];
}
#if 0

static void CmdEngine_GetCurrentChan ( const struct cfg_host_cmd *HostCmd )
{
    u8  cur_chan = gDevInfo->current_channel;
}


//Security
static void CmdEngine_SetPairSecurity( const struct cfg_host_cmd *HostCmd )
{
    drv_mac_set_pair_security(HostCmd->dat8[0]);
}


static void CmdEngine_SetGroupSecurity( const struct cfg_host_cmd *HostCmd )
{
    drv_mac_set_group_security(HostCmd->dat8[0]);
}

static void CmdEngine_SetPairEntry( const struct cfg_host_cmd *HostCmd )
{
}

static void CmdEngine_SetGroupEntry( const struct cfg_host_cmd *HostCmd )
{
}
#endif
extern u8 sram_set_wep(u8 wsid,int key_idx,const u8 *key,size_t key_len);
static void CmdEngine_SetSecurityEntry( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    const struct securityEntry *entry = (struct securityEntry *)&HostCmd->dat32;
    u8 zero_address[ETH_ALEN] = {0,0,0,0,0,0};

    CMD_INFO("Setting key for ...\n");

#if 0
    switch(entry->cipher)
    {
        case CIPHER_HOST_WEP40:
        case CIPHER_HOST_WEP104:
            CMD_INFO("WEP\n");
            u8 cipher;
            //For WEP mode -- we must setting group and pair register to same mode.
            if(CIPHER_HOST_WEP40 == entry->cipher)
            {
                cipher = SECURITY_WEP40;
            }
            else
            {
                cipher = SECURITY_WEP104;
            }
            drv_mac_set_group_security(cipher);
            drv_mac_set_pair_security(cipher);
            sram_set_wep(0,entry->keyIndex,entry->key,entry->keyLen);
            sram_set_wep(1,entry->keyIndex,entry->key,entry->keyLen);
            break;
        default:
            break;
    }
#else
	switch(entry->cipher)
	{
	   case CIPHER_HOST_WEP40:
	   case CIPHER_HOST_WEP104:
		   CMD_INFO("WEP\n");
		   wpa_supplicant_set_key(NULL, WPA_ALG_WEP,
                                    (const u8*)entry->bssid,
                                    entry->keyIndex, 2,
                                    (const u8 *)NULL, 0,
                                    entry->key,
                                    entry->keyLen);
		   break;
	   case CIPHER_HOST_TKIP:
		   CMD_INFO("TKIP - %s\n", entry->wpaUnicast ? "uni" : "mcast");
		   wpa_supplicant_set_key(NULL, WPA_ALG_TKIP,
                                    (const u8*)entry->bssid,
                                    entry->keyIndex, 1,
                                    (const u8 *)NULL, 0,
                                    entry->key,
                                    entry->keyLen);
		   break;
	   case CIPHER_HOST_CCMP:
		   CMD_INFO("CCMP - %s\n", entry->wpaUnicast ? "uni" : "mcast");
		   wpa_supplicant_set_key(NULL, WPA_ALG_CCMP,
                                    (const u8*)entry->bssid,
                                    entry->keyIndex, 1,
                                    (const u8 *)NULL, 0,
                                    entry->key,
                                    entry->keyLen);
		   break;
	   case CIPHER_HOST_NONE:
		if(memcmp(entry->bssid,zero_address,ETHER_ADDR_LEN)==0)
			wpa_supplicant_set_key(NULL, WPA_ALG_NONE, (const u8 *)NULL, 0, 0, (const u8 *)NULL, 0, (const u8 *)NULL, 0);
		else
			wpa_supplicant_set_key(NULL, WPA_ALG_NONE, (const u8*)entry->bssid, 0, 0, (const u8 *)NULL, 0, (const u8 *)NULL, 0);
			break;
	   default:
		   break;
   }

#endif
}
#ifdef ENABLE_BACKGROUND_PMK_CALC

void pmk_done(u8 *pmk)
{

    // Send to cmd_engine
    HDR_HostEvent   *ev=NULL;
    u32 size;
    size=PMK_EVT_HEADER_SIZE;

    //HOST_EVENT_ALLOC(ev,SOC_EVT_GET_PMK , size);
    HOST_EVENT_ALLOC_0(ev,SOC_EVT_GET_PMK , size);

    struct resp_evt_result *pmkdata = (struct resp_evt_result *)HOST_EVENT_DATA_PTR(ev);
    memset((void *)pmkdata,0,PMK_EVT_HEADER_SIZE);
    memcpy(pmkdata->u.pmk,pmk,PMK_EVT_HEADER_SIZE);
    hex_dump(pmkdata->u.pmk,32);
    HOST_EVENT_SET_LEN(ev, size);
    HOST_EVENT_SEND(ev);


}

static void *_init_ap_pmk_in_background (const u8 *ssid,size_t ssid_len,const u8 *passphrase)
{
    printf("%s\n\r",__FUNCTION__);
    PMK_CALC_DATA_S *pmk_calc_data = init_pbkdf2_sha1_calc((const u8 *)passphrase,
                                                           ssid,
                                                           ssid_len, 4096);
    return (void *)pmk_calc_data;
} // end of - _init_pmk_in_background -

static int _calc_ap_pmk_in_background (void *data)
{
    int ret;
    u8 pmk[32];

    PMK_CALC_DATA_S *pmk_calc_data = (PMK_CALC_DATA_S *)data;
    ret = pbkdf2_sha1_calc_func(pmk_calc_data, 32);
    if (ret != 0)
    {
        return 1; // More to go
    }

    //wpa_hexdump_key(MSG_MSGDUMP, "PSK (from passphrase)",
    //               pmk_calc_data->pmk, 32);
    memcpy((void *)pmk, (void *)pmk_calc_data->pmk, 32);
    //wpa_msg(wpa_s, MSG_INFO, "PMK Set .");

	#ifdef PROFILE_SUPPLICANT
    LOG_PRINTF("PMK finished with %uus.\n", pmk_calc_data->pmk_begin_time.ts.lt);
	#endif // PROFILE_SUPPLICANT
    ap_calc_pmk_data.pmk_calc_task=  NULL;
	pmk_done(&pmk[0]);

    deinit_pbkdf2_sha1_calc(pmk_calc_data);

    return 0;
}


void mlme_ap_pmk_calc_task (void *data)
{
    struct ap_calc_data_st *ap_data = (struct ap_calc_data_st *)data;
    if (ap_data == NULL)
        return;

    if ((*ap_data->pmk_calc_task)(ap_data->pmk_calc_data))
        MLME_add_background_task(mlme_ap_pmk_calc_task, data);
}
#endif

static void CmdEngine_CountPMK( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    const struct ap_calc_pmk_cfg *entry = (struct ap_calc_pmk_cfg *)&HostCmd->dat32;

    printf("Count PMK for ssid=%s,password=%s\n\r",entry->ssid,entry->password);

    #ifdef ENABLE_BACKGROUND_PMK_CALC
        //pmk_calc_data = _init_ap_pmk_in_background( ssid);
        //pmk_calc_task = _calc_ap_pmk_in_background;
        #ifdef ENABLE_BACKGROUND_PMK_CALC
        //ap_calc_pmk_data = OS_MemAlloc(sizeof(struct ap_calc_data_st));
        ap_calc_pmk_data.pmk_calc_data= _init_ap_pmk_in_background((u8 *)entry->ssid,entry->ssid_len,entry->password);
        ap_calc_pmk_data.pmk_calc_task = _calc_ap_pmk_in_background;
        MLME_add_background_task((void *)mlme_ap_pmk_calc_task, &ap_calc_pmk_data);
        #endif
   #else // ENABLE_BACKGROUND_PMK_CALC
        u8 psk[PMK_LEN];
        wpa_msg(wpa_s, MSG_INFO, "Calculating PSK...\n");
        pbkdf2_sha1(ssid->passphrase, (const char *)ssid->ssid, ssid->ssid_len, 4096, psk, PMK_LEN);
        //pbkdf2_sha1_flatten(ssid->passphrase, (const char *)ssid->ssid, ssid->ssid_len, 4096, psk, PMK_LEN);
        //pbkdf2_sha1_iter(ssid->passphrase, (const char *)ssid->ssid, ssid->ssid_len, 4096, psk1, PMK_LEN);
        wpa_hexdump_key(MSG_MSGDUMP, "PSK (from passphrase)",
                        psk, PMK_LEN);
		wpa_sm_set_pmk(wpa_s->wpa, psk, PMK_LEN);
		wpa_msg(wpa_s, MSG_INFO, "PMK Set .");
   #endif // ENABLE_BACKGROUND_PMK_CALC

}

static void CmdEngine_CountPTK( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    const struct ap_calc_ptk_cfg *entry = (struct ap_calc_ptk_cfg *)&HostCmd->dat32;

    u8 ptk[48];
    wpa_pmk_to_ptk(entry->pmk,32,"Pairwise key expansion",entry->AAddr,entry->SAddr,entry->ANonce,entry->SNonce,ptk,48,0);
}


#if 0

static void CmdEngine_SetTxInfoSecurity( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "\n");
}

#if (BEACON_DBG == 1)
extern int g_tx_report_test_time;
#endif
static void CmdEngine_SetWmmParam( const struct cfg_host_cmd *HostCmd )
{
#if (BEACON_DBG == 1)
	if(g_tx_report_test_time)
		return;
#endif
    CMD_DUMP(HostCmd, "\n");
    drv_edca_set_wmm_param((edca_tx_queue_type)HostCmd->dat32[0], (wmm_param *)&HostCmd->dat32[1]);
}
#endif


#if (BEACON_DBG == 1)


static u32 test_bcn_cnt = 0;
PKT_TxInfo *test_bcn_addr=NULL;
struct cfg_bcn_info test_bcn_info;
bool bReleaseTest=FALSE;


PKT_TxInfo * cmd_eng_gen_test_bcn(PKT_TxInfo * pTxBcnInfo)
{
	//PKT_TxInfo * pTxInfo;
    u16 len = pTxBcnInfo->len;
	u8 *raw;
    PKT_TxInfo *pTxInfo = (PKT_TxInfo *)PBUF_MAlloc(len, SOC_PBUF);
    ASSERT_RET(pTxInfo != NULL, (PKT_TxInfo *)NULL);


	LOG_PRINTF("Gen Test Beacon %08x\n", pTxInfo);

    //Fill some necessary descriptor info
    pTxInfo->c_type = M0_TXREQ;
    pTxInfo->len = len;
    pTxInfo->ack_policy = 1;//no ack;
    pTxInfo->hdr_offset =  pTxBcnInfo->hdr_offset;
    pTxInfo->hdr_len = pTxBcnInfo->hdr_len;
    pTxInfo->payload_offset = pTxBcnInfo->payload_offset;

    memcpy((u8 *)pTxInfo + pTxInfo->hdr_offset, (u8 *)pTxBcnInfo + pTxBcnInfo->hdr_offset, len);

    //add packet header offset in it to make prebeacon interrupt handle DTIM cnt more quickly.
    //bcn_info->tim_cnt_oft += pTxInfo->hdr_offset;

	//Modify last bit.

	test_bcn_cnt++;


	raw = (u8*)pTxInfo+pTxInfo->hdr_offset+pTxInfo->len;



#define WPA_PUT_BE32(a, val)					\
		do { /*lint -save -e845 */ \
			(a)[0] = (u8) ((((u32) (val)) >> 24) & 0xff);	\
			(a)[1] = (u8) ((((u32) (val)) >> 16) & 0xff);	\
			(a)[2] = (u8) ((((u32) (val)) >> 8) & 0xff);	\
			(a)[3] = (u8) (((u32) (val)) & 0xff);		\
			/*lint -restore */ \
		} while (0)



	{
		//u32 *ptr=(u32*)raw;
		//ptr--;

		raw-=4;
		WPA_PUT_BE32(raw, test_bcn_cnt);

		//*ptr = test_bcn_cnt;
	}


	//if(test_bcn_cnt == 65535)
	//		test_bcn_cnt=0;


    return  pTxInfo;
}


void cmd_eng_bcn_release_test()
{
#if (CONFIG_SIM_PLATFORM == 0)
	taskENTER_CRITICAL();
#endif
	if(bReleaseTest&&test_bcn_addr)
	{
		PKT_TxInfo *pTxInfo = cmd_eng_gen_test_bcn(test_bcn_addr);
		LOG_PRINTF("cmd_eng_bcn_release_test\n");

		drv_bcn_set((void*)pTxInfo, test_bcn_info.tim_cnt_oft);
		cmd_eng_bcn_create_timer_SIM();
	}

#if (CONFIG_SIM_PLATFORM == 0)
	taskEXIT_CRITICAL();
#endif
}


static void CmdEngine_SetBcnReleaseTest( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    bReleaseTest = TRUE;
    cmd_eng_bcn_release_test();
}


#endif//#if (BEACON_DBG == 1)


#if (EDCA_DBG == 1)

//u32 g_edca_rdm_idx;
static void CmdEngine_SetWmmRandom( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
#if 0
    u32 i;
    CMD_DUMP(HostCmd, "Random cnt:%d ->",HostCmd->dat32[0]);

    LOG_TAG_SUPPRESS_ON();
    for(i=0;i<HostCmd->dat32[0];i++)
    {
        CMD_PRINTF( "%d ->", HostCmd->dat32[i+1]);
        g_soc_table->rdm[i] =  HostCmd->dat32[i+1];
    }
    LOG_TAG_SUPPRESS_OFF();

    CMD_PRINTF( "\n");
#endif
}


static void CmdEngine_SetTxPacketCnt( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    u32 val = HostCmd->dat32[0];
    //	g_edca_rdm_idx = 0;
    if(g_tx_report_test_time)
        return;

    CMD_DUMP(HostCmd, "value=%u\n", val);
    //Write frame counter
    SET_TX_SEG((u16)val);
}
#endif//#if (EDCA_DBG == 1)

#if 0
static void CmdEngine_SetTxopSubFrmTime( const struct cfg_host_cmd *HostCmd )
{
	u32 val = HostCmd->dat32[0];
    CMD_DUMP(HostCmd, "value=%04X\n", val);

	if(val == 0x01)
	{
		//Fixed Backoff counter
	    SET_TXQ0_MTX_Q_BKF_CNT_FIXED(1);
		SET_TXQ1_MTX_Q_BKF_CNT_FIXED(1);
		SET_TXQ2_MTX_Q_BKF_CNT_FIXED(1);
		SET_TXQ3_MTX_Q_BKF_CNT_FIXED(1);
		//SET_TXQ4_MTX_Q_BKF_CNT_FIXED(1);
		//SET_TXQ5_MTX_Q_BKF_CNT_FIXED(1);

		SET_TXQ0_MTX_Q_TXOP_SUB_FRM_TIME(1);
		SET_TXQ1_MTX_Q_TXOP_SUB_FRM_TIME(1);
		SET_TXQ2_MTX_Q_TXOP_SUB_FRM_TIME(1);
		SET_TXQ3_MTX_Q_TXOP_SUB_FRM_TIME(1);

		SET_TXQ0_MTX_Q_TXOP_FRC_BUR(1);
		SET_TXQ1_MTX_Q_TXOP_FRC_BUR(1);
		SET_TXQ2_MTX_Q_TXOP_FRC_BUR(1);
		SET_TXQ3_MTX_Q_TXOP_FRC_BUR(1);

	}
}


static void CmdEngine_SetTxQueueHalt( const struct cfg_host_cmd *HostCmd )
{
    u32 val = HostCmd->dat32[0];
    CMD_DUMP(HostCmd, "value=%04X\n", val);

    if(val == 0x3f)
        drv_edca_queue_halt(EDCA_AC_ALL, TRUE);
    else
        drv_edca_queue_halt(EDCA_AC_ALL, FALSE);
}


static void CmdEngine_SetSTAOperationMode( const struct cfg_host_cmd *HostCmd )
{
    //u32 op_mode = HostCmd->dat32[0];
    //CMD_DUMP(HostCmd, "op_mode=0x%08X\n", op_mode);

    CHECK_DATA_LEN(HostCmd, 4);

    drv_mac_set_op_mode(HostCmd->dat32[0]);
}


static void CmdEngine_SetNav( const struct cfg_host_cmd *HostCmd )
{
    u32 nav = HostCmd->dat32[0];

    CHECK_DATA_LEN(HostCmd, 4);

    CMD_DUMP(HostCmd, "NAV=%u\n", nav);

    drv_mac_set_nav(nav);
}

static void CmdEngine_SetRxSniffer( const struct cfg_host_cmd *HostCmd )
{
    u32 enable = HostCmd->dat32[0];

    CHECK_DATA_LEN(HostCmd, 4);

    CMD_DUMP(HostCmd, "val=%u\n", enable);
    drv_mac_set_sniffer_mode(enable);
}


static void CmdEngine_SetRx2Host( const struct cfg_host_cmd *HostCmd )
{
    u32 rx2host = HostCmd->dat32[0];

    CHECK_DATA_LEN(HostCmd, 4);

    CMD_DUMP(HostCmd, "val=%u\n", rx2host);

    drv_mac_set_rx2host(rx2host);
}


static void CmdEngine_SetTxRx( const struct cfg_host_cmd *HostCmd)
{
    u32 TxRx = HostCmd->dat32[0];

    CHECK_DATA_LEN(HostCmd, 4);

    CMD_DUMP(HostCmd, "val=%u\n", TxRx);
    drv_mac_set_mac_loopback(TxRx);
}


static void CmdEngine_Set80211HdrStripOff( const struct cfg_host_cmd *HostCmd )
{
    u32     hdr_strip_off = HostCmd->dat32[0];

    CMD_DUMP(HostCmd, "val=%u\n", hdr_strip_off);

    CHECK_DATA_LEN(HostCmd, 4);

    drv_mac_set_hdr_strip(hdr_strip_off);
}


static void CmdEngine_SetTrapMask( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "val0=0x%08x, val1=0x%08x\n",
             HostCmd->dat32[0], HostCmd->dat32[1]);

    CHECK_DATA_LEN(HostCmd, 8);

    drv_mac_set_trap_mask(HostCmd->dat32);
}


static void CmdEngine_SetGlobalSeqCtrl( const struct cfg_host_cmd *HostCmd )
{
    u32 global_seq_ctrl = HostCmd->dat32[0];
    CMD_DUMP(HostCmd, "val=%u\n", global_seq_ctrl);

    CHECK_DATA_LEN(HostCmd, sizeof(u32));

    drv_mac_set_group_seq_ctrl(global_seq_ctrl);
}


static void CmdEngine_SetAutoSeqNo( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "val =%u\n", (HostCmd->dat32[0]));

    CHECK_DATA_LEN(HostCmd, 4);

    drv_mac_set_auto_seqno(HostCmd->dat32[0]);
}


static void CmdEngine_RxNullDataTrap( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "val =%u\n", (HostCmd->dat32[0]));

    CHECK_DATA_LEN(HostCmd, 4);

    drv_mac_set_rx_null_data_trap_en(HostCmd->dat32[0]);
}

static void CmdEngine_SetM2Rx2Host( const struct cfg_host_cmd *HostCmd )
{
	CMD_DUMP(HostCmd, "val=%u\n", (HostCmd->dat32[0]));

    CHECK_DATA_LEN(HostCmd, 4);

	drv_mac_set_rx2host_m2(HostCmd->dat32[0]);
}

#endif

extern void rate_control_set_erp_protect(u8 param);
static void CmdEngine_SetErpProtect( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    bool erp_protect = HostCmd->dat8[0];
    CMD_DUMP(HostCmd, "val=%u\n", (HostCmd->dat32[0]));

    //CHECK_DATA_LEN(HostCmd, 4);

    //This reg will force hci to set do_rts_cts flag.
    //But we need sw to control it by phy rate, so remove!!
    //drv_mac_set_erp_protect(HostCmd->dat32[0]);

    printf( "Erp_protect=%d\n", erp_protect);
    rate_control_set_erp_protect(erp_protect);
}

#if 0
static void CmdEngine_SetPBufOffset( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "val=%u\n", (HostCmd->dat32[0]));

    CHECK_DATA_LEN(HostCmd, 4);

    drv_mac_set_pbuf_offset(HostCmd->dat32[0]);
}


static void CmdEngine_SetDupFilter( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "val=%u\n", (HostCmd->dat32[0]));

    CHECK_DATA_LEN(HostCmd, 4);

    drv_mac_set_dup_detect(HostCmd->dat32[0]);
}



static void CmdEngine_SetHTMode( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "val=%u\n", (HostCmd->dat32[0]));

    CHECK_DATA_LEN(HostCmd, 4);

    drv_mac_set_ht_mode(HostCmd->dat32[0]);
}


static void CmdEngine_SetPromisMode( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "val=%u\n", (HostCmd->dat32[0]));

    CHECK_DATA_LEN(HostCmd, 4);

    drv_mac_set_promis_mode(HostCmd->dat32[0]);
}



static void CmdEngine_SetQoSCapability( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "val=%u\n", (HostCmd->dat32[0]));

    CHECK_DATA_LEN(HostCmd, 4);

    drv_mac_set_qos_en(HostCmd->dat32[0]);
}



static void CmdEngine_SetGMFilter( const struct cfg_host_cmd *HostCmd )
{
    const struct gmfilter_st *gmflt=(const gmfilter *)(void *)&HostCmd->dat32[1];
    s32 index=(s32)HostCmd->dat32[0];

    CMD_DUMP(HostCmd, "ctrl=0x%02x, "
        "mac=%02x:%02x:%02x:%02x:%02x:%02x "
        "msk=%02x:%02x:%02x:%02x:%02x:%02x\n",
        gmflt->ctrl,
        gmflt->mac.addr[0], gmflt->mac.addr[1], gmflt->mac.addr[2],
        gmflt->mac.addr[3], gmflt->mac.addr[4], gmflt->mac.addr[5],
        gmflt->msk.addr[0], gmflt->msk.addr[1], gmflt->msk.addr[2],
        gmflt->msk.addr[3], gmflt->msk.addr[4], gmflt->msk.addr[5]
    );

    CHECK_DATA_LEN(HostCmd, sizeof(gmfilter)+sizeof(s32));

    drv_mac_set_gmfilter(index, gmflt);
}


static void CmdEngine_SetMgmtTxQID( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "val=%u\n", (HostCmd->dat32[0]));

    CHECK_DATA_LEN(HostCmd, 4);

    drv_mac_set_mgmt_txqid(HostCmd->dat32[0]);
}



static void CmdEngine_SetNonQosTxQID( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "val=%u\n", (HostCmd->dat32[0]));

    CHECK_DATA_LEN(HostCmd, 4);

    drv_mac_set_nonqos_txqid(HostCmd->dat32[0]);
}


static void CmdEngine_SetMacMode( const struct cfg_host_cmd *HostCmd )
{
    CMD_DUMP(HostCmd, "val=%u\n", (HostCmd->dat32[0]));

    CHECK_DATA_LEN(HostCmd, 4);

    if (HostCmd->dat32[0] == 0x01)
    {
    	drv_mac_set_mac_loopback(1);							//mac-loop
		drv_mac_set_mac_loopback_without_rx_check(1);
        drv_mac_set_mac_m2m(0);
		drv_mac_set_phy_loopback(0);
    }

    else if (HostCmd->dat32[0] == 0x02) //mac 2 mac loop
    {
        drv_mac_set_mac_m2m(1);
        drv_mac_set_mac_loopback(1);
        drv_mac_set_mac_loopback_without_rx_check(0);
        drv_mac_set_phy_loopback(0);

    }

    else if (HostCmd->dat32[0] == 0x04) //phy loop
    {
        drv_mac_set_phy_loopback(1);
        drv_mac_set_mac_loopback(1);
        drv_mac_set_mac_m2m(0);
        drv_mac_set_mac_loopback_without_rx_check(1);
    }

    else if (HostCmd->dat32[0] == 0x08) //rx check  to mac-loopback
    {
    	drv_mac_set_mac_loopback(1);
        drv_mac_set_phy_loopback(0);
        drv_mac_set_mac_m2m(0);
        drv_mac_set_mac_loopback_without_rx_check(0);
    }

    else
    {

        drv_mac_set_phy_loopback(0);
        drv_mac_set_mac_loopback(0);
        drv_mac_set_mac_loopback_without_rx_check(0);
        drv_mac_set_mac_m2m(0);
    }
}



static void CmdEngine_ShowAll( const struct cfg_host_cmd *HostCmd )
{
#if (CONFIG_SIM_PLATFORM == 1)
    dump_all();
#endif
}
#endif // 0

#ifdef THROUGHPUT_TEST

static void CmdEngine_Hci2Sdio( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    MsgEvent *MsgEv;

    MsgEv = (MsgEvent *)msg_evt_alloc();
    ASSERT(MsgEv != NULL);


    /* Fill MsgEvent Content: */
    MsgEv->MsgType  = MEVT_SDIO_TEST_CMD;
    MsgEv->MsgData	= (u32)HostCmd->dat32[0];
    MsgEv->MsgData1 = (u32)HostCmd->dat32[1];
    MsgEv->MsgData2 = (u32)HostCmd->dat32[2];
    MsgEv->MsgData3 = M_ENG_HWHCI;
    msg_evt_post(MBOX_TESTINGS, MsgEv);
}


static void CmdEngine_Mic2Sdio( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    MsgEvent *MsgEv;

    MsgEv = (MsgEvent *)msg_evt_alloc();
    ASSERT(MsgEv != NULL);

    /* Fill MsgEvent Content: */
    MsgEv->MsgType  = MEVT_SDIO_TEST_CMD;
    MsgEv->MsgData	= (u32)HostCmd->dat32[0];
    MsgEv->MsgData1 = (u32)HostCmd->dat32[1];
    MsgEv->MsgData2 = (u32)HostCmd->dat32[2];
    MsgEv->MsgData3 = M_ENG_MIC;

    msg_evt_post(MBOX_TESTINGS, MsgEv);
}

static void CmdEngine_Sec2Sdio( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    MsgEvent *MsgEv;

    MsgEv = (MsgEvent *)msg_evt_alloc();
    ASSERT(MsgEv != NULL);

    /* Fill MsgEvent Content: */
    MsgEv->MsgType  = MEVT_SDIO_TEST_CMD;
    MsgEv->MsgData	= (u32)HostCmd->dat32[0];
    MsgEv->MsgData1 = (u32)HostCmd->dat32[1];
    MsgEv->MsgData2 = (u32)HostCmd->dat32[2];
    MsgEv->MsgData3 = M_ENG_ENCRYPT;
    msg_evt_post(MBOX_TESTINGS, MsgEv);
}

static void  CmdEngine_Throughput( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    MsgEvent *MsgEv;

    MsgEv = (MsgEvent *)msg_evt_alloc();
    ASSERT(MsgEv != NULL);

    /* Fill MsgEvent Content: */
    MsgEv->MsgType	= MEVT_THROUGHPUT;
    MsgEv->MsgData	= (u32)HostCmd->dat32[0];
    MsgEv->MsgData1 = (u32)HostCmd->dat32[1];
    MsgEv->MsgData2 = (u32)HostCmd->dat32[2];
    MsgEv->MsgData3 = 0;
    msg_evt_post(MBOX_TESTINGS, MsgEv);
}

#endif
extern void check_rf_Setting(void);

static void CmdEngine_InitCali(const struct cfg_host_cmd **_HostCmd)
{
#define IQK_ALIGN_SIZE(size) (((size) + 4 - 1) & ~(4-1))

    printf("IQK... \r\n");
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    struct ssv6xxx_iqk_cfg *p_iqk_cfg;
    u32 TableAddr=0;
    bool ret =FALSE;

    p_iqk_cfg = (struct ssv6xxx_iqk_cfg *)HostCmd->dat32;

    g_rf_config.cfg_xtal      = p_iqk_cfg->cfg_xtal;
    g_rf_config.cfg_pa        = p_iqk_cfg->cfg_pa;
    g_rf_config.cfg_pabias_ctrl = GET_RG_PABIAS_CTRL;
    g_rf_config.cfg_pacascode_ctrl = GET_RG_PACASCODE_CTRL;
    g_rf_config.cfg_tssi_trgt = p_iqk_cfg->cfg_tssi_trgt;
    g_rf_config.cfg_tssi_div  = p_iqk_cfg->cfg_tssi_div;

    printf("xtal type[%d],%x,%x,%x,%x\n",p_iqk_cfg->cfg_xtal,
        p_iqk_cfg->cfg_def_tx_scale_11b,
        p_iqk_cfg->cfg_def_tx_scale_11b_p0d5,
        p_iqk_cfg->cfg_def_tx_scale_11g,
        p_iqk_cfg->cfg_def_tx_scale_11g_p0d5);

    g_rf_config.cfg_def_tx_scale_11b      = p_iqk_cfg->cfg_def_tx_scale_11b;
    g_rf_config.cfg_def_tx_scale_11b_p0d5 = p_iqk_cfg->cfg_def_tx_scale_11b_p0d5;
    g_rf_config.cfg_def_tx_scale_11g      = p_iqk_cfg->cfg_def_tx_scale_11g;
    g_rf_config.cfg_def_tx_scale_11g_p0d5 = p_iqk_cfg->cfg_def_tx_scale_11g_p0d5;

    if (p_iqk_cfg->cmd_sel == SSV6XXX_IQK_CMD_INIT_CALI ) {

        LOG_PRINTF("# got a init-cali cmd\n");

        extern u8 *pFirmware_phy_setting;
        extern u8 *pFirmware_rf_setting;
        extern ssv_rf_temperature firmware_rf_temper_setting[MAX_RF_TEMPER_SETTING_TABLE_SIZE];
        extern u32 firmware_phy_setting_len;
        extern u32 firmware_rf_setting_len;
        extern u32 firmware_rf_temper_setting_len;
        firmware_phy_setting_len = p_iqk_cfg->phy_tbl_size;
        firmware_rf_setting_len = p_iqk_cfg->rf_tbl_size;
        firmware_rf_temper_setting_len =p_iqk_cfg->rf_temper_tbl_size;
        TableAddr=*(u8 *)((u32)HostCmd->dat8+IQK_CFG_LEN)|
                    (*(u8 *)((u32)HostCmd->dat8+IQK_CFG_LEN+1)<<8)|
                    (*(u8 *)((u32)HostCmd->dat8+IQK_CFG_LEN+2)<<16)|
                    (*(u8 *)((u32)HostCmd->dat8+IQK_CFG_LEN+3)<<24);

        printf("HW PBUF Addr(0x%x) for PHY/RF/RF Temper table\n",TableAddr);

        if(firmware_phy_setting_len)
        {
            pFirmware_phy_setting=(u8 *)(TableAddr);
            printf("pFirmware_phy_setting(0x%x)\n",pFirmware_phy_setting);
        }

        if(firmware_rf_setting_len)
        {
            pFirmware_rf_setting=(u8 *)((u32)TableAddr+IQK_ALIGN_SIZE(firmware_phy_setting_len));
            printf("pFirmware_rf_setting(0x%x)\n",pFirmware_rf_setting);
        }
        if(firmware_rf_temper_setting_len)
        {
            if(firmware_rf_temper_setting_len==MAX_RF_TEMPER_SETTING_TABLE_SIZE*sizeof(struct ssv_rf_temperature_st))
            {
                memset(firmware_rf_temper_setting,0x00,firmware_rf_temper_setting_len);
                memcpy(firmware_rf_temper_setting, (void *)((u32)TableAddr+IQK_ALIGN_SIZE(firmware_phy_setting_len)+IQK_ALIGN_SIZE(firmware_rf_setting_len)), firmware_rf_temper_setting_len);
                printf("firmware_rf_temper_setting(0x%x)\n",(u32)((u32)TableAddr+IQK_ALIGN_SIZE(firmware_phy_setting_len)+IQK_ALIGN_SIZE(firmware_rf_setting_len)));
                
            }
            else
            {
                printf("\33[31m RF temperture setting size not match\33[0m\r\n");
            }
        }

        drv_phy_init_cali       (p_iqk_cfg->fx_sel);
        drv_phy_cali_rtbl_export(p_iqk_cfg->fx_sel);

        //check temperture to setting RF (Temperture get at function drv_phy_init_cali)
        check_rf_Setting();

        SET_TX_SEG(0x5A5AA5A5); //info host iqk done

        //Avoid TX EVM unnormal
        ret = drv_phy_tx_loopback();   

        //Send event to host
        {        
            HDR_HostEvent *ev=NULL;
            struct cfg_tx_loopback_info tx_loopback_evt;
            memset(&tx_loopback_evt,0,sizeof(struct cfg_tx_loopback_info));
            
            tx_loopback_evt.reg = REG32(ADR_PMU_3);
            tx_loopback_evt.result = ret;
            
            HOST_EVENT_ALLOC_1(ev,SOC_EVT_TX_LOOPBACK_DONE,sizeof(struct cfg_tx_loopback_info));
            memcpy((void *)HOST_EVENT_DATA_PTR(ev),&tx_loopback_evt,sizeof(struct cfg_tx_loopback_info));
            HOST_EVENT_SEND(ev);            
        }

        //These two point are assigned to packe buffer during IQK,
        //I must force to set to null after IQK because this packet buffer will be released
        pFirmware_rf_setting=NULL;
        pFirmware_phy_setting=NULL;
    }
#if 0
    else if (p_iqk_cfg->cmd_sel == SSV6XXX_IQK_CMD_RTBL_LOAD) {

        LOG_PRINTF("# got a rtbl-load cmd\n");

        drv_phy_cali_rtbl_load(p_iqk_cfg->fx_sel);
    }
    else if (p_iqk_cfg->cmd_sel == SSV6XXX_IQK_CMD_RTBL_LOAD_DEF) {

        LOG_PRINTF("# got a rtbl-load-def cmd\n");

        drv_phy_cali_rtbl_load_def(p_iqk_cfg->fx_sel);
    }
    else if (p_iqk_cfg->cmd_sel == SSV6XXX_IQK_CMD_RTBL_RESET) {

        LOG_PRINTF("# got a rtbl-reset cmd\n");

        drv_phy_cali_rtbl_reset(p_iqk_cfg->fx_sel);
    }
    else if (p_iqk_cfg->cmd_sel == SSV6XXX_IQK_CMD_RTBL_SET) {

        LOG_PRINTF("# got a rtbl-set cmd\n");

        drv_phy_cali_rtbl_set(p_iqk_cfg->fx_sel);
    }
    else if (p_iqk_cfg->cmd_sel == SSV6XXX_IQK_CMD_RTBL_EXPORT) {

        LOG_PRINTF("# got a rtbl-export cmd\n");

        drv_phy_cali_rtbl_export(p_iqk_cfg->fx_sel);
    }
    else if (p_iqk_cfg->cmd_sel == SSV6XXX_IQK_CMD_TK_EVM) {

        LOG_PRINTF("# got a tk-evm cmd\n");

        drv_phy_evm_tk(p_iqk_cfg->argv);
    }
    else if (p_iqk_cfg->cmd_sel == SSV6XXX_IQK_CMD_TK_TONE) {

        LOG_PRINTF("# got a tk-tone cmd\n");

        drv_phy_tone_tk(p_iqk_cfg->argv);
    }
    else if (p_iqk_cfg->cmd_sel == SSV6XXX_IQK_CMD_TK_CHCH) {

        LOG_PRINTF("# change channel cmd\n");

        drv_phy_channel_change(p_iqk_cfg->argv);
    }
    else {
        LOG_PRINTF("# got a invalid cmd\n");
    }
#endif
	SET_RG_MRX_EN_CNT_RST_N(1); //enable rx en cnt

    printf("IQK done \r\n");

}

extern bool bIPDEnable;
extern u32 def_setting_CE01000C;
extern u32 def_setting_CE010008;
extern u32 def_setting_CE010030;

static void CmdEngine_IPD(const struct cfg_host_cmd **_HostCmd)
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    bIPDEnable=HostCmd->dat8[0];

    //For restore channel 1-12 default value
    //0xCE01000C
    //SMAC_REG_READ(sc->sh, ADR_ABB_REGISTER_1, &sc->def_setting_CE01000C);
    def_setting_CE01000C=REG32(ADR_ABB_REGISTER_1);
    //0xCE010008
    //SMAC_REG_READ(sc->sh, ADR_LDO_REGISTER, &sc->def_setting_CE010008);
    def_setting_CE010008=REG32(ADR_LDO_REGISTER);
    //0xCE010030
    //SMAC_REG_READ(sc->sh, ADR_RX_ADC_REGISTER, &sc->def_setting_CE010030);
    def_setting_CE010030=REG32(ADR_RX_ADC_REGISTER);



    if(bIPDEnable)
        printf("IPD on\r\nCE01000C[%08x]\r\nCE010008[%08x]\r\nCE010030[%08x]\r\n",
            def_setting_CE01000C, def_setting_CE010008, def_setting_CE010030);
    else
        printf("IPD off\r\n");

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#if 0
void cmd_eng_set_beacon(u32 *bcn, struct cfg_bcn_info *info)
{
    PKT_TxInfo *pTxInfo = (PKT_TxInfo *)bcn;

	//CMD_INFO("new beacon id:%08x\n", (u32)pTxInfo);
//	{
//		u32 i;
//
//		const u8 *raw= (u8*)pTxInfo+pTxInfo->hdr_offset;
//
//		LOG_TAG_MODULE_TURN_OFF();
//		for(i=0;i<pTxInfo->len;i++)
//		{
//			CMD_PRINTF("%02x ",  raw[i]);
//		}
//		CMD_PRINTF("\n");
//		LOG_TAG_MODULE_TURN_ON();
//	}

	//LOG_PRINTF("TIM Offset:%d\n",info->tim_cnt_oft);

	drv_bcn_set((void*)pTxInfo, info->tim_cnt_oft);
}
#endif


u16 null_data_status [WLAN_MAX_STA];
/*    CFG_STA_ADD : return index for new sta, or return 0xff for no empty sta
 *    CFG_STA_DEL_ALL : return 0xff
 *    Other : return index if sta is found, or return 0xff
 */
int cmd_eng_wsid_opr(struct cfg_set_sta *sta_info, enum cfg_sta_oper sta_oper)
{
    int i;
    int empty=0xff;
    mwsid_entry wen={0};
    //const struct cfg_wsid_info *p_wsid_info = &sta_info->wsid_info;

    do{
        if(sta_oper == CFG_STA_DEL_ALL)
        {
            //reset all wsid in hw reg.
            for(i=0;i<WLAN_MAX_STA;i++)
                drv_mac_set_wsid_table(i, &wen);

            break;
        }


        //search entity
        /*
        for(i=0;i<WLAN_MAX_STA;i++)
        {

            drv_mac_get_wsid_table(i, &wen);

            if(!IS_STA_INFO_VALID(&wen))
            {
                empty = i;
                break;
            }
            else
            {
                if( IS_EQUAL_MACADDR(&wen.sta_mac, &p_wsid_info->addr))
                    break;
            }
        }
        */

        i = sta_info->wsid_info.wsid_idx;


        //clear
        memset(&wen, 0 ,sizeof(mwsid_entry));

        if(sta_oper == CFG_STA_ADD)
        {
            if(i<WLAN_MAX_STA)
            {
                empty = i;
            }
            else
            {
                printf("Add stattion wsid > WLAN_MAX_STAn");
                return 0xff;
            }

            printf("add sta" ETH_ADDR_FORMAT "in WSID:%d\n", ETH_ADDR(&sta_info->wsid_info.addr), empty);
            ssv6xxx_SWQOutUnLock(empty,TX_SW_QUEUE);
            ssv6xxx_SWQIntUnLock(empty,TX_SW_QUEUE);

            memcpy(&wen, &sta_info->wsid_info, sizeof(struct cfg_wsid_info));

            drv_mac_set_wsid_table(empty, &wen);
            return empty;
        }
        else
        {
            if(i>=WLAN_MAX_STA)
            {
                printf("Del stattion wsid > WLAN_MAX_STA\n");
                return 0xff;
            }
            printf("del sta" ETH_ADDR_FORMAT "in WSID:%d\n", ETH_ADDR(&sta_info->wsid_info.addr), i);

            drv_mac_set_wsid_table(i, &wen);
            taskENTER_CRITICAL();
            null_data_status[i] =0;
            taskEXIT_CRITICAL();

            drv_edca_queue_halt(EDCA_AC_BK+i, FALSE);
            ssv6xxx_SWQOutLock(i,TX_SW_QUEUE);
            ssv6xxx_SWQIntLock(i,TX_SW_QUEUE);
            flush_sw_queue(i,TX_SW_QUEUE);
            return i;
        }

    }while(0);

    return 0xff;
}

#if (AUTO_BEACON == 1)
enum ssv6xxx_beacon_type{
	SSV6xxx_BEACON_0,
	SSV6xxx_BEACON_1,
    SSV6xxx_BEACON_TYPE_MAX
};
u8 bcn_data[4];
u32 bcn_ptr=0;
u8 curr_bcn_idx = SSV6xxx_BEACON_TYPE_MAX;


enum ssv6xxx_beacon_type _get_working_bcn(void)
{
	u32 regval =0;
	regval = GET_MTX_BCN_CFG_VLD;

	//get MTX_BCN_CFG_VLD

	if(regval==0x2 || regval == 0x0)//bcn 0 is availabke, so working bcn is 1.
		return SSV6xxx_BEACON_1;
	else if(regval==0x1)//bcn 1 is availabke to use, so working bcn is 0.
		return SSV6xxx_BEACON_0;
	else
		LOG_PRINTF("=============>ERROR!!drv_bcn_reg_available\n");//ASSERT(FALSE);// 11 error happened need check with ASIC.

	return SSV6xxx_BEACON_0;
}

bool chk_bcn_idx_backup()
{
    int curr_idx = _get_working_bcn();

    if(curr_bcn_idx != curr_idx)
    {
        curr_bcn_idx = curr_idx;
        if(curr_bcn_idx == SSV6xxx_BEACON_0)
        {
            bcn_ptr = ((0x80000000 | (GET_MTX_BCN_PKT_ID0 << 16)) + 8);
        }
        else
        {
            bcn_ptr = ((0x80000000 | (GET_MTX_BCN_PKT_ID1 << 16)) + 8);
        }
        printf("xbackup bcn=%x\r\n",bcn_ptr);
        memcpy(bcn_data,(u8*)bcn_ptr,4);
        return true;
    }
    return false;
}
#endif //#if (AUTO_BEACON == 1)

extern SSV6XXX_STA	*g6200StaInfo;
#if (N_MODE_AMPDU_TX == 1)
extern void ampdu_addba_req(SSV6XXX_STA *sta, u8 *p8_da, u8 *p8_sa, u8 *p8_bssid);
extern u8 AMPDU_TX_ENABLE;
#endif //#if (N_MODE_AMPDU_TX == 1)
static void CmdEngine_SetSta( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    struct cfg_set_sta sta_info;
    u8 wsid = 0xff;

    CMD_DUMP(HostCmd, "val=%u\n", (HostCmd->dat32[0]));
    memcpy((void *)&sta_info,(const void *)HostCmd->dat8,sizeof(struct cfg_set_sta));

    wsid = (u8)cmd_eng_wsid_opr(&sta_info, sta_info.sta_oper);
    if(wsid < WLAN_MAX_STA)
    {
        if (sta_info.sta_oper == CFG_STA_ADD)
        {
            rate_control_pid_sta_connect(wsid, &sta_info);
            {
                HDR_HostEvent   *ev=NULL;
                u32 size=ADD_STA_EVT_HEADER_SIZE;

                HOST_EVENT_REALLOC_0(_HostCmd,ev,SOC_EVT_ADD_STA_DONE,size);

                memcpy(ev->dat,&wsid,size);
                HOST_EVENT_SET_LEN(ev, size);
                HOST_EVENT_SEND(ev);
            }
#if (N_MODE_AMPDU_TX == 1)
            if(AMPDU_TX_ENABLE == 1)
            {
                u8 bssid[6];
                drv_mac_get_bssid(bssid);
                ampdu_addba_req(&g6200StaInfo[wsid], (u8*)&sta_info.wsid_info.addr, bssid, bssid);
            }
#endif            
        }

        if(sta_info.sta_oper == CFG_STA_DEL)
        {
            rate_control_pid_sta_disconnect(wsid);
            memset(&g6200StaInfo[wsid],0,sizeof(SSV6XXX_STA));
        }
    }
    /* To-do:CmdEngine_SetSta is called at ap mode only. If other mode would call
     * this function. Need to modify beacon checking according to current mode
     */
#if (AUTO_BEACON == 1)
    if(chk_bcn_idx_backup())
        OS_TimerStart(bcn_timer);
#endif

}

#if (AUTO_BEACON == 1)
void check_bcn(void)
{
    if(GET_MTX_BCN_TIMER_EN != 1)
        return;

    chk_bcn_idx_backup();

    if(bcn_ptr)
    {
        if(memcmp(bcn_data,(u8*)bcn_ptr,4)!=0)
        {
            printf("reset bcn=%x\r\n",bcn_ptr);
            memcpy((u8*)bcn_ptr,bcn_data,4);
        }
        else
        {
            //printf("b ok\r\n");
        }
    }
}
#else //#if (AUTO_BEACON == 1)
u8 bcn_back = 0;
void set_fw_bcn(u8 bcn_idx)
{
    if ((bcn_idx == 1) || (bcn_idx == 2))
        SET_FW_BCN_ST(bcn_idx);
    else
        printf("!!!!Invalid value %d\n", bcn_idx);
}

void send_bcn(void *data)
{
    PKT_TxInfo *bcn = NULL, *target_bcn = NULL;
    u32 regval = GET_MTX_BCN_CFG_VLD;
    u8 fw_bcn_st = GET_FW_BCN_ST;
    u32 bcn_pkt_len = 0;


    if(regval == 0x1)
        bcn = (PKT_TxInfo *)((0x80000000 | (GET_MTX_BCN_PKT_ID0 << 16)));
    else if(regval == 0x2)
        bcn = (PKT_TxInfo *)((0x80000000 | (GET_MTX_BCN_PKT_ID1 << 16)));
    else
    {
        printf("=============>ERROR!!bcn is unavailable, %d\n", regval);
        return;
    }

    if ((GET_FFO6_CNT + GET_FF6_CNT) != 0 )//use macro to get mailbox number of txq 3
    {
        //printf("txq 0 has frm, bcn_back = %d \n", bcn_back);
        return;
    }
    bcn_pkt_len = bcn->len+bcn->hdr_offset;
    if(bcn_pkt_len > 511)
    {
        printf("\33[31m beacon is too big to send \33[0m\n");
        return ;
    }

    target_bcn = (PKT_TxInfo *)PBUF_MAlloc_Raw(512, 0 , NOTYPE_BUF);
    if (target_bcn == NULL)
    {
        printf("\33[31m No buff for sending beacon \33[0m\n");
        return ;
    }

    memcpy(target_bcn,bcn, bcn_pkt_len);


    if ((fw_bcn_st & regval) == 0)
        set_fw_bcn(regval);

    target_bcn->fCmd = 0x000000F6;
    target_bcn->fCmdIdx = 0;
    target_bcn->reason = 0;
    target_bcn->drate_idx = 0;
    target_bcn->crate_idx = 0;
    bcn_back = 0;
    drv_mailbox_send((u32)M_ENG_TX_EDCA0,(u32)target_bcn);
    last_bcn_ptr = (void *)target_bcn;
}
/*
void show_bcn()
{
    PKT_TxInfo *bcn0 = NULL, *bcn1 = NULL;
    bcn0 = (PKT_TxInfo *)((0x80000000 | (GET_MTX_BCN_PKT_ID0 << 16)));
    bcn1 = (PKT_TxInfo *)((0x80000000 | (GET_MTX_BCN_PKT_ID1 << 16)));
    printf("=============>bcn 0<=============\n");
    hex_dump((void*)bcn0, 64);
    printf("=============>end<=============\n");
    printf("=============>bcn 1<=============\n");
    hex_dump((void*)bcn1, 64);
    printf("=============>end<=============\n");
}
*/
void CmdEngine_set_self_bcn( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    u8 bcn_st = HostCmd->dat8[0];

    if (bcn_st != self_bcn_st)
    {
        if (bcn_st != 0)
        {
            //show_bcn();
            hwtmr_start(BCN_TIMER, BEACON_PERIOD, send_bcn, NULL, HTMR_PERIODIC);
        }
        else
            hwtmr_stop(BCN_TIMER);
        self_bcn_st = bcn_st;
    }

}

#endif

#if 0
static void CmdEngine_SetMultiMacMode (const struct cfg_host_cmd *HostCmd)
{
    u8 mask = 0;
    mask = *((u8*)HostCmd->dat8);
    if (mask > 7)
        return;
    CMD_DUMP(HostCmd, "val=%u\n", (HostCmd->dat32[0]));
    SET_MRX_MULTI_ADDR1(mask);
}

static void CmdEngine_SetRXToDSMask (const struct cfg_host_cmd *HostCmd)
{
    u8 mask = 0;
    mask = *((u8*)HostCmd->dat8);
    if (mask > 2)
        return;
    CMD_DUMP(HostCmd, "val=%u\n", (HostCmd->dat32[0]));
    SET_MRX_TODS_MASK(mask);

}
#endif

static u32 length=0;
#define PER_SIZE 1516
u32 last_seq_no=0;
static void  CmdEngine_Bus_Throughput( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    HDR_HostEvent   *ev=NULL;
    ST_BUS_THROUTHPUT *pstTput=NULL;
    u32 i=0;

    pstTput=(ST_BUS_THROUTHPUT *)HostCmd->dat8;
    //printf("CmdEngine_Bus_Throughput:%d\r\n",HostCmd->cmd_seq_no);
    if(pstTput->status==EN_TX_START){
        length=pstTput->un.TotalSize;
        printf("CmdEngine_Bus_Throughput: TX Start\r\n");
        printf("pstTput->size=%d\r\n",pstTput->un.Size);
        printf("pstTput->status=%d\r\n",pstTput->status);
        printf("length=%d\r\n",length);
        last_seq_no=HostCmd->cmd_seq_no;
    }

    if(pstTput->status==EN_GOING){
        if(HostCmd->cmd_seq_no!=(last_seq_no+1)){
            //printf("last_seq_no=%d\r\n",last_seq_no);
            //printf("last_seq_no=%d\r\n",last_seq_no);
            //printf("last_seq_no=%d\r\n",last_seq_no);
            //printf("last_seq_no=%d\r\n",last_seq_no);
            //printf("last_seq_no=%d\r\n",last_seq_no);
            //printf("last_seq_no=%d\r\n",last_seq_no);
            //printf("last_seq_no=%d\r\n",last_seq_no);
            ASSERT(0);
        }
        length-=pstTput->un.Size;
        //printf("CmdEngine_Bus_Throughput:Going\r\n");
        //printf("pstTput->size=%d\r\n",pstTput->un.Size);
        //printf("pstTput->status=%d\r\n",pstTput->status);
        //printf("length=%d\r\n",length);
        last_seq_no=HostCmd->cmd_seq_no;
    }

    if(pstTput->status==EN_RX_START){
        length=pstTput->un.TotalSize;
        //printf("CmdEngine_Bus_Throughput:RX Start\r\n");
        //printf("pstTput->size=%d\r\n",pstTput->un.Size);
        //printf("pstTput->status=%d\r\n",pstTput->status);
        //printf("length=%d\r\n",length);
        do{
            //printf("Send Data,i=%d\r\n",i);
            HOST_EVENT_ALLOC(ev,SOC_EVT_BUS_THROUTHPUT_TEST,PER_SIZE);
            ev->evt_seq_no = i;
            pstTput = (ST_BUS_THROUTHPUT *)(HOST_EVENT_DATA_PTR(ev));
            pstTput->un.Size=PER_SIZE;
            pstTput->status=EN_GOING;
            HOST_EVENT_SET_LEN(ev,PER_SIZE);
            /*
            Here, I don't send the HOST_EVT through rx sw queue, because this function works on soft-mac-task,
            if I send the packet to rx data queue first, it will cause the dead lock.
            */
            //HOST_EVENT_SEND(ev);
            drv_mailbox_send(M_ENG_HWHCI,(u32)ev);
            length-=PER_SIZE;
            i++;
            ev=NULL;
        }while(length!=0);
    }

    if(length==0){
        printf("CmdEngine_Bus_Throughput:end\r\n");
        HOST_EVENT_ALLOC(ev, SOC_EVT_BUS_THROUTHPUT_TEST , sizeof(ST_BUS_THROUTHPUT));
        pstTput = (ST_BUS_THROUTHPUT *)(HOST_EVENT_DATA_PTR(ev));
        pstTput->un.Size=0;
        pstTput->status=EN_END;
        HOST_EVENT_SET_LEN(ev, sizeof(ST_BUS_THROUTHPUT));
        /*
        Here, I don't send the HOST_EVT through rx sw queue, because this function works on soft-mac-task,
        if I send the packet to rx data queue first, it will cause the dead lock.
        */
        //HOST_EVENT_SEND(ev);
        drv_mailbox_send(M_ENG_HWHCI,(u32)ev);
    }


    return;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void CmdEngine_BusLoopback(const struct cfg_host_cmd **_HostCmd)
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    PKT_RxInfo *NewPkt=NULL;
    HDR_HostEvent *host_evt;
    u32 count=0;
    count=HostCmd->dat8[0]|(HostCmd->dat8[1]<<8)|(HostCmd->dat8[2]<<16)|(HostCmd->dat8[3]<<24);

    printf("bus test cmd,len=%d, count=%d\n",HostCmd->len,count);
    //hex_dump(((void*)HostCmd->dat8), HostCmd->len-HOST_CMD_HDR_LEN);
    ssv6xxx_raw_dump((char *)HostCmd->dat8,HostCmd->len-HOST_CMD_HDR_LEN);

    NewPkt = (PKT_RxInfo *)PBUF_MAlloc(HostCmd->len, RX_BUF);
    if(NewPkt == NULL)
    {
        printf("allocat bus test cmd fail!\n");
        return;
    }

    host_evt = (HDR_HostEvent *)NewPkt;
    host_evt->c_type = HOST_EVENT;
    host_evt->h_event = SOC_EVT_BUS_LOOPBACK;
    host_evt->len = HostCmd->len;
    memcpy(host_evt->dat,HostCmd->dat8,(HostCmd->len-HOST_CMD_HDR_LEN));
    printf("bus test cmd,len=%d\n",HostCmd->len);
    TX_FRAME((u32)NewPkt);

    NewPkt = NULL;
    printf("loop back data\r\n");

}

extern void rate_control_set_default_rates_mask(u16 dftrate);
extern void rate_control_set_param (u32 param);
extern void rate_control_set_gf(u8 gf_st);
extern void rate_control_set_variables (u32 mode, u32 param);
extern void rate_control_set_feature(u32 feature, bool IsActive);

static void CmdEngine_SetRCValues(const struct cfg_host_cmd **_HostCmd)
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    struct Host_cfg *p_rccfg = (struct Host_cfg *)HostCmd->dat32;
    u32 per = p_rccfg->upgrade_per<<16|p_rccfg->downgrade_per;

    rate_control_set_variables(RC_OPT_DRATE_ENDIAN,(u32)p_rccfg->rc_drate_endian);
    rate_control_set_default_rates_mask(p_rccfg->rate_mask);        
    rate_control_set_feature(RC_FEAT_RESEND_FAILREPORT, (u32)p_rccfg->resent_fail_report);
    rate_control_set_variables(RC_OPT_SET_PER, per);
    rate_control_set_variables(RC_OPT_ENA_PREALLOCATED_PRB_FRM, (u32)p_rccfg->pre_alloc_prb_frm);
    rate_control_set_variables(RC_OPT_SET_UP_PER_FASTEST_B, (u32)p_rccfg->upper_fastestb);
    rate_control_set_variables(RC_OPT_DIRECT_DWGRADE_RATE, (u32)p_rccfg->direct_rc_down);
    rate_control_set_variables(RC_OPT_FORCE_RTS_CTS, (u32)p_rccfg->force_RTSCTS);

    
}

void CmdEngine_SetPwrSaving( const struct cfg_host_cmd **_HostCmd )
{
    // Only valid @ STA mode
    if(drv_mac_get_op_mode()!=0) //0:STA mode
    {
    	LOG_PRINTF("PS cmd Only valid @ STA mode\n");
    	return;
    }

    printf("ps cmd\r\n");
    _bypass_cmd_to_mlme_task(_HostCmd);
    *_HostCmd=NULL; //Reuse pbuf
}
u8 recover_mechanism=0;
u8 recover_enable=0;
extern Time_T last_tx_rx_time;
void check_tx_rx_traffic(void)
{
    u32 elapsed = dbg_get_elapsed(&last_tx_rx_time);
    if(elapsed>100000) // drv_mac no packet over 100 ms.
    {
        //printf("elapsed%d\n",elapsed);
        dbg_get_time(&last_tx_rx_time);
        REG_MS_TIMER[3].TMR_CTRL =1;
    }

}
OsTimer recover_timer;
void CmdEngine_recover( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    //OsTimer recover_timer;
    recover_enable = HostCmd->dat8[0];
    recover_mechanism = HostCmd->dat8[1];
    printf("recover(%d,%d)\n",recover_enable,recover_mechanism);

    if(recover_enable > 0 && recover_mechanism == 1)
    {
        OS_TimerCreate(&recover_timer, 100, (u8)TRUE, NULL, (OsTimerHandler)check_tx_rx_traffic);
        OS_TimerStart(recover_timer);
    }
}
#define HT_INFO_OPERATION_MODE_OP_MODE_MASK	\
		((le16) (0x0001 | 0x0002))
#define HT_INFO_OPERATION_MODE_OP_MODE_OFFSET		0
#define HT_INFO_OPERATION_MODE_NON_GF_DEVS_PRESENT	((u8) BIT(2))
#define HT_INFO_OPERATION_MODE_TRANSMIT_BURST_LIMIT	((u8) BIT(3))
#define HT_INFO_OPERATION_MODE_NON_HT_STA_PRESENT	((u8) BIT(4))

void CmdEngine_SetHTProtect( const struct cfg_host_cmd ** _HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    u16 operation_mode = (u16)HostCmd->dat16[0];
    u8 gf_st = ((operation_mode & 0x17)==0)?1:0;
    //bool erp_protect = ((!!(operation_mode&HT_INFO_OPERATION_MODE_OP_MODE_MASK))==1)?TRUE:FALSE;

    printf( "HT protect=%d\n", !!(operation_mode&HT_INFO_OPERATION_MODE_OP_MODE_MASK));
    printf( "NON_GF_STA=%d\n", !!(operation_mode&HT_INFO_OPERATION_MODE_NON_GF_DEVS_PRESENT));
    printf( "OBSS_NON_HT_STA=%d\n", !!(operation_mode&HT_INFO_OPERATION_MODE_NON_HT_STA_PRESENT));

    rate_control_set_gf(gf_st);
}

static void CmdEngine_GetDQStatus( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    struct cfg_dq_status *_dq=(struct cfg_dq_status *)HostCmd->dat8;
    s32 wsid=_dq->wsid;
    HDR_HostEvent *ev=NULL;
    struct resp_evt_result *resp=NULL;

    HOST_EVENT_REALLOC_1((void **)_HostCmd,ev,SOC_EVT_DATA_QUEUE_STATUS,DQ_STATUS_EVT_HEADER_SIZE);
    if(ev!=NULL)
    {
        resp=(struct resp_evt_result *)ev->dat;
        resp->u.dq_status.wsid=wsid;
        resp->u.dq_status.len=ssv6xxx_SWQWaittingSize(wsid,TX_SW_QUEUE);
        HOST_EVENT_SEND(ev);
    }

}
#if (N_MODE_AMPDU_TX == 1)
void CmdEngine_SetAMPDU_Param( const struct cfg_host_cmd ** _HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    u8 mode = HostCmd->dat8[0];
    u8 value = HostCmd->dat8[1]; 
    
    ampdu_tx_set_options(mode, value);
}
#endif
/*----------------------------  End of HostCmd  ----------------------------*/

static void CmdEngine_SetConfig( const struct cfg_host_cmd **HostCmd );
static void CmdEngine_GetConfig( const struct cfg_host_cmd **HostCmd );
static void CmdEngine_GetSocStatus( const struct cfg_host_cmd **HostCmd );
//static void CmdEngine_GetStaMac( const struct cfg_host_cmd *HostCmd );
//static void CmdEngine_GetBSSID( const struct cfg_host_cmd *HostCmd );
//static void CmdEngine_GetDeciTable( const struct cfg_host_cmd *HostCmd );
//static void CmdEngine_GetWSIDTable( const struct cfg_host_cmd *HostCmd );
//static void CmdEngine_GetTxEthTrap( const struct cfg_host_cmd *HostCmd );
//static void CmdEngine_GetRxEthTrap( const struct cfg_host_cmd *HostCmd );
//static void CmdEngine_GetEtherTrap( const struct cfg_host_cmd *HostCmd );
//static void CmdEngine_GetFCmdTxData( const struct cfg_host_cmd *HostCmd );
//static void CmdEngine_GetFCmdTxMgmt( const struct cfg_host_cmd *HostCmd );
//static void CmdEngine_GetFCmdTxCtrl( const struct cfg_host_cmd *HostCmd );
//static void CmdEngine_GetFCmdRxData( const struct cfg_host_cmd *HostCmd );
//static void CmdEngine_GetFCmdRxMgmt( const struct cfg_host_cmd *HostCmd );
//static void CmdEngine_GetFCmdRxCtrl( const struct cfg_host_cmd *HostCmd );
//static void CmdEngine_SetMib( const struct cfg_host_cmd *HostCmd );
#if 0
static void CmdEngine_GetSifs( const struct cfg_host_cmd *HostCmd );
static void CmdEngine_GetDifs( const struct cfg_host_cmd *HostCmd );
static void CmdEngine_GetEifs( const struct cfg_host_cmd *HostCmd );
#endif

#ifdef ENABLE_LOG_TO_HOST
static void CmdEngine_LOG( const struct cfg_host_cmd **HostCmd );
#endif
/**
 *  Host Command Registration Table:
 *
 *  Register supported host command to this table, so the
 *  CmdEng_CmdRequest() can dispatch commands to their
 *  corresponding handlers.
 */
typedef void (*TBL_HostCmdHandler)(const struct cfg_host_cmd **);


typedef struct _HostCmdMap_S {
    ssv6xxx_host_cmd_id     event_id;
    TBL_HostCmdHandler  event_handler;
} HostCmdMap_S;

const HostCmdMap_S HostCmdMap[] = {
    //===========================================================================
    //Public command
    {SSV6XXX_HOST_CMD_SCAN,                  CmdEngine_ScanRequest            },
#ifdef AIRKISS_ENABLE
    {SSV6XXX_HOST_CMD_SMART_CONFIG,          CmdEngine_SconfigRequest         },
#endif
    {SSV6XXX_HOST_CMD_JOIN,                  CmdEngine_JoinRequest            },
    {SSV6XXX_HOST_CMD_LEAVE,                 CmdEngine_LeaveRequest           },
    {SSV6XXX_HOST_CMD_SET_CONFIG,            CmdEngine_SetConfig              },
    {SSV6XXX_HOST_CMD_GET_CONFIG,            CmdEngine_GetConfig              },
//    {SSV6XXX_HOST_CMD_SET_REG,               CmdEngine_SetReg                 },
//    {SSV6XXX_HOST_CMD_GET_REG,               CmdEngine_GetReg                 },
//    {SSV6XXX_HOST_CMD_SET_STA_MAC,           CmdEngine_SetStaMac              },
//    {SSV6XXX_HOST_CMD_SET_BSSID,             CmdEngine_SetBSSID               },
//    {SSV6XXX_HOST_CMD_SET_DECITBL,           CmdEngine_SetDeciTable           },
//    {SSV6XXX_HOST_CMD_SET_WSIDTBL,           CmdEngine_SetWSIDTable           },
//    {SSV6XXX_HOST_CMD_SET_TX_ETHTRAP,        CmdEngine_SetTxEthTrap           },
//    {SSV6XXX_HOST_CMD_SET_RX_ETHTRAP,        CmdEngine_SetRxEthTrap           },
    {SSV6XXX_HOST_CMD_SET_FCMD_TXDATA,       CmdEngine_SetFCmdTxData          },
    {SSV6XXX_HOST_CMD_SET_FCMD_TXMGMT,       CmdEngine_SetFCmdTxMgmt          },
    {SSV6XXX_HOST_CMD_SET_FCMD_TXCTRL,       CmdEngine_SetFCmdTxCtrl          },
    {SSV6XXX_HOST_CMD_SET_FCMD_RXDATA,       CmdEngine_SetFCmdRxData          },
    {SSV6XXX_HOST_CMD_SET_FCMD_RXMGMT,       CmdEngine_SetFCmdRxMgmt          },
    {SSV6XXX_HOST_CMD_SET_FCMD_RXCTRL,       CmdEngine_SetFCmdRxCtrl          },
//    {SSV6XXX_HOST_CMD_SET_SIFS,              CmdEngine_SetSifs                },
//    {SSV6XXX_HOST_CMD_SET_DIFS,              CmdEngine_SetDifs                },
//    {SSV6XXX_HOST_CMD_SET_EIFS,              CmdEngine_SetEifs                },
    //Security
//    {SSV6XXX_HOST_CMD_SET_PAIR_SECURITY,     CmdEngine_SetPairSecurity        },
//    {SSV6XXX_HOST_CMD_SET_GROUP_SECURITY,    CmdEngine_SetGroupSecurity       },
//	{SSV6XXX_HOST_CMD_SET_PAIR_ENTRY,		 CmdEngine_SetPairEntry			  },
//	{SSV6XXX_HOST_CMD_SET_GROUP_ENTRY,       CmdEngine_SetGroupEntry          },
    {SSV6XXX_HOST_CMD_SET_SECURITY_ENTRY,    CmdEngine_SetSecurityEntry       },
//    {SSV6XXX_HOST_CMD_SET_TX_INFO_SECURITY,  CmdEngine_SetTxInfoSecurity	  },
//    {SSV6XXX_HOST_CMD_SET_WMM_PARAM,         CmdEngine_SetWmmParam            },
#if (EDCA_DBG == 1)
	{SSV6XXX_HOST_CMD_SET_WMM_RANDOM,        CmdEngine_SetWmmRandom           },
    {SSV6XXX_HOST_CMD_SET_TX_PACKET_CNT,     CmdEngine_SetTxPacketCnt         },
#endif
//	{SSV6XXX_HOST_CMD_SET_TX_QUEUE_HALT,     CmdEngine_SetTxQueueHalt         },
//    {SSV6XXX_HOST_CMD_SET_TXOP_SUB_FRM_TIME, CmdEngine_SetTxopSubFrmTime       },
//    {SSV6XXX_HOST_CMD_SET_OPMODE,            CmdEngine_SetSTAOperationMode    },
//    {SSV6XXX_HOST_CMD_SET_NAV,               CmdEngine_SetNav                 },
//    {SSV6XXX_HOST_CMD_SET_RX2HOST,           CmdEngine_SetRx2Host             },
//    {SSV6XXX_HOST_CMD_SET_RXSNIFFER,         CmdEngine_SetRxSniffer             },
//    {SSV6XXX_HOST_CMD_STRIP_OFF,             CmdEngine_Set80211HdrStripOff    },
//    {SSV6XXX_HOST_CMD_SET_TRAP_MASK,         CmdEngine_SetTrapMask            },
//    {SSV6XXX_HOST_CMD_SET_GLOBAL_SEQCTRL,    CmdEngine_SetGlobalSeqCtrl       },
//    {SSV6XXX_HOST_CMD_SET_AUTO_SEQNO,        CmdEngine_SetAutoSeqNo           },
//    {SSV6XXX_HOST_CMD_SET_RX_NULL_DATA_TRAP, CmdEngine_RxNullDataTrap           },
//    {SSV6XXX_HOST_CMD_SET_RX_INFO_SIZE,      CmdEngine_Set_Rx_Info_Size       },
    {SSV6XXX_HOST_CMD_SET_ERP_PROTECT,       CmdEngine_SetErpProtect          },
//    {SSV6XXX_HOST_CMD_SET_PBUF_OFFSET,       CmdEngine_SetPBufOffset          },
//    {SSV6XXX_HOST_CMD_SET_DUP_FLT,           CmdEngine_SetDupFilter           },
//    {SSV6XXX_HOST_CMD_SET_HT_MODE,           CmdEngine_SetHTMode              },
//    {SSV6XXX_HOST_CMD_SET_PROMIS_MODE,       CmdEngine_SetPromisMode          },
//    {SSV6XXX_HOST_CMD_SET_QOS_CAP,           CmdEngine_SetQoSCapability       },
//    {SSV6XXX_HOST_CMD_SET_GMFLT,             CmdEngine_SetGMFilter            },
//    {SSV6XXX_HOST_CMD_SET_MGMT_TXQID,        CmdEngine_SetMgmtTxQID           },
//    {SSV6XXX_HOST_CMD_SET_NONQOS_TXQID,      CmdEngine_SetNonQosTxQID         },
//    {SSV6XXX_HOST_CMD_SET_MAC_MODE,          CmdEngine_SetMacMode             },

//	{SSV6XXX_HOST_CMD_SET_STOP_BCN, 		CmdEngine_SetStopBeacon			  },

    {SSV6XXX_HOST_CMD_SET_STA,               CmdEngine_SetSta                 },
    {SSV6XXX_HOST_CMD_DQ_STATUS,            CmdEngine_GetDQStatus             },
//    {SSV6XXX_HOST_CMD_GET_STA_MAC,           CmdEngine_GetStaMac              },
//    {SSV6XXX_HOST_CMD_GET_BSSID,             CmdEngine_GetBSSID               },
//    {SSV6XXX_HOST_CMD_GET_DECI_TBL,          CmdEngine_GetDeciTable           },
//    {SSV6XXX_HOST_CMD_GET_WSID_TBL,          CmdEngine_GetWSIDTable           },
//
//    {SSV6XXX_HOST_CMD_GET_ETHER_TRAP,        CmdEngine_GetEtherTrap           },
//    {SSV6XXX_HOST_CMD_GET_FCMDS,             CmdEngine_GetFCmds               },
#if 0
    {SSV6XXX_HOST_CMD_GET_TX_ETH_TRAP,       CmdEngine_GetTxEthTrap           },
    {SSV6XXX_HOST_CMD_GET_RX_ETH_TRAP,       CmdEngine_GetRxEthTrap           },
    {SSV6XXX_HOST_CMD_GET_FCMD_TX_DATA,      CmdEngine_GetFCmdTxData          },
    {SSV6XXX_HOST_CMD_GET_FCMD_TX_MGMT,      CmdEngine_GetFCmdTxMgmt          },
    {SSV6XXX_HOST_CMD_GET_FCMD_TX_CTRL,      CmdEngine_GetFCmdTxCtrl          },
    {SSV6XXX_HOST_CMD_GET_FCMD_RX_DATA,      CmdEngine_GetFCmdRxData          },
    {SSV6XXX_HOST_CMD_GET_FCMD_RX_MGMT,      CmdEngine_GetFCmdRxMgmt          },
    {SSV6XXX_HOST_CMD_GET_FCMD_RX_CTRL,      CmdEngine_GetFCmdRxCtrl          },
#endif
//    {SSV6XXX_HOST_CMD_SET_MIB,               CmdEngine_SetMib                 },
//	{SSV6XXX_HOST_CMD_INIT_PHY_TABLE,        CmdEngine_InitPhyTable           },
//    {SSV6XXX_HOST_CMD_SET_PHY_INFO_TBL,      CmdEngine_SetPhyInfoTable        },
//    {SSV6XXX_HOST_CMD_GET_PHY_INFO_TBL,      CmdEngine_GetPhyInfoTable        },
//    {SSV6XXX_HOST_CMD_SET_RX_CHECK ,         CmdEngine_SetTxRx                },
//    {SSV6XXX_HOST_CMD_SHOW_ALL ,             CmdEngine_ShowAll                },
#if 0
    {SSV6XXX_HOST_CMD_GET_SIFS,              CmdEngine_GetSifs                },
    {SSV6XXX_HOST_CMD_GET_DIFS,              CmdEngine_GetDifs                },
    {SSV6XXX_HOST_CMD_GET_EIFS,              CmdEngine_GetEifs                },
#endif
#if 0
    {SSV6XXX_HOST_CMD_ADDBA_RESP,             CmdEngine_ADDBA_RESP             },
    {SSV6XXX_HOST_CMD_DELBA,                  CmdEngine_DELBA                  },
#endif

#ifdef ENABLE_LOG_TO_HOST
	{SSV6XXX_HOST_CMD_LOG,					 CmdEngine_LOG                    },
#endif

//	{SSV6XXX_HOST_CMD_PHY_ON,				CmdEngine_PHY_ON				},
	{SSV6XXX_HOST_CMD_CAL,					 CmdEngine_Cal					  },
    {SSV6XXX_HOST_CMD_SET_BCN_TIMER_EN,      CmdEngine_BCN_Timer_En           },
    {SSV6XXX_HOST_CMD_GET_SOC_STATUS,        CmdEngine_GetSocStatus           },
    {SSV6XXX_HOST_CMD_INIT_CALI,             CmdEngine_InitCali               },
    {SSV6XXX_HOST_CMD_IPD,               CmdEngine_IPD               },

//    {SSV6XXX_HOST_CMD_GET_CHANNEL,           CmdEngine_GetCurrentChan         },
#ifdef THROUGHPUT_TEST
	{SSV6XXX_HOST_CMD_HCI2SDIO,               CmdEngine_Hci2Sdio 			  },
	{SSV6XXX_HOST_CMD_MIC2SDIO,               CmdEngine_Mic2Sdio 			  },
	{SSV6XXX_HOST_CMD_SEC2SDIO,               CmdEngine_Sec2Sdio 			  },

	{SSV6XXX_HOST_CMD_THROUGHTPUT,			 CmdEngine_Throughput 			  },

#endif
#if (BEACON_DBG == 1)
	{SSV6XXX_HOST_CMD_BEACON_RELEASE_TEST,        CmdEngine_SetBcnReleaseTest},
#endif
    {SSV6XXX_HOST_CMD_BUS_THROUGHPUT_TEST,          CmdEngine_Bus_Throughput},
    {SSV6XXX_HOST_CMD_BUS_LOOPBACK_TEST,            CmdEngine_BusLoopback},
    {SSV6XXX_HOST_CMD_SET_PWR_SAVING,               CmdEngine_SetPwrSaving  },
    {SSV6XXX_HOST_CMD_SET_HT_PROTECT,               CmdEngine_SetHTProtect  },
    {SSV6XXX_HOST_CMD_GET_PMK,             CmdEngine_CountPMK},
    {SSV6XXX_HOST_CMD_CALC_PTK,            CmdEngine_CountPTK},
    {SSV6XXX_HOST_CMD_RECOVER,            CmdEngine_recover},
#if (AUTO_BEACON != 1)
    {SSV6XXX_HOST_CMD_SELF_BCN_ENABLE,            CmdEngine_set_self_bcn},
#endif
#if (N_MODE_AMPDU_TX ==1)
    {SSV6XXX_HOST_CMD_SET_AMPDU_PARAM,   CmdEngine_SetAMPDU_Param},
#endif
    {SSV6XXX_HOST_CMD_SET_RC_VALUES,   CmdEngine_SetRCValues},
//BA releated
//	{SSV6XXX_HOST_CMD_SET_MULTI_MAC_MODE,        CmdEngine_SetMultiMacMode},
//	{SSV6XXX_HOST_CMD_SET_RX_TODS_MASK,        CmdEngine_SetRXToDSMask},

};

#if 0
static void _send_hcmd_ack_to_host(void)
{
    HDR_HostEvent *ev=NULL;
    u8 *msg;

    HOST_EVENT_ALLOC(ev, SOC_EVT_ACK, sizeof(u32));
    msg = HOST_EVENT_DATA_PTR(ev);
    *(u32 *)msg = 1;
    HOST_EVENT_SET_LEN(ev, sizeof(u32));
    HOST_EVENT_SEND(ev);
    return;
}
#endif

static void _exec_host_cmd(struct cfg_host_cmd **p_host_cmd)
{
    s32 idx, max_idx = sizeof(HostCmdMap)/sizeof(HostCmdMap_S );
    for (idx = 0; idx < max_idx; idx++)
    {
        if (HostCmdMap[idx].event_id == (*p_host_cmd)->h_cmd)
        {
            HostCmdMap[idx].event_handler((const struct cfg_host_cmd **)p_host_cmd);
            return;
        }
    }
    CMD_ERROR("Host command %u is not found.\n", (*p_host_cmd)->h_cmd);
} // end of - _exec_host_cmd -


static void CmdEngine_SetConfig( const struct cfg_host_cmd **_HostCmd )
{
    struct cfg_host_cmd *HostCmd=(struct cfg_host_cmd *)(*_HostCmd);
    struct cfg_host_cmd *SubHostCmd;
    const u8 *RawDat=HostCmd->dat8;
    u16 len=0, max_len;

    CMD_DUMP(HostCmd, "\n");
    _CmdEnginBatchMode = true;
    max_len = HostCmd->len - HOST_CMD_HDR_LEN;
    while(len < max_len) {
        SubHostCmd = (struct cfg_host_cmd *)RawDat;
        len += SubHostCmd->len;
        RawDat += SubHostCmd->len;
        if (len > max_len) {
            return;
        }
        if (SubHostCmd->h_cmd < SSV6XXX_HOST_SOC_CMD_MAXID) {
            //TBL_HostCmd[SubHostCmd->h_cmd](SubHostCmd);
            _exec_host_cmd(&SubHostCmd);
        }
    }
    _CmdEnginBatchMode = false;
}


static void CmdEngine_GetConfig( const struct cfg_host_cmd **_HostCmd )
{

}

static void CmdEngine_GetSocStatus( const struct cfg_host_cmd **_HostCmd )
{
    HDR_HostEvent   *ev=NULL;
    u8              *msg=NULL;
    struct ST_SOC_STATUS{
        u8 u8SocState;
        u32 u32SocAction;
    } *ps1=NULL;

    HOST_EVENT_ALLOC_0(ev, SOC_EVT_GET_SOC_STATUS , sizeof(struct ST_SOC_STATUS));
    msg = HOST_EVENT_DATA_PTR(ev);
    ps1 = (struct ST_SOC_STATUS *)msg;

    ps1->u8SocState=querySTAState();
    ps1->u32SocAction=querySTAAction();
    HOST_EVENT_SET_LEN(ev, sizeof(struct ST_SOC_STATUS));
    HOST_EVENT_SEND(ev);
}

/**
* CmdEngoine_HostEventAlloc() - Allocate both a packet buffer and a message
*                                                   event and then link them together.
*
*/
HDR_HostEvent *HostEventAlloc(ssv6xxx_soc_event hEvtID, u32 size)
{
    HDR_HostEvent *chevt;

    chevt = (HDR_HostEvent *)PBUF_MAlloc(size+sizeof(HDR_HostEvent) ,SOC_PBUF);
    if (chevt == NULL) {
        return NULL;
    }
    chevt->c_type  = HOST_EVENT;
    chevt->h_event = hEvtID;
    chevt->len     = size+sizeof(HDR_HostEvent);
    return chevt;
}

HDR_HostEvent *HostEventReAlloc(void **_pp, ssv6xxx_soc_event hEvtID, u32 size)
{
    HDR_HostEvent *chevt;

    chevt = (HDR_HostEvent *)PBUF_MReAlloc(_pp, size+sizeof(HDR_HostEvent));
    if (chevt == NULL)
    {
        return NULL;
    }

    chevt->c_type   = HOST_EVENT;
    chevt->h_event  = hEvtID;
    chevt->len     = size+sizeof(HDR_HostEvent);
    return chevt;
}


void CmdEngine_handle(MsgEvent *MsgEv)
{
    struct cfg_host_cmd *HostCmd;
    HostCmd = (struct cfg_host_cmd *)MsgEv->MsgData;
#if (CONFIG_SIM_PLATFORM == 1 || BEACON_DBG == 1)
    if(MsgEv->MsgType == MEVT_BCN_CMD)
    {
        switch (MsgEv->MsgData)
        {
            case BCN_GEN_BCN:
                cmd_eng_bcn_gen_bcn_to_mactx_SIM();
                cmd_eng_bcn_create_timer_SIM();
                break;
            case BCN_TEST_RELEASE_BCN:
#if (BEACON_DBG == 1)
                cmd_eng_bcn_release_test();
#endif
                break;
        }
    }
#endif //(CONFIG_SIM_PLATFORM == 1 || BEACON_DBG == 1)

    switch (HostCmd->c_type)
    {
        case HOST_CMD:
            if (HostCmd->h_cmd >= SSV6XXX_HOST_SOC_CMD_MAXID) {
                LOG_PRINTF("%s(): Unknown command type (0x%02x)!!\n",
                           __FUNCTION__, HostCmd->h_cmd);
                PBUF_MFree(HostCmd);
                break;
            }
            _exec_host_cmd(&HostCmd);
            //If HostCmd = NULL, it measn the pbuf is reused
            if(HostCmd!=NULL)
                PBUF_MFree(HostCmd);
            break;
        case HOST_EVENT:
            if (HostCmd->h_cmd >= SOC_EVT_MAXID) {
                LOG_PRINTF("%s(): Unknown event type (0x%02x)!!\n",
                          __FUNCTION__, HostCmd->h_cmd);
                PBUF_MFree(HostCmd);
                break;
            }
            HOST_EVENT_SEND((u32)HostCmd);
            break;
        default: PBUF_MFree(HostCmd);
    }

}


s32 cmd_engine_init()
{
    OS_MutexInit(&CmdEngineMutex);
    memset(&g_bcn_info, 0, sizeof(struct cmd_bcn_info));
    LOG_MODULE_TURN_ON(LOG_MODULE_CMD);
	sg_soc_evt_seq_no=1000;
#if (CONFIG_SIM_PLATFORM == 1)
    //create this to simulate beacon timer.
    cmd_eng_bcn_timer_init_SIM();
#endif
#if (AUTO_BEACON == 1)
    if( OS_TimerCreate(&bcn_timer, 500, (u8)TRUE, NULL, (OsTimerHandler)check_bcn) == OS_FAILED)
		return OS_FAILED;
#endif

    return 0;
}

