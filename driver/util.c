/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <string.h>
#include <errno.h>

#include "util.h"
#include "log.h"
#include "error.h"



BOOL wstr2bool(wstr_st *val)
{
	/*INDENT-OFF*/
	switch (val->cnt) {
		case /*0*/1: return ! EQ_CASE_WSTR(val, &MK_WSTR("0"));
		case /*no*/2: return ! EQ_CASE_WSTR(val, &MK_WSTR("no"));
		case /*false*/5: return ! EQ_CASE_WSTR(val, &MK_WSTR("false"));
	}
	/*INDENT-ON*/
	return TRUE;
}

BOOL wstr2ullong(wstr_st *val, unsigned long long *out)
{
	unsigned long long res, digit;
	static const unsigned long long max_div_10 = ULLONG_MAX / 10ULL;
	int i = 0;

	if (val->cnt < 1) {
		errno = EINVAL;
		return FALSE;
	} else if (val->str[0] == L'+') {
		i ++;
	}

	for (res = 0; i < val->cnt; i ++) {
		/* is it a number? */
		if (val->str[i] < L'0' || L'9' < val->str[i]) {
			errno = EINVAL;
			return FALSE;
		} else {
			digit = val->str[i] - L'0';
		}
		assert(sizeof(unsigned long long) == sizeof(uint64_t));
		if (i < ESODBC_PRECISION_UINT64 - 1) {
			res *= 10;
			res += digit;
		} else {
			/* would it overflow? */
			if (max_div_10 < res) {
				errno = ERANGE;
				return FALSE;
			} else {
				res *= 10;
			}
			if (ULLONG_MAX - res < digit) {
				errno = ERANGE;
				return FALSE;
			} else {
				res += digit;
			}
		}
	}
	*out = res;
	return TRUE;
}

BOOL wstr2llong(wstr_st *val, long long *out)
{
	unsigned long long ull;
	wstr_st uval;
	int i = 0;
	BOOL negative;

	if (val->cnt < 1) {
		errno = EINVAL;
		return FALSE;
	} else {
		switch (val->str[0]) {
			case L'-':
				negative = TRUE;
				i ++;
				break;
			case '+':
				negative = FALSE;
				i ++;
				break;
			default:
				negative = FALSE;
		}
	}

	uval = (wstr_st) {
		.str = val->str + i, .cnt = val->cnt - i
	};
	if (! wstr2ullong(&uval, &ull)) {
		return FALSE;
	}
	if (negative) {
		if ((unsigned long long)LLONG_MIN < ull) {
			errno = ERANGE;
			return FALSE; /* underflow */
		} else {
			*out = -(long long)ull;
		}
	} else {
		if ((unsigned long long)LLONG_MAX < ull) {
			errno = ERANGE;
			return FALSE; /* overflow */
		} else {
			*out = (long long)ull;
		}
	}
	return TRUE;
}


size_t i64tot(int64_t i64, void *buff, BOOL wide)
{
	if (wide) {
		_i64tow((int64_t)i64, buff, /*radix*/10);
		/* TODO: find/write a function that returns len of conversion? */
		return wcslen(buff);
	} else {
		_i64toa((int64_t)i64, buff, /*radix*/10);
		return strlen(buff);
	}
}

size_t ui64tot(uint64_t ui64, void *buff, BOOL wide)
{
	if (wide) {
		_ui64tow((uint64_t)ui64, buff, /*radix*/10);
		return wcslen(buff);
	} else {
		_ui64toa((uint64_t)ui64, buff, /*radix*/10);
		return strlen(buff);
	}
}

/*
 * Trims leading and trailing WS of a wide string of 'chars' length.
 * 0-terminator should not be counted (as it's a non-WS).
 */
const SQLWCHAR *trim_ws(const SQLWCHAR *wstr, size_t *chars)
{
	const SQLWCHAR *wend;
	size_t cnt = *chars;

	/* right trim */
	for (wend = wstr + cnt; wstr < wend && iswspace(*wstr); wstr ++) {
		cnt --;
	}

	while ((0 < cnt) && iswspace(wstr[cnt - 1])) {
		cnt --;
	}

	*chars = cnt;
	return wstr;
}

/*
 * Converts a wchar_t string to a C string for ANSI characters.
 * 'dst' should be at least as character-long as 'src', if 'src' is
 * 0-terminated, OR one character longer otherwise (for the 0-term).
 * 'dst' will always be 0-term'd.
 * Returns negative if conversion fails, OR number of converted wchars,
 * including the 0-term.
 *
 */
int ansi_w2c(const SQLWCHAR *src, char *dst, size_t chars)
{
	int i = 0;

	if (chars < 1) {
		return -1;
	}

	do {
		if (CHAR_MAX < src[i]) {
			return -(i + 1);
		}
		dst[i] = (char)src[i];
	} while (src[i] && (++i < chars));

	if (chars <= i) { /* equiv to: (src[i] != 0) */
		/* loop stopped b/c of length -> src is not 0-term'd */
		dst[i] = 0; /* chars + 1 <= [dst] */
	}
	return i + 1;
}

int wmemncasecmp(const SQLWCHAR *a, const SQLWCHAR *b, size_t len)
{
	size_t i;
	int diff = 0; /* if len == 0 */
	for (i = 0; i < len; i ++) {
		diff = towlower(a[i]) - towlower(b[i]);
		if (diff) {
			break;
		}
	}
	//DBG("`" LWPDL "` vs `" LWPDL "` => %d (len=%zd, i=%d).",
	//		len, a, len, b, diff, len, i);
	return diff;
}

int wszmemcmp(const SQLWCHAR *a, const SQLWCHAR *b, long count)
{
	int diff;

	for (; *a && *b && count; a ++, b ++, count --) {
		diff = *a - *b;
		if (diff) {
			return diff;
		}
	}
	if (! count) {
		return 0;
	}
	return *a - *b;
}

const SQLWCHAR *wcsnstr(const SQLWCHAR *hay, size_t len, SQLWCHAR needle)
{
	size_t i;
	for (i = 0; i < len; i ++) {
		if (hay[i] == needle) {
			return hay + i;
		}
	}
	return NULL;
}

/* retuns the length of a buffer to hold the escaped variant of the unescaped
 * given json object  */
static inline size_t json_escaped_len(const char *json, size_t len)
{
	size_t i, newlen = 0;
	unsigned char uchar;
	for (i = 0; i < len; i ++) {
		uchar = (unsigned char)json[i];
		switch(uchar) {
			case '"':
			case '\\':
			case '/':
			case '\b':
			case '\f':
			case '\n':
			case '\r':
			case '\t':
				newlen += /* '\' + json[i] */2;
				break;
			default:
				newlen += (0x20 <= uchar) ? 1 : /* \u00XX */6;
				// break
		}
	}
	return newlen;
}

/*
 * JSON-escapes a string.
 * If string len is 0, it assumes a NTS.
 * If output buffer (jout) is NULL, it returns the buffer size needed for
 * escaping.
 * Returns number of used bytes in buffer (which might be less than buffer
 * size, if some char needs an escaping longer than remaining space).
 */
size_t json_escape(const char *jin, size_t inlen, char *jout, size_t outlen)
{
	size_t i, pos;
	unsigned char uchar;

#define I16TOA(_x)	(10 <= (_x)) ? 'A' + (_x) - 10 : '0' + (_x)

	if (! inlen) {
		inlen = strlen(jin);
	}
	if (! jout) {
		return json_escaped_len(jin, inlen);
	}

	for (i = 0, pos = 0; i < inlen; i ++) {
		/*INDENT-OFF*/
		uchar = jin[i];
		switch(uchar) {
			do {
			case '\b': uchar = 'b'; break;
			case '\f': uchar = 'f'; break;
			case '\n': uchar = 'n'; break;
			case '\r': uchar = 'r'; break;
			case '\t': uchar = 't'; break;
			} while (0);
			case '"':
			case '\\':
			case '/':
				if (outlen <= pos + 1) {
					i = inlen; // break the for loop
					continue;
				}
				jout[pos ++] = '\\';
				jout[pos ++] = (char)uchar;
				break;
			default:
				if (0x20 <= uchar) {
					if (outlen <= pos) {
						i = inlen;
						continue;
					}
					jout[pos ++] = uchar;
				} else { // case 0x00 .. 0x1F
					if (outlen <= pos + 5) {
						i = inlen;
						continue;
					}
					memcpy(jout + pos, "\\u00", sizeof("\\u00") - 1);
					pos += sizeof("\\u00") - 1;
					jout[pos ++] = I16TOA(uchar >> 4);
					jout[pos ++] = I16TOA(uchar & 0xF);
				}
				break;
		}
		/*INDENT-ON*/
	}
	return pos;
#undef I16TOA
}

/* Note: for input/output size indication (avail/usedp), some functions
 * require character count (eg. SQLGetDiagRec, SQLDescribeCol), some others
 * bytes length (eg.  SQLGetInfo, SQLGetDiagField, SQLGetConnectAttr,
 * EsSQLColAttributeW). */
/*
 * Copy a WSTR back to application; typically with non-SQLFetch() calls.
 * The WSTR must not count the 0-tem.
 * The function checks against the correct size of available bytes, copies the
 * wstr according to avaialble space and indicates the available bytes to copy
 * back into provided buffer (if not NULL).
 */
SQLRETURN write_wstr(SQLHANDLE hnd, SQLWCHAR *dest, wstr_st *src,
	SQLSMALLINT /*B*/avail, SQLSMALLINT /*B*/*usedp)
{
	size_t awail;

	/* cnt must not count the 0-term (XXX: ever need to copy 0s?) */
	assert(src->cnt <= 0 || src->str[src->cnt - 1]);

	DBGH(hnd, "copying %zd wchars (`" LWPDL "`) into buffer @0x%p, of %dB "
		"len; out-len @0x%p.", src->cnt, LWSTR(src), dest, avail, usedp);

	if (usedp) {
		/* how many bytes are available to return (not how many would be
		 * written into the buffer (which could be less));
		 * it excludes the 0-term .*/
		*usedp = (SQLSMALLINT)(src->cnt * sizeof(src->str[0]));
	} else {
		INFOH(hnd, "NULL required-space-buffer provided.");
	}

	if (dest) {
		/* needs to be multiple of SQLWCHAR units (2 on Win) */
		if (avail % sizeof(SQLWCHAR)) {
			ERRH(hnd, "invalid buffer length provided: %d.", avail);
			RET_DIAG(&HDRH(hnd)->diag, SQL_STATE_HY090, NULL, 0);
		} else {
			awail = avail/sizeof(SQLWCHAR);
		}

		if (awail <= src->cnt) { /* =, since src->cnt doesn't count the \0 */
			wcsncpy(dest, src->str, awail - /* 0-term */1);
			dest[awail - 1] = 0;

			INFOH(hnd, "not enough buffer size to write required string (plus "
				"terminator): `" LWPD "` [%zd]; available: %d.",
				LWSTR(src), src->cnt, awail);
			RET_DIAG(&HDRH(hnd)->diag, SQL_STATE_01004, NULL, 0);
		} else {
			wcsncpy(dest, src->str, src->cnt + /* 0-term */1);
		}
	} else {
		/* only return how large of a buffer we need */
		INFOH(hnd, "NULL out buff.");
	}

	return SQL_SUCCESS;
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
