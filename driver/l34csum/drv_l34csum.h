#ifndef _DRV_PBUF_H_
#define _DRV_PBUF_H_

void drv_l34_init_checksum();
u32 drv_l34_cal_data_checksum(u32 address,u16 len);
u32 drv_l34_cal_pesudo_checksum(u32 ip,u8 ipLen,u8 l4_pro,u16 l4_len);


#endif /* _DRV_PBUF_H_ */

