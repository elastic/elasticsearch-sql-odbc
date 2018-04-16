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

#ifndef __UTIL_H__
#define __UTIL_H__

/* NOTE: this must be included in "top level" file (wherever SQL types are
 * used  */
#if defined(_WIN32) || defined (WIN32)
/* FIXME: why isn't this included in sql/~ext.h? */
/* win function parameter attributes */
#include <windows.h>
#endif /* _WIN32/WIN32 */

#include <inttypes.h>
#include <wchar.h>

#include "sql.h"
#include "sqlext.h"

/* 
 * Stringifying in two preproc. passes 
 */
#define _STR(_x)	# _x
#define STR(_x)		_STR(_x)

/*
 * Turn a C static string to wide char string.
 */
#define _MK_WPTR(_cstr_)	(L ## _cstr_)
#define MK_WPTR(_cstr_)		_MK_WPTR(_cstr_)

typedef struct cstr {
	SQLCHAR *str;
	size_t cnt;
} cstr_st;

/*
 * Copy converted strings from SQLWCHAR to char, for ANSI strings.
 */
int ansi_w2c(const SQLWCHAR *src, char *dst, size_t chars);
/*
 * Compare two wchar_t object, case INsensitive.
 */
int wmemncasecmp(const wchar_t *a, const wchar_t *b, size_t len);
/*
 * Compare two zero-terminated wchar_t* objects, until a 0 is encountered in
 * either of them or until 'count' characters are evaluated. If 'count'
 * parameter is negative, it is ignored.
 *
 * This is useful in comparing SQL strings which the API allows to be passed 
 * either as 0-terminated or not (SQL_NTS).
 * The function does a single pass (no length evaluation of the strings).
 * wmemcmp() might read over the boundary of one of the objects, if the
 * provided 'count' paramter is not the minimum of the strings' length.
 */
int wszmemcmp(const wchar_t *a, const wchar_t *b, long count);

typedef struct wstr {
	SQLWCHAR *str;
	size_t cnt;
} wstr_st;

/*
 * Turn a static C string t a wstr_st.
 */
#define MK_WSTR(_s) \
	((wstr_st){.str = MK_WPTR(_s), .cnt = sizeof(_s) - 1})
/*
 * Test equality of two wstr_st objects.
 */
#define EQ_WSTR(s1, s2) \
	((s1)->cnt == (s2)->cnt && wmemcmp((s1)->str, (s2)->str, (s1)->cnt) == 0)
/*
 * Same as EQ_WSTR, but case INsensitive.
 */
#define EQ_CASE_WSTR(s1, s2) \
	((s1)->cnt == (s2)->cnt && \
	 wmemncasecmp((s1)->str, (s2)->str, (s1)->cnt) == 0)

BOOL wstr2bool(wstr_st *val);
BOOL wstr2long(wstr_st *val, long *out);

#ifdef _WIN32
/* 
 * "[D]oes not null-terminate an output string if the input string length is
 * explicitly specified without a terminating null character. To
 * null-terminate an output string for this function, the application should
 * pass in -1 or explicitly count the terminating null character for the input
 * string."
 * "If successful, returns the number of bytes written" or required (if
 * _ubytes == 0), OR "0 if it does not succeed".
 */
#define WCS2U8(_wstr, _wchars, _u8, _ubytes) \
	WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, \
			_wstr, _wchars, _u8, _ubytes, \
			NULL, NULL)
#define WCS2U8_BUFF_INSUFFICIENT \
	(GetLastError() == ERROR_INSUFFICIENT_BUFFER)
#define WCS2U8_ERRNO() GetLastError()

#else /* _WIN32 */
#error "unsupported platform" /* TODO */
	/* "[R]eturns the number of bytes written into the multibyte output
	 * string, excluding the terminating NULL (if any)".  Copies until \0 is
	 * met in wstr or buffer runs out.  If \0 is met, it's copied, but not
	 * counted in return. (silly fn) */
	/* "[T]he multibyte character string at mbstr is null-terminated
	 * only if wcstombs encounters a wide-character null character
	 * during conversion." */
	// wcstombs(charp, wstr, octet_length);
#endif /* _WIN32 */

#ifdef UNICODE
typedef wstr_st tstr_st;
#else /* UNICODE */
typedef cstr_st tstr_st;
#endif /* UNICODE */


/* 
 * JSON-escapes a string.
 * If string len is 0, it assumes a NTS.
 * If output buffer (jout) is NULL, it returns the buffer size needed for
 * escaping.
 * Returns number of used bytes in buffer (which might be less than buffer
 * size, if some char needs an escaping longer than remaining space).
 */
size_t json_escape(const char *jin, size_t inlen, char *jout, size_t outlen);


#endif /* __UTIL_H__ */

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
