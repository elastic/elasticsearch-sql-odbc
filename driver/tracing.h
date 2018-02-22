/*
 * ELASTICSEARCH CONFIDENTIAL
 * __________________
 *
 *  [2018] Elasticsearch Incorporated. All Rights Reserved.
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
#ifndef __TRACING_H__
#define __TRACING_H__

#include <inttypes.h>
#include <stdio.h>
#include "log.h"

#define TRACE_LOG_LEVEL	LOG_LEVEL_DBG

#define _AVAIL	sizeof(_bf) - _ps

/* TODO: the SQL[U]LEN for _WIN32 */
#define _PRINT_PARAM_VAL(type, val) \
	do { \
		switch(type) { \
			/* numeric pointers */ \
			/* SQLNUMERIC/SQLDATE/SQLDECIMAL/SQLCHAR/etc. = unsigned char */ \
			/* SQLSCHAR = char */ \
			case 'c': /* char signed */ \
				_n = snprintf(_bf + _ps, _AVAIL, "%hhd", \
							  val ? *(char *)(uintptr_t)val : 0); \
				break; \
			case 'C': /* char unsigned */ \
				_n = snprintf(_bf + _ps, _AVAIL, "%hhu", \
							  val ? *(unsigned char *)(uintptr_t)val : 0); \
				break; \
			/* SQL[U]SMALLINT = [unsigned] short */ \
			case 't': /* short signed */ \
				_n = snprintf(_bf + _ps, _AVAIL, "%hd", \
							  val ? *(short *)(uintptr_t)val : 0); \
				break; \
			case 'T': /* short unsigned */ \
				_n = snprintf(_bf + _ps, _AVAIL, "%hu", \
							  val ? *(unsigned short *)(uintptr_t)val : 0); \
				break; \
			/* SQL[U]INTEGER = [unsigned] long */ \
			case 'g': /* long signed */ \
				_n = snprintf(_bf + _ps, _AVAIL, "%ld", \
							  val ? *(long *)(uintptr_t)val : 0); \
				break; \
			case 'G': /* long unsigned */ \
				_n = snprintf(_bf + _ps, _AVAIL, "%lu", \
							  val ? *(unsigned long *)(uintptr_t)val : 0); \
				break; \
			/* SQL[U]LEN = [unsigned] long OR [u]int64_t (64b _WIN32) */ \
			case 'n': /* long/int64_t signed */ \
				_n = snprintf(_bf + _ps, _AVAIL, "%lld", \
							  val ? *(int64_t *)(uintptr_t)val : 0); \
				break; \
			case 'N': /* long/int64_t unsigned */ \
				_n = snprintf(_bf + _ps, _AVAIL, "%llu", \
							  val ? *(uint64_t *)(uintptr_t)val : 0); \
				break; \
			/* non-numeric pointers */ \
			case 'p': /* void* */ \
				_n = snprintf(_bf + _ps, _AVAIL, "@0x%p", \
						(void *)(uintptr_t)val); \
				break; \
			case 'W': /* wchar_t* */ \
				/* TODO: this can be problematic, for untouched buffs: add
				 * len! */ \
				_n = snprintf(_bf + _ps, _AVAIL, "`" LTPD "`[%zd]", \
							  val ? (wchar_t *)(uintptr_t)val : \
							  		MK_WSTR("<null>"), \
							  val ? wcslen((wchar_t *)(uintptr_t)val) : 0); \
				break; \
			/* imediat values */ \
			/* longs */ \
			case 'l': /* long signed */ \
				_n = snprintf(_bf + _ps, _AVAIL, "%ld", \
						(long)(intptr_t)val); \
				break;\
			case 'L': /* long unsigned */ \
				_n = snprintf(_bf + _ps, _AVAIL, "%lu", \
						(unsigned long)(uintptr_t)val); \
				break;\
			/* ints */ \
			case 'd': /* int signed */ \
				_n = snprintf(_bf + _ps, _AVAIL, "%d", \
						(int)(intptr_t)val); \
				break;\
			case 'u': /* int unsigned */ \
				_n = snprintf(_bf + _ps, _AVAIL, "%u", \
						(unsigned)(uintptr_t)val); \
				break;\
			default: \
				_n = snprintf(_bf+_ps, _AVAIL, "BUG! unknown type: %d",type); \
				break; \
		} \
		if (0 < _n) \
			_ps += _n; \
	} while (0)

#define _IS_PTR(type, _is_ptr) \
	do {\
		switch (type) { \
			case 'l': \
			case 'L': \
			case 'd': \
			case 'u': \
				_is_ptr = FALSE; \
				break; \
			default: \
				_is_ptr = TRUE; \
		} \
	} while (0)

#define _PRINT_PARAM(type, param, add_comma) \
	do { \
		BOOL _is_ptr; \
		_IS_PTR(type, _is_ptr); \
		int _n = snprintf(_bf + _ps, _AVAIL, "%s%s%s=", add_comma ? ", " : "",\
				_is_ptr ? "*" : "", # param); \
		if (0 < _n) /* no proper err check */ \
			_ps += _n; \
		_PRINT_PARAM_VAL(type, param); \
	} while (0)


#define _IN			0
#define _TRACE_IN	"ENTER: "
#define _OUT		1
#define _TRACE_OUT	"EXIT: "

#define _TRACE_DECLARATION(end) \
		char _bf[1024]; /*"ought to be enough for anybody"*/\
		int _ps = 0; \
		_ps += snprintf(_bf + _ps, _AVAIL, end ? _TRACE_OUT : _TRACE_IN)

#define _TRACE_ENDING \
		_ps += snprintf(_bf + _ps, _AVAIL, "."); \
		LOG(TRACE_LOG_LEVEL, "%s", _bf)

#define TRACE1(out, fmt, p0) \
	do { \
		_TRACE_DECLARATION(out); \
		_PRINT_PARAM(fmt[0], p0, 0); \
		_TRACE_ENDING; \
	} while(0)

#define TRACE2(out, fmt, p0, p1) \
	do { \
		_TRACE_DECLARATION(out); \
		_PRINT_PARAM(fmt[0], p0, 0); \
		_PRINT_PARAM(fmt[1], p1, 1); \
		_TRACE_ENDING; \
	} while(0)

#define TRACE3(out, fmt, p0, p1, p2) \
	do { \
		_TRACE_DECLARATION(out); \
		_PRINT_PARAM(fmt[0], p0, 0); \
		_PRINT_PARAM(fmt[1], p1, 1); \
		_PRINT_PARAM(fmt[2], p2, 1); \
		_TRACE_ENDING; \
	} while(0)

#define TRACE4(out, fmt, p0, p1, p2, p3) \
	do { \
		_TRACE_DECLARATION(out); \
		_PRINT_PARAM(fmt[0], p0, 0); \
		_PRINT_PARAM(fmt[1], p1, 1); \
		_PRINT_PARAM(fmt[2], p2, 1); \
		_PRINT_PARAM(fmt[3], p3, 1); \
		_TRACE_ENDING; \
	} while(0)

#define TRACE5(out, fmt, p0, p1, p2, p3, p4) \
	do { \
		_TRACE_DECLARATION(out); \
		_PRINT_PARAM(fmt[0], p0, 0); \
		_PRINT_PARAM(fmt[1], p1, 1); \
		_PRINT_PARAM(fmt[2], p2, 1); \
		_PRINT_PARAM(fmt[3], p3, 1); \
		_PRINT_PARAM(fmt[4], p4, 1); \
		_TRACE_ENDING; \
	} while(0)

#define TRACE6(out, fmt, p0, p1, p2, p3, p4, p5) \
	do { \
		_TRACE_DECLARATION(out); \
		_PRINT_PARAM(fmt[0], p0, 0); \
		_PRINT_PARAM(fmt[1], p1, 1); \
		_PRINT_PARAM(fmt[2], p2, 1); \
		_PRINT_PARAM(fmt[3], p3, 1); \
		_PRINT_PARAM(fmt[4], p4, 1); \
		_PRINT_PARAM(fmt[5], p5, 1); \
		_TRACE_ENDING; \
	} while(0)

#define TRACE7(out, fmt, p0, p1, p2, p3, p4, p5, p6) \
	do { \
		_TRACE_DECLARATION(out); \
		_PRINT_PARAM(fmt[0], p0, 0); \
		_PRINT_PARAM(fmt[1], p1, 1); \
		_PRINT_PARAM(fmt[2], p2, 1); \
		_PRINT_PARAM(fmt[3], p3, 1); \
		_PRINT_PARAM(fmt[4], p4, 1); \
		_PRINT_PARAM(fmt[5], p5, 1); \
		_PRINT_PARAM(fmt[6], p6, 1); \
		_TRACE_ENDING; \
	} while(0)

#define TRACE8(out, fmt, p0, p1, p2, p3, p4, p5, p6, p7) \
	do { \
		_TRACE_DECLARATION(out); \
		_PRINT_PARAM(fmt[0], p0, 0); \
		_PRINT_PARAM(fmt[1], p1, 1); \
		_PRINT_PARAM(fmt[2], p2, 1); \
		_PRINT_PARAM(fmt[3], p3, 1); \
		_PRINT_PARAM(fmt[4], p4, 1); \
		_PRINT_PARAM(fmt[5], p5, 1); \
		_PRINT_PARAM(fmt[6], p6, 1); \
		_PRINT_PARAM(fmt[7], p7, 1); \
		_TRACE_ENDING; \
	} while(0)

#define TRACE9(out, fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8) \
	do { \
		_TRACE_DECLARATION(out); \
		_PRINT_PARAM(fmt[0], p0, 0); \
		_PRINT_PARAM(fmt[1], p1, 1); \
		_PRINT_PARAM(fmt[2], p2, 1); \
		_PRINT_PARAM(fmt[3], p3, 1); \
		_PRINT_PARAM(fmt[4], p4, 1); \
		_PRINT_PARAM(fmt[5], p5, 1); \
		_PRINT_PARAM(fmt[6], p6, 1); \
		_PRINT_PARAM(fmt[7], p7, 1); \
		_PRINT_PARAM(fmt[8], p8, 1); \
		_TRACE_ENDING; \
	} while(0)

#define TRACE10(out, fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9) \
	do { \
		_TRACE_DECLARATION(out); \
		_PRINT_PARAM(fmt[0], p0, 0); \
		_PRINT_PARAM(fmt[1], p1, 1); \
		_PRINT_PARAM(fmt[2], p2, 1); \
		_PRINT_PARAM(fmt[3], p3, 1); \
		_PRINT_PARAM(fmt[4], p4, 1); \
		_PRINT_PARAM(fmt[5], p5, 1); \
		_PRINT_PARAM(fmt[6], p6, 1); \
		_PRINT_PARAM(fmt[7], p7, 1); \
		_PRINT_PARAM(fmt[8], p8, 1); \
		_PRINT_PARAM(fmt[9], p9, 1); \
		_TRACE_ENDING; \
	} while(0)

#define TRACE11(out, fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10) \
	do { \
		_TRACE_DECLARATION(out); \
		_PRINT_PARAM(fmt[0], p0, 0); \
		_PRINT_PARAM(fmt[1], p1, 1); \
		_PRINT_PARAM(fmt[2], p2, 1); \
		_PRINT_PARAM(fmt[3], p3, 1); \
		_PRINT_PARAM(fmt[4], p4, 1); \
		_PRINT_PARAM(fmt[5], p5, 1); \
		_PRINT_PARAM(fmt[6], p6, 1); \
		_PRINT_PARAM(fmt[7], p7, 1); \
		_PRINT_PARAM(fmt[8], p8, 1); \
		_PRINT_PARAM(fmt[9], p9, 1); \
		_PRINT_PARAM(fmt[10], p10, 1); \
		_TRACE_ENDING; \
	} while(0)

#define TRACE12(out, fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11) \
	do { \
		_TRACE_DECLARATION(out); \
		_PRINT_PARAM(fmt[0], p0, 0); \
		_PRINT_PARAM(fmt[1], p1, 1); \
		_PRINT_PARAM(fmt[2], p2, 1); \
		_PRINT_PARAM(fmt[3], p3, 1); \
		_PRINT_PARAM(fmt[4], p4, 1); \
		_PRINT_PARAM(fmt[5], p5, 1); \
		_PRINT_PARAM(fmt[6], p6, 1); \
		_PRINT_PARAM(fmt[7], p7, 1); \
		_PRINT_PARAM(fmt[8], p8, 1); \
		_PRINT_PARAM(fmt[9], p9, 1); \
		_PRINT_PARAM(fmt[10], p10, 1); \
		_PRINT_PARAM(fmt[11], p11, 1); \
		_TRACE_ENDING; \
	} while(0)


#endif /* __TRACING_H__ */

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
