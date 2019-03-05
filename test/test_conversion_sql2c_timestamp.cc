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

class ConvertSQL2C_Timestamp : public ::testing::Test, public ConnectedDBC
{

	protected:
		TIMESTAMP_STRUCT ts;

	void prepareAndBind(const char *jsonAnswer)
	{
		prepareStatement(jsonAnswer);

		ret = SQLBindCol(stmt, /*col#*/1, SQL_C_TYPE_TIMESTAMP, &ts,
				sizeof(ts), &ind_len);
		ASSERT_TRUE(SQL_SUCCEEDED(ret));
	}
};


/* ES/SQL 'DATETIME' is actually 'TIMESTAMP' */
TEST_F(ConvertSQL2C_Timestamp, Datetime2Timestamp)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL   "2345-01-23T12:34:56.789Z"
#define SQL   "CAST(" SQL_VAL " AS DATETIME)"

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
	EXPECT_EQ(ts.year, 2345);
	EXPECT_EQ(ts.month, 1);
	EXPECT_EQ(ts.day, 23);
	EXPECT_EQ(ts.hour, 12);
	EXPECT_EQ(ts.minute, 34);
	EXPECT_EQ(ts.second, 56);
	EXPECT_EQ(ts.fraction, 789000000);
}


TEST_F(ConvertSQL2C_Timestamp, Datetime2Char)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL     "2345-01-23T12:34:56.789Z"
#define SQL_VAL_TS  "2345-01-23 12:34:56.789"
#define SQL   "CAST(" SQL_VAL " AS DATETIME)"

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
	prepareStatement(json_answer);

	SQLCHAR val[sizeof(SQL_VAL)];
	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, val, sizeof(val), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	EXPECT_EQ(ind_len, ISO8601_TS_UTC_LEN(ESODBC_DEF_SEC_PRECISION) - /*Z*/1);
	EXPECT_STREQ(SQL_VAL_TS, (char *)val);
#undef SQL_VAL_TS
}


TEST_F(ConvertSQL2C_Timestamp, String2Timestamp)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL   "2345-01-23T12:34:56.7890123Z"
#define SQL   "CAST(" SQL_VAL " AS KEYWORD)"

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
	EXPECT_EQ(ts.year, 2345);
	EXPECT_EQ(ts.month, 1);
	EXPECT_EQ(ts.day, 23);
	EXPECT_EQ(ts.hour, 12);
	EXPECT_EQ(ts.minute, 34);
	EXPECT_EQ(ts.second, 56);
	EXPECT_EQ(ts.fraction, 789012300);
}

/* ES/SQL 'DATETIME' is actually 'TIMESTAMP' */
TEST_F(ConvertSQL2C_Timestamp, Datetime_with_offset2Timestamp)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL   "2345-01-23T12:34:56.789+01:30"
#define SQL   "CAST(" SQL_VAL " AS DATETIME)"

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
	EXPECT_EQ(ts.year, 2345);
	EXPECT_EQ(ts.month, 1);
	EXPECT_EQ(ts.day, 23);
	EXPECT_EQ(ts.hour, 11);
	EXPECT_EQ(ts.minute, 4);
	EXPECT_EQ(ts.second, 56);
	EXPECT_EQ(ts.fraction, 789000000);
}

/* No second fraction truncation done by the driver -> no test for 01S07 */
//TEST_F(ConvertSQL2C_Timestamp, Timestamp2Timestamp_trimming) {}


TEST_F(ConvertSQL2C_Timestamp, Date_Str2Timestamp)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL "2345-01-23T"
#define SQL   "CAST(" SQL_VAL " AS TEXT)"

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

	EXPECT_EQ(ind_len, sizeof(ts));
	EXPECT_EQ(ts.year, 2345);
	EXPECT_EQ(ts.month, 1);
	EXPECT_EQ(ts.day, 23);
	EXPECT_EQ(ts.hour, 0);
	EXPECT_EQ(ts.minute, 0);
	EXPECT_EQ(ts.second, 0);
	EXPECT_EQ(ts.fraction, 0);
}


TEST_F(ConvertSQL2C_Timestamp, Time_Str2Timestamp)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL "10:10:10.1010"
#define SQL   "CAST(" SQL_VAL " AS TEXT)"

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

	EXPECT_EQ(ind_len, sizeof(ts));
	EXPECT_EQ(ts.year, 0);
	EXPECT_EQ(ts.month, 0);
	EXPECT_EQ(ts.day, 0);
	EXPECT_EQ(ts.hour, 10);
	EXPECT_EQ(ts.minute, 10);
	EXPECT_EQ(ts.second, 10);
	EXPECT_EQ(ts.fraction, 101000000);
}


TEST_F(ConvertSQL2C_Timestamp, Datetime2Timestamp_invalidFormat_22018)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL "invalid 2345-01-23T12:34:56.789Z"
#define SQL   "CAST(" SQL_VAL " AS DATETIME)"

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
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22018");
}

TEST_F(ConvertSQL2C_Timestamp, Integer2Timestamp_violation_07006)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL "1"
#define SQL   "select " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"integer\"}\
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



class ConvertSQL2C_Timestamp_TZ : public ConvertSQL2C_Timestamp
{
	protected:
	void SetUp() override
	{
		((esodbc_dbc_st *)dbc)->apply_tz = TRUE;
		ASSERT_EQ(putenv("TZ=NPT-5:45NTP"), 0);
		tzset();
	}

	void TearDown() override
	{
		ASSERT_EQ(putenv("TZ="), 0);
	}
};


TEST_F(ConvertSQL2C_Timestamp_TZ, Datetime2Char_local)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL		"2000-01-23T12:00:00.789Z"
#define SQL_VAL_TS	"2000-01-23 17:45:00.789"
#define SQL   "CAST(" SQL_VAL " AS DATETIME)"

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
	prepareStatement(json_answer);

	SQLCHAR val[sizeof(SQL_VAL)];
	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, val, sizeof(val), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	EXPECT_EQ(ind_len, ISO8601_TS_UTC_LEN(ESODBC_DEF_SEC_PRECISION) - /*Z*/1);
	EXPECT_STREQ(SQL_VAL_TS, (char *)val);

#undef SQL_VAL_TS
}


TEST_F(ConvertSQL2C_Timestamp_TZ, Datetime_offset2Char_local)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL		"2000-01-23T13:00:00.789+01:00"
#define SQL_VAL_TS	"2000-01-23 17:45:00.789"
#define SQL   "CAST(" SQL_VAL " AS DATETIME)"

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
	prepareStatement(json_answer);

	SQLCHAR val[sizeof(SQL_VAL)];
	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, val, sizeof(val), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	EXPECT_EQ(ind_len, ISO8601_TS_UTC_LEN(ESODBC_DEF_SEC_PRECISION) - /*Z*/1);
	EXPECT_STREQ(SQL_VAL_TS, (char *)val);

#undef SQL_VAL_TS
}


TEST_F(ConvertSQL2C_Timestamp_TZ, Datetime_offset_Str2Timestamp)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL		"2000-01-23T13:00:01.789+01:00"
#define SQL_VAL_TS	"2000-01-23 17:45:01.789"
#define SQL   "CAST(" SQL_VAL " AS KEYWORD)"

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
	EXPECT_EQ(ts.year, 2000);
	EXPECT_EQ(ts.month, 1);
	EXPECT_EQ(ts.day, 23);
	EXPECT_EQ(ts.hour, 17);
	EXPECT_EQ(ts.minute, 45);
	EXPECT_EQ(ts.second, 1);
	EXPECT_EQ(ts.fraction, 789000000);

#undef SQL_VAL_TS
}



} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=78 : */
