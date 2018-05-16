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


#if ODBCVER == 0x0380
/* String constant for supported ODBC version */
#define ESODBC_SQL_SPEC_STRING	"03.80"
#else /* ver==3.8 */
#error "unsupported ODBC version"
#endif /* ver==3.8 */

#define ORIG_DISCRIM	"IM"
#define ORIG_CLASS_ISO	"ISO 9075"
#define ORIG_CLASS_ODBC	"ODBC 3.0"

#if defined(_WIN32) || defined (WIN32)
/* DRV_NAME is a define */
#define DRIVER_NAME	STR(DRV_NAME) ".dll"
#else /* win32 */
#endif /* win32 */



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

/* Note: for input/output size indication (avail/usedp), some functions
 * require character count (eg. SQLGetDiagRec, SQLDescribeCol), some others
 * bytes length (eg.  SQLGetInfo, SQLGetDiagField, SQLGetConnectAttr,
 * EsSQLColAttributeW). */
SQLRETURN write_wptr(esodbc_diag_st *diag,
		SQLWCHAR *dest, const SQLWCHAR *src,
		SQLSMALLINT /*B*/avail, SQLSMALLINT /*B*/*usedp)
{
	size_t src_cnt, awail;
	SQLSMALLINT used;

	if (! dest)
		avail = 0;
	/* needs to be multiple of SQLWCHAR units (2 on Win) */
	if (avail % sizeof(SQLWCHAR)) {
		ERR("invalid buffer length provided: %d.", avail);
		RET_CDIAG(diag, SQL_STATE_HY090, "invalid buffer length provided", 0);
	}
	awail = avail/sizeof(SQLWCHAR);
	src_cnt = wcslen(src);

	/* return value always set to what it needs to be written (excluding \0).*/
	used = (SQLSMALLINT)src_cnt * sizeof(SQLWCHAR);
	if (! usedp) {
		WARN("invalid output buffer provided (NULL) to collect used "
				"space.");
		//RET_cDIAG(diag, SQL_STATE_HY013, "invalid used provided (NULL)", 0);
	} else {
		/* how many bytes are available to return (not how many would be
		 * written into the buffer (which could be less)) */
		*usedp = used;
	}

	if (! dest) {
		/* only return how large of a buffer we need */
		INFO("NULL out buff: returning needed buffer size only (%d).",
				used);
	} else {
		if (awail <= src_cnt) { /* =, since src_cnt doesn't count the \0 */
			wcsncpy(dest, src, awail - /* 0-term */1);
			dest[awail - 1] = 0;

			INFO("not enough buffer size to write required string (plus "
					"terminator): `" LWPD "` [%zd]; available: %d.", src,
					src_cnt, awail);
			RET_DIAG(diag, SQL_STATE_01004, NULL, 0);
		} else {
			wcsncpy(dest, src, src_cnt + /* 0-term */1);
		}
	}

	return SQL_SUCCESS;
}

// [0] x-p-es/sql/jdbc/src/main/java/org/elasticsearch/xpack/sql/jdbc/jdbc/JdbcDatabaseMetaData.java : DatabaseMetaData
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
	esodbc_dbc_st *dbc = DBCH(ConnectionHandle);
	SQLINTEGER string_len;
	SQLRETURN ret;

	switch (InfoType) {
		/* Driver Information */
		/* "what version of odbc a driver complies with" */
		case SQL_DRIVER_ODBC_VER:
			return write_wptr(&dbc->hdr.diag, InfoValue,
					MK_WPTR(ESODBC_SQL_SPEC_STRING), BufferLength,
					StringLengthPtr);

		/* "if the driver can execute functions asynchronously on the
		 * connection handle" */
		case SQL_ASYNC_DBC_FUNCTIONS:
			/* TODO: review@alpha */
			*(SQLUSMALLINT *)InfoValue = SQL_FALSE;
			DBGH(dbc, "requested: support for async fuctions: no.");
			break;

		/* "if the driver supports asynchronous notification" */
		case SQL_ASYNC_NOTIFICATION:
			// FIXME: review@alpha */
			*(SQLUINTEGER *)InfoValue = SQL_ASYNC_NOTIFICATION_NOT_CAPABLE;
			DBGH(dbc, "requested: support for async notifications: no.");
			break;

		/* "the maximum number of active statements that the driver can
		 * support for a connection" */
		//case SQL_ACTIVE_STATEMENTS:
		case SQL_MAX_CONCURRENT_ACTIVITIES:
			*(SQLUSMALLINT *)InfoValue = ESODBC_MAX_CONCURRENT_ACTIVITIES;
			DBGH(dbc, "requested: max active statements per connection: %d.",
					*(SQLUSMALLINT *)InfoValue);
			break;

		case SQL_CURSOR_COMMIT_BEHAVIOR:
		case SQL_CURSOR_ROLLBACK_BEHAVIOR:
			DBGH(dbc, "requested: cursor %s behavior.",
					InfoType == SQL_CURSOR_COMMIT_BEHAVIOR ?
					"commit" : "rollback");
			/* assume this is the  of equivalent of
			 * JDBC's HOLD_CURSORS_OVER_COMMIT */
			*(SQLUSMALLINT *)InfoValue = SQL_CB_PRESERVE;
			break;

		case SQL_GETDATA_EXTENSIONS:
			DBGH(dbc, "requested: GetData extentions.");
			// FIXME: review@alpha
			// TODO: GetData review
			*(SQLUINTEGER *)InfoValue = 0;
			break;

		case SQL_DATA_SOURCE_NAME:
			DBGH(dbc, "requested: data source name: `"LWPD"`.", dbc->dsn.str);
			return write_wptr(&dbc->hdr.diag, InfoValue, dbc->dsn.str,
					BufferLength, StringLengthPtr);

		case SQL_DRIVER_NAME:
			DBGH(dbc, "requested: driver (file) name: %s.", DRIVER_NAME);
			return write_wptr(&dbc->hdr.diag, InfoValue,
					MK_WPTR(DRIVER_NAME), BufferLength, StringLengthPtr);
			break;

		case SQL_DATA_SOURCE_READ_ONLY:
			DBGH(dbc, "requested: if data source is read only (`%s`).",
					ESODBC_DATA_SOURCE_READ_ONLY);
			return write_wptr(&dbc->hdr.diag, InfoValue,
					MK_WPTR(ESODBC_DATA_SOURCE_READ_ONLY), BufferLength,
					StringLengthPtr);

		case SQL_SEARCH_PATTERN_ESCAPE:
			DBGH(dbc, "requested: escape character (`%s`).",
					ESODBC_PATTERN_ESCAPE);
			return write_wptr(&dbc->hdr.diag, InfoValue,
					MK_WPTR(ESODBC_PATTERN_ESCAPE), BufferLength,
					StringLengthPtr);

		case SQL_CORRELATION_NAME:
			// JDBC[0]: supportsDifferentTableCorrelationNames()
			DBGH(dbc, "requested: table correlation names (any).");
			/* TODO: JDBC returns true for correlation, but false for
			 * difference. How to signal that in ODBC?? (with no bit mask) */
			*(SQLUSMALLINT *)InfoValue = SQL_CN_ANY;
			break;

		case SQL_NON_NULLABLE_COLUMNS:
			/* JDBC[0]: supportsNonNullableColumns() */
			DBGH(dbc, "requested: nullable columns (true).");
			*(SQLUSMALLINT *)InfoValue = SQL_NNC_NULL;
			break;

		case SQL_CATALOG_NAME:
			// TODO: can the catalog varry? `SYS CATALOGS` here?
			FIXME; // FIXME
			break;

		case SQL_CATALOG_NAME_SEPARATOR: /* SQL_QUALIFIER_NAME_SEPARATOR */
			/* JDBC[0]: getCatalogSeparator() */
			DBGH(dbc, "requested: catalogue separator (`%s`).",
					ESODBC_CATALOG_SEPARATOR);
			return write_wptr(&dbc->hdr.diag, InfoValue,
					MK_WPTR(ESODBC_CATALOG_SEPARATOR), BufferLength,
					StringLengthPtr);

		case SQL_FILE_USAGE:
			/* JDBC[0]: usesLocalFilePerTable() */
			DBGH(dbc, "requested: file usage: table.");
			/* TODO: JDBC indicates true for file per table; howerver, this
			 * can be apparently used to ask GUI user to ask 'file' or
			 * 'table'; elastic uses index => files? */
			*(SQLUSMALLINT *)InfoValue = SQL_FILE_TABLE;
			break;

		case SQL_CATALOG_TERM: /* SQL_QUALIFIER_TERM */
			/* JDBC[0]: getCatalogSeparator() */
			DBGH(dbc, "requested: catalogue term (`%s`).", ESODBC_CATALOG_TERM);
			return write_wptr(&dbc->hdr.diag, InfoValue,
					MK_WPTR(ESODBC_CATALOG_TERM), BufferLength,
					StringLengthPtr);

		case SQL_MAX_SCHEMA_NAME_LEN: /* SQL_MAX_OWNER_NAME_LEN */
			/* JDBC[0]: getMaxSchemaNameLength() */
			DBGH(dbc, "requested: max schema len (%d).", ESODBC_MAX_SCHEMA_LEN);
			*(SQLUSMALLINT *)InfoValue = ESODBC_MAX_SCHEMA_LEN;
			break;

		case SQL_IDENTIFIER_QUOTE_CHAR:
			/* JDBC[0]: getIdentifierQuoteString() */
			DBGH(dbc, "requested: quoting char (`%s`).", ESODBC_QUOTE_CHAR);
			return write_wptr(&dbc->hdr.diag, InfoValue,
					MK_WPTR(ESODBC_QUOTE_CHAR), BufferLength,
					StringLengthPtr);

		/* what Operations are supported by SQLSetPos  */
		// FIXME: review@alpha
		case SQL_DYNAMIC_CURSOR_ATTRIBUTES1:
			DBGH(dbc, "requested cursor attributes 1.");
			*(SQLUINTEGER *)InfoValue = SQL_CA1_POS_UPDATE;
			break;
		case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1:
			*(SQLUINTEGER *)InfoValue = 0;
			break;
		case SQL_KEYSET_CURSOR_ATTRIBUTES1:
		case SQL_STATIC_CURSOR_ATTRIBUTES1:
			/* "An SQL-92 Intermediate level–conformant driver will usually
			 * return the SQL_CA1_NEXT, SQL_CA1_ABSOLUTE, and SQL_CA1_RELATIVE
			 * options as supported, because the driver supports scrollable
			 * cursors through the embedded SQL FETCH statement. " */
			*(SQLUINTEGER *)InfoValue =
				SQL_CA1_NEXT | SQL_CA1_ABSOLUTE | SQL_CA1_RELATIVE;
			break;

		case SQL_BOOKMARK_PERSISTENCE:
			DBGH(dbc, "requested bookmark persistence (none).");
			*(SQLUINTEGER *)InfoValue = 0; /* no support */
			break;

		case SQL_DATABASE_NAME:
			DBGH(dbc, "requested database name.");
			ret = EsSQLGetConnectAttrW(ConnectionHandle,
					SQL_ATTR_CURRENT_CATALOG, InfoValue,
					(SQLINTEGER)BufferLength, &string_len);
			if (StringLengthPtr)
				*StringLengthPtr = (SQLSMALLINT)string_len;
			return ret;

		case SQL_SCHEMA_TERM:
			DBGH(dbc, "requested schema term (`%s`).", ESODBC_SCHEMA_TERM);
			return write_wptr(&dbc->hdr.diag, InfoValue,
					MK_WPTR(ESODBC_SCHEMA_TERM), BufferLength,
					StringLengthPtr);

		case SQL_TABLE_TERM:
			DBGH(dbc, "requested table term (`%s`).", ESODBC_TABLE_TERM);
			return write_wptr(&dbc->hdr.diag, InfoValue,
					MK_WPTR(ESODBC_TABLE_TERM), BufferLength,
					StringLengthPtr);

		/* no procedures support */
		case SQL_PROCEDURES:
		case SQL_ACCESSIBLE_PROCEDURES:
			DBGH(dbc, "requested: procedures support (`%s`).",
					ESODBC_PROCEDURES);
			return write_wptr(&dbc->hdr.diag, InfoValue,
					MK_WPTR(ESODBC_PROCEDURES), BufferLength,
					StringLengthPtr);
		case SQL_MAX_PROCEDURE_NAME_LEN:
			DBGH(dbc, "requested max procedure name len (0).");
			*(SQLUSMALLINT *)InfoValue = 0; /* no support */
			break;
		case SQL_PROCEDURE_TERM:
			DBGH(dbc, "requested: procedure term (``).");
			return write_wptr(&dbc->hdr.diag, InfoValue, MK_WPTR(""),
					BufferLength, StringLengthPtr);

		case SQL_TXN_ISOLATION_OPTION:
			DBGH(dbc, "requested: transaction isolation options (SQL_TXN_*).");
			/* see comment related to definition; TODO */
			*(SQLUINTEGER *)InfoValue = ESODBC_DEF_TXN_ISOLATION;
			break;

		case SQL_SQL92_PREDICATES:
			DBGH(dbc, "requested: SQL92 predicates (%lu).",
					ESODBC_SQL92_PREDICATES);
			*(SQLUINTEGER *)InfoValue = ESODBC_SQL92_PREDICATES;
			break;

		case SQL_SQL92_RELATIONAL_JOIN_OPERATORS:
			DBGH(dbc, "requested: SQL92 relational joins operators (%lu).",
					ESODBC_SQL92_RELATIONAL_JOIN_OPERATORS);
			*(SQLUINTEGER *)InfoValue = ESODBC_SQL92_RELATIONAL_JOIN_OPERATORS;
			break;

		case SQL_OJ_CAPABILITIES: /* SQL_OUTER_JOIN_CAPABILITIES */
			DBGH(dbc, "requested: outer joins capabilities (%lu).",
					ESODBC_OJ_CAPABILITIES);
			*(SQLUINTEGER *)InfoValue = ESODBC_OJ_CAPABILITIES;
			break;

		case SQL_SQL92_DATETIME_FUNCTIONS:
			DBGH(dbc, "requested: SQL92 datetime functions (%lu).",
					ESODBC_SQL92_DATETIME_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_SQL92_DATETIME_FUNCTIONS;
			break;

		//case SQL_STRING_FUNCTIONS:
		case SQL_SQL92_STRING_FUNCTIONS:
			DBGH(dbc, "requested: SQL92 string functions (%lu).",
					ESODBC_SQL92_STRING_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_SQL92_STRING_FUNCTIONS;
			break;

		case SQL_SQL92_NUMERIC_VALUE_FUNCTIONS:
			DBGH(dbc, "requested: SQL92 numeric value functions (%lu).",
					ESODBC_SQL92_NUMERIC_VALUE_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_SQL92_NUMERIC_VALUE_FUNCTIONS;
			break;

		case SQL_SQL92_VALUE_EXPRESSIONS:
			DBGH(dbc, "requested: SQL92 value expressions (%lu).",
					ODBC_SQL92_VALUE_EXPRESSIONS);
			*(SQLUINTEGER *)InfoValue = ODBC_SQL92_VALUE_EXPRESSIONS;
			break;

		case SQL_CONVERT_FUNCTIONS:
			DBGH(dbc, "requested: convert functions (%lu).",
					ESODBC_CONVERT_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_CONVERT_FUNCTIONS;
			break;

		case SQL_SYSTEM_FUNCTIONS:
			DBGH(dbc, "requested: system functions (%lu).",
					ESODBC_SYSTEM_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_SYSTEM_FUNCTIONS;
			break;

		case SQL_DATETIME_LITERALS:
			DBGH(dbc, "requested: SQL92 datetime literals (%lu).",
					ESODBC_DATETIME_LITERALS);
			*(SQLUINTEGER *)InfoValue = ESODBC_DATETIME_LITERALS;
			break;

		case SQL_TIMEDATE_DIFF_INTERVALS:
			DBGH(dbc, "requested: timedate diff intervals (%lu).",
					ESODBC_TIMEDATE_DIFF_INTERVALS);
			*(SQLUINTEGER *)InfoValue = ESODBC_TIMEDATE_DIFF_INTERVALS;
			break;

		case SQL_TIMEDATE_ADD_INTERVALS:
			DBGH(dbc, "requested: timedate add intervals (%lu).",
					ESODBC_TIMEDATE_ADD_INTERVALS);
			*(SQLUINTEGER *)InfoValue = ESODBC_TIMEDATE_ADD_INTERVALS;
			break;

		case SQL_TIMEDATE_FUNCTIONS:
			DBGH(dbc, "requested: timedate functions (%lu).",
					ESODBC_TIMEDATE_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_TIMEDATE_FUNCTIONS;
			break;

		case SQL_NUMERIC_FUNCTIONS:
			DBGH(dbc, "requested: numeric functions (%lu).",
					ESODBC_NUMERIC_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_NUMERIC_FUNCTIONS;
			break;

		case SQL_STRING_FUNCTIONS:
			DBGH(dbc, "requested: string functions (%lu).",
					ESODBC_STRING_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_STRING_FUNCTIONS;
			break;

		case SQL_AGGREGATE_FUNCTIONS:
			DBGH(dbc, "requested: aggregate functions (%lu).",
					ESODBC_AGGREGATE_FUNCTIONS);
			*(SQLUINTEGER *)InfoValue = ESODBC_AGGREGATE_FUNCTIONS;
			break;

		case SQL_SCHEMA_USAGE:
			DBGH(dbc, "requested: schema usage (%lu).", ESODBC_SCHEMA_USAGE);
			*(SQLUINTEGER *)InfoValue = ESODBC_SCHEMA_USAGE;
			break;

		case SQL_CATALOG_USAGE:
			DBGH(dbc, "requested: catalog usage (%lu).", ESODBC_CATALOG_USAGE);
			*(SQLUINTEGER *)InfoValue = ESODBC_CATALOG_USAGE;
			break;

		case SQL_QUOTED_IDENTIFIER_CASE:
			DBGH(dbc, "requested: quoted identifier case (%lu).",
					ESODBC_QUOTED_IDENTIFIER_CASE);
			*(SQLUSMALLINT *)InfoValue = ESODBC_QUOTED_IDENTIFIER_CASE;
			break;

		case SQL_SPECIAL_CHARACTERS:
			DBGH(dbc, "requested: special characters (`%s`).",
					ESODBC_SPECIAL_CHARACTERS);
			return write_wptr(&dbc->hdr.diag, InfoValue,
					MK_WPTR(ESODBC_SPECIAL_CHARACTERS), BufferLength,
					StringLengthPtr);
			break;

		case SQL_MAX_IDENTIFIER_LEN:
			DBGH(dbc, "requested: max identifier len (%u).",
					ESODBC_MAX_IDENTIFIER_LEN);
			*(SQLUSMALLINT *)InfoValue = ESODBC_MAX_IDENTIFIER_LEN;
			break;

		case SQL_COLUMN_ALIAS:
			DBGH(dbc, "requested: column alias (`%s`).", ESODBC_COLUMN_ALIAS);
			return write_wptr(&dbc->hdr.diag, InfoValue,
					MK_WPTR(ESODBC_COLUMN_ALIAS), BufferLength,
					StringLengthPtr);

		case SQL_SQL_CONFORMANCE:
			DBGH(dbc, "requested: SQL conformance (%lu).",
					ESODBC_SQL_CONFORMANCE);
			*(SQLUINTEGER *)InfoValue = ESODBC_SQL_CONFORMANCE;
			break;

		case SQL_ODBC_INTERFACE_CONFORMANCE:
			DBGH(dbc, "requested: ODBC interface conformance (%lu).",
					ESODBC_ODBC_INTERFACE_CONFORMANCE);
			*(SQLUINTEGER *)InfoValue = ESODBC_ODBC_INTERFACE_CONFORMANCE;
			break;

		case SQL_DRIVER_VER:
			DBGH(dbc, "requested: driver version (`%s`).", ESODBC_DRIVER_VER);
			return write_wptr(&dbc->hdr.diag, InfoValue,
					MK_WPTR(ESODBC_DRIVER_VER), BufferLength,
					StringLengthPtr);

		case SQL_DBMS_VER:
			DBGH(dbc, "requested: DBMS version (`%s`).",
					ESODBC_ELASTICSEARCH_VER);
			return write_wptr(&dbc->hdr.diag, InfoValue,
					MK_WPTR(ESODBC_ELASTICSEARCH_VER), BufferLength,
					StringLengthPtr);

		case SQL_DBMS_NAME:
			DBGH(dbc, "requested: DBMS name (`%s`).",
					ESODBC_ELASTICSEARCH_NAME);
			return write_wptr(&dbc->hdr.diag, InfoValue,
					MK_WPTR(ESODBC_ELASTICSEARCH_NAME), BufferLength,
					StringLengthPtr);

		case SQL_TXN_CAPABLE: /* SQL_TRANSACTION_CAPABLE */
			DBGH(dbc, "requested: transaction capable (%u).",
					ESODBC_TXN_CAPABLE);
			*(SQLUSMALLINT *)InfoValue = ESODBC_TXN_CAPABLE;
			break;

		case SQL_CONVERT_BIGINT:
		case SQL_CONVERT_BINARY:
		case SQL_CONVERT_BIT:
		case SQL_CONVERT_CHAR:
		case SQL_CONVERT_DATE:
		case SQL_CONVERT_DECIMAL:
		case SQL_CONVERT_DOUBLE:
		case SQL_CONVERT_FLOAT:
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
			DBGH(dbc, "requested: convert data-type support (0).");
			INFOH(dbc, "no CONVERT scalar function support.");
			*(SQLUINTEGER *)InfoValue = 0;
			break;

		default:
			ERRH(dbc, "unknown InfoType: %u.", InfoType);
			RET_HDIAGS(dbc, SQL_STATE_HYC00/*096?*/);
	}

	return SQL_SUCCESS;
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
	esodbc_diag_st *diag, dummy;
	SQLSMALLINT used;
	size_t len;
	SQLWCHAR *wptr;
	SQLRETURN ret;

	if (RecNumber <= 0) {
		ERRH(Handle, "record number must be >=1; received: %d.", RecNumber);
		return SQL_ERROR;
	}
	if (1 < RecNumber) {
		/* XXX: does it make sense to have error FIFOs? see: EsSQLGetDiagRec */
		// WARN("no error lists supported (yet).");
		return SQL_NO_DATA;
	}

	if (! Handle) {
		ERR("null handle provided.");
		return SQL_INVALID_HANDLE;
	}

	diag = &HDRH(Handle)->diag;

	switch(DiagIdentifier) {
		/* Header Fields */
		case SQL_DIAG_NUMBER:
			if (StringLengthPtr);
				*StringLengthPtr = sizeof(SQLINTEGER);
			if (DiagInfoPtr) {
				// FIXME: check HandleType's record count (1 or 0)
				*(SQLINTEGER *)DiagInfoPtr = 0;
				DBGH(Handle, "available diagnostics: %d.",
						*(SQLINTEGER *)DiagInfoPtr);
			} else {
				DBGH(Handle, "no DiagInfo buffer provided - returning ");
				return SQL_SUCCESS_WITH_INFO;
			}
			FIXME; // FIXME
			break;
		case SQL_DIAG_CURSOR_ROW_COUNT:
		case SQL_DIAG_DYNAMIC_FUNCTION:
		case SQL_DIAG_DYNAMIC_FUNCTION_CODE:
		case SQL_DIAG_ROW_COUNT:
			if (HandleType != SQL_HANDLE_STMT) {
				ERRH(Handle, "DiagIdentifier %d called with non-statement "
						"handle type %d.", DiagIdentifier, HandleType);
				return SQL_ERROR;
			}
			// FIXME
			FIXME;
			//break;
		/* case SQL_DIAG_RETURNCODE: break; -- DM only */

		/* Record Fields */
		do {
		case SQL_DIAG_CLASS_ORIGIN:
			len = (sizeof(ORIG_DISCRIM) - 1) * sizeof (SQLWCHAR);
			assert(len <= sizeof(esodbc_errors[diag->state].code));
			if (memcmp(esodbc_errors[diag->state].code, MK_WPTR(ORIG_DISCRIM),
						len) == 0) {
				wptr = MK_WPTR(ORIG_CLASS_ODBC);
			} else {
				wptr = MK_WPTR(ORIG_CLASS_ISO);
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
					wptr = MK_WPTR(ORIG_CLASS_ODBC);
					break;
				default:
					wptr = MK_WPTR(ORIG_CLASS_ISO);
			}
			break;
		} while (0);
			DBGH(Handle, "diagnostic code '"LWPD"' is of class '"LWPD"'.",
					esodbc_errors[diag->state].code, wptr);
			return write_wptr(&dummy, DiagInfoPtr, wptr, BufferLength,
					StringLengthPtr);
		
		case SQL_DIAG_CONNECTION_NAME:
		/* same as SQLGetInfo(SQL_DATA_SOURCE_NAME) */
		case SQL_DIAG_SERVER_NAME: /* TODO: keep same as _CONNECTION_NAME? */
			switch (HandleType) {
				case SQL_HANDLE_DBC:
					wptr = DBCH(Handle)->dsn.str;
					break;
				case SQL_HANDLE_STMT:
					wptr = STMH(Handle)->hdr.dbc->dsn.str;
					break;
				case SQL_HANDLE_DESC:
					wptr = DSCH(Handle)->hdr.stmt->hdr.dbc->dsn.str;
					break;
				default:
					wptr = MK_WPTR("");
			}
			DBGH(Handle, "inquired connection name (`"LWPD"`)", wptr);
			return write_wptr(&dummy, DiagInfoPtr, wptr, BufferLength,
					StringLengthPtr);
		

		case SQL_DIAG_MESSAGE_TEXT: //break;
		case SQL_DIAG_NATIVE: //break;
		case SQL_DIAG_COLUMN_NUMBER: //break;
		case SQL_DIAG_ROW_NUMBER: //break;
			FIXME; // FIXME

		case SQL_DIAG_SQLSTATE:
			if (diag->state == SQL_STATE_00000) {
				DBGH(Handle, "no diagnostic available for handle type %d.",
						HandleType);
				return SQL_NO_DATA;
			}
			/* GetDiagField can't set diagnostics itself, so use a dummy */
			ret = write_wptr(&dummy, DiagInfoPtr,
					esodbc_errors[diag->state].code, BufferLength, &used);
			if (StringLengthPtr)
				*StringLengthPtr = used;
			else
				/* SQLSTATE is always on 5 chars, but this Identifier sticks
				 * out, by not being given a buffer to write this into */
				WARNH(Handle, "SQLSTATE writen on %uB, but no output buffer "
						"provided.", used);
			return ret;

		default:
			ERRH(Handle, "unknown DiagIdentifier: %d.", DiagIdentifier);
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
	esodbc_diag_st *diag, dummy;
	SQLRETURN ret;
	SQLSMALLINT used;

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
	if (Sqlstate)
		wcscpy(Sqlstate, esodbc_errors[diag->state].code);
	if (NativeError)
		*NativeError = diag->native_code;

	ret = write_wptr(&dummy, MessageText, diag->text,
			BufferLength * sizeof(*MessageText), &used);
	if (TextLength)
		*TextLength = used / sizeof(*MessageText);
	return ret;
}


SQLRETURN EsSQLGetFunctions(SQLHDBC ConnectionHandle,
		SQLUSMALLINT FunctionId,
		_Out_writes_opt_(_Inexpressible_("Buffer length pfExists points to depends on fFunction value.")) SQLUSMALLINT *Supported)
{
	int i;

	if (FunctionId == SQL_API_ODBC3_ALL_FUNCTIONS) {
		DBGH(ConnectionHandle, "ODBC 3.x application asking for supported "
				"function set.");
		memset(Supported, 0,
				SQL_API_ODBC3_ALL_FUNCTIONS_SIZE * sizeof(SQLSMALLINT));
		for (i = 0; i < ESODBC_FUNC_SIZE; i++)
			SQL_FUNC_SET(Supported, esodbc_functions[i]);
	} else if (FunctionId == SQL_API_ALL_FUNCTIONS) {
		DBGH(ConnectionHandle, "ODBC 2.x application asking for supported "
				"function set.");
		memset(Supported, SQL_FALSE,
				SQL_API_ODBC2_ALL_FUNCTIONS_SIZE * sizeof(SQLUSMALLINT));
		for (i = 0; i < ESODBC_FUNC_SIZE; i++)
			if (esodbc_functions[i] < SQL_API_ODBC2_ALL_FUNCTIONS_SIZE)
				Supported[esodbc_functions[i]] = SQL_TRUE;
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

	// TODO: does this require connecting to the server?
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
	if (SQL_SUCCEEDED(ret))
		ret = post_statement(stmt);
	return ret;
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
