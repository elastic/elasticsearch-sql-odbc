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
				/* not fatal a.t.p. */
			}
			if (! OutputHandle) {
				ERR("null output handle provided.");
				RET_STATE(SQL_STATE_HY009);
			}
			*OutputHandle = (SQLHANDLE *)calloc(1, sizeof(esodbc_env_st));
			if (! *OutputHandle) {
				ERRN("failed to callocate env handle.");
				RET_STATE(SQL_STATE_HY001);
			}
			DBG("new Environment handle allocated @0x%p.", *OutputHandle);
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
			if (! ENVH(InputHandle)->version) {
				ERR("environment has not version set when allocating DBC.");
				RET_HDIAG(ENVH(InputHandle), SQL_STATE_HY010, 
						"enviornment has no version set yet", 0);
			}
			*OutputHandle = (SQLHANDLE *)calloc(1, sizeof(esodbc_dbc_st));
			if (! *OutputHandle) {
				ERRN("failed to callocate connection handle.");
				RET_HDIAGS(ENVH(InputHandle), SQL_STATE_HY001);
			}
			DBCH(*OutputHandle)->env = ENVH(InputHandle);
			DBCH(*OutputHandle)->timeout = ESODBC_DBC_CONN_TIMEOUT;
			DBG("new Connection handle allocated @0x%p.", *OutputHandle);
			break;

		case SQL_HANDLE_STMT: /* Statement Handle */
			*OutputHandle = (SQLHANDLE *)calloc(1, sizeof(esodbc_stmt_st));
			if (! *OutputHandle) {
				ERRN("failed to callocate statement handle.");
				RET_HDIAGS(DBCH(InputHandle), SQL_STATE_HY001); 
			}
			STMH(*OutputHandle)->dbc = DBCH(InputHandle);
			/* "When a statement is allocated, four descriptor handles are
			 * automatically allocated and associated with the statement." */
			STMH(*OutputHandle)->ard = &STMH(*OutputHandle)->i_ard;
			STMH(*OutputHandle)->ird = &STMH(*OutputHandle)->i_ird;
			STMH(*OutputHandle)->apd = &STMH(*OutputHandle)->i_apd;
			STMH(*OutputHandle)->ipd = &STMH(*OutputHandle)->i_ipd;
			DBG("new Statement handle allocated @0x%p.", *OutputHandle);
			break;

		case SQL_HANDLE_DESC:
			//break;
		case SQL_HANDLE_SENV: /* Shared Environment Handle */
			RET_NOT_IMPLEMENTED;
			//break;
#if 0
		case SQL_HANDLE_DBC_INFO_TOKEN:
			//break;
#endif
		default:
			ERR("unknown HandleType: %d.", HandleType);
			return SQL_INVALID_HANDLE;
	}

	RET_STATE(SQL_STATE_00000);
}

SQLRETURN EsSQLFreeHandle(SQLSMALLINT HandleType, SQLHANDLE Handle)
{
	if (! Handle) {
		ERR("provided null Handle.");
		RET_STATE(SQL_STATE_HY009);
	}

	switch(HandleType) {
		case SQL_HANDLE_ENV: /* Environment Handle */
			// TODO: check if there are connections (_DBC)
			free(Handle);
			break;
		case SQL_HANDLE_DBC: /* Connection Handle */
			// TODO: remove from (potential) list?
			free(Handle);
			break;

		case SQL_HANDLE_DESC:
			//break;
		case SQL_HANDLE_STMT:
			//break;
			RET_NOT_IMPLEMENTED;

		case SQL_HANDLE_SENV: /* Shared Environment Handle */
			// TODO: do I need to set the state into the Handle?
			RET_STATE(SQL_STATE_HYC00);
#if 0
		case SQL_HANDLE_DBC_INFO_TOKEN:
			//break;
#endif
		default:
			ERR("unknown HandleType: %d.", HandleType);
			return SQL_INVALID_HANDLE;
	}

	RET_STATE(SQL_STATE_00000);
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
		/* TODO: connection pooling? */
		case SQL_ATTR_CONNECTION_POOLING:
		case SQL_ATTR_CP_MATCH:
			RET_HDIAG(ENVH(EnvironmentHandle), SQL_STATE_HYC00, 
					"Connection pooling not yet supported", 0);

		case SQL_ATTR_ODBC_VERSION:
			switch ((intptr_t)Value) {
				// FIXME: review@alpha
				// supporting applications of 2.x and 3.x<3.8 needs extensive
				// review of the options.
				case SQL_OV_ODBC2:
				case SQL_OV_ODBC3:
				case SQL_OV_ODBC3_80:
					break;
				default:
					INFO("application version %zd not supported.", 
							(intptr_t)Value);
					RET_HDIAG(ENVH(EnvironmentHandle), SQL_STATE_HYC00, 
							"application version not supported", 0);
			}
			ENVH(EnvironmentHandle)->version = (SQLUINTEGER)(uintptr_t)Value;
			DBG("set version to %u.", ENVH(EnvironmentHandle)->version);
			break;

		/* "If SQL_TRUE, the driver returns string data null-terminated" */
		case SQL_ATTR_OUTPUT_NTS:
			if ((intptr_t)Value != SQL_TRUE)
				RET_HDIAG(ENVH(EnvironmentHandle), SQL_STATE_HYC00, 
						"Driver always returns null terminated strings", 0);
			break;

		default:
			ERR("unsupported Attribute value: %d.", Attribute);
			RET_HDIAG(ENVH(EnvironmentHandle), SQL_STATE_HY024, 
					"Unsupported attribute value", 0);
	}

	RET_STATE(SQL_STATE_00000);
}

SQLRETURN SQL_API EsSQLGetEnvAttr(SQLHENV EnvironmentHandle,
		SQLINTEGER Attribute, 
		_Out_writes_(_Inexpressible_(BufferLength)) SQLPOINTER Value,
		SQLINTEGER BufferLength, _Out_opt_ SQLINTEGER *StringLength)
{
	switch (Attribute) {
		/* TODO: connection pooling? */
		case SQL_ATTR_CONNECTION_POOLING:
		case SQL_ATTR_CP_MATCH:
			RET_HDIAG(ENVH(EnvironmentHandle), SQL_STATE_HYC00, 
					"Connection pooling not yet supported", 0);

		case SQL_ATTR_ODBC_VERSION:
			*((SQLUINTEGER *)Value) = ENVH(EnvironmentHandle)->version;
			break;
		case SQL_ATTR_OUTPUT_NTS:
			*((SQLUINTEGER *)Value) = SQL_TRUE;
			break;

		default:
			ERR("unsupported Attribute value: %d.", Attribute);
			RET_HDIAG(ENVH(EnvironmentHandle), SQL_STATE_HY024, 
					"Unsupported attribute value", 0);
	}

	RET_STATE(SQL_STATE_00000);
}


/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/unicode-drivers :
 * """
 * When determining the driver type, the Driver Manager will call
 * SQLSetConnectAttr and set the SQL_ATTR_ANSI_APP attribute at connection
 * time. If the application is using ANSI APIs, SQL_ATTR_ANSI_APP will be set
 * to SQL_AA_TRUE, and if it is using Unicode, it will be set to a value of
 * SQL_AA_FALSE.  If a driver exhibits the same behavior for both ANSI and
 * Unicode applications, it should return SQL_ERROR for this attribute. If the
 * driver returns SQL_SUCCESS, the Driver Manager will separate ANSI and
 * Unicode connections when Connection Pooling is used.
 * """
 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/statement-attributes :
 * """
 * The ability to set statement attributes at the connection level by calling
 * SQLSetConnectAttr has been deprecated in ODBC 3.x. ODBC 3.x applications
 * should never set statement attributes at the connection level. ODBC 3.x
 * drivers need only support this functionality if they should work with ODBC
 * 2.x applications.
 * """
 */
SQLRETURN EsSQLSetConnectAttrW(
		SQLHDBC ConnectionHandle,
		SQLINTEGER Attribute,
		_In_reads_bytes_opt_(StringLength) SQLPOINTER Value,
		SQLINTEGER StringLength)
{
	switch(Attribute) {
		case SQL_ATTR_ANSI_APP:
			/* this driver doesn't behave differently based on app being ANSI
			 * or Unicode. */
			INFO("no ANSI/Unicode specific behaviour (app is: %s).",
					(uintptr_t)Value == SQL_AA_TRUE ? "ANSI" : "Unicode");
			/* TODO: API doesn't require to set a state? */
			//state = SQL_STATE_IM001;
			return SQL_ERROR; /* error means ANSI */

		case SQL_ATTR_LOGIN_TIMEOUT:
			if (DBCH(ConnectionHandle)->conn) {
				ERR("connection already established, can't set connection"
						" timeout (to %u).", (SQLUINTEGER)(uintptr_t)Value);
				RET_HDIAG(DBCH(ConnectionHandle), SQL_STATE_HY011,
						"connection established, can't set connection "
						"timeout.", 0);
			}
			INFO("setting connection timeout to: %u, from previous: %u.", 
					DBCH(ConnectionHandle)->timeout, 
					(SQLUINTEGER)(uintptr_t)Value);
			DBCH(ConnectionHandle)->timeout = (SQLUINTEGER)(uintptr_t)Value;
			break;

		default:
			ERR("unknown Attribute: %d.", Attribute);
			RET_HDIAGS(DBCH(ConnectionHandle), SQL_STATE_HY092);
	}

	RET_STATE(SQL_STATE_00000);
}


SQLRETURN EsSQLGetStmtAttrW(
		SQLHSTMT     StatementHandle,
		SQLINTEGER   Attribute,
		SQLPOINTER   ValuePtr,
		SQLINTEGER   BufferLength,
		SQLINTEGER  *StringLengthPtr)
{
	esodbc_desc_st *desc;

	switch (Attribute) {
		do {
		case SQL_ATTR_APP_ROW_DESC: desc = STMH(StatementHandle)->ard; break;
		case SQL_ATTR_APP_PARAM_DESC: desc = STMH(StatementHandle)->apd; break;
		case SQL_ATTR_IMP_ROW_DESC: desc = STMH(StatementHandle)->ird; break;
		case SQL_ATTR_IMP_PARAM_DESC: desc = STMH(StatementHandle)->ipd; break;
		} while (0);
#if 0
			/* probably not needed, just undocumented behavior this check
			 * trips on. */
			if (BufferLength < 0 || BufferLength < sizeof(SQLPOINTER)) {
				WARN("Not enough buffer space to return result: ""have %d"
						", need: %zd.", BufferLength, sizeof(SQLPOINTER));
				/* bail out, don't just copy half of the pointer size */
				//RET_HDIAGS(STMH(StatementHandle), SQL_STATE_01004);
			}
#endif
			/* what a silly API! */
			*(SQLPOINTER *)ValuePtr = desc;
			break;

		default:
			ERR("unknown attribute: %d.", Attribute);
			RET_HDIAGS(STMH(StatementHandle), SQL_STATE_HY092);
	}

	RET_STATE(SQL_STATE_00000);
}


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
