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

#include "log.h"
#include "tracing.h"
#include "handles.h"
#include "info.h"
#include "connect.h"
#include "queries.h"
#include "catalogue.h"

//#include "elasticodbc_export.h"
//#define SQL_API	ELASTICODBC_EXPORT SQL_API

// compile in empty functions (less unref'd params when leaving them out)
#define WITH_EMPTY	1

BOOL WINAPI DllMain(
	HINSTANCE hinstDLL,  // handle to DLL module
	DWORD fdwReason,     // reason for calling function
	LPVOID lpReserved)  // reserved
{
	TRACE3(_IN, "pdp", hinstDLL, fdwReason, lpReserved);

	// Perform actions based on the reason for calling.
	switch (fdwReason) { 
		// Initialize once for each new process.
		// Return FALSE to fail DLL load.
		case DLL_PROCESS_ATTACH:
			INFO("process %d attach.", GetCurrentProcessId());
			if (! connect_init()) {
				ERR("failed to init transport.");
				return  FALSE;
			}
			break;

		// Do thread-specific initialization.
		case DLL_THREAD_ATTACH:
			DBG("thread attach.");
			break;

		// Do thread-specific cleanup.
		case DLL_THREAD_DETACH:
			DBG("thread dettach.");
			break;

		// Perform any necessary cleanup.
		case DLL_PROCESS_DETACH:
			INFO("process %d dettach.", GetCurrentProcessId());
			connect_cleanup();
			break;
	}

	TRACE4(_OUT, "updp", TRUE, hinstDLL, fdwReason, lpReserved);
	return TRUE;
}

/*
 * Connecting to a data source
 */

SQLRETURN SQL_API SQLAllocHandle(SQLSMALLINT HandleType,
	SQLHANDLE InputHandle, _Out_ SQLHANDLE *OutputHandle)
{
	SQLRETURN ret;
	TRACE3(_IN, "dpp", HandleType, InputHandle, OutputHandle);
	ret = EsSQLAllocHandle(HandleType, InputHandle, OutputHandle);
	TRACE4(_OUT, "ddpp", ret, HandleType, InputHandle, *OutputHandle);
	return ret;
}

#if WITH_EMPTY
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
    _In_reads_(cchDSN) SQLWCHAR* szDSN,
    SQLSMALLINT         cchDSN,
    _In_reads_(cchUID) SQLWCHAR* szUID,
    SQLSMALLINT         cchUID,
    _In_reads_(cchAuthStr) SQLWCHAR* szAuthStr,
    SQLSMALLINT         cchAuthStr
)
{
	RET_NOT_IMPLEMENTED;
}

#endif /* WITH_EMPTY */

SQLRETURN SQL_API SQLDriverConnectW
(
		SQLHDBC             hdbc,
		SQLHWND             hwnd,
		_In_reads_(cchConnStrIn) SQLWCHAR* szConnStrIn,
		SQLSMALLINT         cchConnStrIn,
		_Out_writes_opt_(cchConnStrOutMax) SQLWCHAR* szConnStrOut,
		SQLSMALLINT         cchConnStrOutMax,
		_Out_opt_ SQLSMALLINT*        pcchConnStrOut,
		SQLUSMALLINT        fDriverCompletion
)
{
	SQLRETURN ret;
	TRACE8(_IN, "ppWdpdpd", hdbc, hwnd, szConnStrIn, cchConnStrIn, 
			szConnStrOut, cchConnStrOutMax, pcchConnStrOut, fDriverCompletion);
	ret = EsSQLDriverConnectW(hdbc, hwnd, szConnStrIn, cchConnStrIn, 
			szConnStrOut, cchConnStrOutMax, pcchConnStrOut, fDriverCompletion);
	TRACE9(_OUT, "dppWdWdpd", ret, hdbc, hwnd, szConnStrIn, cchConnStrIn, 
			szConnStrOut, cchConnStrOutMax, pcchConnStrOut, fDriverCompletion);
	return ret;
}

#if WITH_EMPTY

SQLRETURN SQL_API SQLBrowseConnectW
(
    SQLHDBC             hdbc,
    _In_reads_(cchConnStrIn) SQLWCHAR* szConnStrIn,
    SQLSMALLINT         cchConnStrIn,
    _Out_writes_opt_(cchConnStrOutMax) SQLWCHAR* szConnStrOut,
    SQLSMALLINT         cchConnStrOutMax,
    _Out_opt_
    SQLSMALLINT*        pcchConnStrOut
)
{
	RET_NOT_IMPLEMENTED;
}



/*
 *
 * Obtaining information about a driver and data source
 *
 */

SQLRETURN SQL_API SQLDataSourcesW
(
    SQLHENV             henv,
    SQLUSMALLINT        fDirection,
    _Out_writes_opt_(cchDSNMax) SQLWCHAR* szDSN,
    SQLSMALLINT         cchDSNMax,
    _Out_opt_
    SQLSMALLINT*        pcchDSN,
    _Out_writes_opt_(cchDescriptionMax) SQLWCHAR* wszDescription,
    SQLSMALLINT         cchDescriptionMax,
    _Out_opt_
    SQLSMALLINT*        pcchDescription
)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLDriversW
(
    SQLHENV         henv,
    SQLUSMALLINT    fDirection,
    _Out_writes_opt_(cchDriverDescMax) SQLWCHAR* szDriverDesc,
    SQLSMALLINT     cchDriverDescMax,
    _Out_opt_
    SQLSMALLINT*    pcchDriverDesc,
    _Out_writes_opt_(cchDrvrAttrMax) SQLWCHAR*     szDriverAttributes,
    SQLSMALLINT     cchDrvrAttrMax,
    _Out_opt_
    SQLSMALLINT*    pcchDrvrAttr
)
{
	RET_NOT_IMPLEMENTED;
}

#endif /* WITH_EMPTY */

SQLRETURN  SQL_API SQLGetInfoW(SQLHDBC ConnectionHandle,
		SQLUSMALLINT InfoType, 
		_Out_writes_bytes_opt_(BufferLength) SQLPOINTER InfoValue,
		SQLSMALLINT BufferLength,
		_Out_opt_ SQLSMALLINT *StringLengthPtr)
{
	SQLRETURN ret;
	TRACE6(_IN, "pupUdp", ConnectionHandle, InfoType, InfoValue, InfoValue, 
			BufferLength, StringLengthPtr);
	ret = EsSQLGetInfoW(ConnectionHandle, InfoType, InfoValue, BufferLength, 
			StringLengthPtr);
	TRACE7(_OUT, "dpupUdD", ret, ConnectionHandle, InfoType, 
			InfoValue, InfoValue, BufferLength, StringLengthPtr);
	return ret;
}

SQLRETURN  SQL_API SQLGetFunctions(SQLHDBC ConnectionHandle,
		SQLUSMALLINT FunctionId, 
		_Out_writes_opt_(_Inexpressible_("Buffer length pfExists points to depends on fFunction value.")) SQLUSMALLINT *Supported)
{
	SQLRETURN ret;
	TRACE3(_IN, "pdU", ConnectionHandle, FunctionId, Supported);
	ret = EsSQLGetFunctions(ConnectionHandle, FunctionId, Supported);
	TRACE4(_IN, "dpdU", ret, ConnectionHandle, FunctionId, Supported);
	return ret;
}

SQLRETURN SQL_API   SQLGetTypeInfoW(
    SQLHSTMT            StatementHandle,
    SQLSMALLINT         DataType)
{
	SQLRETURN ret;
	TRACE2(_IN, "pd", StatementHandle, DataType);
	ret = EsSQLGetTypeInfoW(StatementHandle, DataType);
	TRACE3(_OUT, "dpd", ret, StatementHandle, DataType);
	return ret;
	//RET_NOT_IMPLEMENTED;
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
	TRACE4(_IN, "pdpd", ConnectionHandle, Attribute, Value, StringLength);
	ret = EsSQLSetConnectAttrW(ConnectionHandle, Attribute, Value,
			StringLength);
	TRACE5(_OUT, "dpdpd", ret, ConnectionHandle, Attribute, Value,
			StringLength);
	return ret;
}

SQLRETURN SQL_API SQLGetConnectAttrW(
		SQLHDBC        ConnectionHandle,
		SQLINTEGER     Attribute,
		_Out_writes_opt_(_Inexpressible_(cbValueMax)) SQLPOINTER ValuePtr,
		SQLINTEGER     BufferLength,
		_Out_opt_ SQLINTEGER* StringLengthPtr)
{
	SQLRETURN ret;
	TRACE5(_IN, "pdpdp", ConnectionHandle, Attribute, ValuePtr,
		BufferLength, StringLengthPtr);
	ret = EsSQLGetConnectAttrW(ConnectionHandle, Attribute, ValuePtr, 
			BufferLength, StringLengthPtr);
	TRACE6(_OUT, "dpdpdD", ret, ConnectionHandle, Attribute, ValuePtr,
		BufferLength, StringLengthPtr);
	return ret;
}


SQLRETURN  SQL_API SQLSetEnvAttr(SQLHENV EnvironmentHandle,
		SQLINTEGER Attribute, 
		_In_reads_bytes_opt_(StringLength) SQLPOINTER Value,
		SQLINTEGER StringLength)
{
	SQLRETURN ret;
	TRACE4(_IN, "pdpd", EnvironmentHandle, Attribute, Value, StringLength);
	ret = EsSQLSetEnvAttr(EnvironmentHandle, Attribute, Value, StringLength);
	TRACE5(_OUT, "dpdpd", ret, EnvironmentHandle, Attribute, Value, 
			StringLength);
	return ret;
}

SQLRETURN  SQL_API SQLGetEnvAttr(SQLHENV EnvironmentHandle,
		SQLINTEGER Attribute, 
		_Out_writes_(_Inexpressible_(BufferLength)) SQLPOINTER Value,
		SQLINTEGER BufferLength, _Out_opt_ SQLINTEGER *StringLength)
{
	SQLRETURN ret;
	TRACE5(_IN, "pdpdp", EnvironmentHandle, Attribute, Value, BufferLength, 
			StringLength);
	ret = EsSQLGetEnvAttr(EnvironmentHandle, Attribute, Value, BufferLength, 
			StringLength);
	TRACE6(_OUT, "dpdpdD", ret, EnvironmentHandle, Attribute, Value, 
			BufferLength, StringLength);
	return ret;
}

SQLRETURN SQL_API SQLSetStmtAttrW(
		SQLHSTMT           StatementHandle,
		SQLINTEGER         Attribute,
		SQLPOINTER         ValuePtr,
		SQLINTEGER         BufferLength)
{
	SQLRETURN ret;
	TRACE4(_IN, "pdpd", StatementHandle, Attribute, ValuePtr, BufferLength);
	ret = EsSQLSetStmtAttrW(StatementHandle, Attribute, ValuePtr,
			BufferLength);
	TRACE5(_OUT, "dpdpd", ret, StatementHandle, Attribute, ValuePtr, 
			BufferLength);
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
	TRACE5(_IN, "pdpdp", StatementHandle, Attribute, ValuePtr, BufferLength, 
			StringLengthPtr);
	ret = EsSQLGetStmtAttrW(StatementHandle, Attribute, ValuePtr, BufferLength,
			StringLengthPtr);
	TRACE6(_OUT, "dpdpdD", ret, StatementHandle, Attribute, ValuePtr, 
			BufferLength, StringLengthPtr);
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
	TRACE6(_IN, "pddpdp", DescriptorHandle, RecNumber, FieldIdentifier,
			ValuePtr, BufferLength, StringLengthPtr);
	ret = EsSQLGetDescFieldW(DescriptorHandle, RecNumber, FieldIdentifier,
			ValuePtr, BufferLength, StringLengthPtr);
	TRACE7(_OUT, "dpddpdD", ret, DescriptorHandle, RecNumber, FieldIdentifier,
			ValuePtr, BufferLength, StringLengthPtr);
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
	SQLRETURN ret;
	TRACE11(_IN, "pdpdppppppp", DescriptorHandle, RecNumber, Name, 
			BufferLength, StringLengthPtr, TypePtr, SubTypePtr, LengthPtr, 
			PrecisionPtr, ScalePtr, NullablePtr);
	ret = EsSQLGetDescRecW(DescriptorHandle, RecNumber, Name, 
			BufferLength, StringLengthPtr, TypePtr, SubTypePtr, LengthPtr, 
			PrecisionPtr, ScalePtr, NullablePtr);
	TRACE12(_OUT, "dpdWdDDDDDDD", ret, DescriptorHandle, RecNumber, Name, 
			BufferLength, StringLengthPtr, TypePtr, SubTypePtr, LengthPtr, 
			PrecisionPtr, ScalePtr, NullablePtr);
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
	TRACE5(_IN, "pddpd", DescriptorHandle, RecNumber, FieldIdentifier, 
			Value, BufferLength);
	ret = EsSQLSetDescFieldW(DescriptorHandle, RecNumber, FieldIdentifier, 
			Value, BufferLength);
	TRACE6(_OUT, "dpddpd", ret, DescriptorHandle, RecNumber, FieldIdentifier, 
			Value, BufferLength);
	return ret;
}

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
	SQLRETURN ret;
	TRACE10(_IN, "pddddddppp", DescriptorHandle, RecNumber, Type, SubType,
			Length, Precision, Scale, Data, StringLength, Indicator);
	ret = EsSQLSetDescRec(DescriptorHandle, RecNumber, Type, SubType,
			Length, Precision, Scale, Data, StringLength, Indicator);
	TRACE11(_OUT, "dpddddddpDD", ret, DescriptorHandle, RecNumber, Type, 
			SubType, Length, Precision, Scale, Data, StringLength, Indicator);
	return ret;
}

#if WITH_EMPTY
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
	RET_NOT_IMPLEMENTED;
}
#endif // WITH_EMPTY

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
    _In_reads_(cchSqlStr) SQLWCHAR* szSqlStr,
    SQLINTEGER  cchSqlStr
)
{
	SQLRETURN ret;
	TRACE3(_IN, "ppd", hstmt, szSqlStr, cchSqlStr);
	ret = EsSQLPrepareW(hstmt, szSqlStr, cchSqlStr);
	TRACE4(_OUT, "dpSd", ret, hstmt, szSqlStr, cchSqlStr);
	return ret;
}

#if WITH_EMPTY
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
	RET_NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLGetCursorNameW
(
    SQLHSTMT        hstmt,
    _Out_writes_opt_(cchCursorMax) SQLWCHAR* szCursor,
    SQLSMALLINT     cchCursorMax,
    _Out_opt_
    SQLSMALLINT*    pcchCursor
)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLSetCursorNameW
(
    SQLHSTMT            hstmt,
    _In_reads_(cchCursor) SQLWCHAR* szCursor,
    SQLSMALLINT         cchCursor
)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLSetScrollOptions(    /*      Use SQLSetStmtOptions */
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fConcurrency,
    SQLLEN             crowKeyset,
    SQLUSMALLINT       crowRowset)
{
	RET_NOT_IMPLEMENTED;
}
#endif // WITH_EMPTY

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
	// TODO: set .stmt_curs = 0; in resultset
	SQLRETURN ret;
	TRACE1(_IN, "p", hstmt);
	ret = EsSQLExecute(hstmt);
	TRACE2(_OUT, "dp", ret, hstmt);
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
    _In_reads_opt_(TextLength) SQLWCHAR* szSqlStr,
    SQLINTEGER cchSqlStr 
)
{
	// TODO: set .stmt_curs = 0; in resultset
	SQLRETURN ret;
	TRACE3(_IN, "ppd", hstmt, szSqlStr, cchSqlStr);
	ret = EsSQLExecDirectW(hstmt, szSqlStr, cchSqlStr);
	TRACE4(_OUT, "dpSd", ret, hstmt, szSqlStr, cchSqlStr);
	return ret;
}

#if WITH_EMPTY
SQLRETURN SQL_API SQLNativeSqlW
(
    SQLHDBC                                     hdbc,
    _In_reads_(cchSqlStrIn) SQLWCHAR*          szSqlStrIn,
    SQLINTEGER                                  cchSqlStrIn,
    _Out_writes_opt_(cchSqlStrMax) SQLWCHAR*    szSqlStr,
    SQLINTEGER                                  cchSqlStrMax,
    SQLINTEGER*                                 pcchSqlStr
)
{
	RET_NOT_IMPLEMENTED;
}

/*
 * "drivers are capable of setting the fields of the IPD after a parameterized
 * query has been prepared. The descriptor fields are automatically populated
 * with information about the parameter, including the data type, precision,
 * scale, and other characteristics. This is equivalent to supporting
 * SQLDescribeParam."
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
	RET_NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLNumParams(
    SQLHSTMT           hstmt,
    _Out_opt_
    SQLSMALLINT       *pcpar)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN  SQL_API SQLParamData(SQLHSTMT StatementHandle,
           _Out_opt_ SQLPOINTER *Value)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN  SQL_API SQLPutData(SQLHSTMT StatementHandle,
           _In_reads_(_Inexpressible_(StrLen_or_Ind)) SQLPOINTER Data, SQLLEN StrLen_or_Ind)
{
	RET_NOT_IMPLEMENTED;
}

/*
 *
 * Retrieving results and information about results
 *
 */

SQLRETURN  SQL_API SQLRowCount(_In_ SQLHSTMT StatementHandle,
                               _Out_ SQLLEN* RowCount)
{
	RET_NOT_IMPLEMENTED;
}
#endif /* WITH_EMPTY */

SQLRETURN  SQL_API SQLNumResultCols(SQLHSTMT StatementHandle,
           _Out_ SQLSMALLINT *ColumnCount)
{
	SQLRETURN ret;
	TRACE2(_IN, "pp", StatementHandle, ColumnCount);
	ret = EsSQLNumResultCols(StatementHandle, ColumnCount);
	TRACE3(_OUT, "dpD", ret, StatementHandle, ColumnCount);
	return ret;
}

#if WITH_EMPTY

SQLRETURN SQL_API SQLDescribeColW
(
    SQLHSTMT            hstmt,
    SQLUSMALLINT        icol,
    _Out_writes_opt_(cchColNameMax) SQLWCHAR* szColName,
    SQLSMALLINT         cchColNameMax,
    _Out_opt_
    SQLSMALLINT*        pcchColName,
    _Out_opt_
    SQLSMALLINT*        pfSqlType,
    _Out_opt_
    SQLULEN*            pcbColDef,
    _Out_opt_
    SQLSMALLINT*        pibScale,
    _Out_opt_
    SQLSMALLINT*        pfNullable
)
{
	RET_NOT_IMPLEMENTED;
}

#ifdef _WIN64
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
    SQLLEN          *pNumAttr
)
{
	RET_NOT_IMPLEMENTED;
}
#else /* _WIN64 */
SQLRETURN SQL_API SQLColAttributeW(
    SQLHSTMT        hstmt,
    SQLUSMALLINT    iCol,
    SQLUSMALLINT    iField,
    _Out_writes_bytes_opt_(cbDescMax)
    SQLPOINTER      pCharAttr,
    SQLSMALLINT     cbDescMax,
    _Out_opt_
    SQLSMALLINT     *pcbCharAttr,
    _Out_opt_
    SQLPOINTER      pNumAttr)
{
	RET_NOT_IMPLEMENTED;
}
#endif /* _WIN64 */

#endif /*WITH_EMPTY*/

SQLRETURN  SQL_API SQLBindCol(SQLHSTMT StatementHandle,
           SQLUSMALLINT ColumnNumber, SQLSMALLINT TargetType,
           _Inout_updates_opt_(_Inexpressible_(BufferLength)) SQLPOINTER TargetValue, 
           SQLLEN BufferLength, _Inout_opt_ SQLLEN *StrLen_or_Ind)
{
	SQLRETURN ret;
	TRACE6(_IN, "pddpdp", StatementHandle, ColumnNumber, TargetType,
			TargetValue, BufferLength, StrLen_or_Ind);
	ret = EsSQLBindCol(StatementHandle, ColumnNumber, TargetType,
			TargetValue, BufferLength, StrLen_or_Ind);
	TRACE7(_OUT, "dpddpdD", ret, StatementHandle, ColumnNumber, TargetType,
			TargetValue, BufferLength, StrLen_or_Ind);
	return ret;
}


SQLRETURN  SQL_API SQLFetch(SQLHSTMT StatementHandle)
{
	SQLRETURN ret;
	TRACE1(_IN, "p", StatementHandle);
	ret = EsSQLFetch(StatementHandle);
	TRACE2(_OUT, "dp", ret, StatementHandle);
	return ret;
}

#if WITH_EMPTY
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
	RET_NOT_IMPLEMENTED;
}

SQLRETURN  SQL_API SQLGetData(SQLHSTMT StatementHandle,
           SQLUSMALLINT ColumnNumber, SQLSMALLINT TargetType,
           _Out_writes_opt_(_Inexpressible_(BufferLength)) SQLPOINTER TargetValue, SQLLEN BufferLength,
           _Out_opt_ SQLLEN *StrLen_or_IndPtr)
{
	RET_NOT_IMPLEMENTED;
}
#endif /* WITH_EMPTY */

SQLRETURN SQL_API SQLSetPos(
		SQLHSTMT        StatementHandle,
		SQLSETPOSIROW   RowNumber,
		SQLUSMALLINT    Operation,
		SQLUSMALLINT    LockType)
{
	SQLRETURN ret;
	TRACE4(_IN, "puuu", StatementHandle, RowNumber, Operation, LockType);
	ret = EsSQLSetPos(StatementHandle, RowNumber, Operation, LockType);
	TRACE5(_OUT, "dpuuu", ret,StatementHandle, RowNumber, Operation, LockType);
	return ret;
}

SQLRETURN   SQL_API SQLBulkOperations(
		SQLHSTMT            StatementHandle,
		SQLSMALLINT         Operation)
{
	SQLRETURN ret;
	TRACE2(_IN, "pd", StatementHandle, Operation);
	ret = EsSQLBulkOperations(StatementHandle, Operation);
	TRACE3(_OUT, "dpd", ret, StatementHandle, Operation);
	return ret;
}

#if WITH_EMPTY
SQLRETURN SQL_API SQLMoreResults(
    SQLHSTMT           hstmt)
{
	RET_NOT_IMPLEMENTED;
}
#endif /* WITH_EMPTY */

/* TODO: see error.h: esodbc_errors definition note (2.x apps support) */
SQLRETURN  SQL_API SQLGetDiagFieldW(
		SQLSMALLINT HandleType, 
		SQLHANDLE Handle,
		SQLSMALLINT RecNumber,
		SQLSMALLINT DiagIdentifier,
		_Out_writes_opt_(_Inexpressible_(BufferLength)) SQLPOINTER DiagInfoPtr,
		SQLSMALLINT BufferLength,
		_Out_opt_ SQLSMALLINT *StringLengthPtr)
{
	SQLRETURN ret;
	TRACE7(_IN, "dpddpdp", HandleType, Handle, RecNumber, DiagIdentifier, 
			DiagInfoPtr, BufferLength, StringLengthPtr);
	ret = EsSQLGetDiagFieldW(HandleType, Handle, RecNumber, DiagIdentifier,
			DiagInfoPtr, BufferLength, StringLengthPtr);
	TRACE8(_OUT, "ddpddpdD", ret, HandleType, Handle, RecNumber, 
			DiagIdentifier, DiagInfoPtr, BufferLength, StringLengthPtr);
	return ret;
}

SQLRETURN  SQL_API SQLGetDiagRecW
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
	SQLRETURN ret;
	TRACE8(_IN, "dpdpppdp", HandleType, Handle, RecNumber, Sqlstate, 
			NativeError, MessageText, BufferLength, TextLength);
	ret = EsSQLGetDiagRecW(HandleType, Handle, RecNumber, Sqlstate, 
			NativeError, MessageText, BufferLength, TextLength);
	TRACE9(_OUT, "ddpdWDWdD", ret, HandleType, Handle, RecNumber, Sqlstate, 
			NativeError, MessageText, BufferLength, TextLength);
	return ret;
}

#if WITH_EMPTY
/*
 *
 * Obtaining information about the data source's system tables 
 * (catalog functions)
 *
 */

SQLRETURN SQL_API SQLColumnPrivilegesW(
    SQLHSTMT           hstmt,
    _In_reads_opt_(cchCatalogName) SQLWCHAR*    szCatalogName,
    SQLSMALLINT        cchCatalogName,
    _In_reads_opt_(cchSchemaName) SQLWCHAR*     szSchemaName,
    SQLSMALLINT        cchSchemaName,
    _In_reads_opt_(cchTableName) SQLWCHAR*      szTableName,
    SQLSMALLINT        cchTableName,
    _In_reads_opt_(cchColumnName) SQLWCHAR*     szColumnName,
    SQLSMALLINT        cchColumnName
)
{
	RET_NOT_IMPLEMENTED;
}
#endif /* WITH_EMPTY */

SQLRETURN SQL_API SQLColumnsW
(
    SQLHSTMT           hstmt,
    _In_reads_opt_(cchCatalogName) SQLWCHAR*    szCatalogName,
    SQLSMALLINT        cchCatalogName,
    _In_reads_opt_(cchSchemaName) SQLWCHAR*     szSchemaName,
    SQLSMALLINT        cchSchemaName,
    _In_reads_opt_(cchTableName) SQLWCHAR*      szTableName,
    SQLSMALLINT        cchTableName,
    _In_reads_opt_(cchColumnName) SQLWCHAR*     szColumnName,
    SQLSMALLINT        cchColumnName
)
{
	SQLRETURN ret;
	TRACE9(_IN, "ppdpdpdpd", hstmt, szCatalogName, cchCatalogName,
			szSchemaName, cchSchemaName, szTableName, cchTableName,
			szColumnName, cchColumnName);
	ret = EsSQLColumnsW(hstmt, szCatalogName, cchCatalogName, 
			szSchemaName, cchSchemaName, szTableName, cchTableName,
			szColumnName, cchColumnName);
	TRACE10(_OUT, "dpWdWdWdWd", ret, hstmt, szCatalogName, cchCatalogName,
			szSchemaName, cchSchemaName, szTableName, cchTableName,
			szColumnName, cchColumnName);
	return ret;
}

#if WITH_EMPTY
SQLRETURN SQL_API SQLForeignKeysW
(
    SQLHSTMT           hstmt,
    _In_reads_opt_(cchPkCatalogName) SQLWCHAR*    szPkCatalogName,
    SQLSMALLINT        cchPkCatalogName,
    _In_reads_opt_(cchPkSchemaName) SQLWCHAR*     szPkSchemaName,
    SQLSMALLINT        cchPkSchemaName,
    _In_reads_opt_(cchPkTableName) SQLWCHAR*      szPkTableName,
    SQLSMALLINT        cchPkTableName,
    _In_reads_opt_(cchFkCatalogName) SQLWCHAR*    szFkCatalogName,
    SQLSMALLINT        cchFkCatalogName,
    _In_reads_opt_(cchFkSchemaName) SQLWCHAR*     szFkSchemaName,
    SQLSMALLINT        cchFkSchemaName,
    _In_reads_opt_(cchFkTableName) SQLWCHAR*      szFkTableName,
    SQLSMALLINT        cchFkTableName
)
{
	RET_NOT_IMPLEMENTED;
}


SQLRETURN SQL_API SQLPrimaryKeysW
(
    SQLHSTMT           hstmt,
    _In_reads_opt_(cchCatalogName) SQLWCHAR*    szCatalogName,
    SQLSMALLINT        cchCatalogName,
    _In_reads_opt_(cchSchemaName) SQLWCHAR*     szSchemaName,
    SQLSMALLINT        cchSchemaName,
    _In_reads_opt_(cchTableName) SQLWCHAR*      szTableName,
    SQLSMALLINT        cchTableName
)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLProcedureColumnsW
(
    SQLHSTMT           hstmt,
    _In_reads_opt_(cchCatalogName) SQLWCHAR*    szCatalogName,
    SQLSMALLINT        cchCatalogName,
    _In_reads_opt_(cchSchemaName) SQLWCHAR*     szSchemaName,
    SQLSMALLINT        cchSchemaName,
    _In_reads_opt_(cchProcName) SQLWCHAR*       szProcName,
    SQLSMALLINT        cchProcName,
    _In_reads_opt_(cchColumnName) SQLWCHAR*     szColumnName,
    SQLSMALLINT        cchColumnName
)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLProceduresW
(
    SQLHSTMT           hstmt,
    _In_reads_opt_(cchCatalogName) SQLWCHAR*    szCatalogName,
    SQLSMALLINT        cchCatalogName,
    _In_reads_opt_(cchSchemaName) SQLWCHAR*     szSchemaName,
    SQLSMALLINT        cchSchemaName,
    _In_reads_opt_(cchProcName) SQLWCHAR*      szProcName,
    SQLSMALLINT        cchProcName
)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLSpecialColumnsW
(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fColType,
    _In_reads_opt_(cchCatalogName) SQLWCHAR*    szCatalogName,
    SQLSMALLINT        cchCatalogName,
    _In_reads_opt_(cchSchemaName) SQLWCHAR*     szSchemaName,
    SQLSMALLINT        cchSchemaName,
    _In_reads_opt_(cchTableName) SQLWCHAR*      szTableName,
    SQLSMALLINT        cchTableName,
    SQLUSMALLINT       fScope,
    SQLUSMALLINT       fNullable
)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLStatisticsW
(
    SQLHSTMT           hstmt,
    _In_reads_opt_(cchCatalogName) SQLWCHAR*    szCatalogName,
    SQLSMALLINT        cchCatalogName,
    _In_reads_opt_(cchSchemaName) SQLWCHAR*     szSchemaName,
    SQLSMALLINT        cchSchemaName,
    _In_reads_opt_(cchTableName) SQLWCHAR*      szTableName,
    SQLSMALLINT        cchTableName,
    SQLUSMALLINT       fUnique,
    SQLUSMALLINT       fAccuracy
)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLTablePrivilegesW
(
    SQLHSTMT           hstmt,
    _In_reads_opt_(cchCatalogName) SQLWCHAR*    szCatalogName,
    SQLSMALLINT        cchCatalogName,
    _In_reads_opt_(cchSchemaName) SQLWCHAR*     szSchemaName,
    SQLSMALLINT        cchSchemaName,
    _In_reads_opt_(cchTableName) SQLWCHAR*      szTableName,
    SQLSMALLINT        cchTableName
)
{
	RET_NOT_IMPLEMENTED;
}

#endif /* WITH_EMPTY */

SQLRETURN SQL_API SQLTablesW
(
    SQLHSTMT           hstmt,
    _In_reads_opt_(cchCatalogName) SQLWCHAR*    szCatalogName,
    SQLSMALLINT        cchCatalogName,
    _In_reads_opt_(cchSchemaName) SQLWCHAR*     szSchemaName,
    SQLSMALLINT        cchSchemaName,
    _In_reads_opt_(cchTableName) SQLWCHAR*      szTableName,
    SQLSMALLINT        cchTableName,
    _In_reads_opt_(cchTableType) SQLWCHAR*      szTableType,
    SQLSMALLINT        cchTableType
)
{
	SQLRETURN ret;
	TRACE9(_IN, "ppdpdpdpd", hstmt, szCatalogName, cchCatalogName,
			szSchemaName, cchSchemaName, szTableName, cchTableName,
			szTableType, cchTableType);
	ret = EsSQLTablesW(hstmt, szCatalogName, cchCatalogName,
			szSchemaName, cchSchemaName, szTableName, cchTableName,
			szTableType, cchTableType);
	TRACE10(_OUT, "dpWdWdWdWd", ret, hstmt, szCatalogName, cchCatalogName,
			szSchemaName, cchSchemaName, szTableName, cchTableName,
			szTableType, cchTableType);
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
	TRACE2(_IN, "pd", StatementHandle, Option);
	ret = EsSQLFreeStmt(StatementHandle, Option);
	TRACE3(_OUT, "dpd", ret, StatementHandle, Option);
	return ret;
}

SQLRETURN  SQL_API SQLCloseCursor(SQLHSTMT StatementHandle)
{
	SQLRETURN ret;
	TRACE1(_IN, "p", StatementHandle);
	ret = EsSQLCloseCursor(StatementHandle);
	TRACE2(_OUT, "dpd", ret, StatementHandle);
	return ret;
}

#if WITH_EMPTY

SQLRETURN  SQL_API SQLCancel(SQLHSTMT StatementHandle)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN  SQL_API SQLCancelHandle(SQLSMALLINT HandleType, SQLHANDLE InputHandle)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN  SQL_API SQLEndTran(SQLSMALLINT HandleType, SQLHANDLE Handle,
           SQLSMALLINT CompletionType)
{
	RET_NOT_IMPLEMENTED;
}

#endif /* WITH_EMPTY */


/*
 *
 * Terminating a connection
 *
 */
SQLRETURN  SQL_API SQLDisconnect(SQLHDBC ConnectionHandle)
{
	SQLRETURN ret;
	TRACE1(_IN, "p", ConnectionHandle);
	ret = EsSQLDisconnect(ConnectionHandle);
	TRACE2(_OUT, "dp", ret, ConnectionHandle);
	return ret;
}

SQLRETURN  SQL_API SQLFreeHandle(SQLSMALLINT HandleType, SQLHANDLE Handle)
{
	SQLRETURN ret;
	TRACE2(_IN, "dp", HandleType, Handle);
	ret = EsSQLFreeHandle(HandleType, Handle);
	TRACE3(_OUT, "ddp", ret, HandleType, Handle);
	return ret;
}


#if 0
// API

SQLAllocHandle
SQLConnect
SQLDriverConnect
SQLBrowseConnect

SQLDataSources
SQLDrivers
SQLGetInfo
SQLGetFunctions
SQLGetTypeInfo

SQLSetConnectAttr
SQLGetConnectAttr
SQLSetEnvAttr
SQLGetEnvAttr
SQLSetStmtAttr
SQLGetStmtAttr

SQLGetDescField
SQLGetDescRec
SQLSetDescField
SQLSetDescRec
SQLCopyDesc
SQLPrepare
SQLBindParameter
SQLGetCursorName
SQLSetCursorName
SQLSetScrollOptions

SQLExecute
SQLExecDirect
SQLNativeSql
SQLDescribeParam
SQLNumParams
SQLParamData
SQLPutData

SQLRowCount
SQLNumResultCols
SQLDescribeCol
SQLColAttribute
SQLBindCol
SQLFetch
SQLFetchScroll
SQLGetData
SQLSetPos
SQLBulkOperations
SQLMoreResults
SQLGetDiagField
SQLGetDiagRec

SQLColumnPrivileges
SQLColumns
SQLForeignKeys
SQLPrimaryKeys
SQLProcedureColumns
SQLProcedures
SQLSpecialColumns
SQLStatistics
SQLTablePrivileges
SQLTables

SQLFreeStmt
SQLCloseCursor
SQLCancel
SQLCancelHandle
SQLEndTran

SQLDisconnect
SQLFreeHandle
#endif

#if 0
//ANSI / Unicode

SQLBrowseConnect
SQLColAttribute
SQLColAttributes
SQLColumnPrivileges
SQLColumns
SQLConnect
SQLDataSources
SQLDescribeCol
SQLDriverConnect
SQLDrivers
SQLError
SQLExecDirect
SQLForeignKeys
SQLGetConnectAttr
SQLGetConnectOption
SQLGetCursorName
SQLGetDescField
SQLGetDescRec
SQLGetDiagField
SQLGetDiagRec
SQLGetInfo
SQLGetStmtAttr
SQLNativeSQL
SQLPrepare
SQLPrimaryKeys
SQLProcedureColumns
SQLProcedures
SQLSetConnectAttr
SQLSetConnectOption
SQLSetCursorName
SQLSetDescField
SQLSetStmtAttr
SQLSpecialColumns
SQLStatistics
SQLTablePrivileges
SQLTables
#endif

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
