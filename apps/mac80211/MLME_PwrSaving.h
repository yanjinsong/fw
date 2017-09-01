#ifndef _MLME_PWRSAVING_H_
#define _MLME_PWRSAVING_H_

void MLME_PwrSaving(void* pdata);
u32 MLME_ps_bcn_handling(PKT_RxInfo *PktInfo, struct ieee802_11_elems *elems, u32 bcn_cnt, u32 diff_t);
void MLME_ps_exit(void);
u32 MLME_ps_wow(PKT_Info *pPktInfo);

#endif
