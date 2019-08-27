/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <gtest/gtest.h>
#include "connected_dbc.h"

#include <string.h>

namespace test {

class ConvertC2SQL_Numeric : public ::testing::Test, public ConnectedDBC {
};

/* note: test name used in test */
TEST_F(ConvertC2SQL_Numeric, CStr_Short2Integer)
{
	prepareStatement();

	SQLCHAR val[] = "-12345";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_INTEGER, /*size*/0, /*decdigits*/0, val, SQL_NTSL,
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"INTEGER\", \"value\": -12345}]");
}

TEST_F(ConvertC2SQL_Numeric, CStr_Short2Integer_fail_22001)
{
	prepareStatement();

	SQLCHAR val[] = "-12345.123"; // digits present
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_INTEGER, /*size*/0, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22001");
}

TEST_F(ConvertC2SQL_Numeric, CStr_Short2Integer_fail_22018)
{
	prepareStatement();

	SQLCHAR val[] = "-12345.123X"; // NaN
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_INTEGER, /*size*/0, /*decdigits*/0, val, sizeof(val) - /*\0*/1,
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22018");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Numeric, CStr_LLong2Long)
{
	prepareStatement();

	SQLCHAR val[] = "9223372036854775807"; /* LLONG_MAX */
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_BIGINT, /*size*/0, /*decdigits*/0, val, SQL_NTSL,
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"LONG\", "
		"\"value\": 9223372036854775807}]");
}

TEST_F(ConvertC2SQL_Numeric, CStr_LLong2Integer_fail_22003)
{
	prepareStatement();

	SQLCHAR val[] = "9223372036854775807"; // out of range
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_INTEGER, /*size*/0, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22003");

}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Numeric, CStr_Float2Long)
{
	prepareStatement();

	SQLCHAR val[] = "9223372036854775806.12345"; /* LLONG_MAX.12345 - 1 */
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_BIGINT, /*size*/0, /*decdigits*/0, val, sizeof(val) - /*\0*/1,
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"LONG\", "
		"\"value\": 9223372036854775806.12345}]");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Numeric, WStr_Byte2Integer)
{
	prepareStatement();

	SQLWCHAR val[] = L"-128";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_INTEGER, /*size*/0, /*decdigits*/0, val, SQL_NTSL,
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"INTEGER\", \"value\": -128}]");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Numeric, WStr_Double2HFloat)
{
	prepareStatement();

	SQLWCHAR val[] = L"-12345678901234567890.123456789";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_FLOAT, /*size*/0, /*decdigits*/0, val, SQL_NTSL,
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"HALF_FLOAT\", "
		"\"value\": -12345678901234567890.123456789}]");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Numeric, WStr_Double2SFloat)
{
	prepareStatement();

	SQLWCHAR val[] = L"-12345678901234567890.123456789";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_FLOAT, /*size*/17, /*decdigits*/0, val, sizeof(val) - /*\0*/2,
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"HALF_FLOAT\", "
		"\"value\": -12345678901234567890.123456789}]");
}

TEST_F(ConvertC2SQL_Numeric, WStr_Double2Real_fail_22003)
{
	prepareStatement();

	SQLWCHAR val[] = L"2.225074e-308"; // DBL_MIN
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_REAL, /*size*/17, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22003");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Numeric, Short2Integer)
{
	prepareStatement();

	SQLSMALLINT val = -12345;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_SHORT,
			SQL_INTEGER, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"INTEGER\", \"value\": -12345}]");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Numeric, LLong2Long)
{
	prepareStatement();

	SQLBIGINT val = 9223372036854775807; /* LLONG_MAX */
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_SBIGINT,
			SQL_BIGINT, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"LONG\", "
		"\"value\": 9223372036854775807}]");
}

TEST_F(ConvertC2SQL_Numeric, LLong2Integer_fail_22003)
{
	prepareStatement();

	SQLBIGINT val = 9223372036854775807; // out of range
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_SBIGINT,
			SQL_INTEGER, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22003");

}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Numeric, Float2Long)
{
	prepareStatement();

	SQLREAL val = (SQLREAL)LLONG_MAX;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_FLOAT,
			SQL_BIGINT, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"LONG\", "
		"\"value\": 9223372036854775808}]"); // == (double)LLONG_MAX: XXX
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Numeric, Byte2Integer)
{
	prepareStatement();

	SQLSCHAR val = -128;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_TINYINT,
			SQL_INTEGER, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"INTEGER\", \"value\": -128}]");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Numeric, Double2HFloat)
{
	prepareStatement();

	SQLDOUBLE val = -1234567890.123456789;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_DOUBLE,
			SQL_FLOAT, /*size: ignored*/15, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"HALF_FLOAT\", "
		"\"value\": -1234567890.123}]");
}

TEST_F(ConvertC2SQL_Numeric, Double2Real_fail_22003)
{
	prepareStatement();

	SQLDOUBLE val = 2.225074e-308; // DBL_MIN
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_DOUBLE,
			SQL_REAL, /*size*/17, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22003");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Numeric, Bin_LLong2Long)
{
	prepareStatement();

	SQLBIGINT val = 9223372036854775807; /* LLONG_MAX */
	SQLLEN osize = sizeof(val);
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_BINARY,
			SQL_BIGINT, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			&osize);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"LONG\", "
		"\"value\": 9223372036854775807}]");
}

TEST_F(ConvertC2SQL_Numeric, Bin_Byte2Integer_fail_HY090)
{
	prepareStatement();

	SQLSCHAR val = -128;
	SQLLEN osize = sizeof(val);
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_BINARY,
			SQL_INTEGER, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			&osize);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"HY090");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Numeric, Bin_Real2HFloat)
{
	prepareStatement();

	SQLREAL val = -123456.7890123f;
	SQLLEN osize = sizeof(val);
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_BINARY,
			SQL_FLOAT, /*size: ignored*/25, /*decdigits*/0, &val, sizeof(val),
			&osize);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"HALF_FLOAT\", "
		"\"value\": -123456.789}]");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Numeric, Numeric2HFloat)
{
	prepareStatement();

	SQL_NUMERIC_STRUCT val;
	val.sign = 0;
	val.precision = 5;
	val.scale = 3;
	memset(val.val, 0, sizeof(val.val));
	memcpy(val.val, "|b", 2);
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_NUMERIC,
			SQL_FLOAT, /*size*/11, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"HALF_FLOAT\", "
		"\"value\": -25.212}]");
}

} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
