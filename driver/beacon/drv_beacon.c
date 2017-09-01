#include <config.h>

#include "drv_beacon.h"
#include <pmu/drv_pmu.h>
#include <pbuf.h>
#include <types.h>
#include <regs.h>
#include <log.h>
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// enum{
//
// 	DRV_BCN_BCN0_SET,
// 	DRV_BCN_BCN1_SET,
// }


enum drv_bcn_type{
	DRV_BCN_BCN0,
	DRV_BCN_BCN1,
};




struct drv_bcn_info {
    u32 bcn_info;


};

struct drv_bcn_info g_drv_bcn_info;

void drv_bcn_release(void);


#if 0
void drv_bcn_enable(bool enable, u8 dtim_cnt, u16 bcn_itv)
{


	if(enable)
	{
		SET_MTX_BCN_PERIOD(bcn_itv);
		SET_MTX_DTIM_NUM(dtim_cnt);//Maxium DTIM counter number, not DTIM
		SET_MTX_INT_DTIM_NUM(1);
	}


	SET_MTX_BCN_TIMER_EN(enable);//-------------------------------------------------->Beacon timer
	SET_MTX_TSF_TIMER_EN(enable);
	SET_MTX_EN_INT_Q4_Q_EMPTY(enable);

	//SET_MTX_TIME_STAMP_AUTO_FILL(enable);
	//SET_MTX_HALT_MNG_UNTIL_DTIM(enable);

	//Need to do this after disable bcn timer
	if(!enable)
		drv_bcn_release();



}


#endif

// 00: bcn cfg0 / cfg1 not be config 01: cfg0 is valid 10: cfg1 is valid 11: error (valid cfg be write)
inline enum drv_bcn_type drv_bcn_reg_available()
{

	if(GET_MTX_BCN_CFG_VLD&0x2 || GET_MTX_BCN_CFG_VLD == 0x0)//bcn 0 is availabke to use.
		return DRV_BCN_BCN0;
	else if(GET_MTX_BCN_CFG_VLD&0x1)//bcn 1 is availabke to use.
		return DRV_BCN_BCN1;
	else
		ASSERT(FALSE);// 11 error happened need check with ASIC.


	return DRV_BCN_BCN0;
}


#if 0
//need to stop beacon firstly.
void drv_bcn_release(void)
{
	void *id;
	for(;GET_MTX_AUTO_BCN_ONGOING==1;);//loop until beacon is not in used


	if(IS_BIT_SET(g_drv_bcn_info.bcn_info, DRV_BCN_BCN0))
	{
		//Get packet id and release it.
		id = (void *)PBUF_MapIDtoPkt((((REG32(ADR_MTX_BCN_CFG0)) & 0x0000003f ) >> 0));
		LOG_PRINTF("Release BCN:%08x from CFG0\n");
		PBUF_MFree(id);

		CLEAR_BIT(g_drv_bcn_info.bcn_info, DRV_BCN_BCN0);

	}


	if(IS_BIT_SET(g_drv_bcn_info.bcn_info, DRV_BCN_BCN1))
	{
		//Get packet id and release it.
		id = (void *)PBUF_MapIDtoPkt((((REG32(ADR_MTX_BCN_CFG1)) & 0x0000003f ) >> 0));
		LOG_PRINTF("Release BCN:%08x from CFG1\n");
		PBUF_MFree(id);

		CLEAR_BIT(g_drv_bcn_info.bcn_info, DRV_BCN_BCN1);
	}


}

#endif


#if (CONFIG_SIM_PLATFORM == 1)
extern void  sim_global_bcn_lock();
extern void  sim_global_bcn_unlock();
#endif//#if (CONFIG_SIM_PLATFORM == 1)










#define WPA_GET_BE32(a) ((((u32) (a)[0]) << 24) | (((u32) (a)[1]) << 16) | \
			 (((u32) (a)[2]) << 8) | ((u32) (a)[3]))


void inline beacon_free(PKT_TxInfo  *bcn)
{

#if(BEACON_DBG==1)
	u8 *raw= (u8*)bcn+bcn->hdr_offset+bcn->len;
	raw -=4;
	LOG_PRINTF("R-BCN:%08x. content:%08x\n",bcn, WPA_GET_BE32(raw));
#endif
	//Get packet id and release it.
	PBUF_MFree(bcn);
}






void drv_bcn_set(void *pTxInfo, u16 dtim_offset)
{
	u32 reg_adr = ADR_MTX_BCN_CFG0;
	enum drv_bcn_type avl_bcn_type = DRV_BCN_BCN1;
	u8 id = PBUF_MapPkttoID((void*)pTxInfo);



	SET_MTX_BCN_PKTID_CH_LOCK(1);
#if (CONFIG_SIM_PLATFORM == 1)
	sim_global_bcn_lock();
	//Always use cfg0
	SET_MTX_BCN_CFG_VLD(1);
#else//#if (CONFIG_SIM_PLATFORM == 1)

	avl_bcn_type = drv_bcn_reg_available();
#endif//#if (CONFIG_SIM_PLATFORM == 1)




	if(avl_bcn_type == DRV_BCN_BCN1)
		reg_adr = ADR_MTX_BCN_CFG1;



	//Check if bcn store in this reg
	if(IS_BIT_SET(g_drv_bcn_info.bcn_info, avl_bcn_type))
	{
		void *pkt_addr = (void*)PBUF_MapIDtoPkt((((REG32(reg_adr)) & 0x0000003f ) >> 0));

#if(BEACON_DBG==1)
		LOG_PRINTF("DRV CFG%d S-BCN:%08x. ", pkt_addr, pTxInfo, avl_bcn_type);
#endif

		//Get packet id and release it.
		beacon_free(pkt_addr);
	}


	//LOG_PRINTF("set beacon into register:%08x bcn id:%d\n", reg_adr, id);

	//Set PKT ID
	(REG32(reg_adr))  = (((id) << 0)  | ((REG32(reg_adr)) & 0xffffffc0));
	//Set DTIM offset
	(REG32(reg_adr)) = (((dtim_offset) << 16) | ((REG32(reg_adr)) & 0x0000ffff));

	//Indicate reg is stored packet buf.
	SET_BIT(g_drv_bcn_info.bcn_info, avl_bcn_type);




#if (CONFIG_SIM_PLATFORM == 1)
	 sim_global_bcn_unlock();
#endif//#if (CONFIG_SIM_PLATFORM == 1)




	SET_MTX_BCN_PKTID_CH_LOCK(0);
}





void drv_bcn_init(void)
{
	memset(&g_drv_bcn_info, 0 , sizeof(struct drv_bcn_info));
}
