/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "log.h"
#include "handles.h"


#if ODBCVER == 0X0380
/* String constant for supported ODBC version */
#define ESODBC_SQL_SPEC_STRING	"03.80"
#else /* ver==3.8 */
#error "unsupported ODBC version"
#endif /* ver==3.8 */



/* max # of active statements for a connection" */
/* TODO: review@alpha */
#define ESODBC_MAX_CONCURRENT_ACTIVITIES	16


#define GET_DIAG(_h/*handle*/, _ht/*~type*/, _d/*diagnostic*/) \
	do { \
		switch(_ht) { \
			case SQL_HANDLE_SENV: \
			case SQL_HANDLE_ENV: _d = &(((esodbc_env_st *)_h)->diag); break; \
			case SQL_HANDLE_DBC: _d = &(((esodbc_dbc_st*)_h)->diag); break;\
			case SQL_HANDLE_STMT: _d = &(((esodbc_stmt_st*)_h)->diag); break;\
			case SQL_HANDLE_DESC: _d = &(((esodbc_env_st *)_h)->diag); break; \
			default: \
				ERR("unknown HandleType: %d.", _ht); \
				return SQL_INVALID_HANDLE; \
		} \
	} while(0)


/* List of supported functions in the driver */
// FIXME: review at alpha what's implemented
static SQLUSMALLINT esodbc_functions[] = {
	SQL_API_SQLALLOCHANDLE,
	SQL_API_SQLBINDCOL,
	SQL_API_SQLCANCEL,
	SQL_API_SQLCLOSECURSOR,
	SQL_API_SQLCOLATTRIBUTE,
	SQL_API_SQLCONNECT,
	SQL_API_SQLCOPYDESC,
	SQL_API_SQLDATASOURCES,
	SQL_API_SQLDESCRIBECOL,
	SQL_API_SQLDISCONNECT,
	SQL_API_SQLDRIVERS,
	SQL_API_SQLENDTRAN,
	SQL_API_SQLEXECDIRECT,
	SQL_API_SQLEXECUTE,
	SQL_API_SQLFETCH,
	SQL_API_SQLFETCHSCROLL,
	SQL_API_SQLFREEHANDLE,
	SQL_API_SQLFREESTMT,
	SQL_API_SQLGETCONNECTATTR,
	SQL_API_SQLGETCURSORNAME,
	SQL_API_SQLGETDATA,
	SQL_API_SQLGETDESCFIELD,
	SQL_API_SQLGETDESCREC,
	SQL_API_SQLGETDIAGFIELD,
	SQL_API_SQLGETDIAGREC,
	SQL_API_SQLGETENVATTR,
	SQL_API_SQLGETFUNCTIONS,
	SQL_API_SQLGETINFO,
	SQL_API_SQLGETSTMTATTR,
	SQL_API_SQLGETTYPEINFO,
	SQL_API_SQLNUMRESULTCOLS,
	SQL_API_SQLPARAMDATA,
	SQL_API_SQLPREPARE,
	SQL_API_SQLPUTDATA,
	SQL_API_SQLROWCOUNT,
	SQL_API_SQLSETCONNECTATTR,
	SQL_API_SQLSETCURSORNAME,
	SQL_API_SQLSETDESCFIELD,
	SQL_API_SQLSETDESCREC,
	SQL_API_SQLSETENVATTR,
	SQL_API_SQLSETSTMTATTR,
	SQL_API_SQLCOLUMNS,
	SQL_API_SQLSPECIALCOLUMNS,
	SQL_API_SQLSTATISTICS,
	SQL_API_SQLTABLES,
	SQL_API_SQLBINDPARAMETER,
	SQL_API_SQLBROWSECONNECT,
	SQL_API_SQLBULKOPERATIONS,
	SQL_API_SQLCOLUMNPRIVILEGES,
	SQL_API_SQLDESCRIBEPARAM,
	SQL_API_SQLDRIVERCONNECT,
	SQL_API_SQLFOREIGNKEYS,
	SQL_API_SQLMORERESULTS,
	SQL_API_SQLNATIVESQL,
	SQL_API_SQLNUMPARAMS,
	SQL_API_SQLPRIMARYKEYS,
	SQL_API_SQLPROCEDURECOLUMNS,
	SQL_API_SQLPROCEDURES,
	SQL_API_SQLSETPOS,
	SQL_API_SQLTABLEPRIVILEGES,
};

#define ESODBC_FUNC_SIZE \
	(sizeof(esodbc_functions)/sizeof(esodbc_functions[0]))
// TODO: are these def'd in sql.h??
#define SQL_FUNC_SET(pfExists, uwAPI) \
	*(((UWORD*) (pfExists)) + ((uwAPI) >> 4)) |= (1 << ((uwAPI) & 0x000F))
#define SQL_API_ODBC2_ALL_FUNCTIONS_SIZE	100

/*
 * """
 * The SQL_MAX_DRIVER_CONNECTIONS option in SQLGetInfo specifies how many
 * active connections a particular driver supports.
 * """
 */
SQLRETURN EsSQLGetInfoW(SQLHDBC ConnectionHandle,
		SQLUSMALLINT InfoType, 
		_Out_writes_bytes_opt_(BufferLength) SQLPOINTER InfoValue,
		SQLSMALLINT BufferLength,
		_Out_opt_ SQLSMALLINT *StringLengthPtr)
{
	int len;

	switch (InfoType) {
		/* Driver Information */
		/* "what version of odbc a driver complies with" */
		case SQL_DRIVER_ODBC_VER:
			if (! InfoValue)
				BufferLength = 0;
			if (BufferLength % 2) {
				ERR("invalid (odd) buffer length provided: %d.", BufferLength);
				// FIXME: "post" this states to the environment
				RET_HDIAG(DBCH(ConnectionHandle), SQL_STATE_HY090, 
						"invalid (odd) buffer length provided", 0);
			}
			if (! StringLengthPtr) {
				ERR("invalid StringLengthPtr provided (NULL).");
				RET_HDIAG(DBCH(ConnectionHandle), SQL_STATE_HY013, 
						"invalid StringLengthPtr provided (NULL)", 0);
			}
			/* return value always set to what it needs to be written. */
			*StringLengthPtr = sizeof(ESODBC_SQL_SPEC_STRING) - /*\0*/1;
			*StringLengthPtr *= sizeof(SQLTCHAR);
			if (! InfoValue) {
				/* only return how large of a buffer we need */
				INFO("NULL InfoValue buffer: returning needed buffer size "
						"only (%d).", *StringLengthPtr);
				break;
			}
			len = swprintf(InfoValue, BufferLength/sizeof(SQLTCHAR), 
					WPFCP_DESC, ESODBC_SQL_SPEC_STRING);
			if (len < 0) {
				ERRN("failed to print the driver version with InfoValue: 0x%p,"
						" BufferLength: %d.", InfoValue, BufferLength);
				RET_HDIAGS(DBCH(ConnectionHandle), SQL_STATE_HY000);
			}
			DBG("returning driver version string: "LTPD", len: %d.", 
					InfoValue, *StringLengthPtr);
			/* has the string been trucated? */
			if (len + /*\0*/1 < sizeof(ESODBC_SQL_SPEC_STRING)) {
				INFO("not enough buffer size to write the driver version "
						"(provided: %d).", BufferLength);
				RET_HDIAGS(DBCH(ConnectionHandle), SQL_STATE_01004);
			}
			break;

		/* "if the driver can execute functions asynchronously on the
		 * connection handle" */
		case SQL_ASYNC_DBC_FUNCTIONS:
			/* TODO: review@alpha */
			*(SQLUSMALLINT *)InfoValue = SQL_FALSE;
			DBG("driver does not support async fuctions (currently).");
			break;

		/* "if the driver supports asynchronous notification" */
		case SQL_ASYNC_NOTIFICATION:
			// FIXME: review@alpha */
			*(SQLUINTEGER *)InfoValue = SQL_ASYNC_NOTIFICATION_NOT_CAPABLE;
			DBG("driver does not support async notifications (currently).");
			break;

		/* "the maximum number of active statements that the driver can
		 * support for a connection" */
		//case SQL_ACTIVE_STATEMENTS:
		case SQL_MAX_CONCURRENT_ACTIVITIES:
			*(SQLUSMALLINT *)InfoValue = ESODBC_MAX_CONCURRENT_ACTIVITIES;
			DBG("max active statements per connection: %d.", 
					*(SQLUSMALLINT *)InfoValue);
			break;

		case SQL_CURSOR_COMMIT_BEHAVIOR:
		case SQL_CURSOR_ROLLBACK_BEHAVIOR:
			DBG("DM asking for cursor %s behavior.", 
					InfoType == SQL_CURSOR_COMMIT_BEHAVIOR ? 
					"commit" : "rollback");
			/* assume this is the  of equivalent of
			 * JDBC's HOLD_CURSORS_OVER_COMMIT */
			*(SQLUSMALLINT *)InfoValue = SQL_CB_PRESERVE;
			break;

		case SQL_GETDATA_EXTENSIONS:
			DBG("DM asking for GetData extentions.");
			// FIXME: review@alpha
			*(SQLUINTEGER *)InfoValue = 0;
			break;

		default:
			ERR("unknown InfoType: %u.", InfoType);
			RET_HDIAGS(DBCH(ConnectionHandle), SQL_STATE_HYC00/*096?*/);
	}

	RET_STATE(SQL_STATE_00000);
}

/* TODO: see error.h: esodbc_errors definition note (2.x apps support) */
SQLRETURN EsSQLGetDiagFieldW(
		SQLSMALLINT HandleType, 
		SQLHANDLE Handle,
		SQLSMALLINT RecNumber,
		SQLSMALLINT DiagIdentifier,
		_Out_writes_opt_(_Inexpressible_(BufferLength)) SQLPOINTER DiagInfoPtr,
		SQLSMALLINT BufferLength,
		_Out_opt_ SQLSMALLINT *StringLengthPtr)
{
	esodbc_diag_st *diag;

	if (! Handle) {
		ERR("null handle provided.");
		return SQL_INVALID_HANDLE;
	}
#if 0
	// FIXME: true for all cases?
	// NO: with SQL_DIAG_SQLSTATE DM provides a NULL ptr here... WTF?!
	if (! StringLengthPtr) { 
		ERR("null StringLengthPtr pointer provided.");
		return SQL_ERROR;
	}
#endif

	switch(DiagIdentifier) {
		/* Header Fields */
		case SQL_DIAG_NUMBER:
			assert(StringLengthPtr); // FIXME
			*StringLengthPtr = sizeof(SQLINTEGER);
			if (DiagInfoPtr) {
				// FIXME: check HandleType's record count (1 or 0)
				*(SQLINTEGER *)DiagInfoPtr = 0;
				DBG("available diagnostics: %d.", *(SQLINTEGER *)DiagInfoPtr);
			} else {
				DBG("no DiagInfo buffer provided - returning ");
				return SQL_SUCCESS_WITH_INFO;
			}
			RET_NOT_IMPLEMENTED;
			break;
		case SQL_DIAG_CURSOR_ROW_COUNT:
		case SQL_DIAG_DYNAMIC_FUNCTION:
		case SQL_DIAG_DYNAMIC_FUNCTION_CODE:
		case SQL_DIAG_ROW_COUNT:
			if (HandleType != SQL_HANDLE_STMT) {
				ERR("DiagIdentifier %d called with non-statement handle "
						"type %d.", DiagIdentifier, HandleType);
				return SQL_ERROR;
			}
			RET_NOT_IMPLEMENTED;
			break;
		/* case SQL_DIAG_RETURNCODE: break; -- DM only */

		/* Record Fields */
		case SQL_DIAG_CLASS_ORIGIN: //break;
		case SQL_DIAG_COLUMN_NUMBER: //break;
		case SQL_DIAG_CONNECTION_NAME: //break;
		case SQL_DIAG_MESSAGE_TEXT: //break;
		case SQL_DIAG_NATIVE: //break;
		case SQL_DIAG_ROW_NUMBER: //break;
		case SQL_DIAG_SERVER_NAME: //break;
			RET_NOT_IMPLEMENTED;
		case SQL_DIAG_SQLSTATE: 
			GET_DIAG(Handle, HandleType, diag);
			if (diag->state == SQL_STATE_00000) {
				DBG("no diagnostic available for handle type %d.", HandleType);
				return SQL_NO_DATA;
			} else {
				if (BufferLength % 2) {
					ERR("BufferLength not an even number: %d.", BufferLength);
					return SQL_ERROR;
				}
#if 0
				/* always return how many bytes we would need (or used) */
				*StringLengthPtr = SQL_SQLSTATE_SIZE + /*\0*/1;
				*StringLengthPtr *= sizeof(SQLTCHAR);
#endif
				if (SQL_SQLSTATE_SIZE < BufferLength) {
					/* no error indicator exists */
					wcsncpy(DiagInfoPtr, esodbc_errors[diag->state].code, 
							SQL_SQLSTATE_SIZE);
					DBG("SQL state for handler of type %d: "LTPD".", 
							HandleType, DiagInfoPtr);
					return SQL_SUCCESS;
				} else {
					if (BufferLength < 0) {
						ERR("negative BufferLength rcvd: %d.", BufferLength);
						return SQL_ERROR;
					}
					/* no error indicator exists */
					wcsncpy(DiagInfoPtr, esodbc_errors[diag->state].code, 
							BufferLength);
					INFO("not enough space to copy state; have: %d, need: %d.",
							BufferLength,
/* CL trips on this macro -- WTF?? */
//#if 0
//							*StringLengthPtr
//#else
							(SQL_SQLSTATE_SIZE + /*\0*/1) * sizeof(SQLTCHAR)
//#endif
							);
					return SQL_SUCCESS_WITH_INFO;
				}
			}
			break;
		case SQL_DIAG_SUBCLASS_ORIGIN: //break;
			RET_NOT_IMPLEMENTED;

		default:
			ERR("unknown DiagIdentifier: %d.", DiagIdentifier);
			return SQL_ERROR;
	}

	assert(0); // FIXME: shouldn't get here
	return SQL_ERROR;
}

/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/appendix-a-odbc-error-codes :
 * """
 * SQLGetDiagRec or SQLGetDiagField returns SQLSTATE values as defined by Open
 * Group Data Management: Structured Query Language (SQL), Version 2 (March
 * 1995). SQLSTATE values are strings that contain five characters.
 * """
 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/implementing-sqlgetdiagrec-and-sqlgetdiagfield :
 */
/* TODO: see error.h: esodbc_errors definition note (2.x apps support) */
SQLRETURN EsSQLGetDiagRecW
(
	SQLSMALLINT HandleType,
	SQLHANDLE Handle,
	SQLSMALLINT RecNumber,
	_Out_writes_opt_(6) SQLWCHAR *Sqlstate,
	SQLINTEGER *NativeError,
	_Out_writes_opt_(BufferLength) SQLWCHAR *MessageText,
	SQLSMALLINT BufferLength,
	_Out_opt_ SQLSMALLINT *TextLength
)
{
	esodbc_diag_st *diag;

	if (RecNumber <= 0) {
		ERR("record number must be >=1; received: %d.", RecNumber);
		return SQL_ERROR;
	}
	if (1 < RecNumber) {
		/* XXX: does it make sense to have error FIFOs? (mysql doesn't) */
		ERR("no error lists supported (yet).");
		return SQL_NO_DATA;
	}

	if (! Handle) {
		ERR("NULL handle provided.");
		return SQL_ERROR;
	} else if (HandleType == SQL_HANDLE_SENV) {
		/*
		 * https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdiagrec-function :
		 * """
		 * A call to SQLGetDiagRec will return SQL_INVALID_HANDLE if
		 * HandleType is SQL_HANDLE_SENV.
		 * """
		 */
		ERR("shared environment handle type not allowed.");
		return SQL_INVALID_HANDLE;
	} else {
		GET_DIAG(Handle, HandleType, diag);
	}

	if (diag->state == SQL_STATE_00000) {
		INFO("no diagnostic record available for handle type %d.", HandleType);
		return SQL_NO_DATA;
	}

	/* not documented in API, but both below can be null. */
	/* API assumes there's always enough buffer here.. */
	/* no err indicator */
	if (Sqlstate)
		wcsncpy(Sqlstate, esodbc_errors[diag->state].code, SQL_SQLSTATE_SIZE);
	if (NativeError)
		*NativeError = diag->native_code;

	/* always return how many we would need, or have used */
	*TextLength = diag->text_len; /* count needed in characters */

	if (MessageText && diag->text_len) {
		if (diag->text_len < BufferLength) {
			if ((BufferLength % 2) && (1 < sizeof(SQLTCHAR))) {
				/* not specified in API for this function, but pretty much for
				 * any other wide-char using ones */
				ERR("BufferLength not an even number: %d.", BufferLength);
				return SQL_ERROR;
			}
			/* no error indication exists */
			wcsncpy(MessageText, diag->text, diag->text_len);
			DBG("diagnostic text: '"LTPD"' (%d).", MessageText,diag->text_len);
			return SQL_SUCCESS;
		} else {
			if (BufferLength  < 0) {
				ERR("buffer length must be non-negative; received: %d.", 
						BufferLength);
				return SQL_ERROR;
			}
			/* no error indication exists */
			wcsncpy(MessageText, diag->text, BufferLength);
			INFO("not enough space to copy diagnostic message; "
					"have: %d, need: %d.", BufferLength, *TextLength);
			return SQL_SUCCESS_WITH_INFO;
		}
	}

	DBG("call only asking for available diagnostic characters to return (%d).",
			*TextLength);
	return SQL_SUCCESS;
}


SQLRETURN EsSQLGetFunctions(SQLHDBC ConnectionHandle,
		SQLUSMALLINT FunctionId, 
		_Out_writes_opt_(_Inexpressible_("Buffer length pfExists points to depends on fFunction value.")) SQLUSMALLINT *Supported)
{
	int i;

	if (FunctionId == SQL_API_ODBC3_ALL_FUNCTIONS) {
		DBG("ODBC 3.x application asking for supported function set.");
		memset(Supported, 0,
				SQL_API_ODBC3_ALL_FUNCTIONS_SIZE * sizeof(SQLSMALLINT));
		for (i = 0; i < ESODBC_FUNC_SIZE; i++)
			SQL_FUNC_SET(Supported, esodbc_functions[i]);
	} else if (FunctionId == SQL_API_ALL_FUNCTIONS) {
		DBG("ODBC 2.x application asking for supported function set.");
		memset(Supported, SQL_FALSE,
				SQL_API_ODBC2_ALL_FUNCTIONS_SIZE * sizeof(SQLUSMALLINT));
		for (i = 0; i < ESODBC_FUNC_SIZE; i++)
			if (esodbc_functions[i] < SQL_API_ODBC2_ALL_FUNCTIONS_SIZE)
				Supported[esodbc_functions[i]] = SQL_TRUE;
	} else {
		DBG("application asking for support of function #%d.", FunctionId);
		*Supported = SQL_FALSE;
		for (i = 0; i < ESODBC_FUNC_SIZE; i++)
			if (esodbc_functions[i] == FunctionId) {
				*Supported = SQL_TRUE;
				break;
			}
	}

	// TODO: does this require connecting to the server?
	RET_STATE(SQL_STATE_00000);
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
