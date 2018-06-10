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
	dest->row_number = SQL_NO_ROW_NUMBER;
	dest->column_number = SQL_NO_COLUMN_NUMBER;
}

/* TODO: must the diagnostic be "cleared" after a succesful invokation?? */
SQLRETURN post_diagnostic(esodbc_diag_st *dest, esodbc_state_et state,
	SQLWCHAR *text, SQLINTEGER code)
{
	size_t pos, tcnt, ebufsz;

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
		dest->text[ebufsz - 1] = 0;
		dest->text_len = (int)ebufsz - 1;
	} else {
		wcsncpy(dest->text + pos, text, tcnt + /* 0-term */1);
		dest->text_len = (int)(pos + tcnt);
	}
	DBG("diagnostic message: `" LWPD "` [%d], native code: %d.",
		dest->text, dest->text_len, dest->native_code);

	RET_STATE(state);

}

SQLRETURN post_row_diagnostic(esodbc_diag_st *dest, esodbc_state_et state,
	SQLWCHAR *text, SQLINTEGER code, SQLLEN nrow, SQLINTEGER ncol)
{
	dest->row_number = nrow;
	dest->column_number = ncol;
	return post_diagnostic(dest, state, text, code);
}
