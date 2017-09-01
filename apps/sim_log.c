#include <stdarg.h>
#include <config.h>
#include <rtos.h>
#include "log.h"
#include <lib/ssv_lib.h>
#include <lib/lib-impl.h>

//SIM platform
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <host_apis.h>

// just for testing
#include <apps/mac80211/defrag.h>
#include <apps/mac80211/mlme.h>

_os_mutex		g_log_mutex;
u32				g_dbg_mutex;

u8		g_magic_str[8]	= "$$v62@@";
u32		g_magic_u32		= 0xAAAAAAAA;

log_prnf_cfg_st		g_log_prnf_cfg;
log_out_desc_st		g_log_out;

// global variable for acceleration
//char s_acc_buf[ACC_BUF_MAX] = {0};
char s_prn_buf[PRN_BUF_MAX];
char s_prn_tmp[PRN_BUF_MAX];
char s_tag_lvl[PRN_TAG_MAX];
char s_tag_mod[PRN_TAG_MAX];
char s_tag_fl[PRN_TAG_MAX];

// u8		g_log_frmbuf[LOG_FRMBUF_MAX];

const char*	g_log_lvl_tag[LOG_LEVEL_OFF] = {
	""     ,		// LOG_LEVEL_ON
	"TRACE",		// LOG_LEVEL_TRACE
	"DEBUG",		// LOG_LEVEL_DEBUG
	"INFO" ,		// LOG_LEVEL_INFO
	"WARN" ,		// LOG_LEVEL_WARN
	"FAIL" ,		// LOG_LEVEL_FAIL
	"ERROR",		// LOG_LEVEL_ERROR
	"FATAL",		// LOG_LEVEL_FATAL
	// LOG_LEVEL_OFF
};

const char*	g_log_mod_tag[LOG_MODULE_ALL] = {
	""      ,		// LOG_MODULE_EMPTY
	"mrx"   ,		// LOG_MODULE_MRX
	"mtx"   ,		// LOG_MODULE_MTX
	"edca"  ,		// LOG_MODULE_EDCA
	"pbuf"  ,		// LOG_MODULE_PBUF
	"l3l4"  ,		// LOG_MODULE_L3L4
	"mgmt"  ,		// LOG_MODULE_MGMT
	"frag"  ,		// LOG_MODULE_FRAG
	"defrag",		// LOG_MODULE_DEFRAG
	"mlme"  ,		// LOG_MODULE_MLME
	"cmd"   ,		// LOG_MODULE_CMD
	"wpa"   ,		// LOG_MODULE_WPA
	"main"  ,       // LOG_MODULE_MAIN
	// LOG_MODULE_ALL
};

/* ============================================================================
		static local functions on host/soc side
============================================================================ */
void _log_evt_prnf_dump(log_evt_prnf_st *prnf);

#if (CONFIG_LOG_ENABLE)
static inline const char *_extract_filename(const char *str)
{
#if (defined WIN32)
	char	ch = '\\';
#elif (defined __SSV_UNIX_SIM__)
	char	ch = '/';
#endif
	u32		p = ssv6xxx_strrpos(str, ch);
	if (p>=0)
		return (str + p + 1);

	return "";
}
#endif

void	_log_out_dst_open_dump(u8 dst_open)
{
	log_printf("dst_open : 0x%02x ", dst_open);
	LOG_TAG_SUPPRESS_ON();
	if (dst_open & LOG_OUT_HOST_TERM)		log_printf("(LOG_OUT_HOST_TERM) ");
	if (dst_open & LOG_OUT_HOST_FILE)		log_printf("(LOG_OUT_HOST_FILE) ");
	if (dst_open & LOG_OUT_SOC_TERM)		log_printf("(LOG_OUT_SOC_TERM) ");
	if (dst_open & LOG_OUT_SOC_HOST_TERM)	log_printf("(LOG_OUT_SOC_HOST_TERM) ");
	if (dst_open & LOG_OUT_SOC_HOST_FILE)	log_printf("(LOG_OUT_SOC_HOST_FILE) ");
	LOG_TAG_SUPPRESS_OFF();
	log_printf("\n\r");
}

void	_log_out_dst_cur_dump(u8 dst_cur)
{
	log_printf("dst_cur  : 0x%02x ", g_log_out.dst_cur);
	LOG_TAG_SUPPRESS_ON();
	if (dst_cur & LOG_OUT_HOST_TERM)		log_printf("(LOG_OUT_HOST_TERM) ");
	if (dst_cur & LOG_OUT_HOST_FILE)		log_printf("(LOG_OUT_HOST_FILE) ");
	if (dst_cur & LOG_OUT_SOC_TERM)			log_printf("(LOG_OUT_SOC_TERM) ");
	if (dst_cur & LOG_OUT_SOC_HOST_TERM)	log_printf("(LOG_OUT_SOC_HOST_TERM) ");
	if (dst_cur & LOG_OUT_SOC_HOST_FILE)	log_printf("(LOG_OUT_SOC_HOST_FILE) ");
	LOG_TAG_SUPPRESS_OFF();
	log_printf("\n\r");
}

/* ============================================================================
		static local functions on host side
============================================================================ */
static s32		_log_mod_str2idx(const char str[PRN_TAG_MAX]);
static s32		_log_lvl_str2idx(const char str[PRN_TAG_MAX]);
static void		_log_print_usage(void);
static void		_log_list_all_module(void);

// return > 0 : level idx
//        < 0 : fail
s32	_log_lvl_str2idx(const char str[PRN_TAG_MAX])
{
	s32		i;

	for (i=1; i<LOG_LEVEL_OFF; i++)
	{
		if (strcmp(str, g_log_lvl_tag[i]) == 0)
			return i;
	}
	return -1;
}

// return >= 0 : module idx
//        <  0 : fail
s32	_log_mod_str2idx(const char str[PRN_TAG_MAX])
{
	s32		i;

	for (i=0; i<LOG_MODULE_ALL; i++)
	{
		if (strcmp(str, g_log_mod_tag[i]) == 0)
			return i;
	}
	return -1;
}

void _log_out_desc_dump(log_out_desc_st *p)
{
	log_printf("< %s >\n\r", "out desc");
	_log_out_dst_open_dump(p->dst_open);
	_log_out_dst_cur_dump(p->dst_cur);

	log_printf("path     : %s\n\r",	    p->path);
	log_printf("fp       : 0x%08x\n\r", p->fp);

	return;
}


void	_log_print_usage(void)
{
	log_printf("\nusage of log --\n");
	log_printf("log                    : print log usage\n");
#if (CONFIG_SIM_PLATFORM)
	log_printf("log dbg dmsg           : load .dmsg & .bin file for soc dbg msg\n");
	log_printf("log dbg view           : launch c:\\dbgview.exe (WIN32 only)\n");
	log_printf("log dbg all            : = log dbg dmsg + dbgview\n");
#endif
	log_printf("log mod                : print all available modules\n");
	log_printf("log {soc} st           : print current {soc} log status\n");
	log_printf("log {soc} test         : print {soc} log test strings (for temp use)\n");
	log_printf("log [on|off] fl        : turn on/off fileline info\n");
	log_printf("log {soc} [on|off] lvl : turn on/off {soc} level  tag print string\n");
	log_printf("log {soc} [on|off] mod : turn on/off {soc} module tag print string\n");
	log_printf("log {soc} [on|off] [[mod]|all] : turn on/off {soc} the [mod]/(all [mod])\n");
	log_printf("log {soc} set [on|off|[lvl]]   : set {soc} log level to (all on)/(all off)/[lvl]\n");
	log_printf("< example >\n");
	log_printf("log on  mlme   -> turn on MLME module\n");
	log_printf("log set warn   -> set to WARN level\n");
	log_printf("log soc on lvl -> turn on soc level tag string printing\n");
}

void _log_list_all_module(void)
{
	u32		i;

	log_printf("module count : %d\n", LOG_MODULE_ALL);
	for (i=0; i<LOG_MODULE_ALL; i++)
	{
		log_printf("<%s> ", g_log_mod_tag[i]);
		if ((i+1)%8 == 0)	log_printf("\n");
	}
	return;
}


/* ============================================================================
		exported functions on host/soc side
============================================================================ */
void _log_tag_lvl_str(const u32 n, u8 str[LOG_MAX_TAG])
{
	memset(str, 0, LOG_MAX_TAG);
	switch (n)
	{
	case LOG_LEVEL_ON  : sprintf((char *)str, "[%-5s]\n", "ON");		break;
	case LOG_LEVEL_OFF : sprintf((char *)str, "[%-5s]\n", "OFF");	break;
	default:			 sprintf((char *)str, "[%-5s]\n", g_log_lvl_tag[n]);		break;
	}
	return;
}

// return: # of str entries are copied
u32 _log_tag_mod_str(const u32 m, u8 str[LOG_MODULE_ALL][LOG_MAX_TAG])
{
	u32		i, r;

	r = 0;
	memset(str, 0, LOG_MODULE_ALL*LOG_MAX_TAG);
	for (i=0; i<LOG_MODULE_ALL; i++)
	{
		if ((m & LOG_MODULE_MASK(i)) && (strlen(g_log_mod_tag[i]) > 0))
		{
			sprintf((char *)str[r], "<%s> ", g_log_mod_tag[i]);
			r++;
		}
	}
	return r;
}

void _log_prnf_cfg_dump(log_prnf_cfg_st *p)
{
	u32		i, r;
	u8		b_lvl[LOG_MAX_TAG];
	u8		b_mod[LOG_MODULE_ALL][LOG_MAX_TAG];


	log_printf("< %s >\n\r", "prnf cfg");

	log_printf("%-10s : %d\n", "level  tag", p->prn_tag_lvl);
	log_printf("%-10s : %d\n", "module tag", p->prn_tag_mod);

	_log_tag_lvl_str(p->lvl, b_lvl);
	log_printf("%-10s : %s", "level", b_lvl);

	log_printf("%-10s : %d\n", "fileline", p->fl);

	r = _log_tag_mod_str(p->mod, b_mod);
	log_printf("%-10s : ", "module");
	for (i=0; i<r; i++)
		log_printf("%s", b_mod[i]);

	log_printf("\n\n");

	return;
}

void _log_soc_send_evt_prn_buf(const char *buf)
{
	log_soc_evt_st		*soc_evt;
	log_evt_prnf_buf_st	*prn;
	MsgEvent			*msg_evt;

	LOG_EVENT_ALLOC(msg_evt, SOC_EVT_LOG, LOG_SEVT_MAX_LEN);
	soc_evt = (log_soc_evt_st *)(LOG_EVENT_DATA_PTR(msg_evt));
	memset(soc_evt, 0, LOG_SEVT_MAX_LEN);
	LOG_EVENT_SET_LEN(msg_evt, LOG_SEVT_MAX_LEN);

	prn = (log_evt_prnf_buf_st *)(&(soc_evt->data));
	// dst_open & dst_cur
	prn->dst_open	= g_log_out.dst_open;
	prn->dst_cur		= g_log_out.dst_cur;
	// copy fmt string
	prn->fmt_len = strlen(buf);
	strncpy((char *)(prn->buf), buf, prn->fmt_len);
	// for now, 'arg_len' is always 0
	prn->arg_len = 0;
	prn->len	 = sizeof(log_evt_prnf_buf_st) + prn->fmt_len + prn->arg_len;

	// cal soc evt's length & set cmd id
	soc_evt->len = prn->len + LOG_SEVT_CMD_DATA_OFFSET;
	soc_evt->cmd = LOG_SEVT_PRNF_BUF;
	//printf("# %d\n\r", __LINE__);

	LOG_EVENT_SEND(msg_evt);
	return;
}

void _log_evt_prn_buf_dump(log_evt_prnf_buf_st *prn)
{
	u32		i;
	u32		arg_cnt;

	log_printf("< %s >\n", __FUNCTION__);
	log_printf("len     = %d\n", prn->len);
	log_printf("fmt_len = %d\n", prn->fmt_len);
	log_printf("arg_len = %d\n", prn->arg_len);
	_log_out_dst_open_dump(prn->dst_open);
	_log_out_dst_cur_dump(prn->dst_cur);

	log_printf("fmt = ");
	for (i=0; i<prn->fmt_len; i++)
		log_printf("%c", *((char *)(prn->buf+i)));

	arg_cnt = prn->arg_len / 4;
	log_printf("arg_cnt = %d\n", arg_cnt);
	for (i=0; i<arg_cnt; i++)
		log_printf("%d : 0x%08x\n", i, *(int *)(prn->buf + prn->fmt_len + i*4));
}

// ======================================================================================
// note :
//		 These _log_evt_alloc() & _log_evt_free() are the copy of CmdEngine_HostEventAlloc()
//	to avoid circular inclusion issue. If we want to solve this problem, we have to place
//	our host evt func to another place.
// ======================================================================================
MsgEvent *_log_evt_alloc(ssv6xxx_soc_event hEvtID, s32 size)
{
    MsgEvent *MsgEv;
    HDR_HostEvent *chevt;

    MsgEv = msg_evt_alloc();
    if (MsgEv == NULL)
        return NULL;
    memset((void *)MsgEv, 0, sizeof(MsgEvent));
    MsgEv->MsgType = MEVT_PKT_BUF;
    MsgEv->MsgData = (u32)PBUF_MAlloc(size+sizeof(HDR_HostEvent) , SOC_PBUF);
    if (MsgEv->MsgData == 0) {
        msg_evt_free(MsgEv);
        return NULL;
    }
    chevt = (HDR_HostEvent *)MsgEv->MsgData;
    chevt->c_type  = HOST_EVENT;
    chevt->h_event = hEvtID;
    chevt->len     = size;
    return MsgEv;
}

void	_log_evt_free(MsgEvent *MsgEvt)
{
	if (MsgEvt == NULL)
		return;

	if (MsgEvt->MsgData != (u32)NULL)
		PBUF_MFree((void *)(MsgEvt->MsgData));

	msg_evt_free(MsgEvt);
	return;
}


void LOG_hcmd_dump(log_hcmd_st	*hcmd)
{
	log_printf("< log hcmd >\n");
	log_printf("target : %d (0:HOST , 1:SOC)\n", hcmd->target);
	log_printf("cmd    : %d (", hcmd->cmd);
	switch (hcmd->cmd)
	{
	case LOG_HCMD_TAG_ON_FL		: log_printf("LOG_HCMD_TAG_ON_FL");		break;
	case LOG_HCMD_TAG_OFF_FL	: log_printf("LOG_HCMD_TAG_OFF_FL");	break;
	case LOG_HCMD_TAG_ON_LEVEL	: log_printf("LOG_HCMD_TAG_ON_LEVEL");	break;
	case LOG_HCMD_TAG_OFF_LEVEL	: log_printf("LOG_HCMD_TAG_OFF_LEVEL");	break;
	case LOG_HCMD_TAG_ON_MODULE	: log_printf("LOG_HCMD_TAG_ON_MODULE");	break;
	case LOG_HCMD_TAG_OFF_MODULE: log_printf("LOG_HCMD_TAG_OFF_MODULE");break;
	case LOG_HCMD_MODULE_ON		: log_printf("LOG_HCMD_MODULE_ON");		break;
	case LOG_HCMD_MODULE_OFF	: log_printf("LOG_HCMD_MODULE_OFF");	break;
	case LOG_HCMD_LEVEL_SET		: log_printf("LOG_HCMD_LEVEL_SET");		break;
	case LOG_HCMD_PRINT_USAGE	: log_printf("LOG_HCMD_PRINT_USAGE");	break;
	case LOG_HCMD_PRINT_STATUS	: log_printf("LOG_HCMD_PRINT_STATUS");	break;
	case LOG_HCMD_PRINT_MODULE	: log_printf("LOG_HCMD_PRINT_MODULE");	break;
	case LOG_HCMD_TEST			: log_printf("LOG_HCMD_TEST");			break;
    default: break;
	}
	log_printf("\n");
	log_printf("data   : %d ", hcmd->data);
	if ((hcmd->cmd == LOG_HCMD_MODULE_ON) || (hcmd->cmd == LOG_HCMD_MODULE_OFF))
	{
		switch (hcmd->data)
		{
		case LOG_MODULE_EMPTY:	log_printf("(empty)\n");			break;
		case LOG_MODULE_ALL:	log_printf("(all)\n");				break;
		default:	log_printf("(%s)\n", g_log_mod_tag[hcmd->data]);	break;
		}
		return;
	}
	if (hcmd->cmd == LOG_HCMD_LEVEL_SET)
	{
		switch (hcmd->data)
		{
		case LOG_LEVEL_OFF:		log_printf("(off)\n");				break;
		case LOG_LEVEL_ON:		log_printf("(on)\n");				break;
		default:	log_printf("(%s)\n", g_log_lvl_tag[hcmd->data]);	break;
		}
		return;
	}
	log_printf("\n");
}

void LOG_init(bool tag_level, bool tag_mod, u32 level, u32 mod_mask, bool fileline)
{
	LOG_MUTEX_INIT();

	memset(&g_log_prnf_cfg, 0, sizeof(log_prnf_cfg_st));
	g_log_prnf_cfg.prn_tag_lvl	= true;
	g_log_prnf_cfg.prn_tag_mod	= true;
	g_log_prnf_cfg.prn_tag_sprs	= false;
	g_log_prnf_cfg.chk_tag_sprs	= false;

	g_log_prnf_cfg.prn_tag_lvl	= tag_level;
	g_log_prnf_cfg.prn_tag_mod	= tag_mod;
	g_log_prnf_cfg.lvl			= level;

	mod_mask |= LOG_MODULE_MASK(LOG_MODULE_EMPTY);	// by default, enable LOG_MODULE_EMPTY
	g_log_prnf_cfg.mod			= mod_mask;
	g_log_prnf_cfg.fl			= fileline;

	// init frm buf
	//memset(g_log_frmbuf, 0xAA, LOG_FRMBUF_MAX);
}

bool LOG_soc_exec_hcmd(log_hcmd_st *hcmd)
{
	u8	cmd = hcmd->cmd;
	u8	idx = hcmd->data;

	if (cmd == LOG_HCMD_TEST)
	{
#if 1
		// t_printf(LOG_LEVEL_ON,    LOG_MODULE_EMPTY,		T("0 args\n"));
		// t_printf(LOG_LEVEL_TRACE, LOG_MODULE_MRX,		T("1 char args  - %c\n"), 'a');
		// t_printf(LOG_LEVEL_DEBUG, LOG_MODULE_MTX,		T("1 str args   - %s\n"), T("hello world"));
		// t_printf(LOG_LEVEL_INFO,  LOG_MODULE_EDCA,		T("3 digit args - %d, %3d, %03d\n"), 9, 9, 9);
		// t_printf(LOG_LEVEL_WARN,  LOG_MODULE_PBUF,		T("3 hex args   - %x, %X, %08x\n"), 0xab, 0xab, 0xab);
		// t_printf(LOG_LEVEL_FAIL,  LOG_MODULE_L3L4,		T("mix args     - %10s, %d, %s\n"), T("string"), 10, T("hello world"));
		// t_printf(LOG_LEVEL_ERROR, LOG_MODULE_MGMT,		T("mix args     - %-3d, %-10s, %-08x\n"), 10, "string", 0xab);
		// t_printf(LOG_LEVEL_ON,    LOG_MODULE_FRAG,		T("0 args\n"));
		// t_printf(LOG_LEVEL_TRACE, LOG_MODULE_DEFRAG,		T("1 char args  - %c\n"), 'a');
		// t_printf(LOG_LEVEL_DEBUG, LOG_MODULE_MLME,		T("1 str args   - %s\n"), T("hello world"));
		t_printf(LOG_LEVEL_ERROR,  LOG_MODULE_CMD,		T("3 digit args - %d, %3d, %03d\n"), 9, 9, 9);
		// t_printf(LOG_LEVEL_WARN,  LOG_MODULE_WPA,		T("3 hex args   - %x, %X, %08x\n"), 0xab, 0xab, 0xab);
		// t_printf(LOG_LEVEL_FAIL,  LOG_MODULE_MAIN,		T("mix args     - %10s, %d, %s\n"), T("string"), 10, T("hello world"));

		return true;
#endif
		// LOG_OUT_DST_CUR_OFF(LOG_OUT_SOC_HOST_TERM);
		// LOG_OUT_DST_CUR_OFF(LOG_OUT_SOC_HOST_FILE);
		log_printf("\n");
		log_printf("test : log_printf() %s\n", "Hello~ I am handsome smart Eric Chen!!");
		LOG_PRINTF("test : LOG_PRINTF() %d\n", LOG_LEVEL_ON);
		LOG_TRACE ("test : LOG_TRACE () %d\n", LOG_LEVEL_TRACE);
		LOG_DEBUG ("test : LOG_DEBUG () %d\n", LOG_LEVEL_DEBUG);
		LOG_INFO  ("test : LOG_INFO  () %d\n", LOG_LEVEL_INFO);
		LOG_WARN  ("test : LOG_WARN  () %d\n", LOG_LEVEL_WARN);
		LOG_FAIL  ("test : LOG_FAIL  () %d\n", LOG_LEVEL_FAIL);
		LOG_ERROR ("test : LOG_ERROR () %d\n", LOG_LEVEL_ERROR);
		// LOG_FATAL ("test : LOG_FATAL () %d\n", LOG_LEVEL_FATAL);
		DEFRAG_DEBUG("defrag : DEFRAG_DEBUG() %d\n", LOG_LEVEL_DEBUG);
		DEFRAG_INFO ("defrag : DEFRAG_INFO () %d\n", LOG_LEVEL_INFO);
		DEFRAG_WARN ("defrag : DEFRAG_WARN () %d\n", LOG_LEVEL_WARN);
		DEFRAG_FAIL ("defrag : DEFRAG_FAIL () %d\n", LOG_LEVEL_FAIL);
		DEFRAG_ERROR("defrag : DEFRAG_ERROR() %d\n", LOG_LEVEL_ERROR);
		// DEFRAG_FATAL("defrag : DEFRAG_FATAL() %d\n", LOG_LEVEL_FATAL);
		MLME_TRACE("mlme : MLME_TRACE() %d\n", LOG_LEVEL_TRACE);
		MLME_DEBUG("mlme : MLME_DEBUG() %d\n", LOG_LEVEL_DEBUG);
		MLME_INFO ("mlme : MLME_INFO () %d\n", LOG_LEVEL_INFO);
		MLME_WARN ("mlme : MLME_WARN () %d\n", LOG_LEVEL_WARN);
		MLME_FAIL ("mlme : MLME_FAIL () %d\n", LOG_LEVEL_FAIL);
		MLME_ERROR("mlme : MLME_ERROR() %d\n", LOG_LEVEL_ERROR);
		// MLME_FATAL("mlme : MLME_FATAL() %d\n", LOG_LEVEL_FATAL);

		// LOG_OUT_DST_CUR_ON(LOG_OUT_SOC_HOST_TERM);
		// LOG_OUT_DST_CUR_ON(LOG_OUT_SOC_HOST_FILE);

		//log_printf("g_log_frmbuf = 0x%08x, size = %d\n\r", g_log_frmbuf, LOG_FRMBUF_MAX);
		//ssv6xxx_raw_dump_ex((s8 *)g_log_frmbuf, SIZE_1KB, true, 16, 16, 16, LOG_LEVEL_ON, LOG_MODULE_EMPTY);
		return true;
	}

	switch (cmd)
	{
	case LOG_HCMD_PRINT_STATUS:									goto ret_prn_status;
	case LOG_HCMD_TAG_ON_FL:		LOG_FILELINE_TURN_ON();		goto ret_prn_status;
	case LOG_HCMD_TAG_OFF_FL:		LOG_FILELINE_TURN_OFF();	goto ret_prn_status;
	case LOG_HCMD_TAG_ON_LEVEL:		LOG_TAG_LEVEL_TURN_ON();	goto ret_prn_status;
	case LOG_HCMD_TAG_OFF_LEVEL:	LOG_TAG_LEVEL_TURN_OFF();	goto ret_prn_status;
	case LOG_HCMD_TAG_ON_MODULE:	LOG_TAG_MODULE_TURN_ON();	goto ret_prn_status;
	case LOG_HCMD_TAG_OFF_MODULE:	LOG_TAG_MODULE_TURN_OFF();	goto ret_prn_status;
	case LOG_HCMD_LEVEL_SET:		LOG_LEVEL_SET(idx);			goto ret_prn_status;
	case LOG_HCMD_MODULE_ON:		LOG_MODULE_TURN_ON(idx);	goto ret_prn_status;
	case LOG_HCMD_MODULE_OFF:
		LOG_MODULE_TURN_OFF(idx);
		if (idx == LOG_MODULE_ALL)
			LOG_MODULE_TURN_ON(LOG_MODULE_EMPTY);
		goto ret_prn_status;
	default:
		return false;
	}

	// never reach here!!
	return false;

ret_prn_status:
	{
		u16				len;
		MsgEvent		*msg_evt;
		log_soc_evt_st	*soc_evt;
		log_evt_status_report_st	status;

		len = sizeof(log_soc_evt_st) + sizeof(log_evt_status_report_st);
		LOG_EVENT_ALLOC(msg_evt, SOC_EVT_LOG, len);

		soc_evt = (log_soc_evt_st *)(LOG_EVENT_DATA_PTR(msg_evt));
		memset(soc_evt, 0, len);
		soc_evt->len = len;
		soc_evt->cmd = LOG_SEVT_STATUS_REPORT;

		memcpy(&(status.prnf_cfg), &(g_log_prnf_cfg), sizeof(log_prnf_cfg_st));
		memcpy(&(status.out_desc), &(g_log_out),	  sizeof(log_out_desc_st));

		memcpy(&(soc_evt->data), &status, sizeof(log_evt_status_report_st));
		LOG_EVENT_SET_LEN(msg_evt, len);
#if 0
		log_printf("%s()<=\n", __FUNCTION__);
		log_printf("soc_evt->len = %d\n", soc_evt->len);
		log_printf("sizeof(log_evt_status_report_st) = %d\n", sizeof(log_evt_status_report_st));
		LOG_out_desc_dump();
		ssv6xxx_raw_dump((s8 *)soc_evt, soc_evt->len);
		log_printf("%s()=>\n", __FUNCTION__);
#endif
		LOG_EVENT_SEND(msg_evt);

		return true;
	}

	return true;
}

void LOG_out_desc_dump(void)
{
	log_printf("< log output descriptor >\n\r");
	_log_out_dst_open_dump(g_log_out.dst_open);
	_log_out_dst_cur_dump(g_log_out.dst_cur);

	log_printf("path     : %s\n\r",	    g_log_out.path);
	log_printf("fp       : 0x%08x\n\r", g_log_out.fp);

	return;
}

// init g_log_out
bool LOG_out_init(void)
{
	memset(&g_log_out, 0, sizeof(log_out_desc_st));
	return true;
}

bool	_sim_out_dst_open(u8 _dst, const u8 *_path)
{
	ASSERT((_dst == LOG_OUT_HOST_TERM) || (_dst == LOG_OUT_HOST_FILE));
	if (_dst == LOG_OUT_HOST_TERM)
	{
		if (LOG_OUT_DST_IS_OPEN(LOG_OUT_HOST_TERM))
			return false;

		LOG_OUT_DST_OPEN(LOG_OUT_HOST_TERM);
		return true;
	}

	if (_dst == LOG_OUT_HOST_FILE)
	{
		if (LOG_OUT_DST_IS_OPEN(LOG_OUT_HOST_FILE))
			return false;

		if (g_log_out.fp != NULL)
		{
			fclose((FILE *)g_log_out.fp);	// try to fclose() fp
			g_log_out.fp = NULL;
		}
		if ((g_log_out.fp = (void *)fopen((const char *)_path, LOG_OUT_FILE_MODE)) == NULL)
		{
			LOG_FAIL("fail to fopen() %s\n", _path);
			return false;
		}
		strcpy((char *)(g_log_out.path), (const char *)_path);
		LOG_TRACE("%s(): fopen() %s\n", __FUNCTION__, _path);

		LOG_OUT_DST_OPEN(LOG_OUT_HOST_FILE);
		return true;
	}

	// should never reach here!!
	return false;
}

bool	_sim_out_dst_close(u8 _dst)
{
	ASSERT((_dst == LOG_OUT_HOST_TERM) || (_dst == LOG_OUT_HOST_FILE));
	if (_dst == LOG_OUT_HOST_TERM)
	{
		if (LOG_OUT_DST_IS_OPEN(LOG_OUT_HOST_TERM))
		{
			LOG_OUT_DST_CUR_OFF(LOG_OUT_HOST_TERM);
			LOG_OUT_DST_CLOSE(LOG_OUT_HOST_TERM);
		}
		return true;
	}

	if (_dst == LOG_OUT_HOST_FILE)
	{
		if (LOG_OUT_DST_IS_OPEN(LOG_OUT_HOST_FILE))
		{
			LOG_TRACE("%s(): fclose() %s, fp = 0x%08x\n\r", __FUNCTION__, g_log_out.path, g_log_out.fp);
			memset(g_log_out.path, 0, LOG_MAX_PATH);
			fclose((FILE *)g_log_out.fp);
			g_log_out.fp = NULL;

			LOG_OUT_DST_CUR_OFF(LOG_OUT_HOST_FILE);
			LOG_OUT_DST_CLOSE(LOG_OUT_HOST_FILE);
		}
		return true;
	}
	// should never reach here!!
	return false;
}

bool	_sim_out_dst_turn_on(u8 _dst)
{
	ASSERT((_dst == LOG_OUT_HOST_TERM) || (_dst == LOG_OUT_HOST_FILE));
	if (_dst == LOG_OUT_HOST_TERM)
	{
		if (LOG_OUT_DST_IS_CUR_ON(LOG_OUT_HOST_TERM) == false)
			LOG_OUT_DST_CUR_ON(LOG_OUT_HOST_TERM);

		return true;
	}
	if (_dst == LOG_OUT_HOST_FILE)
	{
		if (LOG_OUT_DST_IS_CUR_ON(LOG_OUT_HOST_FILE) == false)
		{
			if (g_log_out.fp == NULL)
			{
				LOG_ERROR("%s(): g_log_out.fp = NULL!", __FUNCTION__);
				return false;
			}
			LOG_OUT_DST_CUR_ON(LOG_OUT_HOST_FILE);
			return true;
		}
		return true;
	}
	// should never reach here!!
	return false;
}

bool	_sim_out_dst_turn_off(u8 _dst)
{
	ASSERT((_dst == LOG_OUT_HOST_TERM) || (_dst == LOG_OUT_HOST_FILE));
	if (_dst == LOG_OUT_HOST_TERM)
	{
		if (LOG_OUT_DST_IS_CUR_ON(LOG_OUT_HOST_TERM))
			LOG_OUT_DST_CUR_OFF(LOG_OUT_HOST_TERM);
		return true;
	}
	if (_dst == LOG_OUT_HOST_FILE)
	{
		if (LOG_OUT_DST_IS_CUR_ON(LOG_OUT_HOST_FILE))
		{
			if (g_log_out.fp == NULL)
			{
				LOG_ERROR("%s(): g_log_out.fp = NULL!", __FUNCTION__);
				return false;
			}
			LOG_OUT_DST_CUR_OFF(LOG_OUT_HOST_FILE);
			return true;
		}
		return true;
	}
	// should never reach here!!
	return false;
}

/* ============================================================================
		exported functions on host side
============================================================================ */
void log_printf(const char *fmt, ...)
{
	va_list		args;

	memset(s_prn_buf, 0, PRN_BUF_MAX);
	memset(s_prn_tmp, 0, PRN_BUF_MAX);
	memset(s_tag_lvl, 0, PRN_TAG_MAX);
	memset(s_tag_mod, 0, PRN_TAG_MAX);
	memset(s_tag_fl,  0, PRN_TAG_MAX);

	LOG_MUTEX_LOCK();
	va_start(args, fmt);
	vsprintf((char *)s_prn_buf, fmt, args);
	va_end(args);

	printf("%s", s_prn_buf);

	if (LOG_OUT_DST_IS_CUR_ON(LOG_OUT_HOST_FILE))
	{
		fprintf((FILE *)g_log_out.fp, "%s", s_prn_buf);
		fflush((FILE *)g_log_out.fp);
	}
	LOG_MUTEX_UNLOCK();
	return;
}


// exec hcmd on host size
bool	LOG_host_exec_hcmd(log_hcmd_st *hcmd)
{
	u8		cmd = hcmd->cmd;
	u8		idx = hcmd->data;
	bool	rval = true;
	log_hcmd_st		_hcmd;

	if (hcmd->target == LOG_HCMD_TARGET_SOC)
	{
		memcpy(&_hcmd, hcmd, sizeof(log_hcmd_st));
		ssv6xxx_wifi_ioctl(SSV6XXX_HOST_CMD_LOG, &_hcmd, sizeof(log_hcmd_st));
		return true;
	}

	// below : hcmd[0] = LOG_HCMD_TARGET_HOST
	if (cmd == LOG_HCMD_PRINT_USAGE)
	{
		_log_print_usage();
		return true;
	}
	if (cmd == LOG_HCMD_PRINT_MODULE)
	{
		_log_list_all_module();
		return true;
	}

	switch (cmd)
	{
	case LOG_HCMD_PRINT_STATUS:									break;
	case LOG_HCMD_TAG_ON_FL:		LOG_FILELINE_TURN_ON();		break;
	case LOG_HCMD_TAG_OFF_FL:		LOG_FILELINE_TURN_OFF();	break;
	case LOG_HCMD_TAG_ON_LEVEL:		LOG_TAG_LEVEL_TURN_ON();	break;
	case LOG_HCMD_TAG_OFF_LEVEL:	LOG_TAG_LEVEL_TURN_OFF();	break;
	case LOG_HCMD_TAG_ON_MODULE:	LOG_TAG_MODULE_TURN_ON();	break;
	case LOG_HCMD_TAG_OFF_MODULE:	LOG_TAG_MODULE_TURN_OFF();	break;
	case LOG_HCMD_MODULE_ON:		LOG_MODULE_TURN_ON(idx);	break;
	case LOG_HCMD_MODULE_OFF:
		LOG_MODULE_TURN_OFF(idx);
		if (idx == LOG_MODULE_ALL)
			LOG_MODULE_TURN_ON(LOG_MODULE_EMPTY);
		break;
	case LOG_HCMD_LEVEL_SET:		LOG_LEVEL_SET(idx);			break;
	default:						rval = false;				break;
	}

	if (rval)
	{
		_log_prnf_cfg_dump(&g_log_prnf_cfg);
		_log_out_desc_dump(&g_log_out);
	}
	return rval;
}

// after recieving soc evt from soc side, exec it on host side
void	LOG_host_exec_soc_evt(log_soc_evt_st *soc_evt)
{
	u16	len = soc_evt->len;
	u16	cmd = soc_evt->cmd;
	log_evt_prnf_buf_st		*prnbuf;
	log_evt_prnf_st			*prnf;

	if (cmd == LOG_SEVT_STATUS_REPORT)
	{
		LOG_PRINTF("%s, len = %d\n", "LOG_SEVT_STATUS_REPORT", len);
		//ssv6xxx_raw_dump((s8 *)soc_evt, len);
		_log_host_print_status((log_evt_status_report_st *)(&(soc_evt->data)));
		return;
	}

	if (cmd == LOG_SEVT_EXEC_FAIL)
	{
		LOG_PRINTF("soc report 'LOG_SEVT_EXEC_FAIL' to host\n");
		LOG_PRINTF("=> soc has something wrong while exec the cmd.\n");
		return;
	}

	if (cmd == LOG_SEVT_PRNF_BUF)
	{
		LOG_PRINTF("%s, len = %d\n", "LOG_SEVT_PRNF_BUF", len);
		prnbuf = (log_evt_prnf_buf_st *)soc_evt->data;
		_log_evt_prn_buf_dump(prnbuf);
		// ssv6xxx_raw_dump((s8 *)prnbuf, prn->len);

		if ((prnbuf->dst_open & LOG_OUT_SOC_HOST_TERM) &&
			(prnbuf->dst_cur  & LOG_OUT_SOC_HOST_TERM))
			_prnf_soc_host_term((char *)(prnbuf->buf));

		return;
	}

	if (cmd == LOG_SEVT_PRNF)
	{
		//LOG_PRINTF("%s, len = %d\n", "LOG_SEVT_PRNF", len);
		prnf = (log_evt_prnf_st *)soc_evt->data;
		//_log_evt_prnf_dump(prnf);
		//ssv6xxx_raw_dump((s8 *)prnf, prnf->len);
		log_host_exec_evt_prnf(prnf);

		return;
	}

	if (cmd == LOG_SEVT_PRNF_CFG_SET)
	{
		LOG_PRINTF("%s, len = %d\n", "LOG_SEVT_PRNF_CFG_SET", len);
		return;
	}
}

// convert cli cmd to host cmd
bool	LOG_cli2hcmd(s32 argc, char *argv[], log_hcmd_st	*hcmd)
{
	char	str[32] = "";
	char	cli_str[64] = "";
	s32		idx;
	u16		i;

	memset(hcmd, 0, sizeof(log_hcmd_st));
	for (i = 0; i < argc; i++)
	{
		strcat(cli_str, argv[i]);
		if (i != argc - 1)
			strcat(cli_str, " ");
	}

	if (strcmp(cli_str, "log") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_HOST;
		hcmd->cmd		= LOG_HCMD_PRINT_USAGE;
		return true;
	}
	if (strcmp(cli_str, "log mod") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_HOST;
		hcmd->cmd		= LOG_HCMD_PRINT_MODULE;
		return true;
	}
	if (strcmp(cli_str, "log st") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_HOST;
		hcmd->cmd		= LOG_HCMD_PRINT_STATUS;
		return true;
	}
	if (strcmp(cli_str, "log soc st") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_SOC;
		hcmd->cmd		= LOG_HCMD_PRINT_STATUS;
		return true;
	}
	if (strcmp(cli_str, "log on fl") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_HOST;
		hcmd->cmd		= LOG_HCMD_TAG_ON_FL;
		return true;
	}
	if (strcmp(cli_str, "log off fl") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_HOST;
		hcmd->cmd		= LOG_HCMD_TAG_OFF_FL;
		return true;
	}
	if (strcmp(cli_str, "log on lvl") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_HOST;
		hcmd->cmd		= LOG_HCMD_TAG_ON_LEVEL;
		return true;
	}
	if (strcmp(cli_str, "log off lvl") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_HOST;
		hcmd->cmd		= LOG_HCMD_TAG_OFF_LEVEL;
		return true;
	}
	if (strcmp(cli_str, "log soc on lvl") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_SOC;
		hcmd->cmd		= LOG_HCMD_TAG_ON_LEVEL;
		return true;
	}
	if (strcmp(cli_str, "log soc off lvl") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_SOC;
		hcmd->cmd		= LOG_HCMD_TAG_OFF_LEVEL;
		return true;
	}
	if (strcmp(cli_str, "log on mod") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_HOST;
		hcmd->cmd		= LOG_HCMD_TAG_ON_MODULE;
		return true;
	}
	if (strcmp(cli_str, "log off mod") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_HOST;
		hcmd->cmd		= LOG_HCMD_TAG_OFF_MODULE;
		return true;
	}
	if (strcmp(cli_str, "log soc on mod") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_SOC;
		hcmd->cmd		= LOG_HCMD_TAG_ON_MODULE;
		return true;
	}
	if (strcmp(cli_str, "log soc off mod") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_SOC;
		hcmd->cmd		= LOG_HCMD_TAG_OFF_MODULE;
		return true;
	}
	// turn on/off all module...
	if (strcmp(cli_str, "log on all") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_HOST;
		hcmd->cmd		= LOG_HCMD_MODULE_ON;
		hcmd->data		= LOG_MODULE_ALL;
		return true;
	}
	if (strcmp(cli_str, "log off all") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_HOST;
		hcmd->cmd		= LOG_HCMD_MODULE_OFF;
		hcmd->data		= LOG_MODULE_ALL;
		return true;
	}
	if (strcmp(cli_str, "log soc on all") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_SOC;
		hcmd->cmd		= LOG_HCMD_MODULE_ON;
		hcmd->data		= LOG_MODULE_ALL;
		return true;
	}
	if (strcmp(cli_str, "log soc off all") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_SOC;
		hcmd->cmd		= LOG_HCMD_MODULE_OFF;
		hcmd->data		= LOG_MODULE_ALL;
		return true;
	}
	// log on [mod]
	if ((argc == 3) && (strcmp(argv[1], "on") == 0))
	{
		strcpy(str, argv[2]);
		ssv6xxx_str_tolower(str);
		if ((idx = _log_mod_str2idx(str)) > 0)
		{
			hcmd->target		= LOG_HCMD_TARGET_HOST;
			hcmd->cmd		= LOG_HCMD_MODULE_ON;
			hcmd->data		= idx;
			return true;
		}
		return false;
	}
	// log off [mod]
	if ((argc == 3) && (strcmp(argv[1], "off") == 0))
	{
		strcpy(str, argv[2]);
		ssv6xxx_str_tolower(str);
		if ((idx = _log_mod_str2idx(str)) > 0)
		{
			hcmd->target		= LOG_HCMD_TARGET_HOST;
			hcmd->cmd		= LOG_HCMD_MODULE_OFF;
			hcmd->data		= idx;
			return true;
		}
		return false;
	}
	// log soc on  [mod]
	if ((argc == 4) && (strcmp(argv[1], "soc") == 0) && (strcmp(argv[2], "on") == 0))
	{
		strcpy(str, argv[3]);
		ssv6xxx_str_tolower(str);
		if ((idx = _log_mod_str2idx(str)) > 0)
		{
			hcmd->target		= LOG_HCMD_TARGET_SOC;
			hcmd->cmd		= LOG_HCMD_MODULE_ON;
			hcmd->data		= idx;
			return true;
		}
		return false;
	}
	// log soc off [mod]
	if ((argc == 4) && (strcmp(argv[1], "soc") == 0) && (strcmp(argv[2], "off") == 0))
	{
		strcpy(str, argv[3]);
		ssv6xxx_str_tolower(str);
		if ((idx = _log_mod_str2idx(str)) > 0)
		{
			hcmd->target		= LOG_HCMD_TARGET_SOC;
			hcmd->cmd		= LOG_HCMD_MODULE_OFF;
			hcmd->data		= idx;
			return true;
		}
		return false;
	}

	if (strcmp(cli_str, "log set on") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_HOST;
		hcmd->cmd		= LOG_HCMD_LEVEL_SET;
		hcmd->data		= LOG_LEVEL_ON;
		return true;
	}
	if (strcmp(cli_str, "log set off") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_HOST;
		hcmd->cmd		= LOG_HCMD_LEVEL_SET;
		hcmd->data		= LOG_LEVEL_OFF;
		return true;
	}
	// log set [lvl]
	if ((argc == 3) && (strcmp(argv[1], "set") == 0) )
	{
		strcpy(str, argv[2]);
		ssv6xxx_str_toupper(str);
		if ((idx = _log_lvl_str2idx(str)) > 0)
		{
			hcmd->target		= LOG_HCMD_TARGET_HOST;
			hcmd->cmd		= LOG_HCMD_LEVEL_SET;
			hcmd->data		= idx;
			return true;
		}
		return false;
	}

	// log soc set [lvl]
	if (strcmp(cli_str, "log soc set on") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_SOC;
		hcmd->cmd		= LOG_HCMD_LEVEL_SET;
		hcmd->data		= LOG_LEVEL_ON;
		return true;
	}
	if (strcmp(cli_str, "log soc set off") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_SOC;
		hcmd->cmd		= LOG_HCMD_LEVEL_SET;
		hcmd->data		= LOG_LEVEL_OFF;
		return true;
	}
	// log soc set [lvl]
	if ((argc == 4) && (strcmp(argv[1], "soc") == 0) && (strcmp(argv[2], "set") == 0))
	{
		strcpy(str, argv[3]);
		ssv6xxx_str_toupper(str);
		if ((idx = _log_lvl_str2idx(str)) > 0)
		{
			hcmd->target		= LOG_HCMD_TARGET_SOC;
			hcmd->cmd		= LOG_HCMD_LEVEL_SET;
			hcmd->data		= idx;
			return true;
		}
		return false;
	}

	if (strcmp(cli_str, "log soc test") == 0)
	{
		hcmd->target		= LOG_HCMD_TARGET_SOC;
		hcmd->cmd		= LOG_HCMD_TEST;
		return true;
	}

	return false;
}

// 1. always print out, skip the influence of 'level' & 'module' filter.
// 2. always print out file & line info.
void sim_fatal(u32 m, const char *file, u32 line, const char *fmt, ...)
{
#if (CONFIG_LOG_ENABLE)
	va_list args;
	char	*p = (char *)s_prn_buf;

	//memset(s_acc_buf, 0, ACC_BUF_MAX);

	LOG_MUTEX_LOCK();
	sprintf(s_tag_lvl, "[%-5s] ", g_log_lvl_tag[LOG_LEVEL_FATAL]);
	if (strcmp(g_log_mod_tag[m], "") != 0)
		sprintf(s_tag_mod, "<%-5s> ", g_log_mod_tag[m]);

	sprintf(s_tag_fl, "(%s #%4d) ", _extract_filename(file), line);

	p = (char *)s_prn_buf + strlen(s_prn_buf);
	va_start(args, fmt);
	vsprintf(p, fmt, args);
	va_end(args);

	// note : fatal printf will always print string to terminal
	p = (char *)s_prn_buf + strlen(s_prn_buf);
	strcat(p, "\nprogram halt!!!\n");
	printf("%s", s_prn_buf);
	_prnf_soc_host_term(s_prn_buf);

	if (LOG_OUT_DST_IS_CUR_ON(LOG_OUT_HOST_FILE))
	{
		fprintf((FILE *)g_log_out.fp, "%s", s_prn_buf);
		fflush((FILE *)g_log_out.fp);
	}
	LOG_MUTEX_UNLOCK();
	halt();
#endif
	return;
}


// the device's trace functions output only the identifiers instead of entire strings
void sim_printf(u32 n, u32 m, const char *file, u32 line, const char *fmt, ...)
{
#if (CONFIG_LOG_ENABLE)
	va_list args;
	char	*p = (char *)s_prn_buf;

	LOG_MUTEX_LOCK();
	//assert(0 <= (n) && (n) <= LOG_LEVEL_FATAL);
	if ((n < g_log_prnf_cfg.lvl) || ((g_log_prnf_cfg.mod & LOG_MODULE_MASK(m)) == 0))
	{
		LOG_MUTEX_UNLOCK();
		return;
	}

	memset(s_prn_buf, 0, PRN_BUF_MAX);
	memset(s_prn_tmp, 0, PRN_BUF_MAX);
	memset(s_tag_lvl, 0, PRN_TAG_MAX);
	memset(s_tag_mod, 0, PRN_TAG_MAX);
	memset(s_tag_fl,  0, PRN_TAG_MAX);

	if (!g_log_prnf_cfg.prn_tag_sprs)
	{
		if (g_log_prnf_cfg.prn_tag_lvl)
		{
			if (strcmp(g_log_lvl_tag[n], "") != 0)
				sprintf(s_tag_lvl, "[%-5s] ", g_log_lvl_tag[n]);
		}
		if (g_log_prnf_cfg.prn_tag_mod)
		{
			if (strcmp(g_log_mod_tag[m], "") != 0)
				sprintf(s_tag_mod, "<%-5s> ", g_log_mod_tag[m]);
		}
		if (g_log_prnf_cfg.fl)
			sprintf(s_tag_fl, "(%s #%4d) ", (char *)_extract_filename(file), line);
	}
	sprintf(s_prn_buf, "%s%s%s", s_tag_lvl, s_tag_mod, s_tag_fl);
	p += strlen(s_prn_buf);

	va_start(args, fmt);
	vsprintf(p, fmt, args);
	va_end(args);
	if (LOG_OUT_DST_IS_CUR_ON(LOG_OUT_HOST_TERM))
		printf("%s", s_prn_buf);

	if (LOG_OUT_DST_IS_CUR_ON(LOG_OUT_HOST_FILE))
	{
		fprintf((FILE *)g_log_out.fp, "%s", s_prn_buf);
		fflush((FILE *)g_log_out.fp);
	}
	LOG_MUTEX_UNLOCK();
#endif
}

log_dbgmsg_st		g_log_dmsg;
log_dbgmsg_st		g_log_bin;

bool	LOG_dmsg_load(char path[256])
{
	memset(&g_log_dmsg, 0, sizeof(log_dbgmsg_st));
	g_log_dmsg.is_load	= false;
	g_log_dmsg.buf		= NULL;

	if ((g_log_dmsg.fp = fopen(path, "rb")) == NULL)
	{
		LOG_FAIL("%s(): fopen(%s)\n", __FUNCTION__, path);
		goto fail_exit;
	}
	if (fseek(g_log_dmsg.fp, 0, SEEK_END) != 0)
	{
		LOG_FAIL("%s(): fail to locate the 4 bytes addr in dmsg file\n", __FUNCTION__);
		goto fail_exit;
	}
	g_log_dmsg.size	= ftell(g_log_dmsg.fp);
	g_log_dmsg.buf	= (u8 *)malloc(g_log_dmsg.size);

	fseek(g_log_dmsg.fp, 0, SEEK_SET);
	if (fread(g_log_dmsg.buf, g_log_dmsg.size, 1, g_log_dmsg.fp) != 1)
	{
		LOG_FAIL("%s(): fail to read the dmsg file\n", __FUNCTION__);
		goto fail_exit;
	}
	memcpy(&g_log_dmsg.addr, (g_log_dmsg.buf + g_log_dmsg.size - 4), 4);

	if (g_log_dmsg.fp != NULL)
		fclose(g_log_dmsg.fp);

	g_log_dmsg.is_load = true;
	return true;

fail_exit:
	if (g_log_dmsg.fp != NULL)
		fclose(g_log_dmsg.fp);
	return false;
}

//
// get the arg type pointed by 'pct' inside a string
//
// percent : char pointer to '%' inside the string
// max_len : the remaining len to the end of string
//
// return :
//	> 0 : success & the type
//	= 0 : fail
//
#define ARG_ERROR		0x00
#define ARG_CHAR		0x01
#define ARG_NUMBER		0x02
#define ARG_STRING		0x04
u8		_log_get_arg_type(char *percent, u32 max_len)
{
	u8		rval = ARG_ERROR;
	char	*p = percent;

	while ((p - percent) < (s32)max_len)
	{
		p++;
		if (((*p) == 's') || ((*p) == 'S'))
		{
			rval = ARG_STRING;
			goto out;
		}
		if (((*p) == 'd') || ((*p) == 'u') ||
			((*p) == 'x') || ((*p) == 'X'))
		{
			rval = ARG_NUMBER;
			goto out;
		}
		if ((*p) == 'c')
		{
			rval = ARG_CHAR;
			goto out;
		}
	}

out:
	return rval;
}

bool	log_host_exec_evt_prnf(log_evt_prnf_st *prnf)
{
#if 0
	char	*fmt = (char *)(*(u32 *)(prnf->buf));
#else
	u32		fmt_offset = (*(u32 *)(prnf->buf)) - g_log_dmsg.addr;
	char	*fmt = (char *)(g_log_dmsg.buf + fmt_offset);
#endif

	u32		fmt_len = strlen(fmt);

	u16		i_arg;
	u8		arg_type;
	u8		*arg_buf = prnf->buf + 4;
	u32		arg_cnt = prnf->arg_len / 4;
	char	*arg_str;

	char	*p_cur, *p_next;		// the cur & next '%' position in fmt
	u32		i_out;
	char	strtmp[32];
	char	strout[256];

	//log_printf("%s()<=: fmt_offset = %d, fmt = %s\n\r", __FUNCTION__, fmt_offset, fmt);
	memset(strout, 0, 256);

	// decorated strings
	if (!prnf->prnf_cfg.chk_tag_sprs)
	{
		if ((prnf->lvl < prnf->prnf_cfg.lvl) || ((prnf->prnf_cfg.mod & LOG_MODULE_MASK(prnf->mod)) == 0))
		{
			return true;
		}
	}
	//memset(s_acc_buf, 0, ACC_BUF_MAX);
	if (!prnf->prnf_cfg.prn_tag_sprs)
	{
		if (prnf->prnf_cfg.prn_tag_lvl)
		{
			if (strcmp(g_log_lvl_tag[prnf->lvl], "") != 0)
			{
				sprintf(s_tag_lvl, "[%-5s] ", g_log_lvl_tag[prnf->lvl]);
				// p += strlen(p);
			}
		}
		if (prnf->prnf_cfg.prn_tag_mod)
		{
			if (strcmp(g_log_mod_tag[prnf->mod], "") != 0)
			{
				sprintf(s_tag_mod, "<%-5s> ", g_log_mod_tag[prnf->mod]);
				// p += strlen(p);
			}
		}
	}
	sprintf(strout, "%s%s", s_tag_lvl, s_tag_mod);

	// string with 0 args
	if (prnf->arg_len == 0)
	{
		strcpy(strout, fmt);
		goto do_print;
	}

	// else, string with > 1 args ...
	i_out = strlen(strout);
	// (a) print the string before #1 arg
	p_cur = fmt;
	if ((p_next = strchr(p_cur, '%')) == NULL)
	{
		LOG_ERROR("%s(): fail to get %% pos during heading string printing\n", __FUNCTION__);
		return false;
	}
	memcpy(&(strout[i_out]), p_cur, p_next-p_cur);
	i_out += (p_next - p_cur);
	// (b) print each string between #N arg
	i_arg = 0;
	while (i_arg < arg_cnt)
	{
		arg_type = _log_get_arg_type(p_next, (fmt_len - (p_next - fmt)));
		if (arg_type == ARG_ERROR)
		{
			LOG_ERROR("%s(): _log_get_arg_type() fail!\n", __FUNCTION__);
			return false;
		}
		p_cur = p_next;

		memset(strtmp, 0, 32);
		p_next = strchr(p_cur+1, '%');
		if (p_next == 0)			// reach the end of string
			strcpy(strtmp, p_cur);
		else
			memcpy(strtmp, p_cur, p_next - p_cur);

		if ((arg_type == ARG_NUMBER) || (arg_type == ARG_CHAR))
		{
			//log_printf("%3d : arg_type = %d (NUM, CHAR)\n\r", i_arg, arg_type);
			sprintf(&(strout[i_out]), strtmp,         (*(int *)(arg_buf + i_arg*4)));
		}

		if (arg_type == ARG_STRING)
		{
			//log_printf("%3d : arg_type = %d (STRING)\n\r", i_arg, arg_type);
			//log_printf("<1> (*(u32 *)(arg_buf + i_arg*4)) = 0x%08x\n\r", (*(u32 *)(arg_buf + i_arg*4)));
			//log_printf("<2> g_log_dmsg.addr               = 0x%08x\n\r", g_log_dmsg.addr);
			//log_printf("                  ===> <1> - <2>  = 0x%08x\n\r", ((*(u32 *)(arg_buf + i_arg*4)) - g_log_dmsg.addr));
			arg_str = (char *)(g_log_dmsg.buf + ((*(u32 *)(arg_buf + i_arg*4)) - g_log_dmsg.addr));
			//log_printf("arg_str = %s\n\r", arg_str);
			sprintf(&(strout[i_out]), strtmp, arg_str);
		}

		i_out = strlen(strout);		// put 'i_out' to the end of 'strout'
		i_arg++;
	}

do_print:
	_prnf_soc_host_term(strout);
	return true;
}

void _log_evt_prnf_dump(log_evt_prnf_st *prnf)
{
	u32		i;
	u32		arg_cnt;

	log_printf("< %s >\n", __FUNCTION__);
	log_printf("len     = %d\n", prnf->len);
	log_printf("lvl     = %d\n", prnf->lvl);
    log_printf("mod     = %d\n", prnf->mod);
	_log_prnf_cfg_dump(&(prnf->prnf_cfg));

	log_printf("fmt_len = %d, offsetof = %d\n", prnf->fmt_len, sizeof(log_evt_prnf_st));
	log_printf("arg_len = %d\n", prnf->arg_len);

	arg_cnt = prnf->arg_len / 4;
	log_printf("arg_cnt = %d\n", arg_cnt);

	memcpy(&i, prnf->buf, sizeof(u32));	// just for removing compile warnings in soc toolchain
	log_printf("fmt : 0x%08x\n", i);

	for (i=0; i<arg_cnt; i++)			// including fmt, hence, +4
		log_printf("%03d : 0x%08x\n", i, *(int *)(prnf->buf + 4 + i*4));
}

void	_log_soc_send_evt_prnf_cfg_set(const u8 *p)
{
	u16				len;
	MsgEvent		*msg_evt;
	log_soc_evt_st	*soc_evt;

	len = sizeof(log_soc_evt_st) + sizeof(log_evt_prnf_cfg_set_st);
	LOG_EVENT_ALLOC(msg_evt, SOC_EVT_LOG, len);

	soc_evt = (log_soc_evt_st *)(LOG_EVENT_DATA_PTR(msg_evt));
	memset(soc_evt, 0, len);
	soc_evt->len = len;
	soc_evt->cmd = LOG_SEVT_PRNF_CFG_SET;

	memcpy(&(soc_evt->data), p, sizeof(log_evt_prnf_cfg_set_st));
	LOG_EVENT_SET_LEN(msg_evt, len);
#if 0
	log_printf("%s()<=\n", __FUNCTION__);
	log_printf("soc_evt->len = %d\n", soc_evt->len);
	ssv6xxx_raw_dump((s8 *)soc_evt, soc_evt->len);
	log_printf("%s()=>\n", __FUNCTION__);
#endif
	LOG_EVENT_SEND(msg_evt);

	return;
}

void _t_printf(u32 n, u32 m, const char *fmt, ...)
{
	log_evt_prnf_st	*prnf;
	MsgEvent		*msg_evt;
	log_soc_evt_st	*soc_evt;
	va_list			args;
	u32				p, q;
	u32				arg_cnt;
	bool			touch_magic;
	u32				i;

	//printf("%s()<=, n=%d, m=%d\n\r", __FUNCTION__, n, m);

	//=> filter the level & mod af first
	if (!g_log_prnf_cfg.chk_tag_sprs)
	{
		if ((n < g_log_prnf_cfg.lvl) || ((g_log_prnf_cfg.mod & LOG_MODULE_MASK(m)) == 0))
		{
			return;
		}
	}
	//=> msg evt alloc & setting
	LOG_EVENT_ALLOC(msg_evt, SOC_EVT_LOG, LOG_EVT_PRNF_MAXLEN);
	soc_evt = (log_soc_evt_st *)LOG_EVENT_DATA_PTR(msg_evt);
	memset(soc_evt, 0, LOG_EVT_PRNF_MAXLEN);
	LOG_EVENT_SET_LEN(msg_evt, LOG_EVT_PRNF_MAXLEN);

	prnf = (log_evt_prnf_st *)(&(soc_evt->data));

	//=> format string info
	prnf->fmt_len	 = 4;
	*(prnf->buf)		 = (u8)((u32)fmt);
	*(prnf->buf + 1) = (u8)((u32)fmt >>  8);
	*(prnf->buf + 2) = (u8)((u32)fmt >> 16);
	*(prnf->buf + 3) = (u8)((u32)fmt >> 24);
	//printf("fmt : 0x%08x, buf[] = 0x%02x%02x%02x%02x\n", (u32)fmt, prnf->buf[0], prnf->buf[1], prnf->buf[2], prnf->buf[3]);
	//=> arg list
	va_start(args, fmt);
	q = 0;	touch_magic = false;	arg_cnt = 0;
	while (arg_cnt < LOG_DMSG_MAX_ARGS)
	{
		i = prnf->fmt_len + arg_cnt*4;
		p = (u32)va_arg(args, u32);
		if (p == (u32)g_magic_str)
		{
			//printf("%03d : %s (magic str!!)\n", arg_cnt, p);
			q = (u32)va_arg(args, u32);		// lookahead for g_magic_u32
			if (q == g_magic_u32)
			{
				touch_magic = true;
				break;
			}
			else
			{
				prnf->buf[i]   = (u8)(p);
				prnf->buf[i+1] = (u8)(p >>  8);
				prnf->buf[i+2] = (u8)(p >> 16);
				prnf->buf[i+3] = (u8)(p >> 24);
				//printf("%03d : 0x%08x, buf[] = 0x%02x%02x%02x%02x\n", arg_cnt, p, prnf->buf[i], prnf->buf[i+1], prnf->buf[i+2], prnf->buf[i+3]);
				arg_cnt++;

				prnf->buf[i]   = (u8)(q);
				prnf->buf[i+1] = (u8)(q >>  8);
				prnf->buf[i+2] = (u8)(q >> 16);
				prnf->buf[i+3] = (u8)(q >> 24);
				//printf("%03d : 0x%08x, buf[] = 0x%02x%02x%02x%02x\n", arg_cnt, q, prnf->buf[i], prnf->buf[i+1], prnf->buf[i+2], prnf->buf[i+3]);
				arg_cnt++;
				q = 0;
			}
		}
		else
		{
			prnf->buf[i]	   = (u8)(p);
			prnf->buf[i+1] = (u8)(p >>  8);
			prnf->buf[i+2] = (u8)(p >> 16);
			prnf->buf[i+3] = (u8)(p >> 24);
			//printf("%03d : 0x%08x, buf[] = 0x%02x%02x%02x%02x\n", arg_cnt, p, prnf->buf[i], prnf->buf[i+1], prnf->buf[i+2], prnf->buf[i+3]);
			arg_cnt++;
		}
	};
	va_end(args);

	if (!touch_magic)
	{
		//printf("%s()=>: not touch_magic !! return\n", __FUNCTION__);
		LOG_EVENT_FREE(msg_evt);
		return;
	}
	prnf->arg_len	= arg_cnt*4;
	//=> cal the total length
	prnf->len		= prnf->fmt_len + prnf->arg_len + sizeof(log_evt_prnf_st);

	//=> lvl & mod
	prnf->lvl = n;
	prnf->mod = m;

	//=> prnf cfg
	// memcpy(&(prnf->prnf_cfg), &g_log_prnf_cfg, sizeof(log_prnf_cfg_st));
    prnf->prnf_cfg.lvl			= g_log_prnf_cfg.lvl;
    prnf->prnf_cfg.mod			= g_log_prnf_cfg.mod;
    prnf->prnf_cfg.fl			= g_log_prnf_cfg.fl;
    prnf->prnf_cfg.prn_tag_lvl  = g_log_prnf_cfg.prn_tag_lvl;
    prnf->prnf_cfg.prn_tag_mod  = g_log_prnf_cfg.prn_tag_mod;
    prnf->prnf_cfg.prn_tag_sprs = g_log_prnf_cfg.prn_tag_sprs;
    prnf->prnf_cfg.chk_tag_sprs = g_log_prnf_cfg.chk_tag_sprs;

	//_log_evt_prnf_dump(prnf);
	//ssv6xxx_raw_dump((s8 *)prnf, prnf->len);

	// => cal soc evt's length & set cmd id
	soc_evt->len = prnf->len + LOG_SEVT_CMD_DATA_OFFSET;
	soc_evt->cmd = LOG_SEVT_PRNF;

	LOG_EVENT_SEND(msg_evt);
	//printf("%s()=>: \n", __FUNCTION__);
	return;
}



