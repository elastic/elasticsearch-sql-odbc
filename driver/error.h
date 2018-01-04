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

#ifndef __ERROR_H__
#define __ERROR_H__

/* NOTE: this must be included in "top level" file (wherever SQL types are
 * used  */
#if defined(_WIN32) || defined (WIN32)
/* FIXME: why isn't this included in sql/~ext.h???? */
/* win function parameter attributes */
#include <windows.h>
#endif /* _WIN32/WIN32 */

#include "sql.h"
#include "sqlext.h"

typedef struct {
	SQLSTATE code;
	SQLTCHAR *message;
	SQLRETURN retcode;
} esodbc_errors_st;

/* Note: keep in sync with esodbc_errors[] */
typedef enum {
	/* non-standard, must be 0/first position (calloc'ing) */
	/* diagnostic meaning: no diag posted */
	SQL_STATE_00000 = 0,

	SQL_STATE_01000,
	SQL_STATE_01001,
	SQL_STATE_01002,
	SQL_STATE_01003,
	SQL_STATE_01004,
	SQL_STATE_01006,
	SQL_STATE_01007,
	SQL_STATE_01S00,
	SQL_STATE_01S01,
	SQL_STATE_01S02,
	SQL_STATE_01S06,
	SQL_STATE_01S07,
	SQL_STATE_01S08,
	SQL_STATE_01S09,
	SQL_STATE_07001,
	SQL_STATE_07002,
	SQL_STATE_07005,
	SQL_STATE_07006,
	SQL_STATE_07009,
	SQL_STATE_07S01,
	SQL_STATE_08001,
	SQL_STATE_08002,
	SQL_STATE_08003,
	SQL_STATE_08004,
	SQL_STATE_08007,
	SQL_STATE_08S01,
	SQL_STATE_21S01,
	SQL_STATE_21S02,
	SQL_STATE_22001,
	SQL_STATE_22002,
	SQL_STATE_22003,
	SQL_STATE_22007,
	SQL_STATE_22008,
	SQL_STATE_22012,
	SQL_STATE_22015,
	SQL_STATE_22018,
	SQL_STATE_22019,
	SQL_STATE_22025,
	SQL_STATE_22026,
	SQL_STATE_23000,
	SQL_STATE_24000,
	SQL_STATE_25000,
	SQL_STATE_25S01,
	SQL_STATE_25S02,
	SQL_STATE_25S03,
	SQL_STATE_28000,
	SQL_STATE_34000,
	SQL_STATE_3C000,
	SQL_STATE_3D000,
	SQL_STATE_3F000,
	SQL_STATE_40001,
	SQL_STATE_40002,
	SQL_STATE_40003,
	SQL_STATE_42000,
	SQL_STATE_42S01,
	SQL_STATE_42S02,
	SQL_STATE_42S11,
	SQL_STATE_42S12,
	SQL_STATE_42S21,
	SQL_STATE_42S22,
	SQL_STATE_44000,
	SQL_STATE_HY000,
	SQL_STATE_HY001,
	SQL_STATE_HY003,
	SQL_STATE_HY004,
	SQL_STATE_HY007,
	SQL_STATE_HY008,
	SQL_STATE_HY009,
	SQL_STATE_HY010,
	SQL_STATE_HY011,
	SQL_STATE_HY012,
	SQL_STATE_HY013,
	SQL_STATE_HY014,
	SQL_STATE_HY015,
	SQL_STATE_HY016,
	SQL_STATE_HY017,
	SQL_STATE_HY018,
	SQL_STATE_HY019,
	SQL_STATE_HY020,
	SQL_STATE_HY021,
	SQL_STATE_HY024,
	SQL_STATE_HY090,
	SQL_STATE_HY091,
	SQL_STATE_HY092,
	SQL_STATE_HY095,
	SQL_STATE_HY096,
	SQL_STATE_HY097,
	SQL_STATE_HY098,
	SQL_STATE_HY099,
	SQL_STATE_HY100,
	SQL_STATE_HY101,
	SQL_STATE_HY103,
	SQL_STATE_HY104,
	SQL_STATE_HY105,
	SQL_STATE_HY106,
	SQL_STATE_HY107,
	SQL_STATE_HY109,
	SQL_STATE_HY110,
	SQL_STATE_HY111,
	SQL_STATE_HYC00,
	SQL_STATE_HYT00,
	SQL_STATE_HYT01,
	SQL_STATE_IM001,
	SQL_STATE_IM002,
	SQL_STATE_IM003,
	SQL_STATE_IM004,
	SQL_STATE_IM005,
	SQL_STATE_IM006,
	SQL_STATE_IM007,
	SQL_STATE_IM008,
	SQL_STATE_IM009,
	SQL_STATE_IM010,
	SQL_STATE_IM011,
	SQL_STATE_IM012,
	SQL_STATE_IM013,
	SQL_STATE_IM014,
	SQL_STATE_IM015,
} esodbc_state_et;


#define MK_WSTR(_cstr_)	(L ## _cstr_)

#ifdef UNICODE
#define MK_TSTR(_cstr_)	MK_WSTR(_cstr_)
#else /* UNICODE */
#define MK_TSTR(_cstr_)	(_cstr_)
#endif /* UNICODE */


/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/sqlstate-mappings :
 * """
 * In ODBC 3.x, HYxxx SQLSTATEs are returned instead of S1xxx, and 42Sxx
 * SQLSTATEs are returned instead of S00XX.
 * [...]
 * When the SQL_ATTR_ODBC_VERSION environment attribute is set to
 * SQL_OV_ODBC2, the driver posts ODBC 2.x SQLSTATEs instead of ODBC 3.x
 * SQLSTATEs when SQLGetDiagField or SQLGetDiagRec is called.
 * """
 *
 * https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/appendix-a-odbc-error-codes :
 * """
 * The character string value returned for an SQLSTATE consists of a
 * two-character class value followed by a three-character subclass value. A
 * class value of "01" indicates a warning and is accompanied by a return code
 * of SQL_SUCCESS_WITH_INFO. Class values other than "01," except for the
 * class "IM," indicate an error and are accompanied by a return value of
 * SQL_ERROR. The class "IM" is specific to warnings and errors that derive
 * from the implementation of ODBC itself. The subclass value "000" in any
 * class indicates that there is no subclass for that SQLSTATE.
 * """
 */
/* Note: keep in sync with esodbc_state_et */
static esodbc_errors_st esodbc_errors[] = {
	{MK_TSTR("00000"), MK_TSTR("Success"), 
		SQL_SUCCESS}, /* non standard */
	{MK_TSTR("01000"), MK_TSTR("General warning"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_TSTR("01001"), MK_TSTR("Cursor operation conflict"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_TSTR("01002"), MK_TSTR("Disconnect error"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_TSTR("01003"), MK_TSTR("NULL value eliminated in set function"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_TSTR("01004"), MK_TSTR("String data, right-truncated"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_TSTR("01006"), MK_TSTR("Privilege not revoked"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_TSTR("01007"), MK_TSTR("Privilege not granted"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_TSTR("01S00"), MK_TSTR("Invalid connection string attribute"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_TSTR("01S01"), MK_TSTR("Error in row"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_TSTR("01S02"), MK_TSTR("Option value changed"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_TSTR("01S06"), MK_TSTR("Attempt to fetch before the result set "\
			"returned the first rowset"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_TSTR("01S07"), MK_TSTR("Fractional truncation"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_TSTR("01S08"), MK_TSTR("Error saving File DSN"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_TSTR("01S09"), MK_TSTR("Invalid keyword"), 
		SQL_SUCCESS_WITH_INFO},

	{MK_TSTR("07001"), MK_TSTR("Wrong number of parameters"), 
		SQL_ERROR},
	{MK_TSTR("07002"), MK_TSTR("COUNT field incorrect"), 
		SQL_ERROR},
	{MK_TSTR("07005"), 
		MK_TSTR("Prepared statement not a cursor-specification"),
		SQL_ERROR},
	{MK_TSTR("07006"), MK_TSTR("Restricted data type attribute violation"), 
		SQL_ERROR},
	{MK_TSTR("07009"), MK_TSTR("Invalid descriptor index"), 
		SQL_ERROR},
	{MK_TSTR("07S01"), MK_TSTR("Invalid use of default parameter"), 
		SQL_ERROR},

	{MK_TSTR("08001"), MK_TSTR("Client unable to establish connection"), 
		SQL_ERROR},
	{MK_TSTR("08002"), MK_TSTR("Connection name in use"), 
		SQL_ERROR},
	{MK_TSTR("08003"), MK_TSTR("Connection not open"), 
		SQL_ERROR},
	{MK_TSTR("08004"), MK_TSTR("Server rejected the connection"), 
		SQL_ERROR},
	{MK_TSTR("08007"), MK_TSTR("Connection failure during transaction"), 
		SQL_ERROR},
	{MK_TSTR("08S01"), MK_TSTR("Communication link failure"), 
		SQL_ERROR},

	{MK_TSTR("21S01"), MK_TSTR("Insert value list does not match column list"), 
		SQL_ERROR},
	{MK_TSTR("21S02"), 
		MK_TSTR("Degree of derived table does not match column list"), 
		SQL_ERROR},
	{MK_TSTR("22001"), MK_TSTR("String data, right-truncated"), 
		SQL_ERROR},
	{MK_TSTR("22002"), MK_TSTR("Indicator variable required but not supplied"),
		SQL_ERROR},
	{MK_TSTR("22003"), MK_TSTR("Numeric value out of range"), 
		SQL_ERROR},
	{MK_TSTR("22007"), MK_TSTR("Invalid datetime format"), 
		SQL_ERROR},
	{MK_TSTR("22008"), MK_TSTR("Datetime field overflow"), 
		SQL_ERROR},
	{MK_TSTR("22012"), MK_TSTR("Division by zero"), 
		SQL_ERROR},
	{MK_TSTR("22015"), MK_TSTR("Interval field overflow"), 
		SQL_ERROR},
	{MK_TSTR("22018"), 
		MK_TSTR("Invalid character value for cast specification"), 
		SQL_ERROR},
	{MK_TSTR("22019"), MK_TSTR("Invalid escape character"), 
		SQL_ERROR},
	{MK_TSTR("22025"), MK_TSTR("Invalid escape sequence"), 
		SQL_ERROR},
	{MK_TSTR("22026"), MK_TSTR("String data, length mismatch"), 
		SQL_ERROR},
	{MK_TSTR("23000"), MK_TSTR("Integrity constraint violation"), 
		SQL_ERROR},
	{MK_TSTR("24000"), MK_TSTR("Invalid cursor state"), 
		SQL_ERROR},
	{MK_TSTR("25000"), MK_TSTR("Invalid transaction state"), 
		SQL_ERROR},
	{MK_TSTR("25S01"), MK_TSTR("Transaction state"), 
		SQL_ERROR},
	{MK_TSTR("25S02"), MK_TSTR("Transaction is still active"), 
		SQL_ERROR},
	{MK_TSTR("25S03"), MK_TSTR("Transaction is rolled back"), 
		SQL_ERROR},
	{MK_TSTR("28000"), MK_TSTR("Invalid authorization specification"), 
		SQL_ERROR},
	{MK_TSTR("34000"), MK_TSTR("Invalid cursor name"), 
		SQL_ERROR},
	{MK_TSTR("3C000"), MK_TSTR("Duplicate cursor name"), 
		SQL_ERROR},
	{MK_TSTR("3D000"), MK_TSTR("Invalid catalog name"), 
		SQL_ERROR},
	{MK_TSTR("3F000"), MK_TSTR("Invalid schema name"), 
		SQL_ERROR},
	{MK_TSTR("40001"), MK_TSTR("Serialization failure"), 
		SQL_ERROR},
	{MK_TSTR("40002"), MK_TSTR("Integrity constraint violation"), 
		SQL_ERROR},
	{MK_TSTR("40003"), MK_TSTR("Statement completion unknown"), 
		SQL_ERROR},
	{MK_TSTR("42000"), MK_TSTR("Syntax error or access violation"), 
		SQL_ERROR},
	{MK_TSTR("42S01"), MK_TSTR("Base table or view already exists"), 
		SQL_ERROR},
	{MK_TSTR("42S02"), MK_TSTR("Base table or view not found"), 
		SQL_ERROR},
	{MK_TSTR("42S11"), MK_TSTR("Index already exists"), 
		SQL_ERROR},
	{MK_TSTR("42S12"), MK_TSTR("Index not found"), 
		SQL_ERROR},
	{MK_TSTR("42S21"), MK_TSTR("Column already exists"), 
		SQL_ERROR},
	{MK_TSTR("42S22"), MK_TSTR("Column not found"), 
		SQL_ERROR},
	{MK_TSTR("44000"), MK_TSTR("WITH CHECK OPTION violation"), 
		SQL_ERROR},

	{MK_TSTR("HY000"), MK_TSTR("General error"), 
		SQL_ERROR},
	{MK_TSTR("HY001"), MK_TSTR("Memory allocation error"), 
		SQL_ERROR},
	{MK_TSTR("HY003"), MK_TSTR("Invalid application buffer type"), 
		SQL_ERROR},
	{MK_TSTR("HY004"), MK_TSTR("Invalid SQL data type"), 
		SQL_ERROR},
	{MK_TSTR("HY007"), MK_TSTR("Associated statement is not prepared"), 
		SQL_ERROR},
	{MK_TSTR("HY008"), MK_TSTR("Operation canceled"), 
		SQL_ERROR},
	{MK_TSTR("HY009"), MK_TSTR("Invalid use of null pointer"), 
		SQL_ERROR},
	{MK_TSTR("HY010"), MK_TSTR("Function sequence error"), 
		SQL_ERROR},
	{MK_TSTR("HY011"), MK_TSTR("Attribute cannot be set now"), 
		SQL_ERROR},
	{MK_TSTR("HY012"), MK_TSTR("Invalid transaction operation code"), 
		SQL_ERROR},
	{MK_TSTR("HY013"), MK_TSTR("Memory management error"), 
		SQL_ERROR},
	{MK_TSTR("HY014"), MK_TSTR("Limit on the number of handles exceeded"), 
		SQL_ERROR},
	{MK_TSTR("HY015"), MK_TSTR("No cursor name available"), 
		SQL_ERROR},
	{MK_TSTR("HY016"), 
		MK_TSTR("Cannot modify an implementation row descriptor"),
		SQL_ERROR},
	{MK_TSTR("HY017"), MK_TSTR("Invalid use of an automatically allocated "\
			"descriptor handle"), 
		SQL_ERROR},
	{MK_TSTR("HY018"), MK_TSTR("Server declined cancel request"), 
		SQL_ERROR},
	{MK_TSTR("HY019"), 
		MK_TSTR("Non-character and non-binary data sent in pieces"), 
		SQL_ERROR},
	{MK_TSTR("HY020"), MK_TSTR("Attempt to concatenate a null value"), 
		SQL_ERROR},
	{MK_TSTR("HY021"), MK_TSTR("Inconsistent descriptor information"), 
		SQL_ERROR},
	{MK_TSTR("HY024"), MK_TSTR("Invalid attribute value"), 
		SQL_ERROR},
	{MK_TSTR("HY090"), MK_TSTR("Invalid string or buffer length"), 
		SQL_ERROR},
	{MK_TSTR("HY091"), MK_TSTR("Invalid descriptor field identifier"), 
		SQL_ERROR},
	{MK_TSTR("HY092"), MK_TSTR("Invalid attribute/option identifier"), 
		SQL_ERROR},
	{MK_TSTR("HY095"), MK_TSTR("Function type out of range"), 
		SQL_ERROR},
	{MK_TSTR("HY096"), MK_TSTR("Invalid information type"), 
		SQL_ERROR},
	{MK_TSTR("HY097"), MK_TSTR("Column type out of range"), 
		SQL_ERROR},
	{MK_TSTR("HY098"), MK_TSTR("Scope type out of range"), 
		SQL_ERROR},
	{MK_TSTR("HY099"), MK_TSTR("Nullable type out of range"), 
		SQL_ERROR},
	{MK_TSTR("HY100"), MK_TSTR("Uniqueness option type out of range"), 
		SQL_ERROR},
	{MK_TSTR("HY101"), MK_TSTR("Accuracy option type out of range"), 
		SQL_ERROR},
	{MK_TSTR("HY103"), MK_TSTR("Invalid retrieval code"), 
		SQL_ERROR},
	{MK_TSTR("HY104"), MK_TSTR("Invalid precision or scale value"), 
		SQL_ERROR},
	{MK_TSTR("HY105"), MK_TSTR("Invalid parameter type"), 
		SQL_ERROR},
	{MK_TSTR("HY106"), MK_TSTR("Fetch type out of range"), 
		SQL_ERROR},
	{MK_TSTR("HY107"), MK_TSTR("Row value out of range"), 
		SQL_ERROR},
	{MK_TSTR("HY109"), MK_TSTR("Invalid cursor position"), 
		SQL_ERROR},
	{MK_TSTR("HY110"), MK_TSTR("Invalid driver completion"), 
		SQL_ERROR},
	{MK_TSTR("HY111"), MK_TSTR("Invalid bookmark value"), 
		SQL_ERROR},
	{MK_TSTR("HYC00"), MK_TSTR("Optional feature not implemented"), 
		SQL_ERROR},
	{MK_TSTR("HYT00"), MK_TSTR("Timeout expired"), 
		SQL_ERROR},
	{MK_TSTR("HYT01"), MK_TSTR("Connection timeout expired"), 
		SQL_ERROR},

	{MK_TSTR("IM001"), MK_TSTR("Driver does not support this function"), 
		SQL_ERROR},
	{MK_TSTR("IM002"), MK_TSTR("Data source name not found and no default "\
			"driver specified"), 
		SQL_ERROR},
	{MK_TSTR("IM003"), MK_TSTR("Specified driver could not be loaded"), 
		SQL_ERROR},
	{MK_TSTR("IM004"), MK_TSTR("Driver's SQLAllocHandle on SQL_HANDLE_ENV "\
			"failed"), SQL_ERROR},
	{MK_TSTR("IM005"), MK_TSTR("Driver's SQLAllocHandle on "\
			"SQL_HANDLE_DBC failed"), SQL_ERROR},
	{MK_TSTR("IM006"), MK_TSTR("Driver's SQLSetConnectAttr failed"), 
		SQL_ERROR},
	{MK_TSTR("IM007"), MK_TSTR("No data source or driver specified; dialog "\
				"prohibited"), SQL_ERROR},
	{MK_TSTR("IM008"), MK_TSTR("Dialog failed"), 
		SQL_ERROR},
	{MK_TSTR("IM009"), MK_TSTR("Unable to load translation DLL"), 
		SQL_ERROR},
	{MK_TSTR("IM010"), MK_TSTR("Data source name too long"), 
		SQL_ERROR},
	{MK_TSTR("IM011"), MK_TSTR("Driver name too long"), 
		SQL_ERROR},
	{MK_TSTR("IM012"), MK_TSTR("DRIVER keyword syntax error"), 
		SQL_ERROR},
	{MK_TSTR("IM013"), MK_TSTR("Trace file error"), 
		SQL_ERROR},
	{MK_TSTR("IM014"), MK_TSTR("Invalid name of File DSN"), 
		SQL_ERROR},
	{MK_TSTR("IM015"), MK_TSTR("Corrupt file data source"), 
		SQL_ERROR},
};


/* stringifying in two preproc. passes */
#define _STR(_x)	# _x
#define STR(_x)		_STR(_x)

/* driver version ex. 1.2(u) */
#define ESODBC_DRIVER_VER	\
	STR(DRV_VER_MAJOR) "." STR(DRV_VER_MINOR) "(" STR(DRV_ENCODING) ")"
#define ESODBC_DIAG_PREFIX	"[Elastic][EsODBC " ESODBC_DRIVER_VER " Driver]"

typedef struct {
	esodbc_state_et state;
	/* [vendor-identifier][ODBC-component-identifier]component-supplied-text */
	SQLTCHAR text[SQL_MAX_MESSAGE_LENGTH];
	/* lenght of characters in the buffer */
	SQLUSMALLINT text_len; /* in characters, not bytes, w/o the 0-term */
							/* (SQLSMALLINT)wcslen(native_text) */
	/* returned in SQLGetDiagField()/SQL_DIAG_NATIVE, SQLGetDiagRecW() */
	SQLINTEGER native_code;
} esodbc_diag_st;


SQLRETURN post_diagnostic(esodbc_diag_st *dest, esodbc_state_et state, 
		SQLTCHAR *text, SQLINTEGER code);
/* post state into the diagnostic and return state's return code */
#define RET_DIAG(_d/*est*/, _s/*tate*/, _t/*ext*/, _c/*ode*/) \
		return post_diagnostic(_d, _s, _t, _c)
/* same as above, but take C-strings as messages */
#define RET_CDIAG(_d/*est*/, _s/*tate*/, _t/*char text*/, _c/*ode*/) \
		RET_DIAG(_d, _s, MK_TSTR(_t), _c)

#endif /* __ERROR_H__ */


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
