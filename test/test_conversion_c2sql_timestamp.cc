/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <gtest/gtest.h>
#include "connected_dbc.h"

#include <string.h>

#ifdef _WIN64
#	define CLIENT_ID	"\"client_id\": \"odbc64\""
#else /* _WIN64 */
#	define CLIENT_ID	"\"client_id\": \"odbc32\""
#endif /* _WIN64 */


namespace test {

class ConvertC2SQL_Timestamp : public ::testing::Test, public ConnectedDBC {
};

TEST_F(ConvertC2SQL_Timestamp, CStr_Time2Timestamp_fail_22018)
{
	prepareStatement();

	SQLCHAR val[] = "12:34:56.78";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_TYPE_TIMESTAMP, /*size*/0, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22018");
}

TEST_F(ConvertC2SQL_Timestamp, WStr_Time2Timestamp_fail_22018)
{
	prepareStatement();

	SQLWCHAR val[] = L"12:34:56.7890123";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_TYPE_TIMESTAMP, /*size*/0, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22018");
}

TEST_F(ConvertC2SQL_Timestamp, WStr2Timestamp_fail_22008)
{
	prepareStatement();

	SQLWCHAR val[] = L"garbage";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_TYPE_TIMESTAMP, /*size*/0, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22008");
}


/* note: test name used in test */
TEST_F(ConvertC2SQL_Timestamp, WStr_Timestamp2Timestamp_colsize_16)
{
	prepareStatement();

	SQLWCHAR val[] = L"1234-12-23T12:34:56.7890123Z";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_TYPE_TIMESTAMP, /*size*/16, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"WStr_Timestamp2Timestamp_colsize_16\", "
		"\"params\": [{\"type\": \"DATETIME\", "
		"\"value\": \"1234-12-23T12:34Z\"}], "
		"\"field_multi_value_leniency\": true, \"time_zone\": \"Z\", "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Timestamp, WStr_Timestamp2Timestamp_colsize_19)
{
	prepareStatement();

	SQLWCHAR val[] = L"1234-12-23T12:34:56.7890123Z";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_TYPE_TIMESTAMP, /*size*/19, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"WStr_Timestamp2Timestamp_colsize_19\", "
		"\"params\": [{\"type\": \"DATETIME\", "
		"\"value\": \"1234-12-23T12:34:56Z\"}], "
		"\"field_multi_value_leniency\": true, \"time_zone\": \"Z\", "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

TEST_F(ConvertC2SQL_Timestamp, WStr_Timestamp2Timestamp_colsize_17_fail_HY104)
{
	prepareStatement();

	SQLWCHAR val[] = L"1234-12-23T12:34:56.7890123Z";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_TYPE_TIMESTAMP, /*size*/17, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"HY104");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Timestamp, CStr_Timestamp2Timestamp_colsize_decdigits_trim)
{
	prepareStatement();

	SQLCHAR val[] = "1234-12-23T12:34:56.7890123Z";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_TYPE_TIMESTAMP, /*size*/25, /*decdigits*/7, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"CStr_Timestamp2Timestamp_colsize_decdigits_trim\", "
		"\"params\": [{\"type\": \"DATETIME\", "
		"\"value\": \"1234-12-23T12:34:56.78901Z\"}], "
		"\"field_multi_value_leniency\": true, \"time_zone\": \"Z\", "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Timestamp, CStr_Timestamp2Timestamp_colsize_decdigits_full)
{
	prepareStatement();

	SQLCHAR val[] = "1234-12-23T12:34:56.7890123Z";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_TYPE_TIMESTAMP, /*size*/35, /*decdigits*/7, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"CStr_Timestamp2Timestamp_colsize_decdigits_full\", "
		"\"params\": [{\"type\": \"DATETIME\", "
		"\"value\": \"1234-12-23T12:34:56.7890123Z\"}], "
		"\"field_multi_value_leniency\": true, \"time_zone\": \"Z\", "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Timestamp, Timestamp2Timestamp_decdigits_7)
{
	prepareStatement();

	TIMESTAMP_STRUCT val;
	val.year = 2345;
	val.month = 1;
	val.day = 23;
	val.hour = 12;
	val.minute = 34;
	val.second = 56;
	val.fraction = 789012340;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
			SQL_TYPE_TIMESTAMP, /*size*/35, /*decdigits*/7, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"Timestamp2Timestamp_decdigits_7\", "
		"\"params\": [{\"type\": \"DATETIME\", "
		"\"value\": \"2345-01-23T12:34:56.7890123Z\"}], "
		"\"field_multi_value_leniency\": true, \"time_zone\": \"Z\", "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Timestamp, Binary2Timestamp_colsize_0)
{
	prepareStatement();

	TIMESTAMP_STRUCT val;
	val.year = 2345;
	val.month = 1;
	val.day = 23;
	val.hour = 12;
	val.minute = 34;
	val.second = 56;
	val.fraction = 789;
	SQLLEN ind_len = sizeof(val);
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_BINARY,
			SQL_TYPE_TIMESTAMP, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"Binary2Timestamp_colsize_0\", "
		"\"params\": [{\"type\": \"DATETIME\", "
		"\"value\": \"2345-01-23T12:34:56Z\"}], "
		"\"field_multi_value_leniency\": true, \"time_zone\": \"Z\", "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Timestamp, Date2Timestamp)
{
	prepareStatement();

	DATE_STRUCT val;
	val.year = 2345;
	val.month = 1;
	val.day = 23;
	SQLLEN ind_len = sizeof(val);
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_TYPE_DATE,
			SQL_TYPE_TIMESTAMP, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"Date2Timestamp\", "
		"\"params\": [{\"type\": \"DATETIME\", "
		"\"value\": \"2345-01-23T00:00:00Z\"}], "
		"\"field_multi_value_leniency\": true, \"time_zone\": \"Z\", "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

TEST_F(ConvertC2SQL_Timestamp, Time2Timestamp_unimplemented_HYC00)
{
	prepareStatement();

	TIME_STRUCT val;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_TYPE_TIME,
			SQL_TYPE_TIMESTAMP, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"HYC00");
}


class ConvertC2SQL_Timestamp_TZ : public ConvertC2SQL_Timestamp
{
	protected:
	void SetUp() override
	{
		((esodbc_dbc_st *)dbc)->apply_tz = TRUE;
		ASSERT_EQ(putenv("TZ=NPT-5:45"), 0);
		tzset();

		/* The 'time_zone' & co. params are computed once, at library load ->
		 * need to recompute after setting the TZ. */
		ASSERT_TRUE(queries_init());
	}

	void TearDown() override
	{
		ASSERT_EQ(putenv("TZ="), 0);
	}
};

TEST_F(ConvertC2SQL_Timestamp_TZ, WStr_iso8601_Timestamp2Timestamp_colsize_16)
{
	prepareStatement();

	SQLWCHAR val[] = L"1234-12-23T12:34:56.7890123Z";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_TYPE_TIMESTAMP, /*size*/16, /*decdigits*/10, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"WStr_iso8601_Timestamp2Timestamp_colsize_16\", "
		"\"params\": [{\"type\": \"DATETIME\", "
		"\"value\": \"1234-12-23T12:34Z\"}], "
		"\"field_multi_value_leniency\": true, \"time_zone\": \"+05:45\", "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

TEST_F(ConvertC2SQL_Timestamp_TZ, WStr_iso8601_Timestamp2Timestamp_sz23_dd4)
{
	prepareStatement();

	SQLWCHAR val[] = L"1234-12-23T12:34:56.7890123Z";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_TYPE_TIMESTAMP, /*size*/23, /*decdigits*/4, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"WStr_iso8601_Timestamp2Timestamp_sz23_dd4\", "
		"\"params\": [{\"type\": \"DATETIME\", "
		"\"value\": \"1234-12-23T12:34:56.789Z\"}], "
		"\"field_multi_value_leniency\": true, \"time_zone\": \"+05:45\", "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

TEST_F(ConvertC2SQL_Timestamp_TZ, WStr_iso8601_Timestamp2Timestamp_decdig4)
{
	prepareStatement();

	SQLWCHAR val[] = L"1234-12-23T12:34:56.7890123Z";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_TYPE_TIMESTAMP, /*size*/0, /*decdigits*/4, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"WStr_iso8601_Timestamp2Timestamp_decdig4\", "
		"\"params\": [{\"type\": \"DATETIME\", "
		"\"value\": \"1234-12-23T12:34:56.7890Z\"}], "
		"\"field_multi_value_leniency\": true, \"time_zone\": \"+05:45\", "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

TEST_F(ConvertC2SQL_Timestamp_TZ, WStr_SQL_Timestamp_local2Timestamp)
{
	prepareStatement();

	SQLWCHAR val[] = L"2000-12-23 17:45:56.7890123";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_TYPE_TIMESTAMP, /*size*/0, /*decdigits*/4, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"WStr_SQL_Timestamp_local2Timestamp\", "
		"\"params\": [{\"type\": \"DATETIME\", "
		"\"value\": \"2000-12-23T12:00:56.7890Z\"}], "
		"\"field_multi_value_leniency\": true, \"time_zone\": \"+05:45\", "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
