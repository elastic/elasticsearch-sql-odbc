/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

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
		case '-':
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
		if (val->str[i] < '0' || '9' < val->str[i])
			return FALSE;
		digit = val->str[i] - '0';
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
 * 'dst' should be as character-long as 'src', if 'src' is not 0-terminated,
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

	if (chars <= i) {
		/* loop stopped b/c of lenght -> src is not 0-term'd */
		dst[i - 1] = 0;
		return i;
	}
	return i + 1;
}

int wmemncasecmp(const wchar_t *a, const wchar_t *b, size_t len)
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

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
