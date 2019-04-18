/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <gtest/gtest.h>
#include "connected_dbc.h"

#include <string.h>
#include "convert.h" /* TM_TO_TIMESTAMP_STRUCT() */

/* placeholders; will be undef'd and redef'd */
#define SQL_VAL
#define SQL /* attached for troubleshooting purposes */

namespace test {

class ConvertSQL2C_Timestamp : public ::testing::Test, public ConnectedDBC
{

	protected:
		TIMESTAMP_STRUCT ts = {0};

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


TEST_F(ConvertSQL2C_Timestamp, Datetime2Time_01S07)
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
	prepareStatement(json_answer);

	TIME_STRUCT ts = {0};
	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_TYPE_TIME, &ts,
			sizeof(ts), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	assertState(L"01S07");

	EXPECT_EQ(ind_len, sizeof(ts));
	EXPECT_EQ(ts.hour, 12);
	EXPECT_EQ(ts.minute, 34);
	EXPECT_EQ(ts.second, 56);
}


TEST_F(ConvertSQL2C_Timestamp, Datetime2Date_01S07)
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
	prepareStatement(json_answer);

	DATE_STRUCT ds = {0};
	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_TYPE_DATE, &ds,
			sizeof(ds), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	assertState(L"01S07");

	EXPECT_EQ(ind_len, sizeof(ds));
	EXPECT_EQ(ds.year, 2345);
	EXPECT_EQ(ds.month, 1);
	EXPECT_EQ(ds.day, 23);
}


TEST_F(ConvertSQL2C_Timestamp, Datetime2Date)
{
#undef SQL_VAL
#undef SQL
#define SQL_VAL   "2345-01-23T00:00:00.000Z"
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

	DATE_STRUCT ds = {0};
	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_TYPE_DATE, &ds,
			sizeof(ds), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	assertState(L"00000");

	EXPECT_EQ(ind_len, sizeof(ds));
	EXPECT_EQ(ds.year, 2345);
	EXPECT_EQ(ds.month, 1);
	EXPECT_EQ(ds.day, 23);
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


TEST_F(ConvertSQL2C_Timestamp, Datetime2Char_truncate)
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

	SQLCHAR val[sizeof(SQL_VAL_TS) - 2];
	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, val, sizeof(val), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	assertState(L"01004");

	EXPECT_EQ(ind_len, ISO8601_TS_UTC_LEN(ESODBC_DEF_SEC_PRECISION) - /*Z*/1);
	EXPECT_EQ(memcmp(SQL_VAL_TS, (char *)val, sizeof(val) - 1), 0);
#undef SQL_VAL_TS
}


TEST_F(ConvertSQL2C_Timestamp, Datetime2Char_truncate_22003)
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

	SQLCHAR val[sizeof(SQL_VAL_TS)/2];
	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, val, sizeof(val), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22003");
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
		ASSERT_EQ(putenv("TZ=NPT-5:45"), 0);
		tzset();
	}

	void TearDown() override
	{
		ASSERT_EQ(putenv("TZ="), 0);
		tzset();
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


class ConvertSQL2C_Timestamp_DST : public ConvertSQL2C_Timestamp
{
	protected:
	struct tm no_dst_utc = {0};
	struct tm dst_utc = {0};

	void SetUp() override
	{
		ASSERT_EQ(putenv("TZ="), 0);
		tzset();

		((esodbc_dbc_st *)dbc)->apply_tz = TRUE;
	}

	void timestamp_utc_to_local(struct tm *utc, BOOL to_string);
	void print_tm_timestamp(char *dest, size_t size,
		const char *templ, struct tm *src);

	public:
	ConvertSQL2C_Timestamp_DST()
	{
		/* Construct the tm structs of dates when DST is not and is in effect,
		 * respectively. The DST applicability will depend on the testing
		 * machine's settings. */

		/* 2000-01-01T12:00:00Z */
		no_dst_utc.tm_year = 2000 - 1900;
		no_dst_utc.tm_mon = 0; /* Jan */
		no_dst_utc.tm_mday = 1;
		no_dst_utc.tm_hour = 12;

		/* 2000-05-01T12:00:00Z */
		dst_utc.tm_year = 2000 - 1900;
		dst_utc.tm_mon = 4; /* May */
		dst_utc.tm_mday = 1;
		dst_utc.tm_hour = 12;
	}
};

void ConvertSQL2C_Timestamp_DST::print_tm_timestamp(char *dest, size_t size,
		const char *templ, struct tm *src)
{
	int n = snprintf(dest, size, templ,
			src->tm_year + 1900, src->tm_mon + 1, src->tm_mday,
			src->tm_hour, src->tm_min, src->tm_sec);
	ASSERT_TRUE(0 < n && n < (int)size);

}

void ConvertSQL2C_Timestamp_DST::timestamp_utc_to_local(struct tm *utc,
		BOOL to_string)
{
	const char answer_template[] =
		"{\"columns\": [{\"name\": \"value\", \"type\": \"DATETIME\"}],"
		"\"rows\": [[\"%04d-%02d-%02dT%02d:%02d:%02dZ\"]]}";
	const char expected_template[] = "%04d-%02d-%02d %02d:%02d:%02d";

	utc->tm_isdst = -1; /* shouldn't matter, but set it: unknown DST */
	time_t utc_ts = timegm(utc);
	ASSERT_TRUE(utc_ts != (time_t)-1);
	struct tm *local_tm_ptr = localtime(&utc_ts);
	ASSERT_TRUE(local_tm_ptr != NULL);

	TIMESTAMP_STRUCT local_ts = {0};
	TM_TO_TIMESTAMP_STRUCT(local_tm_ptr, &local_ts, 0LU);

	char fetched[1024], expected[1024];
	char answer[1024];
	ASSERT_LT(sizeof(answer_template) + ISO8601_TS_UTC_LEN(0), sizeof(answer));
	print_tm_timestamp(answer, sizeof(answer), answer_template, utc);

	if (to_string) {
		prepareStatement(answer);

		ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, fetched, sizeof(fetched),
				&ind_len);
		ASSERT_TRUE(SQL_SUCCEEDED(ret));

		print_tm_timestamp(expected, sizeof(expected), expected_template,
				local_tm_ptr);
	} else {
		prepareAndBind(answer);
	}

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	if (to_string) {
		ASSERT_STREQ(fetched, expected);
	} else {
		ASSERT_TRUE(memcmp(&local_ts, /*fetched result*/&ts, sizeof(ts)) == 0);
	}

}

TEST_F(ConvertSQL2C_Timestamp_DST, Timestamp_no_DST_UTC2Local_TS)
{
	timestamp_utc_to_local(&no_dst_utc, FALSE);
}

TEST_F(ConvertSQL2C_Timestamp_DST, Timestamp_DST_UTC2Local_TS)
{
	timestamp_utc_to_local(&dst_utc, FALSE);
}

TEST_F(ConvertSQL2C_Timestamp_DST, Timestamp_no_DST_UTC2Local_Char)
{
	timestamp_utc_to_local(&no_dst_utc, TRUE);
}

TEST_F(ConvertSQL2C_Timestamp_DST, Timestamp_DST_UTC2Local_Char)
{
	timestamp_utc_to_local(&dst_utc, TRUE);
}


} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=78 : */
