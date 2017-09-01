#include <config.h>
#include <msgevt.h>
#include <hdr80211.h>
#include <pbuf.h>
#include <log.h>
#include "mlme.h"
#include "MLME_Task.h"
#include "MLME_Leave.h"
#include "sta_info.h"
#include "supplicant/drivers/driver_cabrio.h"
#include <mbox/drv_mbox.h>
#include <hwmac/drv_mac.h>
#include <edca/drv_edca.h>
#include <beacon/drv_beacon.h>
extern void MLME_ps_exit(void);
s32 MLME_Leave_Resp (void **_PktInfo, bool timeout, s16 reason)
{
    u32 size;
    HDR_HostEvent   *ev=NULL;
    struct resp_evt_result *LeaveResp=NULL;
    size = sizeof(struct resp_evt_result);

    if(gDevInfo->ps_state != PWR_WAKE){
        MLME_ps_exit();
    }

    HOST_EVENT_REALLOC_0(_PktInfo,ev,SOC_EVT_LEAVE_RESULT,size);

    LeaveResp = (struct resp_evt_result *)HOST_EVENT_DATA_PTR(ev);

    LeaveResp->u.leave.reason_code = reason;
    printf("Leave reason_code = %d\n",reason);
    // Send command response to host.
    HOST_EVENT_SET_LEN(ev,size);
    HOST_EVENT_SEND(ev);

    // Finish current work.
    _mlme_sta_disconnect(0);
    //SetStaAction(STA_ACTION_READY);


    return MLME_OK;
}// end of - mlme_leave_resp -


s32 MLME_Leave( struct cfg_host_cmd **_HostCmd)
{
    struct cfg_leave_request *LeaveReq=(struct cfg_leave_request *)(((struct cfg_host_cmd *)(*_HostCmd))->dat8);
    gDevInfo->MlmeReqParams=NULL;

    //Mlme_WorkJobReq *WorkJobReq;
    //size_t size;
    u16 reason=0;
    SetStaAction(STA_ACTION_LEAVING);
    /* Allocate buffer for storing WorkJobReq */
    //size = sizeof(Mlme_WorkJobReq);
    //WorkJobReq = (Mlme_WorkJobReq * )PBUF_MAlloc(size, SOC_PBUF);
    //ASSERT_RET(WorkJobReq, MLME_FAILED);


    //GET_BSSID(WorkJobReq->u.disassoc.bssidx);
    //memcpy(WorkJobReq->u.disassoc.bssidx,LeaveReq->bssid.addr,ETH_ALEN);
    //WorkJobReq->work_type = MLME_WORK_LEAVE;
    //WorkJobReq->cmd_seq_no = cmd_seq_no;
    //memcpy(WorkJobReq->u.deauth.bssidx,LeaveReq->bssid.addr,ETH_ALEN);
    //gDevInfo->MlmeReqParams=WorkJobReq;
    gDevInfo->State = STA_STATE_UNAUTH_UNASSOC;
    MLME_TRACE("Leaving with reason %d.\n", LeaveReq->reason);
    SetStaAction(STA_ACTION_READY);
    reason=LeaveReq->reason;
    mlme_send_deauth(reason,(struct ETHER_ADDR_st*)&gDevInfo->info.addr,(struct ETHER_ADDR_st*)&gDevInfo->connected_sta_ptr->addr.addr,(struct ETHER_ADDR_st*)&gDevInfo->connected_sta_ptr->addr.addr,TRUE);
    MLME_Leave_Resp((void **)_HostCmd, 1,0);
    MLME_Cmd_Done(&gDevInfo->MlmeReqParams);

    return MLME_OK;
}

