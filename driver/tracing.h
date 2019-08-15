/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */
#ifndef __TRACING_H__
#define __TRACING_H__

#include <inttypes.h>
#include <stdio.h>
#include <time.h>
#include "log.h"

#define TRACE_LOG_LEVEL	LOG_LEVEL_DBG

/* name of the buffer into which theh trace messages get printed */
#define _BUFF			_trace_buff
/* size of the trace buffer */
#define TBUFF_SIZE		(ESODBC_LOG_BUF_SIZE - /*final `.`*/1)
/* room left in the buffer */
#define _AVAIL(_ps)		((int)(sizeof(_BUFF)/sizeof(_BUFF[0])) - _ps)

/* check if buffer limit has been reached */
#define _CHECK_WRITE_(/*written*/_n, /*position*/_ps) \
	if (_n < 0) { /* printing failed */ \
		assert(_ps < _AVAIL(0)); \
		_BUFF[_ps] = '\0'; /* if previously usable, make sure it's NTS'd */\
		break; \
	} else if (_AVAIL(_ps) <= _ps + _n) { /* buffer end reached */ \
		_ps = _AVAIL(0); \
		break; \
	} else { \
		_ps += _n; \
	}

/*INDENT-OFF*/
/* TODO: the SQL[U]LEN for _WIN32 */
#define _PRINT_PARAM_VAL(type, val, prec) \
	do { \
		int _prec; \
		wchar_t *_w; \
		if (_AVAIL(_ps) <= 0) { \
			break; \
		} \
		switch(type) { \
				/*
				 * numeric pointers
				 */ \
				/* SQLNUMERIC/SQLDATE/SQLCHAR/etc. = unsigned char */ \
				/* SQLSCHAR = char */ \
			case 'c': /* char signed */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "%hhd", \
						val ? *(char *)(uintptr_t)val : 0); \
				break; \
			case 'C': /* char unsigned */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "%hhu", \
						val ? *(unsigned char *)(uintptr_t)val : 0); \
				break; \
				/* SQL[U]SMALLINT = [unsigned] short */ \
			case 't': /* short signed */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "%hd", \
						val ? *(short *)(uintptr_t)val : 0); \
				break; \
			case 'T': /* short unsigned */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "%hu", \
						val ? *(unsigned short *)(uintptr_t)val : 0); \
				break; \
				/* SQL[U]INTEGER = [unsigned] long */ \
			case 'g': /* long signed */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "%ld", \
						val ? *(long *)(uintptr_t)val : 0); \
				break; \
			case 'G': /* long unsigned */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "%lu", \
						val ? *(unsigned long *)(uintptr_t)val : 0); \
				break; \
				/* SQL[U]LEN = [unsigned] long OR [u]int64_t (64b _WIN32) */ \
			case 'n': /* long/int64_t signed */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "%lld", \
						val ? *(int64_t *)(uintptr_t)val : 0); \
				break; \
			case 'N': /* long/int64_t unsigned */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "%llu", \
						val ? *(uint64_t *)(uintptr_t)val : 0); \
				break; \
				/*
				 * non-numeric pointers
				 */ \
			case 'p': /* void* */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "@0x%p", \
						(void *)(uintptr_t)val); \
				break; \
				/* TODO: support for (buffer*, max, written*) pattern */ \
			case 'W': /* wchar_t* with precision (always following param) */ \
				if (val) { \
					_w = (wchar_t *)(uintptr_t)val; \
					_prec = (int)(intptr_t)prec; \
					_prec = (_prec == SQL_NTS) ? (int)wcslen(_w) : _prec; \
					_n = snprintf(_BUFF + _ps, _AVAIL(_ps), \
							"[%d] `" LWPDL "`", _prec, _prec, _w); \
				} else { \
					_n = snprintf(_BUFF + _ps, _AVAIL(_ps), TS_NULL); \
				} \
				break; \
			case 'w': /* NTS wchar_t* */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "[%zu] `" LWPD "`", \
						val ? wcslen((wchar_t *)(uintptr_t)val) : 0, \
						val ? (wchar_t *)(uintptr_t)val : TWS_NULL); \
				break; \
			case 's': /* NTS char* */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "[%zu] `" LCPD "`", \
						val ? strlen((char *)(uintptr_t)val) : 0, \
						val ? (char *)(uintptr_t)val : TS_NULL); \
				break; \
				/*
				 * imediat values
				 */ \
				/* long longs */ \
			case 'z': /* long long signed (SQLLEN) */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "%lld", \
						(long long)(intptr_t)val); \
				break; \
			case 'Z': /* long long unsigned (SQLULEN) */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "%llu", \
						(unsigned long long)(uintptr_t)val); \
				break; \
				/* longs */ \
			case 'l': /* long signed (SQLINTEGER) */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "%ld", \
						(long)(intptr_t)val); \
				break; \
			case 'L': /* long unsigned (SQLUINTEGER) */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "%lu", \
						(unsigned long)(uintptr_t)val); \
				break; \
				/* ints */ \
			case 'd': /* int signed */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "%d", \
						(int)(intptr_t)val); \
				break; \
			case 'u': /* int unsigned */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "%u", \
						(unsigned)(uintptr_t)val); \
				break; \
			case 'h': /* short signed (SQLSMALLINT) */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "%hd", \
						(short)(intptr_t)val); \
				break; \
			case 'H': /* short unsigned (SQLUSMALLINT) */ \
				_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "%hu", \
						(unsigned short)(uintptr_t)val); \
				break; \
			default: \
				_n = snprintf(_BUFF+_ps, _AVAIL(_ps), "BUG! unknown type: %d",\
						type); \
				break; \
		} \
		_CHECK_WRITE_(_n, _ps) \
	} while (0)
/*INDENT-ON*/

#define _IS_PTR(type, _is_ptr) \
	do {\
		switch (type) { \
			case 'l': \
			case 'L': \
			case 'd': \
			case 'u': \
			case 'h': \
			case 'H': \
				_is_ptr = FALSE; \
				break; \
			default: \
				_is_ptr = TRUE; \
		} \
	} while (0)

#define _PRINT_PARAM(type, param, prec, add_comma) \
	do { \
		BOOL _is_ptr; \
		_IS_PTR(type, _is_ptr); \
		_n = snprintf(_BUFF + _ps, _AVAIL(_ps), "%s%s%s=", \
				add_comma ? ", " : "", _is_ptr ? "*" : "", # param); \
		if (0 < _n) /* no proper err check */ \
			_ps += _n; \
		_PRINT_PARAM_VAL(type, param, prec); \
	} while (0)


#define _IN			0
#define _TRACE_IN	"ENTER: "
#define _OUT		1
#define _TRACE_OUT	"EXIT: "

/* account for the total time (across all threads!) spent within the driver:
 * the time the ODBC API calls take to be serviced. */
#ifdef WITH_OAPI_TIMING
extern volatile LONG64 api_ticks;
extern thread_local clock_t in_ticks;
#	define OAPI_TIMING(inout) \
	do { \
		if (inout == _IN) { \
			in_ticks = clock(); \
		} else { /* _OUT */ \
			clock_t out_ticks = clock(); \
			if (out_ticks != (clock_t)-1 && in_ticks < out_ticks) { \
				InterlockedExchangeAdd64(&api_ticks, out_ticks - in_ticks); \
			} \
		} \
	} while (0)
#else /* WITH_API_TIMING */
#	define OAPI_TIMING
#endif /* WITH_API_TIMING */

#define _TRACE_HEADER_(inout, hnd) \
	char _BUFF[TBUFF_SIZE]; \
	int _ps = 0, _n; \
	esodbc_filelog_st *_log; \
	/* no accounting of the "out" tracing, but that should be OK, this being
	 * most useful with release builds on non-dbg logging (when the rest of
	 * the tracing code is skipped anyways) */\
	OAPI_TIMING(inout); \
	_log = (hnd && HDRH(hnd)->log) ? HDRH(hnd)->log : _gf_log; \
	if ((! _log) || (_log->level < TRACE_LOG_LEVEL)) { \
		/* skip all the printing as early as possible */ \
		break; \
	} \
	_n = snprintf(_BUFF + _ps, _AVAIL(_ps), inout ? _TRACE_OUT : _TRACE_IN); \
	_CHECK_WRITE_(_n, _ps)

#define _TRACE_FOOTER_ \
	_esodbc_log(_log, TRACE_LOG_LEVEL, /*werr*/0, \
		__func__, __FILE__, __LINE__, "%s.", _BUFF);


#define TRACE1(inout, hnd, fmt, p0) \
	do { \
		_TRACE_HEADER_(inout, hnd) \
		_PRINT_PARAM(fmt[0], p0, SQL_NTS, 0); \
		_TRACE_FOOTER_ \
	} while(0)

#define TRACE2(inout, hnd, fmt, p0, p1) \
	do { \
		_TRACE_HEADER_(inout, hnd) \
		_PRINT_PARAM(fmt[0], p0, p1, 0); \
		_PRINT_PARAM(fmt[1], p1, SQL_NTS, 1); \
		_TRACE_FOOTER_ \
	} while(0)

#define TRACE3(inout, hnd, fmt, p0, p1, p2) \
	do { \
		_TRACE_HEADER_(inout, hnd) \
		_PRINT_PARAM(fmt[0], p0, p1, 0); \
		_PRINT_PARAM(fmt[1], p1, p2, 1); \
		_PRINT_PARAM(fmt[2], p2, SQL_NTS, 1); \
		_TRACE_FOOTER_ \
	} while(0)

#define TRACE4(inout, hnd, fmt, p0, p1, p2, p3) \
	do { \
		_TRACE_HEADER_(inout, hnd) \
		_PRINT_PARAM(fmt[0], p0, p1, 0); \
		_PRINT_PARAM(fmt[1], p1, p2, 1); \
		_PRINT_PARAM(fmt[2], p2, p3, 1); \
		_PRINT_PARAM(fmt[3], p3, SQL_NTS, 1); \
		_TRACE_FOOTER_ \
	} while(0)

#define TRACE5(inout, hnd, fmt, p0, p1, p2, p3, p4) \
	do { \
		_TRACE_HEADER_(inout, hnd) \
		_PRINT_PARAM(fmt[0], p0, p1, 0); \
		_PRINT_PARAM(fmt[1], p1, p2, 1); \
		_PRINT_PARAM(fmt[2], p2, p3, 1); \
		_PRINT_PARAM(fmt[3], p3, p4, 1); \
		_PRINT_PARAM(fmt[4], p4, SQL_NTS, 1); \
		_TRACE_FOOTER_ \
	} while(0)

#define TRACE6(inout, hnd, fmt, p0, p1, p2, p3, p4, p5) \
	do { \
		_TRACE_HEADER_(inout, hnd) \
		_PRINT_PARAM(fmt[0], p0, p1, 0); \
		_PRINT_PARAM(fmt[1], p1, p2, 1); \
		_PRINT_PARAM(fmt[2], p2, p3, 1); \
		_PRINT_PARAM(fmt[3], p3, p4, 1); \
		_PRINT_PARAM(fmt[4], p4, p5, 1); \
		_PRINT_PARAM(fmt[5], p5, SQL_NTS, 1); \
		_TRACE_FOOTER_ \
	} while(0)

#define TRACE7(inout, hnd, fmt, p0, p1, p2, p3, p4, p5, p6) \
	do { \
		_TRACE_HEADER_(inout, hnd) \
		_PRINT_PARAM(fmt[0], p0, p1, 0); \
		_PRINT_PARAM(fmt[1], p1, p2, 1); \
		_PRINT_PARAM(fmt[2], p2, p3, 1); \
		_PRINT_PARAM(fmt[3], p3, p4, 1); \
		_PRINT_PARAM(fmt[4], p4, p5, 1); \
		_PRINT_PARAM(fmt[5], p5, p6, 1); \
		_PRINT_PARAM(fmt[6], p6, SQL_NTS, 1); \
		_TRACE_FOOTER_ \
	} while(0)

#define TRACE8(inout, hnd, fmt, p0, p1, p2, p3, p4, p5, p6, p7) \
	do { \
		_TRACE_HEADER_(inout, hnd) \
		_PRINT_PARAM(fmt[0], p0, p1, 0); \
		_PRINT_PARAM(fmt[1], p1, p2, 1); \
		_PRINT_PARAM(fmt[2], p2, p3, 1); \
		_PRINT_PARAM(fmt[3], p3, p4, 1); \
		_PRINT_PARAM(fmt[4], p4, p5, 1); \
		_PRINT_PARAM(fmt[5], p5, p6, 1); \
		_PRINT_PARAM(fmt[6], p6, p7, 1); \
		_PRINT_PARAM(fmt[7], p7, SQL_NTS, 1); \
		_TRACE_FOOTER_ \
	} while(0)

#define TRACE9(inout, hnd, fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8) \
	do { \
		_TRACE_HEADER_(inout, hnd) \
		_PRINT_PARAM(fmt[0], p0, p1, 0); \
		_PRINT_PARAM(fmt[1], p1, p2, 1); \
		_PRINT_PARAM(fmt[2], p2, p3, 1); \
		_PRINT_PARAM(fmt[3], p3, p4, 1); \
		_PRINT_PARAM(fmt[4], p4, p5, 1); \
		_PRINT_PARAM(fmt[5], p5, p6, 1); \
		_PRINT_PARAM(fmt[6], p6, p7, 1); \
		_PRINT_PARAM(fmt[7], p7, p8, 1); \
		_PRINT_PARAM(fmt[8], p8, SQL_NTS, 1); \
		_TRACE_FOOTER_ \
	} while(0)

#define TRACE10(inout, hnd, fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9) \
	do { \
		_TRACE_HEADER_(inout, hnd) \
		_PRINT_PARAM(fmt[0], p0, p1, 0); \
		_PRINT_PARAM(fmt[1], p1, p2, 1); \
		_PRINT_PARAM(fmt[2], p2, p3, 1); \
		_PRINT_PARAM(fmt[3], p3, p4, 1); \
		_PRINT_PARAM(fmt[4], p4, p5, 1); \
		_PRINT_PARAM(fmt[5], p5, p6, 1); \
		_PRINT_PARAM(fmt[6], p6, p7, 1); \
		_PRINT_PARAM(fmt[7], p7, p8, 1); \
		_PRINT_PARAM(fmt[8], p8, p9, 1); \
		_PRINT_PARAM(fmt[9], p9, SQL_NTS, 1); \
		_TRACE_FOOTER_ \
	} while(0)

#define TRACE11(inout, hnd, fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10) \
	do { \
		_TRACE_HEADER_(inout, hnd) \
		_PRINT_PARAM(fmt[0], p0, p1, 0); \
		_PRINT_PARAM(fmt[1], p1, p2, 1); \
		_PRINT_PARAM(fmt[2], p2, p3, 1); \
		_PRINT_PARAM(fmt[3], p3, p4, 1); \
		_PRINT_PARAM(fmt[4], p4, p5, 1); \
		_PRINT_PARAM(fmt[5], p5, p6, 1); \
		_PRINT_PARAM(fmt[6], p6, p7, 1); \
		_PRINT_PARAM(fmt[7], p7, p8, 1); \
		_PRINT_PARAM(fmt[8], p8, p9, 1); \
		_PRINT_PARAM(fmt[9], p9, p10, 1); \
		_PRINT_PARAM(fmt[10], p10, SQL_NTS, 1); \
		_TRACE_FOOTER_ \
	} while(0)

#define TRACE12(inout, hnd, fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, \
	p11) \
do { \
	_TRACE_HEADER_(inout, hnd) \
	_PRINT_PARAM(fmt[0], p0, p1, 0); \
	_PRINT_PARAM(fmt[1], p1, p2, 1); \
	_PRINT_PARAM(fmt[2], p2, p3, 1); \
	_PRINT_PARAM(fmt[3], p3, p4, 1); \
	_PRINT_PARAM(fmt[4], p4, p5, 1); \
	_PRINT_PARAM(fmt[5], p5, p6, 1); \
	_PRINT_PARAM(fmt[6], p6, p7, 1); \
	_PRINT_PARAM(fmt[7], p7, p8, 1); \
	_PRINT_PARAM(fmt[8], p8, p9, 1); \
	_PRINT_PARAM(fmt[9], p9, p10, 1); \
	_PRINT_PARAM(fmt[10], p10, p11, 1); \
	_PRINT_PARAM(fmt[11], p11, SQL_NTS, 1); \
	_TRACE_FOOTER_ \
} while(0)

/*INDENT-OFF*/ //astyle trips on these following two defs
#define TRACE13(inout, hnd, fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, \
		p11, p12) \
	do { \
		_TRACE_HEADER_(inout, hnd) \
		_PRINT_PARAM(fmt[0], p0, p1, 0); \
		_PRINT_PARAM(fmt[1], p1, p2, 1); \
		_PRINT_PARAM(fmt[2], p2, p3, 1); \
		_PRINT_PARAM(fmt[3], p3, p4, 1); \
		_PRINT_PARAM(fmt[4], p4, p5, 1); \
		_PRINT_PARAM(fmt[5], p5, p6, 1); \
		_PRINT_PARAM(fmt[6], p6, p7, 1); \
		_PRINT_PARAM(fmt[7], p7, p8, 1); \
		_PRINT_PARAM(fmt[8], p8, p9, 1); \
		_PRINT_PARAM(fmt[9], p9, p10, 1); \
		_PRINT_PARAM(fmt[10], p10, p11, 1); \
		_PRINT_PARAM(fmt[11], p11, p12, 1); \
		_PRINT_PARAM(fmt[12], p12, SQL_NTS, 1); \
		_TRACE_FOOTER_ \
	} while(0)

#define TRACE14(inout, hnd, fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, \
		p11, p12, p13) \
	do { \
		_TRACE_HEADER_(inout, hnd) \
		_PRINT_PARAM(fmt[0], p0, p1, 0); \
		_PRINT_PARAM(fmt[1], p1, p2, 1); \
		_PRINT_PARAM(fmt[2], p2, p3, 1); \
		_PRINT_PARAM(fmt[3], p3, p4, 1); \
		_PRINT_PARAM(fmt[4], p4, p5, 1); \
		_PRINT_PARAM(fmt[5], p5, p6, 1); \
		_PRINT_PARAM(fmt[6], p6, p7, 1); \
		_PRINT_PARAM(fmt[7], p7, p8, 1); \
		_PRINT_PARAM(fmt[8], p8, p9, 1); \
		_PRINT_PARAM(fmt[9], p9, p10, 1); \
		_PRINT_PARAM(fmt[10], p10, p11, 1); \
		_PRINT_PARAM(fmt[11], p11, p12, 1); \
		_PRINT_PARAM(fmt[12], p12, p13, 1); \
		_PRINT_PARAM(fmt[13], p13, SQL_NTS, 1); \
		_TRACE_FOOTER_ \
	} while(0)
/*INDENT-ON*/


#endif /* __TRACING_H__ */

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
