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
	static char buff[sizeof(STR(DRV_VERSION))] = {0};
	char *ptr;

	assert(getenv("TZ") == NULL);

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

	version = STR(DRV_VERSION);
	if ((ptr = strchr(version, '-'))) {
		strncpy(buff, STR(DRV_VERSION), ptr - version);
		version = buff;
	}
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
	const static char *answ_templ_no_params = "{"
		JSON_KEY_QUERY "\"%s\""
		/* params */ "%s"
		JSON_KEY_MULTIVAL ESODBC_DEF_MFIELD_LENIENT
		JSON_KEY_IDX_FROZEN ESODBC_DEF_IDX_INC_FROZEN
		JSON_KEY_TIMEZONE "%s%s%s"
		JSON_KEY_VERSION "\"%s\""
		JSON_KEY_VAL_MODE
		JSON_KEY_CLT_ID
		JSON_KEY_BINARY_FMT "false"
		"}";
	const static char *answ_templ = "{"
		JSON_KEY_QUERY "\"%s\""
		JSON_KEY_PARAMS "%s"
		JSON_KEY_MULTIVAL ESODBC_DEF_MFIELD_LENIENT
		JSON_KEY_IDX_FROZEN ESODBC_DEF_IDX_INC_FROZEN
		JSON_KEY_TIMEZONE "%s%s%s"
		JSON_KEY_VERSION "\"%s\""
		JSON_KEY_VAL_MODE
		JSON_KEY_CLT_ID
		JSON_KEY_BINARY_FMT "false"
		"}";
	const char *templ;
	char expect[1024];
	int n;

	cstr_st actual = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &actual);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	if (params) {
		templ = answ_templ;
	} else {
		templ = answ_templ_no_params;
		params = "";
	}

	if (tz) {
		n = snprintf(expect, sizeof(expect), templ, test_name, params,
				"\"", tz, "\"", version);
	} else {
		n = snprintf(expect, sizeof(expect), templ, test_name, params,
				"", JSON_VAL_TIMEZONE_Z, "", version);
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

void ConnectedDBC::prepareStatement(const SQLWCHAR *sql)
{
	ret = ATTACH_SQL(stmt, sql, wcslen(sql));
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
}

void ConnectedDBC::prepareStatement(const SQLWCHAR *sql,
    const char *jsonAnswer)
{
	ret = ATTACH_SQL(stmt, sql, wcslen(sql));
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st answer = {
		(SQLCHAR *)STRDUP(jsonAnswer),
		strlen(jsonAnswer)
	};
	ASSERT_TRUE(answer.str != NULL);
	ret =  ATTACH_ANSWER(stmt, &answer);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
}

void ConnectedDBC::prepareStatement(const char *jsonAnswer)
{
	prepareStatement();

	cstr_st answer = {
		(SQLCHAR *)STRDUP(jsonAnswer),
		strlen(jsonAnswer)
	};
	ASSERT_TRUE(answer.str != NULL);
	ret =  ATTACH_ANSWER(stmt, &answer);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=78 : */
