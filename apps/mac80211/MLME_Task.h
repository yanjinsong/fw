#ifndef _MLME_TASK_H_
#define _MLME_TASK_H_

/* Define constants for MsgSubType field of MLME_Timer */
#define MLME_WORK_ACTIVE_SCANING_TIMEOUT        1
#define MLME_WORK_AUTH_TIMEOUT                  2
#define MLME_WORK_ASSOC_TIMEOUT					3
#define MLME_WORK_4way_TIMEOUT                  4
#define MLME_WORK_PASSIVE_SCANING_TIMEOUT		5
#define MLME_WORK_LEAVE_TIMEOUT                 6
#ifdef AIRKISS_ENABLE
#define MLME_WORK_SMART_CONFIG_SCAN_TIMEOUT     7
#define MLME_WORK_SMART_CONFIG_PROBE_TIMEOUT    8
#define MLME_WORK_SMART_CONFIG_LOCK_TIMEOUT     9
#endif
#define MLME_WORK_SUBTYPE_TEST                  99

#define IS_STA_CONNECTED()((gDevInfo->State==STA_STATE_AUTHED_ASSOCED)||(gDevInfo->State==STA_STATE_ASSOCED_4way))

extern u32 mlme_cmd_seq_no;
extern bool mlme_cmd_running;
void MLME_Cmd_Done(void **ppMlmeReqParams);
s32 MLME_add_background_task (void (*task_func)(void *), void *task_data);
void MLME_Task( void *args );

#endif /* _MLME_TASK_H_ */

