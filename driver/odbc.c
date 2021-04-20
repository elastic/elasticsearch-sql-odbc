/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include "log.h"
#include "tracing.h"
#include "handles.h"
#include "info.h"
#include "connect.h"
#include "queries.h"
#include "convert.h"
#include "catalogue.h"
#include "tinycbor.h"


#define RET_NOT_IMPLEMENTED(hnd) \
	do { \
		ERR("not implemented."); \
		RET_HDIAGS(hnd, SQL_STATE_HYC00); \
	} while (0)

#ifdef WITH_OAPI_TIMING
volatile LONG64 api_ticks = 0;
clock_t thread_local in_ticks = 0;
#endif /* WITH_API_TIMING */

static BOOL driver_init()
{
	if (! log_init()) {
		return FALSE;
	}
#ifndef NDEBUG
	if (_gf_log) {
		ERR("leak reporting: on."); /* force create the log handle */
		_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
		_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
		_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_WARN, _gf_log->handle);
		_CrtSetReportFile(_CRT_ERROR, _gf_log->handle);
		_CrtSetReportFile(_CRT_ASSERT, _gf_log->handle);
	}
#endif /* !NDEBUG */
	INFO("initializing driver.");
	if (! queries_init()) {
		return FALSE;
	}
	convert_init();
	if (! connect_init()) {
		return FALSE;
	}
	return TRUE;
}

static void driver_cleanup()
{
	connect_cleanup();
	tinycbor_cleanup();

#	ifdef WITH_EXTENDED_BUFF_LOG
	cstr_hex_dump(NULL); /* util.[ch] */
#	endif /* WITH_EXTENDED_BUFF_LOG */
}

BOOL WINAPI DllMain(
	HINSTANCE hinstDLL,  // handle to DLL module
	DWORD fdwReason,     // reason for calling function
	LPVOID lpReserved)  // reserved
{
	SQLWCHAR path[MAX_PATH];
	// Perform actions based on the reason for calling.
	switch (fdwReason) {
		// Initialize once for each new process.
		// Return FALSE to fail DLL load.
		case DLL_PROCESS_ATTACH:
			if (! driver_init()) {
				return FALSE;
			}
			INFO("process %u attached.", GetCurrentProcessId());
			if (GetModuleFileNameW(NULL, path, sizeof(path)/sizeof(*path))
				> 0) {
				INFO("process path: `" PFWP_DESC "`.", path);
			}
			break;

		// Do thread-specific initialization.
		case DLL_THREAD_ATTACH:
			DBG("thread %u attached.", GetCurrentThreadId());
			break;

		// Do thread-specific cleanup.
		case DLL_THREAD_DETACH:
			DBG("thread %u dettached.", GetCurrentThreadId());
			break;

		// Perform any necessary cleanup.
		case DLL_PROCESS_DETACH:
			driver_cleanup();
#ifndef NDEBUG
			if (_gf_log) {
				ERR("dumping tracked leaks (log.c leak is safe to ignore):");
				/* _CrtDumpMemoryLeaks() will always report at least one leak,
				 * that of the allocated logger itself that the function uses
				 * to log into. This is freed below, in log_cleanup(). */
				ERR("leaks dumped: %d.", _CrtDumpMemoryLeaks());
			}
#endif /* !NDEBUG */
#ifdef WITH_OAPI_TIMING
			INFO("total in-driver time (secs): %.3f.",
				(float)api_ticks / CLOCKS_PER_SEC);
#endif /* WITH_OAPI_TIMING */
			INFO("process %u detaching.", GetCurrentProcessId());
			log_cleanup();
			break;
	}

	return TRUE;
}

/*
 * Connecting to a data source
 */

SQLRETURN SQL_API SQLAllocHandle(SQLSMALLINT HandleType,
	SQLHANDLE InputHandle, _Out_ SQLHANDLE *OutputHandle)
{
	SQLRETURN ret;
	TRACE3(_IN, InputHandle, "dpp", HandleType, InputHandle, OutputHandle);
	/* no synchronization required */
	ret = EsSQLAllocHandle(HandleType, InputHandle, OutputHandle);
	TRACE4(_OUT, InputHandle, "dhpp", ret, HandleType, InputHandle,
		*OutputHandle);
	return ret;
}

/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/unicode-drivers :
 * """
 * A Unicode driver must export SQLConnectW to be recognized as a Unicode
 * driver by the Driver Manager.
 * """
 */
SQLRETURN SQL_API SQLConnectW
(
	SQLHDBC             hdbc,
	_In_reads_(cchDSN) SQLWCHAR *szDSN,
	SQLSMALLINT         cchDSN,
	_In_reads_(cchUID) SQLWCHAR *szUID,
	SQLSMALLINT         cchUID,
	_In_reads_(cchAuthStr) SQLWCHAR *szAuthStr,
	SQLSMALLINT         cchAuthStr
)
{
#ifndef NDEBUG /* don't print the PWD */
	const char *fmt_in = "pWhWhWh";
	const char *fmt_out = "dpWhWhWh";
#else /* NDEBUG */
	const char *fmt_in = "pWhWhph";
	const char *fmt_out = "dpWhWhph";
#endif /* NDEBUG */
	SQLRETURN ret;
	TRACE7(_IN, hdbc, fmt_in, hdbc, szDSN, cchDSN, szUID, cchUID,
		szAuthStr, cchAuthStr);
	HND_LOCK(hdbc);
	ret = EsSQLConnectW(hdbc, szDSN, cchDSN, szUID, cchUID,
			szAuthStr, cchAuthStr);
	HND_UNLOCK(hdbc);
	TRACE8(_OUT, hdbc, fmt_out, ret, hdbc, szDSN, cchDSN, szUID, cchUID,
		szAuthStr, cchAuthStr);
	return ret;
}

SQLRETURN SQL_API SQLDriverConnectW
(
	SQLHDBC             hdbc,
	SQLHWND             hwnd,
	_In_reads_(cchConnStrIn)
	SQLWCHAR           *szConnStrIn,
	SQLSMALLINT         cchConnStrIn,
	_Out_writes_opt_(cchConnStrOutMax)
	SQLWCHAR           *szConnStrOut,
	SQLSMALLINT         cchConnStrOutMax,
	_Out_opt_
	SQLSMALLINT        *pcchConnStrOut,
	SQLUSMALLINT        fDriverCompletion
)
{
#ifndef NDEBUG /* don't print the PWD */
	const char *fmt_in = "ppWhphpH";
	const char *fmt_out = "dppWhWhtH";
#else /* NDEBUG */
	const char *fmt_in = "ppphphpH";
	const char *fmt_out = "dppphWhtH";
#endif /* NDEBUG */
	SQLRETURN ret;
	TRACE8(_IN, hdbc, fmt_in, hdbc, hwnd, szConnStrIn, cchConnStrIn,
		szConnStrOut, cchConnStrOutMax, pcchConnStrOut, fDriverCompletion);
	HND_LOCK(hdbc);
	ret = EsSQLDriverConnectW(hdbc, hwnd, szConnStrIn, cchConnStrIn,
			szConnStrOut, cchConnStrOutMax, pcchConnStrOut, fDriverCompletion);
	HND_UNLOCK(hdbc);
	TRACE9(_OUT, hdbc, fmt_out, ret, hdbc, hwnd, szConnStrIn, cchConnStrIn,
		szConnStrOut, cchConnStrOutMax, pcchConnStrOut, fDriverCompletion);
	return ret;
}

SQLRETURN SQL_API SQLBrowseConnectW
(
	SQLHDBC             hdbc,
	_In_reads_(cchConnStrIn) SQLWCHAR *szConnStrIn,
	SQLSMALLINT         cchConnStrIn,
	_Out_writes_opt_(cchConnStrOutMax) SQLWCHAR *szConnStrOut,
	SQLSMALLINT         cchConnStrOutMax,
	_Out_opt_
	SQLSMALLINT        *pcchConnStrOut
)
{
	RET_NOT_IMPLEMENTED(hdbc);
}

SQLRETURN  SQL_API SQLGetInfoW(SQLHDBC ConnectionHandle,
	SQLUSMALLINT InfoType,
	_Out_writes_bytes_opt_(BufferLength) SQLPOINTER InfoValue,
	SQLSMALLINT BufferLength,
	_Out_opt_ SQLSMALLINT *StringLengthPtr)
{
	SQLRETURN ret;
	TRACE5(_IN, ConnectionHandle, "pHphp", ConnectionHandle, InfoType,
		InfoValue, BufferLength, StringLengthPtr);
	/* Note_sync: no synchronization really required for setting/getting of
	 * integer attributes (atomic) or string ones (= reading static string
	 * locations), but error handling involves posting an SQL state & string,
	 * which needs serialization. */
	HND_LOCK(ConnectionHandle);
	ret = EsSQLGetInfoW(ConnectionHandle, InfoType, InfoValue,
			BufferLength, StringLengthPtr);
	HND_UNLOCK(ConnectionHandle);
	TRACE6(_OUT, ConnectionHandle, "dpHpht", ret, ConnectionHandle, InfoType,
		InfoValue, BufferLength, StringLengthPtr);
	return ret;
}

SQLRETURN  SQL_API SQLGetFunctions(SQLHDBC ConnectionHandle,
	SQLUSMALLINT FunctionId,
	_Out_writes_opt_
	(_Inexpressible_("Buffer length pfExists points to depends on fFunction value."))
	SQLUSMALLINT *Supported)
{
	SQLRETURN ret;
	TRACE3(_IN, ConnectionHandle, "pHp", ConnectionHandle, FunctionId,
		Supported);
	/* no synchronization required */
	ret = EsSQLGetFunctions(ConnectionHandle, FunctionId, Supported);
	TRACE4(_IN, ConnectionHandle, "dpHT", ret, ConnectionHandle, FunctionId,
		Supported);
	return ret;
}

SQLRETURN SQL_API   SQLGetTypeInfoW(
	SQLHSTMT            StatementHandle,
	SQLSMALLINT         DataType)
{
	SQLRETURN ret;
	TRACE2(_IN, StatementHandle, "ph", StatementHandle, DataType);
	HND_LOCK(StatementHandle);
	ret = EsSQLGetTypeInfoW(StatementHandle, DataType);
	HND_UNLOCK(StatementHandle);
	TRACE3(_OUT, StatementHandle, "dph", ret, StatementHandle, DataType);
	return ret;
}


/*
 *
 * Setting and retrieving driver attributes
 *
 */

SQLRETURN  SQL_API SQLSetConnectAttrW(
	SQLHDBC ConnectionHandle,
	SQLINTEGER Attribute,
	_In_reads_bytes_opt_(StringLength) SQLPOINTER Value,
	SQLINTEGER StringLength)
{
	SQLRETURN ret;
	TRACE4(_IN, ConnectionHandle, "plpl", ConnectionHandle, Attribute,
		Value, StringLength);
	HND_LOCK(ConnectionHandle); /* see Note_sync above */
	ret = EsSQLSetConnectAttrW(ConnectionHandle, Attribute,
			Value, StringLength);
	HND_UNLOCK(ConnectionHandle);
	TRACE5(_OUT, ConnectionHandle, "dplpl", ret, ConnectionHandle, Attribute,
		Value, StringLength);
	return ret;
}

SQLRETURN SQL_API SQLGetConnectAttrW(
	SQLHDBC        ConnectionHandle,
	SQLINTEGER     Attribute,
	_Out_writes_opt_(_Inexpressible_(cbValueMax)) SQLPOINTER ValuePtr,
	SQLINTEGER     BufferLength,
	_Out_opt_ SQLINTEGER *StringLengthPtr)
{
	SQLRETURN ret;
	TRACE5(_IN, ConnectionHandle, "plplp", ConnectionHandle, Attribute,
		ValuePtr, BufferLength, StringLengthPtr);
	HND_LOCK(ConnectionHandle);
	ret = EsSQLGetConnectAttrW(ConnectionHandle, Attribute,
			ValuePtr, BufferLength, StringLengthPtr);
	HND_UNLOCK(ConnectionHandle);
	TRACE6(_OUT, ConnectionHandle, "dplplg", ret, ConnectionHandle, Attribute,
		ValuePtr, BufferLength, StringLengthPtr);
	return ret;
}


SQLRETURN  SQL_API SQLSetEnvAttr(
	SQLHENV EnvironmentHandle,
	SQLINTEGER Attribute,
	_In_reads_bytes_opt_(StringLength) SQLPOINTER Value,
	SQLINTEGER StringLength)
{
	SQLRETURN ret;
	TRACE4(_IN, EnvironmentHandle, "plpl", EnvironmentHandle, Attribute, Value,
		StringLength);
	HND_LOCK(EnvironmentHandle); /* see Note_sync above */
	ret = EsSQLSetEnvAttr(EnvironmentHandle, Attribute, Value, StringLength);
	HND_UNLOCK(EnvironmentHandle);
	TRACE5(_OUT, EnvironmentHandle, "dplpl", ret, EnvironmentHandle, Attribute,
		Value, StringLength);
	return ret;
}

SQLRETURN  SQL_API SQLGetEnvAttr(
	SQLHENV EnvironmentHandle,
	SQLINTEGER Attribute,
	_Out_writes_(_Inexpressible_(BufferLength)) SQLPOINTER Value,
	SQLINTEGER BufferLength,
	_Out_opt_ SQLINTEGER *StringLength)
{
	SQLRETURN ret;
	TRACE5(_IN, EnvironmentHandle, "plplp", EnvironmentHandle,
		Attribute, Value, BufferLength, StringLength);
	HND_LOCK(EnvironmentHandle); /* see Note_sync above */
	ret = EsSQLGetEnvAttr(EnvironmentHandle,
			Attribute, Value, BufferLength, StringLength);
	HND_UNLOCK(EnvironmentHandle);
	TRACE6(_OUT, EnvironmentHandle, "dplplg", ret, EnvironmentHandle,
		Attribute, Value, BufferLength, StringLength);
	return ret;
}

SQLRETURN SQL_API SQLSetStmtAttrW(
	SQLHSTMT           StatementHandle,
	SQLINTEGER         Attribute,
	SQLPOINTER         ValuePtr,
	SQLINTEGER         BufferLength)
{
	SQLRETURN ret;
	TRACE4(_IN, StatementHandle, "plpl", StatementHandle, Attribute,
		ValuePtr, BufferLength);
	HND_LOCK(StatementHandle);
	ret = EsSQLSetStmtAttrW(StatementHandle, Attribute,
			ValuePtr, BufferLength);
	HND_UNLOCK(StatementHandle);
	TRACE5(_OUT, StatementHandle, "dplpl", ret, StatementHandle, Attribute,
		ValuePtr, BufferLength);
	return ret;
}

SQLRETURN SQL_API SQLGetStmtAttrW(
	SQLHSTMT           StatementHandle,
	SQLINTEGER         Attribute,
	SQLPOINTER         ValuePtr,
	SQLINTEGER         BufferLength,
	SQLINTEGER     *StringLengthPtr)
{
	SQLRETURN ret;
	TRACE5(_IN, StatementHandle, "plplp", StatementHandle, Attribute,
		ValuePtr, BufferLength, StringLengthPtr);
	HND_LOCK(StatementHandle);
	ret = EsSQLGetStmtAttrW(StatementHandle, Attribute,
			ValuePtr, BufferLength, StringLengthPtr);
	HND_UNLOCK(StatementHandle);
	TRACE6(_OUT, StatementHandle, "dplplg", ret, StatementHandle, Attribute,
		ValuePtr, BufferLength, StringLengthPtr);
	return ret;
}


/*
 *
 * Setting and retrieving descriptor fields
 *
 */

SQLRETURN SQL_API SQLGetDescFieldW(
	SQLHDESC        DescriptorHandle,
	SQLSMALLINT     RecNumber,
	SQLSMALLINT     FieldIdentifier,
	_Out_writes_opt_(_Inexpressible_(BufferLength))
	SQLPOINTER      ValuePtr,
	SQLINTEGER      BufferLength,
	SQLINTEGER      *StringLengthPtr)
{
	SQLRETURN ret;
	TRACE6(_IN, DescriptorHandle, "phhplp", DescriptorHandle, RecNumber,
		FieldIdentifier, ValuePtr, BufferLength, StringLengthPtr);
	/* Note_stmt_sync: API descriptor access is serialized by statement's
	 * mutex, not descriptor's, since statement functions working on
	 * descriptors won't lock these, but will lock the statement instead
	 * (which keeps the code simpler; besides, no "high-speed" concurrent
	 * thread access on descriptors is necessary anyway). */
	HND_LOCK(DSCH(DescriptorHandle)->hdr.stmt);
	ret = EsSQLGetDescFieldW(DescriptorHandle, RecNumber,
			FieldIdentifier, ValuePtr, BufferLength, StringLengthPtr);
	HND_UNLOCK(DSCH(DescriptorHandle)->hdr.stmt);
	TRACE7(_OUT, DescriptorHandle, "dphhplg", ret, DescriptorHandle, RecNumber,
		FieldIdentifier, ValuePtr, BufferLength, StringLengthPtr);
	return ret;
}

SQLRETURN  SQL_API SQLSetDescFieldW
(
	SQLHDESC        DescriptorHandle,
	SQLSMALLINT     RecNumber,
	SQLSMALLINT     FieldIdentifier,
	SQLPOINTER      Value,
	SQLINTEGER      BufferLength
)
{
	SQLRETURN ret;
	TRACE5(_IN, DescriptorHandle, "phhpl", DescriptorHandle, RecNumber,
		FieldIdentifier, Value, BufferLength);
	HND_LOCK(DSCH(DescriptorHandle)->hdr.stmt); /* see Note_stmt_sync */
	ret = EsSQLSetDescFieldW(DescriptorHandle, RecNumber,
			FieldIdentifier, Value, BufferLength);
	HND_UNLOCK(DSCH(DescriptorHandle)->hdr.stmt);
	TRACE6(_OUT, DescriptorHandle, "dphhpl", ret, DescriptorHandle, RecNumber,
		FieldIdentifier, Value, BufferLength);
	return ret;
}

SQLRETURN SQL_API SQLGetDescRecW(
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
	RET_NOT_IMPLEMENTED(DescriptorHandle);
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
 * "The SQL_DESC_DATA_PTR field of an IPD is not normally set; however, an
 * application can do so to force a consistency check of IPD fields. The value
 * that the SQL_DESC_DATA_PTR field of the IPD is set to is not actually
 * stored and cannot be retrieved by a call to SQLGetDescField or
 * SQLGetDescRec; the setting is made only to force the consistency check. A
 * consistency check cannot be performed on an IRD."
 *
 * "A call to SQLSetDescRec sets the interval leading precision to the default
 * but sets the interval seconds precision (in the SQL_DESC_PRECISION field)
 * to the value of its Precision argument"
 */
SQLRETURN  SQL_API SQLSetDescRec(
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

	/* needs to trigger consistency_check */

	RET_NOT_IMPLEMENTED(DescriptorHandle);
}

/*
 * "SQLCopyDesc function is called to copy the fields of one descriptor to
 * another descriptor. Fields can be copied only to an application descriptor
 * or an IPD, but not to an IRD. Fields can be copied from any type of
 * descriptor. Only those fields that are defined for both the source and
 * target descriptors are copied." (with the exception of SQL_DESC_ALLOC_TYPE
 * field, which can't be changed)
 *
 * "An ARD on one statement handle can serve as the APD on another statement
 * handle." (= copying data between tables w/o extra copy in App; only if
 * SQL_MAX_CONCURRENT_ACTIVITIES > 1)
 */
SQLRETURN  SQL_API SQLCopyDesc(SQLHDESC SourceDescHandle,
	SQLHDESC TargetDescHandle)
{
	RET_NOT_IMPLEMENTED(SourceDescHandle);
}

/*
 * "The prepared statement associated with the statement handle can be
 * re-executed by calling SQLExecute until the application frees the statement
 * with a call to SQLFreeStmt with the SQL_DROP option or until the statement
 * handle is used in a call to SQLPrepare, SQLExecDirect, or one of the
 * catalog functions (SQLColumns, SQLTables, and so on)."
 */
SQLRETURN SQL_API SQLPrepareW
(
	SQLHSTMT    hstmt,
	_In_reads_(cchSqlStr)
	SQLWCHAR   *szSqlStr,
	SQLINTEGER  cchSqlStr
)
{
	SQLRETURN ret;
	TRACE3(_IN, hstmt, "pWl", hstmt, szSqlStr, cchSqlStr);
	HND_LOCK(hstmt);
	ret = EsSQLPrepareW(hstmt, szSqlStr, cchSqlStr);
	HND_UNLOCK(hstmt);
	TRACE4(_OUT, hstmt, "dpWl", ret, hstmt, szSqlStr, cchSqlStr);
	return ret;
}

/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/sending-long-data
 */
SQLRETURN SQL_API SQLBindParameter(
	SQLHSTMT           hstmt,
	SQLUSMALLINT       ipar,
	SQLSMALLINT        fParamType,
	SQLSMALLINT        fCType,
	SQLSMALLINT        fSqlType,
	SQLULEN            cbColDef,
	SQLSMALLINT        ibScale,
	SQLPOINTER         rgbValue,
	SQLLEN             cbValueMax,
	SQLLEN             *pcbValue)
{
	SQLRETURN ret;
	TRACE10(_IN, hstmt, "pHhhhZhpzp", hstmt, ipar, fParamType, fCType,
		fSqlType, cbColDef, ibScale, rgbValue, cbValueMax, pcbValue);
	HND_LOCK(hstmt);
	ret = EsSQLBindParameter(hstmt, ipar, fParamType, fCType,
			fSqlType, cbColDef, ibScale, rgbValue, cbValueMax, pcbValue);
	HND_UNLOCK(hstmt);
	TRACE11(_OUT, hstmt, "dpHhhhZhpzn", ret, hstmt, ipar, fParamType, fCType,
		fSqlType, cbColDef, ibScale, rgbValue, cbValueMax, pcbValue);
	return ret;
}

SQLRETURN SQL_API SQLGetCursorNameW
(
	SQLHSTMT        hstmt,
	_Out_writes_opt_(cchCursorMax) SQLWCHAR *szCursor,
	SQLSMALLINT     cchCursorMax,
	_Out_opt_
	SQLSMALLINT    *pcchCursor
)
{
	RET_NOT_IMPLEMENTED(hstmt);
}

SQLRETURN SQL_API SQLSetCursorNameW
(
	SQLHSTMT            hstmt,
	_In_reads_(cchCursor) SQLWCHAR *szCursor,
	SQLSMALLINT         cchCursor
)
{
	RET_NOT_IMPLEMENTED(hstmt);
}

SQLRETURN SQL_API SQLSetScrollOptions(    /*      Use SQLSetStmtOptions */
	SQLHSTMT           hstmt,
	SQLUSMALLINT       fConcurrency,
	SQLLEN             crowKeyset,
	SQLUSMALLINT       crowRowset)
{
	RET_NOT_IMPLEMENTED(hstmt);
}

/*
 *
 * Submitting requests
 *
 */

/*
 * "In the IPD, this header field points to a parameter status array
 * containing status information for each set of parameter values after a call
 * to SQLExecute or SQLExecDirect." = .array_status_ptr
 *
 * "In the APD, this header field points to a parameter operation array of
 * values that can be set by the application to indicate whether this set of
 * parameters is to be ignored when SQLExecute or SQLExecDirect is called."
 * = .array_status_ptr
 * "If no elements of the array are set, all sets of parameters in the array
 * are used in the SQLExecute or SQLExecDirect calls."
 */
SQLRETURN  SQL_API SQLExecute(SQLHSTMT hstmt)
{
	SQLRETURN ret;
	TRACE1(_IN, hstmt, "p", hstmt);
	HND_LOCK(hstmt);
	ret = EsSQLExecute(hstmt);
	HND_UNLOCK(hstmt);
	TRACE2(_OUT, hstmt, "dp", ret, hstmt);
	return ret;
}

/*
 * "In the IPD, this header field points to a parameter status array
 * containing status information for each set of parameter values after a call
 * to SQLExecute or SQLExecDirect." = .array_status_ptr
 *
 * "In the APD, this header field points to a parameter operation array of
 * values that can be set by the application to indicate whether this set of
 * parameters is to be ignored when SQLExecute or SQLExecDirect is called."
 * = .array_status_ptr
 * "If no elements of the array are set, all sets of parameters in the array
 * are used in the SQLExecute or SQLExecDirect calls."
 */
SQLRETURN SQL_API SQLExecDirectW
(
	SQLHSTMT    hstmt,
	_In_reads_opt_(TextLength)
	SQLWCHAR   *szSqlStr,
	SQLINTEGER  cchSqlStr
)
{
	SQLRETURN ret;
	TRACE3(_IN, hstmt, "pWl", hstmt, szSqlStr, cchSqlStr);
	HND_LOCK(hstmt);
	ret = EsSQLExecDirectW(hstmt, szSqlStr, cchSqlStr);
	HND_UNLOCK(hstmt);
	TRACE4(_OUT, hstmt, "dpWl", ret, hstmt, szSqlStr, cchSqlStr);
	return ret;
}

SQLRETURN SQL_API SQLNativeSqlW
(
	SQLHDBC                                     hdbc,
	_In_reads_(cchSqlStrIn) SQLWCHAR           *szSqlStrIn,
	SQLINTEGER                                  cchSqlStrIn,
	_Out_writes_opt_(cchSqlStrMax) SQLWCHAR    *szSqlStr,
	SQLINTEGER                                  cchSqlStrMax,
	SQLINTEGER                                 *pcchSqlStr
)
{
	SQLRETURN ret;
	TRACE6(_IN, hdbc, "pplplp", hdbc, szSqlStrIn, cchSqlStrIn, szSqlStr,
		cchSqlStrMax, pcchSqlStr);
	HND_LOCK(hdbc);
	ret = EsSQLNativeSqlW(hdbc, szSqlStrIn, cchSqlStrIn, szSqlStr,
			cchSqlStrMax, pcchSqlStr);
	HND_UNLOCK(hdbc);
	TRACE7(_OUT, hdbc, "dpplplg", ret, hdbc, szSqlStrIn, cchSqlStrIn, szSqlStr,
		cchSqlStrMax, pcchSqlStr);
	return ret;
}

/*
 * "drivers are capable of setting the fields of the IPD after a parameterized
 * query has been prepared. The descriptor fields are automatically populated
 * with information about the parameter, including the data type, precision,
 * scale, and other characteristics. This is equivalent to supporting
 * SQLDescribeParam."
 * Note: see EsSQLDescribeColW() for size & dec digits impl.
 */
SQLRETURN SQL_API SQLDescribeParam(
	SQLHSTMT           hstmt,
	SQLUSMALLINT       ipar,
	_Out_opt_
	SQLSMALLINT       *pfSqlType,
	_Out_opt_
	SQLULEN           *pcbParamDef,
	_Out_opt_
	SQLSMALLINT       *pibScale,
	_Out_opt_
	SQLSMALLINT       *pfNullable)
{
	RET_NOT_IMPLEMENTED(hstmt);
}

SQLRETURN SQL_API SQLNumParams(
	SQLHSTMT           hstmt,
	_Out_opt_
	SQLSMALLINT       *pcpar)
{
	SQLRETURN ret;
	TRACE2(_IN, hstmt, "pp", hstmt, pcpar);
	HND_LOCK(hstmt);
	ret = EsSQLNumParams(hstmt, pcpar);
	HND_UNLOCK(hstmt);
	TRACE3(_OUT, hstmt, "dpt", ret, hstmt, pcpar);
	return ret;
}

SQLRETURN  SQL_API SQLParamData(SQLHSTMT StatementHandle,
	_Out_opt_ SQLPOINTER *Value)
{
	RET_NOT_IMPLEMENTED(StatementHandle);
}

SQLRETURN  SQL_API SQLPutData(SQLHSTMT StatementHandle,
	_In_reads_(_Inexpressible_(StrLen_or_Ind)) SQLPOINTER Data,
	SQLLEN StrLen_or_Ind)
{
	RET_NOT_IMPLEMENTED(StatementHandle);
}

/*
 *
 * Retrieving results and information about results
 *
 */

SQLRETURN  SQL_API SQLRowCount(_In_ SQLHSTMT StatementHandle,
	_Out_ SQLLEN *RowCount)
{
	SQLRETURN ret;
	TRACE2(_IN, StatementHandle, "pp", StatementHandle, RowCount);
	HND_LOCK(StatementHandle); /* see Note_sync */
	ret = EsSQLRowCount(StatementHandle, RowCount);
	HND_UNLOCK(StatementHandle);
	TRACE3(_OUT, StatementHandle, "dpn", ret, StatementHandle, RowCount);
	return ret;
}

SQLRETURN  SQL_API SQLNumResultCols(SQLHSTMT StatementHandle,
	_Out_ SQLSMALLINT *ColumnCount)
{
	SQLRETURN ret;
	TRACE2(_IN, StatementHandle, "pp", StatementHandle, ColumnCount);
	HND_LOCK(StatementHandle); /* see Note_sync */
	ret = EsSQLNumResultCols(StatementHandle, ColumnCount);
	HND_UNLOCK(StatementHandle);
	TRACE3(_OUT, StatementHandle, "dpt", ret, StatementHandle, ColumnCount);
	return ret;
}


SQLRETURN SQL_API SQLDescribeColW
(
	SQLHSTMT            hstmt,
	SQLUSMALLINT        icol,
	_Out_writes_opt_(cchColNameMax)
	SQLWCHAR           *szColName,
	SQLSMALLINT         cchColNameMax,
	_Out_opt_
	SQLSMALLINT        *pcchColName,
	_Out_opt_
	SQLSMALLINT        *pfSqlType,
	_Out_opt_
	SQLULEN            *pcbColDef,
	_Out_opt_
	SQLSMALLINT        *pibScale,
	_Out_opt_
	SQLSMALLINT        *pfNullable
)
{
	SQLRETURN ret;
	TRACE9(_IN, hstmt, "pHphppppp", hstmt, icol, szColName,
		cchColNameMax, pcchColName, pfSqlType, pcbColDef, pibScale,
		pfNullable);
	HND_LOCK(hstmt);
	ret = EsSQLDescribeColW(hstmt, icol, szColName, cchColNameMax,
			pcchColName, pfSqlType, pcbColDef, pibScale, pfNullable);
	HND_UNLOCK(hstmt);
	TRACE10(_OUT, hstmt, "dpHphttNtt", ret, hstmt, icol, szColName,
		cchColNameMax, pcchColName, pfSqlType, pcbColDef, pibScale,
		pfNullable);
	return ret;
}

SQLRETURN SQL_API SQLColAttributeW
(
	SQLHSTMT        hstmt,
	SQLUSMALLINT    iCol,
	SQLUSMALLINT    iField,
	_Out_writes_bytes_opt_(cbDescMax)
	SQLPOINTER      pCharAttr,
	SQLSMALLINT     cbDescMax,
	_Out_opt_
	SQLSMALLINT     *pcbCharAttr,
	_Out_opt_
#ifdef _WIN64
	SQLLEN          *pNumAttr
#else /* _WIN64 */
	SQLPOINTER      pNumAttr
#endif /* _WIN64 */
)
{
	SQLRETURN ret;
	TRACE7(_IN, hstmt, "pHHphpp", hstmt, iCol, iField, pCharAttr,
		cbDescMax, pcbCharAttr, pNumAttr);
	HND_LOCK(hstmt);
	ret = EsSQLColAttributeW(hstmt, iCol, iField, pCharAttr,
			cbDescMax, pcbCharAttr, pNumAttr);
	HND_UNLOCK(hstmt);
#ifdef _WIN64
	TRACE8(_OUT, hstmt, "dpHHphtn", ret, hstmt, iCol, iField, pCharAttr,
		cbDescMax, pcbCharAttr, pNumAttr);
#else /* _WIN64 */
	TRACE8(_OUT, hstmt, "dpddpdtg", ret, hstmt, iCol, iField, pCharAttr,
		cbDescMax, pcbCharAttr, pNumAttr);
#endif /* _WIN64 */
	return ret;
}


SQLRETURN  SQL_API SQLBindCol(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT ColumnNumber,
	SQLSMALLINT TargetType,
	_Inout_updates_opt_(_Inexpressible_(BufferLength))
	SQLPOINTER TargetValue,
	SQLLEN BufferLength,
	_Inout_opt_
	SQLLEN *StrLen_or_Ind)
{
	SQLRETURN ret;
	TRACE6(_IN, StatementHandle, "pHhpzp", StatementHandle,
		ColumnNumber, TargetType, TargetValue, BufferLength, StrLen_or_Ind);
	HND_LOCK(StatementHandle);
	ret = EsSQLBindCol(StatementHandle,
			ColumnNumber, TargetType, TargetValue, BufferLength, StrLen_or_Ind);
	HND_UNLOCK(StatementHandle);
	TRACE7(_OUT, StatementHandle, "dpHhpzn", ret, StatementHandle,
		ColumnNumber, TargetType, TargetValue, BufferLength, StrLen_or_Ind);
	return ret;
}


SQLRETURN  SQL_API SQLFetch(SQLHSTMT StatementHandle)
{
	SQLRETURN ret;
	TRACE1(_IN, StatementHandle, "p", StatementHandle);
	HND_LOCK(StatementHandle);
	ret = EsSQLFetch(StatementHandle);
	HND_UNLOCK(StatementHandle);
	TRACE2(_OUT, StatementHandle, "dp", ret, StatementHandle);
	return ret;
}

/*
 * "SQLFetch and SQLFetchScroll use the rowset size at the time of the call to
 * determine how many rows to fetch. However, SQLFetchScroll with a
 * FetchOrientation of SQL_FETCH_NEXT increments the cursor based on the
 * rowset of the previous fetch and then fetches a rowset based on the current
 * rowset size."
 *
 * "In the IRD, this header field points to a row status array containing
 * status values after a call to SQLBulkOperations, SQLFetch, SQLFetchScroll,
 * or SQLSetPos."  = row status array of IRD (.array_status_ptr)
 *
 * "In ARDs, this field specifies the binding orientation when SQLFetchScroll
 * or SQLFetch is called on the associated statement handle." (.bind_type)
 *
 * "In an IRD, this SQLULEN * header field points to a buffer containing the
 * number of rows fetched after a call to SQLFetch or SQLFetchScroll, or the
 * number of rows affected in a bulk operation performed by a call to
 * SQLBulkOperations or SQLSetPos, including error rows."
 * (.rows_processed_ptr)
 *
 * "The variable that the StrLen_or_Ind argument refers to is used for both
 * indicator and length information. If a fetch encounters a null value for
 * the column, it stores SQL_NULL_DATA in this variable; otherwise, it stores
 * the data length in this variable. Passing a null pointer as StrLen_or_Ind
 * keeps the fetch operation from returning the data length but makes the
 * fetch fail if it encounters a null value and has no way to return
 * SQL_NULL_DATA." (.indicator_ptr)
 */
SQLRETURN  SQL_API SQLFetchScroll(SQLHSTMT StatementHandle,
	SQLSMALLINT FetchOrientation, SQLLEN FetchOffset)
{
	SQLRETURN ret;
	TRACE3(_IN, StatementHandle, "phz", StatementHandle,
		FetchOrientation, FetchOffset);
	HND_LOCK(StatementHandle);
	ret = EsSQLFetchScroll(StatementHandle, FetchOrientation, FetchOffset);
	HND_UNLOCK(StatementHandle);
	TRACE4(_OUT, StatementHandle, "dphz", ret, StatementHandle,
		FetchOrientation, FetchOffset);
	return ret;
}

SQLRETURN SQL_API SQLGetData(
	SQLHSTMT StatementHandle,
	SQLUSMALLINT ColumnNumber,
	SQLSMALLINT TargetType,
	_Out_writes_opt_(_Inexpressible_(BufferLength))
	SQLPOINTER TargetValuePtr,
	SQLLEN BufferLength,
	_Out_opt_
	SQLLEN *StrLen_or_IndPtr)
{
	SQLRETURN ret;
	TRACE6(_IN, StatementHandle, "pHhplp", StatementHandle,
		ColumnNumber, TargetType, TargetValuePtr, BufferLength,
		StrLen_or_IndPtr);
	HND_LOCK(StatementHandle);
	ret = EsSQLGetData(StatementHandle,
			ColumnNumber, TargetType, TargetValuePtr, BufferLength,
			StrLen_or_IndPtr);
	HND_UNLOCK(StatementHandle);
	TRACE7(_OUT, StatementHandle, "dpHhpln", ret, StatementHandle,
		ColumnNumber, TargetType, TargetValuePtr, BufferLength,
		StrLen_or_IndPtr);
	return ret;
}

SQLRETURN SQL_API SQLSetPos(
	SQLHSTMT        StatementHandle,
	SQLSETPOSIROW   RowNumber, /* SQLULEN / SQLUSMALLINT */
	SQLUSMALLINT    Operation,
	SQLUSMALLINT    LockType)
{
	SQLRETURN ret;
	TRACE4(_IN, StatementHandle, "pZHH", StatementHandle, RowNumber,
		Operation, LockType);
	HND_LOCK(StatementHandle);
	ret = EsSQLSetPos(StatementHandle, RowNumber,
			Operation, LockType);
	HND_UNLOCK(StatementHandle);
	TRACE5(_OUT, StatementHandle, "dpZHH", ret,StatementHandle, RowNumber,
		Operation, LockType);
	return ret;
}

SQLRETURN   SQL_API SQLBulkOperations(
	SQLHSTMT            StatementHandle,
	SQLSMALLINT         Operation)
{
	SQLRETURN ret;
	TRACE2(_IN, StatementHandle, "ph", StatementHandle, Operation);
	HND_LOCK(StatementHandle);
	ret = EsSQLBulkOperations(StatementHandle, Operation);
	HND_UNLOCK(StatementHandle);
	TRACE3(_OUT, StatementHandle, "dph", ret, StatementHandle, Operation);
	return ret;
}

SQLRETURN SQL_API SQLMoreResults(SQLHSTMT StatementHandle)
{
	SQLRETURN ret;
	TRACE1(_IN, StatementHandle, "p", StatementHandle);
	/* no synchronization required */
	ret = EsSQLMoreResults(StatementHandle);
	TRACE2(_OUT, StatementHandle, "dp", ret, StatementHandle);
	return ret;
}

SQLRETURN  SQL_API SQLGetDiagFieldW(
	SQLSMALLINT HandleType,
	SQLHANDLE Handle,
	SQLSMALLINT RecNumber,
	SQLSMALLINT DiagIdentifier,
	_Out_writes_opt_(_Inexpressible_(BufferLength))
	SQLPOINTER DiagInfoPtr,
	SQLSMALLINT BufferLength,
	_Out_opt_
	SQLSMALLINT *StringLengthPtr)
{
	SQLRETURN ret;
	TRACE7(_IN, Handle, "hphhphp", HandleType, Handle, RecNumber,
		DiagIdentifier, DiagInfoPtr, BufferLength, StringLengthPtr);
	/* Note_diag: locking here only really makes sense for stmt and dbc, but
	 * uniformly locking env and desc too is harmless and simple */
	HND_LOCK(Handle);
	ret = EsSQLGetDiagFieldW(HandleType, Handle, RecNumber,
			DiagIdentifier, DiagInfoPtr, BufferLength, StringLengthPtr);
	HND_UNLOCK(Handle);
	TRACE8(_OUT, Handle, "dhphhpht", ret, HandleType, Handle, RecNumber,
		DiagIdentifier, DiagInfoPtr, BufferLength, StringLengthPtr);
	return ret;
}

SQLRETURN  SQL_API SQLGetDiagRecW
(
	SQLSMALLINT HandleType,
	SQLHANDLE Handle,
	SQLSMALLINT RecNumber,
	_Out_writes_opt_(6)
	SQLWCHAR *Sqlstate,
	SQLINTEGER *NativeError,
	_Out_writes_opt_(BufferLength)
	SQLWCHAR *MessageText,
	SQLSMALLINT BufferLength,
	_Out_opt_
	SQLSMALLINT *TextLength
)
{
	SQLRETURN ret;
	TRACE8(_IN, Handle, "hphppphp", HandleType, Handle, RecNumber,
		Sqlstate, NativeError, MessageText, BufferLength, TextLength);
	HND_LOCK(Handle); /* see Note_diag */
	ret = EsSQLGetDiagRecW(HandleType, Handle, RecNumber,
			Sqlstate, NativeError, MessageText, BufferLength, TextLength);
	HND_UNLOCK(Handle);
	TRACE9(_OUT, Handle, "dhphwgwht", ret, HandleType, Handle, RecNumber,
		Sqlstate, NativeError, MessageText, BufferLength, TextLength);
	return ret;
}

/*
 *
 * Obtaining information about the data source's system tables
 * (catalog functions)
 *
 */

SQLRETURN SQL_API SQLColumnPrivilegesW(
	SQLHSTMT           hstmt,
	_In_reads_opt_(cchCatalogName) SQLWCHAR    *szCatalogName,
	SQLSMALLINT        cchCatalogName,
	_In_reads_opt_(cchSchemaName) SQLWCHAR     *szSchemaName,
	SQLSMALLINT        cchSchemaName,
	_In_reads_opt_(cchTableName) SQLWCHAR      *szTableName,
	SQLSMALLINT        cchTableName,
	_In_reads_opt_(cchColumnName) SQLWCHAR     *szColumnName,
	SQLSMALLINT        cchColumnName
)
{
	RET_NOT_IMPLEMENTED(hstmt);
}

SQLRETURN SQL_API SQLColumnsW
(
	SQLHSTMT       hstmt,
	_In_reads_opt_(cchCatalogName)
	SQLWCHAR      *szCatalogName,
	SQLSMALLINT    cchCatalogName,
	_In_reads_opt_(cchSchemaName)
	SQLWCHAR      *szSchemaName,
	SQLSMALLINT    cchSchemaName,
	_In_reads_opt_(cchTableName)
	SQLWCHAR      *szTableName,
	SQLSMALLINT    cchTableName,
	_In_reads_opt_(cchColumnName)
	SQLWCHAR      *szColumnName,
	SQLSMALLINT    cchColumnName
)
{
	SQLRETURN ret;
	TRACE9(_IN, hstmt, "pWhWhWhWh", hstmt, szCatalogName,
		cchCatalogName, szSchemaName, cchSchemaName, szTableName,
		cchTableName, szColumnName, cchColumnName);
	HND_LOCK(hstmt);
	ret = EsSQLColumnsW(hstmt, szCatalogName,
			cchCatalogName, szSchemaName, cchSchemaName, szTableName,
			cchTableName, szColumnName, cchColumnName);
	HND_UNLOCK(hstmt);
	TRACE10(_OUT, hstmt, "dpWhWhWhWh", ret, hstmt, szCatalogName,
		cchCatalogName, szSchemaName, cchSchemaName, szTableName,
		cchTableName, szColumnName, cchColumnName);
	return ret;
}

SQLRETURN SQL_API SQLForeignKeysW
(
	SQLHSTMT           hstmt,
	_In_reads_opt_(cchPkCatalogName)
	SQLWCHAR          *szPkCatalogName,
	SQLSMALLINT        cchPkCatalogName,
	_In_reads_opt_(cchPkSchemaName)
	SQLWCHAR          *szPkSchemaName,
	SQLSMALLINT        cchPkSchemaName,
	_In_reads_opt_(cchPkTableName)
	SQLWCHAR          *szPkTableName,
	SQLSMALLINT        cchPkTableName,
	_In_reads_opt_(cchFkCatalogName)
	SQLWCHAR          *szFkCatalogName,
	SQLSMALLINT        cchFkCatalogName,
	_In_reads_opt_(cchFkSchemaName)
	SQLWCHAR          *szFkSchemaName,
	SQLSMALLINT        cchFkSchemaName,
	_In_reads_opt_(cchFkTableName)
	SQLWCHAR          *szFkTableName,
	SQLSMALLINT        cchFkTableName
)
{
	SQLRETURN ret;
	TRACE13(_IN, hstmt, "pWhWhWhWhWhWh", hstmt,
		szPkCatalogName, cchPkCatalogName,
		szPkSchemaName, cchPkSchemaName,
		szPkTableName, cchPkTableName,
		szFkCatalogName, cchFkCatalogName,
		szFkSchemaName, cchFkSchemaName,
		szFkTableName, cchFkTableName);
	HND_LOCK(hstmt);
	ret = EsSQLForeignKeysW(hstmt,
			szPkCatalogName, cchPkCatalogName,
			szPkSchemaName, cchPkSchemaName,
			szPkTableName, cchPkTableName,
			szFkCatalogName, cchFkCatalogName,
			szFkSchemaName, cchFkSchemaName,
			szFkTableName, cchFkTableName);
	HND_UNLOCK(hstmt);
	TRACE14(_OUT, hstmt, "dpWhWhWhWhWhWh", ret, hstmt,
		szPkCatalogName, cchPkCatalogName,
		szPkSchemaName, cchPkSchemaName,
		szPkTableName, cchPkTableName,
		szFkCatalogName, cchFkCatalogName,
		szFkSchemaName, cchFkSchemaName,
		szFkTableName, cchFkTableName);
	return ret;
}


SQLRETURN SQL_API SQLPrimaryKeysW
(
	SQLHSTMT           hstmt,
	_In_reads_opt_(cchCatalogName)
	SQLWCHAR          *szCatalogName,
	SQLSMALLINT        cchCatalogName,
	_In_reads_opt_(cchSchemaName)
	SQLWCHAR          *szSchemaName,
	SQLSMALLINT        cchSchemaName,
	_In_reads_opt_(cchTableName)
	SQLWCHAR          *szTableName,
	SQLSMALLINT        cchTableName
)
{
	SQLRETURN ret;
	TRACE7(_IN, hstmt, "pWhWhWh", hstmt,
		szCatalogName, cchCatalogName,
		szSchemaName, cchSchemaName,
		szTableName, cchTableName);
	HND_LOCK(hstmt);
	ret = EsSQLPrimaryKeysW(hstmt,
			szCatalogName, cchCatalogName,
			szSchemaName, cchSchemaName,
			szTableName, cchTableName);
	HND_UNLOCK(hstmt);
	TRACE8(_OUT, hstmt, "dpWhWhWh", ret, hstmt,
		szCatalogName, cchCatalogName,
		szSchemaName, cchSchemaName,
		szTableName, cchTableName);
	return ret;
}

SQLRETURN SQL_API SQLProcedureColumnsW
(
	SQLHSTMT           hstmt,
	_In_reads_opt_(cchCatalogName) SQLWCHAR    *szCatalogName,
	SQLSMALLINT        cchCatalogName,
	_In_reads_opt_(cchSchemaName) SQLWCHAR     *szSchemaName,
	SQLSMALLINT        cchSchemaName,
	_In_reads_opt_(cchProcName) SQLWCHAR       *szProcName,
	SQLSMALLINT        cchProcName,
	_In_reads_opt_(cchColumnName) SQLWCHAR     *szColumnName,
	SQLSMALLINT        cchColumnName
)
{
	RET_NOT_IMPLEMENTED(hstmt);
}

SQLRETURN SQL_API SQLProceduresW
(
	SQLHSTMT           hstmt,
	_In_reads_opt_(cchCatalogName) SQLWCHAR    *szCatalogName,
	SQLSMALLINT        cchCatalogName,
	_In_reads_opt_(cchSchemaName) SQLWCHAR     *szSchemaName,
	SQLSMALLINT        cchSchemaName,
	_In_reads_opt_(cchProcName) SQLWCHAR      *szProcName,
	SQLSMALLINT        cchProcName
)
{
	RET_NOT_IMPLEMENTED(hstmt);
}

SQLRETURN SQL_API SQLSpecialColumnsW
(
	SQLHSTMT           hstmt,
	SQLUSMALLINT       fColType,
	_In_reads_opt_(cchCatalogName)
	SQLWCHAR          *szCatalogName,
	SQLSMALLINT        cchCatalogName,
	_In_reads_opt_(cchSchemaName)
	SQLWCHAR          *szSchemaName,
	SQLSMALLINT        cchSchemaName,
	_In_reads_opt_(cchTableName)
	SQLWCHAR          *szTableName,
	SQLSMALLINT        cchTableName,
	SQLUSMALLINT       fScope,
	SQLUSMALLINT       fNullable
)
{
	SQLRETURN ret;
	TRACE10(_IN, hstmt, "pHWhWhWhHH", hstmt, fColType, szCatalogName,
		cchCatalogName, szSchemaName, cchSchemaName, szTableName,
		cchTableName, fScope, fNullable);
	HND_LOCK(hstmt);
	ret = EsSQLSpecialColumnsW(hstmt, fColType, szCatalogName,
			cchCatalogName, szSchemaName, cchSchemaName, szTableName,
			cchTableName, fScope, fNullable);
	HND_UNLOCK(hstmt);
	TRACE11(_OUT, hstmt, "dpHWhWhWhHH", ret, hstmt, fColType, szCatalogName,
		cchCatalogName, szSchemaName, cchSchemaName, szTableName,
		cchTableName, fScope, fNullable);
	return ret;
}

SQLRETURN SQL_API SQLStatisticsW
(
	SQLHSTMT           hstmt,
	_In_reads_opt_(cchCatalogName) SQLWCHAR    *szCatalogName,
	SQLSMALLINT        cchCatalogName,
	_In_reads_opt_(cchSchemaName) SQLWCHAR     *szSchemaName,
	SQLSMALLINT        cchSchemaName,
	_In_reads_opt_(cchTableName) SQLWCHAR      *szTableName,
	SQLSMALLINT        cchTableName,
	SQLUSMALLINT       fUnique,
	SQLUSMALLINT       fAccuracy
)
{
	SQLRETURN ret;
	TRACE9(_IN, hstmt, "pWhWhWhHH", hstmt,
		szCatalogName, cchCatalogName, szSchemaName, cchSchemaName,
		szTableName, cchTableName, fUnique, fAccuracy);
	HND_LOCK(hstmt);
	ret = EsSQLStatisticsW(hstmt,
			szCatalogName, cchCatalogName,
			szSchemaName, cchSchemaName, szTableName, cchTableName,
			fUnique, fAccuracy);
	HND_UNLOCK(hstmt);
	TRACE10(_OUT, hstmt, "dpWhWhWhHH", ret, hstmt,
		szCatalogName, cchCatalogName, szSchemaName, cchSchemaName,
		szTableName, cchTableName, fUnique, fAccuracy);
	return ret;
}

SQLRETURN SQL_API SQLTablePrivilegesW
(
	SQLHSTMT           hstmt,
	_In_reads_opt_(cchCatalogName) SQLWCHAR    *szCatalogName,
	SQLSMALLINT        cchCatalogName,
	_In_reads_opt_(cchSchemaName) SQLWCHAR     *szSchemaName,
	SQLSMALLINT        cchSchemaName,
	_In_reads_opt_(cchTableName) SQLWCHAR      *szTableName,
	SQLSMALLINT        cchTableName
)
{
	RET_NOT_IMPLEMENTED(hstmt);
}


SQLRETURN SQL_API SQLTablesW
(
	SQLHSTMT           hstmt,
	_In_reads_opt_(cchCatalogName)
	SQLWCHAR          *szCatalogName,
	SQLSMALLINT        cchCatalogName,
	_In_reads_opt_(cchSchemaName)
	SQLWCHAR          *szSchemaName,
	SQLSMALLINT        cchSchemaName,
	_In_reads_opt_(cchTableName)
	SQLWCHAR          *szTableName,
	SQLSMALLINT        cchTableName,
	_In_reads_opt_(cchTableType)
	SQLWCHAR          *szTableType,
	SQLSMALLINT        cchTableType
)
{
	SQLRETURN ret;
	TRACE9(_IN, hstmt, "pWhWhWhWh", hstmt,
		szCatalogName, cchCatalogName, szSchemaName, cchSchemaName,
		szTableName, cchTableName, szTableType, cchTableType);
	HND_LOCK(hstmt);
	ret = EsSQLTablesW(hstmt,
			szCatalogName, cchCatalogName, szSchemaName, cchSchemaName,
			szTableName, cchTableName, szTableType, cchTableType);
	HND_UNLOCK(hstmt);
	TRACE10(_OUT, hstmt, "dpWhWhWhWh", ret, hstmt,
		szCatalogName, cchCatalogName, szSchemaName, cchSchemaName,
		szTableName, cchTableName, szTableType, cchTableType);
	return ret;
}


/*
 *
 * Terminating a statement
 *
 */

SQLRETURN  SQL_API SQLFreeStmt(SQLHSTMT StatementHandle, SQLUSMALLINT Option)
{
	SQLRETURN ret;
	TRACE2(_IN, StatementHandle, "pH", StatementHandle, Option);
	HND_LOCK(StatementHandle);
	ret = EsSQLFreeStmt(StatementHandle, Option);
	HND_UNLOCK(StatementHandle);
	TRACE3(_OUT, StatementHandle, "dpH", ret, StatementHandle, Option);
	return ret;
}

SQLRETURN  SQL_API SQLCloseCursor(SQLHSTMT StatementHandle)
{
	SQLRETURN ret;
	TRACE1(_IN, StatementHandle, "p", StatementHandle);
	HND_LOCK(StatementHandle);
	ret = EsSQLCloseCursor(StatementHandle);
	HND_UNLOCK(StatementHandle);
	TRACE2(_OUT, StatementHandle, "dp", ret, StatementHandle);
	return ret;
}

SQLRETURN  SQL_API SQLCancel(SQLHSTMT StatementHandle)
{
	SQLRETURN ret;
	TRACE1(_IN, StatementHandle, "p", StatementHandle);
	HND_LOCK(StatementHandle);
	ret = EsSQLCancel(StatementHandle);
	HND_UNLOCK(StatementHandle);
	TRACE2(_OUT, StatementHandle, "dp", ret, StatementHandle);
	return ret;
}

SQLRETURN  SQL_API SQLCancelHandle(SQLSMALLINT HandleType,
	SQLHANDLE InputHandle)
{
	SQLRETURN ret;
	TRACE2(_IN, InputHandle, "hp", HandleType, InputHandle);
	HND_LOCK(InputHandle);
	ret = EsSQLCancelHandle(HandleType, InputHandle);
	HND_UNLOCK(InputHandle);
	TRACE3(_IN, InputHandle, "dhp", ret, HandleType, InputHandle);
	return ret;
}

SQLRETURN  SQL_API SQLEndTran(SQLSMALLINT HandleType, SQLHANDLE Handle,
	SQLSMALLINT CompletionType)
{
	SQLRETURN ret;
	TRACE3(_IN, Handle, "hph", HandleType, Handle, CompletionType);
	HND_LOCK(Handle);
	ret = EsSQLEndTran(HandleType, Handle, CompletionType);
	HND_UNLOCK(Handle);
	TRACE4(_IN, Handle, "dhph", ret, HandleType, Handle, CompletionType);
	return ret;
}


/*
 *
 * Terminating a connection
 *
 */
SQLRETURN  SQL_API SQLDisconnect(SQLHDBC ConnectionHandle)
{
	SQLRETURN ret;
	TRACE1(_IN, ConnectionHandle, "p", ConnectionHandle);
	HND_LOCK(ConnectionHandle);
	ret = EsSQLDisconnect(ConnectionHandle);
	HND_UNLOCK(ConnectionHandle);
	TRACE2(_OUT, ConnectionHandle, "dp", ret, ConnectionHandle);
	return ret;
}

SQLRETURN  SQL_API SQLFreeHandle(SQLSMALLINT HandleType, SQLHANDLE Handle)
{
	SQLRETURN ret;
	TRACE2(_IN, Handle, "hp", HandleType, Handle);
	if (! HND_TRYLOCK(Handle)) {
		BUGH(Handle, "handle still locked while freeing attempt.");
		return SQL_ERROR;
	}
	ret = EsSQLFreeHandle(HandleType, Handle);
	TRACE3(_OUT, NULL/*Handle's freed*/, "dhp", ret, HandleType, Handle);
	return ret;
}


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=78 : */
