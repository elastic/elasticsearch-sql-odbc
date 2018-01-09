/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <assert.h>
#include "queries.h"
#include "handles.h"

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
 */
SQLRETURN EsSQLBindCol(
		SQLHSTMT StatementHandle,
		SQLUSMALLINT ColumnNumber,
		SQLSMALLINT TargetType,
		_Inout_updates_opt_(_Inexpressible_(BufferLength)) 
				SQLPOINTER TargetValue,
		SQLLEN BufferLength,
		_Inout_opt_ SQLLEN *StrLen_or_Ind)
{

	if (BufferLength < 0) {
		ERR("invalid negative BufferLength: %d.", BufferLength);
		RET_HDIAGS(STMH(StatementHandle), SQL_STATE_HY090);
	}

	if (STMH(StatementHandle)->options.bookmarks != SQL_UB_OFF) {
		/* "The statement attribute SQL_ATTR_USE_BOOKMARKS should always be
		 * set before binding a column to column 0. This is not required but
		 * is strongly recommended." */
		BUG("not implemented.");
		//RET_HDIAGS(STMH(StatementHandle), SQL_STATE_IM001);
		// FIXME: implement bookmarks
		assert(0);
	}

	// FIXME: consider STMH(StatementHandle)->options.bind_offset
	// FIXME: consider STMH(StatementHandle)->options.array_size



	BUG("not implemented.");
	//RET_NOT_IMPLEMENTED;
	return SQL_SUCCESS;
}

/*
 * "SQLFetch and SQLFetchScroll use the rowset size at the time of the call to
 * determine how many rows to fetch."
 */
SQLRETURN EsSQLFetch(SQLHSTMT StatementHandle)
{
	RET_NOT_IMPLEMENTED;
}

/*
 * "SQLSetPos uses the rowset size that is in effect as of the preceding call
 * to SQLFetch or SQLFetchScroll, because SQLSetPos operates on a rowset that
 * has already been set. SQLSetPos also will pick up the new rowset size if
 * SQLBulkOperations has been called after the rowset size was changed."
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
			BUG("not yet implemented");
			assert(0);
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
}

/*
 * == JDBC's Jdbc/PreparedStatement.executeLargeUpdate()
 * "SQLBulkOperations uses the rowset size in effect at the time of the call,
 * because it performs operations on a table independent of any fetched
 * rowset."
 */
SQLRETURN EsSQLBulkOperations(
		SQLHSTMT            StatementHandle,
		SQLSMALLINT         Operation)
{
	ERR("data update functions not supported");
	RET_HDIAGS(STMH(StatementHandle), SQL_STATE_IM001);
}
