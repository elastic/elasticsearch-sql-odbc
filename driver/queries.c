/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <inttypes.h>
#include <stdlib.h>
#include <windows.h> /* WideCharToMultiByte() */
#include <float.h>
#include <math.h>
#include <errno.h>

#include "ujdecode.h"
#include "timestamp.h"

#include "queries.h"
#include "log.h"
#include "connect.h"
#include "info.h"

#define JSON_VAL_NULL			"null"
#define JSON_VAL_TRUE			"true"
#define JSON_VAL_FALSE			"false"

/* key names used in Elastic/SQL REST/JSON answers */
#define JSON_ANSWER_COLUMNS		"columns"
#define JSON_ANSWER_ROWS		"rows"
#define JSON_ANSWER_CURSOR		"cursor"
#define JSON_ANSWER_STATUS		"status"
#define JSON_ANSWER_ERROR		"error"
#define JSON_ANSWER_ERR_TYPE	"type"
#define JSON_ANSWER_ERR_REASON	"reason"
#define JSON_ANSWER_COL_NAME	"name"
#define JSON_ANSWER_COL_TYPE	"type"


#define MSG_INV_SRV_ANS		"Invalid server answer"

#define TM_TO_TIMESTAMP_STRUCT(_tmp/*src*/, _tsp/*dst*/) \
	do { \
		(_tsp)->year = (_tmp)->tm_year + 1900; \
		(_tsp)->month = (_tmp)->tm_mon + 1; \
		(_tsp)->day = (_tmp)->tm_mday; \
		(_tsp)->hour = (_tmp)->tm_hour; \
		(_tsp)->minute = (_tmp)->tm_min; \
		(_tsp)->second = (_tmp)->tm_sec; \
	} while (0)

/* For fixed size (destination) types, the target buffer can't be NULL. */
#define REJECT_IF_NULL_DEST_BUFF(_s/*tatement*/, _p/*ointer*/) \
	do { \
		if (! _p) { \
			ERRH(_s, "destination buffer can't be NULL."); \
			RET_HDIAGS(stmt, SQL_STATE_HY009); \
		} \
	} while (0)
#define REJECT_AS_OOR(_stmt, _val, _fix_val, _target) /* Out Of Range */ \
	do { \
		if (_fix_val) { \
			ERRH(_stmt, "can't convert value %llx to %s: out of range", \
				_val, STR(_target)); \
		} else { \
			ERRH(_stmt, "can't convert value %f to %s: out of range", \
				_val, STR(_target)); \
		} \
		RET_HDIAGS(_stmt, SQL_STATE_22003); \
	} while (0)

/* TODO: this is inefficient: add directly into ujson4c lib (as .size of
 * ArrayItem struct, inc'd in arrayAddItem()) or local utils file. */
static size_t UJArraySize(UJObject obj)
{
	UJObject _u;
	size_t size = 0;
	void *iter = UJBeginArray(obj);
	if (iter) {
		while (UJIterArray(&iter, &_u)) {
			size ++;
		}
	}
	return size;
}

#if (0x0300 <= ODBCVER)
#	define ESSQL_TYPE_MIN		SQL_GUID
#	define ESSQL_TYPE_MAX		SQL_INTERVAL_MINUTE_TO_SECOND
#	define ESSQL_C_TYPE_MIN		SQL_C_UTINYINT
#	define ESSQL_C_TYPE_MAX		SQL_C_INTERVAL_MINUTE_TO_SECOND
#else /* ODBCVER < 0x0300 */
/* would need to adjust the limits  */
#	error "ODBC version not supported; must be 3.0 (0x0300) or higher"
#endif /* 0x0300 <= ODBCVER  */

#define ESSQL_NORM_RANGE		(ESSQL_TYPE_MAX - ESSQL_TYPE_MIN + 1)
#define ESSQL_C_NORM_RANGE		(ESSQL_C_TYPE_MAX - ESSQL_C_TYPE_MIN + 1)

/* conversion matrix SQL indexer */
#define	ESSQL_TYPE_IDX(_t)		(_t - ESSQL_TYPE_MIN)
/* conversion matrix C SQL indexer */
#define	ESSQL_C_TYPE_IDX(_t)	(_t - ESSQL_C_TYPE_MIN)

/* sparse SQL-C_SQL types conversion matrix, used for quick compatiblity check
 * on columns and parameters binding */
static BOOL compat_matrix[ESSQL_NORM_RANGE][ESSQL_C_NORM_RANGE] = {FALSE};

/* Note: check is array-access unsafe: types IDs must be validated prior to
 * checking compatibility (ex. meta type setting)  */
#define ESODBC_TYPES_COMPATIBLE(_sql, _csql) \
	/* if not within the ODBC range, it can only by a binary conversion;.. */ \
	((ESSQL_TYPE_MAX < _sql && _csql == SQL_C_BINARY) || \
		/* ..otheriwse use the conversion matrix */ \
		compat_matrix[ESSQL_TYPE_IDX(_sql)][ESSQL_C_TYPE_IDX(_csql)])

/* populates the compat_matrix as required in:
 * https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/converting-data-from-c-to-sql-data-types */
void queries_init()
{
	SQLSMALLINT i, j, sql, csql;
	/*INDENT-OFF*/
	SQLSMALLINT block_idx_sql[] = {SQL_CHAR, SQL_VARCHAR, SQL_LONGVARCHAR,
			SQL_WCHAR, SQL_WVARCHAR, SQL_WLONGVARCHAR, SQL_DECIMAL,
			SQL_NUMERIC, SQL_BIT, ESODBC_SQL_BOOLEAN, SQL_TINYINT,
			SQL_SMALLINT, SQL_INTEGER, SQL_BIGINT, SQL_REAL, SQL_FLOAT,
			SQL_DOUBLE
		};
	/*INDENT-ON*/
	SQLSMALLINT block_idx_csql[] = {SQL_C_CHAR, SQL_C_WCHAR,
			SQL_C_BIT, SQL_C_NUMERIC, SQL_C_STINYINT, SQL_C_UTINYINT,
			SQL_C_TINYINT, SQL_C_SBIGINT, SQL_C_UBIGINT, SQL_C_SSHORT,
			SQL_C_USHORT, SQL_C_SHORT, SQL_C_SLONG, SQL_C_ULONG,
			SQL_C_LONG, SQL_C_FLOAT, SQL_C_DOUBLE, SQL_C_BINARY
		};
	SQLSMALLINT to_csql_interval[] = {SQL_CHAR, SQL_VARCHAR, SQL_LONGVARCHAR,
			SQL_WCHAR, SQL_WVARCHAR, SQL_WLONGVARCHAR, SQL_DECIMAL,
			SQL_NUMERIC, SQL_TINYINT, SQL_SMALLINT, SQL_INTEGER, SQL_BIGINT
		};
	SQLSMALLINT from_sql_interval[] = {SQL_C_CHAR, SQL_C_WCHAR,
			SQL_C_BIT,SQL_C_NUMERIC, SQL_C_STINYINT, SQL_C_UTINYINT,
			SQL_C_TINYINT, SQL_C_SBIGINT, SQL_C_UBIGINT, SQL_C_SSHORT,
			SQL_C_USHORT, SQL_C_SHORT, SQL_C_SLONG, SQL_C_ULONG,
			SQL_C_LONG
		};
	SQLSMALLINT sql_interval[] = {SQL_INTERVAL_MONTH, SQL_INTERVAL_YEAR,
			SQL_INTERVAL_YEAR_TO_MONTH, SQL_INTERVAL_DAY,
			SQL_INTERVAL_HOUR, SQL_INTERVAL_MINUTE, SQL_INTERVAL_SECOND,
			SQL_INTERVAL_DAY_TO_HOUR, SQL_INTERVAL_DAY_TO_MINUTE,
			SQL_INTERVAL_DAY_TO_SECOND, SQL_INTERVAL_HOUR_TO_MINUTE,
			SQL_INTERVAL_HOUR_TO_SECOND, SQL_INTERVAL_MINUTE_TO_SECOND
		};
	SQLSMALLINT csql_interval[] = {SQL_C_INTERVAL_DAY, SQL_C_INTERVAL_HOUR,
			SQL_C_INTERVAL_MINUTE, SQL_C_INTERVAL_SECOND,
			SQL_C_INTERVAL_DAY_TO_HOUR, SQL_C_INTERVAL_DAY_TO_MINUTE,
			SQL_C_INTERVAL_DAY_TO_SECOND, SQL_C_INTERVAL_HOUR_TO_MINUTE,
			SQL_C_INTERVAL_HOUR_TO_SECOND,
			SQL_C_INTERVAL_MINUTE_TO_SECOND, SQL_C_INTERVAL_MONTH,
			SQL_C_INTERVAL_YEAR, SQL_C_INTERVAL_YEAR_TO_MONTH
		};
	SQLSMALLINT to_csql_datetime[] = {SQL_CHAR, SQL_VARCHAR, SQL_LONGVARCHAR,
			SQL_WCHAR, SQL_WVARCHAR, SQL_WLONGVARCHAR, SQL_TYPE_DATE,
			SQL_TYPE_TIME, SQL_TYPE_TIMESTAMP
		};
	SQLSMALLINT csql_datetime[] = {SQL_C_TYPE_DATE, SQL_C_TYPE_TIME,
			SQL_C_TYPE_TIMESTAMP
		};

	/* fill the compact block of TRUEs (growing from the upper left corner) */
	for (i = 0; i < sizeof(block_idx_sql)/sizeof(*block_idx_sql) ; i ++) {
		for (j = 0; j < sizeof(block_idx_csql)/sizeof(*block_idx_csql) ; j ++) {
			sql = block_idx_sql[i];
			csql = block_idx_csql[j];
			compat_matrix[ESSQL_TYPE_IDX(sql)][ESSQL_C_TYPE_IDX(csql)] = TRUE;
		}
	}

	/* SQL_C_ BINARY, CHAR, WCHAR and DEFAULT are comatible with all SQL types;
	 * this will set also non-ODBC intersections (but it's convenient) */
	for (sql = 0; sql < ESSQL_NORM_RANGE; sql ++) {
		compat_matrix[sql][ESSQL_C_TYPE_IDX(SQL_C_CHAR)] = TRUE;
		compat_matrix[sql][ESSQL_C_TYPE_IDX(SQL_C_WCHAR)] = TRUE;
		compat_matrix[sql][ESSQL_C_TYPE_IDX(SQL_C_BINARY)] = TRUE;
		compat_matrix[sql][ESSQL_C_TYPE_IDX(SQL_C_DEFAULT)] = TRUE;
	}

	/* ESODBC_SQL_NULL (NULL) is compabitle with all SQL_C types */
	for (csql = 0; csql < ESSQL_C_NORM_RANGE; csql ++) {
		compat_matrix[ESSQL_TYPE_IDX(ESODBC_SQL_NULL)][csql] = TRUE;
	}

	/* set conversions to INTERVAL_C */
	for (i = 0; i < sizeof(to_csql_interval)/sizeof(*to_csql_interval); i ++) {
		for (j = 0; j < sizeof(csql_interval)/sizeof(*csql_interval); j ++ ) {
			sql = to_csql_interval[i];
			csql = csql_interval[j];
			compat_matrix[ESSQL_TYPE_IDX(sql)][ESSQL_C_TYPE_IDX(csql)] = TRUE;
		}
	}

	/* set conversions from INTERVAL_SQL */
	for (i = 0; i < sizeof(sql_interval)/sizeof(*sql_interval); i ++) {
		for (j = 0; j < sizeof(from_sql_interval)/sizeof(*from_sql_interval);
			j ++ ) {
			sql = sql_interval[i];
			csql = from_sql_interval[j];
			compat_matrix[ESSQL_TYPE_IDX(sql)][ESSQL_C_TYPE_IDX(csql)] = TRUE;
		}
	}

	/* set conversions between date-time types */
	for (i = 0; i < sizeof(to_csql_datetime)/sizeof(*to_csql_datetime); i ++) {
		for (j = 0; j < sizeof(csql_datetime)/sizeof(*csql_datetime); j ++ ) {
			sql = to_csql_datetime[i];
			csql = csql_datetime[j];
			if (sql == SQL_TYPE_DATE && csql == SQL_C_TYPE_TIME) {
				continue;
			}
			if (sql == SQL_TYPE_TIME && csql == SQL_C_TYPE_DATE) {
				continue;
			}
			compat_matrix[ESSQL_TYPE_IDX(sql)][ESSQL_C_TYPE_IDX(csql)] = TRUE;
		}
	}

	/* GUID conversion */
	sql = SQL_GUID;
	csql = SQL_C_GUID;
	compat_matrix[ESSQL_TYPE_IDX(sql)][ESSQL_C_TYPE_IDX(csql)] = TRUE;
}


void clear_resultset(esodbc_stmt_st *stmt)
{
	DBGH(stmt, "clearing result set; vrows=%zd, nrows=%zd, frows=%zd.",
		stmt->rset.vrows, stmt->rset.nrows, stmt->rset.frows);
	if (stmt->rset.buff) {
		free(stmt->rset.buff);
	}
	if (stmt->rset.state) {
		UJFree(stmt->rset.state);
	}
	memset(&stmt->rset, 0, sizeof(stmt->rset));
}

/* Set the desriptor fields associated with "size". This step is needed since
 * the application could read the descriptors - like .length - individually,
 * rather than through functions that make use of get_col_size() (where we
 * could just read the es_type directly). */
static void set_col_size(esodbc_rec_st *rec)
{
	assert(rec->desc->type == DESC_TYPE_IRD);

	switch (rec->meta_type) {
		case METATYPE_UNKNOWN:
			/* SYS TYPES call */
			break;
		case METATYPE_EXACT_NUMERIC:
		case METATYPE_FLOAT_NUMERIC:
			/* ignore, the .precision field is not used in IRDs, its value is
			 * always read from es_type.column_size directly */
			break;

		/*
		 * https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/column-size : */
		case METATYPE_STRING:
		/* "The defined or maximum column size in characters of the
		 * column" */
		/* no break */
		case METATYPE_BIN:
		/* "The defined or maximum length in bytes of the column " */
		/* no break */
		case METATYPE_DATETIME:
			/* "number of characters in the character representation" */
			rec->length = rec->es_type->column_size;
			break;

		default:
			BUGH(rec->desc, "unsupported data c-type: %d.", rec->concise_type);
	}
}

static SQLRETURN attach_columns(esodbc_stmt_st *stmt, UJObject columns)
{
	esodbc_desc_st *ird;
	esodbc_dbc_st *dbc;
	esodbc_rec_st *rec;
	SQLRETURN ret;
	SQLSMALLINT recno;
	void *iter;
	UJObject col_o, name_o, type_o;
	wstr_st col_type;
	size_t ncols, i;
	const wchar_t *keys[] = {
		MK_WPTR(JSON_ANSWER_COL_NAME),
		MK_WPTR(JSON_ANSWER_COL_TYPE)
	};

	ird = stmt->ird;
	dbc = stmt->hdr.dbc;

	ncols = UJArraySize(columns);
	DBGH(stmt, "columns received: %zd.", ncols);
	ret = update_rec_count(ird, (SQLSMALLINT)ncols);
	if (! SQL_SUCCEEDED(ret)) {
		ERRH(stmt, "failed to set IRD's record count to %d.", ncols);
		HDIAG_COPY(ird, stmt);
		return ret;
	}

	iter = UJBeginArray(columns);
	if (! iter) {
		ERRH(stmt, "failed to obtain array iterator: %s.",
			UJGetError(stmt->rset.state));
		RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
	}
	recno = 0;
	while (UJIterArray(&iter, &col_o)) {
		if (UJObjectUnpack(col_o, 2, "SS", keys, &name_o, &type_o) < 2) {
			ERRH(stmt, "failed to decode JSON column: %s.",
				UJGetError(stmt->rset.state));
			RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		}
		rec = &ird->recs[recno]; // +recno

		ASSERT_INTEGER_TYPES_EQUAL(wchar_t, SQLWCHAR);
		rec->name.str = (SQLWCHAR *)UJReadString(name_o, &rec->name.cnt);
		if (! rec->name.str) {
			rec->name = MK_WSTR("");
		}

		col_type.str = (SQLWCHAR *)UJReadString(type_o, &col_type.cnt);

		assert(! rec->es_type);
		/* lookup the DBC-cashed ES type */
		for (i = 0; i < dbc->no_types; i ++) {
			if (EQ_CASE_WSTR(&dbc->es_types[i].type_name, &col_type)) {
				rec->es_type = &dbc->es_types[i];
				break;
			}
		}
		if (rec->es_type) {
			/* copy fileds pre-calculated at DB connect time */
			rec->concise_type = rec->es_type->data_type;
			rec->type = rec->es_type->sql_data_type;
			rec->datetime_interval_code = rec->es_type->sql_datetime_sub;
			rec->meta_type = rec->es_type->meta_type;
		} else if (! dbc->no_types) {
			/* the connection doesn't have yet the types cached (this is the
			 * caching call) and don't have access to the data itself either,
			 * just the column names & type names => set unknowns.  */
			rec->concise_type = SQL_UNKNOWN_TYPE;
			rec->type = SQL_UNKNOWN_TYPE;
			rec->datetime_interval_code = 0;
			rec->meta_type = METATYPE_UNKNOWN;
		} else {
			ERRH(stmt, "type lookup failed for `" LWPDL "`.",LWSTR(&col_type));
			RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		}

		set_col_size(rec);

		/* TODO: set remaining of settable fields (base table etc.) */

		/* "If a base column name does not exist (as in the case of columns
		 * that are expressions), then this variable contains an empty
		 * string." */
		rec->base_column_name = MK_WSTR("");
		/* "If a column does not have a label, the column name is returned. If
		 * the column is unlabeled and unnamed, an empty string is ret" */
		rec->label = rec->name.cnt ? rec->name : MK_WSTR("");

		assert(rec->name.str && rec->label.str);
		rec->unnamed = (rec->name.cnt || rec->label.cnt) ?
			SQL_NAMED : SQL_UNNAMED;

#ifndef NDEBUG
		//dump_record(rec);
#endif /* NDEBUG */

		DBGH(stmt, "column #%d: name=`" LWPDL "`, type=%d (`" LWPDL "`).",
			recno, LWSTR(&rec->name), rec->concise_type, LWSTR(&col_type));
		recno ++;
	}

	/* new columsn attached, need to check compatiblity */
	stmt->sql2c_conversion = CONVERSION_UNCHECKED;

	return SQL_SUCCESS;
}


/*
 * Processes a received answer:
 * - takes a dynamic buffer, buff, of length blen. Will handle the buff memory
 * even if the call fails.
 * - parses it, preparing iterators for SQLFetch()'ing.
 */
SQLRETURN TEST_API attach_answer(esodbc_stmt_st *stmt, char *buff, size_t blen)
{
	int unpacked;
	UJObject obj, columns, rows, cursor;
	const wchar_t *wcurs;
	size_t eccnt;
	const wchar_t *keys[] = {
		MK_WPTR(JSON_ANSWER_COLUMNS),
		MK_WPTR(JSON_ANSWER_ROWS),
		MK_WPTR(JSON_ANSWER_CURSOR)
	};

	/* clear any previous result set */
	if (STMT_HAS_RESULTSET(stmt)) {
		clear_resultset(stmt);
	}

	/* the statement takes ownership of mem obj */
	stmt->rset.buff = buff;
	stmt->rset.blen = blen;
	DBGH(stmt, "attaching answer [%zd]`" LCPDL "`.", blen, blen, buff);

	/* parse the entire JSON answer */
	obj = UJDecode(buff, blen, NULL, &stmt->rset.state);
	if (! obj) {
		ERRH(stmt, "failed to decode JSON answer (`%.*s`): %s.", blen, buff,
			stmt->rset.state ? UJGetError(stmt->rset.state) : "<none>");
		RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
	}
	columns = rows = cursor = NULL;
	/* extract the columns and rows objects */
	unpacked = UJObjectUnpack(obj, 3, "AAS", keys, &columns, &rows, &cursor);
	if (unpacked < /* 'rows' must always be present */1) {
		ERRH(stmt, "failed to unpack JSON answer (`%.*s`): %s.",
			blen, buff, UJGetError(stmt->rset.state));
		RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
	}

	/*
	 * set the internal cursor (UJSON4C array iterator)
	 */
	if (! rows) {
		ERRH(stmt, "no rows JSON object received in answer: `%.*s`[%zd].",
			blen, buff, blen);
		RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
	}
	stmt->rset.rows_iter = UJBeginArray(rows);
	if (! stmt->rset.rows_iter) {
#if 0 /* UJSON4C will return NULL above, for empty array (meh!) */
		ERRH(stmt, "failed to get iterrator on received rows: %s.",
			UJGetError(stmt->rset.state));
		RET_HDIAGS(stmt, SQL_STATE_HY000);
#else /*0*/
		DBGH(stmt, "received empty resultset array: forcing nodata.");
		STMT_FORCE_NODATA(stmt);
		stmt->rset.nrows = 0;
#endif /*0*/
	} else {
		stmt->rset.nrows = (size_t)UJArraySize(rows);
	}
	DBGH(stmt, "rows received in result set: %zd.", stmt->rset.nrows);

	/*
	 * copy Elastic's cursor (if there's one)
	 */
	if (cursor) {
		wcurs = UJReadString(cursor, &eccnt);
		if (eccnt) {
			/* this can happen automatically if hitting scroller size */
			if (! stmt->hdr.dbc->fetch.max) {
				INFOH(stmt, "no fetch size defined, but cursor returned.");
			}
			if (stmt->rset.ecurs)
				DBGH(stmt, "replacing old cursor `" LWPDL "`.",
					stmt->rset.eccnt, stmt->rset.ecurs);
			/* store new cursor vals */
			stmt->rset.ecurs = wcurs;
			stmt->rset.eccnt = eccnt;
			DBGH(stmt, "new elastic cursor: `" LWPDL "`[%zd].",
				stmt->rset.eccnt, stmt->rset.ecurs, stmt->rset.eccnt);
		} else {
			WARNH(stmt, "empty cursor found in the answer.");
		}
	} else {
		/* should have been cleared by now */
		assert(! stmt->rset.eccnt);
	}

	/*
	 * process the sent columns, if any.
	 */
	if (columns) {
		if (0 < stmt->ird->count) {
			ERRH(stmt, "%d columns already attached.", stmt->ird->count);
			RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		}
		return attach_columns(stmt, columns);
	} else {
		/* no cols available in this answer: check if already received */
		if (stmt->ird->count <= 0) {
			ERRH(stmt, "no columns available in result set; answer: "
				"`%.*s`[%zd].", blen, buff, blen);
			RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		}
	}

	return SQL_SUCCESS;
}

/*
 * Parse an error and push it as statement diagnostic.
 */
SQLRETURN TEST_API attach_error(esodbc_stmt_st *stmt, char *buff, size_t blen)
{
	UJObject obj, o_status, o_error, o_type, o_reason;
	const wchar_t *wtype, *wreason;
	size_t tlen, rlen, left;
	wchar_t wbuf[sizeof(((esodbc_diag_st *)NULL)->text) /
								sizeof(*((esodbc_diag_st *)NULL)->text)];
	size_t wbuflen = sizeof(wbuf)/sizeof(*wbuf);
	int n;
	void *state = NULL;
	const wchar_t *outer_keys[] = {
		MK_WPTR(JSON_ANSWER_ERROR),
		MK_WPTR(JSON_ANSWER_STATUS)
	};
	const wchar_t *err_keys[] = {
		MK_WPTR(JSON_ANSWER_ERR_TYPE),
		MK_WPTR(JSON_ANSWER_ERR_REASON)
	};

	INFOH(stmt, "REST request failed with `%.*s` (%zd).", blen, buff, blen);

	/* parse the entire JSON answer */
	obj = UJDecode(buff, blen, NULL, &state);
	if (! obj) {
		ERRH(stmt, "failed to decode JSON answer (`%.*s`): %s.",
			blen, buff, state ? UJGetError(state) : "<none>");
		SET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		goto end;
	}
	/* extract the status and error object */
	if (UJObjectUnpack(obj, 2, "ON", outer_keys, &o_error, &o_status) < 2) {
		ERRH(stmt, "failed to unpack JSON answer (`%.*s`): %s.",
			blen, buff, UJGetError(state));
		SET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		goto end;
	}
	/* unpack error object */
	if (UJObjectUnpack(o_error, 2, "SS", err_keys, &o_type, &o_reason) < 2) {
		ERRH(stmt, "failed to unpack error object (`%.*s`): %s.",
			blen, buff, UJGetError(state));
		SET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		goto end;
	}

	wtype = UJReadString(o_type, &tlen);
	wreason = UJReadString(o_reason, &rlen);
	/* these return empty string in case of mismatch */
	assert(wtype && wreason);
	DBGH(stmt, "server failures: type: [%zd] `" LWPDL "`, reason: [%zd] `"
		LWPDL "`, status: %d.", tlen, tlen, wtype, rlen, rlen, wreason,
		UJNumericInt(o_status));

	/* swprintf will fail if formated string would overrun the buffer size (as
	 * opposed to write up to its limit) => find out the limit first.*/
	n = swprintf(NULL, 0, MK_WPTR("%.*s: %.*s"), (int)tlen, wtype, (int)rlen,
			wreason);
	if (0 < n) {
		wbuflen -= /* ": " */2 + /*\0*/1;
		tlen = wbuflen < tlen ? wbuflen : tlen;
		left = wbuflen - tlen;
		rlen = left < rlen ? left : rlen;
		wbuflen += /* ": " */2 + /*\0*/1;
		/* swprintf will add the 0-term (or fail, if it can't) */
		n = swprintf(wbuf, wbuflen, MK_WPTR("%.*s: %.*s"), (int)tlen, wtype,
				(int)rlen, wreason);
	}
	if (n < 0) {
		ERRNH(stmt, "failed to print error message from server.");
		assert(sizeof(MSG_INV_SRV_ANS) < sizeof(wbuf));
		memcpy(wbuf, MK_WPTR(MSG_INV_SRV_ANS),
			sizeof(MSG_INV_SRV_ANS)*sizeof(SQLWCHAR));
	}

	post_diagnostic(&stmt->hdr.diag, SQL_STATE_HY000, wbuf,
		UJNumericInt(o_status));

end:
	if (state) {
		UJFree(state);
	}
	if (buff) {
		free(buff);
	}

	RET_STATE(stmt->hdr.diag.state);
}

/*
 * Attach an SQL query to the statment: malloc, convert, copy.
 */
SQLRETURN TEST_API attach_sql(esodbc_stmt_st *stmt,
	const SQLWCHAR *sql, /* SQL text statement */
	size_t sqlcnt /* count of chars of 'sql' */)
{
	char *u8;
	int len;

	DBGH(stmt, "attaching SQL `" LWPDL "` (%zd).", sqlcnt, sql, sqlcnt);

	len = WCS2U8(sql, (int)sqlcnt, NULL, 0);
	if (len <= 0) {
		ERRNH(stmt, "failed to UCS2/UTF8 convert SQL `" LWPDL "` (%zd).",
			sqlcnt, sql, sqlcnt);
		RET_HDIAG(stmt, SQL_STATE_HY000, "UCS2/UTF8 conversion failure", 0);
	}
	DBGH(stmt, "wide char SQL `" LWPDL "`[%zd] converts to UTF8 on %d "
		"octets.", sqlcnt, sql, sqlcnt, len);

	u8 = malloc(len);
	if (! u8) {
		ERRNH(stmt, "failed to alloc %dB.", len);
		RET_HDIAGS(stmt, SQL_STATE_HY001);
	}

	len = WCS2U8(sql, (int)sqlcnt, u8, len);
	if (len <= 0) { /* can it happen? it's just succeded above */
		ERRNH(stmt, "failed to UCS2/UTF8 convert SQL `" LWPDL "` (%zd).",
			sqlcnt, sql, sqlcnt);
		free(u8);
		RET_HDIAG(stmt, SQL_STATE_HY000, "UCS2/UTF8 conversion failure(2)", 0);
	}

	assert(! stmt->u8sql.str);
	stmt->u8sql.str = u8;
	stmt->u8sql.cnt = (size_t)len;

	DBGH(stmt, "attached SQL `%.*s` (%zd).", len, u8, len);

	return SQL_SUCCESS;
}

/*
 * Detach the existing query (if any) from the statement.
 */
void detach_sql(esodbc_stmt_st *stmt)
{
	if (! stmt->u8sql.str) {
		return;
	}
	free(stmt->u8sql.str);
	stmt->u8sql.str = NULL;
	stmt->u8sql.cnt = 0;
}


/*
 * "An application can unbind the data buffer for a column but still have a
 * length/indicator buffer bound for the column, if the TargetValuePtr
 * argument in the call to SQLBindCol is a null pointer but the
 * StrLen_or_IndPtr argument is a valid value."
 *
 * "When the driver returns fixed-length data, such as an integer or a date
 * structure, the driver ignores BufferLength and assumes the buffer is large
 * enough to hold the data." BUT:
 * "This is an error if the data returned by the driver is NULL but is common
 * when retrieving fixed-length, non-nullable data, because neither a length
 * nor an indicator value is needed."
 *
 * "The binding remains in effect until it is replaced by a new binding, the
 * column is unbound, or the statement is freed."
 *
 * "If ColumnNumber refers to an unbound column, SQLBindCol still returns
 * SQL_SUCCESS."
 *
 * "Call SQLBindCol to specify a new binding for a column that is already
 * bound. The driver overwrites the old binding with the new one."
 *
 * "Binding Offsets: the same offset is added to each address in each binding"
 *
 * "https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/buffers":
 * SQL_LEN_BINARY_ATTR, SQL_NTS, SQL_IS_POINTER/_INTEGER/etc.
 *
 * " The application sets the SQL_BIND_BY_COLUMN statement attribute to
 * specify whether it is using column-wise or row-wise binding"
 */
SQLRETURN EsSQLBindCol(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT ColumnNumber,
	SQLSMALLINT TargetType, /* identifier of the C data type */
	_Inout_updates_opt_(_Inexpressible_(BufferLength))
	SQLPOINTER TargetValue,
	SQLLEN BufferLength,
	_Inout_opt_ SQLLEN *StrLen_or_Ind)
{
	SQLRETURN ret;
	esodbc_stmt_st *stmt = STMH(StatementHandle);
	esodbc_desc_st *ard = stmt->ard;
	SQLSMALLINT ard_prev_count;

	if (BufferLength < 0) {
		ERRH(stmt, "invalid negative BufferLength: %d.", BufferLength);
		RET_HDIAGS(STMH(StatementHandle), SQL_STATE_HY090);
	}

	if ((STMH(StatementHandle)->bookmarks != SQL_UB_OFF) || (! ColumnNumber)) {
		/* "The statement attribute SQL_ATTR_USE_BOOKMARKS should always be
		 * set before binding a column to column 0. This is not required but
		 * is strongly recommended." */
		//RET_HDIAGS(STMH(StatementHandle), SQL_STATE_IM001);
		/* TODO: implement bookmarks? */
		FIXME;
	}

	ard_prev_count = ard->count;
	/* "if the value in the ColumnNumber argument exceeds the value of
	 * SQL_DESC_COUNT, calls SQLSetDescField to increase the value of
	 * SQL_DESC_COUNT to ColumnNumber." */
	if (ard_prev_count < ColumnNumber) {
		ret = EsSQLSetDescFieldW(ard, NO_REC_NR, SQL_DESC_COUNT,
				(SQLPOINTER)(uintptr_t)ColumnNumber, SQL_IS_SMALLINT);
		if (SQL_SUCCEEDED(ret)) {
			/* value set to negative if count needs to be restored to it */
			ard_prev_count = -ard_prev_count;
		} else {
			goto copy_ret;
		}
	}

	/* set types (or verbose for datetime/interval types) */
	ret = EsSQLSetDescFieldW(ard, ColumnNumber, SQL_DESC_CONCISE_TYPE,
			(SQLPOINTER)(intptr_t)TargetType, SQL_IS_SMALLINT);
	if (ret != SQL_SUCCESS) {
		goto copy_ret;
	}

	// TODO: § "Cautions Regarding SQL_DEFAULT"

	/* "Sets the SQL_DESC_OCTET_LENGTH field to the value of BufferLength." */
	ret = EsSQLSetDescFieldW(ard, ColumnNumber, SQL_DESC_OCTET_LENGTH,
			(SQLPOINTER)(intptr_t)BufferLength, SQL_IS_INTEGER);
	if (ret != SQL_SUCCESS) {
		goto copy_ret;
	}

	/* Sets the SQL_DESC_INDICATOR_PTR field to the value of StrLen_or_Ind" */
	ret = EsSQLSetDescFieldW(ard, ColumnNumber, SQL_DESC_INDICATOR_PTR,
			StrLen_or_Ind,
			SQL_LEN_BINARY_ATTR((SQLINTEGER)sizeof(StrLen_or_Ind)));
	if (ret != SQL_SUCCESS) {
		goto copy_ret;
	}

	/* "Sets the SQL_DESC_OCTET_LENGTH_PTR field to the value of
	 * StrLen_or_Ind." */
	ret = EsSQLSetDescFieldW(ard, ColumnNumber, SQL_DESC_OCTET_LENGTH_PTR,
			StrLen_or_Ind,
			SQL_LEN_BINARY_ATTR((SQLINTEGER)sizeof(StrLen_or_Ind)));
	if (ret != SQL_SUCCESS) {
		goto copy_ret;
	}

	/* "Sets the SQL_DESC_DATA_PTR field to the value of TargetValue."
	 * Note: needs to be last set field, as setting other fields unbinds. */
	ret = EsSQLSetDescFieldW(ard, ColumnNumber, SQL_DESC_DATA_PTR,
			TargetValue, SQL_IS_POINTER);
	if (ret != SQL_SUCCESS) {
		goto copy_ret;
	}

	/* every binding resets conversion flag */
	stmt->sql2c_conversion = CONVERSION_UNCHECKED;

	DBGH(stmt, "succesfully bound column #%hu of type %hd, "
		"buffer@0x%p of length: %lld, LenInd@0x%p", ColumnNumber, TargetType,
		TargetValue, BufferLength, StrLen_or_Ind);

	return SQL_SUCCESS;

copy_ret:
	/* copy error at top handle level, where it's going to be inquired from */
	HDIAG_COPY(ard, stmt);

	ERRH(stmt, "binding parameter failed -- resetting ARD count");
	if (ard_prev_count < 0) {
		ret = EsSQLSetDescFieldW(ard, NO_REC_NR, SQL_DESC_COUNT,
				(SQLPOINTER)(uintptr_t)-ard_prev_count, SQL_IS_SMALLINT);
		if (! SQL_SUCCEEDED(ret)) {
			ERRH(stmt, "failed to reset ARD count back to %hd - descriptor "
				"might be left in inconsistent state!", -ard_prev_count);
		}
	}
	RET_STATE(stmt->hdr.diag.state);
}

/*
 * field: SQL_DESC_: DATA_PTR / INDICATOR_PTR / OCTET_LENGTH_PTR
 * pos: position in array/row_set (not result_set)
 */
static inline void *deferred_address(SQLSMALLINT field_id, size_t pos,
	esodbc_rec_st *rec)
{
	size_t elem_size;
	SQLLEN offt;
	void *base;
	esodbc_desc_st *desc = rec->desc;

#define ROW_OFFSETS \
	do { \
		elem_size = desc->bind_type; \
		offt = desc->bind_offset_ptr ? *(desc->bind_offset_ptr) : 0; \
	} while (0)

	switch (field_id) {
		case SQL_DESC_DATA_PTR:
			base = rec->data_ptr;
			if (desc->bind_type == SQL_BIND_BY_COLUMN) {
				elem_size = (size_t)rec->octet_length;
				offt = 0;
			} else { /* by row */
				ROW_OFFSETS;
			}
			break;
		case SQL_DESC_INDICATOR_PTR:
			base = rec->indicator_ptr;
			if (desc->bind_type == SQL_BIND_BY_COLUMN) {
				elem_size = sizeof(*rec->indicator_ptr);
				offt = 0;
			} else { /* by row */
				ROW_OFFSETS;
			}
			break;
		case SQL_DESC_OCTET_LENGTH_PTR:
			base = rec->octet_length_ptr;
			if (desc->bind_type == SQL_BIND_BY_COLUMN) {
				elem_size = sizeof(*rec->octet_length_ptr);
				offt = 0;
			} else { /* by row */
				ROW_OFFSETS;
			}
			break;
		default:
			BUG("can't calculate the deferred address of field type %d.",
				field_id);
			return NULL;
	}
#undef ROW_OFFSETS

	DBGH(desc->hdr.stmt, "rec@0x%p, field_id:%hd, pos: %zu : base@0x%p, "
		"offset=%lld, elem_size=%zu", rec, field_id, pos, base, offt,
		elem_size);

	return base ? (char *)base + offt + pos * elem_size : NULL;
}

/*
 * Handles the lengths of the data to copy out to the application:
 * (1) returns the max amount of bytes to copy (in the data_ptr), taking into
 *     account size of data and of buffer, relevant statement attribute and
 *     buffer type;
 * (2) indicates if truncation occured into 'state'.
 * WARN: only to be used with ARD.meta_type == STR || BIN (as it can indicate
 * a size to copy smaller than the original -- truncating).
 */
static size_t buff_octet_size(
	size_t avail, /* how many bytes are there to copy out */
	size_t unit_size, /* the unit size of the buffer (i.e. sizeof(wchar_t)) */
	esodbc_rec_st *arec, esodbc_rec_st *irec,
	esodbc_state_et *state /* out param: only written when truncating */)
{
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;
	/* how large (bytes) is the buffer to copy into*/
	size_t room = (size_t)arec->octet_length;
	/* statement attribute SQL_ATTR_MAX_LENGTH value */
	size_t attr_max = stmt->max_length;
	/* meta type of IRD */
	esodbc_metatype_et ird_mt = irec->meta_type;
	size_t max_copy, max;

	/* type is signed, driver should not allow a negative to this point:
	 * making sure the cast above is sane. */
	assert(0 <= arec->octet_length);

	/* truncate to statment max bytes, only if "the column contains character
	 * or binary data" */
	max = (ird_mt == METATYPE_STRING || ird_mt == METATYPE_BIN) ? attr_max : 0;

	/* apply "network" truncation first, if need to */
	if (0 < max && max < avail) {
		INFO("applying 'network' truncation %zd -> %zd.", avail, max);
		max_copy = max;
		/* no truncation indicated for this case */
	} else {
		max_copy = avail;
	}

	/* is target buffer to small? adjust size if so and indicate truncation */
	/* Note: this should only be tested/applied if ARD.meta_type == STR||BIN */
	// FIXME: check note above
	if (room < max_copy) {
		INFO("applying buffer truncation %zd -> %zd.", max_copy, room);
		max_copy = room;
		*state = SQL_STATE_01004;
	}

	/* adjust to align to target buffer unit */
	if (max_copy % unit_size) {
		max_copy -= max_copy % unit_size;
	}

	DBG("avail=%zd, room=%zd, attr_max=%zd, metatype:%d => "
		"max_copy=%zd, state=%d.",
		avail, room, attr_max, ird_mt, max_copy, *state);
	return max_copy;
}

/*
 * Indicate the amount of data available to the application, taking into
 * account: the type of data, should truncation - due to max length attr
 * setting - need to be indicated, since original length is indicated, w/o
 * possible buffer truncation, but with possible 'network' truncation.
 */
static inline void write_out_octets(
	SQLLEN *octet_len_ptr, /* buffer to write the avail octets into */
	size_t avail, /* amount of bytes avail */
	esodbc_rec_st *irec)
{
	esodbc_stmt_st *stmt = irec->desc->hdr.stmt;
	/* statement attribute SQL_ATTR_MAX_LENGTH value */
	size_t attr_max = stmt->max_length;
	/* meta type of IRD */
	esodbc_metatype_et ird_mt = irec->meta_type;
	size_t max;

	if (! octet_len_ptr) {
		DBG("NULL octet len pointer, length (%zd) not indicated.", avail);
		return;
	}

	/* truncate to statment max bytes, only if "the column contains character
	 * or binary data" */
	max = (ird_mt == METATYPE_STRING || ird_mt == METATYPE_BIN) ? attr_max : 0;

	if (0 < max) {
		/* put the value of SQL_ATTR_MAX_LENGTH attribute..  even
		 * if this would be larger than what the data actually
		 * occupies after conversion: "the driver has no way of
		 * figuring out what the actual length is" */
		*octet_len_ptr = max;
		DBG("max length (%zd) attribute enforced.", max);
	} else {
		/* if no "network" truncation done, indicate data's length, no
		 * matter if truncated to buffer's size or not */
		*octet_len_ptr = avail;
	}

	DBG("length of data available for transfer: %ld", *octet_len_ptr);
}

/* if an application doesn't specify the conversion, use column's type */
static inline SQLSMALLINT get_rec_c_type(esodbc_rec_st *arec,
	esodbc_rec_st *irec)
{
	SQLSMALLINT ctype;
	/* "To use the default mapping, an application specifies the SQL_C_DEFAULT
	 * type identifier." */
	if (arec->concise_type != SQL_C_DEFAULT) {
		ctype = arec->concise_type;
	} else {
		ctype = irec->es_type->c_concise_type;
	}
	DBGH(arec->desc, "target data C type: %hd.", ctype);
	return ctype;
}

/* transfer to the application a 0-terminated (but unaccounted for) wstr_st */
static SQLRETURN transfer_wstr0(esodbc_rec_st *arec, esodbc_rec_st *irec,
	wstr_st *src, void *data_ptr, SQLLEN *octet_len_ptr)
{
	size_t in_bytes;
	esodbc_state_et state;
	SQLWCHAR *dst;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;

	/* the source string must be 0-term'd (since this needs to be transfered
	 * out to the application) */
	assert(src->str[src->cnt] == 0);

	/* always return the app the untruncated number of bytes */
	write_out_octets(octet_len_ptr, src->cnt * sizeof(*src->str), irec);

	if (data_ptr) {
		dst = (SQLWCHAR *)data_ptr;
		state = SQL_STATE_00000;
		in_bytes = buff_octet_size((src->cnt + 1) * sizeof(*src->str),
				sizeof(*src->str), arec, irec, &state);

		if (in_bytes) {
			memcpy(dst, src->str, in_bytes);
			/* TODO: should the left be filled with spaces? :
			 * https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/rules-for-conversions */

			if (state != SQL_STATE_00000) {
				/* 0-term the buffer */
				((SQLWCHAR *)data_ptr)[(in_bytes/sizeof(SQLWCHAR)) - 1] = 0;
				DBGH(stmt, "aREC@0x%p: `" LWPDL "` transfered truncated as "
					"`%s`.", arec, LWSTR(src), dst);
				RET_HDIAGS(stmt, state);
			} else {
				assert(((SQLWCHAR *)data_ptr)[(in_bytes/sizeof(SQLWCHAR))-1]
					== 0);
				DBGH(stmt, "aREC@0x%p: `" LWPDL "` transfered @ "
					"data_ptr@0x%p.", arec, LWSTR(src), dst);
			}
		}
	} else {
		DBGH(stmt, "aREC@0x%p: NULL transfer buffer.", arec);
	}

	return SQL_SUCCESS;
}

/* transfer to the application a 0-terminated (but unaccounted for) cstr_st */
static SQLRETURN transfer_cstr0(esodbc_rec_st *arec, esodbc_rec_st *irec,
	cstr_st *src, void *data_ptr, SQLLEN *octet_len_ptr)
{
	size_t in_bytes;
	esodbc_state_et state;
	SQLCHAR *dst = (SQLCHAR *)data_ptr;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;

	/* the source string must be 0-term'd (since this needs to be transfered
	 * out to the application) */
	assert(src->str[src->cnt] == 0);

	/* always return the app the untruncated number of bytes */
	write_out_octets(octet_len_ptr, src->cnt * sizeof(*src->str), irec);

	if (data_ptr) {
		dst = (SQLCHAR *)data_ptr;
		state = SQL_STATE_00000;
		in_bytes = buff_octet_size((src->cnt + 1) * sizeof(*src->str),
				sizeof(*src->str), arec, irec, &state);

		if (in_bytes) {
			memcpy(dst, src->str, in_bytes);
			/* TODO: should the left be filled with spaces? :
			 * https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/rules-for-conversions */

			if (state != SQL_STATE_00000) {
				/* 0-term the buffer */
				dst[(in_bytes/sizeof(SQLCHAR)) - 1] = 0;
				DBGH(stmt, "aREC@0x%p: `" LCPDL "` transfered truncated as "
					"`%s`.", arec, LCSTR(src), dst);
				RET_HDIAGS(stmt, state);
			} else {
				assert(dst[(in_bytes/sizeof(SQLCHAR)) - 1] == 0);
				DBGH(stmt, "aREC@0x%p: `" LCPDL "` transfered @ "
					"data_ptr@0x%p.", arec, LCSTR(src), dst);
			}
		}
	} else {
		DBGH(stmt, "aREC@0x%p: NULL transfer buffer.", arec);
	}

	return SQL_SUCCESS;
}

/* 10^n */
static inline unsigned long long pow10(unsigned n)
{
	unsigned long long pow = 1;
	pow <<= n;
	while (n--) {
		pow += pow << 2;
	}
	return pow;
}

static SQLRETURN double_to_numeric(esodbc_rec_st *arec, double src, void *dst)
{
	SQL_NUMERIC_STRUCT *numeric;
	esodbc_stmt_st *stmt;
	SQLSMALLINT prec/*..ision*/;
	unsigned long long ullng, pow;
	long long llng;

	stmt = arec->desc->hdr.stmt;
	numeric = (SQL_NUMERIC_STRUCT *)dst;
	assert(numeric);

	numeric->scale = (SQLCHAR)arec->scale;
	numeric->sign = 0 <= src;

	ullng = numeric->sign ? (unsigned long long)src : (unsigned long long)-src;
	/* =~ log10(abs(src)) */
	for (prec = 0; ullng; prec ++) {
		ullng /= 10;
	}
	if (arec->scale < 0) {
		pow = pow10(-arec->scale);
		llng = (long long)(src / pow);
		prec += arec->scale; /* prec lowers */
	} else if (0 < arec->scale) {
		pow = pow10(arec->scale);
		if (DBL_MAX / pow < src) {
			// TODO: numeric.val is 16 octets long -> expand
			ERRH(stmt, "max numeric conversion scale reached.");
			RET_HDIAGS(stmt, SQL_STATE_22003);
		}
		llng = (long long)(src * pow);
		prec += arec->scale; /* prec grows */
	} else {
		llng = (long long)src;
	}
	ullng = numeric->sign ? (unsigned long long)llng :
		(unsigned long long)-llng;

	DBGH(stmt, "arec@0x%p: precision=%hd, scale=%hd; src.precision=%hd",
		arec, arec->precision, arec->scale, prec);
	if ((UCHAR_MAX < prec) || (0 < arec->precision && arec->precision < prec)) {
		/* precision of source is higher than requested => overflow */
		ERRH(stmt, "conversion overflow. source: %.6e; requested: "
			"precisions: %d, scale: %d.", src, arec->precision, arec->scale);
		RET_HDIAGS(stmt, SQL_STATE_22003);
	} else if (prec < 0) {
		prec = 0;
		assert(ullng == 0);
	}
	numeric->precision = (SQLCHAR)prec;


#if REG_DWORD != REG_DWORD_LITTLE_ENDIAN
	ullng = _byteswap_ulong(ullng);
#endif /* LE */
	assert(sizeof(ullng) <= sizeof(numeric->val));
	memcpy(numeric->val, (char *)&ullng, sizeof(ullng));
	memset(numeric->val+sizeof(ullng), 0, sizeof(numeric->val)-sizeof(ullng));

	DBGH(stmt, "double %.6e converted to numeric: .sign=%d, precision=%d "
		"(req: %d), .scale=%d (req: %d), .val:`" LCPDL "` (0x%lx).", src,
		numeric->sign, numeric->precision, arec->precision,
		numeric->scale, arec->scale, (int)sizeof(numeric->val), numeric->val,
		ullng);
	return SQL_SUCCESS;
}

static SQLRETURN numeric_to_double(esodbc_rec_st *irec, void *src, double *dst)
{
	unsigned long long ullng, pow;
	double dbl;
	SQLSMALLINT prec/*..ision*/;
	SQL_NUMERIC_STRUCT *numeric;
	esodbc_stmt_st *stmt = irec->desc->hdr.stmt;

	assert(src);
	numeric = (SQL_NUMERIC_STRUCT *)src;

	assert(2 * sizeof(ullng) == sizeof(numeric->val));
	ullng = *(unsigned long long *)&numeric->val[sizeof(ullng)];
	if (ullng) {
		// TODO: shift down with scale
		ERRH(stmt, "max numeric precision scale reached.");
		goto erange;
	}
	ullng = *(unsigned long long *)&numeric->val[0];
#if REG_DWORD != REG_DWORD_LITTLE_ENDIAN
	ullng = _byteswap_ulong(ullng);
#endif /* LE */

	/* =~ log10(abs(ullng)) */
	for (prec = 0, pow = ullng; pow; prec ++) {
		pow /= 10;
	}

	if (DBL_MAX < ullng) {
		goto erange;
	} else {
		dbl = (double)ullng;
	}

	if (numeric->scale < 0) {
		pow = pow10(-numeric->scale);
		if (DBL_MAX / pow < dbl) {
			goto erange;
		}
		dbl *= pow;
		prec -= numeric->scale; /* prec grows */
	} else if (0 < numeric->scale) {
		pow = pow10(numeric->scale);
		dbl /= pow;
		prec -= numeric->scale; /* prec lowers */
	}

	DBGH(stmt, "irec@0x%p: precision=%hd, scale=%hd; src.precision=%hd",
		irec, irec->precision, irec->scale, prec);
	if ((UCHAR_MAX < prec) || (0 < irec->precision && irec->precision < prec)) {
		ERRH(stmt, "source precision (%hd) larger than requested (%hd)",
			prec, irec->precision);
		goto erange;
	} else {
		if (! numeric->sign) {
			dbl = -dbl;
		}
	}

	DBGH(stmt, "VAL: %f", dbl);
	DBGH(stmt, "numeric val: %llu, scale: %hhd, precision: %hhu converted to "
		"double %.6e.", ullng, numeric->scale, numeric->precision, dbl);

	*dst = dbl;
	return SQL_SUCCESS;
erange:
	ERRH(stmt, "can't convert numeric val: %llu, scale: %hhd, precision: %hhu"
		" to double.", ullng, numeric->scale, numeric->precision);
	RET_HDIAGS(stmt, SQL_STATE_22003);
}

/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/transferring-data-in-its-binary-form
 */
static SQLRETURN llong_to_binary(esodbc_rec_st *arec, esodbc_rec_st *irec,
	long long src, void *dst, SQLLEN *src_len)
{
	size_t cnt;
	char *s = (char *)&src;
	esodbc_state_et state = SQL_STATE_00000;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;
	unsigned long long ull = src < 0 ? -src : src;

	/* UJ4C uses long long for any integer type -> find out the
	 * smallest type that would accomodate the value (since fixed negatives
	 * would take more space then minimally required). */
	if (ull < CHAR_MAX) {
		cnt = sizeof(char);
	} else if (ull < SHRT_MAX) {
		cnt = sizeof(short);
	} else if (ull < INT_MAX) {
		cnt = sizeof(int);
	} else if (ull < LONG_MAX) {
		cnt = sizeof(long);
	} else { /* definetely ull < LLONG_MAX */
		cnt = sizeof(long long);
	}

	cnt = buff_octet_size(cnt, sizeof(*s), arec, irec, &state);
	if (state) { /* has it been shrunk? */
		REJECT_AS_OOR(stmt, src, /*fixed?*/TRUE, "[BINARY]<[value]");
	}

	if (dst) {
		/* copy bytes as-are: the reverse conversion need to take place on
		 * "same DBMS and hardare platform". */
		memcpy(dst, s, cnt);
		//TODO: should the driver clear all the received buffer?? Cfg option?
		//memset((char *)dst + cnt, 0, arec->octet_length - cnt);
	}
	write_out_octets(src_len, cnt, irec);
	DBGH(stmt, "long long value %lld, converted on %zd octets.", src, cnt);

	return SQL_SUCCESS;
}

static SQLRETURN longlong_to_str(esodbc_rec_st *arec, esodbc_rec_st *irec,
	long long ll, void *data_ptr, SQLLEN *octet_len_ptr, BOOL wide)
{
	/* buffer is overprovisioned for !wide, but avoids double declaration */
	SQLCHAR buff[(ESODBC_PRECISION_INT64 + /*0-term*/1 + /*+/-*/1)
		* sizeof(SQLWCHAR)];
	size_t cnt;
	SQLRETURN ret;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;

	cnt = i64tot((int64_t)ll, buff, wide);

	if (wide) {
		wstr_st llwstr = {.str = (SQLWCHAR *)buff, .cnt = cnt};
		ret = transfer_wstr0(arec, irec, &llwstr, data_ptr, octet_len_ptr);
		DBGH(stmt, "long long %lld convertible to w-string `" LWPD "` on "
			"%zd octets.", ll, (SQLWCHAR *)buff, cnt);
	} else {
		cstr_st llcstr = {.str = buff, .cnt = cnt};
		ret = transfer_cstr0(arec, irec, &llcstr, data_ptr, octet_len_ptr);
		DBGH(stmt, "long long %lld convertible to string `" LCPD "` on "
			"%zd octets.", ll, (SQLCHAR *)buff, cnt);
	}

	/* need to change the error code from truncation to "out of
	 * range", since "whole digits" are truncated */
	if (ret == SQL_SUCCESS_WITH_INFO &&
		HDRH(stmt)->diag.state == SQL_STATE_01004) {
		REJECT_AS_OOR(stmt, ll, /*fixed?*/TRUE, "[STRING]<[value]");
	}
	return SQL_SUCCESS;
}

static SQLRETURN copy_longlong(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, long long ll)
{
	esodbc_stmt_st *stmt;
	void *data_ptr;
	SQLLEN *octet_len_ptr;
	esodbc_desc_st *ard, *ird;
	SQLRETURN ret;

	stmt = arec->desc->hdr.stmt;
	ird = stmt->ird;
	ard = stmt->ard;

	/* pointer where to write how many characters we will/would use */
	octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR, pos, arec);
	/* pointer to app's buffer */
	data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, arec);

	/* Assume a C type behind an SQL C type, but check size representation.
	 * Note: won't work if _min==0 is a legit limit */
#	define REJECT_IF_OOR(_stmt, _ll, _min, _max, _sqlctype, _ctype) \
	do { \
		assert(sizeof(_sqlctype) == sizeof(_ctype)); \
		if ((_min && _ll < _min) || _max < _ll) { \
			REJECT_AS_OOR(_stmt, _ll, /*fixed int*/TRUE, _ctype); \
		} \
	} while (0)
	/* Transfer a long long to an SQL integer type.
	 * Uses local vars: stmt, data_ptr, irec, octet_len_ptr. */
#	define TRANSFER_LL(_ll, _min, _max, _sqlctype, _ctype) \
	do { \
		REJECT_IF_NULL_DEST_BUFF(stmt, data_ptr); \
		REJECT_IF_OOR(stmt, _ll, _min, _max, _sqlctype, _ctype); \
		*(_sqlctype *)data_ptr = (_sqlctype)_ll; \
		write_out_octets(octet_len_ptr, sizeof(_sqlctype), irec); \
		DBGH(stmt, "converted long long %lld to " STR(_sqlctype) " 0x%llx.", \
			_ll, (intptr_t)*(_sqlctype *)data_ptr); \
	} while (0)

	switch (get_rec_c_type(arec, irec)) {
		case SQL_C_CHAR:
			return longlong_to_str(arec, irec, ll, data_ptr, octet_len_ptr,
					FALSE);
		case SQL_C_WCHAR:
			return longlong_to_str(arec, irec, ll, data_ptr, octet_len_ptr,
					TRUE);

		case SQL_C_TINYINT:
		case SQL_C_STINYINT:
			TRANSFER_LL(ll, CHAR_MIN, CHAR_MAX, SQLSCHAR, char);
			break;
		case SQL_C_UTINYINT:
			TRANSFER_LL(ll, 0, UCHAR_MAX, SQLCHAR, unsigned char);
			break;
		case SQL_C_SHORT:
		case SQL_C_SSHORT:
			TRANSFER_LL(ll, SHRT_MIN, SHRT_MAX, SQLSMALLINT, short);
			break;
		case SQL_C_USHORT:
			TRANSFER_LL(ll, 0, USHRT_MAX, SQLUSMALLINT, unsigned short);
			break;
		case SQL_C_LONG:
		case SQL_C_SLONG:
			TRANSFER_LL(ll, LONG_MIN, LONG_MAX, SQLINTEGER, long);
			break;
		case SQL_C_ULONG:
			TRANSFER_LL(ll, 0, ULONG_MAX, SQLUINTEGER, unsigned long);
			break;
		case SQL_C_SBIGINT:
			TRANSFER_LL(ll, LLONG_MIN, LLONG_MAX, SQLBIGINT, long long);
			break;
		case SQL_C_UBIGINT:
			TRANSFER_LL(ll, 0, ULLONG_MAX, SQLUBIGINT, unsigned long long);
			break;

		case SQL_C_BIT:
			REJECT_IF_NULL_DEST_BUFF(stmt, data_ptr);
			if (ll < 0 || 2 <= ll) {
				REJECT_AS_OOR(stmt, ll, /*fixed int*/TRUE, SQL_C_BIT);
			} else { /* 0 or 1 */
				*(SQLCHAR *)data_ptr = (SQLCHAR)ll;
			}
			write_out_octets(octet_len_ptr, sizeof(SQLSCHAR), irec);
			break;

		case SQL_C_NUMERIC:
			REJECT_IF_NULL_DEST_BUFF(stmt, data_ptr);
			ret = double_to_numeric(arec, (double)ll, data_ptr);
			if (! SQL_SUCCEEDED(ret)) {
				return ret;
			}
			write_out_octets(octet_len_ptr, sizeof(SQL_NUMERIC_STRUCT), irec);
			break;

		case SQL_C_FLOAT:
			REJECT_IF_NULL_DEST_BUFF(stmt, data_ptr);
			REJECT_IF_OOR(stmt, ll, -FLT_MAX, FLT_MAX, SQLREAL, float);
			*(SQLREAL *)data_ptr = (SQLREAL)ll;
			write_out_octets(octet_len_ptr, sizeof(SQLREAL), irec);
			break;

		case SQL_C_DOUBLE:
			REJECT_IF_NULL_DEST_BUFF(stmt, data_ptr);
			REJECT_IF_OOR(stmt, ll, -DBL_MAX, DBL_MAX, SQLDOUBLE, double);
			*(SQLDOUBLE *)data_ptr = (SQLDOUBLE)ll;
			write_out_octets(octet_len_ptr, sizeof(SQLDOUBLE), irec);
			break;

		case SQL_C_BINARY:
			return llong_to_binary(arec, irec, ll, data_ptr, octet_len_ptr);

		default:
			BUGH(stmt, "unexpected unhanlded data type: %d.",
				get_rec_c_type(arec, irec));
			return SQL_ERROR;
	}
	DBGH(stmt, "REC@0x%p, data_ptr@0x%p, copied long long: %lld.", arec,
		data_ptr, ll);

	return SQL_SUCCESS;

#	undef REJECT_IF_OOR
#	undef TRANSFER_LL
}

static SQLRETURN double_to_bit(esodbc_rec_st *arec, esodbc_rec_st *irec,
	double src, void *data_ptr, SQLLEN *octet_len_ptr)
{
	esodbc_state_et state = SQL_STATE_00000;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;

	REJECT_IF_NULL_DEST_BUFF(stmt, data_ptr);

	write_out_octets(octet_len_ptr, sizeof(SQLCHAR), irec);

	if (src < 0. || 2. <= src) {
		REJECT_AS_OOR(stmt, src, /*fixed?*/FALSE, SQL_C_BIT);
	} else if (0. < src && src < 1.) {
		*(SQLCHAR *)data_ptr = 0;
		state = SQL_STATE_01S07;
	} else if (1. < src && src < 2.) {
		*(SQLCHAR *)data_ptr = 1;
		state = SQL_STATE_01S07;
	} else { /* 0 or 1 */
		*(SQLCHAR *)data_ptr = (SQLCHAR)src;
	}
	if (state != SQL_STATE_00000) {
		INFOH(stmt, "truncating when converting %f as %d.", src,
			*(SQLCHAR *)data_ptr);
		RET_HDIAGS(stmt, state);
	}

	DBGH(stmt, "double %f converted to bit %d.", src, *(SQLCHAR *)data_ptr);

	return SQL_SUCCESS;
}

static SQLRETURN double_to_binary(esodbc_rec_st *arec, esodbc_rec_st *irec,
	double dbl, void *data_ptr, SQLLEN *octet_len_ptr)
{
	size_t cnt;
	double udbl = dbl < 0. ? -dbl : dbl;
	float flt;
	char *ptr;
	esodbc_state_et state = SQL_STATE_00000;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;

	if (udbl < FLT_MIN || FLT_MAX < udbl) {
		/* value's precision/scale requires a double */
		cnt = sizeof(dbl);
		ptr = (char *)&dbl;
	} else {
		flt = (float)dbl;
		cnt = sizeof(flt);
		ptr = (char *)&flt;
	}

	cnt = buff_octet_size(cnt, sizeof(*ptr), arec, irec, &state);
	if (state) {
		REJECT_AS_OOR(stmt, dbl, /*fixed?*/FALSE, "[BINARY]<[floating]");
	}
	write_out_octets(octet_len_ptr, cnt, irec);
	if (data_ptr) {
		memcpy(data_ptr, ptr, cnt);
		//TODO: should the driver clear all the received buffer?? Cfg option?
		//memset((char *)data_ptr + cnt, 0, arec->octet_length - cnt);
	}

	DBGH(stmt, "converted double %f to binary on %zd octets.", dbl, cnt);

	return SQL_SUCCESS;
}

/*
 * TODO!!!
 * 1. use default precision
 * 2. config for scientific notation.
 * 3. sprintf (for now)
 */
static SQLRETURN double_to_str(esodbc_rec_st *arec, esodbc_rec_st *irec,
	double dbl, void *data_ptr, SQLLEN *octet_len_ptr, BOOL wide)
{
	long long whole;
	unsigned long long fraction;
	double rest;
	SQLSMALLINT scale;
	size_t pos, octets;
	/* buffer is overprovisioned for !wide, but avoids double declaration */
	SQLCHAR buff[(2 * ESODBC_PRECISION_INT64 + /*.*/1 + /*\0*/1)
		* sizeof(SQLWCHAR)];
	/* buffer unit size */
	size_t usize = wide ? sizeof(SQLWCHAR) : sizeof(SQLCHAR);
	esodbc_state_et state = SQL_STATE_00000;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;

	/*
	 * split the whole and fractional parts
	 */
	assert(sizeof(dbl) == sizeof(whole)); /* [double]==[long long] */
	whole = (long long)dbl;
	rest = dbl - whole;

	/* retain user defined or data source default number of fraction digits */
	scale = 0 < arec->scale ? arec->scale : irec->es_type->maximum_scale;
	rest *= pow10(scale);
	rest = round(rest);
	fraction = rest < 0 ? (unsigned long long) -rest
		: (unsigned long long)rest;

	/* copy integer part into work buffer */
	pos = i64tot((int64_t)whole, buff, wide);
	/* would writing just the whole part + \0 fit into the buffer? */
	octets = buff_octet_size((pos + 1) * usize, usize, arec, irec, &state);
	if (state) {
		REJECT_AS_OOR(stmt, dbl, /*fixed?*/FALSE, "[STRING]<[floating.whole]");
	} else {
		assert(octets == (pos + 1) * usize);
	}

	if (wide) {
		((SQLWCHAR *)buff)[pos ++] = L'.';
	} else {
		((SQLCHAR *)buff)[pos ++] = '.';
	}

	/* copy fractional part into work buffer */
	pos += ui64tot((uint64_t)fraction, (char *)buff + pos * usize, wide);

	/* write how many bytes (w/o \0) we'd write if buffer is large enough */
	write_out_octets(octet_len_ptr, pos * usize, irec);
	/* compute how many bytes we can actually transfer, including \0 */
	octets = buff_octet_size((pos + 1) * usize, usize, arec, irec, &state);

	if (data_ptr) {
		/* transfer the bytes out */
		memcpy(data_ptr, buff, octets);
		if (state) {
			/* usize < octets, since user input is checked above for OOR  */
			if (wide) {
				((SQLWCHAR *)data_ptr)[octets/usize - 1] = L'\0';
			} else {
				((SQLCHAR *)data_ptr)[octets/usize - 1] = '\0';
			}
		}
	}

	if (wide) {
		DBGH(stmt, "double %.6e converted to w-string `" LWPD "` on %zd "
			"octets (state: %d; scale: %d).", dbl, buff, octets, state, scale);
	} else {
		DBGH(stmt, "double %.6e converted to string `" LCPD "` on %zd "
			"octets (state: %d; scale: %d).", dbl, buff, octets, state, scale);
	}

	if (state) {
		RET_HDIAGS(stmt, state);
	} else {
		return SQL_SUCCESS;
	}
}

static SQLRETURN copy_double(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, double dbl)
{
	esodbc_stmt_st *stmt;
	void *data_ptr;
	SQLLEN *octet_len_ptr;
	esodbc_desc_st *ard, *ird;
	SQLRETURN ret;
	double udbl;

	stmt = arec->desc->hdr.stmt;
	ird = stmt->ird;
	ard = stmt->ard;

	/* pointer where to write how many characters we will/would use */
	octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR, pos, arec);
	/* pointer to app's buffer */
	data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, arec);

	/* Transfer a double to an SQL integer type.
	 * Uses local vars: stmt, data_ptr, irec, octet_len_ptr.
	 * Returns - RET_ - 01S07 on success (due to truncation of fractionals). */
#	define RET_TRANSFER_DBL(_dbl, _min, _max, _sqlctype, _ctype) \
	do { \
		/* using C type limits, so check C and SQL C type precision */ \
		assert(sizeof(_sqlctype) == sizeof(_ctype)); \
		if (_dbl) { \
			if ((_sqlctype)_dbl < _min || _max < (_sqlctype)_dbl) { \
				REJECT_AS_OOR(stmt, _dbl, /*fixed?*/FALSE, _sqlctype); \
			} \
		} else { \
			double __udbl = dbl < 0 ? -dbl : dbl; \
			if (_max < (_sqlctype)__udbl) { \
				REJECT_AS_OOR(stmt, _dbl, /*fixed?*/FALSE, _sqlctype); \
			} \
		} \
		*(_sqlctype *)data_ptr = (_sqlctype)_dbl; \
		write_out_octets(octet_len_ptr, sizeof(_sqlctype), irec); \
		DBGH(stmt, "converted double %f to " STR(_sqlctype) " 0x%llx.", _dbl, \
			(intptr_t)*(_sqlctype *)data_ptr); \
		RET_HDIAGS(stmt, SQL_STATE_01S07); \
	} while (0)

	switch (get_rec_c_type(arec, irec)) {
		case SQL_C_CHAR:
			return double_to_str(arec, irec, dbl, data_ptr, octet_len_ptr,
					FALSE);
		case SQL_C_WCHAR:
			return double_to_str(arec, irec, dbl, data_ptr, octet_len_ptr,
					TRUE);

		case SQL_C_TINYINT:
		case SQL_C_STINYINT:
			RET_TRANSFER_DBL(dbl, CHAR_MIN, CHAR_MAX, SQLSCHAR, char);
		case SQL_C_UTINYINT:
			RET_TRANSFER_DBL(dbl, 0, UCHAR_MAX, SQLCHAR, unsigned char);
		case SQL_C_SBIGINT:
			RET_TRANSFER_DBL(dbl, LLONG_MIN, LLONG_MAX, SQLBIGINT, long long);
		case SQL_C_UBIGINT:
			RET_TRANSFER_DBL(dbl, 0, LLONG_MAX, SQLUBIGINT, long long);
		case SQL_C_SHORT:
		case SQL_C_SSHORT:
			RET_TRANSFER_DBL(dbl, SHRT_MIN, SHRT_MAX, SQLSMALLINT, short);
		case SQL_C_USHORT:
			RET_TRANSFER_DBL(dbl, 0, USHRT_MAX, SQLUSMALLINT, unsigned short);
		case SQL_C_LONG:
		case SQL_C_SLONG:
			RET_TRANSFER_DBL(dbl, LONG_MIN, LONG_MAX, SQLINTEGER, long);
		case SQL_C_ULONG:
			RET_TRANSFER_DBL(dbl, 0, ULONG_MAX, SQLINTEGER, unsigned long);

		case SQL_C_NUMERIC:
			REJECT_IF_NULL_DEST_BUFF(stmt, data_ptr);
			ret = double_to_numeric(arec, dbl, data_ptr);
			if (! SQL_SUCCEEDED(ret)) {
				return ret;
			}
			write_out_octets(octet_len_ptr, sizeof(SQL_NUMERIC_STRUCT), irec);
			break;

		case SQL_C_FLOAT:
			REJECT_IF_NULL_DEST_BUFF(stmt, data_ptr);
			udbl = dbl < 0 ? -dbl : dbl;
			if (udbl < FLT_MIN || FLT_MAX < udbl) {
				REJECT_AS_OOR(stmt, dbl, /* is fixed */FALSE, SQLREAL);
			}
			*(SQLREAL *)data_ptr = (SQLREAL)dbl;
			write_out_octets(octet_len_ptr, sizeof(SQLREAL), irec);
			break;
		case SQL_C_DOUBLE:
			REJECT_IF_NULL_DEST_BUFF(stmt, data_ptr);
			*(SQLDOUBLE *)data_ptr = dbl;
			write_out_octets(octet_len_ptr, sizeof(SQLDOUBLE), irec);
			break;

		case SQL_C_BIT:
			return double_to_bit(arec, irec, dbl, data_ptr, octet_len_ptr);

		case SQL_C_BINARY:
			return double_to_binary(arec, irec, dbl, data_ptr, octet_len_ptr);

		default:
			BUGH(stmt, "unexpected unhanlded data type: %d.",
				get_rec_c_type(arec, irec));
			return SQL_ERROR;
	}

	DBGH(stmt, "REC@0x%p, data_ptr@0x%p, copied double: %.6e.", arec,
		data_ptr, dbl);

	return SQL_SUCCESS;

#	undef RET_TRANSFER_DBL
}

/* https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/sql-to-c-bit */
static SQLRETURN copy_boolean(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, BOOL boolval)
{
	esodbc_stmt_st *stmt;
	void *data_ptr;
	SQLLEN *octet_len_ptr;
	wstr_st wbool;
	cstr_st cbool;

	stmt = arec->desc->hdr.stmt;

	/* pointer where to write how many bytes we will/would use */
	octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR, pos, arec);
	/* pointer to app's buffer */
	data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, arec);

	switch (get_rec_c_type(arec, irec)) {
		case SQL_C_WCHAR:
			if (arec->octet_length < 1) { /* can't inquiry needed buffer len */
				REJECT_AS_OOR(stmt, boolval, /*fixed int*/TRUE, NULL WCHAR);
			}
			/* "When bit SQL data is converted to character C data, the
			 * possible values are "0" and "1"." */
			wbool = boolval ? MK_WSTR("1") : MK_WSTR("0");
			return transfer_wstr0(arec, irec, &wbool, data_ptr, octet_len_ptr);
		case SQL_C_CHAR:
			if (arec->octet_length < 1) { /* can't inquiry needed buffer len */
				REJECT_AS_OOR(stmt, boolval, /*fixed int*/TRUE, NULL CHAR);
			}
			cbool = boolval ? MK_CSTR("1") : MK_CSTR("0");
			return transfer_cstr0(arec, irec, &cbool, data_ptr, octet_len_ptr);
		default:
			return copy_longlong(arec, irec, pos, boolval ? 1LL : 0LL);
	}

	DBGH(stmt, "REC@0x%p, data_ptr@0x%p, copied boolean: `%d`.", arec,
		data_ptr, boolval);
	return SQL_SUCCESS;
}

/*
 * -> SQL_C_CHAR
 * Note: chars_0 param accounts for 0-term, but length indicated back to the
 * application must not.
 */
static SQLRETURN wstr_to_cstr(esodbc_rec_st *arec, esodbc_rec_st *irec,
	void *data_ptr, SQLLEN *octet_len_ptr,
	const wchar_t *wstr, size_t chars_0)
{
	esodbc_state_et state = SQL_STATE_00000;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;
	char *charp;
	int in_bytes, out_bytes, c;

	if (data_ptr) {
		charp = (char *)data_ptr;

		in_bytes = (int)buff_octet_size(chars_0 * sizeof(*wstr),
				sizeof(*charp), arec, irec, &state);
		/* trim the original string until it fits in output buffer, with given
		 * length limitation */
		for (c = (int)chars_0; 0 < c; c --) {
			out_bytes = WCS2U8(wstr, c, charp, in_bytes);
			if (out_bytes <= 0) {
				if (WCS2U8_BUFF_INSUFFICIENT) {
					continue;
				}
				ERRNH(stmt, "failed to convert wchar* to char* for string `"
					LWPDL "`.", chars_0, wstr);
				RET_HDIAGS(stmt, SQL_STATE_22018);
			} else {
				/* conversion succeeded */
				break;
			}
		}

		/* if 0's present => 0 < out_bytes */
		assert(wstr[chars_0 - 1] == L'\0');
		assert(0 < out_bytes);
		/* is user gives 0 as buffer size, out_bytes will also be 0 */
		if (charp[out_bytes - 1]) {
			/* ran out of buffer => not 0-terminated and truncated already */
			charp[out_bytes - 1] = 0;
			state = SQL_STATE_01004; /* indicate truncation */
		}

		DBGH(stmt, "REC@0x%p, data_ptr@0x%p, copied %zd bytes: `" LWPD "`.",
			arec, data_ptr, out_bytes, charp);
	} else {
		DBGH(stmt, "REC@0x%p, NULL data_ptr.", arec);
	}

	/* if length needs to be given, calculate it (not truncated) & converted */
	if (octet_len_ptr) {
		out_bytes = (size_t)WCS2U8(wstr, (int)chars_0, NULL, 0);
		if (out_bytes <= 0) {
			ERRNH(stmt, "failed to convert wchar* to char* for string `"
				LWPDL "`.", chars_0, wstr);
			RET_HDIAGS(stmt, SQL_STATE_22018);
		} else {
			/* chars_0 accounts for 0-terminator, so WCS2U8 will count that in
			 * the output as well => trim it, since we must not count it when
			 * indicating the length to the application */
			out_bytes --;
		}
		write_out_octets(octet_len_ptr, out_bytes, irec);
	} else {
		DBGH(stmt, "REC@0x%p, NULL octet_len_ptr.", arec);
	}

	if (state != SQL_STATE_00000) {
		RET_HDIAGS(stmt, state);
	}
	return SQL_SUCCESS;
}

/*
 * -> SQL_C_WCHAR
 * Note: chars_0 accounts for 0-term, but length indicated back to the
 * application must not.
 */
static SQLRETURN wstr_to_wstr(esodbc_rec_st *arec, esodbc_rec_st *irec,
	void *data_ptr, SQLLEN *octet_len_ptr,
	const wchar_t *wstr, size_t chars_0)
{
	wstr_st wsrc = {(SQLWCHAR *)wstr, chars_0 - 1};
	return transfer_wstr0(arec, irec, &wsrc, data_ptr, octet_len_ptr);
}

/* Converts an xstr to a TS.
 * xstr needs to be trimmed to exact data (no padding, no 0-term counted).
 * If ts_buff is non-NULL, the xstr will be copied (possibly W-to-C converted)
 * into it. */
static BOOL xstr_to_timestamp_struct(xstr_st *xstr, TIMESTAMP_STRUCT *tss,
	cstr_st *ts_buff)
{
	/* need the 0-term in the buff, since ansi_w2c will write it */
	char buff[sizeof(ESODBC_ISO8601_TEMPLATE)/*+\0*/];
	cstr_st ts_str, *ts_ptr;
	timestamp_t tsp;
	struct tm tmp;

	if (ts_buff) {
		assert(sizeof(ESODBC_ISO8601_TEMPLATE) - 1 <= ts_buff->cnt);
		ts_ptr = ts_buff;
	} else {
		ts_str.str = buff;
		ts_str.cnt = sizeof(buff);
		ts_ptr = &ts_str;
	}

	if (xstr->wide) {
		DBG("converting ISO 8601 `" LWPDL "` to timestamp.", LWSTR(&xstr->w));
		if (sizeof(ESODBC_ISO8601_TEMPLATE) - 1 < xstr->w.cnt) {
			ERR("`" LWPDL "` not a TIMESTAMP.", LWSTR(&xstr->w));
			return FALSE;
		}
		/* convert the W-string to C-string; also, copy it directly into out
		 * ts_buff, if given (thus saving one extra copying) */
		ts_ptr->cnt = ansi_w2c(xstr->w.str, ts_ptr->str, xstr->w.cnt) - 1;
	} else {
		DBG("converting ISO 8601 `" LCPDL "` to timestamp.", LCSTR(&xstr->c));
		if (sizeof(ESODBC_ISO8601_TEMPLATE) - 1 < xstr->c.cnt) {
			ERR("`" LCPDL "` not a TIMESTAMP.", LCSTR(&xstr->c));
			return FALSE;
		}
		/* no conversion needed; but copying to the out ts_buff, if given */
		if (ts_buff) {
			memcpy(ts_ptr->str, xstr->c.str, xstr->c.cnt);
			ts_ptr->cnt = xstr->c.cnt;
		} else {
			ts_ptr = &xstr->c;
		}
	}

	/* len counts the 0-term */
	if (ts_ptr->cnt <= 1 || timestamp_parse(ts_ptr->str, ts_ptr->cnt, &tsp) ||
		(! timestamp_to_tm_local(&tsp, &tmp))) {
		ERR("data `" LCPDL "` not an ANSI ISO 8601 format.", LCSTR(ts_ptr));
		return FALSE;
	}
	TM_TO_TIMESTAMP_STRUCT(&tmp, tss);
	tss->fraction = tsp.nsec / 1000000;

	DBG("parsed ISO 8601: `%04d-%02d-%02dT%02d:%02d:%02d.%u+%dm`.",
		tss->year, tss->month, tss->day,
		tss->hour, tss->minute, tss->second, tss->fraction,
		tsp.offset);

	return TRUE;
}


static BOOL parse_timedate(xstr_st *xstr, TIMESTAMP_STRUCT *tss,
	SQLSMALLINT *format, cstr_st *ts_buff)
{
	/* template buffer: date or time values will be copied in place and
	 * evaluated as a timestamp (needs to be valid) */
	SQLCHAR templ[] = "0001-01-01T00:00:00.0000000Z";
	/* conversion Wide to C-string buffer */
	SQLCHAR w2c[sizeof(ESODBC_ISO8601_TEMPLATE)/*+\0*/];
	cstr_st td;/* timedate string */
	xstr_st xtd;

	/* is this a TIMESTAMP? */
	if (sizeof(ESODBC_TIME_TEMPLATE) - 1 < XSTR_LEN(xstr)) {
		/* longer than a date-value -> try a timestamp */
		if (! xstr_to_timestamp_struct(xstr, tss, ts_buff)) {
			return FALSE;
		}
		if (format) {
			*format = SQL_TYPE_TIMESTAMP;
		}
		return TRUE;
	}

	/* W-strings will eventually require convertion to C-string for TS
	 * conversion => do it now to simplify string analysis */
	if (xstr->wide) {
		td.cnt = ansi_w2c(xstr->w.str, w2c, xstr->w.cnt) - 1;
		td.str = w2c;
	} else {
		td = xstr->c;
	}
	xtd.wide = FALSE;

	/* could this be a TIME-val? */
	if (/*hh:mm:ss*/8 <= td.cnt && td.str[2] == ':' && td.str[5] == ':') {
		/* copy active value in template and parse it as TS */
		/* copy is safe: cnt <= [time template] < [templ] */
		memcpy(templ + sizeof(ESODBC_DATE_TEMPLATE) - 1, td.str, td.cnt);
		/* there could be a varying number of fractional digits */
		templ[sizeof(ESODBC_DATE_TEMPLATE) - 1 + td.cnt] = 'Z';
		xtd.c.str = templ;
		xtd.c.cnt = td.cnt + sizeof(ESODBC_DATE_TEMPLATE);
		if (! xstr_to_timestamp_struct(&xtd, tss, ts_buff)) {
			ERR("`" LCPDL "` not a TIME.", LCSTR(&td));
			return FALSE;
		} else {
			tss->year = tss->month = tss->day = 0;
			if (format) {
				*format = SQL_TYPE_TIME;
			}
		}
		return TRUE;
	}

	/* could this be a DATE-val? */
	if (/*yyyy-mm-dd*/10 <= td.cnt && td.str[4] == '-' && td.str[7] == '-') {
		/* copy active value in template and parse it as TS */
		/* copy is safe: cnt <= [time template] < [templ] */
		memcpy(templ, td.str, td.cnt);
		xtd.c.str = templ;
		xtd.c.cnt = sizeof(templ)/sizeof(templ[0]) - 1;
		if (! xstr_to_timestamp_struct(&xtd, tss, ts_buff)) {
			ERR("`" LCPDL "` not a DATE.", LCSTR(&td));
			return FALSE;
		} else {
			tss->hour = tss->minute = tss->second = 0;
			tss->fraction = 0;
			if (format) {
				*format = SQL_TYPE_DATE;
			}
		}
		return TRUE;
	}

	ERR("`" LCPDL "` not a Time/Date/Timestamp.", LCSTR(&td));
	return FALSE;
}

/*
 * -> SQL_C_TYPE_TIMESTAMP
 *
 * Conversts an ES/SQL 'date' or a text representation of a
 * timestamp/date/time value into a TIMESTAMP_STRUCT (indicates the detected
 * input format into the "format" parameter).
 */
static SQLRETURN wstr_to_timestamp(esodbc_rec_st *arec, esodbc_rec_st *irec,
	void *data_ptr, SQLLEN *octet_len_ptr,
	const wchar_t *w_str, size_t chars_0, SQLSMALLINT *format)
{
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;
	TIMESTAMP_STRUCT *tss = (TIMESTAMP_STRUCT *)data_ptr;
	xstr_st xstr = (xstr_st) {
		.wide = TRUE,
		.w = (wstr_st) {
			(SQLWCHAR *)w_str, chars_0 - 1
		}
	};

	if (octet_len_ptr) {
		*octet_len_ptr = sizeof(*tss);
	}

	if (data_ptr) {
		/* right & left trim the data before attempting conversion */
		wtrim_ws(&xstr.w);

		switch (irec->concise_type) {
			case SQL_TYPE_TIMESTAMP:
				if (! xstr_to_timestamp_struct(&xstr, tss, NULL)) {
					RET_HDIAGS(stmt, SQL_STATE_22018);
				}
				if (format) {
					*format = SQL_TYPE_TIMESTAMP;
				}
				break;
			case SQL_VARCHAR:
				if (! parse_timedate(&xstr, tss, format, NULL)) {
					RET_HDIAGS(stmt, SQL_STATE_22018);
				}
				break;

			case SQL_CHAR:
			case SQL_LONGVARCHAR:
			case SQL_WCHAR:
			case SQL_WLONGVARCHAR:
			case SQL_TYPE_DATE:
			case SQL_TYPE_TIME:
				BUGH(stmt, "unexpected (but permitted) SQL type.");
				RET_HDIAGS(stmt, SQL_STATE_HY004);
			default:
				BUGH(stmt, "uncought invalid conversion.");
				RET_HDIAGS(stmt, SQL_STATE_07006);
		}
	} else {
		DBGH(stmt, "REC@0x%p, NULL data_ptr", arec);
	}

	return SQL_SUCCESS;
}

/*
 * -> SQL_C_TYPE_DATE
 */
static SQLRETURN wstr_to_date(esodbc_rec_st *arec, esodbc_rec_st *irec,
	void *data_ptr, SQLLEN *octet_len_ptr,
	const wchar_t *wstr, size_t chars_0)
{
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;
	DATE_STRUCT *ds = (DATE_STRUCT *)data_ptr;
	TIMESTAMP_STRUCT tss;
	SQLRETURN ret;
	SQLSMALLINT fmt;

	if (octet_len_ptr) {
		*octet_len_ptr = sizeof(*ds);
	}

	if (data_ptr) {
		ret = wstr_to_timestamp(arec, irec, &tss, NULL, wstr, chars_0, &fmt);
		if (! SQL_SUCCEEDED(ret)) {
			return ret;
		}
		if (fmt == SQL_TYPE_TIME) {
			/* it's a time-value */
			RET_HDIAGS(stmt, SQL_STATE_22018);
		}
		ds->year = tss.year;
		ds->month = tss.month;
		ds->day = tss.day;
		if (tss.hour || tss.minute || tss.second || tss.fraction) {
			/* value's truncated */
			RET_HDIAGS(stmt, SQL_STATE_01S07);
		}
	} else {
		DBGH(stmt, "REC@0x%p, NULL data_ptr", arec);
	}

	return SQL_SUCCESS;
}

/*
 * -> SQL_C_TYPE_TIME
 */
static SQLRETURN wstr_to_time(esodbc_rec_st *arec, esodbc_rec_st *irec,
	void *data_ptr, SQLLEN *octet_len_ptr,
	const wchar_t *wstr, size_t chars_0)
{
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;
	TIME_STRUCT *ts = (TIME_STRUCT *)data_ptr;
	TIMESTAMP_STRUCT tss;
	SQLRETURN ret;
	SQLSMALLINT fmt;

	if (octet_len_ptr) {
		*octet_len_ptr = sizeof(*ts);
	}

	if (data_ptr) {
		ret = wstr_to_timestamp(arec, irec, &tss, NULL, wstr, chars_0, &fmt);
		if (! SQL_SUCCEEDED(ret)) {
			return ret;
		}
		/* need to differentiate between:
		 * - 1234-12-34T00:00:00Z : valid and
		 * - 1234-12-34 : invalid */
		if (fmt == SQL_TYPE_DATE) {
			RET_HDIAGS(stmt, SQL_STATE_22018);
		}
		ts->hour = tss.hour;
		ts->minute = tss.minute;
		ts->second = tss.second;
		if (tss.fraction) {
			/* value's truncated */
			RET_HDIAGS(stmt, SQL_STATE_01S07);
		}
	} else {
		DBGH(stmt, "REC@0x%p, NULL data_ptr", arec);
	}

	return SQL_SUCCESS;
}

/*
 * wstr: is 0-terminated and terminator is counted in 'chars_0'.
 * However: "[w]hen C strings are used to hold character data, the
 * null-termination character is not considered to be part of the data and is
 * not counted as part of its byte length."
 * "If the data was converted to a variable-length data type, such as
 * character or binary [...][i]t then null-terminates the data."
 */
static SQLRETURN copy_string(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, const wchar_t *wstr, size_t chars_0)
{
	esodbc_stmt_st *stmt;
	void *data_ptr;
	SQLLEN *octet_len_ptr;
	esodbc_desc_st *ard, *ird;
	SQLSMALLINT ctarget;
	long long ll;
	unsigned long long ull;
	wstr_st wval;
	double dbl;
	SQLWCHAR *endp;

	stmt = arec->desc->hdr.stmt;
	ird = stmt->ird;
	ard = stmt->ard;

	/* pointer where to write how many characters we will/would use */
	octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR, pos, arec);
	/* pointer to app's buffer */
	data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, arec);

	switch ((ctarget = get_rec_c_type(arec, irec))) {
		case SQL_C_CHAR:
			return wstr_to_cstr(arec, irec, data_ptr, octet_len_ptr,
					wstr, chars_0);
		case SQL_C_BINARY: /* treat binary as WCHAR */
		case SQL_C_WCHAR:
			return wstr_to_wstr(arec, irec, data_ptr, octet_len_ptr,
					wstr, chars_0);

		case SQL_C_TYPE_TIMESTAMP:
			return wstr_to_timestamp(arec, irec, data_ptr, octet_len_ptr,
					wstr, chars_0, NULL);
		case SQL_C_TYPE_DATE:
			return wstr_to_date(arec, irec, data_ptr, octet_len_ptr,
					wstr, chars_0);
		case SQL_C_TYPE_TIME:
			return wstr_to_time(arec, irec, data_ptr, octet_len_ptr,
					wstr, chars_0);

		case SQL_C_TINYINT:
		case SQL_C_STINYINT:
		case SQL_C_SHORT:
		case SQL_C_SSHORT:
		case SQL_C_LONG:
		case SQL_C_SLONG:
		case SQL_C_SBIGINT:
			wval = (wstr_st) {
				(SQLWCHAR *)wstr, chars_0 - 1
			};
			/* trim any white spaces */
			wtrim_ws(&wval);
			/* convert to integer type */
			errno = 0;
			if (! str2bigint(&wval, /*wide?*/TRUE, (SQLBIGINT *)&ll)) {
				ERRH(stmt, "can't convert `" LWPD "` to long long.", wstr);
				RET_HDIAGS(stmt, errno == ERANGE ? SQL_STATE_22003 :
					SQL_STATE_22018);
			}
			DBGH(stmt, "string `" LWPD "` converted to LL=%lld.", wstr, ll);
			/* delegate to existing functionality */
			return copy_longlong(arec, irec, pos, ll);

		case SQL_C_UTINYINT:
		case SQL_C_USHORT:
		case SQL_C_ULONG:
		case SQL_C_UBIGINT:
			wval = (wstr_st) {
				(SQLWCHAR *)wstr, chars_0 - 1
			};
			/* trim any white spaces */
			wtrim_ws(&wval);
			/* convert to integer type */
			errno = 0;
			if (! str2ubigint(&wval, /*wide?*/TRUE, (SQLUBIGINT *)&ull)) {
				ERRH(stmt, "can't convert `" LWPD "` to unsigned long long.",
					wstr);
				RET_HDIAGS(stmt, errno == ERANGE ? SQL_STATE_22003 :
					SQL_STATE_22018);
			}
			DBGH(stmt, "string `" LWPD "` converted to ULL=%llu.", wstr, ull);
			if (ull <= LLONG_MAX) {
				/* the cast is safe, delegate to existing functionality */
				return copy_longlong(arec, irec, pos, (long long)ull);
			}
			/* value is larger than what long long can hold: can only convert
			 * to SQLUBIGINT (and SQLULONG, if it has the same size), or fail
			 * as out-of-range */
			assert(sizeof(SQLUBIGINT) == sizeof(unsigned long long));
			if ((ctarget == SQL_C_UBIGINT) || (ctarget == SQL_C_ULONG &&
					sizeof(SQLUINTEGER) == sizeof(SQLUBIGINT))) {
				/* write out the converted value */
				REJECT_IF_NULL_DEST_BUFF(stmt, data_ptr);
				*(SQLUBIGINT *)data_ptr = (SQLUBIGINT)ull;
				write_out_octets(octet_len_ptr, sizeof(SQLUBIGINT), irec);
				DBGH(stmt, "converted string `" LWPD "` to "
					"unsigned long long %llu.", wstr, ull);
			} else {
				REJECT_AS_OOR(stmt, ull, /*fixed?*/TRUE, "non-ULL");
			}
			break;

		case SQL_C_FLOAT:
		case SQL_C_DOUBLE:
		case SQL_C_NUMERIC:
		case SQL_C_BIT:
			wval = (wstr_st) {
				(SQLWCHAR *)wstr, chars_0 - 1
			};
			/* trim any white spaces */
			wtrim_ws(&wval);
			/* convert to double */
			errno = 0;
			dbl = wcstod((wchar_t *)wval.str, (wchar_t **)&endp);
			DBGH(stmt, "string `" LWPD "` converted to dbl=%.6e.", wstr, dbl);
			/* if empty string, non-numeric or under/over-flow, bail out */
			if ((! wval.cnt) || (wval.str + wval.cnt != endp) || errno) {
				ERRH(stmt, "can't convert `" LWPD "` to double.", wstr);
				RET_HDIAGS(stmt, errno == ERANGE ? SQL_STATE_22003 :
					SQL_STATE_22018);
			}
			/* delegate to existing functionality */
			return copy_double(arec, irec, pos, dbl);


		default:
			BUGH(stmt, "unexpected unhandled data type: %d.",
				get_rec_c_type(arec, irec));
			return SQL_ERROR;
	}

	return SQL_SUCCESS;
}

/*
 * Copy one row from IRD to ARD.
 * pos: row number in the rowset
 * Returns: ...
 */
static SQLRETURN copy_one_row(esodbc_stmt_st *stmt, SQLULEN pos, UJObject row)
{
	SQLSMALLINT i;
	SQLLEN rowno;
	SQLRETURN ret;
	UJObject obj;
	void *iter_row;
	SQLLEN *ind_len;
	long long ll;
	double dbl;
	const wchar_t *wstr;
	BOOL boolval;
	size_t len;
	BOOL with_info;
	esodbc_desc_st *ard, *ird;
	esodbc_rec_st *arec, *irec;

	rowno = stmt->rset.frows + pos + /*1-based*/1;
	ard = stmt->ard;
	ird = stmt->ird;

#define RET_ROW_DIAG(_state, _message, _colno) \
	do { \
		if (ard->array_status_ptr) \
			ard->array_status_ptr[pos] = SQL_ROW_ERROR; \
		return post_row_diagnostic(&stmt->hdr.diag, _state, MK_WPTR(_message),\
				0, rowno, _colno); \
	} while (0)
#define SET_ROW_DIAG(_rowno, _colno) \
	do { \
		stmt->hdr.diag.row_number = _rowno; \
		stmt->hdr.diag.column_number = _colno; \
	} while (0)

	if (! UJIsArray(row)) {
		ERRH(stmt, "one '%s' (#%zd) element in result set not array; type:"
			" %d.", JSON_ANSWER_ROWS, stmt->rset.vrows, UJGetType(row));
		RET_ROW_DIAG(SQL_STATE_01S01, MSG_INV_SRV_ANS,
			SQL_NO_COLUMN_NUMBER);
	}
	iter_row = UJBeginArray(row);
	if (! iter_row) {
		ERRH(stmt, "Failed to obtain iterator on row (#%zd): %s.", rowno,
			UJGetError(stmt->rset.state));
		RET_ROW_DIAG(SQL_STATE_01S01, MSG_INV_SRV_ANS,
			SQL_NO_COLUMN_NUMBER);
	}

	with_info = FALSE;
	/* iterate over the contents of one table row */
	for (i = 0; i < ard->count && UJIterArray(&iter_row, &obj); i ++) {
		arec = &ard->recs[i]; /* access safe if 'i < ard->count' */
		/* if record not bound skip it */
		if (! REC_IS_BOUND(arec)) {
			DBGH(stmt, "column #%d not bound, skipping it.", i + 1);
			continue;
		}

		irec = &ird->recs[i]; /* access checked by UJIterArray() condition */

		switch (UJGetType(obj)) {
			default:
				ERRH(stmt, "unexpected object of type %d in row L#%zd/T#%zd.",
					UJGetType(obj), stmt->rset.vrows, stmt->rset.frows);
				RET_ROW_DIAG(SQL_STATE_01S01, MSG_INV_SRV_ANS, i + 1);
			/* RET_.. returns */

			case UJT_Null:
				DBGH(stmt, "value [%zd, %d] is NULL.", rowno, i + 1);
				/* Note: if ever causing an issue, check
				 * arec->es_type->nullable before returning NULL to app */
				ind_len = deferred_address(SQL_DESC_INDICATOR_PTR, pos, arec);
				if (! ind_len) {
					ERRH(stmt, "no buffer to signal NULL value.");
					RET_ROW_DIAG(SQL_STATE_22002, "Indicator variable required"
						" but not supplied", i + 1);
				}
				*ind_len = SQL_NULL_DATA;
				continue; /* instead of break! no 'ret' processing to do. */

			case UJT_String:
				wstr = UJReadString(obj, &len);
				DBGH(stmt, "value [%zd, %d] is string [%d]:`" LWPDL "`.",
					rowno, i + 1, len, len, wstr);
				/* UJSON4C returns chars count, but 0-terminates w/o counting
				 * the terminator */
				assert(wstr[len] == 0);
				/* "When character data is returned from the driver to the
				 * application, the driver must always null-terminate it." */
				ret = copy_string(arec, irec, pos, wstr, len + /*\0*/1);
				break;

			case UJT_Long:
			case UJT_LongLong:
				ll = UJNumericLongLong(obj);
				DBGH(stmt, "value [%zd, %d] is numeric: %lld.", rowno, i + 1,
					ll);
				ret = copy_longlong(arec, irec, pos, ll);
				break;

			case UJT_Double:
				dbl = UJNumericFloat(obj);
				DBGH(stmt, "value [%zd, %d] is double: %f.", rowno, i + 1,
					dbl);
				ret = copy_double(arec, irec, pos, dbl);
				break;

			case UJT_True:
			case UJT_False:
				boolval = UJGetType(obj) == UJT_True ? TRUE : FALSE;
				DBGH(stmt, "value [%zd, %d] is boolean: %d.", rowno, i + 1,
					boolval);
				ret = copy_boolean(arec, irec, pos, boolval);
				break;
		}

		switch (ret) {
			case SQL_SUCCESS_WITH_INFO:
				with_info = TRUE;
				SET_ROW_DIAG(rowno, i + 1);
			case SQL_SUCCESS:
				break;
			default: /* error */
				SET_ROW_DIAG(rowno, i + 1);
				return ret;
		}
	}

	if (ird->array_status_ptr) {
		ird->array_status_ptr[pos] = with_info ? SQL_ROW_SUCCESS_WITH_INFO :
			SQL_ROW_SUCCESS;
		DBGH(stmt, "status array @0x%p#%d set to %d.", ird->array_status_ptr,
			pos, ird->array_status_ptr[pos]);
	}

	return with_info ? SQL_SUCCESS_WITH_INFO : SQL_SUCCESS;

#undef RET_ROW_DIAG
#undef SET_ROW_DIAG
}

/* TODO: implementation for the below */
static BOOL conv_implemented(SQLSMALLINT sqltype, SQLSMALLINT ctype)
{
	switch (ctype) {
		case SQL_C_GUID:

		case SQL_C_INTERVAL_DAY:
		case SQL_C_INTERVAL_HOUR:
		case SQL_C_INTERVAL_MINUTE:
		case SQL_C_INTERVAL_SECOND:
		case SQL_C_INTERVAL_DAY_TO_HOUR:
		case SQL_C_INTERVAL_DAY_TO_MINUTE:
		case SQL_C_INTERVAL_DAY_TO_SECOND:
		case SQL_C_INTERVAL_HOUR_TO_MINUTE:
		case SQL_C_INTERVAL_HOUR_TO_SECOND:
		case SQL_C_INTERVAL_MINUTE_TO_SECOND:
		case SQL_C_INTERVAL_MONTH:
		case SQL_C_INTERVAL_YEAR:
		case SQL_C_INTERVAL_YEAR_TO_MONTH:
			// case SQL_C_TYPE_TIMESTAMP_WITH_TIMEZONE:
			// case SQL_C_TYPE_TIME_WITH_TIMEZONE:
			return FALSE;
	}

	switch (sqltype) {
		case SQL_C_GUID:

		case SQL_INTERVAL_DAY:
		case SQL_INTERVAL_HOUR:
		case SQL_INTERVAL_MINUTE:
		case SQL_INTERVAL_SECOND:
		case SQL_INTERVAL_DAY_TO_HOUR:
		case SQL_INTERVAL_DAY_TO_MINUTE:
		case SQL_INTERVAL_DAY_TO_SECOND:
		case SQL_INTERVAL_HOUR_TO_MINUTE:
		case SQL_INTERVAL_HOUR_TO_SECOND:
		case SQL_INTERVAL_MINUTE_TO_SECOND:
		case SQL_INTERVAL_MONTH:
		case SQL_INTERVAL_YEAR:
		case SQL_INTERVAL_YEAR_TO_MONTH:
			// case SQL_TYPE_TIMESTAMP_WITH_TIMEZONE:
			// case SQL_TYPE_TIME_WITH_TIMEZONE:
			return FALSE;
	}

	return TRUE;
}


/* check if data types in returned columns are compabile with buffer types
 * bound for those columns */
static int sql2c_convertible(esodbc_stmt_st *stmt)
{
	SQLSMALLINT i, min;
	esodbc_desc_st *ard, *ird;
	esodbc_rec_st *arec, *irec;

	assert(stmt->hdr.dbc->es_types);
	assert(STMT_HAS_RESULTSET(stmt));

	ard = stmt->ard;
	ird = stmt->ird;

	min = ard->count < ird->count ? ard->count : ird->count;
	for (i = 0; i < min; i ++) {
		arec = &ard->recs[i];
		if (! REC_IS_BOUND(arec)) {
			/* skip not bound columns */
			continue;
		}
		irec = &ird->recs[i];

		if (! ESODBC_TYPES_COMPATIBLE(irec->concise_type,
				arec->concise_type)) {
			ERRH(stmt, "type conversion not possible on column %d: IRD: %hd, "
				"ARD: %hd.", i, irec->concise_type, arec->concise_type);
			return CONVERSION_VIOLATION;
		}
		if (! conv_implemented(irec->concise_type, arec->concise_type)) {
			ERRH(stmt, "conversion not supported on column %d types: IRD: %hd,"
				" ARD: %hd.", i, irec->concise_type, arec->concise_type);
			return CONVERSION_UNSUPPORTED;
		}
	}

	return CONVERSION_SUPPORTED;
}

/*
 * "SQLFetch and SQLFetchScroll use the rowset size at the time of the call to
 * determine how many rows to fetch."
 *
 * "If SQLFetch or SQLFetchScroll encounters an error while retrieving one row
 * of a multirow rowset, or if SQLBulkOperations with an Operation argument of
 * SQL_FETCH_BY_BOOKMARK encounters an error while performing a bulk fetch, it
 * sets the corresponding value in the row status array to SQL_ROW_ERROR,
 * continues fetching rows, and returns SQL_SUCCESS_WITH_INFO."
 *
 * "SQLFetch can be used only for multirow fetches when called in ODBC 3.x; if
 * an ODBC 2.x application calls SQLFetch, it will open only a single-row,
 * forward-only cursor."
 *
 * "The application can change the rowset size and bind new rowset buffers (by
 * calling SQLBindCol or specifying a bind offset) even after rows have been
 * fetched."
 *
 * "SQLFetch returns bookmarks if column 0 is bound." Otherwise, "return more
 * than one row" (if avail).
 *
 * "The driver does not return SQLSTATE 01S01 (Error in row) to indicate that
 * an error has occurred while rows were fetched by a call to SQLFetch." (same
 * for SQLFetchScroll).
 *
 * "SQL_ROW_NOROW: The rowset overlapped the end of the result set, and no row
 * was returned that corresponded to this element of the row status array."
 *
 * "If the bound address is 0, no data value is returned" (also for row/column
 * binding)
 *
 * "In the IRD, this header field points to a row status array containing
 * status values after a call to SQLBulkOperations, SQLFetch, SQLFetchScroll,
 * or SQLSetPos."  = row status array of IRD (.array_status_ptr); can be NULL.
 *
 * "The binding offset is always added directly to the values in the
 * SQL_DESC_DATA_PTR, SQL_DESC_INDICATOR_PTR, and SQL_DESC_OCTET_LENGTH_PTR
 * fields." (.bind_offset.ptr)
 *
 * "In ARDs, this field specifies the binding orientation when SQLFetchScroll
 * or SQLFetch is called on the associated statement handle." (.bind_type)
 *
 * "In an IRD, this SQLULEN * header field points to a buffer containing the
 * number of rows fetched after a call to SQLFetch or SQLFetchScroll, or the
 * number of rows affected in a bulk operation performed by a call to
 * SQLBulkOperations or SQLSetPos, including error rows."
 * (.rows_processed_ptr)
 *
 * "The variable that the StrLen_or_Ind argument refers to is used for both
 * indicator and length information. If a fetch encounters a null value for
 * the column, it stores SQL_NULL_DATA in this variable; otherwise, it stores
 * the data length in this variable. Passing a null pointer as StrLen_or_Ind
 * keeps the fetch operation from returning the data length but makes the
 * fetch fail if it encounters a null value and has no way to return
 * SQL_NULL_DATA." (.indicator_ptr)
 */
SQLRETURN EsSQLFetch(SQLHSTMT StatementHandle)
{
	esodbc_stmt_st *stmt;
	esodbc_desc_st *ard, *ird;
	SQLULEN i, j;
	UJObject row;
	SQLRETURN ret;
	int errors;

	stmt = STMH(StatementHandle);
	ard = stmt->ard;
	ird = stmt->ird;

	if (! STMT_HAS_RESULTSET(stmt)) {
		if (STMT_NODATA_FORCED(stmt)) {
			DBGH(stmt, "empty result set flag set - returning no data.");
			return SQL_NO_DATA;
		}
		ERRH(stmt, "no resultset available on statement.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	/* Check if the data [type] stored in DB is compatiblie with the buffer
	 * [type] the application provides. This test can only be done at
	 * fetch-time, since the application can unbind/rebind columns at any time
	 * (i.e. also in-between consecutive fetches). */
	switch (stmt->sql2c_conversion) {
		case CONVERSION_VIOLATION:
			ERRH(stmt, "types compibility check had failed already "
				"(violation).");
			RET_HDIAGS(stmt, SQL_STATE_07006);
		/* RET_ returns */

		case CONVERSION_UNSUPPORTED:
			ERRH(stmt, "types compibility check had failed already "
				"(unsupported).");
			RET_HDIAG(stmt, SQL_STATE_HYC00, "Conversion target type not"
				" supported", 0);
		/* RET_ returns */

		case CONVERSION_SKIPPED:
			DBGH(stmt, "types compatibility skipped.");
			/* check unnecessary (SYS TYPES, possiblity other metas) */
			break;

		case CONVERSION_UNCHECKED:
			stmt->sql2c_conversion = sql2c_convertible(stmt);
			if (stmt->sql2c_conversion < 0) {
				ERRH(stmt, "convertibility check: failed!");
				RET_HDIAGS(stmt,
					stmt->sql2c_conversion == CONVERSION_VIOLATION ?
					SQL_STATE_07006 : SQL_STATE_HYC00);
			}
			DBGH(stmt, "convertibility check: OK.");
		/* no break; */

		default:
			DBGH(stmt, "ES/app data/buffer types found compatible.");
	}

	DBGH(stmt, "(`" LCPDL "`); cursor @ %zd / %zd.", LCSTR(&stmt->u8sql),
		stmt->rset.vrows, stmt->rset.nrows);

	DBGH(stmt, "rowset max size: %d.", ard->array_size);
	errors = 0;
	/* for all rows in rowset/array, iterate over rows in current resultset */
	for (i = stmt->rset.array_pos; i < ard->array_size; i ++) {
		if (! UJIterArray(&stmt->rset.rows_iter, &row)) {
			DBGH(stmt, "ran out of rows in current result set: nrows=%zd, "
				"vrows=%zd.", stmt->rset.nrows, stmt->rset.vrows);
			if (stmt->rset.eccnt) { /*do I have an Elastic cursor? */
				stmt->rset.array_pos = i;
				ret = EsSQLExecute(stmt);
				if (! SQL_SUCCEEDED(ret)) {
					ERRH(stmt, "failed to fetch next results.");
					return ret;
				}
				return EsSQLFetch(StatementHandle);
			} else {
				DBGH(stmt, "reached end of entire result set. fetched=%zd.",
					stmt->rset.frows);
				/* indicate the non-processed rows in rowset */
				if (ard->array_status_ptr)
					for (j = i; j < ard->array_size; j ++) {
						ard->array_status_ptr[j] = SQL_ROW_NOROW;
					}
			}
			break;
		}
		ret = copy_one_row(stmt, i, row);
		if (! SQL_SUCCEEDED(ret)) {
			ERRH(stmt, "copying row %zd failed.", stmt->rset.vrows + i + 1);
			errors ++;
		}
	}
	stmt->rset.array_pos = 0;

	/* account for processed rows */
	stmt->rset.vrows += i;
	stmt->rset.frows += i;

	/* return number of processed rows (even if 0) */
	if (ird->rows_processed_ptr) {
		DBGH(stmt, "setting number of processed rows to: %u.", i);
		*ird->rows_processed_ptr = i;
	}

	if (i <= 0) {
		DBGH(stmt, "no data %sto return.", stmt->rset.vrows ? "left ": "");
		return SQL_NO_DATA;
	}

	if (errors && i <= errors) {
		ERRH(stmt, "processing failed for all rows [%d].", errors);
		return SQL_ERROR;
	}

	/* only failures need stmt.diag defer'ing */
	return SQL_SUCCESS;
}

/*
 * "SQLSetPos uses the rowset size that is in effect as of the preceding call
 * to SQLFetch or SQLFetchScroll, because SQLSetPos operates on a rowset that
 * has already been set. SQLSetPos also will pick up the new rowset size if
 * SQLBulkOperations has been called after the rowset size was changed."
 *
 * "When a block cursor first returns a rowset, the current row is the first
 * row of the rowset. To change the current row, the application calls
 * SQLSetPos or SQLBulkOperations (to update by bookmark)."
 *
 * "The driver returns SQLSTATE 01S01 (Error in row) only to indicate that an
 * error has occurred while rows were fetched by a call to SQLSetPos to
 * perform a bulk operation when the function is called in state S7." (not
 * supported currently, with RO operation)
 *
 * "In the IRD, this header field points to a row status array containing
 * status values after a call to SQLBulkOperations, SQLFetch, SQLFetchScroll,
 * or SQLSetPos."  = row status array of IRD (.array_status_ptr)
 *
 * "In the ARD, this header field points to a row operation array of values
 * that can be set by the application to indicate whether this row is to be
 * ignored for SQLSetPos operations." .array_status_ptr
 * "If the value in the SQL_DESC_ARRAY_STATUS_PTR field of the ARD is a null
 * pointer, all rows are included in the bulk operation"
 */
SQLRETURN EsSQLSetPos(
	SQLHSTMT        StatementHandle,
	SQLSETPOSIROW   RowNumber,
	SQLUSMALLINT    Operation,
	SQLUSMALLINT    LockType)
{
	switch(Operation) {
		case SQL_POSITION:
			// FIXME
			FIXME;
			break;

		case SQL_REFRESH:
		case SQL_UPDATE:
		case SQL_DELETE:
			ERRH(StatementHandle, "operation %d not supported.", Operation);
			RET_HDIAGS(STMH(StatementHandle), SQL_STATE_HYC00);
		default:
			ERRH(StatementHandle, "unknown operation type: %d.", Operation);
			RET_HDIAGS(STMH(StatementHandle), SQL_STATE_HY092);
	}
	return SQL_SUCCESS;
}

/*
 * == JDBC's Jdbc/PreparedStatement.executeLargeUpdate()
 * "SQLBulkOperations uses the rowset size in effect at the time of the call,
 * because it performs operations on a table independent of any fetched
 * rowset."
 * "In the IRD, this header field points to a row status array containing
 * status values after a call to SQLBulkOperations, SQLFetch, SQLFetchScroll,
 * or SQLSetPos."  = row status array of IRD (.array_status_ptr)
 */
SQLRETURN EsSQLBulkOperations(
	SQLHSTMT            StatementHandle,
	SQLSMALLINT         Operation)
{
	ERRH(StatementHandle, "data update functions not supported");
	RET_HDIAGS(STMH(StatementHandle), SQL_STATE_IM001);
}

SQLRETURN EsSQLCloseCursor(SQLHSTMT StatementHandle)
{
	esodbc_stmt_st *stmt = STMH(StatementHandle);
	if (! STMT_HAS_RESULTSET(stmt)) {
		ERRH(stmt, "no open cursor for statement");
		RET_HDIAGS(stmt, SQL_STATE_24000);
	}
	return EsSQLFreeStmt(StatementHandle, SQL_CLOSE);
}

SQLRETURN EsSQLNumResultCols(SQLHSTMT StatementHandle,
	_Out_ SQLSMALLINT *ColumnCount)
{
	return EsSQLGetDescFieldW(STMH(StatementHandle)->ird, NO_REC_NR,
			SQL_DESC_COUNT, ColumnCount, SQL_IS_SMALLINT, NULL);
}

/*
 * "The prepared statement associated with the statement handle can be
 * re-executed by calling SQLExecute until the application frees the statement
 * with a call to SQLFreeStmt with the SQL_DROP option or until the statement
 * handle is used in a call to SQLPrepare, SQLExecDirect, or one of the
 * catalog functions (SQLColumns, SQLTables, and so on)."
 */
SQLRETURN EsSQLPrepareW
(
	SQLHSTMT    hstmt,
	_In_reads_(cchSqlStr) SQLWCHAR *szSqlStr,
	SQLINTEGER  cchSqlStr
)
{
	esodbc_stmt_st *stmt = STMH(hstmt);
	SQLRETURN ret;

	if (cchSqlStr == SQL_NTS) {
		cchSqlStr = (SQLINTEGER)wcslen(szSqlStr);
	} else if (cchSqlStr <= 0) {
		ERRH(stmt, "invalid statment length: %d.", cchSqlStr);
		RET_HDIAGS(stmt, SQL_STATE_HY090);
	}
	DBGH(stmt, "preparing `" LWPDL "` [%d]", cchSqlStr, szSqlStr,
		cchSqlStr);

	ret = EsSQLFreeStmt(stmt, ESODBC_SQL_CLOSE);
	assert(SQL_SUCCEEDED(ret)); /* can't return error */

	return attach_sql(stmt, szSqlStr, cchSqlStr);
}


static SQLRETURN set_param_decdigits(esodbc_rec_st *irec,
	SQLUSMALLINT param_no, SQLSMALLINT decdigits)
{
	assert(irec->desc->type == DESC_TYPE_IPD);

	switch (irec->meta_type) {
		/* for "SQL_TYPE_TIME, SQL_TYPE_TIMESTAMP, SQL_INTERVAL_SECOND,
		 * SQL_INTERVAL_DAY_TO_SECOND, SQL_INTERVAL_HOUR_TO_SECOND, or
		 * SQL_INTERVAL_MINUTE_TO_SECOND, the SQL_DESC_PRECISION field of the
		 * IPD is set to DecimalDigits." */
		case METATYPE_DATETIME:
			if (irec->concise_type == SQL_TYPE_DATE) {
				break;
			}
		case METATYPE_INTERVAL_WSEC:
			if (decdigits < 0) {
				ERRH(irec->desc, "can't set negative (%hd) as fractional "
					"second precision.", decdigits);
				RET_HDIAGS(irec->desc, SQL_STATE_HY104);
			}
			return EsSQLSetDescFieldW(irec->desc, param_no, SQL_DESC_PRECISION,
					(SQLPOINTER)(intptr_t)decdigits, SQL_IS_SMALLINT);

		/* for " SQL_NUMERIC or SQL_DECIMAL, the SQL_DESC_SCALE field of the
		 * IPD is set to DecimalDigits." */
		case METATYPE_EXACT_NUMERIC:
			if (irec->concise_type == SQL_DECIMAL ||
				irec->concise_type == SQL_NUMERIC) {
				return EsSQLSetDescFieldW(irec->desc, param_no, SQL_DESC_SCALE,
						(SQLPOINTER)(intptr_t)decdigits, SQL_IS_SMALLINT);
			}
			break; // formal

		default:
			/* "For all other data types, the DecimalDigits argument is
			 * ignored." */
			;
	}

	return SQL_SUCCESS;
}

static SQLSMALLINT get_param_decdigits(esodbc_rec_st *irec)
{
	assert(irec->desc->type == DESC_TYPE_IPD);

	switch(irec->meta_type) {
		case METATYPE_DATETIME:
			if (irec->concise_type == SQL_TYPE_DATE) {
				break;
			}
		case METATYPE_INTERVAL_WSEC:
			return irec->precision;

		case METATYPE_EXACT_NUMERIC:
			if (irec->concise_type == SQL_DECIMAL ||
				irec->concise_type == SQL_NUMERIC) {
				return irec->scale;
			}
			break;

		default:
			WARNH(irec->desc, "retriving decdigits for IPD metatype: %d.",
				irec->meta_type);
	}

	return 0;
}

static SQLRETURN set_param_size(esodbc_rec_st *irec,
	SQLUSMALLINT param_no, SQLULEN size)
{
	assert(irec->desc->type == DESC_TYPE_IPD);

	switch (irec->meta_type) {
		/* for "SQL_CHAR, SQL_VARCHAR, SQL_LONGVARCHAR, SQL_BINARY,
		 * SQL_VARBINARY, SQL_LONGVARBINARY, or one of the concise SQL
		 * datetime or interval data types, the SQL_DESC_LENGTH field of the
		 * IPD is set to the value of [s]ize." */
		case METATYPE_STRING:
		case METATYPE_BIN:
		case METATYPE_DATETIME:
		case METATYPE_INTERVAL_WSEC:
		case METATYPE_INTERVAL_WOSEC:
			return EsSQLSetDescFieldW(irec->desc, param_no, SQL_DESC_LENGTH,
					(SQLPOINTER)(uintptr_t)size, SQL_IS_UINTEGER);

		/* for "SQL_DECIMAL, SQL_NUMERIC, SQL_FLOAT, SQL_REAL, or SQL_DOUBLE,
		 * the SQL_DESC_PRECISION field of the IPD is set to the value of
		 * [s]ize." */
		case METATYPE_EXACT_NUMERIC:
			if (irec->concise_type != SQL_DECIMAL &&
				irec->concise_type != SQL_NUMERIC) {
				break;
			}
		/* no break */
		case METATYPE_FLOAT_NUMERIC:
			// TODO: https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/column-size :
			// "The ColumnSize argument of SQLBindParameter is ignored for
			// this data type." (floats included): ????
			return EsSQLSetDescFieldW(irec->desc, param_no, SQL_DESC_PRECISION,
					/* cast: ULEN -> SMALLINT; XXX: range check? */
					(SQLPOINTER)(uintptr_t)size, SQL_IS_SMALLINT);

		default:
			;/* "For other data types, the [s]ize argument is ignored." */
	}
	return SQL_SUCCESS;
}

static SQLULEN get_param_size(esodbc_rec_st *irec)
{
	assert(irec->desc->type == DESC_TYPE_IPD);

	switch (irec->meta_type) {
		case METATYPE_STRING:
		case METATYPE_BIN:
		case METATYPE_DATETIME:
		case METATYPE_INTERVAL_WSEC:
		case METATYPE_INTERVAL_WOSEC:
			return irec->length;

		case METATYPE_EXACT_NUMERIC:
			// TODO: make DEC, NUM a floating meta?
			if (irec->concise_type != SQL_DECIMAL &&
				irec->concise_type != SQL_NUMERIC) {
				assert(irec->es_type);
				return irec->es_type->column_size;
			}

		case METATYPE_FLOAT_NUMERIC:
			return irec->precision;

		default:
			WARNH(irec->desc, "retriving colsize for IPD metatype: %d.",
				irec->meta_type);
	}
	return 0;
}

/* Find the ES/SQL type given in es_type; for ID matching multiple types
 * (scaled/half_float and keyword/text) use the best matching col_size, which
 * is the smallest, that's still matching (<=) the given one. This assumes the
 * types are ordered by it (as per the spec). */
static esodbc_estype_st *lookup_es_type(esodbc_dbc_st *dbc,
	SQLSMALLINT es_type, SQLULEN col_size)
{
	SQLULEN i;

	for (i = 0; i < dbc->no_types; i ++) {
		if (dbc->es_types[i].data_type == es_type) {
			if (col_size <= 0) {
				return &dbc->es_types[i];
			} else {
				if (col_size <= dbc->es_types[i].column_size) {
					return &dbc->es_types[i];
				}
				if (es_type == SQL_VARCHAR &&
					dbc->es_types[i].column_size == dbc->max_varchar_size) {
					return &dbc->es_types[i];
				}
				if (es_type == SQL_FLOAT &&
					dbc->es_types[i].column_size == dbc->max_float_size) {
					return &dbc->es_types[i];
				}
			}
		}
	}
	WARNH(dbc, "no ES/SQL type found for ID %hd and column size %hd.",
		es_type, col_size);
	return NULL;
}

/* find the matching ES/SQL type for app's SQL type, which can be an exact
 * math against ES/SQL types, but also some other valid SQL type. */
static esodbc_estype_st *match_es_type(esodbc_rec_st *arec,
	esodbc_rec_st *irec)
{
	SQLULEN i, length;
	esodbc_dbc_st *dbc = arec->desc->hdr.stmt->hdr.dbc;

	for (i = 0; i < dbc->no_types; i ++) {
		if (dbc->es_types[i].data_type == irec->concise_type) {
			switch (irec->concise_type) {
				/* For SQL types mappign to more than one ES/SQL type, choose
				 * the ES/SQL type with smallest "size" that covers user given
				 * precision OR that has maximum precision (in case user's is
				 * larger than max ES/SQL offers. */
				case SQL_FLOAT: /* HALF_FLOAT, SCALED_FLOAT */
					if (irec->precision <= dbc->es_types[i].column_size ||
						dbc->es_types[i].column_size == dbc->max_float_size) {
						return &dbc->es_types[i];
					}
					break;
				case SQL_VARCHAR: /* KEYWORD, TEXT */
					length = irec->length ? irec->length : arec->octet_length;
					if (length <= dbc->es_types[i].column_size ||
						dbc->es_types[i].column_size==dbc->max_varchar_size) {
						return &dbc->es_types[i];
					}
					break;
				default:
					/* unequivocal match */
					return &dbc->es_types[i];
			}
		}
	}

	/* app specified an SQL type with no direct mapping to an ES/SQL type */
	switch (irec->meta_type) {
		case METATYPE_EXACT_NUMERIC:
			assert(irec->concise_type == SQL_DECIMAL ||
				irec->concise_type == SQL_NUMERIC);
			return lookup_es_type(dbc, SQL_FLOAT, irec->precision);

		case METATYPE_STRING:
			length = irec->length ? irec->length : arec->octet_length;
			return lookup_es_type(dbc, SQL_VARCHAR, length);
		case METATYPE_BIN:
			/* SQL_VARBINARY == -3 == ES/SQL BINARY */
			return lookup_es_type(dbc, SQL_VARBINARY, /*no prec*/0);
		case METATYPE_DATETIME:
			assert(irec->concise_type == SQL_TYPE_DATE ||
				irec->concise_type == SQL_TYPE_TIME);
			return lookup_es_type(dbc, SQL_TYPE_TIMESTAMP, /*no prec*/0);
		case METATYPE_BIT:
			return lookup_es_type(dbc, ESODBC_SQL_BOOLEAN, /*no prec*/0);
		case METATYPE_UID:
			return lookup_es_type(dbc, SQL_VARCHAR, /*no prec: TEXT*/0);

		case METATYPE_INTERVAL_WSEC:
		case METATYPE_INTERVAL_WOSEC:
		/* TODO: implement them once avail in ES */

		case METATYPE_FLOAT_NUMERIC: /* should have matched already */
		case METATYPE_MAX:
		/* -> SQL_C_DEFAULT, ESODBC_SQL_NULL, should've matched already */
		case METATYPE_UNKNOWN:
		default:
			BUGH(irec->desc, "unexpected meta type: %d.", irec->meta_type);
			SET_HDIAG(irec->desc, SQL_STATE_HY000,
				"bug converting parameters", 0);
	}

	return NULL;
}

/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/sending-long-data
 * Note: must use EsSQLSetDescFieldW() for param data-type setting, to call
 * set_defaults_from_type(), to meet the "Other fields implicitly set"
 * requirements from the page linked in set_defaults_from_type() comments.
 *
 * "Bindings remain in effect until the application calls SQLBindParameter
 * again, calls SQLFreeStmt with the SQL_RESET_PARAMS option, or calls
 * SQLSetDescField to set the SQL_DESC_COUNT header field of the APD to 0."
 */
SQLRETURN EsSQLBindParameter(
	SQLHSTMT        StatementHandle,
	SQLUSMALLINT    ParameterNumber,
	SQLSMALLINT     InputOutputType,
	SQLSMALLINT     ValueType,
	SQLSMALLINT     ParameterType,
	SQLULEN         ColumnSize,
	SQLSMALLINT     DecimalDigits,
	SQLPOINTER      ParameterValuePtr,
	SQLLEN          BufferLength,
	SQLLEN         *StrLen_or_IndPtr)
{
	SQLRETURN ret;
	esodbc_stmt_st *stmt = STMH(StatementHandle);
	esodbc_desc_st *desc;
	SQLSMALLINT apd_prev_count, ipd_prev_count;
	esodbc_rec_st *irec, *arec;

	if (InputOutputType != SQL_PARAM_INPUT) {
		ERRH(stmt, "parameter IO-type (%hd) not supported.", InputOutputType);
		RET_HDIAG(stmt, SQL_STATE_HYC00, "parameter IO-type not supported", 0);
	}

	/* Note: "If StrLen_or_IndPtr is a null pointer, the driver assumes that
	 * all input parameter values are non-NULL and that character and binary
	 * data is null-terminated." */
	if (StrLen_or_IndPtr) {
		if (*StrLen_or_IndPtr == SQL_DATA_AT_EXEC ||
			*StrLen_or_IndPtr < SQL_NTSL) {
			ERRH(stmt, "data-at-exec not supported (LenInd=%lld).",
				*StrLen_or_IndPtr);
			RET_HDIAG(stmt, SQL_STATE_HYC00, "data-at-exec not supported", 0);
		}
	} else {
		/* "If the ParameterValuePtr and StrLen_or_IndPtr arguments specified
		 * in SQLBindParameter are both null pointers, that function returns
		 * SQLSTATE HY009". */
		if (! ParameterValuePtr) {
			ERRH(stmt, "both value pointer and indicator are NULL.");
			RET_HDIAGS(stmt, SQL_STATE_HY009);
		}
	}

	apd_prev_count = stmt->apd->count;
	ipd_prev_count = stmt->ipd->count;

	/*
	 * APD descriptor setting.
	 */
	desc = stmt->apd;

	if (apd_prev_count < ParameterNumber) {
		ret = EsSQLSetDescFieldW(desc, NO_REC_NR, SQL_DESC_COUNT,
				/* ParameterNumber is unsigned, but SQL_DESC_COUNT is signed */
				(SQLPOINTER)(uintptr_t)ParameterNumber, SQL_IS_SMALLINT);
		if (! SQL_SUCCEEDED(ret)) {
			goto copy_diag;
		}
	}

	/* set types (or verbose for datetime/interval types) */
	ret = EsSQLSetDescFieldW(desc, ParameterNumber, SQL_DESC_CONCISE_TYPE,
			(SQLPOINTER)(intptr_t)ValueType, SQL_IS_SMALLINT);
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	/* "Sets the SQL_DESC_OCTET_LENGTH field to the value of BufferLength." */
	ret = EsSQLSetDescFieldW(desc, ParameterNumber, SQL_DESC_OCTET_LENGTH,
			(SQLPOINTER)(intptr_t)BufferLength, SQL_IS_INTEGER);
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	/* "Sets the SQL_DESC_OCTET_LENGTH_PTR field to the value of
	 * StrLen_or_Ind." */
	ret = EsSQLSetDescFieldW(desc, ParameterNumber, SQL_DESC_OCTET_LENGTH_PTR,
			StrLen_or_IndPtr,
			SQL_LEN_BINARY_ATTR((SQLINTEGER)sizeof(StrLen_or_IndPtr)));
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	/* "Sets the SQL_DESC_INDICATOR_PTR field to the value of
	 * StrLen_or_Ind."
	 * "The SQL_DESC_ARRAY_STATUS_PTR header field in the APD is used to
	 * ignore parameters." (IPD's indicate execution error status.) */
	ret = EsSQLSetDescFieldW(desc, ParameterNumber, SQL_DESC_INDICATOR_PTR,
			StrLen_or_IndPtr,
			SQL_LEN_BINARY_ATTR((SQLINTEGER)sizeof(StrLen_or_IndPtr)));
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	/* "Sets the SQL_DESC_DATA_PTR field to the value of ParameterValue."
	 * Note: needs to be last set field, as setting other fields unbinds. */
	ret = EsSQLSetDescFieldW(desc, ParameterNumber, SQL_DESC_DATA_PTR,
			ParameterValuePtr, SQL_IS_POINTER);
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	/*
	 * IPD descriptor setting.
	 */
	desc = stmt->ipd;

	if (ipd_prev_count < ParameterNumber) {
		ret = EsSQLSetDescFieldW(desc, NO_REC_NR, SQL_DESC_COUNT,
				/* ParameterNumber is unsigned, but SQL_DESC_COUNT is signed */
				(SQLPOINTER)(uintptr_t)ParameterNumber, SQL_IS_SMALLINT);
		if (! SQL_SUCCEEDED(ret)) {
			goto copy_diag;
		}
	}

	/* set types (or verbose for datetime/interval types) */
	ret = EsSQLSetDescFieldW(desc, ParameterNumber, SQL_DESC_CONCISE_TYPE,
			(SQLPOINTER)(intptr_t)ParameterType, SQL_IS_SMALLINT);
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	/* "sets the SQL_DESC_PARAMETER_TYPE field of the IPD." */
	ret = EsSQLSetDescFieldW(desc, ParameterNumber, SQL_DESC_PARAMETER_TYPE,
			(SQLPOINTER)(intptr_t)InputOutputType, SQL_IS_SMALLINT);
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	irec = get_record(desc, ParameterNumber, /*grow?*/FALSE);
	assert(irec);

	ret = set_param_decdigits(irec, ParameterNumber, DecimalDigits);
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	ret = set_param_size(irec, ParameterNumber, ColumnSize);
	if (! SQL_SUCCEEDED(ret)) {
		goto copy_diag;
	}

	/*
	 * data conversion settings
	 */

	arec = get_record(stmt->apd, ParameterNumber, /*grow?*/FALSE);
	assert(arec);

	if (! conv_implemented(irec->concise_type, arec->concise_type)) {
		ERRH(stmt, "type conversion not supported on parameter #%hd:"
			" IPD: %hd, APD: %hd.", ParameterNumber,
			irec->concise_type, arec->concise_type);
		SET_HDIAG(desc, SQL_STATE_HYC00,
			"Optional feature not implemented", 0);
		goto copy_diag;
	}

	irec->es_type = match_es_type(arec, irec);
	if (! irec->es_type) {
		/* validation shoudl have been done earlier on meta type setting
		 * (SQL_DESC_CONCISE_TYPE) */
		BUGH(stmt, "failed to match valid SQL type %hd.", irec->concise_type);
		SET_HDIAG(desc, SQL_STATE_HY000, "parameter binding bug", 0);
		goto copy_diag;
	}
	DBGH(stmt, "SQL type %hd matched to `" LCPDL "` ES/SQL type.",
		ParameterType, LCSTR(&irec->es_type->type_name_c));

	/* check types compatibility once types have been set and validated */
	if (! ESODBC_TYPES_COMPATIBLE(irec->concise_type, arec->concise_type)) {
		ERRH(desc, "type conversion not possible on parameter #%hd: "
			"IPD: %hd, APD: %hd.", ParameterNumber,
			irec->concise_type, arec->concise_type);
		SET_HDIAG(desc, SQL_STATE_07006, "Restricted data type "
			"attribute violation", 0);
		goto copy_diag;
	}

	DBGH(stmt, "succesfully bound parameter #%hu, IO-type: %hd, "
		"SQL C type: %hd, SQL type: %hd, size: %llu, decdigits: %hd, "
		"buffer@0x%p, length: %lld, LenInd@0x%p.", ParameterNumber,
		InputOutputType, ValueType, ParameterType, ColumnSize, DecimalDigits,
		ParameterValuePtr, BufferLength, StrLen_or_IndPtr);

	return SQL_SUCCESS;

copy_diag:
	/* copy initial error at top handle level, where it's going to be "
	 * "inquired from (more might occur below) */
	HDIAG_COPY(desc, stmt);

	ERRH(stmt, "binding parameter failed -- resetting xPD counts.");
	/* "If the call to SQLBindParameter fails, [...] the SQL_DESC_COUNT field
	 * of the APD" [...] "and the SQL_DESC_COUNT field of the IPD is
	 * unchanged." */
	if (apd_prev_count != stmt->apd->count) {
		ret = EsSQLSetDescFieldW(stmt->apd, NO_REC_NR, SQL_DESC_COUNT,
				(SQLPOINTER)(uintptr_t)-apd_prev_count, SQL_IS_SMALLINT);
		if (! SQL_SUCCEEDED(ret)) {
			ERRH(stmt, "failed to reset APD count back to %hd - descriptor "
				"might be left in inconsistent state!", -apd_prev_count);
		}
	}
	if (ipd_prev_count != stmt->ipd->count) {
		ret = EsSQLSetDescFieldW(stmt->ipd, NO_REC_NR, SQL_DESC_COUNT,
				(SQLPOINTER)(uintptr_t)-ipd_prev_count, SQL_IS_SMALLINT);
		if (! SQL_SUCCEEDED(ret)) {
			ERRH(stmt, "failed to reset IPD count back to %hd - descriptor "
				"might be left in inconsistent state!", -ipd_prev_count);
		}
	}
	/* return original failure code */
	RET_STATE(stmt->hdr.diag.state);
}

/* Converts a C/W-string to a u/llong or double (dest_type?); the xstr->wide
 * needs to be set;
 * Returns success of conversion and pointer to trimmed number str
 * representation.  */
static BOOL xstr_to_number(void *data_ptr, SQLLEN *octet_len_ptr,
	xstr_st *xstr, SQLSMALLINT dest_type, void *dest)
{
	BOOL res;

	if (xstr->wide) {
		xstr->w.str = (SQLWCHAR *)data_ptr;
		if ((octet_len_ptr && *octet_len_ptr == SQL_NTSL) || !octet_len_ptr) {
			xstr->w.cnt = wcslen(xstr->w.str);
		} else {
			xstr->w.cnt = (size_t)(*octet_len_ptr / sizeof(*xstr->w.str));
			xstr->w.cnt -= /*0-term*/1;
		}
	} else {
		xstr->c.str = (SQLCHAR *)data_ptr;
		if ((octet_len_ptr && *octet_len_ptr == SQL_NTSL) || !octet_len_ptr) {
			xstr->c.cnt = strlen(xstr->c.str);
		} else {
			xstr->c.cnt = (size_t)(*octet_len_ptr - /*\0*/1);
		}
	}

	if (! dest) {
		return TRUE;
	}

	if (xstr->wide) {
		wtrim_ws(&xstr->w);
		DBG("converting paramter value `" LWPDL "` to number.",
			LWSTR(&xstr->w));
		switch (dest_type) {
			case SQL_C_SBIGINT:
				res = str2bigint(&xstr->w, /*wide?*/TRUE, (SQLBIGINT *)dest);
				break;
			case SQL_C_UBIGINT:
				res = str2bigint(&xstr->w, /*wide?*/TRUE, (SQLUBIGINT *)dest);
				break;
			case SQL_C_DOUBLE:
				res = str2double(&xstr->w, /*wide?*/TRUE, (SQLDOUBLE *)dest);
				break;
			default:
				assert(0);
		}
	} else {
		trim_ws(&xstr->c);
		DBG("converting paramter value `" LCPDL "` to number.",
			LCSTR(&xstr->c));
		switch (dest_type) {
			case SQL_C_SBIGINT:
				res = str2bigint(&xstr->c, /*wide?*/FALSE, (SQLBIGINT *)dest);
				break;
			case SQL_C_UBIGINT:
				res = str2bigint(&xstr->c, /*wide?*/FALSE, (SQLUBIGINT *)dest);
				break;
			case SQL_C_DOUBLE:
				res = str2double(&xstr->c, /*wide?*/FALSE, (SQLDOUBLE *)dest);
				break;
			default:
				assert(0);
		}
	}

	if (! res) {
		if (xstr->wide) {
			ERR("can't convert `" LWPDL "` to type %hd number.",
				LWSTR(&xstr->w), dest_type);
		} else {
			ERR("can't convert `" LCPDL "` to type %hd number.",
				LCSTR(&xstr->c), dest_type);
		}
		return FALSE;
	} else {
		return TRUE;
	}
}


static inline SQLRETURN c2sql_null(esodbc_rec_st *arec,
	esodbc_rec_st *irec, char *dest, size_t *len)
{
	assert(irec->concise_type == ESODBC_SQL_NULL);
	if (dest) {
		memcpy(dest, JSON_VAL_NULL, sizeof(JSON_VAL_NULL) - /*\0*/1);
	}
	*len = sizeof(JSON_VAL_NULL) - /*\0*/1;
	return SQL_SUCCESS;
}

static SQLRETURN double_to_bool(esodbc_stmt_st *stmt, double dbl, BOOL *val)
{
	DBGH(stmt, "converting double %.6e to bool.", dbl);
#ifdef BOOLEAN_IS_BIT
	if (dbl < 0. && 2. < dbl) {
		ERRH(stmt, "double %.6e out of range.", dbl);
		RET_HDIAGS(stmt, SQL_STATE_22003);
	}
	if (0. < dbl && dbl < 2. && dbl != 1.) {
		/* it's a failure, since SUCCESS_WITH_INFO would be returned only
		 * after data is sent to the server.  */
		ERRH(stmt, "double %.6e right truncated.", dbl);
		RET_HDIAGS(stmt, SQL_STATE_22003);
	}
#endif /* BOOLEAN_IS_BIT */
	*val = dbl != 0.;
	return SQL_SUCCESS;
}

static SQLRETURN c2sql_boolean(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, char *dest, size_t *len)
{
	BOOL val;
	SQLSMALLINT ctype;
	SQLRETURN ret;
	void *data_ptr;
	SQLLEN *octet_len_ptr;
	xstr_st xstr;
	double dbl;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;

	if (! dest) {
		/* return the "worst case" len (and only convert data at copy time) */
		*len = sizeof(JSON_VAL_FALSE) - 1;
		return SQL_SUCCESS;
	}

	/* pointer to app's buffer */
	data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, arec);

	/*INDENT-OFF*/
	switch ((ctype = get_rec_c_type(arec, irec))) {
		case SQL_C_CHAR:
		case SQL_C_WCHAR:
			/* pointer to read from how many bytes we have */
			octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR, pos,
					arec);
			xstr.wide = ctype == SQL_C_WCHAR;
			if (! xstr_to_number(data_ptr, octet_len_ptr, &xstr, SQL_C_DOUBLE,
						&dbl)) {
				RET_HDIAGS(stmt, SQL_STATE_22018);
			}
			ret = double_to_bool(stmt, dbl, &val);
			if (! SQL_SUCCEEDED(ret)) {
				return ret;
			}
			break;

		do {
		case SQL_C_BINARY:
			/* pointer to read from how many bytes we have */
			octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR,
				pos, arec);
			if (! octet_len_ptr) {
				if (((char *)data_ptr)[0]) {
					RET_HDIAGS(stmt, SQL_STATE_22003);
				}
			} else if (*octet_len_ptr != sizeof(SQLCHAR)) {
				RET_HDIAGS(stmt, SQL_STATE_22003);
			}
		/* no break */
		case SQL_C_BIT:
		case SQL_C_UTINYINT: dbl = (double)*(SQLCHAR *)data_ptr; break;
		case SQL_C_TINYINT:
		case SQL_C_STINYINT: dbl = (double)*(SQLSCHAR *)data_ptr; break;
		case SQL_C_SHORT:
		case SQL_C_SSHORT: dbl = (double)*(SQLSMALLINT *)data_ptr; break;
		case SQL_C_USHORT: dbl = (double)*(SQLUSMALLINT *)data_ptr; break;
		case SQL_C_LONG:
		case SQL_C_SLONG: dbl = (double)*(SQLINTEGER *)data_ptr; break;
		case SQL_C_ULONG: dbl = (double)*(SQLUINTEGER *)data_ptr; break;
		case SQL_C_SBIGINT: dbl = (double)*(SQLBIGINT *)data_ptr; break;
		case SQL_C_UBIGINT: dbl = (double)*(SQLUBIGINT *)data_ptr; break;
		case SQL_C_FLOAT: dbl = (double)*(SQLREAL *)data_ptr; break;
		case SQL_C_DOUBLE: dbl = (double)*(SQLDOUBLE *)data_ptr; break;

		case SQL_C_NUMERIC:
			// TODO: better? *(uul *)val[0] != 0 && *[uul *]val[8] != 0 */
			ret = numeric_to_double(irec, data_ptr, &dbl);
			if (! SQL_SUCCEEDED(ret)) {
				return ret;
			}
			break;
		} while (0);
			ret = double_to_bool(stmt, dbl, &val);
			if (! SQL_SUCCEEDED(ret)) {
				return ret;
			}
			break;

		//case SQL_C_BOOKMARK:
		//case SQL_C_VARBOOKMARK:
		default:
			BUGH(stmt, "can't convert SQL C type %hd to boolean.",
				get_rec_c_type(arec, irec));
			RET_HDIAG(stmt, SQL_STATE_HY000, "bug converting parameter", 0);
	}
	/*INDENT-ON*/

	DBGH(stmt, "parameter (pos#%lld) converted to boolean: %d.", pos, val);

	if (val) {
		memcpy(dest, JSON_VAL_TRUE, sizeof(JSON_VAL_TRUE) - /*\0*/1);
		*len = sizeof(JSON_VAL_TRUE) - 1;
	} else {
		memcpy(dest, JSON_VAL_FALSE, sizeof(JSON_VAL_FALSE) - /*\0*/1);
		*len = sizeof(JSON_VAL_FALSE) - 1;
	}
	return SQL_SUCCESS;
}

/*
 * Copies a C/W-string representing a number out to send buffer.
 * wide: type of string;
 * min, max: target SQL numeric type's limits;
 * fixed: target SQL numberic type (integer or floating);
 * dest: buffer's pointer; can be null (when eval'ing) needed space;
 * len: how much of the buffer is needed or has been used.
 */
static SQLRETURN string_to_number(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, BOOL wide, double *min, double *max, BOOL fixed,
	char *dest, size_t *len)
{
	void *data_ptr;
	SQLLEN *octet_len_ptr;
	xstr_st xstr;
	SQLDOUBLE dbl, abs_dbl;
	int ret;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;

	/* pointer to app's buffer */
	data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, arec);
	/* pointer to read from how many bytes we have */
	octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR, pos, arec);

	xstr.wide = wide;
	/* do a conversion check: use double, as a capture all cases
	 * value: ES/SQL will accept a float for an INTEGER param */
	if (! xstr_to_number(data_ptr, octet_len_ptr, &xstr,
			SQL_C_DOUBLE, dest ? &dbl : NULL)) {
		ERRH(stmt, "failed to convert param value to a double.");
		RET_HDIAGS(stmt, SQL_STATE_22018);
	}

	if (! dest) {
		/* check is superfluous, but safer  */
		*len = wide ? xstr.w.cnt : xstr.c.cnt;
		return SQL_SUCCESS;
	}

	/* check against truncation, limits and any given precision */
	abs_dbl = dbl < 0 ? -dbl : dbl;
	if (fixed) {
		if (0 < abs_dbl - (SQLUBIGINT)abs_dbl) {
			ERRH(stmt, "conversion of double %.6e to fixed would"
				" truncate fractional digits", dbl);
			RET_HDIAGS(stmt, SQL_STATE_22001);
		}
		if ((min && dbl < *min) || (max && *max < dbl)) {
			ERRH(stmt, "converted double %.6e out of bounds "
				"[%.6e, %.6e]", dbl, min, *max);
			/* spec requires 22001 here, but that is wrong? */
			RET_HDIAGS(stmt, SQL_STATE_22003);
		}
	} else {
		if ((min && abs_dbl < *min) || (max && *max < abs_dbl)) {
			ERRH(stmt, "converted abs double %.6e out of bounds "
				"[%.6e, %.6e]", abs_dbl, min, *max);
			RET_HDIAGS(stmt, SQL_STATE_22003);
		}
	}
	//
	// TODO: check IRD precision against avail value?
	//

	/* copy values from app's buffer directly */
	if (wide) { /* need a conversion to ANSI */
		*len = xstr.w.cnt;
		ret = ansi_w2c((SQLWCHAR *)data_ptr, dest, *len);
		assert(0 < ret); /* it converted to a float already */
	} else {
		*len = xstr.c.cnt;
		memcpy(dest, data_ptr, *len);
	}
	return SQL_SUCCESS;
}

static SQLRETURN sfixed_to_number(esodbc_stmt_st *stmt, SQLBIGINT src,
	double *min, double *max, char *dest, size_t *len)
{
	if (! dest) {
		/* largest space it could occupy */
		*len = ESODBC_PRECISION_INT64;
		return SQL_SUCCESS;
	}

	DBGH(stmt, "converting paramter value %lld as number.", src);

	if ((min && src < *min) || (max && *max < src)) {
		ERRH(stmt, "source value %lld out of range [%e, %e] for dest type",
			src, *min, *max);
		RET_HDIAGS(stmt, SQL_STATE_22003);
	}

	assert(sizeof(SQLBIGINT) == sizeof(int64_t));
	*len = i64tot(src, dest, /*wide?*/FALSE);
	assert(*len <= ESODBC_PRECISION_INT64);

	return SQL_SUCCESS;
}

static SQLRETURN ufixed_to_number(esodbc_stmt_st *stmt, SQLUBIGINT src,
	double *max, char *dest, size_t *len)
{
	if (! dest) {
		/* largest space it could occupy */
		*len = ESODBC_PRECISION_UINT64;
		return SQL_SUCCESS;
	}

	DBGH(stmt, "converting paramter value %llu as number.", src);

	if (src < 0 || (max && *max < src)) {
		ERRH(stmt, "source value %llu out of range [0, %e] for dest type",
			src, *max);
		RET_HDIAGS(stmt, SQL_STATE_22003);
	}

	assert(sizeof(SQLBIGINT) == sizeof(int64_t));
	*len = ui64tot(src, dest, /*wide?*/FALSE);
	assert(*len <= ESODBC_PRECISION_UINT64);

	return SQL_SUCCESS;
}

static SQLRETURN floating_to_number(esodbc_rec_st *irec, SQLDOUBLE src,
	double *min, double *max, char *dest, size_t *len)
{
	/* format fixed length in scientific notation, -1.23E-45 */
	//const static size_t ff_len = /*-1.*/3 + /*prec*/0 + /*E-*/ 2 +
	//	sizeof(STR(ESODBC_PRECISION_DOUBLE)) - 1;
	size_t maxlen, width;
	int cnt;
	SQLDOUBLE abs_src;
	esodbc_stmt_st *stmt = irec->desc->hdr.stmt;

	//maxlen = get_param_size(irec) + ff_len;
	maxlen = get_param_size(irec);
	if (! dest) {
		/* largest space it could occupy */
		*len = maxlen + /*0-term, for printf*/1;
		return SQL_SUCCESS;
	}

	abs_src = src < 0 ? -src : src;
	if ((min && abs_src < *min) || (max && *max < abs_src)) {
		ERRH(stmt, "source value %e out of range [%e, %e] for dest type",
			src, *min, *max);
		RET_HDIAGS(stmt, SQL_STATE_22003);
	}

	width = maxlen;
	width -= /*sign*/(src < 0);
	width -= /*1.*/2;
	width -= /*e+00*/4 + /*3rd digit*/(abs_src < 1e-100 || 1e100 < src);
	if (width < 0) {
		ERRH(stmt, "parameter size (%zu) to low for floating point.", maxlen);
		RET_HDIAGS(stmt, SQL_STATE_HY104);
	}
	DBGH(stmt, "converting double param %.6e with precision/width: %zu/%d.",
		src, maxlen, width);
	cnt = snprintf(dest, maxlen + /*\0*/1, "%.*e", (int)width, src);
	if (cnt < 0) {
		ERRH(stmt, "failed to print double %e.", src);
		RET_HDIAGS(stmt, SQL_STATE_HY000);
	} else {
		*len = cnt;
		DBGH(stmt, "value %.6e printed as `" LCPDL "`.", src, *len, dest);
	}

	return SQL_SUCCESS;
}

static SQLRETURN binary_to_number(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, char *dest, size_t *len)
{
	void *data_ptr;
	SQLLEN *octet_len_ptr, osize /*octet~*/;
	SQLBIGINT llng;
	SQLDOUBLE dbl;
	esodbc_stmt_st *stmt = irec->desc->hdr.stmt;

	if (! dest) {
		if (irec->meta_type == METATYPE_EXACT_NUMERIC) {
			return sfixed_to_number(stmt, 0LL, NULL, NULL, NULL, len);
		} else {
			assert(irec->meta_type == METATYPE_FLOAT_NUMERIC);
			return floating_to_number(irec, 0., NULL, NULL, NULL, len);
		}
	}

	/* pointer to read from how many bytes we have */
	octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR, pos, arec);
	/* pointer to app's buffer */
	data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, arec);

	if (! octet_len_ptr) {
		/* "If [...] is a null pointer, the driver assumes [...] that
		 * character and binary data is null-terminated." */
		WARNH(stmt, "no length information provided for binary type: "
			"calculating it as a C-string!");
		osize = strlen((char *)data_ptr);
	} else {
		osize = *octet_len_ptr;
	}

#	define CHK_SIZES(_sqlc_type) \
	do { \
		if (osize != sizeof(_sqlc_type)) { \
			ERRH(stmt, "binary data lenght (%zu) misaligned with target" \
				" data type (%hd) size (%lld)", sizeof(_sqlc_type), \
				irec->es_type->data_type, osize); \
			RET_HDIAGS(stmt, SQL_STATE_HY090); \
		} \
	} while (0)
#	define BIN_TO_LLNG(_sqlc_type) \
	do { \
		CHK_SIZES(_sqlc_type); \
		llng = (SQLBIGINT)*(_sqlc_type *)data_ptr; \
	} while (0)
#	define BIN_TO_DBL(_sqlc_type) \
	do { \
		CHK_SIZES(_sqlc_type); \
		dbl = (SQLDOUBLE)*(_sqlc_type *)data_ptr; \
	} while (0)

	/*INDENT-OFF*/
	switch (irec->es_type->data_type) {
		do {
		/* JSON long */
		case SQL_BIGINT: BIN_TO_LLNG(SQLBIGINT); break; /* LONG */
		case SQL_INTEGER: BIN_TO_LLNG(SQLINTEGER); break; /* INTEGER */
		case SQL_SMALLINT: BIN_TO_LLNG(SQLSMALLINT); break; /* SHORT */
		case SQL_TINYINT: BIN_TO_LLNG(SQLSCHAR); break; /* BYTE */
		} while (0);
			return sfixed_to_number(stmt, llng, NULL, NULL, dest, len);

		/* JSON double */
		do {
			// TODO: check accurate limits for floats in ES/SQL
		case SQL_FLOAT: /* HALF_FLOAT, SCALED_FLOAT */
		case SQL_REAL: BIN_TO_DBL(SQLREAL); break; /* FLOAT */
		case SQL_DOUBLE: BIN_TO_DBL(SQLDOUBLE); break; /* DOUBLE */
		} while (0);
			return floating_to_number(irec, dbl, NULL, NULL, dest, len);
	}
	/*INDENT-ON*/

#	undef BIN_TO_LLNG
#	undef BIN_TO_DBL
#	undef CHK_SIZES

	BUGH(arec->desc->hdr.stmt, "unexpected ES/SQL type %hd.",
		irec->es_type->data_type);
	RET_HDIAG(arec->desc->hdr.stmt, SQL_STATE_HY000,
		"parameter conversion bug", 0);
}

static SQLRETURN numeric_to_number(esodbc_rec_st *irec, void *data_ptr,
	char *dest, size_t *len)
{
	SQLDOUBLE dbl;
	SQLRETURN ret;

	if (! dest) {
		return floating_to_number(irec, 0., NULL, NULL, NULL, len);
	}

	ret = numeric_to_double(irec, data_ptr, &dbl);
	if (! SQL_SUCCEEDED(ret)) {
		return ret;
	}
	return floating_to_number(irec, dbl, NULL, NULL, dest, len);
}

static SQLRETURN c2sql_number(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, double *min, double *max, BOOL fixed, char *dest, size_t *len)
{
	void *data_ptr;
	SQLSMALLINT ctype;
	SQLBIGINT llng;
	SQLUBIGINT ullng;
	SQLDOUBLE dbl;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;

	/* pointer to app's buffer */
	data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, arec);

	/*INDENT-OFF*/
	switch ((ctype = get_rec_c_type(arec, irec))) {
		case SQL_C_WCHAR:
		case SQL_C_CHAR:
			return string_to_number(arec, irec, pos, ctype == SQL_C_WCHAR,
					min, max, fixed, dest, len);

		case SQL_C_BINARY:
			return binary_to_number(arec, irec, pos, dest, len);

		do {
		case SQL_C_TINYINT:
		case SQL_C_STINYINT: llng = (SQLBIGINT)*(SQLSCHAR *)data_ptr; break;
		case SQL_C_SHORT:
		case SQL_C_SSHORT: llng = (SQLBIGINT)*(SQLSMALLINT *)data_ptr; break;
		case SQL_C_LONG:
		case SQL_C_SLONG: llng = (SQLBIGINT)*(SQLINTEGER *)data_ptr; break;
		case SQL_C_SBIGINT: llng = *(SQLBIGINT *)data_ptr; break;
		} while (0);
			return sfixed_to_number(stmt, llng, min, max, dest, len);

		do {
		case SQL_C_BIT: // XXX: check if 0/1?
		case SQL_C_UTINYINT: ullng = (SQLUBIGINT)*(SQLCHAR *)data_ptr; break;
		case SQL_C_USHORT: ullng = (SQLUBIGINT)*(SQLUSMALLINT *)data_ptr;break;
		case SQL_C_ULONG: ullng = (SQLUBIGINT)*(SQLUINTEGER *)data_ptr; break;
		case SQL_C_UBIGINT: ullng = *(SQLUBIGINT *)data_ptr; break;
		} while (0);
			return ufixed_to_number(stmt, ullng, max, dest, len);

		do {
		case SQL_C_FLOAT: dbl = (SQLDOUBLE)*(SQLREAL *)data_ptr; break;
		case SQL_C_DOUBLE: dbl = *(SQLDOUBLE *)data_ptr; break;
		} while (0);
			return floating_to_number(irec, dbl, min, max, dest, len);

		case SQL_C_NUMERIC:
			return numeric_to_number(irec, data_ptr, dest, len);

		//case SQL_C_BOOKMARK:
		//case SQL_C_VARBOOKMARK:
		default:
			BUGH(stmt, "can't convert SQL C type %hd to long long.",
				get_rec_c_type(arec, irec));
			RET_HDIAG(stmt, SQL_STATE_HY000, "bug converting parameter", 0);
	}
	/*INDENT-ON*/

	return SQL_SUCCESS;
}

static SQLRETURN convert_str_to_timestamp(esodbc_stmt_st *stmt,
	SQLLEN *octet_len_ptr, void *data_ptr, BOOL wide,
	char *dest, size_t *len)
{
	xstr_st xstr;
	TIMESTAMP_STRUCT tss;
	SQLSMALLINT format;
	cstr_st ts_buff;

	xstr.wide = wide;

	if (wide) {
		xstr.w.str = (SQLWCHAR *)data_ptr;
		if (octet_len_ptr) {
			xstr.w.cnt = *octet_len_ptr / sizeof(SQLWCHAR) - /*\0*/1;
		} else {
			xstr.w.cnt = wcslen(xstr.w.str);
		}
		wtrim_ws(&xstr.w);
	} else {
		xstr.c.str = (SQLCHAR *)data_ptr;
		if (octet_len_ptr) {
			xstr.c.cnt = *octet_len_ptr / sizeof(SQLCHAR) - /*\0*/1;
		} else {
			xstr.c.cnt = strlen(xstr.c.str);
		}
		trim_ws(&xstr.c);
	}

	ts_buff.str = dest;
	ts_buff.cnt = sizeof(ESODBC_ISO8601_TEMPLATE) - 1;
	if (! parse_timedate(&xstr, &tss, &format, &ts_buff)) {
		ERRH(stmt, "failed to parse input as Time/Date/Timestamp");
		RET_HDIAGS(stmt, SQL_STATE_22008);
	} else if (format == SQL_TYPE_TIME) {
		ERRH(stmt, "can not convert a Time to a Timestamp value");
		RET_HDIAGS(stmt, SQL_STATE_22018);
	} else {
		/* conversion from TIME to TIMESTAMP should have been deined earlier */
		assert(format != SQL_TYPE_TIME);
		*len += ts_buff.cnt;
	}

	return SQL_SUCCESS;
}

static SQLRETURN convert_ts_to_timestamp(esodbc_stmt_st *stmt,
	SQLLEN *octet_len_ptr, void *data_ptr, SQLSMALLINT ctype,
	char *dest, size_t *len)
{
	TIMESTAMP_STRUCT *tss, buff;
	DATE_STRUCT *ds;
	int cnt;
	size_t osize;

	switch (ctype) {
		case SQL_C_TYPE_DATE:
			ds = (DATE_STRUCT *)data_ptr;
			memset(&buff, 0, sizeof(buff));
			buff.year = ds->year;
			buff.month = ds->month;
			buff.day = ds->day;
			tss = &buff;
			break;
		case SQL_C_BINARY:
			if (! octet_len_ptr) {
				WARNH(stmt, "no length information provided for binary type: "
					"calculating it as a C-string!");
				osize = strlen((char *)data_ptr);
			} else {
				osize = *octet_len_ptr;
			}
			if (osize != sizeof(TIMESTAMP_STRUCT)) {
				ERRH(stmt, "incorrect binary object size: %zu; expected: %zu.",
					osize, sizeof(TIMESTAMP_STRUCT));
				RET_HDIAGS(stmt, SQL_STATE_22003);
			}
		/* no break */
		case SQL_C_TYPE_TIMESTAMP:
			tss = (TIMESTAMP_STRUCT *)data_ptr;
			break;

		default:
			BUGH(stmt, "unexpected SQL C type %hd.", ctype);
			RET_HDIAG(stmt, SQL_STATE_HY000, "param conversion bug", 0);
	}

	cnt = snprintf(dest, sizeof(ESODBC_ISO8601_TEMPLATE) - 1,
			"%04d-%02d-%02dT%02d:%02d:%02d.%03uZ",
			tss->year, tss->month, tss->day,
			tss->hour, tss->minute, tss->second, tss->fraction);
	if (cnt < 0) {
		ERRH(stmt, "failed printing timestamp struct: %s.", strerror(errno));
		SET_HDIAG(stmt, SQL_STATE_HY000, "C runtime error", 0);
	}
	*len = cnt;

	return SQL_SUCCESS;
}

static SQLRETURN c2sql_timestamp(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, char *dest, size_t *len)
{
	void *data_ptr;
	SQLLEN *octet_len_ptr;
	SQLSMALLINT ctype;
	SQLRETURN ret;
	SQLULEN colsize, offt, decdigits;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;

	if (! dest) {
		/* maximum possible space it can take */
		*len = /*2x `"`*/2 + sizeof(ESODBC_ISO8601_TEMPLATE) - 1;
		return SQL_SUCCESS;
	} else {
		*dest = '"';
	}

	/* pointer to read from how many bytes we have */
	octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR, pos, arec);
	/* pointer to app's buffer */
	data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, arec);

	switch ((ctype = get_rec_c_type(arec, irec))) {
		case SQL_C_CHAR:
		case SQL_C_WCHAR:
			ret = convert_str_to_timestamp(stmt, octet_len_ptr, data_ptr,
					ctype == SQL_C_WCHAR, dest + /*`"`*/1, len);
			if (! SQL_SUCCEEDED(ret)) {
				return ret;
			}
			break;

		case SQL_C_TYPE_TIME:
			// TODO
			ERRH(stmt, "conversion from time to timestamp not implemented.");
			RET_HDIAG(stmt, SQL_STATE_HYC00, "conversion time to timestamp "
				"not yet supported", 0);

		case SQL_C_TYPE_DATE:
		case SQL_C_BINARY:
		case SQL_C_TYPE_TIMESTAMP:
			ret = convert_ts_to_timestamp(stmt, octet_len_ptr, data_ptr,
					ctype, dest + /*`"`*/1, len);
			if (! SQL_SUCCEEDED(ret)) {
				return ret;
			}
			break;

		default:
			BUGH(stmt, "can't convert SQL C type %hd to timestamp.",
				get_rec_c_type(arec, irec));
			RET_HDIAG(stmt, SQL_STATE_HY000, "bug converting parameter", 0);
	}

	/* apply corrections depending on the (column) size and decimal digits
	 * values given at binding time: nullify or trim the resulted string:
	 * https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/column-size
	 * */
	colsize = get_param_size(irec);
	DBGH(stmt, "requested column size: %llu.", colsize);
	if (colsize) {
		if (colsize < sizeof("yyyy-mm-dd hh:mm") - 1 ||
			colsize == 17 || colsize == 18 ) {
			ERRH(stmt, "invalid column size value: %llu; "
				"allowed: 16, 19, 20+f.", colsize);
			RET_HDIAGS(stmt, SQL_STATE_HY104);
		} else if (colsize == sizeof("yyyy-mm-dd hh:mm") - 1) {
			offt = sizeof("yyyy-mm-ddThh:mm:") - 1;
			offt += /*leading `"`*/1;
			dest[offt ++] = '0';
			dest[offt ++] = '0';
			dest[offt ++] = 'Z';
			*len = offt;
		} else if (colsize == sizeof("yyyy-mm-dd hh:mm:ss") - 1) {
			offt = sizeof("yyyy-mm-ddThh:mm:ss") - 1;
			offt += /*leading `"`*/1;
			dest[offt ++] = 'Z';
			*len = offt;
		} else {
			assert(20 < colsize);
			decdigits = get_param_decdigits(irec);
			DBGH(stmt, "requested decimal digits: %llu.", decdigits);
			if (/*count of fractions in ISO8601 template*/7 < decdigits) {
				INFOH(stmt, "decimal digits value (%hd) reset to 7.");
				decdigits = 7;
			} else if (decdigits == 0) {
				decdigits = -1; /* shave the `.` away */
			}
			if (colsize < decdigits + sizeof("yyyy-mm-ddThh:mm:ss.") - 1) {
				decdigits = colsize - (sizeof("yyyy-mm-ddThh:mm:ss.") - 1);
				WARNH(stmt, "column size adjusted to %hd to fit into a %llu"
					" columns size.", decdigits, colsize);
			}
			offt = sizeof("yyyy-mm-ddThh:mm:ss.") - 1;
			offt += /*leading `"`*/1;
			offt += decdigits;
			dest[offt ++] = 'Z';
			*len = offt;
		}
	} else {
		WARNH(stmt, "column size given 0 -- column size check skipped");
		(*len) ++; /* initial `"` */
	}

	dest[(*len) ++] = '"';
	return SQL_SUCCESS;
}


static SQLRETURN c2sql_cstr2qstr(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, char *dest, size_t *len)
{
	void *data_ptr;
	SQLLEN *octet_len_ptr, cnt;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;

	/* pointer to read from how many bytes we have */
	octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR, pos, arec);
	/* pointer to app's buffer */
	data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, arec);

	cnt = octet_len_ptr ? *octet_len_ptr : strlen((char *)data_ptr);

	if (dest) {
		*dest = '"';
	} else if ((SQLLEN)get_param_size(irec) < cnt) {
		ERRH(stmt, "string's length (%lld) longer than parameter size (%llu).",
			cnt, get_param_size(irec));
		RET_HDIAGS(stmt, SQL_STATE_22001);
	}

	*len = json_escape((char *)data_ptr, cnt, dest + !!dest, SIZE_MAX);

	if (dest) {
		dest[*len + /*1st `"`*/1] = '"';
	}

	*len += /*`"`*/2;

	return SQL_SUCCESS;
}

static SQLRETURN c2sql_wstr2qstr(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, char *dest, size_t *len)
{
	void *data_ptr;
	SQLLEN *octet_len_ptr, cnt, octets;
	int err;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;

	/* pointer to read from how many bytes we have */
	octet_len_ptr = deferred_address(SQL_DESC_OCTET_LENGTH_PTR, pos, arec);
	/* pointer to app's buffer */
	data_ptr = deferred_address(SQL_DESC_DATA_PTR, pos, arec);

	cnt = octet_len_ptr ? *octet_len_ptr : wcslen((wchar_t *)data_ptr);

	if (dest) {
		*dest = '"';
	} else {
		if ((SQLLEN)get_param_size(irec) < cnt) {
			ERRH(stmt, "string's length (%lld) longer than parameter "
				"size (%llu).", cnt, get_param_size(irec));
			RET_HDIAGS(stmt, SQL_STATE_22001);
		}
	}

	DBGH(stmt, "converting w-string [%lld] `" LWPDL "`; target@0x%p.",
		cnt, cnt, (wchar_t *)data_ptr, dest);
	if (cnt) { /* WCS2U8 will fail with empty string */
		SetLastError(0);
		octets = WCS2U8((wchar_t *)data_ptr, (int)cnt, dest + !!dest,
				dest ? INT_MAX : 0);
		if ((err = GetLastError())) {
			ERRH(stmt, "converting to multibyte string failed: %d", err);
			RET_HDIAGS(stmt, SQL_STATE_HY000);
		}
	} else {
		octets = 0;
	}
	assert(0 <= octets); /* buffer might be empty, so 0 is valid */
	*len = (size_t)octets;

	if (dest) {
		/* last param - buffer len - is calculated as if !dest */
		*len = json_escape_overlapping(dest + /*1st `"`*/1, octets,
				JSON_ESC_SEQ_SZ * octets);
		dest[*len + /*1st `"`*/1] = '"';
	} else {
		/* UCS*-to-UTF8 converted buffer is not yet available, so an accurate
		 * estimation of how long the JSON-escaping would take is not possible
		 * => estimate a worst case: 6x */
		*len *= JSON_ESC_SEQ_SZ;
	}

	*len += /*2x `"`*/2;

	return SQL_SUCCESS;
}

static SQLRETURN c2sql_number2qstr(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, char *dest, size_t *len)
{
	SQLRETURN ret;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;

	if (dest) {
		*dest = '"';
	}

	ret = c2sql_number(arec, irec, pos, NULL,NULL, 0, dest + !!dest, len);

	if (dest) {
		/* compare lengths only once number has actually been converted */
		if (get_param_size(irec) < *len) {
			ERRH(stmt, "converted number length (%zu) larger than parameter "
				"size (%llu)", *len, get_param_size(irec));
			RET_HDIAGS(stmt, SQL_STATE_22003);
		}
		dest[*len + /*1st `"`*/1] = '"';
	}
	*len += /*2x `"`*/2;

	return ret;
}

static SQLRETURN c2sql_varchar(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, char *dest, size_t *len)
{
	SQLSMALLINT ctype;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;

	switch ((ctype = get_rec_c_type(arec, irec))) {
		case SQL_C_CHAR:
			return c2sql_cstr2qstr(arec, irec, pos, dest, len);
		case SQL_C_WCHAR:
			return c2sql_wstr2qstr(arec, irec, pos, dest, len);

		case SQL_C_BINARY:
			// XXX: json_escape
			ERRH(stmt, "conversion from SQL C BINARY not implemented.");
			RET_HDIAG(stmt, SQL_STATE_HYC00, "conversion from SQL C BINARY "
				"not yet supported", 0);
			break;

		case SQL_C_TINYINT:
		case SQL_C_STINYINT:
		case SQL_C_SHORT:
		case SQL_C_SSHORT:
		case SQL_C_LONG:
		case SQL_C_SLONG:
		case SQL_C_SBIGINT:

		case SQL_C_BIT:
		case SQL_C_UTINYINT:
		case SQL_C_USHORT:
		case SQL_C_ULONG:
		case SQL_C_UBIGINT:

		case SQL_C_FLOAT:
		case SQL_C_DOUBLE:
		case SQL_C_NUMERIC:
			return c2sql_number2qstr(arec, irec, pos, dest, len);

		case SQL_C_TYPE_DATE:
		case SQL_C_TYPE_TIME:
		case SQL_C_TYPE_TIMESTAMP:
			// TODO: leave it a timestamp, or actual DATE/TIME/TS?
			return c2sql_timestamp(arec, irec, pos, dest, len);

		// case SQL_C_GUID:
		default:
			BUGH(stmt, "can't convert SQL C type %hd to timestamp.",
				get_rec_c_type(arec, irec));
			RET_HDIAG(stmt, SQL_STATE_HY000, "bug converting parameter", 0);
	}
}

/* Converts parameter values to string, JSON-escaping where necessary
 * The conversion is actually left to the server: the driver will use the
 * C source data type as value (at least for numbers) and specify what ES/SQL
 * type that value should be converted to. */
static SQLRETURN convert_param_val(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, char *dest, size_t *len)
{
	SQLLEN *ind_ptr;
	double min, max;
	BOOL fixed;
	esodbc_stmt_st *stmt = arec->desc->hdr.stmt;

	ind_ptr = deferred_address(SQL_DESC_INDICATOR_PTR, pos, arec);
	if (ind_ptr && *ind_ptr == SQL_NULL_DATA) {
		return c2sql_null(arec, irec, dest, len);
	}
	/* from here on, "input parameter value[ is] non-NULL" */
	assert(deferred_address(SQL_DESC_DATA_PTR, pos, arec));

	/*INDENT-OFF*/
	switch (irec->es_type->data_type) {
		/* JSON null */
		case SQL_UNKNOWN_TYPE: /* NULL */
			return c2sql_null(arec, irec, dest, len);

		case ESODBC_SQL_BOOLEAN: /* BOOLEAN */
			return c2sql_boolean(arec, irec, pos, dest, len);

		do {
		/* JSON long */
		case SQL_BIGINT: /* LONG */
			min = (double)LLONG_MIN; /* precision warnings */
			max = (double)LLONG_MAX;
			fixed = TRUE;
			break;
		case SQL_INTEGER: /* INTEGER */
			min = LONG_MIN;
			max = LONG_MAX;
			fixed = TRUE;
			break;
		case SQL_SMALLINT: /* SHORT */
			min = SHRT_MIN;
			max = SHRT_MAX;
			fixed = TRUE;
		case SQL_TINYINT: /* BYTE */
			min = CHAR_MIN;
			max = CHAR_MAX;
			fixed = TRUE;
			break;

		/* JSON double */
			// TODO: check accurate limits for floats in ES/SQL
		case SQL_REAL: /* FLOAT */
			min = FLT_MIN;
			max = FLT_MAX;
			fixed = FALSE;
			break;
		case SQL_FLOAT: /* HALF_FLOAT, SCALED_FLOAT */
		case SQL_DOUBLE: /* DOUBLE */
			min = DBL_MIN;
			max = DBL_MAX;
			fixed = FALSE;
			break;
		} while (0);
			return c2sql_number(arec, irec, pos, &min, &max, fixed, dest, len);

		/* JSON string */
		case SQL_VARCHAR: /* KEYWORD, TEXT */
			return c2sql_varchar(arec, irec, pos, dest, len);

		case SQL_TYPE_TIMESTAMP: /* DATE */
			return c2sql_timestamp(arec, irec, pos, dest, len);

		/* JSON (Base64 encoded) string */
		case SQL_VARBINARY: /* BINARY */
			// XXX: json_escape
			ERRH(stmt, "conversion to SQL BINARY not implemented.");
			RET_HDIAG(stmt, SQL_STATE_HYC00, "conversion to SQL BINARY "
				"not yet supported", 0);
			break;

		default:
			BUGH(arec->desc->hdr.stmt, "unexpected ES/SQL type %hd.",
				irec->es_type->data_type);
			RET_HDIAG(arec->desc->hdr.stmt, SQL_STATE_HY000,
				"parameter conversion bug", 0);
	}
	/*INDENT-ON*/

	return SQL_SUCCESS;
}


/* Forms the JSON array with params:
 * [{"type": "<ES/SQL type name>", "value": <param value>}(,etc)*] */
static SQLRETURN serialize_params(esodbc_stmt_st *stmt, char *dest,
	size_t *len)
{
	/* JSON keys for building one parameter object */
#	define JSON_KEY_TYPE	"{\"type\": \""
#	define JSON_KEY_VALUE	"\", \"value\": "

	esodbc_rec_st *arec, *irec;
	SQLRETURN ret;
	SQLSMALLINT i;
	size_t l, pos;

	pos = 0;
	if (dest) {
		dest[pos] = '[';
	}
	pos ++;

	for (i = 0; i < stmt->apd->count; i ++) {
		arec = get_record(stmt->apd, i + 1, /*grow?*/FALSE);
		irec = get_record(stmt->ipd, i + 1, /*grow?*/FALSE);
		assert(arec && irec && irec->es_type);

		if (dest) {
			if (i) {
				memcpy(dest + pos, ", ", 2);
			}
			/* copy 'type' JSON key name */
			memcpy(dest + pos + 2 * !!i, JSON_KEY_TYPE,
				sizeof(JSON_KEY_TYPE) - 1);
		}
		pos += 2 * !!i + sizeof(JSON_KEY_TYPE) - 1;

		/* copy/eval ES/SQL type name */
		pos += json_escape(irec->es_type->type_name_c.str,
				irec->es_type->type_name_c.cnt, dest ? dest + pos : NULL,
				/*"unlimited" output buffer*/(size_t)-1);

		if (dest) {
			/* copy 'value' JSON key name */
			memcpy(dest + pos, JSON_KEY_VALUE, sizeof(JSON_KEY_VALUE) - 1);
		}
		pos += sizeof(JSON_KEY_VALUE) - 1;

		/* copy converted parameter value */
		ret = convert_param_val(arec, irec, /*params array pos*/0LLU,
				dest ? dest + pos : NULL, &l);
		if (SQL_SUCCEEDED(ret)) {
			pos += l;
		} else {
			ERRH(stmt, "converting parameter #%hd failed.", i + 1);
			return ret;
		}

		if (dest) {
			dest[pos] = '}';
		}
		pos ++;
	}

	if (dest) {
		dest[pos] = ']';
	}
	pos ++;

	*len = pos;
	return SQL_SUCCESS;

#	undef JSON_KEY_TYPE
#	undef JSON_KEY_VALUE
}

/*
 * Build a serialized JSON object out of the statement.
 * If resulting string fits into the given buff, the result is copied in it;
 * othewise a new one will be allocated and returned.
 */
SQLRETURN TEST_API serialize_statement(esodbc_stmt_st *stmt, cstr_st *buff)
{
	/* JSON body build elements */
#	define JSON_KEY_QUERY		"\"query\": " /* will always be the 1st key */
#	define JSON_KEY_CURSOR		"\"cursor\": " /* 1st key */
#	define JSON_KEY_PARAMS		", \"params\": " /* n-th key */
#	define JSON_KEY_FETCH		", \"fetch_size\": " /* n-th key */
#	define JSON_KEY_REQ_TOUT	", \"request_timeout\": " /* n-th key */
#	define JSON_KEY_PAGE_TOUT	", \"page_timeout\": " /* n-th key */
#	define JSON_KEY_TIME_ZONE	", \"time_zone\": " /* n-th key */

	SQLRETURN ret = SQL_SUCCESS;
	size_t bodylen, pos, u8len, len;
	char *body;
	char u8curs[ESODBC_BODY_BUF_START_SIZE];
	esodbc_dbc_st *dbc = stmt->hdr.dbc;
	esodbc_desc_st *apd = stmt->apd;

	/* TODO: move escaping/x-coding (to JSON or CBOR) in attach_sql() and/or
	 * attach_answer() to avoid these operations for each execution of the
	 * statement (especially for the SQL statement; the cursor might not
	 * always be used - if app decides to no longer fetch - but would then
	 * clean this function). */

	/* enforced in EsSQLSetDescFieldW(SQL_DESC_ARRAY_SIZE) */
	assert(apd->array_size <= 1);

	bodylen = 1; /* { */
	/* evaluate how long the stringified REST object will be */
	if (stmt->rset.eccnt) { /* eval CURSOR object length */
		/* convert cursor to C [mb]string. */
		/* TODO: ansi_w2c() fits better for Base64 encoded cursors. */
		u8len = WCS2U8(stmt->rset.ecurs, (int)stmt->rset.eccnt, u8curs,
				sizeof(u8curs));
		if (u8len <= 0) {
			ERRH(stmt, "failed to convert cursor `" LWPDL "` to UTF8: %d.",
				stmt->rset.eccnt, stmt->rset.ecurs, WCS2U8_ERRNO());
			RET_HDIAGS(stmt, SQL_STATE_24000);
		}

		bodylen += sizeof(JSON_KEY_CURSOR) - 1; /* "cursor":  */
		bodylen += json_escape(u8curs, u8len, NULL, 0);
		bodylen += 2; /* 2x `"` for cursor value */
		/* TODO: page_timeout */
	} else { /* eval QUERY object length */
		bodylen += sizeof(JSON_KEY_QUERY) - 1;
		bodylen += json_escape(stmt->u8sql.str, stmt->u8sql.cnt, NULL, 0);
		bodylen += 2; /* 2x `"` for query value */

		/* does the statement have any parameters? */
		if (apd->count) {
			bodylen += sizeof(JSON_KEY_PARAMS) - 1;
			/* serialize_params will count/copy array delimiters (`[`, `]`) */
			ret = serialize_params(stmt, /* don't copy, just eval len */NULL,
					&len);
			if (! SQL_SUCCEEDED(ret)) {
				ERRH(stmt, "failed to eval parameters length");
				return ret;
			}
			bodylen += len;
		}
		/* does the statement have any fetch_size? */
		if (dbc->fetch.slen) {
			bodylen += sizeof(JSON_KEY_FETCH) - /*\0*/1;
			bodylen += dbc->fetch.slen;
		}
	}
	bodylen += 1; /* } */

	/* allocate memory for the stringified buffer, if needed */
	if (buff->cnt < bodylen) {
		WARNH(dbc, "local buffer too small (%zd), need %zdB; will alloc.",
			buff->cnt, bodylen);
		WARNH(dbc, "local buffer too small, SQL: `" LCPDL "`.",
			LCSTR(&stmt->u8sql));
		body = malloc(bodylen);
		if (! body) {
			ERRNH(stmt, "failed to alloc %zdB.", bodylen);
			RET_HDIAGS(stmt, SQL_STATE_HY001);
		}
		buff->str = body;
	} else {
		body = buff->str;
	}

	pos = 0;
	body[pos ++] = '{';
	/* build the actual stringified JSON object */
	if (stmt->rset.eccnt) { /* copy CURSOR object */
		memcpy(body + pos, JSON_KEY_CURSOR, sizeof(JSON_KEY_CURSOR) - 1);
		pos += sizeof(JSON_KEY_CURSOR) - 1;
		body[pos ++] = '"';
		pos += json_escape(u8curs, u8len, body + pos, bodylen - pos);
		body[pos ++] = '"';
	} else { /* copy QUERY object */
		memcpy(body + pos, JSON_KEY_QUERY, sizeof(JSON_KEY_QUERY) - 1);
		pos += sizeof(JSON_KEY_QUERY) - 1;
		body[pos ++] = '"';
		pos += json_escape(stmt->u8sql.str, stmt->u8sql.cnt, body + pos,
				bodylen - pos);
		body[pos ++] = '"';

		/* does the statement have any parameters? */
		if (apd->count) {
			memcpy(body + pos, JSON_KEY_PARAMS, sizeof(JSON_KEY_PARAMS) - 1);
			pos += sizeof(JSON_KEY_PARAMS) - 1;
			/* serialize_params will count/copy array delimiters (`[`, `]`) */
			ret = serialize_params(stmt, body + pos, &len);
			if (! SQL_SUCCEEDED(ret)) {
				ERRH(stmt, "failed to serialize parameters");
				return ret;
			}
			pos += len;
		}
		/* does the statement have any fetch_size? */
		if (dbc->fetch.slen) {
			memcpy(body + pos, JSON_KEY_FETCH, sizeof(JSON_KEY_FETCH) - 1);
			pos += sizeof(JSON_KEY_FETCH) - 1;
			memcpy(body + pos, dbc->fetch.str, dbc->fetch.slen);
			pos += dbc->fetch.slen;
		}
	}
	body[pos ++] = '}';

	buff->cnt = pos;

	DBGH(stmt, "JSON serialized to: [%zd] `" LCPDL "`.", pos, LCSTR(buff));
	return ret;

#	undef JSON_KEY_QUERY
#	undef JSON_KEY_CURSOR
#	undef JSON_KEY_PARAMS
#	undef JSON_KEY_FETCH
#	undef JSON_KEY_REQ_TOUT
#	undef JSON_KEY_PAGE_TOUT
#	undef JSON_KEY_TIME_ZONE
}


/*
 * "In the IPD, this header field points to a parameter status array
 * containing status information for each set of parameter values after a call
 * to SQLExecute or SQLExecDirect." = .array_status_ptr
 *
 * "In the APD, this header field points to a parameter operation array of
 * values that can be set by the application to indicate whether this set of
 * parameters is to be ignored when SQLExecute or SQLExecDirect is called."
 * = .array_status_ptr
 * "If no elements of the array are set, all sets of parameters in the array
 * are used in the SQLExecute or SQLExecDirect calls."
 *
 * "When the statement is executed, [...] the *ParameterValuePtr buffer must
 * contain a valid input value, or the *StrLen_or_IndPtr buffer must contain
 * SQL_NULL_DATA, SQL_DATA_AT_EXEC, or the result of the SQL_LEN_DATA_AT_EXEC
 * macro."
 *
 * APD.SQL_DESC_BIND_OFFSET_PTR: "[i]f the field is non-null, the driver
 * dereferences the pointer and, if none of the values in the
 * SQL_DESC_DATA_PTR, SQL_DESC_INDICATOR_PTR, and SQL_DESC_OCTET_LENGTH_PTR
 * fields is a null pointer, adds the dereferenced value to those fields in
 * the descriptor records at execution time."
 *
 * APD.SQL_DESC_ARRAY_STATUS_PTR: "SQL_PARAM_IGNORE, to indicate that the row
 * is excluded from statement execution" and, "[i]n addition [...] the
 * following codes cause a parameter in an SQL statement to be ignored:
 * SQL_ROW_DELETED, SQL_ROW_UPDATED, and SQL_ROW_ERROR."; the opposite is:
 * SQL_ROW_PROCEED, SQL_ROW_SUCCESS, SQL_ROW_SUCCESS_WITH_INFO, and
 * SQL_ROW_ADDED.
 */
SQLRETURN EsSQLExecute(SQLHSTMT hstmt)
{
	SQLRETURN ret;
	esodbc_stmt_st *stmt = STMH(hstmt);
	char buff[ESODBC_BODY_BUF_START_SIZE];
	cstr_st body = {buff, sizeof(buff)};

	DBGH(stmt, "executing SQL: [%zd] `" LCPDL "`.", stmt->u8sql.cnt,
		LCSTR(&stmt->u8sql));

	ret = serialize_statement(stmt, &body);
	if (SQL_SUCCEEDED(ret)) {
		ret = post_json(stmt, &body);
	}

	if (buff != body.str) {
		free(body.str);
	}

	return ret;
}

/*
 * "In the IPD, this header field points to a parameter status array
 * containing status information for each set of parameter values after a call
 * to SQLExecute or SQLExecDirect." = .array_status_ptr
 *
 * "In the APD, this header field points to a parameter operation array of
 * values that can be set by the application to indicate whether this set of
 * parameters is to be ignored when SQLExecute or SQLExecDirect is called."
 * = .array_status_ptr
 * "If no elements of the array are set, all sets of parameters in the array
 * are used in the SQLExecute or SQLExecDirect calls."
 */
SQLRETURN EsSQLExecDirectW
(
	SQLHSTMT    hstmt,
	_In_reads_opt_(TextLength) SQLWCHAR *szSqlStr,
	SQLINTEGER cchSqlStr
)
{
	esodbc_stmt_st *stmt = STMH(hstmt);
	SQLRETURN ret;

	if (cchSqlStr == SQL_NTS) {
		cchSqlStr = (SQLINTEGER)wcslen(szSqlStr);
	} else if (cchSqlStr <= 0) {
		ERRH(stmt, "invalid statment length: %d.", cchSqlStr);
		RET_HDIAGS(stmt, SQL_STATE_HY090);
	}
	DBGH(stmt, "directly executing SQL: `" LWPDL "` [%d].", cchSqlStr,
		szSqlStr, cchSqlStr);

	ret = EsSQLFreeStmt(stmt, ESODBC_SQL_CLOSE);
	assert(SQL_SUCCEEDED(ret)); /* can't return error */

	/* no support for parameter arrays (yet) */
	// & param marker is set!
	assert(stmt->apd->array_size <= 1);

	ret = attach_sql(stmt, szSqlStr, cchSqlStr);
	if (SQL_SUCCEEDED(ret)) {
		ret = EsSQLExecute(stmt);
	}
#ifndef NDEBUG
	/* no reason to keep it (it can't be re-executed), except for debugging */
	detach_sql(stmt);
#endif /* NDEBUG */
	return ret;
}

static inline SQLULEN get_col_size(esodbc_rec_st *rec)
{
	assert(DESC_TYPE_IS_IMPLEMENTATION(rec->desc->type));

	switch (rec->meta_type) {
		case METATYPE_EXACT_NUMERIC:
		case METATYPE_FLOAT_NUMERIC:
			return rec->es_type->column_size;

		case METATYPE_STRING:
		case METATYPE_BIN:
		case METATYPE_DATETIME:
		case METATYPE_INTERVAL_WSEC:
		case METATYPE_INTERVAL_WOSEC:
		case METATYPE_BIT:
			return rec->length;

		case METATYPE_UID:
			BUGH(rec->desc, "unsupported data c-type: %d.", rec->concise_type);
	}
	/*
	 * https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqldescribecol-function#arguments :
	 * "If the column size cannot be determined, the driver returns 0." */
	return 0;
}

static inline SQLSMALLINT get_col_decdigits(esodbc_rec_st *rec)
{
	assert(DESC_TYPE_IS_IMPLEMENTATION(rec->desc->type));

	switch (rec->meta_type) {
		case METATYPE_DATETIME:
		case METATYPE_INTERVAL_WSEC:
			/* TODO: pending GH#30002 actually */
			return 3;

		case METATYPE_EXACT_NUMERIC:
			return rec->es_type->maximum_scale;
	}
	/* 0 to be returned for unknown case:
	 * https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqldescribecol-function#syntax
	 */
	return 0;
}

SQLRETURN EsSQLDescribeColW(
	SQLHSTMT            hstmt,
	SQLUSMALLINT        icol,
	_Out_writes_opt_(cchColNameMax)
	SQLWCHAR            *szColName,
	SQLSMALLINT         cchColNameMax,
	_Out_opt_
	SQLSMALLINT        *pcchColName,
	_Out_opt_
	SQLSMALLINT        *pfSqlType,
	_Out_opt_
	SQLULEN            *pcbColDef,
	_Out_opt_
	SQLSMALLINT        *pibScale,
	_Out_opt_
	SQLSMALLINT        *pfNullable)
{
	esodbc_stmt_st *stmt = STMH(hstmt);
	esodbc_rec_st *rec;
	SQLRETURN ret;
	SQLSMALLINT col_blen;

	DBGH(stmt, "IRD@0x%p, column #%d.", stmt->ird, icol);

	if (! STMT_HAS_RESULTSET(stmt)) {
		ERRH(stmt, "no resultset available on statement.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	if (icol < 1) {
		/* TODO: if implementing bookmarks */
		RET_HDIAGS(stmt, SQL_STATE_HYC00);
	}

	rec = get_record(stmt->ird, icol, FALSE);
	if (! rec) {
		ERRH(stmt, "no record for columns #%d.", icol);
		RET_HDIAGS(stmt, SQL_STATE_07009);
	}
#ifndef NDEBUG
	//dump_record(rec);
#endif /* NDEBUG */

	if (szColName) {
		ret = write_wstr(stmt, szColName, &rec->name,
				cchColNameMax * sizeof(*szColName), &col_blen);
		if (! SQL_SUCCEEDED(ret)) {
			ERRH(stmt, "failed to copy column name `" LWPDL "`.",
				LWSTR(&rec->name));
			return ret;
		}
	} else {
		DBGH(stmt, "NULL column name buffer provided.");
		col_blen = -1;
	}

	if (! pcchColName) {
		ERRH(stmt, "no column name length buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090,
			"no column name length buffer provided", 0);
	}
	*pcchColName = 0 <= col_blen ? (col_blen / sizeof(*szColName)) :
		(SQLSMALLINT)rec->name.cnt;
	DBGH(stmt, "col #%d name has %d chars.", icol, *pcchColName);

	if (! pfSqlType) {
		ERRH(stmt, "no column data type buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090,
			"no column data type buffer provided", 0);
	}
	*pfSqlType = rec->concise_type;
	DBGH(stmt, "col #%d has concise type=%d.", icol, *pfSqlType);

	if (! pcbColDef) {
		ERRH(stmt, "no column size buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090, "no column size buffer provided", 0);
	}
	*pcbColDef = get_col_size(rec);
	DBGH(stmt, "col #%d of meta type %d has size=%llu.",
		icol, rec->meta_type, *pcbColDef);

	if (! pibScale) {
		ERRH(stmt, "no column decimal digits buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090,
			"no column decimal digits buffer provided", 0);
	}
	*pibScale = get_col_decdigits(rec);
	DBGH(stmt, "col #%d of meta type %d has decimal digits=%d.",
		icol, rec->meta_type, *pibScale);

	if (! pfNullable) {
		ERRH(stmt, "no column decimal digits buffer provided.");
		RET_HDIAG(stmt, SQL_STATE_HY090,
			"no column decimal digits buffer provided", 0);
	}
	ASSERT_IXD_HAS_ES_TYPE(rec);
	/* TODO: this would be available in SQLColumns resultset. */
	*pfNullable = rec->es_type->nullable;
	DBGH(stmt, "col #%d nullable=%d.", icol, *pfNullable);

	return SQL_SUCCESS;
}


SQLRETURN EsSQLColAttributeW(
	SQLHSTMT        hstmt,
	SQLUSMALLINT    iCol,
	SQLUSMALLINT    iField,
	_Out_writes_bytes_opt_(cbDescMax)
	SQLPOINTER      pCharAttr, /* [out] value, if string; can be NULL */
	SQLSMALLINT     cbDescMax, /* [in] byte len of pCharAttr */
	_Out_opt_
	SQLSMALLINT     *pcbCharAttr, /* [out] len written in pCharAttr (w/o \0 */
	_Out_opt_
#ifdef _WIN64
	SQLLEN          *pNumAttr /* [out] value, if numeric */
#else /* _WIN64 */
	SQLPOINTER      pNumAttr
#endif /* _WIN64 */
)
{
	esodbc_stmt_st *stmt = STMH(hstmt);
	esodbc_desc_st *ird = stmt->ird;
	esodbc_rec_st *rec;
	SQLSMALLINT sint;
	wstr_st *wstrp;
	SQLLEN len;
	SQLINTEGER iint;

#ifdef _WIN64
#define PNUMATTR_ASSIGN(type, value) *pNumAttr = (SQLLEN)(value)
#else /* _WIN64 */
#define PNUMATTR_ASSIGN(type, value) *(type *)pNumAttr = (type)(value)
#endif /* _WIN64 */

	DBGH(stmt, "IRD@0x%p, column #%d, field: %d.", ird, iCol, iField);

	if (! STMT_HAS_RESULTSET(stmt)) {
		ERRH(stmt, "no resultset available on statement.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	if (iCol < 1) {
		/* TODO: if implementing bookmarks */
		RET_HDIAGS(stmt, SQL_STATE_HYC00);
	}

	rec = get_record(ird, iCol, FALSE);
	if (! rec) {
		ERRH(stmt, "no record for columns #%d.", iCol);
		RET_HDIAGS(stmt, SQL_STATE_07009);
	}

	ASSERT_IXD_HAS_ES_TYPE(rec);

	/*INDENT-OFF*/
	switch (iField) {
		/* SQLSMALLINT */
		do {
		case SQL_DESC_CONCISE_TYPE: sint = rec->concise_type; break;
		case SQL_DESC_TYPE: sint = rec->type; break;
		case SQL_DESC_UNNAMED: sint = rec->unnamed; break;
		case SQL_DESC_NULLABLE: sint = rec->es_type->nullable; break;
		case SQL_DESC_SEARCHABLE: sint = rec->es_type->searchable; break;
		case SQL_DESC_UNSIGNED: sint = rec->es_type->unsigned_attribute; break;
		case SQL_DESC_UPDATABLE: sint = rec->updatable; break;
		case SQL_DESC_PRECISION:
			sint = rec->es_type->fixed_prec_scale;
			break;
		case SQL_DESC_SCALE:
			sint = rec->es_type->maximum_scale;
			break;
		case SQL_DESC_FIXED_PREC_SCALE:
			sint = rec->es_type->fixed_prec_scale;
			break;
		} while (0);
			PNUMATTR_ASSIGN(SQLSMALLINT, sint);
			break;

		/* SQLWCHAR* */
		do {
		case SQL_DESC_BASE_COLUMN_NAME: wstrp = &rec->base_column_name; break;
		case SQL_DESC_LABEL: wstrp = &rec->label; break;
		case SQL_DESC_BASE_TABLE_NAME: wstrp = &rec->base_table_name; break;
		case SQL_DESC_CATALOG_NAME: wstrp = &rec->catalog_name; break;
		case SQL_DESC_NAME: wstrp = &rec->name; break;
		case SQL_DESC_SCHEMA_NAME: wstrp = &rec->schema_name; break;
		case SQL_DESC_TABLE_NAME: wstrp = &rec->table_name; break;
		case SQL_DESC_LITERAL_PREFIX:
			wstrp = &rec->es_type->literal_prefix;
			break;
		case SQL_DESC_LITERAL_SUFFIX:
			wstrp = &rec->es_type->literal_suffix;
			break;
		case SQL_DESC_LOCAL_TYPE_NAME:
			wstrp = &rec->es_type->type_name;
			break;
		case SQL_DESC_TYPE_NAME:
			wstrp = &rec->es_type->type_name;
			break;
		} while (0);
			return write_wstr(stmt, pCharAttr, wstrp, cbDescMax, pcbCharAttr);

		/* SQLLEN */
		do {
		case SQL_DESC_DISPLAY_SIZE: len = rec->es_type->display_size; break;
		case SQL_DESC_OCTET_LENGTH: len = rec->octet_length; break;
		} while (0);
			PNUMATTR_ASSIGN(SQLLEN, len);
			break;

		/* SQLULEN */
		case SQL_DESC_LENGTH:
			/* "This information is returned from the SQL_DESC_LENGTH record
			 * field of the IRD." */
			PNUMATTR_ASSIGN(SQLULEN, rec->length);
			break;

		/* SQLINTEGER */
		do {
		case SQL_DESC_AUTO_UNIQUE_VALUE:
			iint = rec->es_type->auto_unique_value;
			break;
		case SQL_DESC_CASE_SENSITIVE:
			iint = rec->es_type->case_sensitive;
			break;
		case SQL_DESC_NUM_PREC_RADIX: iint = rec->num_prec_radix; break;
		} while (0);
			PNUMATTR_ASSIGN(SQLINTEGER, iint);
			break;


		case SQL_DESC_COUNT:
			PNUMATTR_ASSIGN(SQLSMALLINT, ird->count);
			break;

		default:
			ERRH(stmt, "unknown field type %d.", iField);
			RET_HDIAGS(stmt, SQL_STATE_HY091);
	}
	/*INDENT-ON*/

	return SQL_SUCCESS;
#undef PNUMATTR_ASSIGN
}

/* function implementation is correct, but it can't really be used as
 * intended, since the driver's "preparation" doesn't really involve sending
 * it to ES or even parameter marker counting.
 * TODO: marker counting? (SQLDescribeParam would need ES/SQL support, tho) */
SQLRETURN EsSQLNumParams(
	SQLHSTMT           StatementHandle,
	_Out_opt_
	SQLSMALLINT       *ParameterCountPtr)
{
	esodbc_stmt_st *stmt = STMH(StatementHandle);

	if (! stmt->u8sql.cnt) {
		ERRH(stmt, "statement hasn't been prepared.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	return EsSQLGetDescFieldW(stmt->ipd, NO_REC_NR,
			SQL_DESC_COUNT, ParameterCountPtr, SQL_IS_SMALLINT, NULL);
}

SQLRETURN EsSQLRowCount(_In_ SQLHSTMT StatementHandle, _Out_ SQLLEN *RowCount)
{
	esodbc_stmt_st *stmt = STMH(StatementHandle);

	if (! STMT_HAS_RESULTSET(stmt)) {
		ERRH(stmt, "no resultset available on statement.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	DBGH(stmt, "current resultset rows count: %zd.", stmt->rset.nrows);
	*RowCount = (SQLLEN)stmt->rset.nrows;

	if (stmt->rset.eccnt) {
		/* fetch_size or scroller size chunks the result */
		WARNH(stmt, "this function will only return the row count of the "
			"partial result set available.");
		/* returning a _WITH_INFO here will fail the query for MSQRY32.. */
		//RET_HDIAG(stmt, SQL_STATE_01000, "row count is for partial result "
		//		"only", 0);
	}
	return SQL_SUCCESS;
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
