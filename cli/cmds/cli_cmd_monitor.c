#include <config.h>
#include <log.h>
#include <cli/cli.h>
#include <soc_global.h>
#include "cli_cmd.h"
#include "cli_cmd_monitor.h"

static OsTimer monitor_timer;


static bool IsMonitorTimerRunning=FALSE;
static u32 repeat_counter=0;
static bool never_stop=FALSE;
void mointerHandler(void)
{

    if((never_stop==FALSE)&&(0==repeat_counter)){
        mac_monitor_stop();
        return;
    }

    repeat_counter--;


    printf("*******************\r\n");
    printf("Dump queue stats\r\n");
    printf("*******************\r\n");
    ssv6xxx_queue_status();
    printf("\r\n");
    printf("\r\n");
    printf("\r\n");
    printf("*******************\r\n");
    printf("Dump secure setting\r\n");
    printf("*******************\r\n");
    ssv6xxx_queue_status();
    printf("\r\n");
    printf("\r\n");
    printf("\r\n");
    return;

}

s32 mac_monitor_start (u32 repeat, u32 period)
{
    if(TRUE==IsMonitorTimerRunning){
        printf("Please stop the monitor first\r\n");
        return -1;
    }

    if(NULL==monitor_timer){
	    if( OS_TimerCreate(&monitor_timer, period, (u8)TRUE, NULL, (OsTimerHandler)mointerHandler) == OS_FAILED)
		    return -1;
    }else{
        OS_TimerSet(monitor_timer,period,TRUE,NULL);
    }
    repeat_counter=repeat;
    never_stop=(0==repeat)?TRUE:FALSE;
    IsMonitorTimerRunning=TRUE;

	OS_TimerStart(monitor_timer);

    return 0;
}


void mac_monitor_stop()
{
    OS_TimerStop(monitor_timer);
    IsMonitorTimerRunning=FALSE;
}
