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

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"CStr2Varchar_empty\", "
		"\"params\": [{\"type\": \"KEYWORD\", "
		"\"value\": \"\"}], "
		"\"mode\": \"ODBC\"}");

	ASSERT_CSTREQ(buff, expect);
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

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"WStr2Varchar_empty\", "
		"\"params\": [{\"type\": \"KEYWORD\", "
		"\"value\": \"\"}], "
		"\"mode\": \"ODBC\"}");

	ASSERT_CSTREQ(buff, expect);
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

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"WStr2Varchar_ansi\", "
		"\"params\": [{\"type\": \"KEYWORD\", "
		"\"value\": \"0123abcABC\"}], "
		"\"mode\": \"ODBC\"}");

	ASSERT_CSTREQ(buff, expect);
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

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"CStr2Varchar\", "
		"\"params\": [{\"type\": \"KEYWORD\", "
		"\"value\": \"0123abcABC\"}], "
		"\"mode\": \"ODBC\"}");

	ASSERT_CSTREQ(buff, expect);
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

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"WStr2Varchar_ansi_jsonescape\", "
		"\"params\": [{\"type\": \"KEYWORD\", "
		"\"value\": \"START_{xxx}=\\\"yyy\\\"\\r__END\"}], "
		"\"mode\": \"ODBC\"}");

	ASSERT_CSTREQ(buff, expect);
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

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"CStr2Varchar_jsonescape\", "
		"\"params\": [{\"type\": \"KEYWORD\", "
		"\"value\": \"START_{xxx}=\\\"yyy\\\"\\r__END\"}], "
		"\"mode\": \"ODBC\"}");

	ASSERT_CSTREQ(buff, expect);
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

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"WStr2Varchar_u8_jsonescape\", "
		"\"params\": [{\"type\": \"KEYWORD\", "
		"\"value\": \"START_\\\"A\u00C4o\u00F6U\u00FC\\\"__END\"}], "
		"\"mode\": \"ODBC\"}");

	ASSERT_CSTREQ(buff, expect);
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

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT(
		"{\"query\": \"WStr2Varchar_u8_fullescape\", "
		"\"params\": [{\"type\": \"KEYWORD\", "
		"\"value\": \"\u00E4\u00F6\u00FC\u00C4\u00D6\u00DC\"}], "
		"\"mode\": \"ODBC\"}");

	ASSERT_CSTREQ(buff, expect);
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

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT("{\"query\": \"Short2Varchar\", "
		"\"params\": [{\"type\": \"KEYWORD\", \"value\": \"-12345\"}], "
		"\"mode\": \"ODBC\"}");

	ASSERT_CSTREQ(buff, expect);
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

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT("{\"query\": \"Bigint2Varchar\", "
		"\"params\": [{\"type\": \"KEYWORD\", "
		"\"value\": \"9223372036854775807\"}], "
		"\"mode\": \"ODBC\"}");

	ASSERT_CSTREQ(buff, expect);
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

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT("{\"query\": \"Double2Varchar\", "
		"\"params\": [{\"type\": \"KEYWORD\", "
		"\"value\": \"1.2000e+00\"}], "
		"\"mode\": \"ODBC\"}");

	ASSERT_CSTREQ(buff, expect);
}


} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
