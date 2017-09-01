#include <config.h>
#include <bsp/irq.h>
#include <dma/drv_dma.h>
#include <pbuf.h>
#include <lib/ssv_lib.h>
#include "types.h"

#define	BUFFER_LENGTH	0xc
#define	TEST_LENGTH		0xb

static u8 programRam_src[BUFFER_LENGTH];
static u8 programRam_dst[BUFFER_LENGTH];

#define ALIGNMENT_OFFSET	0x04

void _Cmd_HwDMA() {
	u8 *packetbuffer_src,*packetbuffer_dst;
	u32	x,y;
	u8	value;

	LOG_PRINTF("**************************************************\n");
	packetbuffer_src = PBUF_MAlloc_Raw(BUFFER_LENGTH,0,SOC_PBUF);
	LOG_PRINTF("packetbuffer_src address = %08x\n",packetbuffer_src);
	packetbuffer_dst = PBUF_MAlloc_Raw(BUFFER_LENGTH,0,SOC_PBUF);
	LOG_PRINTF("packetbuffer_dst address = %08x\n",packetbuffer_dst);

	LOG_PRINTF("programRam_src address = %08x\n",programRam_src);
	LOG_PRINTF("programRam_dst address = %08x\n",programRam_dst);
	LOG_PRINTF("**************************************************\n");

	//Program ram memset verify
	for(x=0;x<ALIGNMENT_OFFSET;x++)
	{
		//Memset verify
		memset(programRam_src,x,BUFFER_LENGTH);
		memset(programRam_dst,0x55,BUFFER_LENGTH);
		memset(packetbuffer_src,x,BUFFER_LENGTH);
		memset(packetbuffer_dst,0x55,BUFFER_LENGTH);

		drv_dma_memset_inc(&programRam_dst[x],programRam_src,TEST_LENGTH);
		y = TEST_LENGTH;
		value = (u8)x;
		do{
			if(memcmp(&programRam_dst[x+y-1],&value,1))
			{
				LOG_PRINTF("x=[%x] value=[%x] value address=[%x]\n",x,value,&value);
				LOG_PRINTF("Program memset_inc check fail offset[%x]\n",y);
				while(1);
			}
			y--;
		}while(y);

		if(x)
		{
			if(programRam_dst[0] == (u8)x)
			{
				LOG_PRINTF("Step 0.1-[%d] something worong!!\n",x);
				while(1);
			}
		}
	}

	//Packet buffer memset verify
	for(x=0;x<ALIGNMENT_OFFSET;x++)
	{
		//Memset verify
		memset(programRam_src,x,BUFFER_LENGTH);
		memset(programRam_dst,0x55,BUFFER_LENGTH);
		memset(packetbuffer_src,x,BUFFER_LENGTH);
		memset(packetbuffer_dst,0x55,BUFFER_LENGTH);

		drv_dma_memset_inc(&packetbuffer_dst[x],programRam_src,TEST_LENGTH);
		y = TEST_LENGTH;
		value = (u8)x;
		do{
			if(memcmp(&packetbuffer_dst[x+y-1],&value,1))
			{
				LOG_PRINTF("x=[%x] value=[%x] value address=[%x]\n",x,value,&value);
				LOG_PRINTF("Packet buffer memset_inc check fail offset[%x]\n",y);
				while(1);
			}
			y--;
		}while(y);

		if(x)
		{
			if(packetbuffer_dst[0] == (u8)x)
			{
				LOG_PRINTF("Step 0.2-[%d] something worong!!\n",x);
				while(1);
			}
		}
	}

	//Program ram memset verify
	for(x=0;x<ALIGNMENT_OFFSET;x++)
	{
		//Memset verify
		memset(programRam_src,x,BUFFER_LENGTH);
		memset(programRam_dst,0x55,BUFFER_LENGTH);
		memset(packetbuffer_src,x,BUFFER_LENGTH);
		memset(packetbuffer_dst,0x55,BUFFER_LENGTH);

		drv_dma_memset(&programRam_dst[x],(x | (x<<8) | (x<<16) | (x<<24)),TEST_LENGTH);
		y = TEST_LENGTH;
		value = (u8)x;
		do{
			if(memcmp(&programRam_dst[x+y-1],&value,1))
			{
				LOG_PRINTF("x=[%x] value=[%x] value address=[%x]\n",x,value,&value);
				LOG_PRINTF("Program memset check fail offset[%x]\n",y);
				while(1);
			}
			y--;
		}while(y);

		if(x)
		{
			if(programRam_dst[0] == (u8)x)
			{
				LOG_PRINTF("Step 1-[%d] something worong!!\n",x);
				while(1);
			}
		}
	}

	//Packet buffer memset verify
	for(x=0;x<ALIGNMENT_OFFSET;x++)
	{
		//Memset verify
		memset(programRam_src,x,BUFFER_LENGTH);
		memset(programRam_dst,0x55,BUFFER_LENGTH);
		memset(packetbuffer_src,x,BUFFER_LENGTH);
		memset(packetbuffer_dst,0x55,BUFFER_LENGTH);

		drv_dma_memset(&packetbuffer_dst[x],(x | (x<<8) | (x<<16) | (x<<24)),TEST_LENGTH);
		y = TEST_LENGTH;
		value = (u8)x;
		do{
			if(memcmp(&packetbuffer_dst[x+y-1],&value,1))
			{
				LOG_PRINTF("x=[%x] value=[%x] value address=[%x]\n",x,value,&value);
				LOG_PRINTF("Packet buffer memset check fail offset[%x]\n",y);
				while(1);
			}
			y--;
		}while(y);

		if(x)
		{
			if(packetbuffer_dst[0] == (u8)x)
			{
				LOG_PRINTF("Step 2-[%d] something worong!!\n",x);
				while(1);
			}
		}
	}

	//Program ram to program ram
	for(x=0;x<ALIGNMENT_OFFSET;x++)
	{
		//Memcpy verify
		memset(programRam_src,x,BUFFER_LENGTH);
		memset(programRam_dst,0x55,BUFFER_LENGTH);
		memset(packetbuffer_src,x,BUFFER_LENGTH);
		memset(packetbuffer_dst,0x55,BUFFER_LENGTH);

		drv_dma_memcpy(&programRam_dst[x],&programRam_src[x],TEST_LENGTH);
		if(memcmp(&programRam_dst[x],&programRam_src[x],TEST_LENGTH))
		{
            LOG_PRINTF("x=[%x] \n",x);
			LOG_PRINTF("Program-Program memcpy check fail\n");
			while(1);
		}

		if(x)
		{
			if(programRam_dst[0] == (u8)x)
			{
				LOG_PRINTF("Step 3-[%d] something worong!!\n",x);
				while(1);
			}
		}
	}

	//Program ram to packet buffer
	for(x=0;x<ALIGNMENT_OFFSET;x++)
	{
		//Memcpy verify
		memset(programRam_src,x,BUFFER_LENGTH);
		memset(programRam_dst,0x55,BUFFER_LENGTH);
		memset(packetbuffer_src,x,BUFFER_LENGTH);
		memset(packetbuffer_dst,0x55,BUFFER_LENGTH);

		drv_dma_memcpy(&packetbuffer_dst[x],&programRam_src[x],TEST_LENGTH);
		if(memcmp(&packetbuffer_dst[x],&programRam_src[x],TEST_LENGTH))
		{
			LOG_PRINTF("Program-Packet memcpy check fail\n");
			while(1);
		}

		if(x)
		{
			if(packetbuffer_dst[0] == (u8)x)
			{
				LOG_PRINTF("Step 4-[%d] something worong!!\n",x);
				while(1);
			}
		}
	}

	//Packet buffer to program ram
	for(x=0;x<ALIGNMENT_OFFSET;x++)
	{
		//Memcpy verify
		memset(programRam_src,x,BUFFER_LENGTH);
		memset(programRam_dst,0x55,BUFFER_LENGTH);
		memset(packetbuffer_src,x,BUFFER_LENGTH);
		memset(packetbuffer_dst,0x55,BUFFER_LENGTH);

		drv_dma_memcpy(&programRam_dst[x],&packetbuffer_src[x],TEST_LENGTH);
		if(memcmp(&programRam_dst[x],&packetbuffer_src[x],TEST_LENGTH))
		{
			LOG_PRINTF("Packet-Program memcpy check fail\n");
			while(1);
		}

		if(x)
		{
			if(programRam_dst[0] == (u8)x)
			{
				LOG_PRINTF("Step 5-[%d] something worong!!\n",x);
				while(1);
			}
		}
	}

	//Packet buffer to packet buffer
	for(x=0;x<ALIGNMENT_OFFSET;x++)
	{
		//Memcpy verify
		memset(programRam_src,x,BUFFER_LENGTH);
		memset(programRam_dst,0x55,BUFFER_LENGTH);
		memset(packetbuffer_src,x,BUFFER_LENGTH);
		memset(packetbuffer_dst,0x55,BUFFER_LENGTH);

		drv_dma_memcpy(&packetbuffer_dst[x],&packetbuffer_src[x],TEST_LENGTH);
		if(memcmp(&packetbuffer_dst[x],&packetbuffer_src[x],TEST_LENGTH))
		{
			LOG_PRINTF("Packet-Packet memcpy check fail\n");
			while(1);
		}

		if(x)
		{
			if(packetbuffer_dst[0] == (u8)x)
			{
				LOG_PRINTF("Step 6-[%d] something worong!!\n",x);
				while(1);
			}
		}
	}

	LOG_PRINTF("DMA verify done!!!!\n");

	while(1);
}
