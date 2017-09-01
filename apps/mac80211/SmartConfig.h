#ifndef _SMARTCONFIG_H_
#define _SMARTCONFIG_H_


s32 MLME_SConfig( struct cfg_host_cmd **_HostCmd);
s32 SConfig_Rx_Handler(PKT_RxInfo *pPktInfo);
s32 SConfig_action(u8 work_type, void *data, u32 data_len);
#endif /* _SMARTCONFIG_H_ */

