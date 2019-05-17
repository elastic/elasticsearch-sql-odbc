/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <gtest/gtest.h>
#include "connected_dbc.h"

#include <string.h>
#include "timestamp.h"

namespace test {

class ConvertC2SQL_Date : public ::testing::Test, public ConnectedDBC
{
	void SetUp() override
	{
		prepareStatement();
	}
};

/* note: test name used in test */
TEST_F(ConvertC2SQL_Date, Date2Date)
{
	DATE_STRUCT val;
	val.year = 1234;
	val.month = 12;
	val.day = 23;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_TYPE_DATE,
			SQL_TYPE_DATE, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"DATE\", "
		"\"value\": \"1234-12-23T00:00:00Z\"}]");
}

TEST_F(ConvertC2SQL_Date, CStr_Date2Date)
{
	SQLCHAR val[] = "2000-01-01"; // treated as utc, since apply_tz==FALSE
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_TYPE_DATE, /*size*/0, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"DATE\", "
		"\"value\": \"2000-01-01T00:00:00Z\"}]");
}

TEST_F(ConvertC2SQL_Date, Time2Date_fail_07006)
{
	TIME_STRUCT val = {0};
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_TYPE_TIME,
			SQL_TYPE_DATE, /*size*/0, /*decdigits*/3, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"07006");
}

TEST_F(ConvertC2SQL_Date, WStr_Time2Date_fail_22018)
{
	SQLWCHAR val[] = L"12:34:56";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_TYPE_DATE, /*size*/0, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22018");
}

TEST_F(ConvertC2SQL_Date, WStr2Date_fail_22008)
{
	SQLWCHAR val[] = L"garbage";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_TYPE_DATE, /*size*/0, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22008");
}


/* note: test name used in test */
TEST_F(ConvertC2SQL_Date, WStr_Timestamp2Date)
{
	SQLWCHAR val[] = L"1234-12-23T12:34:56.7890123Z";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_TYPE_DATE, /*size*/0, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"DATE\", "
		"\"value\": \"1234-12-23T00:00:00Z\"}]");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Date, Timestamp2Date)
{
	TIMESTAMP_STRUCT val;
	val.year = 2345;
	val.month = 1;
	val.day = 23;
	val.hour = 12;
	val.minute = 34;
	val.second = 56;
	val.fraction = 789012340;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
			SQL_TYPE_DATE, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"DATE\", "
		"\"value\": \"2345-01-23T00:00:00Z\"}]");
}

} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
