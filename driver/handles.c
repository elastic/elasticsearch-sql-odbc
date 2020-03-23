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


static void free_rec_fields(esodbc_rec_st *rec)
{
	int i;
	SQLWCHAR **wptr[] = {
		&rec->base_column_name.str,
		&rec->base_table_name.str,
		&rec->catalog_name.str,
		&rec->label.str,
		&rec->name.str,
		&rec->schema_name.str,
		&rec->table_name.str,
	};
	for (i = 0; i < sizeof(wptr)/sizeof(wptr[0]); i ++) {
		if (*wptr[i]) {
			DBGH(rec->desc, "freeing field #%d = 0x%p.", i, *wptr[i]);
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
		for (i = 0; i < desc->count; i++) {
			free_rec_fields(&desc->recs[i]);
		}
	free(desc->recs);
	desc->recs = NULL;
	desc->count = 0;
}

static void init_hheader(esodbc_hhdr_st *hdr, SQLSMALLINT type, void *parent)
{
	hdr->type = type;
	init_diagnostic(&hdr->diag);
	ESODBC_MUX_INIT(&hdr->mutex);
	hdr->parent = parent;

	/* init logging helpers */
	switch(type) {
		case SQL_HANDLE_ENV:
			hdr->typew = MK_WSTR("ENV");
			hdr->log = _gf_log; /* use global logger */
			break;
		case SQL_HANDLE_DBC:
			hdr->typew = MK_WSTR("DBC");
			hdr->log = _gf_log; /* use global logger, by default */
			break;
		case SQL_HANDLE_STMT:
			hdr->typew = MK_WSTR("STMT");
			hdr->log = HDRH(parent)->log; /* inherit */
			break;
		case SQL_HANDLE_DESC:
			hdr->typew = MK_WSTR("DESC");
			hdr->log = HDRH(parent)->log; /* inherit */
			break;
		default:
			assert(0);
	}
}

/*
 *  "un-static" the initializers as needed
 */
static inline void init_env(esodbc_env_st *env)
{
	memset(env, 0, sizeof(*env));
	init_hheader(HDRH(env), SQL_HANDLE_ENV, NULL);
}

void init_dbc(esodbc_dbc_st *dbc, SQLHANDLE InputHandle)
{
	memset(dbc, 0, sizeof(*dbc));
	init_hheader(HDRH(dbc), SQL_HANDLE_DBC, InputHandle);

	dbc->metadata_id = SQL_FALSE;
	ESODBC_MUX_INIT(&dbc->curl_mux);
	/* rest of initialization done at connect time */
}

static void init_desc(esodbc_desc_st *desc, esodbc_stmt_st *stmt,
	desc_type_et type, SQLSMALLINT alloc_type)
{
	memset(desc, 0, sizeof(esodbc_desc_st));
	/* mandatory defaulting to NULL/0: array_status_ptr, bind_offset_ptr,
	 * rows_processed_ptr, count */

	init_hheader(&desc->hdr, SQL_HANDLE_DESC, stmt);

	desc->alloc_type = alloc_type;
	/* user allocated descriptors are reset to ANON */
	desc->type = alloc_type == SQL_DESC_ALLOC_USER ? DESC_TYPE_ANON : type;

	desc->array_size = ESODBC_DEF_ARRAY_SIZE;
	if (DESC_TYPE_IS_APPLICATION(type)) {
		desc->bind_type = SQL_BIND_BY_COLUMN;
	}
}

static void clear_desc(esodbc_desc_st *desc, BOOL reinit)
{
	DBGH(desc, "clearing desc type %d.", desc->type);
	switch (desc->type) {
		case DESC_TYPE_ANON:
		case DESC_TYPE_ARD:
		case DESC_TYPE_APD:
		case DESC_TYPE_IPD:
			break;

		case DESC_TYPE_IRD:
			if (STMT_HAS_CURSOR(HDRH(desc)->stmt)) {
				close_es_cursor(HDRH(desc)->stmt);
			}
			if (STMT_HAS_RESULTSET(desc->hdr.stmt)) {
				clear_resultset(desc->hdr.stmt, reinit);
			}
			break;
		default:
			BUG("no such descriptor of type %d.", desc->type);
			return;
	}
	if (desc->recs) {
		free_desc_recs(desc);
	}
	if (reinit) {
		init_desc(desc, desc->hdr.stmt, desc->type, desc->alloc_type);
	}
}

static void init_stmt(esodbc_stmt_st *stmt, SQLHANDLE InputHandle)
{
	memset(stmt, 0, sizeof(*stmt));
	init_hheader(HDRH(stmt), SQL_HANDLE_STMT, InputHandle);

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
	/* TODO: change to SQL_UB_DEFAULT when supporting bookmarks */
	stmt->bookmarks = SQL_UB_OFF;
	/* inherit this connection-statement attributes
	 * Note: these attributes won't propagate at statement level when
	 * set at connection level. */
	stmt->metadata_id = DBCH(InputHandle)->metadata_id;
	stmt->sql2c_conversion = CONVERSION_UNCHECKED;
}

void dump_record(esodbc_rec_st *rec)
{
	DBGH(rec->desc, "Dumping REC@0x%p", rec);
#define DUMP_FIELD(_strp, _name, _desc) \
	DBGH(rec->desc, "0x%p->%s: `" _desc "`.", _strp, # _name, (_strp)->_name)

	// TODO: add dumping for the es_type?

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
	DUMP_FIELD(rec, name, LWPD);
	DUMP_FIELD(rec, schema_name, LWPD);
	DUMP_FIELD(rec, table_name, LWPD);

	DUMP_FIELD(rec, indicator_ptr, "0x%p");
	DUMP_FIELD(rec, octet_length_ptr, "0x%p");

	DUMP_FIELD(rec, octet_length, "%lld");
	DUMP_FIELD(rec, length, "%llu");

	DUMP_FIELD(rec, datetime_interval_precision, "%d");
	DUMP_FIELD(rec, num_prec_radix, "%d");

	DUMP_FIELD(rec, parameter_type, "%d");
	DUMP_FIELD(rec, precision, "%d");
	DUMP_FIELD(rec, rowver, "%d");
	DUMP_FIELD(rec, scale, "%d");
	DUMP_FIELD(rec, unnamed, "%d");
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
	if (! OutputHandle) {
		ERR("null output handle provided.");
		RET_STATE(SQL_STATE_HY009);
	}

	if (HandleType != SQL_HANDLE_ENV) {
		if (! InputHandle) {
			ERR("null input handle provided");
			RET_STATE(SQL_STATE_HY009);
		}
	} else {
		if (InputHandle) {
			ERR("not null (@0x%p) input handle for Environment.", InputHandle);
			/* not fatal a.t.p., tho it'll likely fail (->error level) */
		}
	}

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
			*OutputHandle = (SQLHANDLE)malloc(sizeof(esodbc_env_st));
			if (*OutputHandle) {
				init_env(*OutputHandle);
				DBG("new Environment handle @0x%p.", *OutputHandle);
			}
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
				ERRH(InputHandle, "no version set in ENV.");
				RET_HDIAG(InputHandle, SQL_STATE_HY010,
					"enviornment has no version set", 0);
			}
			*OutputHandle = (SQLHANDLE)malloc(sizeof(esodbc_dbc_st));
			if (*OutputHandle) {
				init_dbc(*OutputHandle, InputHandle);
				DBGH(InputHandle, "new Connection @0x%p.", *OutputHandle);
			}
			break;

		case SQL_HANDLE_STMT: /* Statement Handle */
			*OutputHandle = (SQLHANDLE)malloc(sizeof(esodbc_stmt_st));
			if (*OutputHandle) {
				init_stmt(*OutputHandle, InputHandle);
				DBGH(InputHandle, "new Statement @0x%p.", *OutputHandle);
			}
			break;

		case SQL_HANDLE_DESC: /* Descriptor Handle */
			*OutputHandle = (SQLHANDLE)malloc(sizeof(esodbc_desc_st));
			if (*OutputHandle) {
				init_desc(*OutputHandle, InputHandle, DESC_TYPE_ANON,
					SQL_DESC_ALLOC_USER);
				DBGH(InputHandle, "new Statement @0x%p.", *OutputHandle);
			}
			break;

		case SQL_HANDLE_SENV: /* Shared Environment Handle */
			// case SQL_HANDLE_DBC_INFO_TOKEN:
			ERR("handle type %hd not implemented.", HandleType);
			RET_STATE(SQL_STATE_HYC00); // optional feature

		default:
			ERR("unknown HandleType: %d.", HandleType);
			return SQL_INVALID_HANDLE;
	}

	if (! *OutputHandle) {
		ERRN("OOM for handle type %hd, on input handle@0x%p.", HandleType,
			InputHandle);
		if (InputHandle) {
			RET_HDIAGS(InputHandle, SQL_STATE_HY001);
		} else {
			RET_STATE(SQL_STATE_HY001);
		}
	} else {
		/* new handle has been init'ed */
		assert(HDRH(*OutputHandle)->type);
	}

	return SQL_SUCCESS;
}

SQLRETURN EsSQLFreeHandle(SQLSMALLINT HandleType, SQLHANDLE Handle)
{
	esodbc_dbc_st *dbc;
	esodbc_stmt_st *stmt;
	esodbc_desc_st *desc;

	if (! Handle) {
		ERR("provided null Handle.");
		RET_STATE(SQL_STATE_HY009);
	}

	switch(HandleType) {
		case SQL_HANDLE_ENV: /* Environment Handle */
			break;
		case SQL_HANDLE_DBC: /* Connection Handle */
			dbc = DBCH(Handle);
			/* app/DM should have SQLDisconnect'ed, but just in case  */
			cleanup_dbc(dbc);
			ESODBC_MUX_DEL(&dbc->curl_mux);
			break;
		case SQL_HANDLE_STMT:
			stmt = STMH(Handle);

			detach_sql(stmt);

			clear_desc(stmt->ard, FALSE);
			clear_desc(stmt->ird, FALSE);
			clear_desc(stmt->apd, FALSE);
			clear_desc(stmt->ipd, FALSE);
			break;

		// FIXME:
		/* "When an explicitly allocated descriptor is freed, all statement
		 * handles to which the freed descriptor applied automatically revert
		 * to the descriptors implicitly allocated for them." */
		case SQL_HANDLE_DESC:
			desc = DSCH(Handle);
			clear_desc(desc, FALSE);
			break;

		case SQL_HANDLE_SENV: /* Shared Environment Handle */
			RET_STATE(SQL_STATE_HYC00);
#if 0
		case SQL_HANDLE_DBC_INFO_TOKEN:
			//break;
#endif
		default:
			ERR("unknown HandleType: %d.", HandleType);
			return SQL_INVALID_HANDLE;
	}

	ESODBC_MUX_DEL(&HDRH(Handle)->mutex);
	free(Handle);
	return SQL_SUCCESS;
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
			ERRH(stmt, "DROPing is deprecated -- no action taken! "
				"(might leak memory)");
			//return SQLFreeHandle(SQL_HANDLE_STMT,(SQLHANDLE)StatementHandle);
			break;

		/* "This does not unbind the bookmark column; to do that, the
		 * SQL_DESC_DATA_PTR field of the ARD for the bookmark column is set
		 * to NULL." TODO, with bookmarks. */
		case SQL_UNBIND:
			DBGH(stmt, "unbinding.");
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
			DBGH(stmt, "ES-closing.");
			detach_sql(stmt);
		/* no break */
		/* "Closes the cursor associated with StatementHandle and discards all
		 * pending results." */
		case SQL_CLOSE:
			DBGH(stmt, "closing.");
			clear_desc(stmt->ird, TRUE);
			break;

		/* "Sets the SQL_DESC_COUNT field of the APD to 0, releasing all
		 * parameter buffers set by SQLBindParameter for the given
		 * StatementHandle." */
		case SQL_RESET_PARAMS:
			DBGH(stmt, "resetting params.");
			init_diagnostic(&stmt->hdr.diag);
			ret = EsSQLSetDescFieldW(stmt->apd, NO_REC_NR, SQL_DESC_COUNT,
					(SQLPOINTER)0, SQL_IS_SMALLINT);
			if (! SQL_SUCCEEDED(ret)) {
				/* copy error at top handle level, where it's going to be
				 * inquired from */
				HDIAG_COPY(stmt->ard, stmt);
			}
			/* docs aren't explicit when should the IPD be cleared; but since
			 * execution of a statement is independent from params binding,
			 * the IPD and APD clearning can only be linked together. */
			ret = EsSQLSetDescFieldW(stmt->ipd, NO_REC_NR, SQL_DESC_COUNT,
					(SQLPOINTER)0, SQL_IS_SMALLINT);
			if (! SQL_SUCCEEDED(ret)) {
				HDIAG_COPY(stmt->ird, stmt);
			}
			RET_STATE(stmt->hdr.diag.state);

		default:
			ERRH(stmt, "unknown Option value: %d.", Option);
			RET_HDIAGS(STMH(StatementHandle), SQL_STATE_HY092);

	}

	return SQL_SUCCESS;
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
		case SQL_ATTR_CONNECTION_POOLING:
		case SQL_ATTR_CP_MATCH:
			RET_HDIAG(ENVH(EnvironmentHandle), SQL_STATE_HYC00,
				"Connection pooling not supported", 0);

		case SQL_ATTR_ODBC_VERSION:
			switch ((intptr_t)Value) {
				// supporting applications of 2.x and 3.x<3.8 needs extensive
				// review of the options.
				case SQL_OV_ODBC2:
				case SQL_OV_ODBC3:
					WARNH(EnvironmentHandle, "application version %d not fully"
						" supported.", (intptr_t)Value);
				case SQL_OV_ODBC3_80:
					break;
				default:
					ERRH(EnvironmentHandle, "application version %zd not "
						"supported.", (intptr_t)Value);
					RET_HDIAG(ENVH(EnvironmentHandle), SQL_STATE_HYC00,
						"application version not supported", 0);
			}
			ENVH(EnvironmentHandle)->version = (SQLUINTEGER)(uintptr_t)Value;
			INFOH(EnvironmentHandle, "set version to %u.",
				ENVH(EnvironmentHandle)->version);
			break;

		/* "If SQL_TRUE, the driver returns string data null-terminated" */
		case SQL_ATTR_OUTPUT_NTS:
			if ((intptr_t)Value != SQL_TRUE)
				RET_HDIAG(ENVH(EnvironmentHandle), SQL_STATE_HYC00,
					"Driver always returns null terminated strings", 0);
			break;

		default:
			ERRH(EnvironmentHandle, "unsupported Attribute value: %d.",
				Attribute);
			RET_HDIAG(ENVH(EnvironmentHandle), SQL_STATE_HY024,
				"Unsupported attribute value", 0);
	}

	return SQL_SUCCESS;
}

SQLRETURN EsSQLGetEnvAttr(SQLHENV EnvironmentHandle, SQLINTEGER Attribute,
	_Out_writes_(_Inexpressible_(BufferLength)) SQLPOINTER Value,
	SQLINTEGER BufferLength, _Out_opt_ SQLINTEGER *StringLength)
{
	switch (Attribute) {
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
			ERRH(EnvironmentHandle, "unsupported Attribute value: %d.",
				Attribute);
			RET_HDIAG(ENVH(EnvironmentHandle), SQL_STATE_HY024,
				"Unsupported attribute value", 0);
	}

	return SQL_SUCCESS;
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

	/*INDENT-OFF*/
	switch(Attribute) {
		case SQL_ATTR_USE_BOOKMARKS:
			DBGH(stmt, "setting use-bookmarks to: %u.", (SQLULEN)ValuePtr);
			if ((SQLULEN)ValuePtr != SQL_UB_OFF) {
				ERRH(stmt, "bookmarks are not supported by driver.");
				RET_HDIAG(stmt, SQL_STATE_HYC00,
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
			DBGH(stmt, "setting row-bind-offset pointer to: 0x%p.", ValuePtr);
			desc = stmt->ard;
			break;
		case SQL_ATTR_PARAM_BIND_OFFSET_PTR:
			DBGH(stmt, "setting param-bind-offset pointer to: 0x%p.", ValuePtr);
			desc = stmt->apd;
			break;
		} while (0);
			ret = EsSQLSetDescFieldW(desc, NO_REC_NR, SQL_DESC_BIND_OFFSET_PTR,
					ValuePtr, BufferLength);
			if (ret != SQL_SUCCESS) { /* _WITH_INFO wud be "error" here */
				/* if SetDescField() fails, DM will check statement's diag */
				HDIAG_COPY(desc, stmt);
			}
			return ret;

		do {
		/* "Setting this statement attribute sets the SQL_DESC_ARRAY_SIZE
		 * field in the ARD header." */
		case SQL_ATTR_ROW_ARRAY_SIZE:
			DBGH(stmt, "setting row array size to: %d.", (SQLULEN)ValuePtr);
			desc = stmt->ard;
			break;
		/* "Setting this statement attribute sets the SQL_DESC_ARRAY_SIZE
		 * field in the APD header." */
		case SQL_ATTR_PARAMSET_SIZE:
			DBGH(stmt, "setting param set size to: %d.", (SQLULEN)ValuePtr);
			desc = stmt->apd;
			break;
		} while (0);
			ret = EsSQLSetDescFieldW(desc, NO_REC_NR, SQL_DESC_ARRAY_SIZE,
					ValuePtr, BufferLength);
			if (ret != SQL_SUCCESS) { /* _WITH_INFO wud be "error" here */
				/* if SetDescField() fails, DM will check statement's diag */
				HDIAG_COPY(desc, stmt);
			}
			return ret;

		do {
		/* "sets the binding orientation to be used when SQLFetch or
		 * SQLFetchScroll is called on the associated statement" */
		/* "Setting this statement attribute sets the SQL_DESC_BIND_TYPE field
		 * in the ARD header." */
		case SQL_ATTR_ROW_BIND_TYPE:
			DBGH(stmt, "setting row bind type to: %d.", (SQLULEN)ValuePtr);
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
			DBGH(stmt, "setting param bind type to: %d.", (SQLULEN)ValuePtr);
			desc = stmt->apd;
			break;
		} while (0);
			ret = EsSQLSetDescFieldW(desc, NO_REC_NR, SQL_DESC_BIND_TYPE,
					/* note: SetStmt()'s spec defineds the ValuePtr as
					 * SQLULEN, but SetDescField()'s as SQLUINTEGER.. */
					ValuePtr, BufferLength);
			if (ret != SQL_SUCCESS) { /* _WITH_INFO wud be "error" here */
				/* if SetDescField() fails, DM will check statement's diag */
				HDIAG_COPY(desc, stmt);
			}
			return ret;

		do {
		/* "an array of SQLUSMALLINT values containing row status values after
		 * a call to SQLFetch or SQLFetchScroll." */
		/* https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/row-status-array */
		/* "Setting this statement attribute sets the
		 * SQL_DESC_ARRAY_STATUS_PTR field in the IRD header." */
		case SQL_ATTR_ROW_STATUS_PTR:
			// TODO: call SQLSetDescField(IRD) here?
			DBGH(stmt, "setting row status pointer to: 0x%p.", ValuePtr);
			desc = stmt->ird;
			break;
		case SQL_ATTR_PARAM_STATUS_PTR:
			DBGH(stmt, "setting param status pointer to: 0x%p.", ValuePtr);
			desc = stmt->ipd;
			break;
		case SQL_ATTR_ROW_OPERATION_PTR:
			DBGH(stmt, "setting row operation array pointer to: 0x%p.",
					ValuePtr);
			desc = stmt->ard;
			break;
		case SQL_ATTR_PARAM_OPERATION_PTR:
			DBGH(stmt, "setting param operation array pointer to: 0x%p.",
					ValuePtr);
			desc = stmt->apd;
			break;
		} while (0);
			ret = EsSQLSetDescFieldW(desc, NO_REC_NR,
					SQL_DESC_ARRAY_STATUS_PTR, ValuePtr, BufferLength);
			if (ret != SQL_SUCCESS) { /* _WITH_INFO wud be "error" here */
				/* if SetDescField() fails, DM will check statement's diag */
				HDIAG_COPY(desc, stmt);
			}
			return ret;

		do {
		/* "Setting this statement attribute sets the
		 * SQL_DESC_ROWS_PROCESSED_PTR field in the IRD header." */
		case SQL_ATTR_ROWS_FETCHED_PTR:
			DBGH(stmt, "setting rows fetched pointer to: 0x%p.", ValuePtr);
			/* NOTE: documentation writes about "ARD", while also stating that
			 * this field is unused in the ARD. I assume the former as wrong */
			desc = stmt->ird;
			break;
		case SQL_ATTR_PARAMS_PROCESSED_PTR:
			DBGH(stmt, "setting params processed pointer to: 0x%p.", ValuePtr);
			desc = stmt->ipd;
			break;
		} while (0);
			ret = EsSQLSetDescFieldW(desc, NO_REC_NR,
					SQL_DESC_ROWS_PROCESSED_PTR, ValuePtr, BufferLength);
			if (ret != SQL_SUCCESS) { /* _WITH_INFO wud be "error" here */
				/* if SetDescField() fails, DM will check statement's diag */
				HDIAG_COPY(desc, stmt);
			}
			return ret;

		case SQL_ATTR_APP_ROW_DESC:
			desc = DSCH(ValuePtr);
			if (desc == stmt->ard) {
				WARNH(stmt, "trying to overwrite ARD with same value (@0x%p).",
						ValuePtr);
				break; /* nop */
			}
			if (desc == &stmt->i_ard || desc == SQL_NULL_HDESC) {
				DBGH(stmt, "unbinding current ARD (@0x%p), rebinding with "
						"implicit value (@0x%p).", stmt->ard, &stmt->i_ard);
				/* re-anonymize the descriptor, makingit re-usable */
				stmt->ard = &stmt->i_ard;
			} else {
				/* "This attribute cannot be set to a descriptor handle that
				 * was implicitly allocated for another statement or to
				 * another descriptor handle that was implicitly set on the
				 * same statement; implicitly allocated descriptor handles
				 * cannot be associated with more than one statement or
				 * descriptor handle." */
				if (desc->alloc_type == SQL_DESC_ALLOC_AUTO) {
					ERRH(stmt, "source ARD (@0x%p) is implicit (alloc: %d).",
							desc, desc->alloc_type);
					RET_HDIAGS(stmt, SQL_STATE_HY017);
				} else {
					switch (desc->type) {
						case DESC_TYPE_ANON:
							desc->type = DESC_TYPE_ARD;
						case DESC_TYPE_ARD:
							break;
						default:
							// TODO: should this be allowed?
							/* this means a descriptor can not be changed from
							 * APD to ARD (IxD is also ruled out) */
							ERRH(stmt, "can't convert descriptor from type %d"
									" to ARD.", desc->type);
							RET_HDIAGS(stmt, SQL_STATE_HY024);
					}
					DBGH(stmt, "overwritting current ARD (@0x%p) with new "
							"value (@0x%p).", stmt->ard, desc);
					stmt->ard = desc;
				}
			}
			break;
		case SQL_ATTR_APP_PARAM_DESC:
			// FIXME: same logic for ARD as above (part of params passing)
			FIXME;
			break;

		case SQL_ATTR_IMP_ROW_DESC:
		case SQL_ATTR_IMP_PARAM_DESC:
			ERRH(stmt, "trying to set IxD (%d) descriptor (to @0x%p).",
					Attribute, ValuePtr);
			RET_HDIAGS(stmt, SQL_STATE_HY017);

		case SQL_ATTR_ROW_NUMBER:
			ERRH(stmt, "row number attribute is read-only.");
			RET_HDIAGS(stmt, SQL_STATE_HY024);

		case SQL_ATTR_METADATA_ID:
			DBGH(stmt, "setting metadata_id to: %u", (SQLULEN)ValuePtr);
			stmt->metadata_id = (SQLULEN)ValuePtr;
			break;

		case SQL_ATTR_ASYNC_ENABLE:
			ERRH(stmt, "no support for async API (setting param: %llu)",
					(SQLULEN)(uintptr_t)ValuePtr);
			if ((SQLULEN)(uintptr_t)ValuePtr == SQL_ASYNC_ENABLE_ON) {
				RET_HDIAGS(stmt, SQL_STATE_HYC00);
			}
			break;
		case SQL_ATTR_ASYNC_STMT_EVENT:
		// case SQL_ATTR_ASYNC_STMT_PCALLBACK:
		// case SQL_ATTR_ASYNC_STMT_PCONTEXT:
			ERRH(stmt, "no support for async API (attr: %ld)", Attribute);
			RET_HDIAGS(stmt, SQL_STATE_S1118);

		case SQL_ATTR_MAX_LENGTH:
			ulen = (SQLULEN)ValuePtr;
			DBGH(stmt, "setting max_length to: %u.", ulen);
			if (ulen < ESODBC_LO_MAX_LENGTH) {
				WARNH(stmt, "MAX_LENGTH lower than min allowed (%d) -- "
						"correcting value.", ESODBC_LO_MAX_LENGTH);
				ulen = ESODBC_LO_MAX_LENGTH;
			} else if (ESODBC_UP_MAX_LENGTH && ESODBC_UP_MAX_LENGTH < ulen) {
				WARNH(stmt, "MAX_LENGTH higher than max allowed (%d) -- "
						"correcting value", ESODBC_UP_MAX_LENGTH);
				ulen = ESODBC_UP_MAX_LENGTH;
			}
			stmt->max_length = ulen;
			if (ulen != (SQLULEN)ValuePtr)
				RET_HDIAGS(stmt, SQL_STATE_01S02);
			break;

		case SQL_ATTR_QUERY_TIMEOUT:
			DBGH(stmt, "setting query timeout to: %u.", (SQLULEN)ValuePtr);
			stmt->query_timeout = (SQLULEN)ValuePtr;
			break;

		case SQL_ATTR_CURSOR_TYPE:
			DBGH(stmt, "setting cursor type: %llu.", (SQLULEN)ValuePtr);
			if ((SQLULEN)ValuePtr != SQL_CURSOR_FORWARD_ONLY) {
				WARNH(stmt, "requested cursor_type substituted with "
						"forward-only (%llu).", SQL_CURSOR_FORWARD_ONLY);
				RET_HDIAGS(stmt, SQL_STATE_01S02);
			}
			break;

		case SQL_ATTR_NOSCAN:
			DBGH(stmt, "setting escape seq scanning: %llu -- NOOP.",
					(SQLULEN)ValuePtr);
			/* nothing to do: the driver never scans the input, ESSQL processes
			 * the escape sequences */
			break;

		case SQL_ATTR_CONCURRENCY:
			DBGH(stmt, "setting concurrency: %llu.", (SQLULEN)ValuePtr);
			if ((SQLULEN)ValuePtr != SQL_CONCUR_READ_ONLY) {
				WARNH(stmt, "requested concurrency substituted with "
						"read-only (%llu).", SQL_CONCUR_READ_ONLY);
				RET_HDIAGS(stmt, SQL_STATE_01S02);
			}
			break;

		case SQL_ATTR_MAX_ROWS:
			DBGH(stmt, "setting max rows: %llu.", (SQLULEN)ValuePtr);
			if ((SQLULEN)ValuePtr != 0) {
				WARNH(stmt, "requested max_rows substituted with 0.");
				RET_HDIAGS(stmt, SQL_STATE_01S02);
			}
			break;

		case SQL_ATTR_CURSOR_SENSITIVITY:
			DBGH(stmt, "setting cursor sensitivity: %llu.", (SQLULEN)ValuePtr);
			if ((SQLULEN)ValuePtr != SQL_UNSPECIFIED) {
				ERRH(stmt, "driver supports forward-only cursors.");
				RET_HDIAGS(stmt, SQL_STATE_HYC00);
			}
			break;

		case SQL_ATTR_CURSOR_SCROLLABLE:
			DBGH(stmt, "setting scrollable cursor: %llu.", (SQLULEN)ValuePtr);
			if ((SQLULEN)ValuePtr != SQL_NONSCROLLABLE) {
				ERRH(stmt, "driver supports only non-scrollable cursors.");
				RET_HDIAGS(stmt, SQL_STATE_HYC00);
			}
			break;

		case SQL_ATTR_RETRIEVE_DATA:
			DBGH(stmt, "setting data retrieving: %llu.", (SQLULEN)ValuePtr);
			if ((SQLULEN)ValuePtr != SQL_RD_ON) {
				WARNH(stmt, "no fetching without data retrieval possible.");
				RET_HDIAGS(stmt, SQL_STATE_01S02);
			}
			break;

		/* SQL Server non-standard attributes */
		case 1226:
		case 1227:
		case 1228:
			ERRH(stmt, "non-standard attribute: %d.", Attribute);
			/* "Invalid attribute/option identifier" */
			RET_HDIAGS(stmt, SQL_STATE_HY092);

		default:
			// FIXME
			BUGH(stmt, "unknown Attribute: %d.", Attribute);
			RET_HDIAGS(stmt, SQL_STATE_HY092);
	}
	/*INDENT-ON*/

	return SQL_SUCCESS;
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

	/*INDENT-OFF*/
	switch (Attribute) {
		do {
		case SQL_ATTR_APP_ROW_DESC: desc = stmt->ard; break;
		case SQL_ATTR_APP_PARAM_DESC: desc = stmt->apd; break;
		case SQL_ATTR_IMP_ROW_DESC: desc = stmt->ird; break;
		case SQL_ATTR_IMP_PARAM_DESC: desc = stmt->ipd; break;
		} while (0);
			DBGH(stmt, "getting descriptor (attr type %d): 0x%p.", Attribute,
					desc);
			*(SQLPOINTER *)ValuePtr = desc;
			break;

		case SQL_ATTR_METADATA_ID:
			DBGH(stmt, "getting metadata_id: %llu", stmt->metadata_id);
			*(SQLULEN *)ValuePtr = stmt->metadata_id;
			break;
		case SQL_ATTR_ASYNC_ENABLE:
			DBGH(stmt, "getting async mode: %llu", SQL_ASYNC_ENABLE_OFF);
			*(SQLULEN *)ValuePtr = SQL_ASYNC_ENABLE_OFF;
			break;
		case SQL_ATTR_MAX_LENGTH:
			DBGH(stmt, "getting max_length: %llu", stmt->max_length);
			*(SQLULEN *)ValuePtr = stmt->max_length;
			break;
		case SQL_ATTR_QUERY_TIMEOUT:
			DBGH(stmt, "getting query timeout: %llu", stmt->query_timeout);
			*(SQLULEN *)ValuePtr = stmt->query_timeout;
			break;

		/* "determine the number of the current row in the result set" */
		case SQL_ATTR_ROW_NUMBER:
			*(SQLULEN *)ValuePtr = (SQLULEN)stmt->tv_rows;
			DBGH(stmt, "getting row number: %llu", *(SQLULEN *)ValuePtr);
			break;

		case SQL_ATTR_CURSOR_TYPE:
			DBGH(stmt, "getting cursor type: %llu.", SQL_CURSOR_FORWARD_ONLY);
			/* we only support forward only cursor, so far - TODO */
			*(SQLULEN *)ValuePtr = SQL_CURSOR_FORWARD_ONLY;
			break;

		do {
		/* "Setting this statement attribute sets the SQL_DESC_ARRAY_SIZE
		 * field in the ARD header." */
		case SQL_ATTR_ROW_ARRAY_SIZE:
			DBGH(stmt, "getting row array size.");
			desc = stmt->ard;
			break;
		/* "Setting this statement attribute sets the SQL_DESC_ARRAY_SIZE
		 * field in the APD header." */
		case SQL_ATTR_PARAMSET_SIZE:
			DBGH(stmt, "getting param set size.");
			desc = stmt->apd;
			break;
		} while (0);
			/* " If Attribute is an ODBC-defined attribute and *ValuePtr is an
			 * integer, BufferLength is ignored." */
			ret = EsSQLGetDescFieldW(desc, NO_REC_NR, SQL_DESC_ARRAY_SIZE,
					ValuePtr, SQL_IS_UINTEGER, NULL);
			if (ret != SQL_SUCCESS) { /* _WITH_INFO wud be "error" here */
				/* if SetDescField() fails, DM will check statement's diag */
				HDIAG_COPY(desc, stmt);
			}
			return ret;

		case SQL_ATTR_USE_BOOKMARKS:
			DBGH(stmt, "getting use-bookmarks: %llu.", SQL_UB_OFF);
			*(SQLULEN *)ValuePtr = SQL_UB_OFF;
			break;

		case SQL_ATTR_NOSCAN:
			DBGH(stmt, "getting noscan: %llu.", SQL_NOSCAN_OFF);
			*(SQLULEN *)ValuePtr = SQL_NOSCAN_OFF;
			break;

		case SQL_ATTR_CONCURRENCY:
			DBGH(stmt, "getting concurrency: %llu.", SQL_CONCUR_READ_ONLY);
			*(SQLULEN *)ValuePtr = SQL_CONCUR_READ_ONLY;
			break;

		case SQL_ATTR_MAX_ROWS:
			DBGH(stmt, "getting max rows: 0.");
			*(SQLULEN *)ValuePtr = 0;
			break;

		case SQL_ATTR_CURSOR_SENSITIVITY:
			DBGH(stmt, "getting cursor sensitivity: %llu.", SQL_UNSPECIFIED);
			*(SQLULEN *)ValuePtr = SQL_UNSPECIFIED;
			break;

		case SQL_ATTR_CURSOR_SCROLLABLE:
			DBGH(stmt, "getting scrollable cursor: %llu.", SQL_NONSCROLLABLE);
			*(SQLULEN *)ValuePtr = SQL_NONSCROLLABLE;
			break;

		case SQL_ATTR_RETRIEVE_DATA:
			DBGH(stmt, "getting data retrieving: %llu.", SQL_RD_ON);
			*(SQLULEN *)ValuePtr = SQL_RD_ON;
			break;

		default:
			ERRH(stmt, "unknown attribute: %d.", Attribute);
			RET_HDIAGS(stmt, SQL_STATE_HY092);
	}
	/*INDENT-ON*/

	return SQL_SUCCESS;
}
/*INDENT-ON*/


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
				ERR("buffer not alligned to SQLWCHAR size (%d).",
					buff_len);
				return SQL_STATE_HY090;
			}
			if ((! writable) && (ESODBC_MAX_IDENTIFIER_LEN < buff_len)) {
				ERR("trying to set field %d to a string buffer larger "
					"than max allowed (%d).", field_id,
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
				ERR("buffer is for binary buffer, but length indicator "
					"is positive (%d).", buff_len);
				return SQL_STATE_HY090;
			}
			return SQL_STATE_00000;

		/* pointer to a value other than string or binary string */
		case SQL_DESC_DATA_PTR:
			if ((buff_len != SQL_IS_POINTER) && (buff_len < 0)) {
				/* according to the spec the length "should" be its size =>
				 * this check might be too strict? */
				ERR("buffer is for pointer, but its length indicator "
					"doesn't match (%d).", buff_len);
				return SQL_STATE_HY090;
			}
			return SQL_STATE_00000;
	}

	if (! writable)
		/* this call is from SetDescField, so length for integer types are
		 * ignored */
	{
		return SQL_STATE_00000;
	}

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
				ERR("buffer is for interger, but its length indicator "
					"doesn't match (%d).", buff_len);
				return SQL_STATE_HY090;
			}
			break;

		/* SQL_IS_UINTEGER */
		case SQL_DESC_ARRAY_SIZE:
		case SQL_DESC_LENGTH:
			if (buff_len != SQL_IS_UINTEGER) {
				ERR("buffer is for uint, but its length indicator "
					"doesn't match (%d).", buff_len);
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
				ERR("buffer is for short, but its length indicator "
					"doesn't match (%d).", buff_len);
				return SQL_STATE_HY090;
			}
			break;

		default:
			ERR("unknown field identifier: %d.", field_id);
			return SQL_STATE_HY091;
	}

	return SQL_STATE_00000;
}

/*INDENT-OFF*/
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
 * SQL_DESC_DATA_PTR		rw	rw		[rw]
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
/*INDENT-ON*/
// TODO: individual tests just for this
/*
 * Check access to record headers/fields.
 * IxD must have es_type pointer set and it's value is not checked at
 * header/field access time -> only this function guards against NP deref'ing.
 */
static BOOL check_access(esodbc_desc_st *desc, SQLSMALLINT field_id,
	char mode /* O_RDONLY | O_RDWR */)
{
	BOOL ret;
	desc_type_et desc_type = desc->type;

	if (desc_type == DESC_TYPE_ANON) {
		BUGH(desc, "can't check permissions against ANON descryptor type.");
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
			ret = DESC_TYPE_IS_IMPLEMENTATION(desc_type);
			break;
		case SQL_DESC_PARAMETER_TYPE:
			ret = desc_type == DESC_TYPE_IPD;
			break;

		case SQL_DESC_ARRAY_SIZE:
		case SQL_DESC_BIND_OFFSET_PTR:
		case SQL_DESC_BIND_TYPE:
		case SQL_DESC_INDICATOR_PTR:
		case SQL_DESC_OCTET_LENGTH_PTR:
			ret = DESC_TYPE_IS_APPLICATION(desc_type);
			break;

		case SQL_DESC_DATA_PTR:
			/* "The SQL_DESC_DATA_PTR field in the IPD can be set to force a
			 * consistency check." */
			ret = desc_type != DESC_TYPE_IRD;
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
			ret = mode == O_RDONLY && DESC_TYPE_IS_IMPLEMENTATION(desc_type);
			break;

		case SQL_DESC_NAME:
		case SQL_DESC_UNNAMED:
			ret = desc_type == DESC_TYPE_IPD ||
				(desc_type == DESC_TYPE_IRD && mode == O_RDONLY);
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
				default:
					ret = FALSE;
			}
			break;

		default:
			BUGH(desc, "unknown field identifier: %d.", field_id);
			ret = FALSE;
	}
	LOGH(desc, ret ? LOG_LEVEL_DBG : LOG_LEVEL_ERR, /*werr*/0,
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

	/*INDENT-OFF*/
	switch (buff_len) {
		case SQL_IS_INTEGER: sz = sizeof(SQLINTEGER); break;
		case SQL_IS_UINTEGER: sz = sizeof(SQLULEN); break;
		case SQL_IS_SMALLINT: sz = sizeof(SQLSMALLINT); break;
		default: sz = 0;
	}
	/*INDENT-ON*/

	if (sz) {
		memset(buff, 0, sz);
	}
}

static inline void init_rec(esodbc_rec_st *rec, esodbc_desc_st *desc)
{
	memset(rec, 0, sizeof(esodbc_rec_st));
	rec->desc = desc;
	if (rec->desc->type == DESC_TYPE_IPD) {
		/* "the field is set to SQL_PARAM_INPUT by default if the IPD is not
		 * automatically populated by the driver" */
		rec->parameter_type = SQL_PARAM_INPUT;
	}
	/* further init to defaults is done once the data type is set */
}

SQLRETURN update_rec_count(esodbc_desc_st *desc, SQLSMALLINT new_count)
{
	esodbc_rec_st *recs;
	int i;

	if (new_count < 0) {
		ERRH(desc, "new record count is negative (%d).", new_count);
		RET_HDIAGS(desc, SQL_STATE_07009);
	}

	if (desc->count == new_count) {
		LOGH(desc, new_count ? LOG_LEVEL_INFO : LOG_LEVEL_DBG, 0,
			"new descriptor count equals old one, %d.", new_count);
		return SQL_SUCCESS;
	}

	/* the count type in the API (SQLBindCol/Param()) is given as unsigned */
	assert(ESODBC_MAX_DESC_COUNT <= SHRT_MAX);
	if (ESODBC_MAX_DESC_COUNT < new_count) {
		ERRH(desc, "count value (%d) higher than allowed max (%d).",
			new_count, ESODBC_MAX_DESC_COUNT);
		RET_HDIAGS(desc, SQL_STATE_07009);
	}

	if (new_count == 0) {
		DBGH(desc, "freeing the array of %d elems.", desc->count);
		free_desc_recs(desc);
		recs = NULL;
	} else {
		recs = (esodbc_rec_st *)realloc(desc->recs,
				sizeof(esodbc_rec_st) * new_count);
		if (! recs) {
			ERRN("can't (re)alloc %d records.", new_count);
			RET_HDIAGS(desc, SQL_STATE_HY001);
		}
		if (new_count < desc->count) { /* shrinking array */
			DBGH(desc, "recs array is shrinking %d -> %d.", desc->count,
				new_count);
			for (i = new_count - 1; i < desc->count; i ++) {
				free_rec_fields(&desc->recs[i]);
			}
		} else { /* growing array */
			DBGH(desc, "recs array is growing %d -> %d.", desc->count,
				new_count);
			/* init all new records */
			for (i = desc->count; i < new_count; i ++) {
				init_rec(&recs[i], desc);
			}
		}
	}

	desc->count = new_count;
	desc->recs = recs;
	return SQL_SUCCESS;
}

/*
 * Returns the record with desired 1-based index.
 * Grow the array if needed (desired index higher than current count).
 */
esodbc_rec_st *get_record(esodbc_desc_st *desc, SQLSMALLINT rec_no, BOOL grow)
{
	assert(0 < rec_no);

	if (desc->count < rec_no) {
		if (! grow) {
			return NULL;
		} else if (! SQL_SUCCEEDED(update_rec_count(desc, rec_no))) {
			return NULL;
		}
	}
	return &desc->recs[rec_no - 1];
}

SQLSMALLINT count_bound(esodbc_desc_st *desc)
{
	SQLSMALLINT i;
	for (i = desc->count; 0 < i && !REC_IS_BOUND(&desc->recs[i - 1]); i --) {
		;
	}
	return i;
}

esodbc_desc_st *getdata_set_ard(esodbc_stmt_st *stmt, esodbc_desc_st *gd_ard,
	SQLUSMALLINT colno, esodbc_rec_st *recs, SQLUSMALLINT count)
{
	SQLRETURN ret;
	esodbc_desc_st *ard = stmt->ard;

	init_desc(gd_ard, stmt, DESC_TYPE_ARD, SQL_DESC_ALLOC_USER);
	ret = EsSQLSetStmtAttrW(stmt, SQL_ATTR_APP_ROW_DESC, gd_ard,
			SQL_IS_POINTER);
	if (! SQL_SUCCEEDED(ret)) {
		stmt->ard = ard;
		return NULL;
	}

	if (colno < count) { /* can the static recs be used? */
		assert(0 < colno);
		init_rec(&recs[colno - 1], gd_ard);

		gd_ard->count = colno;
		gd_ard->recs = recs;
	}
	/* else: recs will be alloc'd later when binding the column */

	DBGH(stmt, "GD ARD @0x%p, records allocated %s.", gd_ard,
		colno < count ? "statically" : "dynamically");
	return ard;
}

void getdata_reset_ard(esodbc_stmt_st *stmt, esodbc_desc_st *ard,
	SQLUSMALLINT colno, esodbc_rec_st *recs, SQLUSMALLINT count)
{
	SQLRETURN ret;

	if (stmt->ard->recs != recs) { /* the recs are allocated */
		ret = update_rec_count(stmt->ard, 0); /* free all */
		assert(SQL_SUCCEEDED(ret));
	} else { /* recs are on stack */
		/* only the fields of the used rec */
		free_rec_fields(&recs[colno - 1]);
	}

	/* re-instate old ARD value */
	stmt->ard = ard;
	DBGH(stmt, "ARD reset @0x%p.", stmt->ard);
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
	wstr_st wstr;
	SQLSMALLINT word;
	SQLINTEGER intgr;
	esodbc_rec_st *rec;

	if (! check_access(desc, FieldIdentifier, O_RDONLY)) {
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
		LOGH(desc, llev, 0, "field (%d) access check failed: not defined for "
			"desciptor (type: %d).", FieldIdentifier, desc->type);
		RET_HDIAG(desc, state,
			"field type not defined for descriptor", 0);
	}

	state = check_buff(FieldIdentifier, ValuePtr, BufferLength, TRUE);
	if (state != SQL_STATE_00000) {
		ERRH(desc, "buffer/~ length check failed (%d).", state);
		RET_HDIAGS(desc, state);
	}

	/* header fields */
	switch (FieldIdentifier) {
		case SQL_DESC_ALLOC_TYPE:
			*(SQLSMALLINT *)ValuePtr = desc->alloc_type;
			DBGH(desc, "returning: desc alloc type: %u.",
				*(SQLSMALLINT *)ValuePtr);
			return SQL_SUCCESS;

		case SQL_DESC_ARRAY_SIZE:
			*(SQLULEN *)ValuePtr = desc->array_size;
			DBGH(desc, "returning: desc array size: %u.",
				*(SQLULEN *)ValuePtr);
			return SQL_SUCCESS;

		case SQL_DESC_ARRAY_STATUS_PTR:
			*(SQLUSMALLINT **)ValuePtr =
				desc->array_status_ptr;
			DBGH(desc, "returning: status array ptr: 0x%p.",
				(SQLUSMALLINT *)ValuePtr);
			return SQL_SUCCESS;

		case SQL_DESC_BIND_OFFSET_PTR:
			*(SQLLEN **)ValuePtr = desc->bind_offset_ptr;
			DBGH(desc, "returning binding offset ptr: 0x%p.",
				(SQLLEN *)ValuePtr);
			return SQL_SUCCESS;

		case SQL_DESC_BIND_TYPE:
			*(SQLUINTEGER *)ValuePtr = desc->bind_type;
			DBGH(desc, "returning bind type: %u.", *(SQLUINTEGER *)ValuePtr);
			return SQL_SUCCESS;

		case SQL_DESC_COUNT:
			*(SQLSMALLINT *)ValuePtr = desc->count;
			DBGH(desc, "returning count: %d.", *(SQLSMALLINT *)ValuePtr);
			return SQL_SUCCESS;

		case SQL_DESC_ROWS_PROCESSED_PTR:
			*(SQLULEN **)ValuePtr = desc->rows_processed_ptr;
			DBGH(desc, "returning desc rows processed ptr: 0x%p.", ValuePtr);
			return SQL_SUCCESS;
	}

	/*
	 * The field is a record field -> get the record to apply the field to.
	 */
	if (RecNumber < 0) { /* TODO: need to check also if AxD, as per spec?? */
		ERRH(desc, "negative record number provided (%d) with record field "
			"(%d).", RecNumber, FieldIdentifier);
		RET_HDIAG(desc, SQL_STATE_07009,
			"Negative record number provided with record field", 0);
	} else if (RecNumber == 0) {
		ERRH(desc, "unsupported record number 0."); /* TODO: bookmarks? */
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
			WARNH(desc, "record #%d not yet set; returning defaults.",
				RecNumber);
			get_rec_default(FieldIdentifier, BufferLength, ValuePtr);
			return SQL_SUCCESS;
		}
		DBGH(desc, "getting field %d of record #%d @ 0x%p.", FieldIdentifier,
			RecNumber, rec);
	}

	ASSERT_IXD_HAS_ES_TYPE(rec);

	/*INDENT-OFF*/
	/* record fields */
	switch (FieldIdentifier) {
		/* <SQLPOINTER> */
		case SQL_DESC_DATA_PTR:
			*(SQLPOINTER *)ValuePtr = rec->data_ptr;
			DBGH(desc, "returning data pointer 0x%p.", rec->data_ptr);
			break;

		/* <SQLWCHAR *> */
		do {
		case SQL_DESC_BASE_COLUMN_NAME: wstr = rec->base_column_name; break;
		case SQL_DESC_BASE_TABLE_NAME: wstr = rec->base_table_name; break;
		case SQL_DESC_CATALOG_NAME: wstr = rec->catalog_name; break;
		case SQL_DESC_LABEL: wstr = rec->label; break;
		case SQL_DESC_NAME: wstr = rec->name; break;
		case SQL_DESC_SCHEMA_NAME: wstr = rec->schema_name; break;
		case SQL_DESC_TABLE_NAME: wstr = rec->table_name; break;
		case SQL_DESC_LITERAL_PREFIX:
			wstr = rec->es_type->literal_prefix;
			break;
		case SQL_DESC_LITERAL_SUFFIX:
			wstr = rec->es_type->literal_suffix;
			break;
		case SQL_DESC_LOCAL_TYPE_NAME:
			wstr = rec->es_type->local_type_name;
			break;
		case SQL_DESC_TYPE_NAME:
			wstr = rec->es_type->type_name;
			break;
		} while (0);
			if (! StringLengthPtr) {
				RET_HDIAGS(desc, SQL_STATE_HY009);
			} else {
				*StringLengthPtr = (SQLINTEGER)wstr.cnt;
			}
			if (ValuePtr) {
				memcpy(ValuePtr, wstr.str, *StringLengthPtr);
			}
			DBGH(desc, "returning SQLWCHAR record field %d: `" LWPDL "`.",
					FieldIdentifier, LWSTR(&wstr));
			break;

		/* <SQLLEN *> */
		case SQL_DESC_INDICATOR_PTR:
			*(SQLLEN **)ValuePtr = rec->indicator_ptr;
			DBGH(desc, "returning indicator pointer: 0x%p.",
					rec->indicator_ptr);
			break;
		case SQL_DESC_OCTET_LENGTH_PTR:
			*(SQLLEN **)ValuePtr = rec->octet_length_ptr;
			DBGH(desc, "returning octet length pointer 0x%p.",
					rec->octet_length_ptr);
			break;

		/* <SQLLEN> */
		case SQL_DESC_DISPLAY_SIZE:
			*(SQLLEN *)ValuePtr = rec->es_type->display_size;
			DBGH(desc, "returning display size: %d.",
					rec->es_type->display_size);
			break;
		case SQL_DESC_OCTET_LENGTH:
			*(SQLLEN *)ValuePtr = rec->octet_length;
			DBGH(desc, "returning octet length: %d.", rec->octet_length);
			break;

		/* <SQLULEN> */
		case SQL_DESC_LENGTH:
			*(SQLULEN *)ValuePtr = rec->length;
			DBGH(desc, "returning length: %u.", rec->length);
			break;

		/* <SQLSMALLINT> */
		do {
		case SQL_DESC_CONCISE_TYPE: word = rec->concise_type; break;
		case SQL_DESC_TYPE: word = rec->type; break;
		case SQL_DESC_DATETIME_INTERVAL_CODE:
			word = rec->datetime_interval_code; break;

		case SQL_DESC_PARAMETER_TYPE: word = rec->parameter_type; break;
		case SQL_DESC_ROWVER: word = rec->rowver; break;
		case SQL_DESC_UNNAMED: word = rec->unnamed; break;
		case SQL_DESC_PRECISION:
			if (rec->desc->type == DESC_TYPE_IRD) {
				word = (SQLSMALLINT)rec->es_type->column_size;
			} else {
				word = rec->precision;
			}
			break;
		case SQL_DESC_SCALE:
			if (rec->desc->type == DESC_TYPE_IRD) {
				word = rec->es_type->maximum_scale;
			} else {
				word = rec->scale;
			}
			break;
		case SQL_DESC_FIXED_PREC_SCALE:
			word = rec->es_type->fixed_prec_scale;
			break;
		case SQL_DESC_NULLABLE: word = rec->es_type->nullable; break;
		case SQL_DESC_SEARCHABLE: word = rec->es_type->searchable; break;
		case SQL_DESC_UNSIGNED: word = rec->es_type->unsigned_attribute; break;
		case SQL_DESC_UPDATABLE: word = rec->updatable; break;
		} while (0);
			*(SQLSMALLINT *)ValuePtr = word;
			DBGH(desc, "returning record field %d as %d.", FieldIdentifier,
					word);
			break;

		/* <SQLINTEGER> */
		do {
		case SQL_DESC_DATETIME_INTERVAL_PRECISION:
			if (DESC_TYPE_IS_IMPLEMENTATION(rec->desc->type)) {
				/* not used with ES (so far), as no interval types are sup. */
				intgr = rec->es_type->interval_precision;
			} else {
				intgr = rec->datetime_interval_precision;
			}
			break;
		case SQL_DESC_NUM_PREC_RADIX:
			if DESC_TYPE_IS_IMPLEMENTATION(rec->desc->type) {
				intgr = rec->es_type->num_prec_radix;
			} else {
				intgr = rec->num_prec_radix;
			}
			break;
		case SQL_DESC_AUTO_UNIQUE_VALUE:
			intgr = rec->es_type->auto_unique_value;
			break;
		case SQL_DESC_CASE_SENSITIVE:
			intgr = rec->es_type->case_sensitive;
			break;
		} while (0);
			*(SQLINTEGER *)ValuePtr = intgr;
			DBGH(desc, "returning record field %d as %d.", FieldIdentifier,
					intgr);
			break;

		default:
			ERRH(desc, "unknown FieldIdentifier: %d.", FieldIdentifier);
			RET_HDIAGS(desc, SQL_STATE_HY091);
	}
	/*INDENT-ON*/

	return SQL_SUCCESS;
}

#if 0
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
#endif //0

/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/data-type-identifiers-and-descriptors
 *
 * Note: C and SQL types have the same value for the case values below, so
 * this function will work no matter the concise type (i.e. for both IxD,
 * using SQL_C_<type>, and AxD, using SQL_<type>, descriptors).
 * The case values are for datetime and interval data types (see also the
 * comment at function end), so these values must stay in sync, since there
 * are no C corresponding defines for verbose and sub-code (i.e. nothing like
 * "SQL_C_DATETIME" or "SQL_C_CODE_DATE").
 * The identity does not hold across the board, though (extended values, like
 * BIGINTs, do differ)!
 */
void concise_to_type_code(SQLSMALLINT concise, SQLSMALLINT *type,
	SQLSMALLINT *code)
{
	switch (concise) {
		case SQL_TYPE_DATE:
			*type = SQL_DATETIME;
			*code = SQL_CODE_DATE;
			break;
		case SQL_TYPE_TIME:
			//case SQL_TYPE_TIME_WITH_TIMEZONE: //4.0
			*type = SQL_DATETIME;
			*code = SQL_CODE_TIME;
			break;
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
		default:
			/* "For all data types except datetime and interval data types,
			 * the verbose type identifier is the same as the concise type
			 * identifier and the value in SQL_DESC_DATETIME_INTERVAL_CODE is
			 * equal to 0." */
			*type = concise;
			*code = 0;
	}
}

/*
 * From table in § SQL_DESC_TYPE of:
 * https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetdescfield-function##record-fields
 */
static void set_defaults_from_meta_type(esodbc_rec_st *rec)
{
	DBGH(rec->desc, "(re)setting record@0x%p length/precision/scale to "
		"defaults.", rec);
	switch (rec->meta_type) {
		case METATYPE_STRING:
			rec->length = ESODBC_DEF_STRING_LENGTH;
			rec->precision = ESODBC_DEF_STRING_PRECISION;
			break;
		case METATYPE_DATE_TIME:
			if (rec->datetime_interval_code == SQL_CODE_DATE ||
				rec->datetime_interval_code == SQL_CODE_TIME) {
				rec->precision = ESODBC_DEF_DATETIME_PRECISION;
			} else if (rec->datetime_interval_code == SQL_CODE_TIMESTAMP) {
				rec->precision = ESODBC_DEF_TIMESTAMP_PRECISION;
			}
			break;
		case METATYPE_INTERVAL_WSEC:
			rec->precision = ESODBC_DEF_IVL_WS_PRECISION;
		/* no break */
		case METATYPE_INTERVAL_WOSEC:
			rec->datetime_interval_precision = ESODBC_DEF_IVL_WOS_DT_PREC;
			break;
		case METATYPE_EXACT_NUMERIC:
			if (rec->concise_type == SQL_DECIMAL ||
				rec->concise_type == SQL_NUMERIC) { /* == SQL_C_NUMERIC */
				rec->scale = ESODBC_DEF_DECNUM_SCALE;
				rec->precision = ESODBC_DEF_DECNUM_PRECISION;
			}
			break;
		case METATYPE_FLOAT_NUMERIC:
			if (rec->concise_type == SQL_FLOAT) { /* == SQL_C_FLOAT */
				/* "implementation-defined default precision for SQL_FLOAT" */
				rec->precision = ESODBC_DEF_FLOAT_PRECISION;
			}
			break;

		case METATYPE_MAX: /* SQL_C_DEFAULT, ESODBC_SQL_NULL */
			assert((DESC_TYPE_IS_APPLICATION(rec->desc->type) &&
					rec->concise_type == SQL_C_DEFAULT) ||
				(DESC_TYPE_IS_IMPLEMENTATION(rec->desc->type) &&
					rec->concise_type == ESODBC_SQL_NULL));
			DBGH(rec->desc, "max meta type (C default / SQL NULL): "
				"can't set defaults");
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
		/* SQL_DATE, SQL_TIME, SQL_TIMESTAMP:
		 * "In ODBC 3.x, the identifiers for date, time, and timestamp SQL
		 * data types have changed from SQL_DATE, SQL_TIME, and SQL_TIMESTAMP
		 * to SQL_TYPE_DATE, SQL_TYPE_TIME, and SQL_TYPE_TIMESTAMP".
		 * "Because of how the ODBC 3.x Driver Manager performs mapping of the
		 * date, time, and timestamp data types, ODBC 3.x drivers need only
		 * recognize" the new defines.
		 * */
		case SQL_TYPE_DATE:
		case SQL_TYPE_TIME:
		case SQL_TYPE_TIMESTAMP:
			// case SQL_TYPE_UTCDATETIME:
			// case SQL_TYPE_UTCTIME:
			return METATYPE_DATE_TIME;

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

		case SQL_GUID:
			return METATYPE_UID;

		/* bool */
		case SQL_BIT:
		/* ES/SQL types */
		case ESODBC_SQL_BOOLEAN:
			return METATYPE_BIT;

		case ESODBC_SQL_NULL:
			return METATYPE_MAX;

		/* TODO: how to handle these?? */
		case ESODBC_SQL_UNSUPPORTED:
		case ESODBC_SQL_OBJECT: /* == ESODBC_SQL_NESTED */
			ERR("ES/SQL types 'UNSUPPORTED'/'OBJECT'/'NESTED' "
				"not allowed in DML (used: %hd).", concise);
	}

	ERR("unknown meta type for concise SQL type %d.", concise);
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
#if (ODBCVER < 0x0300)
		case SQL_C_BOOKMARK:
#endif /* ODBCVER [12].x */
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
		case SQL_C_UBIGINT:
		case SQL_C_NUMERIC:
			return METATYPE_EXACT_NUMERIC;

		/* numeric floating */
		case SQL_C_FLOAT:
		case SQL_C_DOUBLE:
			return METATYPE_FLOAT_NUMERIC;

		/* datetime */
		/* SQL_C_DATE, SQL_C_TIME, SQL_C_TIMESTAMP: see sqltype_to_meta() */
		case SQL_C_TYPE_DATE:
		case SQL_C_TYPE_TIME:
		case SQL_C_TYPE_TIMESTAMP:
			// case SQL_C_TYPE_TIME_WITH_TIMEZONE:
			// case SQL_C_TYPE_TIMESTAMP_WITH_TIMEZONE:
			return METATYPE_DATE_TIME;

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

		/* boolean */
		case SQL_C_BIT:
			return METATYPE_BIT;

		case SQL_C_GUID:
			return METATYPE_UID;

		case SQL_C_DEFAULT:
			return METATYPE_MAX;
	}

	ERR("unknown meta type for concise C SQL type %d.", concise);
	return METATYPE_UNKNOWN;
}

static SQLSMALLINT sqlctype_to_es(SQLSMALLINT c_concise)
{
	switch (c_concise) {
		case SQL_C_CHAR:
		case SQL_C_WCHAR:
			return ES_TEXT_TO_SQL;
		case SQL_C_SHORT:
		case SQL_C_SSHORT:
		case SQL_C_USHORT:
			return ES_SHORT_TO_SQL;
		case SQL_C_LONG:
		case SQL_C_SLONG:
		case SQL_C_ULONG:
			return ES_INTEGER_TO_SQL;
		case SQL_C_FLOAT:
			return ES_FLOAT_TO_SQL;
		case SQL_C_DOUBLE:
			return ES_DOUBLE_TO_SQL;
		case SQL_C_BIT:
		case SQL_C_TINYINT:
		case SQL_C_STINYINT:
		case SQL_C_UTINYINT:
			return ES_BYTE_TO_SQL;
		case SQL_C_SBIGINT:
		case SQL_C_UBIGINT:
			return ES_LONG_TO_SQL;
		case SQL_C_BINARY:
			return ES_BINARY_TO_SQL;

		case SQL_C_TYPE_DATE:
		case SQL_C_TYPE_TIME:
		case SQL_C_TYPE_TIMESTAMP:
		case SQL_C_NUMERIC:
		case SQL_C_GUID:
		case SQL_C_INTERVAL_DAY:
		case SQL_C_INTERVAL_HOUR:
		case SQL_C_INTERVAL_MINUTE:
		case SQL_C_INTERVAL_SECOND:
		case SQL_C_INTERVAL_DAY_TO_HOUR:
		case SQL_C_INTERVAL_DAY_TO_MINUTE:
		case SQL_C_INTERVAL_DAY_TO_SECOND:
		case SQL_C_INTERVAL_HOUR_TO_MINUTE:
		case SQL_C_INTERVAL_HOUR_TO_SECOND:
		case SQL_C_INTERVAL_MINUTE_TO_SECOND:
		case SQL_C_INTERVAL_MONTH:
		case SQL_C_INTERVAL_YEAR:
		case SQL_C_INTERVAL_YEAR_TO_MONTH:
			/* C and SQL have the same ID mapping */
			return c_concise;

		default:
		case SQL_C_DEFAULT:
		/* case SQL_C_BOOKMARK: = SQL_C_UBIGINT, SQL_C_ULONG */
		/* case SQL_C_VARBOOKMARK: = SQL_C_BINARY */
		case SQL_C_DATE:
		case SQL_C_TIME:
		case SQL_C_TIMESTAMP:
			/* case SQL_C_TYPE_TIME_WITH_TIMEZONE:
			case SQL_C_TYPE_TIMESTAMP_WITH_TIMEZONE: */
			break;
	}
	WARN("no C to ES SQL mapping exists for C type %hd.", c_concise);
	return 0;
}

/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetdescrec-function#consistency-checks
 */
static BOOL consistency_check(esodbc_rec_st *rec)
{
	SQLSMALLINT type, code, concise_type;
	esodbc_desc_st *desc = rec->desc;
	SQLINTEGER max_prec, column_size;
	esodbc_estype_st *es_type;
	esodbc_dbc_st *dbc;

	/* validity of C / SQL datatypes is checked when setting the meta_type */
	assert(METATYPE_UNKNOWN <= rec->meta_type &&
		rec->meta_type <= METATYPE_MAX);
	if (! rec->meta_type) {
		ERRH(desc, "no meta type set.", rec);
		return FALSE;
	} /* else: concise_type had been set */

	concise_to_type_code(rec->concise_type, &type, &code);
	if (rec->type != type || rec->datetime_interval_code != code) {
		ERRH(desc, "inconsistent types found: concise: %d, verbose: %d, "
			"code: %d.", rec->concise_type, rec->type,
			rec->datetime_interval_code);
		return FALSE;
	}

	switch (rec->meta_type) {
		case METATYPE_EXACT_NUMERIC:
			if (rec->concise_type != SQL_NUMERIC && /* == _C_ type */
				rec->concise_type != SQL_DECIMAL) { /* == _C_ type */
				if (rec->scale) {
					ERRH(desc, "fixed numeric type %hd having non-zero scale.",
						rec->concise_type, rec->scale);
					return FALSE;
				}
			} else {
				break; /* TODO: check NUMERIC,DECIMAL bounds */
			}
		/* no break */
		case METATYPE_FLOAT_NUMERIC:
			if (rec->es_type) {
				column_size = rec->es_type->column_size;
			} else {
				dbc = HDRH(HDRH(desc)->stmt)->dbc;
				if (rec->concise_type == SQL_FLOAT) {
					assert(desc->type == DESC_TYPE_IPD);
					column_size = dbc->max_float_type->column_size;
				} else {
					if (DESC_TYPE_IS_APPLICATION(desc->type)) {
						concise_type = sqlctype_to_es(rec->concise_type);
						assert(concise_type); /* all numerics must be mapped */
					} else {
						concise_type = rec->concise_type;
					}
					if (! dbc->no_types) {
						/* bootstraping mode still, loading types */
						break;
					}
					es_type = lookup_es_type(dbc, concise_type, 0);
					if (! es_type) {
						BUGH(desc, "type lookup failed for concise type: %hd,"
							" desc type: %d", concise_type, desc->type);
						return FALSE;
					}
					column_size = es_type->column_size;
				}
			}
			if (rec->precision < 0 || column_size < rec->precision) {
				ERRH(desc, "precision (%hd) out of bounds [0, %ld].",
					rec->precision, column_size);
				return FALSE;
			}
			break;

		/* check SQL_DESC_PRECISION field */
		/* "a time or timestamp data type" */
		case METATYPE_DATE_TIME:
			if (rec->concise_type == SQL_TYPE_DATE) {
				break;
			}
		/* "an interval type with a seconds component" */
		case METATYPE_INTERVAL_WSEC:
		/* "or one of the interval data types with a time component" */
		case METATYPE_INTERVAL_WOSEC:
			if (SQL_INTERVAL_MONTH < rec->concise_type &&
				rec->concise_type != SQL_INTERVAL_YEAR_TO_MONTH) {
				if (rec->precision < 0 ||
					ESODBC_MAX_SEC_PRECISION < rec->precision) {
					ERRH(desc, "precision (%hd) out of bounds [0, %d].",
						rec->precision, ESODBC_MAX_SEC_PRECISION);
					return FALSE;
				}
			}
			if (rec->meta_type == METATYPE_DATE_TIME) {
				break;
			}
			/* check SQL_DESC_DATETIME_INTERVAL_PRECISION */
			switch (rec->concise_type) {
				case SQL_INTERVAL_YEAR:
					max_prec = ESODBC_MAX_IVL_YEAR_LEAD_PREC;
					break;
				case SQL_INTERVAL_MONTH:
					max_prec = ESODBC_MAX_IVL_MONTH_LEAD_PREC;
					break;
				case SQL_INTERVAL_DAY:
					max_prec = ESODBC_MAX_IVL_DAY_LEAD_PREC;
					break;
				case SQL_INTERVAL_HOUR:
					max_prec = ESODBC_MAX_IVL_HOUR_LEAD_PREC;
					break;
				case SQL_INTERVAL_MINUTE:
					max_prec = ESODBC_MAX_IVL_MINUTE_LEAD_PREC;
					break;
				case SQL_INTERVAL_SECOND:
					max_prec = ESODBC_MAX_IVL_SECOND_LEAD_PREC;
					break;
				default:
					max_prec = -1;
			}
			if (0 < max_prec &&
				(rec->datetime_interval_precision < 0 ||
					max_prec < rec->datetime_interval_precision)) {
				ERRH(desc, "datetime_interval_precision (%hd) out of bounds "
					"[0, %d].", rec->datetime_interval_precision, max_prec);
				return FALSE;
			}
			break;
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
 * pointer, unbinding the record." &&
 * "If TargetValuePtr is a null pointer, the driver unbinds the data buffer
 * for the column. [...] An application can unbind the data buffer for a
 * column but still have a length/indicator buffer bound for the column, if
 * the TargetValuePtr argument in the call to SQLBindCol is a null pointer but
 * the StrLen_or_IndPtr argument is a valid value."
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
	esodbc_rec_st *rec;
	wstr_st *wstrp;
	SQLSMALLINT *wordp;
	SQLINTEGER *intp;
	SQLSMALLINT count, type, chk_type, chk_code;
	SQLULEN ulen;
	SQLLEN slen;
	size_t wlen;

	if (! check_access(desc, FieldIdentifier, O_RDWR)) {
		/* "The SQL_DESC_DATA_PTR field of an IPD is not normally set;
		 * however, an application can do so to force a consistency check of
		 * IPD fields."
		 * TODO: the above won't work with the generic check implementation:
		 * is it worth hacking an exception here? (since IPD/.data_ptr is
		 * marked RO) */
		ERRH(desc, "field access check failed: not defined or RO for "
			"desciptor.");
		RET_HDIAGS(desc, SQL_STATE_HY091);
	}

	state = check_buff(FieldIdentifier, ValuePtr, BufferLength, FALSE);
	if (state != SQL_STATE_00000) {
		ERRH(desc, "buffer/~ length check failed (%d).", state);
		RET_HDIAGS(desc, state);
	}

	/* header fields */
	switch (FieldIdentifier) {
		case SQL_DESC_ARRAY_SIZE:
			ulen = (SQLULEN)(uintptr_t)ValuePtr;
			DBGH(desc, "setting desc array size to: %llu.", ulen);
			if (DESC_TYPE_IS_RECORD(desc->type)) {
				if (ESODBC_MAX_ROW_ARRAY_SIZE < ulen) {
					WARNH(desc, "provided desc array size (%u) larger than "
						"allowed max (%u) -- set value adjusted to max.", ulen,
						ESODBC_MAX_ROW_ARRAY_SIZE);
					desc->array_size = ESODBC_MAX_ROW_ARRAY_SIZE;
					RET_HDIAGS(desc, SQL_STATE_01S02);
				} else if (ulen < 1) {
					ERRH(desc, "can't set the array size to less than 1.");
					RET_HDIAGS(desc, SQL_STATE_HY092);
				}
			} else { /* IS_PARAMETER */
				/* no support for param arrays (yet) TODO */
				if (1 < ulen) {
					ERRH(desc, "no support for arrays of parameters.");
					RET_HDIAG(desc, SQL_STATE_HYC00,
						"Parameter arrays not implemented", 0);
				}
			}
			desc->array_size = ulen;
			return SQL_SUCCESS;

		case SQL_DESC_ARRAY_STATUS_PTR:
			DBGH(desc, "setting desc array status ptr to: 0x%p.", ValuePtr);
			/* deferred */
			desc->array_status_ptr = (SQLUSMALLINT *)ValuePtr;
			return SQL_SUCCESS;

		case SQL_DESC_BIND_OFFSET_PTR:
			DBGH(desc, "setting binding offset ptr to: 0x%p.", ValuePtr);
			/* deferred */
			desc->bind_offset_ptr = (SQLLEN *)ValuePtr;
			return SQL_SUCCESS;

		case SQL_DESC_BIND_TYPE:
			DBGH(desc, "setting bind type to: %u.",
				(SQLUINTEGER)(uintptr_t)ValuePtr);
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
			return update_rec_count(desc, (SQLSMALLINT)(intptr_t)ValuePtr);

		case SQL_DESC_ROWS_PROCESSED_PTR:
			DBGH(desc, "setting desc rows processed ptr to: 0x%p.", ValuePtr);
			desc->rows_processed_ptr = (SQLULEN *)ValuePtr;
			return SQL_SUCCESS;
	}

	/*
	 * The field is a record field -> get the record to apply the field to.
	 */
	if (RecNumber < 0) { /* TODO: need to check also if AxD, as per spec?? */
		ERRH(desc, "negative record number provided (%d) with record field "
			"(%d).", RecNumber, FieldIdentifier);
		RET_HDIAG(desc, SQL_STATE_07009,
			"Negative record number provided with record field", 0);
	} else if (RecNumber == 0) {
		ERRH(desc, "unsupported record number 0."); /* TODO: bookmarks? */
		RET_HDIAG(desc, SQL_STATE_07009,
			"Unsupported record number 0", 0);
	} else { /* apparently one can set a record before the count is set */
		rec = get_record(desc, RecNumber, TRUE);
		if (! rec) {
			ERRH(desc, "can't get record with number %d.", RecNumber);
			RET_STATE(desc->hdr.diag.state);
		}
		DBGH(desc, "setting field %d of record #%d @ 0x%p.", FieldIdentifier,
			RecNumber, rec);
	}

	/*
	 * "If the application changes the data type or attributes after setting
	 * the SQL_DESC_DATA_PTR field, the driver sets SQL_DESC_DATA_PTR to a
	 * null pointer, unbinding the record."
	 *
	 * NOTE: the record can actually still be bound by the length/indicator
	 * buffer(s), so the above "binding" definition is incomplete.
	 */
	if (FieldIdentifier != SQL_DESC_DATA_PTR) {
		DBGH(desc, "attribute to set is different than data ptr (%d) => "
			"unbinding data buffer (was 0x%p).", SQL_DESC_DATA_PTR,
			rec->data_ptr);
		rec->data_ptr = NULL;
	}

	/*INDENT-OFF*/
	/* record fields */
	switch (FieldIdentifier) {
		/* "For datetime and interval data types, however, a verbose type
		 * (SQL_DATETIME or SQL_INTERVAL) is stored in SQL_DESC_TYPE, a
		 * concise type is stored in SQL_DESC_CONCISE_TYPE, and a subcode for
		 * each concise type is stored in SQL_DESC_DATETIME_INTERVAL_CODE." */
		case SQL_DESC_TYPE:
			type = (SQLSMALLINT)(intptr_t)ValuePtr;
			DBGH(desc, "setting type of rec@0x%p to %d.", rec, type);
			/* Note: SQL_[C_]DATE == SQL_DATETIME (== 9) =>
			 * 1. one needs to always use SQL_DESC_CONCISE_TYPE for setting
			 * the types from within the driver (binding cols, params):
			 * "SQL_DESC_CONCISE_TYPE can be set by a call to SQLBindCol or
			 * SQLBindParameter, or SQLSetDescField. SQL_DESC_TYPE can be set
			 * by a call to SQLSetDescField or SQLSetDescRec."
			 * 2. SQL_DESC_TYPE can only be used when setting the type record
			 * fields (.type, .concise_type, datetime_interval_code)
			 * individually. */
			if (type == SQL_DATETIME || type == SQL_INTERVAL) {
				/* "When the application sets the SQL_DESC_TYPE field, the
				 * driver checks that other fields that specify the type are
				 * valid and consistent." */
				/* setting the verbose type only */
				concise_to_type_code(rec->concise_type, &chk_type, &chk_code);
				if (chk_type != type ||
						chk_code != rec->datetime_interval_code ||
						(! rec->datetime_interval_code)) {
					ERRH(desc, "type fields found inconsistent when setting "
						"the type to %hd: concise: %hd, datetime_code: %hd.",
						(SQLSMALLINT)(intptr_t)ValuePtr,
						rec->concise_type, rec->datetime_interval_code);
					RET_HDIAGS(desc, SQL_STATE_HY021);
				} else {
					rec->type = type;
				}
				break;
			}
			/* no break! */
		case SQL_DESC_CONCISE_TYPE:
			DBGH(desc, "setting concise type of rec 0x%p to %d.", rec,
					(SQLSMALLINT)(intptr_t)ValuePtr);
			rec->concise_type = (SQLSMALLINT)(intptr_t)ValuePtr;

			concise_to_type_code(rec->concise_type, &rec->type,
					&rec->datetime_interval_code);
			rec->meta_type = concise_to_meta(rec->concise_type, desc->type);
			if (rec->meta_type == METATYPE_UNKNOWN) {
				ERRH(desc, "REC@0x%p: incorrect concise type %d for rec #%d.",
						rec, rec->concise_type, RecNumber);
				RET_HDIAGS(desc, DESC_TYPE_IS_APPLICATION(desc->type) ?
						SQL_STATE_HY003 : SQL_STATE_HY004);
			}
			/* "When the SQL_DESC_TYPE or SQL_DESC_CONCISE_TYPE field is set
			 * for some data types, the SQL_DESC_DATETIME_INTERVAL_PRECISION,
			 * SQL_DESC_LENGTH, SQL_DESC_PRECISION, and SQL_DESC_SCALE fields
			 * are automatically set to default values". */
			set_defaults_from_meta_type(rec);
			DBGH(desc, "REC@0x%p types: concise: %d, verbose: %d, code: %d.",
					rec, rec->concise_type, rec->type,
					rec->datetime_interval_code);
			break;

		case SQL_DESC_DATA_PTR:
			DBGH(desc, "setting data ptr to 0x%p of type %d.", ValuePtr,
					BufferLength);
			/* deferred */
			rec->data_ptr = ValuePtr;
			if (rec->data_ptr) {
				/* "A consistency check is performed by the driver
				 * automatically whenever an application sets the
				 * SQL_DESC_DATA_PTR field of an APD, ARD, or IPD."
				 * "The SQL_DESC_DATA_PTR field of an IPD is not normally set;
				 * however, an application can do so to force a consistency
				 * check of IPD fields. A consistency check cannot be
				 * performed on an IRD." */
				if ((desc->type != DESC_TYPE_IRD) &&
						(! consistency_check(rec))) {
					ERRH(desc, "consistency check failed on rec@0x%p.", rec);
					RET_HDIAGS(desc, SQL_STATE_HY021);
				} else {
					DBGH(desc, "rec@0x%p: bound data ptr@0x%p.", rec,
							rec->data_ptr);
				}
			} else {
				/* "If the highest-numbered column or parameter is unbound,
				 * then SQL_DESC_COUNT is changed to the number of the next
				 * highest-numbered column or parameter. " */
				if (DESC_TYPE_IS_APPLICATION(desc->type) &&
						/* see function-top comments on when to unbind */
						(! REC_IS_BOUND(rec))) {
					DBGH(desc, "rec 0x%p of desc type %d unbound.", rec,
							desc->type);
					if (RecNumber == desc->count) {
						count = count_bound(desc);
						/* worst case: trying to unbind a not-yet-bound rec */
						if (count != desc->count) {
							DBGH(desc, "adjusting rec count from %hd to %hd.",
									desc->count, count);
							return update_rec_count(desc, count);
						}
					}
				}
			}
			break;

		case SQL_DESC_NAME:
			WARNH(desc, "stored procedure params (to set to `"LWPD"`) not "
					"supported.", ValuePtr ? (SQLWCHAR *)ValuePtr : TWS_NULL);
			RET_HDIAG(desc, SQL_STATE_HYC00,
					"stored procedure params not supported", 0);

		/* <SQLWCHAR *> */
		do {
		case SQL_DESC_BASE_COLUMN_NAME: wstrp = &rec->base_column_name; break;
		case SQL_DESC_BASE_TABLE_NAME: wstrp = &rec->base_table_name; break;
		case SQL_DESC_CATALOG_NAME: wstrp = &rec->catalog_name; break;
		case SQL_DESC_LABEL: wstrp = &rec->label; break;
		/* R/O fields: literal_prefix/_suffix, local_type_name, type_name */
		case SQL_DESC_SCHEMA_NAME: wstrp = &rec->schema_name; break;
		case SQL_DESC_TABLE_NAME: wstrp = &rec->table_name; break;
		} while (0);
			if (BufferLength == SQL_NTS) {
				wlen = ValuePtr ? wcslen((SQLWCHAR *)ValuePtr) : 0;
			} else {
				wlen = BufferLength;
			}
			DBGH(desc, "setting SQLWCHAR field %d to `" LWPDL "`(@0x%p).",
				FieldIdentifier, wlen, ValuePtr, wlen, ValuePtr);
			if (wstrp->str) {
				DBGH(desc, "freeing previously allocated value for field %d "
					"(`" LWPDL "`).", FieldIdentifier, LWSTR(wstrp));
				free(wstrp->str);
				wstrp->str = NULL;
				wstrp->cnt = 0;
			}
			if (! ValuePtr) {
				DBGH(desc, "field %d reset to NULL.", FieldIdentifier);
				break;
			}
			if (! (wstrp->str = (SQLWCHAR *)malloc((wlen + /*0-term*/1)
					* sizeof(SQLWCHAR)))) {
				ERRH(desc, "failed to alloc w-string buffer of len %zd.",
					wlen + 1);
				RET_HDIAGS(desc, SQL_STATE_HY001);
			}
			memcpy(wstrp->str, ValuePtr, wlen * sizeof(SQLWCHAR));
			wstrp->str[wlen] = 0;
			wstrp->cnt = wlen;
			break;

		/* <SQLLEN *>, deferred */
		case SQL_DESC_INDICATOR_PTR:
			DBGH(desc, "setting indicator pointer to 0x%p.", ValuePtr);
			rec->indicator_ptr = (SQLLEN *)ValuePtr;
			break;
		case SQL_DESC_OCTET_LENGTH_PTR:
			DBGH(desc, "setting octet length pointer to 0x%p.", ValuePtr);
			rec->octet_length_ptr = (SQLLEN *)ValuePtr;
			break;

		/* <SQLLEN> */
		/* R/O fields: display_size */
		case SQL_DESC_OCTET_LENGTH:
			slen = (SQLLEN)(intptr_t)ValuePtr;
			DBGH(desc, "setting octet length: %ld.", slen);
			/* rec field's type is signed; a negative can be dangerous */
			if (slen < 0) {
				WARNH(desc, "negative octet length provided (%lld)", slen);
				/* no eror returned: in non-str/binary, it is to be ignorred */
			}
			rec->octet_length = slen;
			break;

		/* <SQLULEN> */
		case SQL_DESC_LENGTH:
			DBGH(desc, "setting length: %u.", (SQLULEN)(uintptr_t)ValuePtr);
			rec->length = (SQLULEN)(uintptr_t)ValuePtr;
			break;

		/* <SQLSMALLINT> */
		do {
		case SQL_DESC_DATETIME_INTERVAL_CODE:
			wordp = &rec->datetime_interval_code; break;
		case SQL_DESC_PARAMETER_TYPE: wordp = &rec->parameter_type; break;
		case SQL_DESC_PRECISION: wordp = &rec->precision; break;
		case SQL_DESC_ROWVER: wordp = &rec->rowver; break;
		case SQL_DESC_SCALE: wordp = &rec->scale; break;
		case SQL_DESC_UNNAMED:
			/* only driver can set this value */
			if ((SQLSMALLINT)(intptr_t)ValuePtr == SQL_NAMED) {
				ERRH(desc, "only the driver can set %d field to 'SQL_NAMED'.",
						FieldIdentifier);
				RET_HDIAGS(desc, SQL_STATE_HY091);
			}
			wordp = &rec->unnamed;
			break;
		/* R/O field: fixed_prec_scale, nullable, searchable, unsigned  */
		case SQL_DESC_UPDATABLE: wordp = &rec->updatable; break;
		} while (0);
			DBGH(desc, "setting record field %d to %d.", FieldIdentifier,
					(SQLSMALLINT)(intptr_t)ValuePtr);
			*wordp = (SQLSMALLINT)(intptr_t)ValuePtr;
			break;

		/* <SQLINTEGER> */
		do {
		/* R/O field: auto_unique_value, case_sensitive  */
		case SQL_DESC_DATETIME_INTERVAL_PRECISION:
			intp = &rec->datetime_interval_precision;
			break;
		case SQL_DESC_NUM_PREC_RADIX:
			intp = &rec->num_prec_radix;
			break;
		} while (0);
			DBGH(desc, "returning record field %d as %d.", FieldIdentifier,
					(SQLINTEGER)(intptr_t)ValuePtr);
			*intp = (SQLINTEGER)(intptr_t)ValuePtr;
			break;

		default:
			ERRH(desc, "unknown FieldIdentifier: %d.", FieldIdentifier);
			RET_HDIAGS(desc, SQL_STATE_HY091);
	}
	/*INDENT-ON*/

	return SQL_SUCCESS;
}


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
