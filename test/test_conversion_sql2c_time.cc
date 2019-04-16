/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <gtest/gtest.h>
#include "connected_dbc.h"

#include <string.h>

/* placeholders; will be undef'd and redef'd */
#define SQL_VAL
#define SQL /* attached for troubleshooting purposes */

namespace test {

class ConvertSQL2C_Time : public ::testing::Test, public ConnectedDBC
{

	protected:
		TIME_STRUCT ts;

	void prepareAndBind(const char *jsonAnswer)
	{
		prepareStatement(jsonAnswer);

		ret = SQLBindCol(stmt, /*col#*/1, SQL_C_TYPE_TIME, &ts, sizeof(ts),
			&ind_len);
		ASSERT_TRUE(SQL_SUCCEEDED(ret));
	}
};


/* ES/SQL 'date' is actually 'timestamp' */
TEST_F(ConvertSQL2C_Time, Timestamp2Time)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL "2345-01-23T12:34:56.000Z"
#define SQL "CAST(" SQL_VAL "AS DATETIME)"

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"DATETIME\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareAndBind(json_answer);

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	EXPECT_EQ(ind_len, sizeof(ts));
	EXPECT_EQ(ts.hour, 12);
	EXPECT_EQ(ts.minute, 34);
	EXPECT_EQ(ts.second, 56);
}

TEST_F(ConvertSQL2C_Time, Timestamp_Str2Time)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL "2345-01-23T12:34:56.000Z"
#define SQL "CAST(" SQL_VAL "AS KEYWORD)"

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"KEYWORD\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareAndBind(json_answer);

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	EXPECT_EQ(ind_len, sizeof(ts));
	EXPECT_EQ(ts.hour, 12);
	EXPECT_EQ(ts.minute, 34);
	EXPECT_EQ(ts.second, 56);
}

TEST_F(ConvertSQL2C_Time, Timestamp2Time_truncate)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL "   2345-01-23T12:34:56.789Z  "
#define SQL "CAST(" SQL_VAL "AS DATETIME)"

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"DATETIME\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareAndBind(json_answer);

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	assertState(L"01S07");

	EXPECT_EQ(ind_len, sizeof(ts));
	EXPECT_EQ(ts.hour, 12);
	EXPECT_EQ(ts.minute, 34);
	EXPECT_EQ(ts.second, 56);
}


TEST_F(ConvertSQL2C_Time, Time_Str2Time)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL "12:34:56.0"
#define SQL "CAST(" SQL_VAL "AS TEXT)"

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL_VAL "\", \"type\": \"text\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareAndBind(json_answer);

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	EXPECT_EQ(ind_len, sizeof(ts));
	EXPECT_EQ(ts.hour, 12);
	EXPECT_EQ(ts.minute, 34);
	EXPECT_EQ(ts.second, 56);
}

TEST_F(ConvertSQL2C_Time, Time2Time)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL "12:34:56Z"
#define SQL "CAST(" SQL_VAL "AS TIME)"

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL_VAL "\", \"type\": \"TIME\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareAndBind(json_answer);

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	EXPECT_EQ(ind_len, sizeof(ts));
	EXPECT_EQ(ts.hour, 12);
	EXPECT_EQ(ts.minute, 34);
	EXPECT_EQ(ts.second, 56);
}


TEST_F(ConvertSQL2C_Time, Time_Str2Time_truncate)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL "12:34:56.7777777"
#define SQL "CAST(" SQL_VAL "AS TEXT)"

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"text\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareAndBind(json_answer);

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	assertState(L"01S07");

	EXPECT_EQ(ind_len, sizeof(ts));
	EXPECT_EQ(ts.hour, 12);
	EXPECT_EQ(ts.minute, 34);
	EXPECT_EQ(ts.second, 56);
}


TEST_F(ConvertSQL2C_Time, Time_Z2Char)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL "12:34:56Z"
#define SQL "CAST(" SQL_VAL "AS TIME)"

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL_VAL "\", \"type\": \"TIME\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	SQLCHAR buff[1024];
	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, &buff, sizeof(buff),
		&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ASSERT_EQ(ind_len, sizeof(SQL_VAL) - /*\0*/1 - /*'Z'*/1);
	ASSERT_TRUE(memcmp(SQL_VAL, buff, sizeof(SQL_VAL) - /*Z\0*/2) == 0);
}


TEST_F(ConvertSQL2C_Time, Time_Z2Char_truncate)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL "12:34:56.789Z"
#define VAL_EXPECTED "12:34:56.7"
#define SQL "CAST(" SQL_VAL "AS TIME)"

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL_VAL "\", \"type\": \"TIME\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	SQLCHAR buff[sizeof(VAL_EXPECTED)];
	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, &buff, sizeof(buff),
		&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	assertState(L"01004");

	ASSERT_EQ(ind_len, sizeof(SQL_VAL) - /*Z\0*/2);
	ASSERT_STREQ((char *)buff, VAL_EXPECTED);
}


TEST_F(ConvertSQL2C_Time, Time_Z2Char_truncate_22003)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL "12:34:56.789Z"
#define SQL "CAST(" SQL_VAL "AS TIME)"

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL_VAL "\", \"type\": \"TIME\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	SQLCHAR buff[5];
	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, &buff, sizeof(buff),
		&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22003");
}


TEST_F(ConvertSQL2C_Time, Time_Offset2Char)
{
#undef VAL_EXPECTED
#undef SQL_VAL
#undef SQL
#define SQL_VAL "15:34:56+03:00"
#define VAL_EXPECTED "12:34:56"
#define SQL "CAST(" SQL_VAL "AS TIME)"

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL_VAL "\", \"type\": \"TIME\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	SQLCHAR buff[1024];
	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, &buff, sizeof(buff),
		&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ASSERT_EQ(ind_len, sizeof(VAL_EXPECTED) - /*\0*/1);
	ASSERT_STREQ((char *)buff, VAL_EXPECTED);
}


TEST_F(ConvertSQL2C_Time, Date2Time_22018)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL "2345-01-23"
#define SQL "CAST(" SQL_VAL "AS TEXT)"

	const SQLWCHAR *sql = MK_WPTR(SQL_VAL);
	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL_VAL "\", \"type\": \"text\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareAndBind(json_answer);

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22018");
}


TEST_F(ConvertSQL2C_Time, Time2Timestamp)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL "12:34:56.789Z"
#define SQL "CAST(" SQL_VAL "AS TIME)"

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL_VAL "\", \"type\": \"TIME\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	TIMESTAMP_STRUCT tss = {1};
	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_TYPE_TIMESTAMP, &tss, sizeof(tss),
		&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	EXPECT_EQ(ind_len, sizeof(tss));
	EXPECT_EQ(tss.hour, 12);
	EXPECT_EQ(tss.minute, 34);
	EXPECT_EQ(tss.second, 56);

	EXPECT_EQ(tss.fraction, 0);

	time_t utc = time(NULL);
	ASSERT_TRUE(utc != (time_t)-1);
	struct tm *tm = localtime(&utc);
	ASSERT_TRUE(tm != NULL);
	EXPECT_EQ(tss.year, tm->tm_year + 1900);
	EXPECT_EQ(tss.month, tm->tm_mon + 1);
	EXPECT_EQ(tss.day, tm->tm_mday);
}


TEST_F(ConvertSQL2C_Time, Time_Str2Timestamp)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL "10:10:10.1010"
#define SQL   "CAST(" SQL_VAL " AS TEXT)"

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"TEXT\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	TIMESTAMP_STRUCT tss = {1};
	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_TYPE_TIMESTAMP, &tss, sizeof(tss),
		&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	EXPECT_EQ(ind_len, sizeof(tss));
	EXPECT_EQ(tss.hour, 10);
	EXPECT_EQ(tss.minute, 10);
	EXPECT_EQ(tss.second, 10);

	EXPECT_EQ(tss.fraction, 101000000);

	time_t utc = time(NULL);
	ASSERT_TRUE(utc != (time_t)-1);
	struct tm *tm = localtime(&utc);
	ASSERT_TRUE(tm != NULL);
	EXPECT_EQ(tss.year, tm->tm_year + 1900);
	EXPECT_EQ(tss.month, tm->tm_mon + 1);
	EXPECT_EQ(tss.day, tm->tm_mday);
}


TEST_F(ConvertSQL2C_Time, Integer2Date_violation_07006)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL "1"
#define SQL   "select " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"select " SQL "\", \"type\": \"integer\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
	prepareAndBind(json_answer);

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"07006");
}


} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=78 : */
