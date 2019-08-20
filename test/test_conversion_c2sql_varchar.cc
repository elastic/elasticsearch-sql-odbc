/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <gtest/gtest.h>
#include "connected_dbc.h"

#include <string.h>

namespace test {

class ConvertC2SQL_Varchar : public ::testing::Test, public ConnectedDBC {
};

/* note: test name used in test */
TEST_F(ConvertC2SQL_Varchar, CStr2Varchar_empty)
{
	prepareStatement();

	SQLCHAR val[] = "";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_VARCHAR, /*size*/35, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"KEYWORD\", \"value\": \"\"}]");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Varchar, WStr2Varchar_empty)
{
	prepareStatement();

	SQLWCHAR val[] = L"";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_VARCHAR, /*size*/35, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"KEYWORD\", \"value\": \"\"}]");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Varchar, WStr2Varchar_ansi)
{
	prepareStatement();

	SQLWCHAR val[] = L"0123abcABC";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_VARCHAR, /*size*/35, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"KEYWORD\", \"value\": \"0123abcABC\"}]");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Varchar, CStr2Varchar)
{
	prepareStatement();

	SQLCHAR val[] = "0123abcABC";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_VARCHAR, /*size*/35, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"KEYWORD\", \"value\": \"0123abcABC\"}]");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Varchar, WStr2Varchar_ansi_jsonescape)
{
	prepareStatement();

	SQLWCHAR val[] = L"START_{xxx}=\"yyy\"\r__END";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_VARCHAR, /*size*/35, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"KEYWORD\", "
		"\"value\": \"START_{xxx}=\\\"yyy\\\"\\r__END\"}]");
}

TEST_F(ConvertC2SQL_Varchar, CStr2Varchar_jsonescape_oct_len_ptr)
{
	prepareStatement();

	SQLCHAR val[] = "START_{xxx}=\"yyy\"\r__END";
	SQLLEN octet_len = strlen((char *)val);
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_VARCHAR, /*size*/35, /*decdigits*/0, val, sizeof(val),
			&octet_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"KEYWORD\", "
		"\"value\": \"START_{xxx}=\\\"yyy\\\"\\r__END\"}]");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Varchar, CStr2Varchar_jsonescape)
{
	prepareStatement();

	SQLCHAR val[] = "START_{xxx}=\"yyy\"\r__END";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
			SQL_VARCHAR, /*size*/35, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"KEYWORD\", "
		"\"value\": \"START_{xxx}=\\\"yyy\\\"\\r__END\"}]");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Varchar, WStr2Varchar_u8_jsonescape)
{
	prepareStatement();

	SQLWCHAR val[] = L"START_\"AÄoöUü\"__END";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_VARCHAR, /*size*/35, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"KEYWORD\", "
		"\"value\": \"START_\\\"A\u00C4o\u00F6U\u00FC\\\"__END\"}]");
}

TEST_F(ConvertC2SQL_Varchar, WStr2Varchar_u8_fullescape_oct_len_ptr)
{
	prepareStatement();

	SQLWCHAR val[] = L"äöüÄÖÜ";
	SQLLEN octet_len = SQL_NTSL;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_VARCHAR, /*size*/35, /*decdigits*/0, val, sizeof(val),
			&octet_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"KEYWORD\", "
		"\"value\": \"\u00E4\u00F6\u00FC\u00C4\u00D6\u00DC\"}]");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Varchar, WStr2Varchar_u8_fullescape)
{
	prepareStatement();

	SQLWCHAR val[] = L"äöüÄÖÜ";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			SQL_VARCHAR, /*size*/35, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"KEYWORD\", "
		"\"value\": \"\u00E4\u00F6\u00FC\u00C4\u00D6\u00DC\"}]");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Varchar, Short2Varchar)
{
	prepareStatement();

	SQLSMALLINT val = -12345;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_SHORT,
			SQL_VARCHAR, /*size*/6, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"KEYWORD\", \"value\": \"-12345\"}]");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Varchar, Bigint2Varchar)
{
	prepareStatement();

	SQLBIGINT val = LLONG_MAX;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_SBIGINT,
			SQL_VARCHAR, /*size*/19, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"KEYWORD\", "
		"\"value\": \"9223372036854775807\"}]");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Varchar, Bigint2Varchar_fail_22003)
{
	prepareStatement();

	SQLBIGINT val = LLONG_MAX;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_SBIGINT,
			SQL_VARCHAR, /*size*/17, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22003");
}

/* note: test name used in test */
TEST_F(ConvertC2SQL_Varchar, Double2Varchar)
{
	prepareStatement();

	SQLDOUBLE val = 1.2;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_DOUBLE,
			SQL_VARCHAR, /*size*/10, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"KEYWORD\", "
		"\"value\": \"1.2000e+00\"}]");
}


} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
