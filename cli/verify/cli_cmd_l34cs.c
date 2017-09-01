#include <config.h>
#include <stdarg.h>
#include <bsp/irq.h>
#include <pbuf.h>
#include <timer/drv_timer.h>
#include <mbox/drv_mbox.h>
#include <timer/drv_timer.h>
#include <verify/cli_cmd_rtc.h>
#include <l34csum/drv_l34csum.h>
#include <verify/cli_cmd_l34cs.h>
#include "cli.h"
u32 cpu_cal_muldata_csum(u32 csum,u8 *pattern, u16 len)
{

    u16 val= 0;
	s16 idx = 0;
	u32 sum = csum;


	while (len > 1)
	{
		val = (pattern[idx])<<8 | pattern[idx+1];
		sum += val;
		idx += 2;
		len -= 2;
	}

	if (len > 0)
	{
		val = (pattern[idx]<<8) | 0x00;

		sum += val;
	}
	sum = (sum>>16) + (sum&0x0000FFFF);
    sum = (sum>>16) + (sum&0x0000FFFF);
	log_printf("--cal data csum =%x\n",sum);
	return sum;
}

u32 cpu_cal_data_csum(u8 *pattern, u16 len)
{
	u16 val= 0;
	s16 idx = 0;
	u32 sum = 0;


	while (len > 1)
	{
		val = (pattern[idx])<<8 | pattern[idx+1];
		sum += val;
		idx += 2;
		len -= 2;
	}

	if (len > 0)
	{
		val = (pattern[idx]<<8) | 0x00;

		sum += val;
	}
	sum = (sum>>16) + (sum&0x0000FFFF);
    sum = (sum>>16) + (sum&0x0000FFFF);
	log_printf("--cal data csum =%x\n",sum);


	return sum;

}

u32 cpu_cal_pseudo_csum(u32 csum,  u16 l4_len, u16 l4_proto,u8 iplen, u8 *src_ip, u8 *dst_ip)
{
	 s16 idx;
	 u32 l4_csum=0;
	 u8 *ptr;
     u32 val= 0;

	 if(l4_proto ==0x01)
		 return ~csum;
    /* add pseudo header check sum to csum */

    log_printf("src_ip =%x\n",src_ip);
	log_printf("dst_ip =%x\n",dst_ip);
    log_printf("data source checksum =%x\n",csum);
	for(idx=0, ptr=src_ip; idx<iplen; idx+=2)
    {
        val = (ptr[idx]<<8) | ptr[idx+1];
        log_printf("ip value=:%x\n",val);
        csum += val;
        //csum += ((ptr[idx]<<8) | ptr[idx+1]);
    }

    //log_printf("source ip checksum = %x\n",csum);
    for(idx=0; ptr=dst_ip, idx<iplen; idx+=2)
    {
        val = (ptr[idx]<<8) | ptr[idx+1];
        log_printf("ip value=:%x\n",val);
        //csum += ((ptr[idx]<<8) | ptr[idx+1]);
        csum += val;
    }
    //log_printf("dst checksum = %x",csum);
    csum += l4_len;
    csum += l4_proto;
	log_printf("l4_len=%x,L4_pro =%x\n\r");

    csum = ((csum>>16) + (csum & 0x0000FFFF));

    csum = ((csum>>16)+(csum & 0x0000FFFF));

	return (~csum&0x0000FFFF);

}



#define RAND_MAX_NUM  2048
#define MAC_HEADER_LEN 0xE
#define RAND_MAX_STARTOFF 0x04
#define RAND_MAX_MULTIPLE 0x10

#define IP4_HEADER_LEN 0x20
#define IP4_ADDR_LEN 0x04
#define IP6_ADDR_LEN 0x10
#define pesudoNum  0x02

void _Cmd_HwCheckSum( s32 argc, char *argv[])
{

    u32  *randArray1 =NULL;
    u8 *sourceIp = NULL;
    u16 randLen =0;
    u32 rawCsum1 = 0 ,rawCsum2 = 0 ,cpuRawCsum = 0;
	int j=0,i=0,testNum = 0;
    US_TIMER0->TMR_CTRL = 0x1ffff;
    //rand raw data

    randArray1 = PBUF_MAlloc_Raw(RAND_MAX_NUM,0, SOC_PBUF);

    while(1)
    {
    	testNum++;
        log_printf("*************test %d**************\n\r",testNum);
        u8 startAddr = US_TIMER0->TMR_CTRL % 0x02;

        randLen = (US_TIMER0->TMR_CTRL % (RAND_MAX_NUM));

        if(randLen == 0)
        {
            randLen =2;
        }

        log_printf("RAND DATA Len=0x%04x\n\r",randLen);

    	j=0;
    	//startAddr =2;
        //randLen = 7;
        startAddr = 0;
        if(randLen>2)
        {
            if(startAddr == 1)
              startAddr = 2;
        }

    	for(i=startAddr;i<randLen;i++)
    	{
    		if(j>15)
    		{
    			log_printf("\n");
    			j=0;
    		}

            randArray1[i] = US_TIMER0->TMR_CTRL %255+1;
            log_printf("%02x",randArray1[i]);
            log_printf("%c",0x20);
    		j++;

        }
        log_printf("\n\r");

        cpuRawCsum = cpu_cal_data_csum(&randArray1[startAddr],randLen-startAddr);

        log_printf("CPU check sum =%04x\n\r",(~cpuRawCsum)& 0x0000ffff);
        drv_l34_init_checksum();


        rawCsum1 = drv_l34_cal_data_checksum(&randArray1[startAddr],randLen-startAddr);
        log_printf("packet buffer check sum =%04x\n\r",rawCsum1);


        if( ((~cpuRawCsum)& 0x0000ffff) != rawCsum1 )
        {
            log_printf("%dth test,check sum for packet buffer error,\n\r",testNum);
             PBUF_MFree(sourceIp);
             PBUF_MFree(randArray1);
            return;
        }



        //for packetbuffer
        log_printf("********for  pesudo data *********************\n");
        u16 L4_pro = 0;
        u16 L4_Len = 0;
        u32 singlePCsum1 = 0;
        u32 singlePCsum2 =0;
        u8  proArray[pesudoNum]= {0x06,0x11};
        u8  IPLenArray[2] ={IP4_ADDR_LEN,IP6_ADDR_LEN};
		u8  hLenArray[2] = {20,40};

        u8 IpVersionIndex =  US_TIMER0->TMR_CTRL %2;

        u8 tempIpLen = IPLenArray[IpVersionIndex];


        sourceIp = PBUF_MAlloc_Raw(tempIpLen*2,0, SOC_PBUF);


        for(i=0;i<tempIpLen*2;i++)
        {
            if(j>15)
    		{
    			log_printf("\n");
    			j=0;
    		}
            sourceIp[i] = US_TIMER0->TMR_CTRL %255+1;
            log_printf("%02x",sourceIp[i]);
            log_printf("%c",0x20);
        }
        u8 temp = US_TIMER0->TMR_CTRL%pesudoNum;
        L4_pro = proArray[temp];
		L4_Len = hLenArray[temp];
        singlePCsum1 = cpu_cal_pseudo_csum(cpuRawCsum,L4_Len,L4_pro,tempIpLen,sourceIp,&sourceIp[tempIpLen]);
        log_printf("CPU pesudo check sum =%04x\n\r",singlePCsum1);
		log_printf("l4_len= %x,l4_pro=%x\n\r",L4_Len,L4_pro);
        singlePCsum2 = drv_l34_cal_pesudo_checksum(sourceIp,tempIpLen*2,L4_pro,L4_Len);
        log_printf("pesudo check sum =%04x\n\r",singlePCsum2);
        //mailbox_mng_rls(sourceIp);

        if(singlePCsum1 != singlePCsum2)
        {
             log_printf("%dth test,check sum for pesudo error,\n\r",testNum);
             PBUF_MFree(sourceIp);
             PBUF_MFree(randArray1);
            return;
        }
        //rand multiple data
        log_printf("********for multiple raw data*********************\n");
        //u16 j=0;
        drv_l34_init_checksum();
        u16 tempLen = 0;
        u16 remainLen = randLen-startAddr;
        u16 tempStartAddr = startAddr;
        cpuRawCsum = 0;
        u16 val = 0;

        j=0;
        if(remainLen >5)
        {
            while(true)
            {
                if(remainLen<5)
                    break;
                j++;
                log_printf("------------%dth--------------\n",j);
                //log_printf("startAddr =%08x\n",tempStartAddr);
                tempLen = (US_TIMER0->TMR_CTRL % remainLen+1);
                //tempLen = 2;
                log_printf("len=%08x\n",tempLen);
                cpuRawCsum = cpu_cal_muldata_csum(cpuRawCsum,&randArray1[tempStartAddr],tempLen);

                log_printf("cpu raw check sum=%04x\n",(~cpuRawCsum&0x0000FFFF));
                rawCsum1 = drv_l34_cal_data_checksum(&randArray1[tempStartAddr],tempLen);
                log_printf("raw check sum=%04x\n",rawCsum1);
                tempStartAddr = tempStartAddr+tempLen+(tempLen%2);
                if(tempStartAddr <(randLen-startAddr));
                    break;
                remainLen = randLen - tempStartAddr;

            }

            if((~cpuRawCsum&0x0000FFFF) != rawCsum1)
            {
                 log_printf("%dth test,check sum for multiple text error,\n\r",testNum);
                 PBUF_MFree(sourceIp);
                 PBUF_MFree(randArray1);
                return;
            }

        log_printf("********for multiple pesudo data *********************\n");
        u32 mulPCsum1,mulPCsum2;
        mulPCsum1 = cpu_cal_pseudo_csum(cpuRawCsum,L4_Len,L4_pro,tempIpLen,sourceIp,&sourceIp[tempIpLen]);
        log_printf("CPU multip pesudo check sum =%04x\n\r",mulPCsum1);


		mulPCsum2 = drv_l34_cal_pesudo_checksum(sourceIp,tempIpLen*2,L4_pro,L4_Len);
        log_printf("pesudo check sum =%04x\n\r",mulPCsum2);

        if(mulPCsum1 != mulPCsum2)
        {
             log_printf("%dth test,check sum for multiple pesudo error,\n\r",testNum);
             PBUF_MFree(sourceIp);
             PBUF_MFree(randArray1);
            return;
        }
        }
         PBUF_MFree(sourceIp);


    }

    PBUF_MFree(randArray1);
    return ;



}







