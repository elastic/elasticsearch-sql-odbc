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

class ConvertC2SQL_Boolean : public ::testing::Test, public ConnectedDBC {
};


TEST_F(ConvertC2SQL_Boolean, CStr2Boolean) /* note: test name used in test */
{
  prepareStatement();

	SQLCHAR val[] = "1";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
			ESODBC_SQL_BOOLEAN, /*size*/0, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT("{\"query\": \"CStr2Boolean\", "
		"\"params\": [{\"type\": \"BOOLEAN\", \"value\": true}], "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

TEST_F(ConvertC2SQL_Boolean, WStr2Boolean) /* note: test name used in test */
{
  prepareStatement();

	SQLWCHAR val[] = L"0";
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
			ESODBC_SQL_BOOLEAN, /*size*/0, /*decdigits*/0, val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT("{\"query\": \"WStr2Boolean\", "
		"\"params\": [{\"type\": \"BOOLEAN\", \"value\": false}], "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

TEST_F(ConvertC2SQL_Boolean, Smallint2Boolean) /* note: name used in test */
{
  prepareStatement();

	SQLSMALLINT val = 1;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_SSHORT,
			ESODBC_SQL_BOOLEAN, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT("{\"query\": \"Smallint2Boolean\", "
		"\"params\": [{\"type\": \"BOOLEAN\", \"value\": true}], "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

TEST_F(ConvertC2SQL_Boolean, UShort2Boolean) /* note: name used in test */
{
  prepareStatement();

	SQLUSMALLINT val = 0;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_USHORT,
			ESODBC_SQL_BOOLEAN, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT("{\"query\": \"UShort2Boolean\", "
		"\"params\": [{\"type\": \"BOOLEAN\", \"value\": false}], "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

TEST_F(ConvertC2SQL_Boolean, LongLong2Boolean) /* note: name used in test */
{
  prepareStatement();

	SQLBIGINT val = 1;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_SBIGINT,
			ESODBC_SQL_BOOLEAN, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT("{\"query\": \"LongLong2Boolean\", "
		"\"params\": [{\"type\": \"BOOLEAN\", \"value\": true}], "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

TEST_F(ConvertC2SQL_Boolean, Float2Boolean) /* note: name used in test */
{
  prepareStatement();

	SQLREAL val = 1.11f;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_FLOAT,
			ESODBC_SQL_BOOLEAN, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT("{\"query\": \"Float2Boolean\", "
		"\"params\": [{\"type\": \"BOOLEAN\", \"value\": true}], "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

TEST_F(ConvertC2SQL_Boolean, Double2Boolean) /* note: name used in test */
{
  prepareStatement();

	SQLDOUBLE val = 1.11;
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_DOUBLE,
			ESODBC_SQL_BOOLEAN, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT("{\"query\": \"Double2Boolean\", "
		"\"params\": [{\"type\": \"BOOLEAN\", \"value\": true}], "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

TEST_F(ConvertC2SQL_Boolean, Numeric2Boolean) /* note: name used in test */
{
  prepareStatement();

	SQL_NUMERIC_STRUCT val;
	val.sign = 0;
	val.precision = 5;
	val.scale = 3;
	memset(val.val, 0, sizeof(val.val));
	memcpy(val.val, "|b", 2);
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_NUMERIC,
			ESODBC_SQL_BOOLEAN, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT("{\"query\": \"Numeric2Boolean\", "
		"\"params\": [{\"type\": \"BOOLEAN\", \"value\": true}], "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

TEST_F(ConvertC2SQL_Boolean, Binary2Boolean) /* note: name used in test */
{
  prepareStatement();

	SQLCHAR val = 0;
	SQLLEN indlen = sizeof(val);
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_BINARY,
			ESODBC_SQL_BOOLEAN, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			&indlen);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st expect = CSTR_INIT("{\"query\": \"Binary2Boolean\", "
		"\"params\": [{\"type\": \"BOOLEAN\", \"value\": false}], "
		"\"mode\": \"ODBC\", " CLIENT_ID "}");

	ASSERT_CSTREQ(buff, expect);
}

TEST_F(ConvertC2SQL_Boolean, Binary2Boolean_fail_22003)
{
  prepareStatement();

	SQLCHAR val[2] = {1};
	ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_BINARY,
			ESODBC_SQL_BOOLEAN, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st buff = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &buff);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22003");
}


} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
