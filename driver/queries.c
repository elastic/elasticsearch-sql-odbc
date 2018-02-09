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

#include "queries.h"
#include "handles.h"
#include "connect.h"
#include "info.h"

#define MSG_INV_SRV_ANS		"Invalid server answer"

void clear_resultset(esodbc_stmt_st *stmt)
{
	DBG("clearping result set on stmt 0x%p; vrows=%zd, nrows=%zd, frows=%zd.", 
			stmt, stmt->rset.vrows, stmt->rset.nrows, stmt->rset.frows);
	if (stmt->rset.buff)
		free(stmt->rset.buff);
	if (stmt->rset.state)
		UJFree(stmt->rset.state);
	memset(&stmt->rset, 0, sizeof(stmt->rset));
}

static int wmemncasecmp(const wchar_t *a, const wchar_t *b, size_t len)
{
	size_t i;
	int diff = 0; /* if len == 0 */
	for (i = 0; i < len; i ++) {
		diff = towlower(a[i]) - towlower(b[i]);
		if (diff)
			break;
	}
	//DBG("`" LTPDL "` vs `" LTPDL "` => %d (len=%zd, i=%d).", 
	//		len, a, len, b, diff, len, i);
	return diff;
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
					if (wmemncasecmp(type_name, MK_WSTR(JSON_COL_INTEGER),
								len) == 0)
						return SQL_C_SLONG;
					break;
				case (wchar_t)'b': /* boolean */
					if (wmemncasecmp(type_name, MK_WSTR(JSON_COL_BOOLEAN),
								len) == 0)
						return SQL_C_UTINYINT;
					break;
				case (wchar_t)'k': /* keyword */
					if (wmemncasecmp(type_name, MK_WSTR(JSON_COL_KEYWORD),
								len) == 0)
						return SQL_C_CHAR;
					break;
			}
			break;
		// case sizeof(JSON_COL_DATE) - 1:
		case sizeof(JSON_COL_TEXT) - 1: 
			switch(tolower(type_name[0])) {
				case (wchar_t)'t':
					if (! wmemncasecmp(type_name, MK_WSTR(JSON_COL_TEXT), len))
						// TODO: char/longvarchar/wchar/wvarchar?
						return SQL_C_CHAR;
					break;
				case (wchar_t)'d':
					if (! wmemncasecmp(type_name, MK_WSTR(JSON_COL_DATE), len))
						// TODO: time/timestamp
						return SQL_C_TYPE_DATE;
					break;
#if 1 // BUG FIXME
				case (wchar_t)'n':
					if (! wmemncasecmp(type_name, MK_WSTR("null"), len))
						// TODO: time/timestamp
						return SQL_C_SSHORT;
					break;
#endif
			}
			break;
		case sizeof(JSON_COL_SHORT) - 1:
			if (! wmemncasecmp(type_name, MK_WSTR(JSON_COL_SHORT), len))
				// TODO: time/timestamp
				return SQL_C_SSHORT;
			break;
	}
	ERR("unrecognized Elastic type `" LTPDL "` (%zd).", len, type_name, len);
	return SQL_UNKNOWN_TYPE;
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
		MK_WSTR(JSON_ANSWER_COL_NAME),
		MK_WSTR(JSON_ANSWER_COL_TYPE)
	};


	ncols = UJArraySize(columns);
	DBG("STMT@0x%p: columns received: %zd.", stmt, ncols);
	ret = update_rec_count(ird, (SQLSMALLINT)ncols);
	if (! SQL_SUCCEEDED(ret)) {
		ERR("failed to set IRD's record count to %d.", ncols);
		HDIAG_COPY(ird, stmt);
		return ret;
	}

	iter = UJBeginArray(columns);
	if (! iter) {
		ERR("failed to obtain array iterator: %s.", 
				UJGetError(stmt->rset.state));
		RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
	}
	recno = 0;
	while (UJIterArray(&iter, &col_o)) {
		if (UJObjectUnpack(col_o, 2, "SS", keys, &name_o, &type_o) < 2) {
			ERR("failed to decode JSON column: %s.", 
					UJGetError(stmt->rset.state));
			RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		}
		rec = &ird->recs[recno]; // +recno

		col_wname = UJReadString(name_o, &len);
		assert(sizeof(*col_wname) == sizeof(SQLTCHAR)); /* TODO: no ANSI */
		rec->name = (SQLTCHAR *)col_wname;
		rec->unnamed = SQL_NAMED;

		col_wtype = UJReadString(type_o, &len);
		/* 
		 * TODO: to ELASTIC types, rather?
		 * TODO: Read size (precision/lenght) and dec-dig(scale/precision)
		 * from received type.
		 */
		col_stype = type_elastic2csql(col_wtype, len);
		if (col_stype == SQL_UNKNOWN_TYPE) {
			ERR("failed to convert Elastic to C SQL type `" LTPDL "`.", 
					len, col_wtype);
			RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		}
		rec->concise_type = col_stype;
		concise_to_type_code(col_stype, &rec->type, 
				&rec->datetime_interval_code);

		/* TODO: set all settable fields */
		rec->base_column_name = MK_TSTR("");
		rec->display_size = 256;

		DBG("STMT@0x%p: column #%d: name=`" LTPD "`, type=%d (`" LTPD "`).", 
				stmt, recno, col_wname, col_stype, col_wtype);
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
	UJObject obj, columns, rows;
	wchar_t *keys[] = {
		MK_WSTR(JSON_ANSWER_COLUMNS), 
		MK_WSTR(JSON_ANSWER_ROWS) 
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
		ERR("failed to decode JSON answer (`%.*s`): %s.", blen, buff, 
				stmt->rset.state ? UJGetError(stmt->rset.state) : "<none>");
		RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
	}
	/* extract the columns and rows objects */
    if (UJObjectUnpack(obj, 2, "AA", keys, &columns, &rows) < 2) {
		ERR("failed to unpack JSON answer (`%.*s`): %s.", blen, buff, 
				UJGetError(stmt->rset.state));
		RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
	}

	/* set the cursor */
	stmt->rset.rows_iter = UJBeginArray(rows);
	if (! stmt->rset.rows_iter) {
#if 0 /* UJSON4C will return NULL above, for empty array (meh!) */
		ERR("failed to get iterrator on received rows: %s.", 
				UJGetError(stmt->rset.state));
		RET_HDIAGS(stmt, SQL_STATE_HY000);
#else /*0*/
		DBG("received empty resultset array: forcing nodata.");
		STMT_FORCE_NODATA(stmt);
		stmt->rset.nrows = 0;
#endif /*0*/
	} else {
		stmt->rset.nrows = (size_t)UJArraySize(rows);
	}
	DBG("rows received in result set: %zd.", stmt->rset.nrows);

	return attach_columns(stmt, columns);
}

/*
 * Parse an error and push it as statement diagnostic.
 */
SQLRETURN attach_error(esodbc_stmt_st *stmt, char *buff, size_t blen)
{
	UJObject obj, o_status, o_error, o_type, o_reason;
	const wchar_t *wtype, *wreason;
	size_t tlen, rlen, left;
	wchar_t wbuf[SQL_MAX_MESSAGE_LENGTH];
	size_t wbuflen = sizeof(wbuf)/sizeof(wbuf[0]);
	int n;
	void *state = NULL;
	wchar_t *outer_keys[] = {
		MK_WSTR(JSON_ANSWER_ERROR),
		MK_WSTR(JSON_ANSWER_STATUS)
	};
	wchar_t *err_keys[] = {
		MK_WSTR(JSON_ANSWER_ERR_TYPE),
		MK_WSTR(JSON_ANSWER_ERR_REASON)
	};

	INFO("STMT@0x%p REST request failed with `%.*s` (%zd).", stmt, blen, buff, 
			blen);

	/* parse the entire JSON answer */
	obj = UJDecode(buff, blen, NULL, &state);
	if (! obj) {
		ERR("failed to decode JSON answer (`%.*s`): %s.", blen, buff, 
				state ? UJGetError(state) : "<none>");
		SET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		goto end;
	}
	/* extract the status and error object */
	if (UJObjectUnpack(obj, 2, "ON", outer_keys, &o_error, &o_status) < 2) {
		ERR("failed to unpack JSON answer (`%.*s`): %s.", blen, buff, 
				UJGetError(state));
		SET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		goto end;
	}
	/* unpack error object */
	if (UJObjectUnpack(o_error, 2, "SS", err_keys, &o_type, &o_reason) < 2) {
		ERR("failed to unpack error object (`%.*s`): %s.", blen, buff, 
				UJGetError(state));
		SET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		goto end;
	}

	wtype = UJReadString(o_type, &tlen);
	wreason = UJReadString(o_reason, &rlen);
	/* these return empty string in case of mismatch */
	assert(wtype && wreason);
	DBG("Server failures: type: [%zd] `" LTPDL "`, reason: [%zd] `" LTPDL "`, "
			"status: %d.", tlen, tlen, wtype, rlen, rlen, wreason, 
			UJNumericInt(o_status));

	/* swprintf will fail if formated string would overrun the buffer size (as
	 * opposed to write up to its limit) => find out the limit first.*/
	n = swprintf(NULL, 0, MK_WSTR("%.*s: %.*s"), (int)tlen, wtype, (int)rlen,
			wreason);
	if (0 < n) {
		wbuflen -= /* ": " */2 + /*\0*/1;
		tlen = wbuflen < tlen ? wbuflen : tlen;
		left = wbuflen - tlen;
		rlen = left < rlen ? left : rlen;
		wbuflen += /* ": " */2 + /*\0*/1;
		n = swprintf(wbuf, wbuflen, MK_WSTR("%.*s: %.*s"), (int)tlen, wtype,
				(int)rlen, wreason);
	}
	if (n < 0) {
		ERRN("failed to print error message from server.");
		assert(sizeof(MSG_INV_SRV_ANS) < sizeof(wbuf));
		memcpy(wbuf, MK_TSTR(MSG_INV_SRV_ANS),
				sizeof(MSG_INV_SRV_ANS)*sizeof(SQLTCHAR));
#if 0 /* this won't happen with swprintf */
	} else if (wbuflen <= n) { /* add skipped (due to size) 0-term */
		wbuf[wbuflen - 1] = 0;
#endif
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
//		const SQLTCHAR *sql, /* SQL text statement */
		const SQLTCHAR *_sql, /* SQL text statement */
		size_t sqlcnt /* count of chars of 'sql' */)
{
	char *u8;
	int len;

#if 1 // FIXME
	DBG("STMT@0x%p orig attaching SQL `" LTPDL "` (%zd).", stmt, sqlcnt,
			_sql, sqlcnt);
	SQLTCHAR *sql = _sql;
	if (wcslen(sql) < 1256) {
		if (wcsstr(sql, L"FROM test_emp")) {
			sql = L"SELECT emp_no, first_name, birth_date, 2+3 AS foo FROM test_emp";
			sqlcnt = wcslen(sql);
		}
	}
#endif
	DBG("STMT@0x%p attaching SQL `" LTPDL "` (%zd).", stmt, sqlcnt,sql,sqlcnt);

	assert(! stmt->u8sql);

	len = WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, sql, (int)sqlcnt,
			NULL, 0, NULL, NULL);
	if (len <= 0) {
		ERRN("STMT@0x%p: failed to UCS2/UTF8 convert SQL `" LTPDL "` (%zd).", 
				stmt, sqlcnt, sql, sqlcnt);
		RET_HDIAG(stmt, SQL_STATE_HY000, "UCS2/UTF8 conversion failure", 0);
	}
	DBG("wide char SQL `" LTPDL "`[%zd] converts to UTF8 on %d octets.",
			sqlcnt, sql, sqlcnt, len);

	u8 = malloc(len);
	if (! u8) {
		ERRN("failed to alloc %dB.", len);
		RET_HDIAGS(stmt, SQL_STATE_HY001);
	}

	len = WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, sql, (int)sqlcnt, 
			u8, len, NULL, NULL);
	if (len <= 0) { /* can it happen? it's just succeded above */
		ERRN("STMT@0x%p: failed to UCS2/UTF8 convert SQL `" LTPDL "` (%zd).", 
				stmt, sqlcnt, sql, sqlcnt);
		free(u8);
		RET_HDIAG(stmt, SQL_STATE_HY000, "UCS2/UTF8 conversion failure(2)", 0);
	}

	stmt->u8sql = u8;
	stmt->sqllen = (size_t)len;
	
	DBG("STMT@0x%p attached SQL `%.*s` (%zd).", stmt, len, u8, len);

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
		ERR("invalid negative BufferLength: %d.", BufferLength);
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

	DBG("rec@0x%p, field_id:%d : base@0x%p, offt=%d, esize=%zd", 
			rec, field_id, base, offt, elem_size);

	return base ? (char *)base + offt + pos * elem_size : NULL;
}

static inline void get_deferred_buffers(desc_rec_st *rec, size_t pos,
		void **data_ptr, void **octet_len_ptr)
{
	/* only indicate length if have dedicated buffer for it */
	if (rec->octet_length_ptr == rec->indicator_ptr)
		*octet_len_ptr = NULL;
	else
		/* pointer where to write how many characters we will/would use */
		*octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR, pos, rec);
	/* pointer to app's buffer */
	*data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, rec);
}

/*
 * Returns the amount of bytes to copy (in the data_ptr), taking into account
 * the bytes of the data 'have', buffer size 'available' and statement
 * attribute SQL_ATTR_MAX_LENGTH 'max'.
 * Sets the octet len pointer and indicates truncation into 'state'.
 */
static inline size_t bytes_to_copy(size_t have, size_t available, size_t max,
		SQLLEN *octet_len_ptr, esodbc_state_et *state)
{
	size_t to_copy;

	/* apply "network" truncation first, if limitation exists */
	if (0 < max && max < have) {
		to_copy = max;
		/* no truncation indicated for this case */
	} else {
		to_copy = have;
	}

	/* is target buffer to small? adjust size if so and indicate truncation */
	if (available < to_copy) {
		to_copy = available;
		*state = SQL_STATE_22001;
	}

	if (octet_len_ptr) {
		if (0 < max)
			/* put the value of SQL_ATTR_MAX_LENGTH attribute..  even
			 * if this would be larger than what the data actually
			 * occupies after conversion: "the driver has no way of
			 * figuring out what the actual length is" */
			*octet_len_ptr = max;
		else
			/* if no "network" truncation done, indicate data's lenght, no
			 * matter if truncated to buffer's size or not */
			*octet_len_ptr = have;
	}

	return to_copy;
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
	SQLTCHAR wbuff[sizeof("18446744073709551616")]; /* = 1 << 8*8 */
	size_t tocopy;
	esodbc_state_et state = SQL_STATE_00000;

	stmt = arec->desc->stmt;
	ird = stmt->ird;
	ard = stmt->ard;

	get_deferred_buffers(arec, pos, &data_ptr, (void **)&octet_len_ptr);
	if (! data_ptr) {
		ERR("STMT@0x%p: received integer type, but NULL data ptr.", stmt);
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
			tocopy = bytes_to_copy(strlen(buff) + /* \0 */1, 
					arec->octet_length, stmt->max_length, octet_len_ptr, 
					&state);
			memcpy(data_ptr, buff, tocopy);
			break;
		case SQL_C_SLONG:
		case SQL_C_SSHORT:
			*(SQLINTEGER *)data_ptr = (SQLINTEGER)ll;
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

/* convert count of wchar characters to count of bytes, taking into account the
 * truncation requried by SQL_ATTR_MAX_LENGTH statement attribute */
static size_t trunc_len_w2b(desc_rec_st *irec, size_t chars, BOOL *trunc)
{
	esodbc_stmt_st *stmt = irec->desc->stmt;
	size_t in_bytes = chars * sizeof(wchar_t);
	/* truncate to statment max bytes, if "the column contains character or
	 * binary data" */
	if (stmt->max_length && stmt->max_length < in_bytes) {
		switch (irec->type) {
			case SQL_CHAR:
			case SQL_VARCHAR:
			case SQL_LONGVARCHAR:
			case SQL_WCHAR:
			case SQL_WVARCHAR:
			case SQL_WLONGVARCHAR:
			case SQL_BINARY:
			case SQL_VARBINARY:
			case SQL_LONGVARBINARY:
				DBG("STMT@0x%p truncation: from %d bytes...", stmt, in_bytes);
				in_bytes = stmt->max_length;
				/* discard left bytes that otherwise wouldn't make a complete
				 * wchar_t */
				if (in_bytes % sizeof(wchar_t))
					in_bytes -= in_bytes % sizeof(wchar_t);
				DBG("STMT@0x%p truncation: ...to %d bytes", stmt, in_bytes);
				*trunc = TRUE;
				break;
			default:
				*trunc = FALSE;
		}
	} else {
		*trunc = FALSE;
	}

	return in_bytes;
}

static SQLRETURN wstr_to_cstr(desc_rec_st *arec, desc_rec_st *irec, 
		void *data_ptr, SQLLEN *octet_len_ptr,
		const wchar_t *wstr, size_t chars)
{
	esodbc_state_et state = SQL_STATE_00000;
	esodbc_stmt_st *stmt;
	char *charp;
	size_t in_bytes, out_bytes;
	size_t octet_length; /* need it for comparision to -1 (w/o int promotion)*/
	BOOL was_truncated; /* TODO: use bytes_to_copy() */


	stmt = arec->desc->stmt;
	in_bytes = trunc_len_w2b(irec, chars, &was_truncated);

	if (octet_len_ptr) {
		if (was_truncated) {
			/* put the value of SQL_ATTR_MAX_LENGTH attribute..  even
			 * if this would be larger than what the data actually
			 * occupies after conversion: "the driver has no way of
			 * figuring out what the actual length is" */
			octet_length = stmt->max_length;
		} else {
			/* the space it would take if target buffer was large
			 * enough, after _ATTR_MAX_LENGTH truncation, but before
			 * buffer size truncation. */
			// FIXME: use WideCharToMultiByte on _WIN32
			octet_length = wcstombs(NULL, wstr, in_bytes);
			if (octet_length == (size_t)-1) {
				ERRN("failed to convert wchar* to char* for string `"
						LTPDL "`.", chars, wstr);
				RET_HDIAGS(stmt, SQL_STATE_22018);
			}
			octet_length ++; /* wcstombs doesn't count the \0 */
		}
		*octet_len_ptr = octet_length;
		DBG("REC@0x%p, octet_lenght_ptr@0x%p, value=%zd.",
				arec, octet_len_ptr, octet_length);
	} else {
		DBG("REC@0X%p: NULL octet_length_ptr.", arec);
	}

	if (data_ptr) {
		charp = (char *)data_ptr;

		/* type is signed, driver should not allow a negative to this point. */
		assert(0 <= arec->octet_length);
		octet_length = (size_t)arec->octet_length;

		/* "[R]eturns the number of bytes written into the multibyte
		 * output string, excluding the terminating NULL (if any)".
		 * Copies until \0 is met in wstr or buffer runs out.
		 * If \0 is met, it's copied, but not counted in return. (silly fn) */
		// FIXME: use WideCharToMultiByte on _WIN32
		out_bytes = wcstombs(charp, wstr, octet_length);
		if (out_bytes == (size_t)-1) {
			ERRN("failed to convert wchar* to char* for string `"
					LTPDL "`.", chars, wstr);
			RET_HDIAGS(stmt, SQL_STATE_22018);
		}
		/* 0-terminate, if not done */
		/* "[T]he multibyte character string at mbstr is null-terminated
		 * only if wcstombs encounters a wide-character null character
		 * during conversion." */
		if (octet_length <= out_bytes) { /* == */
			/* ran out of buffer => not 0-terminated and truncated already */
			state = SQL_STATE_01004; /* indicate truncation */
			/* truncate further, by 'attr max len', if needed */
			out_bytes = in_bytes < octet_length ? in_bytes : octet_length;
			charp[out_bytes - 1] = 0;
		} else {
			/* buffer was enough => 0-term'd already, BUT: must truncate? */
			out_bytes ++; /* b/c wcstombs doesn't count the 0-term */
			if (in_bytes < out_bytes) { /* yes, truncate by 'attr max len' */
				state = SQL_STATE_01004; /* indicate truncation */
				out_bytes = in_bytes;
				charp[out_bytes - 1] = 0;
			} /* else: no */
		}

		DBG("REC@0x%p, data_ptr@0x%p, copied %zd bytes: `%s`.", arec,
				data_ptr, out_bytes, charp);
	} else {
		DBG("REC@0x%p, NULL data_ptr", arec);
	}

	RET_HDIAGS(stmt, state);
}


static SQLRETURN wstr_to_wstr(desc_rec_st *arec, desc_rec_st *irec, 
		void *data_ptr, SQLLEN *octet_len_ptr,
		const wchar_t *wstr, size_t chars)
{
	esodbc_stmt_st *stmt;
	esodbc_state_et state = SQL_STATE_00000;
	size_t in_bytes, out_bytes;
	size_t octet_length;
	BOOL was_truncated;
	wchar_t *widep;


	stmt = arec->desc->stmt;
	in_bytes = trunc_len_w2b(irec, chars, &was_truncated);

	if (octet_len_ptr) {
		/* see explanations from wstr_to_cstr */
		*octet_len_ptr = was_truncated ? stmt->max_length : in_bytes;
	}

	if (data_ptr) {
		widep = (wchar_t *)data_ptr;

		assert(0 <= arec->octet_length);
		octet_length = (size_t)arec->octet_length;

		if (in_bytes < octet_length) { /* enough buffer */
			out_bytes = in_bytes;
			memcpy(widep, wstr, out_bytes);
			if (was_truncated)
				state = SQL_STATE_01004; /* indicate truncation */
		} else {
			out_bytes = octet_length - /* space for 0-term */sizeof(wchar_t);
			memcpy(widep, wstr, out_bytes);
			widep[out_bytes] = 0;
			state = SQL_STATE_01004; /* indicate truncation */
		}
		DBG("REC@0x%p, data_ptr@0x%p, copied %zd bytes: `%s`.", arec,
				data_ptr, out_bytes, widep);
	}
	
	RET_HDIAGS(stmt, state);
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
	
	get_deferred_buffers(arec, pos, &data_ptr, (void **)&octet_len_ptr);

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
		return post_row_diagnostic(&stmt->diag, _state, MK_TSTR(_message), 0, \
				rowno, _colno); \
	} while (0)
#define SET_ROW_DIAG(_rowno, _colno) \
	do { \
		stmt->diag.row_number = _rowno; \
		stmt->diag.column_number = _colno; \
	} while (0)

	if (! UJIsArray(row)) {
		ERR("one '%s' (#%zd) element in result set not array; type: %d.", 
				JSON_ANSWER_ROWS, stmt->rset.vrows, UJGetType(row));
		RET_ROW_DIAG(SQL_STATE_01S01, MSG_INV_SRV_ANS, 
				SQL_NO_COLUMN_NUMBER);
	}
	iter_row = UJBeginArray(row);
	if (! iter_row) {
		ERR("Failed to obtain iterator on row (#%zd): %s.", rowno,
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
					ERR("STMT@0x%p: received a NULL for a not nullable val.");
					RET_ROW_DIAG(SQL_STATE_HY003, "NULL value received for non"
							" nullable data type", i + 1);
				}
#endif //0
				ind_len = deferred_address(SQL_DESC_INDICATOR_PTR, pos, arec);
				if (! ind_len) {
					ERR("STMT@0x%p: no buffer to signal NULL value.", stmt);
					RET_ROW_DIAG(SQL_STATE_22002, "Indicator variable required"
							" but not supplied", i + 1);
				}
				*ind_len = SQL_NULL_DATA;
				break;

			case UJT_String:
				wstr = UJReadString(obj, &len);
				DBG("value [%zd, %d] is string: `" LTPD "`.", rowno, i + 1,
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
				DBG("value [%zd, %d] is int: %lld.", rowno, i + 1, ll);
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
			DBG("empty result set flag set - returning no data.");
			return SQL_NO_DATA;
		}
		ERR("no resultset available on statement 0x%p.", stmt);
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	DBG("STMT@0x%p (`%.*s`); 'cursor' @ %zd / %zd.", stmt, stmt->sqllen,
			stmt->u8sql, stmt->rset.vrows, stmt->rset.nrows);
	
	DBG("rowset max size: %d.", ard->array_size);
	errors = 0;
	/* for all rows in rowset/array, iterate over rows in current resultset */
	for (i = stmt->rset.array_pos; i < ard->array_size; i ++) {
		if (! UJIterArray(&stmt->rset.rows_iter, &row)) {
			DBG("ran out of rows in current result set: nrows=%zd, vrows=%zd.",
					stmt->rset.nrows, stmt->rset.vrows);
			if (0) { // TODO: if having Elastic/SQL "cursor" ... // FIXME
				stmt->rset.array_pos = i;
				// TODO: ...issue another post_sql, then attach_answer()
				return EsSQLFetch(StatementHandle);
			} else {
				DBG("reached end of entire result set. fetched=%zd.",
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
			ERR("copying row %zd failed.", stmt->rset.vrows + i + 1);
			errors ++;
		}
	}
	stmt->rset.array_pos = 0;

	/* account for processed rows */
	stmt->rset.vrows += i;
	stmt->rset.frows += i;

	/* return number of processed rows (even if 0) */
	if (ird->rows_processed_ptr) {
		DBG("setting number of processed rows to: %u.", i);
		*ird->rows_processed_ptr = i;
	}

	if (i <= 0) {
		DBG("no data %sto return.", stmt->rset.vrows ? "left ": "");
		return SQL_NO_DATA;
	}
	
	if (errors && i <= errors) {
		ERR("processing failed for all rows [%d].", errors);
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
		ERR("no open cursor for statement 0x%p.", stmt);
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

	DBG("STMT@0x%p preparing `" LTPDL "` (%d)", stmt, cchSqlStr, szSqlStr, 
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

	DBG("STMT@0x%p executing `%.*s` (%zd)", stmt, stmt->sqllen, stmt->u8sql,
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

	DBG("STMT@0x%p directly executing SQL.", stmt);
	
	ret = EsSQLFreeStmt(stmt, ESODBC_SQL_CLOSE);
	assert(SQL_SUCCEEDED(ret)); /* can't return error */

	if (stmt->apd->array_size)
		FIXME; //FIXME: multiple executions?

	ret = attach_sql(stmt, szSqlStr, cchSqlStr);
	if (SQL_SUCCEEDED(ret)) {
		ret = post_statement(stmt);
	}
	detach_sql(stmt);
	return ret;
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
	SQLSMALLINT col_len = -1;

	DBG("STMT@0x%p, IRD@0x%p, column #%d.", stmt, stmt->ird, icol);

	if (! STMT_HAS_RESULTSET(stmt)) {
		ERR("no resultset available on statement 0x%p.", stmt);
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	if (icol < 1) {
		/* TODO: if implementing bookmarks */
		RET_HDIAGS(stmt, SQL_STATE_HYC00);
	}
	
	rec = get_record(stmt->ird, icol, FALSE);
	if (! rec) {
		ERR("STMT@0x%p: no record for columns #%d.", stmt, icol);
		RET_HDIAGS(stmt, SQL_STATE_07009);
	}

	if (szColName) {
		ret = write_tstr(&stmt->diag, szColName, rec->name, 
				cchColNameMax * sizeof(SQLTCHAR), &col_len);
		if (! SQL_SUCCEEDED(ret)) {
			ERR("STMT@0x%p: failed to copy column name `" LTPD "`.", stmt, 
					rec->name);
			return ret;
		}
	} else {
		DBG("STMT@0x%p: NULL column name buffer provided.", stmt);
	}

	if (! pcchColName) {
		/* TODO: STMT_ERR -> ERR("STMT@...", stmt, ...) */
		ERR("STMT@0x%p, no column name lenght buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090, 
				"no column name lenght buffer provided", 0);
	}
	*pcchColName = 0 <= col_len ? col_len : (SQLSMALLINT)wcslen(rec->name);

	if (! pfSqlType) {
		ERR("STMT@0x%p, no column data type buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090, 
				"no column data type buffer provided", 0);
	}
	*pfSqlType = rec->concise_type;

	if (! pcbColDef) {
		ERR("STMT@0x%p, no column size buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090, "no column size buffer provided", 0);
	}
	*pcbColDef = 0; // TODO: concise to size

	if (! pibScale) {
		ERR("STMT@0x%p, no column decimal digits buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090, 
				"no column decimal digits buffer provided", 0);
	}
	*pibScale = 0; // TODO: concise to decimal digits

	if (! pfNullable) {
		ERR("STMT@0x%p, no column decimal digits buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090, 
				"no column decimal digits buffer provided", 0);
	}
	/* TODO: this would be available in SQLColumns resultset. */
	*pfNullable = SQL_NULLABLE_UNKNOWN;

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
	SQLTCHAR *tstr;
	SQLLEN len;
	SQLINTEGER iint;

#ifdef _WIN64
#define PNUMATTR_ASSIGN(type, value) *pNumAttr = (SQLLEN)(value)
#else /* _WIN64 */
#define PNUMATTR_ASSIGN(type, value) *(type *)pNumAttr = (type)(value)
#endif /* _WIN64 */

	DBG("STMT@0x%p, IRD@0x%p, column #%d, field: %d.", stmt, ird, iCol,iField);

	if (! STMT_HAS_RESULTSET(stmt)) {
		ERR("no resultset available on statement 0x%p.", stmt);
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	if (iCol < 1) {
		/* TODO: if implementing bookmarks */
		RET_HDIAGS(stmt, SQL_STATE_HYC00);
	}
	
	rec = get_record(ird, iCol, FALSE);
	if (! rec) {
		ERR("STMT@0x%p: no record for columns #%d.", stmt, iCol);
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

		/* SQLTCHAR* */
		do {
		case SQL_DESC_BASE_COLUMN_NAME: tstr = rec->base_column_name; break;
		case SQL_DESC_BASE_TABLE_NAME: tstr = rec->base_table_name; break;
		case SQL_DESC_CATALOG_NAME: tstr = rec->catalog_name; break;
		case SQL_DESC_LABEL: tstr = rec->label; break;
		case SQL_DESC_LITERAL_PREFIX: tstr = rec->literal_prefix; break;
		case SQL_DESC_LITERAL_SUFFIX: tstr = rec->literal_suffix; break;
		case SQL_DESC_LOCAL_TYPE_NAME: tstr = rec->type_name; break;
		case SQL_DESC_NAME: tstr = rec->name; break;
		case SQL_DESC_SCHEMA_NAME: tstr = rec->schema_name; break;
		case SQL_DESC_TABLE_NAME: tstr = rec->table_name; break;
		case SQL_DESC_TYPE_NAME: tstr = rec->type_name; break;
		} while (0);
			if (! tstr) {
				//BUG -- TODO: re-eval, once type handling is decided.
				ERR("STMT@0x%p IRD@0x%p record field type %d not initialized.",
						stmt, ird, iField);
				*(SQLTCHAR **)pCharAttr = MK_TSTR("");
				*pcbCharAttr = 0;
			} else {
				return write_tstr(&stmt->diag, pcbCharAttr, tstr, cbDescMax,
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
		case SQL_DESC_AUTO_UNIQUE_VALUE: iint = rec->auto_unique_value;
		case SQL_DESC_CASE_SENSITIVE: iint = rec->case_sensitive;
		case SQL_DESC_NUM_PREC_RADIX: iint = rec->num_prec_radix;
		} while (0);
			PNUMATTR_ASSIGN(SQLINTEGER, iint);
			break;


		case SQL_DESC_COUNT:
			PNUMATTR_ASSIGN(SQLSMALLINT, ird->count);
			break;

		default:
			ERR("STMT@0x%p: unknown field type %d.", stmt, iField);
			RET_HDIAGS(stmt, SQL_STATE_HY091);
	}

	return SQL_SUCCESS;
#undef PNUMATTR_ASSIGN
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
