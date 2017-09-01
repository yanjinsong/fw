#include <config.h>
#include <regs.h>

#include "drv_flash.h"

#define PageSize 256
#define PPcmd  0x02
#define WriteEN 0x06
#define ReadCmd 0x03
u8 drv_flash_readSatus()
{
    volatile u8 wbuffer[4]={0},rbuffer[4]={0};
    //volatile u16 status =0 ;
    wbuffer[0]=0x05;
    

    SET_CMD_ADDR((u32)&wbuffer[0]);
    SET_SRAM_ADDR((u32)rbuffer);
    SET_CMD_LEN(2);
    OS_MsDelay(1);
    //printf("status=%x\n",rbuffer[1]);
    return (rbuffer[1]);
    
} 

void drv_flash_writeEn()
{
    u8 wbuffer[4]={0},rbuffer[4]={0};
    
    wbuffer[0]=0x06;

    SET_CMD_ADDR((u32)wbuffer);
    SET_SRAM_ADDR((u32)rbuffer);
    SET_CMD_LEN(1); 
    OS_MsDelay(1);
   
} 

void drv_flash_pageProgram(u32 address,u8 *data,u8 size)
{
   
   //need to check address??
   int i=0;
   u8 wbuffer[4+size],rbuffer[4+size];
   drv_flash_writeEn();
   
   memset(wbuffer,0x0,4+size);
   
   wbuffer[0]=0x02;
   wbuffer[1] =(address>>16)&0xff;
   wbuffer[2] =(address>>8)&0xff;
   wbuffer[3] = address&0xff;

  // printf("address=%x\n",address);
   for(i=0;i<size;i++)
   {
      wbuffer[i+4]=data[i];
     //printf("data=%x\n",data[i]);
   }
 
   SET_CMD_ADDR((u32)wbuffer);   
   SET_SRAM_ADDR((u32)rbuffer);
   SET_CMD_LEN(size+4);
   OS_MsDelay(1);
   while((drv_flash_readSatus()&0x01)==0x01)
    {}
   
  
} // end of - Cmd_Read -
   
   
   
void drv_flash_sectorErase(u32 address)
{    
  
   u8 wbuffer[4],rbuffer[4];
   drv_flash_writeEn();
   wbuffer[0] = 0x20;
   wbuffer[1] = (address>>16)&0xff;
   wbuffer[2] = (address>>8)&0xff;
   wbuffer[3] = address&0xff;

  SET_CMD_ADDR((u32)wbuffer);
  SET_SRAM_ADDR((u32)rbuffer);
  SET_CMD_LEN(4);
  OS_MsDelay(1);
   while((drv_flash_readSatus()&0x01)==0x01)
   {}


} 

 
void drv_flash_blockErase(u32 address)
{    
  
   u8 wbuffer[4],rbuffer[4];
   drv_flash_writeEn();
   wbuffer[0] = 0xd8;
   wbuffer[1] = (address>>16)&0xff;
   wbuffer[2] = (address>>8)&0xff;
   wbuffer[3] = address&0xff;

   SET_CMD_ADDR((u32)wbuffer);
   SET_SRAM_ADDR((u32)rbuffer);
   SET_CMD_LEN(4);
   OS_MsDelay(1);
   while((drv_flash_readSatus()&0x01)==0x01)
   {}


} 


void drv_flash_read(u32 addr,u8 *rdata,u32 size)
{
     
    u8 wbuffer[4+size],rbuffer[size+4];

    memset(rbuffer,0x0,size+4);
    memset(wbuffer,0x0,size+4);
    wbuffer[0]=0x03;
    wbuffer[1] =(addr>>16)&0xff;
    wbuffer[2] =(addr>>8)&0xff;
    wbuffer[3] = addr&0xff;
    
    SET_CMD_ADDR((u32) wbuffer);   
    SET_SRAM_ADDR((u32)rbuffer);
    SET_CMD_LEN(size+4);
    
    OS_MsDelay(1);
    
   
    memcpy(rdata,&rbuffer[4],size); 
   
    
}


