#include <config.h>
#include <rtos.h>
#include "drv_dma.h"

OsMutex dmaMutex;

void drv_dma_init()
{
	OS_MutexInit(&dmaMutex);
}

static void drv_dma_mutex_lock()
{
	OS_MutexLock(dmaMutex);
}

static void drv_dma_mutex_unlock()
{
	OS_MutexUnLock(dmaMutex);
}

/*
	Memset anywhere.
*/
void drv_dma_memset_inc(void * dstAddr, void * srcAddr, u16 length)
{
#if (CONFIG_SIM_PLATFORM == 1)	// /wifi/Wifi-SimPlatform-New-Arch
	;
#else
	drv_dma_mutex_lock();
	SET_DMA_ADR_SRC((u32)srcAddr);
	SET_DMA_ADR_DST((u32)dstAddr);
	//Avoid hardware bug
	SET_DMA_LEN(0);
	SET_DMA_SRC_INC(ADDRESS_UPDATE_FIXED);
	SET_DMA_LEN(length);

	while(GET_DMA_FINISH == DMA_STATUS_BUSY);
	//Avoid hardware bug
	SET_DMA_LEN(0);
	SET_DMA_SRC_INC(ADDRESS_UPDATE_INCREASE);

	drv_dma_mutex_unlock();
#endif
}


/*
	Memset anywhere.
*/
void drv_dma_memset(void * dstAddr, u32 value, u16 length)
{
	

#if (CONFIG_SIM_PLATFORM == 1) 	// /wifi/Wifi-SimPlatform-New-Arch
	memset(dstAddr,value,length);
#else

#if DMA_INCREMENT
	u32 srcAddr = value;
	drv_dma_memset_inc(dstAddr, &srcAddr, length);
#else
	drv_dma_mutex_lock();
	//Avoid hardware bug
	SET_DMA_LEN(0);
	SET_DMA_FAST_FILL(FAST_FILL_ENABLE);
	SET_DMA_ADR_DST((u32)dstAddr);
	SET_DMA_CONST(value);
	SET_DMA_LEN(length);

	while(GET_DMA_FINISH == DMA_STATUS_BUSY);
	//Avoid hardware bug
	SET_DMA_LEN(0);
	SET_DMA_FAST_FILL(FAST_FILL_DISABLE);
	
	drv_dma_mutex_unlock();
#endif
#endif
}

/*
	Copy anywhere.
*/
void drv_dma_memcpy(void * dstAddr, const u8 * srcAddr, u16 length)
{
#if (CONFIG_SIM_PLATFORM == 1) 	// /wifi/Wifi-SimPlatform-New-Arch
	memcpy(dstAddr,srcAddr,length);
#else
	drv_dma_mutex_lock();
	SET_DMA_ADR_SRC((u32)srcAddr);
	SET_DMA_ADR_DST((u32)dstAddr);
	SET_DMA_LEN(length);

	while (GET_DMA_FINISH == DMA_STATUS_BUSY);
	drv_dma_mutex_unlock();
#endif
}

s32 drv_dma_set_interrupt_handle(irq_handler dma_handle, void *m_data)
{
	return irq_request(IRQ_DMA0, dma_handle, m_data);
}


