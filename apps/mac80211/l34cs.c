#include <config.h>
#include <pbuf.h>
#include "l34cs.h"
#include <mbox/drv_mbox.h>




void L34_Handler( PKT_Info *pPktInfo, bool is_tx )
{
#if (CONFIG_SIM_PLATFORM == 1)
	PBUF_AddFlow(pPktInfo,"CPU_L34","(out)");
#endif
	ENG_MBOX_NEXT((u32)pPktInfo);
}



