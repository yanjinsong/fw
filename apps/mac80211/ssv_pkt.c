#include <config.h>
#include <ssv_pktdef.h>

u8 ssv6xxx_get_pbuf_hdr80211(void *p,u32 i)
{
    PKT_Info *pktInfo=(PKT_Info *)p;
    PKT_TxInfo *pktTxInfo=(PKT_TxInfo *)p;
    u32 ampdu_offset=0;
    if(IS_TX_PKT(pktInfo)){
        if(1==pktTxInfo->aggregation){
            ampdu_offset=4; //sizeof(delimiter)=4;
        }
    }

    return *((u8 *)((u32)p+pktInfo->hdr_offset+i+ampdu_offset));

}

u8* ssv6xxx_get_addr_hdr80211(void *p,u32 i)
{
    PKT_Info *pktInfo=(PKT_Info *)p;
    PKT_TxInfo *pktTxInfo=(PKT_TxInfo *)p;
    u32 ampdu_offset=0;
    if(IS_TX_PKT(pktInfo)){
        if(1==pktTxInfo->aggregation){
            ampdu_offset=4; //sizeof(delimiter)=4;
        }
    }

    return ((u8 *)((u32)p+pktInfo->hdr_offset+i+ampdu_offset));

}
