/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#ifndef __CONNECT_H__
#define __CONNECT_H__

#include "error.h"
#include "handles.h"

BOOL connect_init();
void connect_cleanup();

long post_sql(esodbc_dbc_st *dbc, long timeout, const char *u8json, 
		long jlen, char *const answer, long avail);
long post_sql_tables(esodbc_dbc_st *dbc, long timeout, const char *u8json, 
		long jlen, char *const answer, long avail);

SQLRETURN EsSQLDriverConnectW
(
		SQLHDBC             hdbc,
		SQLHWND             hwnd,
		_In_reads_(cchConnStrIn) SQLWCHAR* szConnStrIn,
		SQLSMALLINT         cchConnStrIn,
		_Out_writes_opt_(cchConnStrOutMax) SQLWCHAR* szConnStrOut,
		SQLSMALLINT         cchConnStrOutMax,
		_Out_opt_ SQLSMALLINT*        pcchConnStrOut,
		SQLUSMALLINT        fDriverCompletion
);

SQLRETURN EsSQLDisconnect(SQLHDBC ConnectionHandle);

#endif /* __CONNECT_H__ */


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
