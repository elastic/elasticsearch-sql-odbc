/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <assert.h>
#include <fcntl.h>

#include "handles.h"
#include "log.h"
#include "queries.h"
#include "connect.h"


static void free_rec_fields(desc_rec_st *rec)
{
	int i;
	SQLWCHAR **wptr[] = {
		&rec->base_column_name,
		&rec->base_table_name,
		&rec->catalog_name,
		&rec->label,
		&rec->literal_prefix,
		&rec->literal_suffix,
		&rec->local_type_name,
		&rec->name,
		&rec->schema_name,
		&rec->table_name,
		&rec->type_name,
	};
	for (i = 0; i < sizeof(wptr)/sizeof(wptr[0]); i ++) {
		DBG("freeing field #%d = 0x%p.", i, *wptr[i]);
		if (*wptr[i]) {
			free(*wptr[i]);
			*wptr[i] = NULL;
		}
	}
}

static inline void free_desc_recs(esodbc_desc_st *desc)
{
	int i;
	/* IRD's pointers are 0-copy, no need to free them */
	if (desc->type != DESC_TYPE_IRD)
		for (i = 0; i < desc->count; i++)
			free_rec_fields(&desc->recs[i]);
	free(desc->recs);
	desc->recs = NULL;
	desc->count = 0;
}


void init_desc(esodbc_desc_st *desc, esodbc_stmt_st *stmt, desc_type_et type, 
		SQLSMALLINT alloc_type)
{
	memset(desc, 0, sizeof(esodbc_desc_st));

	desc->stmt = stmt;
	init_diagnostic(&desc->diag);

	desc->type = type;
	desc->alloc_type = alloc_type;
	/* a user can only alloc an anon type -> can't have IxD on _USER type */
	assert((alloc_type == SQL_DESC_ALLOC_USER && type == DESC_TYPE_ANON) || 
			(alloc_type == SQL_DESC_ALLOC_AUTO));

	desc->array_size = ESODBC_DEF_ARRAY_SIZE;
	// desc->array_status_ptr = NULL; /* formal, 0'd above */
	// desc->bind_offset_ptr = NULL; /* formal, 0'd above */
	if (type == DESC_TYPE_ARD || type == DESC_TYPE_APD)
		desc->bind_type = SQL_BIND_BY_COLUMN;
	// desc->count = 0; /* formal, 0'd above */
	// desc->rows_processed_ptr = NULL; /* formal, 0'd above */
}

static void clear_desc(esodbc_stmt_st *stmt, desc_type_et dtype, BOOL reinit)
{
	esodbc_desc_st *desc;
	DBG("clearing desc type %d for statement 0x%p.", dtype, stmt);
	switch (dtype) {
		case DESC_TYPE_ARD:
			desc = stmt->ard;
			break;
		case DESC_TYPE_IRD:
			if (STMT_HAS_RESULTSET(stmt))
				clear_resultset(stmt);
			desc = stmt->ird;
			break;
		case DESC_TYPE_APD:
			desc = stmt->apd;
			break;
		case DESC_TYPE_IPD:
			desc = stmt->ipd;
			break;
		default:
			BUG("no such descriptor of type %d.", dtype);
			return;
	}
	if (desc->recs)
		free_desc_recs(desc);
	if (reinit)
		init_desc(desc, desc->stmt, desc->type, desc->alloc_type);
}

void dump_record(desc_rec_st *rec)
{
	DBG("Dumping REC@0x%p", rec);
#define DUMP_FIELD(_strp, _name, _desc) \
	DBG("0x%p->%s: `" _desc "`.", _strp, # _name, (_strp)->_name)

	DUMP_FIELD(rec, desc, "0x%p");
	DUMP_FIELD(rec, meta_type, "%d");
	
	DUMP_FIELD(rec, concise_type, "%d");
	DUMP_FIELD(rec, type, "%d");
	DUMP_FIELD(rec, datetime_interval_code, "%d");
	
	DUMP_FIELD(rec, data_ptr, "0x%p");
	
	DUMP_FIELD(rec, base_column_name, LWPD);
	DUMP_FIELD(rec, base_table_name, LWPD);
	DUMP_FIELD(rec, catalog_name, LWPD);
	DUMP_FIELD(rec, label, LWPD);
	DUMP_FIELD(rec, literal_prefix, LWPD);
	DUMP_FIELD(rec, literal_suffix, LWPD);
	DUMP_FIELD(rec, local_type_name, LWPD);
	DUMP_FIELD(rec, name, LWPD);
	DUMP_FIELD(rec, schema_name, LWPD);
	DUMP_FIELD(rec, table_name, LWPD);
	DUMP_FIELD(rec, type_name, LWPD);

	DUMP_FIELD(rec, indicator_ptr, "0x%p");
	DUMP_FIELD(rec, octet_length_ptr, "0x%p");
	
	DUMP_FIELD(rec, display_size, "%lld");
	DUMP_FIELD(rec, octet_length, "%lld");
	
	DUMP_FIELD(rec, length, "%llu");
	
	DUMP_FIELD(rec, auto_unique_value, "%d");
	DUMP_FIELD(rec, case_sensitive, "%d");
	DUMP_FIELD(rec, datetime_interval_precision, "%d");
	DUMP_FIELD(rec, num_prec_radix, "%d");
	
	DUMP_FIELD(rec, fixed_prec_scale, "%d");
	DUMP_FIELD(rec, nullable, "%d");
	DUMP_FIELD(rec, parameter_type, "%d");
	DUMP_FIELD(rec, precision, "%d");
	DUMP_FIELD(rec, rowver, "%d");
	DUMP_FIELD(rec, scale, "%d");
	DUMP_FIELD(rec, searchable, "%d");
	DUMP_FIELD(rec, unnamed, "%d");
	DUMP_FIELD(rec, usigned, "%d");
	DUMP_FIELD(rec, updatable, "%d");
#undef DUMP_FIELD
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
	esodbc_dbc_st *dbc;
	esodbc_stmt_st *stmt;

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
			init_diagnostic(&ENVH(*OutputHandle)->diag);
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
			dbc = (esodbc_dbc_st *)calloc(1, sizeof(esodbc_dbc_st));
			*OutputHandle = (SQLHANDLE *)dbc;
			if (! dbc) {
				ERRN("failed to callocate connection handle.");
				RET_HDIAGS(ENVH(InputHandle), SQL_STATE_HY001);
			}
			init_diagnostic(&dbc->diag);
			dbc->dsn.str = MK_WPTR(""); /* see explanation in cleanup_dbc() */
			dbc->metadata_id = SQL_FALSE;
			dbc->async_enable = SQL_ASYNC_ENABLE_OFF;
			dbc->txn_isolation = ESODBC_DEF_TXN_ISOLATION;

			dbc->env = ENVH(InputHandle);
			/* rest of initialization done at connect time */

			DBG("new Connection handle allocated @0x%p.", *OutputHandle);
			break;

		case SQL_HANDLE_STMT: /* Statement Handle */
			dbc = DBCH(InputHandle);
			stmt = (esodbc_stmt_st *)calloc(1, sizeof(esodbc_stmt_st));
			*OutputHandle = stmt;
			if (! stmt) {
				ERRN("failed to callocate statement handle.");
				RET_HDIAGS(dbc, SQL_STATE_HY001); 
			}
			init_diagnostic(&stmt->diag);
			stmt->dbc = dbc;

			init_desc(&stmt->i_ard, stmt, DESC_TYPE_ARD, SQL_DESC_ALLOC_AUTO);
			init_desc(&stmt->i_ird, stmt, DESC_TYPE_IRD, SQL_DESC_ALLOC_AUTO);
			init_desc(&stmt->i_apd, stmt, DESC_TYPE_APD, SQL_DESC_ALLOC_AUTO);
			init_desc(&stmt->i_ipd, stmt, DESC_TYPE_IPD, SQL_DESC_ALLOC_AUTO);

			/* "When a statement is allocated, four descriptor handles are
			 * automatically allocated and associated with the statement." */
			stmt->ard = &stmt->i_ard;
			stmt->ird = &stmt->i_ird;
			stmt->apd = &stmt->i_apd;
			stmt->ipd = &stmt->i_ipd;

			/* set option defaults */
			stmt->bookmarks = SQL_UB_OFF;
			/* inherit this connection-statement attributes
			 * Note: these attributes won't propagate at statement level when
			 * set at connection level. */
			stmt->metadata_id = dbc->metadata_id;
			stmt->async_enable = dbc->async_enable;
			DBG("new Statement handle allocated @0x%p.", *OutputHandle);
			break;

		case SQL_HANDLE_DESC:
			*OutputHandle = (SQLHANDLE *)calloc(1, sizeof(esodbc_desc_st));
			if (! *OutputHandle) {
				ERRN("failed to callocate descriptor handle.");
				RET_HDIAGS(STMH(InputHandle), SQL_STATE_HY001); 
			}
			init_desc(*OutputHandle, InputHandle, DESC_TYPE_ANON, 
					SQL_DESC_ALLOC_USER);
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
	esodbc_stmt_st *stmt;

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
			/* app/DM should have SQLDisconnect'ed, but just in case  */
			cleanup_dbc(DBCH(Handle));
			free(Handle);
			break;
		case SQL_HANDLE_STMT:
			// TODO: remove from (potential) list?
			stmt = STMH(Handle);
			detach_sql(stmt);
			clear_desc(stmt, DESC_TYPE_ARD, FALSE);
			clear_desc(stmt, DESC_TYPE_IRD, FALSE);
			clear_desc(stmt, DESC_TYPE_APD, FALSE);
			clear_desc(stmt, DESC_TYPE_IPD, FALSE);
			free(stmt);
			break;

		
		/* "When an explicitly allocated descriptor is freed, all statement
		 * handles to which the freed descriptor applied automatically revert
		 * to the descriptors implicitly allocated for them." */
		case SQL_HANDLE_DESC:
			//break;
			// FIXME
			FIXME;

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
 *
 * "If all columns are unbound by calling SQLFreeStmt with the SQL_UNBIND
 * option, the SQL_DESC_COUNT fields in the ARD and IRD are set to 0. If
 * SQLFreeStmt is called with the SQL_RESET_PARAMS option, the SQL_DESC_COUNT
 * fields in the APD and IPD are set to 0." (.count)
 * */
SQLRETURN EsSQLFreeStmt(SQLHSTMT StatementHandle, SQLUSMALLINT Option)
{
	SQLRETURN ret;
	esodbc_stmt_st *stmt = STMH(StatementHandle);

	switch (Option) {
		/* "deprecated. A call to SQLFreeStmt with an Option of SQL_DROP is
		 * mapped in the Driver Manager to SQLFreeHandle." */
		case SQL_DROP:
			/*TODO: if freeing, the app/DM might reuse the handler; if
			 * doing nothing, it might leak mem. */
			ERR("STMT@0x%p: DROPing is deprecated -- no action taken! "
					"(might leak memory)", stmt);
			//return SQLFreeStmt(SQL_HANDLE_STMT, (SQLHANDLE)StatementHandle);
			break;

		/* "This does not unbind the bookmark column; to do that, the
		 * SQL_DESC_DATA_PTR field of the ARD for the bookmark column is set
		 * to NULL." TODO, with bookmarks. */
		case SQL_UNBIND:
			DBG("STMT@0x%p unbinding.", stmt);
			/* "Sets the SQL_DESC_COUNT field of the ARD to 0, releasing all
			 * column buffers bound by SQLBindCol for the given
			 * StatementHandle." */
			ret = EsSQLSetDescFieldW(stmt->ard, NO_REC_NR, SQL_DESC_COUNT,
					(SQLPOINTER)0, SQL_IS_SMALLINT);
			if (ret != SQL_SUCCESS) {
				/* copy error at top handle level, where it's going to be
				 * inquired from */
				HDIAG_COPY(stmt->ard, stmt);
				return ret;
			}
			break;

		case ESODBC_SQL_CLOSE:
			DBG("STMT@0x%p: ES-closing.", stmt);
			detach_sql(stmt);
			/* no break */
		/* "Closes the cursor associated with StatementHandle and discards all
		 * pending results." */
		case SQL_CLOSE:
			DBG("STMT@0x%p: closing.", stmt);
			clear_desc(stmt, DESC_TYPE_IRD, FALSE /*keep the header values*/);
			break;

		/* "Sets the SQL_DESC_COUNT field of the APD to 0, releasing all
		 * parameter buffers set by SQLBindParameter for the given
		 * StatementHandle." */
		case SQL_RESET_PARAMS:
			DBG("STMT@0x%p: resetting params.", stmt);
			ret = EsSQLSetDescFieldW(stmt->apd, NO_REC_NR, SQL_DESC_COUNT,
					(SQLPOINTER)0, SQL_IS_SMALLINT);
			if (ret != SQL_SUCCESS) {
				/* copy error at top handle level, where it's going to be
				 * inquired from */
				HDIAG_COPY(stmt->ard, stmt);
				return ret;
			}
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




SQLRETURN EsSQLSetStmtAttrW(
		SQLHSTMT           StatementHandle,
		SQLINTEGER         Attribute,
		SQLPOINTER         ValuePtr,
		SQLINTEGER         BufferLength)
{
	SQLRETURN ret;
	SQLULEN ulen;
	esodbc_desc_st *desc;
	esodbc_stmt_st *stmt = STMH(StatementHandle);

	switch(Attribute) {
		case SQL_ATTR_USE_BOOKMARKS:
			DBG("setting use-bookmarks to: %u.", (SQLULEN)ValuePtr);
			if ((SQLULEN)ValuePtr != SQL_UB_OFF) {
				WARN("bookmarks are not supported by driver.");
				RET_HDIAG(stmt, SQL_STATE_01000,
						"bookmarks are not supported by driver", 0);
			}
			break;

		do {
		/* "If this field is non-null, the driver dereferences the pointer,
		 * adds the dereferenced value to each of the deferred fields in the
		 * descriptor record (SQL_DESC_DATA_PTR, SQL_DESC_INDICATOR_PTR, and
		 * SQL_DESC_OCTET_LENGTH_PTR), and uses the new pointer values when
		 * binding. It is set to null by default." */
		case SQL_ATTR_ROW_BIND_OFFSET_PTR:
			/* offset in bytes */
			/* "Setting this statement attribute sets the
			 * SQL_DESC_BIND_OFFSET_PTR field in the ARD header." */
			DBG("setting row-bind-offset pointer to: 0x%p.", ValuePtr);
			desc = stmt->ard;
			break;
		case SQL_ATTR_PARAM_BIND_OFFSET_PTR:
			DBG("setting param-bind-offset pointer to: 0x%p.", ValuePtr);
			desc = stmt->apd;
			break;
		} while (0);
			ret = EsSQLSetDescFieldW(desc, NO_REC_NR, SQL_DESC_BIND_OFFSET_PTR,
					ValuePtr, BufferLength);
			if (ret != SQL_SUCCESS) /* _WITH_INFO wud be "error" here */
				/* if SetDescField() fails, DM will check statement's diag */
				HDIAG_COPY(desc, stmt);
			return ret;

		do {
		/* "Setting this statement attribute sets the SQL_DESC_ARRAY_SIZE
		 * field in the ARD header." */
		case SQL_ATTR_ROW_ARRAY_SIZE:
			DBG("setting row array size to: %d.", (SQLULEN)ValuePtr);
			desc = stmt->ard;
			break;
		/* "Setting this statement attribute sets the SQL_DESC_ARRAY_SIZE
		 * field in the APD header." */
		case SQL_ATTR_PARAMSET_SIZE:
			DBG("setting param set size to: %d.", (SQLULEN)ValuePtr);
			desc = stmt->apd;
			break;
		} while (0);
			ret = EsSQLSetDescFieldW(desc, NO_REC_NR, SQL_DESC_ARRAY_SIZE,
					ValuePtr, BufferLength);
			if (ret != SQL_SUCCESS) /* _WITH_INFO wud be "error" here */
				/* if SetDescField() fails, DM will check statement's diag */
				HDIAG_COPY(desc, stmt);
			return ret;

		do {
		/* "sets the binding orientation to be used when SQLFetch or
		 * SQLFetchScroll is called on the associated statement" */
		/* "Setting this statement attribute sets the SQL_DESC_BIND_TYPE field
		 * in the ARD header." */
		case SQL_ATTR_ROW_BIND_TYPE:
			DBG("setting row bind type to: %d.", (SQLULEN)ValuePtr);
			/* value is SQL_BIND_BY_COLUMN (0UL) or struct len  */
			/* "the driver can calculate the address of the data for a
			 * particular row and column as:
			 * Address = Bound Address + ((Row Number - 1) * Structure Size)"
			 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/column-wise-binding
			 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/row-wise-binding
			 */
			desc = stmt->ard;
			break;
		case SQL_ATTR_PARAM_BIND_TYPE:
			DBG("setting param bind type to: %d.", (SQLULEN)ValuePtr);
			desc = stmt->apd;
			break;
		} while (0);
			ret = EsSQLSetDescFieldW(desc, NO_REC_NR, SQL_DESC_BIND_TYPE,
					/* note: SetStmt()'s spec defineds the ValuePtr as
					 * SQLULEN, but SetDescField()'s as SQLUINTEGER.. */
					ValuePtr, BufferLength);
			if (ret != SQL_SUCCESS) /* _WITH_INFO wud be "error" here */
				/* if SetDescField() fails, DM will check statement's diag */
				HDIAG_COPY(desc, stmt);
			return ret;

		do {
		/* "an array of SQLUSMALLINT values containing row status values after
		 * a call to SQLFetch or SQLFetchScroll." */
		/* https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/row-status-array */
		/* "Setting this statement attribute sets the
		 * SQL_DESC_ARRAY_STATUS_PTR field in the IRD header." */
		case SQL_ATTR_ROW_STATUS_PTR:
			// TODO: call SQLSetDescField(IRD) here?
			DBG("setting row status pointer to: 0x%p.", ValuePtr);
			desc = stmt->ird;
			break;
		case SQL_ATTR_PARAM_STATUS_PTR:
			DBG("setting param status pointer to: 0x%p.", ValuePtr);
			desc = stmt->ipd;
			break;
		case SQL_ATTR_ROW_OPERATION_PTR:
			DBG("setting row operation array pointer to: 0x%p.", ValuePtr);
			desc = stmt->ard;
			break;
		case SQL_ATTR_PARAM_OPERATION_PTR:
			DBG("setting param operation array pointer to: 0x%p.", ValuePtr);
			desc = stmt->apd;
			break;
		} while (0);
			ret = EsSQLSetDescFieldW(desc, NO_REC_NR, 
					SQL_DESC_ARRAY_STATUS_PTR, ValuePtr, BufferLength);
			if (ret != SQL_SUCCESS) /* _WITH_INFO wud be "error" here */
				/* if SetDescField() fails, DM will check statement's diag */
				HDIAG_COPY(desc, stmt);
			return ret;

		do {
		/* "Setting this statement attribute sets the
		 * SQL_DESC_ROWS_PROCESSED_PTR field in the IRD header." */
		case SQL_ATTR_ROWS_FETCHED_PTR:
			DBG("setting rows fetched pointer to: 0x%p.", ValuePtr);
			/* NOTE: documentation writes about "ARD", while also stating that
			 * this field is unused in the ARD. I assume the former as wrong */
			desc = stmt->ird;
			break;
		case SQL_ATTR_PARAMS_PROCESSED_PTR:
			DBG("setting params processed pointer to: 0x%p.", ValuePtr);
			desc = stmt->ipd;
			break;
		} while (0);
			ret = EsSQLSetDescFieldW(desc, NO_REC_NR, 
					SQL_DESC_ROWS_PROCESSED_PTR, ValuePtr, BufferLength);
			if (ret != SQL_SUCCESS) /* _WITH_INFO wud be "error" here */
				/* if SetDescField() fails, DM will check statement's diag */
				HDIAG_COPY(desc, stmt);
			return ret;

		case SQL_ATTR_APP_ROW_DESC:
			if ((ValuePtr == (SQLPOINTER *)&stmt->i_ard) || 
					(ValuePtr == SQL_NULL_HDESC)) {
				if (stmt->ard) {
					DBG("unbinding ARD 0x%p from statement 0x%p.");
					// FIXME: unbind
					FIXME;
				}
				stmt->ard = &stmt->i_ard;
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
				RET_HDIAGS(stmt, SQL_STATE_HY017);
			} else {
				stmt->ard = (esodbc_desc_st *)ValuePtr;
				// FIXME: bind: re-init
				FIXME;
			}	
		case SQL_ATTR_APP_PARAM_DESC:
			// FIXME: same logic for ARD as above (part of params passing)
			FIXME;
			break;

		case SQL_ATTR_IMP_ROW_DESC:
		case SQL_ATTR_IMP_PARAM_DESC:
			ERR("trying to set IxD (%d) descriptor (to @0x%p).", Attribute,
					ValuePtr);
			RET_HDIAGS(stmt, SQL_STATE_HY017);

		case SQL_ATTR_ROW_NUMBER:
			ERR("row number attribute is read-only.");
			RET_HDIAGS(stmt, SQL_STATE_HY024);

		case SQL_ATTR_METADATA_ID:
			DBG("setting metadata_id to: %u", (SQLULEN)ValuePtr);
			stmt->metadata_id = (SQLULEN)ValuePtr;
			break;
		case SQL_ATTR_ASYNC_ENABLE:
			DBG("setting async_enable to: %u", (SQLULEN)ValuePtr);
			stmt->async_enable = (SQLULEN)ValuePtr;
			break;

		case SQL_ATTR_MAX_LENGTH:
			ulen = (SQLULEN)ValuePtr;
			DBG("setting max_lenght to: %u.", ulen);
			if (ulen < ESODBC_LO_MAX_LENGTH) {
				WARN("MAX_LENGHT lower than min allowed (%d) -- correcting "
						"value.", ESODBC_LO_MAX_LENGTH);
				ulen = ESODBC_LO_MAX_LENGTH;
			} else if (ESODBC_UP_MAX_LENGTH && ESODBC_UP_MAX_LENGTH < ulen) {
				WARN("MAX_LENGTH higher than max allowed (%d) -- correcting "
						"value", ESODBC_UP_MAX_LENGTH);
				ulen = ESODBC_UP_MAX_LENGTH;
			}
			stmt->max_length = ulen;
			if (ulen != (SQLULEN)ValuePtr)
				RET_HDIAGS(stmt, SQL_STATE_01S02);
			break;

		default:
			// FIXME
			FIXME;
			ERR("unknown Attribute: %d.", Attribute);
			RET_HDIAGS(stmt, SQL_STATE_HY092);
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
	esodbc_stmt_st *stmt = STMH(StatementHandle);
	SQLRETURN ret;

	switch (Attribute) {
		do {
		case SQL_ATTR_APP_ROW_DESC: desc = stmt->ard; break;
		case SQL_ATTR_APP_PARAM_DESC: desc = stmt->apd; break;
		case SQL_ATTR_IMP_ROW_DESC: desc = stmt->ird; break;
		case SQL_ATTR_IMP_PARAM_DESC: desc = stmt->ipd; break;
		} while (0);
			DBG("getting descriptor (attr type %d): 0x%p.", Attribute, desc);
			*(SQLPOINTER *)ValuePtr = desc;
			break;

		case SQL_ATTR_METADATA_ID:
			DBG("getting metadata_id: %u", stmt->metadata_id);
			*(SQLULEN *)ValuePtr = stmt->metadata_id;
			break;
		case SQL_ATTR_ASYNC_ENABLE:
			DBG("getting async_enable: %u", stmt->async_enable);
			*(SQLULEN *)ValuePtr = stmt->async_enable;
			break;
		case SQL_ATTR_MAX_LENGTH:
			DBG("getting max_length: %u", stmt->max_length);
			*(SQLULEN *)ValuePtr = stmt->max_length;
			break;

		/* "determine the number of the current row in the result set" */
		case SQL_ATTR_ROW_NUMBER:
			DBG("getting row number: %u", (SQLULEN)stmt->rset.frows);
			*(SQLULEN *)ValuePtr = (SQLULEN)stmt->rset.frows;
			break;

		case SQL_ATTR_CURSOR_TYPE:
			DBG("getting cursor type: %u.", SQL_CURSOR_FORWARD_ONLY);
			/* we only support forward only cursor, so far - TODO */
			*(SQLULEN *)ValuePtr = SQL_CURSOR_FORWARD_ONLY;
			break;

		do {
		/* "Setting this statement attribute sets the SQL_DESC_ARRAY_SIZE
		 * field in the ARD header." */
		case SQL_ATTR_ROW_ARRAY_SIZE:
			DBG("getting row array size.");
			desc = stmt->ard;
			break;
		/* "Setting this statement attribute sets the SQL_DESC_ARRAY_SIZE
		 * field in the APD header." */
		case SQL_ATTR_PARAMSET_SIZE:
			DBG("getting param set size.");
			desc = stmt->apd;
			break;
		} while (0);
			ret = EsSQLGetDescFieldW(desc, NO_REC_NR, SQL_DESC_ARRAY_SIZE,
					ValuePtr, BufferLength, NULL);
			if (ret != SQL_SUCCESS) /* _WITH_INFO wud be "error" here */
				/* if SetDescField() fails, DM will check statement's diag */
				HDIAG_COPY(desc, stmt);
			return ret;

		case SQL_ATTR_USE_BOOKMARKS:
			DBG("getting use-bookmarks: %u.", SQL_UB_OFF);
			*(SQLULEN *)ValuePtr = SQL_UB_OFF;
			break;

		default:
			ERR("unknown attribute: %d.", Attribute);
			RET_HDIAGS(stmt, SQL_STATE_HY092);
	}

	return SQL_SUCCESS;
}


/*
 * SQL_DESC_BASE_COLUMN_NAME	pointer to a character string
 * SQL_DESC_BASE_TABLE_NAME	pointer to a character string
 * SQL_DESC_CATALOG_NAME	pointer to a character string
 * SQL_DESC_LABEL	pointer to a character string
 * SQL_DESC_LITERAL_PREFIX	pointer to a character string
 * SQL_DESC_LITERAL_SUFFIX	pointer to a character string
 * SQL_DESC_LOCAL_TYPE_NAME	pointer to a character string
 * SQL_DESC_NAME	pointer to a character string
 * SQL_DESC_SCHEMA_NAME	pointer to a character string
 * SQL_DESC_TABLE_NAME	pointer to a character string
 * SQL_DESC_TYPE_NAME	pointer to a character string
 * SQL_DESC_INDICATOR_PTR	pointer to a binary buffer
 * SQL_DESC_OCTET_LENGTH_PTR	pointer to a binary buffer
 * SQL_DESC_BIND_OFFSET_PTR	pointer to a binary buffer
 * SQL_DESC_ROWS_PROCESSED_PTR	pointer to a binary buffer
 * SQL_DESC_ARRAY_STATUS_PTR	pointer to a binary buffer
 * SQL_DESC_DATA_PTR	pointer to a value,
 *                      other than a character string or binary string
 * SQL_DESC_BIND_TYPE	SQL_IS_INTEGER
 * SQL_DESC_AUTO_UNIQUE_VALUE	SQL_IS_INTEGER
 * SQL_DESC_CASE_SENSITIVE	SQL_IS_INTEGER
 * SQL_DESC_DATETIME_INTERVAL_PRECISION	SQL_IS_INTEGER
 * SQL_DESC_NUM_PREC_RADIX	SQL_IS_INTEGER
 * SQL_DESC_DISPLAY_SIZE	SQL_IS_INTEGER
 * SQL_DESC_OCTET_LENGTH	SQL_IS_INTEGER
 * SQL_DESC_ARRAY_SIZE	SQL_IS_UINTEGER
 * SQL_DESC_LENGTH	SQL_IS_UINTEGER
 * SQL_DESC_ALLOC_TYPE	SQL_IS_SMALLINT
 * SQL_DESC_COUNT	SQL_IS_SMALLINT
 * SQL_DESC_CONCISE_TYPE	SQL_IS_SMALLINT
 * SQL_DESC_DATETIME_INTERVAL_CODE	SQL_IS_SMALLINT
 * SQL_DESC_FIXED_PREC_SCALE	SQL_IS_SMALLINT
 * SQL_DESC_NULLABLE	SQL_IS_SMALLINT
 * SQL_DESC_PARAMETER_TYPE	SQL_IS_SMALLINT
 * SQL_DESC_PRECISION	SQL_IS_SMALLINT
 * SQL_DESC_ROWVER	SQL_IS_SMALLINT
 * SQL_DESC_SCALE	SQL_IS_SMALLINT
 * SQL_DESC_SEARCHABLE	SQL_IS_SMALLINT
 * SQL_DESC_TYPE	SQL_IS_SMALLINT
 * SQL_DESC_UNNAMED	SQL_IS_SMALLINT
 * SQL_DESC_UNSIGNED	SQL_IS_SMALLINT
 * SQL_DESC_UPDATABLE	SQL_IS_SMALLINT
 */
static esodbc_state_et check_buff(SQLSMALLINT field_id, SQLPOINTER buff,
		/*writable == buff is a pointer to write into (=Get)*/
		SQLINTEGER buff_len, BOOL writable)
{
	switch (field_id) {
		/* pointer to a character string */
		case SQL_DESC_BASE_COLUMN_NAME:
		case SQL_DESC_BASE_TABLE_NAME:
		case SQL_DESC_CATALOG_NAME:
		case SQL_DESC_LABEL:
		case SQL_DESC_LITERAL_PREFIX:
		case SQL_DESC_LITERAL_SUFFIX:
		case SQL_DESC_LOCAL_TYPE_NAME:
		case SQL_DESC_NAME:
		case SQL_DESC_SCHEMA_NAME:
		case SQL_DESC_TABLE_NAME:
		case SQL_DESC_TYPE_NAME:
			if (buff_len < 0 && buff_len != SQL_NTS) {
				ERR("buffer is for a character string and its length is "
						"negative (%d).", buff_len);
				return SQL_STATE_HY090;
			}
			if (buff_len % sizeof(SQLWCHAR)) {
				ERR("buffer not alligned to SQLWCHAR size (%d).", buff_len);
				return SQL_STATE_HY090;
			}
			if ((! writable) && (ESODBC_MAX_IDENTIFIER_LEN < buff_len)) {
				ERR("trying to set field %d to a string buffer larger than "
						"max allowed (%d).", field_id, 
						ESODBC_MAX_IDENTIFIER_LEN);
				return SQL_STATE_22001;
			}
			return SQL_STATE_00000;
	}

	/* do this check after string types: they may be NULL for both funcs */
	if (writable && (! buff)) {
		ERR("null output buffer provided for field (%d).", field_id);
		return SQL_STATE_HY090;
	}

	switch (field_id) {
		/* pointer to a binary buffer */
		case SQL_DESC_INDICATOR_PTR:
		case SQL_DESC_OCTET_LENGTH_PTR:
		case SQL_DESC_BIND_OFFSET_PTR:
		case SQL_DESC_ROWS_PROCESSED_PTR:
		case SQL_DESC_ARRAY_STATUS_PTR:
			if (0 < buff_len) {
				ERR("buffer is for binary buffer, but length indicator is "
						"positive (%d).", buff_len);
				return SQL_STATE_HY090;
			}
			return SQL_STATE_00000;

		/* pointer to a value other than string or binary string */
		case SQL_DESC_DATA_PTR:
			if ((buff_len != SQL_IS_POINTER) && (buff_len < 0)) {
				/* spec says the lenght "should" be it's size => this check
				 * might be too strict? */
				ERR("buffer is for pointer, but its lenght indicator doesn't "
						"match (%d).", buff_len);
				return SQL_STATE_HY090;
			}
			return SQL_STATE_00000;
	}

	if (! writable)
		/* this call is from SetDescField, so lenght for integer types are
		 * ignored */
		return SQL_STATE_00000;

	switch (field_id) {
		/* SQL_IS_INTEGER */
		case SQL_DESC_BIND_TYPE:
		case SQL_DESC_AUTO_UNIQUE_VALUE:
		case SQL_DESC_CASE_SENSITIVE:
		case SQL_DESC_DATETIME_INTERVAL_PRECISION:
		case SQL_DESC_NUM_PREC_RADIX:
		case SQL_DESC_DISPLAY_SIZE:
		case SQL_DESC_OCTET_LENGTH:
			if (buff_len != SQL_IS_INTEGER) {
				ERR("buffer is for interger, but its lenght indicator doesn't "
						"match (%d).", buff_len);
				return SQL_STATE_HY090;
			}
			break;

		/* SQL_IS_UINTEGER */
		case SQL_DESC_ARRAY_SIZE:
		case SQL_DESC_LENGTH:
			if (buff_len != SQL_IS_UINTEGER) {
				ERR("buffer is for uint, but its lenght indicator doesn't "
						"match (%d).", buff_len);
				return SQL_STATE_HY090;
			}
			break;

		/* SQL_IS_SMALLINT */
		case SQL_DESC_ALLOC_TYPE:
		case SQL_DESC_COUNT:
		case SQL_DESC_CONCISE_TYPE:
		case SQL_DESC_DATETIME_INTERVAL_CODE:
		case SQL_DESC_FIXED_PREC_SCALE:
		case SQL_DESC_NULLABLE:
		case SQL_DESC_PARAMETER_TYPE:
		case SQL_DESC_PRECISION:
		case SQL_DESC_ROWVER:
		case SQL_DESC_SCALE:
		case SQL_DESC_SEARCHABLE:
		case SQL_DESC_TYPE:
		case SQL_DESC_UNNAMED:
		case SQL_DESC_UNSIGNED:
		case SQL_DESC_UPDATABLE:
			if (buff_len != SQL_IS_SMALLINT) {
				ERR("buffer is for short, but its lenght indicator doesn't "
						"match (%d).", buff_len);
				return SQL_STATE_HY090;
			}
			break;

		default:
			ERR("unknown field identifier: %d.", field_id);
			return SQL_STATE_HY091;
	}

	return SQL_STATE_00000;
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
				// just for compiler warning, ANON mode checked above;
				default: ret = FALSE;
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

static void get_rec_default(SQLSMALLINT field_id, SQLINTEGER buff_len, 
		SQLPOINTER buff)
{
	size_t sz;

	switch (field_id) {
		case SQL_DESC_CONCISE_TYPE:
		case SQL_DESC_TYPE:
			*(SQLSMALLINT *)buff = SQL_C_DEFAULT;
			return;
		case SQL_DESC_PARAMETER_TYPE:
			*(SQLSMALLINT *)buff = SQL_PARAM_INPUT;
			return;
	}

	switch (buff_len) {
		case SQL_IS_INTEGER: sz = sizeof(SQLINTEGER); break;
		case SQL_IS_UINTEGER: sz = sizeof(SQLULEN); break;
		case SQL_IS_SMALLINT: sz = sizeof(SQLSMALLINT); break;
		default: sz = 0;
	}

	if (sz)
		memset(buff, 0, sz);
}

static inline void init_rec(desc_rec_st *rec, esodbc_desc_st *desc)
{
	memset(rec, 0, sizeof(desc_rec_st));
	rec->desc = desc;
	/* further init to defaults is done once the data type is set */
}

SQLRETURN update_rec_count(esodbc_desc_st *desc, SQLSMALLINT new_count)
{
	desc_rec_st *recs;
	int i;

	if (new_count < 0) {
		ERR("new record count is negative (%d).", new_count);
		RET_HDIAGS(desc, SQL_STATE_07009);
	}

	if (desc->count == new_count) {
		LOG(new_count ? LOG_LEVEL_INFO : LOG_LEVEL_DBG, 
				"new descriptor count equals old one, %d.", new_count);
		RET_STATE(SQL_STATE_00000);
	}

	if (ESODBC_MAX_DESC_COUNT < new_count) {
		ERR("count value (%d) higher than allowed max (%d).", new_count, 
				ESODBC_MAX_DESC_COUNT);
		RET_HDIAGS(desc, SQL_STATE_07009);
	}

	if (new_count == 0) {
		DBG("freeing the array of %d elems.", desc->count);
		free_desc_recs(desc);
		recs = NULL;
	} else {
		/* TODO: change to a list implementation? review@alpha */
		recs = (desc_rec_st *)realloc(desc->recs, 
				sizeof(desc_rec_st) * new_count);
		if (! recs) {
			ERRN("can't (re)alloc %d records.", new_count);
			RET_HDIAGS(desc, SQL_STATE_HY001);
		}
		if (new_count < desc->count) { /* shrinking array */
			DBG("recs array is shrinking %d -> %d.", desc->count, new_count);
			for (i = new_count - 1; i < desc->count; i ++)
				free_rec_fields(&desc->recs[i]);
		} else { /* growing array */
			DBG("recs array is growing %d -> %d.", desc->count, new_count);
			/* init all new records */
			for (i = desc->count; i < new_count; i ++)
				init_rec(&recs[i], desc);
		}
	}

	desc->count = new_count;
	desc->recs = recs;
	RET_STATE(SQL_STATE_00000);
}

/*
 * Returns the record with desired 1-based index.
 * Grow the array if needed (desired index higher than current count).
 */
desc_rec_st* get_record(esodbc_desc_st *desc, SQLSMALLINT rec_no, BOOL grow)
{
	assert(0 <= rec_no);

	if (desc->count < rec_no) {
		if (! grow)
			return NULL;
		else if (! SQL_SUCCEEDED(update_rec_count(desc, rec_no)))
			return NULL;
	}
	return &desc->recs[rec_no - 1];
}

static SQLSMALLINT recount_bound(esodbc_desc_st *desc)
{
	SQLSMALLINT i;
	for (i = desc->count; 0 < i && REC_IS_BOUND(&desc->recs[i-1]); i--)
		;
	return i;
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
		SQLINTEGER      BufferLength, /* byte count, also for wchar's */
		SQLINTEGER      *StringLengthPtr)
{
	esodbc_desc_st *desc = DSCH(DescriptorHandle);
	esodbc_state_et state;
	SQLWCHAR *wptr;
	SQLSMALLINT word;
	SQLINTEGER intgr;
	desc_rec_st *rec;

	if (! check_access(desc->type, FieldIdentifier, 
				O_RDONLY)) {
		int llev;
#if 0
		/* 
		 * Actually, the spec ask to return success, but just set nothing: ???
		 * "When an application calls SQLGetDescField to retrieve the value of
		 * a field that is undefined for a particular descriptor type, the
		 * function returns SQL_SUCCESS but the value returned for the field
		 * is undefined." 
		 */
		llev = LOG_LEVEL_ERR;
		state = SQL_STATE_HY091;
#else /* 0 */
		llev = LOG_LEVEL_WARN;
		state = SQL_STATE_01000;
#endif /* 0 */
		LOG(llev, "field (%d) access check failed: not defined for desciptor "
				"(type: %d).", FieldIdentifier, desc->type);
		RET_HDIAG(desc, state, 
				"field type not defined for descriptor", 0);
	}

	state = check_buff(FieldIdentifier, ValuePtr, BufferLength, TRUE);
	if (state != SQL_STATE_00000) {
		ERR("buffer/~ length check failed (%d).", state);
		RET_HDIAGS(desc, state);
	}

	/* header fields */
	switch (FieldIdentifier) {
		case SQL_DESC_ALLOC_TYPE:
			*(SQLSMALLINT *)ValuePtr = desc->alloc_type;
			DBG("returning: desc alloc type: %u.", *(SQLSMALLINT *)ValuePtr);
			RET_STATE(SQL_STATE_00000);

		case SQL_DESC_ARRAY_SIZE:
			*(SQLULEN *)ValuePtr = desc->array_size;
			DBG("returning: desc array size: %u.", *(SQLULEN *)ValuePtr);
			RET_STATE(SQL_STATE_00000);

		case SQL_DESC_ARRAY_STATUS_PTR:
			*(SQLUSMALLINT **)ValuePtr = 
				desc->array_status_ptr;
			DBG("returning: status array ptr: 0x%p.",(SQLUSMALLINT *)ValuePtr);
			RET_STATE(SQL_STATE_00000);

		case SQL_DESC_BIND_OFFSET_PTR:
			*(SQLLEN **)ValuePtr = desc->bind_offset_ptr;
			DBG("returning binding offset ptr: 0x%p.", (SQLLEN *)ValuePtr);
			RET_STATE(SQL_STATE_00000);

		case SQL_DESC_BIND_TYPE:
			*(SQLUINTEGER *)ValuePtr = desc->bind_type;
			DBG("returning bind type: %u.", *(SQLUINTEGER *)ValuePtr);
			RET_STATE(SQL_STATE_00000);

		case SQL_DESC_COUNT:
			*(SQLSMALLINT *)ValuePtr = desc->count;
			DBG("returning count: %d.", *(SQLSMALLINT *)ValuePtr);
			RET_STATE(SQL_STATE_00000);

		case SQL_DESC_ROWS_PROCESSED_PTR:
			*(SQLULEN **)ValuePtr = desc->rows_processed_ptr;
			DBG("returning desc rows processed ptr: 0x%p.", ValuePtr);
			RET_STATE(SQL_STATE_00000);
	}

	/* 
	 * The field is a record field -> get the record to apply the field to.
	 */
	if (RecNumber < 0) { /* TODO: need to check also if AxD, as per spec?? */
		ERR("negative record number provided (%d) with record field (%d).",
				RecNumber, FieldIdentifier);
		RET_HDIAG(desc, SQL_STATE_07009, 
				"Negative record number provided with record field", 0);
	} else if (RecNumber == 0) {
		ERR("unsupported record number 0."); /* TODO: bookmarks? */
		RET_HDIAG(desc, SQL_STATE_07009, 
				"Unsupported record number 0", 0);
	} else {
		/*
		 * "When an application calls SQLGetDescField to retrieve the value of
		 * a field that is defined for a particular descriptor type but that
		 * has no default value and has not been set yet, the function returns
		 * SQL_SUCCESS but the value returned for the field is undefined."
		 */
		rec = get_record(desc, RecNumber, FALSE);
		if (! rec) {
			WARN("record #%d not yet set; returning defaults.", RecNumber);
			get_rec_default(FieldIdentifier, BufferLength, ValuePtr);
			RET_STATE(SQL_STATE_00000);
		}
		DBG("getting field %d of record #%d @ 0x%p.", FieldIdentifier, 
				RecNumber, rec);
	}


	/* record fields */
	switch (FieldIdentifier) {
		/* <SQLPOINTER> */
		case SQL_DESC_DATA_PTR:
			*(SQLPOINTER *)ValuePtr = rec->data_ptr;
			DBG("returning data pointer 0x%p.", rec->data_ptr);
			break;

		/* <SQLWCHAR *> */
		do {
		case SQL_DESC_BASE_COLUMN_NAME: wptr = rec->base_column_name; break;
		case SQL_DESC_BASE_TABLE_NAME: wptr = rec->base_table_name; break;
		case SQL_DESC_CATALOG_NAME: wptr = rec->catalog_name; break;
		case SQL_DESC_LABEL: wptr = rec->label; break;
		case SQL_DESC_LITERAL_PREFIX: wptr = rec->literal_prefix; break;
		case SQL_DESC_LITERAL_SUFFIX: wptr = rec->literal_suffix; break;
		case SQL_DESC_LOCAL_TYPE_NAME: wptr = rec->local_type_name; break;
		case SQL_DESC_NAME: wptr = rec->name; break;
		case SQL_DESC_SCHEMA_NAME: wptr = rec->schema_name; break;
		case SQL_DESC_TABLE_NAME: wptr = rec->table_name; break;
		case SQL_DESC_TYPE_NAME: wptr = rec->type_name; break;
		} while (0);
			if (! wptr) {
				*StringLengthPtr = 0;
			} else {
				*StringLengthPtr = (SQLINTEGER)wcslen(wptr) * sizeof(SQLWCHAR);
			}
			if (ValuePtr) {
				memcpy(ValuePtr, wptr, *StringLengthPtr);
				/* TODO: 0-term setting? */
			}
			DBG("returning record field %d as SQLWCHAR 0x%p (`"LWPD"`).", 
					FieldIdentifier, wptr, wptr ? wptr : TS_NULL);
			break;

		/* <SQLLEN *> */
		case SQL_DESC_INDICATOR_PTR:
			*(SQLLEN **)ValuePtr = rec->indicator_ptr;
			DBG("returning indicator pointer: 0x%p.", rec->indicator_ptr);
			break;
		case SQL_DESC_OCTET_LENGTH_PTR:
			*(SQLLEN **)ValuePtr = rec->octet_length_ptr;
			DBG("returning octet length pointer 0x%p.", rec->octet_length_ptr);
			break;

		/* <SQLLEN> */
		case SQL_DESC_DISPLAY_SIZE:
			*(SQLLEN *)ValuePtr = rec->display_size;
			DBG("returning display size: %d.", rec->display_size);
			break;
		case SQL_DESC_OCTET_LENGTH:
			*(SQLLEN *)ValuePtr = rec->octet_length;
			DBG("returning octet length: %d.", rec->octet_length);
			break;

		/* <SQLULEN> */
		case SQL_DESC_LENGTH:
			*(SQLULEN *)ValuePtr = rec->length;
			DBG("returning lenght: %u.", rec->length);
			break;

		/* <SQLSMALLINT> */
		do {
		case SQL_DESC_CONCISE_TYPE: word = rec->concise_type; break;
		case SQL_DESC_TYPE: word = rec->type; break;
		case SQL_DESC_DATETIME_INTERVAL_CODE:
			word = rec->datetime_interval_code; break;

		case SQL_DESC_FIXED_PREC_SCALE: word = rec->fixed_prec_scale; break;
		case SQL_DESC_NULLABLE: word = rec->nullable; break;
		case SQL_DESC_PARAMETER_TYPE: word = rec->parameter_type; break;
		case SQL_DESC_PRECISION: word = rec->precision; break;
		case SQL_DESC_ROWVER: word = rec->rowver; break;
		case SQL_DESC_SCALE: word = rec->scale; break;
		case SQL_DESC_SEARCHABLE: word = rec->searchable; break;
		case SQL_DESC_UNNAMED: word = rec->unnamed; break;
		case SQL_DESC_UNSIGNED: word = rec->usigned; break;
		case SQL_DESC_UPDATABLE: word = rec->updatable; break;
		} while (0);
			*(SQLSMALLINT *)ValuePtr = word;
			DBG("returning record field %d as %d.", FieldIdentifier, word);
			break;

		/* <SQLINTEGER> */
		do {
		case SQL_DESC_AUTO_UNIQUE_VALUE: intgr = rec->auto_unique_value; break;
		case SQL_DESC_CASE_SENSITIVE: intgr = rec->case_sensitive; break;
		case SQL_DESC_DATETIME_INTERVAL_PRECISION: 
			intgr = rec->datetime_interval_precision; break;
		case SQL_DESC_NUM_PREC_RADIX: intgr = rec->num_prec_radix; break;
		} while (0);
			*(SQLINTEGER *)ValuePtr = intgr;
			DBG("returning record field %d as %d.", FieldIdentifier, intgr);
			break;

		default:
			ERR("unknown FieldIdentifier: %d.", FieldIdentifier);
			RET_HDIAGS(desc, SQL_STATE_HY091);
	}
	
	RET_STATE(SQL_STATE_00000);
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
 * https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/data-type-identifiers-and-descriptors
 *
 * Note: C and SQL types have the same value for these following defines,
 * so this function will work for both IxD and AxD descriptors. (There is no
 * SQL_C_DATETIME or SQL_C_CODE_DATE.)
 */
void concise_to_type_code(SQLSMALLINT concise, SQLSMALLINT *type, 
		SQLSMALLINT *code)
{
	switch (concise) {
		case SQL_DATE:
		case SQL_TYPE_DATE:
			*type = SQL_DATETIME;
			*code = SQL_CODE_DATE;
			break;
		case SQL_TIME:
		case SQL_TYPE_TIME:
		//case SQL_TYPE_TIME_WITH_TIMEZONE: //4.0
			*type = SQL_DATETIME;
			*code = SQL_CODE_TIME;
			break;
		case SQL_TIMESTAMP:
		case SQL_TYPE_TIMESTAMP:
		//case SQL_TYPE_TIMESTAMP_WITH_TIMEZONE: // 4.0
			*type = SQL_DATETIME;
			*code = SQL_CODE_TIMESTAMP;
			break;
		case SQL_INTERVAL_MONTH:
			*type = SQL_INTERVAL;
			*code = SQL_CODE_MONTH;
			break;
		case SQL_INTERVAL_YEAR:
			*type = SQL_INTERVAL;
			*code = SQL_CODE_YEAR;
			break;
		case SQL_INTERVAL_YEAR_TO_MONTH:
			*type = SQL_INTERVAL;
			*code = SQL_CODE_YEAR_TO_MONTH;
			break;
		case SQL_INTERVAL_DAY:
			*type = SQL_INTERVAL;
			*code = SQL_CODE_DAY;
			break;
		case SQL_INTERVAL_HOUR:
			*type = SQL_INTERVAL;
			*code = SQL_CODE_HOUR;
			break;
		case SQL_INTERVAL_MINUTE:
			*type = SQL_INTERVAL;
			*code = SQL_CODE_MINUTE;
			break;
		case SQL_INTERVAL_SECOND:
			*type = SQL_INTERVAL;
			*code = SQL_CODE_SECOND;
			break;
		case SQL_INTERVAL_DAY_TO_HOUR:
			*type = SQL_INTERVAL;
			*code = SQL_CODE_DAY_TO_HOUR;
			break;
		case SQL_INTERVAL_DAY_TO_MINUTE:
			*type = SQL_INTERVAL;
			*code = SQL_CODE_DAY_TO_MINUTE;
			break;
		case SQL_INTERVAL_DAY_TO_SECOND:
			*type = SQL_INTERVAL;
			*code = SQL_CODE_DAY_TO_SECOND;
			break;
		case SQL_INTERVAL_HOUR_TO_MINUTE:
			*type = SQL_INTERVAL;
			*code = SQL_CODE_HOUR_TO_MINUTE;
			break;
		case SQL_INTERVAL_HOUR_TO_SECOND:
			*type = SQL_INTERVAL;
			*code = SQL_CODE_HOUR_TO_SECOND;
			break;
		case SQL_INTERVAL_MINUTE_TO_SECOND:
			*type = SQL_INTERVAL;
			*code = SQL_CODE_MINUTE_TO_SECOND;
			break;
	}
	*type = concise;
	*code = 0;
}

/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetdescfield-function#comments
 */
static void set_defaults_from_type(desc_rec_st *rec)
{
	switch (rec->meta_type) {
		case METATYPE_STRING:
			rec->length = 1;
			rec->precision = 0;
			break;
		case METATYPE_DATETIME:
			if (rec->datetime_interval_code == SQL_CODE_DATE || 
					rec->datetime_interval_code == SQL_CODE_TIME)
				rec->precision = 0;
			else if (rec->datetime_interval_code == SQL_CODE_TIMESTAMP)
				rec->precision = 6;
			break;
		case METATYPE_EXACT_NUMERIC:
			rec->scale = 0;
			rec->precision = 19; /* TODO: "implementation-defined precision" */
			break;
		case METATYPE_FLOAT_NUMERIC:
			rec->precision = 8; /* TODO: "implementation-defined precision" */
			break;
		case METATYPE_INTERVAL_WSEC:
			rec->precision = 6;
			break;
	}
}


static esodbc_metatype_et sqltype_to_meta(SQLSMALLINT concise)
{
	switch(concise) {
		/* character */
		case SQL_CHAR:
		case SQL_VARCHAR:
		case SQL_LONGVARCHAR:
		case SQL_WCHAR:
		case SQL_WVARCHAR:
		case SQL_WLONGVARCHAR:
			return METATYPE_STRING;
		/* binary */
		case SQL_BINARY:
		case SQL_VARBINARY:
		case SQL_LONGVARBINARY:
			return METATYPE_BIN;

		/* numeric exact */
		case SQL_DECIMAL:
		case SQL_INTEGER:
		case SQL_NUMERIC:
		case SQL_SMALLINT:
		case SQL_TINYINT:
		case SQL_BIGINT:
			return METATYPE_EXACT_NUMERIC;

		/* numeric floating */
		case SQL_REAL:
		case SQL_FLOAT:
		case SQL_DOUBLE:
			return METATYPE_FLOAT_NUMERIC;

		/* datetime (note: SQL_DATETIME is verbose, not concise) */
		case SQL_TYPE_DATE:
		case SQL_TYPE_TIME:
		case SQL_TYPE_TIMESTAMP:
		// case SQL_TYPE_UTCDATETIME:
		// case SQL_TYPE_UTCTIME:
			return METATYPE_DATETIME;

		/* interval (note: SQL_INTERVAL is verbose, not concise) */
		case SQL_INTERVAL_MONTH:
		case SQL_INTERVAL_YEAR:
		case SQL_INTERVAL_YEAR_TO_MONTH:
		case SQL_INTERVAL_DAY:
		case SQL_INTERVAL_HOUR:
		case SQL_INTERVAL_MINUTE:
		case SQL_INTERVAL_DAY_TO_HOUR:
		case SQL_INTERVAL_DAY_TO_MINUTE:
		case SQL_INTERVAL_HOUR_TO_MINUTE:
			return METATYPE_INTERVAL_WOSEC;

		case SQL_INTERVAL_SECOND:
		case SQL_INTERVAL_DAY_TO_SECOND:
		case SQL_INTERVAL_HOUR_TO_SECOND:
		case SQL_INTERVAL_MINUTE_TO_SECOND:
			return METATYPE_INTERVAL_WSEC;

		case SQL_BIT:
			return METATYPE_BIT;

		case SQL_GUID:
			return METATYPE_UID;
	}

	// BUG?
	WARN("unknown meta type for concise SQL type %d.", concise);
	return METATYPE_UNKNOWN;
}

static esodbc_metatype_et sqlctype_to_meta(SQLSMALLINT concise)
{
	switch (concise) {
		/* character */
		case SQL_C_CHAR:
		case SQL_C_WCHAR:
		// case SQL_C_TCHAR:
			return METATYPE_STRING;
		/* binary */
		case SQL_C_BINARY:
#ifdef _WIN64
		case SQL_C_BOOKMARK:
#endif
		//case SQL_C_VARBOOKMARK:
			return METATYPE_BIN;

		/* numeric exact */
		case SQL_C_SHORT:
		case SQL_C_SSHORT:
		case SQL_C_USHORT:
		case SQL_C_LONG:
		case SQL_C_SLONG:
		case SQL_C_ULONG:
		case SQL_C_TINYINT:
		case SQL_C_STINYINT:
		case SQL_C_UTINYINT:
		case SQL_C_SBIGINT:
		//case SQL_C_UBIGINT:
		case SQL_C_NUMERIC:
			return METATYPE_EXACT_NUMERIC;

		/* numeric floating */
		case SQL_C_FLOAT:
		case SQL_C_DOUBLE:
			return METATYPE_FLOAT_NUMERIC;

		/* datetime */
		case SQL_C_DATE:
		case SQL_C_TYPE_DATE:
		case SQL_C_TIME:
		case SQL_C_TYPE_TIME:
		// case SQL_C_TYPE_TIME_WITH_TIMEZONE:
		case SQL_C_TIMESTAMP:
		case SQL_C_TYPE_TIMESTAMP:
		// case SQL_C_TYPE_TIMESTAMP_WITH_TIMEZONE:
			return METATYPE_DATETIME;

		/* interval */
		case SQL_C_INTERVAL_DAY:
		case SQL_C_INTERVAL_HOUR:
		case SQL_C_INTERVAL_MINUTE:
		case SQL_C_INTERVAL_DAY_TO_HOUR:
		case SQL_C_INTERVAL_DAY_TO_MINUTE:
		case SQL_C_INTERVAL_HOUR_TO_MINUTE:
		case SQL_C_INTERVAL_MONTH:
		case SQL_C_INTERVAL_YEAR:
		case SQL_C_INTERVAL_YEAR_TO_MONTH:
			return METATYPE_INTERVAL_WOSEC;

		case SQL_C_INTERVAL_SECOND:
		case SQL_C_INTERVAL_DAY_TO_SECOND:
		case SQL_C_INTERVAL_HOUR_TO_SECOND:
		case SQL_C_INTERVAL_MINUTE_TO_SECOND:
			return METATYPE_INTERVAL_WSEC;
		
		case SQL_C_BIT:
			return METATYPE_BIT;

		case SQL_C_GUID:
			return METATYPE_UID;

		case SQL_C_DEFAULT:
			return METATYPE_MAX;
	}

	// BUG?
	WARN("unknown meta type for concise C SQL type %d.", concise);
	return METATYPE_UNKNOWN;
}

/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetdescrec-function#consistency-checks
 */
static BOOL consistency_check(esodbc_desc_st *desc, desc_rec_st *rec)
{

	SQLSMALLINT type, code; 

	/* validity of C / SQL datatypes is checked when setting the meta_type */

	concise_to_type_code(rec->concise_type, &type, &code);
	if (rec->type != type || rec->datetime_interval_code != code) {
		ERR("inconsistent types for rec 0x%p: concise: %d, verbose: %d, "
				"code: %d.", rec, rec->concise_type, rec->type, 
				rec->datetime_interval_code);
		return FALSE;
	}

	/* TODO: use the get_rec_size/get_rec_decdigits() (below)? */

	//if (rec->meta_type == METATYPE_NUMERIC) {
	if (0) { // FIXME
		/* TODO: actually check validity of precision/scale for data type */
		if ((! rec->precision) || (! rec->scale)) {
			ERR("invalid numeric precision/scale: %d/%d for data type %d.", 
					rec->precision, rec->scale, rec->type);
			return FALSE;
		}
	}

	if (rec->meta_type == METATYPE_DATETIME || 0) { // FIXME
//			rec->meta_type == METATYPE_INTERVAL) {
		/* TODO: actually check validity of precision for data type */
		/* 
		 * TODO: this should be rec->length, acc to:
		 * https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/column-size 
		 * but rec->precision, acc to:
		 * https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetdescrec-function#consistency-checks
		 * ???
		 */
		if (! rec->precision) {
			ERR("invalid time/timestamp/interval with seconds/time "
					"precision %d for concise type %d.", 
					rec->precision, rec->concise_type);
			return FALSE;
		}
	}

	// if (rec->meta_type == METATYPE_INTERVAL) {
	if (0) { // FIXME
		/* TODO: actually check the validity of dt_i_precision for data type */
		if (! rec->datetime_interval_precision) {
			ERR("invalid datetime_interval_precision %d for interval concise "
					"type %d.", rec->datetime_interval_precision, 
					rec->concise_type);
			return FALSE;
		}
	}

	return TRUE;
}

esodbc_metatype_et concise_to_meta(SQLSMALLINT concise_type, 
		desc_type_et desc_type)
{
	switch (desc_type) {
		case DESC_TYPE_ARD:
		case DESC_TYPE_APD:
			return sqlctype_to_meta(concise_type);

		case DESC_TYPE_IRD:
		case DESC_TYPE_IPD:
			return sqltype_to_meta(concise_type);

		default:
			BUG("can't use anonymous record type");
	}

	return METATYPE_UNKNOWN;
}

/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/column-size
 */
SQLULEN get_rec_size(desc_rec_st *rec)
{
	if (rec->meta_type == METATYPE_EXACT_NUMERIC || 
			rec->meta_type == METATYPE_FLOAT_NUMERIC) {
		if (rec->precision < 0) {
			BUG("precision can't be negative.");
			return 0;
		}
		return (SQLULEN)rec->precision;
	} else {
		return rec->length;
	}
}

SQLULEN get_rec_decdigits(desc_rec_st *rec)
{
	switch (rec->meta_type) {
		case METATYPE_DATETIME:
		case METATYPE_INTERVAL_WSEC:
			return rec->precision;
		case METATYPE_EXACT_NUMERIC:
			return rec->scale;
	}
	/* 0 to be returned for unknown case:
	 * https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqldescribecol-function#syntax
	 */
	return 0;
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
 *
 * "The only way to unbind a bookmark column is to set the SQL_DESC_DATA_PTR
 * field to a null pointer."
 */
SQLRETURN EsSQLSetDescFieldW(
		SQLHDESC        DescriptorHandle,
		SQLSMALLINT     RecNumber,
		SQLSMALLINT     FieldIdentifier,
		SQLPOINTER      ValuePtr,
		SQLINTEGER      BufferLength)
{
	esodbc_desc_st *desc = DSCH(DescriptorHandle);
	esodbc_state_et state;
	desc_rec_st *rec;
	SQLWCHAR **wptrp, *wptr;
	SQLSMALLINT *wordp;
	SQLINTEGER *intp;
	SQLSMALLINT count, type;
	SQLULEN ulen;
	size_t wlen;

	if (! check_access(desc->type, FieldIdentifier, O_RDWR)) {
		/* "The SQL_DESC_DATA_PTR field of an IPD is not normally set;
		 * however, an application can do so to force a consistency check of
		 * IPD fields." 
		 * TODO: the above won't work with the generic check implementation:
		 * is it worth hacking an exception here? (since IPD/.data_ptr is 
		 * marked RO) */
		ERR("field access check failed: not defined or RO for desciptor.");
		RET_HDIAGS(desc, SQL_STATE_HY091);
	}
	
	state = check_buff(FieldIdentifier, ValuePtr, BufferLength, FALSE);
	if (state != SQL_STATE_00000) {
		ERR("buffer/~ length check failed (%d).", state);
		RET_HDIAGS(desc, state);
	}

	/* header fields */
	switch (FieldIdentifier) {
		case SQL_DESC_ARRAY_SIZE:
			ulen = (SQLULEN)(uintptr_t)ValuePtr;
			DBG("setting desc array size to: %u.", ulen);
			if (ESODBC_MAX_ROW_ARRAY_SIZE < ulen) {
				WARN("provided desc array size (%u) larger than allowed max "
						"(%u) -- set value adjusted to max.", ulen,
						ESODBC_MAX_ROW_ARRAY_SIZE);
				desc->array_size = ESODBC_MAX_ROW_ARRAY_SIZE;
				RET_HDIAGS(desc, SQL_STATE_01S02);
			} else if (ulen < 1) {
				ERR("can't set the array size to less than 1.");
				RET_HDIAGS(desc, SQL_STATE_HY092);
			} else {
				desc->array_size = ulen;
			}
			return SQL_SUCCESS;

		case SQL_DESC_ARRAY_STATUS_PTR:
			DBG("setting desc array status ptr to: 0x%p.", ValuePtr);
			/* deferred */
			desc->array_status_ptr = (SQLUSMALLINT *)ValuePtr;
			return SQL_SUCCESS;

		case SQL_DESC_BIND_OFFSET_PTR:
			DBG("setting binding offset ptr to: 0x%p.", ValuePtr);
			/* deferred */
			desc->bind_offset_ptr = (SQLLEN *)ValuePtr;
			return SQL_SUCCESS;

		case SQL_DESC_BIND_TYPE:
			DBG("setting bind type to: %u.", (SQLUINTEGER)(uintptr_t)ValuePtr);
			desc->bind_type = (SQLUINTEGER)(uintptr_t)ValuePtr;
			return SQL_SUCCESS;

		/* 
		 * "Specifies the 1-based index of the highest-numbered record that
		 * contains data."
		 * "Is not a count of all data columns or of all parameters that are
		 * bound, but the number of the highest-numbered record." 
		 *
		 * "If the highest-numbered column or parameter is unbound, then
		 * SQL_DESC_COUNT is changed to the number of the next
		 * highest-numbered column or parameter. If a column or a parameter
		 * with a number that is less than the number of the highest-numbered
		 * column is unbound, SQL_DESC_COUNT is not changed. If additional
		 * columns or parameters are bound with numbers greater than the
		 * highest-numbered record that contains data, the driver
		 * automatically increases the value in the SQL_DESC_COUNT field."
		 *
		 * "If the value in SQL_DESC_COUNT is explicitly decreased, all
		 * records with numbers greater than the new value in SQL_DESC_COUNT
		 * are effectively removed. If the value in SQL_DESC_COUNT is
		 * explicitly set to 0 and the field is in an ARD, all data buffers
		 * except a bound bookmark column are released."
		 */
		case SQL_DESC_COUNT:
			return update_rec_count(desc,(SQLSMALLINT)(intptr_t)ValuePtr);

		case SQL_DESC_ROWS_PROCESSED_PTR:
			DBG("setting desc rwos processed ptr to: 0x%p.", ValuePtr);
			desc->rows_processed_ptr = (SQLULEN *)ValuePtr;
			return SQL_SUCCESS;
	}

	/* 
	 * The field is a record field -> get the record to apply the field to.
	 */
	if (RecNumber < 0) { /* TODO: need to check also if AxD, as per spec?? */
		ERR("negative record number provided (%d) with record field (%d).",
				RecNumber, FieldIdentifier);
		RET_HDIAG(desc, SQL_STATE_07009, 
				"Negative record number provided with record field", 0);
	} else if (RecNumber == 0) {
		ERR("unsupported record number 0."); /* TODO: bookmarks? */
		RET_HDIAG(desc, SQL_STATE_07009, 
				"Unsupported record number 0", 0);
	} else { /* apparently one can set a record before the count is set */
		rec = get_record(desc, RecNumber, TRUE);
		if (! rec) {
			ERR("can't get record with number %d.", RecNumber);
			RET_STATE(desc->diag.state);
		}
		DBG("setting field %d of record #%d @ 0x%p.", FieldIdentifier, 
				RecNumber, rec);
	}

	/*
	 * "If the application changes the data type or attributes after setting
	 * the SQL_DESC_DATA_PTR field, the driver sets SQL_DESC_DATA_PTR to a
	 * null pointer, unbinding the record."
	 *
	 * NOTE: the record can actually still be bound by the lenght/indicator
	 * buffer(s), so the above "binding" definition is incomplete.
	 */
	if (FieldIdentifier != SQL_DESC_DATA_PTR)
		rec->data_ptr = NULL;

	/* record fields */
	switch (FieldIdentifier) {
		case SQL_DESC_TYPE:
			type = (SQLSMALLINT)(intptr_t)ValuePtr;
			DBG("setting type of rec 0x%p to %d.", rec, type);
			if (type == SQL_DATETIME || type == SQL_INTERVAL)
				RET_HDIAGS(desc, SQL_STATE_HY021);
			/* no break */
		case SQL_DESC_CONCISE_TYPE:
			DBG("setting concise type of rec 0x%p to %d.", rec, 
					(SQLSMALLINT)(intptr_t)ValuePtr); 
			rec->concise_type = (SQLSMALLINT)(intptr_t)ValuePtr;

			concise_to_type_code(rec->concise_type, &rec->type, 
					&rec->datetime_interval_code);
			rec->meta_type = concise_to_meta(rec->concise_type, desc->type);
			if (rec->meta_type == METATYPE_UNKNOWN) {
				ERR("REC@0x%p: incorrect concise type %d for rec #%d.", rec, 
						rec->concise_type, RecNumber);
				RET_HDIAGS(desc, SQL_STATE_HY021);
			}
			set_defaults_from_type(rec);
			DBG("REC@0x%p types: concise: %d, verbose: %d, code: %d.", rec,
					rec->concise_type, rec->type, rec->datetime_interval_code);
			break;

		case SQL_DESC_DATA_PTR:
			DBG("setting data ptr to 0x%p of type %d.", ValuePtr,BufferLength);
			/* deferred */
			rec->data_ptr = ValuePtr;
			if (rec->data_ptr) {
				if (
#if 1
						0 /* FIXME when adding data defs! */ && 
#endif //1
						(desc->type != DESC_TYPE_IRD) && 
						(! consistency_check(desc, rec))) {
					ERR("consistency check failed on record 0x%p.", rec);
					RET_HDIAGS(desc, SQL_STATE_HY021);
				} else {
					DBG("data ptr 0x%p bound to rec@0x%p.", rec->data_ptr,rec);
				}
			} else {
				// TODO: should this actually use REC_IS_BOUND()?
				/* "If the highest-numbered column or parameter is unbound,
				 * then SQL_DESC_COUNT is changed to the number of the next
				 * highest-numbered column or parameter. " */
				if ((desc->type == DESC_TYPE_ARD) || 
						(desc->type == DESC_TYPE_ARD)) {
					DBG("rec 0x%p of desc type %d unbound.", rec, desc->type);
					if (RecNumber == desc->count) {
						count = recount_bound(desc);
						/* worst case: trying to unbound a not-yet-bound rec */
						if (count != desc->count) {
							DBG("adjusting rec count from %d to %d.", 
									desc->count, count);
							return update_rec_count(desc, count);
						}
					}
				}
			}
			break;

		case SQL_DESC_NAME:
			WARN("stored procedure params (to set to `"LWPD"`) not "
					"supported.", ValuePtr ? (SQLWCHAR *)ValuePtr : TS_NULL);
			RET_HDIAG(desc, SQL_STATE_HYC00, 
					"stored procedure params not supported", 0);

		/* <SQLWCHAR *> */
		do {
		case SQL_DESC_BASE_COLUMN_NAME: wptrp = &rec->base_column_name; break;
		case SQL_DESC_BASE_TABLE_NAME: wptrp = &rec->base_table_name; break;
		case SQL_DESC_CATALOG_NAME: wptrp = &rec->catalog_name; break;
		case SQL_DESC_LABEL: wptrp = &rec->label; break;
		case SQL_DESC_LITERAL_PREFIX: wptrp = &rec->literal_prefix; break;
		case SQL_DESC_LITERAL_SUFFIX: wptrp = &rec->literal_suffix; break;
		case SQL_DESC_LOCAL_TYPE_NAME: wptrp = &rec->local_type_name; break;
		case SQL_DESC_SCHEMA_NAME: wptrp = &rec->schema_name; break;
		case SQL_DESC_TABLE_NAME: wptrp = &rec->table_name; break;
		case SQL_DESC_TYPE_NAME: wptrp = &rec->type_name; break;
		} while (0);
			DBG("setting SQLWCHAR field %d to 0x%p(`"LWPD"`).", 
					FieldIdentifier, ValuePtr, 
					ValuePtr ? (SQLWCHAR *)ValuePtr : TS_NULL);
			if (*wptrp) {
				DBG("freeing previously allocated value for field %d "
						"(`"LWPD"`).", *wptrp);
				free(*wptrp);
			}
			if (! ValuePtr) {
				*wptrp = NULL;
				DBG("field %d reset to NULL.", FieldIdentifier);
				break;
			}
			if (BufferLength == SQL_NTS)
				wlen = wcslen((SQLWCHAR *)ValuePtr);
			else
				wlen = BufferLength;
			wptr = (SQLWCHAR *)malloc((wlen + /*0-term*/1) * sizeof(SQLWCHAR));
			if (! wptr) {
				ERR("failed to alloc string buffer of len %d.", wlen + 1);
				RET_HDIAGS(desc, SQL_STATE_HY001);
			}
			memcpy(wptr, ValuePtr, wlen * sizeof(SQLWCHAR));
			wptr[wlen] = 0;
			*wptrp = wptr;
			break;

		/* <SQLLEN *>, deferred */
		case SQL_DESC_INDICATOR_PTR:
			DBG("setting indicator pointer to 0x%p.", ValuePtr);
			rec->indicator_ptr = (SQLLEN *)ValuePtr;
			break;
		case SQL_DESC_OCTET_LENGTH_PTR:
			DBG("setting octet length pointer to 0x%p.", ValuePtr);
			rec->octet_length_ptr = (SQLLEN *)ValuePtr;
			break;

		/* <SQLLEN> */
		case SQL_DESC_DISPLAY_SIZE:
			DBG("setting display size: %d.", (SQLLEN)(intptr_t)ValuePtr);
			rec->display_size = (SQLLEN)(intptr_t)ValuePtr;
			break;
		case SQL_DESC_OCTET_LENGTH:
			DBG("setting octet length: %d.", (SQLLEN)(intptr_t)ValuePtr);
			rec->octet_length = (SQLLEN)(intptr_t)ValuePtr;
			break;

		/* <SQLULEN> */
		case SQL_DESC_LENGTH:
			DBG("setting lenght: %u.", (SQLULEN)(uintptr_t)ValuePtr);
			rec->length = (SQLULEN)(uintptr_t)ValuePtr;
			break;

		/* <SQLSMALLINT> */
		do {
		case SQL_DESC_DATETIME_INTERVAL_CODE:
			wordp = &rec->datetime_interval_code; break;
		case SQL_DESC_FIXED_PREC_SCALE: wordp = &rec->fixed_prec_scale; break;
		case SQL_DESC_NULLABLE: wordp = &rec->nullable; break;
		case SQL_DESC_PARAMETER_TYPE: wordp = &rec->parameter_type; break;
		case SQL_DESC_PRECISION: wordp = &rec->precision; break;
		case SQL_DESC_ROWVER: wordp = &rec->rowver; break;
		case SQL_DESC_SCALE: wordp = &rec->scale; break;
		case SQL_DESC_SEARCHABLE: wordp = &rec->searchable; break;
		case SQL_DESC_UNNAMED:
			/* only driver can set this value */
			if ((SQLSMALLINT)(intptr_t)ValuePtr == SQL_NAMED) {
				ERR("only the driver can set %d field to 'SQL_NAMED'.", 
						FieldIdentifier);
				RET_HDIAGS(desc, SQL_STATE_HY091);
			}
			wordp = &rec->unnamed;
			break;
		case SQL_DESC_UNSIGNED: wordp = &rec->usigned; break;
		case SQL_DESC_UPDATABLE: wordp = &rec->updatable; break;
		} while (0);
			DBG("setting record field %d to %d.", FieldIdentifier, 
					(SQLSMALLINT)(intptr_t)ValuePtr);
			*wordp = (SQLSMALLINT)(intptr_t)ValuePtr;
			break;

		/* <SQLINTEGER> */
		do {
		case SQL_DESC_AUTO_UNIQUE_VALUE: intp = &rec->auto_unique_value; break;
		case SQL_DESC_CASE_SENSITIVE: intp = &rec->case_sensitive; break;
		case SQL_DESC_DATETIME_INTERVAL_PRECISION: 
			intp = &rec->datetime_interval_precision; break;
		case SQL_DESC_NUM_PREC_RADIX: intp = &rec->num_prec_radix; break;
		} while (0);
			DBG("returning record field %d as %d.", FieldIdentifier,
					(SQLINTEGER)(intptr_t)ValuePtr);
			*intp = (SQLINTEGER)(intptr_t)ValuePtr;
			break;

		default:
			ERR("unknown FieldIdentifier: %d.", FieldIdentifier);
			RET_HDIAGS(desc, SQL_STATE_HY091);
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

	// TODO: needs to trigger consistency_check

	RET_NOT_IMPLEMENTED;
}


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
