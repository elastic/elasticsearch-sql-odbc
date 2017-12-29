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
	esodbc_state_et state = SQL_STATE_HY000;
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
				state = SQL_STATE_HY090;
				break;
			}
			if (! StringLengthPtr) {
				state = SQL_STATE_HY013;
				ERR("invalid StringLengthPtr provided (NULL).");
				break;
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
				state = SQL_STATE_HY000;
				break;
			}
			/* has the string been trucated? */
			if (len + /*\0*/1 < sizeof(ESODBC_SQL_SPEC_STRING)) {
				INFO("not enough buffer size to write the driver version "
						"(provided: %d).", BufferLength);
				state = SQL_STATE_01004;
			} else {
				state = SQL_STATE_00000;
			}
			DBG("returning driver version string: "LTPD", len: %d.", 
					InfoValue ? InfoValue : "", *StringLengthPtr);
			break;

		/* "if the driver can execute functions asynchronously on the
		 * connection handle" */
		case SQL_ASYNC_DBC_FUNCTIONS:
			/* TODO: review@alpha */
			*(SQLUSMALLINT *)InfoValue = SQL_FALSE;
			DBG("driver does not support async fuctions (currently).");
			state = SQL_STATE_00000;
			break;

		/* "if the driver supports asynchronous notification" */
		case SQL_ASYNC_NOTIFICATION:
			/* TODO: review@alpha */
			*(SQLUINTEGER *)InfoValue = SQL_ASYNC_NOTIFICATION_NOT_CAPABLE;
			DBG("driver does not support async notifications (currently).");
			state = SQL_STATE_00000;
			break;

		/* "the maximum number of active statements that the driver can
		 * support for a connection" */
		//case SQL_ACTIVE_STATEMENTS:
		case SQL_MAX_CONCURRENT_ACTIVITIES:
			*(SQLUSMALLINT *)InfoValue = ESODBC_MAX_CONCURRENT_ACTIVITIES;
			DBG("max active statements per connection: %d.", 
					*(SQLUSMALLINT *)InfoValue);
			state = SQL_STATE_00000;
			break;

		default:
			ERR("unknown InfoType: %u.", InfoType);
			state = SQL_STATE_HYC00; //096
			break;
	}

	return SQLRET4STATE(state);
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

	/* API assumes there's always enough buffer here.. */
	/* no err indicator */
	wcsncpy(Sqlstate, esodbc_errors[diag->state].code, SQL_SQLSTATE_SIZE);
	*NativeError = diag->native_code;

	/* always return how many we would need, or have used */
	*TextLength = diag->native_len; /* count needed in characters */

	if (MessageText && diag->native_text) {
		if (diag->native_len < BufferLength) {
			if ((BufferLength % 2) && (1 < sizeof(SQLTCHAR))) {
				/* not specified in API for this function, but pretty much for
				 * any other wide-char using ones */
				ERR("BufferLength not an even number: %d.", BufferLength);
				return SQL_ERROR;
			}
			/* no error indication exists */
			wcsncpy(MessageText, diag->native_text, diag->native_len);
			DBG("native diagnostic text: '"LTPD"' (%d).", MessageText, 
					diag->native_len);
			return SQL_SUCCESS;
		} else {
			if (BufferLength  < 0) {
				ERR("buffer length must be non-negative; received: %d.", 
						BufferLength);
				return SQL_ERROR;
			}
			/* no error indication exists */
			wcsncpy(MessageText, diag->native_text, BufferLength);
			INFO("not enough space to copy native message; "
					"have: %d, need: %d.", BufferLength, *TextLength);
			return SQL_SUCCESS_WITH_INFO;
		}
	}

	assert(0); /* shouldn't get here */
	return SQL_ERROR;
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
