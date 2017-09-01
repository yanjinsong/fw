#ifndef _UTIL_H_
#define _UTIL_H_

extern u16 gBitRates[12];
void BitRate_BitMask2IE(u32 RateMask, u32 BasicRateMask, u8 *rate_ie,u8 mlme_eid);
u32 BitRate_IE2BitMask(const u8 *rate_ie);

#endif /* _UTIL_H_ */

