#include <config.h>

#include "drv_nav.h"



//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#define SET_NAV_PARAM(_PHY, _SHORT_PREAMBLE, _SHORT_GI, _DATA_RATE, _MCS, _FRAME_LEN) 	\
    do { \
        REG_NAV_DATA = ((_PHY << PHY_MODE_SFT)&PHY_MODE_MSK                 | \
                        (_SHORT_PREAMBLE << SHRT_PREAM_SFT)&SHRT_PREAM_MSK  | \
                        (_SHORT_GI << SHRT_GI_SFT)&SHRT_GI_MSK              | \
                        (_DATA_RATE << DATA_RATE_SFT)&DATA_RATE_MSK         | \
                        (_MCS << MCS_SFT)&MCS_MSK                           | \
                        (_FRAME_LEN << FRAME_LEN_SFT)&FRAME_LEN_MSK); \
    } while (0)

//CCK->Preamble 
//HT->GI
u16 drv_get_duration(NAV_PHY_MODE phy_mode, NAV_PREAMBLE preamble, NAV_GI gi, NAV_DATA_RATE data_rate ,NAV_MCS mcs, u32 framelen)
{
	//SET_NAV_PARAM(phy_mode,preamble,gi,data_rate,mcs,framelen);
	SET_PHY_MODE(phy_mode);
	SET_SHRT_PREAM(preamble);
	SET_SHRT_GI(gi);
	SET_DATA_RATE(data_rate);
	SET_MCS(mcs);
	SET_FRAME_LEN(framelen);

	return GET_DURATION;
}












