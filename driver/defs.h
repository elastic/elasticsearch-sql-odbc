/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#ifndef __DEFS_H__
#define __DEFS_H__

/*
 * DEFaultS
 */

/* Tracing log buffer size. */
#define ESODBC_LOG_BUF_SIZE			(4 * 1024)
/* Log file prefix. The format is: prefix_datime */
#define ESODBC_LOG_FILE_PREFIX		"esodbc"
#define ESODBC_LOG_FILE_SUFFIX		".log"
/* Environment variable name holding the log directory name */
#define ESODBC_LOG_DIR_ENV_VAR		"ESODBC_LOG_DIR"
/* number of consecutive logging failures that will disable logging */
#define ESODBC_LOG_MAX_RETRY		5

#define ESODBC_MAX_ROW_ARRAY_SIZE	USHRT_MAX
/* max number of ES/SQL types supported */
#define ESODBC_MAX_NO_TYPES			64
#define ESODBC_DEF_ARRAY_SIZE		1
/* max cols or args to bind; needs to stay <= SHRT_MAX */
#define ESODBC_MAX_DESC_COUNT		SHRT_MAX
/* number of static records for SQLGetData(): if using SQLGetData() with more
 * columns than this def, recs will be allocated dynamically. */
#define ESODBC_GD_DESC_COUNT		128
/* values for SQL_ATTR_MAX_LENGTH statement attribute */
#define ESODBC_UP_MAX_LENGTH		0 // USHORT_MAX
#define ESODBC_LO_MAX_LENGTH		0
/* Prepare a STMT for a new SQL operation.
 * To be used with catalog functions, that can be all called with same stmt.
 * After SQL_CLOSE, an applicatoin can re-open a cursor for same query. */
#define ESODBC_SQL_CLOSE			((SQLUSMALLINT)-1)
#ifdef TESTING
/* connecting mode for testing */
#define ESODBC_SQL_DRIVER_TEST		((SQLUSMALLINT)-1)
#endif /* TESTING */

#define ESODBC_ALL_TABLES			"%"
#define ESODBC_ALL_COLUMNS			"%"
#define ESODBC_STRING_DELIM			"'"
#define ESODBC_QUOTE_CHAR			"\""
#define ESODBC_PATTERN_ESCAPE		"\\"
#define ESODBC_CATALOG_SEPARATOR	":"
#define ESODBC_CATALOG_TERM			"catalog"
#define ESODBC_TABLE_TERM			"table"
#define ESODBC_SCHEMA_TERM			"schema"
#define ESODBC_PARAM_MARKER			"?"

/* maximum identifer length: match ES/Lucene byte max */
#define ESODBC_MAX_IDENTIFIER_LEN		SHRT_MAX
/* "the relationship between the columns in the GROUP BY clause and the
 * nonaggregated columns in the select list" */
#define ESODBC_GROUP_BY						SQL_GB_NO_RELATION

/* 20 = len("18446744073709551616"), 1 << (sizeof(uint64_t) * 8bits) */
#define ESODBC_PRECISION_UINT64			20
/* 19 = len("9223372036854775808"), 1 << 63 */
#define ESODBC_PRECISION_INT64			19
#define ESODBC_PRECISION_DOUBLE			308


/* TODO: validate "implementation-defined precision" choices */
/* default precision for DECIMAL and NUMERIC */
#define ESODBC_DEF_DECNUM_PRECISION		19
/* default precision for float */
#define ESODBC_DEF_FLOAT_PRECISION		8
/* maximum fixed numeric precision */
#define ESODBC_MAX_FIX_PRECISION		ESODBC_PRECISION_UINT64
/* maximum floating numeric precision */
#define ESODBC_MAX_FLT_PRECISION		ESODBC_PRECISION_DOUBLE
/* maximum seconds precision */
#define ESODBC_MAX_SEC_PRECISION		ESODBC_PRECISION_UINT64
/*
 * standard specified defaults:
 * https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetdescfield-function##record-fields
 */
/* string */
#define ESODBC_DEF_STRING_LENGTH		1
#define ESODBC_DEF_STRING_PRECISION		0
/* time */
#define ESODBC_DEF_DATETIME_PRECISION	0
#define ESODBC_DEF_TIMESTAMP_PRECISION	6
/* interval */
#define ESODBC_DEF_INTVL_WS_PRECISION	6
#define ESODBC_DEF_INTVL_WOS_DT_PREC	2
/* decimal, numeric */
#define ESODBC_DEF_DECNUM_SCALE			0




/*
 * Not authoriative (there's actually no formal limit), but pretty informed:
 * https://stackoverflow.com/questions/417142/what-is-the-maximum-length-of-a-url-in-different-browsers
 */
/* maximum URL size */
#define ESODBC_MAX_URL_LEN				2048
/* maximum DNS attribute value lenght (should be long enought to accomodate a
 * decently long FQ file path name) */
#define ESODBC_DSN_MAX_ATTR_LEN			1024
/* sample DSN name provisioned with the installation  */
#define ESODBC_DSN_SAMPLE_NAME			"Elasticsearch ODBC Sample DSN"

/* SQL plugin's REST endpoint for SQL */
#define ELASTIC_SQL_PATH				"/_xpack/sql"

/* initial receive buffer size for REST answers */
#define ESODBC_BODY_BUF_START_SIZE		(4 * 1024)

/*
 * Versions
 */
/* driver version ex. 7.0.0(b0a34b4,u,d) */
#define ESODBC_DRIVER_VER	STR(DRV_VERSION) \
	"(" STR(DRV_SRC_VER) "," STR(DRV_ENCODING) "," STR(DRV_BUILD_TYPE) ")"
/* TODO: POST / (together with cluster "sniffing") */
#define ESODBC_ELASTICSEARCH_VER	"7.0.0"
#define ESODBC_ELASTICSEARCH_NAME	"Elasticsearch"

/*
 * Config defaults
 */
/* default maximum amount of bytes to accept in REST answers */
#define ESODBC_DEF_MAX_BODY_SIZE_MB	"10"
/* max number of rows to request from server */
#define ESODBC_DEF_FETCH_SIZE		"0" // no fetch size
/* default host to connect to */
//#define ESODBC_DEF_SERVER			"localhost"
/* to allow loopback capture on Win10 */
#define ESODBC_DEF_SERVER			"127.0.0.1"
/* Elasticsearch'es default port */
#define ESODBC_DEF_PORT				"9200"
/* default security setting: use SSL */
#define ESODBC_DEF_SECURE			"1"
/* default global request timeout (0: no timeout) */
#define ESODBC_DEF_TIMEOUT			"0"
/* don't follow redirection from the server  */
#define ESODBC_DEF_FOLLOW			"yes"
/* packing of REST bodies (JSON or CBOR) */
#define ESODBC_DEF_PACKING			"JSON"
/* default tracing activation */
#define ESODBC_DEF_TRACE_ENABLED	"0"
/* default tracing level */
#define ESODBC_DEF_TRACE_LEVEL		"WARN"
#define ESODBC_PWD_VAL_SUBST		"<original substituted>"

/*
 *
 * Driver/Elasticsearch capabilities (SQLGetInfo)
 *
 */

/* What level of SQL92 grammar is supported?
 * Note: minimum (entry) level includes DISTINCT.
 * Intermediate sql1992.txt "Leveling Rules" enforces rules I'm not sure we
 * respect yet (ex., see CURRENT_ functions). */
#define ESODBC_SQL_CONFORMANCE					SQL_SC_SQL92_ENTRY
/* Driver conformance level: CORE.
 * No scrollabe cursors et al. just yet */
#define ESODBC_ODBC_INTERFACE_CONFORMANCE		SQL_OIC_CORE
#define ESODBC_GETDATA_EXTENSIONS				(0 | \
	SQL_GD_ANY_COLUMN | SQL_GD_ANY_ORDER | SQL_GD_BOUND)

/*
 * Catalog support:
 * - supported in: DML STATEMENTS, ODBC PROCEDURE INVOCATION.
 * - not supported in: TABLE/INDEX/PRIVILEGE DEFINITION.
 */
#define ESODBC_CATALOG_USAGE					(0 | \
	SQL_CU_DML_STATEMENTS | SQL_CU_PROCEDURE_INVOCATION)
/* what's allowed in an identifier name (eg. table, column, index) except
 * [a-zA-Z0-9_], accepted in a delimited specification. */
#define ESODBC_SPECIAL_CHARACTERS				" !\"#$%&'()*+,-./" /*[0-9]*/ \
	";<=>?@" /*[A-Z]*/ "[\\]^" /*[_]*/ "`" /*[a-z]*/ "{|}~"
/* SQLFetchScroll() and SQLSetPos() capabilities.
 * TODO: NEXT, ABS, RELATIVE: pending SetPos()
 * TODO: BULK_FETC: pending bookmarks */
#define ESODBC_FORWARD_ONLY_CURSOR_ATTRIBUTES1	(0 | \
	SQL_CA1_NEXT | SQL_CA1_ABSOLUTE | SQL_CA1_RELATIVE | \
	SQL_CA1_LOCK_NO_CHANGE | \
	SQL_CA1_POS_POSITION | \
	SQL_CA1_BULK_FETCH_BY_BOOKMARK )
#define ESODBC_FORWARD_ONLY_CURSOR_ATTRIBUTES2	(0 | \
	SQL_CA2_READ_ONLY_CONCURRENCY | \
	SQL_CA2_SIMULATE_NON_UNIQUE ) /* tho no update/delete supported */

/*
 * SELECT predicates:
 * - supported: BETWEEN, COMPARISON, EXISTS, IN, ISNOTNULL, ISNULL, LIKE.
 * - not supported: MATCH FULL/PARTIAL, MATCH UNIQUE FULL/PARTIAL, OVERLAPS,
 * QUANTIFIED COMPARISON, UNIQUE.
 */
#define ESODBC_SQL92_PREDICATES					(0 | \
	SQL_SP_BETWEEN | SQL_SP_COMPARISON  | SQL_SP_EXISTS | SQL_SP_IN | \
	SQL_SP_ISNOTNULL | SQL_SP_ISNULL | SQL_SP_LIKE)
/*
 * No JOIN support.
 */
#define ESODBC_SQL92_RELATIONAL_JOIN_OPERATORS	0LU

/*
 * String functions support:
 * - supported: ASCII, BIT_LENGTH, CHAR, CHAR_LENGTH, CHARACTER_LENGTH,
 *   CONCAT, INSERT, LCASE, LEFT, LENGTH, LOCATE, LTRIM, OCTET_LENGTH,
 *   POSITION, REPEAT, REPLACE, RIGHT, RTRIM, SPACE, SUBSTRING, UCASE.
 * - not supported: DIFFERENCE, SOUNDEX.
 */
#define ESODBC_STRING_FUNCTIONS						(0LU | \
	SQL_FN_STR_ASCII | SQL_FN_STR_BIT_LENGTH | SQL_FN_STR_CHAR | \
	SQL_FN_STR_CHAR_LENGTH | SQL_FN_STR_CHARACTER_LENGTH | \
	SQL_FN_STR_CONCAT | SQL_FN_STR_INSERT | SQL_FN_STR_LCASE | \
	SQL_FN_STR_LEFT | SQL_FN_STR_LENGTH | SQL_FN_STR_LOCATE | \
	SQL_FN_STR_LTRIM | SQL_FN_STR_OCTET_LENGTH | SQL_FN_STR_POSITION | \
	SQL_FN_STR_REPEAT | SQL_FN_STR_REPLACE | SQL_FN_STR_RIGHT | \
	SQL_FN_STR_RTRIM | SQL_FN_STR_SPACE | SQL_FN_STR_SUBSTRING | \
	SQL_FN_STR_UCASE)
/*
 * Numeric functions support:
 * - supported: ABS, ACOS, ASIN, ATAN, ATAN2, CEIL, COS, COT, DEGREES, EXP,
 *   FLOOR, LOG, LOG10, MOD, PI, POWER, RADIANS, RAND, ROUND, SIGN, SIN, SQRT,
 *   TAN, TRUNCATE;
 * - not supported: none.
 */
#define ESODBC_NUMERIC_FUNCTIONS				(0LU | \
	SQL_FN_NUM_ABS | SQL_FN_NUM_ACOS | SQL_FN_NUM_ASIN | SQL_FN_NUM_ATAN |\
	SQL_FN_NUM_ATAN2 | SQL_FN_NUM_CEILING | SQL_FN_NUM_COS | \
	SQL_FN_NUM_COT | SQL_FN_NUM_DEGREES | SQL_FN_NUM_EXP | \
	SQL_FN_NUM_FLOOR | SQL_FN_NUM_LOG | SQL_FN_NUM_LOG10 | \
	SQL_FN_NUM_MOD | SQL_FN_NUM_PI | SQL_FN_NUM_POWER | \
	SQL_FN_NUM_RADIANS | SQL_FN_NUM_RAND | SQL_FN_NUM_ROUND | \
	SQL_FN_NUM_SIGN | SQL_FN_NUM_SIN | SQL_FN_NUM_SQRT | SQL_FN_NUM_TAN | \
	SQL_FN_NUM_TRUNCATE)
/*
 * Timedate functions support:
 * - supported: DAYNAME, DAYOFMONTH, DAYOFWEEK, DAYOFYEAR, EXTRACT, HOUR,
 *   MINUTE, MONTH, MONTHNAME, QUARTER, SECOND, WEEK, YEAR;
 * - not supported: CURRENT_DATE, CURRENT_TIME, CURRENT_TIMESTAMP, CURDATE,
 *   CURTIME, NOW, TIMESTAMPADD, TIMESTAMPDIFF.
 */
#define ESODBC_TIMEDATE_FUNCTIONS				(0LU | \
	SQL_FN_TD_DAYNAME | SQL_FN_TD_DAYOFMONTH | SQL_FN_TD_DAYOFWEEK | \
	SQL_FN_TD_DAYOFYEAR | SQL_FN_TD_EXTRACT | SQL_FN_TD_HOUR | \
	SQL_FN_TD_MINUTE | SQL_FN_TD_MONTH | SQL_FN_TD_MONTHNAME | \
	SQL_FN_TD_QUARTER | SQL_FN_TD_SECOND | SQL_FN_TD_WEEK | \
	SQL_FN_TD_YEAR)

/*
 * TIMESTAMPDIFF timestamp intervals:
 * - supported: none.
 * - not supported: FRAC_SECOND, SECOND, MINUTE, HOUR, DAY, WEEK, MONTH,
 *   QUARTER, YEAR.
 */
#define ESODBC_TIMEDATE_DIFF_INTERVALS			0LU
/*
 * TIMESTAMPADD timestamp intervals:
 * - supported: none
 * - not supported: FRAC_SECOND, SECOND, MINUTE, HOUR, DAY, WEEK, MONTH,
 *   QUARTER, YEAR.
 */
#define ESODBC_TIMEDATE_ADD_INTERVALS			0LU
/*
 * System functions:
 * - supported: none.
 * - not supported: DATABASE, IFNULL, USER
 */
#define ESODBC_SYSTEM_FUNCTIONS					0LU
/*
 * Convert functions support:
 * - supported: CAST.
 * - not supported: CONVERT.
 */
#define ESODBC_CONVERT_FUNCTIONS				(0LU | \
	SQL_FN_CVT_CONVERT | SQL_FN_CVT_CAST)
/*
 * Aggregate functions support:
 * - supported: ALL, AVG, COUNT, MAX, MIN, SUM.
 * - not supported: DISTINCT.
 */
#define ESODBC_AGGREGATE_FUNCTIONS				(0LU | \
	SQL_AF_ALL | SQL_AF_AVG | SQL_AF_COUNT | SQL_AF_MAX | SQL_AF_MIN | \
	SQL_AF_SUM)


/*** SQL92 support ***/

/*
 * SQL92 string functions:
 * - supported: none.
 * - not supported: CONVERT, LOWER, UPPER, SUBSTRING, TRANSLATE, TRIM, trim
 *   leading, trim trailing
 */
#define ESODBC_SQL92_STRING_FUNCTIONS			0LU
/*
 * SQL92 numeric value functions:
 * - supported: none.
 * - not supported: BIT_LENGTH, CHAR_LENGTH, CHARACTER_LENGTH, EXTRACT,
 *   OCTET_LENGTH, POSITION
 */
#define ESODBC_SQL92_NUMERIC_VALUE_FUNCTIONS	0LU
/*
 * SQL92 datetime functions:
 * - supported: none.
 * - not supported: CURRENT_DATE, CURRENT_TIME, CURRENT_DATETIME
 */
#define ESODBC_SQL92_DATETIME_FUNCTIONS			0LU
/*
 * SQL92 datetime literals support:
 * - supported: TIMESTAMP;
 * - not supported: DATE, TIME, INTERVAL: YEAR/MONTH/DAY/HOUR/MINUTE/SECOND/
 *   YEAR_TO_MONTH/DAY_TO_HOUR/DAY_TO_MINUTE/DAY_TO_SECOND/HOUR_TO_MINUTE/
 *   HOUR_TO_SECOND/MINUTE_TO_SECOND.
 */
#define ESODBC_DATETIME_LITERALS				SQL_DL_SQL92_TIMESTAMP
/*
 * SQL92 value functions:
 * - supported: none.
 * - not supported: CASE, CAST, COALESCE, NULLIF
 */
#define ODBC_SQL92_VALUE_EXPRESSIONS			0LU

/*
 * ES specific data types
 */
#define ESODBC_SQL_BOOLEAN			16
#define ESODBC_SQL_NULL				0
#define ESODBC_SQL_UNSUPPORTED		1111
#define ESODBC_SQL_OBJECT			2002
#define ESODBC_SQL_NESTED			2002

/*
 * ISO8601 template ('yyyy-mm-ddThh:mm:ss.sss+hh:mm')
 */
#define ESODBC_ISO8601_TEMPLATE		"yyyy-mm-ddThh:mm:ss.sssssssZ"
/* https://docs.microsoft.com/en-us/sql/relational-databases/native-client-odbc-date-time/data-type-support-for-odbc-date-and-time-improvements */
#define ESODBC_DATE_TEMPLATE		"yyyy-mm-ddT"
#define ESODBC_TIME_TEMPLATE		"hh:mm:ss.9999999"

/*
 * ES-to-C-SQL mappings
 * DATA_TYPE(SYS TYPES) : SQL_<type> -> SQL_C_<type>
 * Collected here for a quick overview (and easy change); can't be automated.
 */
/* -6: SQL_TINYINT -> SQL_C_TINYINT */
#define ESODBC_ES_TO_CSQL_BYTE			SQL_C_TINYINT
#define ESODBC_ES_TO_SQL_BYTE			SQL_TINYINT
/* 5: SQL_SMALLINT -> SQL_C_SHORT */
#define ESODBC_ES_TO_CSQL_SHORT			SQL_C_SSHORT
#define ESODBC_ES_TO_SQL_SHORT			SQL_SMALLINT
/* 4: SQL_INTEGER -> SQL_C_LONG */
#define ESODBC_ES_TO_CSQL_INTEGER		SQL_C_SLONG
#define ESODBC_ES_TO_SQL_INTEGER		SQL_INTEGER
/* -5: SQL_BIGINT -> SQL_C_SBIGINT */
#define ESODBC_ES_TO_CSQL_LONG			SQL_C_SBIGINT
#define ESODBC_ES_TO_SQL_LONG			SQL_BIGINT
/* 6: SQL_FLOAT -> SQL_C_DOUBLE */
#define ESODBC_ES_TO_CSQL_HALF_FLOAT	SQL_C_DOUBLE
#define ESODBC_ES_TO_SQL_HALF_FLOAT		SQL_FLOAT
/* 6: SQL_FLOAT -> SQL_C_DOUBLE */
#define ESODBC_ES_TO_CSQL_SCALED_FLOAT	SQL_C_DOUBLE
#define ESODBC_ES_TO_SQL_SCALED_FLOAT	SQL_FLOAT
/* 7: SQL_REAL -> SQL_C_DOUBLE */
#define ESODBC_ES_TO_CSQL_FLOAT			SQL_C_FLOAT
#define ESODBC_ES_TO_SQL_FLOAT			SQL_REAL
/* 8: SQL_DOUBLE -> SQL_C_FLOAT */
#define ESODBC_ES_TO_CSQL_DOUBLE		SQL_C_DOUBLE
#define ESODBC_ES_TO_SQL_DOUBLE			SQL_DOUBLE
/* 16: ??? -> SQL_C_TINYINT */
#define ESODBC_ES_TO_CSQL_BOOLEAN		SQL_C_BIT
#define ESODBC_ES_TO_SQL_BOOLEAN		SQL_BIT
/* 12: SQL_VARCHAR -> SQL_C_WCHAR */
#define ESODBC_ES_TO_CSQL_KEYWORD		SQL_C_WCHAR /* XXX: CBOR needs _CHAR */
#define ESODBC_ES_TO_SQL_KEYWORD		SQL_VARCHAR
/* 12: SQL_VARCHAR -> SQL_C_WCHAR */
#define ESODBC_ES_TO_CSQL_TEXT			SQL_C_WCHAR /* XXX: CBOR needs _CHAR */
#define ESODBC_ES_TO_SQL_TEXT			SQL_VARCHAR
/* 12: SQL_VARCHAR -> SQL_C_WCHAR */
#define ESODBC_ES_TO_CSQL_IP			SQL_C_WCHAR /* XXX: CBOR needs _CHAR */
#define ESODBC_ES_TO_SQL_IP				SQL_VARCHAR
/* 93: SQL_TYPE_TIMESTAMP -> SQL_C_TYPE_TIMESTAMP */
#define ESODBC_ES_TO_CSQL_DATE			SQL_C_TYPE_TIMESTAMP
#define ESODBC_ES_TO_SQL_DATE			SQL_TYPE_TIMESTAMP
/* -3: SQL_VARBINARY -> SQL_C_BINARY */
#define ESODBC_ES_TO_CSQL_BINARY		SQL_C_BINARY
#define ESODBC_ES_TO_SQL_BINARY			SQL_VARBINARY
/* 0: SQL_TYPE_NULL -> SQL_C_TINYINT */
#define ESODBC_ES_TO_CSQL_NULL			SQL_C_STINYINT
#define ESODBC_ES_TO_SQL_NULL			SQL_TYPE_NULL
/* 1111: ??? -> SQL_C_BINARY */
#define ESODBC_ES_TO_CSQL_UNSUPPORTED	SQL_C_BINARY
#define ESODBC_ES_TO_SQL_UNSUPPORTED	ESODBC_SQL_UNSUPPORTED
/* 2002: ??? -> SQL_C_BINARY */
#define ESODBC_ES_TO_CSQL_OBJECT		SQL_C_BINARY
#define ESODBC_ES_TO_SQL_OBJECT			ESODBC_SQL_OBJECT
/* 2002: ??? -> SQL_C_BINARY */
#define ESODBC_ES_TO_CSQL_NESTED		SQL_C_BINARY
#define ESODBC_ES_TO_SQL_NESTED			ESODBC_SQL_NESTED

#endif /* __DEFS_H__ */
