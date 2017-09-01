#include <config.h>
#include <msgevt.h>
#include <pbuf.h>

#include "mlme.h"
#include "util.h"


u16 gBitRates[] = { 10, 20, 55, 110, 60, 90, 120, 180, 240, 360, 480, 540 };
//u16 gBitRates[] = {10,20,55,110, 0, 0, 0, 0, 0, 0, 0, 0};

void BitRate_BitMask2IE(u32 RateMask, u32 BasicRateMask, u8 *rate_ie, u8 mlme_eid)
{
    u32 index;
    u8 MsbFlag, *pos=&rate_ie[2];


    rate_ie[1] = 0;
    for(index=0; index<sizeof(gBitRates)/sizeof(u16); index++)
    {
        if (!(RateMask & (1<<index)))
            continue;
        
        /* If BasicRate is set, the MSB bit should be set to 1 */
        if (BasicRateMask & (1<<index))
            MsbFlag = 0x80;
        else
            MsbFlag = 0;

        *pos++ = (u8)(gBitRates[index]/5)| MsbFlag;
        rate_ie[1]++;
	
    }
    rate_ie[0] = mlme_eid;
}

/**
 *  @ rate_ie: Rate IE, including ID, length, and data.
 */
u32 BitRate_IE2BitMask(const u8 *rate_ie)
{
    u32 index, bit, BitRateMask=0;
    const u8 *pos;

    pos = &rate_ie[2];
    for(index=0; index<rate_ie[1]; index++)
    {
        for(bit=0; bit<sizeof(gBitRates)/sizeof(u16); bit++)
        {
            if (((u16)(*pos&0x7F)*5) != gBitRates[bit])
                continue;
            BitRateMask |= (1<<bit);
            break;            
        }
    }
    return BitRateMask;
}




