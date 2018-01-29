/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <string.h>
#include <wchar.h>

#include "ujdecode.h"

#include "catalogue.h"
#include "log.h"
#include "handles.h"
#include "connect.h"
#include "info.h"
#include "queries.h"

#define SQL_TABLES_JSON		"{\"table_pattern\" : \"" WPFWP_DESC "\"}"

#define JSON_TABLES_PREFIX \
	"{\"" JSON_ANSWER_COLUMNS "\": [" \
		"{" \
			"\"name\": \"TABLE_CAT\"," \
			"\"type\": \"text\"" \
		"}," \
		"{" \
			"\"name\": \"TABLE_SCHEM\"," \
			"\"type\": \"text\"" \
		"}," \
		"{" \
			"\"name\": \"TABLE_NAME\"," \
			"\"type\": \"text\"" \
		"}," \
		"{" \
			"\"name\": \"TABLE_TYPE\"," \
			"\"type\": \"text\"" \
		"}," \
		"{" \
			"\"name\": \"REMARKS\"," \
			"\"type\": \"text\"" \
		"}" \
	"]," \
	"\"" JSON_ANSWER_ROWS "\": ["
#define JSON_TABLES_SUFFIX "]}"

#define JSON_TABLES_ROW_TEMPLATE \
	"[null,null,\"" LTPDL "\",\"TABLE\",\"\"]"

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
	char answer[1<<16], faked[1<<16]; // FIXME: buffer handling
	char *dupped;
	esodbc_stmt_st *stmt = STMH(StatementHandle);
	esodbc_dbc_st *dbc = stmt->dbc;
	int i;
	long count;
	size_t len, pos;
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

	/*
	 * Note: UJDecode will treat UTF8 strings and UJReadString will return
	 * directly wchar_t.
	 */
    obj = UJDecode(answer, count, NULL, &state);
    if (UJObjectUnpack(obj, 1, "A", keys, &tables) < 1) {
		ERR("failed to decode JSON answer (`%.*s`): %s.", count, answer, 
				state ? UJGetError(state) : "<none>");
		goto err;
	}
	DBG("unpacked array of size: %zd.", UJArraySize(tables));

	assert(sizeof(JSON_TABLES_PREFIX) < sizeof(faked));
	memcpy(faked, JSON_TABLES_PREFIX, sizeof(JSON_TABLES_PREFIX) - /*0term*/1);
	pos = sizeof(JSON_TABLES_PREFIX) - 1;
	
	iter = UJBeginArray(tables);
	if (! iter) {
		ERR("failed to obtain array iterator: %s.", UJGetError(state));
		goto err;
	}
	count = 0;
	while (UJIterArray(&iter, &table)) {
		if (! UJIsString(table)) {
			ERR("received non-string table name element (%d) - skipped.", 
					UJGetType(table));
			continue;
		}
		tname = UJReadString(table, &len);
		DBG("available table: `" LTPDL "`.", len, tname);
		count = snprintf(faked + pos, sizeof(faked), 
				"%s" JSON_TABLES_ROW_TEMPLATE, count ? "," : "",
				(int)len, tname);
		if (count < 0) {
			ERRN("buffer printing failed");
			goto err;
		} else if (count < sizeof(JSON_TABLES_ROW_TEMPLATE) - 1 
				- sizeof(LTPDL) - 1 + len) {
			ERR("buffer to small (%d) to print faked 'tables' JSON.", 
					sizeof(faked));
			goto err;
		}
		pos += count;
	}
    UJFree(state);
	state = NULL;
	
	if (sizeof(faked) <= pos + sizeof(JSON_TABLES_SUFFIX)/*w/ \0*/) {
		ERR("buffer to small (%d) to print faked 'tables' JSON.", 
				sizeof(faked));
		goto err;
	}
	memcpy(faked + pos, JSON_TABLES_SUFFIX, sizeof(JSON_TABLES_SUFFIX));
	pos += sizeof(JSON_TABLES_SUFFIX); /* includes \0 */

	DBG("faked 'tables' answer: `%.*s`.", pos, faked);

	dupped = (char *)malloc(pos);
	if (! dupped) {
		ERRN("failed to strndup faked answer.");
		RET_HDIAGS(stmt, SQL_STATE_HY001);
	}
	memcpy(dupped, faked, pos);

	return attach_answer(stmt, dupped, pos - /*\0*/1);

empty:
	// FIXME: set empty result to statement
	FIXME;
	return SQL_SUCCESS;

err:
	// FIXME
	if (state)
		UJFree(state);
	RET_HDIAGS(stmt, SQL_STATE_HY000);
}
