#include <config.h>
#include <msgevt.h>
#include <pbuf.h>

#include <hwmac/drv_mac.h>
#include "mlme.h"
#include "MLME_Task.h"
#include "sta_info.h"
#include "../../driver/hw_regs_api.h"

#define STA_MONITOR_TIMER_TIME 500 //500ms

DevInfo	        *gDevInfo=NULL;
SSV6XXX_STA     *g6200StaInfo=NULL;
OsMutex         staInfoMutex;

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void staInfo_mutex_init()
{
    OS_MutexInit(&staInfoMutex);
}


void staInfo_mutex_lock()
{
    if(!gOsFromISR) OS_MutexLock(staInfoMutex);
}

void staInfo_mutex_unlock()
{
    if(!gOsFromISR) OS_MutexUnLock(staInfoMutex);
}


void SetStaAction(s32 flag)
{
    staInfo_mutex_lock();
    gDevInfo->Action=(u32)flag;//STA_INFOFLAG_RUNNING
	staInfo_mutex_unlock();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void StaMonitorTimerHandler( void *args )
{
    u8 res=OS_FAILED;
    SSV6XXX_STA		*sta=gDevInfo->connected_sta_ptr;

    if(sta!=NULL)
    {
        /*
            Monitor if the AP exists or not ...
            When STA join the AP sucessfully, we set ap_heartbeat_cnt and last_ap_heartbeat_cnt to zero,
            we don't need to worry about these two variables has the same values "0", because we set the STAMonitorTimer 15s,
            it's impossible to get no beacons during this 15s.
        */
        if(sta->ap_heartbeat_cnt==sta->last_ap_heartbeat_cnt)
        {
            printf("be\n");
            res = msg_evt_post_data1(MBOX_MLME, MEVT_DETECT_AP, 0,0,0,0);

            if(res != OS_SUCCESS){
                printf("post MEVT_DETECT_AP faiL. (msg queue len = %d) \n",OS_MsgQWaitingSize(MBOX_MLME));
            }
        }
        else
        {
            sta->last_ap_heartbeat_cnt = sta->ap_heartbeat_cnt;
            sta->poke_ap=0;        
        }
    }
}

//u32 LastTickTime=0;
int StartStaMonitorTimer(void)
{
    s32 res=0;
    SSV6XXX_STA		*sta=gDevInfo->connected_sta_ptr;

    if(sta==NULL){
        printf("No available sta\n");
        return -1;
    }
    sta->ap_heartbeat_cnt=0;
    sta->last_ap_heartbeat_cnt=0;
    sta->poke_ap=0;

    if(gDevInfo->STAMonitorTimer==0){
        printf("Create StaMonitorTimer ...\n");
        res = OS_TimerCreate(&gDevInfo->STAMonitorTimer, STA_MONITOR_TIMER_TIME,TRUE, NULL, StaMonitorTimerHandler);
        OS_TimerSet(gDevInfo->STAMonitorTimer, STA_MONITOR_TIMER_TIME, TRUE, NULL);
        printf("Start StaMonitorTimer ...\n");
        OS_TimerStart(gDevInfo->STAMonitorTimer);

    }else{
        printf("Re-start StaMonitorTimer ...\n");
        OS_TimerStart(gDevInfo->STAMonitorTimer);
    }
    return res;
}

int StopStaMonitorTimer(void)
{
    printf("StopStaMonitorTimer ... \n");
    if(gDevInfo->STAMonitorTimer!=0){
        OS_TimerStop(gDevInfo->STAMonitorTimer);
    }

    return 0;
}




SSV6XXX_STA *STAInfo_FindSTAByAddr( const ETHER_ADDR *mac )
{
    SSV6XXX_STA *StaInfo=g6200StaInfo;
    s32 index;

    for(index=0; index<WLAN_MAX_STA; index++)
    {
		StaInfo=&g6200StaInfo[index];


        if (IS_EQUAL_MACADDR(mac, &StaInfo->addr))
		{
            return StaInfo;
		}
    }
    return (SSV6XXX_STA *)NULL;
}



void STAInfo_Init( void )
{

    u32 size;

    /*  Allocate Memory for 6200 STA data structure. */

    size = sizeof(SSV6XXX_STA)*WLAN_MAX_STA;
    g6200StaInfo = (SSV6XXX_STA *)OS_MemAlloc(size);
    ASSERT(g6200StaInfo != NULL);
	memset(g6200StaInfo, 0, size);

    /*  Allocate Memory for gDevInfo data structure. */
    size = sizeof(DevInfo);
    gDevInfo = (DevInfo *)OS_MemAlloc(size);
    ASSERT(gDevInfo != NULL);
    memset(gDevInfo, 0, size);

    /* Assign initial value of flag and timer */
    //gDevInfo->InfoFlags |= INFO_FLAG_STAINFO;
	gDevInfo->State=STA_STATE_UNAUTH_UNASSOC;
	gDevInfo->Action=STA_ACTION_INIT;
    MLME_INIT_TIMER(&gDevInfo->MTimer);

    drv_mac_get_sta_mac(gDevInfo->info.addr.addr);
    staInfo_mutex_init();

}



static inline SSV6XXX_STA *stainfo_get_unused_sta()
{
	SSV6XXX_STA *sta;
	int i;

	/* Create Timer to each STA */
    for(i=0; i<WLAN_MAX_STA; i++)
    {
    	sta = &g6200StaInfo[i];
       	if(!sta->aid)
			break;
    }
	return sta;

}




static inline s8 stainfo_erase_sta(const ETHER_ADDR *mac)
{

	//memset() sta structure

	return 0;
}





//-----------------------------------------------------------------------------
//Interface
SSV6XXX_STA *stainfo_add_sta(void)
{

	SSV6XXX_STA * sta;

	//get empty space from memory
	sta = stainfo_get_unused_sta();


	//add info into hw regster
	//drv_mac_set_wsid_table


	return sta;
}


s8 stainfo_del_sta(const ETHER_ADDR *mac)
{


	//delete from gStaInfo
	stainfo_erase_sta(mac);


	//add info into hw regster
	//drv_mac_set_wsid_table



	//Del sec key from sec table
	//sram_del_key


	return 0;
}





s8 stainfo_reset_sta()
{



	return 0;
}




s8 stainfo_set_sec_key()
{

	//sram_set_key

	return 0;
}


bool stainfo_is_ampdu_ready(u8 wsid)
{
    SSV6XXX_STA	*pSta=NULL;
    pSta=&g6200StaInfo[wsid];
    if(pSta->ampdu_tx==TRUE)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}



//-----------------------------------------------------------------------------







#ifdef __AP_DEBUG__
u16 get_sta_aid()
{
	return gDevInfo->aid;
}
#endif

u32 is_sta_timer (u32 addr)
{
    extern DevInfo *gDevInfo;
    if (addr == (u32)&gDevInfo->MTimer)
           return true;
	else
		   return false;
}

EN_STA_ACTION querySTAAction(void)
{
    EN_STA_ACTION val;
    staInfo_mutex_lock();
    val=gDevInfo->Action;
    staInfo_mutex_unlock();
    return val;
}

EN_STA_STATE querySTAState(void)
{
    EN_STA_STATE val;
    staInfo_mutex_lock();
    val=gDevInfo->State;
    staInfo_mutex_unlock();
    return val;
}

bool IsSTAActReady(void)
{
    EN_STA_ACTION val;
    bool retVal=TRUE;

    val=querySTAAction();

    if (val != STA_ACTION_READY)
    {
        retVal=FALSE;
    }

    return retVal;
}


