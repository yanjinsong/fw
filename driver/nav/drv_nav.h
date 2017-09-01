#ifndef _DRV_NAV_H_
#define _DRV_NAV_H_

#include <regs.h>





typedef enum
{
	NAV_PHY_CCK 	= 0x00,
	NAV_PHY_OFDM 	= 0x01,
	NAV_PHY_HT_MF 	= 0x02,
	NAV_PHY_HT_GF 	= 0x03,
} NAV_PHY_MODE;


typedef enum
{
	NAV_PREAMBLE_LONG = 0x00, 
	NAV_PREAMBLE_SHORT = 0x01,	
} NAV_PREAMBLE;


typedef enum
{
	NAV_GI_LONG = 0x00, 
	NAV_GI_SHORT = 0x01,	
} NAV_GI;


typedef enum
{
	NAV_DATA_RATE_1Mbps 	= 2, 
	NAV_DATA_RATE_2Mbps 	= 4,
	NAV_DATA_RATE_5_5Mbps 	= 11, 
	NAV_DATA_RATE_6Mbps 	= 12,
	NAV_DATA_RATE_9Mbps 	= 18, 
	NAV_DATA_RATE_11Mbps 	= 22,	
	NAV_DATA_RATE_12Mbps 	= 24, 
	NAV_DATA_RATE_18Mbps 	= 36,
	NAV_DATA_RATE_22Mbps 	= 44, 
	NAV_DATA_RATE_24Mbps 	= 48,
	NAV_DATA_RATE_33Mbps 	= 66,
	NAV_DATA_RATE_36Mbps 	= 72,
	NAV_DATA_RATE_48Mbps 	= 96,
	NAV_DATA_RATE_54Mbps 	= 108,
} NAV_DATA_RATE;

typedef enum
{
	NAV_MCS_IDX00 = 0,
	NAV_MCS_IDX01,
	NAV_MCS_IDX02,
	NAV_MCS_IDX03,
	NAV_MCS_IDX04,
	NAV_MCS_IDX05,
	NAV_MCS_IDX06,
	NAV_MCS_IDX07,
} NAV_MCS;


//CCK->Preamble 
//HT->GI
u16 drv_get_duration(NAV_PHY_MODE phy_mode, NAV_PREAMBLE preamble, NAV_GI gi, NAV_DATA_RATE data_rate ,NAV_MCS mcs, u32 framelen);


#endif /* _DRV_NAV_H_ */

