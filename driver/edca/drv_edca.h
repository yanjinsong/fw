#ifndef _DRV_EDCA_H_
#define _DRV_EDCA_H_

#include <regs.h>

#if (CONFIG_SIM_PLATFORM==1)
//#include <asic/hw_regs.h>
#endif



enum edca_txop_control
{
	EDCA_TXOP_CONTROL_BY_HW 	= 0x00, //HW Schedule TXOP
	EDCA_TXOP_CONTROL_BY_FW 	= 0x01,	//Could set more_txop flag in TXInfo to do more detail control	
};





typedef enum _edca_tx_queue_type {
	EDCA_AC_BK	= 0	,
	EDCA_AC_BE		,
	EDCA_AC_VI		,
	EDCA_AC_VO 		,						
	EDCA_AC_MAX		,
	EDCA_AC_ALL		= EDCA_AC_MAX,
	EDCA_AC_BC      = EDCA_AC_MAX,
	EDCA_AC_BCN     ,	
	EDCA_QUE_MAX	,
	
}edca_tx_queue_type;



typedef struct wmm_param_st
{
    union {
        struct {
        	u32				aifsn:4;	//0~15	 
        	u32				acm:1;
        	u32				resv:3;
        	u32 			cwmin:4;   	//(1, 3, 7, 15, 31, 63, 127, 255, 511, 1023) millionsecond
        	u32				cwmax:4;	//(1, 3, 7, 15, 31, 63, 127, 255, 511, 1023)	
        	u32				txop:16;	//65536  milliseconds	
        };
        u32                 all;
    }a;

	u32                backoffvalue:16;
	u32				   enable_backoffvalue:1;
	u32				   RESV:15;

	
}wmm_param;


enum edca_tx_int_type {
	EDCA_TX_INT_OFF				,
	EDCA_TX_INT_BY_Q_EMPTY		,
	EDCA_TX_INT_BY_TXOP_RUN_OUT	,
};


#define ENABLE_Q_EMPTY_INT					0x55			// eable int from BK to VO
#define ENABLE_Q_TXOP_RUNOUT_INT			0xAA			// eable int from BK to VO
#define DISABLE_Q_INT						0x00
#define ENABLE_INT_MASK						0xff00ffff		



#define TX_QUEUE_ENABLE() \
    do{ \
        drv_edca_queue_halt(EDCA_AC_BK, FALSE); \
        drv_edca_queue_halt(EDCA_AC_BE, FALSE);  \
        drv_edca_queue_halt(EDCA_AC_VI, FALSE);  \
        drv_edca_queue_halt(EDCA_AC_VO, FALSE);  \
    }while(0)

#define TX_QUEUE_DISABLE() \
    do{ \
        drv_edca_queue_halt(EDCA_AC_BK, TRUE); \
        drv_edca_queue_halt(EDCA_AC_BE, TRUE);  \
        drv_edca_queue_halt(EDCA_AC_VI, TRUE);  \
        drv_edca_queue_halt(EDCA_AC_VO, TRUE);  \
    }while(0)





void drv_edca_set_wmm_param(edca_tx_queue_type que_type, wmm_param *pWmmParam);

void drv_edca_queue_halt(edca_tx_queue_type que_type, bool bEnable);

void drv_edca_enable_interrupt(enum edca_tx_int_type tx_int_type);
//void drv_edca_enable_beacon_interrupt(bool bEnable);
//void drv_edca_txop_control(enum edca_txop_control txop_control);

#define drv_edca_txop_sub_frm_time(_TF) SET_MTX_QOS_TXOP_SUB_FRM_TIME(_TF);
#define drv_edca_beacon_id SET_MTX_BCN_MBGET_PKT_ID_INT(_pid);


//#define drv_edca_txop_control_by_fw



#endif /* _DRV_EDCA_H_ */

