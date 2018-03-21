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

#include "error.h"

/*
 * w/printf() desriptors for char/wchar_t *
 * "WPrintF Wide/Char Pointer _ DESCriptor"
 */
#ifdef _WIN32
/* funny M$ 'inverted' logic */
/* wprintf wide_t pointer descriptor */
#define WPFWP_DESC		L"%s"
#define WPFWP_LDESC		L"%.*s"
/* printf wide_t pointer descriptor */
#define PFWP_DESC		"%S"
#define PFWP_LDESC		"%.*S"
/* wprintf char pointer descriptor */
#define WPFCP_DESC		L"%S" 
#define WPFCP_LDESC		L"%.*S" 
/* printf char pointer descriptor */
#define PFCP_DESC		"%s"
#define PFCP_LDESC		"%.*s"
#else /* _WIN32 */
/* wprintf wide_t pointer descriptor */
#define WPFWP_DESC		L"%S"
#define WPFWP_LDESC		L"%.*S"
/* printf wide_t pointer descriptor */
#define PFWP_DESC		"%S"
#define PFWP_LDESC		"%.*S"
/* silly M$ */
/* wprintf char pointer descriptor */
#define WPFCP_DESC		L"%s" 
#define WPFCP_LDESC		L"%.*s" 
/* printf char pointer descriptor */
#define PFCP_DESC		"%s"
#define PFCP_LDESC		"%.*s"
#endif /* _WIN32 */

/*
 * Descriptors to be used with logging with SQLWCHAR pointer type.
 * "Log Wchar Pointer Descriptor [with Lenght]"
 */
#ifdef UNICODE
#define LWPD	PFWP_DESC
#define LWPDL	PFWP_LDESC
#else /* UNICODE */
#define LWPD	PFCP_DESC
#define LWPDL	PFCP_LDESC
#endif /* UNICODE */

/* macro for logging of wstr_st objects */
#define LWSTR(_wptr)	(int)(_wptr)->cnt, (_wptr)->str



/* Note: keep in sync with __ESODBC_LVL2STR */
enum osodbc_log_levels {
	LOG_LEVEL_ERR,
	LOG_LEVEL_WARN,
	LOG_LEVEL_INFO,
	LOG_LEVEL_DBG,
};

void _esodbc_log(int lvl, int werrno, const char *func, 
		const char *srcfile, int lineno, const char *fmt, ...);
extern int _esodbc_log_level;

#define _LOG(lvl, werr, fmt, ...) \
	if (lvl <= _esodbc_log_level) \
		_esodbc_log(lvl, werr, __func__, __FILE__, __LINE__, fmt, __VA_ARGS__)
#define LOG(lvl, fmt, ...)	_LOG(lvl, 0, fmt, __VA_ARGS__)

#define ERR(fmt, ...)	LOG(LOG_LEVEL_ERR, fmt, __VA_ARGS__)
#define ERRN(fmt, ...)	_LOG(LOG_LEVEL_ERR, 1, fmt, __VA_ARGS__)
#define WARN(fmt, ...)	LOG(LOG_LEVEL_WARN, fmt, __VA_ARGS__)
#define INFO(fmt, ...)	LOG(LOG_LEVEL_INFO, fmt, __VA_ARGS__)
#define DBG(fmt, ...)	LOG(LOG_LEVEL_DBG, fmt, __VA_ARGS__)

#define ERRSTMT(stmt, fmt, ...)		ERR("STMT@0x%p: " fmt, stmt, __VA_ARGS__)
#define WARNSTMT(stmt, fmt, ...)	WARN("STMT@0x%p: " fmt, stmt, __VA_ARGS__)
#define DBGSTMT(stmt, fmt, ...)		DBG("STMT@0x%p: " fmt, stmt, __VA_ARGS__)

#define BUG(fmt, ...) \
	do { \
		LOG(LOG_LEVEL_ERR, fmt, __VA_ARGS__); \
		assert(0); \
	} while (0)

#define FIXME	BUG("not yet implemented")
#define TRACE	DBG("===== TR4C3 =====");

#define TS_NULL	MK_WPTR("<null>")

#endif /* __LOG_H__ */

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
