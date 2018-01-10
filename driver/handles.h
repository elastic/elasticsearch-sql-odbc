/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#ifndef __HANDLES_H__
#define __HANDLES_H__

#include "error.h"
#include "log.h"

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


typedef enum {
	DESC_TYPE_ANON, /* SQLAllocHandle()'ed */
	DESC_TYPE_ARD,
	DESC_TYPE_IRD,
	DESC_TYPE_APD,
	DESC_TYPE_IPD,
} desc_type_et;

typedef struct struct_desc {
	//esodbc_stmt_st *stmt;
	esodbc_diag_st diag;
	desc_type_et type; /* APD, IPD, ARD, IRD */
} esodbc_desc_st;


typedef struct stmt_options {
	/* use bookmarks? */
	SQLULEN bookmarks;
	/* offset in bytes to the bound addresses */
	/* "The driver calculates the buffer address just before it writes to the
	 * buffers (such as during fetch time)." */
	SQLULEN *bind_offset; /* TODO: ARD option only? */
	/* bound array size */
	SQLULEN array_size; /* TODO: ARD option only */
	/* row/column, with block cursors */
	SQLULEN bind_type; /* TODO: ARD option only */
	/* row status values after Fetch/Scroll */
	SQLUSMALLINT *row_status; /* TODO: IRD option only */
	/* number of rows fetched */
	SQLULEN *rows_fetched; /* TODO: IRD option only */
} stmt_options_st;

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

	stmt_options_st options;
} esodbc_stmt_st;


// FIXME: review@alpha
#define ESODBC_DBC_CONN_TIMEOUT		120
#define ESODBC_MAX_ROW_ARRAY_SIZE	128 /* TODO: should there be a max? */

SQLRETURN EsSQLAllocHandle(SQLSMALLINT HandleType,
	SQLHANDLE InputHandle, _Out_ SQLHANDLE *OutputHandle);
SQLRETURN EsSQLFreeHandle(SQLSMALLINT HandleType, SQLHANDLE Handle);
SQLRETURN EsSQLFreeStmt(SQLHSTMT StatementHandle, SQLUSMALLINT Option);

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


SQLRETURN EsSQLSetStmtAttrW(
		SQLHSTMT           StatementHandle,
		SQLINTEGER         Attribute,
		SQLPOINTER         ValuePtr,
		SQLINTEGER         BufferLength);
SQLRETURN EsSQLGetStmtAttrW(
		SQLHSTMT     StatementHandle,
		SQLINTEGER   Attribute,
		SQLPOINTER   ValuePtr,
		SQLINTEGER   BufferLength,
		SQLINTEGER  *StringLengthPtr);

SQLRETURN EsSQLSetDescRec(
		SQLHDESC DescriptorHandle,
		SQLSMALLINT RecNumber,
		SQLSMALLINT Type,
		SQLSMALLINT SubType,
		SQLLEN Length,
		SQLSMALLINT Precision,
		SQLSMALLINT Scale,
		_Inout_updates_bytes_opt_(Length) SQLPOINTER Data, 
		_Inout_opt_ SQLLEN *StringLength,
		_Inout_opt_ SQLLEN *Indicator);

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

#include <assert.h>
#define FIXME \
	do { \
		BUG("not yet implemented"); \
		assert(0); \
	} while (0)

#endif /* __HANDLES_H__ */

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
