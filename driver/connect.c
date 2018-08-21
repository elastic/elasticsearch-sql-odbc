/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>

#include "connect.h"
#include "queries.h"
#include "catalogue.h"
#include "log.h"
#include "info.h"
#include "util.h"
#include "dsn.h"

/* HTTP headers default for every request */
#define HTTP_ACCEPT_JSON		"Accept: application/json"
#define HTTP_CONTENT_TYPE_JSON	"Content-Type: application/json; charset=utf-8"

/* Elasticsearch/SQL data types */
/* 4 */
#define JSON_COL_BYTE			"byte"
#define JSON_COL_LONG			"long"
#define JSON_COL_TEXT			"text"
#define JSON_COL_DATE			"date"
#define JSON_COL_NULL			"null"
/* 5 */
#define JSON_COL_SHORT			"short"
#define JSON_COL_FLOAT			"float"
/* 6 */
#define JSON_COL_DOUBLE			"double"
#define JSON_COL_BINARY			"binary"
#define JSON_COL_OBJECT			"object"
#define JSON_COL_NESTED			"nested"
/* 7 */
#define JSON_COL_BOOLEAN		"boolean"
#define JSON_COL_INTEGER		"integer"
#define JSON_COL_KEYWORD		"keyword"
/* 10 */
#define JSON_COL_HALF_FLOAT		"half_float"
/* 11 */
#define JSON_COL_UNSUPPORTED	"unsupported"
/* 12 */
#define JSON_COL_SCALED_FLOAT	"scaled_float"


/* structure for one row returned by the ES.
 * This is a mirror of elasticsearch_type, with length-or-indicator fields
 * for each of the members in elasticsearch_type */
typedef struct {
	SQLWCHAR		type_name[ESODBC_MAX_IDENTIFIER_LEN];
	SQLLEN			type_name_loi; /* _ length or indicator */
	SQLSMALLINT		data_type;
	SQLLEN			data_type_loi;
	SQLINTEGER		column_size;
	SQLLEN			column_size_loi;
	SQLWCHAR		literal_prefix[ESODBC_MAX_IDENTIFIER_LEN];
	SQLLEN			literal_prefix_loi;
	SQLWCHAR		literal_suffix[ESODBC_MAX_IDENTIFIER_LEN];
	SQLLEN			literal_suffix_loi;
	SQLWCHAR		create_params[ESODBC_MAX_IDENTIFIER_LEN];
	SQLLEN			create_params_loi;
	SQLSMALLINT		nullable;
	SQLLEN			nullable_loi;
	SQLSMALLINT		case_sensitive;
	SQLLEN			case_sensitive_loi;
	SQLSMALLINT		searchable;
	SQLLEN			searchable_loi;
	SQLSMALLINT		unsigned_attribute;
	SQLLEN			unsigned_attribute_loi;
	SQLSMALLINT		fixed_prec_scale;
	SQLLEN			fixed_prec_scale_loi;
	SQLSMALLINT		auto_unique_value;
	SQLLEN			auto_unique_value_loi;
	SQLWCHAR		local_type_name[ESODBC_MAX_IDENTIFIER_LEN];
	SQLLEN			local_type_name_loi;
	SQLSMALLINT		minimum_scale;
	SQLLEN			minimum_scale_loi;
	SQLSMALLINT		maximum_scale;
	SQLLEN			maximum_scale_loi;
	SQLSMALLINT		sql_data_type;
	SQLLEN			sql_data_type_loi;
	SQLSMALLINT		sql_datetime_sub;
	SQLLEN			sql_datetime_sub_loi;
	SQLINTEGER		num_prec_radix;
	SQLLEN			num_prec_radix_loi;
	SQLSMALLINT		interval_precision;
	SQLLEN			interval_precision_loi;
} estype_row_st;
/*
 * HTTP headers used for all requests (Content-Type, Accept).
 */
static struct curl_slist *http_headers = NULL;
#ifndef NDEBUG
static char curl_err_buff[CURL_ERROR_SIZE];
#endif /* NDEBUG */

BOOL connect_init()
{
	CURLcode code;

	DBG("libcurl: global init.");

	/* "this will init the winsock stuff" */
	code = curl_global_init(CURL_GLOBAL_ALL);
	if (code != CURLE_OK) {
		ERR("libcurl: failed to initialize: '%s' (%d).",
			curl_easy_strerror(code), code);
		return FALSE;
	}

	http_headers = curl_slist_append(http_headers, HTTP_ACCEPT_JSON);
	if (http_headers) {
		http_headers = curl_slist_append(http_headers, HTTP_CONTENT_TYPE_JSON);
	}
	if (! http_headers) {
		ERR("libcurl: failed to init headers.");
		return FALSE;
	}

	DBG("libcurl: init OK.");
	return TRUE;
}

void connect_cleanup()
{
	DBG("cleaning up connection/transport.");
	curl_slist_free_all(http_headers);
	curl_global_cleanup();
}

/*
 * "ptr points to the delivered data, and the size of that data is size
 * multiplied with nmemb."
 */
static size_t write_callback(char *ptr, size_t size, size_t nmemb,
	void *userdata)
{
	esodbc_dbc_st *dbc = (esodbc_dbc_st *)userdata;
	char *wbuf;
	size_t need, avail, have;

	assert(dbc->apos <= dbc->alen);
	avail = dbc->alen - dbc->apos;
	have = size * nmemb;

	DBGH(dbc, "libcurl: new data chunk of size [%zd] x %zd arrived; "
		"available buffer: %zd/%zd.", nmemb, size, avail, dbc->alen);

	/* do I need to grow the existing buffer? */
	if (avail < have) {
		/* calculate how much space to allocate. start from existing length,
		 * if set, othewise from a constant (on first allocation). */
		for (need = dbc->alen ? dbc->alen : ESODBC_BODY_BUF_START_SIZE;
			need < dbc->apos + have; need *= 2)
			;
		DBGH(dbc, "libcurl: growing buffer for new chunk of %zd "
			"from %zd to %zd.", have, dbc->alen, need);
		if (dbc->amax < need) { /* would I need more than max? */
			if (dbc->amax <= (size_t)dbc->alen) { /* am I at max already? */
				ERRH(dbc, "libcurl: can't grow past max %zd; have: %zd",
					dbc->amax, have);
				return 0;
			} else { /* am not: alloc max possible (possibly still < need!) */
				need = dbc->amax;
				WARNH(dbc, "libcurl: capped buffer to max: %zd", need);
				/* re'eval what I have available... */
				avail = dbc->amax - dbc->apos;
				/* ...and how much I could copy (possibly less than received)*/
				have = have < avail ? have : avail;
			}
		}
		wbuf = realloc(dbc->abuff, need);
		if (! wbuf) {
			ERRNH(dbc, "libcurl: failed to realloc to %zdB.", need);
			return 0;
		}
		dbc->abuff = wbuf;
		dbc->alen = need;
	}

	memcpy(dbc->abuff + dbc->apos, ptr, have);
	dbc->apos += have;
	DBGH(dbc, "libcurl: copied %zdB: `%.*s`.", have, have, ptr);


	/*
	 * "Your callback should return the number of bytes actually taken care
	 * of. If that amount differs from the amount passed to your callback
	 * function, it'll signal an error condition to the library. This will
	 * cause the transfer to get aborted"
	 *
	 * "If your callback function returns CURL_WRITEFUNC_PAUSE it will cause
	 * this transfer to become paused. See curl_easy_pause for further
	 * details."
	 */
	return have;
}

static SQLRETURN init_curl(esodbc_dbc_st *dbc)
{
	CURLcode res;
	CURL *curl;

	assert(! dbc->curl);

	/* get a libcurl handle */
	curl = curl_easy_init();
	if (! curl) {
		ERRNH(dbc, "libcurl: failed to fetch new handle.");
		RET_HDIAG(dbc, SQL_STATE_HY000, "failed to init the transport", 0);
	}

#ifndef NDEBUG
	res = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_err_buff);
	if (res != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to set errorbuffer: %s (%d).",
			curl_easy_strerror(res), res);
		/* not fatal */
	}
#endif /* NDEBUG */

	/* set URL to connect to */
	res = curl_easy_setopt(curl, CURLOPT_URL, dbc->url);
	if (res != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to set URL `%s`: %s (%d).", dbc->url,
			curl_easy_strerror(res), res);
		SET_HDIAG(dbc, SQL_STATE_HY000, "failed to init the transport", 0);
		goto err;
	}
	/* always do POSTs (seconded by CURLOPT_POSTFIELDS) */
	res = curl_easy_setopt(curl, CURLOPT_POST, 1L);
	if (res != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to set POST method: %s (%d).",
			curl_easy_strerror(res), res);
		SET_HDIAG(dbc, SQL_STATE_HY000, "failed to init the transport", 0);
		goto err;
	}

	/* set the Content-Type, Accept HTTP headers */
	res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_headers);
	if (res != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to set headers: %s (%d).",
			curl_easy_strerror(res), res);
		SET_HDIAG(dbc, SQL_STATE_HY000, "failed to init the transport", 0);
		goto err;
	}

	/* set the behavior for redirection */
	res = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, dbc->follow);
	if (res != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to set redirection: %s (%d).",
			curl_easy_strerror(res), res);
		SET_HDIAG(dbc, SQL_STATE_HY000, "failed to init the transport", 0);
		goto err;
	}

	/* set the write call-back for answers */
	res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	if (res != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to set write callback: %s (%d).",
			curl_easy_strerror(res), res);
		SET_HDIAG(dbc, SQL_STATE_HY000, "failed to init the transport", 0);
		goto err;
	}
	/* ... and its argument */
	res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, dbc);
	if (res != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to set callback argument: %s (%d).",
			curl_easy_strerror(res), res);
		SET_HDIAG(dbc, SQL_STATE_HY000, "failed to init the transport", 0);
		goto err;
	}

	dbc->curl = curl;
	DBGH(dbc, "libcurl: new handle 0x%p.", curl);
	return SQL_SUCCESS;

err:
	if (curl) {
		curl_easy_cleanup(curl);
	}
	RET_STATE(dbc->hdr.diag.state);
}

static void cleanup_curl(esodbc_dbc_st *dbc)
{
	if (! dbc->curl) {
		return;
	}
	DBGH(dbc, "libcurl: handle 0x%p cleanup.", dbc->curl);
	curl_easy_cleanup(dbc->curl); /* TODO: _reset() rather? */
	dbc->curl = NULL;
}

/*
 * Sends a POST request with the given JSON object body.
 */
SQLRETURN post_json(esodbc_stmt_st *stmt, const cstr_st *u8body)
{
	// char *const answer, /* buffer to receive the answer in */
	// long avail /*size of the answer buffer */)
	CURLcode res = CURLE_OK;
	esodbc_dbc_st *dbc = stmt->hdr.dbc;
	char *abuff = NULL;
	size_t apos;
	SQLULEN tout;
	long code;

	DBGH(stmt, "POSTing JSON [%zd] `" LCPDL "`.", u8body->cnt, LCSTR(u8body));

	ESODBC_MUX_LOCK(&dbc->curl_mux);

	if (! dbc->curl) {
		init_curl(dbc);
	}

	/* set timeout as maximum between connection and statement value */
	tout = dbc->timeout < stmt->query_timeout ? stmt->query_timeout :
		dbc->timeout;
	if (0 < tout) {
		res = curl_easy_setopt(dbc->curl, CURLOPT_TIMEOUT, tout);
		if (res != CURLE_OK) {
			goto err;
		} else {
			DBGH(stmt, "libcurl: set curl 0x%p timeout to %ld.", dbc->curl,
				tout);
		}
	}

	/* len of the body */
	res = curl_easy_setopt(dbc->curl, CURLOPT_POSTFIELDSIZE, u8body->cnt);
	if (res != CURLE_OK) {
		goto err;
	} else {
		DBGH(stmt, "libcurl: set curl 0x%p post fieldsize to %ld.",
			dbc->curl, u8body->cnt);
	}
	/* body itself */
	res = curl_easy_setopt(dbc->curl, CURLOPT_POSTFIELDS, u8body->str);
	if (res != CURLE_OK) {
		goto err;
	} else {
		DBGH(stmt, "libcurl: set curl 0x%p post fields to `" LCPDL "`.",
			dbc->curl, LCSTR(u8body));
	}

	assert(dbc->abuff == NULL);

	/* execute the request */
	res = curl_easy_perform(dbc->curl);

	abuff = dbc->abuff;
	apos = dbc->apos;

	/* clear call-back members for next call */
	dbc->abuff = NULL;
	dbc->apos = 0;
	dbc->alen = 0;

	if (res != CURLE_OK) {
		goto err;
	}
	res = curl_easy_getinfo(dbc->curl, CURLINFO_RESPONSE_CODE, &code);
	if (res != CURLE_OK) {
		goto err;
	}
	DBGH(stmt, "libcurl: request succesfull, received code %ld and %zd bytes"
		" back.", code, apos);

	ESODBC_MUX_UNLOCK(&dbc->curl_mux);

	if (code != 200) {
		ERRH(stmt, "libcurl: non-200 HTTP response code %ld received.", code);
		/* expect a 200 with body; everything else is failure (todo?)  */
		if (400 <= code) {
			return attach_error(stmt, abuff, apos);
		}
		goto err_net;
	}

	return attach_answer(stmt, abuff, apos);

err:
	ERRH(stmt, "libcurl: request failed (timeout:%llu, body:`" LCPDL "`) "
		"failed: '%s' (%d).", tout, LCSTR(u8body),
		res != CURLE_OK ? curl_easy_strerror(res) : "<unspecified>", res);
err_net: /* the error occured after the request hit hit the network */
	cleanup_curl(dbc);
	ESODBC_MUX_UNLOCK(&dbc->curl_mux);

	if (abuff) {
		free(abuff);
		abuff = NULL;
		/* if buffer had been set, the error occured in _perform() */
		RET_HDIAG(stmt, SQL_STATE_08S01, "data transfer failure", res);
	}
	RET_HDIAG(stmt, SQL_STATE_HY000, "failed to init transport", res);
}

static SQLRETURN test_connect(CURL *curl)
{
	CURLcode res;

	/* we only one to connect */
	res = curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
	if (res != CURLE_OK) {
		ERR("libcurl: failed to set connect_only: %s (%d).",
			curl_easy_strerror(res), res);
		return SQL_ERROR;
	}

	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		ERR("libcurl: failed connect: %s (%d).",
			curl_easy_strerror(res), res);
		return SQL_ERROR;
	}

	res = curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 0L);
	if (res != CURLE_OK) {
		ERR("libcurl: failed to unset connect_only: %s (%d).",
			curl_easy_strerror(res), res);
		return SQL_ERROR;
	}

	DBG("successfully connected to server.");
	return SQL_SUCCESS;
}

/*
 * init dbc from configured attributes
 */
static SQLRETURN process_config(esodbc_dbc_st *dbc, esodbc_dsn_attrs_st *attrs)
{
	esodbc_state_et state = SQL_STATE_HY000;
	int n, cnt;
	SQLWCHAR urlw[ESODBC_MAX_URL_LEN];
	BOOL secure;
	long long timeout, max_body_size, max_fetch_size;

	/*
	 * build connection URL
	 */
	secure = wstr2bool(&attrs->secure);
	INFOH(dbc, "connect secure: %s.", secure ? "true" : "false");
	cnt = swprintf(urlw, sizeof(urlw)/sizeof(urlw[0]),
			L"http" WPFCP_DESC "://" WPFWP_LDESC ":" WPFWP_LDESC
			ELASTIC_SQL_PATH, secure ? "s" : "", LWSTR(&attrs->server),
			LWSTR(&attrs->port));
	if (cnt < 0) {
		ERRNH(dbc, "failed to print URL out of server: `" LWPDL "` [%zd], "
			"port: `" LWPDL "` [%zd].", LWSTR(&attrs->server),
			LWSTR(&attrs->port));
		goto err;
	}
	/* length of URL converted to U8 */
	n = WCS2U8(urlw, cnt, NULL, 0);
	if (! n) {
		ERRNH(dbc, "failed to estimate U8 conversion space necessary for `"
			LWPDL " [%d]`.", cnt, urlw, cnt);
		goto err;
	}
	dbc->url = malloc(n + /*0-term*/1);
	if (! dbc->url) {
		ERRNH(dbc, "OOM for size: %dB.", n);
		state = SQL_STATE_HY001;
		goto err;
	}
	n = WCS2U8(urlw, cnt, dbc->url, n);
	if (! n) {
		ERRNH(dbc, "failed to U8 convert URL `" LWPDL "` [%d].",cnt, urlw, cnt);
		goto err;
	}
	dbc->url[n] = 0;
	/* URL should be 0-term'd, as printed by swprintf */
	INFOH(dbc, "connection URL: `%s`.", dbc->url);

	/* follow param for liburl */
	dbc->follow = wstr2bool(&attrs->follow);
	INFOH(dbc, "follow: %s.", dbc->follow ? "true" : "false");

	/*
	 * request timeout for liburl: negative reset to 0
	 */
	if (! str2bigint(&attrs->timeout, /*wide?*/TRUE, (SQLBIGINT *)&timeout)) {
		ERRH(dbc, "failed to convert `" LWPDL "` [%zu] to big int.",
			LWSTR(&attrs->timeout), attrs->timeout.cnt);
		goto err;
	}
	if (timeout < 0) {
		WARNH(dbc, "set timeout is negative (%ld), normalized to 0.", timeout);
		timeout = 0;
	}
	dbc->timeout = (SQLUINTEGER)timeout;
	INFOH(dbc, "timeout: %lu.", dbc->timeout);

	/*
	 * set max body size
	 */
	if (! str2bigint(&attrs->max_body_size, /*wide?*/TRUE,
			(SQLBIGINT *)&max_body_size)) {
		ERRH(dbc, "failed to convert `" LWPDL "` [%zu] to long long.",
			LWSTR(&attrs->max_body_size), attrs->max_body_size.cnt);
		goto err;
	}
	if (max_body_size < 0) {
		ERRH(dbc, "'%s' setting can't be negative (%ld).",
			ESODBC_DSN_MAX_BODY_SIZE_MB, max_body_size);
		goto err;
	} else {
		dbc->amax = (size_t)max_body_size * 1024 * 1024;
	}
	INFOH(dbc, "max body size: %zd.", dbc->amax);

	/*
	 * set max fetch size
	 */
	if (! str2bigint(&attrs->max_fetch_size, /*wide?*/TRUE,
			(SQLBIGINT *)&max_fetch_size)) {
		ERRH(dbc, "failed to convert `" LWPDL "` [%zu] to long long.",
			LWSTR(&attrs->max_fetch_size), attrs->max_fetch_size.cnt);
		goto err;
	}
	if (max_fetch_size < 0) {
		ERRH(dbc, "'%s' setting can't be negative (%ld).",
			ESODBC_DSN_MAX_FETCH_SIZE, max_fetch_size);
		goto err;
	} else {
		dbc->fetch.max = (size_t)max_fetch_size;
	}
	/* set the string representation of fetch_size, once for all STMTs */
	if (dbc->fetch.max) {
		dbc->fetch.slen = (char)attrs->max_fetch_size.cnt;
		dbc->fetch.str = malloc(dbc->fetch.slen + /*\0*/1);
		if (! dbc->fetch.str) {
			ERRNH(dbc, "failed to alloc %zdB.", dbc->fetch.slen);
			RET_HDIAGS(dbc, SQL_STATE_HY001);
		}
		dbc->fetch.str[dbc->fetch.slen] = 0;
		ansi_w2c(attrs->max_fetch_size.str, dbc->fetch.str, dbc->fetch.slen);
	}
	INFOH(dbc, "fetch_size: %s.", dbc->fetch.str ? dbc->fetch.str : "none" );

	// TODO: catalog handling

	/*
	 * set the REST body format: JSON/CBOR
	 */
	if (EQ_CASE_WSTR(&attrs->packing, &MK_WSTR("JSON"))) {
		dbc->pack_json = TRUE;
	} else if (EQ_CASE_WSTR(&attrs->packing, &MK_WSTR("CBOR"))) {
		dbc->pack_json = FALSE;
	} else {
		ERRH(dbc, "unknown packing encoding '" LWPDL "'.",
			LWSTR(&attrs->packing));
		goto err;
	}
	INFOH(dbc, "pack JSON: %s.", dbc->pack_json ? "true" : "false");

	return SQL_SUCCESS;
err:
	if (state == SQL_STATE_HY000) {
		RET_HDIAG(dbc, state, "invalid configuration parameter", 0);
	}
	RET_HDIAGS(dbc, state);
}


/* release all resources, except the handler itself */
void cleanup_dbc(esodbc_dbc_st *dbc)
{
	if (dbc->url) {
		free(dbc->url);
		dbc->url = NULL;
	}
	if (dbc->fetch.str) {
		free(dbc->fetch.str);
		dbc->fetch.str = NULL;
		dbc->fetch.slen = 0;
	}
	if (dbc->dsn.str && 0 < dbc->dsn.cnt) {
		free(dbc->dsn.str);
		dbc->dsn.str = NULL;
	} else {
		/* small hack: the API allows querying for a DSN also in the case a
		 * connection actually fails to be established; in which case the
		 * actual DSN value hasn't been allocated/copied. */
		dbc->dsn.str = MK_WPTR("");
		dbc->dsn.cnt = 0;
	}
	if (dbc->es_types) {
		free(dbc->es_types);
		dbc->es_types = NULL;
		dbc->no_types = 0;
	} else {
		assert(dbc->no_types == 0);
	}
	assert(dbc->abuff == NULL);
	cleanup_curl(dbc);
}

static SQLRETURN do_connect(esodbc_dbc_st *dbc, esodbc_dsn_attrs_st *attrs)
{
	SQLRETURN ret;
	char *url = NULL;

	/* multiple connection attempts are possible (when prompting user) */
	cleanup_dbc(dbc);

	ret = process_config(dbc, attrs);
	if (! SQL_SUCCEEDED(ret)) {
		return ret;
	}

	/* init libcurl objects */
	ret = init_curl(dbc);
	if (! SQL_SUCCEEDED(ret)) {
		ERRH(dbc, "failed to init transport.");
		return ret;
	}

	/* perform a connection test, to fail quickly if wrong server/port AND
	 * populate the DNS cache; this won't guarantee succesful post'ing, tho! */
	ret = test_connect(dbc->curl);
	/* still ok if fails */
	curl_easy_getinfo(dbc->curl, CURLINFO_EFFECTIVE_URL, &url);
	if (! SQL_SUCCEEDED(ret)) {
		ERRH(dbc, "test connection to URL %s failed!", url);
		cleanup_curl(dbc);
		RET_HDIAG(dbc, SQL_STATE_HYT01, "connection test failed", 0);
	} else {
		DBGH(dbc, "test connection to URL %s OK.", url);
	}

	return SQL_SUCCESS;
}


/* Maps ES/SQL type name to C SQL and SQL id values. */
static BOOL elastic_name2types(wstr_st *type_name,
	SQLSMALLINT *c_sql, SQLSMALLINT *sql)
{
	switch (type_name->cnt) {
		/* 4: BYTE, LONG, TEXT, DATE, NULL */
		case sizeof(JSON_COL_BYTE) - 1:
			switch (tolower(type_name->str[0])) {
				case (SQLWCHAR)'b':
					if (! wmemncasecmp(type_name->str,
							MK_WPTR(JSON_COL_BYTE), type_name->cnt)) {
						*c_sql = ESODBC_ES_TO_CSQL_BYTE;
						*sql = ESODBC_ES_TO_SQL_BYTE;
						return TRUE;
					}
					break;
				case (SQLWCHAR)'l':
					if (! wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_LONG),
							type_name->cnt)) {
						*c_sql = ESODBC_ES_TO_CSQL_LONG;
						*sql = ESODBC_ES_TO_SQL_LONG;
						return TRUE;
					}
					break;
				case (SQLWCHAR)'t':
					if (! wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_TEXT),
							type_name->cnt)) {
						*c_sql = ESODBC_ES_TO_CSQL_TEXT;
						*sql = ESODBC_ES_TO_SQL_TEXT;
						return TRUE;
					}
					break;
				case (SQLWCHAR)'d':
					if (! wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_DATE),
							type_name->cnt)) {
						*c_sql = ESODBC_ES_TO_CSQL_DATE;
						*sql = ESODBC_ES_TO_SQL_DATE;
						return TRUE;
					}
					break;
				case (SQLWCHAR)'n':
					if (! wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_NULL),
							type_name->cnt)) {
						*c_sql = ESODBC_ES_TO_CSQL_NULL;
						*sql = ESODBC_ES_TO_SQL_NULL;
						return TRUE;
					}
					break;
			}
			break;

		/* 5: SHORT, FLOAT */
		case sizeof(JSON_COL_SHORT) - 1:
			switch (tolower(type_name->str[0])) {
				case (SQLWCHAR)'s':
					if (! wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_SHORT),
							type_name->cnt)) {
						*c_sql = ESODBC_ES_TO_CSQL_SHORT;
						*sql = ESODBC_ES_TO_SQL_SHORT;
						return TRUE;
					}
					break;
				case (SQLWCHAR)'f':
					if (! wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_FLOAT),
							type_name->cnt)) {
						*c_sql = ESODBC_ES_TO_CSQL_FLOAT;
						*sql = ESODBC_ES_TO_SQL_FLOAT;
						return TRUE;
					}
					break;
			}
			break;

		/* 6: DOUBLE, BINARY, OBJECT, NESTED */
		case sizeof(JSON_COL_DOUBLE) - 1:
			switch (tolower(type_name->str[0])) {
				case (SQLWCHAR)'d':
					if (! wmemncasecmp(type_name->str,
							MK_WPTR(JSON_COL_DOUBLE), type_name->cnt)) {
						*c_sql = ESODBC_ES_TO_CSQL_DOUBLE;
						*sql = ESODBC_ES_TO_SQL_DOUBLE;
						return TRUE;
					}
					break;
				case (SQLWCHAR)'b':
					if (! wmemncasecmp(type_name->str,
							MK_WPTR(JSON_COL_BINARY), type_name->cnt)) {
						*c_sql = ESODBC_ES_TO_CSQL_BINARY;
						*sql = ESODBC_ES_TO_SQL_BINARY;
						return TRUE;
					}
					break;
				case (SQLWCHAR)'o':
					if (! wmemncasecmp(type_name->str,
							MK_WPTR(JSON_COL_OBJECT), type_name->cnt)) {
						*c_sql = ESODBC_ES_TO_CSQL_OBJECT;
						*sql = ESODBC_ES_TO_SQL_OBJECT;
						return TRUE;
					}
					break;
				case (SQLWCHAR)'n':
					if (! wmemncasecmp(type_name->str,
							MK_WPTR(JSON_COL_NESTED), type_name->cnt)) {
						*c_sql = ESODBC_ES_TO_CSQL_NESTED;
						*sql = ESODBC_ES_TO_SQL_NESTED;
						return TRUE;
					}
					break;
			}
			break;

		/* 7: INTEGER, BOOLEAN, KEYWORD */
		case sizeof(JSON_COL_INTEGER) - 1:
			switch (tolower(type_name->str[0])) {
				case (SQLWCHAR)'i': /* integer */
					if (wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_INTEGER),
							type_name->cnt) == 0) {
						*c_sql = ESODBC_ES_TO_CSQL_INTEGER;
						*sql = ESODBC_ES_TO_SQL_INTEGER;
						return TRUE;
					}
					break;
				case (SQLWCHAR)'b': /* boolean */
					if (wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_BOOLEAN),
							type_name->cnt) == 0) {
						*c_sql = ESODBC_ES_TO_CSQL_BOOLEAN;
						*sql = ESODBC_ES_TO_SQL_BOOLEAN;
						return TRUE;
					}
					break;
				case (SQLWCHAR)'k': /* keyword */
					if (wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_KEYWORD),
							type_name->cnt) == 0) {
						*c_sql = ESODBC_ES_TO_CSQL_KEYWORD;
						*sql = ESODBC_ES_TO_SQL_KEYWORD;
						return TRUE;
					}
					break;
			}
			break;

		/* 10: HALF_FLOAT */
		case sizeof(JSON_COL_HALF_FLOAT) - 1:
			if (! wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_HALF_FLOAT),
					type_name->cnt)) {
				*c_sql = ESODBC_ES_TO_CSQL_HALF_FLOAT;
				*sql = ESODBC_ES_TO_SQL_HALF_FLOAT;
				return TRUE;
			}
			break;

		/* 11: UNSUPPORTED */
		case sizeof(JSON_COL_UNSUPPORTED) - 1:
			if (! wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_UNSUPPORTED),
					type_name->cnt)) {
				*c_sql = ESODBC_ES_TO_CSQL_UNSUPPORTED;
				*sql = ESODBC_ES_TO_SQL_UNSUPPORTED;
				return TRUE;
			}
			break;

		/* 12: SCALED_FLOAT */
		case sizeof(JSON_COL_SCALED_FLOAT) - 1:
			if (! wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_SCALED_FLOAT),
					type_name->cnt)) {
				*c_sql = ESODBC_ES_TO_CSQL_SCALED_FLOAT;
				*sql = ESODBC_ES_TO_SQL_SCALED_FLOAT;
				return TRUE;
			}
			break;

	}
	ERR("unrecognized Elastic type `" LWPDL "` (%zd).", LWSTR(type_name),
		type_name->cnt);
	return FALSE;
}

/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/display-size
 */
static void set_display_size(esodbc_estype_st *es_type)
{
	switch (es_type->data_type) {
		case SQL_CHAR:
		case SQL_VARCHAR: /* KEYWORD, TEXT */
		case SQL_LONGVARCHAR:
		case SQL_WCHAR:
		case SQL_WVARCHAR:
		case SQL_WLONGVARCHAR:
			es_type->display_size = es_type->column_size;
			break;

		case SQL_REAL: /* FLOAT */
			es_type->display_size = 14;
			break;
		case SQL_FLOAT: /* HALF_, SCALED_FLOAT */
		case SQL_DOUBLE: /* DOUBLE */
			es_type->display_size = 24;
			break;

		case SQL_TINYINT: /* BYTE */
		case SQL_SMALLINT: /* SHORT */
		case SQL_INTEGER: /* INTEGER */
			es_type->display_size = es_type->column_size;
			if (! es_type->unsigned_attribute) {
				es_type->display_size ++;
			}
			break;
		case SQL_BIGINT: /* LONG */
			es_type->display_size = es_type->column_size;
			if (es_type->unsigned_attribute) {
				es_type->display_size ++;
			}
			break;

		case SQL_BINARY:
		case SQL_VARBINARY: /* BINARY */
		case SQL_LONGVARBINARY:
			/* 0xAB */
			es_type->display_size = 2 * es_type->column_size;
			break;

		case SQL_TYPE_DATE:
		case SQL_TYPE_TIME:
		case SQL_TYPE_TIMESTAMP: /* SQL/ES DATE */
			es_type->display_size = sizeof(ESODBC_ISO8601_TEMPLATE) - /*0*/1;
			break;


		case ESODBC_SQL_BOOLEAN:
			es_type->display_size = /*'false'*/5;
			break;

		case ESODBC_SQL_NULL:
			es_type->display_size = /*'null'*/4;
			break;

		/* treat these as variable binaries with unknown size */
		case ESODBC_SQL_UNSUPPORTED:
		case ESODBC_SQL_OBJECT: /* == ESODBC_SQL_NESTED */
			es_type->display_size = SQL_NO_TOTAL;
			break;

		/*
		case SQL_TYPE_UTCDATETIME:
		case SQL_TYPE_UTCTIME:
		*/

		case SQL_DECIMAL:
		case SQL_NUMERIC:

		case SQL_BIT:

		case SQL_INTERVAL_MONTH:
		case SQL_INTERVAL_YEAR:
		case SQL_INTERVAL_YEAR_TO_MONTH:
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

		case SQL_GUID:

		default:
			BUG("unsupported ES/SQL data type: %d.", es_type->data_type);
	}
}

static BOOL bind_types_cols(esodbc_stmt_st *stmt, estype_row_st *type_row)
{

	/* bind one column */
#define ES_TYPES_BINDCOL(_col_nr, _member, _c_type) \
	do { \
		SQLPOINTER _ptr = _c_type == SQL_C_WCHAR ? \
			(SQLPOINTER)(uintptr_t)type_row[0]._member : \
			(SQLPOINTER)&type_row[0]._member; \
		if (! SQL_SUCCEEDED(EsSQLBindCol(stmt, _col_nr, _c_type, \
					_ptr, sizeof(type_row[0]._member), \
					&type_row[0]._member ## _loi))) { \
			ERRH(stmt, "failed to bind column #" STR(_col_nr) "."); \
			return FALSE; \
		} \
	} while (0)

	ES_TYPES_BINDCOL(1, type_name, SQL_C_WCHAR);
	ES_TYPES_BINDCOL(2, data_type, SQL_C_SSHORT);
	ES_TYPES_BINDCOL(3, column_size, SQL_C_SLONG);
	ES_TYPES_BINDCOL(4, literal_prefix, SQL_C_WCHAR);
	ES_TYPES_BINDCOL(5, literal_suffix, SQL_C_WCHAR);
	ES_TYPES_BINDCOL(6, create_params, SQL_C_WCHAR);
	ES_TYPES_BINDCOL(7, nullable, SQL_C_SSHORT);
	ES_TYPES_BINDCOL(8, case_sensitive, SQL_C_SSHORT);
	ES_TYPES_BINDCOL(9, searchable, SQL_C_SSHORT);
	ES_TYPES_BINDCOL(10, unsigned_attribute, SQL_C_SSHORT);
	ES_TYPES_BINDCOL(11, fixed_prec_scale, SQL_C_SSHORT);
	ES_TYPES_BINDCOL(12, auto_unique_value, SQL_C_SSHORT);
	ES_TYPES_BINDCOL(13, local_type_name, SQL_C_WCHAR);
	ES_TYPES_BINDCOL(14, minimum_scale, SQL_C_SSHORT);
	ES_TYPES_BINDCOL(15, maximum_scale, SQL_C_SSHORT);
	ES_TYPES_BINDCOL(16, sql_data_type, SQL_C_SSHORT);
	ES_TYPES_BINDCOL(17, sql_datetime_sub, SQL_C_SSHORT);
	ES_TYPES_BINDCOL(18, num_prec_radix, SQL_C_SLONG);
	ES_TYPES_BINDCOL(19, interval_precision, SQL_C_SSHORT);

#undef ES_TYPES_BINDCOL

	return TRUE;
}

/* Copies the type info from the result-set to array to be associated with the
 * connection. */
static void *copy_types_rows(estype_row_st *type_row, SQLULEN rows_fetched,
	esodbc_estype_st *types)
{
	SQLWCHAR *pos;
	int c;
	SQLULEN i;
	SQLSMALLINT sql_type;

	/* pointer to start position where the strings will be copied in */
	pos = (SQLWCHAR *)&types[rows_fetched];

	/* copy one integer member */
#define ES_TYPES_COPY_INT(_member) \
	do { \
		if (type_row[i]._member ## _loi == SQL_NULL_DATA) { \
			/*null->0 is harmless for cached types */\
			types[i]._member = 0; \
		} else { \
			types[i]._member = type_row[i]._member; \
		} \
	} while (0)
	/* copy one wstr_st member
	 * Note: it'll shift NULLs to empty strings, as most of the API asks for
	 * empty strings if data is unavailable ("unkown"). */
#define ES_TYPES_COPY_WSTR(_wmember) \
	do { \
		if (type_row[i]._wmember ## _loi == SQL_NULL_DATA) { \
			types[i]._wmember.cnt = 0; \
			types[i]._wmember.str = MK_WPTR(""); \
		} else { \
			types[i]._wmember.cnt = \
				type_row[i]._wmember ## _loi / sizeof(SQLWCHAR); \
			types[i]._wmember.str = pos; \
			wmemcpy(types[i]._wmember.str, \
				type_row[i]._wmember, types[i]._wmember.cnt + /*\0*/1); \
			pos += types[i]._wmember.cnt + /*\0*/1; \
		} \
	} while (0)

	for (i = 0; i < rows_fetched; i ++) {
		/* copy row data */
		ES_TYPES_COPY_WSTR(type_name);
		ES_TYPES_COPY_INT(data_type);
		ES_TYPES_COPY_INT(column_size);
		ES_TYPES_COPY_WSTR(literal_prefix);
		ES_TYPES_COPY_WSTR(literal_suffix);
		ES_TYPES_COPY_WSTR(create_params);
		ES_TYPES_COPY_INT(nullable);
		ES_TYPES_COPY_INT(case_sensitive);
		ES_TYPES_COPY_INT(searchable);
		ES_TYPES_COPY_INT(unsigned_attribute);
		ES_TYPES_COPY_INT(fixed_prec_scale);
		ES_TYPES_COPY_INT(auto_unique_value);
		ES_TYPES_COPY_WSTR(local_type_name);
		ES_TYPES_COPY_INT(minimum_scale);
		ES_TYPES_COPY_INT(maximum_scale);
		ES_TYPES_COPY_INT(sql_data_type);
		ES_TYPES_COPY_INT(sql_datetime_sub);
		ES_TYPES_COPY_INT(num_prec_radix);
		ES_TYPES_COPY_INT(interval_precision);

		/* convert type_name to C and copy it too */
		types[i].type_name_c.str = (char *)pos; /**/
		pos += types[i].type_name.cnt + /*\0*/1;
		if ((c = ansi_w2c(types[i].type_name.str, types[i].type_name_c.str,
						types[i].type_name.cnt)) < 0) {
			ERR("failed to convert ES/SQL type `" LWPDL "` to C-str.",
				LWSTR(&types[i].type_name));
			return NULL;
		} else {
			assert(c == types[i].type_name.cnt + /*\0*/1);
			types[i].type_name_c.cnt = types[i].type_name.cnt;
		}

		/*
		 * apply any needed fixes
		 */

		/* notify if scales extremes are different */
		if (types[i].maximum_scale != types[i].minimum_scale) {
			INFO("type `" LWPDL "` returned with non-equal max/min "
				"scale: %d/%d -- using the max.", LWSTR(&types[i].type_name),
				types[i].maximum_scale, types[i].minimum_scale);
		}

		/* resolve ES type to SQL C type */
		if (! elastic_name2types(&types[i].type_name, &types[i].c_concise_type,
				&sql_type)) {
			/* ES version newer than driver's? */
			ERR("failed to convert type name `" LWPDL "` to SQL C type.",
				LWSTR(&types[i].type_name));
			return NULL;
		}
		/* .data_type is used in data conversions -> make sure the SQL type
		 * derived from type's name is the same with type reported value */
		if (sql_type != types[i].data_type) {
			ERR("type `" LWPDL "` derived (%d) and reported (%d) SQL type "
				"identifiers differ.", LWSTR(&types[i].type_name),
				sql_type, types[i].data_type);
			return NULL;
		}
		/* set meta type */
		types[i].meta_type = concise_to_meta(types[i].c_concise_type,
				/*C type -> AxD*/DESC_TYPE_ARD);

		/* fix SQL_DATA_TYPE and SQL_DATETIME_SUB columns TODO: GH issue */
		concise_to_type_code(types[i].data_type, &types[i].sql_data_type,
			&types[i].sql_datetime_sub);

		set_display_size(types + i);
	}

#undef ES_TYPES_COPY_INT
#undef ES_TYPES_COPY_WCHAR

	return pos;
}

/* associate the types with the DBC: store the refs and store the max
 * type size/length for SQL type ID-overlapping ES/SQL types */
static void set_es_types(esodbc_dbc_st *dbc, SQLULEN rows_fetched,
	esodbc_estype_st *types)
{
	SQLULEN i;

	assert(!dbc->max_float_size && !dbc->max_varchar_size);

	for (i = 0; i < rows_fetched; i ++) {
		if (types[i].data_type == SQL_FLOAT) {
			if (dbc->max_float_size < types[i].column_size) {
				dbc->max_float_size = types[i].column_size;
			}
		} else if (types[i].data_type == SQL_VARCHAR) {
			if (dbc->max_varchar_size < types[i].column_size) {
				dbc->max_varchar_size = types[i].column_size;
			}
		}
	}

	DBGH(dbc, "%llu ES/SQL types available, maximum sizes supported for: "
		"SQL_FLOAT: %ld, SQL_VARCHAR: %ld.", rows_fetched,
		dbc->max_float_size, dbc->max_varchar_size);
	dbc->es_types = types;
	dbc->no_types = rows_fetched;
}

/*
 * Load SYS TYPES data.
 *
 * One can not do a row-wise rowset fetch with ODBC where the
 * length-indicator buffer is separate from the row structure => need to
 * "manually" copy from a row structure (defined into the function) into the
 * estype structure. The array of estype structs is returned.
 */
static BOOL load_es_types(esodbc_dbc_st *dbc)
{
	esodbc_stmt_st *stmt = NULL;
	SQLRETURN ret = FALSE;
	SQLSMALLINT col_cnt;
	SQLLEN row_cnt;
	/* both arrays below must use ESODBC_MAX_ROW_ARRAY_SIZE since no SQLFetch()
	 * looping is implemented (see check after SQLFetch() below). */
	SQLUSMALLINT row_status[ESODBC_MAX_ROW_ARRAY_SIZE];
	/* a static estype_row_st array is over 350KB and too big for the default
	 * stack size in certain cases: needs allocation on heap */
	estype_row_st *type_row = NULL;
	SQLULEN rows_fetched, i, strs_len;
	size_t size;
	esodbc_estype_st *types = NULL;
	void *pos;

	type_row = calloc(ESODBC_MAX_ROW_ARRAY_SIZE, sizeof(estype_row_st));
	if (! type_row) {
		ERRNH(dbc, "OOM");
		return FALSE;
	}

	if (! SQL_SUCCEEDED(EsSQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) {
		ERRH(dbc, "failed to alloc a statement handle.");
		free(type_row);
		return FALSE;
	}
	assert(stmt);

#ifdef TESTING
	/* for testing cases with no ES server available, the connection needs to
	 * be init'ed with ES types: the test suite passes a JSON answer of ES
	 * types through the (otherwise) unused window handler pointer */
	if (dbc->hwin) {
		cstr_st *types_answer = (cstr_st *)dbc->hwin;
		dbc->hwin = NULL;
		if (! SQL_SUCCEEDED(attach_answer(stmt, types_answer->str,
					types_answer->cnt))) {
			ERRH(stmt, "failed to attach dummmy ES types answer");
			goto end;
		}
	} else {
#else /* TESTING */
	if (TRUE) {
#endif /* TESTING */
		if (! SQL_SUCCEEDED(EsSQLGetTypeInfoW(stmt, SQL_ALL_TYPES))) {
			ERRH(stmt, "failed to query Elasticsearch.");
			goto end;
		}
	}

	/* check that we have as many columns as members in target row struct */
	if (! SQL_SUCCEEDED(EsSQLNumResultCols(stmt, &col_cnt))) {
		ERRH(stmt, "failed to get result columns count.");
		goto end;
	} else if (col_cnt != ESODBC_TYPES_COLUMNS) {
		ERRH(stmt, "Elasticsearch returned an unexpected number of columns "
			"(%d vs expected %d).", col_cnt, ESODBC_TYPES_COLUMNS);
		goto end;
	} else {
		DBGH(stmt, "Elasticsearch types columns count: %d.", col_cnt);
	}

	/* check that we have received proper number of rows (non-0, less than
	 * max allowed here) */
	if (! SQL_SUCCEEDED(EsSQLRowCount(stmt, &row_cnt))) {
		ERRH(stmt, "failed to get result rows count.");
		goto end;
	} else if (row_cnt <= 0) {
		ERRH(stmt, "Elasticsearch returned no type as supported.");
		goto end;
	} else if (ESODBC_MAX_ROW_ARRAY_SIZE < row_cnt) {
		ERRH(stmt, "Elasticsearch returned too many types (%d vs limit %zd).",
			row_cnt, ESODBC_MAX_ROW_ARRAY_SIZE);
		goto end;
	} else {
		DBGH(stmt, "Elasticsearch types rows count: %ld.", row_cnt);
	}

	/* indicate bind type: row-wise (i.e. row size) */
	if (! SQL_SUCCEEDED(EsSQLSetStmtAttrW(stmt, SQL_ATTR_ROW_BIND_TYPE,
				(SQLPOINTER)sizeof(type_row[0]), 0))) {
		ERRH(stmt, "failed to set bind type to row-wise.");
		goto end;
	}
	/* indicate rowset size */
	if (! SQL_SUCCEEDED(EsSQLSetStmtAttrW(stmt, SQL_ATTR_ROW_ARRAY_SIZE,
				(SQLPOINTER)ESODBC_MAX_ROW_ARRAY_SIZE, 0))) {
		ERRH(stmt, "failed to set rowset size (%zd).",
			ESODBC_MAX_ROW_ARRAY_SIZE);
		goto end;
	}
	/* indicate array to write row status into; initialize with error first */
	for (i = 0; i < ESODBC_MAX_ROW_ARRAY_SIZE; i ++) {
		row_status[i] = SQL_ROW_ERROR;
	}
	if (! SQL_SUCCEEDED(EsSQLSetStmtAttrW(stmt, SQL_ATTR_ROW_STATUS_PTR,
				row_status, 0))) {
		ERRH(stmt, "failed to set row status array.");
		goto end;
	}
	/* indicate pointer to write how many rows were fetched into */
	if (! SQL_SUCCEEDED(EsSQLSetStmtAttrW(stmt, SQL_ATTR_ROWS_FETCHED_PTR,
				&rows_fetched, 0))) {
		ERRH(stmt, "failed to set fetched size pointer.");
		goto end;
	}

	if (! bind_types_cols(stmt, type_row)) {
		goto end;
	}

	/* don't check for data compatiblity (since the check needs data fetched
	 * by this function) */
	stmt->sql2c_conversion = CONVERSION_SKIPPED;

	/* fetch the results into the type_row array */
	if (! SQL_SUCCEEDED(EsSQLFetch(stmt))) {
		ERRH(stmt, "failed to fetch results.");
		goto end;
	} else if (rows_fetched < (SQLULEN)row_cnt) {
		/* we're using arrays with max size that SQLFetch() accepts => it
		 * should read all data in one go. */
		ERRH(stmt, "failed to fetch all the rows in one go.");
		goto end;
	}

	/* check row statuses;
	 * calculate the length of all strings (SQLWCHAR members) returned
	 * count also the 0-terms, which are not counted for in the indicator */
	strs_len = 0;
	for (i = 0; i < rows_fetched; i ++) {
		if (row_status[i] != SQL_ROW_SUCCESS) {
			ERRH(stmt, "row #%d not succesfully fetched; status: %d.", i,
				row_status[i]);
			goto end;
		}
		if (type_row[i].type_name_loi != SQL_NULL_DATA) {
			/* 2x: also reserve room for type's C-string name: reserve as much
			 * space as for the wide version (1x), to laster simplify pointer
			 * indexing (despite the small waste). */
			strs_len += 2* type_row[i].type_name_loi;
			strs_len += 2* sizeof(*type_row[i].type_name); /* 0-term */
		}
		if (type_row[i].literal_prefix_loi != SQL_NULL_DATA) {
			strs_len += type_row[i].literal_prefix_loi;
			strs_len += sizeof(*type_row[i].literal_prefix); /* 0-term */
		}
		if (type_row[i].literal_suffix_loi != SQL_NULL_DATA) {
			strs_len += type_row[i].literal_suffix_loi;
			strs_len += sizeof(*type_row[i].literal_suffix); /* 0-term */
		}
		if (type_row[i].create_params_loi != SQL_NULL_DATA) {
			strs_len += type_row[i].create_params_loi;
			strs_len += sizeof(*type_row[i].create_params); /* 0-term */
		}
		if (type_row[i].local_type_name_loi != SQL_NULL_DATA) {
			strs_len += type_row[i].local_type_name_loi;
			strs_len += sizeof(*type_row[i].local_type_name); /* 0-term */
		}
	}

	/* collate types array and the strings referenced within it */
	size = rows_fetched * sizeof(esodbc_estype_st) + strs_len;
	if (! (types = calloc(1, size))) {
		ERRNH(stmt, "OOM for %ldB.", size);
		goto end;
	}

	if (! (pos = copy_types_rows(type_row, rows_fetched, types))) {
		ERRH(dbc, "failed to process recieved ES/SQL data types.");
		goto end;
	}
	/* I didn't overrun the buffer */
	assert((char *)pos - (char *)(types + rows_fetched) <=
		(intptr_t)(strs_len));

	ret = TRUE;
end:
	if (! SQL_SUCCEEDED(EsSQLFreeStmt(stmt, SQL_UNBIND))) {
		ERRH(stmt, "failed to unbind statement");
		ret = FALSE;
	}
	if (! SQL_SUCCEEDED(EsSQLFreeHandle(SQL_HANDLE_STMT, stmt))) {
		ERRH(dbc, "failed to free statement handle!");
		ret = FALSE;
	}

	if (ret) {
		/* finally, associate the types to the dbc handle */
		set_es_types(dbc, rows_fetched, types);
	} else if (types) {
		/* freeing the statement went wrong */
		free(types);
		types = NULL;
	}
	if (type_row) {
		free(type_row);
		type_row = NULL;
	}

	return ret;
}

SQLRETURN EsSQLDriverConnectW
(
	SQLHDBC             hdbc,
	SQLHWND             hwnd,
	/* "A full connection string, a partial connection string, or an empty
	 * string" */
	_In_reads_(cchConnStrIn) SQLWCHAR *szConnStrIn,
	/* "Length of *InConnectionString, in characters if the string is
	 * Unicode, or bytes if string is ANSI or DBCS." */
	SQLSMALLINT         cchConnStrIn,
	/* "Pointer to a buffer for the completed connection string. Upon
	 * successful connection to the target data source, this buffer
	 * contains the completed connection string." */
	_Out_writes_opt_(cchConnStrOutMax) SQLWCHAR *szConnStrOut,
	/* "Length of the *OutConnectionString buffer, in characters." */
	SQLSMALLINT         cchConnStrOutMax,
	/* "Pointer to a buffer in which to return the total number of
	 * characters (excluding the null-termination character) available to
	 * return in *OutConnectionString" */
	_Out_opt_ SQLSMALLINT        *pcchConnStrOut,
	/* "Flag that indicates whether the Driver Manager or driver must
	 * prompt for more connection information" */
	SQLUSMALLINT        fDriverCompletion
)
{
	esodbc_dbc_st *dbc = DBCH(hdbc);
	SQLRETURN ret;
	esodbc_dsn_attrs_st attrs;
	SQLWCHAR buff_dsn[ESODBC_DSN_MAX_ATTR_LEN];
	wstr_st orig_dsn = {buff_dsn, 0};
	BOOL disable_nonconn = FALSE;
	BOOL prompt_user = TRUE;
	int res;

	init_dsn_attrs(&attrs);

	if (szConnStrIn) {
		DBGH(dbc, "Input connection string: '"LWPD"'[%d].", szConnStrIn,
			cchConnStrIn);
		/* parse conn str into attrs */
		if (! parse_connection_string(&attrs, szConnStrIn, cchConnStrIn)) {
			ERRH(dbc, "failed to parse connection string `" LWPDL "`.",
				cchConnStrIn < 0 ? wcslen(szConnStrIn) : cchConnStrIn,
				szConnStrIn);
			RET_HDIAGS(dbc, SQL_STATE_HY000);
		}
		/* original received DSN saved for later query by the app */
		memcpy(orig_dsn.str, attrs.dsn.str,
			attrs.dsn.cnt * sizeof(*attrs.dsn.str));
		orig_dsn.cnt = attrs.dsn.cnt;

		/* set DSN (to DEFAULT) only if both DSN and Driver kw are missing */
		if ((! attrs.driver.cnt) && (! attrs.dsn.cnt)) {
			/* If the connection string does not contain the DSN keyword, the
			 * specified data source is not found, or the DSN keyword is set
			 * to "DEFAULT", the driver retrieves the information for the
			 * Default data source. */
			INFOH(dbc, "no DRIVER or DSN keyword found in connection string: "
				"using the \"DEFAULT\" DSN.");
			res = assign_dsn_attr(&attrs, &MK_WSTR(ESODBC_DSN_DSN),
					&MK_WSTR("DEFAULT"), /*overwrite?*/TRUE);
			assert(0 < res);
		}
	} else {
		INFOH(dbc, "empty connection string: using the \"DEFAULT\" DSN.");
		res = assign_dsn_attr(&attrs, &MK_WSTR(ESODBC_DSN_DSN),
				&MK_WSTR("DEFAULT"), /*overwrite?*/TRUE);
		assert(0 < res);
	}
	assert(attrs.driver.cnt || attrs.dsn.cnt);

	if (attrs.dsn.cnt) {
		/* "The driver uses any information it retrieves from the system
		 * information to augment the information passed to it in the
		 * connection string. If the information in the system information
		 * duplicates information in the connection string, the driver uses
		 * the information in the connection string." */
		INFOH(dbc, "configuring the driver by DSN '" LWPDL "'.",
			LWSTR(&attrs.dsn));
		if (! read_system_info(&attrs)) {
			/* warn, but try to carry on */
			WARNH(dbc, "failed to read system info for DSN '" LWPDL "' data.",
				LWSTR(&attrs.dsn));
			/* DM should take care of this, but just in case */
			if (! EQ_WSTR(&attrs.dsn, &MK_WSTR("DEFAULT"))) {
				res = assign_dsn_attr(&attrs, &MK_WSTR(ESODBC_DSN_DSN),
						&MK_WSTR("DEFAULT"), /*overwrite?*/TRUE);
				assert(0 < res);
				if (! read_system_info(&attrs)) {
					ERRH(dbc, "failed to read system info for default DSN.");
					RET_HDIAGS(dbc, SQL_STATE_IM002);
				}
			}
		}
	} else {
		/* "If the connection string contains the DRIVER keyword, the driver
		 * cannot retrieve information about the data source from the system
		 * information." */
		INFOH(dbc, "configuring the driver '" LWPDL "'.",
			LWSTR(&attrs.driver));
	}

	/* whatever attributes haven't yet been set, init them with defaults */
	assign_dsn_defaults(&attrs);

	switch (fDriverCompletion) {
		case SQL_DRIVER_NOPROMPT:
			ret = do_connect(dbc, &attrs);
			if (! SQL_SUCCEEDED(ret)) {
				return ret;
			}
			break;

		case SQL_DRIVER_COMPLETE_REQUIRED:
			disable_nonconn = TRUE;
		/* no break */
		case SQL_DRIVER_COMPLETE:
			/* try connect first, then, if that fails, prompt user */
			prompt_user = FALSE;
		/* no break; */
		case SQL_DRIVER_PROMPT: /* prompt user first, then try connect */
			do {
				res = prompt_user ?
					prompt_user_config(hdbc, &attrs, FALSE) : 1;
				if (res < 0) {
					ERRH(dbc, "user interaction failed.");
					RET_HDIAGS(dbc, SQL_STATE_IM008);
				} else if (! res) {
					/* user canceled */
					return SQL_NO_DATA;
				}
				/* promt user on next iteration */
				prompt_user = TRUE;
			} while (! SQL_SUCCEEDED(do_connect(dbc, &attrs)));
			break;

#ifdef TESTING
		case ESODBC_SQL_DRIVER_TEST:
			/* abusing the window handler to pass type data for non-network
			 * tests (see load_es_types()). */
			assert(! dbc->hwin);
			dbc->hwin = hwnd;
			break;
#endif /* TESTING */

		default:
			ERRH(dbc, "unknown driver completion mode: %d", fDriverCompletion);
			RET_HDIAGS(dbc, SQL_STATE_HY110);
	}

	if (! load_es_types(dbc)) {
		ERRH(dbc, "failed to load Elasticsearch/SQL types.");
		RET_HDIAG(dbc, SQL_STATE_HY000,
			"failed to load Elasticsearch/SQL types", 0);
	}

	/* save the original DSN for later inquiry by app */
	dbc->dsn.str = malloc((orig_dsn.cnt + /*0*/1) * sizeof(SQLWCHAR));
	if (! dbc->dsn.str) {
		ERRNH(dbc, "OOM for %zdB.", (orig_dsn.cnt + 1) * sizeof(SQLWCHAR));
		RET_HDIAGS(dbc, SQL_STATE_HY001);
	}
	dbc->dsn.str[orig_dsn.cnt] = '\0';
	wcsncpy(dbc->dsn.str, orig_dsn.str, orig_dsn.cnt);
	dbc->dsn.cnt = orig_dsn.cnt;

	/* return the final connection string */
	if (szConnStrOut || pcchConnStrOut) {
		/* might have been reset to DEFAULT, if orig was not found */
		res = assign_dsn_attr(&attrs, &MK_WSTR(ESODBC_DSN_DSN), &orig_dsn,
				/*overwrite?*/TRUE);
		assert(0 < res);
		if (! write_connection_string(&attrs, szConnStrOut, cchConnStrOutMax,
				pcchConnStrOut)) {
			ERRH(dbc, "failed to build output connection string.");
			RET_HDIAG(dbc, SQL_STATE_HY000, "failed to build connection "
				"string", 0);
		}
	}

	return SQL_SUCCESS;
}

/* "Implicitly allocated descriptors can be freed only by calling
 * SQLDisconnect, which drops any statements or descriptors open on the
 * connection" */
SQLRETURN EsSQLDisconnect(SQLHDBC ConnectionHandle)
{
	esodbc_dbc_st *dbc = DBCH(ConnectionHandle);
	cleanup_dbc(dbc);
	return SQL_SUCCESS;
}



/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/unicode-drivers :
 * """
 * When determining the driver type, the Driver Manager will call
 * SQLSetConnectAttr and set the SQL_ATTR_ANSI_APP attribute at connection
 * time. If the application is using ANSI APIs, SQL_ATTR_ANSI_APP will be set
 * to SQL_AA_TRUE, and if it is using Unicode, it will be set to a value of
 * SQL_AA_FALSE.  If a driver exhibits the same behavior for both ANSI and
 * Unicode applications, it should return SQL_ERROR for this attribute. If the
 * driver returns SQL_SUCCESS, the Driver Manager will separate ANSI and
 * Unicode connections when Connection Pooling is used.
 * """
 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/statement-attributes :
 * """
 * The ability to set statement attributes at the connection level by calling
 * SQLSetConnectAttr has been deprecated in ODBC 3.x. ODBC 3.x applications
 * should never set statement attributes at the connection level. ODBC 3.x
 * drivers need only support this functionality if they should work with ODBC
 * 2.x applications.
 * """
 */
SQLRETURN EsSQLSetConnectAttrW(
	SQLHDBC ConnectionHandle,
	SQLINTEGER Attribute,
	_In_reads_bytes_opt_(StringLength) SQLPOINTER Value,
	SQLINTEGER StringLength)
{
	esodbc_dbc_st *dbc = DBCH(ConnectionHandle);

	switch(Attribute) {
		case SQL_ATTR_ANSI_APP:
			/* this driver doesn't behave differently based on app being ANSI
			 * or Unicode. */
			INFOH(dbc, "no ANSI/Unicode specific behaviour (app is: %s).",
				(uintptr_t)Value == SQL_AA_TRUE ? "ANSI" : "Unicode");
			/* TODO: API doesn't require to set a state? */
			//state = SQL_STATE_IM001;
			return SQL_ERROR; /* error means ANSI */

		case SQL_ATTR_LOGIN_TIMEOUT:
			if (dbc->es_types) {
				ERRH(dbc, "connection already established, can't set "
					"connection timeout anymore (to %u).",
					(SQLUINTEGER)(uintptr_t)Value);
				RET_HDIAG(dbc, SQL_STATE_HY011, "connection established, "
					"can't set connection timeout.", 0);
			}
			INFOH(dbc, "setting connection timeout to: %u, from previous: %u.",
				dbc->timeout, (SQLUINTEGER)(uintptr_t)Value);
			dbc->timeout = (long)(uintptr_t)Value;
			break;

		/* https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/automatic-population-of-the-ipd */
		case SQL_ATTR_AUTO_IPD:
			ERRH(dbc, "trying to set read-only attribute AUTO IPD.");
			RET_HDIAGS(dbc, SQL_STATE_HY092);
		case SQL_ATTR_ENABLE_AUTO_IPD:
			if (*(SQLUINTEGER *)Value != SQL_FALSE) {
				ERRH(dbc, "trying to enable unsupported attribute AUTO IPD.");
				RET_HDIAGS(dbc, SQL_STATE_HYC00);
			}
			WARNH(dbc, "disabling (unsupported) attribute AUTO IPD -- NOOP.");
			break;

		case SQL_ATTR_METADATA_ID:
			DBGH(dbc, "setting metadata_id to %u.", (SQLULEN)Value);
			dbc->metadata_id = (SQLULEN)Value;
			break;

		case SQL_ATTR_ASYNC_ENABLE:
			ERRH(dbc, "no support for async API (setting param: %llu)",
				(SQLULEN)(uintptr_t)Value);
			if ((SQLULEN)(uintptr_t)Value == SQL_ASYNC_ENABLE_ON) {
				RET_HDIAGS(dbc, SQL_STATE_HYC00);
			}
			break;
		case SQL_ATTR_ASYNC_DBC_FUNCTIONS_ENABLE:
			ERRH(dbc, "no support for async API (setting param: %llu)",
				(SQLULEN)(uintptr_t)Value);
			if ((SQLULEN)(uintptr_t)Value == SQL_ASYNC_DBC_ENABLE_ON) {
				RET_HDIAGS(dbc, SQL_STATE_HY114);
			}
			break;
		case SQL_ATTR_ASYNC_DBC_EVENT:
			// case SQL_ATTR_ASYNC_DBC_PCALLBACK:
			// case SQL_ATTR_ASYNC_DBC_PCONTEXT:
			ERRH(dbc, "no support for async API (attr: %ld)", Attribute);
			RET_HDIAGS(dbc, SQL_STATE_S1118);

		case SQL_ATTR_QUIET_MODE:
			DBGH(dbc, "setting window handler to 0x%p.", Value);
			dbc->hwin = (HWND)Value;
			break;

		case SQL_ATTR_TXN_ISOLATION:
			DBGH(dbc, "setting transaction isolation to: %u.",
				(SQLUINTEGER)(uintptr_t)Value);
			dbc->txn_isolation = (SQLUINTEGER)(uintptr_t)Value;
			break;

		default:
			ERRH(dbc, "unknown Attribute: %d.", Attribute);
			RET_HDIAGS(DBCH(ConnectionHandle), SQL_STATE_HY092);
	}

	return SQL_SUCCESS;
}

SQLRETURN EsSQLGetConnectAttrW(
	SQLHDBC        ConnectionHandle,
	SQLINTEGER     Attribute,
	_Out_writes_opt_(_Inexpressible_(cbValueMax)) SQLPOINTER ValuePtr,
	SQLINTEGER     BufferLength,
	_Out_opt_ SQLINTEGER *StringLengthPtr)
{
	esodbc_dbc_st *dbc = DBCH(ConnectionHandle);
	SQLSMALLINT used;

	switch(Attribute) {
		/* https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/automatic-population-of-the-ipd */
		/* "whether automatic population of the IPD after a call to SQLPrepare
		 * is supported" */
		case SQL_ATTR_AUTO_IPD:
			DBGH(dbc, "requested: support for attribute AUTO IPD (false).");
			/* "Servers that do not support prepared statements will not be
			 * able to populate the IPD automatically." */
			*(SQLUINTEGER *)ValuePtr = SQL_FALSE;
			break;

		/* "the name of the catalog to be used by the data source" */
		case SQL_ATTR_CURRENT_CATALOG:
			DBGH(dbc, "requested: catalog name.");
			if (! dbc->es_types) {
				ERRH(dbc, "no connection active.");
				RET_HDIAGS(dbc, SQL_STATE_08003);
			} else if ((used = copy_current_catalog(dbc, (SQLWCHAR *)ValuePtr,
							(SQLSMALLINT)BufferLength)) < 0) {
				ERRH(dbc, "failed to get current catalog.");
				RET_STATE(dbc->hdr.diag.state);
			}
			if (StringLengthPtr) {
				*StringLengthPtr = (SQLINTEGER)used;
			}
			break;

		case SQL_ATTR_METADATA_ID:
			DBGH(dbc, "requested: metadata_id: %u.", dbc->metadata_id);
			*(SQLULEN *)ValuePtr = dbc->metadata_id;
			break;
		case SQL_ATTR_ASYNC_ENABLE:
			DBGH(dbc, "getting async mode: %llu", SQL_ASYNC_ENABLE_OFF);
			*(SQLULEN *)ValuePtr = SQL_ASYNC_ENABLE_OFF;
			break;

		case SQL_ATTR_QUIET_MODE:
			DBGH(dbc, "requested: window handler 0x%p.", dbc->hwin);
			*(HWND *)ValuePtr = dbc->hwin;
			break;

		case SQL_ATTR_TXN_ISOLATION:
			DBGH(dbc, "requested: transaction isolation: %u.",
				dbc->txn_isolation);
			*(SQLUINTEGER *)ValuePtr = dbc->txn_isolation;
			break;

		case SQL_ATTR_ACCESS_MODE:
		case SQL_ATTR_ASYNC_DBC_EVENT:
		case SQL_ATTR_ASYNC_DBC_FUNCTIONS_ENABLE:
		//case SQL_ATTR_ASYNC_DBC_PCALLBACK:
		//case SQL_ATTR_ASYNC_DBC_PCONTEXT:
		case SQL_ATTR_AUTOCOMMIT:
		case SQL_ATTR_CONNECTION_DEAD:
		case SQL_ATTR_CONNECTION_TIMEOUT:
		//case SQL_ATTR_DBC_INFO_TOKEN:
		case SQL_ATTR_ENLIST_IN_DTC:
		case SQL_ATTR_LOGIN_TIMEOUT:
		case SQL_ATTR_ODBC_CURSORS:
		case SQL_ATTR_PACKET_SIZE:
		case SQL_ATTR_TRACE:
		case SQL_ATTR_TRACEFILE:
		case SQL_ATTR_TRANSLATE_LIB:
		case SQL_ATTR_TRANSLATE_OPTION:

		default:
			// FIXME: add the other attributes
			FIXME;
			ERRH(dbc, "unknown Attribute type %d.", Attribute);
			RET_HDIAGS(DBCH(ConnectionHandle), SQL_STATE_HY092);
	}

	return SQL_SUCCESS;
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
