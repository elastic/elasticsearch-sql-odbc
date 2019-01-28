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
 * TODO: (Changes: MINIMUM_SCALE changed from 0 to MAXIMUM_SCALE for:
 * HALF_FLOAT, SCALED_FLOAT, FLOAT, DOUBLE.)
 */
static const char systypes_answer[] = "\
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
}\
";

/* minimal, valid connection string */
static const SQLWCHAR connect_string[] = L"Driver=ElasticODBC";


/*
 * Class will provide a "connected" DBC: the ES types are loaded.
 */
ConnectedDBC::ConnectedDBC()
{
	SQLRETURN ret;
	cstr_st types;

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

void ConnectedDBC::prepareStatement()
{
	const char *testName =
		::testing::UnitTest::GetInstance()->current_test_info()->name();
	size_t nameLen = strlen(testName);
	std::wstring wstr(nameLen, L' ');
	ASSERT_TRUE(mbstowcs(&wstr[0], testName, nameLen + 1) != (size_t)-1);

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
