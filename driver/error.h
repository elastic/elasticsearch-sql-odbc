/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#ifndef __ERROR_H__
#define __ERROR_H__

#include "util.h"

typedef struct {
	SQLSTATE code;
	SQLWCHAR *message;
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
	SQL_STATE_MAX
} esodbc_state_et;



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
	{MK_WPTR("00000"), MK_WPTR("Success"), 
		SQL_SUCCESS}, /* non standard */
	{MK_WPTR("01000"), MK_WPTR("General warning"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_WPTR("01001"), MK_WPTR("Cursor operation conflict"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_WPTR("01002"), MK_WPTR("Disconnect error"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_WPTR("01003"), MK_WPTR("NULL value eliminated in set function"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_WPTR("01004"), MK_WPTR("String data, right-truncated"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_WPTR("01006"), MK_WPTR("Privilege not revoked"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_WPTR("01007"), MK_WPTR("Privilege not granted"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_WPTR("01S00"), MK_WPTR("Invalid connection string attribute"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_WPTR("01S01"), MK_WPTR("Error in row"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_WPTR("01S02"), MK_WPTR("Option value changed"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_WPTR("01S06"), MK_WPTR("Attempt to fetch before the result set "\
			"returned the first rowset"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_WPTR("01S07"), MK_WPTR("Fractional truncation"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_WPTR("01S08"), MK_WPTR("Error saving File DSN"), 
		SQL_SUCCESS_WITH_INFO},
	{MK_WPTR("01S09"), MK_WPTR("Invalid keyword"), 
		SQL_SUCCESS_WITH_INFO},

	{MK_WPTR("07001"), MK_WPTR("Wrong number of parameters"), 
		SQL_ERROR},
	{MK_WPTR("07002"), MK_WPTR("COUNT field incorrect"), 
		SQL_ERROR},
	{MK_WPTR("07005"), 
		MK_WPTR("Prepared statement not a cursor-specification"),
		SQL_ERROR},
	{MK_WPTR("07006"), MK_WPTR("Restricted data type attribute violation"), 
		SQL_ERROR},
	{MK_WPTR("07009"), MK_WPTR("Invalid descriptor index"), 
		SQL_ERROR},
	{MK_WPTR("07S01"), MK_WPTR("Invalid use of default parameter"), 
		SQL_ERROR},

	{MK_WPTR("08001"), MK_WPTR("Client unable to establish connection"), 
		SQL_ERROR},
	{MK_WPTR("08002"), MK_WPTR("Connection name in use"), 
		SQL_ERROR},
	{MK_WPTR("08003"), MK_WPTR("Connection not open"), 
		SQL_ERROR},
	{MK_WPTR("08004"), MK_WPTR("Server rejected the connection"), 
		SQL_ERROR},
	{MK_WPTR("08007"), MK_WPTR("Connection failure during transaction"), 
		SQL_ERROR},
	{MK_WPTR("08S01"), MK_WPTR("Communication link failure"), 
		SQL_ERROR},

	{MK_WPTR("21S01"), MK_WPTR("Insert value list does not match column list"), 
		SQL_ERROR},
	{MK_WPTR("21S02"), 
		MK_WPTR("Degree of derived table does not match column list"), 
		SQL_ERROR},
	{MK_WPTR("22001"), MK_WPTR("String data, right-truncated"), 
		SQL_ERROR},
	{MK_WPTR("22002"), MK_WPTR("Indicator variable required but not supplied"),
		SQL_ERROR},
	{MK_WPTR("22003"), MK_WPTR("Numeric value out of range"), 
		SQL_ERROR},
	{MK_WPTR("22007"), MK_WPTR("Invalid datetime format"), 
		SQL_ERROR},
	{MK_WPTR("22008"), MK_WPTR("Datetime field overflow"), 
		SQL_ERROR},
	{MK_WPTR("22012"), MK_WPTR("Division by zero"), 
		SQL_ERROR},
	{MK_WPTR("22015"), MK_WPTR("Interval field overflow"), 
		SQL_ERROR},
	{MK_WPTR("22018"), 
		MK_WPTR("Invalid character value for cast specification"), 
		SQL_ERROR},
	{MK_WPTR("22019"), MK_WPTR("Invalid escape character"), 
		SQL_ERROR},
	{MK_WPTR("22025"), MK_WPTR("Invalid escape sequence"), 
		SQL_ERROR},
	{MK_WPTR("22026"), MK_WPTR("String data, length mismatch"), 
		SQL_ERROR},
	{MK_WPTR("23000"), MK_WPTR("Integrity constraint violation"), 
		SQL_ERROR},
	{MK_WPTR("24000"), MK_WPTR("Invalid cursor state"), 
		SQL_ERROR},
	{MK_WPTR("25000"), MK_WPTR("Invalid transaction state"), 
		SQL_ERROR},
	{MK_WPTR("25S01"), MK_WPTR("Transaction state"), 
		SQL_ERROR},
	{MK_WPTR("25S02"), MK_WPTR("Transaction is still active"), 
		SQL_ERROR},
	{MK_WPTR("25S03"), MK_WPTR("Transaction is rolled back"), 
		SQL_ERROR},
	{MK_WPTR("28000"), MK_WPTR("Invalid authorization specification"), 
		SQL_ERROR},
	{MK_WPTR("34000"), MK_WPTR("Invalid cursor name"), 
		SQL_ERROR},
	{MK_WPTR("3C000"), MK_WPTR("Duplicate cursor name"), 
		SQL_ERROR},
	{MK_WPTR("3D000"), MK_WPTR("Invalid catalog name"), 
		SQL_ERROR},
	{MK_WPTR("3F000"), MK_WPTR("Invalid schema name"), 
		SQL_ERROR},
	{MK_WPTR("40001"), MK_WPTR("Serialization failure"), 
		SQL_ERROR},
	{MK_WPTR("40002"), MK_WPTR("Integrity constraint violation"), 
		SQL_ERROR},
	{MK_WPTR("40003"), MK_WPTR("Statement completion unknown"), 
		SQL_ERROR},
	{MK_WPTR("42000"), MK_WPTR("Syntax error or access violation"), 
		SQL_ERROR},
	{MK_WPTR("42S01"), MK_WPTR("Base table or view already exists"), 
		SQL_ERROR},
	{MK_WPTR("42S02"), MK_WPTR("Base table or view not found"), 
		SQL_ERROR},
	{MK_WPTR("42S11"), MK_WPTR("Index already exists"), 
		SQL_ERROR},
	{MK_WPTR("42S12"), MK_WPTR("Index not found"), 
		SQL_ERROR},
	{MK_WPTR("42S21"), MK_WPTR("Column already exists"), 
		SQL_ERROR},
	{MK_WPTR("42S22"), MK_WPTR("Column not found"), 
		SQL_ERROR},
	{MK_WPTR("44000"), MK_WPTR("WITH CHECK OPTION violation"), 
		SQL_ERROR},

	{MK_WPTR("HY000"), MK_WPTR("General error"), 
		SQL_ERROR},
	{MK_WPTR("HY001"), MK_WPTR("Memory allocation error"), 
		SQL_ERROR},
	{MK_WPTR("HY003"), MK_WPTR("Invalid application buffer type"), 
		SQL_ERROR},
	{MK_WPTR("HY004"), MK_WPTR("Invalid SQL data type"), 
		SQL_ERROR},
	{MK_WPTR("HY007"), MK_WPTR("Associated statement is not prepared"), 
		SQL_ERROR},
	{MK_WPTR("HY008"), MK_WPTR("Operation canceled"), 
		SQL_ERROR},
	{MK_WPTR("HY009"), MK_WPTR("Invalid use of null pointer"), 
		SQL_ERROR},
	{MK_WPTR("HY010"), MK_WPTR("Function sequence error"), 
		SQL_ERROR},
	{MK_WPTR("HY011"), MK_WPTR("Attribute cannot be set now"), 
		SQL_ERROR},
	{MK_WPTR("HY012"), MK_WPTR("Invalid transaction operation code"), 
		SQL_ERROR},
	{MK_WPTR("HY013"), MK_WPTR("Memory management error"), 
		SQL_ERROR},
	{MK_WPTR("HY014"), MK_WPTR("Limit on the number of handles exceeded"), 
		SQL_ERROR},
	{MK_WPTR("HY015"), MK_WPTR("No cursor name available"), 
		SQL_ERROR},
	{MK_WPTR("HY016"), 
		MK_WPTR("Cannot modify an implementation row descriptor"),
		SQL_ERROR},
	{MK_WPTR("HY017"), MK_WPTR("Invalid use of an automatically allocated "\
			"descriptor handle"), 
		SQL_ERROR},
	{MK_WPTR("HY018"), MK_WPTR("Server declined cancel request"), 
		SQL_ERROR},
	{MK_WPTR("HY019"), 
		MK_WPTR("Non-character and non-binary data sent in pieces"), 
		SQL_ERROR},
	{MK_WPTR("HY020"), MK_WPTR("Attempt to concatenate a null value"), 
		SQL_ERROR},
	{MK_WPTR("HY021"), MK_WPTR("Inconsistent descriptor information"), 
		SQL_ERROR},
	{MK_WPTR("HY024"), MK_WPTR("Invalid attribute value"), 
		SQL_ERROR},
	{MK_WPTR("HY090"), MK_WPTR("Invalid string or buffer length"), 
		SQL_ERROR},
	{MK_WPTR("HY091"), MK_WPTR("Invalid descriptor field identifier"), 
		SQL_ERROR},
	{MK_WPTR("HY092"), MK_WPTR("Invalid attribute/option identifier"), 
		SQL_ERROR},
	{MK_WPTR("HY095"), MK_WPTR("Function type out of range"), 
		SQL_ERROR},
	{MK_WPTR("HY096"), MK_WPTR("Invalid information type"), 
		SQL_ERROR},
	{MK_WPTR("HY097"), MK_WPTR("Column type out of range"), 
		SQL_ERROR},
	{MK_WPTR("HY098"), MK_WPTR("Scope type out of range"), 
		SQL_ERROR},
	{MK_WPTR("HY099"), MK_WPTR("Nullable type out of range"), 
		SQL_ERROR},
	{MK_WPTR("HY100"), MK_WPTR("Uniqueness option type out of range"), 
		SQL_ERROR},
	{MK_WPTR("HY101"), MK_WPTR("Accuracy option type out of range"), 
		SQL_ERROR},
	{MK_WPTR("HY103"), MK_WPTR("Invalid retrieval code"), 
		SQL_ERROR},
	{MK_WPTR("HY104"), MK_WPTR("Invalid precision or scale value"), 
		SQL_ERROR},
	{MK_WPTR("HY105"), MK_WPTR("Invalid parameter type"), 
		SQL_ERROR},
	{MK_WPTR("HY106"), MK_WPTR("Fetch type out of range"), 
		SQL_ERROR},
	{MK_WPTR("HY107"), MK_WPTR("Row value out of range"), 
		SQL_ERROR},
	{MK_WPTR("HY109"), MK_WPTR("Invalid cursor position"), 
		SQL_ERROR},
	{MK_WPTR("HY110"), MK_WPTR("Invalid driver completion"), 
		SQL_ERROR},
	{MK_WPTR("HY111"), MK_WPTR("Invalid bookmark value"), 
		SQL_ERROR},
	{MK_WPTR("HYC00"), MK_WPTR("Optional feature not implemented"), 
		SQL_ERROR},
	{MK_WPTR("HYT00"), MK_WPTR("Timeout expired"), 
		SQL_ERROR},
	{MK_WPTR("HYT01"), MK_WPTR("Connection timeout expired"), 
		SQL_ERROR},

	{MK_WPTR("IM001"), MK_WPTR("Driver does not support this function"), 
		SQL_ERROR},
	{MK_WPTR("IM002"), MK_WPTR("Data source name not found and no default "\
			"driver specified"), 
		SQL_ERROR},
	{MK_WPTR("IM003"), MK_WPTR("Specified driver could not be loaded"), 
		SQL_ERROR},
	{MK_WPTR("IM004"), MK_WPTR("Driver's SQLAllocHandle on SQL_HANDLE_ENV "\
			"failed"), SQL_ERROR},
	{MK_WPTR("IM005"), MK_WPTR("Driver's SQLAllocHandle on "\
			"SQL_HANDLE_DBC failed"), SQL_ERROR},
	{MK_WPTR("IM006"), MK_WPTR("Driver's SQLSetConnectAttr failed"), 
		SQL_ERROR},
	{MK_WPTR("IM007"), MK_WPTR("No data source or driver specified; dialog "\
				"prohibited"), SQL_ERROR},
	{MK_WPTR("IM008"), MK_WPTR("Dialog failed"), 
		SQL_ERROR},
	{MK_WPTR("IM009"), MK_WPTR("Unable to load translation DLL"), 
		SQL_ERROR},
	{MK_WPTR("IM010"), MK_WPTR("Data source name too long"), 
		SQL_ERROR},
	{MK_WPTR("IM011"), MK_WPTR("Driver name too long"), 
		SQL_ERROR},
	{MK_WPTR("IM012"), MK_WPTR("DRIVER keyword syntax error"), 
		SQL_ERROR},
	{MK_WPTR("IM013"), MK_WPTR("Trace file error"), 
		SQL_ERROR},
	{MK_WPTR("IM014"), MK_WPTR("Invalid name of File DSN"), 
		SQL_ERROR},
	{MK_WPTR("IM015"), MK_WPTR("Corrupt file data source"), 
		SQL_ERROR},
};


#define ESODBC_DIAG_PREFIX	"[Elastic][EsODBC " ESODBC_DRIVER_VER " Driver]"

typedef struct {
	esodbc_state_et state;
	/* [vendor-identifier][ODBC-component-identifier]component-supplied-text */
	SQLWCHAR text[SQL_MAX_MESSAGE_LENGTH];
	/* lenght of characters in the buffer */
	SQLUSMALLINT text_len; /* in characters, not bytes, w/o the 0-term */
							/* (SQLSMALLINT)wcslen(native_text) */
	/* returned in SQLGetDiagField()/SQL_DIAG_NATIVE, SQLGetDiagRecW() */
	SQLINTEGER native_code;
	SQLLEN row_number;
	SQLINTEGER column_number;
} esodbc_diag_st;


void init_diagnostic(esodbc_diag_st *dest);
SQLRETURN post_diagnostic(esodbc_diag_st *dest, esodbc_state_et state, 
		SQLWCHAR *text, SQLINTEGER code);
/* post state into the diagnostic and return state's return code */
#define RET_DIAG(_d/*est*/, _s/*tate*/, _t/*ext*/, _c/*ode*/) \
		return post_diagnostic(_d, _s, _t, _c)
/* same as above, but take C-strings as messages */
#define RET_CDIAG(_d/*est*/, _s/*tate*/, _t/*char text*/, _c/*ode*/) \
		RET_DIAG(_d, _s, MK_WPTR(_t), _c)

SQLRETURN post_row_diagnostic(esodbc_diag_st *dest, esodbc_state_et state, 
		SQLWCHAR *text, SQLINTEGER code, SQLLEN nrow, SQLINTEGER ncol);

#endif /* __ERROR_H__ */


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
