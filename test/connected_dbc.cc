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
 * Class will provide a "connected" DBC: the ES types are loaded.
 */
ConnectedDBC::ConnectedDBC()
{
	cstr_st types = {0};

	ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
	assert(SQL_SUCCEEDED(ret));

	ret = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION,
			(SQLPOINTER)SQL_OV_ODBC3_80, NULL);
	assert(SQL_SUCCEEDED(ret));

	ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
	assert(SQL_SUCCEEDED(ret));

	types.str = (SQLCHAR *)strdup(SYSTYPES_ANSWER);
	assert(types.str != NULL);
	types.cnt = sizeof(SYSTYPES_ANSWER) - 1;

	ret = SQLDriverConnect(dbc, (SQLHWND)&types, (SQLWCHAR *)CONNECT_STRING,
		sizeof(CONNECT_STRING) / sizeof(CONNECT_STRING[0]) - 1, NULL, 0, NULL,
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
