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

#include <string.h>

#include "util.h"



BOOL wstr2bool(wstr_st *val)
{
	switch (val->cnt) {
		case /*0*/1: return ! EQ_CASE_WSTR(val, &MK_WSTR("0"));
		case /*no*/2: return ! EQ_CASE_WSTR(val, &MK_WSTR("no"));
		case /*false*/5: return ! EQ_CASE_WSTR(val, &MK_WSTR("false"));
	}
	return TRUE;
}

BOOL wstr2long(wstr_st *val, long *out)
{
	long res = 0, digit;
	int i = 0;
	BOOL negative;

	if (val->cnt < 1)
		return FALSE;

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

	for ( ; i < val->cnt; i ++) {
		/* is it a number? */
		if (val->str[i] < L'0' || L'9' < val->str[i])
			return FALSE;
		digit = val->str[i] - L'0';
		/* would it overflow?*/
		if (LONG_MAX - res < digit)
			return FALSE;
		res *= 10;
		res += digit;
	}
	*out = negative ? - res : res;
	return TRUE;
}

/*
 * Converts a wchar_t string to a C string for ANSI characters.
 * 'dst' should be as character-long as 'src', if 'src' is 0-terminated,
 * OR one character longer otherwise (for the 0-term).
 * 'dst' will always be 0-term'd.
 * Returns negative if conversion fails, OR number of converted wchars,
 * including the 0-term.
 *
 */
int ansi_w2c(const SQLWCHAR *src, char *dst, size_t chars)
{
	int i = 0;
	
	do {
		if (CHAR_MAX < src[i])
			return -(i + 1);
		dst[i] = (char)src[i];
	} while (src[i] && (++i < chars));

	if (chars <= i) { /* equiv to: (src[i] != 0) */
		/* loop stopped b/c of length -> src is not 0-term'd */
		dst[i] = 0;
	}
	return i + 1;
}

int wmemncasecmp(const SQLWCHAR *a, const SQLWCHAR *b, size_t len)
{
	size_t i;
	int diff = 0; /* if len == 0 */
	for (i = 0; i < len; i ++) {
		diff = towlower(a[i]) - towlower(b[i]);
		if (diff)
			break;
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

const SQLWCHAR* wcsnstr(const SQLWCHAR *hay, size_t len, SQLWCHAR needle)
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

	if (! inlen)
		inlen = strlen(jin);
	if (! jout)
		return json_escaped_len(jin, inlen);

	for (i = 0, pos = 0; i < inlen; i ++) {
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
	}
	return pos;
#undef I16TOA
}


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
