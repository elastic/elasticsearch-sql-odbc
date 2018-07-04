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


#define SYS_CATALOGS \
	"SYS CATALOGS"

/* SYS TABLES syntax tokens; these need to stay broken down, since this
 * query makes a difference between a predicate being '%' or left out */
// TODO: schema, when supported
#define SQL_TABLES \
	"SYS TABLES"
#define SQL_TABLES_CAT \
	" CATALOG LIKE " ESODBC_STRING_DELIM WPFWP_LDESC ESODBC_STRING_DELIM
#define SQL_TABLES_TAB \
	" LIKE " ESODBC_STRING_DELIM WPFWP_LDESC ESODBC_STRING_DELIM
#define SQL_TABLES_TYP \
	" TYPE " WPFWP_LDESC

// TODO add schema, when supported
#define SQL_COLUMNS(...)		"SYS COLUMNS" __VA_ARGS__ \
	" TABLE LIKE " ESODBC_STRING_DELIM WPFWP_LDESC ESODBC_STRING_DELIM \
	" LIKE " ESODBC_STRING_DELIM WPFWP_LDESC ESODBC_STRING_DELIM
#define SQL_COL_CAT \
	" CATALOG " ESODBC_STRING_DELIM WPFWP_LDESC ESODBC_STRING_DELIM \


/* writes into 'dest', of size 'room', the current catalog of 'dbc'.
 * returns negative on error, or the char count written otherwise */
SQLSMALLINT copy_current_catalog(esodbc_dbc_st *dbc, SQLWCHAR *dest,
	SQLSMALLINT room)
{
	esodbc_stmt_st *stmt = NULL;
	SQLSMALLINT used = -1; /*failure*/
	SQLLEN row_cnt;
	SQLLEN ind_len = SQL_NULL_DATA;
	SQLWCHAR buff[ESODBC_MAX_IDENTIFIER_LEN];
	wstr_st catalog;

	if (! SQL_SUCCEEDED(EsSQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) {
		ERRH(dbc, "failed to alloc a statement handle.");
		return -1;
	}
	assert(stmt);

	if (! SQL_SUCCEEDED(attach_sql(stmt, MK_WPTR(SYS_CATALOGS),
				sizeof(SYS_CATALOGS) - 1))) {
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
		WARNH(stmt, "Elasticsearch returned no current catalog.");
		catalog = MK_WSTR(""); /* empty string, it's not quite an error */
	} else {
		DBGH(stmt, "Elasticsearch catalogs rows count: %ld.", row_cnt);
		if (1 < row_cnt) {
			WARNH(dbc, "Elasticsearch connected to %d clusters, returning "
				"the first's name as current catalog.", row_cnt);
		}

		if (! SQL_SUCCEEDED(EsSQLBindCol(stmt, /*col#*/1, SQL_C_WCHAR, buff,
					sizeof(buff), &ind_len))) {
			ERRH(dbc, "failed to bind first column.");
			goto end;
		}
		if (! SQL_SUCCEEDED(EsSQLFetch(stmt))) {
			ERRH(stmt, "failed to fetch results.");
			goto end;
		}
		if (ind_len <= 0) {
			WARNH(dbc, "NULL catalog received."); /*tho maybe != NULL_DATA */
			catalog = MK_WSTR("");
		} else {
			catalog = (wstr_st) {
				buff, ind_len
			};
			DBGH(dbc, "current catalog (first value returned): `" LWPDL "`.",
				LWSTR(&catalog));
		}
	}

	if (! SQL_SUCCEEDED(write_wstr(dbc, dest, &catalog, room, &used))) {
		ERRH(dbc, "failed to copy catalog: `" LWPDL "`.", LWSTR(&catalog));
		used = -1; /* write_wstr() can change pointer, and still fail */
	}

end:
	/* safe even if no binding occured */
	if (! SQL_SUCCEEDED(EsSQLFreeStmt(stmt, SQL_UNBIND))) {
		ERRH(stmt, "failed to unbind statement");
		used = -1;
	}
	if (! SQL_SUCCEEDED(EsSQLFreeHandle(SQL_HANDLE_STMT, stmt))) {
		ERRH(dbc, "failed to free statement handle!");
	}
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
				}
				copying = TRUE;
		}
		*pos ++ = src[i];
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
	SQLRETURN ret;
	/* b/c declaring an array with a const doesn't work with MSVC's compiler */
	enum wbuf_len { wbuf_len = sizeof(SQL_TABLES)
			+ sizeof(SQL_TABLES_CAT)
			+ sizeof(SQL_TABLES_TAB)
			+ sizeof(SQL_TABLES_TYP)
			+ 3 * ESODBC_MAX_IDENTIFIER_LEN /* it has 4x 0-term space */
	};
	SQLWCHAR wbuf[wbuf_len];
	SQLWCHAR *table, *schema, *catalog, *type;
	size_t cnt_tab, cnt_sch, cnt_cat, cnt_typ, pos;
	/* 2x: "a,b,c" -> "'a','b','c'" : each "x," => "'x'," */
	SQLWCHAR typ_buf[2 * ESODBC_MAX_IDENTIFIER_LEN];

	if (stmt->metadata_id == SQL_TRUE) {
		FIXME;    // FIXME
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
				RET_HDIAG(stmt, SQL_STATE_HY090, "catalog name too long", 0);
			}
		} else {
			cnt_cat = NameLength1;
		}

		cnt_cat = swprintf(wbuf + pos, wbuf_len - pos, SQL_TABLES_CAT,
				(int)cnt_cat, catalog);
		if (cnt_cat <= 0) {
			ERRH(stmt, "failed to print 'catalog' for tables catalog SQL.");
			RET_HDIAGS(stmt, SQL_STATE_HY000);
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
				RET_HDIAG(stmt, SQL_STATE_HY090, "schema name too long", 0);
			}
		} else {
			cnt_sch = NameLength2;
		}

		/* TODO: server support needed for sch. name filtering */
		if (wszmemcmp(schema, MK_WPTR(SQL_ALL_SCHEMAS), (long)cnt_sch)) {
			ERRH(stmt, "filtering by schemas is not supported.");
			RET_HDIAG(stmt, SQL_STATE_IM001, "schema filtering not supported",
				0);
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
				RET_HDIAG(stmt, SQL_STATE_HY090, "table name too long", 0);
			}
		} else {
			cnt_tab = NameLength3;
		}

		cnt_tab = swprintf(wbuf + pos, wbuf_len - pos, SQL_TABLES_TAB,
				(int)cnt_tab, table);
		if (cnt_tab <= 0) {
			ERRH(stmt, "failed to print 'table' for tables catalog SQL.");
			RET_HDIAGS(stmt, SQL_STATE_HY000);
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
				RET_HDIAG(stmt, SQL_STATE_HY090, "type name too long", 0);
			}
		} else {
			cnt_typ = NameLength4;
		}

		/* In this argument, "each value can be enclosed in single quotation
		 * marks (') or unquoted" => quote if not quoted (see GH#30398). */
		if (! wcsnstr(type, cnt_typ, L'\'')) {
			cnt_typ = quote_tokens(type, cnt_typ, typ_buf);
			type = typ_buf;
		}

		cnt_typ = swprintf(wbuf + pos, wbuf_len - pos, SQL_TABLES_TYP,
				(int)cnt_typ, type);
		if (cnt_typ <= 0) {
			ERRH(stmt, "failed to print 'type' for tables catalog SQL.");
			RET_HDIAGS(stmt, SQL_STATE_HY000);
		} else {
			pos += cnt_typ;
		}
	}

	DBGH(stmt, "tables catalog SQL [%d]:`" LWPDL "`.", pos, pos, wbuf);

	ret = EsSQLFreeStmt(stmt, ESODBC_SQL_CLOSE);
	assert(SQL_SUCCEEDED(ret)); /* can't return error */
	ret = attach_sql(stmt, wbuf, pos);
	if (SQL_SUCCEEDED(ret)) {
		ret = EsSQLExecute(stmt);
	}
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
	SQLRETURN ret;
	SQLWCHAR wbuf[sizeof(SQL_COLUMNS(SQL_COL_CAT))
		+ 3 * ESODBC_MAX_IDENTIFIER_LEN];
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

	/* print SQL to send to server */
	if (catalog) {
		pos = swprintf(wbuf, sizeof(wbuf)/sizeof(wbuf[0]),
				SQL_COLUMNS(SQL_COL_CAT), (int)cnt_cat, catalog,
				(int)cnt_tab, table, (int)cnt_col, column);
	} else {
		pos = swprintf(wbuf, sizeof(wbuf)/sizeof(wbuf[0]),
				SQL_COLUMNS(),
				(int)cnt_tab, table, (int)cnt_col, column);
	}
	if (pos <= 0) {
		ERRH(stmt, "failed to print 'columns' catalog SQL.");
		RET_HDIAGS(stmt, SQL_STATE_HY000);
	}

	ret = EsSQLFreeStmt(stmt, ESODBC_SQL_CLOSE);
	assert(SQL_SUCCEEDED(ret)); /* can't return error */
	ret = attach_sql(stmt, wbuf, pos);
	if (SQL_SUCCEEDED(ret)) {
		ret = EsSQLExecute(stmt);
	}
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
	// TODO: is there a "rowid" equivalent: ID uniquely a ROW in the table?
	// or unique indexes equivalents
	WARNH(hstmt, "no special columns available.");
	STMT_FORCE_NODATA(STMH(hstmt));
	return SQL_SUCCESS;
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
	WARNH(hstmt, "no foreign keys supported.");
	STMT_FORCE_NODATA(STMH(hstmt));
	return SQL_SUCCESS;
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
	WARNH(hstmt, "no primary keys supported.");
	STMT_FORCE_NODATA(STMH(hstmt));
	return SQL_SUCCESS;
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
