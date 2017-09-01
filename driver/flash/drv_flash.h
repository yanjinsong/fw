#ifndef _DRV_FLASH_H_
#define _DRV_FLASH_H_

#include <regs.h>

 #define PageSize 256
#define PPcmd  0x02
#define WriteEN 0x06
#define ReadCmd 0x03

u8 drv_flash_readSatus();
void drv_flash_writeEn();
void drv_flash_pageProgram(u32 address,u8 *data,u8 size);

void drv_flash_sectorErase(u32 address);
void drv_flash_blockErase(u32 address);


#endif /* _DRV_UART_H_ */

