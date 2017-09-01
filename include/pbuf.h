#ifndef _PBUF_H_
#define _PBUF_H_

#include <ssv_pktdef.h>


bool PBUF_is_reusable(void *old_packet, u32 new_packet_len);
void        *PBUF_MAlloc_Raw(u32 size, u32 need_header, PBuf_Type_E buf_type);
void		_PBUF_MFree(void *PKTMSG);
void *_PBUF_MReAlloc(void **_pp, u32 new_size);

#define PBUF_SET_REUSE(pp) ((*pp)=NULL) //parameter type is ponit to point

#if (CONFIG_SIM_PLATFORM ==0)

#define PBUF_BASE_ADDR	            0x80000000
#define PBUF_ADDR_SHIFT	            16
#define PBUF_ADDR_MAX               128
#define PBUF_PAGE_SIZE              256


//#define PBUF_GetPktByID(_ID)        (PBUF_BASE_ADDR|((_ID)<<PBUF_ADDR_SHIFT))
//#define PBUF_GetPktID(_PKTMSG)      (((u32)_PKTMSG&0x0FFFFFFF)>>PBUF_ADDR_SHIFT)

#define PBUF_MapPkttoID(_PKT)		(((u32)_PKT&0x0FFFFFFF)>>PBUF_ADDR_SHIFT)
#define PBUF_MapIDtoPkt(_ID)		(PBUF_BASE_ADDR|((_ID)<<PBUF_ADDR_SHIFT))

#define PBUF_isPkt(addr)    (((u32)(addr) >= (u32)PBUF_BASE_ADDR) && \
                                ((u32)(addr) <= (u32)PBUF_MapIDtoPkt(PBUF_ADDR_MAX-1)) && \
                                ((((u32)(addr))&0xFFFF)==0))

u32 PBUF_GetEngineID(PKT_Info *PktInfo, enum fcmd_seek_type seek);

#define PBUF_MAlloc(size, type)  PBUF_MAlloc_Raw(size, 1, type);
#define PBUF_MFree(PKTMSG) _PBUF_MFree(PKTMSG)

/**
 * This function only support to reuse a pbuf, and this pbuf is a HOST_CMD, PKT_TxInfo or Pkt_RxInfo
 */
#define PBUF_MReAlloc(pp,new_size)  _PBUF_MReAlloc((void **)pp,new_size)

#else /* CONFIG_SIM_PLATFORM */

#define PBUF_MAlloc(size, type)  PBUF_MAlloc_Raw(size, 1, type);
#define PBUF_MFree(PKTMSG) _PBUF_MFree(PKTMSG)
#define PBUF_isPkt(addr)    (1)

#if (CONFIG_SIM_PLATFORM == 1)
void	PBUF_HwDrop(void *PKTMSG);
u16		PBUF_GetPbufID(void *PKTMSG);
u8		PBUF_GetParsedStatus(void *PKTMSG);
void	PBUF_SetParsedStatus(void *PKTMSG);
void	PBUF_SetDropFlag(void *PKTMSG);
u8		PBUF_GetDropFlag(void *PKTMSG);
void	PBUF_AddFlow(void *PKTMSG, u8 *fcmd, u8 *scmd);
u8		*PBUF_GetFlow(void *PKTMSG);
void	PBUF_Logging(void *PKTMSG);
void	PBUF_SetPktID(void *PKTMSG, u32 pktid);
u32		PBUF_GetPktID(void *PKTMSG) ;
#if 0
u32		PBUF_MapPktByID(u32 PKTID);
#endif

void	PBUF_Set2HostFlag(void *PKTMSG);
u8		PBUF_Get2HostFlag(void *PKTMSG);
void	PBUF_Dump(void);
bool	PBUF_AllUnUse(void);

//Just copy pktid, fragid, flow_buffer
void	PBUF_CopyPBUFInfo(void *DSTPKTMSG, void *SRCPKTMSG);



u8 PBUF_MapPkttoID(void *PKTMSG);
void* PBUF_MapIDtoPkt(u32 pktid);

#endif // (CONFIG_SIM_PLATFORM == 1)

#endif /* CONFIG_SIM_PLATFORM */

u32		PBUF_GetEngineID(PKT_Info *PktInfo, enum fcmd_seek_type seek);
s32		PBUF_Init(void);

#endif /* _PBUF_H_ */





