#include <config.h>
#include <bsp/irq.h>
#include <rtc/drv_rtc.h>
#include <lib/ssv_lib.h>
#include "types.h"
#include "cli.h"

#define RTC_FPGA_TEST 0

#if ( RTC_FPGA_TEST==1 )
typedef u32 (*CliCmdRtcTestFunc)(s32, char **);
typedef struct CLICmdRtcTest_st {
	char *Cmd;
	CliCmdRtcTestFunc CmdRtcTestHandler;
} CLICmdRtcTest, *PCLICmdRtcTest;
static const CLICmdRtcTest gCliCmdRtcTestTable[];

u32 testCmdTableSize;
u8 rtcSecIntStatus;
u8 rtcAlarmIntStatus;

static void hwrtc_sec_handler(void *m_data) {
	rtcSecIntStatus = 1;
	//log_printf("%s(): Hardware RTC %u finish !!\n", __FUNCTION__, (u32) m_data);
}

static void hwrtc_alarm_handler(void *m_data) {
	rtcAlarmIntStatus = 1;
	//log_printf("%s(): Hardware DMA %u finish !!\n", __FUNCTION__, (u32) m_data);
}
#endif

#if ( RTC_FPGA_TEST==1 )
static u32 CLICmdRtcTestSecondCounter(s32 argc, char *argv[]) {
	// Block for 500ms.
	//const portTickType xDelay = 500 / portTICK_RATE_MS;
	u32 o_time,n_time;

	log_printf("Hardware RTC portTICK_RATE_MS [%d] !!\n", portTICK_RATE_MS);

	//reset rtc
	drv_rtc_init();

	drv_rtc_set_source(RTC_SOURCE_PRESCALER);

	drv_rtc_en(RTC_ENABLE);

	drv_rtc_getrealtime(&o_time);
	//delay 1100 ms
	vTaskDelay(1000 / portTICK_RATE_MS);
	//vTaskDelay(1 / portTICK_RATE_MS);
	drv_rtc_getrealtime(&n_time);

	drv_rtc_en(RTC_DISABLE);

	log_printf("Hardware RTC o_time [%d] o_time [%d] !!\n", o_time, n_time);

	if (n_time-o_time >=1  )
	{
		return -1;
	}

	return 0;

}

static u32 CLICmdRtcTestSecIntMask(s32 argc, char *argv[]) {

	//reset rtc
	drv_rtc_init();

	//mask int , should be not toggle int flag
	drv_rtc_set_sec_interrupt_mask();
	drv_rtc_clear_sec_interrupt_pending();

	rtcSecIntStatus = 0;

	drv_rtc_set_sec_interrupt_handle(hwrtc_sec_handler,NULL);

	drv_rtc_set_source(RTC_SOURCE_PRESCALER);

	drv_rtc_en(RTC_ENABLE);

	//delay 1100 ms
	//vTaskDelay(1100 / portTICK_RATE_MS);
		vTaskDelay(1 / portTICK_RATE_MS);

	drv_rtc_en(RTC_DISABLE);

		if ( rtcSecIntStatus == 1 )
		{
			return -1;
		}

		//reset rtc
			drv_rtc_init();

			//mask int , should be not toggle int flag
			drv_rtc_clear_sec_interrupt_mask();
			drv_rtc_clear_sec_interrupt_pending();

			rtcSecIntStatus = 0;
			rtcAlarmIntStatus = 0;

			drv_rtc_set_source(RTC_SOURCE_PRESCALER);

			drv_rtc_en(RTC_ENABLE);

			//delay 1100 ms
			//vTaskDelay(1100 / portTICK_RATE_MS);
				vTaskDelay(1 / portTICK_RATE_MS);

			drv_rtc_en(RTC_DISABLE);

			//if (drv_dma_get_interrupt_status(DMA0)!=DMA_INT_PEND)
			if ( rtcSecIntStatus != 1 )
			{
				return -1;
			}

			drv_rtc_set_sec_interrupt_handle(NULL,NULL);

			drv_rtc_clear_sec_interrupt_mask();
			drv_rtc_clear_sec_interrupt_pending();

			return 0;
}

static u32 CLICmdRtcTestAlarmIntMask(s32 argc, char *argv[]) {

	//reset rtc
	drv_rtc_init();

	//mask int , should be not toggle int flag
	drv_rtc_set_alarm_interrupt_mask();
	drv_rtc_clear_alarm_interrupt_pending();

	rtcAlarmIntStatus = 0;

	drv_rtc_set_alarm_interrupt_handle(hwrtc_alarm_handler,NULL);

	drv_rtc_set_source(RTC_SOURCE_PRESCALER);

	//set alarm to 5 sec
	drv_rtc_setalarmtime(5);

	drv_rtc_en(RTC_ENABLE);

	//delay 5100 ms
	//vTaskDelay(5100 / portTICK_RATE_MS);
	vTaskDelay(5 / portTICK_RATE_MS);

	drv_rtc_en(RTC_DISABLE);

		if ( rtcAlarmIntStatus == 1 )
		{
			return -1;
		}

		//reset rtc
			drv_rtc_init();

			//mask int , should be not toggle int flag
			drv_rtc_clear_alarm_interrupt_mask();
			drv_rtc_clear_alarm_interrupt_pending();

			rtcSecIntStatus = 0;
			rtcAlarmIntStatus = 0;

			drv_rtc_set_source(RTC_SOURCE_PRESCALER);

			//set alarm to 5 sec
			drv_rtc_setalarmtime(5);

			drv_rtc_en(RTC_ENABLE);

			//delay 5100 ms
			//vTaskDelay(5100 / portTICK_RATE_MS);
				vTaskDelay(5 / portTICK_RATE_MS);


			drv_rtc_en(RTC_DISABLE);

			//if (drv_dma_get_interrupt_status(DMA0)!=DMA_INT_PEND)
			if ( rtcAlarmIntStatus != 1 )
			{
				return -1;
			}

			drv_rtc_set_alarm_interrupt_handle(NULL,NULL);

			drv_rtc_clear_alarm_interrupt_mask();
			drv_rtc_clear_alarm_interrupt_pending();

			return 0;
}

static u32 CLICmdRtcTestRam(s32 argc, char *argv[]) {
	u32 *pStartPointer = drv_rtc_get_ramaddress();
	u32 ramSize = drv_rtc_get_ramsize();
	 u32 i;

	for (  i=0;i <ramSize/sizeof(u32);i++)
	{
		*pStartPointer++ = i;
	}

	pStartPointer = drv_rtc_get_ramaddress();

	for (  i=0;i <ramSize/sizeof(u32);i++)
		{
			if (*pStartPointer++ != i  )
			{
				return -1;
			}
		}

	return 1;
}

static u32 CLICmdRtcTestAll(s32 argc, char *argv[]) {
	int i = 0, res = 0;

	for (i = 1; i < testCmdTableSize; i++) {
		if (gCliCmdRtcTestTable[i].CmdRtcTestHandler != NULL) {
			res = gCliCmdRtcTestTable[i].CmdRtcTestHandler(argc, argv);
			if (res == 0) {
				log_printf("Execute Test [%s] Success\n",
						gCliCmdRtcTestTable[i].Cmd);
			} else {
				log_printf("Execute Test [%s] Fail\n", gCliCmdRtcTestTable[i].Cmd);
				return res;
			}
		}
	}
	return res;
}

#endif

/*---------------------------------- CMDs -----------------------------------*/
#if ( RTC_FPGA_TEST==1 )
static const CLICmdRtcTest gCliCmdRtcTestTable[] =
{
		{"All Rtc test",CLICmdRtcTestAll },
		{"RTC second counter" ,CLICmdRtcTestSecondCounter},
		{"RTC second int" ,CLICmdRtcTestSecIntMask},
		{"RTC alarm int" ,CLICmdRtcTestAlarmIntMask},
		{"RTC Ram" ,CLICmdRtcTestRam},
};
#endif

void _Cmd_HwRTC(s32 argc, char *argv[]) {
#if ( RTC_FPGA_TEST==1 )
	u32 commandIndex;
#endif
	s32 res;
	//void *m_data;

	/**
	 * Usage:  hw-dma test [?]
	 */

	if (argc != 3) {
		log_printf("Invalid hw-rtc command!!\n");
		return;
	}

#if ( RTC_FPGA_TEST==1 )
	if (strcmp(argv[1], "test") == 0) {
		commandIndex = (u32) ssv6xxx_atoi(argv[2]);

		testCmdTableSize = (sizeof(gCliCmdRtcTestTable)
				/ sizeof(gCliCmdRtcTestTable[0]));

		/* Dispatch command */
		if (commandIndex < testCmdTableSize) {
			if (gCliCmdRtcTestTable[commandIndex].Cmd != NULL) {
				res = gCliCmdRtcTestTable[commandIndex].CmdRtcTestHandler(argc,
						argv);
				if (res == 0) {
					log_printf("Execute Test [%s] Success\n",
							gCliCmdRtcTestTable[commandIndex].Cmd);
				} else {
					log_printf("Execute Test [%s] Fail\n",
							gCliCmdRtcTestTable[commandIndex].Cmd);
				}
			} else {
				log_printf("Command not found!!\n");
			}
		} else {
			log_printf("\nCommand index out of boundary [%d]!!\n",
					testCmdTableSize);
		}
	} else
#endif
	{
		log_printf("Invalid dma cli command !!\n");
		return;
	}
}
