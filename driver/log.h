/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#ifndef __LOG_H__
#define __LOG_H__

#include <errno.h>
#include <string.h>
#include <assert.h>

#include "util.h"
#include "error.h"


/*
 * Descriptors to be used with logging for SQLWCHAR pointer type.
 * "Log Wchar Pointer Descriptor [with Length]"
 */
#define LWPD	PFWP_DESC
#define LWPDL	PFWP_LDESC

/*
 * Descriptors to be used with logging for SQLCHAR pointer type.
 * "Log Char Pointer Descriptor [with Length]"
 */
#define LCPD	PFCP_DESC
#define LCPDL	PFCP_LDESC

/*
 * Descriptors to be used with logging for SQLTCHAR pointer type.
 * "Log Tchar Pointer Descriptor [with Length]"
 */
#ifdef UNICODE
#	define LTPD		LWPD
#	define LTPDL	LWPDL
#else /* UNICODE */
#	define LTPD		LCPD
#	define LTPDL	LCPDL
#endif /* UNICODE */


/* macro for logging of Xstr_st objects */
#define LWSTR(_wptr)	(int)(_wptr)->cnt, (_wptr)->str
#define LCSTR(_cptr)	(int)(_cptr)->cnt, (_cptr)->str

#ifdef UNICODE
#	define LTSTR	LWSTR
#else /* UNICODE */
#	define LTSTR	LCSTR
#endif /* UNICODE */


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

typedef struct struct_filelog {
	int level;
	HANDLE handle;
	wchar_t *path;
	unsigned char fails;
	esodbc_mutex_lt mux;
} esodbc_filelog_st;

esodbc_filelog_st *filelog_new(wstr_st *path, int level);
void filelog_del(esodbc_filelog_st *log);

int parse_log_level(wstr_st *level);
BOOL filelog_print_path(wstr_st *dest, wstr_st *dir_path, wstr_st *ident);

extern esodbc_filelog_st *_gf_log;
void _esodbc_log(esodbc_filelog_st *log, int lvl, int werrno,
	const char *func, const char *srcfile, int lineno, const char *fmt, ...);

#define _LOG(logp, lvl, werr, fmt, ...) \
	do { \
		if ((logp) && ((lvl) <= (logp)->level)) { \
			_esodbc_log(logp, lvl, werr, __func__, __FILE__, __LINE__, \
				fmt, __VA_ARGS__); \
		} \
	} while (0)

#define LOG(lvl, fmt, ...)	_LOG(_gf_log, lvl, 0, fmt, __VA_ARGS__)
#define ERRN(fmt, ...)		_LOG(_gf_log, LOG_LEVEL_ERR, 1, fmt, __VA_ARGS__)

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

/* NULL as ("to Wide") string */
#define TWS_NULL	MK_WPTR("<null>")
#define TS_NULL		MK_CPTR("<null>")




#endif /* __LOG_H__ */

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
