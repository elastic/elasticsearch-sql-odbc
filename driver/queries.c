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

#include <inttypes.h>
#include <windows.h> /* WideCharToMultiByte() */

#include "timestamp.h"

#include "queries.h"
#include "handles.h"
#include "connect.h"
#include "info.h"

/* key names used in Elastic/SQL REST/JSON answers */
#define JSON_ANSWER_COLUMNS		"columns"
#define JSON_ANSWER_ROWS		"rows"
#define JSON_ANSWER_CURSOR		"cursor"
#define JSON_ANSWER_STATUS		"status"
#define JSON_ANSWER_ERROR		"error"
#define JSON_ANSWER_ERR_TYPE	"type"
#define JSON_ANSWER_ERR_REASON	"reason"
#define JSON_ANSWER_COL_NAME	"name"
#define JSON_ANSWER_COL_TYPE	"type"
/* 4 */
#define JSON_COL_TEXT			"text"
#define JSON_COL_DATE			"date"
#define JSON_COL_BYTE			"byte"
/* 5 */
#define JSON_COL_SHORT			"short"
/* 7 */
#define JSON_COL_BOOLEAN		"boolean"
#define JSON_COL_INTEGER		"integer"
#define JSON_COL_KEYWORD		"keyword"


#define MSG_INV_SRV_ANS		"Invalid server answer"

#define ISO8601_TEMPLATE	"yyyy-mm-ddThh:mm:ss.sss+hh:mm"
#define TM_TO_TIMESTAMP_STRUCT(_tmp/*src*/, _tsp/*dst*/) \
	do { \
		(_tsp)->year = (_tmp)->tm_year + 1900; \
		(_tsp)->month = (_tmp)->tm_mon + 1; \
		(_tsp)->day = (_tmp)->tm_mday; \
		(_tsp)->hour = (_tmp)->tm_hour; \
		(_tsp)->minute = (_tmp)->tm_min; \
		(_tsp)->second = (_tmp)->tm_sec; \
	} while (0);


void clear_resultset(esodbc_stmt_st *stmt)
{
	DBGSTMT(stmt, "clearing result set; vrows=%zd, nrows=%zd, frows=%zd.", 
			stmt->rset.vrows, stmt->rset.nrows, stmt->rset.frows);
	if (stmt->rset.buff)
		free(stmt->rset.buff);
	if (stmt->rset.state)
		UJFree(stmt->rset.state);
	memset(&stmt->rset, 0, sizeof(stmt->rset));
}

static SQLSMALLINT type_elastic2csql(const wchar_t *type_name, size_t len)
{
	switch (len) {
		// TODO: take in the precision for a better
		// representation
		// case sizeof(JSON_COL_KEYWORD) - 1:
		// case sizeof(JSON_COL_BOOLEAN) - 1:
		case sizeof(JSON_COL_INTEGER) - 1:
			switch(tolower(type_name[0])) {
				case (wchar_t)'i': /* integer */
					if (wmemncasecmp(type_name, MK_WPTR(JSON_COL_INTEGER),
								len) == 0)
						return SQL_C_SLONG;
					break;
				case (wchar_t)'b': /* boolean */
					if (wmemncasecmp(type_name, MK_WPTR(JSON_COL_BOOLEAN),
								len) == 0)
						return SQL_C_UTINYINT;
					break;
				case (wchar_t)'k': /* keyword */
					if (wmemncasecmp(type_name, MK_WPTR(JSON_COL_KEYWORD),
								len) == 0)
						return SQL_C_CHAR;
					break;
			}
			break;
		// case sizeof(JSON_COL_DATE) - 1:
		case sizeof(JSON_COL_TEXT) - 1: 
			switch(tolower(type_name[0])) {
				case (wchar_t)'t':
					if (! wmemncasecmp(type_name, MK_WPTR(JSON_COL_TEXT), len))
						// TODO: char/longvarchar/wchar/wvarchar?
						return SQL_C_CHAR;
					break;
				case (wchar_t)'d':
					if (! wmemncasecmp(type_name, MK_WPTR(JSON_COL_DATE), len))
						// TODO: time/timestamp
						return SQL_C_TYPE_DATE;
					break;
				case (wchar_t)'b':
					if (! wmemncasecmp(type_name, MK_WPTR(JSON_COL_BYTE), len))
						return SQL_C_STINYINT;
					break;
#if 1 // BUG FIXME
				case (wchar_t)'n':
					if (! wmemncasecmp(type_name, MK_WPTR("null"), len))
						// TODO: time/timestamp
						return SQL_C_SSHORT;
					break;
#endif
			}
			break;
		case sizeof(JSON_COL_SHORT) - 1:
			if (! wmemncasecmp(type_name, MK_WPTR(JSON_COL_SHORT), len))
				// TODO: time/timestamp
				return SQL_C_SSHORT;
			break;
	}
	ERR("unrecognized Elastic type `" LWPDL "` (%zd).", len, type_name, len);
	return SQL_UNKNOWN_TYPE;
}

static void set_col_size(desc_rec_st *rec)
{
	switch (rec->concise_type) {
		/* .precision */
		// TODO: lifted from SYS TYPES: automate this?
		case SQL_C_SLONG: rec->precision = 19; break;
		case SQL_C_UTINYINT:
		case SQL_C_STINYINT: rec->precision = 3; break;
		case SQL_C_SSHORT: rec->precision = 5; break;

		/* .length */
		case SQL_C_CHAR: 
			rec->length = 256;  /*TODO: max TEXT size */
			break;

		case SQL_C_TYPE_DATE: 
			rec->length = sizeof(ISO8601_TEMPLATE)/*+\0*/;
			break;
		
		default:
			FIXME; // FIXME
	}
}

static void set_col_decdigits(desc_rec_st *rec)
{
	switch (rec->concise_type) {
		case SQL_C_SLONG:
		case SQL_C_UTINYINT:
		case SQL_C_STINYINT:
		case SQL_C_SSHORT:
			rec->scale = 0;
			break;

		case SQL_C_TYPE_DATE:
			rec->precision = 3; /* [seconds].xxx of ISO 8601 */
			break;
		
		case SQL_C_CHAR: break; /* n/a */

		default:
			FIXME; // FIXME
	}

}

static SQLRETURN attach_columns(esodbc_stmt_st *stmt, UJObject columns)
{
	desc_rec_st *rec;
	SQLRETURN ret;
	SQLSMALLINT recno;
	void *iter;
	UJObject col_o, name_o, type_o;
	const wchar_t *col_wname, *col_wtype;
	SQLSMALLINT col_stype;
	size_t len, ncols;
	
	esodbc_desc_st *ird = stmt->ird;
	wchar_t *keys[] = {
		MK_WPTR(JSON_ANSWER_COL_NAME),
		MK_WPTR(JSON_ANSWER_COL_TYPE)
	};


	ncols = UJArraySize(columns);
	DBGSTMT(stmt, "columns received: %zd.", ncols);
	ret = update_rec_count(ird, (SQLSMALLINT)ncols);
	if (! SQL_SUCCEEDED(ret)) {
		ERRSTMT(stmt, "failed to set IRD's record count to %d.", ncols);
		HDIAG_COPY(ird, stmt);
		return ret;
	}

	iter = UJBeginArray(columns);
	if (! iter) {
		ERRSTMT(stmt, "failed to obtain array iterator: %s.", 
				UJGetError(stmt->rset.state));
		RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
	}
	recno = 0;
	while (UJIterArray(&iter, &col_o)) {
		if (UJObjectUnpack(col_o, 2, "SS", keys, &name_o, &type_o) < 2) {
			ERRSTMT(stmt, "failed to decode JSON column: %s.", 
					UJGetError(stmt->rset.state));
			RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		}
		rec = &ird->recs[recno]; // +recno

		col_wname = UJReadString(name_o, &len);
		assert(sizeof(*col_wname) == sizeof(SQLWCHAR)); /* TODO: no ANSI */
		rec->name = len ? (SQLWCHAR *)col_wname : MK_WPTR("");

		col_wtype = UJReadString(type_o, &len);
		/* If the type is unknown, an empty string is returned." */
		rec->type_name = len ? (SQLWCHAR *)col_wtype : MK_WPTR("");
		/* 
		 * TODO: to ELASTIC types, rather?
		 * TODO: Read size (precision/lenght) and dec-dig(scale/precision)
		 * from received type.
		 */
		col_stype = type_elastic2csql(col_wtype, len);
		if (col_stype == SQL_UNKNOWN_TYPE) {
			ERRSTMT(stmt, "failed to convert Elastic to C SQL type `" 
					LWPDL "`.", len, col_wtype);
			RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		}
		rec->concise_type = col_stype;
		concise_to_type_code(col_stype, &rec->type, 
				&rec->datetime_interval_code);
		/* TODO: here it should be 'ird->type'. But we're setting C SQL types
		 * for IRD as well for now. */
		rec->meta_type = concise_to_meta(rec->concise_type, DESC_TYPE_ARD);

		set_col_size(rec);
		set_col_decdigits(rec);

		/* TODO: set all settable fields */

		/* "If a base column name does not exist (as in the case of columns
		 * that are expressions), then this variable contains an empty
		 * string." */
		rec->base_column_name = MK_WPTR("");
		/* "If a column does not have a label, the column name is returned. If
		 * the column is unlabeled and unnamed, an empty string is ret" */
		rec->label = rec->name ? rec->name : MK_WPTR("");
		rec->display_size = 256;

		assert(rec->name && rec->label);
		rec->unnamed = (rec->name[0] || rec->label[0]) ?
			SQL_NAMED : SQL_UNNAMED;

#ifndef NDEBUG
		//dump_record(rec);
#endif /* NDEBUG */

		DBGSTMT(stmt, "column #%d: name=`" LWPD "`, type=%d (`" LWPD "`).", 
				recno, col_wname, col_stype, col_wtype);
		recno ++;
	}

	return SQL_SUCCESS;
}


/*
 * Processes a received answer: 
 * - takes a dynamic buffer, buff, of length blen. Will handle the buff memory
 * even if the call fails.
 * - parses it, preparing iterators for SQLFetch()'ing.
 */
SQLRETURN attach_answer(esodbc_stmt_st *stmt, char *buff, size_t blen)
{
	int unpacked;
	UJObject obj, columns, rows, cursor;
	const wchar_t *wcurs;
	size_t eccnt;
	wchar_t *keys[] = {
		MK_WPTR(JSON_ANSWER_COLUMNS),
		MK_WPTR(JSON_ANSWER_ROWS),
		MK_WPTR(JSON_ANSWER_CURSOR) 
	};

	/* clear any previous result set */
	if (STMT_HAS_RESULTSET(stmt))
		clear_resultset(stmt);

	/* the statement takes ownership of mem obj */
	stmt->rset.buff = buff;
	stmt->rset.blen = blen;

	/* parse the entire JSON answer */
	obj = UJDecode(buff, blen, NULL, &stmt->rset.state);
	if (! obj) {
		ERRSTMT(stmt, "failed to decode JSON answer (`%.*s`): %s.", blen, buff,
				stmt->rset.state ? UJGetError(stmt->rset.state) : "<none>");
		RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
	}
	columns = rows = cursor = NULL;
	/* extract the columns and rows objects */
	unpacked = UJObjectUnpack(obj, 3, "AAS", keys, &columns, &rows, &cursor);
    if (unpacked < /* 'rows' must always be present */1) {
		ERRSTMT(stmt, "failed to unpack JSON answer (`%.*s`): %s.", 
				blen, buff, UJGetError(stmt->rset.state));
		RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
	}

	/*
	 * set the internal cursor (UJSON4C array iterator) 
	 */
	if (! rows) {
		ERRSTMT(stmt, "no rows JSON object received in answer: `%.*s`[%zd].",
				blen, buff, blen);
		RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
	}
	stmt->rset.rows_iter = UJBeginArray(rows);
	if (! stmt->rset.rows_iter) {
#if 0 /* UJSON4C will return NULL above, for empty array (meh!) */
		ERRSTMT(stmt, "failed to get iterrator on received rows: %s.", 
				UJGetError(stmt->rset.state));
		RET_HDIAGS(stmt, SQL_STATE_HY000);
#else /*0*/
		DBGSTMT(stmt, "received empty resultset array: forcing nodata.");
		STMT_FORCE_NODATA(stmt);
		stmt->rset.nrows = 0;
#endif /*0*/
	} else {
		stmt->rset.nrows = (size_t)UJArraySize(rows);
	}
	DBGSTMT(stmt, "rows received in result set: %zd.", stmt->rset.nrows);

	/*
	 * copy Elastic's cursor (if there's one)
	 */
	if (cursor) {
		wcurs = UJReadString(cursor, &eccnt);
		if (eccnt) {
			/* this can happen automatically if hitting scroller size */
			if (! stmt->dbc->fetch.max)
				INFO("STMT@0x%p: no fetch size defined, but cursor returned.");
			if (stmt->rset.ecurs)
				DBGSTMT(stmt, "replacing old cursor `" LWPDL "`.", 
						stmt->rset.eccnt, stmt->rset.ecurs);
			/* store new cursor vals */
			stmt->rset.ecurs = wcurs;
			stmt->rset.eccnt = eccnt;
			DBGSTMT(stmt, "new elastic cursor: `" LWPDL "`[%zd].", 
					stmt->rset.eccnt, stmt->rset.ecurs, stmt->rset.eccnt);
		} else {
			WARNSTMT(stmt, "empty cursor found in the answer.");
		}
	} else {
		/* should have been cleared by now */
		assert(! stmt->rset.eccnt);
	}

	/*
	 * process the sent columns, if any.
	 */
	if (columns) {
		if (0 < stmt->ird->count) {
			ERRSTMT(stmt, "%d columns already attached.", stmt->ird->count);
			RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		}
		return attach_columns(stmt, columns);
	} else {
		/* no cols available in this answer: check if already received */
		if (stmt->ird->count <= 0) {
			ERRSTMT(stmt, "no columns available in result set; answer: "
					"`%.*s`[%zd].", blen, buff, blen);
			RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		}
	}

	return SQL_SUCCESS;
}

/*
 * Parse an error and push it as statement diagnostic.
 */
SQLRETURN attach_error(esodbc_stmt_st *stmt, char *buff, size_t blen)
{
	UJObject obj, o_status, o_error, o_type, o_reason;
	const wchar_t *wtype, *wreason;
	size_t tlen, rlen, left;
	wchar_t wbuf[sizeof(((esodbc_diag_st*)NULL)->text) / 
		sizeof(*((esodbc_diag_st*)NULL)->text)];
	size_t wbuflen = sizeof(wbuf)/sizeof(*wbuf);
	int n;
	void *state = NULL;
	wchar_t *outer_keys[] = {
		MK_WPTR(JSON_ANSWER_ERROR),
		MK_WPTR(JSON_ANSWER_STATUS)
	};
	wchar_t *err_keys[] = {
		MK_WPTR(JSON_ANSWER_ERR_TYPE),
		MK_WPTR(JSON_ANSWER_ERR_REASON)
	};

	INFO("STMT@0x%p REST request failed with `%.*s` (%zd).", stmt, blen, buff, 
			blen);

	/* parse the entire JSON answer */
	obj = UJDecode(buff, blen, NULL, &state);
	if (! obj) {
		ERRSTMT(stmt, "failed to decode JSON answer (`%.*s`): %s.", 
				blen, buff, state ? UJGetError(state) : "<none>");
		SET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		goto end;
	}
	/* extract the status and error object */
	if (UJObjectUnpack(obj, 2, "ON", outer_keys, &o_error, &o_status) < 2) {
		ERRSTMT(stmt, "failed to unpack JSON answer (`%.*s`): %s.", 
				blen, buff, UJGetError(state));
		SET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		goto end;
	}
	/* unpack error object */
	if (UJObjectUnpack(o_error, 2, "SS", err_keys, &o_type, &o_reason) < 2) {
		ERRSTMT(stmt, "failed to unpack error object (`%.*s`): %s.", 
				blen, buff, UJGetError(state));
		SET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		goto end;
	}

	wtype = UJReadString(o_type, &tlen);
	wreason = UJReadString(o_reason, &rlen);
	/* these return empty string in case of mismatch */
	assert(wtype && wreason);
	DBGSTMT(stmt, "server failures: type: [%zd] `" LWPDL "`, reason: [%zd] `" 
			LWPDL "`, status: %d.", tlen, tlen, wtype, rlen, rlen, wreason, 
			UJNumericInt(o_status));

	/* swprintf will fail if formated string would overrun the buffer size (as
	 * opposed to write up to its limit) => find out the limit first.*/
	n = swprintf(NULL, 0, MK_WPTR("%.*s: %.*s"), (int)tlen, wtype, (int)rlen,
			wreason);
	if (0 < n) {
		wbuflen -= /* ": " */2 + /*\0*/1;
		tlen = wbuflen < tlen ? wbuflen : tlen;
		left = wbuflen - tlen;
		rlen = left < rlen ? left : rlen;
		wbuflen += /* ": " */2 + /*\0*/1;
		/* swprintf will add the 0-term (or fail, if it can't) */
		n = swprintf(wbuf, wbuflen, MK_WPTR("%.*s: %.*s"), (int)tlen, wtype,
				(int)rlen, wreason);
	}
	if (n < 0) {
		ERRN("failed to print error message from server.");
		assert(sizeof(MSG_INV_SRV_ANS) < sizeof(wbuf));
		memcpy(wbuf, MK_WPTR(MSG_INV_SRV_ANS),
				sizeof(MSG_INV_SRV_ANS)*sizeof(SQLWCHAR));
	}

	post_diagnostic(&stmt->diag, SQL_STATE_HY000, wbuf, 
			UJNumericInt(o_status));

end:
	if (state)
		UJFree(state);
	if (buff)
		free(buff);

	RET_STATE(stmt->diag.state);
}

SQLRETURN attach_sql(esodbc_stmt_st *stmt, 
		const SQLWCHAR *sql, /* SQL text statement */
		size_t sqlcnt /* count of chars of 'sql' */)
{
	char *u8;
	int len;

	DBGSTMT(stmt, "attaching SQL `" LWPDL "` (%zd).", sqlcnt, sql, sqlcnt);
#if 0 // FIXME
	if (wcslen(sql) < 1256) {
		if (wcsstr(sql, L"FROM test_emp")) {
			sql = L"SELECT emp_no, first_name, last_name, birth_date, 2+3 AS foo FROM test_emp";
			sqlcnt = wcslen(sql);
			DBGSTMT(stmt, "RE-attaching SQL `" LWPDL "` (%zd).", sqlcnt,
					sql, sqlcnt);
		}
	}
#endif

	assert(! stmt->u8sql);

	len = WCS2U8(sql, (int)sqlcnt, NULL, 0);
	if (len <= 0) {
		ERRN("STMT@0x%p: failed to UCS2/UTF8 convert SQL `" LWPDL "` (%zd).", 
				stmt, sqlcnt, sql, sqlcnt);
		RET_HDIAG(stmt, SQL_STATE_HY000, "UCS2/UTF8 conversion failure", 0);
	}
	DBGSTMT(stmt, "wide char SQL `" LWPDL "`[%zd] converts to UTF8 on %d "
			"octets.", sqlcnt, sql, sqlcnt, len);

	u8 = malloc(len);
	if (! u8) {
		ERRN("failed to alloc %dB.", len);
		RET_HDIAGS(stmt, SQL_STATE_HY001);
	}

	len = WCS2U8(sql, (int)sqlcnt, u8, len);
	if (len <= 0) { /* can it happen? it's just succeded above */
		ERRN("STMT@0x%p: failed to UCS2/UTF8 convert SQL `" LWPDL "` (%zd).", 
				stmt, sqlcnt, sql, sqlcnt);
		free(u8);
		RET_HDIAG(stmt, SQL_STATE_HY000, "UCS2/UTF8 conversion failure(2)", 0);
	}

	stmt->u8sql = u8;
	stmt->sqllen = (size_t)len;
	
	DBGSTMT(stmt, "attached SQL `%.*s` (%zd).", len, u8, len);

	return SQL_SUCCESS;
}

void detach_sql(esodbc_stmt_st *stmt)
{
	if (! stmt->u8sql)
		return;
	free(stmt->u8sql);
	stmt->u8sql = NULL;
	stmt->sqllen = 0;
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

	if (BufferLength < 0) {
		ERRSTMT(stmt, "invalid negative BufferLength: %d.", BufferLength);
		RET_HDIAGS(STMH(StatementHandle), SQL_STATE_HY090);
	}

	if ((STMH(StatementHandle)->bookmarks != SQL_UB_OFF) || (! ColumnNumber)) {
		/* "The statement attribute SQL_ATTR_USE_BOOKMARKS should always be
		 * set before binding a column to column 0. This is not required but
		 * is strongly recommended." */
		//RET_HDIAGS(STMH(StatementHandle), SQL_STATE_IM001);
		/* TODO: implement bookmarks? */
		FIXME;
	}

	/* "if the value in the ColumnNumber argument exceeds the value of
	 * SQL_DESC_COUNT, calls SQLSetDescField to increase the value of
	 * SQL_DESC_COUNT to ColumnNumber." */
	if (ard->count < ColumnNumber) {
		ret = EsSQLSetDescFieldW(ard, NO_REC_NR, SQL_DESC_COUNT,
				(SQLPOINTER)(uintptr_t)ColumnNumber, SQL_IS_SMALLINT);
		if (ret != SQL_SUCCESS)
			goto copy_ret;
	}

	/* set concise type (or verbose for datetime/interval types) */
	ret = EsSQLSetDescFieldW(ard, ColumnNumber, SQL_DESC_CONCISE_TYPE,
			(SQLPOINTER)(intptr_t)TargetType, SQL_IS_SMALLINT);
	if (ret != SQL_SUCCESS) 
		goto copy_ret;

	 // TODO: "Sets one or more of SQL_DESC_LENGTH, SQL_DESC_PRECISION,
	 // SQL_DESC_SCALE, and SQL_DESC_DATETIME_INTERVAL_PRECISION, as
	 // appropriate for TargetType."
	 // TODO: Cautions Regarding SQL_DEFAULT

	/* "Sets the SQL_DESC_OCTET_LENGTH field to the value of BufferLength." */
	ret = EsSQLSetDescFieldW(ard, ColumnNumber, SQL_DESC_OCTET_LENGTH,
			(SQLPOINTER)(intptr_t)BufferLength, SQL_IS_INTEGER);
	if (ret != SQL_SUCCESS)
		goto copy_ret;

	/* Sets the SQL_DESC_INDICATOR_PTR field to the value of StrLen_or_Ind" */
	ret = EsSQLSetDescFieldW(ard, ColumnNumber, SQL_DESC_INDICATOR_PTR,
			StrLen_or_Ind, 
			SQL_LEN_BINARY_ATTR((SQLINTEGER)sizeof(StrLen_or_Ind)));
	if (ret != SQL_SUCCESS)
		goto copy_ret;

	/* "Sets the SQL_DESC_OCTET_LENGTH_PTR field to the value of
	 * StrLen_or_Ind." */
	ret = EsSQLSetDescFieldW(ard, ColumnNumber, SQL_DESC_OCTET_LENGTH_PTR,
			StrLen_or_Ind, 
			SQL_LEN_BINARY_ATTR((SQLINTEGER)sizeof(StrLen_or_Ind)));
	if (ret != SQL_SUCCESS)
		goto copy_ret;

	/* "Sets the SQL_DESC_DATA_PTR field to the value of TargetValue."
	 * Note: needs to be last set field, as setting other fields unbind. */
	ret = EsSQLSetDescFieldW(ard, ColumnNumber, SQL_DESC_DATA_PTR,
			TargetValue, SQL_IS_POINTER);
	if (ret != SQL_SUCCESS)
		goto copy_ret;

	return SQL_SUCCESS;

copy_ret:
	/* copy error at top handle level, where it's going to be inquired from */
	HDIAG_COPY(ard, stmt);
	return ret;
}

/*
 * field: SQL_DESC_: DATA_PTR / INDICATOR_PTR / OCTET_LENGTH_PTR
 * pos: position in array/row_set (not result_set)
 */
static void* deferred_address(SQLSMALLINT field_id, size_t pos,
		desc_rec_st *rec)
{
	size_t elem_size;
	SQLLEN offt;
	void *base;
	esodbc_desc_st *desc = rec->desc;

#define ROW_OFFSETS \
	do { \
		elem_size = desc->bind_type; \
		offt = desc->bind_offset_ptr ? *(desc->bind_offset_ptr) : 0; \
	} while (0)

	switch (field_id) {
		case SQL_DESC_DATA_PTR: 
			base = rec->data_ptr;
			if (desc->bind_type == SQL_BIND_BY_COLUMN) {
				elem_size = (size_t)rec->octet_length;
				offt = 0;
			} else { /* by row */
				ROW_OFFSETS;
			}
			break;
		case SQL_DESC_INDICATOR_PTR: 
			base = rec->indicator_ptr; 
			if (desc->bind_type == SQL_BIND_BY_COLUMN) {
				elem_size = sizeof(*rec->indicator_ptr);
				offt = 0;
			} else { /* by row */
				ROW_OFFSETS;
			}
			break;
		case SQL_DESC_OCTET_LENGTH_PTR: 
			base = rec->octet_length_ptr; 
			if (desc->bind_type == SQL_BIND_BY_COLUMN) {
				elem_size = sizeof(*rec->octet_length_ptr);
				offt = 0;
			} else { /* by row */
				ROW_OFFSETS;
			}
			break;
		default:
			BUG("can't calculate the deferred address of field type %d.",
					field_id);
			return NULL;
	}
#undef ROW_OFFSETS

	DBG("rec@0x%p, field_id:%d : base@0x%p, offset=%d, elem_size=%zd", 
			rec, field_id, base, offt, elem_size);

	return base ? (char *)base + offt + pos * elem_size : NULL;
}

/*
 * Handles the lenghts of the data to copy out to the application:
 * (1) returns the max amount of bytes to copy (in the data_ptr), taking into
 *     account:
 *     - the bytes of the data 'avail',
 *     - buffer size 'room',
 *     - statement attribute SQL_ATTR_MAX_LENGTH 'attr_max' and
 *     - required byte alignment (potentially to wchar_t size);
 * (2) indicates if truncation occured into 'state'.
 * Only to be used with ARD.meta_type == STR || BIN (as it can truncate).
 */
static size_t buff_octet_size(
		size_t avail, size_t room, size_t attr_max, size_t unit_size,
		esodbc_metatype_et ird_mt, esodbc_state_et *state)
{
	size_t max_copy, max;

	/* truncate to statment max bytes, only if "the column contains character
	 * or binary data" */
	max = (ird_mt == METATYPE_STRING || ird_mt == METATYPE_BIN) ? attr_max : 0;

	/* apply "network" truncation first, if need to */
	if (0 < max && max < avail) {
		INFO("applying 'network' truncation %zd -> %zd.", avail, max);
		max_copy = max;
		/* no truncation indicated for this case */
	} else {
		max_copy = avail;
	}

	/* is target buffer to small? adjust size if so and indicate truncation */
	/* Note: this should only be tested/applied if ARD.meta_type == STR||BIN */
	if (room < max_copy) {
		INFO("applying buffer truncation %zd -> %zd.", max_copy, room);
		max_copy = room;
		*state = SQL_STATE_01004;
	}

	/* adjust to align to target buffer unit */
	if (max_copy % unit_size)
		max_copy -= max_copy % unit_size;

	DBG("avail=%zd, room=%zd, attr_max=%zd, metatype:%d => "
			"max_copy=%zd, state=%d.", 
			avail, room, attr_max, ird_mt, max_copy, *state);
	return max_copy;
}

static inline void write_copied_octets(SQLLEN *octet_len_ptr, size_t copied, 
		size_t attr_max, esodbc_metatype_et ird_mt)
{
	size_t max;

	if (! octet_len_ptr) {
		DBG("NULL octet len pointer, length (%zd) not indicated.", copied);
		return;
	}

	/* truncate to statment max bytes, only if "the column contains character
	 * or binary data" */
	max = (ird_mt == METATYPE_STRING || ird_mt == METATYPE_BIN) ? attr_max : 0;

	if (0 < max)
		/* put the value of SQL_ATTR_MAX_LENGTH attribute..  even
		 * if this would be larger than what the data actually
		 * occupies after conversion: "the driver has no way of
		 * figuring out what the actual length is" */
		*octet_len_ptr = max;
	else
		/* if no "network" truncation done, indicate data's lenght, no
		 * matter if truncated to buffer's size or not */
		*octet_len_ptr = copied;
}

static SQLRETURN copy_longlong(desc_rec_st *arec, desc_rec_st *irec,
		SQLULEN pos, long long ll)
{
	esodbc_stmt_st *stmt;
	void *data_ptr;
	SQLLEN *octet_len_ptr;
	esodbc_desc_st *ard, *ird;
	SQLSMALLINT target_type;
	char buff[sizeof("18446744073709551616")]; /* = 1 << 8*8 */
	SQLWCHAR wbuff[sizeof("18446744073709551616")]; /* = 1 << 8*8 */
	size_t tocopy, blen;
	esodbc_state_et state = SQL_STATE_00000;

	stmt = arec->desc->stmt;
	ird = stmt->ird;
	ard = stmt->ard;

	/* pointer where to write how many characters we will/would use */
	octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR, pos, arec);
	/* pointer to app's buffer */
	data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, arec);
	if (! data_ptr) {
		ERRSTMT(stmt, "received numeric type, but NULL data ptr.");
		RET_HDIAGS(stmt, SQL_STATE_HY009);
	}

	/* "To use the default mapping, an application specifies the SQL_C_DEFAULT
	 * type identifier." */
	target_type = arec->type == SQL_C_DEFAULT ? irec->type : arec->type;
	DBG("target data type: %d.", target_type);
	switch (target_type) {
		case SQL_C_CHAR:
			_i64toa((int64_t)ll, buff, /*radix*/10);
			/* TODO: find/write a function that returns len of conversion? */
			blen = strlen(buff) + /* \0 */1;
			tocopy = buff_octet_size(blen, arec->octet_length,
					stmt->max_length, sizeof(*buff), irec->meta_type, &state);
			memcpy(data_ptr, buff, tocopy);
			write_copied_octets(octet_len_ptr, blen, stmt->max_length, 
					irec->meta_type);
			break;
		case SQL_C_WCHAR:
			_i64tow((int64_t)ll, wbuff, /*radix*/10);
			/* TODO: find/write a function that returns len of conversion? */
			blen = (wcslen(wbuff) + /* \0 */1) * sizeof(*wbuff);
			tocopy = buff_octet_size(blen, arec->octet_length, 
					stmt->max_length, sizeof(*wbuff), irec->meta_type, &state);
			memcpy(data_ptr, wbuff, tocopy);
			write_copied_octets(octet_len_ptr, blen, stmt->max_length, 
					irec->meta_type);
			break;

		case SQL_C_SLONG:
		case SQL_C_SSHORT:
			*(SQLINTEGER *)data_ptr = (SQLINTEGER)ll;
			write_copied_octets(octet_len_ptr, sizeof(SQLINTEGER), 
					stmt->max_length, irec->meta_type);
			break;
		
		case SQL_C_SBIGINT:
			*(SQLBIGINT *)data_ptr = (SQLBIGINT)ll;
			write_copied_octets(octet_len_ptr, sizeof(SQLBIGINT), 
					stmt->max_length, irec->meta_type);
			break;

		default:
			FIXME; // FIXME
			return SQL_ERROR;
	}
	DBG("REC@0x%p, data_ptr@0x%p, copied long long: `%d`.", arec, data_ptr,
			(SQLINTEGER)ll);

	RET_STATE(state);
}

static SQLRETURN copy_double(desc_rec_st *arec, desc_rec_st *irec, 
		SQLULEN pos, double dbl)
{
	FIXME; // FIXME
	return SQL_ERROR;
}

/*
 * -> SQL_C_CHAR
 */
static SQLRETURN wstr_to_cstr(desc_rec_st *arec, desc_rec_st *irec, 
		void *data_ptr, SQLLEN *octet_len_ptr,
		const wchar_t *wstr, size_t chars)
{
	esodbc_state_et state = SQL_STATE_00000;
	esodbc_stmt_st *stmt = arec->desc->stmt;
	char *charp;
	int in_bytes, out_bytes, c;

	if (data_ptr) {
		charp = (char *)data_ptr;

		/* type is signed, driver should not allow a negative to this point. */
		assert(0 <= arec->octet_length);
		in_bytes = (int)buff_octet_size(chars * sizeof(*wstr),
				(size_t)arec->octet_length, stmt->max_length, sizeof(*charp),
				irec->meta_type, &state);
		/* trim the original string until it fits in output buffer, with given
		 * length limitation */
		for (c = (int)chars; 0 < c; c --) {
			out_bytes = WCS2U8(wstr, c, charp, in_bytes);
			if (out_bytes <= 0) {
				if (WCS2U8_BUFF_INSUFFICIENT)
					continue;
				ERRN("failed to convert wchar* to char* for string `" LWPDL 
						"`.", chars, wstr);
				RET_HDIAGS(stmt, SQL_STATE_22018);
			} else {
				/* conversion succeeded */
				break;
			}
		}

		if (charp[out_bytes - 1]) {
			/* ran out of buffer => not 0-terminated and truncated already */
			charp[out_bytes - 1] = 0;
			state = SQL_STATE_01004; /* indicate truncation */
		}

		DBG("REC@0x%p, data_ptr@0x%p, copied %zd bytes: `" LWPD "`.", arec,
				data_ptr, out_bytes, charp);
	} else {
		DBG("REC@0x%p, NULL data_ptr.", arec);
	}

	/* if length needs to be given, calculate  it not truncated & converted */
	if (octet_len_ptr) {
		out_bytes = (size_t)WCS2U8(wstr, (int)chars, NULL, 0);
		if (out_bytes <= 0) {
			ERRN("failed to convert wchar* to char* for string `" LWPDL "`.", 
					chars, wstr);
			RET_HDIAGS(stmt, SQL_STATE_22018);
		}
		write_copied_octets(octet_len_ptr, out_bytes, stmt->max_length,
				irec->meta_type);
	} else {
		DBG("REC@0x%p, NULL octet_len_ptr.", arec);
	}

	if (state != SQL_STATE_00000)
		RET_HDIAGS(stmt, state);
	return SQL_SUCCESS;
}

/*
 * -> SQL_C_WCHAR
 */
static SQLRETURN wstr_to_wstr(desc_rec_st *arec, desc_rec_st *irec, 
		void *data_ptr, SQLLEN *octet_len_ptr,
		const wchar_t *wstr, size_t chars)
{
	esodbc_state_et state = SQL_STATE_00000;
	esodbc_stmt_st *stmt = arec->desc->stmt;
	size_t in_bytes, out_bytes;
	wchar_t *widep;


	if (data_ptr) {
		widep = (wchar_t *)data_ptr;
		
		assert(0 <= arec->octet_length);
		in_bytes = buff_octet_size(chars * sizeof(*wstr),
				(size_t)arec->octet_length, stmt->max_length, sizeof(*wstr),
				irec->meta_type, &state);

		memcpy(widep, wstr, in_bytes);
		out_bytes = in_bytes / sizeof(*widep);
		/* if buffer too small to accomodate original, 0-term it */
		if (widep[out_bytes - 1]) {
			widep[out_bytes - 1] = 0;
			state = SQL_STATE_01004; /* indicate truncation */
		}

		DBG("REC@0x%p, data_ptr@0x%p, copied %zd bytes: `" LWPD "`.", arec,
				data_ptr, out_bytes, widep);
	} else {
		DBG("REC@0x%p, NULL data_ptr", arec);
	}
	
	write_copied_octets(octet_len_ptr, chars * sizeof(*wstr), stmt->max_length,
			irec->meta_type);
	
	if (state != SQL_STATE_00000)
		RET_HDIAGS(stmt, state);
	return SQL_SUCCESS;
}


static BOOL wstr_to_timestamp_struct(const wchar_t *wstr, size_t chars,
		TIMESTAMP_STRUCT *tss)
{
	char buff[sizeof(ISO8601_TEMPLATE)/*+\0*/];
	int len;
	timestamp_t tsp;
	struct tm tmp;

	DBG("converting ISO 8601 `" LWPDL "` to timestamp.", chars, wstr);

	len = (int)(chars < sizeof(buff) - 1 ? chars : sizeof(buff) - 1);
	len = ansi_w2c(wstr, buff, len);
	if (len <= 0 || timestamp_parse(buff, len - 1, &tsp) || 
			(! timestamp_to_tm_local(&tsp, &tmp))) {
		ERR("data `" LWPDL "` not an ANSI ISO 8601 format.", chars, wstr);
		return FALSE;
	}
	TM_TO_TIMESTAMP_STRUCT(&tmp, tss);
	tss->fraction = tsp.nsec / 1000000;

	DBG("parsed ISO 8601: `%d-%d-%dT%d:%d:%d.%u+%dm`.", 
			tss->year, tss->month, tss->day, 
			tss->hour, tss->minute, tss->second, tss->fraction, 
			tsp.offset);

	return TRUE;
}


/*
 * -> SQL_C_TYPE_TIMESTAMP
 */
static SQLRETURN wstr_to_timestamp(desc_rec_st *arec, desc_rec_st *irec, 
		void *data_ptr, SQLLEN *octet_len_ptr,
		const wchar_t *wstr, size_t chars)
{
	esodbc_stmt_st *stmt = arec->desc->stmt;
	TIMESTAMP_STRUCT *tss = (TIMESTAMP_STRUCT *)data_ptr;

	if (octet_len_ptr) {
		*octet_len_ptr = sizeof(*tss);
	}

	if (data_ptr) {
		if (! wstr_to_timestamp_struct(wstr, chars, tss))
			RET_HDIAGS(stmt, SQL_STATE_07006);
	} else {
		DBG("REC@0x%p, NULL data_ptr", arec);
	}

	return SQL_SUCCESS;
}

/*
 * -> SQL_C_TYPE_DATE
 */
static SQLRETURN wstr_to_date(desc_rec_st *arec, desc_rec_st *irec, 
		void *data_ptr, SQLLEN *octet_len_ptr,
		const wchar_t *wstr, size_t chars)
{
	esodbc_stmt_st *stmt = arec->desc->stmt;
	DATE_STRUCT *ds = (DATE_STRUCT *)data_ptr;
	TIMESTAMP_STRUCT tss;

	if (octet_len_ptr) {
		*octet_len_ptr = sizeof(*ds);
	}

	if (data_ptr) {
		if (! wstr_to_timestamp_struct(wstr, chars, &tss))
			RET_HDIAGS(stmt, SQL_STATE_07006);
		ds->year = tss.year;
		ds->month = tss.month;
		ds->day = tss.day;
	} else {
		DBG("REC@0x%p, NULL data_ptr", arec);
	}

	return SQL_SUCCESS;
}

/*
 * wstr: is 0-terminated, terminator is counted in 'chars'.
 */
static SQLRETURN copy_string(desc_rec_st *arec, desc_rec_st *irec, 
		SQLULEN pos, const wchar_t *wstr, size_t chars)
{
	esodbc_stmt_st *stmt;
	void *data_ptr;
	SQLLEN *octet_len_ptr;
	esodbc_desc_st *ard, *ird;
	SQLSMALLINT target_type;

	stmt = arec->desc->stmt;
	ird = stmt->ird;
	ard = stmt->ard;

	/* pointer where to write how many characters we will/would use */
	octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR, pos, arec);
	/* pointer to app's buffer */
	data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, arec);

	/* "To use the default mapping, an application specifies the SQL_C_DEFAULT
	 * type identifier." */
	target_type = arec->type == SQL_C_DEFAULT ? irec->type : arec->type;
	DBG("target data type: %d.", target_type);
	switch (target_type) {
		case SQL_C_CHAR:
			return wstr_to_cstr(arec, irec, data_ptr, octet_len_ptr, 
					wstr, chars);
		case SQL_C_WCHAR:
			return wstr_to_wstr(arec, irec, data_ptr, octet_len_ptr, 
					wstr, chars);
		case SQL_C_TYPE_TIMESTAMP:
			return wstr_to_timestamp(arec, irec, data_ptr, octet_len_ptr, 
					wstr, chars);
		case SQL_C_TYPE_DATE:
			return wstr_to_date(arec, irec, data_ptr, octet_len_ptr, 
					wstr, chars);

		default:
			// FIXME: convert data
			FIXME;
	}

	return SQL_SUCCESS;
}

/*
 * Copy one row from IRD to ARD.
 * pos: row number in the rowset
 * Returns: ...
 */
static SQLRETURN copy_one_row(esodbc_stmt_st *stmt, SQLULEN pos, UJObject row)
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
	size_t len;
	BOOL with_info;
	esodbc_desc_st *ard, *ird;
	desc_rec_st* arec, *irec;

	rowno = stmt->rset.frows + pos + /*1-based*/1;
	ard = stmt->ard;
	ird = stmt->ird;

#define RET_ROW_DIAG(_state, _message, _colno) \
	do { \
		if (ard->array_status_ptr) \
			ard->array_status_ptr[pos] = SQL_ROW_ERROR; \
		return post_row_diagnostic(&stmt->diag, _state, MK_WPTR(_message), 0, \
				rowno, _colno); \
	} while (0)
#define SET_ROW_DIAG(_rowno, _colno) \
	do { \
		stmt->diag.row_number = _rowno; \
		stmt->diag.column_number = _colno; \
	} while (0)

	if (! UJIsArray(row)) {
		ERRSTMT(stmt, "one '%s' (#%zd) element in result set not array; type:"
				" %d.", JSON_ANSWER_ROWS, stmt->rset.vrows, UJGetType(row));
		RET_ROW_DIAG(SQL_STATE_01S01, MSG_INV_SRV_ANS, 
				SQL_NO_COLUMN_NUMBER);
	}
	iter_row = UJBeginArray(row);
	if (! iter_row) {
		ERRSTMT(stmt, "Failed to obtain iterator on row (#%zd): %s.", rowno,
				UJGetError(stmt->rset.state));
		RET_ROW_DIAG(SQL_STATE_01S01, MSG_INV_SRV_ANS, 
				SQL_NO_COLUMN_NUMBER);
	}

	with_info = FALSE;
	/* iterate over the contents of one table row */
	for (i = 0; i < ard->count && UJIterArray(&iter_row, &obj); i ++) {
		/* if record not bound skip it */
		if (! REC_IS_BOUND(&ard->recs[i])) {
			DBG("column #%d not bound, skipping it.", i + 1);
			continue;
		}
		
		arec = get_record(ard, i + 1, FALSE);
		irec = get_record(ird, i + 1, FALSE);
		assert(arec);
		assert(irec);
		
		switch (UJGetType(obj)) {
			case UJT_Null:
				DBG("value [%zd, %d] is NULL.", rowno, i + 1);
#if 0
				if (! arec->nullable) {
					ERRSTMT(stmt, "received a NULL for a not nullable val.");
					RET_ROW_DIAG(SQL_STATE_HY003, "NULL value received for non"
							" nullable data type", i + 1);
				}
#endif //0
				ind_len = deferred_address(SQL_DESC_INDICATOR_PTR, pos, arec);
				if (! ind_len) {
					ERRSTMT(stmt, "no buffer to signal NULL value.");
					RET_ROW_DIAG(SQL_STATE_22002, "Indicator variable required"
							" but not supplied", i + 1);
				}
				*ind_len = SQL_NULL_DATA;
				break;

			case UJT_String:
				wstr = UJReadString(obj, &len);
				DBG("value [%zd, %d] is string: `" LWPD "`.", rowno, i + 1,
						wstr);
				/* UJSON4C returns chars count, but 0-terminates w/o counting
				 * the terminator */
				assert(wstr[len] == 0); 
				ret = copy_string(arec, irec, pos, wstr, len + /*\0*/1);
				switch (ret) {
					case SQL_SUCCESS_WITH_INFO: 
						with_info = TRUE;
						SET_ROW_DIAG(rowno, i + 1);
					case SQL_SUCCESS: 
						break;
					default: 
						SET_ROW_DIAG(rowno, i + 1);
						return ret;
				}
				break;

			case UJT_Long:
			case UJT_LongLong:
				ll = UJNumericLongLong(obj);
				DBG("value [%zd, %d] is numeric: %lld.", rowno, i + 1, ll);
				ret = copy_longlong(arec, irec, pos, ll);
				switch (ret) {
					case SQL_SUCCESS_WITH_INFO: 
						with_info = TRUE;
						SET_ROW_DIAG(rowno, i + 1);
					case SQL_SUCCESS: 
						break;
					default: 
						SET_ROW_DIAG(rowno, i + 1);
						return ret;
				}
				break;

			case UJT_Double:
				dbl = UJNumericFloat(obj);
				DBG("value [%zd, %d] is double: %f.", rowno, i + 1, dbl);
				ret = copy_double(arec, irec, pos, dbl);
				switch (ret) {
					case SQL_SUCCESS_WITH_INFO: 
						with_info = TRUE;
						SET_ROW_DIAG(rowno, i + 1);
					case SQL_SUCCESS: 
						break;
					default: 
						SET_ROW_DIAG(rowno, i + 1);
						return ret;
				}
				break;

			/* TODO: convert to 1/0? */
			case UJT_True:
			case UJT_False:
			default:
				ERR("unexpected object of type %d in row L#%zd/T#%zd.",
						UJGetType(obj), stmt->rset.vrows, stmt->rset.frows);
				RET_ROW_DIAG(SQL_STATE_01S01, MSG_INV_SRV_ANS, i + 1);
		}
	}

	if (ard->array_status_ptr)
		ard->array_status_ptr[pos] = with_info ? SQL_ROW_SUCCESS_WITH_INFO : 
			SQL_ROW_SUCCESS;

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
	SQLULEN i, j;
	UJObject row;
	SQLRETURN ret;
	int errors;

	stmt = STMH(StatementHandle);
	ard = stmt->ard;
	ird = stmt->ird;

	if (! STMT_HAS_RESULTSET(stmt)) {
		if (STMT_NODATA_FORCED(stmt)) {
			DBGSTMT(stmt, "empty result set flag set - returning no data.");
			return SQL_NO_DATA;
		}
		ERRSTMT(stmt, "no resultset available on statement.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	DBGSTMT(stmt, "(`%.*s`); cursor @ %zd / %zd.", stmt->sqllen,
			stmt->u8sql, stmt->rset.vrows, stmt->rset.nrows);
	
	DBGSTMT(stmt, "rowset max size: %d.", ard->array_size);
	errors = 0;
	/* for all rows in rowset/array, iterate over rows in current resultset */
	for (i = stmt->rset.array_pos; i < ard->array_size; i ++) {
		if (! UJIterArray(&stmt->rset.rows_iter, &row)) {
			DBGSTMT(stmt, "ran out of rows in current result set: nrows=%zd, "
					"vrows=%zd.", stmt->rset.nrows, stmt->rset.vrows);
			if (stmt->rset.eccnt) { /*do I have an Elastic cursor? */
				stmt->rset.array_pos = i;
				ret = post_statement(stmt);
				if (! SQL_SUCCEEDED(ret)) {
					ERRSTMT(stmt, "failed to fetch next results.");
					return ret;
				}
				return EsSQLFetch(StatementHandle);
			} else {
				DBGSTMT(stmt, "reached end of entire result set. fetched=%zd.",
						stmt->rset.frows);
				/* indicate the non-processed rows in rowset */
				if (ard->array_status_ptr)
					for (j = i; j < ard->array_size; j ++)
						ard->array_status_ptr[j] = SQL_ROW_NOROW;
			}
			break;
		}
		ret = copy_one_row(stmt, i, row);
		if (! SQL_SUCCEEDED(ret)) {
			ERRSTMT(stmt, "copying row %zd failed.", stmt->rset.vrows + i + 1);
			errors ++;
		}
	}
	stmt->rset.array_pos = 0;

	/* account for processed rows */
	stmt->rset.vrows += i;
	stmt->rset.frows += i;

	/* return number of processed rows (even if 0) */
	if (ird->rows_processed_ptr) {
		DBGSTMT(stmt, "setting number of processed rows to: %u.", i);
		*ird->rows_processed_ptr = i;
	}

	if (i <= 0) {
		DBGSTMT(stmt, "no data %sto return.", stmt->rset.vrows ? "left ": "");
		return SQL_NO_DATA;
	}
	
	if (errors && i <= errors) {
		ERRSTMT(stmt, "processing failed for all rows [%d].", errors);
		return SQL_ERROR;
	}

	/* only failures need stmt.diag defer'ing */
	return SQL_SUCCESS;
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
			FIXME;
			break;

		case SQL_REFRESH:
		case SQL_UPDATE:
		case SQL_DELETE:
			ERR("operation %d not supported.", Operation);
			RET_HDIAGS(STMH(StatementHandle), SQL_STATE_HYC00);
		default:
			ERR("unknown operation type: %d.", Operation);
			RET_HDIAGS(STMH(StatementHandle), SQL_STATE_HY092);
	}
	RET_STATE(SQL_STATE_00000);
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
	ERR("data update functions not supported");
	RET_HDIAGS(STMH(StatementHandle), SQL_STATE_IM001);
}

SQLRETURN EsSQLCloseCursor(SQLHSTMT StatementHandle)
{
	esodbc_stmt_st *stmt = STMH(StatementHandle);
	if (! STMT_HAS_RESULTSET(stmt)) {
		ERRSTMT(stmt, "no open cursor for statement");
		RET_HDIAGS(stmt, SQL_STATE_24000);
	}
	return EsSQLFreeStmt(StatementHandle, SQL_CLOSE);
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
		_In_reads_(cchSqlStr) SQLWCHAR* szSqlStr,
		SQLINTEGER  cchSqlStr
)
{
	esodbc_stmt_st *stmt = STMH(hstmt);
	SQLRETURN ret;

	if (cchSqlStr == SQL_NTS) {
		cchSqlStr = (SQLINTEGER)wcslen(szSqlStr);
	} else if (cchSqlStr <= 0) {
		ERRSTMT(stmt, "invalid statment lenght: %d.", cchSqlStr);
		RET_HDIAGS(stmt, SQL_STATE_HY090);
	}
	DBGSTMT(stmt, "preparing `" LWPDL "` [%d]", cchSqlStr, szSqlStr, 
			cchSqlStr);
	
	ret = EsSQLFreeStmt(stmt, ESODBC_SQL_CLOSE);
	assert(SQL_SUCCEEDED(ret)); /* can't return error */
	
	return attach_sql(stmt, szSqlStr, cchSqlStr);
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
SQLRETURN EsSQLExecute(SQLHSTMT hstmt)
{
	esodbc_stmt_st *stmt = STMH(hstmt);

	DBGSTMT(stmt, "executing `%.*s` (%zd)", stmt->sqllen, stmt->u8sql,
			stmt->sqllen);
	
	return post_statement(stmt);
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
		_In_reads_opt_(TextLength) SQLWCHAR* szSqlStr,
		SQLINTEGER cchSqlStr 
)
{
	esodbc_stmt_st *stmt = STMH(hstmt);
	SQLRETURN ret;

	if (cchSqlStr == SQL_NTS) {
		cchSqlStr = (SQLINTEGER)wcslen(szSqlStr);
	} else if (cchSqlStr <= 0) {
		ERRSTMT(stmt, "invalid statment lenght: %d.", cchSqlStr);
		RET_HDIAGS(stmt, SQL_STATE_HY090);
	}
	DBGSTMT(stmt, "directly executing SQL: `" LWPDL "` [%d].", cchSqlStr,
			szSqlStr, cchSqlStr);
	
	ret = EsSQLFreeStmt(stmt, ESODBC_SQL_CLOSE);
	assert(SQL_SUCCEEDED(ret)); /* can't return error */

	if (1 < stmt->apd->array_size) // & param marker is set!
		FIXME; //FIXME: multiple executions?

	ret = attach_sql(stmt, szSqlStr, cchSqlStr);
	if (SQL_SUCCEEDED(ret)) {
		ret = post_statement(stmt);
	}
#ifndef NDEBUG
	/* no reason to keep it (it can't be re-executed), except for debugging */
	detach_sql(stmt); 
#endif /* NDEBUG */
	return ret;
}

static inline SQLULEN get_col_size(desc_rec_st *rec)
{
	assert(rec->desc->type == DESC_TYPE_IRD);
	switch (rec->meta_type) {
		case METATYPE_EXACT_NUMERIC:
		case METATYPE_FLOAT_NUMERIC:
			return rec->precision;
		
		case METATYPE_STRING:
		case METATYPE_BIN:
		case METATYPE_DATETIME:
		case METATYPE_INTERVAL_WSEC:
		case METATYPE_INTERVAL_WOSEC:
		case METATYPE_BIT:
			return rec->length;
	}
	return 0;
}

static inline SQLSMALLINT get_col_decdigits(desc_rec_st *rec)
{
	assert(rec->desc->type == DESC_TYPE_IRD);
	switch (rec->meta_type) {
		case METATYPE_DATETIME:
		case METATYPE_INTERVAL_WSEC:
			return rec->precision;
		
		case METATYPE_EXACT_NUMERIC:
			return rec->scale;
	}
	return 0;
}

SQLRETURN EsSQLDescribeColW(
		SQLHSTMT            hstmt,
		SQLUSMALLINT        icol,
		_Out_writes_opt_(cchColNameMax) 
		SQLWCHAR            *szColName,
		SQLSMALLINT         cchColNameMax,
		_Out_opt_
		SQLSMALLINT*        pcchColName,
		_Out_opt_
		SQLSMALLINT*        pfSqlType,
		_Out_opt_
		SQLULEN*            pcbColDef,
		_Out_opt_
		SQLSMALLINT*        pibScale,
		_Out_opt_
		SQLSMALLINT*        pfNullable)
{
	esodbc_stmt_st *stmt = STMH(hstmt);
	desc_rec_st *rec;
	SQLRETURN ret;
	SQLSMALLINT col_blen = -1;

	DBGSTMT(stmt, "IRD@0x%p, column #%d.", stmt->ird, icol);

	if (! STMT_HAS_RESULTSET(stmt)) {
		ERRSTMT(stmt, "no resultset available on statement.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	if (icol < 1) {
		/* TODO: if implementing bookmarks */
		RET_HDIAGS(stmt, SQL_STATE_HYC00);
	}
	
	rec = get_record(stmt->ird, icol, FALSE);
	if (! rec) {
		ERRSTMT(stmt, "no record for columns #%d.", icol);
		RET_HDIAGS(stmt, SQL_STATE_07009);
	}
#ifndef NDEBUG
	//dump_record(rec);
#endif /* NDEBUG */

	if (szColName) {
		ret = write_wptr(&stmt->diag, szColName, rec->name, 
				cchColNameMax * sizeof(*szColName), &col_blen);
		if (! SQL_SUCCEEDED(ret)) {
			ERRSTMT(stmt, "failed to copy column name `" LWPD "`.", rec->name);
			return ret;
		}
	} else {
		DBGSTMT(stmt, "NULL column name buffer provided.");
	}

	if (! pcchColName) {
		ERRSTMT(stmt, "no column name lenght buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090, 
				"no column name lenght buffer provided", 0);
	}
	*pcchColName = 0 <= col_blen ? (col_blen / sizeof(*szColName)) : 
		(SQLSMALLINT)wcslen(rec->name);
	DBGSTMT(stmt, "col #%d name has %d chars.", icol, *pcchColName);

	if (! pfSqlType) {
		ERRSTMT(stmt, "no column data type buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090, 
				"no column data type buffer provided", 0);
	}
	*pfSqlType = rec->concise_type;
	DBGSTMT(stmt, "col #%d has concise type=%d.", icol, *pfSqlType);

	if (! pcbColDef) {
		ERRSTMT(stmt, "no column size buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090, "no column size buffer provided", 0);
	}
	*pcbColDef = get_col_size(rec); // TODO: set "size" of columns from type
	DBGSTMT(stmt, "col #%d of meta type %d has size=%llu.", 
			icol, rec->meta_type, *pcbColDef);

	if (! pibScale) {
		ERRSTMT(stmt, "no column decimal digits buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090, 
				"no column decimal digits buffer provided", 0);
	}
	*pibScale = get_col_decdigits(rec); // TODO: set "decimal digits" from type
	DBGSTMT(stmt, "col #%d of meta type %d has decimal digits=%d.", 
			icol, rec->meta_type, *pibScale);

	if (! pfNullable) {
		ERRSTMT(stmt, "no column decimal digits buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090, 
				"no column decimal digits buffer provided", 0);
	}
	/* TODO: this would be available in SQLColumns resultset. */
	*pfNullable = rec->nullable;
	DBGSTMT(stmt, "col #%d nullable=%d.", icol, *pfNullable);

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
	desc_rec_st *rec;
	SQLSMALLINT sint;
	SQLWCHAR *wptr;
	SQLLEN len;
	SQLINTEGER iint;

#ifdef _WIN64
#define PNUMATTR_ASSIGN(type, value) *pNumAttr = (SQLLEN)(value)
#else /* _WIN64 */
#define PNUMATTR_ASSIGN(type, value) *(type *)pNumAttr = (type)(value)
#endif /* _WIN64 */

	DBGSTMT(stmt, "IRD@0x%p, column #%d, field: %d.", ird, iCol,iField);

	if (! STMT_HAS_RESULTSET(stmt)) {
		ERRSTMT(stmt, "no resultset available on statement.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	if (iCol < 1) {
		/* TODO: if implementing bookmarks */
		RET_HDIAGS(stmt, SQL_STATE_HYC00);
	}
	
	rec = get_record(ird, iCol, FALSE);
	if (! rec) {
		ERRSTMT(stmt, "no record for columns #%d.", iCol);
		RET_HDIAGS(stmt, SQL_STATE_07009);
	}

	switch (iField) {
		/* SQLSMALLINT */
		do {
		case SQL_DESC_CONCISE_TYPE: sint = rec->concise_type; break;
		case SQL_DESC_TYPE: sint = rec->type; break;
		case SQL_DESC_FIXED_PREC_SCALE: sint = rec->fixed_prec_scale; break;
		case SQL_DESC_NULLABLE: sint = rec->nullable; break;
		case SQL_DESC_PRECISION: sint = rec->precision; break;
		case SQL_DESC_SCALE: sint = rec->scale; break;
		case SQL_DESC_SEARCHABLE: sint = rec->searchable; break;
		case SQL_DESC_UNNAMED: sint = rec->unnamed; break;
		case SQL_DESC_UNSIGNED: sint = rec->usigned; break;
		case SQL_DESC_UPDATABLE: sint = rec->updatable; break;
		} while (0);
			PNUMATTR_ASSIGN(SQLSMALLINT, sint);
			break;

		/* SQLWCHAR* */
		do {
		case SQL_DESC_BASE_COLUMN_NAME: wptr = rec->base_column_name; break;
		case SQL_DESC_LABEL: wptr = rec->label; break;
		case SQL_DESC_BASE_TABLE_NAME: wptr = rec->base_table_name; break;
		case SQL_DESC_CATALOG_NAME: wptr = rec->catalog_name; break;
		case SQL_DESC_LITERAL_PREFIX: wptr = rec->literal_prefix; break;
		case SQL_DESC_LITERAL_SUFFIX: wptr = rec->literal_suffix; break;
		case SQL_DESC_LOCAL_TYPE_NAME: wptr = rec->type_name; break;
		case SQL_DESC_NAME: wptr = rec->name; break;
		case SQL_DESC_SCHEMA_NAME: wptr = rec->schema_name; break;
		case SQL_DESC_TABLE_NAME: wptr = rec->table_name; break;
		case SQL_DESC_TYPE_NAME: wptr = rec->type_name; break;
		} while (0);
			if (! wptr) {
				//BUG -- TODO: re-eval, once type handling is decided.
				ERRSTMT(stmt, "IRD@0x%p record field type %d not initialized.",
						ird, iField);
				*(SQLWCHAR **)pCharAttr = MK_WPTR("");
				*pcbCharAttr = 0;
			} else {
				return write_wptr(&stmt->diag, pcbCharAttr, wptr, cbDescMax,
						pcbCharAttr);
			}
			break;
	
		/* SQLLEN */
		do {
		case SQL_DESC_DISPLAY_SIZE: len = rec->display_size; break;
		case SQL_DESC_OCTET_LENGTH: len = rec->octet_length; break;
		} while (0);
			PNUMATTR_ASSIGN(SQLLEN, len);
			break;
	
		/* SQLULEN */
		case SQL_DESC_LENGTH:
			PNUMATTR_ASSIGN(SQLULEN, rec->length);
			break;

		/* SQLINTEGER */
		do {
		case SQL_DESC_AUTO_UNIQUE_VALUE: iint = rec->auto_unique_value; break;
		case SQL_DESC_CASE_SENSITIVE: iint = rec->case_sensitive; break;
		case SQL_DESC_NUM_PREC_RADIX: iint = rec->num_prec_radix; break;
		} while (0);
			PNUMATTR_ASSIGN(SQLINTEGER, iint);
			break;


		case SQL_DESC_COUNT:
			PNUMATTR_ASSIGN(SQLSMALLINT, ird->count);
			break;

		default:
			ERRSTMT(stmt, "unknown field type %d.", iField);
			RET_HDIAGS(stmt, SQL_STATE_HY091);
	}

	return SQL_SUCCESS;
#undef PNUMATTR_ASSIGN
}

SQLRETURN EsSQLRowCount(_In_ SQLHSTMT StatementHandle, _Out_ SQLLEN* RowCount)
{
	esodbc_stmt_st *stmt = STMH(StatementHandle);
	
	if (! STMT_HAS_RESULTSET(stmt)) {
		ERRSTMT(stmt, "no resultset available on statement.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}
	
	DBGSTMT(stmt, "current resultset rows count: %zd.", stmt->rset.nrows);
	*RowCount = (SQLLEN)stmt->rset.nrows;

	if (stmt->rset.eccnt) {
		/* fetch_size or scroller size chunks the result */
		WARN("this function will only return the row count of the partial "
				"result set available.");
		/* returning a _WITH_INFO here will fail the query for MSQRY32.. */
		//RET_HDIAG(stmt, SQL_STATE_01000, "row count is for partial result "
		//		"only", 0);
	}
	return SQL_SUCCESS;
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
