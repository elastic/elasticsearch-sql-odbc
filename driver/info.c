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
#include "queries.h"
#include "connect.h"
#include "info.h"

#define ORIG_DISCRIM	"IM"
#define ORIG_CLASS_ISO	"ISO 9075"
#define ORIG_CLASS_ODBC	"ODBC 3.0"

#if defined(_WIN32) || defined (WIN32)
/* DRV_NAME defined in CMakeLists.txt */
#define DRIVER_NAME	STR(DRV_NAME) ".dll"
#else /* win32 */
#endif /* win32 */



/* List of supported functions in the driver.
 * Advertize them as being all implemented and fail at call time, to prevent
 * an early failure in the client application. */
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
	/* SQL_API_SQLBROWSECONNECT, */
	/* SQL_API_SQLBULKOPERATIONS, */
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

static SQLRETURN getinfo_driver(
	BOOL *handled,
	SQLHDBC ConnectionHandle,
	SQLUSMALLINT InfoType,
	_Out_writes_bytes_opt_(BufferLength) SQLPOINTER InfoValue,
	SQLSMALLINT BufferLength,
	_Out_opt_ SQLSMALLINT *StringLengthPtr)
{
	esodbc_dbc_st *dbc = DBCH(ConnectionHandle);

	*handled = TRUE;
	switch(InfoType) {
		case SQL_ACTIVE_ENVIRONMENTS:
			DBGH(dbc, "requested: max active environments (0).");
			*(SQLUSMALLINT *)InfoValue = 0;
			return SQL_SUCCESS;
		/* "if the driver can execute functions asynchronously on the
		 * connection handle" */
		case SQL_ASYNC_DBC_FUNCTIONS:
			DBGH(dbc, "requested: async DBC functions (no - %lu).",
				SQL_ASYNC_DBC_NOT_CAPABLE);
			*(SQLUINTEGER *)InfoValue = SQL_ASYNC_DBC_NOT_CAPABLE;
			return SQL_SUCCESS;
		case SQL_ASYNC_MODE:
			DBGH(dbc, "requested: async mode (%lu).", SQL_AM_NONE);
			*(SQLUINTEGER *)InfoValue = SQL_AM_NONE;
			return SQL_SUCCESS;
		/* "if the driver supports asynchronous notification" */
		case SQL_ASYNC_NOTIFICATION:
			DBGH(dbc, "requested: async notification (no - %lu).",
				SQL_ASYNC_NOTIFICATION_NOT_CAPABLE);
			*(SQLUINTEGER *)InfoValue = SQL_ASYNC_NOTIFICATION_NOT_CAPABLE;
			return SQL_SUCCESS;
		case SQL_BATCH_ROW_COUNT:
			DBGH(dbc, "requested: batch row count (rolled up).");
			*(SQLUINTEGER *)InfoValue = SQL_BRC_ROLLED_UP;
			return SQL_SUCCESS;
		case SQL_BATCH_SUPPORT:
			DBGH(dbc, "requested: batch support (0).");
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
		case SQL_DATA_SOURCE_NAME:
			DBGH(dbc, "requested: data source name: `" LWPDL "`.",
				LWSTR(&dbc->dsn));
			return write_wstr(dbc, InfoValue,
					/* request may occur before connection */
					dbc->dsn.str ? &dbc->dsn : &MK_WSTR(""),
					BufferLength, StringLengthPtr);
		case SQL_DRIVER_AWARE_POOLING_SUPPORTED:
			DBGH(dbc, "requested: driver aware pooling (not capable).");
			*(SQLUINTEGER *)InfoValue = SQL_DRIVER_AWARE_POOLING_NOT_CAPABLE;
			return SQL_SUCCESS;
		/*
		 * DM-only:
		case SQL_DRIVER_HDBC:
		case SQL_DRIVER_HDESC:
		case SQL_DRIVER_HENV:
		case SQL_DRIVER_HLIB:
		case SQL_DRIVER_HSTMT:
		*/
		case SQL_DRIVER_NAME:
			DBGH(dbc, "requested: driver (file) name: %s.", DRIVER_NAME);
			return write_wstr(dbc, InfoValue, &MK_WSTR(DRIVER_NAME),
					BufferLength, StringLengthPtr);
		/* Driver Information */
		/* "what version of odbc a driver complies with" */
		case SQL_DRIVER_ODBC_VER:
			return write_wstr(dbc, InfoValue, &MK_WSTR(ESODBC_SQL_SPEC_STRING),
					BufferLength, StringLengthPtr);
		case SQL_DRIVER_VER:
			DBGH(dbc, "requested: driver version (`%s`).", ESODBC_DRIVER_VER);
			return write_wstr(dbc, InfoValue, &MK_WSTR(ESODBC_DRIVER_VER),
					BufferLength, StringLengthPtr);
		case SQL_DTC_TRANSITION_COST:
			INFOH(dbc, "no connection pooling / DTC support.");
			DBGH(dbc, "requested: DTC transition cost (0).");
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
		/* what Operations are supported by SQLSetPos  */
		// FIXME: review@alpha
		case SQL_DYNAMIC_CURSOR_ATTRIBUTES1:
		case SQL_DYNAMIC_CURSOR_ATTRIBUTES2:
			DBGH(dbc, "requested cursor attributes %hu (0).", InfoType);
			*(SQLUINTEGER *)InfoValue = SQL_CA1_POS_UPDATE;
			return SQL_SUCCESS;
		case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1:
			DBGH(dbc, "requested: fwd curs attrs1 (%lx).",
				ESODBC_FORWARD_ONLY_CURSOR_ATTRIBUTES1);
			*(SQLUINTEGER *)InfoValue = ESODBC_FORWARD_ONLY_CURSOR_ATTRIBUTES1;
			return SQL_SUCCESS;
		case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2:
			DBGH(dbc, "requested: fwd curs attrs1 (%lx).",
				ESODBC_FORWARD_ONLY_CURSOR_ATTRIBUTES2);
			*(SQLUINTEGER *)InfoValue = ESODBC_FORWARD_ONLY_CURSOR_ATTRIBUTES2;
			return SQL_SUCCESS;
		case SQL_FILE_USAGE:
			/* JDBC[0]: usesLocalFilePerTable() */
			DBGH(dbc, "requested: file usage: table.");
			*(SQLUSMALLINT *)InfoValue = SQL_FILE_TABLE;
			return SQL_SUCCESS;
		case SQL_GETDATA_EXTENSIONS:
			DBGH(dbc, "requested: GetData extentions.");
			*(SQLUINTEGER *)InfoValue = ESODBC_GETDATA_EXTENSIONS;
			return SQL_SUCCESS;
		case SQL_INFO_SCHEMA_VIEWS:
			DBGH(dbc, "requested: schema views (0).");
			WARNH(dbc, "schema not yet supported by data source.");
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
		case SQL_KEYSET_CURSOR_ATTRIBUTES1:
		case SQL_KEYSET_CURSOR_ATTRIBUTES2:
			DBGH(dbc, "requested: keyset cursor attributes (0).");
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
		case SQL_MAX_ASYNC_CONCURRENT_STATEMENTS:
			DBGH(dbc, "requested: async concurrent statements (0).");
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
		/* "the maximum number of active statements that the driver can
		 * support for a connection" */
		//case SQL_ACTIVE_STATEMENTS:
		case SQL_MAX_CONCURRENT_ACTIVITIES:
			*(SQLUSMALLINT *)InfoValue = 0;
			DBGH(dbc, "requested: max concurrent activities (0).");
			return SQL_SUCCESS;
		case SQL_MAX_DRIVER_CONNECTIONS:
			DBGH(dbc, "requested: max driver connections (0).");
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
		case SQL_ODBC_INTERFACE_CONFORMANCE:
			DBGH(dbc, "requested: ODBC interface conformance (%lu).",
				ESODBC_ODBC_INTERFACE_CONFORMANCE);
			*(SQLUINTEGER *)InfoValue = ESODBC_ODBC_INTERFACE_CONFORMANCE;
			return SQL_SUCCESS;
		// case SQL_ODBC_STANDARD_CLI_CONFORMANCE // undef'd:
		// case SQL_ODBC_VER: DM-only
		case SQL_PARAM_ARRAY_ROW_COUNTS:
			DBGH(dbc, "requested: param array row counts (no batch).");
			*(SQLUINTEGER *)InfoValue = SQL_PARC_NO_BATCH ;
			return SQL_SUCCESS;
		case SQL_PARAM_ARRAY_SELECTS:
			DBGH(dbc, "requested: reselt set availability with parameterized "
				"execution (%lu).", SQL_PAS_NO_SELECT);
			*(SQLUINTEGER *)InfoValue = SQL_PAS_NO_SELECT;
			return SQL_SUCCESS;
#if (ODBCVER < 0x0400)
		/* this is an ODBC 4.0, but Excel seems to asks for it anyways in
		 * certain cases with a 3.80 driver */
		case 180: /* = SQL_RETURN_ESCAPE_CLAUSE */
#else
		case SQL_RETURN_ESCAPE_CLAUSE:
#endif /* SQL_RETURN_ESCAPE_CLAUSE */
			/* actually an error, but po */
			INFOH(dbc, "SQL_RETURN_ESCAPE_CLAUSE not supported.");
			*(SQLUINTEGER *)InfoValue = 0; /* = SQL_RC_NONE */
			return SQL_SUCCESS;
		case SQL_ROW_UPDATES:
			DBGH(dbc, "requested: row updates detection (N).");
			WARNH(dbc, "no keyset-driven or mixed cursor support.");
			return write_wstr(dbc, InfoValue, &MK_WSTR("N"),
					BufferLength, StringLengthPtr);
		case SQL_SEARCH_PATTERN_ESCAPE:
			DBGH(dbc, "requested: escape character (`%s`).",
				ESODBC_PATTERN_ESCAPE);
			return write_wstr(dbc, InfoValue, &MK_WSTR(ESODBC_PATTERN_ESCAPE),
					BufferLength, StringLengthPtr);
		case SQL_SERVER_NAME:
			DBGH(dbc, "requested: server name: `" LWPDL "`.",
				LWSTR(&dbc->server));
			return write_wstr(dbc, InfoValue,
					/* request may occur before connection */
					dbc->server.str ? &dbc->server : &MK_WSTR(""),
					BufferLength, StringLengthPtr);
		case SQL_STATIC_CURSOR_ATTRIBUTES1:
		case SQL_STATIC_CURSOR_ATTRIBUTES2:
			DBGH(dbc, "requested: static cursor attributes %hu (0).",
				InfoType);
			WARNH(dbc, "no static cursor support.");
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
	}
	*handled = FALSE;
	return SQL_ERROR;
}

static SQLRETURN getinfo_dbms_product(
	BOOL *handled,
	SQLHDBC ConnectionHandle,
	SQLUSMALLINT InfoType,
	_Out_writes_bytes_opt_(BufferLength) SQLPOINTER InfoValue,
	SQLSMALLINT BufferLength,
	_Out_opt_ SQLSMALLINT *StringLengthPtr)
{
	esodbc_dbc_st *dbc = DBCH(ConnectionHandle);
	SQLINTEGER string_len;
	SQLRETURN ret;

	*handled = TRUE;
	switch(InfoType) {
		case SQL_DATABASE_NAME:
			DBGH(dbc, "requested database name.");
			ret = EsSQLGetConnectAttrW(ConnectionHandle,
					SQL_ATTR_CURRENT_CATALOG, InfoValue,
					(SQLINTEGER)BufferLength, &string_len);
			if (StringLengthPtr) {
				*StringLengthPtr = (SQLSMALLINT)string_len;
			}
			return ret;
		case SQL_DBMS_NAME:
			DBGH(dbc, "requested: DBMS name (`%s`).",
				ESODBC_ELASTICSEARCH_NAME);
			return write_wstr(dbc, InfoValue,
					&MK_WSTR(ESODBC_ELASTICSEARCH_NAME), BufferLength,
					StringLengthPtr);
		// FIXME: get version from server
		case SQL_DBMS_VER:
			DBGH(dbc, "requested: DBMS version (`%s`).",
				ESODBC_ELASTICSEARCH_VER);
			return write_wstr(dbc, InfoValue,
					&MK_WSTR(ESODBC_ELASTICSEARCH_VER), BufferLength,
					StringLengthPtr);
	}
	*handled = FALSE;
	return SQL_ERROR;
}

static SQLRETURN getinfo_data_source(
	BOOL *handled,
	SQLHDBC ConnectionHandle,
	SQLUSMALLINT InfoType,
	_Out_writes_bytes_opt_(BufferLength) SQLPOINTER InfoValue,
	SQLSMALLINT BufferLength,
	_Out_opt_ SQLSMALLINT *StringLengthPtr)
{
	esodbc_dbc_st *dbc = DBCH(ConnectionHandle);

	*handled = TRUE;
	switch(InfoType) {
		case SQL_ACCESSIBLE_PROCEDURES:
			DBGH(dbc, "requested: accessible procedures (N).");
			return write_wstr(dbc, InfoValue, &MK_WSTR("N"),
					BufferLength, StringLengthPtr);
		case SQL_ACCESSIBLE_TABLES:
			DBGH(dbc, "requested: accessible tables (`Y`).");
			return write_wstr(dbc, InfoValue, &MK_WSTR("Y"),
					BufferLength, StringLengthPtr);
		case SQL_BOOKMARK_PERSISTENCE:
			DBGH(dbc, "requested bookmark persistence (none).");
			*(SQLUINTEGER *)InfoValue = 0; /* no support */
			return SQL_SUCCESS;
		case SQL_CATALOG_TERM: /* SQL_QUALIFIER_TERM */
			/* JDBC[0]: getCatalogSeparator() */
			DBGH(dbc, "requested: catalog term (`%s`).", ESODBC_CATALOG_TERM);
			return write_wstr(dbc, InfoValue, &MK_WSTR(ESODBC_CATALOG_TERM),
					BufferLength, StringLengthPtr);
		case SQL_COLLATION_SEQ:
			DBGH(dbc, "requested: collation seq (`UTF8`).");
			return write_wstr(dbc, InfoValue, &MK_WSTR("UTF8"),
					BufferLength, StringLengthPtr);
		case SQL_CONCAT_NULL_BEHAVIOR:
			DBGH(dbc, "requested: concat NULL behavior (%u).", SQL_CB_NULL);
			*(SQLUSMALLINT *)InfoValue = SQL_CB_NULL;
			return SQL_SUCCESS;
		case SQL_CURSOR_COMMIT_BEHAVIOR:
		case SQL_CURSOR_ROLLBACK_BEHAVIOR:
			DBGH(dbc, "requested: cursor %s behavior.",
				InfoType == SQL_CURSOR_COMMIT_BEHAVIOR ?
				"commit" : "rollback");
			/* assume this is the  of equivalent of
			 * JDBC's HOLD_CURSORS_OVER_COMMIT */
			*(SQLUSMALLINT *)InfoValue = SQL_CB_PRESERVE;
			return SQL_SUCCESS;
		case SQL_CURSOR_SENSITIVITY:
			DBGH(dbc, "requested: cursor sensitivity (%u).", SQL_INSENSITIVE);
			*(SQLUINTEGER *)InfoValue = SQL_INSENSITIVE;
			return SQL_SUCCESS;
		case SQL_DATA_SOURCE_READ_ONLY:
			DBGH(dbc, "requested: if data source is read only (Y).");
			return write_wstr(dbc, InfoValue, &MK_WSTR("Y"), BufferLength,
					StringLengthPtr);
		case SQL_DEFAULT_TXN_ISOLATION:
			DBGH(dbc, "requested: def txn isolation (0).");
			WARNH(dbc, "no support for transactions available.");
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
		case SQL_DESCRIBE_PARAMETER:
			DBGH(dbc, "requested: describe param (`N`).");
			return write_wstr(dbc, InfoValue, &MK_WSTR("N"), BufferLength,
					StringLengthPtr);
		case SQL_MULT_RESULT_SETS:
			DBGH(dbc, "requested: multiple result set support (N).");
			return write_wstr(dbc, InfoValue, &MK_WSTR("N"), BufferLength,
					StringLengthPtr);
		case SQL_MULTIPLE_ACTIVE_TXN:
			INFOH(dbc, "no transactions support.");
			DBGH(dbc, "requested: multiple active transactions (`Y`).");
			/* returning Y in the spirit of concurrency */
			return write_wstr(dbc, InfoValue, &MK_WSTR("Y"), BufferLength,
					StringLengthPtr);
		case SQL_NEED_LONG_DATA_LEN:
			DBGH(dbc, "requested: long data len (`N`).");
			return write_wstr(dbc, InfoValue, &MK_WSTR("N"), BufferLength,
					StringLengthPtr);
		case SQL_NULL_COLLATION:
			DBGH(dbc, "requested: null collation (%d).", SQL_NC_END);
			*(SQLUSMALLINT *)InfoValue = SQL_NC_END;
			return SQL_SUCCESS;
		case SQL_PROCEDURE_TERM:
			DBGH(dbc, "requested: procedure term (``).");
			return write_wstr(dbc, InfoValue, &MK_WSTR(""), BufferLength,
					StringLengthPtr);
		case SQL_SCHEMA_TERM:
			DBGH(dbc, "requested schema term (`%s`).", ESODBC_SCHEMA_TERM);
			return write_wstr(dbc, InfoValue, &MK_WSTR(ESODBC_SCHEMA_TERM),
					BufferLength, StringLengthPtr);
		case SQL_SCROLL_OPTIONS:
			DBGH(dbc, "requested: scroll options (%lu).", SQL_SO_FORWARD_ONLY);
			*(SQLUINTEGER *)InfoValue = SQL_SO_FORWARD_ONLY;
			return SQL_SUCCESS;
		case SQL_TABLE_TERM:
			DBGH(dbc, "requested table term (`%s`).", ESODBC_TABLE_TERM);
			return write_wstr(dbc, InfoValue, &MK_WSTR(ESODBC_TABLE_TERM),
					BufferLength, StringLengthPtr);
		case SQL_TXN_CAPABLE: /* SQL_TRANSACTION_CAPABLE */
			DBGH(dbc, "requested: transaction capable (%u).", SQL_TC_NONE);
			*(SQLUSMALLINT *)InfoValue = SQL_TC_NONE;
			return SQL_SUCCESS;
		case SQL_TXN_ISOLATION_OPTION:
			WARNH(dbc, "transactions not supported.");
			DBGH(dbc, "requested: transaction isolation options (0).");
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
		case SQL_USER_NAME:
			break; // TODO
	}
	*handled = FALSE;
	return SQL_ERROR;
}

static SQLRETURN getinfo_sql(
	BOOL *handled,
	SQLHDBC ConnectionHandle,
	SQLUSMALLINT InfoType,
	_Out_writes_bytes_opt_(BufferLength) SQLPOINTER InfoValue,
	SQLSMALLINT BufferLength,
	_Out_opt_ SQLSMALLINT *StringLengthPtr)
{
	esodbc_dbc_st *dbc = DBCH(ConnectionHandle);

	*handled = TRUE;
	switch(InfoType) {
		case SQL_AGGREGATE_FUNCTIONS:
			DBGH(dbc, "requested: aggregate functions (%lu).",
				ESODBC_AGGREGATE_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_AGGREGATE_FUNCTIONS;
			return SQL_SUCCESS;
		case SQL_ALTER_DOMAIN:
			DBGH(dbc, "requested: alter domain (0).");
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
		// case SQL_ALTER_SCHEMA: // undef'd
		case SQL_ALTER_TABLE:
			DBGH(dbc, "requested: alter table (0).");
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
		// case SQL_ANSI_SQL_DATETIME_LITERALS: // undef'd
		case SQL_CATALOG_LOCATION:
			DBGH(dbc, "requested: catalogue location (`%hu`).", SQL_CL_START);
			*(SQLUSMALLINT *)InfoValue = SQL_CL_START;
			return SQL_SUCCESS;
		case SQL_CATALOG_NAME:
			DBGH(dbc, "requested: catalog name support (Y).");
			return write_wstr(dbc, InfoValue, &MK_WSTR("Y"),
					BufferLength, StringLengthPtr);
		case SQL_CATALOG_NAME_SEPARATOR: /* SQL_QUALIFIER_NAME_SEPARATOR */
			/* JDBC[0]: getCatalogSeparator() */
			DBGH(dbc, "requested: catalogue separator (`%s`).",
				ESODBC_CATALOG_SEPARATOR);
			return write_wstr(dbc, InfoValue,
					&MK_WSTR(ESODBC_CATALOG_SEPARATOR), BufferLength,
					StringLengthPtr);
		case SQL_CATALOG_USAGE:
			DBGH(dbc, "requested: catalog usage (%lu).", ESODBC_CATALOG_USAGE);
			*(SQLUINTEGER *)InfoValue = ESODBC_CATALOG_USAGE;
			return SQL_SUCCESS;
		case SQL_COLUMN_ALIAS:
			DBGH(dbc, "requested: column alias (Y).");
			return write_wstr(dbc, InfoValue, &MK_WSTR("Y"),
					BufferLength, StringLengthPtr);
		case SQL_CORRELATION_NAME:
			// JDBC[0]: supportsDifferentTableCorrelationNames()
			DBGH(dbc, "requested: table correlation names (any).");
			/* TODO: JDBC returns true for correlation, but false for
			 * difference. How to signal that in ODBC?? (with no bit mask) */
			*(SQLUSMALLINT *)InfoValue = SQL_CN_ANY;
			return SQL_SUCCESS;
		case SQL_CREATE_ASSERTION:
		case SQL_CREATE_CHARACTER_SET:
		case SQL_CREATE_COLLATION:
		case SQL_CREATE_DOMAIN:
		case SQL_CREATE_SCHEMA:
		case SQL_CREATE_TABLE:
		case SQL_CREATE_TRANSLATION:
			DBGH(dbc, "requested: create statement: %hu (0).", InfoType);
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
		case SQL_DATETIME_LITERALS:
			DBGH(dbc, "requested: SQL92 datetime literals (%lu).",
				ESODBC_DATETIME_LITERALS);
			*(SQLUINTEGER *)InfoValue = ESODBC_DATETIME_LITERALS;
			return SQL_SUCCESS;
		case SQL_DDL_INDEX:
			DBGH(dbc, "requested: index managment (0).");
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
		case SQL_DROP_ASSERTION:
		case SQL_DROP_CHARACTER_SET:
		case SQL_DROP_COLLATION:
		case SQL_DROP_DOMAIN:
		case SQL_DROP_SCHEMA:
		case SQL_DROP_TABLE:
		case SQL_DROP_TRANSLATION:
		case SQL_DROP_VIEW:
			DBGH(dbc, "requested: drop statement: %hu (0).", InfoType);
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
		case SQL_EXPRESSIONS_IN_ORDERBY:
			DBGH(dbc, "requested: expressions in order by (0).");
			return write_wstr(dbc, InfoValue, &MK_WSTR("Y"),
					BufferLength, StringLengthPtr);
		case SQL_GROUP_BY:
			DBGH(dbc, "requested: requirement for GROUP BY (%u).",
				SQL_GB_NO_RELATION);
			*(SQLUSMALLINT *)InfoValue = SQL_GB_NO_RELATION;
			return SQL_SUCCESS;
		case SQL_IDENTIFIER_CASE:
			DBGH(dbc, "requested: identifier case (%hu).", SQL_IC_MIXED );
			*(SQLUSMALLINT *)InfoValue = SQL_IC_MIXED;
			return SQL_SUCCESS;
		case SQL_IDENTIFIER_QUOTE_CHAR:
			/* JDBC[0]: getIdentifierQuoteString() */
			DBGH(dbc, "requested: quoting char (`%s`).", ESODBC_QUOTE_CHAR);
			return write_wstr(dbc, InfoValue, &MK_WSTR(ESODBC_QUOTE_CHAR),
					BufferLength, StringLengthPtr);
		case SQL_INDEX_KEYWORDS:
			DBGH(dbc, "requested: identifier case (%hu).", SQL_IK_NONE);
			*(SQLUSMALLINT *)InfoValue = SQL_IK_NONE;
			return SQL_SUCCESS;
		case SQL_INSERT_STATEMENT:
			DBGH(dbc, "requested: insert support (0).");
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
		case SQL_INTEGRITY:
			DBGH(dbc, "requested: Integrity Enhancement Facility (N).");
			return write_wstr(dbc, InfoValue, &MK_WSTR("N"),
					BufferLength, StringLengthPtr);
		case SQL_KEYWORDS:
			break; // TODO
		case SQL_LIKE_ESCAPE_CLAUSE:
			DBGH(dbc, "requested: like escape clause (Y).");
			return write_wstr(dbc, InfoValue, &MK_WSTR("Y"),
					BufferLength, StringLengthPtr);
		case SQL_NON_NULLABLE_COLUMNS:
			/* JDBC[0]: supportsNonNullableColumns() */
			DBGH(dbc, "requested: nullable columns (true).");
			*(SQLUSMALLINT *)InfoValue = SQL_NNC_NULL;
			return SQL_SUCCESS;
		case SQL_SQL_CONFORMANCE:
			DBGH(dbc, "requested: SQL conformance (%lu).",
				ESODBC_SQL_CONFORMANCE);
			*(SQLUINTEGER *)InfoValue = ESODBC_SQL_CONFORMANCE;
			return SQL_SUCCESS;
		case SQL_OJ_CAPABILITIES: /* SQL_OUTER_JOIN_CAPABILITIES */
			DBGH(dbc, "requested: outer joins capabilities (0).");
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
		case SQL_ORDER_BY_COLUMNS_IN_SELECT:
			DBGH(dbc, "requested: order by columns in select (N).");
			return write_wstr(dbc, InfoValue, &MK_WSTR("N"), BufferLength,
					StringLengthPtr);
		case SQL_OUTER_JOINS:
			DBGH(dbc, "requested: outer join support (Y).");
			return write_wstr(dbc, InfoValue, &MK_WSTR("Y"), BufferLength,
					StringLengthPtr);
		case SQL_PROCEDURES:
			DBGH(dbc, "requested: procedures (N).");
			return write_wstr(dbc, InfoValue, &MK_WSTR("N"),
					BufferLength, StringLengthPtr);
		case SQL_QUOTED_IDENTIFIER_CASE:
			DBGH(dbc, "requested: quoted identifier case (%hu).",
				SQL_IC_SENSITIVE);
			*(SQLUSMALLINT *)InfoValue = SQL_IC_SENSITIVE;
			return SQL_SUCCESS;
		case SQL_SCHEMA_USAGE:
			/* no schema support, but accepted (=currently ignored) by drv */
			DBGH(dbc, "requested: schema usage (%lu).",
				SQL_SU_PROCEDURE_INVOCATION);
			*(SQLUINTEGER *)InfoValue = SQL_SU_PROCEDURE_INVOCATION;
			return SQL_SUCCESS;
		case SQL_SPECIAL_CHARACTERS:
			DBGH(dbc, "requested: special characters (`%s`).",
				ESODBC_SPECIAL_CHARACTERS);
			return write_wstr(dbc, InfoValue,
					&MK_WSTR(ESODBC_SPECIAL_CHARACTERS), BufferLength,
					StringLengthPtr);
		case SQL_SQL92_DATETIME_FUNCTIONS:
			DBGH(dbc, "requested: SQL92 datetime functions (%lu).",
				ESODBC_SQL92_DATETIME_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_SQL92_DATETIME_FUNCTIONS;
			return SQL_SUCCESS;
		case SQL_SQL92_NUMERIC_VALUE_FUNCTIONS:
			DBGH(dbc, "requested: SQL92 numeric value functions (%lu).",
				ESODBC_SQL92_NUMERIC_VALUE_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_SQL92_NUMERIC_VALUE_FUNCTIONS;
			return SQL_SUCCESS;
		case SQL_SQL92_PREDICATES:
			DBGH(dbc, "requested: SQL92 predicates (%lu).",
				ESODBC_SQL92_PREDICATES);
			*(SQLUINTEGER *)InfoValue = ESODBC_SQL92_PREDICATES;
			return SQL_SUCCESS;
		case SQL_SQL92_RELATIONAL_JOIN_OPERATORS:
			DBGH(dbc, "requested: SQL92 relational joins operators (%lu).",
				ESODBC_SQL92_RELATIONAL_JOIN_OPERATORS);
			*(SQLUINTEGER *)InfoValue = ESODBC_SQL92_RELATIONAL_JOIN_OPERATORS;
			return SQL_SUCCESS;
		//case SQL_STRING_FUNCTIONS:
		case SQL_SQL92_STRING_FUNCTIONS:
			DBGH(dbc, "requested: SQL92 string functions (%lu).",
				ESODBC_SQL92_STRING_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_SQL92_STRING_FUNCTIONS;
			return SQL_SUCCESS;
		case SQL_SQL92_VALUE_EXPRESSIONS:
			DBGH(dbc, "requested: SQL92 value expressions (%lu).",
				ODBC_SQL92_VALUE_EXPRESSIONS);
			*(SQLUINTEGER *)InfoValue = ODBC_SQL92_VALUE_EXPRESSIONS;
			return SQL_SUCCESS;
		case SQL_SUBQUERIES:
			DBGH(dbc, "requested: subqueries support (0).");
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
		case SQL_UNION:
			DBGH(dbc, "requested: union support (%lu).", SQL_U_UNION_ALL);
			*(SQLUINTEGER *)InfoValue = SQL_U_UNION_ALL;
			return SQL_SUCCESS;
	}
	*handled = FALSE;
	return SQL_ERROR;
}

static SQLRETURN getinfo_sql_limits(
	BOOL *handled,
	SQLHDBC ConnectionHandle,
	SQLUSMALLINT InfoType,
	_Out_writes_bytes_opt_(BufferLength) SQLPOINTER InfoValue,
	SQLSMALLINT BufferLength,
	_Out_opt_ SQLSMALLINT *StringLengthPtr)
{
	esodbc_dbc_st *dbc = DBCH(ConnectionHandle);
	size_t len;

	*handled = TRUE;
	/*INDENT-OFF*/
	switch(InfoType) {
		do {
			/* spec (alphabetically) ordered */
		case SQL_MAX_BINARY_LITERAL_LEN: len = sizeof(SQLUINTEGER); break;
		case SQL_MAX_CATALOG_NAME_LEN: len = sizeof(SQLUSMALLINT); break;
		case SQL_MAX_CHAR_LITERAL_LEN: len = sizeof(SQLUINTEGER); break;
		case SQL_MAX_COLUMN_NAME_LEN: len = sizeof(SQLUSMALLINT); break;
		case SQL_MAX_COLUMNS_IN_GROUP_BY: len = sizeof(SQLUSMALLINT); break;
		case SQL_MAX_COLUMNS_IN_INDEX: len = sizeof(SQLUSMALLINT); break;
		case SQL_MAX_COLUMNS_IN_ORDER_BY: len = sizeof(SQLUSMALLINT); break;
		case SQL_MAX_COLUMNS_IN_SELECT: len = sizeof(SQLUSMALLINT); break;
		case SQL_MAX_COLUMNS_IN_TABLE: len = sizeof(SQLUSMALLINT); break;
		case SQL_MAX_CURSOR_NAME_LEN: len = sizeof(SQLUSMALLINT); break;
		case SQL_MAX_IDENTIFIER_LEN:
			DBGH(dbc, "requested: max identifier len (%u).",
				ESODBC_MAX_IDENTIFIER_LEN);
			*(SQLUSMALLINT *)InfoValue = ESODBC_MAX_IDENTIFIER_LEN;
			return SQL_SUCCESS;
		case SQL_MAX_INDEX_SIZE: len = sizeof(SQLUINTEGER); break;
		case SQL_MAX_PROCEDURE_NAME_LEN: len = sizeof(SQLUSMALLINT); break;
		case SQL_MAX_ROW_SIZE: len = sizeof(SQLUINTEGER); break;
		case SQL_MAX_ROW_SIZE_INCLUDES_LONG:
			DBGH(dbc, "requested: max row size includes longs (Y).");
			return write_wstr(dbc, InfoValue, &MK_WSTR("Y"),
					BufferLength, StringLengthPtr);
		case SQL_MAX_SCHEMA_NAME_LEN: len = sizeof(SQLUSMALLINT); break;
		case SQL_MAX_STATEMENT_LEN: len = sizeof(SQLUINTEGER); break;
		case SQL_MAX_TABLE_NAME_LEN: len = sizeof(SQLUSMALLINT); break;
		case SQL_MAX_TABLES_IN_SELECT: len = sizeof(SQLUSMALLINT); break;
		case SQL_MAX_USER_NAME_LEN: len = sizeof(SQLUSMALLINT); break;
		} while (0);
			DBGH(dbc, "requested: max %hu (0).", InfoType);
			memset(InfoValue, 0, len);
			return SQL_SUCCESS;
	}
	/*INDENT-ON*/
	*handled = FALSE;
	return SQL_ERROR;
}

static SQLRETURN getinfo_scalars(
	BOOL *handled,
	SQLHDBC ConnectionHandle,
	SQLUSMALLINT InfoType,
	_Out_writes_bytes_opt_(BufferLength) SQLPOINTER InfoValue,
	SQLSMALLINT BufferLength,
	_Out_opt_ SQLSMALLINT *StringLengthPtr)
{
	esodbc_dbc_st *dbc = DBCH(ConnectionHandle);

	*handled = TRUE;
	switch(InfoType) {
		case SQL_CONVERT_FUNCTIONS:
			DBGH(dbc, "requested: convert functions (%lu).",
				ESODBC_CONVERT_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_CONVERT_FUNCTIONS;
			return SQL_SUCCESS;
		case SQL_NUMERIC_FUNCTIONS:
			DBGH(dbc, "requested: numeric functions (%lu).",
				ESODBC_NUMERIC_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_NUMERIC_FUNCTIONS;
			return SQL_SUCCESS;
		case SQL_STRING_FUNCTIONS:
			DBGH(dbc, "requested: string functions (%lu).",
				ESODBC_STRING_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_STRING_FUNCTIONS;
			return SQL_SUCCESS;
		case SQL_SYSTEM_FUNCTIONS:
			DBGH(dbc, "requested: system functions (%lu).",
				ESODBC_SYSTEM_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_SYSTEM_FUNCTIONS;
			return SQL_SUCCESS;
		case SQL_TIMEDATE_ADD_INTERVALS:
			DBGH(dbc, "requested: timedate add intervals (%lu).",
				ESODBC_TIMEDATE_ADD_INTERVALS);
			*(SQLUINTEGER *)InfoValue = ESODBC_TIMEDATE_ADD_INTERVALS;
			return SQL_SUCCESS;
		case SQL_TIMEDATE_DIFF_INTERVALS:
			DBGH(dbc, "requested: timedate diff intervals (%lu).",
				ESODBC_TIMEDATE_DIFF_INTERVALS);
			*(SQLUINTEGER *)InfoValue = ESODBC_TIMEDATE_DIFF_INTERVALS;
			return SQL_SUCCESS;
		case SQL_TIMEDATE_FUNCTIONS:
			DBGH(dbc, "requested: timedate functions (%lu).",
				ESODBC_TIMEDATE_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_TIMEDATE_FUNCTIONS;
			return SQL_SUCCESS;
	}
	*handled = FALSE;
	return SQL_ERROR;
}

static SQLRETURN getinfo_conversion(
	BOOL *handled,
	SQLHDBC ConnectionHandle,
	SQLUSMALLINT InfoType,
	_Out_writes_bytes_opt_(BufferLength) SQLPOINTER InfoValue,
	SQLSMALLINT BufferLength,
	_Out_opt_ SQLSMALLINT *StringLengthPtr)
{
	esodbc_dbc_st *dbc = DBCH(ConnectionHandle);

	*handled = TRUE;
	switch(InfoType) {
		case SQL_CONVERT_BIGINT:
		case SQL_CONVERT_BINARY:
		case SQL_CONVERT_BIT:
		case SQL_CONVERT_CHAR:
		case SQL_CONVERT_DATE:
		case SQL_CONVERT_DECIMAL:
		case SQL_CONVERT_DOUBLE:
		case SQL_CONVERT_FLOAT:
		case SQL_CONVERT_GUID:
		case SQL_CONVERT_INTEGER:
		case SQL_CONVERT_INTERVAL_YEAR_MONTH:
		case SQL_CONVERT_INTERVAL_DAY_TIME:
		case SQL_CONVERT_LONGVARBINARY:
		case SQL_CONVERT_LONGVARCHAR:
		case SQL_CONVERT_NUMERIC:
		case SQL_CONVERT_REAL:
		case SQL_CONVERT_SMALLINT:
		case SQL_CONVERT_TIME:
		case SQL_CONVERT_TIMESTAMP:
		case SQL_CONVERT_TINYINT:
		case SQL_CONVERT_VARBINARY:
		case SQL_CONVERT_VARCHAR:
		case SQL_CONVERT_WCHAR:
		case SQL_CONVERT_WLONGVARCHAR:
		case SQL_CONVERT_WVARCHAR:
			DBGH(dbc, "requested: convert data-type support (0).");
			INFOH(dbc, "no CONVERT scalar function support.");
			*(SQLUINTEGER *)InfoValue = 0;
			return SQL_SUCCESS;
	}
	*handled = FALSE;
	return SQL_ERROR;
}


// [0] x-p-es/sql/jdbc/src/main/java/org/elasticsearch/xpack/sql/jdbc/jdbc/JdbcDatabaseMetaData.java : DatabaseMetaData
/*
 * """
 * The SQL_MAX_DRIVER_CONNECTIONS option in SQLGetInfo specifies how many
 * active connections a particular driver supports.
 * """
 */
SQLRETURN EsSQLGetInfoW(
	SQLHDBC ConnectionHandle,
	SQLUSMALLINT InfoType,
	_Out_writes_bytes_opt_(BufferLength) SQLPOINTER InfoValue,
	SQLSMALLINT BufferLength,
	_Out_opt_ SQLSMALLINT *StringLengthPtr)
{
	esodbc_dbc_st *dbc = DBCH(ConnectionHandle);
	BOOL handled;
	SQLRETURN ret;

	ret = getinfo_driver(&handled, ConnectionHandle, InfoType, InfoValue,
			BufferLength, StringLengthPtr);
	if (handled) {
		return ret;
	}

	ret = getinfo_dbms_product(&handled, ConnectionHandle, InfoType, InfoValue,
			BufferLength, StringLengthPtr);
	if (handled) {
		return ret;
	}

	ret = getinfo_data_source(&handled, ConnectionHandle, InfoType, InfoValue,
			BufferLength, StringLengthPtr);
	if (handled) {
		return ret;
	}

	ret = getinfo_sql(&handled, ConnectionHandle, InfoType, InfoValue,
			BufferLength, StringLengthPtr);
	if (handled) {
		return ret;
	}

	ret = getinfo_sql_limits(&handled, ConnectionHandle, InfoType, InfoValue,
			BufferLength, StringLengthPtr);
	if (handled) {
		return ret;
	}

	ret = getinfo_scalars(&handled, ConnectionHandle, InfoType, InfoValue,
			BufferLength, StringLengthPtr);
	if (handled) {
		return ret;
	}

	ret = getinfo_conversion(&handled, ConnectionHandle, InfoType, InfoValue,
			BufferLength, StringLengthPtr);
	if (handled) {
		return ret;
	}

	ERRH(dbc, "unknown InfoType: %u.", InfoType);
	RET_HDIAGS(dbc, SQL_STATE_HY096);
}


/* TODO: see error.h: esodbc_errors definition note (2.x apps support) */
/* Note: with SQL_DIAG_SQLSTATE DM provides a NULL StringLengthPtr */
SQLRETURN EsSQLGetDiagFieldW(
	SQLSMALLINT HandleType,
	SQLHANDLE Handle,
	SQLSMALLINT RecNumber,
	SQLSMALLINT DiagIdentifier,
	_Out_writes_opt_(_Inexpressible_(BufferLength)) SQLPOINTER DiagInfoPtr,
	SQLSMALLINT BufferLength,
	_Out_opt_ SQLSMALLINT *StringLengthPtr)
{
	esodbc_dbc_st *dbc;
	esodbc_diag_st *diag, bak;
	esodbc_env_st dummy;
	SQLSMALLINT used;
	size_t len;
	void *srcptr;
	wstr_st *wstrp, wstr;
	SQLRETURN ret;

	if (RecNumber <= 0) {
		ERRH(Handle, "record number must be >=1; received: %d.", RecNumber);
		return SQL_ERROR;
	} else if (1 < RecNumber) {
		return SQL_NO_DATA;
	}

	if (! Handle) {
		ERR("null handle provided.");
		return SQL_INVALID_HANDLE;
	}
	diag = &HDRH(Handle)->diag;
	/* GetDiagField can't set diagnostics itself, so use a dummy */
	*HDRH(&dummy) = *HDRH(Handle); /* need a valid hhdr struct */

	/*INDENT-OFF*/
	switch (DiagIdentifier) {
		/* Header Fields */
		case SQL_DIAG_NUMBER:
			if (! DiagInfoPtr) {
				ERRH(Handle, "NULL DiagInfo with SQL_DIAG_NUMBER");
				return SQL_ERROR;
			}
			*(SQLINTEGER *)DiagInfoPtr =
				(diag->state != SQL_STATE_00000) ? 1 : 0;
			DBGH(Handle, "available diagnostics count: %ld.",
					*(SQLINTEGER *)DiagInfoPtr);
			return SQL_SUCCESS;

		case SQL_DIAG_CURSOR_ROW_COUNT:
		case SQL_DIAG_DYNAMIC_FUNCTION:
		case SQL_DIAG_DYNAMIC_FUNCTION_CODE:
		case SQL_DIAG_ROW_COUNT:
			/* should be handled by DM */
			if (HandleType != SQL_HANDLE_STMT) {
				ERRH(Handle, "DiagIdentifier %d called with non-statement "
						"handle type %d.", DiagIdentifier, HandleType);
				return SQL_ERROR;
			}
			ERRH(Handle, "DiagIdentifier %hd is not supported.");
			return SQL_ERROR;

		/* case SQL_DIAG_RETURNCODE: break; -- DM only */

		/* Record Fields */
		do {
		case SQL_DIAG_CLASS_ORIGIN:
			len = (sizeof(ORIG_DISCRIM) - 1) * sizeof (SQLWCHAR);
			assert(len <= sizeof(esodbc_errors[diag->state].code));
			if (memcmp(esodbc_errors[diag->state].code, MK_WPTR(ORIG_DISCRIM),
						len) == 0) {
				wstrp = &MK_WSTR(ORIG_CLASS_ODBC);
			} else {
				wstrp = &MK_WSTR(ORIG_CLASS_ISO);
			}
			break;
		case SQL_DIAG_SUBCLASS_ORIGIN:
			switch (diag->state) {
				case SQL_STATE_01S00:
				case SQL_STATE_01S01:
				case SQL_STATE_01S02:
				case SQL_STATE_01S06:
				case SQL_STATE_01S07:
				case SQL_STATE_07S01:
				case SQL_STATE_08S01:
				case SQL_STATE_21S01:
				case SQL_STATE_21S02:
				case SQL_STATE_25S01:
				case SQL_STATE_25S02:
				case SQL_STATE_25S03:
				case SQL_STATE_42S01:
				case SQL_STATE_42S02:
				case SQL_STATE_42S11:
				case SQL_STATE_42S12:
				case SQL_STATE_42S21:
				case SQL_STATE_42S22:
				case SQL_STATE_HY095:
				case SQL_STATE_HY097:
				case SQL_STATE_HY098:
				case SQL_STATE_HY099:
				case SQL_STATE_HY100:
				case SQL_STATE_HY101:
				case SQL_STATE_HY105:
				case SQL_STATE_HY107:
				case SQL_STATE_HY109:
				case SQL_STATE_HY110:
				case SQL_STATE_HY111:
				case SQL_STATE_HYT00:
				case SQL_STATE_HYT01:
				case SQL_STATE_IM001:
				case SQL_STATE_IM002:
				case SQL_STATE_IM003:
				case SQL_STATE_IM004:
				case SQL_STATE_IM005:
				case SQL_STATE_IM006:
				case SQL_STATE_IM007:
				case SQL_STATE_IM008:
				case SQL_STATE_IM010:
				case SQL_STATE_IM011:
				case SQL_STATE_IM012:
					wstrp = &MK_WSTR(ORIG_CLASS_ODBC);
					break;
				default:
					wstrp = &MK_WSTR(ORIG_CLASS_ISO);
			}
			break;
		} while (0);
			DBGH(Handle, "diagnostic code '"LWPD"' is of class '" LWPDL "'.",
					esodbc_errors[diag->state].code, LWSTR(wstrp));
			return write_wstr(&dummy, DiagInfoPtr, wstrp, BufferLength,
					StringLengthPtr);

		do {
		case SQL_DIAG_CONNECTION_NAME:
		case SQL_DIAG_SERVER_NAME:
			switch (HandleType) {
				case SQL_HANDLE_DBC:
					dbc = DBCH(Handle);
					break;
				case SQL_HANDLE_STMT:
					dbc = STMH(Handle)->hdr.dbc;
					break;
				case SQL_HANDLE_DESC:
					dbc = DSCH(Handle)->hdr.stmt->hdr.dbc;
					break;
				default:
					ERR("unknown handle type %hd, @0x%p.", HandleType, Handle);
					return SQL_ERROR; // SQL_INVALID_HANDLE?
			}
		} while (0);
			/* save (and then restore) the diag state, since this call mustn't
			 * change it */
			bak = dbc->hdr.diag;
			ret = EsSQLGetInfoW(dbc,
					DiagIdentifier == SQL_DIAG_CONNECTION_NAME ?
							SQL_DATA_SOURCE_NAME : SQL_SERVER_NAME,
					DiagInfoPtr, BufferLength, StringLengthPtr);
			dbc->hdr.diag = bak;
			return ret;

		case SQL_DIAG_MESSAGE_TEXT:
			wstr.str = diag->text;
			wstr.cnt = diag->text_len;
			return write_wstr(Handle, DiagInfoPtr, &wstr,
					BufferLength * sizeof(*diag->text), StringLengthPtr);

		do {
		case SQL_DIAG_NATIVE:
			len = sizeof(diag->native_code);
			srcptr = &diag->native_code;
			break;
		case SQL_DIAG_COLUMN_NUMBER:
			len = sizeof(diag->column_number);
			srcptr = &diag->column_number;
			break;
		case SQL_DIAG_ROW_NUMBER:
			len = sizeof(diag->row_number);
			srcptr = &diag->row_number;
			break;
		} while (0);
			if (BufferLength != SQL_IS_POINTER) {
				WARNH(Handle, "BufferLength param not indicating a ptr type.");
			}
			if (! DiagInfoPtr) {
				ERRH(Handle, "integer diagnostic field %hd asked for, but "
						"NULL destination provided.");
				RET_HDIAGS(Handle, SQL_STATE_HY009);
			} else {
				memcpy(DiagInfoPtr, srcptr, len);
			}
			return SQL_SUCCESS;

		case SQL_DIAG_SQLSTATE:
			if (diag->state == SQL_STATE_00000) {
				DBGH(Handle, "no diagnostic available for handle type %d.",
						HandleType);
				return SQL_NO_DATA;
			}
			wstr.str = esodbc_errors[diag->state].code;
			wstr.cnt = wcslen(wstr.str);
			ret = write_wstr(&dummy, DiagInfoPtr, &wstr, BufferLength, &used);
			if (StringLengthPtr) {
				*StringLengthPtr = used;
			} else {
				/* SQLSTATE is always on 5 chars, but this Identifier sticks
				 * out, by not being given a buffer to write this into */
				WARNH(Handle, "SQLSTATE writen on %uB, but no output buffer "
						"provided.", used);
			}
			return ret;

		default:
			ERRH(Handle, "unknown DiagIdentifier: %d.", DiagIdentifier);
			return SQL_ERROR;
	}
	/*INDENT-ON*/

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
	esodbc_env_st dummy;
	SQLRETURN ret;
	SQLSMALLINT used;
	wstr_st wstr;

	if (RecNumber <= 0) {
		ERRH(Handle, "record number must be >=1; received: %d.", RecNumber);
		return SQL_ERROR;
	}
	if (1 < RecNumber) {
		/* XXX: does it make sense to have error FIFOs? maybe, for one
		 * diagnostic per failed fetched row */
		// WARN("no error lists supported (yet).");
		return SQL_NO_DATA;
	}

	if (! Handle) {
		ERRH(Handle, "NULL handle provided.");
		return SQL_ERROR;
	} else if (HandleType == SQL_HANDLE_SENV) {
		/*
		 * https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdiagrec-function :
		 * """
		 * A call to SQLGetDiagRec will return SQL_INVALID_HANDLE if
		 * HandleType is SQL_HANDLE_SENV.
		 * """
		 */
		ERRH(Handle, "shared environment handle type not allowed.");
		return SQL_INVALID_HANDLE;
	} else {
		diag = &HDRH(Handle)->diag;
	}

	if (diag->state == SQL_STATE_00000) {
		INFOH(Handle, "no diagnostic record available for handle type %d.",
			HandleType);
		return SQL_NO_DATA;
	}

	/* not documented in API, but both below can be null. */
	/* API assumes there's always enough buffer here.. */
	/* no err indicator */
	if (Sqlstate) {
		wcscpy(Sqlstate, esodbc_errors[diag->state].code);
	}
	if (NativeError) {
		*NativeError = diag->native_code;
	}

	wstr.str = diag->text;
	wstr.cnt = diag->text_len;
	*HDRH(&dummy) = *HDRH(Handle); /* need a valid hhdr struct */
	ret = write_wstr(&dummy, MessageText, &wstr,
			BufferLength * sizeof(*MessageText), &used);
	if (TextLength) {
		*TextLength = used / sizeof(*MessageText);
	}
	return ret;
}


SQLRETURN EsSQLGetFunctions(SQLHDBC ConnectionHandle,
	SQLUSMALLINT FunctionId,
	_Out_writes_opt_
	(_Inexpressible_("Buffer length pfExists points to depends on fFunction value."))
	SQLUSMALLINT *Supported)
{
	int i;

	if (FunctionId == SQL_API_ODBC3_ALL_FUNCTIONS) {
		DBGH(ConnectionHandle, "ODBC 3.x application asking for supported "
			"function set.");
		memset(Supported, 0,
			SQL_API_ODBC3_ALL_FUNCTIONS_SIZE * sizeof(SQLSMALLINT));
		for (i = 0; i < ESODBC_FUNC_SIZE; i++) {
			SQL_FUNC_SET(Supported, esodbc_functions[i]);
		}
	} else if (FunctionId == SQL_API_ALL_FUNCTIONS) {
		DBGH(ConnectionHandle, "ODBC 2.x application asking for supported "
			"function set.");
		memset(Supported, SQL_FALSE,
			SQL_API_ODBC2_ALL_FUNCTIONS_SIZE * sizeof(SQLUSMALLINT));
		for (i = 0; i < ESODBC_FUNC_SIZE; i++)
			if (esodbc_functions[i] < SQL_API_ODBC2_ALL_FUNCTIONS_SIZE) {
				Supported[esodbc_functions[i]] = SQL_TRUE;
			}
	} else {
		DBGH(ConnectionHandle, "application asking for support of function "
			"#%d.", FunctionId);
		*Supported = SQL_FALSE;
		for (i = 0; i < ESODBC_FUNC_SIZE; i++)
			if (esodbc_functions[i] == FunctionId) {
				*Supported = SQL_TRUE;
				break;
			}
	}

	return SQL_SUCCESS;
}

/*
 * Equivalent of JDBC's getTypeInfo() ([0]:900)
 */
SQLRETURN EsSQLGetTypeInfoW(SQLHSTMT StatementHandle, SQLSMALLINT DataType)
{
	SQLRETURN ret;
	esodbc_stmt_st *stmt = STMH(StatementHandle);

#define SQL_TYPES_STATEMENT	"SYS TYPES"

	switch (DataType) {
		case SQL_ALL_TYPES:
			DBGH(stmt, "requested type description for all supported types.");
			break;

		/* "If the DataType argument specifies a data type which is valid for
		 * the version of ODBC supported by the driver, but is not supported
		 * by the driver, then it will return an empty result set." */
		default:
			ERRH(stmt, "invalid DataType: %d.", DataType);
			FIXME; // FIXME : implement filtering..
			RET_HDIAGS(STMH(StatementHandle), SQL_STATE_HY004);
	}

	ret = EsSQLFreeStmt(stmt, ESODBC_SQL_CLOSE);
	assert(SQL_SUCCEEDED(ret)); /* can't return error */
	ret = attach_sql(stmt, MK_WPTR(SQL_TYPES_STATEMENT),
			sizeof(SQL_TYPES_STATEMENT) - 1);
	if (SQL_SUCCEEDED(ret)) {
		ret = EsSQLExecute(stmt);
	}
	return ret;
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
