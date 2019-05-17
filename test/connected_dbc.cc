/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <assert.h>
#include <gtest/gtest.h>

extern "C" {
#include "util.h"
#include "defs.h"
}

#include "connected_dbc.h"

/*
 * Answer ES/SQL sends to SYS TYPES
 */
static const char systypes_answer[] = "\
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
}";

/* minimal, valid connection string */
static const SQLWCHAR connect_string[] = L"Driver=ElasticODBC";


/*
 * Class will provide a "connected" DBC: the ES types are loaded.
 */
ConnectedDBC::ConnectedDBC()
{
	SQLRETURN ret;
	cstr_st types;

	assert(getenv("TZ") == NULL);

	ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
	assert(SQL_SUCCEEDED(ret));

	ret = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION,
			(SQLPOINTER)SQL_OV_ODBC3_80, NULL);
	assert(SQL_SUCCEEDED(ret));

	ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
	assert(SQL_SUCCEEDED(ret));


	types.cnt = sizeof(systypes_answer) - 1;
	types.str = (SQLCHAR *)malloc(types.cnt);
	assert(types.str != NULL);
	memcpy(types.str, systypes_answer, types.cnt);

	ret = SQLDriverConnect(dbc, (SQLHWND)&types, (SQLWCHAR *)connect_string,
		sizeof(connect_string) / sizeof(connect_string[0]) - 1, NULL, 0, NULL,
		ESODBC_SQL_DRIVER_TEST);
	assert(SQL_SUCCEEDED(ret));

	ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
	assert(SQL_SUCCEEDED(ret));
	assert(stmt != NULL);
}

ConnectedDBC::~ConnectedDBC()
{
	SQLRETURN ret;

	ret = SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	assert(SQL_SUCCEEDED(ret));

	ret = SQLFreeHandle(SQL_HANDLE_DBC, dbc);
	assert(SQL_SUCCEEDED(ret));

	ret = SQLFreeHandle(SQL_HANDLE_ENV, env);
	assert(SQL_SUCCEEDED(ret));
}

void ConnectedDBC::assertState(SQLSMALLINT htype, const SQLWCHAR *state)
{
	SQLHANDLE hnd;
	SQLWCHAR buff[SQL_SQLSTATE_SIZE+1] = {L'\0'};
	SQLSMALLINT len;

	switch (htype) {
		case SQL_HANDLE_STMT: hnd = stmt; break;
		case SQL_HANDLE_DBC: hnd = dbc; break;
		case SQL_HANDLE_ENV: hnd = env; break;
		default: ASSERT_TRUE(FALSE);
	}
	ret = SQLGetDiagField(htype, hnd, 1, SQL_DIAG_SQLSTATE, buff,
			(SQL_SQLSTATE_SIZE + 1) * sizeof(buff[0]), &len);
	if (state) {
		if (! wcscmp(state, MK_WPTR("00000"))) {
			ASSERT_EQ(ret, SQL_NO_DATA);
		} else {
			ASSERT_TRUE(SQL_SUCCEEDED(ret));
			ASSERT_EQ(len, SQL_SQLSTATE_SIZE * sizeof(buff[0]));
			ASSERT_STREQ(buff, state);
		}
	} else {
		ASSERT_EQ(ret, SQL_NO_DATA);
	}
}

void ConnectedDBC::assertState(const SQLWCHAR *state)
{
	assertState(SQL_HANDLE_STMT, state);
}

void ConnectedDBC::assertRequest(const char *params, const char *tz)
{
	const static char *answ_templ = "{"
		JSON_KEY_QUERY "\"%s\""
		JSON_KEY_PARAMS "%s"
		JSON_KEY_MULTIVAL ESODBC_DEF_MFIELD_LENIENT
		JSON_KEY_IDX_FROZEN ESODBC_DEF_IDX_INC_FROZEN
		JSON_KEY_TIMEZONE "%s%s%s"
		JSON_KEY_VAL_MODE
		JSON_KEY_CLT_ID
		"}";
	char expect[1024];
	int n;

	cstr_st actual = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &actual);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	if (tz) {
		n = snprintf(expect, sizeof(expect), answ_templ, test_name, params,
				"\"", tz, "\"");
	} else {
		n = snprintf(expect, sizeof(expect), answ_templ, test_name, params,
				"", JSON_VAL_TIMEZONE_Z, "");
	}
	ASSERT_LT(actual.cnt, sizeof(expect));
	ASSERT_EQ(n, actual.cnt);
	ASSERT_EQ(strncmp(expect, (char *)actual.str, n), 0);

	free(actual.str);

}

void ConnectedDBC::assertRequest(const char *params)
{
	assertRequest(params, NULL);
}

void ConnectedDBC::prepareStatement()
{
	test_name =
		::testing::UnitTest::GetInstance()->current_test_info()->name();
	size_t nameLen = strlen(test_name);
	std::wstring wstr(nameLen, L' ');
	ASSERT_TRUE(mbstowcs(&wstr[0], test_name, nameLen + 1) != (size_t)-1);

	ret = ATTACH_SQL(stmt, &wstr[0], nameLen);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
}

void ConnectedDBC::prepareStatement(const SQLWCHAR *sql,
    const char *jsonAnswer)
{
	ret = ATTACH_SQL(stmt, sql, wcslen(sql));
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	char *answer = STRDUP(jsonAnswer);
	ASSERT_TRUE(answer != NULL);
	ret =  ATTACH_ANSWER(stmt, answer, strlen(answer));
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
}

void ConnectedDBC::prepareStatement(const char *jsonAnswer)
{
	prepareStatement();

	char *answer = STRDUP(jsonAnswer);
	ASSERT_TRUE(answer != NULL);
	ret =  ATTACH_ANSWER(stmt, answer, strlen(answer));
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=78 : */
