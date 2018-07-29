/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#ifndef __DSN_H__
#define __DSN_H__

#include "util.h"

#define SUBKEY_ODBCINST		"ODBCINST.INI"
#define SUBKEY_ODBC			"ODBC.INI"

/* attribute keywords used in connection strings */
#define ESODBC_DSN_DRIVER			"Driver"
#define ESODBC_DSN_DESCRIPTION		"Description"
#define ESODBC_DSN_DSN				"DSN"
#define ESODBC_DSN_PWD				"PWD"
#define ESODBC_DSN_UID				"UID"
#define ESODBC_DSN_SAVEFILE			"SAVEFILE"
#define ESODBC_DSN_FILEDSN			"FILEDSN"
#define ESODBC_DSN_SERVER			"Server"
#define ESODBC_DSN_PORT				"Port"
#define ESODBC_DSN_SECURE			"Secure"
#define ESODBC_DSN_TIMEOUT			"Timeout"
#define ESODBC_DSN_FOLLOW			"Follow"
#define ESODBC_DSN_CATALOG			"Catalog"
#define ESODBC_DSN_PACKING			"Packing"
#define ESODBC_DSN_MAX_FETCH_SIZE	"MaxFetchSize"
#define ESODBC_DSN_MAX_BODY_SIZE_MB	"MaxBodySizeMB"
#define ESODBC_DSN_TRACE_FILE		"TraceFile"
#define ESODBC_DSN_TRACE_LEVEL		"TraceLevel"

/* stucture to collect all attributes in a connection string */
typedef struct {
	wstr_st driver;
	wstr_st description;
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
} esodbc_dsn_attrs_st;

BOOL assign_dsn_defaults(esodbc_dsn_attrs_st *attrs, BOOL duplicate);
BOOL assign_dsn_attr(esodbc_dsn_attrs_st *attrs,
	wstr_st *keyword, wstr_st *value, BOOL overwrite, BOOL duplicate);

BOOL read_system_info(esodbc_dsn_attrs_st *attrs, TCHAR *buff);
esodbc_dsn_attrs_st *load_system_dsn(SQLWCHAR *list00);
BOOL write_system_dsn(esodbc_dsn_attrs_st *attrs);
void free_dsn_attrs(esodbc_dsn_attrs_st *attrs);

BOOL parse_connection_string(esodbc_dsn_attrs_st *attrs,
	SQLWCHAR *szConnStrIn, SQLSMALLINT cchConnStrIn, BOOL duplicate);
BOOL write_connection_string(esodbc_dsn_attrs_st *attrs,
	SQLWCHAR *szConnStrOut, SQLSMALLINT cchConnStrOutMax,
	SQLSMALLINT *pcchConnStrOut);

#endif /* __DSN_H__ */


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
