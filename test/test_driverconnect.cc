/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <gtest/gtest.h>
#include "connected_dbc.h"

/*  */
extern "C" {
#include "catalogue.h" /* set_current_catalog() */
}

namespace test {

class DriverConnect : public ::testing::Test, public ConnectedDBC
{
	protected:
		cstr_st types = {0};
		SQLHANDLE my_dbc;
		SQLSMALLINT out_avail = -1;

	void SetUp() override
	{
		ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &my_dbc);
		ASSERT_TRUE(SQL_SUCCEEDED(ret));

		types.str = (SQLCHAR *)strdup(SYSTYPES_ANSWER);
		ASSERT_TRUE(types.str != NULL);
		types.cnt = sizeof(SYSTYPES_ANSWER) - 1;
	}

	void TearDown() override
	{
		ret = SQLFreeHandle(SQL_HANDLE_DBC, my_dbc);
		ASSERT_TRUE(SQL_SUCCEEDED(ret));
	}
};

TEST_F(DriverConnect, OutputCount)
{
	ret = SQLDriverConnect(my_dbc, (SQLHWND)&types, (SQLWCHAR *)CONNECT_STRING,
		sizeof(CONNECT_STRING) / sizeof(CONNECT_STRING[0]) - 1, NULL, 0,
		&out_avail, ESODBC_SQL_DRIVER_TEST);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(0 < out_avail);
}

TEST_F(DriverConnect, OutputZeroTerm)
{
	static const size_t buff_sz = 1024;
	SQLWCHAR out_buff[buff_sz];

	ret = SQLDriverConnect(my_dbc, (SQLHWND)&types, (SQLWCHAR *)CONNECT_STRING,
		sizeof(CONNECT_STRING) / sizeof(CONNECT_STRING[0]) - 1,
		out_buff, buff_sz, &out_avail, ESODBC_SQL_DRIVER_TEST);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(out_avail < buff_sz);
	ASSERT_EQ(out_buff[out_avail], (SQLWCHAR)L'\0');

}

TEST_F(DriverConnect, OutputTruncated)
{
	static const size_t buff_sz = 3;
	SQLWCHAR out_buff[buff_sz];

	ret = SQLDriverConnect(my_dbc, (SQLHWND)&types, (SQLWCHAR *)CONNECT_STRING,
		sizeof(CONNECT_STRING) / sizeof(CONNECT_STRING[0]) - 1,
		out_buff, buff_sz, &out_avail, ESODBC_SQL_DRIVER_TEST);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(buff_sz < out_avail);
	ASSERT_EQ(out_buff[buff_sz - 1], (SQLWCHAR)L'\0');

}

TEST_F(DriverConnect, ReinitCurrentCatalog)
{
	ret = SQLDriverConnect(my_dbc, (SQLHWND)&types, (SQLWCHAR *)CONNECT_STRING,
		sizeof(CONNECT_STRING) / sizeof(CONNECT_STRING[0]) - 1, NULL, 0,
		&out_avail, ESODBC_SQL_DRIVER_TEST);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	esodbc_dbc_st *dbc = (esodbc_dbc_st *)my_dbc;
	wstr_st crr_cat = WSTR_INIT("crr_catalog");
	ASSERT_TRUE(set_current_catalog(dbc, &crr_cat));
	ASSERT_FALSE(set_current_catalog(dbc, &crr_cat));

	wstr_st other_cat = WSTR_INIT("other_catalog");
	ASSERT_TRUE(set_current_catalog(dbc, &other_cat));
}

TEST_F(DriverConnect, ResetCurrentCatalog)
{
	ret = SQLDriverConnect(my_dbc, (SQLHWND)&types, (SQLWCHAR *)CONNECT_STRING,
		sizeof(CONNECT_STRING) / sizeof(CONNECT_STRING[0]) - 1, NULL, 0,
		&out_avail, ESODBC_SQL_DRIVER_TEST);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	esodbc_dbc_st *dbc = (esodbc_dbc_st *)my_dbc;
	wstr_st crr_cat = WSTR_INIT("crr_catalog");
	ASSERT_TRUE(set_current_catalog(dbc, &crr_cat));
	ASSERT_TRUE(SQL_SUCCEEDED(SQLSetConnectAttrW(my_dbc,
					SQL_ATTR_CURRENT_CATALOG, (SQLPOINTER)crr_cat.str,
					(SQLINTEGER)crr_cat.cnt)));

	wstr_st other_cat = WSTR_INIT("other_catalog");
	ASSERT_FALSE(SQL_SUCCEEDED(SQLSetConnectAttrW(my_dbc,
					SQL_ATTR_CURRENT_CATALOG, (SQLPOINTER)other_cat.str,
					(SQLINTEGER)other_cat.cnt)));
}

} // test namespace

