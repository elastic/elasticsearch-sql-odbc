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

#include "connect.h"
#include "log.h"

/*
 * Not authoriative (there's no formal limit), but pretty informed:
 * https://stackoverflow.com/questions/417142/what-is-the-maximum-length-of-a-url-in-different-browsers
 */
/* maximum URL size */
#define ESODBC_MAX_URL_LEN		2048
/* maximum DNS name */
#define ESODBC_MAX_DNS_LEN		255 /* SQL_MAX_DSN_LENGTH=32 < IPv6 len */
/* SQL plugin's REST endpoint for SQL */
#define ELASTIC_SQL_PATH		"/_xpack/sql"
#define ELASTIC_SQL_PATH_TABLES		"tables"
/* default host to connect to */
#define ESODBC_DEFAULT_HOST		"localhost"
/* Elasticsearch'es default port */
#define ESODBC_DEFAULT_PORT		9200
/* default security (TLS) setting */
#define ESODBC_DEFAULT_SEC		0
/* default global request timeout */
#define ESODBC_DEFAULT_TIMEOUT	0
/* don't follow redirection from the server  */
#define ESODBC_DEFAULT_FOLLOW	0 // TODO: review@alpha

/* HTTP headers default for every request */
#define HTTP_ACCEPT_JSON		"Accept: application/json"
#define HTTP_CONTENT_TYPE_JSON	"Content-Type: application/json; charset=utf-8"

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
	long avail = dbc->wlen - dbc->wpos;
	long have = (long)(size * nmemb);
	long count;

	DBG("libcurl: DBC 0x%p, new data chunk of size [%zd] x %zd arrived; "
			"available buffer: %ld.", dbc, nmemb, size, avail);

	if (! dbc->wbuf) {
		ERR("libcurl: callback has no buffer to write into.");
		return 0;
	}
	if (avail <= 0) {
		INFO("libcurl: write buffer in DBC 0x%p full, can't write %d bytes.",
				dbc, have);
		return 0;
	}
	if (avail < have) {
		WARN("libcurl: buffer to small for available answer: "
				"left %ld, have %ld.", avail, have);
		count = avail;
	} else {
		count = have;
	}

	memcpy(dbc->wbuf + dbc->wpos, ptr, count);
	dbc->wpos += count;
	DBG("libcurl: DBC 0x%p, copied: %ld bytes.", count);


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
	return count;
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
 * Returns how many bytes have been gotten back, or negative on failure.
 */
long post_sql(esodbc_dbc_st *dbc, 
		long timeout, /* req timeout; set to negative for default */
		const char *u8json, /* stringified JSON object, UTF-8 encoded */
		long jlen, /* size of the u8json buffer */
		char *const answer, /* buffer to receive the answer in */
		long avail /*size of the answer buffer */)
{
	CURLcode res = CURLE_OK;
	SQLTCHAR *emsg;
	long to;

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
			emsg = MK_TSTR("failed to set transport timeout value");
			goto err;
		} else {
			DBG("libcurl: set curl 0x%p timeout to %ld.", dbc, to);
		}
	}

	/* set body to send */
	if (u8json) {
		/* len of the body */
		res = curl_easy_setopt(dbc->curl, CURLOPT_POSTFIELDSIZE, jlen);
		if (res != CURLE_OK) {
			emsg = MK_TSTR("failed to set transport content");
			goto err;
		} else {
			DBG("libcurl: set curl 0x%p post fieldsize to %ld.", dbc, jlen);
		}
		/* body itself */
		res = curl_easy_setopt(dbc->curl, CURLOPT_POSTFIELDS, u8json);
		if (res != CURLE_OK) {
			emsg = MK_TSTR("failed to set transport content");
			goto err;
		} else {
			DBG("libcurl: set curl 0x%p post fields to `%.*s`.", dbc, 
					jlen, u8json);
		}
	}

	/* set call-back members */
	dbc->wbuf = answer;
	dbc->wpos = 0;
	dbc->wlen = avail;

	/* execute the request */
	res = curl_easy_perform(dbc->curl);
	if (res != CURLE_OK) {
		emsg = MK_TSTR("data transfer failure");
		goto err;
	} else {
		DBG("libcurl: request succesfull, got back %ld bytes in answer.",  
				dbc->wpos);
	}

	return dbc->wpos;

err:
	ERR("libcurl: request on DBC 0x%p (timeout:%ld, u8json:`%.*s`, "
			"answer:0x%p, avail:%ld) failed: '"LTPD"', '%s' (%d).", dbc, 
			timeout, 0, u8json ? u8json : "<NULL>", answer, avail, emsg,
			res ? curl_easy_strerror(res) : "<unspecified>", res);
	/* if buffer len has been set, the error occured in _perform() */
	post_diagnostic(&dbc->diag, dbc->wlen ? SQL_STATE_08S01 : SQL_STATE_HY000, 
			emsg, res);
	cleanup_curl(dbc);
	return -1;
}


/* temporary function to get the available tables (indexes) */
long post_sql_tables(esodbc_dbc_st *dbc, long timeout, const char *u8json, 
		long jlen, char *const answer, long avail)
{
	char *urlp;
	char url[ESODBC_MAX_URL_LEN], orig_url[ESODBC_MAX_URL_LEN];
	long ret;
	CURLcode res;
	CURL *curl = dbc->curl;
	
	/* get current URL */
	res = curl_easy_getinfo(dbc->curl, CURLINFO_EFFECTIVE_URL, &urlp);
	if (res != CURLE_OK) {
		ERR("libcurl: failed to get effective URL: %s (%d)", 
				curl_easy_strerror(res), res);
		goto err;
	} 
	/* low perf, lazy copy */
	if (snprintf(orig_url, sizeof(orig_url), "%s", urlp) < 0) {
		ERRN("failed to save original URL `%s`.", urlp);
		goto err;
	}
	DBG("saved original URL: `%s`.", orig_url);
	/* construct new URL */
	if (snprintf(url, sizeof(url), "%s/%s", urlp, ELASTIC_SQL_PATH_TABLES)<0) {
		ERRN("failed to build tables URL.");
		goto err;
	}
	DBG("tables URL: `%s`.", url);

	/* TODO: will this free the original URL?? */
	res = curl_easy_setopt(curl, CURLOPT_URL, url);
	if (res != CURLE_OK) {
		ERR("libcurl: failed to set URL `%s`: %s (%d).", url,
				curl_easy_strerror(res), res);
		goto err;
	}

	ret = post_sql(dbc, timeout, u8json, jlen, answer, avail);
	if (ret < 0) {
		ERR("SQL posting failed; ret=%d.", ret);
		goto err;
	}

	/* reinstate original URL */
	res = curl_easy_setopt(curl, CURLOPT_URL, orig_url);
	if (res != CURLE_OK) {
		ERR("libcurl: failed to set URL `%s`: %s (%d).", url,
				curl_easy_strerror(res), res);
		goto err;
	}

	return ret;

err:
	cleanup_curl(dbc);
	SET_HDIAG(dbc, SQL_STATE_HY000, "failed to init the transport", 0);
	return -1;
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
	int n;
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
				"Timeout=%d;"
				"Follow=%d;"
				"TraceFile="WPFCP_DESC";"
				"TraceLevel="WPFCP_DESC";"
				"User=user;Password=pass;Catalog=cat",
				szConnStrIn, "host", 9200, 0, "json", 100, -1, 0, 
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

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
