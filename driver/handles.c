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

#include <assert.h>

#include "handles.h"
#include "log.h"

/*
 * The Driver Manager does not call the driver-level environment handle
 * allocation function until the application calls SQLConnect,
 * SQLBrowseConnect, or SQLDriverConnect.
 *
 * 1. The Driver Manager then calls SQLAllocHandle in the driver with the
 * SQL_HANDLE_DBC option, whether or not it was just loaded.
 * 2. SQLSetConnectAttr()
 * 3. Finally, the Driver Manager calls the connection function in the driver.
 * 4. SQLDisconnect()
 * 5. SQLFreeHandle()
 */
SQLRETURN EsSQLAllocHandle(SQLSMALLINT HandleType,
	SQLHANDLE InputHandle, _Out_ SQLHANDLE *OutputHandle)
{
	//SQLHANDLE *out_handle = SQL_NULL_HANDLE;
	esodbc_env_st *env_h = SQL_NULL_HANDLE;
	esodbc_dbc_st *dbc_h = SQL_NULL_HANDLE;
	esodbc_state_et state = SQL_STATE_HY000;

	switch(HandleType) {
		/* 
		 * https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlallochandle-function :
		 * """
		 * If the Driver Manager cannot allocate memory for *OutputHandlePtr
		 * when SQLAllocHandle with a HandleType of SQL_HANDLE_ENV is called,
		 * or the application provides a null pointer for OutputHandlePtr,
		 * SQLAllocHandle returns SQL_ERROR. The Driver Manager sets
		 * *OutputHandlePtr to SQL_NULL_HENV (unless the application provided
		 * a null pointer, which returns SQL_ERROR). There is no handle with
		 * which to associate additional diagnostic information.
		 * """
		 */
		case SQL_HANDLE_ENV: /* Environment Handle */
			if (InputHandle != SQL_NULL_HANDLE) {
				WARN("passed InputHandle not null (=0x%p).", InputHandle);
				state = SQL_STATE_01000;
			}
			if (! OutputHandle) {
				ERR("null output handle provided.");
				state = SQL_STATE_HY009;
				break;
			}
			env_h = (esodbc_env_st *)calloc(1, sizeof(esodbc_env_st));
			if (! env_h) {
				ERRN("failed to callocate env handle.");
				*OutputHandle = SQL_NULL_HENV;
				state = SQL_STATE_HY001;
			} else {
				env_h->diag = NULL_DIAG;
				*OutputHandle = (SQLHANDLE *)env_h;
				state = SQL_STATE_00000;
			}
			// TODO: PROTO
			break;
		case SQL_HANDLE_DBC: /* Connection Handle */
			/* https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlallochandle-function#diagnostics :
			 * """
			 * If the SQL_ATTR_ODBC_VERSION environment attribute is not set
			 * before SQLAllocHandle is called to allocate a connection handle
			 * on the environment, the call to allocate the connection will
			 * return SQLSTATE HY010 (Function sequence error).
			 * """
			 */
			if (! ((esodbc_env_st *)InputHandle)->version) {
				state = SQL_STATE_HY010;
				((esodbc_env_st *)InputHandle)->diag.state = state;
				break;
			}
			dbc_h = (esodbc_dbc_st *)calloc(1, sizeof(esodbc_dbc_st));
			if (! dbc_h) {
				ERRN("failed to callocate connection handle.");
				*OutputHandle = SQL_NULL_HDBC;
				state = SQL_STATE_HY001;
			} else {
				dbc_h->diag = NULL_DIAG;
				*OutputHandle = (SQLHANDLE *)dbc_h;
				state = SQL_STATE_00000;
			}
			break;
		case SQL_HANDLE_DESC:
			//break;
		case SQL_HANDLE_STMT:
			//break;
		case SQL_HANDLE_SENV: /* Shared Environment Handle */
			break;
#if 0
		case SQL_HANDLE_DBC_INFO_TOKEN:
			//break;
#endif
		default:
			ERR("unknown HandleType: %d.", HandleType);
			return SQL_INVALID_HANDLE;
	}

	return SQLRET4STATE(state);
}

SQLRETURN EsSQLFreeHandle(SQLSMALLINT HandleType, SQLHANDLE Handle)
{
	esodbc_state_et state;
	
	switch(HandleType) {
		case SQL_HANDLE_ENV: /* Environment Handle */
			if (! Handle) {
				ERR("provided null Handle.");
				state = SQL_STATE_HY009;
			} else {
				/* TODO: check if there are connections (_DBC) */
				free(Handle);
				state = SQL_STATE_00000;
			}
			break;
		case SQL_HANDLE_DBC: /* Connection Handle */
			if (! Handle) {
				ERR("provided null Handle.");
				state = SQL_STATE_HY009;
			} else {
				/* TODO: remove from (potential) list? */
				free(Handle);
				state = SQL_STATE_00000;
			}
			break;
		case SQL_HANDLE_DESC:
			//break;
		case SQL_HANDLE_STMT:
			//break;
		case SQL_HANDLE_SENV: /* Shared Environment Handle */
			state = SQL_STATE_HYC00;
			break;
#if 0
		case SQL_HANDLE_DBC_INFO_TOKEN:
			//break;
#endif
		default:
			ERR("unknown HandleType: %d.", HandleType);
			return SQL_INVALID_HANDLE;
	}

	return SQLRET4STATE(state);
}


/*
 * To specify an ODBC compliance level of 3.8, an application calls
 * SQLSetEnvAttr with the SQL_ATTR_ODBC_VERSION attribute set to
 * SQL_OV_ODBC3_80. To determine the version of the driver, an application
 * calls SQLGetInfo with SQL_DRIVER_ODBC_VER.
 */
SQLRETURN EsSQLSetEnvAttr(SQLHENV EnvironmentHandle,
		SQLINTEGER Attribute, 
		_In_reads_bytes_opt_(StringLength) SQLPOINTER Value,
		SQLINTEGER StringLength)
{
	esodbc_state_et state = SQL_STATE_HY000;
	
	/*
	 * https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetenvattr-function :
	 * """
	 * ValuePtr
	 * [Input] Pointer to the value to be associated with Attribute. Depending
	 * on the value of Attribute, ValuePtr will be a 32-bit integer value or
	 * point to a null-terminated character string.
	 * """
	 */
	assert(sizeof(SQLINTEGER) == 4);

	switch (Attribute) {
		case SQL_ATTR_CONNECTION_POOLING:
		case SQL_ATTR_CP_MATCH:
			/* TODO: connection pooling */
			state = SQL_STATE_HYC00;
			break;
		case SQL_ATTR_ODBC_VERSION:
			if ((SQLINTEGER)Value != SQL_OV_ODBC3_80) {
				state = SQL_STATE_HYC00;
			} else {
				assert(0 < (SQLINTEGER)Value);
				((esodbc_env_st *)EnvironmentHandle)->version = 
					(SQLUINTEGER)Value;
				DBG("set version to %u.", 
						((esodbc_env_st *)EnvironmentHandle)->version);
				state = SQL_STATE_00000;
			}
			break;
		case SQL_ATTR_OUTPUT_NTS:
			if ((SQLINTEGER)Value == SQL_TRUE)
				state = SQL_STATE_00000;
			else
				state = SQL_STATE_HYC00;
			break;
		default:
			state = SQL_STATE_HY024;
			break;
	}

	return SQLRET4STATE(state);
}

SQLRETURN SQL_API EsSQLGetEnvAttr(SQLHENV EnvironmentHandle,
		SQLINTEGER Attribute, 
		_Out_writes_(_Inexpressible_(BufferLength)) SQLPOINTER Value,
		SQLINTEGER BufferLength, _Out_opt_ SQLINTEGER *StringLength)
{
	esodbc_state_et state = SQL_STATE_HY000;
	
	switch (Attribute) {
		case SQL_ATTR_CONNECTION_POOLING:
		case SQL_ATTR_CP_MATCH:
			/* TODO: connection pooling */
			state = SQL_STATE_HYC00;
			break;
		case SQL_ATTR_ODBC_VERSION:
			*((SQLUINTEGER *)Value) = 
				((esodbc_env_st *)EnvironmentHandle)->version;
			state = SQL_STATE_00000;
			break;
		case SQL_ATTR_OUTPUT_NTS:
			*((SQLUINTEGER *)Value) = SQL_TRUE;
			state = SQL_STATE_00000;
			break;
		default:
			state = SQL_STATE_HY024;
			break;
	}

	return SQLRET4STATE(state);
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
