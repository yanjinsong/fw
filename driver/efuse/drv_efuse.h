#ifndef _SSV_EFUSE_H_
#define _SSV_EFUSE_H_

#include <regs.h>

struct efuse_map {
    u8 offset;
    u8 byte_cnts;
    u16 value;
};

enum efuse_data_item {
   EFUSE_R_CALIBRAION_RESULT = 1,
   EFUSE_SAR_RESULT,
   EFUSE_MAC,
   EFUSE_CRYSTAL_FREQUECY_OFFSET,
   //EFUSE_IQ_CALIBRAION_RESULT,
   EFUSE_TX_POWER_INDEX_1,
   EFUSE_TX_POWER_INDEX_2,
   EFUSE_CHIP_IDENTITY
};

struct efuse_cfg {
   u8 r_calbration_result;
   u8 sar_result;
   u8 mac[8];
   u8 crystal_frequecy_offse;
   //u8 dc_calbration_result;
   //u16 iq_calbration_result;
   u8 tx_power_index_1;
   u8 tx_power_index_2;
   u8 chip_identity;
};

void efuse_read_all_map();

int get_efuse_capacity_size();

int get_efuse_available_size();
void write_efuse_item(u8 idx,u8 *value);
// Return 1 success 0 fail
u8 read_efuse_item(u8 idx,u8 *value);
u8 write_to_efuse();

#endif

