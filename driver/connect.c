/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "connect.h"
#include "queries.h"
#include "log.h"
#include "info.h"

/* HTTP headers default for every request */
#define HTTP_ACCEPT_JSON		"Accept: application/json"
#define HTTP_CONTENT_TYPE_JSON	"Content-Type: application/json; charset=utf-8"

#define JSON_SQL_QUERY_START		"{\"query\":\""
#define JSON_SQL_QUERY_MID			"\""
#define JSON_SQL_QUERY_MID_FETCH	JSON_SQL_QUERY_MID ",\"fetch_size\":"
#define JSON_SQL_QUERY_END			"}"
#define JSON_SQL_CURSOR_START		"{\"cursor\":\""
#define JSON_SQL_CURSOR_END			"\"}"

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

	DBG("libcurl: DBC@0x%p, new data chunk of size [%zd] x %zd arrived; "
			"available buffer: %zd/%zd.", dbc, nmemb, size, avail, dbc->alen);

	/* do I need to grow the existing buffer? */
	if (avail < have) {
		/* calculate how much space to allocate. start from existing lenght,
		 * if set, othewise from a constant (on first allocation). */
		for (need = dbc->alen ? dbc->alen : ESODBC_BODY_BUF_START_SIZE; 
				need < have; need *= 2)
			;
		DBG("libcurl: DBC@0x%p: growing buffer for new chunk of %zd "
				"from %zd to %zd.", dbc, have, dbc->alen, need);
		if (dbc->amax < need) { /* would I need more than max? */
			if (dbc->amax <= (size_t)dbc->alen) { /* am I at max already? */
				ERR("libcurl: DBC@0x%p: can't grow past max %zd; have: %zd", 
						dbc, dbc->amax, have);
				return 0;
			} else { /* am not: alloc max possible (possibly still < need!) */
				need = dbc->amax;
				WARN("libcurl: DBC@0x%p: capped buffer to max: %zd", dbc,need);
				/* re'eval what I have available... */
				avail = dbc->amax - dbc->apos;
				/* ...and how much I could copy (possibly less than received)*/
				have = have < avail ? have : avail;
			}
		}
		wbuf = realloc(dbc->abuff, need);
		if (! wbuf) {
			ERRN("libcurl: DBC@0x%p: failed to realloc to %zdB.", dbc, need);
			return 0;
		}
		dbc->abuff = wbuf;
		dbc->alen = need;
	}

	memcpy(dbc->abuff + dbc->apos, ptr, have);
	dbc->apos += have;
	DBG("libcurl: DBC@0x%p, copied: %zd bytes.", dbc, have);
	DBG("libcurl: DBC@0x%p, copied: `%.*s`.", dbc, have, ptr);


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
	int n;
	char *host, *port_s, *endptr;
	char url[ESODBC_MAX_URL_LEN];
	long port, secure, timeout, follow;

	assert(! dbc->curl);

	// FIXME: derive from connstr:
	host = ESODBC_DEFAULT_HOST;
	port_s = NULL;
	secure = ESODBC_DEFAULT_SEC;
	timeout = ESODBC_DEFAULT_TIMEOUT;
	follow = ESODBC_DEFAULT_FOLLOW;

	/* check host and port values */
	if (ESODBC_MAX_DNS_LEN < strlen(host)) {
		ERR("host name `%s` longer than max allowed (%d)", host, 
				ESODBC_MAX_DNS_LEN);
		RET_HDIAG(dbc, SQL_STATE_IM010, "host name longer than max", 0);
	}
	if (port_s) {
		if (/*65535*/5 < strlen(port_s)) {
			ERR("port `%s` longer than allowed (5).", port_s);
			/* reusing code */
			RET_HDIAG(dbc, SQL_STATE_IM010, "port longer than max", 0); 
		}
		errno = 0;
		port = strtol(port_s, &endptr, 10);
		if (errno) {
			ERRN("failed to scan port `%s` to integer.", port_s);
			RET_HDIAG(dbc, SQL_STATE_IM010, "invalid port number", 0); 
		} else if (*endptr) {
			ERR("failed to scan all chars in port `%s`.", port_s);
			RET_HDIAG(dbc, SQL_STATE_IM010, "invalid port number", 0); 
		} else if (USHRT_MAX < port) {
			ERR("port value %d higher than max (%u)", port, USHRT_MAX);
			RET_HDIAG(dbc, SQL_STATE_IM010, "port value to high", 0); 
		}
	} else {
		port = ESODBC_DEFAULT_PORT;
	}
	n = snprintf(url, sizeof(url), "http%s://%s:%d" ELASTIC_SQL_PATH, 
			secure ? "s" : "", host, port);
	if (n < 0 || sizeof(url) <= n) {
		ERR("libcurl: failed to build destination URL.");
		RET_HDIAG(dbc, SQL_STATE_IM010, "failed to build destination URL", 0); 
	}
	DBG("libcurl: SQL URL to connect to %s.", url);

	/* get a libcurl handle */
	curl = curl_easy_init();
	if (! curl) {
		ERRN("libcurl: failed to fetch new handle.");
		RET_HDIAG(dbc, SQL_STATE_HY000, "failed to init the transport", 0);
	}

#ifndef NDEBUG
	res = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_err_buff);
	if (res != CURLE_OK) {
		ERR("libcurl: failed to set errorbuffer: %s (%d).", 
				curl_easy_strerror(res), res);
		/* not fatal */
	}
#endif /* NDEBUG */


	/* set URL to connect to */
	res = curl_easy_setopt(curl, CURLOPT_URL, url);
	if (res != CURLE_OK) {
		ERR("libcurl: failed to set URL `%s`: %s (%d).", url,
				curl_easy_strerror(res), res);
		SET_HDIAG(dbc, SQL_STATE_HY000, "failed to init the transport", 0);
		goto err;
	}
	/* always do POSTs (seconded by CURLOPT_POSTFIELDS) */
	res = curl_easy_setopt(curl, CURLOPT_POST, 1L);
	if (res != CURLE_OK) {
		ERR("libcurl: failed to set POST method: %s (%d).",
				curl_easy_strerror(res), res);
		SET_HDIAG(dbc, SQL_STATE_HY000, "failed to init the transport", 0);
		goto err;
	}

	/* set the Content-Type, Accept HTTP headers */
	res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_headers);
	if (res != CURLE_OK) {
		ERR("libcurl: failed to set headers: %s (%d).",
				curl_easy_strerror(res), res);
		SET_HDIAG(dbc, SQL_STATE_HY000, "failed to init the transport", 0);
		goto err;
	}

	/* set the behavior for redirection */
	res = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, follow);
	if (res != CURLE_OK) {
		ERR("libcurl: failed to set redirection: %s (%d).",
				curl_easy_strerror(res), res);
		SET_HDIAG(dbc, SQL_STATE_HY000, "failed to init the transport", 0);
		goto err;
	}

	/* set the write call-back for answers */
	res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	if (res != CURLE_OK) {
		ERR("libcurl: failed to set write callback: %s (%d).",
				curl_easy_strerror(res), res);
		SET_HDIAG(dbc, SQL_STATE_HY000, "failed to init the transport", 0);
		goto err;
	}
	/* ... and its argument */
	res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, dbc);
	if (res != CURLE_OK) {
		ERR("libcurl: failed to set callback argument: %s (%d).",
				curl_easy_strerror(res), res);
		SET_HDIAG(dbc, SQL_STATE_HY000, "failed to init the transport", 0);
		goto err;
	}

	dbc->curl = curl;
	DBG("libcurl: new handle 0x%p for dbc 0x%p.", curl, dbc);
	RET_STATE(SQL_STATE_00000);

err:
	if (curl)
		curl_easy_cleanup(curl);
	RET_STATE(dbc->diag.state);
}

static void cleanup_curl(esodbc_dbc_st *dbc)
{
	if (! dbc->curl)
		return;
	DBG("libcurl: handle 0x%p cleanup for dbc 0x%p.", dbc->curl, dbc);
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
	esodbc_dbc_st *dbc = stmt->dbc;
	char *abuff = NULL;
	size_t apos;
	long to, code;

	DBGSTMT(stmt, "posting request `%.*s` (%zd).", blen, u8body, blen);

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
			DBG("libcurl: set curl 0x%p timeout to %ld.", dbc, to);
		}
	}

	/* set body to send */
	if (u8body) {
		/* len of the body */
		res = curl_easy_setopt(dbc->curl, CURLOPT_POSTFIELDSIZE, blen);
		if (res != CURLE_OK) {
			goto err;
		} else {
			DBG("libcurl: set curl 0x%p post fieldsize to %ld.", dbc, blen);
		}
		/* body itself */
		res = curl_easy_setopt(dbc->curl, CURLOPT_POSTFIELDS, u8body);
		if (res != CURLE_OK) {
			goto err;
		} else {
			DBG("libcurl: set curl 0x%p post fields to `%.*s`.", dbc, 
					blen, u8body);
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
	DBG("libcurl: request succesfull, received code %ld and %zd bytes back.", 
			code, apos);

	if (code != 200) {
		ERR("libcurl: non-200 HTTP response code %ld received.", code);
		/* expect a 200 with body; everything else is failure (todo?)  */
		if (400 <= code)
			return attach_error(stmt, abuff, apos);
		goto err_net;
	}

	return attach_answer(stmt, abuff, apos);

err:
	ERRSTMT(stmt, "libcurl: request failed (timeout:%ld, body:`%.*s`) "
			"failed: '%s' (%d).", timeout, blen, u8body, 
			res != CURLE_OK ? curl_easy_strerror(res) : "<unspecified>", res);
err_net: /* the error occured after the request hit hit the network */
	if (abuff)
		free(abuff);
	cleanup_curl(dbc);
	if (abuff) /* if buffer had been set, the error occured in _perform() */
		RET_HDIAG(stmt, SQL_STATE_08S01, "data transfer failure", res);
	else
		RET_HDIAG(stmt, SQL_STATE_HY000, "failed to init transport", res);
}

/* retuns the lenght of a buffer to hold the escaped variant of the unescaped
 * given json object  */
static inline size_t json_escaped_len(const char *json, size_t len)
{
	size_t i, newlen = 0;
	unsigned char uchar;
	for (i = 0; i < len; i ++) {
		uchar = (unsigned char)json[i];
		switch(uchar) {
			case '"':
			case '\\':
			case '/':
			case '\b':
			case '\f':
			case '\n':
			case '\r':
			case '\t':
				newlen += /* '\' + json[i] */2;
				break;
			default:
				newlen += (0x20 <= uchar) ? 1 : /* \u00XX */6;
				// break
		}
	}
	return newlen;
}

/* 
 * JSON-escapes a string.
 * If string len is 0, it assumes a NTS.
 * If output buffer (jout) is NULL, it returns the buffer size needed for
 * escaping.
 * Returns number of used bytes in buffer (which might be less than buffer
 * size, if some char needs an escaping longer than remaining space).
 */
static size_t json_escape(const char *jin, size_t inlen, char *jout, 
		size_t outlen)
{
	size_t i, pos;
	unsigned char uchar;

#define I16TOA(_x)	(10 <= (_x)) ? 'A' + (_x) - 10 : '0' + (_x)

	if (! inlen)
		inlen = strlen(jin);
	if (! jout)
		return json_escaped_len(jin, inlen);

	for (i = 0, pos = 0; i < inlen; i ++) {
		uchar = jin[i];
		switch(uchar) {
			do {
			case '\b': uchar = 'b'; break;
			case '\f': uchar = 'f'; break;
			case '\n': uchar = 'n'; break;
			case '\r': uchar = 'r'; break;
			case '\t': uchar = 't'; break;
			} while (0);
			case '"':
			case '\\':
			case '/':
				if (outlen <= pos + 1) {
					i = inlen; // break the for loop
					continue;
				}
				jout[pos ++] = '\\';
				jout[pos ++] = (char)uchar;
				break;
			default:
				if (0x20 <= uchar) {
					if (outlen <= pos) {
						i = inlen;
						continue;
					}
					jout[pos ++] = uchar;
				} else { // case 0x00 .. 0x1F
					if (outlen <= pos + 5) {
						i = inlen;
						continue;
					}
					memcpy(jout + pos, "\\u00", sizeof("\\u00") - 1);
					pos += sizeof("\\u00") - 1;
					jout[pos ++] = I16TOA(uchar >> 4);
					jout[pos ++] = I16TOA(uchar & 0xF);
				}
				break;
		}
	}
	return pos;
#undef I16TOA
}

/*
 * Build a JSON query or cursor object (if statment having one) and send it as
 * the body of a REST POST requrest.
 */
SQLRETURN post_statement(esodbc_stmt_st *stmt)
{
	SQLRETURN ret;
	esodbc_dbc_st *dbc = stmt->dbc;
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
	if (stmt->rset.eccnt) { /* eval CURSOR object lenght */
		/* convert cursor to C [mb]string. */
		/* TODO: ansi_w2c() fits better for Base64 encoded cursors. */
		u8len = WCS2U8(stmt->rset.ecurs, (int)stmt->rset.eccnt, u8curs,
				sizeof(u8curs));
		if (u8len <= 0) {
			ERRSTMT(stmt, "failed to convert cursor `" LTPDL "` to UTF8: %d.",
					stmt->rset.eccnt, stmt->rset.ecurs, WCS2U8_ERRNO());
			RET_HDIAGS(stmt, SQL_STATE_24000);
		}

		bodylen = sizeof(JSON_SQL_CURSOR_START) - /*\0*/1;
		bodylen += json_escape(u8curs, u8len, NULL, 0);
		bodylen += sizeof(JSON_SQL_CURSOR_END) - /*\0*/1;
	} else { /* eval QUERY object lenght */
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
		WARN("local buffer too small (%zd), need %zdB; will alloc.", 
				sizeof(buff), bodylen);
		WARN("local buffer too small, SQL: `%.*s`.", stmt->sqllen,stmt->u8sql);
		body = malloc(bodylen);
		if (! body) {
			ERRN("failed to alloc %zdB.", bodylen);
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

	ret = post_request(stmt, ESODBC_DEFAULT_TIMEOUT, body, pos);

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
	SQLTCHAR *connstr;
	size_t n, i;
	char *url = NULL;

	DBG("Input connection string: '"LTPD"' (%d).", szConnStrIn, cchConnStrIn);
	if (! pcchConnStrOut) {
		ERR("null pcchConnStrOut parameter");
		RET_HDIAGS(dbc, SQL_STATE_HY000);
	}

	//
	// FIXME: get and parse the connection string.
	//
#if 0
/* Options for SQLDriverConnect */
#define SQL_DRIVER_NOPROMPT             0
#define SQL_DRIVER_COMPLETE             1
#define SQL_DRIVER_PROMPT               2
#define SQL_DRIVER_COMPLETE_REQUIRED    3
#endif

	// FIXME: set the proper connection string;
	connstr = MK_TSTR("connection string placeholder");
	dbc->connstr = _wcsdup(connstr);
	if (! dbc->connstr) {
		ERRN("failed to dup string `%s`.", connstr);
		RET_HDIAGS(dbc, SQL_STATE_HY001);
	}
	// FIXME: read from connection string
	dbc->amax = ESODBC_DEFAULT_MAX_BODY_SIZE;

	dbc->timeout = ESODBC_TIMEOUT_DEFAULT;
	dbc->fetch.max = ESODBC_DEF_FETCH_SIZE;
	dbc->metadata_id = SQL_FALSE;
	dbc->async_enable = SQL_ASYNC_ENABLE_OFF;

	/* set the string representation of fetch_size, once for all STMTs */
	if (dbc->fetch.max) {
		for (n = dbc->fetch.max, dbc->fetch.slen = 0; n; n /= 10)
			dbc->fetch.slen ++;
		dbc->fetch.str = malloc(dbc->fetch.slen + /*\0*/1);
		if (! dbc->fetch.str) {
			ERRN("failed to alloc %zdB.", dbc->fetch.slen);
			RET_HDIAGS(dbc, SQL_STATE_HY001);
		}
		dbc->fetch.str[dbc->fetch.slen] = 0;
		for (n = dbc->fetch.max, i = dbc->fetch.slen; 0 < i; n /= 10, i --)
			dbc->fetch.str[i - 1] = '0' + (n % 10);

		DBG("DBC@0x%p, fetch_size: `%.*s` (%c).", dbc, dbc->fetch.slen, 
				dbc->fetch.str, dbc->fetch.slen);
	}

	ret = init_curl(dbc);
	if (! SQL_SUCCEEDED(ret)) {
		ERR("failed to init transport for DBC 0x%p.", dbc);
		return ret;
	}

	/* perform a connection test, to fail quickly if wrong address/port AND
	 * populate the DNS cache; this won't guarantee succesful post'ing, tho! */
	ret = test_connect(dbc->curl);
	/* still ok if fails */
	curl_easy_getinfo(dbc->curl, CURLINFO_EFFECTIVE_URL, &url);
	if (! SQL_SUCCEEDED(ret)) {
		ERR("test connection to URL %s failed!", url);
		cleanup_curl(dbc);
		RET_HDIAG(dbc, SQL_STATE_HYT01, "connection test failed", 0);
	} else {
		DBG("test connection to URL %s OK.", url);
	}

	/* return the final connection string */
	if (szConnStrOut) {
		// TODO: Driver param
		n = swprintf(szConnStrOut, cchConnStrOutMax, WPFWP_DESC ";"
				"Server="WPFCP_DESC";"
				"Port=%d;"
				"Secure=%d;"
				"Packing="WPFCP_DESC";"
				"MaxFetchSize=%d;"
				"MaxBodySize="WPFCP_DESC";"
				"Timeout=%d;"
				"Follow=%d;"
				"TraceFile="WPFCP_DESC";"
				"TraceLevel="WPFCP_DESC";"
				"User=user;Password=pass;Catalog=cat",
				szConnStrIn, "host", 9200, 0, "json", 100, "10M", -1, 0, 
				"C:\\foo.txt", "DEBUG");
		if (n < 0) {
			ERRN("failed to outprint connection string.");
			RET_HDIAGS(dbc, SQL_STATE_HY000);
		} else {
			*pcchConnStrOut = (SQLSMALLINT)n;
		}
	}

	RET_STATE(SQL_STATE_00000);
}

/* "Implicitly allocated descriptors can be freed only by calling
 * SQLDisconnect, which drops any statements or descriptors open on the
 * connection" */
SQLRETURN EsSQLDisconnect(SQLHDBC ConnectionHandle)
{
	esodbc_dbc_st *dbc = DBCH(ConnectionHandle);
	// FIXME: disconnect
	DBG("disconnecting from 0x%p", dbc);

	if (dbc->connstr) {
		free(dbc->connstr);
		dbc->connstr = NULL;
	}

	cleanup_curl(dbc);

	RET_STATE(SQL_STATE_00000);
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
			INFO("no ANSI/Unicode specific behaviour (app is: %s).",
					(uintptr_t)Value == SQL_AA_TRUE ? "ANSI" : "Unicode");
			/* TODO: API doesn't require to set a state? */
			//state = SQL_STATE_IM001;
			return SQL_ERROR; /* error means ANSI */

		case SQL_ATTR_LOGIN_TIMEOUT:
			if (dbc->conn) {
				ERR("connection already established, can't set connection"
						" timeout (to %u).", (SQLUINTEGER)(uintptr_t)Value);
				RET_HDIAG(dbc, SQL_STATE_HY011, "connection established, "
						"can't set connection timeout.", 0);
			}
			INFO("setting connection timeout to: %u, from previous: %u.", 
					dbc->timeout, (SQLUINTEGER)(uintptr_t)Value);
			dbc->timeout = (long)(uintptr_t)Value;
			break;

		/* https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/automatic-population-of-the-ipd */
		case SQL_ATTR_AUTO_IPD:
			ERR("trying to set read-only attribute AUTO IPD.");
			RET_HDIAGS(dbc, SQL_STATE_HY092);
		case SQL_ATTR_ENABLE_AUTO_IPD:
			if (*(SQLUINTEGER *)Value != SQL_FALSE) {
				ERR("trying to enable unsupported attribute AUTO IPD.");
				RET_HDIAGS(dbc, SQL_STATE_HYC00);
			}
			WARN("disabling (unsupported) attribute AUTO IPD -- NOOP.");
			break;

		case SQL_ATTR_METADATA_ID:
			DBG("DBC 0x%p setting metadata_id to %u.", dbc, (SQLULEN)Value);
			dbc->metadata_id = (SQLULEN)Value;
			break;
		case SQL_ATTR_ASYNC_ENABLE:
			DBG("DBC 0x%p setting async enable to %u.", dbc, (SQLULEN)Value);
			dbc->async_enable = (SQLULEN)Value;
			break;


		default:
			ERR("unknown Attribute: %d.", Attribute);
			RET_HDIAGS(DBCH(ConnectionHandle), SQL_STATE_HY092);
	}

	RET_STATE(SQL_STATE_00000);
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
//	SQLTCHAR *val;

	switch(Attribute) {
		/* https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/automatic-population-of-the-ipd */
		/* "whether automatic population of the IPD after a call to SQLPrepare
		 * is supported" */
		case SQL_ATTR_AUTO_IPD:
			DBG("requested: support for attribute AUTO IPD (false).");
			/* "Servers that do not support prepared statements will not be
			 * able to populate the IPD automatically." */
			*(SQLUINTEGER *)ValuePtr = SQL_FALSE;
			break;

		/* "the name of the catalog to be used by the data source" */
		case SQL_ATTR_CURRENT_CATALOG:
			DBG("requested: catalog name (@0x%p).", dbc->catalog);
#if 0
			if (! dbc->conn) {
				ERR("no connection active on handle 0x%p.", dbc);
				/* TODO: check connection state and correct state */
				RET_HDIAGS(dbc, SQL_STATE_08003);
			} else if (! get_current_catalog(dbc)) {
				ERR("failed to get current catalog on handle 0x%p.", dbc);
				RET_STATE(dbc, dbc->diag.state);
			}
#endif //0
#if 0
			val = dbc->catalog ? dbc->catalog : MK_TSTR("null");
			*StringLengthPtr = wcslen(*(SQLTCHAR **)ValuePtr);
			*StringLengthPtr *= sizeof(SQLTCHAR);
			*(SQLTCHAR **)ValuePtr = val;
#else //0
			// FIXME;
			ret = write_tstr(&dbc->diag, (SQLTCHAR *)ValuePtr, 
					MK_TSTR("NulL"), (SQLSMALLINT)BufferLength, &used);
			if (StringLengthPtr);
				*StringLengthPtr = (SQLINTEGER)used;
			return ret;
			
#endif //0
			break;

		case SQL_ATTR_METADATA_ID:
			DBG("requested: metadata_id: %u.", dbc->metadata_id);
			*(SQLULEN *)ValuePtr = dbc->metadata_id;
			break;
		case SQL_ATTR_ASYNC_ENABLE:
			DBG("requested: async enable: %u.", dbc->async_enable);
			*(SQLULEN *)ValuePtr = dbc->async_enable;
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
		case SQL_ATTR_QUIET_MODE:
		case SQL_ATTR_TRACE:
		case SQL_ATTR_TRACEFILE:
		case SQL_ATTR_TRANSLATE_LIB:
		case SQL_ATTR_TRANSLATE_OPTION:
		case SQL_ATTR_TXN_ISOLATION:

		default:
			// FIXME: add the other attributes
			FIXME;
			ERR("unknown Attribute type %d.", Attribute);
			RET_HDIAGS(DBCH(ConnectionHandle), SQL_STATE_HY092);
	}
	
	RET_STATE(SQL_STATE_00000);
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
