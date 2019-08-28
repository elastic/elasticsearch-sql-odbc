/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <gtest/gtest.h>
#include "connected_dbc.h"

namespace test {

class ConvertC2SQL_Null : public ::testing::Test, public ConnectedDBC {
};


TEST_F(ConvertC2SQL_Null, CStr2Boolean_null)
{
	prepareStatement();

	SQLCHAR val[] = "1";
	SQLLEN osize = SQL_NULL_DATA;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
			ESODBC_SQL_BOOLEAN, /*size*/0, /*decdigits*/0, val,
			sizeof(val) - /*\0*/1, &osize);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"BOOLEAN\", \"value\": null}]");
}

TEST_F(ConvertC2SQL_Null, WStr2Null)
{
	prepareStatement();

	SQLWCHAR val[] = L"0X";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			ESODBC_SQL_NULL, /*size*/0, /*decdigits*/0, val, 0, NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"NULL\", \"value\": null}]");
}


} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
