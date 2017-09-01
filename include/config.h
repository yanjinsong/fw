#ifndef _CONFIG_H_
#define _CONFIG_H_

// Common configuration should be applied to for both host simuator and device
// FW calculate checksum to check correctness of downloaded image.
#define ENABLE_FW_SELF_CHECK
#define FW_BLOCK_SIZE                   (1024)
#define FW_CHECKSUM_INIT                (0x12345678)
#define _INTERNAL_MCU_SUPPLICANT_SUPPORT_

// Get TX queue statistics in EDCA handler.
//#define ENABLE_TX_QUEUE_STATS

//#define THROUGHPUT_TEST
//HOST_SDIO_THROUGHPUT_TEST base on THROUGHPUT_TEST
//#define HOST_SDIO_THROUGHPUT_TEST

//
//#define ENABLE_CLI_CMD_DUMP_REG

//
#define ENABLE_CLI_CMD_MIB
// ------------------------- config -------------------------------
#if (defined _WIN32) || (defined __SSV_UNIX_SIM__)
#define CONFIG_SIM_PLATFORM         1
#include <sim_config.h>
#else
#define CONFIG_SIM_PLATFORM         0
#include <soc_config.h>
#endif

typedef enum {
    SSV6XXX_IQK_CFG_XTAL_26M = 0,
    SSV6XXX_IQK_CFG_XTAL_40M,
    SSV6XXX_IQK_CFG_XTAL_24M,
    SSV6XXX_IQK_CFG_XTAL_MAX,
} ssv6xxx_iqk_cfg_xtal;


//#define CONFIG_SSV_CABRIO_A			1
#define CONFIG_SSV_CABRIO_E			1
#define SSV6XXX_IQK_CFG_XTAL		SSV6XXX_IQK_CFG_XTAL_26M


//AP mode send deauth information
#define SEND_DEAUTH_COUNT 3
#define NO_NEED_TO_SEND_DEAUTH_COUNT 3

//------------------------Beacon--------------------
#define AUTO_BEACON 0

#define WATCHDOG_TIMER 100 //ms
#define HIGH_TEMPERATURE 32 //60 degree
#define LOW_TEMPERATURE 12 //0 degree

#define N_MODE_AMPDU_TX             1
#if (N_MODE_AMPDU_TX == 1)
#define N_MODE_AMPDU_RETRY_TO_B_MODE        1
#endif

#define PACKET_TRACE        0

//This is just a testing function now, it need to enable on host and fw at the same time
#define ENABLE_DYNAMIC_RX_SENSITIVE 1

#endif	/* _CONFIG_H_ */
