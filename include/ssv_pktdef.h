#ifndef _SSV_PKTDEF_H_
#define _SSV_PKTDEF_H_

#include <types.h>

/**
 *  The flag definition for c_type (command type) field of PKTInfo:
 *
 *      @ M0_TXREQ:(Data)
 *      @ M1_TXREQ(Data)
 *      @ M2_TXREQ(Data)
 *      @ M0_RXEVENT(Data)
 *      @ M2_RXEVENT(Data)
 *      @ HOST_CMD(control)
 *      @ HOST_EVENT(control)
 *
 */
#define M0_TXREQ                            0 //Mgmt frame
#define M1_TXREQ                            1 //none
#define M2_TXREQ                            2 //Data frame
#define M0_RXEVENT                          3 //Rx data
//#define M2_RXEVENT                          4 //none
#define HOST_CMD                            5
#define HOST_EVENT                          6
#define TEST_CMD                          	7




#define M0_HDR_LEN							4
#define M1_HDR_LEN							8
#define M2_HDR_LEN							16

#define RX_M0_HDR_LEN						24


/**
 *
 *  Offset Table (register):
 *
 *    c_type            hdr_len
 *  ----------     ----------
 *  M0_TXREQ         8-bytes
 *  M1_TXREQ         12-bytes
 *  M2_TXREQ         sizeof(PKT_TXInfo)
 *  M0_RXEVENT
 *  M1_RXEVENT
 *  HOST_CMD
 *  HOSt_EVENT
 *
 *
 */

#define IS_RX_PKT(_p)       ((_p)->c_type==M0_RXEVENT)
#define IS_TX_PKT(_p)       (/*((_p)->c_type>=M0_TXREQ)&&*/((_p)->c_type<=M2_TXREQ))
#define IS_TX_TEST_CMD(_p)	((_p)->c_type==TEST_CMD)
#define IS_STA_RX_BMCAST(_p)    ((*((u8 *)(_p)+(_p)->hdr_offset +  4))&0x01)

/*	WMM_Specification_1-1 : Table 14  802.1D Priority to AC mappings

	UP		Access Category
	-------------------------
	1, 2	AC_BK
	0, 3	AC_BE
	4, 5	AC_VI
	6, 7	AC_VO
*/
#define	AC_BK	 0
#define	AC_BE	 1
#define	AC_VI	 2
#define	AC_VO	 3

extern u8 ssv6xxx_get_pbuf_hdr80211(void *p,u32 i);
extern u8* ssv6xxx_get_addr_hdr80211(void *p,u32 i);
#define	GET_PBUF_HDR80211(p, i)               ssv6xxx_get_pbuf_hdr80211((void *)p,i)
#define	GET_ADDR_HDR80211(p, i)               ssv6xxx_get_addr_hdr80211((void *)p,i)
//#define _GET_PBUF_HDR80211(p, off, i)		  (*((u8 *)(p)+(p)->hdr_offset + (off) +(i)))
//#define GET_PBUF_HDR80211(p, i)             IS_TX_PKT(((PKT_Info *)(p)))? ((((PKT_TxInfo *)p)->aggregation)?_GET_PBUF_HDR80211(((PKT_TxInfo *)p),4,i):_GET_PBUF_HDR80211(((PKT_TxInfo *)p),0,i)):_GET_PBUF_HDR80211(((PKT_Info *)p),0,i)

#define	SET_PBUF_HDR80211(p, i)				(*((u8 *)(p)+(p)->hdr_offset + (i)))

#define	GET_SC_SEQNUM(sc)				(((sc) & 0xfff0) >> 4)
#define	GET_SC_FRAGNUM(sc)				(((sc) & 0x000f)     )

#define GET_QC_TID(qc)					((qc) & 0x000f)
#define	GET_QC_UP(qc)					((qc) & 0x0007)
#define GET_QC_AC(qc) 					((GET_QC_UP(qc) == 0) ? AC_BE : \
										 (GET_QC_UP(qc) == 1) ? AC_BK : \
										 (GET_QC_UP(qc) == 2) ? AC_BK : \
										 (GET_QC_UP(qc) == 3) ? AC_BE : \
										 (GET_QC_UP(qc) == 4) ? AC_VI : \
										 (GET_QC_UP(qc) == 5) ? AC_VI : \
										 (GET_QC_UP(qc) == 6) ? AC_VO : AC_VO)


#define GET_HDR80211_FC(p)				(((p)->f80211==1) ? (((u16)GET_PBUF_HDR80211(p, 1) << 8) | GET_PBUF_HDR80211(p, 0)) : 0)
#define GET_HDR80211_FC_TYPE(p)			((GET_HDR80211_FC(p) & 0x0c) >> 2)
#define GET_HDR80211_FC_SUBTYPE(p)			((GET_HDR80211_FC(p) & 0xF0) >> 4)
#define GET_HDR80211_FC_TYPE_STR(t)		((t == 0) ? "Mgmt" : ((t == 1) ? "Control" : ((t == 2) ? "Data" : "Reserved")))
#define	GET_HDR80211_FC_VER(p)			((GET_HDR80211_FC(p) & M_FC_VER))
#define GET_HDR80211_FC_TODS(p)			((GET_HDR80211_FC(p) & M_FC_TODS)      >>  8)
#define GET_HDR80211_FC_FROMDS(p)		((GET_HDR80211_FC(p) & M_FC_FROMDS)    >>  9)
#define GET_HDR80211_FC_MOREFRAG(p)		((GET_HDR80211_FC(p) & M_FC_MOREFRAGS) >> 10)
#define	GET_HDR80211_FC_RETRY(p)	    ((GET_HDR80211_FC(p) & M_FC_RETRY)     >> 11)
#define	GET_HDR80211_FC_PWRMGNT(p)		((GET_HDR80211_FC(p) & M_FC_PWRMGMT)   >> 12)
#define	GET_HDR80211_FC_MOREDATA(p)		((GET_HDR80211_FC(p) & M_FC_MOREDATA)  >> 13)
#define	GET_HDR80211_FC_PROTECTED(p)	((GET_HDR80211_FC(p) & M_FC_PROTECTED) >> 14)
#define	GET_HDR80211_FC_ORDER(p)		((GET_HDR80211_FC(p) & M_FC_ORDER)     >> 15)

#define SET_HDR80211_FC_MOREFRAG(p)		(SET_PBUF_HDR80211(p, 1) |= (M_FC_MOREFRAGS>>8))
#define SET_HDR80211_FC_MOREDATA(p)		(SET_PBUF_HDR80211(p, 1) |= (M_FC_MOREDATA>>8))

#define UNSET_HDR80211_FC_MOREFRAG(p)	(SET_PBUF_HDR80211(p, 1) &= 0xfb)

#define GET_HDR80211_SC(p)				((u16)GET_PBUF_HDR80211(p, 23) << 8 | (GET_PBUF_HDR80211(p, 22)))
#define GET_HDR80211_SC_SEQNUM(p)		((GET_HDR80211_SC(p) & 0xfff0) >> 4)
#define SET_HDR80211_SC_SEQNUM(p, seq)		(SET_PBUF_HDR80211(p,22) = (u8)((seq<<4)&0x00f0)); \
                                                (SET_PBUF_HDR80211(p,23) = (u8)(((seq<<4)&0xff00)>>8));
#define GET_HDR80211_SC_FRAGNUM(p)		((GET_HDR80211_SC(p) & 0x000f))
#define GET_HDR80211_Ether_Type(p)      (((p)->f80211==1) ? ((p->qos == 1)?(((u16)GET_PBUF_HDR80211(p, 32) << 8) | GET_PBUF_HDR80211(p, 33)):(((u16)GET_PBUF_HDR80211(p, 30) << 8) | GET_PBUF_HDR80211(p, 31))) : 0)

//
//	Function			ToDS	FromDS	Addr1	Addr2	Addr3	Addr4
//	-------------------------------------------------------------------------
//	IBSS			0		0		DA		SA		BSSID	Not_Used
//	To AP (infra.)		1		0		BSSID	SA		DA		Not_Used
//	From AP (infra.)	0		1		DA		BSSID	SA		Not_Used
//	WDS (bridge)		1		1		RA		TA		DA		SA
#define HAS_HDR80211_ADDRESS_4(p)		(GET_HDR80211_FC_TODS(p) & GET_HDR80211_FC_FROMDS(p))

// QoS Control Field
#define	GET_HDR80211_QC(p)				(((p)->qos == 1) ? (((u16)GET_PBUF_HDR80211(p, 25 + (HAS_HDR80211_ADDRESS_4(p)*6)) << 8) | GET_PBUF_HDR80211(p, 24 + (HAS_HDR80211_ADDRESS_4(p)*6))) : 0)
#define	SET_HDR80211_QC(p,v)    \
        SET_PBUF_HDR80211(p, 24)=v&0xFF;\
        SET_PBUF_HDR80211(p, 25)=(v>>8)&0xFF;
#define GET_HDR80211_ADDRESS_1(a, p)	memcpy((a), ((u8 *)(p)+(p)->hdr_offset +  4), 6)
#define GET_HDR80211_ADDRESS_2(a, p)	memcpy((a), ((u8 *)(p)+(p)->hdr_offset + 10), 6)
#define GET_HDR80211_ADDRESS_3(a, p)	memcpy((a), ((u8 *)(p)+(p)->hdr_offset + 16), 6)

struct cfg_host_rxpkt {

	 /* The definition of WORD_1: */
	u32             len:16;
	u32             c_type:3;
    u32             f80211:1;
	u32             qos:1;          /* 0: without qos control field, 1: with qos control field */
    u32             ht:1;           /* 0: without ht control field, 1: with ht control field */
    u32             use_4addr:1;
	u32             l3cs_err:1;
    u32             l4cs_err:1;
    u32             align2:1;
    u32             RSVD_0:2;
	u32             psm:1;
    u32             stype_b5b4:2;
    u32             extra_info:1;

    /* The definition of WORD_2: */
    u32             fCmd;

    /* The definition of WORD_3: */
    u32             hdr_offset:8;
    u32             frag:1;
    u32             unicast:1;
    u32             hdr_len:6;
    u32             RxResult:8;
    u32             wildcard_bssid:1;
    u32             RSVD_1:1;
    u32             reason:6;

    /* The definition of WORD_4: */
    u32             payload_offset:8;
    u32             next_frag_pid:7;
    u32             RSVD_2:1;
    u32             fCmdIdx:3;
    u32             wsid:4;
	u32				RSVD_3:3;
	u32				drate_idx:6;

	/* The definition of WORD_5: */
	u32 			seq_num:16;
	u32				tid:16;

	/* The definition of WORD_6: */
	u32				pkt_type:8;
	u32				RCPI:8;
	u32				SNR:8;
	u32				RSVD:8;



};


typedef struct PKT_TxPhyInfo_st
{
	/* The definition of WORD_1: */
	u32 Llength:8;

	u32 Mlength:8;

	u32 RESV1:8;

	u32 RESV2:8;

	/* The definition of WORD_2: */
	u32 mode:3;
	u32 ch_bw:3;
	u32 preamble_option:1;
	u32 HTshortGI:1;

	u32 rate:7;
	u32 RESV3:1;

	u32 smoothing:1;
	u32 no_sounding:1;
	u32 aggregation:1;
	u32 stbc:2;
	u32 fec:1;
	u32 n_ess:2;

	u32 txpwrlvl:8;

	/* The definition of WORD_3: */
	u32 Ll_length:8;

	u32 Ml_length:4;
	u32 l_rate:3;
	u32 RESV4:1;

	u32 RESV5:16;

	/* The definition of WORD_4: */
	u32 RESV6:32;

	/* The definition of WORD_5: */
	u32 RESV7:16;

	u32 Lservice:8;

	u32 Mservice:8;

	/* The definition of WORD_6: */
	u32 RESV8:32;

	/* The definition of WORD_7: */
	u32 RESV9:32;



}PKT_TxPhyInfo, *PPKT_TxPhyInfo;






typedef struct PKT_RxPhyInfo_st
{
	/* The definition of WORD_1: */
	u32 Llength:8;

	u32 Mlength:8;

	u32 RESV1:8;

	u32 RESV2:8;

	/* The definition of WORD_2: */
	u32 mode:3;
	u32 ch_bw:3;
	u32 preamble_option:1;
	u32 HTshortGI:1;

	u32 rate:7;
	u32 RESV3:1;

	u32 smoothing:1;
	u32 no_sounding:1;
	u32 aggregation:1;
	u32 stbc:2;
	u32 fec:1;
	u32 n_ess:2;

	u32 RESV4:8;

	/* The definition of WORD_3: */
	u32 Ll_length:8;

	u32 Ml_length:4;
	u32 l_rate:3;
	u32 RESV5:1;

	u32 RESV6:16;

	/* The definition of WORD_4: */
	u32 RESV7:32;

	/* The definition of WORD_5: */
	u32 RESV8:16;

	u32 Lservice:8;

	u32 Mservice:8;





}PKT_RxPhyInfo, *PPKT_RxPhyInfo;







typedef struct PKT_TxInfo_st
{
    /* The definition of WORD_1: */
	u32             len:16;         // only data length
	u32             c_type:3;
    u32             f80211:1;
    u32             qos:1;          /* 0: without qos control field, 1: with qos control field */
    u32             ht:1;           /* 0: without ht control field, 1: with ht control field */
    u32             use_4addr:1;
    u32             RSVD_0:3;
	u32             bc_que:1;
	u32				security:1;
    u32             more_data:1;
    u32             stype_b5b4:2;
    u32             extra_info:1;   /* 0: don't trap to cpu after parsing, 1: trap to cpu after parsing */

    /* The definition of WORD_2: */
    u32             fCmd;

    /* The definition of WORD_3: */
    u32             hdr_offset:8;
    u32             frag:1;
    u32             unicast:1;
    u32             hdr_len:6;
    u32             tx_report:1;
    u32             tx_burst:1;     /* 0: normal, 1: burst tx */
    u32             ack_policy:2;   /* 0: normal ack, 1: no_ack, 2: PSMP Ack(Reserved), 3:Block Ack. See Table 8-6, IEEE 802.11 Spec. 2012 p.490*/
	u32             aggregation:1;
    u32             RSVD_1:3;
	u32             do_rts_cts:2;   /* 0: no RTS/CTS, 1: need RTS/CTS */
                                    /* 2: CTS protection, 3: RSVD */
    u32             reason:6;

    /* The definition of WORD_4: */
    u32             payload_offset:8;
    u32             next_frag_pid:7;
    u32             RSVD_2:1;
    u32             fCmdIdx:3;
    u32             wsid:4;
    u32             txq_idx:3;      //SW AMPDU use to record pad size
	u32				TxF_ID:6;       //SW AMDPU use to record ampdu data


	/* The definition of WORD_4: */
	u32				rts_cts_nav:16;
	u32				frame_consume_time:10;	//32 units
	u32				crate_idx:6;

	/* The definition of WORD_5: */
	u32				drate_idx:6;
	u32				dl_length:12;
	u32				RSVD_3:14;              //SW use to record AMPDU retry count


	union{
		PKT_TxPhyInfo  tx_phy_info;
		u8 			   phy_info[28];
	};






} PKT_TxInfo, *PPKT_TxInfo;

typedef struct PKT_RxInfo_st
{
    /* The definition of WORD_1: */
	u32             len:16;         //include pkt info length + data length
	u32             c_type:3;
    u32             f80211:1;
	u32             qos:1;          /* 0: without qos control field, 1: with qos control field */
    u32             ht:1;           /* 0: without ht control field, 1: with ht control field */
    u32             use_4addr:1;
	u32             l3cs_err:1;
    u32             l4cs_err:1;
    u32             align2:1;
    u32             RSVD_0:2;
	u32             psm:1;
    u32             stype_b5b4:2;
    u32             extra_info:1;

    /* The definition of WORD_2: */
    u32             fCmd;

    /* The definition of WORD_3: */
    u32             hdr_offset:8;
    u32             frag:1;
    u32             unicast:1;
    u32             hdr_len:6;
    u32             RxResult:8;
    u32             wildcard_bssid:1;
    u32             RSVD_1:1;
    u32             reason:6;

    /* The definition of WORD_4: */
    u32             payload_offset:8;
    u32             next_frag_pid:7;
    u32             RSVD_2:1;
    u32             fCmdIdx:3;
    u32             wsid:4;
	u32				RSVD_3:3;
	u32				drate_idx:6;



	union{
		PKT_RxPhyInfo  rx_phy_info;
		u8 			   phy_info[20];
	};

} PKT_RxInfo, *PPKT_RxInfo;




typedef struct PKT_Info_st
{
    /* The definition of WORD_1: */
    u32             len:16;
    u32             c_type:3;
    u32             f80211:1;
    u32             qos:1;          /* 0: without qos control field, 1: with qos control field */
    u32             ht:1;           /* 0: without ht control field, 1: with ht control field */
    u32             use_4addr:1;
    u32             RSVD_0:5;
    u32             more_data:1;
    u32             stype_b5b4:2;
    u32             extra_info:1;

    /* The definition of WORD_2: */
    u32             fCmd;

    /* The definition of WORD_3: */
    u32             hdr_offset:8;
    u32             frag:1;
    u32             unicast:1;
    u32             hdr_len:6;
    u32             RSVD_1:10;
    u32             reason:6;

    /* The definition of WORD_4: */
    u32             payload_offset:8;
    u32             next_frag_pid:7;
    u32             RSVD_2:1;
    u32             fCmdIdx:3;
    u32             wsid:4;
    u32             RSVD_3:3;
	u32				drate_idx:6;

} PKT_Info, *PPKT_Info;



typedef struct PHY_Info_st
{
    u32             WORD1;
    u32             WORD2;
    u32             WORD3;
    u32             WORD4;
    u32             WORD5;
    u32             WORD6;
    u32             WORD7;

} PHY_Info, *PPHY_Info;

#define IS_AMPDU_RETRY_PACKET(p) (p->RSVD_3)


/**
 * Define constants for do_rts_cts field of PKT_TxInfo structure
 *
 * @ TX_NO_RTS_CTS
 * @ TX_RTS_CTS
 * @ TX_CTS
 */
#define TX_NO_RTS_CTS                   0
#define TX_RTS_CTS                      1
#define TX_CTS                          2

#ifdef THROUGHPUT_TEST

typedef struct SDIO_THROUGHPUT_COMMAND_st
{
	u32		transferCount;
	u16		transferLength;
	u16		resverd;
} SDIO_THROUGHPUT_COMMAND;


typedef struct THROUGHPUT_COMMAND_st
{
    u8		mode;
	u8		rateIndex;//0-38
	u8      noack;
	u16		transferLength;
	u16      qos;
	u32		transferCount;
} THROUGHPUT_COMMAND;

#endif

enum fcmd_seek_type {
    FCMD_SEEK_PREV  = 0,
    FCMD_SEEK_CUR,
    FCMD_SEEK_NEXT,
};


#endif	/* _SSV_PKTDEF_H_ */

