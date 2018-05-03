/*
 * ELASTICSEARCH CONFIDENTIAL
 * __________________
 *
 *  [2014] Elasticsearch Incorporated. All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of Elasticsearch Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Elasticsearch Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Elasticsearch Incorporated.
 */

#ifndef __LOG_H__
#define __LOG_H__

#include <errno.h>
#include <string.h>
#include <assert.h>

#include "util.h"
#include "error.h"
#include "handles.h"


/*
 * Descriptors to be used with logging for SQLWCHAR pointer type.
 * "Log Wchar Pointer Descriptor [with Length]"
 */
#ifdef UNICODE
#define LWPD	PFWP_DESC
#define LWPDL	PFWP_LDESC
#else /* UNICODE */
#define LWPD	PFCP_DESC
#define LWPDL	PFCP_LDESC
#endif /* UNICODE */

/*
 * Descriptors to be used with logging for SQLTCHAR pointer type.
 * "Log Tchar Pointer Descriptor [with Length]"
 */
#ifdef UNICODE
#define LTPD	PFWP_DESC
#define LTPDL	PFWP_LDESC
#else /* UNICODE */
#define LTPD	PFCP_DESC
#define LTPDL	PFCP_LDESC
#endif /* UNICODE */

/* macro for logging of wstr_st objects */
#define LWSTR(_wptr)	(int)(_wptr)->cnt, (_wptr)->str
#define LTSTR(_wptr)	(int)(_wptr)->cnt, (_wptr)->str



/* Note: keep in sync with __ESODBC_LVL2STR */
enum osodbc_log_levels {

#define LOG_LEVEL_DISABLED	-1

	LOG_LEVEL_ERR = 0,
	LOG_LEVEL_WARN,
	LOG_LEVEL_INFO,
	LOG_LEVEL_DBG,
};

BOOL log_init();
void log_cleanup();

void _esodbc_log(int lvl, int werrno, const char *func, 
		const char *srcfile, int lineno, const char *fmt, ...);
extern int _esodbc_log_level;

#define _LOG(lvl, werr, fmt, ...) \
	if ((lvl) <= _esodbc_log_level) \
		_esodbc_log(lvl, werr, __func__, __FILE__, __LINE__, fmt, __VA_ARGS__)


#define LOG(lvl, fmt, ...)	_LOG(lvl, 0, fmt, __VA_ARGS__)

#define ERRN(fmt, ...)	_LOG(LOG_LEVEL_ERR, 1, fmt, __VA_ARGS__)
#define ERR(fmt, ...)	LOG(LOG_LEVEL_ERR, fmt, __VA_ARGS__)
#define WARN(fmt, ...)	LOG(LOG_LEVEL_WARN, fmt, __VA_ARGS__)
#define INFO(fmt, ...)	LOG(LOG_LEVEL_INFO, fmt, __VA_ARGS__)
#define DBG(fmt, ...)	LOG(LOG_LEVEL_DBG, fmt, __VA_ARGS__)

#define BUG(fmt, ...) \
	do { \
		ERR("[BUG] " fmt, __VA_ARGS__); \
		assert(0); \
	} while (0)

#define FIXME	BUG("not yet implemented")
#define TRACE	DBG("===== TR4C3 =====");

#define TS_NULL	MK_WPTR("<null>")


/*
 * Logging with handle
 */

/* get handle type prefix  */
static inline char* _hhtype2str(void *handle)
{
	if (! handle)
		return "";
	switch (HDRH(handle)->type) {
		case SQL_HANDLE_ENV: return "ENV";
		case SQL_HANDLE_DBC: return "DBC";
		case SQL_HANDLE_STMT: return "STMT";
		case SQL_HANDLE_DESC: return "DESC";
	}
	/* likely mem corruption, it'll probably crash */
	BUG("unknown handle (@0x%p) type (%d)", handle, HDRH(handle)->type);
	return "???";
}

#define LOGH(lvl, werrn, hnd, fmt, ...) \
	_LOG(lvl, werrn, "[%s@0x%p] " fmt, _hhtype2str(hnd), hnd, __VA_ARGS__)

#define ERRNH(hnd, fmt, ...)	LOGH(LOG_LEVEL_ERR, 1, hnd, fmt, __VA_ARGS__)
#define ERRH(hnd, fmt, ...)		LOGH(LOG_LEVEL_ERR, 0, hnd, fmt, __VA_ARGS__)
#define WARNH(hnd, fmt, ...)	LOGH(LOG_LEVEL_WARN, 0, hnd, fmt, __VA_ARGS__)
#define INFOH(hnd, fmt, ...)	LOGH(LOG_LEVEL_INFO, 0, hnd, fmt, __VA_ARGS__)
#define DBGH(hnd, fmt, ...)		LOGH(LOG_LEVEL_DBG, 0, hnd, fmt, __VA_ARGS__)

#define BUGH(hnd, fmt, ...) \
	do { \
		ERRH(hnd, "[BUG] " fmt, __VA_ARGS__); \
		assert(0); \
	} while (0)




#endif /* __LOG_H__ */

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
