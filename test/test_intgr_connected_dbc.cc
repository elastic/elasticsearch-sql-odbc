/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

extern "C" {
#if defined(_WIN32) || defined (WIN32)
#include <windows.h>
//#include <tchar.h>
#endif /* _WIN32/WIN32 */

#include <sql.h>
#include <sqlext.h>
} // extern C

#include <gtest/gtest.h>


namespace test {

class IntgrConnectedDBC : public ::testing::Test {
	protected:
	SQLHANDLE env, dbc, stmt;
	SQLRETURN ret;

	void connect(SQLWCHAR *dsn, SQLWCHAR *uid, SQLWCHAR *pwd) {

		ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
		assert(SQL_SUCCEEDED(ret));

		ret = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION,
			(SQLPOINTER)SQL_OV_ODBC3_80, NULL);
		assert(SQL_SUCCEEDED(ret));

		ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
		assert(SQL_SUCCEEDED(ret));

		ret = SQLConnect(dbc, dsn, SQL_NTS, uid, SQL_NTS, pwd, SQL_NTS);
		assert(SQL_SUCCEEDED(ret));
	}

};

TEST_F(IntgrConnectedDBC, test_connect) {

	connect(L"Localhost_Auth_Clear", L"elastic", L"elastic");

	ret = SQLDisconnect(dbc);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
}


} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
