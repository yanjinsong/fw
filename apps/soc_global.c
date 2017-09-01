#include <config.h>
#include <soc_global.h>


/*  Global Variables Declaration: */
ETHER_ADDR WILDCARD_ADDR = { {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF} };


u32 gTxFlowDataReason;
u32 gRxFlowDataReason;
u16 gTxFlowMgmtReason;
u16 gRxFlowMgmtReason;
u16 gRxFlowCtrlReason;


u32 g_free_msgevt_cnt;


#if 0
/**
* Global Variables declarations for System Table:
*
* @ g_sec_pbuf_addr: the start address of key tables for security
* @ g_phy_pbuf_addr: the start address of phy info table
*/
u32 g_sec_pbuf_addr;
u32 g_phy_pbuf_addr;
#endif


struct soc_table_st *g_soc_table;
struct soc_mib_st	g_soc_mib;

struct associate_req_rate assoc_req_rate;
/*
This variable is used to count the number of error ack_policy.
The detal is in RB-240
*/
u32 gAckPolicyErrorCount=0;

u32 gSoftTaskCount=0;
u32 gMLMETaskCount=0;

