/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#ifndef __UTIL_H__
#define __UTIL_H__

/* NOTE: this must be included in "top level" file (wherever SQL types are
 * used  */
#if defined(_WIN32) || defined (WIN32)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
/* win function parameter attributes */
#include <windows.h>
#include <tchar.h>
#endif /* _WIN32/WIN32 */

#include <inttypes.h>
#include <wchar.h>
#include <assert.h>

#include "sql.h"
#include "sqlext.h"

/* export attribute for internal functions used for testing */
#ifndef TEST_API /* Release builds define this to an empty macro */
#ifdef DRIVER_BUILD
#define TEST_API	__declspec(dllexport)
#else /* _EXPORTS */
#define TEST_API	__declspec(dllimport)
#endif /* _EXPORTS */
#define TESTING		/* compiles in the testing code */
#endif /* TEST_API */

/*
 * Assert two integral types have same storage and sign.
 */
#define ASSERT_INTEGER_TYPES_EQUAL(a, b) \
	assert((sizeof(a) == sizeof(b)) && \
		( (0 < (a)0 - 1 && 0 < (b)0 - 1) || \
			(0 > (a)0 - 1 && 0 > (b)0 - 1) ))

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

#define MK_CPTR(_cstr_)		_cstr_

#ifdef UNICODE
#define MK_TPTR		MK_WPTR
#else /* UNICODE */
#define MK_TPTR		MK_CPTR
#endif /* UNICODE */

#if !defined(_WIN32) && !defined (WIN32)
#define _T			MK_TPTR
#endif /* _WIN32/WIN32 */


typedef struct cstr {
	SQLCHAR *str;
	size_t cnt;
} cstr_st;

/*
 * Converts a wchar_t string to a C string for ASCII characters.
 * 'dst' should be at least as character-long as 'src', if 'src' is
 * 0-terminated, OR one character longer otherwise (for the 0-term).
 * 'dst' will always be 0-term'd.
 * Returns negative if conversion fails, OR number of converted wchars,
 * including/plus the 0-term.
 */
int TEST_API ascii_w2c(const SQLWCHAR *src, SQLCHAR *dst, size_t chars);
int TEST_API ascii_c2w(const SQLCHAR *src, SQLWCHAR *dst, size_t chars);
/*
 * Compare two SQLWCHAR object, case INsensitive.
 */
int wmemncasecmp(const SQLWCHAR *a, const SQLWCHAR *b, size_t len);
/*
 * Compare two zero-terminated SQLWCHAR* objects, until a 0 is encountered in
 * either of them or until 'count' characters are evaluated. If 'count'
 * parameter is negative, it is ignored.
 *
 * This is useful in comparing SQL strings which the API allows to be passed
 * either as 0-terminated or not (SQL_NTS).
 * The function does a single pass (no length evaluation of the strings).
 * wmemcmp() might read over the boundary of one of the objects, if the
 * provided 'count' paramter is not the minimum of the strings' length.
 */
int wszmemcmp(const SQLWCHAR *a, const SQLWCHAR *b, long count);

/*
 * wcsstr() variant for non-NTS.
 */
const SQLWCHAR *wcsnstr(const SQLWCHAR *hay, size_t len, SQLWCHAR needle);

typedef struct wstr {
	SQLWCHAR *str;
	size_t cnt;
} wstr_st;

/*
 * Turns a static C string into a wstr_st.
 */
#ifndef __cplusplus /* no MSVC support for compound literals with /TP */
#	define MK_WSTR(_s)	((wstr_st){.str = MK_WPTR(_s), .cnt = sizeof(_s) - 1})
#	define MK_CSTR(_s)	((cstr_st){.str = _s, .cnt = sizeof(_s) - 1})
#endif /* !__cplusplus */
#define WSTR_INIT(_s)	{MK_WPTR(_s), sizeof(_s) - 1}
#define CSTR_INIT(_s)	{(SQLCHAR *)_s, sizeof(_s) - 1}
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


typedef struct {
	BOOL wide;
	union {
		wstr_st w;
		cstr_st c;
	};
} xstr_st;

/* For as long as wptr_st & cptr_st are only differing in string pointer type,
 * it's safe to save the check if wide or not. */
#define XSTR_LEN(_xptr)	(_xptr)->w.cnt

/*
 * Trims leading and trailing WS of a wide string of 'chars' length.
 * 0-terminator should not be counted (as it's a non-WS).
 */
void trim_ws(cstr_st *str);
void wltrim_ws(wstr_st *wstr);
void wrtrim_ws(wstr_st *wstr);
#define wtrim_ws(_w) do { wltrim_ws(_w); wrtrim_ws(_w); } while (0)
/* Trim the w-string at first encounter of the give w-char.
 * Returns TRUE if character has been encounter / trimming occured. */
BOOL wtrim_at(wstr_st *wstr, SQLWCHAR wchar);

BOOL wstr2bool(wstr_st *val);
/* Converts a [cw]str_st to a SQL(U)BIGINT.
 * If !strict, parsing stops at first non-digit char.
 * Returns the number of parsed characters or negative of failure. */
int str2ubigint(void *val, BOOL wide, SQLUBIGINT *out, BOOL strict);
int str2bigint(void *val,  BOOL wide, SQLBIGINT *out, BOOL strict);
int str2double(void *val, BOOL wide, SQLDOUBLE *dbl, BOOL strict);

/* converts the int types to a C or wide string, returning the string length */
size_t i64tot(int64_t i64, void *buff, BOOL wide);
size_t ui64tot(uint64_t ui64, void *buff, BOOL wide);

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
	WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, \
		_wstr, _wchars, _u8, _ubytes, \
		NULL, NULL)
#define WCS2U8_BUFF_INSUFFICIENT \
	(GetLastError() == ERROR_INSUFFICIENT_BUFFER)
#define WCS2U8_ERRNO() GetLastError()


/*
 * Locking type and primitives.
 */
typedef SRWLOCK esodbc_mutex_lt;

#define ESODBC_MUX_SINIT		SRWLOCK_INIT
#define ESODBC_MUX_INIT(_m)		InitializeSRWLock(_m)
#define ESODBC_MUX_DEL(_m)		/* not needed/possible */

#define ESODBC_MUX_LOCK(_m)		AcquireSRWLockExclusive(_m)
#define ESODBC_MUX_TRYLOCK(_m)	TryAcquireSRWLockExclusive(_m)
#define ESODBC_MUX_UNLOCK(_m)	ReleaseSRWLockExclusive(_m)

#if defined(DRIVER_BUILD) && !defined(thread_local)
#define thread_local __declspec(thread)
#endif /* DRIVER_BUILD && !thread_local */

#define timegm _mkgmtime

#else /* _WIN32 */

#error "unsupported platform"
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


/* generic char JSON escaping prefix */
#define JSON_ESC_GEN_PREF	"\\u00"
/* octet length of one generic JSON escaped character */
#define JSON_ESC_SEQ_SZ		(sizeof(JSON_ESC_GEN_PREF) - 1 + /*0xAB*/2)
/*
 * JSON-escapes a string.
 * If string len is 0, it assumes a NTS.
 * If output buffer (jout) is NULL, it returns the buffer size needed for
 * escaping.
 * Returns number of used bytes in buffer (which might be less than buffer
 * size, if some char needs an escaping longer than remaining space).
 */
size_t json_escape(const char *jin, size_t inlen, char *jout, size_t outlen);
/*
 * JSON-escapes a string (str), outputting the result in the same buffer.
 * The buffer needs to be long enough (outlen) for this operation (at
 * least json_escaped_len() long).
 * If string [in]len is 0, it assumes a NTS.
 * Returns number of used bytes in buffer (which might be less than out buffer
 * size (outlen), if some char needs an escaping longer than remaining space).
 */
size_t json_escape_overlapping(char *str, size_t inlen, size_t outlen);

/*
 * Copy a WSTR back to application.
 * The WSTR must not count the 0-tem.
 * The function checks against the correct size of available bytes, copies the
 * wstr according to avaialble space and indicates the available bytes to copy
 * back into provided buffer (if not NULL).
 */
SQLRETURN write_wstr(SQLHANDLE hnd, SQLWCHAR *dest, wstr_st *src,
	SQLSMALLINT /*B*/avail, SQLSMALLINT /*B*/*usedp);

/*
 * Converts a wide string to a UTF-8 MB, allocating the necessary space.
 * The \0 is allocated and written, even if not present in source string, but
 * only counted in output string if counted in input one.
 * If 'dst' is null, the destination is also going to be allocate (collated
 * with the string). The caller only needs to free the allocated chunk
 * (returned pointer or dst->str).
 * Returns NULL on error.
 */
cstr_st TEST_API *wstr_to_utf8(wstr_st *src, cstr_st *dst);

/* Escape `%`, `_`, `\` characters in 'src'.
 * If not 'force'-d, the escaping will stop on detection of pre-existing
 * escaping(*), OR if the chars to be escaped are stand-alone.
 * (*): invalid/incomplete escaping sequences - `\\\` -  are still considered
 * as containing escaping.
 * Returns: TRUE, if escaping has been applied  */
BOOL TEST_API metadata_id_escape(wstr_st *src, wstr_st *dst, BOOL force);

/*
 * Printing aids.
 */

/*
 * w/printf() desriptors for char/wchar_t *
 * "WPrintF Wide/Char Pointer _ DESCriptor"
 */
#ifdef _WIN32
/* wprintf wide_t pointer descriptor */
#	define WPFWP_DESC		L"%s"
#	define WPFWP_LDESC		L"%.*s"
/* printf wide_t pointer descriptor */
#	define PFWP_DESC		"%S"
#	define PFWP_LDESC		"%.*S"
/* wprintf char pointer descriptor */
#	define WPFCP_DESC		L"%S"
#	define WPFCP_LDESC		L"%.*S"
/* printf char pointer descriptor */
#	define PFCP_DESC		"%s"
#	define PFCP_LDESC		"%.*s"
#else /* _WIN32 */
/* wprintf wide_t pointer descriptor */
#	define WPFWP_DESC		L"%S"
#	define WPFWP_LDESC		L"%.*S"
/* printf wide_t pointer descriptor */
#	define PFWP_DESC		"%S"
#	define PFWP_LDESC		"%.*S"
/* wprintf char pointer descriptor */
#	define WPFCP_DESC		L"%s"
#	define WPFCP_LDESC		L"%.*s"
/* printf char pointer descriptor */
#	define PFCP_DESC		"%s"
#	define PFCP_LDESC		"%.*s"
#endif /* _WIN32 */


/* ISO time formats lenghts.
 * ES/SQL interface should only use UTC ('Z'ulu offset). */
#define ISO8601_TIMESTAMP_LEN(prec)		\
	(sizeof("yyyy-mm-ddThh:mm:ss+hh:mm") - /*\0*/1 + /*'.'*/!!prec + prec)
#define ISO8601_TS_UTC_LEN(prec)		\
	(sizeof("yyyy-mm-ddThh:mm:ssZ") - /*\0*/1 + /*'.'*/!!prec + prec)
#define ISO8601_TIMESTAMP_MAX_LEN		\
	ISO8601_TIMESTAMP_LEN(ESODBC_MAX_SEC_PRECISION)
#define ISO8601_TIMESTAMP_MIN_LEN		\
	ISO8601_TS_UTC_LEN(0)

#define DATE_TEMPLATE_LEN				\
	(sizeof("yyyy-mm-dd") - /*\0*/1)
#define TIME_TEMPLATE_LEN(prec)			\
	(sizeof("hh:mm:ss") - /*\0*/1 + /*'.'*/!!prec + prec)
#define TIMESTAMP_TEMPLATE_LEN(prec)	\
	(DATE_TEMPLATE_LEN + /*' '*/1 + TIME_TEMPLATE_LEN(prec))

#endif /* __UTIL_H__ */

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
