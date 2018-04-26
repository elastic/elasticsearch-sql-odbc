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
#include "log.h"
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
	DBGH(stmt, "clearing result set; vrows=%zd, nrows=%zd, frows=%zd.",
			stmt->rset.vrows, stmt->rset.nrows, stmt->rset.frows);
	if (stmt->rset.buff)
		free(stmt->rset.buff);
	if (stmt->rset.state)
		UJFree(stmt->rset.state);
	memset(&stmt->rset, 0, sizeof(stmt->rset));
}

static void set_col_size(esodbc_rec_st *rec)
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

static void set_col_decdigits(esodbc_rec_st *rec)
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
	esodbc_desc_st *ird;
	esodbc_dbc_st *dbc;
	esodbc_rec_st *rec;
	SQLRETURN ret;
	SQLSMALLINT recno;
	void *iter;
	UJObject col_o, name_o, type_o;
	wstr_st col_name, col_type;
	size_t ncols, i;
	wchar_t *keys[] = {
		MK_WPTR(JSON_ANSWER_COL_NAME),
		MK_WPTR(JSON_ANSWER_COL_TYPE)
	};

	ird = stmt->ird;
	dbc = stmt->hdr.dbc;

	ncols = UJArraySize(columns);
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
		col_name.str = (SQLWCHAR *)UJReadString(name_o, &col_name.cnt);
		rec->name = col_name.cnt ? col_name.str : MK_WPTR("");

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
			rec->concise_type = rec->es_type->sql_c_type;
		} else {
			do {
				if (! dbc->no_types) {
					/* the connection doesn't have the types cached yet (this
					 * is the SYS TYPES call) -> resolve the types "directly"
					 * */
					rec->concise_type = type_elastic2csql(&col_type);
					if (rec->concise_type != SQL_UNKNOWN_TYPE)
						break;
				}
				ERRH(stmt, "type lookup failed for `" LWPDL "`. (ES-driver "
						"out of sync? check versions.)", LWSTR(&col_type));
				RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
			} while (0);
		}
		concise_to_type_code(rec->concise_type, &rec->type,
				&rec->datetime_interval_code);
		rec->meta_type = concise_to_meta(rec->concise_type, DESC_TYPE_ARD);

		set_col_size(rec);
		set_col_decdigits(rec);

		/* TODO: set remaining of settable fields (base table etc.) */

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

		DBGH(stmt, "column #%d: name=`" LWPDL "`, type=%d (`" LWPDL "`).",
				recno, LWSTR(&col_name), rec->concise_type,
				LWSTR(&col_type));
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
		ERRH(stmt, "failed to decode JSON answer (`%.*s`): %s.", blen, buff,
				stmt->rset.state ? UJGetError(stmt->rset.state) : "<none>");
		RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
	}
	columns = rows = cursor = NULL;
	/* extract the columns and rows objects */
	unpacked = UJObjectUnpack(obj, 3, "AAS", keys, &columns, &rows, &cursor);
    if (unpacked < /* 'rows' must always be present */1) {
		ERRH(stmt, "failed to unpack JSON answer (`%.*s`): %s.",
				blen, buff, UJGetError(stmt->rset.state));
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
		stmt->rset.nrows = (size_t)UJArraySize(rows);
	}
	DBGH(stmt, "rows received in result set: %zd.", stmt->rset.nrows);

	/*
	 * copy Elastic's cursor (if there's one)
	 */
	if (cursor) {
		wcurs = UJReadString(cursor, &eccnt);
		if (eccnt) {
			/* this can happen automatically if hitting scroller size */
			if (! stmt->hdr.dbc->fetch.max)
				INFOH(stmt, "no fetch size defined, but cursor returned.");
			if (stmt->rset.ecurs)
				DBGH(stmt, "replacing old cursor `" LWPDL "`.",
						stmt->rset.eccnt, stmt->rset.ecurs);
			/* store new cursor vals */
			stmt->rset.ecurs = wcurs;
			stmt->rset.eccnt = eccnt;
			DBGH(stmt, "new elastic cursor: `" LWPDL "`[%zd].",
					stmt->rset.eccnt, stmt->rset.ecurs, stmt->rset.eccnt);
		} else {
			WARNH(stmt, "empty cursor found in the answer.");
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

	INFOH(stmt, "REST request failed with `%.*s` (%zd).", blen, buff, blen);

	/* parse the entire JSON answer */
	obj = UJDecode(buff, blen, NULL, &state);
	if (! obj) {
		ERRH(stmt, "failed to decode JSON answer (`%.*s`): %s.",
				blen, buff, state ? UJGetError(state) : "<none>");
		SET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		goto end;
	}
	/* extract the status and error object */
	if (UJObjectUnpack(obj, 2, "ON", outer_keys, &o_error, &o_status) < 2) {
		ERRH(stmt, "failed to unpack JSON answer (`%.*s`): %s.",
				blen, buff, UJGetError(state));
		SET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		goto end;
	}
	/* unpack error object */
	if (UJObjectUnpack(o_error, 2, "SS", err_keys, &o_type, &o_reason) < 2) {
		ERRH(stmt, "failed to unpack error object (`%.*s`): %s.",
				blen, buff, UJGetError(state));
		SET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		goto end;
	}

	wtype = UJReadString(o_type, &tlen);
	wreason = UJReadString(o_reason, &rlen);
	/* these return empty string in case of mismatch */
	assert(wtype && wreason);
	DBGH(stmt, "server failures: type: [%zd] `" LWPDL "`, reason: [%zd] `"
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
		ERRNH(stmt, "failed to print error message from server.");
		assert(sizeof(MSG_INV_SRV_ANS) < sizeof(wbuf));
		memcpy(wbuf, MK_WPTR(MSG_INV_SRV_ANS),
				sizeof(MSG_INV_SRV_ANS)*sizeof(SQLWCHAR));
	}

	post_diagnostic(&stmt->hdr.diag, SQL_STATE_HY000, wbuf,
			UJNumericInt(o_status));

end:
	if (state)
		UJFree(state);
	if (buff)
		free(buff);

	RET_STATE(stmt->hdr.diag.state);
}

/*
 * Attach an SQL query to the statment: malloc, convert, copy.
 */
SQLRETURN attach_sql(esodbc_stmt_st *stmt,
		const SQLWCHAR *sql, /* SQL text statement */
		size_t sqlcnt /* count of chars of 'sql' */)
{
	char *u8;
	int len;

	DBGH(stmt, "attaching SQL `" LWPDL "` (%zd).", sqlcnt, sql, sqlcnt);
#if 0 // FIXME
	if (wcslen(sql) < 1256) {
		if (wcsstr(sql, L"FROM test_emp")) {
			sql = L"SELECT emp_no, first_name, last_name, birth_date, 2+3 AS foo FROM test_emp";
			sqlcnt = wcslen(sql);
			DBGH(stmt, "RE-attaching SQL `" LWPDL "` (%zd).", sqlcnt,
					sql, sqlcnt);
		}
	}
#endif

	assert(! stmt->u8sql);

	len = WCS2U8(sql, (int)sqlcnt, NULL, 0);
	if (len <= 0) {
		ERRNH(stmt, "failed to UCS2/UTF8 convert SQL `" LWPDL "` (%zd).",
				sqlcnt, sql, sqlcnt);
		RET_HDIAG(stmt, SQL_STATE_HY000, "UCS2/UTF8 conversion failure", 0);
	}
	DBGH(stmt, "wide char SQL `" LWPDL "`[%zd] converts to UTF8 on %d "
			"octets.", sqlcnt, sql, sqlcnt, len);

	u8 = malloc(len);
	if (! u8) {
		ERRNH(stmt, "failed to alloc %dB.", len);
		RET_HDIAGS(stmt, SQL_STATE_HY001);
	}

	len = WCS2U8(sql, (int)sqlcnt, u8, len);
	if (len <= 0) { /* can it happen? it's just succeded above */
		ERRNH(stmt, "failed to UCS2/UTF8 convert SQL `" LWPDL "` (%zd).",
				sqlcnt, sql, sqlcnt);
		free(u8);
		RET_HDIAG(stmt, SQL_STATE_HY000, "UCS2/UTF8 conversion failure(2)", 0);
	}

	stmt->u8sql = u8;
	stmt->sqllen = (size_t)len;

	DBGH(stmt, "attached SQL `%.*s` (%zd).", len, u8, len);

	return SQL_SUCCESS;
}

/*
 * Detach the existing query (if any) from the statement.
 */
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
		ERRH(stmt, "invalid negative BufferLength: %d.", BufferLength);
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
		esodbc_rec_st *rec)
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

	DBGH(desc->hdr.stmt, "rec@0x%p, field_id:%d : base@0x%p, offset=%d, "
			"elem_size=%zd", rec, field_id, base, offt, elem_size);

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
	// FIXME: check note above
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
		DBG("NULL octet len pointer, length (%zd) not indicated.",
				copied);
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

static SQLRETURN copy_longlong(esodbc_rec_st *arec, esodbc_rec_st *irec,
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

	stmt = arec->desc->hdr.stmt;
	ird = stmt->ird;
	ard = stmt->ard;

	/* pointer where to write how many characters we will/would use */
	octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR, pos, arec);
	/* pointer to app's buffer */
	data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, arec);
	if (! data_ptr) {
		ERRH(stmt, "received numeric type, but NULL data ptr.");
		RET_HDIAGS(stmt, SQL_STATE_HY009);
	}

	/* "To use the default mapping, an application specifies the SQL_C_DEFAULT
	 * type identifier." */
	target_type = arec->type == SQL_C_DEFAULT ? irec->type : arec->type;
	DBGH(stmt, "target data type: %d.", target_type);
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

		case SQL_C_STINYINT:
			*(SQLSCHAR *)data_ptr = (SQLSCHAR)ll;
			write_copied_octets(octet_len_ptr, sizeof(SQLSCHAR),
					stmt->max_length, irec->meta_type);
			break;

		case SQL_C_SSHORT:
			*(SQLSMALLINT *)data_ptr = (SQLSMALLINT)ll;
			write_copied_octets(octet_len_ptr, sizeof(SQLSMALLINT),
					stmt->max_length, irec->meta_type);
			break;

		case SQL_C_SLONG:
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
	DBGH(stmt, "REC@0x%p, data_ptr@0x%p, copied long long: `%d`.", arec,
			data_ptr, (SQLINTEGER)ll);

	RET_STATE(state);
}

static SQLRETURN copy_double(esodbc_rec_st *arec, esodbc_rec_st *irec,
		SQLULEN pos, double dbl)
{
	FIXME; // FIXME
	return SQL_ERROR;
}

static SQLRETURN copy_boolean(esodbc_rec_st *arec, esodbc_rec_st *irec,
		SQLULEN pos, BOOL boolval)
{
	esodbc_stmt_st *stmt;
	void *data_ptr;
	SQLLEN *octet_len_ptr;
	SQLSMALLINT target_type;

	stmt = arec->desc->hdr.stmt;

	/* pointer where to write how many characters we will/would use */
	octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR, pos, arec);
	/* pointer to app's buffer */
	data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, arec);
	if (! data_ptr) {
		ERRH(stmt, "received boolean type, but NULL data ptr.");
		RET_HDIAGS(stmt, SQL_STATE_HY009);
	}

	target_type = arec->type == SQL_C_DEFAULT ? irec->type : arec->type;
	DBGH(stmt, "target data type: %d.", target_type);
	switch (target_type) {
		case SQL_C_STINYINT:
		case SQL_C_UTINYINT:
			*(SQLSCHAR *)data_ptr = (SQLSCHAR)boolval;
			write_copied_octets(octet_len_ptr, sizeof(SQLSCHAR),
					stmt->max_length, irec->meta_type);
			break;

		case SQL_C_SSHORT:
		case SQL_C_USHORT:
			*(SQLSMALLINT *)data_ptr = (SQLSMALLINT)boolval;
			write_copied_octets(octet_len_ptr, sizeof(SQLSMALLINT),
					stmt->max_length, irec->meta_type);
			break;

		case SQL_C_SLONG:
		case SQL_C_ULONG:
			*(SQLINTEGER *)data_ptr = (SQLINTEGER)boolval;
			write_copied_octets(octet_len_ptr, sizeof(SQLINTEGER),
					stmt->max_length, irec->meta_type);
			break;

		case SQL_C_SBIGINT:
		case SQL_C_UBIGINT:
			*(SQLBIGINT *)data_ptr = (SQLBIGINT)boolval;
			write_copied_octets(octet_len_ptr, sizeof(SQLBIGINT),
					stmt->max_length, irec->meta_type);
			break;

		case SQL_C_CHAR:
		case SQL_C_WCHAR:

		default:
			FIXME; // FIXME
			return SQL_ERROR;
	}

	DBGH(stmt, "REC@0x%p, data_ptr@0x%p, copied boolean: `%d`.", arec,
			data_ptr, boolval);
	return SQL_SUCCESS;
}

/*
 * -> SQL_C_CHAR
 */
static SQLRETURN wstr_to_cstr(esodbc_rec_st *arec, esodbc_rec_st *irec,
		void *data_ptr, SQLLEN *octet_len_ptr,
		const wchar_t *wstr, size_t chars)
{
	esodbc_state_et state = SQL_STATE_00000;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;
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
				ERRNH(stmt, "failed to convert wchar* to char* for string `"
						LWPDL "`.", chars, wstr);
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

		DBGH(stmt, "REC@0x%p, data_ptr@0x%p, copied %zd bytes: `" LWPD "`.",
				arec, data_ptr, out_bytes, charp);
	} else {
		DBGH(stmt, "REC@0x%p, NULL data_ptr.", arec);
	}

	/* if length needs to be given, calculate  it not truncated & converted */
	if (octet_len_ptr) {
		out_bytes = (size_t)WCS2U8(wstr, (int)chars, NULL, 0);
		if (out_bytes <= 0) {
			ERRNH(stmt, "failed to convert wchar* to char* for string `"
					LWPDL "`.", chars, wstr);
			RET_HDIAGS(stmt, SQL_STATE_22018);
		}
		write_copied_octets(octet_len_ptr, out_bytes, stmt->max_length,
				irec->meta_type);
	} else {
		DBGH(stmt, "REC@0x%p, NULL octet_len_ptr.", arec);
	}

	if (state != SQL_STATE_00000)
		RET_HDIAGS(stmt, state);
	return SQL_SUCCESS;
}

/*
 * -> SQL_C_WCHAR
 */
static SQLRETURN wstr_to_wstr(esodbc_rec_st *arec, esodbc_rec_st *irec,
		void *data_ptr, SQLLEN *octet_len_ptr,
		const wchar_t *wstr, size_t chars)
{
	esodbc_state_et state = SQL_STATE_00000;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;
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

		DBGH(stmt, "REC@0x%p, data_ptr@0x%p, copied %zd bytes: `" LWPD "`.",
				arec, data_ptr, out_bytes, widep);
	} else {
		DBGH(stmt, "REC@0x%p, NULL data_ptr", arec);
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
		ERR("data `" LWPDL "` not an ANSI ISO 8601 format.", chars,wstr);
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
static SQLRETURN wstr_to_timestamp(esodbc_rec_st *arec, esodbc_rec_st *irec,
		void *data_ptr, SQLLEN *octet_len_ptr,
		const wchar_t *wstr, size_t chars)
{
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;
	TIMESTAMP_STRUCT *tss = (TIMESTAMP_STRUCT *)data_ptr;

	if (octet_len_ptr) {
		*octet_len_ptr = sizeof(*tss);
	}

	if (data_ptr) {
		if (! wstr_to_timestamp_struct(wstr, chars, tss)) {
			RET_HDIAGS(stmt, SQL_STATE_07006);
		}
	} else {
		DBGH(stmt, "REC@0x%p, NULL data_ptr", arec);
	}

	return SQL_SUCCESS;
}

/*
 * -> SQL_C_TYPE_DATE
 */
static SQLRETURN wstr_to_date(esodbc_rec_st *arec, esodbc_rec_st *irec,
		void *data_ptr, SQLLEN *octet_len_ptr,
		const wchar_t *wstr, size_t chars)
{
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;
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
		DBGH(stmt, "REC@0x%p, NULL data_ptr", arec);
	}

	return SQL_SUCCESS;
}

/*
 * wstr: is 0-terminated, terminator is counted in 'chars'.
 */
static SQLRETURN copy_string(esodbc_rec_st *arec, esodbc_rec_st *irec,
		SQLULEN pos, const wchar_t *wstr, size_t chars)
{
	esodbc_stmt_st *stmt;
	void *data_ptr;
	SQLLEN *octet_len_ptr;
	esodbc_desc_st *ard, *ird;
	SQLSMALLINT target_type;

	stmt = arec->desc->hdr.stmt;
	ird = stmt->ird;
	ard = stmt->ard;

	/* pointer where to write how many characters we will/would use */
	octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR, pos, arec);
	/* pointer to app's buffer */
	data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, arec);

	/* "To use the default mapping, an application specifies the SQL_C_DEFAULT
	 * type identifier." */
	target_type = arec->type == SQL_C_DEFAULT ? irec->type : arec->type;
	DBGH(stmt, "target data type: %d.", target_type);
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
	BOOL boolval;
	size_t len;
	BOOL with_info;
	esodbc_desc_st *ard, *ird;
	esodbc_rec_st* arec, *irec;

	rowno = stmt->rset.frows + pos + /*1-based*/1;
	ard = stmt->ard;
	ird = stmt->ird;

#define RET_ROW_DIAG(_state, _message, _colno) \
	do { \
		if (ard->array_status_ptr) \
			ard->array_status_ptr[pos] = SQL_ROW_ERROR; \
		return post_row_diagnostic(&stmt->hdr.diag, _state, MK_WPTR(_message),\
				0, rowno, _colno); \
	} while (0)
#define SET_ROW_DIAG(_rowno, _colno) \
	do { \
		stmt->hdr.diag.row_number = _rowno; \
		stmt->hdr.diag.column_number = _colno; \
	} while (0)

	if (! UJIsArray(row)) {
		ERRH(stmt, "one '%s' (#%zd) element in result set not array; type:"
				" %d.", JSON_ANSWER_ROWS, stmt->rset.vrows, UJGetType(row));
		RET_ROW_DIAG(SQL_STATE_01S01, MSG_INV_SRV_ANS,
				SQL_NO_COLUMN_NUMBER);
	}
	iter_row = UJBeginArray(row);
	if (! iter_row) {
		ERRH(stmt, "Failed to obtain iterator on row (#%zd): %s.", rowno,
				UJGetError(stmt->rset.state));
		RET_ROW_DIAG(SQL_STATE_01S01, MSG_INV_SRV_ANS,
				SQL_NO_COLUMN_NUMBER);
	}

	with_info = FALSE;
	/* iterate over the contents of one table row */
	for (i = 0; i < ard->count && UJIterArray(&iter_row, &obj); i ++) {
		/* if record not bound skip it */
		if (! REC_IS_BOUND(&ard->recs[i])) {
			DBGH(stmt, "column #%d not bound, skipping it.", i + 1);
			continue;
		}

		arec = get_record(ard, i + 1, FALSE);
		irec = get_record(ird, i + 1, FALSE);
		assert(arec);
		assert(irec);

		switch (UJGetType(obj)) {
			default:
				ERRH(stmt, "unexpected object of type %d in row L#%zd/T#%zd.",
						UJGetType(obj), stmt->rset.vrows, stmt->rset.frows);
				RET_ROW_DIAG(SQL_STATE_01S01, MSG_INV_SRV_ANS, i + 1);
				/* RET_.. returns */

			case UJT_Null:
				DBGH(stmt, "value [%zd, %d] is NULL.", rowno, i + 1);
#if 0
				// FIXME: needed?
				if (! arec->nullable) {
					ERRH(stmt, "received a NULL for a not nullable val.");
					RET_ROW_DIAG(SQL_STATE_HY003, "NULL value received for non"
							" nullable data type", i + 1);
				}
#endif //0
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
				ret = copy_string(arec, irec, pos, wstr, len + /*\0*/1);
				break;

			case UJT_Long:
			case UJT_LongLong:
				ll = UJNumericLongLong(obj);
				DBGH(stmt, "value [%zd, %d] is numeric: %lld.", rowno, i + 1,
						ll);
				ret = copy_longlong(arec, irec, pos, ll);
				break;

			case UJT_Double:
				dbl = UJNumericFloat(obj);
				DBGH(stmt, "value [%zd, %d] is double: %f.", rowno, i + 1,
						dbl);
				ret = copy_double(arec, irec, pos, dbl);
				break;

			/* TODO: convert to 1/0? */
			case UJT_True:
			case UJT_False:
				boolval = UJGetType(obj) == UJT_True ? TRUE : FALSE;
				DBGH(stmt, "value [%zd, %d] is boolean: %d.", rowno, i + 1,
						boolval);
				ret = copy_boolean(arec, irec, pos, boolval);
				break;
		}

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
	}

	if (ird->array_status_ptr) {
		ird->array_status_ptr[pos] = with_info ? SQL_ROW_SUCCESS_WITH_INFO :
			SQL_ROW_SUCCESS;
		DBGH(stmt, "status array @0x%p#%d set to %d.", ird->array_status_ptr,
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
	SQLULEN i, j;
	UJObject row;
	SQLRETURN ret;
	int errors;

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

	DBGH(stmt, "(`%.*s`); cursor @ %zd / %zd.", stmt->sqllen,
			stmt->u8sql, stmt->rset.vrows, stmt->rset.nrows);
	
	DBGH(stmt, "rowset max size: %d.", ard->array_size);
	errors = 0;
	/* for all rows in rowset/array, iterate over rows in current resultset */
	for (i = stmt->rset.array_pos; i < ard->array_size; i ++) {
		if (! UJIterArray(&stmt->rset.rows_iter, &row)) {
			DBGH(stmt, "ran out of rows in current result set: nrows=%zd, "
					"vrows=%zd.", stmt->rset.nrows, stmt->rset.vrows);
			if (stmt->rset.eccnt) { /*do I have an Elastic cursor? */
				stmt->rset.array_pos = i;
				ret = post_statement(stmt);
				if (! SQL_SUCCEEDED(ret)) {
					ERRH(stmt, "failed to fetch next results.");
					return ret;
				}
				return EsSQLFetch(StatementHandle);
			} else {
				DBGH(stmt, "reached end of entire result set. fetched=%zd.",
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
			ERRH(stmt, "copying row %zd failed.", stmt->rset.vrows + i + 1);
			errors ++;
		}
	}
	stmt->rset.array_pos = 0;

	/* account for processed rows */
	stmt->rset.vrows += i;
	stmt->rset.frows += i;

	/* return number of processed rows (even if 0) */
	if (ird->rows_processed_ptr) {
		DBGH(stmt, "setting number of processed rows to: %u.", i);
		*ird->rows_processed_ptr = i;
	}

	if (i <= 0) {
		DBGH(stmt, "no data %sto return.", stmt->rset.vrows ? "left ": "");
		return SQL_NO_DATA;
	}
	
	if (errors && i <= errors) {
		ERRH(stmt, "processing failed for all rows [%d].", errors);
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

SQLRETURN EsSQLCloseCursor(SQLHSTMT StatementHandle)
{
	esodbc_stmt_st *stmt = STMH(StatementHandle);
	if (! STMT_HAS_RESULTSET(stmt)) {
		ERRH(stmt, "no open cursor for statement");
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
		ERRH(stmt, "invalid statment lenght: %d.", cchSqlStr);
		RET_HDIAGS(stmt, SQL_STATE_HY090);
	}
	DBGH(stmt, "preparing `" LWPDL "` [%d]", cchSqlStr, szSqlStr,
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

	DBGH(stmt, "executing `%.*s` (%zd)", stmt->sqllen, stmt->u8sql,
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
		ERRH(stmt, "invalid statment lenght: %d.", cchSqlStr);
		RET_HDIAGS(stmt, SQL_STATE_HY090);
	}
	DBGH(stmt, "directly executing SQL: `" LWPDL "` [%d].", cchSqlStr,
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

static inline SQLULEN get_col_size(esodbc_rec_st *rec)
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

static inline SQLSMALLINT get_col_decdigits(esodbc_rec_st *rec)
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
	esodbc_rec_st *rec;
	SQLRETURN ret;
	SQLSMALLINT col_blen = -1;

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
		ret = write_wptr(&stmt->hdr.diag, szColName, rec->name,
				cchColNameMax * sizeof(*szColName), &col_blen);
		if (! SQL_SUCCEEDED(ret)) {
			ERRH(stmt, "failed to copy column name `" LWPD "`.", rec->name);
			return ret;
		}
	} else {
		DBGH(stmt, "NULL column name buffer provided.");
	}

	if (! pcchColName) {
		ERRH(stmt, "no column name lenght buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090,
				"no column name lenght buffer provided", 0);
	}
	*pcchColName = 0 <= col_blen ? (col_blen / sizeof(*szColName)) :
		(SQLSMALLINT)wcslen(rec->name);
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
	*pcbColDef = get_col_size(rec); // TODO: set "size" of columns from type
	DBGH(stmt, "col #%d of meta type %d has size=%llu.",
			icol, rec->meta_type, *pcbColDef);

	if (! pibScale) {
		ERRH(stmt, "no column decimal digits buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090,
				"no column decimal digits buffer provided", 0);
	}
	*pibScale = get_col_decdigits(rec); // TODO: set "decimal digits" from type
	DBGH(stmt, "col #%d of meta type %d has decimal digits=%d.",
			icol, rec->meta_type, *pibScale);

	if (! pfNullable) {
		ERRH(stmt, "no column decimal digits buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090,
				"no column decimal digits buffer provided", 0);
	}
	/* TODO: this would be available in SQLColumns resultset. */
	*pfNullable = rec->nullable;
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
	SQLWCHAR *wptr;
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
				ERRH(stmt, "IRD@0x%p record field type %d not initialized.",
						ird, iField);
				*(SQLWCHAR **)pCharAttr = MK_WPTR("");
				*pcbCharAttr = 0;
			} else {
				return write_wptr(&stmt->hdr.diag, pcbCharAttr, wptr, cbDescMax,
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
			ERRH(stmt, "unknown field type %d.", iField);
			RET_HDIAGS(stmt, SQL_STATE_HY091);
	}

	return SQL_SUCCESS;
#undef PNUMATTR_ASSIGN
}

SQLRETURN EsSQLRowCount(_In_ SQLHSTMT StatementHandle, _Out_ SQLLEN* RowCount)
{
	esodbc_stmt_st *stmt = STMH(StatementHandle);
	
	if (! STMT_HAS_RESULTSET(stmt)) {
		ERRH(stmt, "no resultset available on statement.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}
	
	DBGH(stmt, "current resultset rows count: %zd.", stmt->rset.nrows);
	*RowCount = (SQLLEN)stmt->rset.nrows;

	if (stmt->rset.eccnt) {
		/* fetch_size or scroller size chunks the result */
		WARNH(stmt, "this function will only return the row count of the "
				"partial result set available.");
		/* returning a _WITH_INFO here will fail the query for MSQRY32.. */
		//RET_HDIAG(stmt, SQL_STATE_01000, "row count is for partial result "
		//		"only", 0);
	}
	return SQL_SUCCESS;
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
