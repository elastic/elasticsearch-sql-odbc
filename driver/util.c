/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "util.h"
#include "handles.h"
#include "error.h"

BOOL wstr2bool(wstr_st *val)
{
	/*INDENT-OFF*/
	switch (val->cnt) {
		case /*""*/0: return FALSE;
		case /*0*/1: return ! EQ_CASE_WSTR(val, &MK_WSTR("0"));
		case /*no*/2: return ! EQ_CASE_WSTR(val, &MK_WSTR("no"));
		case /*off*/3: return ! EQ_CASE_WSTR(val, &MK_WSTR("off"));
		case /*false*/5: return ! EQ_CASE_WSTR(val, &MK_WSTR("false"));
	}
	/*INDENT-ON*/
	return TRUE;
}

int str2ubigint(void *val, BOOL wide, SQLUBIGINT *out, BOOL strict)
{
	SQLUBIGINT res, digit;
	size_t i, cnt;
	SQLWCHAR *wstr;
	SQLCHAR *cstr;
	static const SQLUBIGINT max_div_10 = ULLONG_MAX / 10ULL;
	assert(sizeof(SQLUBIGINT) == sizeof(unsigned long long));

	if (wide) {
		wstr = ((wstr_st *)val)->str;
		cnt = ((wstr_st *)val)->cnt;
	} else {
		cstr = ((cstr_st *)val)->str;
		cnt = ((cstr_st *)val)->cnt;
	}

	if (cnt < 1) {
		errno = EINVAL;
		return -1;
	}
	digit = wide ? wstr[0] : cstr[0];
	i = (digit == '+') ? 1 : 0; /* L'+' =(ubigint)= '+' */

	for (res = 0; i < cnt; i ++) {
		digit = wide ? wstr[i] - L'0' : cstr[i] - '0';
		/* is it a number? */
		if (digit < 0 || 9 < digit) {
			if (strict) {
				errno = EINVAL;
				return -1;
			} else {
				break;
			}
		}
		assert(sizeof(SQLUBIGINT) == sizeof(uint64_t));
		if (i < ESODBC_PRECISION_UINT64 - 1) {
			res *= 10;
			res += digit;
		} else {
			/* would it overflow? */
			if (max_div_10 < res) {
				errno = ERANGE;
				return -1;
			} else {
				res *= 10;
			}
			if (ULLONG_MAX - res < digit) {
				errno = ERANGE;
				return -1;
			} else {
				res += digit;
			}
		}
	}
	if (cnt <= 1 && digit == '+') {
		/* prevent '+' or smth like '+a..' from returning a valid result */
		return -1;
	}
	*out = res;
	assert(i <= INT_MAX); /* long will take less than INT_MAX digits */
	return (long)i;
}

int str2bigint(void *val, BOOL wide, SQLBIGINT *out, BOOL strict)
{
	SQLUBIGINT ull; /* unsigned long long */
	size_t i;
	BOOL negative;
	int ret;
	cstr_st cstr;
	wstr_st wstr;

	if (wide) {
		wstr = *(wstr_st *)val;
		i = wstr.cnt;
	} else {
		cstr = *(cstr_st *)val;
		i = cstr.cnt;
	}

	if (i < 1) {
		errno = EINVAL;
		return -1;
	} else {
		switch (wide ? wstr.str[0] : cstr.str[0]) {
			case '-': /* L'-' =(size_t)= '-' */
				negative = TRUE;
				i = 1;
				break;
			case '+': /* L'+' =(size_t)= '+' */
				negative = FALSE;
				i  = 1;
				break;
			default:
				negative = FALSE;
				i = 0;
		}
	}

	if (wide) {
		wstr.str += i;
		wstr.cnt -= i;
		ret = str2ubigint(&wstr, wide, &ull, strict);
	} else {
		cstr.str += i;
		cstr.cnt -= i;
		ret = str2ubigint(&cstr, wide, &ull, strict);
	}
	if (ret < 0) { /* str2ubigint(,strict) won't return 0 */
		return -1;
	} else if (ret == 0 && i) { /* +/- only is NaN */
		return -1;
	}
	if (negative) {
		if ((SQLUBIGINT)LLONG_MIN < ull) {
			errno = ERANGE;
			return -1; /* underflow */
		} else {
			*out = -(SQLBIGINT)ull;
		}
	} else {
		if ((SQLUBIGINT)LLONG_MAX < ull) {
			errno = ERANGE;
			return -1; /* overflow */
		} else {
			*out = (SQLBIGINT)ull;
		}
	}
	return ret + (int)i;
}

int str2double(void *val, BOOL wide, SQLDOUBLE *dbl, BOOL strict)
{
	wstr_st wstr;
	cstr_st cstr;
	wchar_t *endwptr;
	char *endptr;
	size_t digits;

	errno = 0;
	if (wide) {
		wstr = *(wstr_st *)val;
		if (wstr.str[wstr.cnt]) {
			// TODO: simple quick parser instead of scanf
			if (_snwscanf(wstr.str, wstr.cnt, L"%le", (double *)dbl) != 1) {
				return -1;
			}
			digits = wstr.cnt; // TODO
		} else {
			*dbl = wcstod((wchar_t *)wstr.str, &endwptr);
			digits = endwptr - wstr.str;
			if (errno || (strict && wstr.cnt != digits)) {
				return -1;
			}
		}
	} else {
		cstr = *(cstr_st *)val;
		if (cstr.str[cstr.cnt]) {
			if (_snscanf(cstr.str, cstr.cnt, "%le", (double *)dbl) != 1) {
				return -1;
			}
			digits = cstr.cnt;
		} else {
			*dbl = strtod((char *)cstr.str, &endptr);
			digits = endptr - cstr.str;
			if (errno || (strict && cstr.cnt != digits)) {
				return -1;
			}
		}
	}
	assert(digits <= INT_MAX);
	return (int)digits;
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
 * Trims leading WS of a wide string of 'chars' length.
 * 0-terminator should not be counted (as it's a non-WS).
 */
void wltrim_ws(wstr_st *wstr)
{
	size_t cnt = wstr->cnt;
	SQLWCHAR *wcrr = wstr->str;
	SQLWCHAR *wend = wcrr + cnt;

	/* left trim */
	while (wcrr < wend && iswspace(*wcrr)) {
		wcrr ++;
		cnt --;
	}

	*wstr = (wstr_st) {
		wcrr, cnt
	};
}

/*
 * Trims leading and trailing WS of a wide string of 'chars' length.
 * 0-terminator should not be counted (as it's a non-WS).
 */
void wrtrim_ws(wstr_st *wstr)
{
	size_t cnt = wstr->cnt;
	SQLWCHAR *wcrr = wstr->str;

	/* right trim */
	while (cnt && iswspace(wcrr[cnt - 1])) {
		cnt --;
	}

	*wstr = (wstr_st) {
		wcrr, cnt
	};
}

void trim_ws(cstr_st *cstr)
{
	size_t cnt = cstr->cnt;
	SQLCHAR *crr = cstr->str;
	SQLCHAR *end = crr + cnt;

	/* right trim */
	while (crr < end && isspace(*crr)) {
		crr ++;
		cnt --;
	}

	/* left trim */
	while (cnt && isspace(crr[cnt - 1])) {
		cnt --;
	}

	*cstr = (cstr_st) {
		crr, cnt
	};
}

BOOL wtrim_at(wstr_st *wstr, SQLWCHAR wchar)
{
	SQLWCHAR *pos, *end;

	for (pos = wstr->str, end = pos + wstr->cnt; pos < end; pos ++) {
		if (*pos == wchar) {
			wstr->cnt = pos - wstr->str;
			return TRUE;
		}
	}
	return FALSE;
}

/*
 * Converts a wchar_t string to a C string for ASCII characters.
 * 'dst' should be at least as character-long as 'src', if 'src' is
 * 0-terminated, OR one character longer otherwise (for the 0-term).
 * 'dst' will always be 0-term'd.
 * Returns negative if conversion fails, OR number of converted wchars,
 * including/plus the 0-term.
 */
int TEST_API ascii_w2c(const SQLWCHAR *src, SQLCHAR *dst, size_t chars)
{
	size_t i = 0;

	if (chars < 1) {
		return -1;
	}
	assert(chars < INT_MAX);

	do {
		if (SCHAR_MAX < src[i]) {
			return -((int)i + 1);
		}
		dst[i] = (SQLCHAR)src[i];
	} while ((src[i] != L'\0') && (++i < chars));

	if (chars <= i) { /* equiv to: (src[i] != 0) */
		/* loop stopped b/c of length -> src is not 0-term'd */
		dst[i] = 0; /* chars + 1 <= [dst] */
	}
	return (int)i + 1;
}

/*
 * This is the inverse of ascii_w2c().
 */
int TEST_API ascii_c2w(const SQLCHAR *src, SQLWCHAR *dst, size_t chars)
{
	size_t i = 0;

	if (chars < 1) {
		return -1;
	}
	assert(chars < INT_MAX);

	do {
		if (src[i] < 0) {
			return -((int)i + 1);
		}
		dst[i] = (SQLWCHAR)src[i];
	} while ((src[i] != '\0') && (++i < chars));

	if (chars <= i) { /* equiv to: (src[i] != 0) */
		/* loop stopped b/c of length -> src is not 0-term'd */
		dst[i] = L'\0'; /* chars + 1 <= [dst] */
	}
	return (int)i + 1;
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
			/* '/' needs no quoting as per ECMA spec, even if listed on
			 * json.org; ES/SQL uses it unescaped in cursor values. */
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
 * If string [in]len is 0, it assumes a NTS.
 * If output buffer (jout) is NULL, it returns the buffer size needed for
 * escaping.
 * Returns number of used bytes in buffer (which might be less than out buffer
 * size (outlen), if some char needs an escaping longer than remaining space).
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
			/* '/' needs no quoting as per ECMA spec, even if listed on
			 * json.org; ES/SQL uses it unescaped in cursor values. */
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
					memcpy(jout + pos, JSON_ESC_GEN_PREF,
							sizeof(JSON_ESC_GEN_PREF) - 1);
					pos += sizeof(JSON_ESC_GEN_PREF) - 1;
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

/*
 * JSON-escapes a string (str), outputting the result in the same buffer.
 * The buffer needs to be long enough (outlen) for this operation (at
 * least json_escaped_len() long).
 * If string [in]len is 0, it assumes a NTS.
 * Returns number of used bytes in buffer (which might be less than out buffer
 * size (outlen), if some char needs an escaping longer than remaining space).
 */
size_t json_escape_overlapping(char *str, size_t inlen, size_t outlen)
{
	char *jin;

	/* move the string to the end of the buffer */
	jin = str + (outlen - inlen);
	memcpy(jin, str, inlen);
	return json_escape(jin, inlen, str, outlen);
}

/* Note: for input/output size indication (avail/usedp), some functions
 * require character count (eg. SQLGetDiagRec, SQLDescribeCol), some others
 * bytes length (eg.  SQLGetInfo, SQLGetDiagField, SQLGetConnectAttr,
 * EsSQLColAttributeW). */
/*
 * Copy a WSTR back to application; typically with non-SQLFetch() calls.
 * The WSTR must not count the 0-term, but must include it.
 * The function checks against the correct size of available bytes, copies the
 * wstr according to avaialble space and indicates the available bytes to copy
 * back into provided buffer (if not NULL).
 */
SQLRETURN TEST_API write_wstr(SQLHANDLE hnd, SQLWCHAR *dest, wstr_st *src,
	SQLSMALLINT /*B*/avail, SQLSMALLINT /*B*/*usedp)
{
	size_t wide_avail;

	DBGH(hnd, "copying %zd wchars (`" LWPDL "`) into buffer @0x%p, of %dB "
		"len; out-len @0x%p.", src->cnt, LWSTR(src), dest, avail, usedp);

	/* cnt must not count the 0-term (XXX: ever need to copy 0s?) */
	assert(src->cnt <= 0 || src->str[src->cnt - 1] != L'\0');
	assert(src->cnt <= 0 || src->str[src->cnt] == L'\0');

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
		if (avail < 0 || avail % sizeof(SQLWCHAR)) {
			ERRH(hnd, "invalid buffer length provided: %hd.", avail);
			RET_HDIAGS(hnd, SQL_STATE_HY090);
		} else {
			wide_avail = avail/sizeof(SQLWCHAR);
		}

		/* '=' (in <=), since src->cnt doesn't count the \0 */
		if (wide_avail <= src->cnt) {
			if (0 < wide_avail) {
				wcsncpy(dest, src->str, wide_avail - /* 0-term */1);
				dest[wide_avail - 1] = L'\0';
			}

			INFOH(hnd, "not enough buffer size to write required string (plus "
				"terminator): `" LWPDL "` [%zu]; available: %zu.",
				LWSTR(src), src->cnt, wide_avail);
			RET_HDIAGS(hnd, SQL_STATE_01004);
		} else {
			assert(src->str);
			wcsncpy(dest, src->str, src->cnt + /* 0-term */1);
		}
	} else {
		/* only return how large of a buffer we need */
		INFOH(hnd, "NULL out buff.");
	}

	return SQL_SUCCESS;
}

cstr_st TEST_API *wstr_to_utf8(wstr_st *src, cstr_st *dst)
{
	int len;
	size_t cnt;
	void *addr;
	BOOL nts; /* is the \0 present and counted in source string? */

	if (0 < src->cnt) {
		nts = !src->str[src->cnt - 1];

		/* eval the needed space for conversion */
		len = U16WC_TO_MBU8(src->str, src->cnt, NULL, 0);
		if (! len) {
			ERRN("failed to eval UTF-8 conversion space necessary for [%zu] "
				"`" LWPDL "`.", src->cnt, LWSTR(src));
			return NULL;
		}
	} else {
		nts = FALSE;
		len = 0;
	}

	assert(0 <= len);
	/* explicitely allocate the \0 if not present&counted  */
	cnt = len + /*0-term?*/!nts;
	if (! dst) { /* if null destination, allocate that as well */
		cnt += sizeof(cstr_st);
	}

	if (! (addr = malloc(cnt))) {
		ERRN("OOM for size: %zuB.", cnt);
		return NULL;
	}
	if (! dst) {
		dst = (cstr_st *)addr;
		dst->str = (uint8_t *)addr + sizeof(cstr_st);
	} else {
		dst->str = (SQLCHAR *)addr;
	}

	if (0 < src->cnt) {
		/* convert the string */
		len = U16WC_TO_MBU8(src->str, src->cnt, dst->str, len);
		if (! len) {
			/* should not happen, since a first scan already happened */
			ERRN("failed to UTF-8 convert `" LWPDL "`.", LWSTR(src));
			free(addr);
			return NULL;
		}
	}

	if (! nts) {
		dst->str[len] = 0;
	}
	dst->cnt = len;

	return dst;
}

wstr_st TEST_API *utf8_to_wstr(cstr_st *src, wstr_st *dst)
{
	int len;
	size_t cnt;
	void *addr;
	BOOL nts; /* is the \0 present and counted in source string? */

	if (0 < src->cnt) {
		nts = !src->str[src->cnt - 1];

		/* eval the needed space for conversion */
		len = U8MB_TO_U16WC(src->str, src->cnt, NULL, 0);
		if (! len) {
			ERRN("failed to eval UTF-16 conversion space necessary for [%zu] "
				"`" LCPDL "`.", src->cnt, LCSTR(src));
			return NULL;
		}
	} else {
		nts = FALSE;
		len = 0;
	}

	assert(0 <= len);
	/* explicitely allocate the \0 if not present&counted  */
	cnt = len + /*0-term?*/!nts;
	if (! dst) { /* if null destination, allocate that as well */
		cnt += sizeof(wstr_st);
	}

	if (! (addr = malloc(cnt * sizeof(SQLWCHAR)))) {
		ERRN("OOM for size: %zuB.", cnt);
		return NULL;
	}
	if (! dst) {
		dst = (wstr_st *)addr;
		dst->str = (SQLWCHAR *)((uint8_t *)addr + sizeof(wstr_st));
	} else {
		dst->str = (SQLWCHAR *)addr;
	}

	if (0 < src->cnt) {
		/* convert the string */
		len = U8MB_TO_U16WC(src->str, src->cnt, dst->str, len);
		if (! len) {
			/* should not happen, since a first scan already happened */
			ERRN("failed to UTF-16 convert `" LCPDL "`.", LCSTR(src));
			free(addr);
			return NULL;
		}
	}

	if (! nts) {
		dst->str[len] = 0;
	}
	dst->cnt = len;

	return dst;
}

/* Escape `%`, `_`, `\` characters in 'src'.
 * If not 'force'-d, the escaping will stop on detection of pre-existing
 * escaping(*), OR if the chars to be escaped are stand-alone.
 * (*): invalid/incomplete escaping sequences - `\\\` -  are still considered
 * as containing escaping.
 * Returns: TRUE, if escaping has been applied  */
BOOL TEST_API metadata_id_escape(wstr_st *src, wstr_st *dst, BOOL force)
{
	size_t i, j;
	SQLWCHAR crr, prev;
	BOOL ret;

	ret = FALSE;
	prev = 0;
	for (i = 0, j = 0; i <= src->cnt; i ++) {
		crr = src->str[i];
		switch (crr) {
			case '%':
			case L'_':
			case MK_WPTR(ESODBC_CHAR_ESCAPE):
				/* is current char already escaped OR is it stand-alone? */
				if (prev == MK_WPTR(ESODBC_CHAR_ESCAPE) || src->cnt == 1) {
					if (! force) {
						wmemcpy(dst->str, src->str, src->cnt);
						dst->cnt = src->cnt;
						return FALSE;
					}
				}
				dst->str[j ++] = MK_WPTR(ESODBC_CHAR_ESCAPE);
				ret = TRUE;
			/* no break */
			default:
				dst->str[j ++] = crr;
		}
		prev = crr;
	}

	dst->cnt = 0 < j ? j - 1 : 0;
	return ret;
}

/* Simple hex printing of a cstr_st object.
 * Returns (thread local static) printed buffer, always 0-term'd. */
char *cstr_hex_dump(const cstr_st *buff)
{
#	ifndef WITH_EXTENDED_BUFF_LOG
	static thread_local char dest[ESODBC_LOG_BUF_SIZE];
	const size_t dest_sz = sizeof(dest);
#	else /* !WITH_EXTENDED_BUFF_LOG */
	static char *dest = NULL;
	static esodbc_mutex_lt dest_mux = ESODBC_MUX_SINIT;
	const size_t dest_sz = ESODBC_EXT_LOG_BUF_SIZE;
#	endif /* !WITH_EXTENDED_BUFF_LOG */
	char *to, *from;
	char *to_end, *from_end;
	int n;


#	ifdef WITH_EXTENDED_BUFF_LOG
	ESODBC_MUX_LOCK(&dest_mux);
	if (! buff) { /* free resorces */
		if (dest) {
			free(dest);
			dest = NULL;
		}
		ESODBC_MUX_DEL(&dest_mux);
		goto end;
	} else if (! dest) {
		if (! (dest = malloc(dest_sz))) {
			ERRN("OOM for %zd bytes.", dest_sz);
			ESODBC_MUX_UNLOCK(&dest_mux);
			return "<OOM>";
		}
	}
#	endif /* WITH_EXTENDED_BUFF_LOG */

	to = dest;
	to_end = dest + dest_sz;
	from = buff->str;
	from_end = buff->str + buff->cnt;
	int i = 0;
	while (to < to_end && from < from_end) {
		n = sprintf(to, "%.2X", (uint8_t)*from ++);
		if (n < 0) {
			ERRN("failed to print serialized CBOR object");
			return NULL;
		}
		to += (size_t)n;
	}
	/* add the 0-terminator */
	if (to < to_end) { /* still space for it? */
		*to ++ = '\0';
	} else { /* == */
		dest[dest_sz - 1] = '\0'; /* overwrite last position */
	}

#	ifdef WITH_EXTENDED_BUFF_LOG
end:
	ESODBC_MUX_UNLOCK(&dest_mux);
#	endif /* WITH_EXTENDED_BUFF_LOG */
	return dest;
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
