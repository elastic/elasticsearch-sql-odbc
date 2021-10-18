/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <float.h>

#include <cborinternal_p.h> /* for decode_half() */

#include "queries.h"
#include "log.h"
#include "connect.h"
#include "info.h"
#include "convert.h"

/* key names used in Elastic/SQL REST/JSON answers */
#define PACK_PARAM_COLUMNS		"columns"
#define PACK_PARAM_ROWS			"rows"
#define PACK_PARAM_CURSOR		"cursor"
#define PACK_PARAM_STATUS		"status"
#define PACK_PARAM_ERROR		"error"
#define PACK_PARAM_ERR_TYPE		"type"
#define PACK_PARAM_ERR_REASON	"reason"
#define PACK_PARAM_ERR_RCAUSE	"root_cause"
#define PACK_PARAM_COL_NAME		"name"
#define PACK_PARAM_COL_TYPE		"type"
#define PACK_PARAM_COL_DSIZE	"display_size"
#define PACK_PARAM_CURS_CLOSE	"succeeded"

#define MSG_INV_SRV_ANS		"Invalid server answer"

/* Macro valid for *_cbor() functions only.
 * Assumes presence of a variable 'res' of type CborError in used scope and a
 * label named 'err'. */
#define CHK_RES(_hnd, _fmt, ...) \
	JUMP_ON_CBOR_ERR(res, err, _hnd, _fmt, __VA_ARGS__)

/* fwd decl */
static SQLRETURN statement_params_len_cbor(esodbc_stmt_st *stmt,
	size_t *enc_len, size_t *conv_len);
static SQLRETURN count_param_markers(esodbc_stmt_st *stmt, SQLSMALLINT *p_cnt);

static thread_local cstr_st tz_param;
static cstr_st version = CSTR_INIT(STR(DRV_VERSION)); /* build-time define */

static BOOL print_tz_param(long tz_dst_offt)
{
	static char time_zone[sizeof("\"-05:45\"")];
	int n;
	long abs_tz;

	abs_tz = (tz_dst_offt < 0) ? -tz_dst_offt : tz_dst_offt;

	n = snprintf(time_zone, sizeof(time_zone), "\"%c%02ld:%02ld\"",
			/* negative offset means ahead of UTC -> '+' */
			tz_dst_offt <= 0 ? '+' : '-',
			abs_tz / 3600, (abs_tz % 3600) / 60);

	if (n <= 0 || sizeof(time_zone) <= n) {
		ERRN("failed to print timezone param for offset: %ld.", tz_dst_offt);
		return FALSE;
	}
	tz_param.str = time_zone;
	tz_param.cnt = n;
	DBG("timezone request parameter updated to: `" LCPDL "` (where "
		ESODBC_DSN_APPLY_TZ ").", LCSTR(&tz_param));
	return TRUE;
}

/* Returns total timezone plus daylight saving offset */
static BOOL get_tz_dst_offset(long *_tz_dst_offt, struct tm *now)
{
	static char *tz = NULL;
	struct tm *tm, local, gm;
	long tz_dst_offt;
	time_t utc;

	utc = time(NULL);
	if (utc == (time_t)-1) {
		ERRN("failed to read current time.");
		return FALSE;
	}
	tm = localtime(&utc);
	assert(tm); /* value returned by time() should always be valid */
	local = *tm;
	tm = gmtime(&utc);
	assert(tm);
	gm = *tm;

	/* calculating the offset only works if the DST won't occur on year
	 * end/start, which should be a safe assumption */
	tz_dst_offt = gm.tm_yday * 24 * 3600 + gm.tm_hour * 3600 + gm.tm_min * 60;
	tz_dst_offt -= local.tm_yday * 24 * 3600;
	tz_dst_offt -= local.tm_hour * 3600 + local.tm_min * 60;

	if (! tz) {
		tz = getenv(ESODBC_TZ_ENV_VAR);
		INFO("time offset (timezone%s): %ld seconds, "
			"TZ: `%s`, standard: `%s`, daylight: `%s`.",
			local.tm_isdst ? "+DST" : "", tz_dst_offt,
			tz ? tz : "<not set>", _tzname[0], _tzname[1]);
		/* TZ allows :ss specification, but that can't be sent to ES */
		if (local.tm_sec != gm.tm_sec) {
			ERR("sub-minute timezone offsets are not supported.");
			return FALSE;
		}
		if (_tzname[1] && _tzname[1][0]) {
			WARN("DST calculation works with 'TZ' only for US timezones. "
				"No 'TZ' validation is performed by the driver!");
		}
		if (! tz) {
			tz = (char *)0x1; /* only execute this block once */
		}
	}

	if (now) {
		*now = local;
	}
	*_tz_dst_offt = tz_dst_offt;
	return TRUE;
}

static inline BOOL update_tz_param()
{
	/* offset = 1 -- impossible value -> trigger an update */
	static thread_local long tz_dst_offt = 1;
	static thread_local int tm_yday = -1;
	long offset;
	struct tm now;

	if (! get_tz_dst_offset(&offset, &now)) {
		return FALSE;
	}
	if (tz_dst_offt == offset && tm_yday == now.tm_yday) {
		/* nothing changed, previously set values can be reused */
		return TRUE;
	} else {
		tz_dst_offt = offset;
		tm_yday = now.tm_yday;
	}
	return print_tz_param(tz_dst_offt) && update_crr_date(&now);
}

BOOL queries_init()
{
	char *ptr;

	/* for the casts in this module */
	ASSERT_INTEGER_TYPES_EQUAL(wchar_t, SQLWCHAR);
	ASSERT_INTEGER_TYPES_EQUAL(char, SQLCHAR);

	/* trim qualifiers */
	ptr = strchr(version.str, '-');
	if (ptr) {
		version.cnt = ptr - version.str;
	}

	/* needed to correctly run the unit tests */
	return update_tz_param();
}

void clear_resultset(esodbc_stmt_st *stmt, BOOL on_close)
{
	INFOH(stmt, "clearing result set #%zu, visited rows in set: %zu.",
		stmt->nset, stmt->rset.vrows);
	if (stmt->rset.body.str) {
		assert(stmt->rset.body.cnt);
		free(stmt->rset.body.str);
	}
	if (stmt->rset.pack_json) {
		if (stmt->rset.pack.json.state) {
			UJFree(stmt->rset.pack.json.state);
		}
	} else {
		if (stmt->rset.pack.cbor.cols_buff.cnt) {
			assert(stmt->rset.pack.cbor.cols_buff.str);
			free(stmt->rset.pack.cbor.cols_buff.str);
		} else {
			assert(! stmt->rset.pack.cbor.cols_buff.str);
		}
		if (stmt->rset.pack.cbor.curs_allocd) {
			free(stmt->rset.pack.cbor.curs.str);
			stmt->rset.pack.cbor.curs.str = NULL;
			stmt->rset.pack.cbor.curs_allocd = false;
		} else if (stmt->rset.pack.cbor.curs.str) {
			/* the cursor is contained entirely in the received body */
			assert(stmt->rset.body.str < stmt->rset.pack.cbor.curs.str); // &&
			assert(stmt->rset.pack.cbor.curs.str +
				stmt->rset.pack.cbor.curs.cnt <
				stmt->rset.body.str + stmt->rset.body.cnt);
		}
	}
	memset(&stmt->rset, 0, sizeof(stmt->rset));

	if (on_close) {
		INFOH(stmt, "on close, total visited rows: %zu.", stmt->tv_rows);
		STMT_ROW_CNT_RESET(stmt);
	}

	/* reset SQLGetData state to detect sequence "SQLExec*(); SQLGetData();" */
	STMT_GD_RESET(stmt);
}

/* Set the descriptor fields associated with "size". This step is needed since
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
		case METATYPE_BIT:
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
		case METATYPE_DATE_TIME:
			/* "number of characters in the character representation" */
			rec->length = rec->es_type->column_size;
			break;

		case METATYPE_INTERVAL_WSEC:
		case METATYPE_INTERVAL_WOSEC:
			rec->length = rec->es_type->display_size;
			break;

		default:
			BUGH(rec->desc, "unsupported data c-type: %d.", rec->concise_type);
	}
}

/* Note: col_name/_type need to reference pre-allocated str. objects. */
static BOOL attach_one_column(esodbc_rec_st *rec, wstr_st *col_name,
	wstr_st *col_type) // TODO: disp size
{
	size_t i;
	static const wstr_st EMPTY_WSTR = WSTR_INIT("");
	esodbc_stmt_st *stmt;
	esodbc_dbc_st *dbc;

	/* uncounted 0-term is present */
	assert(col_name->str[col_name->cnt] == '\0');
	assert(col_type->str[col_type->cnt] == '\0');

	stmt = HDRH(rec->desc)->stmt;
	dbc = HDRH(stmt)->dbc;

	rec->name = col_name->cnt ? *col_name : EMPTY_WSTR;

	assert(! rec->es_type);
	/* lookup the DBC-cached ES type */
	for (i = 0; i < dbc->no_types; i ++) {
		if (EQ_CASE_WSTR(&dbc->es_types[i].type_name, col_type)) {
			rec->es_type = &dbc->es_types[i];
			break;
		}
	}
	if (rec->es_type) {
		/* copy fields pre-calculated at DB connect time */
		rec->concise_type = rec->es_type->data_type;
		rec->type = rec->es_type->sql_data_type;
		rec->datetime_interval_code = rec->es_type->sql_datetime_sub;
		rec->meta_type = rec->es_type->meta_type;
		/* set INTERVAL record's seconds precision */
		if (rec->meta_type == METATYPE_INTERVAL_WSEC) {
			assert(rec->precision == 0);
			rec->precision = rec->es_type->maximum_scale;
		}
	} else if (! dbc->no_types) {
		/* the connection doesn't have yet the types cached (this is the
		 * caching call) and don't have access to the data itself either,
		 * just the column names & type names => set unknowns.  */
		rec->concise_type = SQL_UNKNOWN_TYPE;
		rec->type = SQL_UNKNOWN_TYPE;
		rec->datetime_interval_code = 0;
		rec->meta_type = METATYPE_UNKNOWN;
	} else {
		ERRH(stmt, "type lookup failed for `" LWPDL "`.", LWSTR(col_type));
		return FALSE;
	}

	set_col_size(rec);

	/* setting the remaining of settable fields (base table etc.) requires
	 * server side changes => set them to "" */

	/* "If a base column name does not exist (as in the case of columns
	 * that are expressions), then this variable contains an empty
	 * string." */
	rec->base_column_name = EMPTY_WSTR;
	/* "If a column does not have a label, the column name is returned. If
	 * the column is unlabeled and unnamed, an empty string is ret" */
	rec->label = rec->name.cnt ? rec->name : EMPTY_WSTR;

	assert(rec->name.str && rec->label.str);
	rec->unnamed = (rec->name.cnt || rec->label.cnt) ?
		SQL_NAMED : SQL_UNNAMED;

	/* All rec fields must be init'ed to a valid string in case their value
	 * is requested (and written with write_wstr()). The values would
	 * normally be provided by the data source, this is not the case here
	 * (yet), though. */
	rec->base_table_name = EMPTY_WSTR;
	rec->catalog_name = EMPTY_WSTR;
	rec->schema_name = EMPTY_WSTR;
	rec->table_name = EMPTY_WSTR;
#ifndef NDEBUG
	//dump_record(rec);
#endif /* NDEBUG */

	DBGH(stmt, "column #%zu: name=`" LWPDL "`, type=%d (`" LWPDL "`).",
		((uintptr_t)rec - (uintptr_t)stmt->ird->recs) / sizeof(*rec),
		LWSTR(&rec->name), rec->concise_type, LWSTR(col_type));

	return TRUE;
}

static SQLRETURN attach_columns_json(esodbc_stmt_st *stmt, UJObject columns)
{
	esodbc_desc_st *ird;
	esodbc_rec_st *rec;
	SQLRETURN ret;
	SQLSMALLINT recno;
	void *iter;
	UJObject col_o, name_o, type_o;
	wstr_st col_type, col_name;
	size_t ncols;
	const wchar_t *keys[] = {
		MK_WPTR(PACK_PARAM_COL_NAME),
		MK_WPTR(PACK_PARAM_COL_TYPE)
	};
	static const wstr_st EMPTY_WSTR = WSTR_INIT("");

	ird = stmt->ird;

	ncols = UJLengthArray(columns);
	INFOH(stmt, "columns received: %zu.", ncols);
	ret = update_rec_count(ird, (SQLSMALLINT)ncols);
	if (! SQL_SUCCEEDED(ret)) {
		ERRH(stmt, "failed to set IRD's record count to %d.", ncols);
		HDIAG_COPY(ird, stmt);
		return ret;
	}

	if (! (iter = UJBeginArray(columns))) {
		ERRH(stmt, "failed to obtain array iterator: %s.",
			UJGetError(stmt->rset.pack.json.state));
		goto err;
	}
	for (recno = 0; UJIterArray(&iter, &col_o); recno ++) {
		if (UJObjectUnpack(col_o, 2, "SS", keys, &name_o, &type_o) < 2) {
			ERRH(stmt, "failed to decode JSON column: %s.",
				UJGetError(stmt->rset.pack.json.state));
			RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		}
		rec = &ird->recs[recno];

		col_name.str = (SQLWCHAR *)UJReadString(name_o, &col_name.cnt);
		col_type.str = (SQLWCHAR *)UJReadString(type_o, &col_type.cnt);

		if (! attach_one_column(rec, &col_name, &col_type)) {
			goto err;
		}
	}

	return SQL_SUCCESS;
err:
	RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
}

static SQLRETURN attach_answer_json(esodbc_stmt_st *stmt)
{
	int unpacked;
	UJObject obj, columns, rows, cursor;
	const wchar_t *keys[] = {
		MK_WPTR(PACK_PARAM_COLUMNS),
		MK_WPTR(PACK_PARAM_ROWS),
		MK_WPTR(PACK_PARAM_CURSOR)
	};

	DBGH(stmt, "attaching JSON answer: [%zu] `" LCPDL "`.",
		stmt->rset.body.cnt, LCSTR(&stmt->rset.body));

	/* parse the entire JSON answer */
	obj = UJDecode(stmt->rset.body.str, stmt->rset.body.cnt, NULL,
			&stmt->rset.pack.json.state);
	if (! obj) {
		ERRH(stmt, "failed to decode JSON answer: %s ([%zu] `" LCPDL "`).",
			stmt->rset.pack.json.state ?
			UJGetError(stmt->rset.pack.json.state) : "<none>",
			stmt->rset.body.cnt, LCSTR(&stmt->rset.body));
		goto err;
	}
	columns = rows = cursor = NULL;
	/* extract the columns and rows objects */
	unpacked = UJObjectUnpack(obj, 3, "AAS", keys, &columns, &rows, &cursor);
	if (unpacked < /* 'rows' must always be present */1) {
		ERRH(stmt, "failed to unpack JSON answer: %s (`" LCPDL "`).",
			UJGetError(stmt->rset.pack.json.state), LCSTR(&stmt->rset.body));
		goto err;
	}

	/*
	 * set the internal cursor (UJSON4C array iterator)
	 */
	if (! rows) {
		ERRH(stmt, "no rows object received in answer: `" LCPDL "`.",
			LCSTR(&stmt->rset.body));
		goto err;
	}
	stmt->rset.pack.json.rows_iter = UJBeginArray(rows);
	/* UJSON4C will return NULL above, for empty array (meh!) */
	if (! stmt->rset.pack.json.rows_iter) {
		STMT_FORCE_NODATA(stmt);
	}
	/* save the object, as it might be required by EsSQLRowCount() */
	stmt->rset.pack.json.rows_obj = rows;
	/* unlike with tinycbor, the count is readily available with ujson4c
	 * (since the lib parses the entire JSON object upfront) => keep it in
	 * Release builds. */
	INFOH(stmt, "rows received in current (#%zu) result set: %d.",
		stmt->nset + 1, UJLengthArray(rows));

	/*
	 * copy ref to ES'es cursor (if there's one)
	 */
	if (cursor) {
		/* should have been cleared by now */
		assert(! stmt->rset.pack.json.curs.cnt);
		/* store new cursor vals */
		stmt->rset.pack.json.curs.str =
			(SQLWCHAR *)UJReadString(cursor, &stmt->rset.pack.json.curs.cnt);
		DBGH(stmt, "new paginating cursor: [%zd] `" LWPDL "`.",
			stmt->rset.pack.json.curs.cnt, LWSTR(&stmt->rset.pack.json.curs));
	}

	/*
	 * process the received columns, if any.
	 */
	if (columns) {
		if (0 < stmt->ird->count) {
			ERRH(stmt, "%d columns already attached.", stmt->ird->count);
			goto err;
		}
		return attach_columns_json(stmt, columns);
	}

	return SQL_SUCCESS;
err:
	RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
}


/* Function iterates over the recived "columns" array (of map elements).
 * The recived column names (and their types) are UTF-8 multi-bytes, which
 * need to be converted to UTF-16 wide-chars =>
 * - on first invocation, it calculates the space needed for the converted
 *   values (one chunk for all wide chars strings, two per column: name and
 *   type) and allocates it;
 * - on second invocation, it converts and attaches the columns to the
 *   statement. */
static BOOL iterate_on_columns(esodbc_stmt_st *stmt, CborValue columns)
{
	SQLSMALLINT recno;
	esodbc_desc_st *ird;
	CborError res;
	CborValue it, name_obj, type_obj;//, dsize_obj;
	cstr_st name_cstr, type_cstr;
	wstr_st name_wstr, type_wstr;
	const char *keys[] = {
		PACK_PARAM_COL_NAME,
		PACK_PARAM_COL_TYPE,
		//PACK_PARAM_COL_DSIZE
	};
	const size_t lens[] = {
		sizeof(PACK_PARAM_COL_NAME) - 1,
		sizeof(PACK_PARAM_COL_TYPE) - 1,
		//sizeof(PACK_PARAM_COL_DSIZE) - 1
	};
	CborValue *objs[] = {&name_obj, &type_obj};//, &dsize_obj};
	size_t keys_cnt = sizeof(keys)/sizeof(keys[0]);
	int n, left, need;
	wchar_t *wrptr; /* write pointer */

	ird = stmt->ird;

	res = cbor_value_enter_container(&columns, &it);
	CHK_RES(stmt, "failed to enter '" PACK_PARAM_COLUMNS "' array");

	if (! stmt->rset.pack.cbor.cols_buff.cnt) { /* 1st iter */
		wrptr = NULL;
		need = 0;
		left = 0;
	} else { /* 2nd iter */
		wrptr = (wchar_t *)stmt->rset.pack.cbor.cols_buff.str;
		/* .cnt convered from an int before (in 1st iter) */
		left = (int)stmt->rset.pack.cbor.cols_buff.cnt;
	}

	for (recno = 0; ! cbor_value_at_end(&it); recno ++) {
		res = cbor_value_skip_tag(&it);
		CHK_RES(stmt, "failed to skip tags in '" PACK_PARAM_COLUMNS "' array");
		if (! cbor_value_is_map(&it)) {
			ERRH(stmt, "invalid element type in '" PACK_PARAM_COLUMNS
				"' array.");
			return FALSE;
		}
		res = cbor_map_lookup_keys(&it, keys_cnt, keys, lens, objs);
		CHK_RES(stmt, "failed to lookup keys in '" PACK_PARAM_COLUMNS
			"' element #%hd", recno);

		/*
		 * column "name"
		 */
		if (! cbor_value_is_text_string(&name_obj)) {
			ERRH(stmt, "invalid non-text element '" PACK_PARAM_COL_NAME "'.");
			return FALSE;
		}
		res = cbor_value_get_unchunked_string(&name_obj, &name_cstr.str,
				&name_cstr.cnt);
		CHK_RES(stmt, "can't fetch value of '" PACK_PARAM_COL_NAME "' elem");

		n = U8MB_TO_U16WC(name_cstr.str, name_cstr.cnt, wrptr, left);
		if (n <= 0) {
			/* MultiByteToWideChar() can fail with empty string, but that's
			 * not a valid value in the "columns" anyway, so it should be OK
			 * to leave that case be handled by this branch. */
			ERRH(stmt, "failed to translate UTF-8 multi-byte stream: 0x%x.",
				WAPI_ERRNO());
			return FALSE;
		}
		if (! wrptr) { /* 1st iter */
			need += n + /*\0*/1;
		} else { /* 2nd iter */
			name_wstr.str = wrptr;
			name_wstr.cnt = (size_t)n; /* no 0-counting */

			wrptr += (size_t)n;
			left -= n;
			*wrptr ++ = '\0';
			left --;
		}

		/*
		 * column "type"
		 */
		if (! cbor_value_is_text_string(&type_obj)) {
			ERRH(stmt, "invalid non-text element '" PACK_PARAM_COL_TYPE "'.");
			return FALSE;
		}
		res = cbor_value_get_unchunked_string(&type_obj, &type_cstr.str,
				&type_cstr.cnt);
		CHK_RES(stmt, "can't fetch value of '" PACK_PARAM_COL_TYPE "' elem");
		/* U8MB_TO_U16WC fails with 0-len source */
		assert(type_cstr.cnt);

		n = U8MB_TO_U16WC(type_cstr.str, type_cstr.cnt, wrptr, left);
		if (n <= 0) {
			ERRH(stmt, "failed to translate UTF-8 multi-byte stream: 0x%x.",
				WAPI_ERRNO());
			return FALSE;
		}
		if (! wrptr) { /* 1st iter */
			need += n + /*\0*/1;
		} else { /* 2nd iter */
			type_wstr.str = wrptr;
			type_wstr.cnt = (size_t)n; /* no 0-counting */

			wrptr += (size_t)n;
			left -= n;
			/* add \0 */
			*wrptr ++ = '\0';
			left --;
		}

		if (! wrptr) { /* 1st iter: collect lengths only */
			continue;
		}
		/* 2nd iter: attach column */
		if (! attach_one_column(&ird->recs[recno], &name_wstr, &type_wstr)) {
			ERRH(stmt, "failed to attach column #%d `" LWPDL "`.", recno + 1,
				LWSTR(&name_wstr));
			return FALSE;
		}
	}
	/* no overflow */
	assert((! wrptr) || left == 0);

	if ((! wrptr) /* 1st iter: alloc cols slab/buffer */ && (0 < need)) {
		if (! (wrptr = malloc(need * sizeof(wchar_t)))) {
			ERRNH(stmt, "OOM: %zu B.", need * sizeof(wchar_t));
			return FALSE;
		}
		/* attach the buffer to the statement */
		stmt->rset.pack.cbor.cols_buff.str = (SQLWCHAR *)wrptr;
		/* cast is safe, 'need' checked against overflow */
		stmt->rset.pack.cbor.cols_buff.cnt = (size_t)need;
	}

	return TRUE;
err:
	return FALSE;
}

static SQLRETURN attach_columns_cbor(esodbc_stmt_st *stmt, CborValue columns)
{
	size_t ncols;
	SQLRETURN ret;
	CborError res;

	res = cbor_get_array_count(columns, &ncols);
	CHK_RES(stmt, "failed to get '" PACK_PARAM_COLUMNS "' array count.");
	INFOH(stmt, "columns received: %zu.", ncols);
	ret = update_rec_count(stmt->ird, (SQLSMALLINT)ncols);
	if (! SQL_SUCCEEDED(ret)) {
		ERRH(stmt, "failed to set IRD's record count to %d.", ncols);
		HDIAG_COPY(stmt->ird, stmt);
		return ret;
	}

	assert(! stmt->rset.pack.cbor.cols_buff.cnt);
	/* calculate buffer requirements and allocate it */
	if ((! iterate_on_columns(stmt, columns)) ||
		/* convert multi-byte to wchar_t and attach columns */
		(! iterate_on_columns(stmt, columns))) {
		goto err;
	}

	return SQL_SUCCESS;
err:
	RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
}

static SQLRETURN attach_answer_cbor(esodbc_stmt_st *stmt)
{
	CborError res;
	CborParser parser;
	CborValue top_obj, cols_obj, curs_obj, rows_obj;
	CborType obj_type;
	const char *keys[] = {
		PACK_PARAM_COLUMNS,
		PACK_PARAM_CURSOR,
		PACK_PARAM_ROWS,
	};
	size_t keys_no = sizeof(keys) / sizeof(keys[0]);
	const size_t lens[] = {
		sizeof(PACK_PARAM_COLUMNS) - 1,
		sizeof(PACK_PARAM_CURSOR) - 1,
		sizeof(PACK_PARAM_ROWS) - 1,
	};
	CborValue *vals[] = {&cols_obj, &curs_obj, &rows_obj};
	BOOL empty;
#	ifndef NDEBUG
	size_t nrows;
#	endif /* !NDEBUG */

	DBGH(stmt, "attaching CBOR answer: [%zu] `%s`.", stmt->rset.body.cnt,
		cstr_hex_dump(&stmt->rset.body));

	res = cbor_parser_init(stmt->rset.body.str, stmt->rset.body.cnt,
			ES_CBOR_PARSE_FLAGS, &parser, &top_obj);
	CHK_RES(stmt, "failed to init CBOR parser for object: [%zu] `%s`",
		stmt->rset.body.cnt, cstr_hex_dump(&stmt->rset.body));
#	ifndef NDEBUG
#	if 0 // ES uses indefinite-length containers (TODO) which trips this check
	/* the _init() doesn't actually validate the object */
	res = cbor_value_validate(&top_obj, ES_CBOR_PARSE_FLAGS);
	CHK_RES(stmt, "failed to validate CBOR object: [%zu] `%s`",
		stmt->rset.body.cnt, cstr_hex_dump(&stmt->rset.body));
#	endif /*0*/
#	endif /* !NDEBUG */

	if ((obj_type = cbor_value_get_type(&top_obj)) != CborMapType) {
		ERRH(stmt, "top object (of type 0x%x) is not a map.", obj_type);
		goto err;
	}
	res = cbor_map_lookup_keys(&top_obj, keys_no, keys, lens, vals);
	CHK_RES(stmt, "failed to lookup answer keys in map");

	/*
	 * set the internal "rows" cursor (tinycbor array object)
	 */
	/* check that we have a valid array object for "rows" */
	if (! cbor_value_is_array(&rows_obj)) {
		ERRH(stmt, "no '" PACK_PARAM_ROWS "' array object received in "
			"answer: `%s`.", cstr_hex_dump(&stmt->rset.body));
		goto err;
	}
	/* save the object, as it might be required by EsSQLRowCount() */
	stmt->rset.pack.cbor.rows_obj = rows_obj;

	/* ES uses indefinite-length arrays -- meh. */
	res = cbor_container_is_empty(rows_obj, &empty);
	CHK_RES(stmt, "failed to check if '" PACK_PARAM_ROWS "' array is empty");
	if (empty) {
		STMT_FORCE_NODATA(stmt);
	} else {
		/* Note: "expensive", as it requires ad-hoc parsing; so only keep for
		 * debugging (switching to JSON should be easy if troubleshooting) */
#		ifndef NDEBUG
		res = cbor_get_array_count(rows_obj, &nrows);
		CHK_RES(stmt, "failed to fetch '" PACK_PARAM_ROWS "' array length");
		INFOH(stmt, "rows received in current (#%zu) result set: %zu.",
			stmt->nset + 1, nrows);
#		endif /* NDEBUG */
		/* prepare iterator for EsSQLFetch(); recursing object and iterator
		 * can be the same, since there's no need to "leave" the container. */
		res = cbor_value_enter_container(&rows_obj, &rows_obj);
		CHK_RES(stmt, "failed to access '" PACK_PARAM_ROWS "' container");
		stmt->rset.pack.cbor.rows_iter = rows_obj;
	}

	/*
	 * copy ref to ES'es cursor (if there's one)
	 */
	if (cbor_value_is_valid(&curs_obj)) {
		obj_type = cbor_value_get_type(&curs_obj);
		if (obj_type != CborTextStringType) {
			ERRH(stmt, "invalid '" PACK_PARAM_CURSOR "' parameter type "
				"(0x%x)", obj_type);
			goto err;
		}
		/* should have been cleared by now */
		assert(! stmt->rset.pack.cbor.curs.cnt);
		res = cbor_value_get_unchunked_string(&curs_obj,
				&stmt->rset.pack.cbor.curs.str,
				&stmt->rset.pack.cbor.curs.cnt);
		if (res == CborErrorUnknownLength) {
			assert(stmt->rset.pack.cbor.curs_allocd == false);
			/* cursor is in chunked string; get it assembled in one chunk */
			res = cbor_value_dup_text_string(&curs_obj,
					&stmt->rset.pack.cbor.curs.str,
					&stmt->rset.pack.cbor.curs.cnt,
					&curs_obj);
			if (res == CborNoError) {
				stmt->rset.pack.cbor.curs_allocd = true;
			}
		}
		CHK_RES(stmt, "failed to read '" PACK_PARAM_CURSOR "' value");
		DBGH(stmt, "new paginating cursor: [%zd] `" LCPDL "`.",
			stmt->rset.pack.cbor.curs.cnt, LWSTR(&stmt->rset.pack.cbor.curs));
	}

	/*
	 * process the received columns, if any.
	 */
	if (cbor_value_is_valid(&cols_obj)) {
		if (0 < stmt->ird->count) {
			ERRH(stmt, "%d columns already attached.", stmt->ird->count);
			goto err;
		}
		return attach_columns_cbor(stmt, cols_obj);
	}

	return SQL_SUCCESS;
err:
	RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, res);
}

static BOOL attach_error_cbor(SQLHANDLE hnd, cstr_st *body)
{
	CborError res;
	CborParser parser;
	CborValue top_obj, err_obj, status_obj, rcause_obj, type_obj, reason_obj;
	CborType obj_type;
	const char *keys[] = {
		PACK_PARAM_ERROR,
		PACK_PARAM_STATUS
	};
	const size_t lens[] = {
		sizeof(PACK_PARAM_ERROR) - 1,
		sizeof(PACK_PARAM_STATUS) - 1
	};
	size_t keys_cnt = sizeof(keys) / sizeof(keys[0]);
	CborValue *vals[] = {&err_obj, &status_obj};

	const char *err_keys[] = {
		PACK_PARAM_ERR_RCAUSE,
		PACK_PARAM_ERR_TYPE,
		PACK_PARAM_ERR_REASON
	};
	size_t err_lens[] = {
		sizeof(PACK_PARAM_ERR_RCAUSE) - 1,
		sizeof(PACK_PARAM_ERR_TYPE) - 1,
		sizeof(PACK_PARAM_ERR_REASON) - 1
	};
	size_t err_keys_cnt = sizeof(err_keys) / sizeof(err_keys[0]);
	CborValue *err_vals[] = {&rcause_obj, &type_obj, &reason_obj};
	wstr_st type_wstr, reason_wstr;

	static const wstr_st msg_sep = WSTR_INIT(": ");
	wstr_st msg;
	int code;
	SQLINTEGER status;
	wchar_t wbuff[SQL_MAX_MESSAGE_LENGTH];
	const size_t wbuff_cnt = sizeof(wbuff)/sizeof(wbuff[0]) - /*\0 l8r*/1;
	size_t n, pos;

	res = cbor_parser_init(body->str, body->cnt, ES_CBOR_PARSE_FLAGS,
			&parser, &top_obj);
	CHK_RES(hnd, "failed to parse CBOR object: [%zu] `%s`", body->cnt,
		cstr_hex_dump(body));
#	ifndef NDEBUG
#	if 0 // ES uses indefinite-length containers (TODO) which trips this check
	/* the _init() doesn't actually validate the object */
	res = cbor_value_validate(&top_obj, ES_CBOR_PARSE_FLAGS);
	CHK_RES(stmt, "failed to validate CBOR object: [%zu] `%s`",
		body->cnt, cstr_hex_dump(body));
#	endif /*0*/
#	endif /* !NDEBUG */

	if ((obj_type = cbor_value_get_type(&top_obj)) != CborMapType) {
		ERRH(hnd, "top object (of type 0x%x) is not a map.", obj_type);
		goto err;
	}
	res = cbor_map_lookup_keys(&top_obj, keys_cnt, keys, lens, vals);
	CHK_RES(hnd, "failed to lookup answer keys in map");

	if ((obj_type = cbor_value_get_type(&status_obj)) == CborIntegerType) {
		res = cbor_value_get_int_checked(&status_obj, &code);
		CHK_RES(hnd, "can't extract status code");
		status = (SQLINTEGER)code;
	} else {
		ERRH(hnd, "Status object is not of integer type (0x%x).", obj_type);
		/* carry on nevertheless */
		status = 0;
	}

	if (cbor_value_is_text_string(&err_obj)) { /* "generic" error */
		res = cbor_value_get_utf16_wstr(&err_obj, &msg);
		CHK_RES(hnd, "failed to fetch error message");
	} else if ((obj_type = cbor_value_get_type(&status_obj)) ==
		CborIntegerType) { /* error with root cause */
		/* unpack "error" object */
		res = cbor_map_lookup_keys(&err_obj, err_keys_cnt, err_keys, err_lens,
				err_vals);
		CHK_RES(hnd, "failed to lookup error object keys in map");
		/* "type" and "reason" objects must be text strings */
		if ((! cbor_value_is_text_string(&type_obj)) ||
			(! cbor_value_is_text_string(&reason_obj))) {
			ERRH(hnd, "unsupported '" PACK_PARAM_ERROR "' obj structure.");
			goto err;
		}
		res = cbor_value_get_utf16_wstr(&type_obj, &type_wstr);
		CHK_RES(hnd, "failed to fetch UTF16 '" PACK_PARAM_ERR_TYPE "'");
		n = type_wstr.cnt < wbuff_cnt ? type_wstr.cnt : wbuff_cnt;
		wmemcpy(wbuff, type_wstr.str, n);
		pos = n;
		if (msg_sep.cnt + pos < wbuff_cnt) {
			wmemcpy(wbuff + pos, msg_sep.str, msg_sep.cnt);
			pos += msg_sep.cnt;
		}
		res = cbor_value_get_utf16_wstr(&reason_obj, &reason_wstr);
		CHK_RES(hnd, "failed to fetch UTF16 '" PACK_PARAM_ERR_REASON "'");
		n = reason_wstr.cnt + pos < wbuff_cnt ? reason_wstr.cnt : wbuff_cnt -
			pos;
		wmemcpy(wbuff + pos, reason_wstr.str, n);
		pos += n;
		assert(pos <= wbuff_cnt);

		wbuff[pos] = '\0';
		msg.str = wbuff;
		msg.cnt = pos;
	} else {
		ERRH(hnd, "unsupported '" PACK_PARAM_ERROR "' obj type (0x%x).",
			obj_type);
		goto err;
	}

	ERRH(hnd, "request fail msg: [%zu] `" LWPDL "`.", msg.cnt, LWSTR(&msg));
	post_diagnostic(hnd, SQL_STATE_HY000, msg.str, status);
	return TRUE;
err:
	return FALSE;
}

/*
 * Processes a received answer:
 * - takes a dynamic buffer, answ->str, of length answ->cnt. Will handle the
 *   buff memory even if the call fails.
 * - parses it, preparing iterators for SQLFetch()'ing.
 */
SQLRETURN TEST_API attach_answer(esodbc_stmt_st *stmt, cstr_st *answer,
	BOOL is_json)
{
	SQLRETURN ret;
	size_t old_ird_cnt;

	/* clear any previous result set */
	if (STMT_HAS_RESULTSET(stmt)) {
		clear_resultset(stmt, /*on_close*/FALSE);
	}

	/* the statement takes ownership of mem obj */
	stmt->rset.body = *answer;
	stmt->rset.pack_json = is_json;
	old_ird_cnt = stmt->ird->count;
	ret = is_json ? attach_answer_json(stmt) : attach_answer_cbor(stmt);

	/* check if the columns either have just or had already been attached */
	if (SQL_SUCCEEDED(ret)) {
		if (stmt->ird->count <= 0) {
			ERRH(stmt, "no columns available in result set; answer: "
				"`" LCPDL "`.", LCSTR(&stmt->rset.body));
			RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
		} else if (old_ird_cnt <= 0) {
			/* new columns have just been attached => force compat. check */
			stmt->sql2c_conversion = CONVERSION_UNCHECKED;
		}
		if (STMT_NODATA_FORCED(stmt)) {
			DBGH(stmt, "empty result set received.");
		} else {
			stmt->nset ++;
		}
	}

	return ret;
}

static BOOL attach_error_json(SQLHANDLE hnd, cstr_st *body)
{
	BOOL ret;
	UJObject obj, o_status, o_error, o_type, o_reason, o_rcause;
	wstr_st type, reason;
	wchar_t wbuf[SQL_MAX_MESSAGE_LENGTH];
	wstr_st msg = {.str = wbuf};
	int cnt;
	void *state, *iter;
	/* following grouped JSON unpacking items must remain in sync */
	/* {"error": {..}, "status":200} */
	const wchar_t *outer_keys[] = {
		MK_WPTR(PACK_PARAM_ERROR),
		MK_WPTR(PACK_PARAM_STATUS)
	};
	const char fmt_outer_keys[] = "UN";
	int cnt_outer_keys = sizeof(fmt_outer_keys) - /*\0*/1;
	/* "error": {"root_cause":[?], "type":"..", "reason":".." ...} */
	const wchar_t *err_keys[] = {
		MK_WPTR(PACK_PARAM_ERR_RCAUSE),
		MK_WPTR(PACK_PARAM_ERR_TYPE),
		MK_WPTR(PACK_PARAM_ERR_REASON),
	};
	const char fmt_err_keys[] = "aSS";
	int cnt_err_keys = sizeof(fmt_err_keys) - /*\0*/1;
	/* "root_cause":[{"type":"..", "reason":".."} ..] */
	const wchar_t *r_err_keys[] = {
		MK_WPTR(PACK_PARAM_ERR_TYPE),
		MK_WPTR(PACK_PARAM_ERR_REASON),
	};
	const char fmt_r_err_keys[] = "SS";
	int cnt_r_err_keys = sizeof(fmt_r_err_keys) - /*\0*/1;

	ret = FALSE;
	state = NULL;

	/* parse the entire JSON answer */
	obj = UJDecode(body->str, body->cnt, NULL, &state);
	if (! obj) {
		ERRH(hnd, "answer not JSON (%s).",
			state ? UJGetError(state) : "<none>");
		goto end;
	}
	/* extract the status and error object */
	if (UJObjectUnpack(obj, cnt_outer_keys, fmt_outer_keys, outer_keys,
			&o_error, &o_status) < cnt_outer_keys) {
		ERRH(hnd, "JSON answer not a SQL error (%s).", UJGetError(state));
		goto end;
	}
	if (UJIsString(o_error)) { /* "generic" error */
		msg.str = (SQLWCHAR *)UJReadString(o_error, &msg.cnt);
		assert(msg.str[msg.cnt] == '\0');
	} else if (UJIsObject(o_error)) { /* error has root cause */
		/* unpack error object */
		if (UJObjectUnpack(o_error, cnt_err_keys, fmt_err_keys, err_keys,
				&o_rcause, &o_type, &o_reason) < cnt_err_keys) {
			ERRH(hnd, "failed to unpack error obj (%s).", UJGetError(state));
			goto end;
		}

		/* this is safe for NULL o_rcause: => -1 */
		cnt = UJLengthArray(o_rcause);
		DBGH(hnd, "root cause(s) received: %d.", cnt);
		if (0 < cnt) {
			/* print the root_cause, if available */
			iter = UJBeginArray(o_rcause);
			/* save, UJIterArray() checks against NULL */
			assert(iter);
			while (UJIterArray(&iter, &o_rcause)) { /* reuse o_rcause obj */
				/* unpack root error object */
				if (UJObjectUnpack(o_rcause, cnt_r_err_keys, fmt_r_err_keys,
						r_err_keys, &o_type, &o_reason) < cnt_r_err_keys) {
					ERRH(hnd, "failed to unpack root error object (%s).",
						UJGetError(state));
					goto end; /* TODO: continue on error? */
				} else {
					/* stop at first element. TODO: is ever [array] > 1? */
					break;
				}
			}
		}
		/* else: root_cause not available, print "generic" reason */
		type.str = (SQLWCHAR *)UJReadString(o_type, &type.cnt);
		reason.str = (SQLWCHAR *)UJReadString(o_reason, &reason.cnt);

		/* should be empty string in case of mismatch */
		assert(type.str && reason.str);
		DBGH(hnd, "reported failure: type: [%zd] `" LWPDL "`, reason: [%zd] `"
			LWPDL "`, status: %d.", type.cnt, LWSTR(&type),
			reason.cnt, LWSTR(&reason), UJNumericInt(o_status));

		/* swprintf will always append the 0-term ("A null character is
		 * appended after the last character written."), but fail if formated
		 * string would overrun the buffer size (in an equivocal way: overrun
		 * <?> encoding error). */
		errno = 0;
		cnt = swprintf(wbuf, sizeof(wbuf)/sizeof(*wbuf),
				WPFWP_LDESC L": " WPFWP_LDESC, LWSTR(&type), LWSTR(&reason));
		assert(cnt);
		if (cnt < 0) {
			if (errno) {
				ERRH(hnd, "printing the received error message failed.");
				goto end;
			}
			/* partial error message printed */
			WARNH(hnd, "current error buffer to small (%zu) for full error "
				"detail.", sizeof(wbuf)/sizeof(*wbuf));
			cnt = sizeof(wbuf)/sizeof(*wbuf) - 1;
		}
		msg.cnt = cnt;
		assert(wbuf[cnt] == L'\0');
	} else {
		ERRH(hnd, "unsupported '" PACK_PARAM_ERROR "' obj type (%d).",
			UJGetType(o_error));
		goto end;
	}
	ERRH(hnd, "request fail msg: [%zd] `" LWPDL "`.", msg.cnt, LWSTR(&msg));

	post_diagnostic(hnd, SQL_STATE_HY000, msg.str, UJNumericInt(o_status));
	ret = TRUE;

end:
	if (state) {
		UJFree(state);
	}

	return ret;
}

/*
 * Parse an error and push it as statement diagnostic.
 */
SQLRETURN TEST_API attach_error(SQLHANDLE hnd, cstr_st *body, BOOL is_json,
	long code)
{
	wchar_t *buff;
	int n;
	BOOL formatted;

	ERRH(hnd, "request failed with %ld (body len: %zu).", code, body->cnt);

	if (body->cnt) {
		/* try to decode it as JSON/CBOR */
		formatted = is_json ? attach_error_json(hnd, body) :
			attach_error_cbor(hnd, body);
		if (! formatted) {
			/* if not an ES-formatted failure, attach it as-is (plus \0) */
			if (! (buff = malloc((body->cnt + 1) * sizeof(wchar_t)))) {
				ERRNH(hnd, "OOM: %zu wchar_t.", body->cnt);
				goto end;
			}
			n = U8MB_TO_U16WC(body->str, body->cnt, buff, body->cnt);
			if (0 < n) {
				buff[n] = '\0';
				post_diagnostic(hnd, SQL_STATE_08S01, buff, code);
			}
			free(buff);
			if (n <= 0) {
				ERRH(hnd, "failed to UTF8/UTF16 convert: 0x%x.", WAPI_ERRNO());
				goto end;
			}
		}

		RET_STATE(HDRH(hnd)->diag.state);
	}

end:
	return post_diagnostic(hnd, SQL_STATE_08S01, NULL, code);
}

/*
 * Attach an SQL query to the statment: malloc, convert, copy.
 */
SQLRETURN TEST_API attach_sql(esodbc_stmt_st *stmt,
	const SQLWCHAR *sql, /* SQL text statement */
	size_t sqlcnt /* count of chars of 'sql' */)
{
	wstr_st sqlw = (wstr_st) {
		(SQLWCHAR *)sql, sqlcnt
	};

	INFOH(stmt, "attaching SQL [%zd] `" LWPDL "`.", sqlcnt, LWSTR(&sqlw));

	assert(! stmt->u8sql.str);
	if (! wstr_to_utf8(&sqlw, &stmt->u8sql)) {
		ERRNH(stmt, "conversion UTF16->UTF8 of SQL [%zu] `" LWPDL "` failed.",
			sqlcnt, LWSTR(&sqlw));
		RET_HDIAG(stmt, SQL_STATE_HY000, "UTF16/UTF8 conversion failure", 0);
	}

	/* if the app correctly SQL_CLOSE'es the statement, this would not be
	 * needed. but just in case: re-init counter of total # of rows and sets */
	STMT_ROW_CNT_RESET(stmt);
	stmt->early_executed = false;

	return SQL_SUCCESS;
}

/*
 * Detach the existing query (if any) from the statement.
 */
void detach_sql(esodbc_stmt_st *stmt)
{
	if (! stmt->u8sql.str) {
		assert(! stmt->u8sql.cnt);
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

	if (! ColumnNumber) {
		/* "The statement attribute SQL_ATTR_USE_BOOKMARKS should always be
		 * set before binding a column to column 0. This is not required but
		 * is strongly recommended." */
		assert(stmt->bookmarks == SQL_UB_OFF); // TODO: bookmarks
		ERRH(stmt, "bookmarks use turned off.");
		RET_HDIAGS(stmt, SQL_STATE_07009);
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
		TargetValue, (int64_t)BufferLength, StrLen_or_Ind);

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

static SQLRETURN set_row_diag(esodbc_desc_st *ird,
	esodbc_state_et state, const char *msg,
	SQLULEN pos, SQLINTEGER colno)
{
	esodbc_stmt_st *stmt = HDRH(ird)->stmt;
	SQLWCHAR wbuff[SQL_MAX_MESSAGE_LENGTH], *wmsg = NULL;
	int res;

	if (ird->array_status_ptr) {
		ird->array_status_ptr[pos] = SQL_ROW_ERROR;
	}
	if (msg) {
		res = ascii_c2w((SQLCHAR *)msg, wbuff, SQL_MAX_MESSAGE_LENGTH - 1);
		if (0 < res) {
			wmsg = wbuff;
		}
	}
	return post_row_diagnostic(stmt, state, wmsg, /*code*/0,
			stmt->tv_rows + /*current*/1, colno);

}

/*
 * Copy one row from IRD to ARD.
 * pos: row number in the rowset
 */
SQLRETURN copy_one_row_json(esodbc_stmt_st *stmt, SQLULEN pos)
{
	SQLINTEGER i;
	size_t rowno;
	UJObject obj;
	SQLRETURN ret;
	SQLLEN *ind_len;
	long long ll;
	double dbl;
	const wchar_t *wstr;
	BOOL boolval;
	size_t len;
	BOOL with_info;
	esodbc_desc_st *ard, *ird;
	esodbc_rec_st *arec, *irec;

	ard = stmt->ard;
	ird = stmt->ird;
	rowno = stmt->tv_rows
		+ STMT_GD_CALLING(stmt)
		? /* SQLFetch() executed already, row counted */0
		: /* SQLFetch() in progress, current row not yet counted */1;

	with_info = FALSE;
	/* iterate over the bound cols of one (table) row */
	assert((! STMT_GD_CALLING(stmt)) || 0 < stmt->gd_col);
	for (i = STMT_GD_CALLING(stmt) ? stmt->gd_col -1 : 0; i < ard->count;
		i ++) {
		arec = &ard->recs[i]; /* access safe if 'i < ard->count' */
		/* if record not bound skip it */
		if (! REC_IS_BOUND(arec)) {
			DBGH(stmt, "column #%d not bound, skipping it.", i + 1);
			continue;
		}
		DBGH(stmt, "column #%d is bound, copying data.", i + 1);
		if (ird->count <= i) {
			ERRH(stmt, "only %hd columns in result set, no data to return in "
				"column #%hd.", ird->count, i + 1);
			return set_row_diag(ird, SQL_STATE_HY000, MSG_INV_SRV_ANS, pos,
					i + 1);
		} else {
			irec = &ird->recs[i];
		}

		obj = irec->i_val.json;
		switch (UJGetType(obj)) {
			default:
				ERRH(stmt, "unexpected object of type %d in row L#%zu/T#%zd.",
					UJGetType(obj), stmt->rset.vrows, rowno);
				return set_row_diag(ird, SQL_STATE_HY000, MSG_INV_SRV_ANS, pos,
						i + 1);

			case UJT_Null:
				DBGH(stmt, "value [%zd, %d] is NULL.", rowno, i + 1);
				ind_len = deferred_address(SQL_DESC_INDICATOR_PTR, pos, arec);
				if (! ind_len) {
					ERRH(stmt, "no buffer to signal NULL value.");
					return set_row_diag(ird, SQL_STATE_22002, NULL, pos,
							i + 1);
				}
				if (arec->es_type && (! arec->es_type->nullable)) {
					WARNH(stmt, "returning NULL for non-nullable type.");
				}
				*ind_len = SQL_NULL_DATA;
				continue; /* instead of break! no 'ret' processing to do. */

			case UJT_String:
				wstr = UJReadString(obj, &len);
				DBGH(stmt, "value [%zd, %d] is string: [%d] `" LWPDL "`.",
					rowno, i + 1, len, len, wstr);
				/* UJSON4C returns chars count, but 0-terminates w/o counting
				 * the terminator */
				assert(wstr[len] == '\0');
				/* "When character data is returned from the driver to the
				 * application, the driver must always null-terminate it." */
				ret = sql2c_string(arec, irec, pos, wstr, len + /*\0*/1);
				break;

			case UJT_Long:
			case UJT_LongLong:
				ll = UJNumericLongLong(obj);
				DBGH(stmt, "value [%zd, %d] is integer: %lld.", rowno, i + 1,
					ll);
				ret = sql2c_longlong(arec, irec, pos, ll);
				break;

			case UJT_Double:
				dbl = UJNumericFloat(obj);
				DBGH(stmt, "value [%zd, %d] is double: %f.", rowno, i + 1,
					dbl);
				ret = sql2c_double(arec, irec, pos, dbl);
				break;

			case UJT_True:
			case UJT_False:
				boolval = UJGetType(obj) == UJT_True ? TRUE : FALSE;
				DBGH(stmt, "value [%zd, %d] is boolean: %d.", rowno, i + 1,
					boolval);
				/* "When bit SQL data is converted to character C data, the
				 * possible values are "0" and "1"." */
				ret = sql2c_longlong(arec, irec, pos, boolval ? 1LL : 0LL);
				break;
		}

		/* set the (row, column) details in the diagnostic, in case the
		 * value copying isn't a clean success (i.e. success with info or an
		 * error) */
		switch (ret) {
			case SQL_SUCCESS_WITH_INFO:
				with_info = TRUE;
				stmt->hdr.diag.row_number = rowno;
				stmt->hdr.diag.column_number = i + 1;
			/* no break */
			case SQL_SUCCESS:
				break; /* continue iteration over row's values */

			default: /* error */
				stmt->hdr.diag.row_number = rowno;
				stmt->hdr.diag.column_number = i + 1;
				return ret; /* row fetching failed */
		}
	}

	if (ird->array_status_ptr) {
		ird->array_status_ptr[pos] = with_info ? SQL_ROW_SUCCESS_WITH_INFO :
			SQL_ROW_SUCCESS;
		DBGH(stmt, "status array @0x%p#%d set to %hu.", ird->array_status_ptr,
			pos, ird->array_status_ptr[pos]);
	}

	return with_info ? SQL_SUCCESS_WITH_INFO : SQL_SUCCESS;
}

static SQLRETURN unpack_one_row_json(esodbc_stmt_st *stmt, SQLULEN pos)
{
	SQLSMALLINT i;
	size_t rowno;
	UJObject obj;
	void *iter_row;
	esodbc_desc_st *ird;
	esodbc_rec_st *irec;

	ird = stmt->ird;
	rowno = stmt->tv_rows + /* current not yet counted */1;

	/* is current object an array? */
	if (! UJIsArray(stmt->rset.pack.json.row_array)) {
		ERRH(stmt, "one '%s' element (#%zu) in result set not an array; type:"
			" %d.", PACK_PARAM_ROWS, stmt->rset.vrows,
			UJGetType(stmt->rset.pack.json.row_array));
		return set_row_diag(ird, SQL_STATE_HY000, MSG_INV_SRV_ANS, pos,
				SQL_NO_COLUMN_NUMBER);
	}
	/* get an iterator over the row array */
	if (! (iter_row = UJBeginArray(stmt->rset.pack.json.row_array))) {
		ERRH(stmt, "Failed to obtain iterator on row (#%zd): %s.", rowno,
			UJGetError(stmt->rset.pack.json.state));
		return set_row_diag(ird, SQL_STATE_HY000, MSG_INV_SRV_ANS, pos,
				SQL_NO_COLUMN_NUMBER);
	}

	/* iterate over the elements in current (table) row and reference-copy
	 * their value */
	for (i = 0; i < ird->count; i ++) {
		/* access made safe by ird->count match against array len above  */
		irec = &ird->recs[i];
		if (! UJIterArray(&iter_row, &irec->i_val.json)) {
			ERRH(stmt, "current row %zd counts fewer elements: %hd than "
				"columns: %hd.", rowno, i, ird->count);
			return set_row_diag(ird, SQL_STATE_HY000, MSG_INV_SRV_ANS, pos,
					i + 1);
		}
	}

	/* Are there further elements in row? This could indicate that the data
	 * returned is not the data asked for => safer to fail. */
	if (UJIterArray(&iter_row, &obj)) {
		ERRH(stmt, "current row %zd counts more elems than columns.", rowno);
		return set_row_diag(ird, SQL_STATE_HY000, MSG_INV_SRV_ANS, pos,
				SQL_NO_COLUMN_NUMBER);
	}

	/* copy values, if there's already any bound column */
	return 0 < stmt->ard->count ? copy_one_row_json(stmt, pos) : SQL_SUCCESS;
}


/*
 * Copy one row from IRD to ARD.
 * pos: row number in the rowset
 */
static SQLRETURN copy_one_row_cbor(esodbc_stmt_st *stmt, SQLULEN pos)
{
	SQLRETURN ret;
	CborError res;
	CborValue obj;
	CborType elem_type;
	SQLINTEGER i;
	esodbc_desc_st *ard, *ird;
	esodbc_rec_st *arec, *irec;
	size_t rowno;
	BOOL with_info;
	SQLLEN *ind_len;
	int64_t i64;
	wstr_st wstr;
	bool boolval;
	double dbl;
	uint16_t ui16;
	float flt;

	ard = stmt->ard;
	ird = stmt->ird;
	rowno = stmt->tv_rows
		+ STMT_GD_CALLING(stmt)
		? /* SQLFetch() executed already, row counted */0
		: /* SQLFetch() in progress, current row not yet counted */1;

	with_info = FALSE;
	/* iterate over the bound cols and contents of one (table) row */
	assert((! STMT_GD_CALLING(stmt)) || 0 < stmt->gd_col);
	for (i = STMT_GD_CALLING(stmt) ? stmt->gd_col -1 : 0; i < ard->count;
		i ++) {
		arec = &ard->recs[i]; /* access safe if 'i < ard->count' */
		/* if record not bound skip it */
		if (! REC_IS_BOUND(arec)) {
			DBGH(stmt, "column #%d not bound, skipping it.", i + 1);
			continue;
		}
		DBGH(stmt, "column #%d is bound, copying data.", i + 1);
		if (ird->count <= i) {
			ERRH(stmt, "only %hd columns in result set, no data to return in "
				"column #%hd.", ird->count, i + 1);
			return set_row_diag(ird, SQL_STATE_HY000, MSG_INV_SRV_ANS, pos,
					i + 1);
		} else {
			irec = &ird->recs[i];
		}

		obj = irec->i_val.cbor;
		elem_type = cbor_value_get_type(&obj);
		DBGH(stmt, "current element of type: 0x%x", elem_type);
		switch (elem_type) {
			case CborByteStringType:
			case CborArrayType:
			case CborMapType:
			case CborSimpleType:
			case CborUndefinedType:
			case CborTagType:
			default: /* + CborInvalidType */
				ERRH(stmt, "unexpected elem. of type 0x%x in row", elem_type);
				goto err;

			case CborNullType:
				DBGH(stmt, "value [%zd, %d] is NULL.", rowno, i + 1);
				ind_len = deferred_address(SQL_DESC_INDICATOR_PTR, pos, arec);
				if (! ind_len) {
					ERRH(stmt, "no buffer to signal NULL value.");
					return set_row_diag(ird, SQL_STATE_22002, NULL, pos,
							i + 1);
				}
				if (arec->es_type && (! arec->es_type->nullable)) {
					WARNH(stmt, "returning NULL for non-nullable type.");
				}
				*ind_len = SQL_NULL_DATA;
				ret = SQL_SUCCESS;
				break;

			case CborTextStringType:
				res = cbor_value_get_utf16_wstr(&obj, &wstr);
				CHK_RES(stmt, "failed to extract text string");
				DBGH(stmt, "value [%zd, %d] is string: [%zu] `" LWPDL "`.",
					rowno, i + 1, wstr.cnt, LWSTR(&wstr));
				/* UTF8/16 conversion terminates the string */
				assert(wstr.str[wstr.cnt] == '\0');
				/* "When character data is returned from the driver to the
				 * application, the driver must always null-terminate it." */
				ret = sql2c_string(arec, irec, pos, wstr.str, wstr.cnt + 1);
				break;

			case CborIntegerType:
				res = cbor_value_get_int64_checked(&obj, &i64);
				CHK_RES(stmt, "failed to extract int64 value");
				DBGH(stmt, "value [%zd, %d] is integer: %I64d.", rowno,
					i + 1, i64);
				assert(sizeof(int64_t) == sizeof(long long));
				ret = sql2c_longlong(arec, irec, pos, (long long)i64);
				break;

			/*INDENT-OFF*/
			do {
			case CborHalfFloatType:
				res = cbor_value_get_half_float(&obj, &ui16);
				/* res not yet checked, but likely(OK) */
				dbl = decode_half(ui16);
				break;
			case CborFloatType:
				res = cbor_value_get_float(&obj, &flt);
				dbl = (double)flt;
				break;
			case CborDoubleType:
				res = cbor_value_get_double(&obj, &dbl);
				break;
			} while (0);
				CHK_RES(stmt, "failed to extract flt. point type 0x%x",
						elem_type);
				DBGH(stmt, "value [%zd, %d] is double: %f.", rowno,
					i + 1, dbl);
				ret = sql2c_double(arec, irec, pos, dbl);
				break;
			/*INDENT-ON*/

			case CborBooleanType:
				res = cbor_value_get_boolean(&obj, &boolval);
				CHK_RES(stmt, "failed to extract boolean value");
				DBGH(stmt, "value [%zd, %d] is boolean: %d.", rowno,
					i + 1, boolval);
				/* 'When bit SQL data is converted to character C data, the
				 * possible values are "0" and "1".' */
				ret = sql2c_longlong(arec, irec, pos, (long long)!!boolval);
				break;
		}

		/* set the (row, column) details in the diagnostic, in case the
		 * value copying isn't a clean success (i.e. success with info or an
		 * error) */
		switch (ret) {
			case SQL_SUCCESS_WITH_INFO:
				with_info = TRUE;
				stmt->hdr.diag.row_number = rowno;
				stmt->hdr.diag.column_number = i + 1;
			/* no break */
			case SQL_SUCCESS:
				break; /* continue iteration over row's values */

			default: /* error */
				stmt->hdr.diag.row_number = rowno;
				stmt->hdr.diag.column_number = i + 1;
				return ret; /* row fetching failed */
		}
	}

	if (ird->array_status_ptr) {
		ird->array_status_ptr[pos] = with_info ? SQL_ROW_SUCCESS_WITH_INFO :
			SQL_ROW_SUCCESS;
		DBGH(stmt, "status array @0x%p#%d set to %hu.", ird->array_status_ptr,
			pos, ird->array_status_ptr[pos]);
	}
	return with_info ? SQL_SUCCESS_WITH_INFO : SQL_SUCCESS;

err:
	return set_row_diag(ird, SQL_STATE_HY000, MSG_INV_SRV_ANS, pos, i + 1);
}

static SQLRETURN unpack_one_row_cbor(esodbc_stmt_st *stmt, SQLULEN pos)
{
	CborError res;
	CborValue *rows_iter, it;
	CborType elem_type;
	SQLSMALLINT i;
	esodbc_desc_st *ird;
	esodbc_rec_st *irec;
	size_t rowno;

	ird = stmt->ird;
	rowno = stmt->tv_rows + /* current row not yet counted */1;

	i = -1;
	rows_iter = &stmt->rset.pack.cbor.rows_iter;
	/* is current object an array? */
	if ((elem_type = cbor_value_get_type(rows_iter)) != CborArrayType) {
		ERRH(stmt, "current (#%zu) element of type 0x%x is not an array -- "
			"skipping it.", stmt->rset.vrows, elem_type);
		goto err;
	}
	/* enter the row array */
	if ((res = cbor_value_enter_container(rows_iter, &it)) != CborNoError) {
		ERRH(stmt, "failed to enter row array: %s.", cbor_error_string(res));
		goto err;
	}

	/* iterate over the elements in current (table) row and reference-copy
	 * their value */
	for (i = 0; i < ird->count; i ++) {
		if (cbor_value_at_end(&it)) {
			ERRH(stmt, "current row %zd counts fewer elements: %hd than "
				"columns: %hd.", rowno, i + 1, ird->count);
		}
		irec = &ird->recs[i];
		irec->i_val.cbor = it;
		assert(! cbor_value_is_tag(&it));

		res = cbor_value_advance(&it);
		if (res != CborNoError) {
			ERRH(stmt, "failed to advance past current value in row array: "
				"%s.", cbor_error_string(res));
			goto err;
		}
	}

	/* Are there further elements in row? This could indicate that the data
	 * returned is not the data asked for => safer to fail. */
	if (! cbor_value_at_end(&it)) {
		ERRH(stmt, "current row %zd counts more elems than columns.", rowno);
#		ifdef NDEBUG
		return set_row_diag(ird, SQL_STATE_HY000, MSG_INV_SRV_ANS, pos,
				SQL_NO_COLUMN_NUMBER);
#		else /* NDEBUG */
		assert(0);
		res = cbor_value_exit_container(rows_iter, &it);
#		endif /* NDEBUG */
	} else {
		res = cbor_value_leave_container(rows_iter, &it);
	}
	if (res != CborNoError) {
		ERRH(stmt, "failed to exit row array: %s.", cbor_error_string(res));
		return set_row_diag(ird, SQL_STATE_HY000, MSG_INV_SRV_ANS, pos,
				SQL_NO_COLUMN_NUMBER);
	}

	return 0 < stmt->ard->count ? copy_one_row_cbor(stmt, pos) : SQL_SUCCESS;

err:
	res = (i < 0) ? cbor_value_advance(rows_iter) :
		cbor_value_exit_container(rows_iter, &it);
	if (res != CborNoError) {
		ERRH(stmt, "failed to %s current row: %s.", (i < 0) ? "skip" : "exit",
			cbor_error_string(res));
	}
	return set_row_diag(ird, SQL_STATE_HY000, MSG_INV_SRV_ANS, pos,
			SQL_NO_COLUMN_NUMBER);
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
	SQLULEN i, j, errors;
	SQLRETURN ret;
	BOOL empty, pack_json;

	stmt = STMH(StatementHandle);

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
			ret = convertability_check(stmt, CONV_CHECK_ALL_COLS,
					(int *)&stmt->sql2c_conversion);
			if (! SQL_SUCCEEDED(ret)) {
				return ret;
			}
		/* no break; */

		default:
			DBGH(stmt, "ES/app data/buffer types found compatible.");
	}

	DBGH(stmt, "cursor found @ row # %zu in set # %zu.", stmt->rset.vrows,
		stmt->nset);

	/* reset SQLGetData state, to reset fetch position */
	STMT_GD_RESET(stmt);

	ard = stmt->ard;
	ird = stmt->ird;
	pack_json = stmt->rset.pack_json;

	DBGH(stmt, "rowset size: %zu.", ard->array_size);
	errors = 0;
	i = 0;
	/* for all rows in rowset/array (of application), iterate over rows in
	 * current resultset (of data source) */
	while (i < ard->array_size) {
		/* is there any array left in resultset? */
		empty = pack_json ? (! UJIterArray(&stmt->rset.pack.json.rows_iter,
					&stmt->rset.pack.json.row_array)) :
			cbor_value_at_end(&stmt->rset.pack.cbor.rows_iter);

		if (empty) {
			DBGH(stmt, "ran out of rows in current result set.");
			if (STMT_HAS_CURSOR(stmt)) { /* is there an ES cursor? */
				ret = EsSQLExecute(stmt);
				if (! SQL_SUCCEEDED(ret)) {
					ERRH(stmt, "failed to fetch next resultset.");
					return ret;
				}
				assert(STMT_HAS_RESULTSET(stmt));
				if (! STMT_NODATA_FORCED(stmt)) {
					/* resume copying from the new resultset, staying on the
					 * same position in rowset. */
					continue;
				}
			}
			/* no cursor and no row left in resultset array: the End. */
			DBGH(stmt, "reached end of entire result set; fetched=%zd.",
				stmt->tv_rows);
			/* indicate the non-processed rows in rowset */
			if (ird->array_status_ptr) {
				DBGH(stmt, "setting rest of %zu rows in status array to "
					"'no row' (%hu).", ard->array_size - i, SQL_ROW_NOROW);
				for (j = i; j < ard->array_size; j ++) {
					ird->array_status_ptr[j] = SQL_ROW_NOROW;
				}
			}
			/* stop the copying loop */
			break;
		}

		/* Unpack one row, then, if any columns are bound, transfer it to the
		 * application.
		 * Unpacking involves (a: JSON) iterating or (b: CBOR) parsing and
		 * iterating over the row-array and copying the respective format's
		 * object reference into the IRD records.
		 * These two are now separate steps since SQLGetData() binds/unbinds
		 * one column at a time (after SQLFetch()), which for CBOR would
		 * involve re-parsing the row for each column otherwise. */
		ret = pack_json ? unpack_one_row_json(stmt, i) :
			unpack_one_row_cbor(stmt, i);
		if (! SQL_SUCCEEDED(ret)) {
			ERRH(stmt, "fetching row %zu failed.", stmt->rset.vrows + 1);
			errors ++;
		}
		i ++;
		/* account for processed rows */
		stmt->rset.vrows ++;
		stmt->tv_rows ++;
	}

	/* return number of processed rows (even if 0) */
	if (ird->rows_processed_ptr) {
		DBGH(stmt, "setting number of processed rows to: %llu.", (uint64_t)i);
		*ird->rows_processed_ptr = i;
	}

	/* no data has been copied out */
	if (i <= 0) {
		/* ard->array_size is in interval [1, ESODBC_MAX_ROW_ARRAY_SIZE] */
		INFOH(stmt, "no data %sto return.", stmt->rset.vrows ? "left ": "");
		return SQL_NO_DATA;
	}

	if (errors && i <= errors) {
		ERRH(stmt, "processing failed for all rows [%llu].", (uint64_t)errors);
		return SQL_ERROR;
	}

	DBGH(stmt, "cursor left @ row # %zu in set # %zu.", stmt->rset.vrows,
		stmt->nset);

	/* only failures need stmt.diag defer'ing */
	return SQL_SUCCESS;
}

SQLRETURN EsSQLFetchScroll(SQLHSTMT StatementHandle,
	SQLSMALLINT FetchOrientation, SQLLEN FetchOffset)
{
	if (FetchOrientation != SQL_FETCH_NEXT) {
		ERRH(StatementHandle, "orientation %hd not supported with forward-only"
			" cursor", FetchOrientation);
		RET_HDIAGS(StatementHandle, SQL_STATE_HY106);
	}

	return EsSQLFetch(StatementHandle);
}

/*
 * data availability, call sanity checks and init'ing
 */
static SQLRETURN gd_checks(esodbc_stmt_st *stmt, SQLUSMALLINT colno)
{
	/* is there a result set? */
	if (! STMT_HAS_RESULTSET(stmt)) {
		if (STMT_NODATA_FORCED(stmt)) {
			DBGH(stmt, "empty result flag set - returning no data.");
			return SQL_NO_DATA;
		}
		ERRH(stmt, "no resultset available on statement.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}
	/* is there a block cursor bound? */
	if (1 < stmt->ard->array_size) {
		ERRH(stmt, "can't use function with block cursor "
			"(array_size=%llu).", (uint64_t)stmt->ard->array_size);
		RET_HDIAGS(stmt, SQL_STATE_HYC00);
	}
#	ifndef NDEBUG
	/* has SQLFetch() been called? DM should have detected this case */
	assert(stmt->ird->count);
	if (stmt->rset.pack_json) {
		assert(stmt->ird->recs[0].i_val.json);
	} else {
		assert(cbor_value_is_valid(&stmt->ird->recs[0].i_val.cbor));
	}
#	endif /* !NDEBUG */
	/* colno will be used as 1-based index, if not using bookmarks */
	if (colno < 1) { // TODO: add bookmark check (when implementing it)
		ERRH(stmt, "column number (%hu) can be less than 1.", colno);
		RET_HDIAGS(stmt, SQL_STATE_07009);
	}

	return SQL_SUCCESS;
}

static SQLRETURN gd_bind_col(
	esodbc_stmt_st *stmt,
	esodbc_desc_st *ard,
	SQLUSMALLINT colno,
	SQLSMALLINT target_type,
	SQLPOINTER buff_ptr,
	SQLLEN buff_len,
	SQLLEN *len_ind)
{
	SQLRETURN ret;
	SQLSMALLINT ctype;
	esodbc_rec_st *arec, *gd_arec;

	switch (target_type) {
		case SQL_ARD_TYPE:
			if (! (arec = get_record(ard, colno, /*grow?*/FALSE))) {
				ERRH(stmt, "no bound column #%hu to copy its concise type.",
					colno);
				RET_HDIAGS(stmt, SQL_STATE_07009);
			}
			ctype = arec->concise_type;
		case SQL_APD_TYPE:
			ERRH(stmt, "procedure parameters not unsupported.");
			RET_HDIAGS(stmt, SQL_STATE_HYC00);
		default:
			arec = NULL;
			ctype = target_type;
	}

	/* bind the column */
	ret = EsSQLBindCol(stmt, colno, ctype, buff_ptr, buff_len, len_ind);
	if (! SQL_SUCCEEDED(ret)) {
		return ret;
	}

	/* If target is SQL_ARD_TYPE, use "SQL_DESC_DATETIME_INTERVAL_PRECISION,
	 * SQL_DESC_PRECISION, and SQL_DESC_SCALE fields of the ARD" */
	if (arec) {
		if (! (gd_arec = get_record(stmt->ard, colno, FALSE))) {
			BUGH(stmt, "can't fetch just set record.");
			RET_HDIAG(stmt, SQL_STATE_HY000, "BUG: failed to prepare "
				"SQLGetData call", 0);
		}
		gd_arec->datetime_interval_precision =
			arec->datetime_interval_precision;
		gd_arec->precision = arec->precision;
		gd_arec->scale = arec->scale;
	}

	return SQL_SUCCESS;
}

SQLRETURN EsSQLGetData(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT ColumnNumber,
	SQLSMALLINT TargetType,
	_Out_writes_opt_(_Inexpressible_(BufferLength)) SQLPOINTER TargetValuePtr,
	SQLLEN BufferLength,
	_Out_opt_ SQLLEN *StrLen_or_IndPtr)
{
	SQLRETURN ret;
	esodbc_desc_st *ard, gd_ard;
	esodbc_rec_st arecs[ESODBC_GD_DESC_COUNT];
	esodbc_stmt_st *stmt = STMH(StatementHandle);

	if (stmt->gd_col == ColumnNumber && stmt->gd_ctype == TargetType) {
		DBGH(stmt, "resuming get on column #%hu (pos @ %lld).",
			stmt->gd_col, (int64_t)stmt->gd_offt);
		if (stmt->gd_offt < 0) {
			WARNH(stmt, "data for current column exhausted.");
			return SQL_NO_DATA;
		}
	} else {
		if (0 <= stmt->gd_col) {
			DBGH(stmt, "previous source column #%hu (pos @ %lld), SQL C %hd "
				"abandoned for new #%hu, SQL C %hd.", stmt->gd_col,
				(int64_t)stmt->gd_offt, stmt->gd_ctype, ColumnNumber,
				TargetType);
			/* reset fields now, should the call eventually fail */
			STMT_GD_RESET(stmt);
		} else {
			DBGH(stmt, "prep. column #%hu as the new data src.", ColumnNumber);
		}
		ret = gd_checks(stmt, ColumnNumber);
		if (! SQL_SUCCEEDED(ret)) {
			return ret;
		}

		stmt->gd_col = ColumnNumber;
		stmt->gd_ctype = TargetType;
	}
	/* data is available */

	/* save stmt's current ARD before overwriting it */
	ard = getdata_set_ard(stmt, &gd_ard, ColumnNumber, arecs,
			sizeof(arecs)/sizeof(arecs[0]));
	if (! ard) {
		BUGH(stmt, "failed to prepare GD ARD.");
		RET_HDIAG(stmt, SQL_STATE_HY000,
			"BUG: failed to prepare SQLGetData call", 0);
	}

	ret = gd_bind_col(stmt, ard, ColumnNumber, TargetType, TargetValuePtr,
			BufferLength, StrLen_or_IndPtr);
	if (! SQL_SUCCEEDED(ret)) {
		goto end;
	}

	/* check if data types are compatible/convertible */
	ret = convertability_check(stmt, -ColumnNumber, NULL);
	if (! SQL_SUCCEEDED(ret)) {
		goto end;
	}

	/* copy the data */
	ret = stmt->rset.pack_json ? copy_one_row_json(stmt, 0) :
		copy_one_row_cbor(stmt, 0);
	if (! SQL_SUCCEEDED(ret)) {
		goto end;
	}

	DBGH(stmt, "succesfully copied data from column #%hu (pos @ %lld), "
		"SQL C %hd.", ColumnNumber, (int64_t)stmt->gd_offt, TargetType);
end:
	/* XXX: if get_record(gd_ard, ColumnNumber)->meta_type != string/bin,
	 * should stmt->gd_offt bet set to -1 ?? */
	/* reinstate previous ARD */
	getdata_reset_ard(stmt, ard, ColumnNumber, arecs,
		sizeof(arecs)/sizeof(arecs[0]));
	if (! SQL_SUCCEEDED(ret)) {
		/* if function call failed, reset GD state */
		STMT_GD_RESET(stmt);
	}
	return ret;
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
		// check ESODBC_GETDATA_EXTENSIONS (SQL_GD_BLOCK) when implementing
		/* setting the position with a read-only forward-only cursor would
		 * only be useful with SQLGetData(). Since SQLFetch/Scroll() must
		 * be called before this function and because SQLFetch() frees an
		 * old result before getting a new one from ES/SQL to fill in the
		 * resultset, positioning the cursor within the result set with
		 * SQLSetPos() would require SQLFetch() to duplicate the resultset
		 * in memory every time, just for the case that
		 * SQLSetPos()+SQLGetData() might be used. This is just not worthy:
		 * - don't advertise SQL_GD_BLOCK; which should then allow to:
		 * - not support SQL_POSITION.
		 */
		// no break;

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

SQLRETURN EsSQLMoreResults(SQLHSTMT hstmt)
{
	INFOH(hstmt, "multiple result sets not supported.");
	return SQL_NO_DATA;
}

static SQLRETURN close_es_handler_json(esodbc_stmt_st *stmt, cstr_st *body,
	BOOL *closed)
{
	UJObject obj, succeeded;
	void *state = NULL;
	int unpacked;

	const wchar_t *keys[] = {
		MK_WPTR(PACK_PARAM_CURS_CLOSE)
	};

	obj = UJDecode(body->str, body->cnt, NULL, &state);
	if (! obj) {
		ERRH(stmt, "failed to decode JSON answer: %s ([%zu] `" LTPDL "`).",
			state ? UJGetError(state) : "<none>", body->cnt, LCSTR(body));
		goto err;
	}
	unpacked = UJObjectUnpack(obj, 1, "B", keys, &succeeded);
	if (unpacked < 1) {
		ERRH(stmt, "failed to unpack JSON answer: %s ([%zu] `" LCPDL "`).",
			UJGetError(state), body->cnt, LCSTR(body));
		goto err;
	}
	switch (UJGetType(succeeded)) {
		case UJT_False:
			*closed = FALSE;
			break;
		case UJT_True:
			*closed = TRUE;
			break;
		default:
			ERRH(stmt, "invalid obj type in answer: %d ([%zu] `" LTPDL "`).",
				UJGetType(succeeded), body->cnt, LCSTR(body));
			goto err;
	}

	return SQL_SUCCESS;
err:
	RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
}

static SQLRETURN close_es_handler_cbor(esodbc_stmt_st *stmt, cstr_st *body,
	BOOL *closed)
{
	CborError res;
	CborParser parser;
	CborType obj_type;
	CborValue top_obj, succ_obj;
	bool boolval;

	res = cbor_parser_init(body->str, body->cnt, ES_CBOR_PARSE_FLAGS,
			&parser, &top_obj);
	CHK_RES(stmt, "failed to init CBOR parser for object: [%zu] `%s`",
		body->cnt, cstr_hex_dump(body));
#	ifndef NDEBUG
#	if 0 // ES uses indefinite-length containers (TODO) which trips this check
	/* the _init() doesn't actually validate the object */
	res = cbor_value_validate(&top_obj, ES_CBOR_PARSE_FLAGS);
	CHK_RES(stmt, "failed to validate CBOR object: [%zu] `%s`",
		body->cnt, cstr_hex_dump(body));
#	endif /*0*/
#	endif /* !NDEBUG */

	if ((obj_type = cbor_value_get_type(&top_obj)) != CborMapType) {
		ERRH(stmt, "top object (of type 0x%x) is not a map.", obj_type);
		goto err;
	}
	res = cbor_value_map_find_value(&top_obj, PACK_PARAM_CURS_CLOSE,
			&succ_obj);
	CHK_RES(stmt, "failed to get '" PACK_PARAM_CURS_CLOSE "' object in answ.");
	if ((obj_type = cbor_value_get_type(&succ_obj)) != CborBooleanType) {
		ERRH(stmt, "object '" PACK_PARAM_CURS_CLOSE "' (of type 0x%x) is not a"
			" boolean.", obj_type);
		goto err;
	}
	res = cbor_value_get_boolean(&succ_obj, &boolval);
	CHK_RES(stmt, "failed to extract boolean value");
	*closed = boolval;
	return SQL_SUCCESS;
err:
	RET_HDIAG(stmt, SQL_STATE_HY000, MSG_INV_SRV_ANS, 0);
}

SQLRETURN close_es_answ_handler(esodbc_stmt_st *stmt, cstr_st *body,
	BOOL is_json)
{
	SQLRETURN ret;
	BOOL closed;

	ret = is_json ? close_es_handler_json(stmt, body, &closed) :
		close_es_handler_cbor(stmt, body, &closed);

	free(body->str);

	if (! SQL_SUCCEEDED(ret)) {
		return ret;
	}

	if (! closed) {
		ERRH(stmt, "failed to close cursor on server side.");
		assert(0);
		/* indicate success: not a driver/client error -- server would answer
		 * with an error answer if that'd be the case */
	}
	DBGH(stmt, "cursor closed on server side.");

	return SQL_SUCCESS;
}

SQLRETURN close_es_cursor(esodbc_stmt_st *stmt)
{
	SQLRETURN ret;
	char buff[ESODBC_BODY_BUF_START_SIZE];
	cstr_st body = {buff, sizeof(buff)};

	if (! STMT_HAS_CURSOR(stmt)) {
		DBGH(stmt, "no cursor to close.");
	}

	ret = serialize_statement(stmt, &body);
	if (SQL_SUCCEEDED(ret)) {
		ret = curl_post(stmt, ESODBC_CURL_CLOSE, &body);
	}

	if (buff != body.str) {
		free(body.str);
	}

#	ifndef NDEBUG
	/* the actual cursor freeing occurs in clear_resultset() (part of
	 * UJSON4C's state or the actual response body, in case of CBOR) */
	if (stmt->rset.pack_json) {
		DBGH(stmt, "clearing JSON cursor: [%zd] `" LWPDL "`.",
			stmt->rset.pack.json.curs.cnt, LWSTR(&stmt->rset.pack.json.curs));
	} else {
		DBGH(stmt, "clearing CBOR cursor: [%zd] `%s`.",
			stmt->rset.pack.cbor.curs.cnt,
			cstr_hex_dump(&stmt->rset.pack.cbor.curs));
	}
	/* clear both possible cursors in union: next cursor could be received
	 * over different encapsulation than the one in current result set */
	stmt->rset.pack.json.curs.cnt = 0;
	stmt->rset.pack.cbor.curs.cnt = 0;
#	endif /* NDEBUG */

	return ret;
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

SQLRETURN EsSQLCancel(SQLHSTMT StatementHandle)
{
	esodbc_stmt_st *stmt = STMH(StatementHandle);

	/*
	 * Use cases:
	 * - "A function running asynchronously on the statement.": no async
	 *   support.
	 * - "A function on a statement that needs data." TODO: if data-at-exec.
	 * - "A function running on the statement on another thread.": this could
	 *   theoretically cancel an ongoing fetch/connect/etc. For now libcurl is
	 *   left to timeout -- TODO: if swiching to "multi" API in libcurl.
	 *   XXX: for this last case: stmt lock is being held here.
	 */

	DBGH(stmt, "canceling current statement operation -- NOOP.");
	return SQL_SUCCESS;
}

SQLRETURN EsSQLCancelHandle(SQLSMALLINT HandleType, SQLHANDLE InputHandle)
{
	/* see EsSQLCancel() */
	DBGH(InputHandle, "canceling current handle operation -- NOOP.");
	return SQL_SUCCESS;
}

SQLRETURN EsSQLEndTran(SQLSMALLINT HandleType, SQLHANDLE Handle,
	SQLSMALLINT CompletionType)
{
	WARNH(Handle, "transaction ending requested (%hd), despite no "
		"transactional support advertized", CompletionType);
	if (CompletionType == SQL_ROLLBACK) {
		RET_HDIAGS(Handle, SQL_STATE_HYC00);
	}
	return SQL_SUCCESS;
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
	SQLSMALLINT markers;

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

	ret = attach_sql(stmt, szSqlStr, cchSqlStr);
	/* if early execution mode is on and the statement has no parameter
	 * markers, execute the query right away */
	if (HDRH(stmt)->dbc->early_exec && SQL_SUCCEEDED(ret)) {
		assert(! stmt->early_executed); /* cleared by now */
		if (! SQL_SUCCEEDED(count_param_markers(stmt, &markers))) {
			ERRH(stmt, "failed to count parameter markers in query. "
				"Early execution disabled.");
			/* clear set diagnostic */
			init_diagnostic(&HDRH(stmt)->diag);
			return ret;
		}
		if (0 < markers) {
			INFOH(stmt, "query contains %hd parameter markers -- early "
				"execution disabled.", markers);
			return ret;
		}
		ret = EsSQLExecute(hstmt);
		if (SQL_SUCCEEDED(ret)) {
			stmt->early_executed = true;
		}
	}
	return ret;
}


/* Find the ES/SQL type given in es_type; for ID matching multiple types
 * (scaled_float/double), but not  keyword/text, use the best matching
 * col_size, which is the smallest, that's still matching (<=) the given one.
 * This assumes the types are ordered by it (as per the spec). */
esodbc_estype_st *lookup_es_type(esodbc_dbc_st *dbc,
	SQLSMALLINT es_type, SQLULEN col_size)
{
	SQLULEN i;
	SQLINTEGER sz;

	/* for strings, choose text straight away: some type (IP, GEO) must coform
	 * to a format and no content inspection is done in the driver */
	if (es_type == ES_VARCHAR_SQL || es_type == ES_WVARCHAR_SQL) {
		return dbc->max_varchar_type;
	}
	for (i = 0; i < dbc->no_types; i ++) {
		if (dbc->es_types[i].data_type == es_type) {
			if (col_size <= 0) {
				return &dbc->es_types[i];
			} else {
				sz = dbc->es_types[i].column_size;
				assert(col_size < LONG_MAX);
				if ((SQLINTEGER)col_size <= sz) {
					return &dbc->es_types[i];
				}
				if (es_type == SQL_DOUBLE &&
					sz == dbc->max_float_type->column_size) {
					return dbc->max_float_type;
				}
			}
		}
	}
	WARNH(dbc, "no ES/SQL type found for ID %hd and column size %hd.",
		es_type, col_size);
	return NULL;
}

/* find the matching ES/SQL type for app's SQL type, which can be an exact
 * match against ES/SQL types, but also some other valid SQL type. */
static esodbc_estype_st *match_es_type(esodbc_rec_st *irec)
{
	SQLULEN i;
	esodbc_dbc_st *dbc = irec->desc->hdr.stmt->hdr.dbc;

	for (i = 0; i < dbc->no_types; i ++) {
		if (dbc->es_types[i].data_type == irec->concise_type) {
			switch (irec->concise_type) {
				case SQL_DOUBLE: /* DOUBLE, SCALED_FLOAT */
					return dbc->max_float_type;
					break;
				case ES_WVARCHAR_SQL: /* KEYWORD, TEXT */
				case ES_VARCHAR_SQL: /* IP, GEO+ */
					return dbc->max_varchar_type;
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
			return lookup_es_type(dbc, SQL_DOUBLE, irec->precision);

		case METATYPE_STRING:
			return lookup_es_type(dbc, ES_TEXT_TO_SQL, irec->precision);
		case METATYPE_BIN:
			return lookup_es_type(dbc, SQL_BINARY, /*no prec*/0);
		case METATYPE_DATE_TIME:
			assert(irec->concise_type == SQL_TYPE_DATE ||
				irec->concise_type == SQL_TYPE_TIME);
			return lookup_es_type(dbc, SQL_TYPE_TIMESTAMP, /*no prec*/0);
		case METATYPE_BIT:
			return lookup_es_type(dbc, ES_BOOLEAN_TO_SQL, /*no prec*/0);
		case METATYPE_UID:
			return lookup_es_type(dbc, ES_TEXT_TO_SQL, /*no prec: TEXT*/0);

		case METATYPE_INTERVAL_WSEC:
		case METATYPE_INTERVAL_WOSEC:
		case METATYPE_FLOAT_NUMERIC: /* these should have matched already */
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
 * set_defaults_from_meta_type(), to meet the "Other fields implicitly set"
 * requirements from the page linked in set_defaults_from_meta_type() comments.
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
	/* ParameterNumber will be used as 1-based index */
	if (ParameterNumber < 1) {
		ERRH(stmt, "param. no (%hd) can't be less than 1.", ParameterNumber);
		RET_HDIAGS(stmt, SQL_STATE_07009);
	}

	/* Note: "If StrLen_or_IndPtr is a null pointer, the driver assumes that
	 * all input parameter values are non-NULL and that character and binary
	 * data is null-terminated." */
	if (StrLen_or_IndPtr) {
		if (*StrLen_or_IndPtr == SQL_DATA_AT_EXEC ||
			*StrLen_or_IndPtr < SQL_NTSL) {
			ERRH(stmt, "data-at-exec not supported (LenInd=%lld).",
				(int64_t)*StrLen_or_IndPtr);
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
	ret = convertability_check(stmt, (SQLINTEGER)ParameterNumber, NULL);
	if (! SQL_SUCCEEDED(ret)) {
		goto err;
	}

	arec = get_record(stmt->apd, ParameterNumber, /*grow?*/FALSE);
	irec->es_type = match_es_type(irec);
	if (! irec->es_type) {
		/* validation shoudl have been done earlier on meta type setting
		 * (SQL_DESC_CONCISE_TYPE) */
		BUGH(stmt, "failed to match valid SQL type %hd.", irec->concise_type);
		SET_HDIAG(desc, SQL_STATE_HY000, "parameter binding bug", 0);
		goto copy_diag;
	}
	DBGH(stmt, "SQL type %hd matched to `" LCPDL "` ES/SQL type.",
		ParameterType, LCSTR(&irec->es_type->type_name_c));

	DBGH(stmt, "succesfully bound parameter #%hu, IO-type: %hd, "
		"SQL C type: %hd, SQL type: %hd, size: %llu, decdigits: %hd, "
		"buffer@0x%p, length: %lld, LenInd@0x%p.", ParameterNumber,
		InputOutputType, ValueType, ParameterType, (uint64_t)ColumnSize,
		DecimalDigits, ParameterValuePtr, (int64_t)BufferLength,
		StrLen_or_IndPtr);

	return SQL_SUCCESS;

copy_diag:
	/* copy initial error at top handle level, where it's going to be "
	 * "inquired from (more might occur below) */
	HDIAG_COPY(desc, stmt);
err:
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

		case SQL_BIT: /* BIT */
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
		case SQL_FLOAT: /* HALF_FLOAT */
		case SQL_DOUBLE: /* DOUBLE, SCALED_FLOAT */
			min = DBL_MIN;
			max = DBL_MAX;
			fixed = FALSE;
			break;
		} while (0);
			return c2sql_number(arec, irec, pos, &min, &max, fixed, dest, len);

		/* JSON string */
		case ES_WVARCHAR_SQL: /* KEYWORD, TEXT */
		case ES_VARCHAR_SQL: /* IP, GEO+ */
			return c2sql_varchar(arec, irec, pos, dest, len);

		case SQL_TYPE_DATE:
		case SQL_TYPE_TIME:
		case SQL_TYPE_TIMESTAMP:
			return c2sql_date_time(arec, irec, pos, dest, len);

		case SQL_INTERVAL_YEAR:
		case SQL_INTERVAL_MONTH:
		case SQL_INTERVAL_DAY:
		case SQL_INTERVAL_HOUR:
		case SQL_INTERVAL_MINUTE:
		case SQL_INTERVAL_SECOND:
		case SQL_INTERVAL_YEAR_TO_MONTH:
		case SQL_INTERVAL_DAY_TO_HOUR:
		case SQL_INTERVAL_DAY_TO_MINUTE:
		case SQL_INTERVAL_DAY_TO_SECOND:
		case SQL_INTERVAL_HOUR_TO_MINUTE:
		case SQL_INTERVAL_HOUR_TO_SECOND:
		case SQL_INTERVAL_MINUTE_TO_SECOND:
			return c2sql_interval(arec, irec, pos, dest, len);

		/* JSON (Base64 encoded) string */
		case SQL_BINARY: /* BINARY */
			// XXX: json_escape
			ERRH(stmt, "conversion to SQL BINARY not implemented.");
			RET_HDIAG(stmt, SQL_STATE_HYC00, "conversion to SQL BINARY "
				"not yet supported", 0);

		default:
			BUGH(arec->desc->hdr.stmt, "unexpected ES/SQL type %hd.",
				irec->es_type->data_type);
			RET_HDIAG(arec->desc->hdr.stmt, SQL_STATE_HY000,
				"parameter conversion bug", 0);
	}
	/*INDENT-ON*/

	assert(0);
	return SQL_SUCCESS;
}

static SQLRETURN get_param_recs(esodbc_stmt_st *stmt, SQLSMALLINT no,
	esodbc_rec_st **arec, esodbc_rec_st **irec)
{
	esodbc_rec_st *a, *i;

	a = get_record(stmt->apd, no, /*grow?*/FALSE);
	if (a && REC_IS_BOUND(a)) {
		*arec = a;
	} else {
		ERRH(stmt, "APD record #%hd @0x%p not bound", no, a);
		RET_HDIAG(stmt, SQL_STATE_07002, "Invalid parameter configuration", 0);
	}

	i = get_record(stmt->ipd, no, /*grow?*/FALSE);
	if (i && i->es_type) {
		*irec = i;
	} else {
		ERRH(stmt, "IPD record #%hd @0x%p not configured", no, i);
		/* possibly "manually" configured param (i.e. no SQLBindParam use) */
		RET_HDIAG(stmt, SQL_STATE_07002, "Invalid parameter configuration", 0);
	}

	return SQL_SUCCESS;
}

/* Forms the JSON array with params:
 * [{"type": "<ES/SQL type name>", "value": <param value>}(,etc)*] */
static SQLRETURN serialize_params_json(esodbc_stmt_st *stmt, char *dest,
	size_t *len)
{
	/* JSON keys for building one parameter object */
	const static cstr_st j_type = CSTR_INIT("{\"" REQ_KEY_PARAM_TYPE "\": \"");
	const static cstr_st j_val = CSTR_INIT("\", \"" REQ_KEY_PARAM_VAL "\": ");
	esodbc_rec_st *arec, *irec;
	SQLRETURN ret;
	SQLSMALLINT i, count;
	size_t l, pos;

	pos = 0;
	if (dest) {
		dest[pos] = '[';
	}
	pos ++;

	/* some apps set/reset various parameter record attributes (maybe to
	 * workaround for some driver issues? like Linked Servers), which increaes
	 * the record count, but (1) do not bind the parameter and (2) do not use
	 * the standard mechanism (through SQL_DESC_ARRAY_STATUS_PTR) to signal
	 * params to ignore. */
	count = count_bound(stmt->apd);
	for (i = 0; i < count; i ++) {
		ret = get_param_recs(stmt, i + 1, &arec, &irec);
		if (! SQL_SUCCEEDED(ret)) {
			return ret;
		}

		if (dest) {
			if (i) {
				memcpy(dest + pos, ", ", 2);
			}
			/* copy 'type' JSON key name */
			memcpy(dest + pos + 2 * !!i, j_type.str, j_type.cnt);
		}
		pos += 2 * !!i + j_type.cnt;

		/* copy/eval ES/SQL type name */
		pos += json_escape(irec->es_type->type_name_c.str,
				irec->es_type->type_name_c.cnt, dest ? dest + pos : NULL,
				/*"unlimited" output buffer*/(size_t)-1);

		if (dest) {
			/* copy 'value' JSON key name */
			memcpy(dest + pos, j_val.str, j_val.cnt);
		}
		pos += j_val.cnt;

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
}

static SQLRETURN statement_len_cbor(esodbc_stmt_st *stmt, size_t *enc_len,
	size_t *conv_len, size_t *keys)
{
	SQLRETURN ret;
	size_t bodylen, len, curslen;
	esodbc_dbc_st *dbc = HDRH(stmt)->dbc;

	/* Initial all-encompassing map preamble. */
	bodylen = cbor_nn_hdr_len(REST_REQ_KEY_COUNT); /* max count */

	*keys = 1; /* cursor or query */
	curslen = STMT_HAS_CURSOR(stmt);
	if (curslen) { /* eval CURSOR object length */
		bodylen += cbor_str_obj_len(sizeof(REQ_KEY_CURSOR) - 1);
		bodylen += cbor_str_obj_len(curslen);
		if (stmt->rset.pack_json) {
			/* if current result set was delivered as JSON, we'll need to
			 * convert the UTF16-stored cursor to ASCII (see note in
			 * statement_len_json()); for that, we'll use the serialization
			 * buffer, writing in it ahead of where the cursor will be
			 * CBOR-packed.  The offset must be at least equal to the CBOR
			 * string object header's length. */
			bodylen += cbor_nn_hdr_len(curslen);
		}
	} else { /* eval QUERY object length */
		bodylen += cbor_str_obj_len(sizeof(REQ_KEY_QUERY) - 1);
		bodylen += cbor_str_obj_len(stmt->u8sql.cnt);

		/* does the statement have any bound parameters? */
		if (count_bound(stmt->apd)) {
			bodylen += cbor_str_obj_len(sizeof(REQ_KEY_PARAMS) - 1);

			ret = statement_params_len_cbor(stmt, &len, conv_len);
			if (! SQL_SUCCEEDED(ret)) {
				ERRH(stmt, "failed to eval parameters length");
				return ret;
			}
			bodylen += len;
			(*keys) ++;
		}

		/* does the statement have any fetch_size? */
		if (dbc->fetch.slen) {
			bodylen += cbor_str_obj_len(sizeof(REQ_KEY_FETCH) - 1);
			bodylen += CBOR_INT_OBJ_LEN(dbc->fetch.max);
			(*keys) ++;
		}
		/* "field_multi_value_leniency": true/false */
		bodylen += cbor_str_obj_len(sizeof(REQ_KEY_MULTIVAL) - 1);
		bodylen += CBOR_OBJ_BOOL_LEN;
		(*keys) ++;
		/* "index_include_frozen": true/false */
		bodylen += cbor_str_obj_len(sizeof(REQ_KEY_IDX_FROZEN) - 1);
		bodylen += CBOR_OBJ_BOOL_LEN;
		(*keys) ++;
		/* "time_zone": "-05:45" */
		bodylen += cbor_str_obj_len(sizeof(REQ_KEY_TIMEZONE) - 1);
		bodylen += cbor_str_obj_len(tz_param.cnt); /* lax len */
		(*keys) ++;
		/* "catalog": "my_cluster" */
		if (dbc->catalog.c.cnt) {
			bodylen += cbor_str_obj_len(sizeof(REQ_KEY_CATALOG) - 1);
			bodylen += cbor_str_obj_len(dbc->catalog.c.cnt);
			(*keys) ++;
		}
		bodylen += cbor_str_obj_len(sizeof(REQ_KEY_VERSION) - 1);
		bodylen += cbor_str_obj_len(version.cnt);
		(*keys) ++;
	}
	/* mode */
	bodylen += cbor_str_obj_len(sizeof(REQ_KEY_MODE) - 1);
	bodylen += cbor_str_obj_len(sizeof(REQ_VAL_MODE) - 1);
	(*keys) ++;
	/* client_id */
	bodylen += cbor_str_obj_len(sizeof(REQ_KEY_CLT_ID) - 1);
	bodylen += cbor_str_obj_len(sizeof(REQ_VAL_CLT_ID) - 1);
	(*keys) ++;
	/* binary_format */
	bodylen += cbor_str_obj_len(sizeof(REQ_KEY_BINARY_FMT) - 1);
	bodylen += CBOR_OBJ_BOOL_LEN;
	(*keys) ++;
	/* TODO: request_/page_timeout */

	assert(*keys <= REST_REQ_KEY_COUNT);
	*enc_len = bodylen;
	return SQL_SUCCESS;
}

static SQLRETURN statement_len_json(esodbc_stmt_st *stmt, size_t *outlen)
{
	SQLRETURN ret;
	size_t bodylen, len, curslen;
	esodbc_dbc_st *dbc = HDRH(stmt)->dbc;

	bodylen = 1; /* { */
	curslen = STMT_HAS_CURSOR(stmt);
	/* evaluate how long the stringified REST object will be */
	if (curslen) { /* eval CURSOR object length */
		/* assumptions: (1) the cursor is a Base64 encoded string and thus
		 * (2) no JSON escaping needed.
		 * (both assumptions checked on copy, in serialize_to_json()). */
		bodylen += sizeof(JSON_KEY_CURSOR) - 1; /* "cursor":  */
		bodylen += curslen;
		bodylen += 2; /* 2x `"` for cursor value */
	} else { /* eval QUERY object length */
		bodylen += sizeof(JSON_KEY_QUERY) - 1;
		bodylen += json_escape(stmt->u8sql.str, stmt->u8sql.cnt, NULL, 0);
		bodylen += 2; /* 2x `"` for query value */

		/* does the statement have any bound parameters? */
		if (count_bound(stmt->apd)) {
			bodylen += sizeof(JSON_KEY_PARAMS) - 1;
			/* serialize_params_json will count/copy array delims (`[`, `]`) */
			ret = serialize_params_json(stmt, /* no copy, just eval */NULL,
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
		/* "field_multi_value_leniency": true/false */
		bodylen += sizeof(JSON_KEY_MULTIVAL) - 1;
		bodylen += /*false*/5;
		/* "index_include_frozen": true/false */
		bodylen += sizeof(JSON_KEY_IDX_FROZEN) - 1;
		bodylen += /*false*/5;
		/* "time_zone": "-05:45" */
		bodylen += sizeof(JSON_KEY_TIMEZONE) - 1;
		bodylen += tz_param.cnt;
		/* "catalog": "my_cluster" */
		if (dbc->catalog.c.cnt) {
			bodylen += sizeof(JSON_KEY_CATALOG) - 1;
			bodylen += dbc->catalog.c.cnt;
			bodylen += /* 2x `"` */2;
		}
		/* "version": */
		bodylen += sizeof(JSON_KEY_VERSION) - 1;
		bodylen += version.cnt + /* 2x`"` */2;
	}
	bodylen += sizeof(JSON_KEY_VAL_MODE) - 1; /* "mode": */
	bodylen += sizeof(JSON_KEY_CLT_ID) - 1; /* "client_id": */
	bodylen += sizeof(JSON_KEY_BINARY_FMT) - 1; /* "binary_format": false */
	bodylen += sizeof("false") - 1;
	/* TODO: request_/page_timeout */
	bodylen += 1; /* } */

	*outlen = bodylen;
	return SQL_SUCCESS;
}

#define FAIL_ON_CBOR_ERR(_hnd, _cbor_err) \
	do { \
		if (_cbor_err != CborNoError) { \
			ERRH(_hnd, "CBOR: %s.", cbor_error_string(_cbor_err)); \
			RET_HDIAG(_hnd, SQL_STATE_HY000, "CBOR serialization error", \
				_cbor_err); \
		} \
	} while (0)

static SQLRETURN statement_params_len_cbor(esodbc_stmt_st *stmt,
	size_t *enc_len, size_t *conv_len)
{
	SQLSMALLINT i;
	size_t len, l, max;
	esodbc_rec_st *arec, *irec;
	SQLRETURN ret;

	/* Initial all-encompassing array preamble. */
	/* ~ [] */
	len = cbor_nn_hdr_len(stmt->apd->count);

	max = 0;
	for (i = 0; i < stmt->apd->count; i ++) {
		assert(stmt->ipd->count == stmt->apd->count);
		arec = &stmt->apd->recs[i];
		irec = &stmt->ipd->recs[i];

		/* ~ {} */
		len = cbor_nn_hdr_len(stmt->apd->count);

		/* ~ "type": "..." */
		len += cbor_str_obj_len(sizeof(REQ_KEY_PARAM_TYPE) - 1);
		len += cbor_str_obj_len(irec->es_type->type_name.cnt);

		/* ~ "value": "..." */
		len += cbor_str_obj_len(sizeof(REQ_KEY_PARAM_VAL) - 1);
		assert(irec->es_type);

		/* assume quick maxes */
		ret = convert_param_val(arec, irec, /*params array pos*/0,
				/*dest: calc length*/NULL, &l);
		if (! SQL_SUCCEEDED(ret)) {
			return ret;
		}
		/* keep maximum space required for storing the converted obj */
		if (max < l) {
			max = l;
		}
		/* the values are going to be sent as strings
		 * (see serialize_param_cbor() note) */
		len += cbor_str_obj_len(l);
	}

	*conv_len = max;
	*enc_len = len;
	return SQL_SUCCESS;
}

/* Note: this implementation will encode numeric SQL types as strings (as it's
 * reusing the JSON converters). This is somewhat negating CBOR's intentions,
 * but: (1) it's a simplified and tested implementation; (2) the overall
 * performance impact is negligible with this driver's currently intended
 * usage pattern (SELECTs only, fetching data volume far outweighing that of
 * the queries); (3) the server will convert the received value according to
 * the correctly indicated type. XXX */
static SQLRETURN serialize_param_cbor(esodbc_rec_st *arec,
	esodbc_rec_st *irec, CborEncoder *pmap, size_t conv_len)
{
	SQLRETURN ret;
	CborError res;
	size_t len;
	SQLLEN *ind_ptr;
	size_t skip_quote;
	static SQLULEN param_array_pos = 0; /* parames array not yet supported */
	esodbc_stmt_st *stmt = HDRH(arec->desc)->stmt;

	ind_ptr = deferred_address(SQL_DESC_INDICATOR_PTR, param_array_pos, arec);
	if (ind_ptr && *ind_ptr == SQL_NULL_DATA) {
		res = cbor_encode_null(pmap);
		FAIL_ON_CBOR_ERR(stmt, res);
		return SQL_SUCCESS;
	}
	/* from here on, "input parameter value[ is] non-NULL" */
	assert(deferred_address(SQL_DESC_DATA_PTR, param_array_pos, arec));

	/* the pmap->end is the start of the conversion buffer */
	ret = convert_param_val(arec, irec, param_array_pos, (char *)pmap->end,
			&len);
	if (! SQL_SUCCEEDED(ret)) {
		return ret;
	}
	assert(len <= conv_len);

	/* need to skip the leading and trailing `"`? */
	switch (irec->es_type->meta_type) {
		case METATYPE_EXACT_NUMERIC:
		case METATYPE_FLOAT_NUMERIC:
		case METATYPE_BIT:
		case METATYPE_BIN:
			skip_quote = 0;
			break;

		case METATYPE_STRING:
		case METATYPE_DATE_TIME:
		case METATYPE_INTERVAL_WSEC:
		case METATYPE_INTERVAL_WOSEC:
		case METATYPE_UID:
			skip_quote = 1;
			break;

		case METATYPE_MAX: /*DEFAULT, NULL*/
		case METATYPE_UNKNOWN:
		default:
			BUGH(stmt, "unexpected SQL meta %d / type %d.",
				irec->es_type->meta_type, irec->es_type->data_type);
			RET_HDIAGS(stmt, SQL_STATE_HY000);
	}
	res = cbor_encode_text_string(pmap, pmap->end + skip_quote,
			len - 2 * skip_quote);
	FAIL_ON_CBOR_ERR(stmt, res);

	return SQL_SUCCESS;
}

static SQLRETURN serialize_params_cbor(esodbc_stmt_st *stmt, CborEncoder *map,
	size_t conv_len)
{
	const static cstr_st p_type = CSTR_INIT(REQ_KEY_PARAM_TYPE);
	const static cstr_st p_val = CSTR_INIT(REQ_KEY_PARAM_VAL);
	SQLSMALLINT i, count;
	CborError res;
	SQLRETURN ret;
	CborEncoder array; /* array for all params */
	CborEncoder pmap; /* map for one param */
	esodbc_rec_st *arec, *irec;

	/* ~ [ */
	res = cbor_encoder_create_array(map, &array, stmt->apd->count);
	FAIL_ON_CBOR_ERR(stmt, res);

	/* see note in serialize_params_json() */
	count = count_bound(stmt->apd);
	for (i = 0; i < count; i ++) {
		ret = get_param_recs(stmt, i + 1, &arec, &irec);
		if (! SQL_SUCCEEDED(ret)) {
			return ret;
		}

		/* ~ { */
		res = cbor_encoder_create_map(&array, &pmap, /* type + value = */2);
		FAIL_ON_CBOR_ERR(stmt, res);

		/*
		 * ~ "type": "..."
		 */
		res = cbor_encode_text_string(&pmap, p_type.str, p_type.cnt);
		FAIL_ON_CBOR_ERR(stmt, res);

		assert(irec->es_type);
		res = cbor_encode_text_string(&pmap, irec->es_type->type_name_c.str,
				irec->es_type->type_name_c.cnt);
		FAIL_ON_CBOR_ERR(stmt, res);

		/*
		 * ~ "value": "..."
		 */
		res = cbor_encode_text_string(&pmap, p_val.str, p_val.cnt);
		FAIL_ON_CBOR_ERR(stmt, res);

		ret = serialize_param_cbor(arec, irec, &pmap, conv_len);
		if (! SQL_SUCCEEDED(ret)) {
			ERRH(stmt, "converting parameter #%hd failed.", i + 1);
			return ret;
		}

		/* ~ } */
		res = cbor_encoder_close_container(&array, &pmap);
		FAIL_ON_CBOR_ERR(stmt, res);
	}

	/* ~ ] */
	res = cbor_encoder_close_container(map, &array);
	FAIL_ON_CBOR_ERR(stmt, res);

	return SQL_SUCCESS;
}

static SQLRETURN serialize_to_cbor(esodbc_stmt_st *stmt, cstr_st *dest,
	size_t conv_len, size_t keys)
{
	CborEncoder encoder, map;
	CborError err;
	cstr_st tz, curs;
	esodbc_dbc_st *dbc = HDRH(stmt)->dbc;
	size_t dest_cnt;

	assert(conv_len < dest->cnt);
	cbor_encoder_init(&encoder, dest->str, dest->cnt - conv_len, /*flags*/0);
	err = cbor_encoder_create_map(&encoder, &map, keys);
	FAIL_ON_CBOR_ERR(stmt, err);

	if (STMT_HAS_CURSOR(stmt)) { /* copy CURSOR object */
		err = cbor_encode_text_string(&map, REQ_KEY_CURSOR,
				sizeof(REQ_KEY_CURSOR) - 1);
		FAIL_ON_CBOR_ERR(stmt, err);
		if (stmt->rset.pack_json) {
			/* calculate the location where to convert the UTF16 cursor to its
			 * UTF8 (even ASCII in this case) representation */
			curs.cnt = stmt->rset.pack.json.curs.cnt;
			curs.str = dest->str + (dest->cnt - curs.cnt);
			if (ascii_w2c(stmt->rset.pack.json.curs.str, curs.str,
					curs.cnt) <= 0) {
				ERRH(stmt, "failed to convert cursor `" LWPDL "` to ASCII.",
					LWSTR(&stmt->rset.pack.json.curs));
				RET_HDIAGS(stmt, SQL_STATE_24000);
			}
		} else {
			curs = stmt->rset.pack.cbor.curs;
		}
		err = cbor_encode_text_string(&map, curs.str, curs.cnt);
		FAIL_ON_CBOR_ERR(stmt, err);
	} else { /* copy QUERY object */
		err = cbor_encode_text_string(&map, REQ_KEY_QUERY,
				sizeof(REQ_KEY_QUERY) - 1);
		FAIL_ON_CBOR_ERR(stmt, err);
		err = cbor_encode_text_string(&map, stmt->u8sql.str, stmt->u8sql.cnt);
		FAIL_ON_CBOR_ERR(stmt, err);

		/* does the statement have any bound parameters? */
		if (count_bound(stmt->apd)) {
			err = cbor_encode_text_string(&map, REQ_KEY_PARAMS,
					sizeof(REQ_KEY_PARAMS) - 1);
			FAIL_ON_CBOR_ERR(stmt, err);
			err = serialize_params_cbor(stmt, &map, conv_len);
			FAIL_ON_CBOR_ERR(stmt, err);
		}
		/* does the statement have any fetch_size? */
		if (dbc->fetch.slen) {
			err = cbor_encode_text_string(&map, REQ_KEY_FETCH,
					sizeof(REQ_KEY_FETCH) - 1);
			FAIL_ON_CBOR_ERR(stmt, err);
			err = cbor_encode_uint(&map, dbc->fetch.max);
			FAIL_ON_CBOR_ERR(stmt, err);
		}
		/* "field_multi_value_leniency": true/false */
		err = cbor_encode_text_string(&map, REQ_KEY_MULTIVAL,
				sizeof(REQ_KEY_MULTIVAL) - 1);
		FAIL_ON_CBOR_ERR(stmt, err);
		err = cbor_encode_boolean(&map, dbc->mfield_lenient);
		FAIL_ON_CBOR_ERR(stmt, err);
		/* "index_include_frozen": true/false */
		err = cbor_encode_text_string(&map, REQ_KEY_IDX_FROZEN,
				sizeof(REQ_KEY_IDX_FROZEN) - 1);
		FAIL_ON_CBOR_ERR(stmt, err);
		err = cbor_encode_boolean(&map, dbc->idx_inc_frozen);
		FAIL_ON_CBOR_ERR(stmt, err);
		/* "time_zone": "-05:45" */
		err = cbor_encode_text_string(&map, REQ_KEY_TIMEZONE,
				sizeof(REQ_KEY_TIMEZONE) - 1);
		FAIL_ON_CBOR_ERR(stmt, err);
		if (dbc->apply_tz) {
			tz = tz_param;
		} else {
			tz = (cstr_st)CSTR_INIT(REQ_VAL_TIMEZONE_Z);
		}
		err = cbor_encode_text_string(&map, tz.str, tz.cnt);
		FAIL_ON_CBOR_ERR(stmt, err);
		if (dbc->catalog.c.cnt) {
			err = cbor_encode_text_string(&map, REQ_KEY_CATALOG,
					sizeof(REQ_KEY_CATALOG) - 1);
			FAIL_ON_CBOR_ERR(stmt, err);
			err = cbor_encode_text_string(&map, dbc->catalog.c.str,
					dbc->catalog.c.cnt);
			FAIL_ON_CBOR_ERR(stmt, err);
		}
		/* version */
		err = cbor_encode_text_string(&map, REQ_KEY_VERSION,
				sizeof(REQ_KEY_VERSION) - 1);
		FAIL_ON_CBOR_ERR(stmt, err);
		err = cbor_encode_text_string(&map, version.str, version.cnt);
		FAIL_ON_CBOR_ERR(stmt, err);
	}
	/* mode : ODBC */
	err = cbor_encode_text_string(&map, REQ_KEY_MODE,
			sizeof(REQ_KEY_MODE) - 1);
	FAIL_ON_CBOR_ERR(stmt, err);
	err = cbor_encode_text_string(&map, REQ_VAL_MODE,
			sizeof(REQ_VAL_MODE) - 1);
	FAIL_ON_CBOR_ERR(stmt, err);
	/* client_id : odbcXX */
	err = cbor_encode_text_string(&map, REQ_KEY_CLT_ID,
			sizeof(REQ_KEY_CLT_ID) - 1);
	FAIL_ON_CBOR_ERR(stmt, err);
	err = cbor_encode_text_string(&map, REQ_VAL_CLT_ID,
			sizeof(REQ_VAL_CLT_ID) - 1);
	FAIL_ON_CBOR_ERR(stmt, err);
	/* binary_format: true (false means JSON) */
	err = cbor_encode_text_string(&map, REQ_KEY_BINARY_FMT,
			sizeof(REQ_KEY_BINARY_FMT) - 1);
	FAIL_ON_CBOR_ERR(stmt, err);
	err = cbor_encode_boolean(&map, TRUE);

	err = cbor_encoder_close_container(&encoder, &map);
	FAIL_ON_CBOR_ERR(stmt, err);

	dest_cnt = cbor_encoder_get_buffer_size(&encoder, dest->str);
	assert(dest_cnt <= dest->cnt); /* tinycbor should check this, but still */
	dest->cnt = dest_cnt;
	DBGH(stmt, "request serialized to CBOR: [%zd] `%s`.", dest->cnt,
		cstr_hex_dump(dest));

	return SQL_SUCCESS;
}

static inline size_t copy_bool_val(char *dest, BOOL val)
{
	if (val) {
		memcpy(dest, "true", sizeof("true") - 1);
		return sizeof("true") - 1;
	} else {
		memcpy(dest, "false", sizeof("false") - 1);
		return sizeof("false") - 1;
	}
}

static SQLRETURN serialize_to_json(esodbc_stmt_st *stmt, cstr_st *dest)
{
	SQLRETURN ret;
	size_t pos, len;
	char *body = dest->str;
	esodbc_dbc_st *dbc = HDRH(stmt)->dbc;

	pos = 0;
	body[pos ++] = '{';
	/* build the actual stringified JSON object */
	if (STMT_HAS_CURSOR(stmt)) { /* copy CURSOR object */
		memcpy(body + pos, JSON_KEY_CURSOR, sizeof(JSON_KEY_CURSOR) - 1);
		pos += sizeof(JSON_KEY_CURSOR) - 1;
		body[pos ++] = '"';
		if (stmt->rset.pack_json) {
			if (ascii_w2c(stmt->rset.pack.json.curs.str, body + pos,
					stmt->rset.pack.json.curs.cnt) <= 0) {
				ERRH(stmt, "failed to convert cursor `" LWPDL "` to ASCII.",
					LWSTR(&stmt->rset.pack.json.curs));
				RET_HDIAGS(stmt, SQL_STATE_24000);
			}
			/* no character needs JSON escaping */
			assert(stmt->rset.pack.json.curs.cnt == json_escape(body + pos,
					stmt->rset.pack.json.curs.cnt, NULL, 0));
			pos += stmt->rset.pack.json.curs.cnt;
		} else {
			memcpy(body + pos, stmt->rset.pack.cbor.curs.str,
				stmt->rset.pack.cbor.curs.cnt);
			pos += stmt->rset.pack.cbor.curs.cnt;
		}
		body[pos ++] = '"';
	} else { /* copy QUERY object */
		memcpy(body + pos, JSON_KEY_QUERY, sizeof(JSON_KEY_QUERY) - 1);
		pos += sizeof(JSON_KEY_QUERY) - 1;
		body[pos ++] = '"';
		pos += json_escape(stmt->u8sql.str, stmt->u8sql.cnt, body + pos,
				dest->cnt - pos);
		body[pos ++] = '"';

		/* does the statement have any parameters? */
		if (count_bound(stmt->apd)) {
			memcpy(body + pos, JSON_KEY_PARAMS, sizeof(JSON_KEY_PARAMS) - 1);
			pos += sizeof(JSON_KEY_PARAMS) - 1;
			/* serialize_params_json will count/copy array delims (`[`, `]`) */
			ret = serialize_params_json(stmt, body + pos, &len);
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
		/* "field_multi_value_leniency": true/false */
		memcpy(body + pos, JSON_KEY_MULTIVAL, sizeof(JSON_KEY_MULTIVAL) - 1);
		pos += sizeof(JSON_KEY_MULTIVAL) - 1;
		pos += copy_bool_val(body + pos, dbc->mfield_lenient);
		/* "index_include_frozen": true/false */
		memcpy(body + pos, JSON_KEY_IDX_FROZEN,
			sizeof(JSON_KEY_IDX_FROZEN) - 1);
		pos += sizeof(JSON_KEY_IDX_FROZEN) - 1;
		pos += copy_bool_val(body + pos, dbc->idx_inc_frozen);
		/* "time_zone": "-05:45" */
		memcpy(body + pos, JSON_KEY_TIMEZONE, sizeof(JSON_KEY_TIMEZONE) - 1);
		pos += sizeof(JSON_KEY_TIMEZONE) - 1;
		if (dbc->apply_tz) {
			memcpy(body + pos, tz_param.str, tz_param.cnt);
			pos += tz_param.cnt;
		} else {
			memcpy(body + pos, JSON_VAL_TIMEZONE_Z,
				sizeof(JSON_VAL_TIMEZONE_Z) - 1);
			pos += sizeof(JSON_VAL_TIMEZONE_Z) - 1;
		}
		if (dbc->catalog.c.cnt) {
			/* "catalog": "my_cluster" */
			memcpy(body + pos, JSON_KEY_CATALOG, sizeof(JSON_KEY_CATALOG) - 1);
			pos += sizeof(JSON_KEY_CATALOG) - 1;
			body[pos ++] = '"';
			memcpy(body + pos, dbc->catalog.c.str, dbc->catalog.c.cnt);
			pos += dbc->catalog.c.cnt;
			body[pos ++] = '"';
		}
		/* "version": ... */
		memcpy(body + pos, JSON_KEY_VERSION, sizeof(JSON_KEY_VERSION) - 1);
		pos += sizeof(JSON_KEY_VERSION) - 1;
		body[pos ++] = '"';
		memcpy(body + pos, version.str, version.cnt);
		pos += version.cnt;
		body[pos ++] = '"';
	}
	/* "mode": "ODBC" */
	memcpy(body + pos, JSON_KEY_VAL_MODE, sizeof(JSON_KEY_VAL_MODE) - 1);
	pos += sizeof(JSON_KEY_VAL_MODE) - 1;
	/* "client_id": "odbcXX" */
	memcpy(body + pos, JSON_KEY_CLT_ID, sizeof(JSON_KEY_CLT_ID) - 1);
	pos += sizeof(JSON_KEY_CLT_ID) - 1;
	/* "binary_format": false (true means CBOR) */
	memcpy(body + pos, JSON_KEY_BINARY_FMT, sizeof(JSON_KEY_BINARY_FMT) - 1);
	pos += sizeof(JSON_KEY_BINARY_FMT) - 1;
	pos += copy_bool_val(body + pos, FALSE);
	body[pos ++] = '}';

	/* check that the buffer hasn't been overrun. it can be used less than
	 * initially calculated, since the calculation is an upper-bound one. */
	assert(pos <= dest->cnt);
	dest->cnt = pos;

	DBGH(stmt, "request serialized to JSON: [%zd] `" LCPDL "`.", pos,
		LCSTR(dest));
	return SQL_SUCCESS;
}

/*
 * Build a serialized JSON/CBOR object out of the statement.
 * If resulting string fits into the given buff, the result is copied in it;
 * othewise a new one will be allocated and returned.
 */
SQLRETURN TEST_API serialize_statement(esodbc_stmt_st *stmt, cstr_st *dest)
{
	SQLRETURN ret;
	size_t enc_len, conv_len, alloc_len, keys;
	esodbc_dbc_st *dbc = HDRH(stmt)->dbc;

	/* enforced in EsSQLSetDescFieldW(SQL_DESC_ARRAY_SIZE) */
	assert(stmt->apd->array_size <= 1);

	if (! update_tz_param()) {
		RET_HDIAG(stmt, SQL_STATE_HY000,
			"Failed to update the timezone parameter", 0);
	}

	conv_len = 0;
	ret = dbc->pack_json ? statement_len_json(stmt, &enc_len) :
		statement_len_cbor(stmt, &enc_len, &conv_len, &keys);
	if (! SQL_SUCCEEDED(ret)) {
		return ret;
	} else {
		alloc_len = enc_len + conv_len;
	}

	/* allocate memory for the stringified statement, if needed */
	if (dest->cnt < alloc_len) {
		INFOH(dbc, "local buffer too small (%zu), need %zuB; will alloc.",
			dest->cnt, alloc_len);
		DBGH(dbc, "local buffer too small, SQL: `" LCPDL "`.",
			LCSTR(&stmt->u8sql));
		if (! (dest->str = malloc(alloc_len))) {
			ERRNH(stmt, "failed to alloc %zdB.", alloc_len);
			RET_HDIAGS(stmt, SQL_STATE_HY001);
		}
		dest->cnt = alloc_len;
	}

	return dbc->pack_json ? serialize_to_json(stmt, dest) :
		serialize_to_cbor(stmt, dest, conv_len, keys);
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

	if (stmt->early_executed) {
		stmt->early_executed = false; /* re-enable subsequent executions */
		if (STMT_HAS_RESULTSET(stmt)) {
			DBGH(stmt, "query early executed: [%zd] `" LCPDL "`.",
				stmt->u8sql.cnt, LCSTR(&stmt->u8sql));
			return SQL_SUCCESS;
		} else {
			WARNH(stmt, "query `" LCPDL "` early executed, but lacking result"
				" set.", LCSTR(&stmt->u8sql));
		}
	}
	DBGH(stmt, "executing query: [%zd] `" LCPDL "`.", stmt->u8sql.cnt,
		LCSTR(&stmt->u8sql));

	ret = serialize_statement(stmt, &body);
	if (SQL_SUCCEEDED(ret)) {
		ret = curl_post(stmt, ESODBC_CURL_QUERY, &body);
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
#ifdef NDEBUG
	/* no reason to keep it (it can't be re-executed), except for debugging */
	detach_sql(stmt);
#endif /* NDEBUG */
	return ret;
}

/* simple loop back, the driver does no actual translation */
SQLRETURN EsSQLNativeSqlW(
	SQLHDBC                                     hdbc,
	_In_reads_(cchSqlStrIn) SQLWCHAR           *szSqlStrIn,
	SQLINTEGER                                  cchSqlStrIn,
	_Out_writes_opt_(cchSqlStrMax) SQLWCHAR    *szSqlStr,
	SQLINTEGER                                  cchSqlStrMax,
	SQLINTEGER                                 *pcchSqlStr)
{
	SQLINTEGER to_copy;
	int state = SQL_STATE_00000;

	if (! szSqlStrIn) {
		RET_HDIAGS(DBCH(hdbc), SQL_STATE_HY009);
	}

	if (cchSqlStrIn == SQL_NTSL) {
		cchSqlStrIn = (SQLINTEGER)wcslen(szSqlStrIn);
	}
	if (pcchSqlStr) {
		*pcchSqlStr = cchSqlStrIn;
	}

	if (szSqlStr) {
		if (cchSqlStrMax <= cchSqlStrIn) {
			to_copy = cchSqlStrMax - /* \0 */1;
			state = SQL_STATE_01004;
		} else {
			to_copy = cchSqlStrIn;
		}
		memcpy(szSqlStr, szSqlStrIn, to_copy * sizeof(*szSqlStr));
		szSqlStr[to_copy] = L'\0';
	}

	if (state == SQL_STATE_01004) {
		RET_HDIAGS(DBCH(hdbc), SQL_STATE_01004);
	} else {
		return SQL_SUCCESS;
	}
}

static inline SQLULEN get_col_size(esodbc_rec_st *rec)
{
	assert(DESC_TYPE_IS_IMPLEMENTATION(rec->desc->type));

	switch (rec->meta_type) {
		case METATYPE_EXACT_NUMERIC:
		case METATYPE_FLOAT_NUMERIC:
			return rec->es_type->column_size; // precision?

		case METATYPE_STRING:
		case METATYPE_BIN:
		case METATYPE_DATE_TIME:
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
		case METATYPE_DATE_TIME:
		case METATYPE_INTERVAL_WSEC:
			return ESODBC_DEF_SEC_PRECISION;

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
		icol, rec->meta_type, (uint64_t)*pcbColDef);

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
		case SQL_COLUMN_NULLABLE: /* 2.x attrib; no break */
		case SQL_DESC_NULLABLE: sint = rec->es_type->nullable; break;
		case SQL_DESC_SEARCHABLE: sint = rec->es_type->searchable; break;
		case SQL_DESC_UNSIGNED: sint = rec->es_type->unsigned_attribute; break;
		case SQL_DESC_UPDATABLE: sint = rec->updatable; break;
		case SQL_COLUMN_PRECISION: /* 2.x attrib; no break */
		case SQL_DESC_PRECISION:
			sint = rec->es_type->fixed_prec_scale;
			break;
		case SQL_COLUMN_SCALE: /* 2.x attrib; no break */
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
		case SQL_COLUMN_NAME: /* 2.x attrib; no break */
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
		case SQL_COLUMN_LENGTH: /* 2.x attrib; no break */
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

/* very simple counter of non-quoted, not-escaped single question marks.
 * no statement validation done */
static SQLRETURN count_param_markers(esodbc_stmt_st *stmt, SQLSMALLINT *p_cnt)
{
	SQLSMALLINT cnt;
	wstr_st u16sql;
	SQLWCHAR *pos, *end, *sav;
	BOOL quoting, escaping;
	SQLWCHAR crr, qchar; /* char that starting the quote (`'` or `"`) */

	/* The SQL query is received originally as UTF-16 SQLWCHAR, but converted
	 * to UTF-8 MB right away and stored like that, for conveience. Easiest to
	 * parse it is in its original SQLWCHAR format, though and since that's
	 * only needed (assumably) rarely, we'll convert it back here, instead of
	 * storing also the original. */
	if (&u16sql != utf8_to_wstr(&stmt->u8sql, &u16sql)) {
		ERRH(stmt, "failed to convert stmt `" LCPDL "` back to UTF-16 WC",
			LCSTR(&stmt->u8sql));
		RET_HDIAGS(stmt, SQL_STATE_HY000);
	}
	pos = u16sql.str;
	end = pos + u16sql.cnt;
	cnt = 0;
	quoting = FALSE;
	escaping = FALSE;
	crr = 0;
	while (pos < end) {
		switch((crr = *pos ++)) {
			case MK_WPTR(ESODBC_PARAM_MARKER): /* ~ L'?' */
				if (escaping || quoting) {
					break;
				}
				/* skip groups `???` */
				sav = pos - 1; /* position of first `?` */
				while (pos < end && *pos == crr) {
					pos ++;
				}
				/* only count if single */
				if (sav + 1 == pos) {
					cnt ++;
				}
				break;
			case L'"':
			case L'\'':
				if (escaping) {
					break;
				}
				if (! quoting) {
					quoting = TRUE;
					qchar = crr;
				} else if (qchar == crr) {
					quoting = FALSE;
				} /* else: sequence is: `"..'` or `'.."` -> ignore */
				break;
			case MK_WPTR(ESODBC_CHAR_ESCAPE): /* ~ L'\\' */
				if (escaping) {
					break;
				}
				escaping = TRUE;
				continue;
		}
		escaping = FALSE;
	}

	if (escaping || quoting) {
		ERRH(stmt, "invalid SQL statement: [%zu] `" LWPDL "`.", u16sql.cnt,
			LWSTR(&u16sql));
		free(u16sql.str);
		RET_HDIAG(stmt, SQL_STATE_HY000, "failed to parse statement", 0);
	}

	DBGH(stmt, "counted %hd param marker(s) in SQL `" LWPDL "`.", cnt,
		LWSTR(&u16sql));
	free(u16sql.str);

	*p_cnt = cnt;
	return SQL_SUCCESS;
}

/* function implementation is correct, but it can't really be used as
 * intended, since the driver's "preparation" doesn't really involve sending
 * it to ES or even parameter marker counting; the later would be doable now,
 * but SQLDescribeParam would either need ES/SQL support, or execute with
 * fetch_size 0 or 1. */
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

#	if 0
	/* The correct implementation, once a statement trully is prepared. */
	return EsSQLGetDescFieldW(stmt->ipd, NO_REC_NR,
			SQL_DESC_COUNT, ParameterCountPtr, SQL_IS_SMALLINT, NULL);
#	else /* 0 */
	/* Only count params on-demand */
	/* some apps (like pyodbc) will verify the user-provided parameters number
	 * against the result of this function -> implement a crude parser. */
	return count_param_markers(stmt, ParameterCountPtr);
#	endif /* 0 */
}

SQLRETURN EsSQLRowCount(_In_ SQLHSTMT StatementHandle, _Out_ SQLLEN *RowCount)
{
	esodbc_stmt_st *stmt = STMH(StatementHandle);
	size_t nrows;
	CborError res;

	if (! STMT_HAS_RESULTSET(stmt)) {
		ERRH(stmt, "no resultset available on statement.");
		RET_HDIAGS(stmt, SQL_STATE_HY010);
	}

	if (stmt->rset.pack_json) {
		*RowCount = UJLengthArray(stmt->rset.pack.json.rows_obj);
	} else {
		res = cbor_get_array_count(stmt->rset.pack.cbor.rows_obj, &nrows);
		if (res != CborNoError) {
			ERRH(stmt, "failed to read row array count: %s.",
				cbor_error_string(res));
			RET_HDIAGS(stmt, SQL_STATE_HY000);
		}
		*RowCount = (SQLLEN)nrows;
	}
	DBGH(stmt, "result set rows count: %zd.", *RowCount);

	/* Log a warning if a cursor is present.
	 * Note: ES/SQL can apparently attach a cursor even for SYS queries and no
	 * restrictive max fetch row count. Since this function is now also called
	 * while attaching types during bootstrapping, it shouldn't fail if a
	 * cursor is attached. This assumes however that ES will never return just
	 * a cursor (and no rows) on a first page. */
	if (STMT_HAS_CURSOR(stmt)) {
		/* fetch_size or scroller size will chunk the result */
		WARNH(stmt, "can't evaluate the total size of paged result set.");
		RET_HDIAG(stmt, SQL_STATE_01000, "row count is for current row set "
			"only", 0);
	}

	return SQL_SUCCESS;
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=78 : */
