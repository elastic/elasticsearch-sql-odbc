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

// FIXME: review@alpha
/* TODO: should there be a max? */
#define ESODBC_MAX_ROW_ARRAY_SIZE	128
#define ESODBC_DEF_ARRAY_SIZE		1
/* max cols or args to bind */
#define ESODBC_MAX_DESC_COUNT		128
/* values for SQL_ATTR_MAX_LENGTH statement attribute */
#define ESODBC_UP_MAX_LENGTH		0 // USHORT_MAX
#define ESODBC_LO_MAX_LENGTH		0
/* prepare a STMT for a new SQL operation.
 * To be used with catalog functions, that can be all called with same stmt */
#define ESODBC_SQL_CLOSE			((SQLUSMALLINT)-1)

#define ESODBC_ALL_TABLES			"%"
#define ESODBC_ALL_COLUMNS			"%"
#define ESODBC_STRING_DELIM			"'"
#define ESODBC_QUOTE_CHAR			"\""
#define ESODBC_PATTERN_ESCAPE		"\\"
#define ESODBC_CATALOG_SEPARATOR	":"
#define ESODBC_CATALOG_TERM			"clusterName"
#define ESODBC_TABLE_TERM			"type" // TODO: or table?
#define ESODBC_SCHEMA_TERM			"schema"
#define ESODBC_MAX_SCHEMA_LEN		0

/* max # of active statements for a connection */
/* TODO: review@alpha */
#define ESODBC_MAX_CONCURRENT_ACTIVITIES	16
/* maximum identifer lenght */
/* TODO: review@alpha */
#define ESODBC_MAX_IDENTIFIER_LEN			128


/*
 * Not authoriative (there's actually no formal limit), but pretty informed:
 * https://stackoverflow.com/questions/417142/what-is-the-maximum-length-of-a-url-in-different-browsers
 */
/* maximum URL size */
#define ESODBC_MAX_URL_LEN				2048
/* maximum DNS name */
/* SQL_MAX_DSN_LENGTH=32 < IPv6 len */
#define ESODBC_MAX_DNS_LEN				255

/* SQL plugin's REST endpoint for SQL */
#define ELASTIC_SQL_PATH				"/_xpack/sql"

/* initial receive buffer size for REST answers */
#define ESODBC_BODY_BUF_START_SIZE		(4 * 1024)

/*
 * Versions
 */
/* driver version ex. 1.2(u) */
#define ESODBC_DRIVER_VER	\
		STR(DRV_VER_MAJOR) "." STR(DRV_VER_MINOR) "(" STR(DRV_ENCODING) ")"
/* TODO: learn it from ES */
#define ESODBC_ELASTICSEARCH_VER	"7.x"
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
/* default security (TLS) setting */
#define ESODBC_DEF_SECURE			"no"
/* default global request timeout (0: no timeout) */
#define ESODBC_DEF_TIMEOUT			"0"
/* don't follow redirection from the server  */
#define ESODBC_DEF_FOLLOW			"yes"
/* packing of REST bodies (JSON or CBOR) */
#define ESODBC_DEF_PACKING			"JSON"
/* default tracing level */
#define ESODBC_DEF_TRACE_LEVEL		"WARN"


/*
 *
 * Driver/Elasticsearch capabilities
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
/* Read-only queries supported only */
#define ESODBC_DATA_SOURCE_READ_ONLY			"Y"
/* no support for transactions */
#define ESODBC_TXN_CAPABLE						SQL_TC_NONE
/* if asked (should not happen, since TXN_CAPABLE is NONE), all transaction
 * levels advertised, since read only operations are supported and no
 * transactions; this would need to be updated if updating ones will be
 * introduced. */
#define ESODBC_DEF_TXN_ISOLATION				(0 | \
		SQL_TXN_READ_UNCOMMITTED | SQL_TXN_READ_COMMITTED | \
		SQL_TXN_REPEATABLE_READ | SQL_TXN_SERIALIZABLE)

/* no schema support, but accepted (=currently ignored) by driver */
#define ESODBC_SCHEMA_USAGE						SQL_SU_PROCEDURE_INVOCATION
/*
 * Catalog support:
 * - supported in: DML STATEMENTS, ODBC PROCEDURE INVOCATION.
 * - not supported in: TABLE/INDEX/PRIVILEGE DEFINITION.
 */
#define ESODBC_CATALOG_USAGE					(0 | \
		SQL_CU_DML_STATEMENTS | SQL_CU_PROCEDURE_INVOCATION)
/* identifiers are case sensitive */
#define ESODBC_QUOTED_IDENTIFIER_CASE			SQL_IC_SENSITIVE
/* what's allowed in an identifier name (eg. table, column, index) except
 * [a-zA-Z0-9_], accepted in a delimited specification -> all printable ASCII
 * (assuming ASCII is the limitation?? TODO ), 0x20-0x7E. */
#define ESODBC_SPECIAL_CHARACTERS				" !\"#$%&'()*+,-./" /*[0-9]*/ \
		";<=>?@" /*[A-Z]*/ "[\\]^" /*[_]*/ "`" /*[a-z]*/ "{|}~"
/* is 'column AS name' accepted? */
#define ESODBC_COLUMN_ALIAS						"Y"
/* no procedures support */
#define ESODBC_PROCEDURES						"N"


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
#define ESODBC_SQL92_RELATIONAL_JOIN_OPERATORS	0
#define ESODBC_OJ_CAPABILITIES					0

/*
 * String functions support:
 * - supported: none.
 * - not supported: ASCII, BIT_LENGTH, CHAR, CHAR_LENGTH, CHARACTER_LENGTH,
 *   CONCAT, DIFFERENCE, INSERT, LCASE, LEFT, LENGTH, LOCATE, LTRIM,
 *   OCTET_LENGTH, POSITION, REPEAT, REPLACE, RIGHT, RTRIM, SOUNDEX, SPACE,
 *   SUBSTRING, UCASE.
 */
#define ESODBC_STRING_FUNCTIONS					0
/*
 * Numeric functions support:
 * - supported: ABS, ACOS, ASIN, ATAN, CEIL, COS, DEGREES, EXP, FLOOR, LOG,
 *   LOG10, PI, RADIANS, ROUND, SIN, SQRT, TAN;
 * - not supported: ATAN2, COT, MOD, POWER, RAND, SIGN, TRUNCATE.
 * Note: CEIL supported, CEILING not.
 */
#define ESODBC_NUMERIC_FUNCTIONS				(0 | \
		SQL_FN_NUM_ABS | SQL_FN_NUM_ACOS | SQL_FN_NUM_ASIN | SQL_FN_NUM_ATAN |\
		SQL_FN_NUM_CEILING | SQL_FN_NUM_COS | SQL_FN_NUM_DEGREES | \
		SQL_FN_NUM_EXP | SQL_FN_NUM_FLOOR | SQL_FN_NUM_LOG | \
		SQL_FN_NUM_LOG10 | SQL_FN_NUM_PI | SQL_FN_NUM_RADIANS | \
		SQL_FN_NUM_ROUND | SQL_FN_NUM_SIN | SQL_FN_NUM_SQRT | SQL_FN_NUM_TAN)
/*
 * Timedate functions support:
 * - supported: EXTRACT, HOUR, MINUTE, MONTH, SECOND, WEEK, YEAR;
 * - not supported: CURRENT_DATE, CURRENT_TIME, CURRENT_TIMESTAMP, CURDATE,
 *   CURTIME, DAYNAME, MONTHNAME, NOW, QUARTER, TIMESTAMPADD, TIMESTAMPDIFF.
 * Wrong `_` name?: DAYOFMONTH, DAYOFWEEK, DAYOFYEAR (mysql, db2, oracle)
 */
#define ESODBC_TIMEDATE_FUNCTIONS				(0 | \
		SQL_FN_TD_EXTRACT | SQL_FN_TD_HOUR | SQL_FN_TD_MINUTE | \
		SQL_FN_TD_MONTH | SQL_FN_TD_SECOND | SQL_FN_TD_WEEK | SQL_FN_TD_YEAR)
		//SQL_FN_TD_DAYOFMONTH | SQL_FN_TD_DAYOFWEEK | SQL_FN_TD_DAYOFYEAR
/*
 * TIMESTAMPDIFF timestamp intervals:
 * - supported: none.
 * - not supported: FRAC_SECOND, SECOND, MINUTE, HOUR, DAY, WEEK, MONTH,
 *   QUARTER, YEAR.
 */
#define ESODBC_TIMEDATE_DIFF_INTERVALS			0
/*
 * TIMESTAMPADD timestamp intervals:
 * - supported: none
 * - not supported: FRAC_SECOND, SECOND, MINUTE, HOUR, DAY, WEEK, MONTH,
 *   QUARTER, YEAR.
 */
#define ESODBC_TIMEDATE_ADD_INTERVALS			0
/*
 * System functions:
 * - supported: none.
 * - not supported: DATABASE, IFNULL, USER
 */
#define ESODBC_SYSTEM_FUNCTIONS					0
/*
 * Convert functions support:
 * - supported: CAST.
 * - not supported: CONVERT.
 */
#define ESODBC_CONVERT_FUNCTIONS				SQL_FN_CVT_CAST
/*
 * Aggregate functions support:
 * - supported: ALL, AVG, COUNT, MAX, MIN, SUM.
 * - not supported: DISTINCT.
 */
#define ESODBC_AGGREGATE_FUNCTIONS				(0 | \
		SQL_AF_ALL | SQL_AF_AVG | SQL_AF_COUNT | SQL_AF_MAX | SQL_AF_MIN | \
		SQL_AF_SUM)


/*** SQL92 support ***/

/*
 * SQL92 string functions:
 * - supported: none.
 * - not supported: CONVERT, LOWER, UPPER, SUBSTRING, TRANSLATE, TRIM, trim
 *   leading, trim trailing
 */
#define ESODBC_SQL92_STRING_FUNCTIONS			0
/*
 * SQL92 numeric value functions:
 * - supported: none.
 * - not supported: BIT_LENGHT, CHAR_LENGTH, CHARACTER_LENGTH, EXTRACT,
 *   OCTET_LENGTH, POSITION
 */
#define ESODBC_SQL92_NUMERIC_VALUE_FUNCTIONS	0
/*
 * SQL92 datetime functions:
 * - supported: none.
 * - not supported: CURRENT_DATE, CURRENT_TIME, CURRENT_DATETIME
 */
#define ESODBC_SQL92_DATETIME_FUNCTIONS			0
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
#define ODBC_SQL92_VALUE_EXPRESSIONS			0



#endif /* __DEFS_H__ */
