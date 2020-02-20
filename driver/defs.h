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

/* the (POSIX) timezone environment variable */
#define ESODBC_TZ_ENV_VAR			"TZ"

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
#define ESODBC_CHAR_ESCAPE			'\\'
#define ESODBC_PATTERN_ESCAPE		"\\"
#define ESODBC_CATALOG_SEPARATOR	":"
#define ESODBC_CATALOG_TERM			"catalog"
#define ESODBC_TABLE_TERM			"table"
#define ESODBC_SCHEMA_TERM			"schema"
#define ESODBC_PARAM_MARKER			'?'

/* maximum identifer length: match ES/Lucene byte max */
#define ESODBC_MAX_IDENTIFIER_LEN		SHRT_MAX
/* "the relationship between the columns in the GROUP BY clause and the
 * nonaggregated columns in the select list" */
#define ESODBC_GROUP_BY					SQL_GB_NO_RELATION

/* 20 = len("18446744073709551616"), 1 << (sizeof(uint64_t) * 8bits) */
#define ESODBC_PRECISION_UINT64			20
/* 19 = len("9223372036854775808"), 1 << 63 */
#define ESODBC_PRECISION_INT64			19


/* TODO: validate "implementation-defined precision" choices */
/* default precision for DECIMAL and NUMERIC */
#define ESODBC_DEF_DECNUM_PRECISION		19
/* default precision for SQL_FLOAT (variable, vs. fixed SQL_DOUBLE/_REAL) */
#define ESODBC_DEF_FLOAT_PRECISION		16 /* minimum = HALF_FLOAT */
/* maximum seconds precision (i.e. sub-second accuracy) */
/* Seconds precision is currently 3, with ES/SQL's ISO8601 millis.
 * (Should move to 9 with nanosecond implementation) */
#define ESODBC_MAX_SEC_PRECISION		9
#define ESODBC_DEF_SEC_PRECISION		3
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
#define ESODBC_DEF_IVL_WS_PRECISION		6
#define ESODBC_DEF_IVL_WOS_DT_PREC		2
/* decimal, numeric */
#define ESODBC_DEF_DECNUM_SCALE			0
/*
 * interval lead precision maxes:
 * - YEAR,MONTH: LONG_MAX (2147483647)
 * - DAY: 106751991167300
 * - HOUR: 2562047788015215
 * - MINUTE: 153722867280912930 (0x0222,2222,2222,2222)
 * - SECOND: LLONG_MAX (9223372036854775807)
 * with Duration/Period of 11.0.1+13-LTS Win x64 O-JVM.
 * TODO: reasoning for these maxes mix??
 */
#define ESODBC_MAX_IVL_YEAR_LEAD_PREC	(sizeof("-2147483647") - 1)
#define ESODBC_MAX_IVL_MONTH_LEAD_PREC	(sizeof("-2147483647") - 1)
#define ESODBC_MAX_IVL_DAY_LEAD_PREC	(sizeof("-106751991167300") - 1)
#define ESODBC_MAX_IVL_HOUR_LEAD_PREC	(sizeof("-2562047788015215") - 1)
#define ESODBC_MAX_IVL_MINUTE_LEAD_PREC	(sizeof("-153722867280912930") - 1)
#define ESODBC_MAX_IVL_SECOND_LEAD_PREC	(sizeof("-9223372036854775807") - 1)



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
#define ELASTIC_SQL_PATH				"/_sql"
#define ELASTIC_SQL_CLOSE_SUBPATH		"/close"

/* initial receive buffer size for REST answers */
#define ESODBC_BODY_BUF_START_SIZE		(4 * 1024)

/*
 * Versions
 */
/* driver version ex. 7.0.0(b0a34b4,u,d) */
#define ESODBC_DRIVER_VER	STR(DRV_VERSION) \
	"(" STR(DRV_SRC_VER) "," STR(DRV_ENCODING) "," STR(DRV_BUILD_TYPE) ")"
#define ESODBC_ELASTICSEARCH_NAME	"Elasticsearch"

/*
 * Config defaults
 */
/* default maximum amount of bytes to accept in REST answers */
#define ESODBC_DEF_MAX_BODY_SIZE_MB	"100"
/* max number of rows to request from server */
#define ESODBC_DEF_FETCH_SIZE		"0" // no fetch size
/* default host to connect to */
//#define ESODBC_DEF_SERVER			"localhost"
/* to allow loopback capture on Win10 */
#define ESODBC_DEF_SERVER			"127.0.0.1"
/* Elasticsearch's default port */
#define ESODBC_DEF_PORT				"9200"
/* Elasticsearch on Cloud default port */
#define ESODBC_DEF_CLOUD_PORT		"9243"
/* default security setting: use SSL */
#define ESODBC_DEF_SECURE			"1"
/* default cloud security setting: check revoke */
#define ESODBC_DEF_CLOUD_SECURE		"4"
/* default global request timeout (0: no timeout) */
#define ESODBC_DEF_TIMEOUT			"0"
/* don't follow redirection from the server  */
#define ESODBC_DEF_FOLLOW			"yes"
/* packing of REST bodies (JSON or CBOR) */
#define ESODBC_DEF_PACKING			ESODBC_DSN_PACK_CBOR
/* zlib compression of REST bodies (auto/true/false) */
#define ESODBC_DEF_COMPRESSION		ESODBC_DSN_CMPSS_AUTO
/* default tracing activation */
#define ESODBC_DEF_TRACE_ENABLED	"0"
/* default tracing level */
#define ESODBC_DEF_TRACE_LEVEL		"WARN"
/* default TZ handling */
#define ESODBC_DEF_APPLY_TZ			"no"
/* default of scientific floats printing */
#define ESODBC_DEF_SCI_FLOATS		ESODBC_DSN_FLTS_DEF
#define ESODBC_PWD_VAL_SUBST		"<redacted>"
/* default version checking mode: strict, major, none (dbg only) */
#define ESODBC_DEF_VERSION_CHECKING	ESODBC_DSN_VC_STRICT
#define ESODBC_DEF_MFIELD_LENIENT	"true"
#define ESODBC_DEF_ESC_PVA			"true"
#define ESODBC_DEF_IDX_INC_FROZEN	"false"

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
 * No scrollabe cursors et al. just yet.
 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/interface-conformance-levels
 */
#define ESODBC_ODBC_INTERFACE_CONFORMANCE		SQL_OIC_CORE
#define ESODBC_GETDATA_EXTENSIONS				(0 | \
	SQL_GD_ANY_COLUMN | SQL_GD_ANY_ORDER | SQL_GD_BOUND)

/*
 * Deprecated info types.
 */
#define ESODBC_FETCH_DIRECTION					SQL_FD_FETCH_NEXT
#define ESODBC_POS_OPERATIONS					0L
#define ESODBC_LOCK_TYPES						0L
#define ESODBC_POSITIONED_STATEMENTS			0L
/* equivalent to ESODBC_ODBC_INTERFACE_CONFORMANCE above */
#define ESODBC_ODBC_API_CONFORMANCE				SQL_OAC_LEVEL1
#define ESODBC_SCROLL_CONCURRENCY				SQL_SCCO_READ_ONLY
/* equivalent to ESODBC_SQL_CONFORMANCE above */
#define ESODBC_ODBC_SQL_CONFORMANCE				SQL_OSC_MINIMUM
//#define ESODBC_ODBC_SQL_CONFORMANCE				SQL_OSC_CORE
#define ESODBC_STATIC_SENSITIVITY				0L

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
 * TODO: SQL_CA1_BULK_FETCH_BY_BOOKMARK: pending bookmarks */
#define ESODBC_FORWARD_ONLY_CURSOR_ATTRIBUTES1	(0 | \
	SQL_CA1_NEXT | \
	SQL_CA1_LOCK_NO_CHANGE ) /* tho SQLSetPos() not really supported */
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
 *   MINUTE, MONTH, MONTHNAME, QUARTER, SECOND, WEEK, YEAR, NOW,
 *   CURRENT_TIMESTAMP, CURRENT_DATE, CURDATE, CURRENT_TIME, CURTIME,
 *   TIMESTAMPADD, TIMESTAMPDIFF;
 * - not supported: none.
 */
#define ESODBC_TIMEDATE_FUNCTIONS				(0LU | \
	SQL_FN_TD_DAYNAME | SQL_FN_TD_DAYOFMONTH | SQL_FN_TD_DAYOFWEEK | \
	SQL_FN_TD_DAYOFYEAR | SQL_FN_TD_EXTRACT | SQL_FN_TD_HOUR | \
	SQL_FN_TD_MINUTE | SQL_FN_TD_MONTH | SQL_FN_TD_MONTHNAME | \
	SQL_FN_TD_QUARTER | SQL_FN_TD_SECOND | SQL_FN_TD_WEEK | \
	SQL_FN_TD_YEAR | SQL_FN_TD_NOW | SQL_FN_TD_CURRENT_TIMESTAMP | \
	SQL_FN_TD_CURDATE | SQL_FN_TD_CURRENT_DATE | SQL_FN_TD_CURRENT_TIME | \
	SQL_FN_TD_CURTIME | SQL_FN_TD_TIMESTAMPADD | SQL_FN_TD_TIMESTAMPDIFF)

/*
 * TIMESTAMPDIFF timestamp intervals:
 * - supported: FRAC_SECOND, SECOND, MINUTE, HOUR, DAY, WEEK, MONTH,
 *   QUARTER, YEAR;
 * - not supported: none.
 */
#define ESODBC_TIMEDATE_DIFF_INTERVALS			(0LU | \
	SQL_FN_TSI_DAY | SQL_FN_TSI_FRAC_SECOND | SQL_FN_TSI_HOUR | \
	SQL_FN_TSI_MINUTE | SQL_FN_TSI_MONTH | SQL_FN_TSI_QUARTER | \
	SQL_FN_TSI_SECOND | SQL_FN_TSI_WEEK | SQL_FN_TSI_YEAR)
/*
 * TIMESTAMPADD timestamp intervals:
 * - supported: FRAC_SECOND, SECOND, MINUTE, HOUR, DAY, WEEK, MONTH,
 *   QUARTER, YEAR;
 * - not supported: none.
 */
#define ESODBC_TIMEDATE_ADD_INTERVALS			(0LU | \
	SQL_FN_TSI_DAY | SQL_FN_TSI_FRAC_SECOND | SQL_FN_TSI_HOUR | \
	SQL_FN_TSI_MINUTE | SQL_FN_TSI_MONTH | SQL_FN_TSI_QUARTER | \
	SQL_FN_TSI_SECOND | SQL_FN_TSI_WEEK | SQL_FN_TSI_YEAR)
/*
 * System functions:
 * - supported: DATABASE, IFNULL, USER.
 * - not supported: none.
 */
#define ESODBC_SYSTEM_FUNCTIONS					(0LU | \
	SQL_FN_SYS_USERNAME | SQL_FN_SYS_DBNAME | SQL_FN_SYS_IFNULL)
/*
 * Convert functions support:
 * - supported: CAST, CONVERT.
 * - not supported: none.
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
 * - supported:  CURRENT_DATE, CURRENT_DATETIME, CURRENT_TIME.
 * - not supported: none.
 */
#define ESODBC_SQL92_DATETIME_FUNCTIONS			(0LU | \
	SQL_SDF_CURRENT_TIMESTAMP | SQL_SDF_CURRENT_DATE | SQL_SDF_CURRENT_TIME)
/*
 * SQL92 datetime literals support:
 * - supported: TIMESTAMP, DATE, TIME, INTERVAL: YEAR/MONTH/DAY/HOUR/MINUTE/
 *   SECOND/YEAR_TO_MONTH/DAY_TO_HOUR/DAY_TO_MINUTE/DAY_TO_SECOND/
 *   HOUR_TO_MINUTE/HOUR_TO_SECOND/MINUTE_TO_SECOND.
 * - not supported: none.
 */
#define ESODBC_DATETIME_LITERALS				(0LU | \
	SQL_DL_SQL92_TIMESTAMP | \
	SQL_DL_SQL92_DATE | \
	SQL_DL_SQL92_TIME | \
	SQL_DL_SQL92_INTERVAL_YEAR | \
	SQL_DL_SQL92_INTERVAL_MONTH | \
	SQL_DL_SQL92_INTERVAL_DAY | \
	SQL_DL_SQL92_INTERVAL_HOUR | \
	SQL_DL_SQL92_INTERVAL_MINUTE | \
	SQL_DL_SQL92_INTERVAL_SECOND | \
	SQL_DL_SQL92_INTERVAL_YEAR_TO_MONTH | \
	SQL_DL_SQL92_INTERVAL_DAY_TO_HOUR | \
	SQL_DL_SQL92_INTERVAL_DAY_TO_MINUTE | \
	SQL_DL_SQL92_INTERVAL_DAY_TO_SECOND | \
	SQL_DL_SQL92_INTERVAL_HOUR_TO_MINUTE | \
	SQL_DL_SQL92_INTERVAL_HOUR_TO_SECOND | \
	SQL_DL_SQL92_INTERVAL_MINUTE_TO_SECOND )
/*
 * SQL92 value functions:
 * - supported: CASE, CAST, COALESCE, NULLIF
 * - not supported: none.
 */
#define ODBC_SQL92_VALUE_EXPRESSIONS			(0LU | \
	SQL_SVE_CASE | SQL_SVE_CAST | SQL_SVE_COALESCE | SQL_SVE_NULLIF)

/*
 * ES specific data types
 */
#define ESODBC_SQL_BOOLEAN			16
#define ESODBC_SQL_GEO				114
#define ESODBC_SQL_NULL				0
#define ESODBC_SQL_UNSUPPORTED		1111
#define ESODBC_SQL_OBJECT			2002
#define ESODBC_SQL_NESTED			2002


#endif /* __DEFS_H__ */

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=78 : */
