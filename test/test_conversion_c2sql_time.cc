/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <gtest/gtest.h>
#include "connected_dbc.h"

#include <string.h>
#include "timestamp.h"

#ifdef _WIN64
#	define CLIENT_ID	"\"client_id\": \"odbc64\""
#else /* _WIN64 */
#	define CLIENT_ID	"\"client_id\": \"odbc32\""
#endif /* _WIN64 */


namespace test {

class ConvertC2SQL_Time : public ::testing::Test, public ConnectedDBC
{
	void SetUp() override
	{
		prepareStatement();
	}
};

TEST_F(ConvertC2SQL_Time, CStr_Time2Time)
{
	const static char *answ_template =
		"{\"query\": \"%s\", "
		"\"params\": [{\"type\": \"TIME\", "
		"\"value\": \"%sZ\"}], "
		"\"field_multi_value_leniency\": true, \"time_zone\": \"Z\", "
		"\"mode\": \"ODBC\", " CLIENT_ID "}";

	SQLCHAR val[] = "12:34:56.789"; // treated as utc, since apply_tz==FALSE
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_TYPE_TIME, /*size*/0, /*decdigits*/3, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	char expected[1024];
	int n = snprintf(expected, sizeof(expected), answ_template, test_name,
		val);
	ASSERT_EQ(n, buff.cnt);
	ASSERT_EQ(strncmp((char *)buff.str, expected, buff.cnt), 0);
}

TEST_F(ConvertC2SQL_Time, Date2Time_fail_07006)
{
	DATE_STRUCT val = {0};
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_TYPE_DATE,
			SQL_TYPE_TIME, /*size*/0, /*decdigits*/3, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"07006");
}

TEST_F(ConvertC2SQL_Time, WStr_Date2Time_fail_22018)
{
	SQLWCHAR val[] = L"2000-01-01";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_TYPE_TIME, /*size*/0, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22018");
}

TEST_F(ConvertC2SQL_Time, WStr2Time_fail_22008)
{
	SQLWCHAR val[] = L"garbage";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_TYPE_TIME, /*size*/0, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22008");
}


/* note: test name used in test */
TEST_F(ConvertC2SQL_Time, WStr_Timestamp2Time_colsize_16)
{
	SQLWCHAR val[] = L"1234-12-23T12:34:56.7890123Z";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_TYPE_TIME, /*size*/16, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"WStr_Timestamp2Time_colsize_16\", "
		"\"params\": [{\"type\": \"TIME\", "
		"\"value\": \"12:34Z\"}], "
		"\"field_multi_value_leniency\": true, \"time_zone\": \"Z\", "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Time, Time2Time)
{
	TIME_STRUCT val;
	val.hour = 12;
	val.minute = 34;
	val.second = 56;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_TYPE_TIME,
			SQL_TYPE_TIME, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"Time2Time\", "
		"\"params\": [{\"type\": \"TIME\", "
		"\"value\": \"12:34:56Z\"}], "
		"\"field_multi_value_leniency\": true, \"time_zone\": \"Z\", "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Time, Timestamp2Time_decdigits_7)
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
			SQL_TYPE_TIME, /*size*/35, /*decdigits*/7, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"Timestamp2Time_decdigits_7\", "
		"\"params\": [{\"type\": \"TIME\", "
		"\"value\": \"12:34:56.7890123Z\"}], "
		"\"field_multi_value_leniency\": true, \"time_zone\": \"Z\", "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}


} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
