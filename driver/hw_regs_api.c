
#include <config.h>
#include <types.h>
#include "hw_regs_api.h"
#include <mbox/drv_mbox.h>
#include <regs.h>
#include <security/drv_security.h>



ETHER_ADDR gSTA_MAC;
ETHER_ADDR gBSSID;

u32 security_key_buffer_init(void)
{
    s32 ret = wpa_drv_init();
	
	return (ret == 0);
}

u32 __HWREG_Init(void)
{
	//Allocate one packet buffer to store security key
	if(security_key_buffer_init() == FALSE)
		return FALSE;

	return TRUE;
}

