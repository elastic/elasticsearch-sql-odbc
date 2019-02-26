/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <float.h>

#include "queries.h"
#include "log.h"
#include "connect.h"
#include "info.h"
#include "convert.h"

/* key names used in Elastic/SQL REST/JSON answers */
#define JSON_ANSWER_COLUMNS		"columns"
#define JSON_ANSWER_ROWS		"rows"
#define JSON_ANSWER_CURSOR		"cursor"
#define JSON_ANSWER_STATUS		"status"
#define JSON_ANSWER_ERROR		"error"
#define JSON_ANSWER_ERR_TYPE	"type"
#define JSON_ANSWER_ERR_REASON	"reason"
#define JSON_ANSWER_ERR_RCAUSE	"root_cause"
#define JSON_ANSWER_COL_NAME	"name"
#define JSON_ANSWER_COL_TYPE	"type"


#define MSG_INV_SRV_ANS		"Invalid server answer"



void clear_resultset(esodbc_stmt_st *stmt, BOOL on_close)
{
	DBGH(stmt, "clearing result set; vrows=%zu, nrows=%zu.",
		stmt->rset.vrows, stmt->rset.nrows);
	if (stmt->rset.buff) {
		free(stmt->rset.buff);
	}
	if (stmt->rset.state) {
		UJFree(stmt->rset.state);
	}
	memset(&stmt->rset, 0, sizeof(stmt->rset));

	if (on_close) {
		DBGH(stmt, "on close, total fetched rows=%zu.", stmt->tf_rows);
		STMT_TFROWS_RESET(stmt);
	}

	/* reset SQLGetData state to detect sequence "SQLExec*(); SQLGetData();" */
	STMT_GD_RESET(stmt);
}

/* Set the descriptor fields associated with "size". This step is needed since
 * the application could read the descriptors - like .length - individually,
 * rather than through functions that make use of get_col_size() (where we
 * could just read the es_type directly). */
static void set_col_size(esodbc_rec_st *rec)
{
	assert(rec->desc->type == DESC_TYPE_IRD);

	switch (rec->meta_type) {
		case METATYPE_UNKNOWN:
			/* SYS TYPES call */
			break;
		case METATYPE_BIT:
		case METATYPE_EXACT_NUMERIC:
		case METATYPE_FLOAT_NUMERIC:
			/* ignore, the .precision field is not used in IRDs, its value is
			 * always read from es_type.column_size directly */
			break;

		/*
		 * https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/column-size : */
		case METATYPE_STRING:
		/* "The defined or maximum column size in characters of the
		 * column" */
		/* no break */
		case METATYPE_BIN:
		/* "The defined or maximum length in bytes of the column " */
		/* no break */
		case METATYPE_DATETIME:
			/* "number of characters in the character representation" */
			rec->length = rec->es_type->column_size;
			break;

		case METATYPE_INTERVAL_WSEC:
		case METATYPE_INTERVAL_WOSEC:
			rec->length = rec->es_type->display_size;
			break;

		default:
			BUGH(rec->desc, "unsupported data c-type: %d.", rec->concise_type);
	}
}

static SQLRETURN attach_columns(esodbc_stmt_st *stmt, UJObject columns)
{
	esodbc_desc_st *ird;
	esodbc_dbc_st *dbc;
	esodbc_rec_st *rec;
	SQLRETURN ret;
	SQLSMALLINT recno;
	void *iter;
	UJObject col_o, name_o, type_o;
	wstr_st col_type;
	size_t ncols, i;
	const wchar_t *keys[] = {
		MK_WPTR(JSON_ANSWER_COL_NAME),
		MK_WPTR(JSON_ANSWER_COL_TYPE)
	};
	static const wstr_st EMPTY_WSTR = WSTR_INIT("");

	ird = stmt->ird;
	dbc = stmt->hdr.dbc;

	ncols = UJLengthArray(columns);
	DBGH(stmt, "columns received: %zd.", ncols);
	ret = update_rec_count(ird, (SQLSMALLINT)ncols);
	if (! SQL_SUCCEEDED(ret)) {
		ERRH(stmt, "failed to set IRD's record count to %d.", ncols);
		HDIAG_COPY(ird, stmt);
		return ret;
	}

	iter = UJBeginArray(columns);
	if (! iter) {
		ERRH(stmt, "failed to obtain array iterator: %s.",
			UJGetError(stmt->rset.state));
		RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
	}
	recno = 0;
	while (UJIterArray(&iter, &col_o)) {
		if (UJObjectUnpack(col_o, 2, "SS", keys, &name_o, &type_o) < 2) {
			ERRH(stmt, "failed to decode JSON column: %s.",
				UJGetError(stmt->rset.state));
			RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		}
		rec = &ird->recs[recno]; // +recno

		ASSERT_INTEGER_TYPES_EQUAL(wchar_t, SQLWCHAR);
		rec->name.str = (SQLWCHAR *)UJReadString(name_o, &rec->name.cnt);
		if (! rec->name.str) {
			rec->name = MK_WSTR("");
		}

		col_type.str = (SQLWCHAR *)UJReadString(type_o, &col_type.cnt);

		assert(! rec->es_type);
		/* lookup the DBC-cashed ES type */
		for (i = 0; i < dbc->no_types; i ++) {
			if (EQ_CASE_WSTR(&dbc->es_types[i].type_name, &col_type)) {
				rec->es_type = &dbc->es_types[i];
				break;
			}
		}
		if (rec->es_type) {
			/* copy fileds pre-calculated at DB connect time */
			rec->concise_type = rec->es_type->data_type;
			rec->type = rec->es_type->sql_data_type;
			rec->datetime_interval_code = rec->es_type->sql_datetime_sub;
			rec->meta_type = rec->es_type->meta_type;
		} else if (! dbc->no_types) {
			/* the connection doesn't have yet the types cached (this is the
			 * caching call) and don't have access to the data itself either,
			 * just the column names & type names => set unknowns.  */
			rec->concise_type = SQL_UNKNOWN_TYPE;
			rec->type = SQL_UNKNOWN_TYPE;
			rec->datetime_interval_code = 0;
			rec->meta_type = METATYPE_UNKNOWN;
		} else {
			ERRH(stmt, "type lookup failed for `" LWPDL "`.",LWSTR(&col_type));
			RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		}

		set_col_size(rec);

		/* setting the remaining of settable fields (base table etc.) requires
		 * server side changes => set them to "" */

		/* "If a base column name does not exist (as in the case of columns
		 * that are expressions), then this variable contains an empty
		 * string." */
		rec->base_column_name = EMPTY_WSTR;
		/* "If a column does not have a label, the column name is returned. If
		 * the column is unlabeled and unnamed, an empty string is ret" */
		rec->label = rec->name.cnt ? rec->name : EMPTY_WSTR;

		assert(rec->name.str && rec->label.str);
		rec->unnamed = (rec->name.cnt || rec->label.cnt) ?
			SQL_NAMED : SQL_UNNAMED;

		/* All rec fields must be init'ed to a valid string in case their value
		 * is requested (and written with write_wstr()). The values would
		 * normally be provided by the data source, this is not the case here
		 * (yet), though. */
		rec->base_table_name = EMPTY_WSTR;
		rec->catalog_name = EMPTY_WSTR;
		rec->schema_name = EMPTY_WSTR;
		rec->table_name = EMPTY_WSTR;
#ifndef NDEBUG
		//dump_record(rec);
#endif /* NDEBUG */

		DBGH(stmt, "column #%d: name=`" LWPDL "`, type=%d (`" LWPDL "`).",
			recno, LWSTR(&rec->name), rec->concise_type, LWSTR(&col_type));
		recno ++;
	}

	/* new columns attached, need to check compatiblity */
	stmt->sql2c_conversion = CONVERSION_UNCHECKED;

	return SQL_SUCCESS;
}


/*
 * Processes a received answer:
 * - takes a dynamic buffer, buff, of length blen. Will handle the buff memory
 * even if the call fails.
 * - parses it, preparing iterators for SQLFetch()'ing.
 */
SQLRETURN TEST_API attach_answer(esodbc_stmt_st *stmt, char *buff, size_t blen)
{
	int unpacked;
	UJObject obj, columns, rows, cursor;
	const wchar_t *wcurs;
	size_t eccnt;
	const wchar_t *keys[] = {
		MK_WPTR(JSON_ANSWER_COLUMNS),
		MK_WPTR(JSON_ANSWER_ROWS),
		MK_WPTR(JSON_ANSWER_CURSOR)
	};

	/* clear any previous result set */
	if (STMT_HAS_RESULTSET(stmt)) {
		clear_resultset(stmt, /*on_close*/FALSE);
	}

	/* the statement takes ownership of mem obj */
	stmt->rset.buff = buff;
	stmt->rset.blen = blen;
	DBGH(stmt, "attaching answer [%zd]`" LCPDL "`.", blen, blen, buff);

	/* parse the entire JSON answer */
	obj = UJDecode(buff, blen, NULL, &stmt->rset.state);
	if (! obj) {
		ERRH(stmt, "failed to decode JSON answer: %s ([%zu] `%.*s`).",
			stmt->rset.state ? UJGetError(stmt->rset.state) : "<none>",
			blen, blen, buff);
		RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
	}
	columns = rows = cursor = NULL;
	/* extract the columns and rows objects */
	unpacked = UJObjectUnpack(obj, 3, "AAS", keys, &columns, &rows, &cursor);
	if (unpacked < /* 'rows' must always be present */1) {
		ERRH(stmt, "failed to unpack JSON answer (`%.*s`): %s.",
			blen, buff, UJGetError(stmt->rset.state));
		assert(0);
		RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
	}

	/*
	 * set the internal cursor (UJSON4C array iterator)
	 */
	if (! rows) {
		ERRH(stmt, "no rows JSON object received in answer: `%.*s`[%zd].",
			blen, buff, blen);
		RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
	}
	stmt->rset.rows_iter = UJBeginArray(rows);
	if (! stmt->rset.rows_iter) {
#if 0 /* UJSON4C will return NULL above, for empty array (meh!) */
		ERRH(stmt, "failed to get iterrator on received rows: %s.",
			UJGetError(stmt->rset.state));
		RET_HDIAGS(stmt, SQL_STATE_HY000);
#else /*0*/
		DBGH(stmt, "received empty resultset array: forcing nodata.");
		STMT_FORCE_NODATA(stmt);
		stmt->rset.nrows = 0;
#endif /*0*/
	} else {
		/* the cast is made safe by the decoding format indicator for array  */
		stmt->rset.nrows = (size_t)UJLengthArray(rows);
		stmt->tf_rows += stmt->rset.nrows;
	}
	DBGH(stmt, "rows received in result set: %zd.", stmt->rset.nrows);

	/*
	 * copy Elastic's cursor (if there's one)
	 */
	if (cursor) {
		wcurs = UJReadString(cursor, &eccnt);
		if (eccnt) {
			/* this can happen automatically if hitting scroller size */
			if (! stmt->hdr.dbc->fetch.max) {
				INFOH(stmt, "no fetch size defined, but cursor returned.");
			}
			if (stmt->rset.ecurs.cnt) {
				DBGH(stmt, "replacing old cursor `" LWPDL "`.",
					LWSTR(&stmt->rset.ecurs));
			}
			/* store new cursor vals */
			stmt->rset.ecurs = (wstr_st) {
				(SQLWCHAR *)wcurs, eccnt
			};
			DBGH(stmt, "new elastic cursor: `" LWPDL "`[%zd].",
				LWSTR(&stmt->rset.ecurs), stmt->rset.ecurs.cnt);
		} else {
			WARNH(stmt, "empty cursor found in the answer.");
		}
	} else {
		/* should have been cleared by now */
		assert(! stmt->rset.ecurs.cnt);
	}

	/*
	 * process the sent columns, if any.
	 */
	if (columns) {
		if (0 < stmt->ird->count) {
			ERRH(stmt, "%d columns already attached.", stmt->ird->count);
			RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		}
		return attach_columns(stmt, columns);
	} else {
		/* no cols available in this answer: check if already received */
		if (stmt->ird->count <= 0) {
			ERRH(stmt, "no columns available in result set; answer: "
				"`%.*s`[%zd].", blen, buff, blen);
			RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		}
	}

	return SQL_SUCCESS;
}

/* parse the error as SQL pluggin generated error */
static BOOL attach_sql_error(SQLHANDLE hnd, cstr_st *body)
{
	BOOL ret;
	UJObject obj, o_status, o_error, o_type, o_reason, o_rcause;
	wstr_st type, reason;
	wchar_t wbuf[SQL_MAX_MESSAGE_LENGTH];
	int cnt;
	void *state, *iter;
	/* following grouped JSON unpacking items must remain in sync */
	/* {"error": {..}, "status":200} */
	const wchar_t *outer_keys[] = {
		MK_WPTR(JSON_ANSWER_ERROR),
		MK_WPTR(JSON_ANSWER_STATUS)
	};
	const char fmt_outer_keys[] = "ON";
	int cnt_outer_keys = sizeof(fmt_outer_keys) - /*\0*/1;
	/* "error": {"root_cause":[?], "type":"..", "reason":".." ...} */
	const wchar_t *err_keys[] = {
		MK_WPTR(JSON_ANSWER_ERR_RCAUSE),
		MK_WPTR(JSON_ANSWER_ERR_TYPE),
		MK_WPTR(JSON_ANSWER_ERR_REASON),
	};
	const char fmt_err_keys[] = "aSS";
	int cnt_err_keys = sizeof(fmt_err_keys) - /*\0*/1;
	/* "root_cause":[{"type":"..", "reason":".."} ..] */
	const wchar_t *r_err_keys[] = {
		MK_WPTR(JSON_ANSWER_ERR_TYPE),
		MK_WPTR(JSON_ANSWER_ERR_REASON),
	};
	const char fmt_r_err_keys[] = "SS";
	int cnt_r_err_keys = sizeof(fmt_r_err_keys) - /*\0*/1;

	ret = FALSE;
	state = NULL;

	/* parse the entire JSON answer */
	obj = UJDecode(body->str, body->cnt, NULL, &state);
	if (! obj) {
		ERRH(hnd, "answer not JSON (%s).",
			state ? UJGetError(state) : "<none>");
		goto end;
	}
	/* extract the status and error object */
	if (UJObjectUnpack(obj, cnt_outer_keys, fmt_outer_keys, outer_keys,
			&o_error, &o_status) < cnt_outer_keys) {
		ERRH(hnd, "JSON answer not a SQL error (%s).", UJGetError(state));
		goto end;
	}
	/* unpack error object */
	if (UJObjectUnpack(o_error, cnt_err_keys, fmt_err_keys, err_keys,
			&o_rcause, &o_type, &o_reason) < cnt_err_keys) {
		ERRH(hnd, "failed to unpack error object (%s).", UJGetError(state));
		goto end;
	}

	/* this is safe for NULL o_rcause: => -1 */
	cnt = UJLengthArray(o_rcause);
	DBGH(hnd, "root cause(s) received: %d.", cnt);
	if (0 < cnt) {
		/* print the root_cause, if available */
		iter = UJBeginArray(o_rcause);
		/* save, UJIterArray() checks against NULL */
		assert(iter);
		while (UJIterArray(&iter, &o_rcause)) { /* reuse o_rcause obj */
			/* unpack root error object */
			if (UJObjectUnpack(o_rcause, cnt_r_err_keys, fmt_r_err_keys,
					r_err_keys, &o_type, &o_reason) < cnt_r_err_keys) {
				ERRH(hnd, "failed to unpack root error object (%s).",
					UJGetError(state));
				goto end; /* TODO: continue on error? */
			} else {
				/* stop at first element. TODO: is ever [array] > 1? */
				break;
			}
		}
	}
	/* else: root_cause not available, print "generic" reason */
	type.str = (SQLWCHAR *)UJReadString(o_type, &type.cnt);
	reason.str = (SQLWCHAR *)UJReadString(o_reason, &reason.cnt);

	/* should be empty string in case of mismatch */
	assert(type.str && reason.str);
	DBGH(hnd, "reported failure: type: [%zd] `" LWPDL "`, reason: [%zd] `"
		LWPDL "`, status: %d.", type.cnt, LWSTR(&type),
		reason.cnt, LWSTR(&reason), UJNumericInt(o_status));

	/* swprintf will always append the 0-term ("A null character is appended
	 * after the last character written."), but fail if formated string would
	 * overrun the buffer size (in an equivocal way: overrun <?> encoding
	 * error). */
	errno = 0;
	cnt = swprintf(wbuf, sizeof(wbuf)/sizeof(*wbuf),
			WPFWP_LDESC L": " WPFWP_LDESC, LWSTR(&type), LWSTR(&reason));
	assert(cnt);
	if (cnt < 0) {
		if (errno) {
			ERRH(hnd, "printing the received error message failed.");
			goto end;
		}
		/* partial error message printed */
		WARNH(hnd, "current error buffer to small (%zu) for full error "
			"detail.", sizeof(wbuf)/sizeof(*wbuf));
		cnt = sizeof(wbuf)/sizeof(*wbuf) - 1;
	}
	assert(wbuf[cnt] == L'\0');
	ERRH(hnd, "request failure reason: [%d] `" LWPD "`.", cnt, wbuf);

	post_diagnostic(hnd, SQL_STATE_HY000, wbuf, UJNumericInt(o_status));
	ret = TRUE;

end:
	if (state) {
		UJFree(state);
	}

	return ret;
}

/*
 * Parse an error and push it as statement diagnostic.
 */
SQLRETURN TEST_API attach_error(SQLHANDLE hnd, cstr_st *body, int code)
{
	char buff[SQL_MAX_MESSAGE_LENGTH];
	size_t to_copy;

	ERRH(hnd, "request failure %d body: len: %zu, content: `%.*s`.", code,
		body->cnt, LCSTR(body));

	if (body->cnt) {
		/* try read it as ES/SQL error */
		if (! attach_sql_error(hnd, body)) {
			/* if not an ES/SQL failure, attach it as-is (plus \0) */
			to_copy = sizeof(buff) <= body->cnt ? sizeof(buff) - 1 : body->cnt;
			memcpy(buff, body->str, to_copy);
			buff[to_copy] = '\0';

			post_c_diagnostic(hnd, SQL_STATE_08S01, buff, code);
		}

		RET_STATE(HDRH(hnd)->diag.state);
	}

	return post_diagnostic(hnd, SQL_STATE_08S01, NULL, code);
}

/*
 * Attach an SQL query to the statment: malloc, convert, copy.
 */
SQLRETURN TEST_API attach_sql(esodbc_stmt_st *stmt,
	const SQLWCHAR *sql, /* SQL text statement */
	size_t sqlcnt /* count of chars of 'sql' */)
{
	wstr_st sqlw = (wstr_st) {
		(SQLWCHAR *)sql, sqlcnt
	};

	DBGH(stmt, "attaching SQL [%zd] `" LWPDL "`.", sqlcnt, LWSTR(&sqlw));

	assert(! stmt->u8sql.str);
	if (! wstr_to_utf8(&sqlw, &stmt->u8sql)) {
		ERRNH(stmt, "conversion UCS2->UTF8 of SQL [%zu] `" LWPDL "` failed.",
			sqlcnt, LWSTR(&sqlw));
		RET_HDIAG(stmt, SQL_STATE_HY000, "UCS2/UTF8 conversion failure", 0);
	}

	/* if the app correctly SQL_CLOSE'es the statement, this would not be
	 * needed. but just in case: re-init counter of total # of rows */
	STMT_TFROWS_RESET(stmt);

	return SQL_SUCCESS;
}

/*
 * Detach the existing query (if any) from the statement.
 */
void detach_sql(esodbc_stmt_st *stmt)
{
	if (! stmt->u8sql.str) {
		return;
	}
	free(stmt->u8sql.str);
	stmt->u8sql.str = NULL;
	stmt->u8sql.cnt = 0;
}


/*
 * "An application can unbind the data buffer for a column but still have a
 * length/indicator buffer bound for the column, if the TargetValuePtr
 * argument in the call to SQLBindCol is a null pointer but the
 * StrLen_or_IndPtr argument is a valid value."
 *
 * "When the driver returns fixed-length data, such as an integer or a date
 * structure, the driver ignores BufferLength and assumes the buffer is large
 * enough to hold the data." BUT:
 * "This is an error if the data returned by the driver is NULL but is common
 * when retrieving fixed-length, non-nullable data, because neither a length
 * nor an indicator value is needed."
 *
 * "The binding remains in effect until it is replaced by a new binding, the
 * column is unbound, or the statement is freed."
 *
 * "If ColumnNumber refers to an unbound column, SQLBindCol still returns
 * SQL_SUCCESS."
 *
 * "Call SQLBindCol to specify a new binding for a column that is already
 * bound. The driver overwrites the old binding with the new one."
 *
 * "Binding Offsets: the same offset is added to each address in each binding"
 *
 * "https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/buffers":
 * SQL_LEN_BINARY_ATTR, SQL_NTS, SQL_IS_POINTER/_INTEGER/etc.
 *
 * " The application sets the SQL_BIND_BY_COLUMN statement attribute to
 * specify whether it is using column-wise or row-wise binding"
 */
SQLRETURN EsSQLBindCol(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT ColumnNumber,
	SQLSMALLINT TargetType, /* identifier of the C data type */
	_Inout_updates_opt_(_Inexpressible_(BufferLength))
	SQLPOINTER TargetValue,
	SQLLEN BufferLength,
	_Inout_opt_ SQLLEN *StrLen_or_Ind)
{
	SQLRETURN ret;
	esodbc_stmt_st *stmt = STMH(StatementHandle);
	esodbc_desc_st *ard = stmt->ard;
	SQLSMALLINT ard_prev_count;

	if (BufferLength < 0) {
		ERRH(stmt, "invalid negative BufferLength: %d.", BufferLength);
		RET_HDIAGS(STMH(StatementHandle), SQL_STATE_HY090);
	}

	if (! ColumnNumber) {
		/* "The statement attribute SQL_ATTR_USE_BOOKMARKS should always be
		 * set before binding a column to column 0. This is not required but
		 * is strongly recommended." */
		assert(stmt->bookmarks == SQL_UB_OFF); // TODO: bookmarks
		ERRH(stmt, "bookmarks use turned off.");
		RET_HDIAGS(stmt, SQL_STATE_07009);
	}

	ard_prev_count = ard->count;
	/* "if the value in the ColumnNumber argument exceeds the value of
	 * SQL_DESC_COUNT, calls SQLSetDescField to increase the value of
	 * SQL_DESC_COUNT to ColumnNumber." */
	if (ard_prev_count < ColumnNumber) {
		ret = EsSQLSetDescFieldW(ard, NO_REC_NR, SQL_DESC_COUNT,
				(SQLPOINTER)(uintptr_t)ColumnNumber, SQL_IS_SMALLINT);
		if (SQL_SUCCEEDED(ret)) {
			/* value set to negative if count needs to be restored to it */
			ard_prev_count = -ard_prev_count;
		} else {
			goto copy_ret;
		}
	}

	/* set types (or verbose for datetime/interval types) */
	ret = EsSQLSetDescFieldW(ard, ColumnNumber, SQL_DESC_CONCISE_TYPE,
			(SQLPOINTER)(intptr_t)TargetType, SQL_IS_SMALLINT);
	if (ret != SQL_SUCCESS) {
		goto copy_ret;
	}

	// TODO: § "Cautions Regarding SQL_DEFAULT"

	/* "Sets the SQL_DESC_OCTET_LENGTH field to the value of BufferLength." */
	ret = EsSQLSetDescFieldW(ard, ColumnNumber, SQL_DESC_OCTET_LENGTH,
			(SQLPOINTER)(intptr_t)BufferLength, SQL_IS_INTEGER);
	if (ret != SQL_SUCCESS) {
		goto copy_ret;
	}

	/* Sets the SQL_DESC_INDICATOR_PTR field to the value of StrLen_or_Ind" */
	ret = EsSQLSetDescFieldW(ard, ColumnNumber, SQL_DESC_INDICATOR_PTR,
			StrLen_or_Ind,
			SQL_LEN_BINARY_ATTR((SQLINTEGER)sizeof(StrLen_or_Ind)));
	if (ret != SQL_SUCCESS) {
		goto copy_ret;
	}

	/* "Sets the SQL_DESC_OCTET_LENGTH_PTR field to the value of
	 * StrLen_or_Ind." */
	ret = EsSQLSetDescFieldW(ard, ColumnNumber, SQL_DESC_OCTET_LENGTH_PTR,
			StrLen_or_Ind,
			SQL_LEN_BINARY_ATTR((SQLINTEGER)sizeof(StrLen_or_Ind)));
	if (ret != SQL_SUCCESS) {
		goto copy_ret;
	}

	/* "Sets the SQL_DESC_DATA_PTR field to the value of TargetValue."
	 * Note: needs to be last set field, as setting other fields unbinds. */
	ret = EsSQLSetDescFieldW(ard, ColumnNumber, SQL_DESC_DATA_PTR,
			TargetValue, SQL_IS_POINTER);
	if (ret != SQL_SUCCESS) {
		goto copy_ret;
	}

	/* every binding resets conversion flag */
	stmt->sql2c_conversion = CONVERSION_UNCHECKED;

	DBGH(stmt, "succesfully bound column #%hu of type %hd, "
		"buffer@0x%p of length: %lld, LenInd@0x%p", ColumnNumber, TargetType,
		TargetValue, BufferLength, StrLen_or_Ind);

	return SQL_SUCCESS;

copy_ret:
	/* copy error at top handle level, where it's going to be inquired from */
	HDIAG_COPY(ard, stmt);

	ERRH(stmt, "binding parameter failed -- resetting ARD count");
	if (ard_prev_count < 0) {
		ret = EsSQLSetDescFieldW(ard, NO_REC_NR, SQL_DESC_COUNT,
				(SQLPOINTER)(uintptr_t)-ard_prev_count, SQL_IS_SMALLINT);
		if (! SQL_SUCCEEDED(ret)) {
			ERRH(stmt, "failed to reset ARD count back to %hd - descriptor "
				"might be left in inconsistent state!", -ard_prev_count);
		}
	}
	RET_STATE(stmt->hdr.diag.state);
}

/*
 * Copy one row from IRD to ARD.
 * pos: row number in the rowset
 * Returns: ...
 */
SQLRETURN copy_one_row(esodbc_stmt_st *stmt, SQLULEN pos)
{
	SQLSMALLINT i;
	SQLLEN rowno;
	SQLRETURN ret;
	UJObject obj;
	void *iter_row;
	SQLLEN *ind_len;
	long long ll;
	double dbl;
	const wchar_t *wstr;
	BOOL boolval;
	size_t len;
	BOOL with_info;
	esodbc_desc_st *ard, *ird;
	esodbc_rec_st *arec, *irec;

	rowno = (SQLLEN)STMT_CRR_ROW_NUMBER(stmt);
	ard = stmt->ard;
	ird = stmt->ird;

#define RET_ROW_DIAG(_state, _message, _colno) \
	do { \
		if (ird->array_status_ptr) { \
			ird->array_status_ptr[pos] = SQL_ROW_ERROR; \
		} \
		return post_row_diagnostic(stmt, _state, MK_WPTR(_message), \
				0, rowno, _colno); \
	} while (0)
#define SET_ROW_DIAG(_rowno, _colno) \
	do { \
		stmt->hdr.diag.row_number = _rowno; \
		stmt->hdr.diag.column_number = _colno; \
	} while (0)

	/* is current object an array? */
	if (! UJIsArray(stmt->rset.row_array)) {
		ERRH(stmt, "one '%s' element (#%zd) in result set not an array; type:"
			" %d.", JSON_ANSWER_ROWS, stmt->rset.vrows,
			UJGetType(stmt->rset.row_array));
		RET_ROW_DIAG(SQL_STATE_HY000, MSG_INV_SRV_ANS, SQL_NO_COLUMN_NUMBER);
	}
	/* are there elements in this row array to at least match the number of
	 * columns? */
	if (UJLengthArray(stmt->rset.row_array) < ird->count) {
		ERRH(stmt, "current row counts less elements (%d) than columns (%hd)",
			UJLengthArray(stmt->rset.row_array), ird->count);
		RET_ROW_DIAG(SQL_STATE_HY000, MSG_INV_SRV_ANS, SQL_NO_COLUMN_NUMBER);
	} else if (ird->count < UJLengthArray(stmt->rset.row_array)) {
		WARNH(stmt, "current row counts more elements (%d) than columns (%hd)",
			UJLengthArray(stmt->rset.row_array), ird->count);
	}
	/* get an iterator over the row array */
	if (! (iter_row = UJBeginArray(stmt->rset.row_array))) {
		ERRH(stmt, "Failed to obtain iterator on row (#%zd): %s.", rowno,
			UJGetError(stmt->rset.state));
		RET_ROW_DIAG(SQL_STATE_HY000, MSG_INV_SRV_ANS, SQL_NO_COLUMN_NUMBER);
	}

	with_info = FALSE;
	/* iterate over the bound cols and contents of one (table) row */
	for (i = 0; i < ard->count && UJIterArray(&iter_row, &obj); i ++) {
		arec = &ard->recs[i]; /* access safe if 'i < ard->count' */
		/* if record not bound skip it */
		if (! REC_IS_BOUND(arec)) {
			DBGH(stmt, "column #%d not bound, skipping it.", i + 1);
			continue;
		}

		/* access made safe by ird->count match against array len above  */
		irec = &ird->recs[i];

		switch (UJGetType(obj)) {
			default:
				ERRH(stmt, "unexpected object of type %d in row L#%zu/T#%zd.",
					UJGetType(obj), stmt->rset.vrows, rowno);
				RET_ROW_DIAG(SQL_STATE_HY000, MSG_INV_SRV_ANS, i + 1);
			/* RET_.. returns */

			case UJT_Null:
				DBGH(stmt, "value [%zd, %d] is NULL.", rowno, i + 1);
				/* Note: if ever causing an issue, check
				 * arec->es_type->nullable before returning NULL to app */
				ind_len = deferred_address(SQL_DESC_INDICATOR_PTR, pos, arec);
				if (! ind_len) {
					ERRH(stmt, "no buffer to signal NULL value.");
					RET_ROW_DIAG(SQL_STATE_22002, "Indicator variable required"
						" but not supplied", i + 1);
				}
				*ind_len = SQL_NULL_DATA;
				continue; /* instead of break! no 'ret' processing to do. */

			case UJT_String:
				wstr = UJReadString(obj, &len);
				DBGH(stmt, "value [%zd, %d] is string [%d]:`" LWPDL "`.",
					rowno, i + 1, len, len, wstr);
				/* UJSON4C returns chars count, but 0-terminates w/o counting
				 * the terminator */
				assert(wstr[len] == 0);
				/* "When character data is returned from the driver to the
				 * application, the driver must always null-terminate it." */
				ret = sql2c_string(arec, irec, pos, wstr, len + /*\0*/1);
				break;

			case UJT_Long:
			case UJT_LongLong:
				ll = UJNumericLongLong(obj);
				DBGH(stmt, "value [%zd, %d] is integer: %lld.", rowno, i + 1,
					ll);
				ret = sql2c_longlong(arec, irec, pos, ll);
				break;

			case UJT_Double:
				dbl = UJNumericFloat(obj);
				DBGH(stmt, "value [%zd, %d] is double: %f.", rowno, i + 1,
					dbl);
				ret = sql2c_double(arec, irec, pos, dbl);
				break;

			case UJT_True:
			case UJT_False:
				boolval = UJGetType(obj) == UJT_True ? TRUE : FALSE;
				DBGH(stmt, "value [%zd, %d] is boolean: %d.", rowno, i + 1,
					boolval);
				/* "When bit SQL data is converted to character C data, the
				 * possible values are "0" and "1"." */
				ret = sql2c_longlong(arec, irec, pos, boolval ? 1LL : 0LL);
				break;
		}

		switch (ret) {
			case SQL_SUCCESS_WITH_INFO:
				with_info = TRUE;
				SET_ROW_DIAG(rowno, i + 1);
			case SQL_SUCCESS:
				break;
			default: /* error */
				SET_ROW_DIAG(rowno, i + 1);
				return ret;
		}
	}

	if (ird->array_status_ptr) {
		ird->array_status_ptr[pos] = with_info ? SQL_ROW_SUCCESS_WITH_INFO :
			SQL_ROW_SUCCESS;
		DBGH(stmt, "status array @0x%p#%d set to %hu.", ird->array_status_ptr,
			pos, ird->array_status_ptr[pos]);
	}

	return with_info ? SQL_SUCCESS_WITH_INFO : SQL_SUCCESS;

#undef RET_ROW_DIAG
#undef SET_ROW_DIAG
}

/*
 * "SQLFetch and SQLFetchScroll use the rowset size at the time of the call to
 * determine how many rows to fetch."
 *
 * "If SQLFetch or SQLFetchScroll encounters an error while retrieving one row
 * of a multirow rowset, or if SQLBulkOperations with an Operation argument of
 * SQL_FETCH_BY_BOOKMARK encounters an error while performing a bulk fetch, it
 * sets the corresponding value in the row status array to SQL_ROW_ERROR,
 * continues fetching rows, and returns SQL_SUCCESS_WITH_INFO."
 *
 * "SQLFetch can be used only for multirow fetches when called in ODBC 3.x; if
 * an ODBC 2.x application calls SQLFetch, it will open only a single-row,
 * forward-only cursor."
 *
 * "The application can change the rowset size and bind new rowset buffers (by
 * calling SQLBindCol or specifying a bind offset) even after rows have been
 * fetched."
 *
 * "SQLFetch returns bookmarks if column 0 is bound." Otherwise, "return more
 * than one row" (if avail).
 *
 * "The driver does not return SQLSTATE 01S01 (Error in row) to indicate that
 * an error has occurred while rows were fetched by a call to SQLFetch." (same
 * for SQLFetchScroll).
 *
 * "SQL_ROW_NOROW: The rowset overlapped the end of the result set, and no row
 * was returned that corresponded to this element of the row status array."
 *
 * "If the bound address is 0, no data value is returned" (also for row/column
 * binding)
 *
 * "In the IRD, this header field points to a row status array containing
 * status values after a call to SQLBulkOperations, SQLFetch, SQLFetchScroll,
 * or SQLSetPos."  = row status array of IRD (.array_status_ptr); can be NULL.
 *
 * "The binding offset is always added directly to the values in the
 * SQL_DESC_DATA_PTR, SQL_DESC_INDICATOR_PTR, and SQL_DESC_OCTET_LENGTH_PTR
 * fields." (.bind_offset.ptr)
 *
 * "In ARDs, this field specifies the binding orientation when SQLFetchScroll
 * or SQLFetch is called on the associated statement handle." (.bind_type)
 *
 * "In an IRD, this SQLULEN * header field points to a buffer containing the
 * number of rows fetched after a call to SQLFetch or SQLFetchScroll, or the
 * number of rows affected in a bulk operation performed by a call to
 * SQLBulkOperations or SQLSetPos, including error rows."
 * (.rows_processed_ptr)
 *
 * "The variable that the StrLen_or_Ind argument refers to is used for both
 * indicator and length information. If a fetch encounters a null value for
 * the column, it stores SQL_NULL_DATA in this variable; otherwise, it stores
 * the data length in this variable. Passing a null pointer as StrLen_or_Ind
 * keeps the fetch operation from returning the data length but makes the
 * fetch fail if it encounters a null value and has no way to return
 * SQL_NULL_DATA." (.indicator_ptr)
 */
SQLRETURN EsSQLFetch(SQLHSTMT StatementHandle)
{
	esodbc_stmt_st *stmt;
	esodbc_desc_st *ard, *ird;
	SQLULEN i, j, errors;
	SQLRETURN ret;

	stmt = STMH(StatementHandle);
	ard = stmt->ard;
	ird = stmt->ird;

	if (! STMT_HAS_RESULTSET(stmt)) {
		if (STMT_NODATA_FORCED(stmt)) {
			DBGH(stmt, "empty result set flag set - returning no data.");
			return SQL_NO_DATA;
		}
		ERRH(stmt, "no resultset available on statement.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	/* Check if the data [type] stored in DB is compatiblie with the buffer
	 * [type] the application provides. This test can only be done at
	 * fetch-time, since the application can unbind/rebind columns at any time
	 * (i.e. also in-between consecutive fetches). */
	switch (stmt->sql2c_conversion) {
		case CONVERSION_VIOLATION:
			ERRH(stmt, "types compibility check had failed already "
				"(violation).");
			RET_HDIAGS(stmt, SQL_STATE_07006);
		/* RET_ returns */

		case CONVERSION_UNSUPPORTED:
			ERRH(stmt, "types compibility check had failed already "
				"(unsupported).");
			RET_HDIAG(stmt, SQL_STATE_HYC00, "Conversion target type not"
				" supported", 0);
		/* RET_ returns */

		case CONVERSION_SKIPPED:
			DBGH(stmt, "types compatibility skipped.");
			/* check unnecessary (SYS TYPES, possiblity other metas) */
			break;

		case CONVERSION_UNCHECKED:
			ret = convertability_check(stmt, /*col bind check*/-1,
					(int *)&stmt->sql2c_conversion);
			if (! SQL_SUCCEEDED(ret)) {
				return ret;
			}
		/* no break; */

		default:
			DBGH(stmt, "ES/app data/buffer types found compatible.");
	}

	DBGH(stmt, "(`" LCPDL "`); cursor @ %zd / %zd.", LCSTR(&stmt->u8sql),
		stmt->rset.vrows, stmt->rset.nrows);

	/* reset SQLGetData state, to reset fetch position */
	STMT_GD_RESET(stmt);

	DBGH(stmt, "rowset max size: %zu.", ard->array_size);
	errors = 0;
	i = 0;
	/* for all rows in rowset/array, iterate over rows in current resultset */
	while (i < ard->array_size) {
		if (! UJIterArray(&stmt->rset.rows_iter, &stmt->rset.row_array)) {
			DBGH(stmt, "ran out of rows in current result set: nrows=%zd, "
				"vrows=%zd.", stmt->rset.nrows, stmt->rset.vrows);
			if (stmt->rset.ecurs.cnt) { /* is there an Elastic cursor? */
				ret = EsSQLExecute(stmt);
				if (! SQL_SUCCEEDED(ret)) {
					ERRH(stmt, "failed to fetch next results.");
					return ret;
				} else {
					assert(STMT_HAS_RESULTSET(stmt));
				}
				if (! STMT_NODATA_FORCED(stmt)) {
					/* resume copying from the new resultset, staying on the
					 * same position in rowset. */
					continue;
				}
			}

			DBGH(stmt, "reached end of entire result set. fetched=%zd.",
				stmt->tf_rows);
			/* indicate the non-processed rows in rowset */
			if (ird->array_status_ptr) {
				DBGH(stmt, "setting rest of %zu rows in status array to "
					"'no row' (%hu).", ard->array_size - i, SQL_ROW_NOROW);
				for (j = i; j < ard->array_size; j ++) {
					ird->array_status_ptr[j] = SQL_ROW_NOROW;
				}
			}

			/* stop the copying loop */
			break;
		}
		ret = copy_one_row(stmt, i);
		if (! SQL_SUCCEEDED(ret)) {
			ERRH(stmt, "copying row %zu failed.", stmt->rset.vrows + 1);
			errors ++;
		}
		i ++;
		/* account for processed rows */
		stmt->rset.vrows ++;
	}

	/* return number of processed rows (even if 0) */
	if (ird->rows_processed_ptr) {
		DBGH(stmt, "setting number of processed rows to: %llu.", i);
		*ird->rows_processed_ptr = i;
	}

	/* no data has been copied out */
	if (i <= 0) {
		/* ard->array_size is in interval [1, ESODBC_MAX_ROW_ARRAY_SIZE] */
		DBGH(stmt, "no data %sto return.", stmt->rset.vrows ? "left ": "");
		return SQL_NO_DATA;
	}

	if (errors && i <= errors) {
		ERRH(stmt, "processing failed for all rows [%llu].", errors);
		return SQL_ERROR;
	}

	/* only failures need stmt.diag defer'ing */
	return SQL_SUCCESS;
}


/*
 * data availability, call sanity checks and init'ing
 */
static SQLRETURN gd_checks(esodbc_stmt_st *stmt, SQLUSMALLINT colno)
{
	/* is there a result set? */
	if (! STMT_HAS_RESULTSET(stmt)) {
		if (STMT_NODATA_FORCED(stmt)) {
			DBGH(stmt, "empty result flag set - returning no data.");
			return SQL_NO_DATA;
		}
		ERRH(stmt, "no resultset available on statement.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}
	/* is there a block cursor bound? */
	if (1 < stmt->ard->array_size) {
		ERRH(stmt, "can't use function with block cursor "
			"(array_size=%llu).", stmt->ard->array_size);
		RET_HDIAGS(stmt, SQL_STATE_HYC00);
	}
	/* has SQLFetch() been called? rset is reset with every new result */
	if (! stmt->rset.row_array) {
		/* DM should have detected this case */
		ERRH(stmt, "SQLFetch() hasn't yet been called on result set.");
		RET_HDIAGS(stmt, SQL_STATE_24000);
	}
	/* colno will be used as 1-based index, if not using bookmarks */
	if (colno < 1) { // TODO: add bookmark check (when implementing it)
		ERRH(stmt, "column number (%hu) can be less than 1.", colno);
		RET_HDIAGS(stmt, SQL_STATE_07009);
	}

	return SQL_SUCCESS;
}

static SQLRETURN gd_bind_col(
	esodbc_stmt_st *stmt,
	esodbc_desc_st *ard,
	SQLUSMALLINT colno,
	SQLSMALLINT target_type,
	SQLPOINTER buff_ptr,
	SQLLEN buff_len,
	SQLLEN *len_ind)
{
	SQLRETURN ret;
	SQLSMALLINT ctype;
	esodbc_rec_st *arec, *gd_arec;

	switch (target_type) {
		case SQL_ARD_TYPE:
			if (! (arec = get_record(ard, colno, /*grow?*/FALSE))) {
				ERRH(stmt, "no bound column #%hu to copy its concise type.",
					colno);
				RET_HDIAGS(stmt, SQL_STATE_07009);
			}
			ctype = arec->concise_type;
		case SQL_APD_TYPE:
			ERRH(stmt, "procedure parameters not unsupported.");
			RET_HDIAGS(stmt, SQL_STATE_HYC00);
		default:
			arec = NULL;
			ctype = target_type;
	}

	/* bind the column */
	ret = EsSQLBindCol(stmt, colno, ctype, buff_ptr, buff_len, len_ind);
	if (! SQL_SUCCEEDED(ret)) {
		return ret;
	}

	/* If target is SQL_ARD_TYPE, use "SQL_DESC_DATETIME_INTERVAL_PRECISION,
	 * SQL_DESC_PRECISION, and SQL_DESC_SCALE fields of the ARD" */
	if (arec) {
		if (! (gd_arec = get_record(stmt->ard, colno, FALSE))) {
			BUGH(stmt, "can't fetch just set record.");
			RET_HDIAG(stmt, SQL_STATE_HY000, "BUG: failed to prepare "
				"SQLGetData call", 0);
		}
		gd_arec->datetime_interval_precision =
			arec->datetime_interval_precision;
		gd_arec->precision = arec->precision;
		gd_arec->scale = arec->scale;
	}

	return SQL_SUCCESS;
}

SQLRETURN EsSQLGetData(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT ColumnNumber,
	SQLSMALLINT TargetType,
	_Out_writes_opt_(_Inexpressible_(BufferLength)) SQLPOINTER TargetValuePtr,
	SQLLEN BufferLength,
	_Out_opt_ SQLLEN *StrLen_or_IndPtr)
{
	SQLRETURN ret;
	esodbc_desc_st *ard, gd_ard;
	esodbc_rec_st arecs[ESODBC_GD_DESC_COUNT];
	esodbc_stmt_st *stmt = STMH(StatementHandle);

	if (stmt->gd_col == ColumnNumber && stmt->gd_ctype == TargetType) {
		DBGH(stmt, "resuming get on column #%hu (pos @ %lld).",
			stmt->gd_col, stmt->gd_offt);
		if (stmt->gd_offt < 0) {
			WARNH(stmt, "data for current column exhausted.");
			return SQL_NO_DATA;
		}
	} else {
		if (0 <= stmt->gd_col) {
			DBGH(stmt, "previous source column #%hu (pos @ %lld), SQL C %hd "
				"abandoned for new #%hu, SQL C %hd.", stmt->gd_col,
				stmt->gd_offt, stmt->gd_ctype, ColumnNumber, TargetType);
			/* reset fields now, should the call eventually fail */
			STMT_GD_RESET(stmt);
		} else {
			DBGH(stmt, "prep. column #%hu as the new data src.", ColumnNumber);
		}
		ret = gd_checks(stmt, ColumnNumber);
		if (! SQL_SUCCEEDED(ret)) {
			return ret;
		}

		stmt->gd_col = ColumnNumber;
		stmt->gd_ctype = TargetType;
	}
	/* data is available */

	/* save stmt's current ARD before overwriting it */
	ard = getdata_set_ard(stmt, &gd_ard, ColumnNumber, arecs,
			sizeof(arecs)/sizeof(arecs[0]));
	if (! ard) {
		BUGH(stmt, "failed to prepare GD ARD.");
		RET_HDIAG(stmt, SQL_STATE_HY000,
			"BUG: failed to prepare SQLGetData call", 0);
	}

	ret = gd_bind_col(stmt, ard, ColumnNumber, TargetType, TargetValuePtr,
			BufferLength, StrLen_or_IndPtr);
	if (! SQL_SUCCEEDED(ret)) {
		goto end;
	}

	/* check if data types are compatible/convertible */
	ret = convertability_check(stmt, /*col bind check*/-1, NULL);
	if (! SQL_SUCCEEDED(ret)) {
		goto end;
	}

	/* copy the data */
	ret = copy_one_row(stmt, 0);
	if (! SQL_SUCCEEDED(ret)) {
		goto end;
	}

	DBGH(stmt, "succesfully copied data from column #%hu (pos @ %lld), "
		"SQL C %hd.", ColumnNumber, stmt->gd_offt, TargetType);
end:
	/* XXX: if get_record(gd_ard, ColumnNumber)->meta_type != string/bin,
	 * should stmt->gd_offt bet set to -1 ?? */
	/* reinstate previous ARD */
	getdata_reset_ard(stmt, ard, ColumnNumber, arecs,
		sizeof(arecs)/sizeof(arecs[0]));
	if (! SQL_SUCCEEDED(ret)) {
		/* if function call failed, reset GD state */
		STMT_GD_RESET(stmt);
	}
	return ret;
}

/*
 * "SQLSetPos uses the rowset size that is in effect as of the preceding call
 * to SQLFetch or SQLFetchScroll, because SQLSetPos operates on a rowset that
 * has already been set. SQLSetPos also will pick up the new rowset size if
 * SQLBulkOperations has been called after the rowset size was changed."
 *
 * "When a block cursor first returns a rowset, the current row is the first
 * row of the rowset. To change the current row, the application calls
 * SQLSetPos or SQLBulkOperations (to update by bookmark)."
 *
 * "The driver returns SQLSTATE 01S01 (Error in row) only to indicate that an
 * error has occurred while rows were fetched by a call to SQLSetPos to
 * perform a bulk operation when the function is called in state S7." (not
 * supported currently, with RO operation)
 *
 * "In the IRD, this header field points to a row status array containing
 * status values after a call to SQLBulkOperations, SQLFetch, SQLFetchScroll,
 * or SQLSetPos."  = row status array of IRD (.array_status_ptr)
 *
 * "In the ARD, this header field points to a row operation array of values
 * that can be set by the application to indicate whether this row is to be
 * ignored for SQLSetPos operations." .array_status_ptr
 * "If the value in the SQL_DESC_ARRAY_STATUS_PTR field of the ARD is a null
 * pointer, all rows are included in the bulk operation"
 */
SQLRETURN EsSQLSetPos(
	SQLHSTMT        StatementHandle,
	SQLSETPOSIROW   RowNumber,
	SQLUSMALLINT    Operation,
	SQLUSMALLINT    LockType)
{
	switch(Operation) {
		case SQL_POSITION:
			// FIXME
			// check ESODBC_GETDATA_EXTENSIONS (GD_BLOCK) when implementing
			FIXME;
			break;

		case SQL_REFRESH:
		case SQL_UPDATE:
		case SQL_DELETE:
			ERRH(StatementHandle, "operation %d not supported.", Operation);
			RET_HDIAGS(STMH(StatementHandle), SQL_STATE_HYC00);
		default:
			ERRH(StatementHandle, "unknown operation type: %d.", Operation);
			RET_HDIAGS(STMH(StatementHandle), SQL_STATE_HY092);
	}
	return SQL_SUCCESS;
}

/*
 * == JDBC's Jdbc/PreparedStatement.executeLargeUpdate()
 * "SQLBulkOperations uses the rowset size in effect at the time of the call,
 * because it performs operations on a table independent of any fetched
 * rowset."
 * "In the IRD, this header field points to a row status array containing
 * status values after a call to SQLBulkOperations, SQLFetch, SQLFetchScroll,
 * or SQLSetPos."  = row status array of IRD (.array_status_ptr)
 */
SQLRETURN EsSQLBulkOperations(
	SQLHSTMT            StatementHandle,
	SQLSMALLINT         Operation)
{
	ERRH(StatementHandle, "data update functions not supported");
	RET_HDIAGS(STMH(StatementHandle), SQL_STATE_IM001);
}

SQLRETURN EsSQLMoreResults(SQLHSTMT hstmt)
{
	INFOH(hstmt, "multiple result sets not supported.");
	return SQL_NO_DATA;
}

SQLRETURN EsSQLCloseCursor(SQLHSTMT StatementHandle)
{
	esodbc_stmt_st *stmt = STMH(StatementHandle);
	if (! STMT_HAS_RESULTSET(stmt)) {
		ERRH(stmt, "no open cursor for statement");
		RET_HDIAGS(stmt, SQL_STATE_24000);
	}
	/* TODO: POST /_xpack/sql/close {"cursor":"<cursor>"} if cursor */
	return EsSQLFreeStmt(StatementHandle, SQL_CLOSE);
}

SQLRETURN EsSQLCancel(SQLHSTMT StatementHandle)
{
	esodbc_stmt_st *stmt = STMH(StatementHandle);

	/*
	 * Use cases:
	 * - "A function running asynchronously on the statement.": no async
	 *   support.
	 * - "A function on a statement that needs data." TODO: if data-at-exec.
	 * - "A function running on the statement on another thread.": this could
	 *   theoretically cancel an ongoing fetch/connect/etc. For now libcurl is
	 *   left to timeout -- TODO: if swiching to "multi" API in libcurl.
	 *   XXX: for this last case: stmt lock is being held here.
	 */

	DBGH(stmt, "canceling current statement operation -- NOOP.");
	return SQL_SUCCESS;
}

SQLRETURN EsSQLCancelHandle(SQLSMALLINT HandleType, SQLHANDLE InputHandle)
{
	/* see EsSQLCancel() */
	DBGH(InputHandle, "canceling current handle operation -- NOOP.");
	return SQL_SUCCESS;
}

SQLRETURN EsSQLNumResultCols(SQLHSTMT StatementHandle,
	_Out_ SQLSMALLINT *ColumnCount)
{
	return EsSQLGetDescFieldW(STMH(StatementHandle)->ird, NO_REC_NR,
			SQL_DESC_COUNT, ColumnCount, SQL_IS_SMALLINT, NULL);
}

/*
 * "The prepared statement associated with the statement handle can be
 * re-executed by calling SQLExecute until the application frees the statement
 * with a call to SQLFreeStmt with the SQL_DROP option or until the statement
 * handle is used in a call to SQLPrepare, SQLExecDirect, or one of the
 * catalog functions (SQLColumns, SQLTables, and so on)."
 */
SQLRETURN EsSQLPrepareW
(
	SQLHSTMT    hstmt,
	_In_reads_(cchSqlStr) SQLWCHAR *szSqlStr,
	SQLINTEGER  cchSqlStr
)
{
	esodbc_stmt_st *stmt = STMH(hstmt);
	SQLRETURN ret;

	if (cchSqlStr == SQL_NTS) {
		cchSqlStr = (SQLINTEGER)wcslen(szSqlStr);
	} else if (cchSqlStr <= 0) {
		ERRH(stmt, "invalid statment length: %d.", cchSqlStr);
		RET_HDIAGS(stmt, SQL_STATE_HY090);
	}
	DBGH(stmt, "preparing `" LWPDL "` [%d]", cchSqlStr, szSqlStr,
		cchSqlStr);

	ret = EsSQLFreeStmt(stmt, ESODBC_SQL_CLOSE);
	assert(SQL_SUCCEEDED(ret)); /* can't return error */

	return attach_sql(stmt, szSqlStr, cchSqlStr);
}


/* Find the ES/SQL type given in es_type; for ID matching multiple types
 * (scaled/half_float and keyword/text) use the best matching col_size, which
 * is the smallest, that's still matching (<=) the given one. This assumes the
 * types are ordered by it (as per the spec). */
static esodbc_estype_st *lookup_es_type(esodbc_dbc_st *dbc,
	SQLSMALLINT es_type, SQLULEN col_size)
{
	SQLULEN i;

	for (i = 0; i < dbc->no_types; i ++) {
		if (dbc->es_types[i].data_type == es_type) {
			if (col_size <= 0) {
				return &dbc->es_types[i];
			} else {
				assert(col_size < LONG_MAX);
				if ((SQLINTEGER)col_size <= dbc->es_types[i].column_size) {
					return &dbc->es_types[i];
				}
				if (es_type == SQL_VARCHAR &&
					dbc->es_types[i].column_size == dbc->max_varchar_size) {
					return &dbc->es_types[i];
				}
				if (es_type == SQL_FLOAT &&
					dbc->es_types[i].column_size == dbc->max_float_size) {
					return &dbc->es_types[i];
				}
			}
		}
	}
	WARNH(dbc, "no ES/SQL type found for ID %hd and column size %hd.",
		es_type, col_size);
	return NULL;
}

/* find the matching ES/SQL type for app's SQL type, which can be an exact
 * match against ES/SQL types, but also some other valid SQL type. */
static esodbc_estype_st *match_es_type(esodbc_rec_st *arec,
	esodbc_rec_st *irec)
{
	SQLULEN i, length;
	esodbc_dbc_st *dbc = arec->desc->hdr.stmt->hdr.dbc;

	for (i = 0; i < dbc->no_types; i ++) {
		if (dbc->es_types[i].data_type == irec->concise_type) {
			switch (irec->concise_type) {
				/* For SQL types mappign to more than one ES/SQL type, choose
				 * the ES/SQL type with smallest "size" that covers user given
				 * precision OR that has maximum precision (in case user's is
				 * larger than max ES/SQL offers. */
				case SQL_FLOAT: /* HALF_FLOAT, SCALED_FLOAT */
					if (irec->precision <= dbc->es_types[i].column_size ||
						dbc->es_types[i].column_size == dbc->max_float_size) {
						return &dbc->es_types[i];
					}
					break;
				case SQL_VARCHAR: /* KEYWORD, TEXT */
					length = irec->length ? irec->length : arec->octet_length;
					assert(length < LONG_MAX);
					if ((SQLINTEGER)length <= dbc->es_types[i].column_size ||
						dbc->es_types[i].column_size==dbc->max_varchar_size) {
						return &dbc->es_types[i];
					}
					break;
				default:
					/* unequivocal match */
					return &dbc->es_types[i];
			}
		}
	}

	/* app specified an SQL type with no direct mapping to an ES/SQL type */
	switch (irec->meta_type) {
		case METATYPE_EXACT_NUMERIC:
			assert(irec->concise_type == SQL_DECIMAL ||
				irec->concise_type == SQL_NUMERIC);
			return lookup_es_type(dbc, SQL_FLOAT, irec->precision);

		case METATYPE_STRING:
			length = irec->length ? irec->length : arec->octet_length;
			return lookup_es_type(dbc, SQL_VARCHAR, length);
		case METATYPE_BIN:
			/* SQL_VARBINARY == -3 == ES/SQL BINARY */
			return lookup_es_type(dbc, SQL_VARBINARY, /*no prec*/0);
		case METATYPE_DATETIME:
			assert(irec->concise_type == SQL_TYPE_DATE ||
				irec->concise_type == SQL_TYPE_TIME);
			return lookup_es_type(dbc, SQL_TYPE_TIMESTAMP, /*no prec*/0);
		case METATYPE_BIT:
			return lookup_es_type(dbc, SQL_BIT, /*no prec*/0);
		case METATYPE_UID:
			return lookup_es_type(dbc, SQL_VARCHAR, /*no prec: TEXT*/0);

		case METATYPE_INTERVAL_WSEC:
		case METATYPE_INTERVAL_WOSEC:
		case METATYPE_FLOAT_NUMERIC: /* these should have matched already */
		case METATYPE_MAX:
		/* -> SQL_C_DEFAULT, ESODBC_SQL_NULL, should've matched already */
		case METATYPE_UNKNOWN:
		default:
			BUGH(irec->desc, "unexpected meta type: %d.", irec->meta_type);
			SET_HDIAG(irec->desc, SQL_STATE_HY000,
				"bug converting parameters", 0);
	}

	return NULL;
}

/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/sending-long-data
 * Note: must use EsSQLSetDescFieldW() for param data-type setting, to call
 * set_defaults_from_type(), to meet the "Other fields implicitly set"
 * requirements from the page linked in set_defaults_from_type() comments.
 *
 * "Bindings remain in effect until the application calls SQLBindParameter
 * again, calls SQLFreeStmt with the SQL_RESET_PARAMS option, or calls
 * SQLSetDescField to set the SQL_DESC_COUNT header field of the APD to 0."
 */
SQLRETURN EsSQLBindParameter(
	SQLHSTMT        StatementHandle,
	SQLUSMALLINT    ParameterNumber,
	SQLSMALLINT     InputOutputType,
	SQLSMALLINT     ValueType,
	SQLSMALLINT     ParameterType,
	SQLULEN         ColumnSize,
	SQLSMALLINT     DecimalDigits,
	SQLPOINTER      ParameterValuePtr,
	SQLLEN          BufferLength,
	SQLLEN         *StrLen_or_IndPtr)
{
	SQLRETURN ret;
	esodbc_stmt_st *stmt = STMH(StatementHandle);
	esodbc_desc_st *desc;
	SQLSMALLINT apd_prev_count, ipd_prev_count;
	esodbc_rec_st *irec, *arec;

	if (InputOutputType != SQL_PARAM_INPUT) {
		ERRH(stmt, "parameter IO-type (%hd) not supported.", InputOutputType);
		RET_HDIAG(stmt, SQL_STATE_HYC00, "parameter IO-type not supported", 0);
	}
	/* ParameterNumber will be used as 1-based index */
	if (ParameterNumber < 1) {
		ERRH(stmt, "param. no (%hd) can't be less than 1.", ParameterNumber);
		RET_HDIAGS(stmt, SQL_STATE_07009);
	}

	/* Note: "If StrLen_or_IndPtr is a null pointer, the driver assumes that
	 * all input parameter values are non-NULL and that character and binary
	 * data is null-terminated." */
	if (StrLen_or_IndPtr) {
		if (*StrLen_or_IndPtr == SQL_DATA_AT_EXEC ||
			*StrLen_or_IndPtr < SQL_NTSL) {
			ERRH(stmt, "data-at-exec not supported (LenInd=%lld).",
				*StrLen_or_IndPtr);
			RET_HDIAG(stmt, SQL_STATE_HYC00, "data-at-exec not supported", 0);
		}
	} else {
		/* "If the ParameterValuePtr and StrLen_or_IndPtr arguments specified
		 * in SQLBindParameter are both null pointers, that function returns
		 * SQLSTATE HY009". */
		if (! ParameterValuePtr) {
			ERRH(stmt, "both value pointer and indicator are NULL.");
			RET_HDIAGS(stmt, SQL_STATE_HY009);
		}
	}

	apd_prev_count = stmt->apd->count;
	ipd_prev_count = stmt->ipd->count;

	/*
	 * APD descriptor setting.
	 */
	desc = stmt->apd;

	if (apd_prev_count < ParameterNumber) {
		ret = EsSQLSetDescFieldW(desc, NO_REC_NR, SQL_DESC_COUNT,
				/* ParameterNumber is unsigned, but SQL_DESC_COUNT is signed */
				(SQLPOINTER)(uintptr_t)ParameterNumber, SQL_IS_SMALLINT);
		if (! SQL_SUCCEEDED(ret)) {
			goto copy_diag;
		}
	}

	/* set types (or verbose for datetime/interval types) */
	ret = EsSQLSetDescFieldW(desc, ParameterNumber, SQL_DESC_CONCISE_TYPE,
			(SQLPOINTER)(intptr_t)ValueType, SQL_IS_SMALLINT);
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	/* "Sets the SQL_DESC_OCTET_LENGTH field to the value of BufferLength." */
	ret = EsSQLSetDescFieldW(desc, ParameterNumber, SQL_DESC_OCTET_LENGTH,
			(SQLPOINTER)(intptr_t)BufferLength, SQL_IS_INTEGER);
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	/* "Sets the SQL_DESC_OCTET_LENGTH_PTR field to the value of
	 * StrLen_or_Ind." */
	ret = EsSQLSetDescFieldW(desc, ParameterNumber, SQL_DESC_OCTET_LENGTH_PTR,
			StrLen_or_IndPtr,
			SQL_LEN_BINARY_ATTR((SQLINTEGER)sizeof(StrLen_or_IndPtr)));
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	/* "Sets the SQL_DESC_INDICATOR_PTR field to the value of
	 * StrLen_or_Ind."
	 * "The SQL_DESC_ARRAY_STATUS_PTR header field in the APD is used to
	 * ignore parameters." (IPD's indicate execution error status.) */
	ret = EsSQLSetDescFieldW(desc, ParameterNumber, SQL_DESC_INDICATOR_PTR,
			StrLen_or_IndPtr,
			SQL_LEN_BINARY_ATTR((SQLINTEGER)sizeof(StrLen_or_IndPtr)));
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	/* "Sets the SQL_DESC_DATA_PTR field to the value of ParameterValue."
	 * Note: needs to be last set field, as setting other fields unbinds. */
	ret = EsSQLSetDescFieldW(desc, ParameterNumber, SQL_DESC_DATA_PTR,
			ParameterValuePtr, SQL_IS_POINTER);
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	/*
	 * IPD descriptor setting.
	 */
	desc = stmt->ipd;

	if (ipd_prev_count < ParameterNumber) {
		ret = EsSQLSetDescFieldW(desc, NO_REC_NR, SQL_DESC_COUNT,
				/* ParameterNumber is unsigned, but SQL_DESC_COUNT is signed */
				(SQLPOINTER)(uintptr_t)ParameterNumber, SQL_IS_SMALLINT);
		if (! SQL_SUCCEEDED(ret)) {
			goto copy_diag;
		}
	}

	/* set types (or verbose for datetime/interval types) */
	ret = EsSQLSetDescFieldW(desc, ParameterNumber, SQL_DESC_CONCISE_TYPE,
			(SQLPOINTER)(intptr_t)ParameterType, SQL_IS_SMALLINT);
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	/* "sets the SQL_DESC_PARAMETER_TYPE field of the IPD." */
	ret = EsSQLSetDescFieldW(desc, ParameterNumber, SQL_DESC_PARAMETER_TYPE,
			(SQLPOINTER)(intptr_t)InputOutputType, SQL_IS_SMALLINT);
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	irec = get_record(desc, ParameterNumber, /*grow?*/FALSE);
	assert(irec);

	ret = set_param_decdigits(irec, ParameterNumber, DecimalDigits);
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	ret = set_param_size(irec, ParameterNumber, ColumnSize);
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	/*
	 * data conversion settings
	 */
	ret = convertability_check(stmt, (SQLINTEGER)ParameterNumber, NULL);
	if (! SQL_SUCCEEDED(ret)) {
		goto err;
	}

	arec = get_record(stmt->apd, ParameterNumber, /*grow?*/FALSE);
	irec->es_type = match_es_type(arec, irec);
	if (! irec->es_type) {
		/* validation shoudl have been done earlier on meta type setting
		 * (SQL_DESC_CONCISE_TYPE) */
		BUGH(stmt, "failed to match valid SQL type %hd.", irec->concise_type);
		SET_HDIAG(desc, SQL_STATE_HY000, "parameter binding bug", 0);
		goto copy_diag;
	}
	DBGH(stmt, "SQL type %hd matched to `" LCPDL "` ES/SQL type.",
		ParameterType, LCSTR(&irec->es_type->type_name_c));

	DBGH(stmt, "succesfully bound parameter #%hu, IO-type: %hd, "
		"SQL C type: %hd, SQL type: %hd, size: %llu, decdigits: %hd, "
		"buffer@0x%p, length: %lld, LenInd@0x%p.", ParameterNumber,
		InputOutputType, ValueType, ParameterType, ColumnSize, DecimalDigits,
		ParameterValuePtr, BufferLength, StrLen_or_IndPtr);

	return SQL_SUCCESS;

copy_diag:
	/* copy initial error at top handle level, where it's going to be "
	 * "inquired from (more might occur below) */
	HDIAG_COPY(desc, stmt);
err:
	ERRH(stmt, "binding parameter failed -- resetting xPD counts.");
	/* "If the call to SQLBindParameter fails, [...] the SQL_DESC_COUNT field
	 * of the APD" [...] "and the SQL_DESC_COUNT field of the IPD is
	 * unchanged." */
	if (apd_prev_count != stmt->apd->count) {
		ret = EsSQLSetDescFieldW(stmt->apd, NO_REC_NR, SQL_DESC_COUNT,
				(SQLPOINTER)(uintptr_t)-apd_prev_count, SQL_IS_SMALLINT);
		if (! SQL_SUCCEEDED(ret)) {
			ERRH(stmt, "failed to reset APD count back to %hd - descriptor "
				"might be left in inconsistent state!", -apd_prev_count);
		}
	}
	if (ipd_prev_count != stmt->ipd->count) {
		ret = EsSQLSetDescFieldW(stmt->ipd, NO_REC_NR, SQL_DESC_COUNT,
				(SQLPOINTER)(uintptr_t)-ipd_prev_count, SQL_IS_SMALLINT);
		if (! SQL_SUCCEEDED(ret)) {
			ERRH(stmt, "failed to reset IPD count back to %hd - descriptor "
				"might be left in inconsistent state!", -ipd_prev_count);
		}
	}
	/* return original failure code */
	RET_STATE(stmt->hdr.diag.state);
}


/* Converts parameter values to string, JSON-escaping where necessary
 * The conversion is actually left to the server: the driver will use the
 * C source data type as value (at least for numbers) and specify what ES/SQL
 * type that value should be converted to. */
static SQLRETURN convert_param_val(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, char *dest, size_t *len)
{
	SQLLEN *ind_ptr;
	double min, max;
	BOOL fixed;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;

	ind_ptr = deferred_address(SQL_DESC_INDICATOR_PTR, pos, arec);
	if (ind_ptr && *ind_ptr == SQL_NULL_DATA) {
		return c2sql_null(arec, irec, dest, len);
	}
	/* from here on, "input parameter value[ is] non-NULL" */
	assert(deferred_address(SQL_DESC_DATA_PTR, pos, arec));

	/*INDENT-OFF*/
	switch (irec->es_type->data_type) {
		/* JSON null */
		case SQL_UNKNOWN_TYPE: /* NULL */
			return c2sql_null(arec, irec, dest, len);

		case SQL_BIT: /* BIT */
		case ESODBC_SQL_BOOLEAN: /* BOOLEAN */
			return c2sql_boolean(arec, irec, pos, dest, len);

		do {
		/* JSON long */
		case SQL_BIGINT: /* LONG */
			min = (double)LLONG_MIN; /* precision warnings */
			max = (double)LLONG_MAX;
			fixed = TRUE;
			break;
		case SQL_INTEGER: /* INTEGER */
			min = LONG_MIN;
			max = LONG_MAX;
			fixed = TRUE;
			break;
		case SQL_SMALLINT: /* SHORT */
			min = SHRT_MIN;
			max = SHRT_MAX;
			fixed = TRUE;
		case SQL_TINYINT: /* BYTE */
			min = CHAR_MIN;
			max = CHAR_MAX;
			fixed = TRUE;
			break;

		/* JSON double */
			// TODO: check accurate limits for floats in ES/SQL
		case SQL_REAL: /* FLOAT */
			min = FLT_MIN;
			max = FLT_MAX;
			fixed = FALSE;
			break;
		case SQL_FLOAT: /* HALF_FLOAT, SCALED_FLOAT */
		case SQL_DOUBLE: /* DOUBLE */
			min = DBL_MIN;
			max = DBL_MAX;
			fixed = FALSE;
			break;
		} while (0);
			return c2sql_number(arec, irec, pos, &min, &max, fixed, dest, len);

		/* JSON string */
		case SQL_VARCHAR: /* KEYWORD, TEXT */
			return c2sql_varchar(arec, irec, pos, dest, len);

		case SQL_TYPE_TIMESTAMP: /* DATE */
			return c2sql_timestamp(arec, irec, pos, dest, len);

		case SQL_INTERVAL_YEAR:
		case SQL_INTERVAL_MONTH:
		case SQL_INTERVAL_DAY:
		case SQL_INTERVAL_HOUR:
		case SQL_INTERVAL_MINUTE:
		case SQL_INTERVAL_SECOND:
		case SQL_INTERVAL_YEAR_TO_MONTH:
		case SQL_INTERVAL_DAY_TO_HOUR:
		case SQL_INTERVAL_DAY_TO_MINUTE:
		case SQL_INTERVAL_DAY_TO_SECOND:
		case SQL_INTERVAL_HOUR_TO_MINUTE:
		case SQL_INTERVAL_HOUR_TO_SECOND:
		case SQL_INTERVAL_MINUTE_TO_SECOND:
			return c2sql_interval(arec, irec, pos, dest, len);

		/* JSON (Base64 encoded) string */
		case SQL_VARBINARY: /* BINARY */
			// XXX: json_escape
			ERRH(stmt, "conversion to SQL BINARY not implemented.");
			RET_HDIAG(stmt, SQL_STATE_HYC00, "conversion to SQL BINARY "
				"not yet supported", 0);
			break;

		default:
			BUGH(arec->desc->hdr.stmt, "unexpected ES/SQL type %hd.",
				irec->es_type->data_type);
			RET_HDIAG(arec->desc->hdr.stmt, SQL_STATE_HY000,
				"parameter conversion bug", 0);
	}
	/*INDENT-ON*/

	return SQL_SUCCESS;
}

/* Forms the JSON array with params:
 * [{"type": "<ES/SQL type name>", "value": <param value>}(,etc)*] */
static SQLRETURN serialize_params(esodbc_stmt_st *stmt, char *dest,
	size_t *len)
{
	/* JSON keys for building one parameter object */
#	define JSON_KEY_TYPE	"{\"type\": \""
#	define JSON_KEY_VALUE	"\", \"value\": "

	esodbc_rec_st *arec, *irec;
	SQLRETURN ret;
	SQLSMALLINT i;
	size_t l, pos;

	pos = 0;
	if (dest) {
		dest[pos] = '[';
	}
	pos ++;

	for (i = 0; i < stmt->apd->count; i ++) {
		arec = get_record(stmt->apd, i + 1, /*grow?*/FALSE);
		irec = get_record(stmt->ipd, i + 1, /*grow?*/FALSE);
		assert(arec && irec && irec->es_type);

		if (dest) {
			if (i) {
				memcpy(dest + pos, ", ", 2);
			}
			/* copy 'type' JSON key name */
			memcpy(dest + pos + 2 * !!i, JSON_KEY_TYPE,
				sizeof(JSON_KEY_TYPE) - 1);
		}
		pos += 2 * !!i + sizeof(JSON_KEY_TYPE) - 1;

		/* copy/eval ES/SQL type name */
		pos += json_escape(irec->es_type->type_name_c.str,
				irec->es_type->type_name_c.cnt, dest ? dest + pos : NULL,
				/*"unlimited" output buffer*/(size_t)-1);

		if (dest) {
			/* copy 'value' JSON key name */
			memcpy(dest + pos, JSON_KEY_VALUE, sizeof(JSON_KEY_VALUE) - 1);
		}
		pos += sizeof(JSON_KEY_VALUE) - 1;

		/* copy converted parameter value */
		ret = convert_param_val(arec, irec, /*params array pos*/0LLU,
				dest ? dest + pos : NULL, &l);
		if (SQL_SUCCEEDED(ret)) {
			pos += l;
		} else {
			ERRH(stmt, "converting parameter #%hd failed.", i + 1);
			return ret;
		}

		if (dest) {
			dest[pos] = '}';
		}
		pos ++;
	}

	if (dest) {
		dest[pos] = ']';
	}
	pos ++;

	*len = pos;
	return SQL_SUCCESS;

#	undef JSON_KEY_TYPE
#	undef JSON_KEY_VALUE
}

/*
 * Build a serialized JSON object out of the statement.
 * If resulting string fits into the given buff, the result is copied in it;
 * othewise a new one will be allocated and returned.
 */
SQLRETURN TEST_API serialize_statement(esodbc_stmt_st *stmt, cstr_st *buff)
{
	SQLRETURN ret = SQL_SUCCESS;
	size_t bodylen, pos, len;
	char *body;
	esodbc_dbc_st *dbc = stmt->hdr.dbc;
	esodbc_desc_st *apd = stmt->apd;

	/* TODO: move escaping/x-coding (to JSON or CBOR) in attach_sql() and/or
	 * attach_answer() to avoid these operations for each execution of the
	 * statement (especially for the SQL statement; the cursor might not
	 * always be used - if app decides to no longer fetch - but would then
	 * clean this function). */

	/* enforced in EsSQLSetDescFieldW(SQL_DESC_ARRAY_SIZE) */
	assert(apd->array_size <= 1);

	bodylen = 1; /* { */
	/* evaluate how long the stringified REST object will be */
	if (stmt->rset.ecurs.cnt) { /* eval CURSOR object length */
		/* assumptions: (1) the cursor is a Base64 encoded string and thus
		 * (2) no JSON escaping needed.
		 * (both assertions checked on copy, below). */
		bodylen += sizeof(JSON_KEY_CURSOR) - 1; /* "cursor":  */
		bodylen += stmt->rset.ecurs.cnt;
		bodylen += 2; /* 2x `"` for cursor value */
	} else { /* eval QUERY object length */
		bodylen += sizeof(JSON_KEY_QUERY) - 1;
		bodylen += json_escape(stmt->u8sql.str, stmt->u8sql.cnt, NULL, 0);
		bodylen += 2; /* 2x `"` for query value */

		/* does the statement have any parameters? */
		if (apd->count) {
			bodylen += sizeof(JSON_KEY_PARAMS) - 1;
			/* serialize_params will count/copy array delimiters (`[`, `]`) */
			ret = serialize_params(stmt, /* don't copy, just eval len */NULL,
					&len);
			if (! SQL_SUCCEEDED(ret)) {
				ERRH(stmt, "failed to eval parameters length");
				return ret;
			}
			bodylen += len;
		}
		/* does the statement have any fetch_size? */
		if (dbc->fetch.slen) {
			bodylen += sizeof(JSON_KEY_FETCH) - /*\0*/1;
			bodylen += dbc->fetch.slen;
		}
	}
	/* TODO: request_/page_timeout, time_zone */
	bodylen += sizeof(JSON_KEY_VAL_MODE) - 1; /* "mode": */
	bodylen += sizeof(JSON_KEY_CLT_ID) - 1; /* "client_id": */
	bodylen += 1; /* } */

	/* allocate memory for the stringified buffer, if needed */
	if (buff->cnt < bodylen) {
		WARNH(dbc, "local buffer too small (%zd), need %zdB; will alloc.",
			buff->cnt, bodylen);
		WARNH(dbc, "local buffer too small, SQL: `" LCPDL "`.",
			LCSTR(&stmt->u8sql));
		body = malloc(bodylen);
		if (! body) {
			ERRNH(stmt, "failed to alloc %zdB.", bodylen);
			RET_HDIAGS(stmt, SQL_STATE_HY001);
		}
		buff->str = body;
	} else {
		body = buff->str;
	}

	pos = 0;
	body[pos ++] = '{';
	/* build the actual stringified JSON object */
	if (stmt->rset.ecurs.cnt) { /* copy CURSOR object */
		memcpy(body + pos, JSON_KEY_CURSOR, sizeof(JSON_KEY_CURSOR) - 1);
		pos += sizeof(JSON_KEY_CURSOR) - 1;
		body[pos ++] = '"';
		if (ascii_w2c(stmt->rset.ecurs.str, body + pos,
					stmt->rset.ecurs.cnt) <= 0) {
			if (buff->cnt < bodylen) { /* has it been alloc'd? */
				free(body);
			}
			ERRH(stmt, "failed to convert cursor `" LWPDL "` to ASCII.",
				LWSTR(&stmt->rset.ecurs));
			RET_HDIAGS(stmt, SQL_STATE_24000);
		} else {
			/* no character needs JSON escaping */
			assert(stmt->rset.ecurs.cnt == json_escape(body + pos,
						stmt->rset.ecurs.cnt, NULL, 0));
			pos += stmt->rset.ecurs.cnt;
		}
		body[pos ++] = '"';
	} else { /* copy QUERY object */
		memcpy(body + pos, JSON_KEY_QUERY, sizeof(JSON_KEY_QUERY) - 1);
		pos += sizeof(JSON_KEY_QUERY) - 1;
		body[pos ++] = '"';
		pos += json_escape(stmt->u8sql.str, stmt->u8sql.cnt, body + pos,
				bodylen - pos);
		body[pos ++] = '"';

		/* does the statement have any parameters? */
		if (apd->count) {
			memcpy(body + pos, JSON_KEY_PARAMS, sizeof(JSON_KEY_PARAMS) - 1);
			pos += sizeof(JSON_KEY_PARAMS) - 1;
			/* serialize_params will count/copy array delimiters (`[`, `]`) */
			ret = serialize_params(stmt, body + pos, &len);
			if (! SQL_SUCCEEDED(ret)) {
				ERRH(stmt, "failed to serialize parameters");
				return ret;
			}
			pos += len;
		}
		/* does the statement have any fetch_size? */
		if (dbc->fetch.slen) {
			memcpy(body + pos, JSON_KEY_FETCH, sizeof(JSON_KEY_FETCH) - 1);
			pos += sizeof(JSON_KEY_FETCH) - 1;
			memcpy(body + pos, dbc->fetch.str, dbc->fetch.slen);
			pos += dbc->fetch.slen;
		}
	}
	/* "mode": */
	memcpy(body + pos, JSON_KEY_VAL_MODE, sizeof(JSON_KEY_VAL_MODE) - 1);
	pos += sizeof(JSON_KEY_VAL_MODE) - 1;
	memcpy(body + pos, JSON_KEY_CLT_ID, sizeof(JSON_KEY_CLT_ID) - 1);
	pos += sizeof(JSON_KEY_CLT_ID) - 1;
	body[pos ++] = '}';

	buff->cnt = pos;

	DBGH(stmt, "JSON serialized to: [%zd] `" LCPDL "`.", pos, LCSTR(buff));
	return ret;

#	undef JSON_KEY_QUERY
#	undef JSON_KEY_CURSOR
#	undef JSON_KEY_PARAMS
#	undef JSON_KEY_FETCH
#	undef JSON_KEY_REQ_TOUT
#	undef JSON_KEY_PAGE_TOUT
#	undef JSON_KEY_TIME_ZONE
}


/*
 * "In the IPD, this header field points to a parameter status array
 * containing status information for each set of parameter values after a call
 * to SQLExecute or SQLExecDirect." = .array_status_ptr
 *
 * "In the APD, this header field points to a parameter operation array of
 * values that can be set by the application to indicate whether this set of
 * parameters is to be ignored when SQLExecute or SQLExecDirect is called."
 * = .array_status_ptr
 * "If no elements of the array are set, all sets of parameters in the array
 * are used in the SQLExecute or SQLExecDirect calls."
 *
 * "When the statement is executed, [...] the *ParameterValuePtr buffer must
 * contain a valid input value, or the *StrLen_or_IndPtr buffer must contain
 * SQL_NULL_DATA, SQL_DATA_AT_EXEC, or the result of the SQL_LEN_DATA_AT_EXEC
 * macro."
 *
 * APD.SQL_DESC_BIND_OFFSET_PTR: "[i]f the field is non-null, the driver
 * dereferences the pointer and, if none of the values in the
 * SQL_DESC_DATA_PTR, SQL_DESC_INDICATOR_PTR, and SQL_DESC_OCTET_LENGTH_PTR
 * fields is a null pointer, adds the dereferenced value to those fields in
 * the descriptor records at execution time."
 *
 * APD.SQL_DESC_ARRAY_STATUS_PTR: "SQL_PARAM_IGNORE, to indicate that the row
 * is excluded from statement execution" and, "[i]n addition [...] the
 * following codes cause a parameter in an SQL statement to be ignored:
 * SQL_ROW_DELETED, SQL_ROW_UPDATED, and SQL_ROW_ERROR."; the opposite is:
 * SQL_ROW_PROCEED, SQL_ROW_SUCCESS, SQL_ROW_SUCCESS_WITH_INFO, and
 * SQL_ROW_ADDED.
 */
SQLRETURN EsSQLExecute(SQLHSTMT hstmt)
{
	SQLRETURN ret;
	esodbc_stmt_st *stmt = STMH(hstmt);
	char buff[ESODBC_BODY_BUF_START_SIZE];
	cstr_st body = {buff, sizeof(buff)};

	DBGH(stmt, "executing SQL: [%zd] `" LCPDL "`.", stmt->u8sql.cnt,
		LCSTR(&stmt->u8sql));

	ret = serialize_statement(stmt, &body);
	if (SQL_SUCCEEDED(ret)) {
		ret = post_json(stmt, &body);
	}

	if (buff != body.str) {
		free(body.str);
	}

	return ret;
}

/*
 * "In the IPD, this header field points to a parameter status array
 * containing status information for each set of parameter values after a call
 * to SQLExecute or SQLExecDirect." = .array_status_ptr
 *
 * "In the APD, this header field points to a parameter operation array of
 * values that can be set by the application to indicate whether this set of
 * parameters is to be ignored when SQLExecute or SQLExecDirect is called."
 * = .array_status_ptr
 * "If no elements of the array are set, all sets of parameters in the array
 * are used in the SQLExecute or SQLExecDirect calls."
 */
SQLRETURN EsSQLExecDirectW
(
	SQLHSTMT    hstmt,
	_In_reads_opt_(TextLength) SQLWCHAR *szSqlStr,
	SQLINTEGER cchSqlStr
)
{
	esodbc_stmt_st *stmt = STMH(hstmt);
	SQLRETURN ret;

	if (cchSqlStr == SQL_NTS) {
		cchSqlStr = (SQLINTEGER)wcslen(szSqlStr);
	} else if (cchSqlStr <= 0) {
		ERRH(stmt, "invalid statment length: %d.", cchSqlStr);
		RET_HDIAGS(stmt, SQL_STATE_HY090);
	}
	DBGH(stmt, "directly executing SQL: `" LWPDL "` [%d].", cchSqlStr,
		szSqlStr, cchSqlStr);

	ret = EsSQLFreeStmt(stmt, ESODBC_SQL_CLOSE);
	assert(SQL_SUCCEEDED(ret)); /* can't return error */

	/* no support for parameter arrays (yet) */
	// & param marker is set!
	assert(stmt->apd->array_size <= 1);

	ret = attach_sql(stmt, szSqlStr, cchSqlStr);
	if (SQL_SUCCEEDED(ret)) {
		ret = EsSQLExecute(stmt);
	}
#ifndef NDEBUG
	/* no reason to keep it (it can't be re-executed), except for debugging */
	detach_sql(stmt);
#endif /* NDEBUG */
	return ret;
}

/* simple loop back, the driver does no actual translation */
SQLRETURN EsSQLNativeSqlW(
	SQLHDBC                                     hdbc,
	_In_reads_(cchSqlStrIn) SQLWCHAR           *szSqlStrIn,
	SQLINTEGER                                  cchSqlStrIn,
	_Out_writes_opt_(cchSqlStrMax) SQLWCHAR    *szSqlStr,
	SQLINTEGER                                  cchSqlStrMax,
	SQLINTEGER                                 *pcchSqlStr)
{
	SQLINTEGER to_copy;
	int state = SQL_STATE_00000;

	if (! szSqlStrIn) {
		RET_HDIAGS(DBCH(hdbc), SQL_STATE_HY009);
	}

	if (cchSqlStrIn == SQL_NTSL) {
		cchSqlStrIn = (SQLINTEGER)wcslen(szSqlStrIn);
	}
	if (pcchSqlStr) {
		*pcchSqlStr = cchSqlStrIn;
	}

	if (szSqlStr) {
		if (cchSqlStrMax <= cchSqlStrIn) {
			to_copy = cchSqlStrMax - /* \0 */1;
			state = SQL_STATE_01004;
		} else {
			to_copy = cchSqlStrIn;
		}
		memcpy(szSqlStr, szSqlStrIn, to_copy * sizeof(*szSqlStr));
		szSqlStr[to_copy] = L'\0';
	}

	if (state == SQL_STATE_01004) {
		RET_HDIAGS(DBCH(hdbc), SQL_STATE_01004);
	} else {
		return SQL_SUCCESS;
	}
}

static inline SQLULEN get_col_size(esodbc_rec_st *rec)
{
	assert(DESC_TYPE_IS_IMPLEMENTATION(rec->desc->type));

	switch (rec->meta_type) {
		case METATYPE_EXACT_NUMERIC:
		case METATYPE_FLOAT_NUMERIC:
			return rec->es_type->column_size; // precision?

		case METATYPE_STRING:
		case METATYPE_BIN:
		case METATYPE_DATETIME:
		case METATYPE_INTERVAL_WSEC:
		case METATYPE_INTERVAL_WOSEC:
		case METATYPE_BIT:
			return rec->length;

		case METATYPE_UID:
			BUGH(rec->desc, "unsupported data c-type: %d.", rec->concise_type);
	}
	/*
	 * https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqldescribecol-function#arguments :
	 * "If the column size cannot be determined, the driver returns 0." */
	return 0;
}

static inline SQLSMALLINT get_col_decdigits(esodbc_rec_st *rec)
{
	assert(DESC_TYPE_IS_IMPLEMENTATION(rec->desc->type));

	switch (rec->meta_type) {
		case METATYPE_DATETIME:
		case METATYPE_INTERVAL_WSEC:
			return ESODBC_MAX_SEC_PRECISION;

		case METATYPE_EXACT_NUMERIC:
			return rec->es_type->maximum_scale;
	}
	/* 0 to be returned for unknown case:
	 * https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqldescribecol-function#syntax
	 */
	return 0;
}

SQLRETURN EsSQLDescribeColW(
	SQLHSTMT            hstmt,
	SQLUSMALLINT        icol,
	_Out_writes_opt_(cchColNameMax)
	SQLWCHAR            *szColName,
	SQLSMALLINT         cchColNameMax,
	_Out_opt_
	SQLSMALLINT        *pcchColName,
	_Out_opt_
	SQLSMALLINT        *pfSqlType,
	_Out_opt_
	SQLULEN            *pcbColDef,
	_Out_opt_
	SQLSMALLINT        *pibScale,
	_Out_opt_
	SQLSMALLINT        *pfNullable)
{
	esodbc_stmt_st *stmt = STMH(hstmt);
	esodbc_rec_st *rec;
	SQLRETURN ret;
	SQLSMALLINT col_blen;

	DBGH(stmt, "IRD@0x%p, column #%d.", stmt->ird, icol);

	if (! STMT_HAS_RESULTSET(stmt)) {
		ERRH(stmt, "no resultset available on statement.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	if (icol < 1) {
		/* TODO: if implementing bookmarks */
		RET_HDIAGS(stmt, SQL_STATE_HYC00);
	}

	rec = get_record(stmt->ird, icol, FALSE);
	if (! rec) {
		ERRH(stmt, "no record for columns #%d.", icol);
		RET_HDIAGS(stmt, SQL_STATE_07009);
	}
#ifndef NDEBUG
	//dump_record(rec);
#endif /* NDEBUG */

	if (szColName) {
		ret = write_wstr(stmt, szColName, &rec->name,
				cchColNameMax * sizeof(*szColName), &col_blen);
		if (! SQL_SUCCEEDED(ret)) {
			ERRH(stmt, "failed to copy column name `" LWPDL "`.",
				LWSTR(&rec->name));
			return ret;
		}
	} else {
		DBGH(stmt, "NULL column name buffer provided.");
		col_blen = -1;
	}

	if (! pcchColName) {
		ERRH(stmt, "no column name length buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090,
			"no column name length buffer provided", 0);
	}
	*pcchColName = 0 <= col_blen ? (col_blen / sizeof(*szColName)) :
		(SQLSMALLINT)rec->name.cnt;
	DBGH(stmt, "col #%d name has %d chars.", icol, *pcchColName);

	if (! pfSqlType) {
		ERRH(stmt, "no column data type buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090,
			"no column data type buffer provided", 0);
	}
	*pfSqlType = rec->concise_type;
	DBGH(stmt, "col #%d has concise type=%d.", icol, *pfSqlType);

	if (! pcbColDef) {
		ERRH(stmt, "no column size buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090, "no column size buffer provided", 0);
	}
	*pcbColDef = get_col_size(rec);
	DBGH(stmt, "col #%d of meta type %d has size=%llu.",
		icol, rec->meta_type, *pcbColDef);

	if (! pibScale) {
		ERRH(stmt, "no column decimal digits buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090,
			"no column decimal digits buffer provided", 0);
	}
	*pibScale = get_col_decdigits(rec);
	DBGH(stmt, "col #%d of meta type %d has decimal digits=%d.",
		icol, rec->meta_type, *pibScale);

	if (! pfNullable) {
		ERRH(stmt, "no column decimal digits buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090,
			"no column decimal digits buffer provided", 0);
	}
	ASSERT_IXD_HAS_ES_TYPE(rec);
	/* TODO: this would be available in SQLColumns resultset. */
	*pfNullable = rec->es_type->nullable;
	DBGH(stmt, "col #%d nullable=%d.", icol, *pfNullable);

	return SQL_SUCCESS;
}


SQLRETURN EsSQLColAttributeW(
	SQLHSTMT        hstmt,
	SQLUSMALLINT    iCol,
	SQLUSMALLINT    iField,
	_Out_writes_bytes_opt_(cbDescMax)
	SQLPOINTER      pCharAttr, /* [out] value, if string; can be NULL */
	SQLSMALLINT     cbDescMax, /* [in] byte len of pCharAttr */
	_Out_opt_
	SQLSMALLINT     *pcbCharAttr, /* [out] len written in pCharAttr (w/o \0 */
	_Out_opt_
#ifdef _WIN64
	SQLLEN          *pNumAttr /* [out] value, if numeric */
#else /* _WIN64 */
	SQLPOINTER      pNumAttr
#endif /* _WIN64 */
)
{
	esodbc_stmt_st *stmt = STMH(hstmt);
	esodbc_desc_st *ird = stmt->ird;
	esodbc_rec_st *rec;
	SQLSMALLINT sint;
	wstr_st *wstrp;
	SQLLEN len;
	SQLINTEGER iint;

#ifdef _WIN64
#define PNUMATTR_ASSIGN(type, value) *pNumAttr = (SQLLEN)(value)
#else /* _WIN64 */
#define PNUMATTR_ASSIGN(type, value) *(type *)pNumAttr = (type)(value)
#endif /* _WIN64 */

	DBGH(stmt, "IRD@0x%p, column #%d, field: %d.", ird, iCol, iField);

	if (! STMT_HAS_RESULTSET(stmt)) {
		ERRH(stmt, "no resultset available on statement.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	if (iCol < 1) {
		/* TODO: if implementing bookmarks */
		RET_HDIAGS(stmt, SQL_STATE_HYC00);
	}

	rec = get_record(ird, iCol, FALSE);
	if (! rec) {
		ERRH(stmt, "no record for columns #%d.", iCol);
		RET_HDIAGS(stmt, SQL_STATE_07009);
	}

	ASSERT_IXD_HAS_ES_TYPE(rec);

	/*INDENT-OFF*/
	switch (iField) {
		/* SQLSMALLINT */
		do {
		case SQL_DESC_CONCISE_TYPE: sint = rec->concise_type; break;
		case SQL_DESC_TYPE: sint = rec->type; break;
		case SQL_DESC_UNNAMED: sint = rec->unnamed; break;
		case SQL_DESC_NULLABLE: sint = rec->es_type->nullable; break;
		case SQL_DESC_SEARCHABLE: sint = rec->es_type->searchable; break;
		case SQL_DESC_UNSIGNED: sint = rec->es_type->unsigned_attribute; break;
		case SQL_DESC_UPDATABLE: sint = rec->updatable; break;
		case SQL_DESC_PRECISION:
			sint = rec->es_type->fixed_prec_scale;
			break;
		case SQL_DESC_SCALE:
			sint = rec->es_type->maximum_scale;
			break;
		case SQL_DESC_FIXED_PREC_SCALE:
			sint = rec->es_type->fixed_prec_scale;
			break;
		} while (0);
			PNUMATTR_ASSIGN(SQLSMALLINT, sint);
			break;

		/* SQLWCHAR* */
		do {
		case SQL_DESC_BASE_COLUMN_NAME: wstrp = &rec->base_column_name; break;
		case SQL_DESC_LABEL: wstrp = &rec->label; break;
		case SQL_DESC_BASE_TABLE_NAME: wstrp = &rec->base_table_name; break;
		case SQL_DESC_CATALOG_NAME: wstrp = &rec->catalog_name; break;
		case SQL_DESC_NAME: wstrp = &rec->name; break;
		case SQL_DESC_SCHEMA_NAME: wstrp = &rec->schema_name; break;
		case SQL_DESC_TABLE_NAME: wstrp = &rec->table_name; break;
		case SQL_DESC_LITERAL_PREFIX:
			wstrp = &rec->es_type->literal_prefix;
			break;
		case SQL_DESC_LITERAL_SUFFIX:
			wstrp = &rec->es_type->literal_suffix;
			break;
		case SQL_DESC_LOCAL_TYPE_NAME:
			wstrp = &rec->es_type->type_name;
			break;
		case SQL_DESC_TYPE_NAME:
			wstrp = &rec->es_type->type_name;
			break;
		} while (0);
			return write_wstr(stmt, pCharAttr, wstrp, cbDescMax, pcbCharAttr);

		/* SQLLEN */
		do {
		case SQL_DESC_DISPLAY_SIZE: len = rec->es_type->display_size; break;
		case SQL_DESC_OCTET_LENGTH: len = rec->octet_length; break;
		} while (0);
			PNUMATTR_ASSIGN(SQLLEN, len);
			break;

		/* SQLULEN */
		case SQL_DESC_LENGTH:
			/* "This information is returned from the SQL_DESC_LENGTH record
			 * field of the IRD." */
			PNUMATTR_ASSIGN(SQLULEN, rec->length);
			break;

		/* SQLINTEGER */
		do {
		case SQL_DESC_AUTO_UNIQUE_VALUE:
			iint = rec->es_type->auto_unique_value;
			break;
		case SQL_DESC_CASE_SENSITIVE:
			iint = rec->es_type->case_sensitive;
			break;
		case SQL_DESC_NUM_PREC_RADIX: iint = rec->num_prec_radix; break;
		} while (0);
			PNUMATTR_ASSIGN(SQLINTEGER, iint);
			break;


		case SQL_DESC_COUNT:
			PNUMATTR_ASSIGN(SQLSMALLINT, ird->count);
			break;

		default:
			ERRH(stmt, "unknown field type %d.", iField);
			RET_HDIAGS(stmt, SQL_STATE_HY091);
	}
	/*INDENT-ON*/

	return SQL_SUCCESS;
#undef PNUMATTR_ASSIGN
}

/* function implementation is correct, but it can't really be used as
 * intended, since the driver's "preparation" doesn't really involve sending
 * it to ES or even parameter marker counting; the later would be doable now,
 * but SQLDescribeParam would either need ES/SQL support, or execute with
 * fetch_size 0 or 1. */
SQLRETURN EsSQLNumParams(
	SQLHSTMT           StatementHandle,
	_Out_opt_
	SQLSMALLINT       *ParameterCountPtr)
{
	esodbc_stmt_st *stmt = STMH(StatementHandle);

	if (! stmt->u8sql.cnt) {
		ERRH(stmt, "statement hasn't been prepared.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	return EsSQLGetDescFieldW(stmt->ipd, NO_REC_NR,
			SQL_DESC_COUNT, ParameterCountPtr, SQL_IS_SMALLINT, NULL);
}

SQLRETURN EsSQLRowCount(_In_ SQLHSTMT StatementHandle, _Out_ SQLLEN *RowCount)
{
	esodbc_stmt_st *stmt = STMH(StatementHandle);

	if (! STMT_HAS_RESULTSET(stmt)) {
		ERRH(stmt, "no resultset available on statement.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	DBGH(stmt, "current resultset rows count: %zd.", stmt->rset.nrows);
	*RowCount = (SQLLEN)stmt->rset.nrows;

	if (stmt->rset.ecurs.cnt) {
		/* fetch_size or scroller size chunks the result */
		WARNH(stmt, "this function will only return the row count of the "
			"partial result set available.");
		/* returning a _WITH_INFO here will fail the query for MSQRY32.. */
		//RET_HDIAG(stmt, SQL_STATE_01000, "row count is for partial result "
		//		"only", 0);
	}
	return SQL_SUCCESS;
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=78 : */
