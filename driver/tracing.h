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
#ifndef __TRACING_H__
#define __TRACING_H__

#include <inttypes.h>
#include <stdio.h>
#include "log.h"

#define TRACE_LOG_LEVEL	LOG_LEVEL_DBG

#define _AVAIL	sizeof(_bf) - _ps

#define _PRINT_PARAM_VAL(type, val) \
	do { \
		switch(type) { \
			case 'd': _n = snprintf(_bf + _ps, _AVAIL, "%zd", \
							  (intptr_t)val); break;\
			case 'u': _n = snprintf(_bf + _ps, _AVAIL, "%zu", \
							  (uintptr_t)val); break;\
			case 'p': _n = snprintf(_bf + _ps, _AVAIL, "0x%p", \
							  (void *)(uintptr_t)val); break; \
			case 'D': _n = snprintf(_bf + _ps, _AVAIL, "%d", \
							  val ? *(int *)(uintptr_t)val : 0); break; \
			case 'U': _n = snprintf(_bf + _ps, _AVAIL, "%d", \
							  val ? *(unsigned *)(uintptr_t)val : 0); break; \
			case 'W': _n = snprintf(_bf + _ps, _AVAIL, "'"LTPD"'", \
							  val ? (wchar_t *)(uintptr_t)val : \
							  MK_WSTR("<null>")); break; \
			default: _n = snprintf(_bf + _ps, _AVAIL, "BUG! unknown type: %d",\
							 type); break; \
		} \
		if (0 < _n) \
			_ps += _n; \
	} while (0)

#define _PRINT_PARAM(type, param, add_comma) \
	do { \
		int _n = snprintf(_bf + _ps, _AVAIL, "%s%s%s=", add_comma ? ", " : "",\
				type&0x20 ? "" : "*", # param); \
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



#endif /* __TRACING_H__ */

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
