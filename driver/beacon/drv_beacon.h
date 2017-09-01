#ifndef _DRV_BEACON_H_
#define _DRV_BEACON_H_

#include <regs.h>


#define DRV_BCN_PREBCN_TIME 10000//us


//CCK->Preamble
//HT->GI



typedef enum{
    EN_STA_BCN_RECEIVE_ALL,
    EN_STA_BCN_RECEIVE_SPECIFIC, //Only receive the bcns from the specific ap that is connected with STA
    EN_STA_BCN_RECEIVE_NONE,
    EN_STA_BCN_RECEIVE_INVALID,
    EN_STA_BCN_RECEIVE_MAX=EN_STA_BCN_RECEIVE_INVALID
}EN_STA_BCN_RECEIVE;



//************IMPORTANT ************
//After disable beacon. need to free beacon which store in hw reg.
//************IMPORTANT ************
void drv_bcn_enable(bool enable, u8 dtim, u16 bcn_itv);


void drv_bcn_set(void *pTxInfo, u16 dtim_offset);


void drv_bcn_release(void);

void drv_bcn_init(void);



#endif /* _DRV_NAV_H_ */

