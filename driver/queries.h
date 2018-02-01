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
#ifndef __QUERIES_H__
#define __QUERIES_H__

#include "error.h"
#include "handles.h"

void clear_resultset(esodbc_stmt_st *stmt);
SQLRETURN attach_answer(esodbc_stmt_st *stmt, char *buff, size_t blen);
SQLRETURN attach_sqltext(esodbc_stmt_st *stmt, SQLTCHAR *text, size_t blen);

/* key names used in Elastic/SQL REST/JSON answers */
#define JSON_ANSWER_COLUMNS		"columns"
#define JSON_ANSWER_ROWS		"rows"
#define JSON_ANSWER_COL_NAME	"name"
#define JSON_ANSWER_COL_TYPE	"type"
#define JSON_COL_INTEGER		"integer"
#define JSON_COL_TEXT			"text"
#define JSON_COL_DATE			"date"

SQLRETURN EsSQLBindCol(
		SQLHSTMT StatementHandle,
		SQLUSMALLINT ColumnNumber,
		SQLSMALLINT TargetType,
		_Inout_updates_opt_(_Inexpressible_(BufferLength))
				SQLPOINTER TargetValue,
		SQLLEN BufferLength,
		_Inout_opt_ SQLLEN *StrLen_or_Ind);
SQLRETURN EsSQLFetch(SQLHSTMT StatementHandle);
SQLRETURN EsSQLSetPos(
		SQLHSTMT        StatementHandle,
		SQLSETPOSIROW   RowNumber,
		SQLUSMALLINT    Operation,
		SQLUSMALLINT    LockType);
SQLRETURN EsSQLBulkOperations(
		SQLHSTMT            StatementHandle,
		SQLSMALLINT         Operation);
SQLRETURN EsSQLCloseCursor(SQLHSTMT StatementHandle);
SQLRETURN EsSQLNumResultCols(SQLHSTMT StatementHandle, 
		_Out_ SQLSMALLINT *ColumnCount);


#endif /* __QUERIES_H__ */
