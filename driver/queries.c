/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include "queries.h"
#include "handles.h"


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
		case sizeof(JSON_COL_INTEGER) - 1:
			if (wmemncasecmp(type_name, MK_WSTR(JSON_COL_INTEGER), len) == 0)
				// TODO: take in the precision for a better representation
				return SQL_INTEGER;
			break;
		// case sizeof(JSON_COL_DATE):
		case sizeof(JSON_COL_TEXT) - 1: 
			if (wmemncasecmp(type_name, MK_WSTR(JSON_COL_TEXT), len) == 0)
				// TODO: char/longvarchar/wchar/wvarchar?
				return SQL_VARCHAR;
			else if (wmemncasecmp(type_name, MK_WSTR(JSON_COL_DATE), len) == 0)
				// TODO: time/timestamp
				return SQL_TYPE_DATE;
	}
	ERR("unrecognized Elastic type `" LTPDL "`.", len, type_name);
	return SQL_UNKNOWN_TYPE;
}


static SQLRETURN attach_columns(esodbc_stmt_st *stmt, UJObject columns)
{
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
	DBG("columns received: %zd.", ncols);
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
		RET_HDIAG(stmt, SQL_STATE_HY000, "Invalid server answer", 0);
	}
	recno = 0;
	while (UJIterArray(&iter, &col_o)) {
		if (UJObjectUnpack(col_o, 2, "SS", keys, &name_o, &type_o) < 2) {
			ERR("failed to decode JSON column: %s.", 
					UJGetError(stmt->rset.state));
			RET_HDIAG(stmt, SQL_STATE_HY000, "Invalid server answer", 0);
		}

		col_wname = UJReadString(name_o, &len);
		ird->recs[recno].base_column_name = (SQLTCHAR *)col_wname;

		col_wtype = UJReadString(type_o, &len);
		col_stype = type_elastic2csql(col_wtype, len);
		if (col_stype == SQL_UNKNOWN_TYPE) {
			ERR("failed to convert Elastic to C SQL type `" LTPDL "`.", 
					len, col_wtype);
			RET_HDIAG(stmt, SQL_STATE_HY000, "Invalid server answer", 0);
		}
		ird->recs[recno].type = col_stype;

		DBG("column #%d: name=`" LTPD "`, type=%d (`" LTPD "`).", recno, 
				col_wname, col_stype, col_wtype);
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
		RET_HDIAG(stmt, SQL_STATE_HY000, "Invalid server answer", 0);
	}
	/* extract the columns and rows objects */
    if (UJObjectUnpack(obj, 2, "AA", keys, &columns, &rows) < 2) {
		ERR("failed to unpack JSON answer (`%.*s`): %s.", blen, buff, 
				stmt->rset.state ? UJGetError(stmt->rset.state) : "<none>");
		RET_HDIAG(stmt, SQL_STATE_HY000, "Invalid server answer", 0);
	}

	/* set the cursor */
	stmt->rset.rows_iter = UJBeginArray(rows);
	if (! stmt->rset.rows_iter) {
		ERR("failed to get iterrator on received rows: %s.", 
				UJGetError(stmt->rset.state));
		RET_HDIAGS(stmt, SQL_STATE_HY000);
	}
	stmt->rset.nrows = (size_t)UJArraySize(rows);
	DBG("rows received in result set: %zd.", stmt->rset.nrows);

	return attach_columns(stmt, columns);
}

/* 
 * tlen: lenght in characters of text
 */
SQLRETURN attach_sqltext(esodbc_stmt_st *stmt, SQLTCHAR *text, size_t tlen)
{
	size_t blen; /* Bytes len from Characters len */

	assert(stmt->text == NULL);

	blen = tlen * sizeof(SQLTCHAR); /* Bytes len from Characters len */
	/* attach the SQL textual statement to the handler */
	stmt->text = (SQLTCHAR *)malloc(blen);
	if (! stmt->text) {
		ERR("failed to allocate buffer for SQL text of size %zd.", blen);
		RET_HDIAGS(stmt, SQL_STATE_HY001);
	}
	memcpy(stmt->text, text, blen);
	stmt->tlen = (SQLINTEGER)tlen;
	DBG("attached SQL text `" LTPDL "` to statement 0x%p.", tlen, stmt->text);
	RET_STATE(SQL_STATE_00000);
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
 * pos: position in array/rowset (not result set)
 */
static void* deferred_address(SQLSMALLINT field_id, size_t pos,
		esodbc_desc_st *desc, desc_rec_st *rec)
{
	size_t elem_size;
	SQLLEN offt;
	void *base;

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
				elem_size = sizeof(*rec->indicator_ptr);
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

static SQLRETURN copy_longlong(esodbc_stmt_st *stmt, desc_rec_st *arec,
		desc_rec_st *irec, SQLULEN pos, long long ll)
{
	FIXME; // FIXME
	return SQL_ERROR;
}

static SQLRETURN copy_double(esodbc_stmt_st *stmt, desc_rec_st *arec,
		desc_rec_st *irec, SQLULEN pos, double dbl)
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
	BOOL was_truncated;


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
	SQLLEN *octet_len_ptr, *indicator_ptr;
	esodbc_desc_st *ard, *ird;
	SQLSMALLINT target_type;

	stmt = arec->desc->stmt;
	ird = stmt->ird;
	ard = stmt->ard;
	
	/* pointer where to write how many characters we will/would use */
	octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR, pos, ard,arec);
	indicator_ptr = deferred_address(SQL_DESC_INDICATOR_PTR, pos, ard, arec);
	/* only indicate length if have dedicated buffer for it */
	if (octet_len_ptr == indicator_ptr)
		octet_len_ptr = NULL;
	/* pointer to app's buffer */
	data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, ard, arec);

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
		RET_ROW_DIAG(SQL_STATE_01S01, "Invalid server answer", 
				SQL_NO_COLUMN_NUMBER);
	}
	iter_row = UJBeginArray(row);
	if (! iter_row) {
		ERR("Failed to obtain iterator on row (#%zd): %s.", rowno,
				UJGetError(stmt->rset.state));
		RET_ROW_DIAG(SQL_STATE_01S01, "Invalid server answer", 
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
				ind_len = deferred_address(SQL_DESC_INDICATOR_PTR, pos, ard, 
						arec);
				if (! ind_len) {
					ERR("no buffer to signal NULL value.");
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
				ret = copy_longlong(stmt, arec, irec, pos, ll);
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
				ret = copy_double(stmt, arec, irec, pos, dbl);
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
				RET_ROW_DIAG(SQL_STATE_01S01, "Invalid server answer", i + 1);
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

	if (stmt->text) // FIXME: remove it, it's just to avoid GetInfoType for now
	if (! STMT_HAS_RESULTSET(stmt)) {
		ERR("no resultset available on statement 0x%p.", stmt);
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	DBG("fetching on statement 0x%p (`" LTPDL "`); 'cursor' @ %zd / %zd.", 
			stmt, stmt->tlen, stmt->text, stmt->rset.vrows, stmt->rset.nrows);
	
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
