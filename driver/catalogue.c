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

/* TODO add schema, when supported */
#define SQL_COLUMNS(...) \
	"SYS COLUMNS" __VA_ARGS__ \
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
static size_t quote_tokens(SQLWCHAR *src, size_t len, SQLWCHAR *dest)
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
	esodbc_dbc_st *dbc = HDRH(stmt)->dbc;
	SQLRETURN ret = SQL_ERROR;
	/* print buffer size */
	static const size_t PBUF_CNT = sizeof(SQL_TABLES)
		+ sizeof(SQL_TABLES_CAT)
		+ sizeof(SQL_TABLES_TAB)
		+ sizeof(SQL_TABLES_TYP)
		+ 3 * ESODBC_MAX_IDENTIFIER_LEN /* it has 4x 0-term space */;
	/* work buffer sizing:
	 * - type: 2x: "a,b,c" -> "'a','b','c'" : each "x," => "'x',"
	 * - escaping: 2x: x -> \x */
	static const size_t WBUF_CNT = 2 * ESODBC_MAX_IDENTIFIER_LEN;
	SQLWCHAR *pbuf; /* print buffer for the final statement */
	SQLWCHAR *wbuf; /* work buffer: for table type and escaping */
	size_t cnt, pos;
	wstr_st esrc, edst; /* escaping src, dst */

	/* the buffer size could actually be more accurately calculated (i.e. to a
	 * smaller size than the max) */
	pbuf = malloc((PBUF_CNT + WBUF_CNT) * sizeof(SQLWCHAR));
	if (! pbuf) {
		ERRNH(stmt, "OOM: %zu wchar_t.", PBUF_CNT + WBUF_CNT);
		RET_HDIAGS(stmt, SQL_STATE_HY001);
	} else {
		wbuf = &pbuf[PBUF_CNT];
	}
	edst.str = wbuf;

	pos = sizeof(SQL_TABLES) - 1;
	wmemcpy(pbuf, MK_WPTR(SQL_TABLES), pos);

	if (CatalogName) {
		esrc.str = CatalogName;
		if (NameLength1 == SQL_NTS) {
			esrc.cnt = wcslen(esrc.str);
			if (ESODBC_MAX_IDENTIFIER_LEN < esrc.cnt) {
				ERRH(stmt, "catalog identifier name '" LWPDL "' too long "
					"(%zd. max=%d).", LWSTR(&esrc), esrc.cnt,
					ESODBC_MAX_IDENTIFIER_LEN);
				SET_HDIAG(stmt, SQL_STATE_HY090, "catalog name too long", 0);
				goto end;
			}
		} else {
			esrc.cnt = NameLength1;
		}

		if (dbc->auto_esc_pva || stmt->metadata_id) {
			metadata_id_escape(&esrc, &edst, (BOOL)stmt->metadata_id);
		}

		cnt = swprintf(pbuf + pos, PBUF_CNT - pos, SQL_TABLES_CAT,
				(int)edst.cnt, edst.str);
		if (cnt <= 0) {
			ERRH(stmt, "failed to print 'catalog' for tables catalog SQL.");
			SET_HDIAG(stmt, SQL_STATE_HY000, "internal printing error", 0);
			goto end;
		} else {
			pos += cnt;
		}
	}

	if (SchemaName) {
		esrc.str = SchemaName;
		if (NameLength2 == SQL_NTS) {
			esrc.cnt = wcslen(esrc.str);
			if (ESODBC_MAX_IDENTIFIER_LEN < esrc.cnt) {
				ERRH(stmt, "schema identifier name '" LWPDL "' too long "
					"(%zd. max=%d).", LWSTR(&esrc), esrc.cnt,
					ESODBC_MAX_IDENTIFIER_LEN);
				SET_HDIAG(stmt, SQL_STATE_HY090, "schema name too long", 0);
				goto end;
			}
		} else {
			esrc.cnt = NameLength2;
		}

		/* TODO: server support needed for sch. name filtering */
		if (wszmemcmp(esrc.str, MK_WPTR(SQL_ALL_SCHEMAS), (long)esrc.cnt)) {
			ERRH(stmt, "filtering by schemas is not supported.");
			SET_HDIAG(stmt, SQL_STATE_IM001, "schema filtering not supported",
				0);
			goto end;
		}
	}

	if (TableName) {
		esrc.str = TableName;
		if (NameLength3 == SQL_NTS) {
			esrc.cnt = wcslen(esrc.str);
			if (ESODBC_MAX_IDENTIFIER_LEN < esrc.cnt) {
				ERRH(stmt, "table identifier name '" LWPDL "' too long "
					"(%zd. max=%d).", LWSTR(&esrc), esrc.cnt,
					ESODBC_MAX_IDENTIFIER_LEN);
				SET_HDIAG(stmt, SQL_STATE_HY090, "table name too long", 0);
				goto end;
			}
		} else {
			esrc.cnt = NameLength3;
		}

		if (dbc->auto_esc_pva || stmt->metadata_id) {
			metadata_id_escape(&esrc, &edst, (BOOL)stmt->metadata_id);
		}

		cnt = swprintf(pbuf + pos, PBUF_CNT - pos, SQL_TABLES_TAB,
				(int)edst.cnt, edst.str);
		if (cnt <= 0) {
			ERRH(stmt, "failed to print 'table' for tables catalog SQL.");
			SET_HDIAG(stmt, SQL_STATE_HY000, "internal printing error", 0);
			goto end;
		} else {
			pos += cnt;
		}
	}

	if (TableType) {
		esrc.str = TableType;
		if (NameLength4 == SQL_NTS) {
			esrc.cnt = wcslen(esrc.str);
			if (ESODBC_MAX_IDENTIFIER_LEN < esrc.cnt) {
				ERRH(stmt, "type identifier name '" LWPDL "' too long "
					"(%zd. max=%d).", LWSTR(&esrc), esrc.cnt,
					ESODBC_MAX_IDENTIFIER_LEN);
				SET_HDIAG(stmt, SQL_STATE_HY090, "type name too long", 0);
				goto end;
			}
		} else {
			esrc.cnt = NameLength4;
		}

		/* Only print TYPE if non-empty. This is incorrect, by the book,
		 * but there's little use to specifying an empty string as the type
		 * (vs NULL), so this should hopefully be safe. (ES/GH#40775) */
		if (0 < esrc.cnt) {
			/* Here, "each value can be enclosed in single quotation marks (')
			 * or unquoted" => quote if not quoted (see GH#30398). */
			if (! wcsnstr(esrc.str, esrc.cnt, L'\'')) {
				edst.cnt = quote_tokens(esrc.str, esrc.cnt, edst.str);
			} else {
				edst = esrc;
			}

			cnt = swprintf(pbuf + pos, PBUF_CNT - pos, SQL_TABLES_TYP,
					(int)edst.cnt, edst.str);
			if (cnt <= 0) {
				ERRH(stmt, "failed to print 'type' for tables catalog SQL.");
				SET_HDIAG(stmt, SQL_STATE_HY000, "internal printing error", 0);
				goto end;
			} else {
				pos += cnt;
			}
		}
	}

	DBGH(stmt, "tables catalog SQL [%zu]:`" LWPDL "`.", pos, (int)pos, pbuf);

	ret = EsSQLFreeStmt(stmt, ESODBC_SQL_CLOSE);
	assert(SQL_SUCCEEDED(ret)); /* can't return error */
	ret = attach_sql(stmt, pbuf, pos);
	if (SQL_SUCCEEDED(ret)) {
		ret = EsSQLExecute(stmt);
	}
end:
	free(pbuf);
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
	esodbc_dbc_st *dbc = HDRH(stmt)->dbc;
	SQLRETURN ret = SQL_ERROR;
	SQLWCHAR *pbuf; /* print buffer */
	size_t arg_cnt, cnt;
	wstr_st src_tab, src_col;
	wstr_st catalog, schema, dst_tab, dst_col;

	if (szCatalogName) {
		catalog.str = szCatalogName;
		if (cchCatalogName == SQL_NTS) {
			catalog.cnt = wcslen(catalog.str);
			if (ESODBC_MAX_IDENTIFIER_LEN < catalog.cnt) {
				ERRH(stmt, "catalog identifier name '" LWPDL "' too long "
					"(%d. max=%d).", LWSTR(&catalog), catalog.cnt,
					ESODBC_MAX_IDENTIFIER_LEN);
				RET_HDIAG(stmt, SQL_STATE_HY090, "catalog name too long", 0);
			}
		} else {
			catalog.cnt = cchCatalogName;
		}
	} else {
		catalog.str = NULL;
		catalog.cnt = 0;
	}

	if (szSchemaName) {
		schema.str = szSchemaName;
		if (cchSchemaName == SQL_NTS) {
			schema.cnt = wcslen(schema.str);
			if (ESODBC_MAX_IDENTIFIER_LEN < schema.cnt) {
				ERRH(stmt, "schema identifier name '" LWPDL "' too long "
					"(%d. max=%d).", LWSTR(&schema), schema.cnt,
					ESODBC_MAX_IDENTIFIER_LEN);
				RET_HDIAG(stmt, SQL_STATE_HY090, "schema name too long", 0);
			}
		} else {
			schema.cnt = cchSchemaName;
		}
	} else {
		schema.str = MK_WPTR(SQL_ALL_SCHEMAS);
		schema.cnt = sizeof(SQL_ALL_SCHEMAS) - /*0-term*/1;
	}

	/* TODO: server support needed for schema name filtering */
	if (schema.cnt && wszmemcmp(schema.str, MK_WPTR(SQL_ALL_SCHEMAS),
			(long)schema.cnt)) {
		ERRH(stmt, "filtering by schemas is not supported.");
		RET_HDIAG(stmt, SQL_STATE_IM001, "schema filtering not supported", 0);
	}

	if (szTableName) {
		src_tab.str = szTableName;
		if (cchTableName == SQL_NTS) {
			src_tab.cnt = wcslen(src_tab.str);
			if (ESODBC_MAX_IDENTIFIER_LEN < src_tab.cnt) {
				ERRH(stmt, "table identifier name '" LWPDL "' too long "
					"(%d. max=%d).", LWSTR(&src_tab), src_tab.cnt,
					ESODBC_MAX_IDENTIFIER_LEN);
				RET_HDIAG(stmt, SQL_STATE_HY090, "table name too long", 0);
			}
		} else {
			src_tab.cnt = cchTableName;
		}
	} else {
		src_tab.str = MK_WPTR(ESODBC_ALL_TABLES);
		src_tab.cnt = sizeof(ESODBC_ALL_TABLES) - /*0-term*/1;
	}

	if (szColumnName) {
		src_col.str = szColumnName;
		if (cchColumnName == SQL_NTS) {
			src_col.cnt = wcslen(src_col.str);
			if (ESODBC_MAX_IDENTIFIER_LEN < src_col.cnt) {
				ERRH(stmt, "column identifier name '" LWPDL "' too long "
					"(%d. max=%d).", LWSTR(&src_col), src_col.cnt,
					ESODBC_MAX_IDENTIFIER_LEN);
				RET_HDIAG(stmt, SQL_STATE_HY090, "column name too long", 0);
			}
		} else {
			src_col.cnt = cchColumnName;
		}
	} else {
		src_col.str = MK_WPTR(ESODBC_ALL_COLUMNS);
		src_col.cnt = sizeof(ESODBC_ALL_COLUMNS) - /*0-term*/1;
	}

	/* determine a maximum buff size for any of the provided value args */
	arg_cnt = catalog.cnt;
	arg_cnt = src_tab.cnt <= arg_cnt ? arg_cnt : src_tab.cnt;
	arg_cnt = src_col.cnt <= arg_cnt ? arg_cnt : src_col.cnt;
	arg_cnt *= 2; /* escaping can up to double the buffer */

	/* size of chunk to allocate for the print buffer, plus each escape buff */
	cnt = sizeof(SQL_COLUMNS(SQL_COL_CAT));
	/* 5: (cat + tab + col) for printing + (tab + col) for escaping */
	cnt += 5 * arg_cnt;

	/* allocate a chunk for the printing buffer plus for each escape buffer */
	pbuf = malloc(cnt * sizeof(*pbuf));
	if (! pbuf) {
		ERRNH(stmt, "OOM: %zu wchar_t.", cnt);
		RET_HDIAGS(stmt, SQL_STATE_HY001);
	} else {
		dst_tab.str = &pbuf[sizeof(SQL_COLUMNS(SQL_COL_CAT)) + 3 * arg_cnt];
		dst_col.str = &pbuf[sizeof(SQL_COLUMNS(SQL_COL_CAT)) + 4 * arg_cnt];
	}

	/* catalog argument is never a patern value -> no escaping */
	/* escape table and column args */
	if (dbc->auto_esc_pva || stmt->metadata_id) {
		metadata_id_escape(&src_tab, &dst_tab, (BOOL)stmt->metadata_id);
		metadata_id_escape(&src_col, &dst_col, (BOOL)stmt->metadata_id);
	}

	/* print SQL to send to server */
	if (catalog.str) {
		cnt = swprintf(pbuf, cnt, SQL_COLUMNS(SQL_COL_CAT),
				(int)catalog.cnt, catalog.str,
				(int)dst_tab.cnt, dst_tab.str,
				(int)dst_col.cnt, dst_col.str);
	} else {
		cnt = swprintf(pbuf, cnt, SQL_COLUMNS(),
				(int)dst_tab.cnt, dst_tab.str,
				(int)dst_col.cnt, dst_col.str);
	}
	if (cnt <= 0) {
		ERRH(stmt, "failed to print 'columns' catalog SQL.");
		SET_HDIAG(stmt, SQL_STATE_HY000, "internal printing error", 0);
		goto end;
	}

	ret = EsSQLFreeStmt(stmt, ESODBC_SQL_CLOSE);
	assert(SQL_SUCCEEDED(ret)); /* can't return error */
	ret = attach_sql(stmt, pbuf, cnt);
	if (SQL_SUCCEEDED(ret)) {
		ret = EsSQLExecute(stmt);
	}
end:
	free(pbuf);
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
