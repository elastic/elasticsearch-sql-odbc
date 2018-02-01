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
		SQLTCHAR *text, SQLINTEGER code)
{
	int ret;
	
	dest->state = state;
	dest->native_code = code;

	// TODO: this won't make sense for ANSI
	// wcsncpy() & co will be faster
	ret = swprintf(dest->text, sizeof(dest->text)/sizeof(dest->text[0]), 
			WPFWP_DESC WPFWP_DESC, MK_TSTR(ESODBC_DIAG_PREFIX),
			/* if no text specified, use the default */
			text ? text : esodbc_errors[state].message);
	if (ret < 0) {
		/* chances are this would fail too. */
		ERRN("failed to copy diagnostic messages (`"LTPD"`, `"LTPD"`).",
				MK_TSTR(ESODBC_DIAG_PREFIX), text);
		dest->text_len = 0;
	} else {
		dest->text_len = (SQLUSMALLINT)ret;
		DBG("diagnostic message: `"LTPD"` (%d).", dest->text, 
				dest->text_len);
	}

	/* return code associated with state, even if setting the diagnostic log
	 * might have failed. */
	RET_STATE(state);

}

SQLRETURN post_row_diagnostic(esodbc_diag_st *dest, esodbc_state_et state, 
		SQLTCHAR *text, SQLINTEGER code, SQLLEN nrow, SQLINTEGER ncol)
{
	dest->row_number = nrow;
	dest->column_number = ncol;
	return post_diagnostic(dest, state, text, code);
}
