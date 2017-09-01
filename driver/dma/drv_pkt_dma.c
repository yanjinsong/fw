#include <config.h>
#include <rtos.h>
#include "drv_pkt_dma.h"


OsMutex pktDmaMutex;

void drv_pkt_dma_init()
{
	OS_MutexInit(&pktDmaMutex);
}

void drv_pkt_dma_mutex_lock()
{
	OS_MutexLock(pktDmaMutex);
}

void drv_pkt_dma_mutex_unlock()
{
	OS_MutexUnLock(pktDmaMutex);
}

/*
	Only copy between hardware packet buffer.
*/
void drv_pkt_memcpy(void *des,void *src,u16 len)
{
	drv_pkt_dma_mutex_lock();

#if (CONFIG_SIM_PLATFORM == 1) 	// /wifi/Wifi-SimPlatform-New-Arch
	memcpy(des,src,len);
#else
    //Setting source address
    REG32(ADR_DMA_RDATA) =(u32)src;
    //Setting destination address
    REG32(ADR_DMA_WDATA) =(u32)des;
    SET_LEN(len);
    //Polling packet buffer DMA done
    /*lint -save -e722 */
    while (GET_CLR) {}
    /*lint -restore */
#endif

	drv_pkt_dma_mutex_unlock();

	return;
}
