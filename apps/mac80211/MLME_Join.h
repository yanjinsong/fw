#ifndef _MLME_JOIN_H_
#define _MLME_JOIN_H_


enum JOIN_RESULT_REASON_CODE
{
    EN_JOIN_RESULT_SUCCESS=0,
    EN_JOIN_RESULT_FAIL=1,
    EN_JOIN_RESULT_WRONG_PASSWORD=2
    
};

#ifdef _INTERNAL_MCU_SUPPLICANT_SUPPORT_
#define JOIN_FAIL_HANDLE(REASON) \
    do{ \
        Join_process_result((REASON)); \
        if(supplicant_bypass)   \
		    ;   \
	    else    \
	    {   \
    	    wpa_supplicant_event(gDevInfo->pSupplicant, EVENT_DEAUTH, (union wpa_event_data *)NULL); \
	    } \
    }while(0)
#else
#define JOIN_FAIL_HANDLE(REASON) \
    do{ \
        Join_process_result((REASON)); \
    }while(0)
#endif

s32 MLME_Join( struct cfg_host_cmd **_HostCmd);
s32 Join_process_result(s32 status_code);
s32 Join_action(u8 work_type, void *data, u32 data_len);

#endif /* _MLME_JOIN_H_ */

