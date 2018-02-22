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

#ifndef __DEFS_H__
#define __DEFS_H__

/*
 * DEFaultS
 */

/* leave the timeout to default value (0: don't timeout, pos: seconds) */
#define ESODBC_TIMEOUT_DEFAULT		-1
// FIXME: review@alpha
/* TODO: should there be a max? */
#define ESODBC_MAX_ROW_ARRAY_SIZE	128
#define ESODBC_DEF_ARRAY_SIZE		1
/* max cols or args to bind */
#define ESODBC_MAX_DESC_COUNT		128
/* values for SQL_ATTR_MAX_LENGTH statement attribute */
#define ESODBC_UP_MAX_LENGTH		0 // USHORT_MAX
#define ESODBC_LO_MAX_LENGTH		0
/* max number of rows to request from server */
#define ESODBC_DEF_FETCH_SIZE		0 // no fetch size
/* prepare a STMT for a new SQL operation.
 * To be used with catalog functions, that can be all called with same stmt */
#define ESODBC_SQL_CLOSE			((SQLUSMALLINT)-1)

#define ESODBC_PATTERN_ESCAPE		"\\"
#define ESODBC_CATALOG_SEPARATOR	":"
#define ESODBC_CATALOG_TERM			"clusterName"
#define ESODBC_MAX_SCHEMA_LEN		0
#define ESODBC_QUOTE_CHAR			"\""
#define ESODBC_SCHEMA_TERM			"schema"

/* max # of active statements for a connection */
/* TODO: review@alpha */
#define ESODBC_MAX_CONCURRENT_ACTIVITIES	16
/* maximum identifer lenght */
/* TODO: review@alpha */
#define ESODBC_MAX_IDENTIFIER_LEN			128


/*
 * Not authoriative (there's actually no formal limit), but pretty informed:
 * https://stackoverflow.com/questions/417142/what-is-the-maximum-length-of-a-url-in-different-browsers
 */
/* maximum URL size */
#define ESODBC_MAX_URL_LEN				2048
/* maximum DNS name */
/* SQL_MAX_DSN_LENGTH=32 < IPv6 len */
#define ESODBC_MAX_DNS_LEN				255
/* default maximum amount of bytes to accept as answer */
#define ESODBC_DEFAULT_MAX_BODY_SIZE	10 * 1024 * 1024
/* initial size of  */
#define ESODBC_BODY_BUF_START_SIZE		4 * 1024
/* SQL plugin's REST endpoint for SQL */
#define ELASTIC_SQL_PATH				"/_xpack/sql"
#define ELASTIC_SQL_PATH_TABLES			"tables"
/* default host to connect to */
//#define ESODBC_DEFAULT_HOST			"localhost"
/* to loopback capture on Win10 */
#define ESODBC_DEFAULT_HOST				"127.0.0.1"
/* Elasticsearch'es default port */
#define ESODBC_DEFAULT_PORT				9200
/* default security (TLS) setting */
#define ESODBC_DEFAULT_SEC				0
/* default global request timeout */
#define ESODBC_DEFAULT_TIMEOUT			0
/* don't follow redirection from the server  */
#define ESODBC_DEFAULT_FOLLOW			1



#endif /* __DEFS_H__ */
