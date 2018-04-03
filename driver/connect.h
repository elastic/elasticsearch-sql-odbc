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

#ifndef __CONNECT_H__
#define __CONNECT_H__

#include "error.h"
#include "handles.h"

BOOL connect_init();
void connect_cleanup();

SQLRETURN post_statement(esodbc_stmt_st *stmt);
void cleanup_dbc(esodbc_dbc_st *dbc);

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
		_Out_opt_ SQLINTEGER* StringLengthPtr);

#endif /* __CONNECT_H__ */


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
