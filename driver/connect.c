/*
 * ELASTICSEARCH CONFIDENTIAL
 * __________________
 *
 *  [2018] Elasticsearch Incorporated. All Rights Reserved.
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
#include <limits.h>
#include <inttypes.h>

#include "connect.h"
#include "queries.h"
#include "log.h"
#include "info.h"
#include "util.h"

/* HTTP headers default for every request */
#define HTTP_ACCEPT_JSON		"Accept: application/json"
#define HTTP_CONTENT_TYPE_JSON	"Content-Type: application/json; charset=utf-8"

/* JSON body build elements */
#define JSON_SQL_QUERY_START		"{\"query\":\""
#define JSON_SQL_QUERY_MID			"\""
#define JSON_SQL_QUERY_MID_FETCH	JSON_SQL_QUERY_MID ",\"fetch_size\":"
#define JSON_SQL_QUERY_END			"}"
#define JSON_SQL_CURSOR_START		"{\"cursor\":\""
#define JSON_SQL_CURSOR_END			"\"}"

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


/* attribute keywords used in connection strings */
#define CONNSTR_KW_DRIVER			"Driver"
#define CONNSTR_KW_DSN				"DSN"
#define CONNSTR_KW_PWD				"PWD"
#define CONNSTR_KW_UID				"UID"
#define CONNSTR_KW_SAVEFILE			"SAVEFILE"
#define CONNSTR_KW_FILEDSN			"FILEDSN"
#define CONNSTR_KW_SERVER			"Server"
#define CONNSTR_KW_PORT				"Port"
#define CONNSTR_KW_SECURE			"Secure"
#define CONNSTR_KW_TIMEOUT			"Timeout"
#define CONNSTR_KW_FOLLOW			"Follow"
#define CONNSTR_KW_CATALOG			"Catalog"
#define CONNSTR_KW_PACKING			"Packing"
#define CONNSTR_KW_MAX_FETCH_SIZE	"MaxFetchSize"
#define CONNSTR_KW_MAX_BODY_SIZE_MB	"MaxBodySizeMB"

#define ODBC_REG_SUBKEY_PATH	"SOFTWARE\\ODBC\\ODBC.INI"
#define REG_HKLM				"HKEY_LOCAL_MACHINE"
#define REG_HKCU				"HKEY_CURRENT_USER"

/* max length of a registry key value name */
#define MAX_REG_VAL_NAME		1024
/* max size of a registry key data */
#define MAX_REG_DATA_SIZE		4096


/* stucture to collect all attributes in a connection string */
typedef struct {
	wstr_st driver;
	wstr_st dsn;
	wstr_st pwd;
	wstr_st uid;
	wstr_st savefile;
	wstr_st filedsn;
	wstr_st server;
	wstr_st port;
	wstr_st secure;
	wstr_st timeout;
	wstr_st follow;
	wstr_st catalog;
	wstr_st packing;
	wstr_st max_fetch_size;
	wstr_st max_body_size;
	wstr_st trace_file;
	wstr_st trace_level;
} config_attrs_st;

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
	if (http_headers)
		http_headers = curl_slist_append(http_headers, HTTP_CONTENT_TYPE_JSON);
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
	if (curl)
		curl_easy_cleanup(curl);
	RET_STATE(dbc->hdr.diag.state);
}

static void cleanup_curl(esodbc_dbc_st *dbc)
{
	if (! dbc->curl)
		return;
	DBGH(dbc, "libcurl: handle 0x%p cleanup.", dbc->curl);
	curl_easy_cleanup(dbc->curl); /* TODO: _reset() rather? */
	dbc->curl = NULL;
}

/*
 * Sends a POST request given the body
 */
static SQLRETURN post_request(esodbc_stmt_st *stmt,
		long timeout, /* req timeout; set to negative for default */
		const char *u8body, /* stringified JSON object, UTF-8 encoded */
		size_t blen /* size of the u8body buffer */)
{
	// char *const answer, /* buffer to receive the answer in */
	// long avail /*size of the answer buffer */)
	CURLcode res = CURLE_OK;
	esodbc_dbc_st *dbc = stmt->hdr.dbc;
	char *abuff = NULL;
	size_t apos;
	long to, code;

	DBGH(stmt, "posting request `%.*s` (%zd).", blen, u8body, blen);

	if (! dbc->curl)
		init_curl(dbc);

	/* set timeout as: statment, with highest prio, dbc next. */
	if (0 <= timeout)
		to = timeout;
	else if (0 <= dbc->timeout)
		to = dbc->timeout;
	else
		to = -1;


	/* set total timeout for request */
	if (0 <= to) {
		res = curl_easy_setopt(dbc->curl, CURLOPT_TIMEOUT, to);
		if (res != CURLE_OK) {
			goto err;
		} else {
			DBGH(stmt, "libcurl: set curl 0x%p timeout to %ld.", dbc->curl, to);
		}
	}

	/* set body to send */
	if (u8body) {
		/* len of the body */
		res = curl_easy_setopt(dbc->curl, CURLOPT_POSTFIELDSIZE, blen);
		if (res != CURLE_OK) {
			goto err;
		} else {
			DBGH(stmt, "libcurl: set curl 0x%p post fieldsize to %ld.",
					dbc->curl, blen);
		}
		/* body itself */
		res = curl_easy_setopt(dbc->curl, CURLOPT_POSTFIELDS, u8body);
		if (res != CURLE_OK) {
			goto err;
		} else {
			DBGH(stmt, "libcurl: set curl 0x%p post fields to `%.*s`.",
					dbc->curl, blen, u8body);
		}
	} else {
		assert(blen == 0);
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

	if (res != CURLE_OK)
		goto err;
	res = curl_easy_getinfo(dbc->curl, CURLINFO_RESPONSE_CODE, &code);
	if (res != CURLE_OK)
		goto err;
	DBGH(stmt, "libcurl: request succesfull, received code %ld and %zd bytes"
			" back.", code, apos);

	if (code != 200) {
		ERRH(stmt, "libcurl: non-200 HTTP response code %ld received.", code);
		/* expect a 200 with body; everything else is failure (todo?)  */
		if (400 <= code)
			return attach_error(stmt, abuff, apos);
		goto err_net;
	}

	return attach_answer(stmt, abuff, apos);

err:
	ERRH(stmt, "libcurl: request failed (timeout:%ld, body:`%.*s`) "
			"failed: '%s' (%d).", timeout, blen, u8body,
			res != CURLE_OK ? curl_easy_strerror(res) : "<unspecified>", res);
err_net: /* the error occured after the request hit hit the network */
	cleanup_curl(dbc);
	if (abuff) {
		free(abuff);
		abuff = NULL;
		/* if buffer had been set, the error occured in _perform() */
		RET_HDIAG(stmt, SQL_STATE_08S01, "data transfer failure", res);
	}
	RET_HDIAG(stmt, SQL_STATE_HY000, "failed to init transport", res);
}

/*
 * Build a JSON query or cursor object (if statment having one) and send it as
 * the body of a REST POST requrest.
 */
SQLRETURN post_statement(esodbc_stmt_st *stmt)
{
	SQLRETURN ret;
	esodbc_dbc_st *dbc = stmt->hdr.dbc;
	size_t bodylen, pos, u8len;
	char *body, buff[ESODBC_BODY_BUF_START_SIZE];
	char u8curs[ESODBC_BODY_BUF_START_SIZE];

	// TODO: add params (if any)

	/* TODO: move escaping/x-coding (to JSON or CBOR) in attach_sql() and/or
	 * attach_answer() to avoid these operations for each execution of the
	 * statement (especially for the SQL statement; the cursor might not
	 * always be used - if app decides to no longer fetch - but would then
	 * clean this function). */

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

		bodylen = sizeof(JSON_SQL_CURSOR_START) - /*\0*/1;
		bodylen += json_escape(u8curs, u8len, NULL, 0);
		bodylen += sizeof(JSON_SQL_CURSOR_END) - /*\0*/1;
	} else { /* eval QUERY object length */
		bodylen = sizeof(JSON_SQL_QUERY_START) - /*\0*/1;
		if (dbc->fetch.slen) {
			bodylen += sizeof(JSON_SQL_QUERY_MID_FETCH) - /*\0*/1;
			bodylen += dbc->fetch.slen;
		} else {
			bodylen += sizeof(JSON_SQL_QUERY_MID) - /*\0*/1;
		}
		bodylen += json_escape(stmt->u8sql, stmt->sqllen, NULL, 0);
		bodylen += sizeof(JSON_SQL_QUERY_END) - /*\0*/1;
	}

	/* allocate memory for the stringified buffer, if needed */
	if (sizeof(buff) < bodylen) {
		WARNH(dbc, "local buffer too small (%zd), need %zdB; will alloc.",
				sizeof(buff), bodylen);
		WARNH(dbc, "local buffer too small, SQL: `%.*s`.", stmt->sqllen,
				stmt->u8sql);
		body = malloc(bodylen);
		if (! body) {
			ERRNH(stmt, "failed to alloc %zdB.", bodylen);
			RET_HDIAGS(stmt, SQL_STATE_HY001);
		}
	} else {
		body = buff;
	}

	/* build the actual stringified JSON object */
	if (stmt->rset.eccnt) { /* copy CURSOR object */
		pos = sizeof(JSON_SQL_CURSOR_START) - /*\0*/1;
		memcpy(body, JSON_SQL_CURSOR_START, pos);
		pos += json_escape(u8curs, u8len, body + pos, bodylen - pos);
		memcpy(body + pos, JSON_SQL_CURSOR_END,
				sizeof(JSON_SQL_CURSOR_END) - /*WITH \0*/0);
		pos += sizeof(JSON_SQL_CURSOR_END) - /* but don't account for it */1;
	} else { /* copy QUERY object */
		pos = sizeof(JSON_SQL_QUERY_START) - 1;
		memcpy(body, JSON_SQL_QUERY_START, pos);
		pos += json_escape(stmt->u8sql, stmt->sqllen, body + pos, bodylen-pos);

		if (dbc->fetch.slen) {
			memcpy(body + pos, JSON_SQL_QUERY_MID_FETCH,
					sizeof(JSON_SQL_QUERY_MID_FETCH) - 1);
			pos += sizeof(JSON_SQL_QUERY_MID_FETCH) - 1;
			memcpy(body + pos, dbc->fetch.str, dbc->fetch.slen);
			pos += dbc->fetch.slen;
		} else {
			memcpy(body + pos, JSON_SQL_QUERY_MID,
					sizeof(JSON_SQL_QUERY_MID) - 1);
			pos += sizeof(JSON_SQL_QUERY_MID) - 1;
		}

		memcpy(body + pos, JSON_SQL_QUERY_END,
				sizeof(JSON_SQL_QUERY_END) - /*WITH \0*/0);
		pos += sizeof(JSON_SQL_QUERY_END) - /* but don't account for it */1;
	}

	ret = post_request(stmt, dbc->timeout, body, pos);

	if (body != buff)
		free(body); /* hehe */

	return ret;
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

static BOOL assign_config_attr(config_attrs_st *attrs,
		wstr_st *keyword, wstr_st *value, BOOL overwrite)
{
	struct {
		wstr_st *kw;
		wstr_st *val;
	} *iter, map[] = {
		{&MK_WSTR(CONNSTR_KW_DRIVER), &attrs->driver},
		{&MK_WSTR(CONNSTR_KW_DSN), &attrs->dsn},
		{&MK_WSTR(CONNSTR_KW_PWD), &attrs->pwd},
		{&MK_WSTR(CONNSTR_KW_UID), &attrs->uid},
		{&MK_WSTR(CONNSTR_KW_SAVEFILE), &attrs->savefile},
		{&MK_WSTR(CONNSTR_KW_FILEDSN), &attrs->filedsn},
		{&MK_WSTR(CONNSTR_KW_SERVER), &attrs->server},
		{&MK_WSTR(CONNSTR_KW_PORT), &attrs->port},
		{&MK_WSTR(CONNSTR_KW_SECURE), &attrs->secure},
		{&MK_WSTR(CONNSTR_KW_TIMEOUT), &attrs->timeout},
		{&MK_WSTR(CONNSTR_KW_FOLLOW), &attrs->follow},
		{&MK_WSTR(CONNSTR_KW_CATALOG), &attrs->catalog},
		{&MK_WSTR(CONNSTR_KW_PACKING), &attrs->packing},
		{&MK_WSTR(CONNSTR_KW_MAX_FETCH_SIZE), &attrs->max_fetch_size},
		{&MK_WSTR(CONNSTR_KW_MAX_BODY_SIZE_MB), &attrs->max_body_size},
		{NULL, NULL}
	};

	for (iter = &map[0]; iter->kw; iter ++) {
		if (! EQ_CASE_WSTR(iter->kw, keyword))
			continue;
		/* it's a match: has it been assigned already? */
		if (iter->val->cnt) {
			if (! overwrite) {
				INFO("multiple occurances of keyword '" LWPDL "'; "
						"ignoring new `" LWPDL "`, keeping `" LWPDL "`.",
						LWSTR(iter->kw), LWSTR(value), LWSTR(iter->val));
				continue;
			}
			INFO("multiple occurances of keyword '" LWPDL "'; "
					"overwriting old `" LWPDL "` with new `" LWPDL "`.",
					LWSTR(iter->kw), LWSTR(iter->val), LWSTR(value));
		}
		*iter->val = *value;
		return TRUE;
	}

	return FALSE;
}

/*
 * Advance position in string, skipping white space.
 * if exended is true, `;` will be treated as white space too.
 */
static SQLWCHAR* skip_ws(SQLWCHAR **pos, SQLWCHAR *end, BOOL extended)
{
	while (*pos < end) {
		switch(**pos) {
			case ' ':
			case '\t':
			case '\r':
			case '\n':
				(*pos)++;
				break;

			case '\0':
				return NULL;

			case ';':
				if (extended) {
					(*pos)++;
					break;
				}
				// no break;

			default:
				return *pos;
		}
	}

	/* end of string reached */
	return NULL;
}

/*
 * Parse a keyword or a value.
 * Within braces, any character is allowed, safe for \0 (i.e. no "bla{bla\0"
 * is supported as keyword or value).
 * Braces within braces are allowed.
 */
static BOOL parse_token(BOOL is_value, SQLWCHAR **pos, SQLWCHAR *end,
		wstr_st *token)
{
	BOOL brace_escaped = FALSE;
	int open_braces = 0;
	SQLWCHAR *start = *pos;
	BOOL stop = FALSE;

	while (*pos < end && (! stop)) {
		switch (**pos) {
			case '\\':
				if (! is_value) {
					ERR("keywords and data source names cannot contain "
							"the backslash.");
					return FALSE;
				}
				(*pos)++;
				break;

			case ' ':
			case '\t':
			case '\r':
			case '\n':
				if (open_braces || is_value)
					(*pos)++;
				else
					stop = TRUE;
				break;

			case '=':
				if (open_braces || is_value)
					(*pos)++;
				else
					stop = TRUE;
				break;

			case ';':
				if (open_braces) {
					(*pos)++;
				} else if (is_value) {
					stop  = TRUE;
				} else {
					ERR("';' found while parsing keyword");
					return FALSE;
				}
				break;

			case '\0':
				if (open_braces) {
					ERR("null terminator found while within braces");
					return FALSE;
				} else if (! is_value) {
					ERR("null terminator found while parsing keyword.");
					return FALSE;
				} /* else: \0 used as delimiter of value string */
				stop = TRUE;
				break;

			case '{':
				if (*pos == start)
					open_braces ++;
				else if (open_braces) {
					ERR("token started with opening brace, so can't use "
							"inner braces");
					/* b/c: `{foo{ = }}bar} = val`; `foo{bar}baz` is fine */
					return FALSE;
				}
				(*pos)++;
				break;

			case '}':
				if (open_braces) {
					open_braces --;
					brace_escaped = TRUE;
					stop = TRUE;
				}
				(*pos)++;
				break;

			default:
				(*pos)++;
		}
	}

	if (open_braces) {
		ERR("string finished with open braces.");
		return FALSE;
	}

	token->str = start + (brace_escaped ? 1 : 0);
	token->cnt = (*pos - start) - (brace_escaped ? 2 : 0);
	return TRUE;
}

static SQLWCHAR* parse_separator(SQLWCHAR **pos, SQLWCHAR *end)
{
	if (*pos < end) {
		if (**pos == '=') {
			(*pos)++;
			return *pos;
		}
	}
	return NULL;
}

/*
 * - "keywords and attribute values that contain the characters []{}(),;?*=!@
 *   not enclosed with braces should be avoided"; => allowed.
 * - "value of the DSN keyword cannot consist only of blanks and should not
 *   contain leading blanks";
 * - "keywords and data source names cannot contain the backslash (\)
 *   character.";
 * - "value enclosed with braces ({}) containing any of the characters
 *   []{}(),;?*=!@ is passed intact to the driver.";
 *
 *  foo{bar}=baz=foo; => "foo{bar}" = "baz=foo"
 *
 *  * `=` is delimiter, unless within {}
 *  * `{` and `}` allowed within {}
 *  * brances need to be returned to out-str;
 */
static BOOL parse_connection_string(config_attrs_st *attrs,
		SQLWCHAR* szConnStrIn, SQLSMALLINT cchConnStrIn)
{

	SQLWCHAR *pos;
	SQLWCHAR *end;
	wstr_st keyword, value;

	/* parse and assign attributes in connection string */
	pos = szConnStrIn;
	end = pos + (cchConnStrIn == SQL_NTS ? SHRT_MAX : cchConnStrIn);

	while (skip_ws(&pos, end, TRUE)) {
		if (! parse_token(FALSE, &pos, end, &keyword)) {
			ERR("failed to parse keyword at position %zd",
					pos - szConnStrIn);
			return FALSE;
		}

		if (! skip_ws(&pos, end, FALSE))
			return FALSE;

		if (! parse_separator(&pos, end)) {
			ERR("failed to parse separator (`=`) at position %zd",
					pos - szConnStrIn);
			return FALSE;
		}

		if (! skip_ws(&pos, end, FALSE))
			return FALSE;

		if (! parse_token(TRUE, &pos, end, &value)) {
			ERR("failed to parse value at position %zd",
					pos - szConnStrIn);
			return FALSE;
		}

		DBG("read connection string attribute: `" LWPDL "` = `" LWPDL
				"`.",  LWSTR(&keyword), LWSTR(&value));
		if (! assign_config_attr(attrs, &keyword, &value, TRUE))
			WARN("keyword '" LWPDL "' is unknown, ignoring it.",
					LWSTR(&keyword));
	}

	return TRUE;
}

static inline BOOL needs_braces(wstr_st *token)
{
	int i;

	for (i = 0; i < token->cnt; i ++) {
		switch(token->str[i]) {
			case ' ':
			case '\t':
			case '\r':
			case '\n':
			case '=':
			case ';':
				return TRUE;
		}
	}
	return FALSE;
}

/* build a connection string to be written in the DSN files */
static BOOL write_connection_string(config_attrs_st *attrs,
		SQLWCHAR *szConnStrOut, SQLSMALLINT cchConnStrOutMax,
		SQLSMALLINT *pcchConnStrOut)
{
	int n, braces;
	size_t pos;
	wchar_t *format;
	struct {
		wstr_st *val;
		char *kw;
	} *iter, map[] = {
		{&attrs->driver, CONNSTR_KW_DRIVER},
		{&attrs->dsn, CONNSTR_KW_DSN},
		{&attrs->pwd, CONNSTR_KW_PWD},
		{&attrs->uid, CONNSTR_KW_UID},
		{&attrs->savefile, CONNSTR_KW_SAVEFILE},
		{&attrs->filedsn, CONNSTR_KW_FILEDSN},
		{&attrs->server, CONNSTR_KW_SERVER},
		{&attrs->port, CONNSTR_KW_PORT},
		{&attrs->secure, CONNSTR_KW_SECURE},
		{&attrs->timeout, CONNSTR_KW_TIMEOUT},
		{&attrs->follow, CONNSTR_KW_FOLLOW},
		{&attrs->catalog, CONNSTR_KW_CATALOG},
		{&attrs->packing, CONNSTR_KW_PACKING},
		{&attrs->max_fetch_size, CONNSTR_KW_MAX_FETCH_SIZE},
		{&attrs->max_body_size, CONNSTR_KW_MAX_BODY_SIZE_MB},
		{NULL, NULL}
	};

	for (iter = &map[0], pos = 0; iter->val; iter ++) {
		if (iter->val->cnt) {
			braces = needs_braces(iter->val) ? 2 : 0;
			if (cchConnStrOutMax && szConnStrOut) {
				/* swprintf will fail if formated string would overrun the
				 * buffer size */
				if (cchConnStrOutMax - pos < iter->val->cnt + braces) {
					/* indicate that we've reached buffer limits: only account
					 * for how long the string would be */
					cchConnStrOutMax = 0;
					pos += iter->val->cnt + braces;
					continue;
				}
				if (braces)
					format = WPFCP_DESC "={" WPFWP_LDESC "};";
				else
					format = WPFCP_DESC "=" WPFWP_LDESC ";";
				n = swprintf(szConnStrOut + pos, cchConnStrOutMax - pos,
						format, iter->kw, LWSTR(iter->val));
				if (n < 0) {
					ERRN("failed to outprint connection string (space "
							"left: %d; needed: %d).", cchConnStrOutMax - pos,
							iter->val->cnt);
					return FALSE;
				} else {
					pos += n;
				}
			} else {
				/* simply increment the counter, since the untruncated length
				 * need to be returned to the app */
				pos += iter->val->cnt + braces;
			}
		}
	}

	*pcchConnStrOut = (SQLSMALLINT)pos;

	DBG("Output connection string: `" LWPD "`; out len: %d.",
			szConnStrOut, pos);
	return TRUE;
}

/*
 * init dbc from configured attributes
 */
static SQLRETURN process_config(esodbc_dbc_st *dbc, config_attrs_st *attrs)
{
	esodbc_state_et state = SQL_STATE_HY000;
	int n, cnt;
	SQLWCHAR urlw[ESODBC_MAX_URL_LEN];
	BOOL secure;
	long timeout, max_body_size, max_fetch_size;

	/*
	 * build connection URL
	 */
	secure = wstr2bool(&attrs->secure);
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
	if (! wstr2long(&attrs->timeout, &timeout)) {
		ERRH(dbc, "failed to convert '" LWPDL "' to long.",
				LWSTR(&attrs->timeout));
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
	if (! wstr2long(&attrs->max_body_size, &max_body_size)) {
		ERRH(dbc, "failed to convert '" LWPDL "' to long.",
				LWSTR(&attrs->max_body_size));
		goto err;
	}
	if (max_body_size < 0) {
		ERRH(dbc, "'%s' setting can't be negative (%ld).",
				CONNSTR_KW_MAX_BODY_SIZE_MB, max_body_size);
		goto err;
	} else {
		dbc->amax = max_body_size * 1024 * 1024;
	}
	INFOH(dbc, "max body size: %zd.", dbc->amax);

	/*
	 * set max fetch size
	 */
	if (! wstr2long(&attrs->max_fetch_size, &max_fetch_size)) {
		ERRH(dbc, "failed to convert '" LWPDL "' to long.",
				LWSTR(&attrs->max_fetch_size));
		goto err;
	}
	if (max_fetch_size < 0) {
		ERRH(dbc, "'%s' setting can't be negative (%ld).",
				CONNSTR_KW_MAX_FETCH_SIZE, max_fetch_size);
		goto err;
	} else {
		dbc->fetch.max = max_fetch_size;
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
	if (state == SQL_STATE_HY000)
		RET_HDIAG(dbc, state, "invalid configuration parameter", 0);
	RET_HDIAGS(dbc, state);
}

static inline void assign_defaults(config_attrs_st *attrs)
{
	/* assign defaults where not assigned and applicable */
	if (! attrs->server.cnt)
		attrs->server = MK_WSTR(ESODBC_DEF_SERVER);
	if (! attrs->port.cnt)
		attrs->port = MK_WSTR(ESODBC_DEF_PORT);
	if (! attrs->secure.cnt)
		attrs->secure = MK_WSTR(ESODBC_DEF_SECURE);
	if (! attrs->timeout.cnt)
		attrs->timeout = MK_WSTR(ESODBC_DEF_TIMEOUT);
	if (! attrs->follow.cnt)
		attrs->follow = MK_WSTR(ESODBC_DEF_FOLLOW);

	/* no default packing */

	if (! attrs->packing.cnt)
		attrs->packing = MK_WSTR(ESODBC_DEF_PACKING);
	if (! attrs->max_fetch_size.cnt)
		attrs->max_fetch_size = MK_WSTR(ESODBC_DEF_FETCH_SIZE);
	if (! attrs->max_body_size.cnt)
		attrs->max_body_size = MK_WSTR(ESODBC_DEF_MAX_BODY_SIZE_MB);

	/* default: no trace file */

	if (! attrs->trace_level.cnt)
		attrs->trace_level = MK_WSTR(ESODBC_DEF_TRACE_LEVEL);
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
	assert(dbc->abuff == NULL); /* reminder for when going multithreaded */
	cleanup_curl(dbc);
}

static SQLRETURN do_connect(esodbc_dbc_st *dbc, config_attrs_st *attrs)
{
	SQLRETURN ret;
	char *url = NULL;

	/* multiple connection attempts are possible (when prompting user) */
	cleanup_dbc(dbc);

	ret = process_config(dbc, attrs);
	if (! SQL_SUCCEEDED(ret))
		return ret;

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


/* Maps ES/SQL type to C SQL. */
SQLSMALLINT type_elastic2csql(wstr_st *type_name)
{
	switch (type_name->cnt) {
		/* 4: BYTE, LONG, TEXT, DATE, NULL */
		case sizeof(JSON_COL_BYTE) - 1:
			switch (tolower(type_name->str[0])) {
				case (SQLWCHAR)'b':
					if (! wmemncasecmp(type_name->str,
								MK_WPTR(JSON_COL_BYTE), type_name->cnt)) {
						return ESODBC_ES_TO_CSQL_BYTE;
					}
					break;
				case (SQLWCHAR)'l':
					if (! wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_LONG),
								type_name->cnt)) {
						return ESODBC_ES_TO_CSQL_LONG;
						}
					break;
				case (SQLWCHAR)'t':
					if (! wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_TEXT),
								type_name->cnt)) {
						return ESODBC_ES_TO_CSQL_TEXT;
					}
					break;
				case (SQLWCHAR)'d':
					if (! wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_DATE),
								type_name->cnt)) {
						return ESODBC_ES_TO_CSQL_DATE;
					}
					break;
				case (SQLWCHAR)'n':
					if (! wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_NULL),
								type_name->cnt)) {
						return ESODBC_ES_TO_CSQL_NULL;
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
						return ESODBC_ES_TO_CSQL_SHORT;
					}
					break;
				case (SQLWCHAR)'f':
					if (! wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_FLOAT),
								type_name->cnt)) {
						return ESODBC_ES_TO_CSQL_FLOAT;
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
						return ESODBC_ES_TO_CSQL_DOUBLE;
					}
					break;
				case (SQLWCHAR)'b':
					if (! wmemncasecmp(type_name->str,
								MK_WPTR(JSON_COL_BINARY), type_name->cnt)) {
						return ESODBC_ES_TO_CSQL_BINARY;
					}
					break;
				case (SQLWCHAR)'o':
					if (! wmemncasecmp(type_name->str,
								MK_WPTR(JSON_COL_OBJECT), type_name->cnt)) {
						return ESODBC_ES_TO_CSQL_OBJECT;
					}
					break;
				case (SQLWCHAR)'n':
					if (! wmemncasecmp(type_name->str,
								MK_WPTR(JSON_COL_NESTED), type_name->cnt)) {
						return ESODBC_ES_TO_CSQL_NESTED;
					}
					break;
			}
			break;

		/* 7: INTEGER, BOOLEAN, KEYWORD */
		case sizeof(JSON_COL_INTEGER) - 1:
			switch (tolower(type_name->str[0])) {
				case (SQLWCHAR)'i': /* integer */
					if (wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_INTEGER),
								type_name->cnt) == 0)
						return ESODBC_ES_TO_CSQL_INTEGER;
					break;
				case (SQLWCHAR)'b': /* boolean */
					if (wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_BOOLEAN),
								type_name->cnt) == 0)
						return ESODBC_ES_TO_CSQL_BOOLEAN;
					break;
				case (SQLWCHAR)'k': /* keyword */
					if (wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_KEYWORD),
								type_name->cnt) == 0)
						return ESODBC_ES_TO_CSQL_KEYWORD;
					break;
			}
			break;

		/* 10: HALF_FLOAT */
		case sizeof(JSON_COL_HALF_FLOAT) - 1:
			if (! wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_HALF_FLOAT),
						type_name->cnt)) {
				return ESODBC_ES_TO_CSQL_HALF_FLOAT;
			}
			break;

		/* 11: UNSUPPORTED */
		case sizeof(JSON_COL_UNSUPPORTED) - 1:
			if (! wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_UNSUPPORTED),
						type_name->cnt)) {
				return ESODBC_ES_TO_CSQL_UNSUPPORTED;
			}
			break;

		/* 12: SCALED_FLOAT */
		case sizeof(JSON_COL_SCALED_FLOAT) - 1:
			if (! wmemncasecmp(type_name->str, MK_WPTR(JSON_COL_SCALED_FLOAT),
						type_name->cnt)) {
				return ESODBC_ES_TO_CSQL_SCALED_FLOAT;
			}
			break;

	}
	ERR("unrecognized Elastic type `" LWPDL "` (%zd).", LWSTR(type_name),
			type_name->cnt);
	return SQL_UNKNOWN_TYPE;
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
			if (! es_type->unsigned_attribute)
				es_type->display_size ++;
			break;
		case SQL_BIGINT: /* LONG */
			es_type->display_size = es_type->column_size;
			if (es_type->unsigned_attribute)
				es_type->display_size ++;
			break;

		case SQL_BINARY:
		case SQL_VARBINARY: /* BINARY */
		case SQL_LONGVARBINARY:
			/* 0xAB */
			es_type->display_size = 2 * es_type->column_size;
			break;

		case SQL_TYPE_DATE:
		case SQL_TYPE_TIME:
		case SQL_TYPE_TIMESTAMP: /* DATE */
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
	/* structure for one row returned by the ES.
	 * This is a mirror of elasticsearch_type, with length-or-indicator fields
	 * for each of the members in elasticsearch_type */
	struct {
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
	} type_row[ESODBC_MAX_ROW_ARRAY_SIZE];
	/* both arrays must use ESODBC_MAX_ROW_ARRAY_SIZE since no SQLFetch()
	 * looping is implemented (see check after SQLFetch() below). */
	SQLUSMALLINT row_status[ESODBC_MAX_ROW_ARRAY_SIZE];
	SQLULEN rows_fetched, i, strs_len;
	size_t size;
	SQLWCHAR *pos;
	esodbc_estype_st *types = NULL;

	if (! SQL_SUCCEEDED(EsSQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) {
		ERRH(dbc, "failed to alloc a statement handle.");
		return FALSE;
	}
	assert(stmt);

	if (! SQL_SUCCEEDED(EsSQLGetTypeInfoW(stmt, SQL_ALL_TYPES))) {
		ERRH(stmt, "failed to query Elasticsearch.");
		goto end;
	}

	/* check that we have as many columns as members in target row struct */
	if (! SQL_SUCCEEDED(EsSQLNumResultCols(stmt, &col_cnt))) {
		ERRH(stmt, "failed to get result columns count.");
		goto end;
	} else if (col_cnt != ESODBC_TYPES_MEMBERS) {
		ERRH(stmt, "Elasticsearch returned an unexpected number of columns "
				"(%d vs expected %d).", col_cnt, ESODBC_TYPES_MEMBERS);
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
			goto end; \
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
			strs_len += type_row[i].type_name_loi;
			strs_len += sizeof(*type_row[i].type_name); /* 0-term */
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

	/* start pointer where the strings will be copied in */
	pos = (SQLWCHAR *)&types[rows_fetched];

	/* copy one integer member
	 * TODO: treat NULL case */
#define ES_TYPES_COPY_INT(_member) \
	do { \
		if (type_row[i]._member ## _loi == SQL_NULL_DATA) { \
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
		/* copy data */
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

		/* apply any needed fixes */

		/* warn if scales extremes are different */
		if (types[i].maximum_scale != types[i].minimum_scale) {
			ERRH(dbc, "type `" LWPDL "` returned with non-equal max/min "
					"scale: %d/%d.", LWSTR(&types[i].type_name),
					types[i].maximum_scale, types[i].minimum_scale);
		}

		/* resolve ES type to SQL C type */
		types[i].c_concise_type = type_elastic2csql(&types[i].type_name);
		if (types[i].c_concise_type == SQL_UNKNOWN_TYPE) {
			/* ES version newer than driver's? */
			ERRH(dbc, "failed to convert type name `" LWPDL "` to SQL C type.",
					LWSTR(&types[i].type_name));
			goto end;
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
		dbc->es_types = types;
		dbc->no_types = rows_fetched;
	} else if (types) {
		/* freeing the statement went wrong */
		free(types);
		types = NULL;
	}

	return ret;
}

#if defined(_WIN32) || defined (WIN32)
/*
 * Reads system registry for ODBC DSN subkey named in attrs->dsn.
 */
static BOOL read_system_info(config_attrs_st *attrs, TCHAR *buff)
{
	HKEY hkey;
	BOOL ret = FALSE;
	const char *ktree;
	DWORD valsno, i, j; /* number of values in subkey */
	DWORD maxvallen; /* len of longest value name */
	DWORD maxdatalen; /* len of longest data (buffer) */
	TCHAR val[MAX_REG_VAL_NAME];
	DWORD vallen;
	DWORD valtype;
	BYTE *d;
	DWORD datalen;
	tstr_st tval, tdata;

	if (swprintf(val, sizeof(val)/sizeof(val[0]), WPFCP_DESC "\\" WPFWP_LDESC,
			ODBC_REG_SUBKEY_PATH, LWSTR(&attrs->dsn)) < 0) {
		ERRN("failed to print registry key path.");
		return FALSE;
	}
	/* try accessing local user's config first, if that fails, systems' */
	if (RegOpenKeyEx(HKEY_CURRENT_USER, val, /*options*/0, KEY_READ,
				&hkey) != ERROR_SUCCESS) {
		INFO("failed to open registry key `" REG_HKCU "\\" LWPD "`.",
				val);
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, val, /*options*/0, KEY_READ,
					&hkey) != ERROR_SUCCESS) {
			INFO("failed to open registry key `" REG_HKLM "\\" LWPD "`.",
					val);
			goto end;
		} else {
			ktree = REG_HKLM;
		}
	} else {
		ktree = REG_HKCU;
	}

	if (RegQueryInfoKey(hkey, /*key class*/NULL, /*len of key class*/NULL,
				/*reserved*/NULL, /*no subkeys*/NULL, /*longest subkey*/NULL,
				/*longest subkey class name*/NULL, &valsno, &maxvallen,
				&maxdatalen, /*sec descr*/NULL,
				/*update time */NULL) != ERROR_SUCCESS) {
		ERRN("Failed to query registery key info for path `%s\\" LWPD
				"`.", ktree, val);
		goto end;
	} else {
		DBG("Subkey '%s\\" LWPD "': vals: %d, lengthiest name: %d, "
				"lengthiest data: %d.", ktree, val, valsno, maxvallen,
				maxdatalen);
		// malloc buffers?
		if (MAX_REG_VAL_NAME < maxvallen)
			BUG("value name buffer too small (%d), needed: %dB.",
					MAX_REG_VAL_NAME, maxvallen);
		if (MAX_REG_DATA_SIZE < maxdatalen)
			BUG("value data buffer too small (%d), needed: %dB.",
					MAX_REG_DATA_SIZE, maxdatalen);
			/* connection could still succeeded, so carry on */
	}

	for (i = 0, j = 0; i < valsno; i ++) {
		vallen = sizeof(val) / sizeof(val[0]);
		datalen = MAX_REG_DATA_SIZE;
		d = (BYTE *)&buff[j * datalen];
		if (RegEnumValue(hkey, i, val, &vallen, /*reserved*/NULL, &valtype,
					d, &datalen) != ERROR_SUCCESS) {
			ERR("failed to read register subkey value.");
			goto end;
		}
		if (valtype != REG_SZ) {
			INFO("unused register values of type %d -- skipping.",
					valtype);
			continue;
		}
		tval = (tstr_st){val, vallen};
		tdata = (tstr_st){(SQLTCHAR *)d, datalen};
		if (assign_config_attr(attrs, &tval, &tdata, FALSE)) {
			j ++;
			DBG("reg entry`" LTPDL "`: `" LTPDL "` assigned.",
					LTSTR(&tval), LTSTR(&tdata));
		} else {
			INFO("ignoring reg entry `" LTPDL "`: `" LTPDL "`.",
					LTSTR(&tval), LTSTR(&tdata));
			/* entry not directly relevant to driver config */
		}
	}


	ret = TRUE;
end:
	RegCloseKey(hkey);

	return ret;
}
#else /* defined(_WIN32) || defined (WIN32) */
#error "unsupported platform" /* TODO */
#endif /* defined(_WIN32) || defined (WIN32) */

static BOOL prompt_user(config_attrs_st *attrs, BOOL disable_conn)
{
	//
	// TODO: display settings dialog box;
	// An error message (if popping it more than once) might be needed.
	//
	return FALSE;
}

SQLRETURN EsSQLDriverConnectW
(
		SQLHDBC             hdbc,
		SQLHWND             hwnd,
		/* "A full connection string, a partial connection string, or an empty
		 * string" */
		_In_reads_(cchConnStrIn) SQLWCHAR* szConnStrIn,
		/* "Length of *InConnectionString, in characters if the string is
		 * Unicode, or bytes if string is ANSI or DBCS." */
		SQLSMALLINT         cchConnStrIn,
		/* "Pointer to a buffer for the completed connection string. Upon
		 * successful connection to the target data source, this buffer
		 * contains the completed connection string." */
		_Out_writes_opt_(cchConnStrOutMax) SQLWCHAR* szConnStrOut,
		/* "Length of the *OutConnectionString buffer, in characters." */
		SQLSMALLINT         cchConnStrOutMax,
		/* "Pointer to a buffer in which to return the total number of
		 * characters (excluding the null-termination character) available to
		 * return in *OutConnectionString" */
		_Out_opt_ SQLSMALLINT*        pcchConnStrOut,
		/* "Flag that indicates whether the Driver Manager or driver must
		 * prompt for more connection information" */
		SQLUSMALLINT        fDriverCompletion
)
{
	esodbc_dbc_st *dbc = DBCH(hdbc);
	SQLRETURN ret;
	config_attrs_st attrs;
	wstr_st orig_dsn;
	BOOL disable_conn = FALSE;
	BOOL user_canceled = FALSE;
	TCHAR buff[(sizeof(config_attrs_st)/sizeof(wstr_st)) * MAX_REG_DATA_SIZE];


	memset(&attrs, 0, sizeof(attrs));

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
		orig_dsn = attrs.dsn;

		/* set DSN (to DEFAULT) only if both DSN and Driver kw are missing */
		if ((! attrs.driver.cnt) && (! attrs.dsn.cnt)) {
			/* If the connection string does not contain the DSN keyword, the
			 * specified data source is not found, or the DSN keyword is set
			 * to "DEFAULT", the driver retrieves the information for the
			 * Default data source. */
			INFOH(dbc, "no DRIVER or DSN keyword found in connection string: "
					"using the \"DEFAULT\" DSN.");
			attrs.dsn = MK_WSTR("DEFAULT");
		}
	} else {
		INFOH(dbc, "empty connection string: using the \"DEFAULT\" DSN.");
		attrs.dsn = MK_WSTR("DEFAULT");
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
		if (! read_system_info(&attrs, buff)) {
			/* warn, but try to carry on */
			WARNH(dbc, "failed to read system info for DSN '" LWPDL "' data.",
					LWSTR(&attrs.dsn));
			/* DM should take care of this, but just in case */
			if (! EQ_WSTR(&attrs.dsn, &MK_WSTR("DEFAULT"))) {
				attrs.dsn = MK_WSTR("DEFAULT");
				if (! read_system_info(&attrs, buff)) {
					ERRH(dbc, "failed to read system info for default DSN.");
					RET_HDIAGS(dbc, SQL_STATE_IM002);
				}
			}
		}
	} else {
		/* "If the connection string contains the DRIVER keyword, the driver
		 * cannot retrieve information about the data source from the system
		 * information." */
		INFOH(dbc, "configuring the driver '" LWPDL "'.", LWSTR(&attrs.driver));
	}

	/* whatever attributes haven't yet been set, init them with defaults */
	assign_defaults(&attrs);

	switch (fDriverCompletion) {
		case SQL_DRIVER_NOPROMPT:
			ret = do_connect(dbc, &attrs);
			if (! SQL_SUCCEEDED(ret))
				return ret;
			break;

		case SQL_DRIVER_COMPLETE_REQUIRED:
			disable_conn = TRUE;
			//no break;
		case SQL_DRIVER_COMPLETE:
			/* try connect first, then, if that fails, prompt user */
			while (! SQL_SUCCEEDED(do_connect(dbc, &attrs))) {
				if (! prompt_user(&attrs, disable_conn))
					/* user canceled */
					return SQL_NO_DATA;
			}
			break;

		case SQL_DRIVER_PROMPT:
			do {
				/* prompt user first, then try connect */
				if (! prompt_user(&attrs, FALSE))
					/* user canceled */
					return SQL_NO_DATA;
			} while (! SQL_SUCCEEDED(do_connect(dbc, &attrs)));
			break;

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
		attrs.dsn = orig_dsn;
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
			DBGH(dbc, "setting async enable to %u.", (SQLULEN)Value);
			dbc->async_enable = (SQLULEN)Value;
			break;

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

#if 0
static BOOL get_current_catalog(esodbc_dbc_st *dbc)
{
	FIXME;
}
#endif //0

SQLRETURN EsSQLGetConnectAttrW(
		SQLHDBC        ConnectionHandle,
		SQLINTEGER     Attribute,
		_Out_writes_opt_(_Inexpressible_(cbValueMax)) SQLPOINTER ValuePtr,
		SQLINTEGER     BufferLength,
		_Out_opt_ SQLINTEGER* StringLengthPtr)
{
	esodbc_dbc_st *dbc = DBCH(ConnectionHandle);
	SQLRETURN ret;
	SQLSMALLINT used;
//	SQLWCHAR *val;

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
			DBGH(dbc, "requested: catalog name (@0x%p).", dbc->catalog);
#if 0
			if (! dbc->es_types) {
				ERRH(dbc, "no connection active.");
				/* TODO: check connection state and correct state */
				RET_HDIAGS(dbc, SQL_STATE_08003);
			} else if (! get_current_catalog(dbc)) {
				ERRH(dbc, "failed to get current catalog.");
				RET_STATE(dbc, dbc->hdr.diag.state);
			}
#endif //0
#if 0
			val = dbc->catalog ? dbc->catalog : MK_WPTR("null");
			*StringLengthPtr = wcslen(*(SQLWCHAR **)ValuePtr);
			*StringLengthPtr *= sizeof(SQLWCHAR);
			*(SQLWCHAR **)ValuePtr = val;
#else //0
			// FIXME;
			ret = write_wptr(&dbc->hdr.diag, (SQLWCHAR *)ValuePtr,
					MK_WPTR("NulL"), (SQLSMALLINT)BufferLength, &used);
			if (StringLengthPtr);
				*StringLengthPtr = (SQLINTEGER)used;
			return ret;
#endif //0
			break;

		case SQL_ATTR_METADATA_ID:
			DBGH(dbc, "requested: metadata_id: %u.", dbc->metadata_id);
			*(SQLULEN *)ValuePtr = dbc->metadata_id;
			break;
		case SQL_ATTR_ASYNC_ENABLE:
			DBGH(dbc, "requested: async enable: %u.", dbc->async_enable);
			*(SQLULEN *)ValuePtr = dbc->async_enable;
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
