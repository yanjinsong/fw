#ifndef _DRV_AMPDU_H_
#define _DRV_AMPDU_H_

#include <regs.h>
#include <uart/drv_uart.h>



// parameter
//First rate retry time
#define AMPDU_MAIN_TRY      (3)         
//For most n mode rate but probe frame for b mode rate to n mode rate when preallocate probe frame is not enable
#define AMPDU_FW_MAX_RETRY      (7)         
//For b mode rate to probe n mode rate in less retry times when preallocate probe frame is not enable
#define AMPDU_FW_MAX_LESS_RETRY      (4)    
#define AMPDU_FW_retry_time_us           (400)

#define AMPDU_RX_BUF_SIZE_DEFAULT			    64

#define RX_AGG_RX_BA_MAX_STATION                1
#define RX_AGG_RX_BA_MAX_SESSIONS				8

// macro
#define us_timer1_int_sts    (0x1 << TU1_TM_INT_STS_DONE_SFT)
#define us_timer1_int_mask   (0x1 << TU1_TM_INT_MASK_SFT)
#define us_timer1_stop(tmr)  ((tmr)->TMR_CTRL = (us_timer1_int_sts))


#define AMPDU_SIGNATURE	             (0x4E)
#define AMPDU_DELIMITER_LEN          (4)
#define AMPDU_FCS_LEN                (4)
#define AMPDU_RESERVED_LEN           (3)

enum AMPDU_OPT
{
    AMPDU_TX_OPT_ENABLE,
    AMPDU_TX_OPT_SET_LAST_TRY_BMODE,
    AMPDU_TX_OPT_SET_LAST_BMODE_RATE,
    AMPDU_TX_OPT_SET_LAST_BMODE_RETRY,
    AMPDU_TX_OPT_SET_BLOCK_NON_NMODE,
    AMPDU_TX_OPT_SET_RETRY_MAX,
    AMPDU_TX_OPT_SET_MAIN_TRY,
    AMPDU_RX_OPT_ENABLE,
    AMPDU_RX_OPT_BUF_SIZE,
};

PACK (struct AMPDU_DELIMITER_st
{
    u16			reserved:4; 	//0-3
    u16			length:12; 		//4-15
    u8			crc;
    u8			signature;
} )

typedef struct AMPDU_DELIMITER_st AMPDU_DELIMITER;

void irq_us_timer1_handler(void *m_data);
bool ampdu_tx_report_handler (u32 rx_data);
bool ampdu_rx_ctrl_handler (u32 rx_data);
void ampdu_dump_retry_list(void);
void ampdu_show_ampdu_mib (void);
void ampdu_reset_ampdu_mib (void);
void ampdu_dump_retry_list(void);
void ampdu_tx_set_options(u16 mode, u16 value);

#endif /* _DRV_AMPDU_H_ */
