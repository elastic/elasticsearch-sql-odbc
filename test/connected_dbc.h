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
#include "convert.h"
} // extern C

#if defined(_WIN32) || defined (WIN32)
#define STRDUP	_strdup
#else /* _WIN32/WIN32 */
#define STRDUP	strdup
#endif /* _WIN32/WIN32 */

/* convenience casting macros */
#define ATTACH_ANSWER(_h, _s)	attach_answer((esodbc_stmt_st *)_h, _s, TRUE)
#define ATTACH_SQL(_h, _s, _l)	attach_sql((esodbc_stmt_st *)_h, _s, _l)

#define ASSERT_CSTREQ(_c1, _c2) \
	do { \
		ASSERT_EQ(_c1.cnt, _c2.cnt); \
		ASSERT_EQ(strncmp((char *)_c1.str, (char *)_c2.str, _c1.cnt), 0); \
	} while (0)

/*
 * Answer ES/SQL sends to SYS TYPES
 */
#define SYSTYPES_ANSWER "\
{\
	\"columns\":[\
		{\"name\":\"TYPE_NAME\",\"type\":\"keyword\",\"display_size\":32766},\
		{\"name\":\"DATA_TYPE\",\"type\":\"integer\",\"display_size\":11},\
		{\"name\":\"PRECISION\",\"type\":\"integer\",\"display_size\":11},\
		{\"name\":\"LITERAL_PREFIX\",\"type\":\"keyword\",\"display_size\":32766},\
		{\"name\":\"LITERAL_SUFFIX\",\"type\":\"keyword\",\"display_size\":32766},\
		{\"name\":\"CREATE_PARAMS\",\"type\":\"keyword\",\"display_size\":32766},\
		{\"name\":\"NULLABLE\",\"type\":\"short\",\"display_size\":6},\
		{\"name\":\"CASE_SENSITIVE\",\"type\":\"boolean\",\"display_size\":1},\
		{\"name\":\"SEARCHABLE\",\"type\":\"short\",\"display_size\":6},\
		{\"name\":\"UNSIGNED_ATTRIBUTE\",\"type\":\"boolean\",\"display_size\":1},\
		{\"name\":\"FIXED_PREC_SCALE\",\"type\":\"boolean\",\"display_size\":1},\
		{\"name\":\"AUTO_INCREMENT\",\"type\":\"boolean\",\"display_size\":1},\
		{\"name\":\"LOCAL_TYPE_NAME\",\"type\":\"keyword\",\"display_size\":32766},\
		{\"name\":\"MINIMUM_SCALE\",\"type\":\"short\",\"display_size\":6},\
		{\"name\":\"MAXIMUM_SCALE\",\"type\":\"short\",\"display_size\":6},\
		{\"name\":\"SQL_DATA_TYPE\",\"type\":\"integer\",\"display_size\":11},\
		{\"name\":\"SQL_DATETIME_SUB\",\"type\":\"integer\",\"display_size\":11},\
		{\"name\":\"NUM_PREC_RADIX\",\"type\":\"integer\",\"display_size\":11},\
		{\"name\":\"INTERVAL_PRECISION\",\"type\":\"integer\",\"display_size\":11}\
	],\
	\"rows\":[\
		[\"BYTE\",-6,3,\"'\",\"'\",null,2,false,3,false,false,false,null,0,0,-6,0,10,null],\
		[\"LONG\",-5,19,\"'\",\"'\",null,2,false,3,false,false,false,null,0,0,-5,0,10,null],\
		[\"BINARY\",-3,2147483647,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,-3,0,null,null],\
		[\"NULL\",0,0,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,0,0,null,null],\
		[\"INTEGER\",4,10,\"'\",\"'\",null,2,false,3,false,false,false,null,0,0,4,0,10,null],\
		[\"SHORT\",5,5,\"'\",\"'\",null,2,false,3,false,false,false,null,0,0,5,0,10,null],\
		[\"HALF_FLOAT\",6,3,\"'\",\"'\",null,2,false,3,false,false,false,null,3,3,6,0,2,null],\
		[\"FLOAT\",7,7,\"'\",\"'\",null,2,false,3,false,false,false,null,7,7,7,0,2,null],\
		[\"DOUBLE\",8,15,\"'\",\"'\",null,2,false,3,false,false,false,null,15,15,8,0,2,null],\
		[\"SCALED_FLOAT\",8,15,\"'\",\"'\",null,2,false,3,false,false,false,null,15,15,8,0,2,null],\
		[\"KEYWORD\",12,32766,\"'\",\"'\",null,2,true,3,true,false,false,null,null,null,12,0,null,null],\
		[\"TEXT\",12,2147483647,\"'\",\"'\",null,2,true,3,true,false,false,null,null,null,12,0,null,null],\
		[\"IP\",12,0,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,12,0,null,null],\
		[\"BOOLEAN\",16,1,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,16,0,null,null],\
		[\"DATE\",91,29,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,91,1,null,null],\
		[\"TIME\",92,18,\"'\",\"'\",null,2,false,3,true,false,false,null,3,3,92,2,null,null],\
		[\"DATETIME\",93,29,\"'\",\"'\",null,2,false,3,true,false,false,null,3,3,9,3,null,null],\
		[\"INTERVAL_YEAR\",101,7,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,101,0,null,null],\
		[\"INTERVAL_MONTH\",102,7,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,102,0,null,null],\
		[\"INTERVAL_DAY\",103,23,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,103,0,null,null],\
		[\"INTERVAL_HOUR\",104,23,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,104,0,null,null],\
		[\"INTERVAL_MINUTE\",105,23,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,105,0,null,null],\
		[\"INTERVAL_SECOND\",106,23,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,106,0,null,null],\
		[\"INTERVAL_YEAR_TO_MONTH\",107,7,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,107,0,null,null],\
		[\"INTERVAL_DAY_TO_HOUR\",108,23,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,108,0,null,null],\
		[\"INTERVAL_DAY_TO_MINUTE\",109,23,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,109,0,null,null],\
		[\"INTERVAL_DAY_TO_SECOND\",110,23,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,110,0,null,null],\
		[\"INTERVAL_HOUR_TO_MINUTE\",111,23,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,111,0,null,null],\
		[\"INTERVAL_HOUR_TO_SECOND\",112,23,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,112,0,null,null],\
		[\"INTERVAL_MINUTE_TO_SECOND\",113,23,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,113,0,null,null],\
		[\"UNSUPPORTED\",1111,0,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,1111,0,null,null],\
		[\"OBJECT\",2002,0,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,2002,0,null,null],\
		[\"NESTED\",2002,0,\"'\",\"'\",null,2,false,3,true,false,false,null,null,null,2002,0,null,null]\
	]\
}"

/* minimal, valid connection string */
#define CONNECT_STRING L"Driver=ElasticODBC"

class ConnectedDBC {
	protected:
		SQLHANDLE env, dbc, stmt;
		SQLRETURN ret;
		SQLLEN ind_len = SQL_NULL_DATA;
		const char *test_name;


	ConnectedDBC();
	virtual ~ConnectedDBC();

	void assertState(const SQLWCHAR *state);
	void assertState(SQLSMALLINT htype, const SQLWCHAR *state);

	void assertRequest(const char *params, const char *tz);
	void assertRequest(const char *params);

	// use the test name as SQL (for faster logs lookup)
	void prepareStatement();
	// use an actual SQL statement (if it might be processed)
	void prepareStatement(const SQLWCHAR *sql);
	// use an actual SQL statement (if it might be processed)
	void prepareStatement(const SQLWCHAR *sql, const char *jsonAnswer);
	// use test name as SQL and attach given answer
	void prepareStatement(const char *jsonAnswer);
};

#endif /* __CONNECTED_DBC_H__ */
