/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include "catalogue.h"
#include "handles.h"


SQLRETURN EsSQLTablesW(
		SQLHSTMT StatementHandle,
		_In_reads_opt_(NameLength1) SQLWCHAR *CatalogName, 
		SQLSMALLINT NameLength1,
		_In_reads_opt_(NameLength2) SQLWCHAR *SchemaName, 
		SQLSMALLINT NameLength2,
		_In_reads_opt_(NameLength3) SQLWCHAR *TableName, 
		SQLSMALLINT NameLength3,
		_In_reads_opt_(NameLength4) SQLWCHAR *TableType, 
		SQLSMALLINT NameLength4)
{
	RET_NOT_IMPLEMENTED;
}
