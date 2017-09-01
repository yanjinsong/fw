#ifndef _DRV_PKT_DMA_H_
#define _DRV_PKT_DMA_H_

#include <regs.h>

void drv_pkt_dma_init();
void drv_pkt_memcpy(void *des,void *src,u16 len);

#endif /* _DRV_PKT_DMA_H_ */

