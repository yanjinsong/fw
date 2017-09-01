#ifndef _MLME_SCAN_H_
#define _MLME_SCAN_H_


s32 MLME_Scan( struct cfg_host_cmd **_HostCmd);
s32 Scan_action(u8 work_type, void *data, u32 data_len);
s32 Scan_process_result(void *data, u32 data_len,struct ieee802_11_elems *elems, u32 cmd_seq_no);

#endif /* _MLME_SCAN_H_ */

