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

//#include "elasticodbc_export.h"
//#define SQL_API	ELASTICODBC_EXPORT SQL_API

// compile in empty functions (less unref'd params when leaving them out)
#define WITH_EMPTY	10

#if WITH_EMPTY
BOOL WINAPI DllMain(
	HINSTANCE hinstDLL,  // handle to DLL module
	DWORD fdwReason,     // reason for calling function
	LPVOID lpReserved )  // reserved
{
	SQLRETURN ret = TRUE;

	TRACE3(_IN, "pdp", hinstDLL, fdwReason, lpReserved);

	// Perform actions based on the reason for calling.
	switch( fdwReason ) { 
		case DLL_PROCESS_ATTACH:
		 // Initialize once for each new process.
		 // Return FALSE to fail DLL load.
			break;

		case DLL_THREAD_ATTACH:
		 // Do thread-specific initialization.
			break;

		case DLL_THREAD_DETACH:
		 // Do thread-specific cleanup.
			break;

		case DLL_PROCESS_DETACH:
		 // Perform any necessary cleanup.
			break;
	}

	TRACE4(_OUT, "updp", ret, hinstDLL, fdwReason, lpReserved);
	return ret;
}
#endif /* WITH_EMPTY */

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
	TRACE6(_OUT, "dpupUdD", ret, ConnectionHandle, InfoType, 
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

#if WITH_EMPTY
SQLRETURN SQL_API   SQLGetTypeInfoW(
    SQLHSTMT            StatementHandle,
    SQLSMALLINT         DataType)
{
	RET_NOT_IMPLEMENTED;
}


/*
 *
 * Setting and retrieving driver attributes
 *
 */

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
 */
SQLRETURN  SQL_API SQLSetConnectAttrW(
		SQLHDBC ConnectionHandle,
		SQLINTEGER Attribute,
		_In_reads_bytes_opt_(StringLength) SQLPOINTER Value,
		SQLINTEGER StringLength)
{
	esodbc_state_et state = SQL_STATE_HY000;

	TRACE4(_IN, "pdpd", ConnectionHandle, Attribute, Value, StringLength);

	switch(Attribute) {
		case SQL_ATTR_ANSI_APP:
			/* this driver doesn't behave differently based on app being ANSI
			 * or Unicode. */
			DBG("no ANSI/Unicode specific behaviour.");
			state = SQL_STATE_IM001;
			break;
		default:
			ERR("unknown Attribute: %d.", Attribute);
			state = SQL_STATE_HY092;
	}

	TRACE4(_OUT, "dpdpd", state, ConnectionHandle, Attribute, Value,
			StringLength);
	return esodbc_errors[state].retcode;
}

SQLRETURN SQL_API SQLGetConnectAttrW
(
    SQLHDBC     hdbc,
    SQLINTEGER  fAttribute,
    _Out_writes_opt_(_Inexpressible_(cbValueMax))
    SQLPOINTER  rgbValue,
    SQLINTEGER  cbValueMax,
    _Out_opt_
    SQLINTEGER* pcbValue
)
{
	RET_NOT_IMPLEMENTED;
}

#endif /* WITH_EMPTY */

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

#if WITH_EMPTY

SQLRETURN SQL_API SQLSetStmtAttrW(
    SQLHSTMT           hstmt,
    SQLINTEGER         fAttribute,
    SQLPOINTER         rgbValue,
    SQLINTEGER         cbValueMax)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLGetStmtAttrW(
    SQLHSTMT           hstmt,
    SQLINTEGER         fAttribute,
    SQLPOINTER         rgbValue,
    SQLINTEGER         cbValueMax,
    SQLINTEGER     *pcbValue)
{
	RET_NOT_IMPLEMENTED;
}


/*
 *
 * Setting and retrieving descriptor fields
 *
 */

SQLRETURN SQL_API SQLGetDescFieldW
(
    SQLHDESC        hdesc,
    SQLSMALLINT     iRecord,
    SQLSMALLINT     iField,
    _Out_writes_opt_(_Inexpressible_(cbBufferLength))
    SQLPOINTER      rgbValue,
    SQLINTEGER      cbBufferLength,
    _Out_opt_
    SQLINTEGER      *StringLength
)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLGetDescRecW
(
    SQLHDESC        hdesc,
    SQLSMALLINT     iRecord,
    _Out_writes_opt_(cchNameMax) SQLWCHAR* szName,
    SQLSMALLINT     cchNameMax,
    _Out_opt_
    SQLSMALLINT     *pcchName,
    _Out_opt_
    SQLSMALLINT     *pfType,
    _Out_opt_
    SQLSMALLINT     *pfSubType,
    _Out_opt_
    SQLLEN          *pLength,
    _Out_opt_
    SQLSMALLINT     *pPrecision,
    _Out_opt_
    SQLSMALLINT     *pScale,
    _Out_opt_
    SQLSMALLINT     *pNullable
)
{
	RET_NOT_IMPLEMENTED;
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
	RET_NOT_IMPLEMENTED;
}

SQLRETURN  SQL_API SQLSetDescRec(SQLHDESC DescriptorHandle,
           SQLSMALLINT RecNumber, SQLSMALLINT Type,
           SQLSMALLINT SubType, SQLLEN Length,
           SQLSMALLINT Precision, SQLSMALLINT Scale,
           _Inout_updates_bytes_opt_(Length) SQLPOINTER Data, _Inout_opt_ SQLLEN *StringLength,
           _Inout_opt_ SQLLEN *Indicator)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN  SQL_API SQLCopyDesc(SQLHDESC SourceDescHandle,
           SQLHDESC TargetDescHandle)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLPrepareW
(
    SQLHSTMT    hstmt,
    _In_reads_(cchSqlStr) SQLWCHAR* szSqlStr,
    SQLINTEGER  cchSqlStr
)
{
	RET_NOT_IMPLEMENTED;
}

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

/*
 *
 * Submitting requests
 *
 */

SQLRETURN  SQL_API SQLExecute(SQLHSTMT StatementHandle)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLExecDirectW
(
    SQLHSTMT    hstmt,
    _In_reads_opt_(TextLength) SQLWCHAR* szSqlStr,
    SQLINTEGER  TextLength
)
{
	RET_NOT_IMPLEMENTED;
}

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

SQLRETURN  SQL_API SQLNumResultCols(SQLHSTMT StatementHandle,
           _Out_ SQLSMALLINT *ColumnCount)
{
	RET_NOT_IMPLEMENTED;
}

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

SQLRETURN  SQL_API SQLBindCol(SQLHSTMT StatementHandle,
           SQLUSMALLINT ColumnNumber, SQLSMALLINT TargetType,
           _Inout_updates_opt_(_Inexpressible_(BufferLength)) SQLPOINTER TargetValue, 
           SQLLEN BufferLength, _Inout_opt_ SQLLEN *StrLen_or_Ind)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN  SQL_API SQLFetch(SQLHSTMT StatementHandle)
{
	RET_NOT_IMPLEMENTED;
}

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

SQLRETURN SQL_API SQLSetPos(
    SQLHSTMT           hstmt,
    SQLSETPOSIROW      irow,
    SQLUSMALLINT       fOption,
    SQLUSMALLINT       fLock)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN   SQL_API SQLBulkOperations(
    SQLHSTMT            StatementHandle,
    SQLSMALLINT         Operation)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN SQL_API SQLMoreResults(
    SQLHSTMT           hstmt)
{
	RET_NOT_IMPLEMENTED;
}

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
	TRACE9(_OUT, "ddpdpppdp", ret, HandleType, Handle, RecNumber, Sqlstate, 
			NativeError, MessageText, BufferLength, TextLength);
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
	RET_NOT_IMPLEMENTED;
}

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
	RET_NOT_IMPLEMENTED;
}

/*
 *
 * Terminating a statement
 *
 */

SQLRETURN  SQL_API SQLFreeStmt(SQLHSTMT StatementHandle,
           SQLUSMALLINT Option)
{
	RET_NOT_IMPLEMENTED;
}

SQLRETURN  SQL_API SQLCloseCursor(SQLHSTMT StatementHandle)
{
	RET_NOT_IMPLEMENTED;
}

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
