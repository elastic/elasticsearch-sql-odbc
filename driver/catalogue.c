/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <string.h>
#include <wchar.h>

#include "catalogue.h"
#include "handles.h"
#include "connect.h"
#include "info.h"
#include "log.h"

#define SQL_TABLES_JSON		"{\"table_pattern\" : \"" WPFWP_DESC "\"}"

SQLRETURN EsSQLTablesW(
		SQLHSTMT StatementHandle,
		_In_reads_opt_(NameLength1) SQLTCHAR *CatalogName, 
		SQLSMALLINT NameLength1,
		_In_reads_opt_(NameLength2) SQLTCHAR *SchemaName, 
		SQLSMALLINT NameLength2,
		_In_reads_opt_(NameLength3) SQLTCHAR *TableName, 
		SQLSMALLINT NameLength3,
		_In_reads_opt_(NameLength4) SQLTCHAR *TableType, 
		SQLSMALLINT NameLength4)
{
	SQLTCHAR *table_type, *table_name;
	SQLTCHAR tbuf[SQL_MAX_IDENTIFIER_LEN + sizeof(SQL_TABLES_JSON)];
	char u8[/*max bytes per character */4 * SQL_MAX_IDENTIFIER_LEN + 
		sizeof(SQLTCHAR) * sizeof(SQL_TABLES_JSON)];
	char answer[1<<16]; // FIXME
	esodbc_stmt_st *stmt = STMH(StatementHandle);
	esodbc_dbc_st *dbc = stmt->dbc;
	int i, count;
	long ret;

	if (CatalogName && (wmemcmp(CatalogName, MK_TSTR(SQL_ALL_CATALOGS), 
					NameLength1) != 0)) {
		WARN("filtering by catalog is not supported."); /* TODO? */
		goto empty;
	}
	if (SchemaName && (wmemcmp(SchemaName, MK_TSTR(SQL_ALL_SCHEMAS), 
					NameLength2) != 0)) {
		WARN("filtering by schemas is not supported."); /* TODO? */
		goto empty;
	}
	if (TableType) {
		table_type = _wcsupr(TableType);
		if (! wcsstr(table_type, MK_TSTR("TABLE"))) {
			INFO("no 'TABLE' type fond in filter `"LTPD"`: empty result set");
			goto empty;
		}
	}
	if (TableName) {
		DBG("filtering by table name `"LTPD"`.", TableName);
		if (stmt->metadata_id == SQL_TRUE) {
			// FIXME: string needs escaping of % \\ _
			FIXME;
		} else {
			/* ES uses * as wildcard instead of % (TODO: only?) */
			for (i = 0; i < NameLength3; i ++)
				if (TableName[i] == (SQLTCHAR)'%')
					TableName[i] = (SQLTCHAR)'*';
			DBG("new table name after s/%%/*: `"LTPD"`.", TableName);
		}
		table_name = TableName;
	} else {
		table_name = MK_TSTR("*");
	}

#ifdef _WIN32
	/* print JSON to send to server */
	count = _snwprintf(tbuf, sizeof(tbuf), SQL_TABLES_JSON, table_name);
	if ((count < 0) || sizeof(tbuf) <= count) {
		ERRN("failed to print JSON or buffer too small (%d).", count);
		RET_HDIAGS(stmt, SQL_STATE_HY000);
	}

	/* convert UCS2 -> UTF8 */
	count = WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, tbuf, count, 
			u8, sizeof(u8), NULL, NULL);
	if (count <= 0) {
		ERRN("conversion UCS2/UTF8 failed (%d).", count);
		RET_HDIAGS(stmt, SQL_STATE_HY000);
	} else {
		DBG("UCS2/UTF8 converted on %d bytes.", count);
	}
#else /* _WIN32 */
#	error "platform not supported"
#endif /* _WIN32 */


	ret = post_sql_tables(dbc, ESODBC_TIMEOUT_DEFAULT, u8, count, answer, 
			sizeof(answer));
	DBG("ret: %d, answer: `%s`.", ret, answer);

	// FIXME: parse into JSON
	FIXME;

	return SQL_SUCCESS;

empty:
	// FIXME: set empty result to statement
	FIXME;
	return SQL_ERROR;
}
