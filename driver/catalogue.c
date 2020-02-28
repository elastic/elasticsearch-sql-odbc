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
#include "info.h"
#include "queries.h"


/* SYS TABLES syntax tokens; these need to stay broken down, since this
 * query makes a difference between a predicate being '%' or left out */
// TODO: schema, when supported
#define SQL_TABLES \
	"SYS TABLES"
#define SQL_TABLES_CAT \
	" CATALOG LIKE " ESODBC_STRING_DELIM WPFWP_LDESC ESODBC_STRING_DELIM \
	" ESCAPE '" ESODBC_PATTERN_ESCAPE "'"
#define SQL_TABLES_TAB \
	" LIKE " ESODBC_STRING_DELIM WPFWP_LDESC ESODBC_STRING_DELIM \
	" ESCAPE '" ESODBC_PATTERN_ESCAPE "'"
#define SQL_TABLES_TYP \
	" TYPE " WPFWP_LDESC

// TODO add schema, when supported
#define SQL_COLUMNS(...)		"SYS COLUMNS" __VA_ARGS__ \
	" TABLE LIKE " ESODBC_STRING_DELIM WPFWP_LDESC ESODBC_STRING_DELIM \
	" ESCAPE '" ESODBC_PATTERN_ESCAPE "'" \
	" LIKE " ESODBC_STRING_DELIM WPFWP_LDESC ESODBC_STRING_DELIM \
	" ESCAPE '" ESODBC_PATTERN_ESCAPE "'"
#define SQL_COL_CAT \
	" CATALOG " ESODBC_STRING_DELIM WPFWP_LDESC ESODBC_STRING_DELIM


static SQLRETURN fake_answer(SQLHSTMT hstmt, const char *src, size_t cnt)
{
	char *dup;

	if (! (dup = strdup(src))) {
		ERRNH(hstmt, "OOM with %zu.", cnt);
		RET_HDIAGS(hstmt, SQL_STATE_HY001);
	}
	return attach_answer(STMH(hstmt), dup, cnt);

}

SQLRETURN EsSQLStatisticsW(
	SQLHSTMT           hstmt,
	_In_reads_opt_(cchCatalogName) SQLWCHAR    *szCatalogName,
	SQLSMALLINT        cchCatalogName,
	_In_reads_opt_(cchSchemaName) SQLWCHAR     *szSchemaName,
	SQLSMALLINT        cchSchemaName,
	_In_reads_opt_(cchTableName) SQLWCHAR      *szTableName,
	SQLSMALLINT        cchTableName,
	SQLUSMALLINT       fUnique,
	SQLUSMALLINT       fAccuracy)
{
	/*INDENT-OFF*/
#	define STATISTICS_EMPTY \
	"{" \
		"\"columns\":[" \
			"{\"name\":\"TABLE_CAT\","			"\"type\":\"TEXT\"}," \
			"{\"name\":\"TABLE_SCHEM\","		"\"type\":\"TEXT\"}," \
			"{\"name\":\"TABLE_NAME\","			"\"type\":\"TEXT\"}," \
			"{\"name\":\"NON_UNIQUE\","			"\"type\":\"SHORT\"}," \
			"{\"name\":\"INDEX_QUALIFIER\","	"\"type\":\"TEXT\"}," \
			"{\"name\":\"INDEX_NAME\","			"\"type\":\"TEXT\"}," \
			"{\"name\":\"TYPE\","				"\"type\":\"SHORT\"}," \
			"{\"name\":\"ORDINAL_POSITION\","	"\"type\":\"SHORT\"}," \
			"{\"name\":\"COLUMN_NAME \","		"\"type\":\"TEXT\"}," \
			"{\"name\":\"ASC_OR_DESC\","		"\"type\":\"BYTE\"}," \
			"{\"name\":\"CARDINALITY\","		"\"type\":\"INTEGER\"}," \
			"{\"name\":\"PAGES\","				"\"type\":\"INTEGER\"}," \
			"{\"name\":\"FILTER_CONDITION\","	"\"type\":\"TEXT\"}" \
		"]," \
		"\"rows\":[]" \
	"}"
	/*INDENT-ON*/

	INFOH(hstmt, "no statistics available.");
	return fake_answer(hstmt, STATISTICS_EMPTY,
			sizeof(STATISTICS_EMPTY) - /*\0*/1);

#	undef STATISTICS_EMPTY
}

BOOL TEST_API set_current_catalog(esodbc_dbc_st *dbc, wstr_st *catalog)
{
	if (dbc->catalog.cnt) {
		DBGH(dbc, "catalog already set to `" LWPDL "`.", LWSTR(&dbc->catalog));
		if (! EQ_WSTR(&dbc->catalog, catalog)) {
			/* this should never happen, as cluster's name is not updateable
			 * on the fly. */
			ERRH(dbc, "overwriting previously set catalog value!");
			free(dbc->catalog.str);
			dbc->catalog.str = NULL;
			dbc->catalog.cnt = 0;
		} else {
			return FALSE;
		}
	}
	if (! catalog->cnt) {
		WARNH(dbc, "attempting to set catalog name to empty value.");
		return FALSE;
	}
	if (! (dbc->catalog.str = malloc((catalog->cnt + 1) * sizeof(SQLWCHAR)))) {
		ERRNH(dbc, "OOM for %zu wchars.", catalog->cnt + 1);
		return FALSE;
	}
	wmemcpy(dbc->catalog.str, catalog->str, catalog->cnt);
	dbc->catalog.str[catalog->cnt] = L'\0';
	dbc->catalog.cnt = catalog->cnt;
	INFOH(dbc, "current catalog name: `" LWPDL "`.", LWSTR(&dbc->catalog));

	return TRUE;
}

/* writes into 'dest', of size 'room', the current requested attr. of 'dbc'.
 * returns negative on error, or the char count written otherwise */
SQLSMALLINT fetch_server_attr(esodbc_dbc_st *dbc, SQLINTEGER attr_id,
	SQLWCHAR *dest, SQLSMALLINT room)
{
	esodbc_stmt_st *stmt = NULL;
	SQLSMALLINT used = -1; /*failure*/
	SQLLEN row_cnt;
	SQLLEN ind_len = SQL_NULL_DATA;
	SQLWCHAR *buff;
	static const size_t buff_cnt = ESODBC_MAX_IDENTIFIER_LEN + /*\0*/1;
	wstr_st attr_val;
	wstr_st attr_sql;

	buff = malloc(sizeof(*buff) * buff_cnt);
	if (! buff) {
		ERRH(dbc, "OOM: %zu wchar_t.", buff_cnt);
		return -1;
	}

	if (! SQL_SUCCEEDED(EsSQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) {
		ERRH(dbc, "failed to alloc a statement handle.");
		goto end;
	}
	assert(stmt);

	switch (attr_id) {
		case SQL_ATTR_CURRENT_CATALOG:
			attr_sql = MK_WSTR("SELECT database()");
			break;
		case SQL_USER_NAME:
			attr_sql = MK_WSTR("SELECT user()");
			break;
		default:
			BUGH(dbc, "unexpected attribute ID: %ld.", attr_id);
			goto end;
	}

	if (! SQL_SUCCEEDED(attach_sql(stmt, attr_sql.str, attr_sql.cnt))) {
		ERRH(dbc, "failed to attach query to statement.");
		goto end;
	}
	if (! SQL_SUCCEEDED(EsSQLExecute(stmt))) {
		ERRH(dbc, "failed to post query.");
		goto end;
	}

	/* check that we have received proper number of rows (non-0, less than
	 * max allowed here) */
	if (! SQL_SUCCEEDED(EsSQLRowCount(stmt, &row_cnt))) {
		ERRH(dbc, "failed to get result rows count.");
		goto end;
	} else if (row_cnt <= 0) {
		WARNH(stmt, "received no value for attribute %ld.", attr_id);
		attr_val = MK_WSTR(""); /* empty string, it's not quite an error */
	} else {
		if (1 < row_cnt) {
			WARNH(dbc, "more than one value (%lld) available for "
				"attribute %ld; picking first.", row_cnt, attr_id);
		}

		if (! SQL_SUCCEEDED(EsSQLBindCol(stmt, /*col#*/1, SQL_C_WCHAR, buff,
					sizeof(*buff) * (buff_cnt - 1), &ind_len))) {
			ERRH(dbc, "failed to bind first column.");
			goto end;
		}
		if (! SQL_SUCCEEDED(EsSQLFetch(stmt))) {
			ERRH(stmt, "failed to fetch results.");
			goto end;
		}
		if (ind_len <= 0) {
			WARNH(dbc, "NULL value received for attribute %ld.", attr_id);
			assert(ind_len == SQL_NULL_DATA);
			attr_val = MK_WSTR("");
		} else {
			attr_val.str = buff;
			attr_val.cnt = ind_len / sizeof(*buff);
			/* 0-term room left out when binding */
			buff[attr_val.cnt] = L'\0'; /* write_wstr() expects the 0-term */
		}
		if (attr_id == SQL_ATTR_CURRENT_CATALOG) {
			set_current_catalog(dbc, &attr_val);
		}
	}
	DBGH(dbc, "attribute %ld value: `" LWPDL "`.", attr_id, LWSTR(&attr_val));

	if (! SQL_SUCCEEDED(write_wstr(dbc, dest, &attr_val, room, &used))) {
		ERRH(dbc, "failed to copy value: `" LWPDL "`.", LWSTR(&attr_val));
		used = -1; /* write_wstr() can change pointer, and still fail */
	}

end:
	if (stmt) {
		/* safe even if no binding occured */
		if (! SQL_SUCCEEDED(EsSQLFreeStmt(stmt, SQL_UNBIND))) {
			ERRH(stmt, "failed to unbind statement");
			used = -1;
		}
		if (! SQL_SUCCEEDED(EsSQLFreeHandle(SQL_HANDLE_STMT, stmt))) {
			ERRH(dbc, "failed to free statement handle!");
		}
	}
	free(buff);
	return used;
}

/*
 * Quote the tokens in a string: "a, b,,c" -> "'a','b',,'c'".
 * No string sanity done (garbage in, garbage out).
 */
size_t quote_tokens(SQLWCHAR *src, size_t len, SQLWCHAR *dest)
{
	size_t i;
	BOOL copying;
	SQLWCHAR *pos;

	copying = FALSE;
	pos = dest;
	for (i = 0; i < len; i ++) {
		switch (src[i]) {
			/* ignore white space */
			case L' ':
			case L'\t':
				if (copying) {
					*pos ++ = L'\''; /* end current token */
					copying = FALSE;
				}
				continue; /* don't copy WS */

			case L',':
				if (copying) {
					*pos ++ = L'\''; /* end current token */
					copying = FALSE;
				} /* else continue; -- to remove extra `,` */
				break;

			default:
				if (! copying) {
					*pos ++ = L'\''; /* start a new token */
					copying = TRUE;
				}
		}
		*pos ++ = src[i];
	}
	if (copying) {
		*pos ++ = L'\''; /* end last token */
	} else if (dest < pos && pos[-1] == L',') {
		/* trim last char, if it's a comma: LibreOffice (6.1.0.3) sends the
		 * table type string `VIEW,TABLE,%,` having the last `,` propagated to
		 * EsSQL, which rejects the query */
		pos --;
	}
	/* should not overrun */
	assert(i < 2/*see typ_buf below*/ * ESODBC_MAX_IDENTIFIER_LEN);
	return pos - dest;
}

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
	esodbc_stmt_st *stmt = STMH(StatementHandle);
	SQLRETURN ret = SQL_ERROR;
	enum {
		wbuf_cnt = sizeof(SQL_TABLES)
			+ sizeof(SQL_TABLES_CAT)
			+ sizeof(SQL_TABLES_TAB)
			+ sizeof(SQL_TABLES_TYP)
			+ 3 * ESODBC_MAX_IDENTIFIER_LEN /* it has 4x 0-term space */,
		/* 2x: "a,b,c" -> "'a','b','c'" : each "x," => "'x'," */
		tbuf_cnt = 2 * ESODBC_MAX_IDENTIFIER_LEN
	};
	SQLWCHAR *wbuf;
	SQLWCHAR *table, *schema, *catalog, *type;
	size_t cnt_tab, cnt_sch, cnt_cat, cnt_typ, pos;
	SQLWCHAR *typ_buf;
	void *ptr;

	if (stmt->metadata_id == SQL_TRUE) {
		FIXME;    // FIXME
	}

	ptr = malloc((wbuf_cnt + tbuf_cnt) * sizeof(SQLWCHAR));
	if (! ptr) {
		ERRNH(stmt, "OOM: %zu wbuf_t.", wbuf_cnt + tbuf_cnt);
		RET_HDIAGS(stmt, SQL_STATE_HY001);
	} else {
		wbuf = (SQLWCHAR *)ptr;
		typ_buf = &wbuf[wbuf_cnt + 1];
	}

	pos = sizeof(SQL_TABLES) - 1;
	wmemcpy(wbuf, MK_WPTR(SQL_TABLES), pos);

	if (CatalogName && NameLength1) {
		catalog = CatalogName;
		if (NameLength1 == SQL_NTS) {
			cnt_cat = wcslen(catalog);
			if (ESODBC_MAX_IDENTIFIER_LEN < cnt_cat) {
				ERRH(stmt, "catalog identifier name '" LTPDL "' too long "
					"(%zd. max=%d).", (int)cnt_cat, catalog, cnt_cat,
					ESODBC_MAX_IDENTIFIER_LEN);
				SET_HDIAG(stmt, SQL_STATE_HY090, "catalog name too long", 0);
				goto end;
			}
		} else {
			cnt_cat = NameLength1;
		}

		cnt_cat = swprintf(wbuf + pos, wbuf_cnt - pos, SQL_TABLES_CAT,
				(int)cnt_cat, catalog);
		if (cnt_cat <= 0) {
			ERRH(stmt, "failed to print 'catalog' for tables catalog SQL.");
			SET_HDIAG(stmt, SQL_STATE_HY000, "internal printing error", 0);
			goto end;
		} else {
			pos += cnt_cat;
		}
	}

	if (SchemaName && NameLength2) {
		schema = SchemaName;
		if (NameLength2 == SQL_NTS) {
			cnt_sch = wcslen(schema);
			if (ESODBC_MAX_IDENTIFIER_LEN < cnt_sch) {
				ERRH(stmt, "schema identifier name '" LTPDL "' too long "
					"(%zd. max=%d).", (int)cnt_sch, schema, cnt_sch,
					ESODBC_MAX_IDENTIFIER_LEN);
				SET_HDIAG(stmt, SQL_STATE_HY090, "schema name too long", 0);
				goto end;
			}
		} else {
			cnt_sch = NameLength2;
		}

		/* TODO: server support needed for sch. name filtering */
		if (wszmemcmp(schema, MK_WPTR(SQL_ALL_SCHEMAS), (long)cnt_sch)) {
			ERRH(stmt, "filtering by schemas is not supported.");
			SET_HDIAG(stmt, SQL_STATE_IM001, "schema filtering not supported",
				0);
			goto end;
		}
	}

	// FIXME: string needs escaping of % \\ _
	if (TableName && NameLength3) {
		table = TableName;
		if (NameLength3 == SQL_NTS) {
			cnt_tab = wcslen(table);
			if (ESODBC_MAX_IDENTIFIER_LEN < cnt_tab) {
				ERRH(stmt, "table identifier name '" LTPDL "' too long "
					"(%zd. max=%d).", (int)cnt_tab, table, cnt_tab,
					ESODBC_MAX_IDENTIFIER_LEN);
				SET_HDIAG(stmt, SQL_STATE_HY090, "table name too long", 0);
				goto end;
			}
		} else {
			cnt_tab = NameLength3;
		}

		cnt_tab = swprintf(wbuf + pos, wbuf_cnt - pos, SQL_TABLES_TAB,
				(int)cnt_tab, table);
		if (cnt_tab <= 0) {
			ERRH(stmt, "failed to print 'table' for tables catalog SQL.");
			SET_HDIAG(stmt, SQL_STATE_HY000, "internal printing error", 0);
			goto end;
		} else {
			pos += cnt_tab;
		}
	}

	if (TableType && NameLength4) {
		type = TableType;
		if (NameLength4 == SQL_NTS) {
			cnt_typ = wcslen(type);
			if (ESODBC_MAX_IDENTIFIER_LEN < cnt_typ) {
				ERRH(stmt, "type identifier name '" LTPDL "' too long "
					"(%zd. max=%d).", (int)cnt_typ, type, cnt_typ,
					ESODBC_MAX_IDENTIFIER_LEN);
				SET_HDIAG(stmt, SQL_STATE_HY090, "type name too long", 0);
				goto end;
			}
		} else {
			cnt_typ = NameLength4;
		}

		/* Only print TYPE if non-empty. This is be incorrect, by the book,
		 * but there's little use to specifying an empty string as the type
		 * (vs NULL), so this should hopefully be safe. -- Qlik */
		if (0 < cnt_typ) {
			/* Here, "each value can be enclosed in single quotation marks (')
			 * or unquoted" => quote if not quoted (see GH#30398). */
			if (! wcsnstr(type, cnt_typ, L'\'')) {
				cnt_typ = quote_tokens(type, cnt_typ, typ_buf);
				type = typ_buf;
			}

			cnt_typ = swprintf(wbuf + pos, wbuf_cnt - pos, SQL_TABLES_TYP,
					(int)cnt_typ, type);
			if (cnt_typ <= 0) {
				ERRH(stmt, "failed to print 'type' for tables catalog SQL.");
				SET_HDIAG(stmt, SQL_STATE_HY000, "internal printing error", 0);
				goto end;
			} else {
				pos += cnt_typ;
			}
		}
	}

	DBGH(stmt, "tables catalog SQL [%d]:`" LWPDL "`.", pos, pos, wbuf);

	ret = EsSQLExecDirectW(stmt, wbuf, (SQLINTEGER)pos);
end:
	free(ptr);
	return ret;
}

SQLRETURN EsSQLColumnsW
(
	SQLHSTMT           hstmt,
	_In_reads_opt_(cchCatalogName) SQLWCHAR    *szCatalogName,
	SQLSMALLINT        cchCatalogName,
	_In_reads_opt_(cchSchemaName) SQLWCHAR     *szSchemaName,
	SQLSMALLINT        cchSchemaName,
	_In_reads_opt_(cchTableName) SQLWCHAR      *szTableName,
	SQLSMALLINT        cchTableName,
	_In_reads_opt_(cchColumnName) SQLWCHAR     *szColumnName,
	SQLSMALLINT        cchColumnName
)
{
	esodbc_stmt_st *stmt = STMH(hstmt);
	SQLRETURN ret = SQL_ERROR;
	SQLWCHAR *wbuf;
	static const size_t wbuf_cnt = sizeof(SQL_COLUMNS(SQL_COL_CAT)) +
		3 * ESODBC_MAX_IDENTIFIER_LEN;
	SQLWCHAR *catalog, *schema, *table, *column;
	size_t cnt_cat, cnt_sch, cnt_tab, cnt_col, pos;

	if (stmt->metadata_id == SQL_TRUE) {
		FIXME;    // FIXME
	}

	/* TODO: server support needed for cat. & sch. name filtering */

	if (szCatalogName) {
		catalog = szCatalogName;
		if (cchCatalogName == SQL_NTS) {
			cnt_cat = wcslen(catalog);
			if (ESODBC_MAX_IDENTIFIER_LEN < cnt_cat) {
				ERRH(stmt, "catalog identifier name '" LTPDL "' too long "
					"(%d. max=%d).", cnt_cat, catalog, cnt_cat,
					ESODBC_MAX_IDENTIFIER_LEN);
				RET_HDIAG(stmt, SQL_STATE_HY090, "catalog name too long", 0);
			}
		} else {
			cnt_cat = cchCatalogName;
		}
	} else {
		catalog = NULL;
	}

	if (szSchemaName) {
		schema = szSchemaName;
		if (cchSchemaName == SQL_NTS) {
			cnt_sch = wcslen(schema);
			if (ESODBC_MAX_IDENTIFIER_LEN < cnt_sch) {
				ERRH(stmt, "schema identifier name '" LTPDL "' too long "
					"(%d. max=%d).", cnt_sch, schema, cnt_sch,
					ESODBC_MAX_IDENTIFIER_LEN);
				RET_HDIAG(stmt, SQL_STATE_HY090, "schema name too long", 0);
			}
		} else {
			cnt_sch = cchSchemaName;
		}
	} else {
		schema = MK_WPTR(SQL_ALL_SCHEMAS);
		cnt_sch = sizeof(SQL_ALL_SCHEMAS) - /*0-term*/1;
	}

	/* TODO: server support needed for sch. name filtering */
	if (cnt_sch && wszmemcmp(schema, MK_WPTR(SQL_ALL_SCHEMAS),
			(long)cnt_sch)) {
		ERRH(stmt, "filtering by schemas is not supported.");
		RET_HDIAG(stmt, SQL_STATE_IM001, "schema filtering not supported", 0);
	}

	// FIXME: string needs escaping of % \\ _
	if (szTableName) {
		table = szTableName;
		if (cchTableName == SQL_NTS) {
			cnt_tab = wcslen(table);
			if (ESODBC_MAX_IDENTIFIER_LEN < cnt_tab) {
				ERRH(stmt, "table identifier name '" LTPDL "' too long "
					"(%d. max=%d).", cnt_tab, table, cnt_tab,
					ESODBC_MAX_IDENTIFIER_LEN);
				RET_HDIAG(stmt, SQL_STATE_HY090, "table name too long", 0);
			}
		} else {
			cnt_tab = cchTableName;
		}
	} else {
		table = MK_WPTR(ESODBC_ALL_TABLES);
		cnt_tab = sizeof(ESODBC_ALL_TABLES) - /*0-term*/1;
	}

	if (szColumnName) {
		column = szColumnName;
		if (cchColumnName == SQL_NTS) {
			cnt_col = wcslen(column);
			if (ESODBC_MAX_IDENTIFIER_LEN < cnt_col) {
				ERRH(stmt, "column identifier name '" LTPDL "' too long "
					"(%d. max=%d).", cnt_col, column, cnt_col,
					ESODBC_MAX_IDENTIFIER_LEN);
				RET_HDIAG(stmt, SQL_STATE_HY090, "column name too long", 0);
			}
		} else {
			cnt_col = cchColumnName;
		}
	} else {
		column = MK_WPTR(ESODBC_ALL_COLUMNS);
		cnt_col = sizeof(ESODBC_ALL_COLUMNS) - /*0-term*/1;
	}

	wbuf = malloc(wbuf_cnt * sizeof(*wbuf));
	if (! wbuf) {
		ERRNH(stmt, "OOM: %zu wbuf_t.", wbuf_cnt);
		RET_HDIAGS(stmt, SQL_STATE_HY001);
	}
	/* print SQL to send to server */
	if (catalog) {
		pos = swprintf(wbuf, wbuf_cnt, SQL_COLUMNS(SQL_COL_CAT), (int)cnt_cat,
				catalog, (int)cnt_tab, table, (int)cnt_col, column);
	} else {
		pos = swprintf(wbuf, wbuf_cnt, SQL_COLUMNS(), (int)cnt_tab, table,
				(int)cnt_col, column);
	}
	if (pos <= 0) {
		ERRH(stmt, "failed to print 'columns' catalog SQL.");
		SET_HDIAG(stmt, SQL_STATE_HY000, "internal printing error", 0);
		goto end;
	}

	ret = EsSQLExecDirectW(stmt, wbuf, (SQLINTEGER)pos);
end:
	free(wbuf);
	return ret;
}

SQLRETURN EsSQLSpecialColumnsW
(
	SQLHSTMT           hstmt,
	SQLUSMALLINT       fColType,
	_In_reads_opt_(cchCatalogName) SQLWCHAR    *szCatalogName,
	SQLSMALLINT        cchCatalogName,
	_In_reads_opt_(cchSchemaName) SQLWCHAR     *szSchemaName,
	SQLSMALLINT        cchSchemaName,
	_In_reads_opt_(cchTableName) SQLWCHAR      *szTableName,
	SQLSMALLINT        cchTableName,
	SQLUSMALLINT       fScope,
	SQLUSMALLINT       fNullable
)
{
	/*INDENT-OFF*/
#	define SPECIAL_COLUMNS_EMPTY \
	"{" \
		"\"columns\":[" \
			"{\"name\":\"SCOPE\","			"\"type\":\"SHORT\"}," \
			"{\"name\":\"COLUMN_NAME\","	"\"type\":\"TEXT\"}," \
			"{\"name\":\"DATA_TYPE\","		"\"type\":\"SHORT\"}," \
			"{\"name\":\"TYPE_NAME\","		"\"type\":\"TEXT\"}," \
			"{\"name\":\"COLUMN_SIZE\","	"\"type\":\"INTEGER\"}," \
			"{\"name\":\"BUFFER_LENGTH\","	"\"type\":\"INTEGER\"}," \
			"{\"name\":\"DECIMAL_DIGITS\","	"\"type\":\"SHORT\"}," \
			"{\"name\":\"PSEUDO_COLUMN\","	"\"type\":\"SHORT\"}" \
		"]," \
		"\"rows\":[]" \
	"}"
	/*INDENT-ON*/


	INFOH(hstmt, "no special columns available.");
	return fake_answer(hstmt, SPECIAL_COLUMNS_EMPTY,
			sizeof(SPECIAL_COLUMNS_EMPTY) - /*\0*/1);

#	undef SPECIAL_COLUMNS_EMPTY
}


SQLRETURN EsSQLForeignKeysW(
	SQLHSTMT           hstmt,
	_In_reads_opt_(cchPkCatalogName) SQLWCHAR    *szPkCatalogName,
	SQLSMALLINT        cchPkCatalogName,
	_In_reads_opt_(cchPkSchemaName) SQLWCHAR     *szPkSchemaName,
	SQLSMALLINT        cchPkSchemaName,
	_In_reads_opt_(cchPkTableName) SQLWCHAR      *szPkTableName,
	SQLSMALLINT        cchPkTableName,
	_In_reads_opt_(cchFkCatalogName) SQLWCHAR    *szFkCatalogName,
	SQLSMALLINT        cchFkCatalogName,
	_In_reads_opt_(cchFkSchemaName) SQLWCHAR     *szFkSchemaName,
	SQLSMALLINT        cchFkSchemaName,
	_In_reads_opt_(cchFkTableName) SQLWCHAR      *szFkTableName,
	SQLSMALLINT        cchFkTableName)
{
	/*INDENT-OFF*/
#	define FOREIGN_KEYS_EMPTY \
	"{" \
		"\"columns\":[" \
			"{\"name\":\"PKTABLE_CAT\","	"\"type\":\"TEXT\"}," \
			"{\"name\":\"PKTABLE_SCHEM\","	"\"type\":\"TEXT\"}," \
			"{\"name\":\"PKTABLE_NAME\","	"\"type\":\"TEXT\"}," \
			"{\"name\":\"PKCOLUMN_NAME\","	"\"type\":\"TEXT\"}," \
			"{\"name\":\"FKTABLE_CAT\","	"\"type\":\"TEXT\"}," \
			"{\"name\":\"FKTABLE_SCHEM\","	"\"type\":\"TEXT\"}," \
			"{\"name\":\"FKTABLE_NAME\","	"\"type\":\"TEXT\"}," \
			"{\"name\":\"FKCOLUMN_NAME\","	"\"type\":\"TEXT\"}," \
			"{\"name\":\"KEY_SEQ\","		"\"type\":\"SHORT\"}," \
			"{\"name\":\"UPDATE_RULE\","	"\"type\":\"SHORT\"}," \
			"{\"name\":\"DELETE_RULE\","	"\"type\":\"SHORT\"}," \
			"{\"name\":\"FK_NAME\","		"\"type\":\"TEXT\"}," \
			"{\"name\":\"PK_NAME\","		"\"type\":\"TEXT\"}," \
			"{\"name\":\"DEFERRABILITY\","	"\"type\":\"SHORT\"}" \
		"]," \
		"\"rows\":[]" \
	"}"
	/*INDENT-ON*/

	INFOH(hstmt, "no foreign keys supported.");
	return fake_answer(hstmt, FOREIGN_KEYS_EMPTY,
			sizeof(FOREIGN_KEYS_EMPTY) - /*\0*/1);

#	undef FOREIGN_KEYS_EMPTY
}

SQLRETURN EsSQLPrimaryKeysW(
	SQLHSTMT           hstmt,
	_In_reads_opt_(cchCatalogName) SQLWCHAR    *szCatalogName,
	SQLSMALLINT        cchCatalogName,
	_In_reads_opt_(cchSchemaName) SQLWCHAR     *szSchemaName,
	SQLSMALLINT        cchSchemaName,
	_In_reads_opt_(cchTableName) SQLWCHAR      *szTableName,
	SQLSMALLINT        cchTableName)
{
	/*INDENT-OFF*/
#	define PRIMARY_KEYS_EMPTY \
	"{" \
		"\"columns\":[" \
			"{\"name\":\"TABLE_CAT\","		"\"type\":\"TEXT\"}," \
			"{\"name\":\"TABLE_SCHEM\","	"\"type\":\"TEXT\"}," \
			"{\"name\":\"TABLE_NAME\","		"\"type\":\"TEXT\"}," \
			"{\"name\":\"COLUMN_NAME\","	"\"type\":\"TEXT\"}," \
			"{\"name\":\"KEY_SEQ\","		"\"type\":\"SHORT\"}," \
			"{\"name\":\"PK_NAME\","		"\"type\":\"TEXT\"}" \
		"]," \
		"\"rows\":[]" \
	"}"
	/*INDENT-ON*/

	INFOH(hstmt, "no primary keys supported.");
	return fake_answer(hstmt, PRIMARY_KEYS_EMPTY,
			sizeof(PRIMARY_KEYS_EMPTY) - /*\0*/1);

#	undef PRIMARY_KEYS_EMPTY
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=78 : */
