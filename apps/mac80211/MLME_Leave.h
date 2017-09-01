#ifndef _MLME_LEAVE_H_
#define _MLME_LEAVE_H_


s32 MLME_Leave( struct cfg_host_cmd **_HostCmd);
s32 MLME_Leave_Resp (void **_PktInfo, bool timeout, s16 reason);
#endif /* _MLME_LEAVE_H_ */

