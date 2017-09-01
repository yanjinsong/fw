#include <config.h>
#include <msgevt.h>
#include <hdr80211.h>
#include <pbuf.h>
#include <hwmac/drv_mac.h>
#include <mbox/drv_mbox.h>
#include <timer/drv_timer.h>
#include <pmu/drv_pmu.h>
#include <dbg_timer/dbg_timer.h>

#include <log.h>

#include "mlme.h"
#include "MLME_Task.h"

#include "sta_info.h"
#include "util.h"
#include "cmd_engine.h"
#include "MLME_PwrSaving.h"
#include <lib/ssv_lib.h>

static OsTimer bcn_sync_timer=NULL;
static OsTimer host_wakeup_timer=NULL;

inline void set_wakeup_BB_gpio(u32 value)
{
    //SET_PAD20_SEL_I(8);
    //SET_PAD20_OE(1);
    //SET_PAD20_OD(value);
    if(value)
        REG32(ADR_PAD20) = 0x181;
    else
        REG32(ADR_PAD20) = 0x081;
        
    //printf("PAD20_OD=%d,%x\n",value,GET_PAD20_OD);
}

inline void ps_stop_all_tmr(void)
{
    hwtmr_stop(US_TIMER2);
    OS_TimerStop(bcn_sync_timer);

}
bool MLME_is_ps_doze(void)
{
    if((gDevInfo->ps_state == PWR_DOZE)||(gDevInfo->ps_state == PWR_BCN_SYNC)||
       (gDevInfo->ps_state == PWR_BCN_SYNC_TIM))
        return TRUE;

    return FALSE;
}

inline bool ps_ip_check(u32 v4ip)
{
    //printf("arphd.ip=%x,ipv4=%x\r\n",v4ip,gDevInfo->ipv4);
    if(v4ip==gDevInfo->wow_req.ipv4addr)
    {
        //printf("arphd ip=%x,ipv4=%x\r\n",v4ip,gDevInfo->wow_req.ipv4addr);
        return TRUE;
    }

    return FALSE;
}
extern u32 ieee80211_hdrlen(u16 fc);
u32 _ps_wow_exit(PKT_Info *pPktInfo)
{
    if((gDevInfo->ps_state == PWR_DOZE)||(gDevInfo->ps_state == PWR_BCN_SYNC)||
       (gDevInfo->ps_state == PWR_BCN_SYNC_TIM))
        MLME_ps_exit();
    
    if(ENG_MBOX_NEXT((u32)pPktInfo))
        return OS_SUCCESS;
    else
        return OS_FAILED;
}
u32 MLME_ps_wow(PKT_Info *pPktInfo)
{
    u16 eth_type;
    const u8 *dat;
    u32 result;
    HDR80211_Data * datahd = HDR80211_DATA(pPktInfo);
    u16 fc = GET_HDR80211_FC(pPktInfo);
    dat =(const u8 *)pPktInfo + pPktInfo->hdr_offset+ieee80211_hdrlen(fc)+6;
    eth_type = (dat[0]<<8) | dat[1];
    //printf("rx eth_type=%x\r\n",eth_type);

    if((IS_NULL_DATA(fc))||(IS_QOS_NULL_DATA(fc)))
    {
        printf("null seq=%d\r\n",datahd->seq_ctrl>>4);
        PBUF_MFree((void *)pPktInfo);
        return OS_SUCCESS;
    }
    
    switch(eth_type)
    {
        case ARP_ETHER_TYPPE:
        {
            struct etharp_hdr* arphd = (struct etharp_hdr*)((u8 *)pPktInfo + pPktInfo->hdr_offset + 24 + 8);
            u32 v4ip = (arphd->dipaddr[3]<<24)|(arphd->dipaddr[2]<<16)|(arphd->dipaddr[1]<<8)|(arphd->dipaddr[0]);
            if(ps_ip_check(v4ip))
            {
                printf("arp seq=%d\r\n",datahd->seq_ctrl>>4);
                result=_ps_wow_exit(pPktInfo);
            }
            else
            {
                //printf("arp\r\n");
                PBUF_MFree((void *)pPktInfo);
                result=OS_SUCCESS;
            }
            break;
        }
        case IP_ETHER_TYPPE:
        {
            //struct iphdr* iphd = (struct iphdr*)((u8 *)pPktInfo + pPktInfo->hdr_offset + 24 + 8);
            //printf("ipda=%d.%d.%d.%d\r\n",((u8*)&(iphd->daddr))[0],((u8*)&(iphd->daddr))[1],((u8*)&(iphd->daddr))[2],((u8*)&(iphd->daddr))[3]);
            if(IS_STA_RX_BMCAST(pPktInfo))
            {
                //printf("Bv4\r\n");
                PBUF_MFree((void *)pPktInfo);
                result=OS_SUCCESS;
            }
            else
            {
                printf("Uv4\r\n");
                result=_ps_wow_exit(pPktInfo);
            }
            break;
        }
        case EAPOL_ETHER_TYPPE:            
            printf("Epl\r\n");
            result=_ps_wow_exit(pPktInfo);
            break;
        //case 0x8899:
        default:
        {
            u8 i;
            bool hitEthTpy=FALSE;
            for(i=0;i<MAX_ETHTYPE_TRAP;i++)
            {
                if((gDevInfo->wow_req.ethtyp[i]!=0) && (eth_type == gDevInfo->wow_req.ethtyp[i]))
                {
                    hitEthTpy = TRUE;
                    break;
                }
            }
            if(hitEthTpy == TRUE)
            {
                printf("hit etyp=0x%x\r\n",eth_type);
                result=_ps_wow_exit(pPktInfo);
            }
            else
            {
                //printf("unw=0x%x\r\n",eth_type);
                PBUF_MFree((void *)pPktInfo);
                result=OS_SUCCESS;
            }
        }
    }
    return result;
}
extern s32 mlme_null_send(DevInfo *dev, bool ps_on, bool edca_direct);
u32 MLME_ps_send_evt2host(ssv6xxx_soc_event evtid)
{
    HDR_HostEvent *ev=NULL;
    HOST_EVENT_ALLOC_1(ev,evtid, 0);
    if(ev)
    {
        HOST_EVENT_SET_LEN(ev,0);
        HOST_EVENT_SEND(ev);
        printf("Snd evt=%d\r\n",evtid);
        return OS_SUCCESS;
    }
    else
    {
        printf("ps alloc evt fail\r\n");
        return OS_FAILED;
    }
}
extern void drv_phy_delay(s32 delay_ms);

void MLME_host_wakeup_timeout(void)
{
    if(gDevInfo->ps_state == PWR_WAIT_HOST_WAKEUP)
    {
        printf("host_wkp_t_out=%d\r\n",gDevInfo->ps_state);
        set_wakeup_BB_gpio(0);
        if(MLME_ps_send_evt2host(SOC_EVT_PS_WAKENED)==OS_SUCCESS)
        {        
            OS_TimerSet(host_wakeup_timer,500,FALSE,NULL);
        }
        else
        {
            OS_TimerSet(host_wakeup_timer,100,FALSE,NULL);
        }
        
        OS_TickDelay(1);
        set_wakeup_BB_gpio(1); //Rising up wakeup BB gpio
        //REG32(ADR_SYS_INT_FOR_HOST) = 0x00000000;
        //REG32(ADR_PAD20) = 0x00000001;
        OS_TimerStart(host_wakeup_timer);//restart timer
    }

}
void host_wakeup_timeout (void *data)
{
#if 0
    if(gDevInfo->ps_state == PWR_WAIT_HOST_WAKEUP)
    {
        printf("host_wkp_t_out=%d\r\n",gDevInfo->ps_state);
        set_wakeup_BB_gpio(0);
        drv_phy_delay(1);
        MLME_ps_send_evt2host(SOC_EVT_PS_WAKENED);
        set_wakeup_BB_gpio(1); //Rising up wakeup BB gpio
        
        REG32(ADR_SYS_INT_FOR_HOST) = 0x00000000;
        REG32(ADR_PAD20) = 0x00000001;
        OS_TimerStart(host_wakeup_timer);//restart timer
    }
#endif
    msg_evt_post_data1(MBOX_MLME, MEVT_POWER_SAVING,PS_EVT_HOST_WAKEUP_TIMOUT,0,0,0);
}
extern void drv_phy_delay(s32 delay_ms);
void MLME_ps_exit(void)
{
    hwtmr_stop(US_TIMER2);
    //dbg_timer_switch(0);
    printf("ps_exit,pst=%d\n",gDevInfo->ps_state);
    if((gDevInfo->ps_state == PWR_DOZE)||(gDevInfo->ps_state == PWR_BCN_SYNC)||(gDevInfo->ps_state == PWR_SLEEP)||
        (gDevInfo->ps_state == PWR_BCN_SYNC_TIM))
    {

        if(NULL==host_wakeup_timer)
        {
            if( OS_TimerCreate(&host_wakeup_timer, 500, (u8)FALSE, NULL, (OsTimerHandler)host_wakeup_timeout) == OS_FAILED)
            {
                goto NO_WKTMR;
            }
        }else{
            OS_TimerStop(host_wakeup_timer);
            OS_TimerSet(host_wakeup_timer,500,FALSE,NULL);
        }
        
        OS_TimerStart(host_wakeup_timer);

NO_WKTMR:
        gDevInfo->ps_state =PWR_WAIT_HOST_WAKEUP;
        set_wakeup_BB_gpio(0);
        MLME_ps_send_evt2host(SOC_EVT_PS_WAKENED);
        OS_TickDelay(1);
        set_wakeup_BB_gpio(1); //Rising up wakeup BB gpio
        //REG32(ADR_SYS_INT_FOR_HOST) = 0x00000000;
        //REG32(ADR_PAD20) = 0x00000001;
    }
}

u8 mlme_active_pmu_sleep(u32 sleep_10us, bool FromIrq)
{

    if(!FromIrq)
        taskENTER_CRITICAL();

    drv_pmu_setwake_cnt(sleep_10us);
    drv_pmu_init();
    drv_pmu_sleep();
    drv_pmu_chk();

    if(!FromIrq)
        taskEXIT_CRITICAL();

    if(drv_pmu_check_interupt_event()==true)
    {
        printf("pmu_check_interupt wakeup!\n");

        return WAKEUP_EVENT_INTERRUPT;        
    }
    else
    {
        return WAKEUP_EVENT_TIMER;
    }

}

u32 enterSystemSleep(u16 beacon_int, u8 dtim_count, u8 dtim_period,u32 diff_t_10us, u32 default_ltc_10us)
{
    u32 sleep_10us=0;
    u32 latency_10us=0;

    if(dtim_count > dtim_period)
    {
        dtim_count =0;
        dtim_period=1;
    }
    beacon_int *= 100;              //unit per 10us for pmu
    beacon_int += beacon_int*0.024; //transfer network unit
    //printf("itv=%d,diff=%d\r\n",beacon_int,diff_t_10us);

    if(gDevInfo->wow_req.dtim_multiple==0)
        gDevInfo->wow_req.dtim_multiple = 3; //Default

    //calc sleep time
    if(dtim_period == 1)
    { //Fixed period at 3 that refer to BRCM/MTK
        sleep_10us = gDevInfo->wow_req.dtim_multiple*beacon_int;
    }else{
        if(dtim_count){
            sleep_10us = dtim_count * beacon_int;

        }else{
            sleep_10us = dtim_period * beacon_int;
        }
    }
    //while(diff_t_10us > (sleep_10us+(beacon_int>>1))) //Case of beacon lost
    //        diff_t_10us -= sleep_10us;
    if(diff_t_10us == 0){
        latency_10us = 1200;
    }else if(diff_t_10us > sleep_10us){
        latency_10us += (diff_t_10us-sleep_10us+400);
    }
    latency_10us += (160+default_ltc_10us); //WIFI ready time 1.6ms

    latency_10us += (latency_10us*0.02);    //rtc compensation
    //printf("ltc=%d,slp=%d,diff=%d\r\n",latency_10us,sleep_10us,diff_t_10us);

    if(sleep_10us>latency_10us){
        sleep_10us-=latency_10us;
    }

    sleep_10us = (sleep_10us |(mlme_active_pmu_sleep(sleep_10us,gOsFromISR)<<24));
    //printf("ltc=%x,akt=%d\r\n",sleep_10us,akt);
    
    return sleep_10us;

}

u8 mlme_ps_deep_sleep(void)
{
    //u32 sleep_10us=6000000; //30 Sec

    //while(gDevInfo->ps_state == PWR_SLEEP)
    {
        return mlme_active_pmu_sleep(6000000,false);
    }
}
void beacon_sync_timeout (void *data)
{
#if 0 //For debug
    if((gDevInfo->ps_state == PWR_DOZE)||(gDevInfo->ps_state == PWR_BCN_SYNC)||
       (gDevInfo->ps_state == PWR_BCN_SYNC_TIM))
    {
        u32 sleep_10us;
        sleep_10us = enterSystemSleep(gDevInfo->beacon_int,gDevInfo->dtim_count,gDevInfo->dtim_period,0,((RX_BCN_TIMEOUT/10)));
        bkn_update_time += sleep_10us;
        if((gDevInfo->ps_state == PWR_DOZE))
            gDevInfo->ps_state = PWR_BCN_SYNC;
        printf("sw T s=%d!\n",sleep_10us);
        //printf("sw T\n");
    }
#endif
    
    //printf("beacon_sync_timeout\r\n");
    //MLME_ps_exit(); //Wakeup host to handle unicast ip packet    
    msg_evt_post_data1(MBOX_MLME, MEVT_POWER_SAVING, PS_EVT_BCN_SYNC_STMR,0,0,0);
}

void _ps_bcn_sync_stmr_start(void) //timeout means can not receive Beacon for 3 seconds, leave ps.
{
    if(NULL==bcn_sync_timer)
    {
        if( OS_TimerCreate(&bcn_sync_timer, 3000, (u8)FALSE, NULL, (OsTimerHandler)beacon_sync_timeout) == OS_FAILED)
        {
            MLME_ps_send_evt2host(SOC_EVT_PS_WAKENED);
            mlme_null_send(gDevInfo, false, true);// ps off
            gDevInfo->ps_state = PWR_WAKE;
            printf("bcn sync timer err\r\n");
            return;
        }
    }else{
        OS_TimerStop(bcn_sync_timer);
        OS_TimerSet(bcn_sync_timer,3000,FALSE,NULL);
    }
    OS_TimerStart(bcn_sync_timer);
}
bool ieee80211_check_tim(struct ieee80211_tim_ie *tim,u8 tim_len, u32 aid)
{
	u8 mask;
	u8 index, indexn1, indexn2;

    //LOG_PRINTF("\nc=%d,p=%d\n",tim->dtim_count,tim->dtim_period);

	aid &= 0x3fff;
	index = aid / 8;
	mask  = 1 << (aid & 7);

	indexn1 = tim->bitmap_ctrl & 0xfe;
	indexn2 = tim_len + indexn1 - 4;

	if (index < indexn1 || index > indexn2)
		return false;

	index -= indexn1;

	return !!(tim->virtual_map[index] & mask);
}
extern u32 bkn_update_time;
#define RX_BCN_TIMEOUT 30000 //30ms
extern u32 current_bcn,last_bcn;
void MLME_ps_beacon_sync_timeout(u32 tdata)
{
    //ps_doze_st* psdata = (ps_doze_st*)data;
    if(tdata == PS_EVT_BCN_SYNC_STMR) //PS exit directly
    {
        printf("STMR PS exit!\r\n");
        MLME_ps_exit();
        return;
    }
            
    switch(gDevInfo->ps_state)
    {
        case PWR_DOZE:
        case PWR_BCN_SYNC:
        case PWR_BCN_SYNC_TIM:
        {
            u32 sleep_10us;
            u8 wkt;
            sleep_10us = enterSystemSleep(gDevInfo->beacon_int,gDevInfo->dtim_count,gDevInfo->dtim_period,0,((RX_BCN_TIMEOUT/10)));
            wkt = (sleep_10us & 0xFF000000)>> 24;
            sleep_10us &= 0x00FFFFFF;
            printf("T s=%d\n",sleep_10us);
            if(wkt == WAKEUP_EVENT_INTERRUPT)            
                MLME_ps_exit();
            
            
            bkn_update_time += sleep_10us;
            if((gDevInfo->ps_state == PWR_DOZE))
            {
                gDevInfo->ps_state = PWR_BCN_SYNC;
                _ps_bcn_sync_stmr_start();
            }

            break;
        }
        default:
            break;

    }

}
void irq_beacon_timer_handler (void *data) //timeout go to sleep
{
    hwtmr_stop(US_TIMER2);
    msg_evt_post_data1(MBOX_MLME, MEVT_POWER_SAVING, PS_EVT_BCN_SYNC_HTMR,0,0,0);
}
bool ps_send_null_frame_mib(bool on)
{
    u16 frameSuccess = 0;
    u16 frameRetry = 0,frameTx = 0;
    u32 pcnt =2;
    u32 tcnt;
	s32 res;
    do
    {
        SET_MTX_MIB_EN0 (1);
        tcnt = GET_MTX_MIB_CNT0; //Read clear
        res = mlme_null_send(gDevInfo, on, true);// ps on
        if(res == 0)
        {
            OS_MsDelay(20);
	        tcnt = GET_MTX_MIB_CNT0;
            
	        //SET_MTX_MIB_EN0 (0);
	        //tcnt = GET_MTX_MIB_CNT0;
	        frameTx = (u16)(tcnt & 0x000003ff);
	        frameRetry = (u16)((tcnt >> 10) & 0x000003ff);
	        frameSuccess = (u16)((tcnt >> 20) & 0x000003ff);
	        printf("%d-P=%d--T=%d,R=%d,S=%d\r\n",pcnt,on,frameTx,frameRetry,frameSuccess);
	        //printf("%d-P=%d--S=%d\r\n",pcnt,on,frameSuccess);
	        if(frameSuccess)
	        {
	            return true;
	        }
	        else
	        {
	            if(pcnt==0)
	            {
	                //MLME_ps_send_evt2host(SOC_EVT_PS_WAKENED);
	                //mlme_null_send(gDevInfo, false, true);// ps off
	                //gDevInfo->ps_state = PWR_WAKE;
	                printf("Send ps null frame fail\r\n");
	                return false;
	            }
	        }
        }
		pcnt--;
    }while(pcnt>0);
	return false;
}
void phy_mode_switch(u32 mode, bool txlp)
{
    
    SET_RG_MODE(_RF_MODE_SHUTDOWN_);
    //printf("b m\r\n");
    OS_TickDelay(1);
    drv_phy_off();

    if(mode == PS_PHY_B)
        (*(volatile u32 *) 0xCE000004) = 0x07e;// B only   drv_phy_b_only();

    else if(mode == PS_PHY_BGN)
        (*(volatile u32 *) 0xCE000004) = 0x17e;// B/G/N only    drv_phy_bgn();

    if(txlp)        
        drv_phy_tx_loopback();

    drv_phy_on();
    SET_RG_MODE(_RF_MODE_TRX_EN_);

}

u32 MLME_ps_bcn_handling(PKT_RxInfo *PktInfo, struct ieee802_11_elems *elems, u32 bcn_cnt, u32 diff_t)
{
    u32 sleep_10us=0;
       
    if((gDevInfo->ps_state == PWR_DOZE)||(gDevInfo->ps_state == PWR_BCN_SYNC)||(gDevInfo->ps_state == PWR_BCN_SYNC_TIM))
    {
        HDR80211_Mgmt *Mgmt80211=HDR80211_MGMT(PktInfo);
        //Time_T current_Time,wakeup_time;
        //current_Time.t = 0;
        //hwtmr_stop(US_TIMER2);
        //OS_TimerStop(bcn_sync_timer);
        if(gDevInfo->aid)
        {
            bool directed_tim = false;
            struct ieee80211_tim_ie *tim=NULL;
            if(elems==NULL){
                //printf("Beacon error,PktInfo->len=%d\n",PktInfo->len);
                return sleep_10us;
            }

            tim = ((struct ieee80211_tim_ie *)(elems->tim));
            gDevInfo->beacon_int = Mgmt80211->u.beacon.beacon_int;
            gDevInfo->dtim_count = tim->dtim_count;
            gDevInfo->dtim_period = tim->dtim_period;
            
            directed_tim = ieee80211_check_tim(tim,elems->tim_len,gDevInfo->aid);


            if(!directed_tim)
            {
                u8 wkt=0;
                if(tim->bitmap_ctrl & 0x01) //Check multicaset buffered frame
                {
                    if(gDevInfo->ps_state == PWR_DOZE)
                    {
                        //drv_phy_off();
                        //drv_phy_bgn();
                        //Avoid TX EVM unnormal
                        //drv_phy_tx_loopback();
                        //drv_phy_on();
                    }
                    printf("M S=%d\r\n",(Mgmt80211->seq_ctrl>>4));
                    gDevInfo->ps_state = PWR_BCN_SYNC;
                    _ps_bcn_sync_stmr_start();
                    return (gDevInfo->beacon_int*102.4); //unit 10us
                }

                //dbg_get_time(&current_Time);
                if((gDevInfo->ps_state == PWR_BCN_SYNC)||(gDevInfo->ps_state == PWR_BCN_SYNC_TIM))
                {   
                    if(gDevInfo->ps_state == PWR_BCN_SYNC_TIM)
                        ps_send_null_frame_mib(true);//ps off

                    //SET_RG_MODE(_RF_MODE_SHUTDOWN_);
                    //drv_phy_off();
                    //drv_phy_b_only();
                    //drv_phy_on();
                    //SET_RG_MODE(_RF_MODE_TRX_EN_);
                    gDevInfo->ps_state = PWR_DOZE;
                }

                sleep_10us = enterSystemSleep(gDevInfo->beacon_int,gDevInfo->dtim_count,gDevInfo->dtim_period,(diff_t/10),400);
                wkt = (sleep_10us & 0xFF000000)>> 24;
                sleep_10us &= 0x00FFFFFF;

                printf("b=%d/%d/%d,S=%d,D=%d\n",gDevInfo->dtim_count,gDevInfo->dtim_period,(Mgmt80211->seq_ctrl>>4),sleep_10us,(diff_t/10));
                //printf("%d/%d\n",gDevInfo->dtim_count,gDevInfo->dtim_period);
                //LOG_PRINTF("B=%d/%d,S=%d,RxT=%d,W9=%d,CrT=%d,D1=%d,D2=%d\n",tim->dtim_count,tim->dtim_period,sleep_ms,PktInfo->time_l,PktInfo->word9,
                //current_Time.ts.lt,(current_Time.ts.lt-PktInfo->word9),(PktInfo->word9-PktInfo->time_l));
                if(wkt == WAKEUP_EVENT_INTERRUPT)            
                    MLME_ps_exit();
                else
                    hwtmr_start(US_TIMER2,RX_BCN_TIMEOUT,irq_beacon_timer_handler,(void*)gDevInfo,HTMR_ONESHOT); //for rx beacon timeout. Go to sleep anyway.
                //OS_TimerSet(bcn_sync_timer,((RX_BCN_TIMEOUT+(sleep_10us*10))/1000),FALSE,(void*)gDevInfo);
                //OS_TimerStart(bcn_sync_timer);
            }
            else
            {
                printf("tim for us,S=%d\n",(Mgmt80211->seq_ctrl>>4));
                phy_mode_switch(PS_PHY_BGN, false);
                
                gDevInfo->ps_state = PWR_BCN_SYNC_TIM;
                ps_send_null_frame_mib(false);//ps off
                return (gDevInfo->beacon_int*102.4); //unit 10us
            }
        }
    }
    return (sleep_10us&0x00FFFFFF);
}

extern s32 drv_rtc_cali(void);
extern OsTimer recover_timer;;
extern u8 recover_mechanism;
extern u8 recover_enable;
void MLME_PwrSaving(void* pdata)
{

    struct cfg_ps_request* wow = (struct cfg_ps_request*)pdata;

    printf("STA aid=%d,st=%d,ps_st=%d,host_ps=%d,ip=%x\r\n",gDevInfo->aid,gDevInfo->State,gDevInfo->ps_state,wow->host_ps_st,wow->ipv4addr);
    drv_rtc_cali();

    switch(wow->host_ps_st)
    {
        case HOST_PS_SETUP:
        {
            if(gDevInfo->ps_state != PWR_WAKE)
            {
                //printf("err! gDev st=%d\r\n",gDevInfo->ps_state);
                return;
            }
            
            if(recover_enable > 0 && recover_mechanism == 1)
            {
                OS_TimerStop(recover_timer);
            }

            gDevInfo->wow_req = *((struct cfg_ps_request*)pdata);
            //dbg_timer_switch(1); //Enable TSF Timer
            if((gDevInfo->aid))
            {
                if(ps_send_null_frame_mib(true)!=true)
                {
                    
                    MLME_ps_send_evt2host(SOC_EVT_PS_WAKENED);
                    mlme_null_send(gDevInfo, false, true);// ps off
                    //gDevInfo->ps_state = PWR_WAKE;
                    return;
                }
                
				SET_RG_MODE(_RF_MODE_SHUTDOWN_);
                OS_MsDelay(500); //Wait to all int be clear
            }
            MLME_ps_send_evt2host(SOC_EVT_PS_SETUP_OK);
            break;
        }
        case HOST_PS_START:
        {
            set_wakeup_BB_gpio(0);
            if((gDevInfo->aid))
            {
                gDevInfo->ps_state = PWR_BCN_SYNC;
                _ps_bcn_sync_stmr_start();

                phy_mode_switch(PS_PHY_B, false);
                
                bkn_update_time=0;
            }
            else
            {
                gDevInfo->ps_state = PWR_SLEEP; //Only can wakeup by host int.

                //Task will be blocked here. Don't wake up host at each time's up.
                //Out of this while loop. means Host wakes up wifi by interrupt,
                while(gDevInfo->ps_state == PWR_SLEEP)
                {
                    if(mlme_ps_deep_sleep() == WAKEUP_EVENT_INTERRUPT)            
                        MLME_ps_exit();
                    printf("DS\r\n");
                }
            }
            
            break;
        }
        case HOST_PS_WAKEUP_OK:
        {            
            gDevInfo->wow_req.host_ps_st = HOST_PS_WAKEUP_OK;
            OS_TimerStop(host_wakeup_timer);
            printf("H_WKP\r\n");
            phy_mode_switch(PS_PHY_BGN, true);
            //OS_TickDelay(1);
            if(gDevInfo->aid)
                ps_send_null_frame_mib(false);//ps off
            gDevInfo->ps_state = PWR_WAKE;

            if(recover_enable > 0 && recover_mechanism == 1)
            {
                OS_TimerStart(recover_timer);
            }

            //printf("WK_OK\r\n");
            break;
        }
        default:
            //printf("PS unknow host st=%d\r\n",gDevInfo->wow_req.host_ps_st);
            break;
    }
}

