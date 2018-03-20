/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#ifndef __DEFS_H__
#define __DEFS_H__

/*
 * DEFaultS
 */

// FIXME: review@alpha
/* TODO: should there be a max? */
#define ESODBC_MAX_ROW_ARRAY_SIZE	128
#define ESODBC_DEF_ARRAY_SIZE		1
/* max cols or args to bind */
#define ESODBC_MAX_DESC_COUNT		128
/* values for SQL_ATTR_MAX_LENGTH statement attribute */
#define ESODBC_UP_MAX_LENGTH		0 // USHORT_MAX
#define ESODBC_LO_MAX_LENGTH		0
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

/* SQL plugin's REST endpoint for SQL */
#define ELASTIC_SQL_PATH				"/_xpack/sql"

#define ELASTIC_SQL_PATH_TABLES			"tables"

/* initial receive buffer size for REST answers */
#define ESODBC_BODY_BUF_START_SIZE		4 * 1024

/*
 * Config defaults
 */
/* default maximum amount of bytes to accept in REST answers */
#define ESODBC_DEF_MAX_BODY_SIZE_MB	"10"
/* max number of rows to request from server */
#define ESODBC_DEF_FETCH_SIZE		"0" // no fetch size
/* default host to connect to */
//#define ESODBC_DEF_HOST			"localhost"
/* to allow loopback capture on Win10 */
#define ESODBC_DEF_HOST				"127.0.0.1"
/* Elasticsearch'es default port */
#define ESODBC_DEF_PORT				"9200"
/* default security (TLS) setting */
#define ESODBC_DEF_SECURE			"no"
/* default global request timeout (0: no timeout) */
#define ESODBC_DEF_TIMEOUT			"0"
/* don't follow redirection from the server  */
#define ESODBC_DEF_FOLLOW			"yes"
/* packing of REST bodies (JSON or CBOR) */
#define ESODBC_DEF_PACKING			"JSON"
/* default tracing level */
#define ESODBC_DEF_TRACE_LEVEL		"WARN"



#endif /* __DEFS_H__ */
