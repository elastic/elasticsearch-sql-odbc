/*
 * ELASTICSEARCH CONFIDENTIAL
 * __________________
 *
 *  [2014] Elasticsearch Incorporated. All Rights Reserved.
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

#ifndef __HANDLES_H__
#define __HANDLES_H__

#include "error.h"

/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/environment-handles :
 * """
 * The environment's state
 * The current environment-level diagnostics
 * The handles of connections currently allocated on the environment
 * The current settings of each environment attribute
 * """
 */
typedef struct struct_env {
	SQLUINTEGER version; /* versions defined as UL (see SQL_OV_ODBC3) */
	/* diagnostic/state keeping */
	// TODO: needed, actually?
	esodbc_diag_st diag;
	// TODO: connections
} esodbc_env_st;

/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/connection-handles : 
 * """
 * The state of the connection
 * The current connection-level diagnostics 
 * The handles of statements and descriptors currently allocated on the
 * connection
 * The current settings of each connection attribute
 * """
 */
typedef struct struct_dbc {
	esodbc_env_st *env;
	/* diagnostic/state keeping */
	esodbc_diag_st diag;
	SQLTCHAR *connstr;
	SQLUINTEGER timeout;
	// FIXME: placeholder; used if connection has been established or not
	// TODO: PROTO
	void *conn;
	// TODO: statements?
} esodbc_dbc_st;

typedef struct struct_desc {
	//esodbc_stmt_st *stmt;
	esodbc_diag_st diag;
} esodbc_desc_st;

typedef struct struct_stmt {
	esodbc_dbc_st *dbc;
	esodbc_diag_st diag;
	// TODO: descriptors
	/* pointers to the current descriptors */
	esodbc_desc_st *ard;
	esodbc_desc_st *ird;
	esodbc_desc_st *apd;
	esodbc_desc_st *ipd;
	/* initial implicit descriptors allocated with the statement */
	esodbc_desc_st i_ard;
	esodbc_desc_st i_ird;
	esodbc_desc_st i_apd;
	esodbc_desc_st i_ipd;
} esodbc_stmt_st;


// FIXME: review@alpha
#define ESODBC_DBC_CONN_TIMEOUT	120

SQLRETURN EsSQLAllocHandle(SQLSMALLINT HandleType,
	SQLHANDLE InputHandle, _Out_ SQLHANDLE *OutputHandle);
SQLRETURN EsSQLFreeHandle(SQLSMALLINT HandleType, SQLHANDLE Handle);

SQLRETURN EsSQLSetEnvAttr(SQLHENV EnvironmentHandle,
		SQLINTEGER Attribute, 
		_In_reads_bytes_opt_(StringLength) SQLPOINTER Value,
		SQLINTEGER StringLength);
SQLRETURN SQL_API EsSQLGetEnvAttr(SQLHENV EnvironmentHandle,
		SQLINTEGER Attribute, 
		_Out_writes_(_Inexpressible_(BufferLength)) SQLPOINTER Value,
		SQLINTEGER BufferLength, _Out_opt_ SQLINTEGER *StringLength);

SQLRETURN EsSQLSetConnectAttrW(
		SQLHDBC ConnectionHandle,
		SQLINTEGER Attribute,
		_In_reads_bytes_opt_(StringLength) SQLPOINTER Value,
		SQLINTEGER StringLength);


SQLRETURN EsSQLGetStmtAttrW(
		SQLHSTMT     StatementHandle,
		SQLINTEGER   Attribute,
		SQLPOINTER   ValuePtr,
		SQLINTEGER   BufferLength,
		SQLINTEGER  *StringLengthPtr);

#define ENVH(_h)	((esodbc_env_st *)(_h))
#define DBCH(_h)	((esodbc_dbc_st *)(_h))
#define STMH(_h)	((esodbc_stmt_st *)(_h))
#define DSCH(_h)	((esodbc_desc_st *)(_h))

/* wraper of RET_CDIAG, compatible with any defined handle */
#define RET_HDIAG(_hp/*handle ptr*/, _s/*tate*/, _t/*char text*/, _c/*ode*/) \
	RET_CDIAG(&(_hp)->diag, _s, _t, _c)
/* similar to RET_HDIAG, but only post the state */
#define RET_HDIAGS(_hp/*handle ptr*/, _s/*tate*/) \
	RET_DIAG(&(_hp)->diag, _s, NULL, 0)

/* return the code associated with the given state (and debug-log) */
#define RET_STATE(_s)	\
	do { \
		SQLRETURN _r = esodbc_errors[_s].retcode; \
		SQLTCHAR *_c = esodbc_errors[_s].code; \
		DBG("returning state "LTPD", code %d.", _c, _r); \
		return _r; \
	} while (0)

#define RET_NOT_IMPLEMENTED	\
	do { \
		BUG("not implemented.");\
		return SQL_ERROR; \
	} while (0)

#endif /* __HANDLES_H__ */

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
