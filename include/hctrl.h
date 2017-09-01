#ifndef _HCTRL_H_
#define _HCTRL_H_

#define SSV6200_ID_AC_BK_OUT_QUEUE          8
#define SSV6200_ID_AC_BE_OUT_QUEUE          15
#define SSV6200_ID_AC_VI_OUT_QUEUE          16
#define SSV6200_ID_AC_VO_OUT_QUEUE          16
#define SSV6200_ID_MANAGER_QUEUE            8

#define SSV6200_ID_TX_THRESHOLD         31    //64 
#define SSV6200_ID_RX_THRESHOLD         31    //64 

#define SSV6200_PAGE_TX_THRESHOLD       61    //128 
#define SSV6200_PAGE_RX_THRESHOLD       61    //128 

struct ssv6xxx_hci_txq_info {
	u32 tx_use_page:8;
    u32 tx_use_id:6;
    u32 txq0_size:4;
	u32 txq1_size:4;
	u32 txq2_size:5;
	u32 txq3_size:5;
};


struct ssv6xxx_hci_txq_info2 {
	u32 tx_use_page:9;
    u32 tx_use_id:8;
	u32 txq4_size:4;
    u32 rsvd:11;
};

#define MMU_PAGE_SIZE	


#endif /* _HCTRL_H */

