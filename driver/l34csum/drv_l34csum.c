#include <config.h>
#include <log.h>
#include "drv_l34csum.h"
#include <regs.h>
#include <log.h>


void drv_l34_init_checksum()
{
	SET_CALCULATE (0xffffffff);
	SET_CS_ADDER_EN(0x0);
}


u32 drv_l34_cal_data_checksum(u32 address,u16 len)
{
	SET_CS_START_ADDR(address&0x0000ffff);
	SET_CS_PKT_ID(address>>16);
	SET_ADD_LEN(len);
	SET_CS_ADDER_EN( 0x01);
	
	while(GET_CS_ADDER_EN) { }
	return GET_CHECK_SUM;
	//return 0;
}

u32 drv_l34_cal_pesudo_checksum(u32 ip,u8 ipLen,u8 l4_pro,u16 l4_len)
{
	
	SET_CS_START_ADDR(ip&0x0000ffff);
	SET_CS_PKT_ID(ip>>16);
	SET_ADD_LEN( ipLen);
	SET_L4_LEN(l4_len);
	SET_L4_PROTOL(l4_pro);

	
	SET_PSEUDO(0x1);
	SET_CS_ADDER_EN(0x1);
	while(GET_PSEUDO) { }
	return GET_CHECK_SUM;
}



