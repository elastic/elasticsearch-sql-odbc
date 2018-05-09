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


#define SYS_CATALOGS \
	"SYS CATALOGS"

/* SYS TABLES synthax tokens; these need to stay broken down, since this
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
	SQLWCHAR *catalog;

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
	if (! SQL_SUCCEEDED(post_statement(stmt))) {
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
		catalog = MK_WPTR(""); /* empty string, it's not quite an error */
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
			catalog = MK_WPTR("");
		} else {
			catalog = buff;
			DBGH(dbc, "current catalog (first value returned): `" LWPD "`.",
					catalog);
		}
	}

	if (! SQL_SUCCEEDED(write_wptr(&dbc->hdr.diag, dest, catalog, room,
					&used))) {
		ERRH(dbc, "failed to copy catalog: `" LWPD "`.", catalog);
		used = -1; /* write_wptr() can change pointer, and still fail */
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
	SQLWCHAR wbuf[sizeof(SQL_TABLES) + 2 * ESODBC_MAX_IDENTIFIER_LEN];
	SQLWCHAR *table, *schema, *catalog;
	size_t cnt_tab, cnt_sch, cnt_cat, pos;

	if (stmt->metadata_id == SQL_TRUE)
		FIXME; // FIXME

	if (CatalogName) {
		catalog = CatalogName;
		if (NameLength1 == SQL_NTS) {
			cnt_cat = wcslen(catalog);
			if (ESODBC_MAX_IDENTIFIER_LEN < cnt_cat) {
				ERRH(stmt, "catalog identifier name '" LTPDL "' too long "
						"(%d. max=%d).", cnt_cat, catalog, cnt_cat,
						ESODBC_MAX_IDENTIFIER_LEN);
				RET_HDIAG(stmt, SQL_STATE_HY090, "catalog name too long", 0);
			}
		} else {
			cnt_cat = NameLength1;
		}
	} else {
		catalog = MK_WPTR(SQL_ALL_CATALOGS);
		cnt_cat = sizeof(SQL_ALL_CATALOGS) - /*0-term*/1;
	}

	if (SchemaName) {
		schema = SchemaName;
		if (NameLength2 == SQL_NTS) {
			cnt_sch = wcslen(schema);
			if (ESODBC_MAX_IDENTIFIER_LEN < cnt_sch) {
				ERRH(stmt, "schema identifier name '" LTPDL "' too long "
						"(%d. max=%d).", cnt_sch, schema, cnt_sch,
						ESODBC_MAX_IDENTIFIER_LEN);
				RET_HDIAG(stmt, SQL_STATE_HY090, "schema name too long", 0);
			}
		} else {
			cnt_sch = NameLength2;
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
	if (TableName) {
		table = TableName;
		if (NameLength3 == SQL_NTS) {
			cnt_tab = wcslen(table);
			if (ESODBC_MAX_IDENTIFIER_LEN < cnt_tab) {
				ERRH(stmt, "table identifier name '" LTPDL "' too long "
						"(%d. max=%d).", cnt_tab, table, cnt_tab,
						ESODBC_MAX_IDENTIFIER_LEN);
				RET_HDIAG(stmt, SQL_STATE_HY090, "table name too long", 0);
			}
		} else {
			cnt_tab = NameLength3;
		}
	} else {
		table = MK_WPTR(ESODBC_ALL_TABLES);
		cnt_tab = sizeof(ESODBC_ALL_TABLES) - /*0-term*/1;
	}
#if 1 // TODO: GH#4334
	if (cnt_tab == 0) {
		table = MK_WPTR(ESODBC_ALL_TABLES);
		cnt_tab = sizeof(ESODBC_ALL_TABLES) - /*0-term*/1;
	}
#endif // 1

	/* print SQL to send to server */
	pos = swprintf(wbuf, sizeof(wbuf)/sizeof(wbuf[0]), SQL_TABLES,
			(int)cnt_cat, catalog, (int)cnt_tab, table);
	if (pos <= 0) {
		ERRH(stmt, "failed to print 'tables' catalog SQL.");
		RET_HDIAGS(stmt, SQL_STATE_HY000);
	}

	ret = EsSQLFreeStmt(stmt, ESODBC_SQL_CLOSE);
	assert(SQL_SUCCEEDED(ret)); /* can't return error */
	ret = attach_sql(stmt, wbuf, pos);
	if (SQL_SUCCEEDED(ret))
		ret = post_statement(stmt);
	return ret;
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
	SQLRETURN ret;
	SQLWCHAR wbuf[sizeof(SQL_COLUMNS(SQL_COL_CAT)) +
		3 * ESODBC_MAX_IDENTIFIER_LEN];
	SQLWCHAR *catalog, *schema, *table, *column;
	size_t cnt_cat, cnt_sch, cnt_tab, cnt_col, pos;

	if (stmt->metadata_id == SQL_TRUE)
		FIXME; // FIXME
	
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
	WARNH(hstmt, "no special columns available.");
	STMT_FORCE_NODATA(STMH(hstmt));
	return SQL_SUCCESS;
}


SQLRETURN EsSQLForeignKeysW(
		SQLHSTMT           hstmt,
		_In_reads_opt_(cchPkCatalogName) SQLWCHAR*    szPkCatalogName,
		SQLSMALLINT        cchPkCatalogName,
		_In_reads_opt_(cchPkSchemaName) SQLWCHAR*     szPkSchemaName,
		SQLSMALLINT        cchPkSchemaName,
		_In_reads_opt_(cchPkTableName) SQLWCHAR*      szPkTableName,
		SQLSMALLINT        cchPkTableName,
		_In_reads_opt_(cchFkCatalogName) SQLWCHAR*    szFkCatalogName,
		SQLSMALLINT        cchFkCatalogName,
		_In_reads_opt_(cchFkSchemaName) SQLWCHAR*     szFkSchemaName,
		SQLSMALLINT        cchFkSchemaName,
		_In_reads_opt_(cchFkTableName) SQLWCHAR*      szFkTableName,
		SQLSMALLINT        cchFkTableName)
{
	WARNH(hstmt, "no foreign keys supported.");
	STMT_FORCE_NODATA(STMH(hstmt));
	return SQL_SUCCESS;
}

SQLRETURN SQL_API EsSQLPrimaryKeysW(
		SQLHSTMT           hstmt,
		_In_reads_opt_(cchCatalogName) SQLWCHAR*    szCatalogName,
		SQLSMALLINT        cchCatalogName,
		_In_reads_opt_(cchSchemaName) SQLWCHAR*     szSchemaName,
		SQLSMALLINT        cchSchemaName,
		_In_reads_opt_(cchTableName) SQLWCHAR*      szTableName,
		SQLSMALLINT        cchTableName)
{
	WARNH(hstmt, "no primary keys supported.");
	STMT_FORCE_NODATA(STMH(hstmt));
	return SQL_SUCCESS;
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
