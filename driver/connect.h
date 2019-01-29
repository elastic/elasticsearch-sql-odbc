/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#ifndef __CONNECT_H__
#define __CONNECT_H__

#include "error.h"
#include "handles.h"
#include "dsn.h"

BOOL connect_init();
void connect_cleanup();

SQLRETURN dbc_curl_set_url(esodbc_dbc_st *dbc, int url_type);
SQLRETURN post_json(esodbc_stmt_st *stmt, int url_type, const cstr_st *u8body);
void cleanup_dbc(esodbc_dbc_st *dbc);
SQLRETURN do_connect(esodbc_dbc_st *dbc, esodbc_dsn_attrs_st *attrs);
SQLRETURN config_dbc(esodbc_dbc_st *dbc, esodbc_dsn_attrs_st *attrs);


SQLRETURN EsSQLDriverConnectW
(
	SQLHDBC             hdbc,
	SQLHWND             hwnd,
	_In_reads_(cchConnStrIn) SQLWCHAR *szConnStrIn,
	SQLSMALLINT         cchConnStrIn,
	_Out_writes_opt_(cchConnStrOutMax) SQLWCHAR *szConnStrOut,
	SQLSMALLINT         cchConnStrOutMax,
	_Out_opt_ SQLSMALLINT        *pcchConnStrOut,
	SQLUSMALLINT        fDriverCompletion
);
SQLRETURN EsSQLConnectW
(
	SQLHDBC             hdbc,
	_In_reads_(cchDSN) SQLWCHAR *szDSN,
	SQLSMALLINT         cchDSN,
	_In_reads_(cchUID) SQLWCHAR *szUID,
	SQLSMALLINT         cchUID,
	_In_reads_(cchAuthStr) SQLWCHAR *szPWD,
	SQLSMALLINT         cchPWD
);
SQLRETURN EsSQLDisconnect(SQLHDBC ConnectionHandle);

SQLRETURN EsSQLSetConnectAttrW(
	SQLHDBC ConnectionHandle,
	SQLINTEGER Attribute,
	_In_reads_bytes_opt_(StringLength) SQLPOINTER Value,
	SQLINTEGER StringLength);
SQLRETURN EsSQLGetConnectAttrW(
	SQLHDBC        ConnectionHandle,
	SQLINTEGER     Attribute,
	_Out_writes_opt_(_Inexpressible_(cbValueMax)) SQLPOINTER ValuePtr,
	SQLINTEGER     BufferLength,
	_Out_opt_ SQLINTEGER *StringLengthPtr);

#endif /* __CONNECT_H__ */


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
