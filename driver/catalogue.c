/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "ujdecode.h"

#include "catalogue.h"
#include "log.h"
#include "handles.h"
#include "connect.h"
#include "info.h"
#include "queries.h"

#if 1
#define SQL_TABLES_START	"SYS TABLES LIKE '"
#define SQL_TABLES_END		"'"
//#define SQL_TABLES_END		"';"
#endif

#define ESODBC_SQL_TABLES	"SYS TABLES LIKE"
#define ESODBC_SQL_COLUMNS	"SYS COLUMNS TABLES LIKE"

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
	esodbc_stmt_st *stmt = STMH(StatementHandle);
	SQLRETURN ret;
	SQLTCHAR wbuf[ESODBC_MAX_IDENTIFIER_LEN + sizeof(SQL_TABLES_START) + 
		sizeof(SQL_TABLES_END)]; /*includes 2x\0*/
	SQLTCHAR *table_type, *table_name;
	size_t count, pos;

	if (stmt->metadata_id == SQL_TRUE)
		FIXME; // FIXME

	/* TODO: server support needed for cat. & sch. name filtering */

	if (CatalogName && (wmemcmp(CatalogName, MK_TSTR(SQL_ALL_CATALOGS), 
					NameLength1) != 0)) {
		ERR("filtering by catalog is not supported.");
		RET_HDIAG(stmt, SQL_STATE_IM001, "catalog filtering not supported", 0);
	}
	if (SchemaName && (wmemcmp(SchemaName, MK_TSTR(SQL_ALL_SCHEMAS), 
					NameLength2) != 0)) {
		ERR("filtering by schemas is not supported.");
		RET_HDIAG(stmt, SQL_STATE_IM001, "schema filtering not supported", 0);
	}
	if (TableType) {
		table_type = _wcsupr(TableType);
		if ((! wcsstr(table_type, MK_TSTR("TABLE"))) && 
				(! wcsstr(table_type, MK_TSTR("ALIAS")))) {
			WARN("no 'TABLE' or 'ALIAS' type fond in filter `"LTPD"`: "
					"empty result set");
			goto empty;
		}
	}
	if (TableName) {
		DBG("filtering by table name `"LTPD"`.", TableName);
		if (stmt->metadata_id == SQL_TRUE) {
			// FIXME: string needs escaping of % \\ _
			FIXME;
		}
		table_name = TableName;
		count = NameLength3;
	} else {
		table_name = MK_TSTR("%");
		count = sizeof("%") - 1;
	}

	if (ESODBC_MAX_IDENTIFIER_LEN < NameLength3) {
		ERR("TableName `" LTPDL "` too long (limit: %zd).", NameLength3,
				TableName, ESODBC_MAX_IDENTIFIER_LEN);
		RET_HDIAG(stmt, SQL_STATE_HY000, "TableName too long", 0);
	}
	/* print SQL to send to server */
	/* count/pos always indicate number of characters (not bytes) */
	pos = sizeof(SQL_TABLES_START) - 1;
	memcpy(wbuf, MK_TSTR(SQL_TABLES_START), pos * sizeof(SQLTCHAR));
	memcpy(&wbuf[pos], table_name, count * sizeof(SQLTCHAR));
	pos += count;
	count = sizeof(SQL_TABLES_END) - 1;
	memcpy(&wbuf[pos], MK_TSTR(SQL_TABLES_END), count * sizeof(SQLTCHAR));
	pos += count;

	ret = EsSQLFreeStmt(stmt, ESODBC_SQL_CLOSE);
	assert(SQL_SUCCEEDED(ret)); /* can't return error */
	ret = attach_sql(stmt, wbuf, pos);
	if (SQL_SUCCEEDED(ret))
		ret = post_statement(stmt);
	return ret;

empty:
	RET_HDIAG(stmt, SQL_STATE_HYC00, "Table filtering not supported", 0);
	// FIXME: add support for it
}

SQLRETURN EsSQLColumnsW
(
    SQLHSTMT           hstmt,
    _In_reads_opt_(cchCatalogName) SQLWCHAR*    szCatalogName,
    SQLSMALLINT        cchCatalogName,
    _In_reads_opt_(cchSchemaName) SQLWCHAR*     szSchemaName,
    SQLSMALLINT        cchSchemaName,
    _In_reads_opt_(cchTableName) SQLWCHAR*      szTableName,
    SQLSMALLINT        cchTableName,
    _In_reads_opt_(cchColumnName) SQLWCHAR*     szColumnName,
    SQLSMALLINT        cchColumnName
)
{
	esodbc_stmt_st *stmt = STMH(hstmt);
	SQLTCHAR *tablename, *columnname;
	SQLSMALLINT tn_clen, cn_clen;
	/* 8 for \' and spaces and extra */
	SQLTCHAR wbuf[sizeof(ESODBC_SQL_COLUMNS) + 2 * ESODBC_MAX_IDENTIFIER_LEN + 8];
	int clen;
	SQLRETURN ret;

	if (stmt->metadata_id == SQL_TRUE)
		FIXME; // FIXME
	
	/* TODO: server support needed for cat. & sch. name filtering */

	if (szCatalogName && (wmemcmp(szCatalogName, MK_TSTR(SQL_ALL_CATALOGS), 
					cchCatalogName) != 0)) {
		ERR("filtering by catalog is not supported."); /* TODO? */
		RET_HDIAG(stmt, SQL_STATE_IM001, "catalog filtering not supported", 0);
	}
	if (szSchemaName && (wmemcmp(szSchemaName, MK_TSTR(SQL_ALL_SCHEMAS), 
					cchSchemaName) != 0)) {
		ERR("filtering by schemas is not supported.");
		RET_HDIAG(stmt, SQL_STATE_IM001, "schema filtering not supported", 0);
	}

	if (szTableName && cchTableName) {
		if (ESODBC_MAX_IDENTIFIER_LEN < cchTableName) {
			ERR("TableName `" LTPDL "` too long (limit: %zd).", cchTableName,
					szTableName, ESODBC_MAX_IDENTIFIER_LEN);
			RET_HDIAG(stmt, SQL_STATE_HY000, "TableName too long", 0);
		}
		tablename = szTableName;
		tn_clen = cchTableName;
	} else {
		tablename = MK_TSTR("%");
		tn_clen = sizeof("%") - 1;
	}
	if (szColumnName && cchColumnName) {
		if (ESODBC_MAX_IDENTIFIER_LEN < cchColumnName) {
			ERR("ColumnName `" LTPDL "` too long (limit: %zd).", cchColumnName,
					szColumnName, ESODBC_MAX_IDENTIFIER_LEN);
			RET_HDIAG(stmt, SQL_STATE_HY000, "ColumnName too long", 0);
		}
		columnname = szColumnName;
		cn_clen = cchColumnName;
	} else {
		columnname = MK_TSTR("%");
		cn_clen = sizeof("%") - 1;
	}

	clen = swprintf(wbuf, sizeof(wbuf)/sizeof(SQLTCHAR), 
			MK_TSTR("%s '%.*s' ESCAPE '" ESODBC_PATTERN_ESCAPE "' "
				"'%.*s' ESCAPE '" ESODBC_PATTERN_ESCAPE "'"), 
			MK_TSTR(ESODBC_SQL_COLUMNS), 
			tn_clen, tablename, cn_clen, columnname);
	if (clen <= 0 || sizeof(wbuf)/sizeof(SQLTCHAR) <= clen) { /* == */
		ERRN("SQL printing failed (buffer too small (%zdB)?).", sizeof(wbuf));
		RET_HDIAGS(stmt, SQL_STATE_HY000);
	}
	
	ret = EsSQLFreeStmt(stmt, ESODBC_SQL_CLOSE);
	assert(SQL_SUCCEEDED(ret)); /* can't return error */
	ret = attach_sql(stmt, wbuf, clen);
	if (SQL_SUCCEEDED(ret))
		ret = post_statement(stmt);
	return ret;
}

SQLRETURN EsSQLSpecialColumnsW
(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fColType,
    _In_reads_opt_(cchCatalogName) SQLWCHAR*    szCatalogName,
    SQLSMALLINT        cchCatalogName,
    _In_reads_opt_(cchSchemaName) SQLWCHAR*     szSchemaName,
    SQLSMALLINT        cchSchemaName,
    _In_reads_opt_(cchTableName) SQLWCHAR*      szTableName,
    SQLSMALLINT        cchTableName,
    SQLUSMALLINT       fScope,
    SQLUSMALLINT       fNullable
)
{
	// TODO: is there a "rowid" equivalent: ID uniquely a ROW in the table?
	// or unique indexes equivalents
	WARN("no special columns available.");
	STMT_FORCE_NODATA(STMH(hstmt));
	return SQL_SUCCESS;
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
