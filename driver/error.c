/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */
#include <stdio.h>

#include "error.h"
#include "log.h"
#include "handles.h"

void init_diagnostic(esodbc_diag_st *dest)
{
	dest->state = SQL_STATE_00000;
	dest->text[0] = '\0';
	dest->text_len = 0;
	dest->native_code = 0;
	dest->row_number = SQL_NO_ROW_NUMBER;
	dest->column_number = SQL_NO_COLUMN_NUMBER;
}

SQLRETURN post_diagnostic(SQLHANDLE hnd, esodbc_state_et state,
	const SQLWCHAR *text, SQLINTEGER code)
{
	size_t pos, tcnt, ebufsz;
	esodbc_diag_st *dest = &HDRH(hnd)->diag;

	ebufsz = sizeof(dest->text)/sizeof(dest->text[0]);

	/* if no text specified, use the default */
	if (! text) {
		text = esodbc_errors[state].message;
	}
	tcnt = wcslen(text);

	dest->state = state;
	dest->native_code = code;

	pos = sizeof(ESODBC_DIAG_PREFIX) - 1;
	assert(pos < ebufsz);
	wcsncpy(dest->text, MK_WPTR(ESODBC_DIAG_PREFIX), pos);

	if (ebufsz <= pos + tcnt) {
		wcsncpy(dest->text + pos, text, ebufsz - (pos + 1));
		dest->text[ebufsz - 1] = L'\0';
		assert(1 < ebufsz && ebufsz < USHRT_MAX);
		dest->text_len = (SQLUSMALLINT)ebufsz - 1;
	} else {
		wcsncpy(dest->text + pos, text, tcnt + /* 0-term */1);
		dest->text_len = (SQLUSMALLINT)(pos + tcnt);
	}
	DBGH(hnd, "diagnostic message: `" LWPD "` [%d], native code: %d.",
		dest->text, dest->text_len, dest->native_code);

	RET_STATE(state);
}

SQLRETURN post_c_diagnostic(SQLHANDLE hnd, esodbc_state_et state,
	const SQLCHAR *text, SQLINTEGER code)
{
	SQLWCHAR wtext[SQL_MAX_MESSAGE_LENGTH], *ptr;
	if (text) {
		if (ascii_c2w(text, wtext, sizeof(wtext)/sizeof(*wtext)) < 0) {
			ERR("failed to convert diagnostic message `%s`.", text);
			wtext[0] = L'\0';
		}
		ptr = wtext;
	} else {
		ptr = NULL;
	}
	return post_diagnostic(hnd, state, ptr, code);
}


SQLRETURN post_row_diagnostic(SQLHANDLE hnd, esodbc_state_et state,
	SQLWCHAR *text, SQLINTEGER code, SQLLEN nrow, SQLINTEGER ncol)
{
	esodbc_diag_st *dest = &HDRH(hnd)->diag;
	dest->row_number = nrow;
	dest->column_number = ncol;
	return post_diagnostic(hnd, state, text, code);
}
