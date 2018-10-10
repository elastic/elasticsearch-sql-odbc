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
#define HTTP_TEST_JSON			"{\"query\": \"SELECT 0\"}"

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
	} else {
		/* if libcurl is loaded, log main attributes (most relevant for dynamic
		 * loading). */
		curl_version_info_data *curl_info = curl_version_info(CURLVERSION_NOW);
		assert(curl_info);
		/* these are available from "age" 0. */
		INFO("Using libcurl version: %s, features: 0x%x, SSL ver.: %s.",
			curl_info->version, curl_info->features,
			curl_info->ssl_version ? curl_info->ssl_version : "NONE");
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

#ifndef NDEBUG
static int debug_callback(CURL *handle, curl_infotype type, char *data,
	size_t size, void *userptr)
{
	esodbc_dbc_st *dbc = DBCH(userptr);
	char *info_type;
	char *info_types[] = {"text", "header-in", "header-out", "data-in",
			"data-out"
		};

	switch (type) {
		case CURLINFO_TEXT:
			if (size) {
				/* strip trailing \n */
				size --;
			}
		case CURLINFO_HEADER_IN:
		case CURLINFO_HEADER_OUT:
		case CURLINFO_DATA_IN:
		case CURLINFO_DATA_OUT:
			info_type = info_types[type - CURLINFO_TEXT];
			break;
		case CURLINFO_SSL_DATA_IN:
		case CURLINFO_SSL_DATA_OUT:
			return 0;
	}
	DBGH(dbc, "libcurl: %s: [%zu] `%.*s`.", info_type, size, size, data);
	return 0;
}
#endif /* NDEBUG */

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

static SQLRETURN dbc_curl_init(esodbc_dbc_st *dbc)
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

	res = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, dbc->curl_err_buff);
	dbc->curl_err_buff[0] = '\0';
	if (res != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to set error buffer.");
		goto err;
	}

	/* set URL to connect to */
	res = curl_easy_setopt(curl, CURLOPT_URL, dbc->url.str);
	if (res != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to set URL `%s`.", dbc->url.str);
		goto err;
	}
	/* always do POSTs (seconded by CURLOPT_POSTFIELDS) */
	res = curl_easy_setopt(curl, CURLOPT_POST, 1L);
	if (res != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to set method to POST.");
		goto err;
	}

	/* set the Content-Type, Accept HTTP headers */
	res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_headers);
	if (res != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to set HTTP headers list.");
		goto err;
	}

	/* set the behavior for redirection */
	res = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, dbc->follow);
	if (res != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to set redirection behavior.");
		goto err;
	}

	if (dbc->secure) {
		res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,
				ESODBC_SEC_CHECK_CA <= dbc->secure ? 1L : 0L);
		if (res != CURLE_OK) {
			ERRH(dbc, "libcurl: failed to enable CA check.");
			goto err;
		}
		if (ESODBC_SEC_CHECK_CA <= dbc->secure) {
			/* set path to CA */
			if (dbc->ca_path.cnt) {
				res = curl_easy_setopt(curl, CURLOPT_CAINFO, dbc->ca_path.str);
				if (res != CURLE_OK) {
					ERRH(dbc, "libcurl: failed to set CA path.");
					goto err;
				}
			}
			/* verify host name */
			res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST,
					ESODBC_SEC_CHECK_HOST <= dbc->secure ? 2L : 0L);
			if (res != CURLE_OK) {
				ERRH(dbc, "libcurl: failed to enable host check.");
				goto err;
			}
			/* verify the revocation chain? */
			res = curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS,
					ESODBC_SEC_CHECK_REVOKE <= dbc->secure ?
					0L : CURLSSLOPT_NO_REVOKE);
			if (res != CURLE_OK) {
				ERRH(dbc, "libcurl: failed to enable host check.");
				goto err;
			}
		}

		/*
		 * TODO expose: CURLOPT_SSLVERSION, CURLOPT_SSLCERTTYPE
		 * CURLOPT_SSL_ENABLE_ALPN, CURLOPT_SSL_ENABLE_NPN,
		 * (CURLOPT_SSL_FALSESTART), CURLOPT_SSL_VERIFYSTATUS,
		 * CURLOPT_PROXY_*
		 */
	}

	/* set authentication parameters */
	if (dbc->uid.cnt) {
		/* set the authentication methods:
		 * "basic" is currently - 7.0.0 - the only supported method */
		/* Note: libcurl (7.61.0) won't pick Basic auth over SSL with _ANY */
		res = curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
		if (res != CURLE_OK) {
			ERRH(dbc, "libcurl: failed to set HTTP auth methods.");
			goto err;
		}
		/* set the username */
		res = curl_easy_setopt(curl, CURLOPT_USERNAME, dbc->uid.str);
		if (res != CURLE_OK) {
			ERRH(dbc, "libcurl: failed to set auth username.");
			goto err;
		}
		/* set the password */
		if (dbc->pwd.cnt) {
			res = curl_easy_setopt(curl, CURLOPT_PASSWORD, dbc->pwd.str);
			if (res != CURLE_OK) {
				ERRH(dbc, "libcurl: failed to set auth password.");
				goto err;
			}
		} else {
			/* not an error per se, but make it always visible */
			ERRH(dbc, "no password provided for username `" LCPDL "`! "
				"Intended?", LCSTR(&dbc->uid));
		}
		if (dbc->follow) {
			/* restrict sharing credentials to first contacted host? */
			res = curl_easy_setopt(curl, CURLOPT_UNRESTRICTED_AUTH,
					/* if not secure, "make it work" */
					dbc->secure ? 0L : 1L);
			if (res != CURLE_OK) {
				ERRH(dbc, "libcurl: failed to set unrestricted auth.");
				goto err;
			}
		}
	} else {
		INFOH(dbc, "no username provided: auth disabled.");
	}

	/* set the write call-back for answers */
	res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	if (res != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to set write callback.");
		goto err;
	}
	/* ... and its argument */
	res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, dbc);
	if (res != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to set callback argument.");
		goto err;
	}

#ifndef NDEBUG
	if (LOG_LEVEL_DBG <= _esodbc_log_level) {
		res = curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debug_callback);
		if (res != CURLE_OK) {
			ERRH(dbc, "libcurl: failed to set debug callback.");
			goto err;
		}
		/* ... and its argument */
		res = curl_easy_setopt(curl, CURLOPT_DEBUGDATA, dbc);
		if (res != CURLE_OK) {
			ERRH(dbc, "libcurl: failed to set dbg callback argument.");
			goto err;
		}
		res = curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		if (res != CURLE_OK) {
			ERRH(dbc, "libcurl: failed to activate verbose mode.");
			goto err;
		}
	}
#endif /* NDEBUG */

	dbc->curl = curl;
	DBGH(dbc, "libcurl: new handle 0x%p.", curl);
	return SQL_SUCCESS;

err:
	assert(res != CURLE_OK);
	ERRH(dbc, "libcurl: failure code %d, message: %s.", res,
		curl_easy_strerror(res));
	assert(curl);
	curl_easy_cleanup(curl);
	return post_c_diagnostic(&dbc->hdr.diag, SQL_STATE_HY000,
			/* propagate CURL's message, if there's any available */
			dbc->curl_err_buff[0] ? dbc->curl_err_buff :
			"failed to init the transport", res);
}

static void cleanup_curl(esodbc_dbc_st *dbc)
{
	if (! dbc->curl) {
		return;
	}
	DBGH(dbc, "libcurl: handle 0x%p cleanup.", dbc->curl);
	dbc->curl_err = CURLE_OK;
	dbc->curl_err_buff[0] = '\0';

	curl_easy_cleanup(dbc->curl);
	dbc->curl = NULL;
}

static BOOL dbc_curl_perform(esodbc_dbc_st *dbc, long *code, cstr_st *resp)
{
	assert(dbc->abuff == NULL);

	/* execute the request */
	dbc->curl_err = curl_easy_perform(dbc->curl);

	/* copy answer references */
	resp->str = dbc->abuff;
	resp->cnt = dbc->apos;

	/* clear call-back members for next call */
	dbc->abuff = NULL;
	dbc->apos = 0;
	dbc->alen = 0;

	if (dbc->curl_err != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to perform.");
		goto err;
	}
	dbc->curl_err = curl_easy_getinfo(dbc->curl, CURLINFO_RESPONSE_CODE, code);
	if (dbc->curl_err != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to retrieve response code.");
		goto err;
	}

	DBGH(dbc, "libcurl: request answered, received code %ld and %zd bytes"
		" back.", *code, resp->cnt);

	return TRUE;

err:
	ERRH(dbc, "libcurl: failure code %d, message: %s.", dbc->curl_err,
		curl_easy_strerror(dbc->curl_err));
	return FALSE;
}

static BOOL dbc_curl_prepare(esodbc_dbc_st *dbc, SQLULEN tout,
	const cstr_st *u8body)
{
	assert(dbc->curl);

	dbc->curl_err = CURLE_OK;
	dbc->curl_err_buff[0] = '\0';

	if (0 < tout) {
		dbc->curl_err = curl_easy_setopt(dbc->curl, CURLOPT_TIMEOUT, tout);
		if (dbc->curl_err != CURLE_OK) {
			ERRH(dbc, "libcurl: failed to set timeout=%ld.", tout);
			goto err;
		}
	}

	/* len of the body */
	dbc->curl_err = curl_easy_setopt(dbc->curl, CURLOPT_POSTFIELDSIZE_LARGE,
			u8body->cnt);
	if (dbc->curl_err != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to set post fieldsize: %zu.", u8body->cnt);
		goto err;
	}
	/* body itself */
	dbc->curl_err = curl_easy_setopt(dbc->curl, CURLOPT_POSTFIELDS,
			u8body->str);
	if (dbc->curl_err != CURLE_OK) {
		ERRH(dbc, "libcurl: failed to set post fields: `" LCPDL "`.",
			LCSTR(u8body));
		goto err;
	}

	return TRUE;

err:
	ERRH(dbc, "libcurl: failure code %d, message: %s.", dbc->curl_err,
		curl_easy_strerror(dbc->curl_err));
	return FALSE;
}

/* post cURL error message: include CURLOPT_ERRORBUFFER if available */
static void dbc_curl_post_diag(esodbc_dbc_st *dbc)
{
	SQLWCHAR buff[SQL_MAX_MESSAGE_LENGTH] = {1};
	SQLWCHAR *fmt;
	int n;

	assert(dbc->curl_err != CURLE_OK);

	/* in some cases ([::1]:0) this buffer will be empty, even though cURL
	 * returns an error code */
	if (dbc->curl_err_buff[0]) {
		fmt = WPFCP_DESC " (code:%d; " WPFCP_DESC ").";
	} else {
		fmt = WPFCP_DESC " (code:%d).";
	}

	n = swprintf(buff, sizeof(buff)/sizeof(*buff), fmt,
			curl_easy_strerror(dbc->curl_err), dbc->curl_err,
			/* this param is present even if there's no spec for it in fmt */
			dbc->curl_err_buff);
	/* if printing succeeded, OR failed, but buff is 0-term'd => OK */
	if (n < 0 && !buff[sizeof(buff)/sizeof(*buff) - 1]) {
		/* else: swprintf will fail if formatted string would overrun the
		 * available buffer room, but 0-terminate it; if that's the case.
		 * retry, skipping formatting. */
		ERRH(dbc, "formatting error message failed; skipping formatting.");
		post_c_diagnostic(&HDRH(dbc)->diag, SQL_STATE_08S01,
			curl_easy_strerror(dbc->curl_err), dbc->curl_err);
	} else {
		ERRH(dbc, "libcurl failure message: " LWPD ".", buff);
		post_diagnostic(&HDRH(dbc)->diag, SQL_STATE_08S01, buff,
			dbc->curl_err);
	}
}

/*
 * Sends a POST request with the given JSON object body.
 */
SQLRETURN post_json(esodbc_stmt_st *stmt, const cstr_st *u8body)
{
	SQLRETURN ret;
	CURLcode res = CURLE_OK;
	esodbc_dbc_st *dbc = stmt->hdr.dbc;
	SQLULEN tout;
	long code;
	cstr_st resp = (cstr_st) {
		NULL, 0
	};

	DBGH(stmt, "POSTing JSON [%zd] `" LCPDL "`.", u8body->cnt, LCSTR(u8body));

	ESODBC_MUX_LOCK(&dbc->curl_mux);

	if (! dbc->curl) {
		ret = dbc_curl_init(dbc);
		if (! SQL_SUCCEEDED(ret)) {
			stmt->hdr.diag = dbc->hdr.diag;
			return ret;
		}
	}

	/* set timeout as maximum between connection and statement value */
	tout = dbc->timeout < stmt->query_timeout ? stmt->query_timeout :
		dbc->timeout;

	code = -1; /* init value */
	if (dbc_curl_prepare(dbc, tout, u8body) &&
		dbc_curl_perform(dbc, &code, &resp)) {
		if (code == 200) {
			if (resp.cnt) {
				ESODBC_MUX_UNLOCK(&dbc->curl_mux);
				return attach_answer(stmt, resp.str, resp.cnt);
			} else {
				ERRH(stmt, "empty body received with 200 response code.");
			}
		}
	} else {
		dbc_curl_post_diag(dbc);
		code = -1; /* make sure that curl's error will surface */
	}
	/* something went wrong */
	cleanup_curl(dbc);
	ESODBC_MUX_UNLOCK(&dbc->curl_mux);

	/* was there an answer received correctly ? */
	if (0 < code) {
		attach_error(stmt, &resp, code);
	}

	/* an answer might have been received, but a late curl error (like
	 * fetching the result code) could have occurred. */
	if (resp.str) {
		free(resp.str);
		resp.str = NULL;
	}
	RET_STATE(stmt->hdr.diag.state);
}

static SQLRETURN test_connect(esodbc_dbc_st *dbc)
{
	long code;
	cstr_st u8body = MK_CSTR(HTTP_TEST_JSON);
	cstr_st resp = (cstr_st) {
		NULL, 0
	};

	if (! (dbc_curl_prepare(dbc, dbc->timeout, &u8body) &&
			dbc_curl_perform(dbc, &code, &resp))) {
		dbc_curl_post_diag(dbc);
		cleanup_curl(dbc);
		code = -1; /* make sure that curl's error will surface */
	}

	if (code == 200) {
		DBGH(dbc, "test connection succesful.");
		dbc->hdr.diag.state = SQL_STATE_00000;
	} else if (0 < code) {
		attach_error(dbc, &resp, code);
	} else {
		/* libcurl failure */
		assert(dbc->hdr.diag.state != SQL_STATE_00000);
	}

	if (resp.cnt) {
		free(resp.str);
		resp.str = NULL;
	}

	RET_STATE(dbc->hdr.diag.state);
}

/*
 * init dbc from configured attributes
 */
SQLRETURN config_dbc(esodbc_dbc_st *dbc, esodbc_dsn_attrs_st *attrs)
{
	SQLRETURN ret;
	int cnt, ipv6;
	SQLBIGINT secure;
	long long timeout, max_body_size, max_fetch_size;
	SQLWCHAR buff_url[ESODBC_MAX_URL_LEN];
	wstr_st url = (wstr_st) {
		buff_url, /*will be init'ed later*/0
	};

	if (! str2bigint(&attrs->secure, /*wide?*/TRUE, &secure)) {
		ERRH(dbc, "failed to read secure param `" LWPDL "`.",
			LWSTR(&attrs->secure));
		ret = SET_HDIAG(dbc, SQL_STATE_HY000, "security setting number "
				"conversion failure", 0);
		goto err;
	}
	if (secure < ESODBC_SEC_NONE || ESODBC_SEC_MAX <= secure) {
		ERRH(dbc, "invalid secure param `" LWPDL "` (not within %d - %d).",
			LWSTR(&attrs->secure), ESODBC_SEC_NONE, ESODBC_SEC_MAX - 1);
		ret = SET_HDIAG(dbc, SQL_STATE_HY000, "invalid security setting", 0);
		goto err;
	} else {
		dbc->secure = (long)secure;
		INFOH(dbc, "connection security level: %ld.", dbc->secure);
	}

	if (secure) {
		if (! wstr_to_utf8(&attrs->ca_path, &dbc->ca_path)) {
			ERRNH(dbc, "failed to convert CA path `" LWPDL "` to UTF8.",
				LWSTR(&attrs->ca_path));
			ret = SET_HDIAG(dbc, SQL_STATE_HY000, "reading the CA file path "
					"failed", 0);
			goto err;
		}
		INFOH(dbc, "CA path: `%s`.", dbc->ca_path.str);
	}

	/*
	 * URL of the cluster
	 */
	/* Note: libcurl won't check hostname validity, it'll just try to resolve
	 * whatever it receives, if it can parse the URL */
	ipv6 = wcsnstr(attrs->server.str, attrs->server.cnt, L':') != NULL;
	cnt = swprintf(url.str, sizeof(buff_url)/sizeof(*buff_url),
			L"http" WPFCP_DESC "://"
			WPFCP_DESC WPFWP_LDESC WPFCP_DESC ":" WPFWP_LDESC
			ELASTIC_SQL_PATH,
			secure ? "s" : "",
			ipv6 ? "[" : "", LWSTR(&attrs->server), ipv6 ? "]" : "",
			LWSTR(&attrs->port));
	if (cnt < 0) {
		ERRNH(dbc, "failed to print URL out of server: `" LWPDL "` [%zd], "
			"port: `" LWPDL "` [%zd].", LWSTR(&attrs->server),
			LWSTR(&attrs->port));
		ret = SET_HDIAG(dbc, SQL_STATE_HY000, "assembling server's URL failed",
				0);
		goto err;
	} else {
		url.cnt = (size_t)cnt;
	}
	if (! wstr_to_utf8(&url, &dbc->url)) {
		ERRNH(dbc, "failed to convert URL `" LWPDL "` to UTF8.", LWSTR(&url));
		ret = SET_HDIAG(dbc, SQL_STATE_HY000, "server URL's UTF8 conversion "
				"failed", 0);
		goto err;
	}
	INFOH(dbc, "connection URL: `%s`.", dbc->url.str);

	/*
	 * credentials
	 */
	if (attrs->uid.cnt) {
		if (! wstr_to_utf8(&attrs->uid, &dbc->uid)) {
			ERRH(dbc, "failed to convert username [%zd] `" LWPDL "` to UTF8.",
				attrs->uid.cnt, LWSTR(&attrs->uid));
			ret = SET_HDIAG(dbc, SQL_STATE_HY000, "username UTF8 conversion "
					"failed", 0);
			goto err;
		}
		if (attrs->pwd.cnt) {
			if (! wstr_to_utf8(&attrs->pwd, &dbc->pwd)) {
				ERRH(dbc, "failed to convert password [%zd] (-not shown-) to "
					"UTF8.", attrs->pwd.cnt);
				ret = SET_HDIAG(dbc, SQL_STATE_HY000, "password UTF8 "
						"conversion failed", 0);
				goto err;
			}
		}
	}

	/* "follow location" param for liburl */
	dbc->follow = wstr2bool(&attrs->follow);
	INFOH(dbc, "follow: %s.", dbc->follow ? "true" : "false");

	/*
	 * request timeout for liburl: negative reset to 0
	 */
	if (! str2bigint(&attrs->timeout, /*wide?*/TRUE, (SQLBIGINT *)&timeout)) {
		ERRH(dbc, "failed to convert `" LWPDL "` [%zu] to big int.",
			LWSTR(&attrs->timeout), attrs->timeout.cnt);
		ret = SET_HDIAG(dbc, SQL_STATE_HY000, "timeout setting number "
				"conversion failure", 0);
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
		ERRH(dbc, "failed to convert max body size `" LWPDL "` [%zu] to LL.",
			LWSTR(&attrs->max_body_size), attrs->max_body_size.cnt);
		ret = SET_HDIAG(dbc, SQL_STATE_HY000, "max body size setting number "
				"conversion failure", 0);
		goto err;
	}
	if (max_body_size < 0) {
		ERRH(dbc, "'%s' setting can't be negative (%ld).",
			ESODBC_DSN_MAX_BODY_SIZE_MB, max_body_size);
		ret = SET_HDIAG(dbc, SQL_STATE_HY000, "invalid max body size setting "
				"(negative)", 0);
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
		ERRH(dbc, "failed to convert max fetch size `" LWPDL "` [%zu] to LL.",
			LWSTR(&attrs->max_fetch_size), attrs->max_fetch_size.cnt);
		ret = SET_HDIAG(dbc, SQL_STATE_HY000, "max fetch size setting number "
				"conversion failure", 0);
		goto err;
	}
	if (max_fetch_size < 0) {
		ERRH(dbc, "'%s' setting can't be negative (%ld).",
			ESODBC_DSN_MAX_FETCH_SIZE, max_fetch_size);
		ret = SET_HDIAG(dbc, SQL_STATE_HY000, "invalid max fetch size setting "
				"(negative)", 0);
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
		ascii_w2c(attrs->max_fetch_size.str, dbc->fetch.str, dbc->fetch.slen);
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
		ret = SET_HDIAG(dbc, SQL_STATE_HY000, "invalid packing encoding "
				"setting", 0);
		goto err;
	}
	INFOH(dbc, "pack JSON: %s.", dbc->pack_json ? "true" : "false");

	return SQL_SUCCESS;
err:
	/* release allocated resources before the failure; not the diag, tho */
	cleanup_dbc(dbc);
	RET_STATE(dbc->hdr.diag.state);
}


/* release all resources, except the handler itself */
void cleanup_dbc(esodbc_dbc_st *dbc)
{
	if (dbc->ca_path.str) {
		free(dbc->ca_path.str);
		dbc->ca_path.str = NULL;
		dbc->ca_path.cnt = 0;
	} else {
		assert(dbc->ca_path.cnt == 0);
	}
	if (dbc->url.str) {
		free(dbc->url.str);
		dbc->url.str = NULL;
		dbc->url.cnt = 0;
	} else {
		assert(dbc->url.cnt == 0);
	}
	if (dbc->uid.str) {
		free(dbc->uid.str);
		dbc->uid.str = NULL;
		dbc->uid.cnt = 0;
	} else {
		assert(dbc->uid.cnt == 0);
	}
	if (dbc->pwd.str) {
		free(dbc->pwd.str);
		dbc->pwd.str = NULL;
		dbc->pwd.cnt = 0;
	} else {
		assert(dbc->pwd.cnt == 0);
	}
	if (dbc->fetch.str) {
		free(dbc->fetch.str);
		dbc->fetch.str = NULL;
		dbc->fetch.slen = 0;
	} else {
		assert(dbc->fetch.slen == 0);
	}
	if (dbc->dsn.str) {
		free(dbc->dsn.str);
		dbc->dsn.str = NULL;
		dbc->dsn.cnt = 0;
		dbc->server.str = NULL; /* both values collocated in same segment */
		dbc->server.cnt = 0;
	} else {
		assert(! dbc->dsn.cnt);
		assert(! dbc->server.cnt);
		assert(! dbc->server.str);
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

SQLRETURN do_connect(esodbc_dbc_st *dbc, esodbc_dsn_attrs_st *attrs)
{
	SQLRETURN ret;

	/* multiple connection attempts are possible (when prompting user) */
	cleanup_dbc(dbc);

	/* set the DBC params based on given attributes (but no validation atp) */
	ret = config_dbc(dbc, attrs);
	if (! SQL_SUCCEEDED(ret)) {
		return ret;
	}

	/* init libcurl objects */
	ret = dbc_curl_init(dbc);
	if (! SQL_SUCCEEDED(ret)) {
		ERRH(dbc, "failed to init transport.");
		return ret;
	}

	/* perform a connection test, to fail quickly if wrong params AND
	 * populate the DNS cache */
	ret = test_connect(dbc);
	if (! SQL_SUCCEEDED(ret)) {
		ERRH(dbc, "test connection to URL `%s` failed!", dbc->url.str);
	} else {
		DBGH(dbc, "test connection to URL %s OK.", dbc->url.str);
	}

	return ret;
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


		case SQL_BIT:
			es_type->display_size = /*'0/1'*/1;
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
		if ((c = ascii_w2c(types[i].type_name.str, types[i].type_name_c.str,
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
		DBG("ES type `" LWPDL "` resolved to C concise: %hd, SQL: %hd.",
			LWSTR(&types[i].type_name), types[i].c_concise_type, sql_type);

		/* BOOLEAN is used in catalog calls (like SYS TYPES / SQLGetTypeInfo),
		 * and the data type is piped through to the app (just like with any
		 * other statement), which causes issues, since it's a non-SQL type
		 * => change it to SQL_BIT */
		if (types[i].data_type == ESODBC_SQL_BOOLEAN) {
			types[i].data_type = ESODBC_ES_TO_SQL_BOOLEAN;
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

static int receive_dsn_cb(void *arg, const wchar_t *dsn_str,
	wchar_t *err_out, size_t eo_max, unsigned int flags)
{
	assert(arg);
	/* The function parses the DSN string into the received attrs, which
	 * remains assigned past the callback scope */
	return validate_dsn((esodbc_dsn_attrs_st *)arg, dsn_str, err_out, eo_max,
			/* Try connect here, to be able to report an error back to the
			 * user right away: otherwise, the connection will fail later in
			 * SQLDriverConnect loop, but with no indication as to why. */
			/*connect?*/TRUE);
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
	BOOL disable_nonconn = TRUE; /* disable non-conn controls by default */
	BOOL prompt_user = TRUE;
	long res;
	size_t cnt;

	init_dsn_attrs(&attrs);

	if (szConnStrIn) {
		DBGH(dbc, "Input connection string: [%hd] `" LWPDL "`.", cchConnStrIn,
			(0 <= cchConnStrIn) ? cchConnStrIn : SHRT_MAX, szConnStrIn);
		/* parse conn str into attrs */
		if (! parse_connection_string(&attrs, szConnStrIn, cchConnStrIn)) {
			ERRH(dbc, "failed to parse connection string `" LWPDL "`.",
				(0 <= cchConnStrIn) ? cchConnStrIn : SHRT_MAX, szConnStrIn);
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
			/* disable non-connection controls, as per the standard (even if
			 * changing the above default). */
			disable_nonconn = TRUE;
		/* no break */
		case SQL_DRIVER_COMPLETE:
			/* try connect first, then, if that fails, prompt user */
			prompt_user = FALSE;
		/* no break; */
		case SQL_DRIVER_PROMPT: /* prompt user first, then try connect */
			do {
				res = prompt_user ? prompt_user_config(hwnd, disable_nonconn,
						&attrs, &receive_dsn_cb) : /*need just > 0*/1;
				if (res < 0) {
					ERRH(dbc, "user GUI interaction failed.");
					RET_HDIAGS(dbc, SQL_STATE_IM008);
				} else if (! res) {
					/* user canceled */
					DBGH(dbc, "user canceled the GUI interaction.");
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

	/* save the original DSN and (new) server name for later inquiry by app */
	cnt = orig_dsn.cnt + /*\0*/1;
	cnt += attrs.server.cnt + /*\0*/1;
	dbc->dsn.str = malloc(cnt * sizeof(SQLWCHAR)); /* alloc for both */
	if (! dbc->dsn.str) {
		ERRNH(dbc, "OOM for %zdB.", (orig_dsn.cnt + 1) * sizeof(SQLWCHAR));
		RET_HDIAGS(dbc, SQL_STATE_HY001);
	}
	/* copy DSN */
	wcsncpy(dbc->dsn.str, orig_dsn.str, orig_dsn.cnt);
	dbc->dsn.str[orig_dsn.cnt] = L'\0';
	dbc->dsn.cnt = orig_dsn.cnt;
	/* copy server name */
	dbc->server.str = dbc->dsn.str + orig_dsn.cnt + /*\0*/1;
	wcsncpy(dbc->server.str, attrs.server.str, attrs.server.cnt);
	dbc->server.str[attrs.server.cnt] = L'\0';
	dbc->server.cnt = attrs.server.cnt;

	/* return the final connection string */
	if (szConnStrOut || pcchConnStrOut) {
		/* might have been reset to DEFAULT, if orig was not found */
		res = assign_dsn_attr(&attrs, &MK_WSTR(ESODBC_DSN_DSN), &orig_dsn,
				/*overwrite?*/TRUE);
		assert(0 < res);
		res = write_connection_string(&attrs, szConnStrOut, cchConnStrOutMax);
		if (res < 0) {
			ERRH(dbc, "failed to build output connection string.");
			RET_HDIAG(dbc, SQL_STATE_HY000, "failed to build connection "
				"string", 0);
		} else if (pcchConnStrOut) {
			*pcchConnStrOut = (SQLSMALLINT)res;
		}
	}

	return SQL_SUCCESS;
}

SQLRETURN EsSQLConnectW
(
	SQLHDBC             hdbc,
	_In_reads_(cchDSN) SQLWCHAR *szDSN,
	SQLSMALLINT         cchDSN,
	_In_reads_(cchUID) SQLWCHAR *szUID,
	SQLSMALLINT         cchUID,
	_In_reads_(cchAuthStr) SQLWCHAR *szPWD,
	SQLSMALLINT         cchPWD
)
{
	esodbc_dsn_attrs_st attrs;
	SQLWCHAR buff[sizeof(attrs.buff)/sizeof(*attrs.buff)];
	size_t i;
	int res;
	SQLSMALLINT written;
	esodbc_dbc_st *dbc = DBCH(hdbc);

	wstr_st kw_dsn = MK_WSTR(ESODBC_DSN_DSN);
	wstr_st val_dsn = (wstr_st) {
		szDSN, cchDSN < 0 ? wcslen(szDSN) : cchDSN
	};
	wstr_st kw_uid = MK_WSTR(ESODBC_DSN_UID);
	wstr_st val_uid = (wstr_st) {
		szUID, cchUID < 0 ? wcslen(szUID) : cchUID
	};
	wstr_st kw_pwd = MK_WSTR(ESODBC_DSN_PWD);
	wstr_st val_pwd = (wstr_st) {
		szPWD, cchPWD < 0 ? wcslen(szPWD) : cchPWD
	};
	wstr_st *kws[] = {&kw_dsn, &kw_uid, &kw_pwd};
	wstr_st *vals[] = {&val_dsn, &val_uid, &val_pwd};

	init_dsn_attrs(&attrs);

	for (i = 0; i < sizeof(kws)/sizeof(*kws); i ++) {
		res = assign_dsn_attr(&attrs, kws[i], vals[i], /*overwrite*/FALSE);
		if (res < 0) {
			ERRH(dbc, "couldn't assign " LWPDL " value [%zu] "
				"`" LWPDL "`.", LWSTR(kws[i]),
				vals[i]->cnt, LWSTR(vals[i]));
		} else {
			assert(res == DSN_ASSIGNED);
		}
	}

	if ((written = (SQLSMALLINT)write_connection_string(&attrs, buff,
					sizeof(buff)/sizeof(*buff))) < 0) {
		ERRH(dbc, "failed to serialize params as connection string.");
		RET_HDIAG(dbc, SQL_STATE_HY000, "failed to serialize connection "
			"parameters", 0);
	}

	return EsSQLDriverConnectW(dbc, /*win hndlr*/NULL, buff, written,
			/*conn str out*/NULL, /*...len*/0, /* out written */NULL,
			SQL_DRIVER_NOPROMPT);
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
			DBGH(dbc, "attempt to set transaction isolation to: %u.",
				(SQLUINTEGER)(uintptr_t)Value);
			WARNH(dbc, "no support for transactions available.");
			/* the driver advertises the data source as read-only, so no
			 * transaction level setting should occur. If an app seems to rely
			 * on it, we need to switch from ignoring the action to rejecting
			 * it: */
			//RET_HDIAGS(dbc, SQL_STATE_HYC00);
			break;

		default:
			ERRH(dbc, "unknown Attribute: %d.", Attribute);
			RET_HDIAGS(dbc, SQL_STATE_HY092);
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
			DBGH(dbc, "requested: transaction isolation (0).");
			ERRH(dbc, "no support for transactions available.");
			*(SQLUINTEGER *)ValuePtr = 0;
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
