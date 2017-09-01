#ifndef _SSV_LIB_H_
#define _SSV_LIB_H_

#include <config.h>

#if (CONFIG_SIM_PLATFORM == 0)
#include <stdarg.h>
#endif
#define SW_QUEUE_DEBUG 0
#if(SW_QUEUE_DEBUG==1)
#define DQ_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define DQ_LOG(fmt, ...)
#endif

struct list_q {
    struct list_q   *next;
    struct list_q   *prev;
    unsigned int    qlen;
};

struct data_packet{
    struct list_q q;
    void *pbuf;
    s32 next_hw; //next hw module
#if(DATA_QUEUE_DEBUG==1)
    u8 idx;
#endif
};

struct data_queue{
    struct list_q qHead;
    u8 qID;

    bool out_lock; // FALSE: unlock, TRUE:lock
    bool int_lock; // FALSE: unlock, TRUE:lock
    bool out_auto_lock;// FALSE: disable, TRUE:enable
    u8 out_auto_lock_count;

    u32 count; //Monitor if the traffic is running or not
    u32 out_last_lock_time; //This variable is available when the out_lock is TRUE
    //u32 int_last_lock_time; //This variable is available when the int_lock is TRUE
    u8  hpPackets; // The amonut of hgigh priority packets in this queue
    u16 max_queue_len;


};

//Now WLAN_MAX_STA=2
//tx_dq_head[0] is for wsid 0
//tx_dq_head[1] is for wsid 1
//tx_dq_head[2] is for all managerment and broadcast  frames

enum{
    TX_DATA_QUEUE_0=0,
    TX_DATA_QUEUE_1=1,
    TX_DATA_QUEUE_2=2,
    LAST_TX_DATA_QUEUE=TX_DATA_QUEUE_2,
    MAX_TX_DATA_QUEUE=(LAST_TX_DATA_QUEUE+1),
}EN_TX_DATA_QUEUE_ID;

//No matter rx data  or events, all clients share the same sw rx queue
enum{
    RX_DATA_QUEUE_0=0,
    LAST_RX_DATA_QUEUE=RX_DATA_QUEUE_0,
    MAX_RX_DATA_QUEUE=(LAST_RX_DATA_QUEUE+1),
}EN_RX_DATA_QUEUE_ID;

extern struct data_queue tx_dq_head[MAX_TX_DATA_QUEUE];
extern struct data_queue rx_dq_head[MAX_RX_DATA_QUEUE];
extern struct data_queue fdq_head;



typedef enum{
    TX_SW_QUEUE=0,
    RX_SW_QUEUE=1,
}EN_SW_QUEUE;

//#define IS_TX_DATA_QUEUE_INTPUT_LAST_LOCK_TIME(ID) (tx_dq_head[ID].int_last_lock_time)
#define IS_TX_DATA_QUEUE_INTPUT_LOCK(ID) (tx_dq_head[ID].int_lock)
#define IS_TX_DATA_QUEUE_INTPUT_UNLOCK(ID) (!tx_dq_head[ID].int_lock)
#define IS_TX_DATA_QUEUE_OUTPUT_LAST_LOCK_TIME(ID) (tx_dq_head[ID].out_last_lock_time)
#define IS_TX_DATA_QUEUE_OUTPUT_LOCK(ID) (tx_dq_head[ID].out_lock)
#define IS_TX_DATA_QUEUE_OUTPUT_AUTO_LOCK(ID) (tx_dq_head[ID].out_auto_lock)
#define IS_TX_DATA_QUEUE_OUTPUT_UNLOCK(ID) (!tx_dq_head[ID].out_lock)
#define IS_TX_DATA_QUEUE_EMPTY(ID) ((0==tx_dq_head[ID].qHead.qlen)?TRUE:FALSE)
#define IS_TX_DATA_QUEUE_COUNT(ID) (tx_dq_head[ID].count)
#define GET_NUMBER_OF_HIGH_PRIORITY_PACKET(ID) (tx_dq_head[ID].hpPackets)
#define MAX_TX_DATA_QUEUE_LEN(ID) (tx_dq_head[ID].max_queue_len)

#define IS_RX_DATA_QUEUE_INTPUT_LOCK() (rx_dq_head[RX_DATA_QUEUE_0].int_lock)
#define IS_RX_DATA_QUEUE_INTPUT_UNLOCK() (!rx_dq_head[RX_DATA_QUEUE_0].int_lock)
#define IS_RX_DATA_QUEUE_OUTPUT_LOCK() (rx_dq_head[RX_DATA_QUEUE_0].out_lock)
#define IS_RX_DATA_QUEUE_OUTPUT_AUTO_LOCK() (rx_dq_head[RX_DATA_QUEUE_0].out_auto_lock)
#define IS_RX_DATA_QUEUE_OUTPUT_UNLOCK() (!rx_dq_head[RX_DATA_QUEUE_0].out_lock)
#define IS_RX_DATA_QUEUE_EMPTY() ((0==rx_dq_head[RX_DATA_QUEUE_0].qHead.qlen)?TRUE:FALSE)
#define IS_RX_DATA_QUEUE_COUNT() (rx_dq_head[RX_DATA_QUEUE_0].count)

#define MAX_RX_DATA_QUEUE_LEN() (rx_dq_head[RX_DATA_QUEUE_0].max_queue_len)

LIB_APIs s32 ssv6xxx_SWQInit(void);
LIB_APIs s32 ssv6xxx_SWQWaittingSize(s32 qID,EN_SW_QUEUE q);
LIB_APIs inline s32 ssv6xxx_SWFreeQWaittingSize(void);
// Enqueue a new packet to the tail of queue
LIB_APIs s32 ssv6xxx_SWQEnqueue(s32 qID, EN_SW_QUEUE q, void *pbuf, s32 next_hw);
// Insert a new packet to the head of queue
LIB_APIs s32 ssv6xxx_SWQInsert(s32 qID, EN_SW_QUEUE q, void *pbuf, s32 next_hw);
// Dequeue a new packet from the head of queue
LIB_APIs s32 ssv6xxx_SWQDequeue(s32 qID, EN_SW_QUEUE q, void **pbuf, s32 *next_hw);
LIB_APIs s32 ssv6xxx_SWQIntLock(s32 qID, EN_SW_QUEUE q);
LIB_APIs s32 ssv6xxx_SWQIntUnLock(s32 qID, EN_SW_QUEUE q);
LIB_APIs s32 ssv6xxx_SWQOutLock(s32 qID, EN_SW_QUEUE q);
LIB_APIs s32 ssv6xxx_SWQOutUnLock(s32 qID, EN_SW_QUEUE q);
LIB_APIs s32 ssv6xxx_SWQOutConUnLock(s32 qID, EN_SW_QUEUE q, u8 count);
LIB_APIs s32 ssv6xxx_SWQFlush(s32 qID, EN_SW_QUEUE q, void (*flush_fun)(void *p));
LIB_APIs s32 flush_sw_queue(s32 qID, EN_SW_QUEUE q);
LIB_APIs s32 clear_sw_queue(s32 qID, EN_SW_QUEUE q);
LIB_APIs void ssv6xxx_sw_queue_status(void);
LIB_APIs inline s32 wake_up_task(OsMsgQ msgevq);
LIB_APIs s32 ENG_MBOX_NEXT(u32 pktmsg);
LIB_APIs s32 ENG_MBOX_SEND(u32 hw_number,u32 pktmsg);
LIB_APIs u32 ssv6xxx_atoi_base( const char *s, u32 base );
LIB_APIs s32 ssv6xxx_atoi( const char *s );
LIB_APIs s32 ssv6xxx_isalpha(s32 c);
LIB_APIs s32 ssv6xxx_str_tolower(char *s);
LIB_APIs s32 ssv6xxx_str_toupper(char *s);

LIB_APIs s32 ssv6xxx_strrpos(const char *str, char delimiter);

#if (CONFIG_SIM_PLATFORM == 1)
u64 ssv6xxx_64atoi( char *s );
#endif

#if (CONFIG_SIM_PLATFORM == 0)
LIB_APIs char toupper(char ch);
LIB_APIs char tolower(char ch);
LIB_APIs s32 printf(const char *format, ...);
LIB_APIs s32 strcmp( const char *s0, const char *s1 );
LIB_APIs char *strcat(char *s, const char *append);
LIB_APIs char *strncpy(char *dst, const char *src, size_t n);
LIB_APIs size_t strlen(const char *s);


LIB_APIs void *memset(void *s, s32 c, size_t n);
LIB_APIs void *memcpy(void *dest, const void *src, size_t n);
LIB_APIs s32 memcmp(const void *s1, const void *s2, size_t n);

LIB_APIs void *malloc(size_t size);
LIB_APIs void free(void *ptr);

LIB_APIs s32 vsnprintf(char *out, size_t size, const char *format, va_list args);
LIB_APIs s32 snprintf(char *out, size_t size, const char *format, ...);
LIB_APIs s32 printf(const char *format, ...);
LIB_APIs s32 putstr(const char *str, size_t size);
LIB_APIs s32 snputstr(char *out, size_t size, const char *str, size_t len);

LIB_APIs s32 fatal_printf(const char *format, ...);
#else
#define fatal_printf		printf
#endif

#if (CLI_ENABLE==1 && CONFIG_SIM_PLATFORM==0)
LIB_APIs s32 kbhit(void);
LIB_APIs s32 getch(void);
LIB_APIs s32 putchar(s32 ch);
#endif
LIB_APIs void ssv6xxx_hw_queue_status(void);
LIB_APIs void ssv6xxx_queue_status(void);
LIB_APIs void ssv6xxx_raw_dump(char *data, s32 len);
LIB_APIs void ssv6xxx_msg_queue_status(void);
// with_addr : (true) -> will print address head "xxxxxxxx : " in begining of each line
// addr_radix: 10 (digial)  -> "00000171 : "
//		     : 16 (hex)		-> "000000ab : "
// line_cols : 8, 10, 16, -1 (just print all in one line)
// radix     : 10 (digital) ->  171 (max num is 255)
//			   16 (hex)		-> 0xab
// log_level : log level  pass to LOG_PRINTF_LM()
// log_module: log module pass to LOG_PRINTF_LM()
//
LIB_APIs bool ssv6xxx_raw_dump_ex(char *data, s32 len, bool with_addr, u8 addr_radix, char line_cols, u8 radix, u32 log_level, u32 log_module);

LIB_APIs void hex_dump(const void *addr, u32 size);

LIB_APIs void halt(void);



void _hexdump(const char *title, const u8 *buf,
                             size_t len);




void _packetdump(const char *title, const u8 *buf,
                             size_t len);


enum pbuf_location
{
    EN_LOC_RECEIVE=0,
    EN_LOC_GET_RATE=1,
    EN_LOC_FILL_DUR=2,
    EN_LOC_WAIT_BA=3,
    EN_LOC_ENQUE=4,
    EN_LOC_DEQUE=5,
    EN_LOC_ALLOCATE_PBUF=6,
    EN_LOC_RX_MGMT_HANDLE=7,
    EN_LOC_RX_BEACON=8,
    EN_LOC_RX_PROBE_RSP=9,
    EN_LOC_LEAVE_FW=10,
    EN_LOC_NONE=11,
};

struct pbuf_trace
{
    u8 InFW:1;
    u8 NextHW:7;
    enum pbuf_location loc;
};


s32 pt_init(void);
s32 pt(void *_p, u8 next_hw, enum pbuf_location loc);
int is_pbuf_in_fw(void *_p);
void dump_pt(void);

#endif /* _SSV_LIB_H_ */

