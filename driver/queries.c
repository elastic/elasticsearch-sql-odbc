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

#include "queries.h"
#include "handles.h"


void clear_resultset(esodbc_stmt_st *stmt)
{
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
 * Takes a dynamic buffer, buff, of length blen. Will handle the buff memory
 * even if the call fails.
 */
SQLRETURN attach_answer(esodbc_stmt_st *stmt, char *buff, size_t blen)
{
	UJObject obj, columns, rows;
	wchar_t *keys[] = {
		MK_WSTR(JSON_ANSWER_COLUMNS), 
		MK_WSTR(JSON_ANSWER_ROWS) 
	};

	/* IRD and statement should be cleaned by closing the statement */
	assert(stmt->ird->recs == NULL);
	assert(stmt->rset.buff == NULL);

	/* the statement takes ownership of mem obj */
	stmt->rset.buff = buff;
	stmt->rset.blen = blen;

	obj = UJDecode(buff, blen, NULL, &stmt->rset.state);
	if (! obj) {
		ERR("failed to decode JSON answer (`%.*s`): %s.", blen, buff, 
				stmt->rset.state ? UJGetError(stmt->rset.state) : "<none>");
		RET_HDIAG(stmt, SQL_STATE_HY000, "Invalid server answer", 0);
	}
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
	stmt->rset.nrows = UJArraySize(rows);
	stmt->rset.cursor = 0;
	DBG("rows received: %zd.", stmt->rset.nrows);

	return attach_columns(stmt, columns);
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

	/* "Sets the SQL_DESC_DATA_PTR field to the value of TargetValue." */
	ret = EsSQLSetDescFieldW(ard, ColumnNumber, SQL_DESC_DATA_PTR,
			TargetValue, SQL_IS_POINTER);
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

	return SQL_SUCCESS;

copy_ret:
	/* copy error at top handle level, where it's going to be inquired from */
	HDIAG_COPY(ard, stmt);
	return ret;
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
	SQLSMALLINT i;
	UJObject obj, row;
	size_t len;
	void *iter_row;

	stmt = STMH(StatementHandle);
	ard = stmt->ard;
	ird = stmt->ird;

	DBG("fetching on statement 0x%p; cursor @ %zd / %zd.", stmt, 
			stmt->rset.cursor, stmt->rset.nrows);

	// FIXME: consider STMH(StatementHandle)->options.bind_offset
	// FIXME: consider STMH(StatementHandle)->options.array_size
	
#if 0
	do {
		while (UJIterArray(&stmt->rset.rows_iter, &row)) {
			if (UJIsArray(row)) {
				DBG("got array of size: %zd.", UJArraySize(row));
			} else {
				ERR("got instead: %d", UJGetType(row));
			}
			iter_row = UJBeginArray(row);
			if (! iter_row) {
				ERR("no iter.");
				break;
			}
			while (UJIterArray(&iter_row, &obj)) {
				if (UJIsString(obj))
					DBG("got string: `" LTPD "`.", UJReadString(obj, &len));
				else if (UJIsInteger(obj))
					DBG("got integer: %d.", UJNumericInt(obj));
				else
					DBG("no idea");
			}
		}
	} while (0);
#endif

	if (stmt->rset.nrows <= stmt->rset.cursor) {
		DBG("cursor past result set size => no more data");
		return SQL_NO_DATA;
	}

	if (! UJIterArray(&stmt->rset.rows_iter, &row)) {
		BUG("Unexpected end of iterator: cursor @ %zd / %zd.",
			stmt->rset.cursor, stmt->rset.nrows);
		return SQL_NO_DATA;
	}
	if (! UJIsArray(row)) {
		ERR("element '%s' in result set not array; type: %d.", 
				JSON_ANSWER_ROWS, UJGetType(row));
		RET_HDIAG(stmt, SQL_STATE_01S01, "Invalid server answer", 0);
	}
	iter_row = UJBeginArray(row);
	if (! iter_row) {
		ERR("Failed to obtain iterator on row: %s.", 
				UJGetError(stmt->rset.state));
		RET_HDIAG(stmt, SQL_STATE_01S01, "Invalid server answer", 0);
	}

	/* iterate over the contents of one row */
	for (i = 0; i < ard->count && UJIterArray(&iter_row, &obj); i ++) {
		/* if record not bound skip it */
		if (! ard->recs[i].data_ptr) {
			DBG("column #%d not bound.", i + 1);
			continue;
		}
		if (UJIsNull(obj))
			DBG("got NULL.");
		else if (UJIsString(obj))
			DBG("got string: `" LTPD "`.", UJReadString(obj, &len));
		else if (UJIsInteger(obj))
			DBG("got integer: %d.", UJNumericInt(obj));
		else
			DBG("received object of type: %d", UJGetType(row));
	}


	stmt->rset.cursor ++;

	
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
	if (! stmt->rset.buff)
		RET_HDIAGS(stmt, SQL_STATE_24000);
	clear_resultset(stmt);
	reinit_desc(stmt->ird);
	RET_STATE(SQL_STATE_00000);
}


