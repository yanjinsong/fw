#ifndef _DRV_DMA_H_
#define _DRV_DMA_H_

#include <regs.h>
#include <bsp/irq.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
	ADDRESS_UPDATE_FIXED =0,
	ADDRESS_UPDATE_INCREASE
} ADDRESS_UPDATE;

typedef enum
{
	TRANSFER_BYTE_UNIT = 0,
	TRANSFER_WORD_UNIT,
	TRANSFER_DWORD_UNIT
} TRANSFER_UNIT;

typedef enum
{
	DMA_STATUS_BUSY = 0
} DMA_STATUS;

typedef enum
{
	FAST_FILL_DISABLE = 0,
	FAST_FILL_ENABLE
} DMA_FAST_FILL;


typedef enum
{
	DMA_INT_UN_MASKED = 0x0,
	DMA_INT_MASKED = 0x1,
} DMA_INT_MASK;

#ifdef __cplusplus
}
#endif

void drv_dma_init();
void drv_dma_memset(void * dstAddr, u32 value, u16 length);
void drv_dma_memcpy(void * dstAddr, const u8 * srcAddr, u16 length);
s32 drv_dma_set_interrupt_handle(irq_handler dma_handle, void *m_data);


#endif /* _DRV_DMA_H_ */

