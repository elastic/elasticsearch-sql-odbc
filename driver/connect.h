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


/*
 * ES-to-C-SQL mappings.
 * DATA_TYPE(SYS TYPES) : SQL_<type> -> SQL_C_<type>
 * Intervals not covered, since C==SQL, with no ES customization.
 */
/* -6: SQL_TINYINT -> SQL_C_TINYINT */
#define ES_BYTE_TO_CSQL			SQL_C_TINYINT
#define ES_BYTE_TO_SQL			SQL_TINYINT
/* 5: SQL_SMALLINT -> SQL_C_SHORT */
#define ES_SHORT_TO_CSQL		SQL_C_SSHORT
#define ES_SHORT_TO_SQL			SQL_SMALLINT
/* 4: SQL_INTEGER -> SQL_C_LONG */
#define ES_INTEGER_TO_CSQL		SQL_C_SLONG
#define ES_INTEGER_TO_SQL		SQL_INTEGER
/* -5: SQL_BIGINT -> SQL_C_SBIGINT */
#define ES_LONG_TO_CSQL			SQL_C_SBIGINT
#define ES_LONG_TO_SQL			SQL_BIGINT
/* 6: SQL_FLOAT -> SQL_C_DOUBLE */
#define ES_HALF_TO_CSQL_FLOAT	SQL_C_DOUBLE
#define ES_HALF_TO_SQL_FLOAT	SQL_FLOAT
/* 6: SQL_FLOAT -> SQL_C_DOUBLE */
#define ES_SCALED_TO_CSQL_FLOAT	SQL_C_DOUBLE
#define ES_SCALED_TO_SQL_FLOAT	SQL_DOUBLE
/* 7: SQL_REAL -> SQL_C_DOUBLE */
#define ES_FLOAT_TO_CSQL		SQL_C_FLOAT
#define ES_FLOAT_TO_SQL			SQL_REAL
/* 8: SQL_DOUBLE -> SQL_C_FLOAT */
#define ES_DOUBLE_TO_CSQL		SQL_C_DOUBLE
#define ES_DOUBLE_TO_SQL		SQL_DOUBLE
/* 16: ??? -> SQL_C_TINYINT */
#define ES_BOOLEAN_TO_CSQL		SQL_C_BIT
#define ES_BOOLEAN_TO_SQL		SQL_BIT
/* 12: SQL_VARCHAR -> SQL_C_WCHAR */
#define ES_KEYWORD_TO_CSQL		SQL_C_WCHAR /* XXX: CBOR needs _CHAR */
#define ES_KEYWORD_TO_SQL		SQL_VARCHAR
/* 12: SQL_VARCHAR -> SQL_C_WCHAR */
#define ES_TEXT_TO_CSQL			SQL_C_WCHAR /* XXX: CBOR needs _CHAR */
#define ES_TEXT_TO_SQL			SQL_VARCHAR
/* 12: SQL_VARCHAR -> SQL_C_WCHAR */
#define ES_IP_TO_CSQL			SQL_C_WCHAR /* XXX: CBOR needs _CHAR */
#define ES_IP_TO_SQL			SQL_VARCHAR
/* 92: SQL_TYPE_TIME -> SQL_C_TYPE_TIME */
#define ES_TIME_TO_CSQL			SQL_C_TYPE_TIME
#define ES_TIME_TO_SQL			SQL_TYPE_TIME
/* 91: SQL_TYPE_DATE -> SQL_C_TYPE_DATE */
#define ES_DATE_TO_CSQL			SQL_C_TYPE_DATE
#define ES_DATE_TO_SQL			SQL_TYPE_DATE
/* 93: SQL_TYPE_TIMESTAMP -> SQL_C_TYPE_TIMESTAMP */
#define ES_DATETIME_TO_CSQL		SQL_C_TYPE_TIMESTAMP
#define ES_DATETIME_TO_SQL		SQL_TYPE_TIMESTAMP
/* -3: SQL_VARBINARY -> SQL_C_BINARY */
#define ES_BINARY_TO_CSQL		SQL_C_BINARY
#define ES_BINARY_TO_SQL		SQL_VARBINARY
/* 0: SQL_TYPE_NULL -> SQL_C_TINYINT */
#define ES_NULL_TO_CSQL			SQL_C_STINYINT // ???
#define ES_NULL_TO_SQL			SQL_TYPE_NULL
/*
 * ES-non mappable
 */
/* 114: ??? -> SQL_C_WCHAR */
#define ES_GEO_TO_CSQL			SQL_C_WCHAR /* XXX: CBOR needs _CHAR */
#define ES_GEO_TO_SQL			SQL_VARCHAR

/* 1111: ??? -> SQL_C_BINARY */
#define ES_UNSUPPORTED_TO_CSQL	SQL_C_BINARY
#define ES_UNSUPPORTED_TO_SQL	ESODBC_SQL_UNSUPPORTED
/* 2002: ??? -> SQL_C_BINARY */
#define ES_OBJECT_TO_CSQL		SQL_C_BINARY
#define ES_OBJECT_TO_SQL		ESODBC_SQL_OBJECT
/* 2002: ??? -> SQL_C_BINARY */
#define ES_NESTED_TO_CSQL		SQL_C_BINARY
#define ES_NESTED_TO_SQL		ESODBC_SQL_NESTED


BOOL connect_init();
void connect_cleanup();

SQLRETURN dbc_curl_set_url(esodbc_dbc_st *dbc, int url_type);
SQLRETURN curl_post(esodbc_stmt_st *stmt, int url_type,
	const cstr_st *req_body);
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
