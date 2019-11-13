/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#ifndef __CONNECTED_DBC_H__
#define __CONNECTED_DBC_H__

extern "C" {
#if defined(_WIN32) || defined (WIN32)
#include <windows.h>
#include <tchar.h>
#endif /* _WIN32/WIN32 */

#include <sql.h>
#include <sqlext.h>

#include "queries.h"
} // extern C

#if defined(_WIN32) || defined (WIN32)
#define STRDUP	_strdup
#else /* _WIN32/WIN32 */
#define STRDUP	strdup
#endif /* _WIN32/WIN32 */

/* convenience casting macros */
#define ATTACH_ANSWER(_h, _s, _l)   attach_answer((esodbc_stmt_st *)_h, _s, _l)
#define ATTACH_SQL(_h, _s, _l)      attach_sql((esodbc_stmt_st *)_h, _s, _l)

#define ASSERT_CSTREQ(_c1, _c2) \
	do { \
		ASSERT_EQ(_c1.cnt, _c2.cnt); \
		ASSERT_EQ(strncmp((char *)_c1.str, (char *)_c2.str, _c1.cnt), 0); \
	} while (0)

/*
 * Answer ES/SQL sends to SYS TYPES
 * TODO: (Changes: MINIMUM_SCALE changed from 0 to MAXIMUM_SCALE for:
 * HALF_FLOAT, SCALED_FLOAT, FLOAT, DOUBLE.)
 */
#define SYSTYPES_ANSWER "\
{\
	\"columns\": [\
		{\"name\": \"TYPE_NAME\", \"type\": \"keyword\"},\
		{\"name\": \"DATA_TYPE\", \"type\": \"integer\"},\
		{\"name\": \"PRECISION\", \"type\": \"integer\"},\
		{\"name\": \"LITERAL_PREFIX\", \"type\": \"keyword\"},\
		{\"name\": \"LITERAL_SUFFIX\", \"type\": \"keyword\"},\
		{\"name\": \"CREATE_PARAMS\", \"type\": \"keyword\"},\
		{\"name\": \"NULLABLE\", \"type\": \"short\"},\
		{\"name\": \"CASE_SENSITIVE\", \"type\": \"boolean\"},\
		{\"name\": \"SEARCHABLE\", \"type\": \"short\"},\
		{\"name\": \"UNSIGNED_ATTRIBUTE\", \"type\": \"boolean\"},\
		{\"name\": \"FIXED_PREC_SCALE\", \"type\": \"boolean\"},\
		{\"name\": \"AUTO_INCREMENT\", \"type\": \"boolean\"},\
		{\"name\": \"LOCAL_TYPE_NAME\", \"type\": \"keyword\"},\
		{\"name\": \"MINIMUM_SCALE\", \"type\": \"short\"},\
		{\"name\": \"MAXIMUM_SCALE\", \"type\": \"short\"},\
		{\"name\": \"SQL_DATA_TYPE\", \"type\": \"integer\"},\
		{\"name\": \"SQL_DATETIME_SUB\", \"type\": \"integer\"},\
		{\"name\": \"NUM_PREC_RADIX\", \"type\": \"integer\"},\
		{\"name\": \"INTERVAL_PRECISION\", \"type\": \"integer\"}\
	  ],\
	\"rows\": [\
		[\"BYTE\", -6, 3, \"'\", \"'\", null, 2, false, 3, false, false,\
			false, null, 0, 0, -6, 0, 10, null],\
		[\"LONG\", -5, 19, \"'\", \"'\", null, 2, false, 3, false, false,\
			false, null, 0, 0, -5, 0, 10, null],\
		[\"BINARY\", -3, 2147483647, \"'\", \"'\", null, 2, false, 3, true,\
			false, false, null, null, null, -3, 0, null, null],\
		[\"NULL\", 0, 0, \"'\", \"'\", null, 2, false, 3, true, false, false,\
			null, null, null, 0, 0, null, null],\
		[\"INTEGER\", 4, 10, \"'\", \"'\", null, 2, false, 3, false, false,\
			false, null, 0, 0, 4, 0, 10, null],\
		[\"SHORT\", 5, 5, \"'\", \"'\", null, 2, false, 3, false, false,\
			false, null, 0, 0, 5, 0, 10, null],\
		[\"HALF_FLOAT\", 6, 16, \"'\", \"'\", null, 2, false, 3, false, false,\
			false, null, 0, 16, 6, 0, 2, null],\
		[\"SCALED_FLOAT\", 6, 19, \"'\", \"'\", null, 2, false, 3, false,\
			false, false, null, 0, 19, 6, 0, 2, null],\
		[\"FLOAT\", 7, 7, \"'\", \"'\", null, 2, false, 3, false, false,\
			false, null, 0, 7, 7, 0, 2, null],\
		[\"DOUBLE\", 8, 15, \"'\", \"'\", null, 2, false, 3, false, false,\
			false, null, 0, 15, 8, 0, 2, null],\
		[\"KEYWORD\", 12, 256, \"'\", \"'\", null, 2, true, 3, true, false,\
			false, null, null, null, 12, 0, null, null],\
		[\"TEXT\", 12, 2147483647, \"'\", \"'\", null, 2, true, 3, true,\
			false, false, null, null, null, 12, 0, null, null],\
		[\"BOOLEAN\", 16, 1, \"'\", \"'\", null, 2, false, 3, true, false,\
			false, null, null, null, 16, 0, null, null],\
		[\"DATE\", 91, 10, \"'\", \"'\", null, 2, false, 3, true, false,\
			false, null, null, null, 91, 0, null, null],\
		[\"DATETIME\", 93, 24, \"'\", \"'\", null, 2, false, 3, true, false,\
			false, null, 3, 3, 9, 3, null, null],\
		[\"INTERVAL_YEAR\", 101, 7, \"'\", \"'\", null, 2, false, 3, true,\
			false, false, null, null, null, 101, 0, null, null],\
		[\"INTERVAL_MONTH\", 102, 7, \"'\", \"'\", null, 2, false, 3, true,\
			false, false, null, null, null, 102, 0, null, null],\
		[\"INTERVAL_DAY\", 103, 23, \"'\", \"'\", null, 2, false, 3, true,\
			false, false, null, null, null, 103, 0, null, null],\
		[\"INTERVAL_HOUR\", 104, 23, \"'\", \"'\", null, 2, false, 3, true,\
			false, false, null, null, null, 104, 0, null, null],\
		[\"INTERVAL_MINUTE\", 105, 23, \"'\", \"'\", null, 2, false, 3, true,\
			false, false, null, null, null, 105, 0, null, null],\
		[\"INTERVAL_SECOND\", 106, 23, \"'\", \"'\", null, 2, false, 3, true,\
			false, false, null, null, null, 106, 0, null, null],\
		[\"INTERVAL_YEAR_TO_MONTH\", 107, 7, \"'\", \"'\", null, 2, false, 3,\
			true, false, false, null, null, null, 107, 0, null, null],\
		[\"INTERVAL_DAY_TO_HOUR\", 108, 23, \"'\", \"'\", null, 2, false, 3,\
			true, false, false, null, null, null, 108, 0, null, null],\
		[\"INTERVAL_DAY_TO_MINUTE\", 109, 23, \"'\", \"'\", null, 2, false, 3,\
			true, false, false, null, null, null, 109, 0, null, null],\
		[\"INTERVAL_DAY_TO_SECOND\", 110, 23, \"'\", \"'\", null, 2, false, 3,\
			true, false, false, null, null, null, 110, 0, null, null],\
		[\"INTERVAL_HOUR_TO_MINUTE\", 111, 23, \"'\", \"'\", null, 2, false,\
			3, true, false, false, null, null, null, 111, 0, null, null],\
		[\"INTERVAL_HOUR_TO_SECOND\", 112, 23, \"'\", \"'\", null, 2, false,\
			3, true, false, false, null, null, null, 112, 0, null, null],\
		[\"INTERVAL_MINUTE_TO_SECOND\", 113, 23, \"'\", \"'\", null, 2, false,\
			3, true, false, false, null, null, null, 113, 0, null, null],\
		[\"UNSUPPORTED\", 1111, 0, \"'\", \"'\", null, 2, false, 3, true,\
			false, false, null, null, null, 1111, 0, null, null],\
		[\"OBJECT\", 2002, 0, \"'\", \"'\", null, 2, false, 3, true, false,\
			false, null, null, null, 2002, 0, null, null],\
		[\"NESTED\", 2002, 0, \"'\", \"'\", null, 2, false, 3, true, false,\
			false, null, null, null, 2002, 0, null, null]\
	]\
}"

/* minimal, valid connection string */
#define CONNECT_STRING L"Driver=ElasticODBC"

class ConnectedDBC {
	protected:
		SQLHANDLE env, dbc, stmt;
		SQLRETURN ret;
		SQLLEN ind_len = SQL_NULL_DATA;


	ConnectedDBC();
	virtual ~ConnectedDBC();

	void assertState(const SQLWCHAR *state);
	void assertState(SQLSMALLINT htype, const SQLWCHAR *state);

	// use the test name as SQL (for faster logs lookup)
	void prepareStatement();
	// use an actual SQL statement (if it might be processed)
	void prepareStatement(const SQLWCHAR *sql, const char *jsonAnswer);
	// use test name as SQL and attach given answer
	void prepareStatement(const char *jsonAnswer);
};

#endif /* __CONNECTED_DBC_H__ */
