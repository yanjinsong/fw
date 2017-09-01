#include "drv_efuse.h"
#include <config.h>

#define EFUSE_HWSET_MAX_SIZE 256
#define EFUSE_MAX_SECTION_MAP (EFUSE_HWSET_MAX_SIZE>>3)


static u8 efuse_mapping_table[EFUSE_MAX_SECTION_MAP+1];
//Bit
static u16 efuse_real_content_len = 0;
static struct efuse_cfg ssv_efuse_config;

#define LOCAL_TEST
#ifdef LOCAL_TEST
static u32 SSV_EFUSE_BASE = 0;
static u8 memory_pool[EFUSE_MAX_SECTION_MAP] ;
#else
#define SSV_EFUSE_BASE	0xC2000108
#endif

static struct efuse_map SSV_EFUSE_ITEM_TABLE[] = {
    {4, 0, 0},
    {4, 8, 0},
    {4, 8, 0},
    {4, 48, 0},//Mac address
    {4, 8, 0},
    //{4, 12, 0},//EFUSE_IQ_CALIBRAION_RESULT
    {4, 8, 0},
    {4, 8, 0},
    {4, 8, 0},
};

#ifndef LOCAL_TEST
static void efuse_power_switch(u32 on)
{
	if(on)
        sdio_write_reg(0xc0000328,0x11);
    else
        sdio_write_reg(0xc0000328,0x1800000a);
}
#endif

void efuse_write_8(u8 *address, u8 value)
{
#ifdef LOCAL_TEST
	volatile u8 *pointer;
    pointer = (volatile u8 *)(SSV_EFUSE_BASE + address);
	if(address)
		*pointer = value | *pointer;
	else
		*pointer = value;
#else
	u8 pointer;
    pointer = (u8)(address);

	if(address)
		efuse_mapping_table[pointer] = value | efuse_mapping_table[pointer];
	else
		efuse_mapping_table[pointer] = value;
#endif
}

void efuse_write_16(u32 address, u16 value)
{
	u8 *pointer;
    pointer = (u8 *)(address);

	efuse_write_8(pointer++,(value>>0)&0xff);
	efuse_write_8(pointer++,(value>>8)&0xff);
}

void efuse_write_32(u32 address, u32 value)
{
	u8 * pointer;

    pointer = (u8 *)(address);
    efuse_write_8(pointer++,(value>>0)&0xff);
    efuse_write_8(pointer++,(value>>8)&0xff);
    efuse_write_8(pointer++,(value>>16)&0xff);
    efuse_write_8(pointer++,(value>>24)&0xff);
}

#ifdef LOCAL_TEST
void read_efuse_32(u16 _offset, u32 *pbuf)
{
	volatile u32 * pointer;

	pointer = (volatile u32 *)(SSV_EFUSE_BASE + _offset);

    *pbuf = *pointer;
}
#endif

u8 read_efuse(u8 *pbuf)
{
    u32 *pointer32;
    u16 i;


	pointer32 = (u32 *)pbuf;
#ifdef LOCAL_TEST
    read_efuse_32(0, pointer32);
#else
    *pointer32 = sdio_read_reg((u32)SSV_EFUSE_BASE);
#endif
    if (*pointer32 == 0x00) {
		return 0;
    }

    for (i=0; i<EFUSE_MAX_SECTION_MAP; i+=4 , pointer32++)
    {
#ifdef LOCAL_TEST
		read_efuse_32( i, pointer32);
#else
		*pointer32 = sdio_read_reg(SSV_EFUSE_BASE+i);
#endif
    }
    return 1;
}

#ifndef LOCAL_TEST
u8 write_to_efuse()
{
	u32 *pointer,i,*efuse_base,ret;
	//Switch on
	efuse_power_switch(1);
	efuse_base = (u32 *)SSV_EFUSE_BASE;
	pointer = (u32 *)efuse_mapping_table;
    for (i = 0; i<(EFUSE_HWSET_MAX_SIZE/32) ; i++ , pointer++, efuse_base++)
    {
		sdio_write_reg((u32)efuse_base,*pointer);
		Sleep(10);
		ret = sdio_read_reg((u32)efuse_base);
		if(*pointer != ret)
		{
			efuse_power_switch(0);
			return 0;
		}
    }
	//Switch off
	efuse_power_switch(0);
	return 1;
}
#endif

static u16 parser_efuse(u8 *pbuf, u8 *mac_addr)
{
    u8 *rtemp8,idx=0;
	u16 shift=0,i;
    efuse_real_content_len = 0;

	rtemp8 = pbuf;

    if (*rtemp8 == 0x00) {
		return efuse_real_content_len;
    }

	do
	{
		idx = (*(rtemp8) >> shift)&0xf;
		switch(idx)
		{
            // 1 byte type
			case EFUSE_R_CALIBRAION_RESULT:
			case EFUSE_CRYSTAL_FREQUECY_OFFSET:
			case EFUSE_TX_POWER_INDEX_1:
			case EFUSE_TX_POWER_INDEX_2:
			case EFUSE_CHIP_IDENTITY:
			case EFUSE_SAR_RESULT:
				if(shift)
				{
					rtemp8 ++;
					SSV_EFUSE_ITEM_TABLE[idx].value = (u16)((u8)(*((u16*)rtemp8)) & ((1<< SSV_EFUSE_ITEM_TABLE[idx].byte_cnts) - 1));
				}
				else
				{
					SSV_EFUSE_ITEM_TABLE[idx].value = (u16)((u8)(*((u16*)rtemp8) >> 4) & ((1<< SSV_EFUSE_ITEM_TABLE[idx].byte_cnts) - 1));
				}
				efuse_real_content_len += (SSV_EFUSE_ITEM_TABLE[idx].offset + SSV_EFUSE_ITEM_TABLE[idx].byte_cnts);
				break;
            //MAC address
			case EFUSE_MAC:
                if(shift)
				{
					rtemp8 ++;
					memcpy(mac_addr,rtemp8,6);
				}
				else
				{
					for(i=0;i<6;i++)
					{
						mac_addr[i] = (u16)(*((u16*)rtemp8) >> 4) & 0xff;
						rtemp8++;
					}
				}
				efuse_real_content_len += (SSV_EFUSE_ITEM_TABLE[idx].offset + SSV_EFUSE_ITEM_TABLE[idx].byte_cnts);
				break;
#if 0
            // 2 bytes type
			case EFUSE_IQ_CALIBRAION_RESULT:
				if(shift)
				{
					rtemp8 ++;
					SSV_EFUSE_ITEM_TABLE[idx].value = (u16)(*((u16*)rtemp8)) & ((1<< SSV_EFUSE_ITEM_TABLE[idx].byte_cnts) - 1);
				}
				else
				{
					SSV_EFUSE_ITEM_TABLE[idx].value = (u16)(*((u16*)rtemp8) >> 4) & ((1<< SSV_EFUSE_ITEM_TABLE[idx].byte_cnts) - 1);
				}
				efuse_real_content_len += (SSV_EFUSE_ITEM_TABLE[idx].offset + SSV_EFUSE_ITEM_TABLE[idx].byte_cnts);
				break;
#endif
			default:
                idx = 0;
				break;
		}
		shift = efuse_real_content_len % 8;
		rtemp8 = &pbuf[efuse_real_content_len / 8];
	}while(idx != 0);

	return efuse_real_content_len;
}

void write_efuse_item(u8 idx,u8 *value)
{
    u8 value8=0;
	u16 shift=0,value16=0;
    u32 value32=0;

    if((efuse_real_content_len + SSV_EFUSE_ITEM_TABLE[idx].offset + SSV_EFUSE_ITEM_TABLE[idx].byte_cnts) > EFUSE_HWSET_MAX_SIZE)
    {
        LOG_PRINTF("There is not enough space!!\n");
        return;
    }

	shift = efuse_real_content_len % 8;

	switch(idx)
	{
		case EFUSE_R_CALIBRAION_RESULT:
		case EFUSE_SAR_RESULT:
		case EFUSE_CRYSTAL_FREQUECY_OFFSET:
		case EFUSE_TX_POWER_INDEX_1:
		case EFUSE_TX_POWER_INDEX_2:
		case EFUSE_CHIP_IDENTITY:
			if(shift)
				value16 = (idx << 4) | (*value << 8);
			else
				value16 = (idx >> 0) | (*value << 4);
			efuse_write_16(efuse_real_content_len / 8,value16);
            efuse_real_content_len += (SSV_EFUSE_ITEM_TABLE[idx].offset + SSV_EFUSE_ITEM_TABLE[idx].byte_cnts);
            break;
		case EFUSE_MAC:
			if(shift)
				value8 = (idx << 4);
			else
				value8 = (idx >> 0);
            efuse_write_8((u8*)(efuse_real_content_len / 8),value8);
            efuse_real_content_len += SSV_EFUSE_ITEM_TABLE[idx].offset;

            shift = efuse_real_content_len % 8;
            if(shift)
            {
                value32 = (*(u32 *)value << 4);
                value32 = value32 | (u32)value8;
            }
            else
                value32 = (*(u32 *)value << 0);
            efuse_write_32(efuse_real_content_len / 8,value32);
            efuse_real_content_len += 32;

            if(shift)
                value += 3;
            else
                value += 4;
            shift = efuse_real_content_len % 8;
            if(shift)
                value32 = (*(u32 *)value >> 4);
            else
                value32 = (*(u32 *)value << 0);
            efuse_write_32(efuse_real_content_len / 8,value32);
            efuse_real_content_len += 16;

			break;
#if 0
        case EFUSE_IQ_CALIBRAION_RESULT:
            if(shift)
                value32 = (idx << 4) | (((*(u16 *)value&0xfff) << 8));
            else
				value32 = (idx << 0) | (((*(u16 *)value&0xfff) << 4));

            efuse_write_32(efuse_real_content_len / 8,value32);
            efuse_real_content_len += (SSV_EFUSE_ITEM_TABLE[idx].offset + SSV_EFUSE_ITEM_TABLE[idx].byte_cnts);
            break;
#endif
		default:
			break;
	}
}

u8 read_efuse_item(u8 idx,u8 *value)
{
	switch(idx)
	{
		case EFUSE_R_CALIBRAION_RESULT:
			*value = ssv_efuse_config.r_calbration_result;
			break;
		case EFUSE_SAR_RESULT:
			*value = ssv_efuse_config.sar_result;
			break;
		case EFUSE_CRYSTAL_FREQUECY_OFFSET:
			*value = ssv_efuse_config.crystal_frequecy_offse;
			break;
		case EFUSE_TX_POWER_INDEX_1:
			*value = ssv_efuse_config.tx_power_index_1;
			break;
		case EFUSE_TX_POWER_INDEX_2:
			*value = ssv_efuse_config.tx_power_index_2;
			break;
		case EFUSE_CHIP_IDENTITY:
			*value = ssv_efuse_config.chip_identity;
			break;
		case EFUSE_MAC:
			memcpy(value,ssv_efuse_config.mac,8);
			break;
#if 0
        case EFUSE_IQ_CALIBRAION_RESULT:
			*((u16 *)value) = ssv_efuse_config.iq_calbration_result;
            break;
#endif
		default:
			return 0;
	}
	return 1;
}

int get_efuse_capacity_size()
{
    return EFUSE_HWSET_MAX_SIZE;
}

int get_efuse_available_size()
{
    return (EFUSE_HWSET_MAX_SIZE - efuse_real_content_len);
}

void efuse_read_all_map()
{
	memset(efuse_mapping_table,0x00,EFUSE_MAX_SECTION_MAP+1);
	memset(&ssv_efuse_config,0x00,sizeof(struct efuse_cfg));
    read_efuse(efuse_mapping_table);
    //Parser data to ssv_efuse_config.
	parser_efuse(efuse_mapping_table,ssv_efuse_config.mac);

    // 1 byte type
    ssv_efuse_config.r_calbration_result = (u8)SSV_EFUSE_ITEM_TABLE[EFUSE_R_CALIBRAION_RESULT].value;
    ssv_efuse_config.sar_result = (u8)SSV_EFUSE_ITEM_TABLE[EFUSE_SAR_RESULT].value;
    ssv_efuse_config.crystal_frequecy_offse = (u8)SSV_EFUSE_ITEM_TABLE[EFUSE_CRYSTAL_FREQUECY_OFFSET].value;
    ssv_efuse_config.tx_power_index_1 = (u8)SSV_EFUSE_ITEM_TABLE[EFUSE_TX_POWER_INDEX_1].value;
    ssv_efuse_config.tx_power_index_2 = (u8)SSV_EFUSE_ITEM_TABLE[EFUSE_TX_POWER_INDEX_2].value;
#if 0
    // 2 bytes type
    ssv_efuse_config.iq_calbration_result = (u16)SSV_EFUSE_ITEM_TABLE[EFUSE_IQ_CALIBRAION_RESULT].value;
#endif
}

void efuse_initialize()
{
#ifdef LOCAL_TEST
	SSV_EFUSE_BASE = (u32)memory_pool ;
#endif
    ssv_efuse_config.r_calbration_result = 0xaa;
    ssv_efuse_config.sar_result = 0xbb;
	ssv_efuse_config.mac[0] = 0x11;
	ssv_efuse_config.mac[1] = 0x22;
	ssv_efuse_config.mac[2] = 0x33;
	ssv_efuse_config.mac[3] = 0x44;
	ssv_efuse_config.mac[4] = 0x55;
	ssv_efuse_config.mac[5] = 0x66;
	ssv_efuse_config.mac[6] = 0x00;
	ssv_efuse_config.mac[7] = 0x00;
    ssv_efuse_config.crystal_frequecy_offse = 0xcc;
    //ssv_efuse_config.iq_calbration_result = 0x789a;
    ssv_efuse_config.tx_power_index_1 = 0xee;
    ssv_efuse_config.tx_power_index_2 = 0xaa;


    write_efuse_item(EFUSE_R_CALIBRAION_RESULT,&ssv_efuse_config.r_calbration_result);
    write_efuse_item(EFUSE_SAR_RESULT,&ssv_efuse_config.sar_result);
    write_efuse_item(EFUSE_MAC,ssv_efuse_config.mac);
    write_efuse_item(EFUSE_CRYSTAL_FREQUECY_OFFSET,&ssv_efuse_config.crystal_frequecy_offse);
#if 0
    write_efuse_item(EFUSE_IQ_CALIBRAION_RESULT,(u8*)&ssv_efuse_config.iq_calbration_result);
#endif
	write_efuse_item(EFUSE_TX_POWER_INDEX_1,&ssv_efuse_config.tx_power_index_1);
    write_efuse_item(EFUSE_TX_POWER_INDEX_2,&ssv_efuse_config.tx_power_index_2);

	efuse_read_all_map();

#ifdef LOCAL_TEST
	LOG_PRINTF("@@ - %x\n",ssv_efuse_config.r_calbration_result);
    LOG_PRINTF("@@ - %x\n",ssv_efuse_config.sar_result);
    LOG_PRINTF("@@ - %x\n",ssv_efuse_config.crystal_frequecy_offse);
    //printf("@@ - %x\n",ssv_efuse_config.iq_calbration_result);
    LOG_PRINTF("@@ - %x\n",ssv_efuse_config.tx_power_index_1);
    LOG_PRINTF("@@ - %x\n",ssv_efuse_config.tx_power_index_2);
	LOG_PRINTF("@@\n");
#endif
}


