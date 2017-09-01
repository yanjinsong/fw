#include <config.h>
#include <regs.h>

#include "drv_edca.h"


const u32 edca_reg_addr[]=
{
	TXQ0_MT_Q_REG_CSR_BASE,
	TXQ1_MT_Q_REG_CSR_BASE,
	TXQ2_MT_Q_REG_CSR_BASE,		
	TXQ3_MT_Q_REG_CSR_BASE,
	TXQ4_MT_Q_REG_CSR_BASE,
	//TXQ5_MT_Q_REG_CSR_BASE,
};



void drv_edca_get_wmm_param(edca_tx_queue_type que_type, wmm_param *pWmmParam)
{
		pWmmParam->a.all = REG32(edca_reg_addr[que_type]+0x00000004) ;

#if (EDCA_DBG == 1)		
		pWmmParam->backoffvalue = REG32(edca_reg_addr[que_type]+0x00000008);	
#endif
		
}


void drv_edca_set_wmm_param(edca_tx_queue_type que_type, wmm_param *pWmmParam)
{
	
	REG32(edca_reg_addr[que_type]+0x00000004) = pWmmParam->a.all;

#if (EDCA_DBG == 1)	
	if(pWmmParam->enable_backoffvalue)
		REG32(edca_reg_addr[que_type]+0x00000008) = pWmmParam->backoffvalue;
#endif		
}




void drv_edca_queue_halt(edca_tx_queue_type que_type, bool bEnable)
{
	if(EDCA_AC_ALL == que_type)
		SET_MTX_HALT_Q_MB(TRUE==bEnable?0x3F:0x00);
	else
	{
		REG32(ADR_MTX_MISC_EN) = ((bEnable) << (16+que_type)) | (REG32(ADR_MTX_MISC_EN)  & ((u32)~(1 << (16+que_type))));
	}
	
}



void drv_edca_enable_interrupt(enum edca_tx_int_type tx_int_type)
{
	u32 val = ENABLE_Q_EMPTY_INT;
		
	if(EDCA_TX_INT_BY_TXOP_RUN_OUT == tx_int_type)	
		val = ENABLE_Q_TXOP_RUNOUT_INT;
	else if(EDCA_TX_INT_OFF == tx_int_type)
		val = DISABLE_Q_INT;
	else
	{;}
	
	REG32(ADR_MTX_INT_EN)             = ((val) << 16) | (REG32(ADR_MTX_INT_EN)     & 0xff00ffff);
}








