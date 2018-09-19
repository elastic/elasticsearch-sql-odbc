/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <gtest/gtest.h>
#include "connected_dbc.h"

#include <string.h>


namespace test {

class ConversionCompatibility : public ::testing::Test, public ConnectedDBC {
};

static BOOL is_interval(SQLSMALLINT type)
{
	switch (type) { /* SQL and SQL C interval concise types have the same vals */
		case SQL_C_INTERVAL_MONTH:
		case SQL_C_INTERVAL_YEAR:
		case SQL_C_INTERVAL_YEAR_TO_MONTH:
		case SQL_C_INTERVAL_DAY:
		case SQL_C_INTERVAL_HOUR:
		case SQL_C_INTERVAL_MINUTE:
		case SQL_C_INTERVAL_SECOND:
		case SQL_C_INTERVAL_DAY_TO_HOUR:
		case SQL_C_INTERVAL_DAY_TO_MINUTE:
		case SQL_C_INTERVAL_DAY_TO_SECOND:
		case SQL_C_INTERVAL_HOUR_TO_MINUTE:
		case SQL_C_INTERVAL_HOUR_TO_SECOND:
		case SQL_C_INTERVAL_MINUTE_TO_SECOND:
			return TRUE;
	}
	return FALSE;
}

/* implements the rotation of the matrix in:
 * https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/converting-data-from-c-to-sql-data-types */
static BOOL conv_supported(SQLSMALLINT sqltype, SQLSMALLINT ctype)
{

	/* interval types not yet implemented */
	if (is_interval(sqltype) || is_interval(ctype))
		return FALSE;
	/* GUID type not yet implemented */
	if (sqltype == SQL_GUID || ctype == SQL_GUID)
		return FALSE;

	switch (ctype) {
		/* application will use implementation's type (irec's) */
		case SQL_C_DEFAULT:
		/* anything's convertible to [w]char & binary */
		case SQL_C_CHAR:
		case SQL_C_WCHAR:
		case SQL_C_BINARY:
			return TRUE;

		case SQL_C_GUID:
			return sqltype == ESODBC_SQL_NULL || sqltype == SQL_GUID;
	}

	switch (sqltype) {
		case ESODBC_SQL_NULL:
		case SQL_CHAR:
		case SQL_VARCHAR:
		case SQL_LONGVARCHAR:
		case SQL_WCHAR:
		case SQL_WVARCHAR:
		case SQL_WLONGVARCHAR:
			break; /* it's not SQL_C_GUID, checked for above */

		case SQL_DECIMAL:
		case SQL_NUMERIC:
		case SQL_SMALLINT:
		case SQL_INTEGER:
		case SQL_TINYINT:
		case SQL_BIGINT:
			switch (ctype) {
				case SQL_C_TYPE_DATE:
				case SQL_C_TYPE_TIME:
				case SQL_C_TYPE_TIMESTAMP:
				case SQL_C_GUID:
					return FALSE;
			}
			break;

		case SQL_BIT:
		case ESODBC_SQL_BOOLEAN:
		case SQL_REAL:
		case SQL_FLOAT:
		case SQL_DOUBLE:
			switch (ctype) {
				case SQL_C_TYPE_DATE:
				case SQL_C_TYPE_TIME:
				case SQL_C_TYPE_TIMESTAMP:
				case SQL_C_GUID:

				case SQL_C_INTERVAL_MONTH:
				case SQL_C_INTERVAL_YEAR:
				case SQL_C_INTERVAL_YEAR_TO_MONTH:
				case SQL_C_INTERVAL_DAY:
				case SQL_C_INTERVAL_HOUR:
				case SQL_C_INTERVAL_MINUTE:
				case SQL_C_INTERVAL_SECOND:
				case SQL_C_INTERVAL_DAY_TO_HOUR:
				case SQL_C_INTERVAL_DAY_TO_MINUTE:
				case SQL_C_INTERVAL_DAY_TO_SECOND:
				case SQL_C_INTERVAL_HOUR_TO_MINUTE:
				case SQL_C_INTERVAL_HOUR_TO_SECOND:
				case SQL_C_INTERVAL_MINUTE_TO_SECOND:
					return FALSE;
			}
			break;

		case SQL_BINARY:
		case SQL_VARBINARY:
		case SQL_LONGVARBINARY:
			return FALSE; /* it's not SQL_C_BINARY, checked for above */


		case SQL_TYPE_DATE:
			switch (ctype) {
				case SQL_C_TYPE_DATE:
				case SQL_C_TYPE_TIMESTAMP:
					return TRUE;
			}
			return FALSE;
		case SQL_TYPE_TIME:
			switch (ctype) {
				case SQL_C_TYPE_TIME:
				case SQL_C_TYPE_TIMESTAMP:
					return TRUE;
			}
			return FALSE;
		case SQL_TYPE_TIMESTAMP:
			switch (ctype) {
				case SQL_C_TYPE_DATE:
				case SQL_C_TYPE_TIME:
				case SQL_C_TYPE_TIMESTAMP:
					return TRUE;
			}
			return FALSE;

		case SQL_INTERVAL_MONTH:
		case SQL_INTERVAL_YEAR:
		case SQL_INTERVAL_YEAR_TO_MONTH:
		case SQL_INTERVAL_DAY:
		case SQL_INTERVAL_HOUR:
		case SQL_INTERVAL_MINUTE:
		case SQL_INTERVAL_SECOND:
		case SQL_INTERVAL_DAY_TO_HOUR:
		case SQL_INTERVAL_DAY_TO_MINUTE:
		case SQL_INTERVAL_DAY_TO_SECOND:
		case SQL_INTERVAL_HOUR_TO_MINUTE:
		case SQL_INTERVAL_HOUR_TO_SECOND:
		case SQL_INTERVAL_MINUTE_TO_SECOND:
			switch (ctype) {
				case SQL_C_BIT:
				case SQL_C_NUMERIC:
				case SQL_C_TINYINT:
				case SQL_C_STINYINT:
				case SQL_C_UTINYINT:
				case SQL_C_SBIGINT:
				case SQL_C_UBIGINT:
				case SQL_C_SHORT:
				case SQL_C_SSHORT:
				case SQL_C_USHORT:
				case SQL_C_LONG:
				case SQL_C_SLONG:
				case SQL_C_ULONG:
					return TRUE;
			}
			return FALSE;

		case SQL_GUID:
			switch (ctype) {
				case SQL_C_CHAR:
				case SQL_C_WCHAR:
				case SQL_C_BINARY:
					break;
				default:
					return FALSE;
			}

	}
	return TRUE;
}


TEST_F(ConversionCompatibility, ConvCompat)
{
	SQLSMALLINT sql_types[] = {ESODBC_SQL_NULL, SQL_CHAR, SQL_VARCHAR,
		SQL_LONGVARCHAR, SQL_WCHAR, SQL_WVARCHAR, SQL_WLONGVARCHAR,
		SQL_DECIMAL, SQL_NUMERIC, SQL_SMALLINT, SQL_INTEGER, SQL_REAL,
		SQL_FLOAT, SQL_DOUBLE, SQL_BIT, ESODBC_SQL_BOOLEAN, SQL_TINYINT,
		SQL_BIGINT, SQL_BINARY, SQL_VARBINARY, SQL_LONGVARBINARY,
		SQL_TYPE_DATE, SQL_TYPE_TIME, SQL_TYPE_TIMESTAMP, SQL_INTERVAL_MONTH,
		SQL_INTERVAL_YEAR, SQL_INTERVAL_YEAR_TO_MONTH, SQL_INTERVAL_DAY,
		SQL_INTERVAL_HOUR, SQL_INTERVAL_MINUTE, SQL_INTERVAL_SECOND,
		SQL_INTERVAL_DAY_TO_HOUR, SQL_INTERVAL_DAY_TO_MINUTE,
		SQL_INTERVAL_DAY_TO_SECOND, SQL_INTERVAL_HOUR_TO_MINUTE,
		SQL_INTERVAL_HOUR_TO_SECOND, SQL_INTERVAL_MINUTE_TO_SECOND, SQL_GUID
		};
	SQLSMALLINT csql_types[] = {SQL_C_DEFAULT, SQL_C_CHAR, SQL_C_WCHAR,
			SQL_C_SHORT, SQL_C_SSHORT, SQL_C_USHORT, SQL_C_LONG, SQL_C_SLONG,
			SQL_C_ULONG, SQL_C_FLOAT, SQL_C_DOUBLE, SQL_C_BIT, SQL_C_TINYINT,
			SQL_C_STINYINT, SQL_C_UTINYINT, SQL_C_SBIGINT, SQL_C_UBIGINT,
			SQL_C_BINARY, SQL_C_BOOKMARK, SQL_C_VARBOOKMARK, SQL_C_TYPE_DATE,
			SQL_C_TYPE_TIME, SQL_C_TYPE_TIMESTAMP, SQL_C_NUMERIC, SQL_C_GUID,
			SQL_C_INTERVAL_DAY, SQL_C_INTERVAL_HOUR, SQL_C_INTERVAL_MINUTE,
			SQL_C_INTERVAL_SECOND, SQL_C_INTERVAL_DAY_TO_HOUR,
			SQL_C_INTERVAL_DAY_TO_MINUTE, SQL_C_INTERVAL_DAY_TO_SECOND,
			SQL_C_INTERVAL_HOUR_TO_MINUTE, SQL_C_INTERVAL_HOUR_TO_SECOND,
			SQL_C_INTERVAL_MINUTE_TO_SECOND, SQL_C_INTERVAL_MONTH,
			SQL_C_INTERVAL_YEAR, SQL_C_INTERVAL_YEAR_TO_MONTH
		};

	prepareStatement();

	SQLLEN indlen = SQL_NULL_DATA;
	for (SQLSMALLINT i = 0; i < sizeof(sql_types)/sizeof(*sql_types); i ++) {
		for (SQLSMALLINT j = 0; j < sizeof(csql_types)/sizeof(*csql_types); j ++) {
			SQLSMALLINT sql = sql_types[i];
			SQLSMALLINT csql = csql_types[j];

			ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, csql, sql,
				/*size*/0, /*decdigits*/0, NULL, sizeof(NULL), &indlen);
			BOOL conv = conv_supported(sql, csql);
			ASSERT_TRUE((!SQL_SUCCEEDED(ret) && !conv) || 
					(SQL_SUCCEEDED(ret) && conv));
			//ASSERT_TRUE((! SQL_SUCCEEDED(ret)) ^ conv_supported(sql, csql));
			//ASSERT_TRUE(((SQL_SUCCEEDED(ret)) && conv_supported(sql, csql)) || 
			//		(! SQL_SUCCEEDED(ret)));
		}
	}
}

} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
