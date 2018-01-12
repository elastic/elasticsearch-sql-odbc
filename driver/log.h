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


/*
 * w/printf() desriptors for char/wchar_t *
 * "WPrintF Wide/Char Pointer _ DESCriptor"
 */
#ifdef _WIN32
/* funny M$ 'inverted' logic */
/* wprintf wide_t pointer descriptor */
#define WPFWP_DESC		L"%s"
/* printf wide_t pointer descriptor */
#define PFWP_DESC		"%S"
/* wprintf char pointer descriptor */
#define WPFCP_DESC		L"%S" 
/* printf char pointer descriptor */
#define PFCP_DESC		"%s"
#else /* _WIN32 */
/* wprintf wide_t pointer descriptor */
#define WPFWP_DESC		L"%S"
/* printf wide_t pointer descriptor */
#define PFWP_DESC		"%S"
/* silly M$ */
/* wprintf char pointer descriptor */
#define WPFCP_DESC		L"%s" 
/* printf char pointer descriptor */
#define PFCP_DESC		"%s"
#endif /* _WIN32 */

/*
 * Descriptors to be used with logging with SQLTCHAR pointer type.
 * "Log Tchar Pointer Descriptor"
 */
#ifdef UNICODE
#define LTPD	PFWP_DESC
#else /* UNICODE */
#define LTPD	PFCP_DESC
#endif /* UNICODE */



/* Note: keep in sync with __ESODBC_LVL2STR */
enum osodbc_log_levels {
	LOG_LEVEL_ERR,
	LOG_LEVEL_WARN,
	LOG_LEVEL_INFO,
	LOG_LEVEL_DBG,
};

#if 0
void _esodbc_log(int lvl, const char *fmt, ...);

#define LOG(lvl, fmt, ...) \
	if (lvl <= LOG_LEVEL_CRR) \
		_esodbc_log(lvl, "[%s] %s()@%s:%d: " fmt, \
				__ESODBC_LVL2STR[lvl], __func__, __FILE__, __LINE__, \
				__VA_ARGS__)

/* FIXME: 4!WINx */
/*
 * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/strerror-s-strerror-s-wcserror-s-wcserror-s :
 * """
 * Your string message can be, at most, 94 characters long.
 * 
 * """
 */
#define __ESODBC_STRERROR_BUF_LEN	128
#define ELOG(lvl, fmt, ...) \
	do { \
		char buff[__ESODBC_STRERROR_BUF_LEN]; \
		if (lvl <= LOG_LEVEL_CRR) \
			break; \
		if (strerror_s(buff, sizeof(buff), errno)) \
			buff[0] = 0; \
		_esodbc_log(lvl, "[%s] %s()@%s:%d (%d:%s): " fmt, \
				__ESODBC_LVL2STR[lvl], __func__, __FILE__, __LINE__, \
				errno, buff, __VA_ARGS__); \
	} while (0)

/* TODO: X(...) Y(lvl, __VA_ARGS__) ?? */
#define BUG(fmt, ...)	LOG(LOG_LEVEL_ERR, fmt, __VA_ARGS__)
#define ERR(fmt, ...)	LOG(LOG_LEVEL_ERR, fmt, __VA_ARGS__)
#define ERRN(fmt, ...)	ELOG(LOG_LEVEL_ERR, fmt, __VA_ARGS__)
#define WARN(fmt, ...)	LOG(LOG_LEVEL_WARN, fmt, __VA_ARGS__)
#define INFO(fmt, ...)	LOG(LOG_LEVEL_INFO, fmt, __VA_ARGS__)
#define DBG(fmt, ...)	LOG(LOG_LEVEL_DBG, fmt, __VA_ARGS__)
#define TRACE			LOG(LOG_LEVEL_DBG, "===== TR4C3 =====")

#else
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
#define TRACE			LOG(LOG_LEVEL_DBG, "===== TR4C3 =====")

#define BUG(fmt, ...) \
	do { \
		LOG(LOG_LEVEL_ERR, fmt, __VA_ARGS__); \
		assert(0); \
	} while (0)

#define FIXME BUG("not yet implemented"); 

#endif

#endif /* __LOG_H__ */

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
