/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include "queries.h"
#include "handles.h"

SQLRETURN EsSQLBindCol(
		SQLHSTMT StatementHandle,
		SQLUSMALLINT ColumnNumber,
		SQLSMALLINT TargetType,
		_Inout_updates_opt_(_Inexpressible_(BufferLength)) SQLPOINTER TargetValue,
		SQLLEN BufferLength,
		_Inout_opt_ SQLLEN *StrLen_or_Ind)
{
	BUG("not implemented.");
	//RET_NOT_IMPLEMENTED;
	return SQL_SUCCESS;
}

SQLRETURN EsSQLFetch(SQLHSTMT StatementHandle)
{
	RET_NOT_IMPLEMENTED;
}

