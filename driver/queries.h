/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */
#ifndef __QUERIES_H__
#define __QUERIES_H__

#include "error.h"
#include "handles.h"

BOOL queries_init();
void clear_resultset(esodbc_stmt_st *stmt, BOOL on_close);
SQLRETURN TEST_API attach_answer(esodbc_stmt_st *stmt, cstr_st *answer,
	BOOL is_json);
SQLRETURN TEST_API attach_error(SQLHANDLE hnd, cstr_st *body, BOOL is_json,
	long code);
SQLRETURN TEST_API attach_sql(esodbc_stmt_st *stmt, const SQLWCHAR *sql,
	size_t tlen);
void detach_sql(esodbc_stmt_st *stmt);
esodbc_estype_st *lookup_es_type(esodbc_dbc_st *dbc,
	SQLSMALLINT es_type, SQLULEN col_size);
SQLRETURN TEST_API serialize_statement(esodbc_stmt_st *stmt, cstr_st *buff);
SQLRETURN close_es_cursor(esodbc_stmt_st *stmt);
SQLRETURN close_es_answ_handler(esodbc_stmt_st *stmt, cstr_st *body,
	BOOL is_json);


SQLRETURN EsSQLBindCol(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT ColumnNumber,
	SQLSMALLINT TargetType,
	_Inout_updates_opt_(_Inexpressible_(BufferLength))
	SQLPOINTER TargetValue,
	SQLLEN BufferLength,
	_Inout_opt_ SQLLEN *StrLen_or_Ind);
SQLRETURN EsSQLFetch(SQLHSTMT StatementHandle);
SQLRETURN EsSQLFetchScroll(SQLHSTMT StatementHandle,
	SQLSMALLINT FetchOrientation, SQLLEN FetchOffset);
SQLRETURN EsSQLGetData(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT ColumnNumber,
	SQLSMALLINT TargetType,
	_Out_writes_opt_(_Inexpressible_(BufferLength)) SQLPOINTER TargetValue,
	SQLLEN BufferLength,
	_Out_opt_ SQLLEN *StrLen_or_IndPtr);
SQLRETURN EsSQLSetPos(
	SQLHSTMT        StatementHandle,
	SQLSETPOSIROW   RowNumber,
	SQLUSMALLINT    Operation,
	SQLUSMALLINT    LockType);
SQLRETURN EsSQLBulkOperations(
	SQLHSTMT            StatementHandle,
	SQLSMALLINT         Operation);
SQLRETURN EsSQLMoreResults(SQLHSTMT hstmt);
SQLRETURN EsSQLCloseCursor(SQLHSTMT StatementHandle);
SQLRETURN EsSQLCancel(SQLHSTMT StatementHandle);
SQLRETURN EsSQLCancelHandle(SQLSMALLINT HandleType, SQLHANDLE InputHandle);
SQLRETURN EsSQLEndTran(SQLSMALLINT HandleType, SQLHANDLE Handle,
	SQLSMALLINT CompletionType);
SQLRETURN EsSQLNumResultCols(SQLHSTMT StatementHandle,
	_Out_ SQLSMALLINT *ColumnCount);

SQLRETURN EsSQLPrepareW(
	SQLHSTMT    hstmt,
	_In_reads_(cchSqlStr) SQLWCHAR *szSqlStr,
	SQLINTEGER  cchSqlStr);
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
	SQLLEN         *StrLen_or_IndPtr);
SQLRETURN EsSQLExecute(SQLHSTMT hstmt);
SQLRETURN EsSQLExecDirectW(
	SQLHSTMT    hstmt,
	_In_reads_opt_(TextLength) SQLWCHAR *szSqlStr,
	SQLINTEGER cchSqlStr);
SQLRETURN EsSQLNativeSqlW(
	SQLHDBC                                     hdbc,
	_In_reads_(cchSqlStrIn) SQLWCHAR           *szSqlStrIn,
	SQLINTEGER                                  cchSqlStrIn,
	_Out_writes_opt_(cchSqlStrMax) SQLWCHAR    *szSqlStr,
	SQLINTEGER                                  cchSqlStrMax,
	SQLINTEGER                                 *pcchSqlStr);

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
	SQLSMALLINT        *pfNullable);
SQLRETURN EsSQLColAttributeW(
	SQLHSTMT        hstmt,
	SQLUSMALLINT    iCol,
	SQLUSMALLINT    iField,
	_Out_writes_bytes_opt_(cbDescMax)
	SQLPOINTER      pCharAttr,
	SQLSMALLINT     cbDescMax,
	_Out_opt_
	SQLSMALLINT     *pcbCharAttr,
	_Out_opt_
#ifdef _WIN64
	SQLLEN          *pNumAttr
#else /* _WIN64 */
	SQLPOINTER      pNumAttr
#endif /* _WIN64 */
);
SQLRETURN EsSQLNumParams(
	SQLHSTMT           StatementHandle,
	_Out_opt_
	SQLSMALLINT       *ParameterCountPtr);
SQLRETURN EsSQLRowCount(_In_ SQLHSTMT StatementHandle, _Out_ SQLLEN *RowCount);

/*
 * REST request parameters
 */
#define REQ_KEY_QUERY			"query"
#define REQ_KEY_CURSOR			"cursor"
#define REQ_KEY_PARAMS			"params"
#define REQ_KEY_FETCH			"fetch_size"
#define REQ_KEY_REQ_TOUT		"request_timeout"
#define REQ_KEY_PAGE_TOUT		"page_timeout"
#define REQ_KEY_TIME_ZONE		"time_zone"
#define REQ_KEY_MODE			"mode"
#define REQ_KEY_CLT_ID			"client_id"
#define REQ_KEY_MULTIVAL		"field_multi_value_leniency"
#define REQ_KEY_IDX_FROZEN		"index_include_frozen"
#define REQ_KEY_TIMEZONE		"time_zone"

#define REST_REQ_KEY_COUNT		11 /* "query" or "cursor" */

#ifdef _WIN64
#	define REQ_VAL_CLT_ID		"odbc64"
#else
#	define REQ_VAL_CLT_ID		"odbc32"
#endif
#define REQ_VAL_MODE			"ODBC"
#define REQ_VAL_TIMEZONE_Z		"Z"

/* JSON body building blocks */
#define JSON_KEY_QUERY			"\"" REQ_KEY_QUERY "\": " /* 1st key */
#define JSON_KEY_CURSOR			"\"" REQ_KEY_CURSOR "\": " /* 1st key */
#define JSON_KEY_PARAMS			", \"" REQ_KEY_PARAMS "\": " /* n-th key */
#define JSON_KEY_FETCH			", \"" REQ_KEY_FETCH "\": " /* n-th key */
#define JSON_KEY_REQ_TOUT		", \"" REQ_KEY_REQ_TOUT "\": " /* n-th key */
#define JSON_KEY_PAGE_TOUT		", \"" REQ_KEY_PAGE_TOUT "\": " /* n-th key */
#define JSON_KEY_TIME_ZONE		", \"" REQ_KEY_TIME_ZONE "\": " /* n-th key */
#define JSON_KEY_VAL_MODE		", \"" REQ_KEY_MODE "\": \"" \
	REQ_VAL_MODE "\"" /* n-th key */
#define JSON_KEY_CLT_ID			", \"" REQ_KEY_CLT_ID "\": \"" \
	REQ_VAL_CLT_ID "\"" /* n-th k. */
#define JSON_KEY_MULTIVAL		", \"" REQ_KEY_MULTIVAL "\": " /* n-th */
#define JSON_KEY_IDX_FROZEN		", \"" REQ_KEY_IDX_FROZEN "\": " /* n-th */
#define JSON_KEY_TIMEZONE		", \"" REQ_KEY_TIMEZONE "\": " /* n-th key */

#define JSON_VAL_TIMEZONE_Z		"\"" REQ_VAL_TIMEZONE_Z "\""


#endif /* __QUERIES_H__ */
