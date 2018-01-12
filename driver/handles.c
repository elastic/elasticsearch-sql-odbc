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

#include <assert.h>
#include <fcntl.h>

#include "handles.h"
#include "log.h"

#define NO_REC_NR	-1

static void init_desc(esodbc_desc_st *desc, desc_type_et type,
		SQLSMALLINT alloc_type)
{
	memset(desc, 0, sizeof(esodbc_desc_st));

	desc->type = type;
	desc->alloc_type = alloc_type;
	/* a user can only alloc an anon type -> can't have IxD on _USER type */
	assert((alloc_type == SQL_DESC_ALLOC_USER && type == DESC_TYPE_ANON) || 
			(alloc_type == SQL_DESC_ALLOC_AUTO));

	desc->array_size = ESODBC_DEF_ARRAY_SIZE;
}

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
 *
 * "The fields of an IRD have a default value only after the statement has
 * been prepared or executed and the IRD has been populated, not when the
 * statement handle or descriptor has been allocated. Until the IRD has been
 * populated, any attempt to gain access to a field of an IRD will return an
 * error."
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

			init_desc(&STMH(*OutputHandle)->i_ard, DESC_TYPE_ARD, 
					SQL_DESC_ALLOC_AUTO);
			init_desc(&STMH(*OutputHandle)->i_ird, DESC_TYPE_IRD, 
					SQL_DESC_ALLOC_AUTO);
			init_desc(&STMH(*OutputHandle)->i_apd, DESC_TYPE_APD, 
					SQL_DESC_ALLOC_AUTO);
			init_desc(&STMH(*OutputHandle)->i_ipd, DESC_TYPE_IPD, 
					SQL_DESC_ALLOC_AUTO);

			/* "When a statement is allocated, four descriptor handles are
			 * automatically allocated and associated with the statement." */
			STMH(*OutputHandle)->ard = &STMH(*OutputHandle)->i_ard;
			STMH(*OutputHandle)->ird = &STMH(*OutputHandle)->i_ird;
			STMH(*OutputHandle)->apd = &STMH(*OutputHandle)->i_apd;
			STMH(*OutputHandle)->ipd = &STMH(*OutputHandle)->i_ipd;

			STMH(*OutputHandle)->options.bookmarks = SQL_UB_OFF;
			DBG("new Statement handle allocated @0x%p.", *OutputHandle);
			break;

		case SQL_HANDLE_DESC:
			*OutputHandle = (SQLHANDLE *)calloc(1, sizeof(esodbc_desc_st));
			if (! *OutputHandle) {
				ERRN("failed to callocate descriptor handle.");
				RET_HDIAGS(DBCH(InputHandle), SQL_STATE_HY001); 
			}
			init_desc(*OutputHandle, DESC_TYPE_ANON, SQL_DESC_ALLOC_USER);
			DBG("new Descriptor handle allocated @0x%p.", *OutputHandle);
			// FIXME: assign/chain to statement?
			break;

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
		case SQL_HANDLE_STMT:
			// TODO: remove from (potential) list?
			free(Handle);
			break;

		
		/* "When an explicitly allocated descriptor is freed, all statement
		 * handles to which the freed descriptor applied automatically revert
		 * to the descriptors implicitly allocated for them." */
		case SQL_HANDLE_DESC:
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
 * "Calling SQLFreeStmt with the SQL_CLOSE, SQL_UNBIND, or SQL_RESET_PARAMS
 * option does not reset statement attributes."
 *
 * "To unbind all columns, an application calls SQLFreeStmt with fOption set
 * to SQL_UNBIND. This can also be accomplished by setting the SQL_DESC_COUNT
 * field of the ARD to zero.
 * */
SQLRETURN EsSQLFreeStmt(SQLHSTMT StatementHandle, SQLUSMALLINT Option)
{
	switch (Option) {
		/* "deprecated. A call to SQLFreeStmt with an Option of SQL_DROP is
		 * mapped in the Driver Manager to SQLFreeHandle." */
		case SQL_DROP:
			/*TODO: what? if freeing, the app/DM might reuse the handler; if
			 * doing nothing, it might leak mem. */
			WARN("DM deprecated call (drop) -- no action taken!");
			// TODO: do nothing?
			//return SQLFreeStmt(SQL_HANDLE_STMT, (SQLHANDLE)StatementHandle);
			break;

		/* "Closes the cursor associated with StatementHandle and discards all
		 * pending results." */
		case SQL_CLOSE: // TODO: PROTO
		/* "Sets the SQL_DESC_COUNT field of the ARD to 0, releasing all
		 * column buffers bound by SQLBindCol for the given StatementHandle"
		 * */
		case SQL_UNBIND: // TODO: PROTO
		/* "Sets the SQL_DESC_COUNT field of the APD to 0, releasing all
		 * parameter buffers set by SQLBindParameter for the given
		 * StatementHandle." */
		case SQL_RESET_PARAMS: // TODO: PROTO
			BUG("not implemented.");
			// returning success, tho, for further developing 
			break;

		default:
			ERR("unknown Option value: %d.", Option);
			RET_HDIAGS(STMH(StatementHandle), SQL_STATE_HY092);

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

		/* https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/automatic-population-of-the-ipd */
		case SQL_ATTR_AUTO_IPD:
			ERR("trying to set read-only attribute AUTO IPD.");
			RET_HDIAGS(DBCH(ConnectionHandle), SQL_STATE_HY092);
		case SQL_ATTR_ENABLE_AUTO_IPD:
			if (*(SQLUINTEGER *)Value != SQL_FALSE) {
				ERR("trying to enable unsupported attribute AUTO IPD.");
				RET_HDIAGS(DBCH(ConnectionHandle), SQL_STATE_HYC00);
			}
			WARN("disabling (unsupported) attribute AUTO IPD -- NOOP.");
			break;

		default:
			ERR("unknown Attribute: %d.", Attribute);
			RET_HDIAGS(DBCH(ConnectionHandle), SQL_STATE_HY092);
	}

	RET_STATE(SQL_STATE_00000);
}


SQLRETURN EsSQLGetConnectAttrW(
		SQLHDBC        ConnectionHandle,
		SQLINTEGER     Attribute,
		_Out_writes_opt_(_Inexpressible_(cbValueMax)) SQLPOINTER ValuePtr,
		SQLINTEGER     BufferLength,
		_Out_opt_ SQLINTEGER* StringLengthPtr)
{
	switch(Attribute) {
		/* https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/automatic-population-of-the-ipd */
		case SQL_ATTR_AUTO_IPD:
			DBG("requested: support for attribute AUTO IPD (false).");
			/* "Servers that do not support prepared statements will not be
			 * able to populate the IPD automatically." */
			*(SQLUINTEGER *)ValuePtr = SQL_FALSE;
			break;

		default:
			// FIXME: add the other attributes
			FIXME;
			ERR("unknown Attribute type %d.", Attribute);
			RET_HDIAGS(DBCH(ConnectionHandle), SQL_STATE_HY092);
	}
	
	RET_STATE(SQL_STATE_00000);
}


SQLRETURN EsSQLSetStmtAttrW(
		SQLHSTMT           StatementHandle,
		SQLINTEGER         Attribute,
		SQLPOINTER         ValuePtr,
		SQLINTEGER         BufferLength)
{
	SQLRETURN ret;
	esodbc_desc_st *desc;

	switch(Attribute) {
		case SQL_ATTR_USE_BOOKMARKS:
			DBG("setting use-bookmarks to: %u.", *(SQLULEN *)ValuePtr);
			if (*(SQLULEN *)ValuePtr != SQL_UB_OFF) {
				WARN("bookmarks are not supported by driver.");
				RET_HDIAG(STMH(StatementHandle), SQL_STATE_01000,
						"bookmarks are not supported by driver", 0);
			}
			break;

		/* "If this field is non-null, the driver dereferences the pointer,
		 * adds the dereferenced value to each of the deferred fields in the
		 * descriptor record (SQL_DESC_DATA_PTR, SQL_DESC_INDICATOR_PTR, and
		 * SQL_DESC_OCTET_LENGTH_PTR), and uses the new pointer values when
		 * binding. It is set to null by default." */
		case SQL_ATTR_ROW_BIND_OFFSET_PTR:
			/* offset in bytes */
			/* "Setting this statement attribute sets the
			 * SQL_DESC_BIND_OFFSET_PTR field in the ARD header." */
			// TODO: call SQLSetDescField(ARD) here?
			DBG("setting row-bind-offset pointer to: 0x%p.", ValuePtr);
			STMH(StatementHandle)->options.bind_offset = (SQLULEN *)ValuePtr;
			break;

		do {
		/* "Setting this statement attribute sets the SQL_DESC_ARRAY_SIZE
		 * field in the ARD header." */
		case SQL_ATTR_ROW_ARRAY_SIZE:
			DBG("setting row array size to: %d.", *(SQLULEN *)ValuePtr);
			desc = STMH(StatementHandle)->ard;
			break;
		/* "Setting this statement attribute sets the SQL_DESC_ARRAY_SIZE
		 * field in the APD header." */
		case SQL_ATTR_PARAMSET_SIZE:
			DBG("setting param set size to: %d.", *(SQLULEN *)ValuePtr);
			desc = STMH(StatementHandle)->apd;
			break;
		} while (0);
			ret = EsSQLSetDescFieldW(desc, NO_REC_NR, SQL_DESC_ARRAY_SIZE,
					ValuePtr, BufferLength);
			if (ret != SQL_SUCCESS) /* _WITH_INFO wud be "error" here */
				/* if SetDescField() fails, DM will check statement's diag */
				HDIAG_COPY(STMH(StatementHandle)->ard, STMH(StatementHandle));
			return ret;

		/* "sets the binding orientation to be used when SQLFetch or
		 * SQLFetchScroll is called on the associated statement" */
		/* "Setting this statement attribute sets the SQL_DESC_BIND_TYPE field
		 * in the ARD header." */
		case SQL_ATTR_ROW_BIND_TYPE:
			// TODO: call SQLSetDescField(ARD) here?
			DBG("setting row bind type to: %d.", *(SQLULEN *)ValuePtr);
			/* value is SQL_BIND_BY_COLUMN (0UL) or struct len  */
			/* "the driver can calculate the address of the data for a
			 * particular row and column as:
			 * Address = Bound Address + ((Row Number - 1) * Structure Size)"
			 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/column-wise-binding
			 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/row-wise-binding
			 */
			STMH(StatementHandle)->options.bind_type = *(SQLULEN *)ValuePtr;
			break;

		/* "an array of SQLUSMALLINT values containing row status values after
		 * a call to SQLFetch or SQLFetchScroll." */
		/* https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/row-status-array */
		/* "Setting this statement attribute sets the
		 * SQL_DESC_ARRAY_STATUS_PTR field in the IRD header." */
		case SQL_ATTR_ROW_STATUS_PTR:
			// TODO: call SQLSetDescField(IRD) here?
			DBG("setting row status pointer to: 0x%p.", ValuePtr);
			STMH(StatementHandle)->options.row_status = 
				(SQLUSMALLINT *)ValuePtr;
			break;

		/* "Setting this statement attribute sets the
		 * SQL_DESC_ROWS_PROCESSED_PTR field in the IRD header." */
		case SQL_ATTR_ROWS_FETCHED_PTR:
			// TODO: call SQLSetDescField(IRD) here?
			DBG("setting rows fetched pointer to: 0x%p.", ValuePtr);
			STMH(StatementHandle)->options.rows_fetched = (SQLULEN *)ValuePtr;
			break;

		case SQL_ATTR_APP_ROW_DESC:
			if ((ValuePtr == (SQLPOINTER *)&STMH(StatementHandle)->i_ard) || 
					(ValuePtr == SQL_NULL_HDESC)) {
				if (STMH(StatementHandle)->ard) {
					DBG("unbinding ARD 0x%p from statement 0x%p.");
					// FIXME: unbind
					FIXME;
				}
				STMH(StatementHandle)->ard = &STMH(StatementHandle)->i_ard;
				FIXME;
			} else if (FALSE) {
				/* "This attribute cannot be set to a descriptor handle that
				 * was implicitly allocated for another statement or to
				 * another descriptor handle that was implicitly set on the
				 * same statement; implicitly allocated descriptor handles
				 * cannot be associated with more than one statement or
				 * descriptor handle." */
				/* TODO: check if this is implicitely allocated in all
				statements??? */
				ERR("trying to set AxD (%d) descriptor to the wrong implicit"
						" descriptor @0x%p.", Attribute, ValuePtr);
				RET_HDIAGS(STMH(StatementHandle), SQL_STATE_HY017);
			} else {
				STMH(StatementHandle)->ard = (esodbc_desc_st *)ValuePtr;
				// FIXME: bind: re-init
				FIXME;
			}	
		case SQL_ATTR_APP_PARAM_DESC:
			// FIXME: same logic for APD as above
			FIXME;
			break;

		case SQL_ATTR_IMP_ROW_DESC:
		case SQL_ATTR_IMP_PARAM_DESC:
			ERR("trying to set IxD (%d) descriptor (to @0x%p).", Attribute, 
					ValuePtr);
			RET_HDIAGS(STMH(StatementHandle), SQL_STATE_HY017);

		default:
			// FIXME
			FIXME;
			ERR("unknown Attribute: %d.", Attribute);
			RET_HDIAGS(STMH(StatementHandle), SQL_STATE_HY092);
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




/*
 * Access permission matrix (TSV) lifted and rearanged from:
 * https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetdescfield-function#fieldidentifier-argument
 *
 * Name		ARD	APD	IRD	IPD
 * SQL_DESC_ALLOC_TYPE		r	r	r	r
 * SQL_DESC_ARRAY_STATUS_PTR		rw	rw	rw	rw
 * SQL_DESC_ROWS_PROCESSED_PTR				rw	rw
 * SQL_DESC_PARAMETER_TYPE					rw
 * SQL_DESC_ARRAY_SIZE		rw	rw		
 * SQL_DESC_BIND_OFFSET_PTR		rw	rw		
 * SQL_DESC_BIND_TYPE		rw	rw		
 * SQL_DESC_DATA_PTR		rw	rw		
 * SQL_DESC_INDICATOR_PTR		rw	rw		
 * SQL_DESC_OCTET_LENGTH_PTR		rw	rw		
 * SQL_DESC_AUTO_UNIQUE_VALUE				r	
 * SQL_DESC_BASE_COLUMN_NAME				r	
 * SQL_DESC_BASE_TABLE_NAME				r	
 * SQL_DESC_DISPLAY_SIZE				r	
 * SQL_DESC_CATALOG_NAME				r	
 * SQL_DESC_LABEL				r	
 * SQL_DESC_LITERAL_PREFIX				r	
 * SQL_DESC_LITERAL_SUFFIX				r	
 * SQL_DESC_SCHEMA_NAME				r	
 * SQL_DESC_SEARCHABLE				r	
 * SQL_DESC_TABLE_NAME				r	
 * SQL_DESC_UPDATABLE				r	
 * SQL_DESC_CASE_SENSITIVE				r	r
 * SQL_DESC_FIXED_PREC_SCALE				r	r
 * SQL_DESC_LOCAL_TYPE_NAME				r	r
 * SQL_DESC_NULLABLE				r	r
 * SQL_DESC_ROWVER				r	r
 * SQL_DESC_UNSIGNED				r	r
 * SQL_DESC_TYPE_NAME				r	r
 * SQL_DESC_NAME				r	rw
 * SQL_DESC_UNNAMED				r	rw
 * SQL_DESC_COUNT		rw	rw	r	rw
 * SQL_DESC_CONCISE_TYPE		rw	rw	r	rw
 * SQL_DESC_DATETIME_INTERVAL_CODE		rw	rw	r	rw
 * SQL_DESC_DATETIME_INTERVAL_PRECISION		rw	rw	r	rw
 * SQL_DESC_LENGTH		rw	rw	r	rw
 * SQL_DESC_NUM_PREC_RADIX		rw	rw	r	rw
 * SQL_DESC_OCTET_LENGTH		rw	rw	r	rw
 * SQL_DESC_PRECISION		rw	rw	r	rw
 * SQL_DESC_SCALE		rw	rw	r	rw
 * SQL_DESC_TYPE		rw	rw	r	rw
 */
// TODO: individual tests just for this
static BOOL check_access(desc_type_et desc_type, SQLSMALLINT field_id, 
		char mode /* O_RDONLY | O_RDWR */)
{
	BOOL ret;

	if (desc_type == DESC_TYPE_ANON) {
		BUG("can't check permissions against ANON descryptor type.");
		return FALSE;
	}
	assert(mode == O_RDONLY || mode == O_RDWR);

	switch (field_id) {
		case SQL_DESC_ALLOC_TYPE:
			ret = mode == O_RDONLY; 
			break;
		case SQL_DESC_ARRAY_STATUS_PTR:
			ret = TRUE;
			break;
		case SQL_DESC_ROWS_PROCESSED_PTR:
			ret = desc_type == DESC_TYPE_IRD || desc_type == DESC_TYPE_IPD;
			break;
		case SQL_DESC_PARAMETER_TYPE:
			ret = desc_type == DESC_TYPE_IPD;
			break;

		case SQL_DESC_ARRAY_SIZE:
		case SQL_DESC_BIND_OFFSET_PTR:
		case SQL_DESC_BIND_TYPE:
		case SQL_DESC_DATA_PTR:
		case SQL_DESC_INDICATOR_PTR:
		case SQL_DESC_OCTET_LENGTH_PTR:
			ret = desc_type == DESC_TYPE_ARD || desc_type == DESC_TYPE_APD;
			break;

		case SQL_DESC_AUTO_UNIQUE_VALUE:
		case SQL_DESC_BASE_COLUMN_NAME:
		case SQL_DESC_BASE_TABLE_NAME:
		case SQL_DESC_DISPLAY_SIZE:
		case SQL_DESC_CATALOG_NAME:
		case SQL_DESC_LABEL:
		case SQL_DESC_LITERAL_PREFIX:
		case SQL_DESC_LITERAL_SUFFIX:
		case SQL_DESC_SCHEMA_NAME:
		case SQL_DESC_SEARCHABLE:
		case SQL_DESC_TABLE_NAME:
		case SQL_DESC_UPDATABLE:
			ret = desc_type == DESC_TYPE_ARD && mode == O_RDONLY;
			break;

		case SQL_DESC_CASE_SENSITIVE:
		case SQL_DESC_FIXED_PREC_SCALE:
		case SQL_DESC_LOCAL_TYPE_NAME:
		case SQL_DESC_NULLABLE:
		case SQL_DESC_ROWVER:
		case SQL_DESC_UNSIGNED:
		case SQL_DESC_TYPE_NAME:
			ret = mode == O_RDONLY && 
				(desc_type == DESC_TYPE_IRD || desc_type == DESC_TYPE_IPD);
			break;

		case SQL_DESC_NAME:
		case SQL_DESC_UNNAMED:
			ret = desc_type == DESC_TYPE_IPD || 
				(desc_type == DESC_TYPE_IPD && mode == O_RDONLY);
			break;

		case SQL_DESC_COUNT:
		case SQL_DESC_CONCISE_TYPE:
		case SQL_DESC_DATETIME_INTERVAL_CODE:
		case SQL_DESC_DATETIME_INTERVAL_PRECISION:
		case SQL_DESC_LENGTH:
		case SQL_DESC_NUM_PREC_RADIX:
		case SQL_DESC_OCTET_LENGTH:
		case SQL_DESC_PRECISION:
		case SQL_DESC_SCALE:
		case SQL_DESC_TYPE:
			switch (desc_type) {
				case DESC_TYPE_ARD:
				case DESC_TYPE_APD:
				case DESC_TYPE_IPD:
					ret = TRUE;
					break;
				case DESC_TYPE_IRD:
					ret = mode == O_RDONLY;
					break;
				//default: ANON mode checked above;
			}
			break;

		default:
			BUG("unknown field identifier: %d.", field_id);
			ret = FALSE;
	}
	LOG(ret ? LOG_LEVEL_DBG : LOG_LEVEL_ERR,
			"Descriptor type: %d, Field ID: %d, mode=%s => grant: %s.", 
			desc_type, field_id, 
			mode == O_RDONLY ? "read" : "read/write",
			ret ? "OK" : "NOT");
	return ret;
}

/*
 * "Even when freed, an implicitly allocated descriptor remains valid, and
 * SQLGetDescField can be called on its fields."
 *
 * "In a subsequent call to SQLGetDescField or SQLGetDescRec, the driver is
 * not required to return the value that SQL_DESC_DATA_PTR was set to."
 */
SQLRETURN EsSQLGetDescFieldW(
		SQLHDESC        DescriptorHandle,
		SQLSMALLINT     RecNumber,
		SQLSMALLINT     FieldIdentifier,
		_Out_writes_opt_(_Inexpressible_(BufferLength))
		SQLPOINTER      ValuePtr,
		SQLINTEGER      BufferLength,
		SQLINTEGER      *StringLengthPtr)
{
	RET_NOT_IMPLEMENTED;
}

/*
 * "The fields of an IRD have a default value only after the statement has
 * been prepared or executed and the IRD has been populated, not when the
 * statement handle or descriptor has been allocated. Until the IRD has been
 * populated, any attempt to gain access to a field of an IRD will return an
 * error."
 *
 * "In a subsequent call to SQLGetDescField or SQLGetDescRec, the driver is
 * not required to return the value that SQL_DESC_DATA_PTR was set to."
 */
SQLRETURN EsSQLGetDescRecW(
		SQLHDESC        DescriptorHandle,
		SQLSMALLINT     RecNumber,
		_Out_writes_opt_(BufferLength)
		SQLWCHAR        *Name,
		_Out_opt_ 
		SQLSMALLINT     BufferLength,
		_Out_opt_ 
		SQLSMALLINT     *StringLengthPtr,
		_Out_opt_ 
		SQLSMALLINT     *TypePtr,
		_Out_opt_ 
		SQLSMALLINT     *SubTypePtr,
		_Out_opt_ 
		SQLLEN          *LengthPtr,
		_Out_opt_ 
		SQLSMALLINT     *PrecisionPtr,
		_Out_opt_ 
		SQLSMALLINT     *ScalePtr,
		_Out_opt_ 
		SQLSMALLINT     *NullablePtr)
{
	RET_NOT_IMPLEMENTED;
}

/*
 * "If an application calls SQLSetDescField to set any field other than
 * SQL_DESC_COUNT or the deferred fields SQL_DESC_DATA_PTR,
 * SQL_DESC_OCTET_LENGTH_PTR, or SQL_DESC_INDICATOR_PTR, the record becomes
 * unbound."
 *
 * "When SQLSetDescField is called to set a header field, the RecNumber
 * argument is ignored."
 *
 * The "driver sets data type attribute fields to the appropriate default
 * values for the data type" (once either of SQL_DESC_TYPE,
 * SQL_DESC_CONCISE_TYPE, or SQL_DESC_DATETIME_INTERVAL_CODE are set).
 *
 * The "application can set SQL_DESC_DATA_PTR. This prompts a consistency
 * check of descriptor fields."
 *
 * "If the application changes the data type or attributes after setting the
 * SQL_DESC_DATA_PTR field, the driver sets SQL_DESC_DATA_PTR to a null
 * pointer, unbinding the record."
 */
SQLRETURN EsSQLSetDescFieldW(
		SQLHDESC        DescriptorHandle,
		SQLSMALLINT     RecNumber,
		SQLSMALLINT     FieldIdentifier,
		SQLPOINTER      Value,
		SQLINTEGER      BufferLength)
{

	if (! check_access(DSCH(DescriptorHandle)->type, FieldIdentifier, O_RDWR))
		RET_HDIAGS(DSCH(DescriptorHandle), SQL_STATE_HY091);

	switch(FieldIdentifier) {
		case SQL_DESC_ARRAY_SIZE:
			if (ESODBC_MAX_ROW_ARRAY_SIZE < *(SQLULEN *)Value) {
				DSCH(DescriptorHandle)->array_size = ESODBC_MAX_ROW_ARRAY_SIZE;
				/* TODO: return the fixed size in Value?? */
				RET_HDIAGS(DSCH(DescriptorHandle), SQL_STATE_01S02);
			} else {
				DSCH(DescriptorHandle)->array_size = *(SQLULEN *)Value;
			}
			break;

		default:
			// FIXME
			FIXME;
			ERR("unknown FieldIdentifier: %d.", FieldIdentifier);
			RET_HDIAGS(DSCH(DescriptorHandle), SQL_STATE_HY092);
	}
	
	RET_STATE(SQL_STATE_00000);
}


/*
 * "When the application sets the SQL_DESC_TYPE field, the driver checks that
 * other fields that specify the type are valid and consistent." AND:
 *
 * "A consistency check is performed by the driver automatically whenever an
 * application sets the SQL_DESC_DATA_PTR field of the APD, ARD, or IPD.
 * Whenever this field is set, the driver checks that the value of the
 * SQL_DESC_TYPE field and the values applicable to the SQL_DESC_TYPE field in
 * the same record are valid and consistent.
 *
 * The SQL_DESC_DATA_PTR field of an IPD is not normally set; however, an
 * application can do so to force a consistency check of IPD fields. The value
 * that the SQL_DESC_DATA_PTR field of the IPD is set to is not actually
 * stored and cannot be retrieved by a call to SQLGetDescField or
 * SQLGetDescRec; the setting is made only to force the consistency check. A
 * consistency check cannot be performed on an IRD."
 */
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
		_Inout_opt_ SQLLEN *Indicator)
{
	/*
	 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/column-wise-binding :
	 * "When using column-wise binding, an application binds one or two, or in
	 * some cases three, arrays to each column for which data is to be
	 * returned. The first array holds the data values, and the second array
	 * holds length/indicator buffers. Indicators and length values can be
	 * stored in separate buffers by setting the SQL_DESC_INDICATOR_PTR and
	 * SQL_DESC_OCTET_LENGTH_PTR descriptor fields to different values; if
	 * this is done, a third array is bound. Each array contains as many
	 * elements as there are rows in the rowset."
	 */

	RET_NOT_IMPLEMENTED;
}


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
