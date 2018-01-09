/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */
#ifndef __QUERIES_H__
#define __QUERIES_H__

#include "error.h"

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


#endif /* __QUERIES_H__ */
