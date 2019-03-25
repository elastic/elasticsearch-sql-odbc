/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */
#ifndef __QUERIES_H__
#define __QUERIES_H__

#include "error.h"
#include "handles.h"

BOOL TEST_API queries_init();
void clear_resultset(esodbc_stmt_st *stmt, BOOL on_close);
SQLRETURN TEST_API attach_answer(esodbc_stmt_st *stmt, char *buff,
	size_t blen);
SQLRETURN TEST_API attach_error(SQLHANDLE hnd, cstr_st *body, int code);
SQLRETURN TEST_API attach_sql(esodbc_stmt_st *stmt, const SQLWCHAR *sql,
	size_t tlen);
void detach_sql(esodbc_stmt_st *stmt);
esodbc_estype_st *lookup_es_type(esodbc_dbc_st *dbc,
	SQLSMALLINT es_type, SQLULEN col_size);
SQLRETURN TEST_API serialize_statement(esodbc_stmt_st *stmt, cstr_st *buff);
SQLRETURN close_es_cursor(esodbc_stmt_st *stmt);
SQLRETURN close_es_answ_handler(esodbc_stmt_st *stmt, char *buff, size_t blen);


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

/* JSON body build elements */
#define JSON_KEY_QUERY			"\"query\": " /* will always be the 1st key */
#define JSON_KEY_CURSOR			"\"cursor\": " /* 1st key */
#define JSON_KEY_PARAMS			", \"params\": " /* n-th key */
#define JSON_KEY_FETCH			", \"fetch_size\": " /* n-th key */
#define JSON_KEY_REQ_TOUT		", \"request_timeout\": " /* n-th key */
#define JSON_KEY_PAGE_TOUT		", \"page_timeout\": " /* n-th key */
#define JSON_KEY_TIME_ZONE		", \"time_zone\": " /* n-th key */
#define JSON_KEY_VAL_MODE		", \"mode\": \"ODBC\"" /* n-th key */
#ifdef _WIN64
#	define JSON_KEY_CLT_ID		", \"client_id\": \"odbc64\"" /* n-th k. */
#else /* _WIN64 */
#	define JSON_KEY_CLT_ID		", \"client_id\": \"odbc32\"" /* n-th k. */
#endif /* _WIN64 */
#define JSON_KEY_MULTIVAL		", \"field_multi_value_leniency\": " /* n-th */
#define JSON_KEY_TIMEZONE		", \"time_zone\": " /* n-th key */


#endif /* __QUERIES_H__ */
