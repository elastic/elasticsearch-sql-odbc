/*
 * ELASTICSEARCH CONFIDENTIAL
 * __________________
 *
 *  [2014] Elasticsearch Incorporated. All Rights Reserved.
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

#include <string.h>
#include <wchar.h>

#include "ujdecode.h"

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
	int i;
	long count;
	size_t len;
	UJObject obj, tables, table;
	wchar_t *keys[] = {L"tables"};
	const wchar_t *tname;
    void *state = NULL, *iter;

	if (CatalogName && (wmemcmp(CatalogName, MK_TSTR(SQL_ALL_CATALOGS), 
					NameLength1) != 0)) {
		WARN("filtering by catalog is not supported."); /* TODO? */
		goto empty;
	}
	if (SchemaName && (wmemcmp(SchemaName, MK_TSTR(SQL_ALL_SCHEMAS), 
					NameLength2) != 0)) {
		WARN("filtering by schemas is not supported.");
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

	/* print JSON to send to server */
	count = _snwprintf(tbuf, sizeof(tbuf), SQL_TABLES_JSON, table_name);
	if ((count < 0) || sizeof(tbuf) <= count) {
		ERRN("failed to print JSON or buffer too small (%d).", count);
		RET_HDIAGS(stmt, SQL_STATE_HY000);
	}

#ifdef _WIN32 /* TODO a Posix friendlier equivalent */
	/* convert UCS2 -> UTF8
	 * WC_NO_BEST_FIT_CHARS: https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/unicode-data
	 */
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


	count = post_sql_tables(dbc, ESODBC_TIMEOUT_DEFAULT, u8, count, answer, 
			sizeof(answer));
	DBG("response received `%.*s` (%ld).", count, answer, count);

    obj = UJDecode(answer, count, NULL, &state);
    if (UJObjectUnpack(obj, 1, "A", keys, &tables) >= 1) {
		iter = UJBeginArray(tables);
		if (! iter) {
			ERR("failed to obtain array iterator: %s.", UJGetError(state));
			goto err;
		}
		while (UJIterArray(&iter, &table)) {
			if (! UJIsString(table)) {
				ERR("received non-string table name element (%d) - skipped.", 
						UJGetType(table));
				continue;
			}
			tname = UJReadString(table, &len);
			DBG("available table: `" LTPDL "`.", len, tname);
			// FIXME
			FIXME;
		}
	} else {
		ERR("failed to decode JSON answer (`%.*s`): %s.", count, answer, 
				UJGetError(state));
		goto err;
	}
    UJFree(state);

	return SQL_SUCCESS;

empty:
	// FIXME: set empty result to statement
	FIXME;
	return SQL_SUCCESS;

err:
	// FIXME
	assert(state);
    UJFree(state);
	return SQL_ERROR;
}
