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
		ASSERT_EQ(putenv("TZ=NPT-5:45"), 0);
		tzset();

		((esodbc_dbc_st *)dbc)->apply_tz = TRUE;
		prepareStatement();
	}

	void TearDown() override
	{
		ASSERT_EQ(putenv("TZ="), 0);
		tzset();
	}
};

TEST_F(ConvertC2SQL_Timestamp_TZ, WStr_iso8601_Timestamp2Timestamp_colsize_16)
{
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



class ConvertC2SQL_Timestamp_DST : public ConvertC2SQL_Timestamp
{
	protected:
	TIMESTAMP_STRUCT no_dst_local = {0};
	TIMESTAMP_STRUCT dst_local = {0};

	void SetUp() override
	{
		ASSERT_EQ(putenv("TZ="), 0);
		tzset();

		((esodbc_dbc_st *)dbc)->apply_tz = TRUE;
		prepareStatement();
	}

	void timestamp_local_to_utc(TIMESTAMP_STRUCT *, BOOL);

	BOOL dst_in_effect(TIMESTAMP_STRUCT *ts)
	{
		/* quick switch, only valid for the dates below */
		return 3 < ts->month && ts->month <= 10;
	}

	public:
	ConvertC2SQL_Timestamp_DST()
	{
		/* see note in ConvertSQL2C_Timestamp_DST's constructor in
		 * test_conversion_sql2c_timestamp.cc */

		/* 2000-01-01 12:34:56.789 */
		no_dst_local.year = 2000;
		no_dst_local.month = 1; /* Jan */
		no_dst_local.day = 1;
		no_dst_local.hour = 12;
		no_dst_local.fraction = 120000000;

		/* 2000-05-01 12:34:56.789 */
		dst_local.year = 2000;
		dst_local.month = 5; /* May */
		dst_local.day = 1;
		dst_local.hour = 12;
	}
};

void ConvertC2SQL_Timestamp_DST::timestamp_local_to_utc(
		TIMESTAMP_STRUCT *src_local, BOOL from_string)
{
	static const SQLSMALLINT DECDIGITS = 3;
	static const SQLSMALLINT BUFF_SZ = 1024;
	static const char VALUE_PREFIX[] = "\"value\": \"";
	static const wchar_t local_fmt[] =
		L"%04hd-%02hu-%02hu %02hu:%02hu:%02hu.%.*lu";

	if (from_string) {
		SQLWCHAR param[BUFF_SZ];
		int n = swprintf(param, BUFF_SZ, local_fmt,
				src_local->year, src_local->month, src_local->day,
				src_local->hour, src_local->minute, src_local->second,
				DECDIGITS, src_local->fraction);
		ASSERT_TRUE(0 < n && n < BUFF_SZ);

		ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
				SQL_TYPE_TIMESTAMP, 0, DECDIGITS, param, n, /*IndLen*/NULL);
	} else {
		ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
				SQL_TYPE_TIMESTAMP, /*size*/0, /*decdigits*/3,
				src_local, sizeof(*src_local), NULL);
	}
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st json = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &json);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(json.str != NULL);

	/* extract ISO8601 value from constructed JSON */
	char *utc_str = strstr((char *)json.str, VALUE_PREFIX);
	ASSERT_TRUE(utc_str != NULL);
	utc_str += sizeof(VALUE_PREFIX) - /*\0*/1;
	char *end = strchr(utc_str, '"');
	ASSERT_TRUE(end != NULL);
	ASSERT_LT(utc_str, end);
	size_t utc_str_len = end - utc_str;

	/* parse and transform the extracted val to a local-time SQL timestamp */
	timestamp_t tsp;
	ASSERT_TRUE(timestamp_parse(utc_str, utc_str_len, &tsp) == 0);
	struct tm utc_tm;
	ASSERT_TRUE(timestamp_to_tm_utc(&tsp, &utc_tm) != NULL);
	time_t utc_ts = timegm(&utc_tm);
	ASSERT_TRUE(utc_ts != (time_t)-1);
	struct tm *local_tm_ptr = localtime(&utc_ts);
	ASSERT_TRUE(local_tm_ptr != NULL);
	TIMESTAMP_STRUCT dst_local = {0};
	TM_TO_TIMESTAMP_STRUCT(local_tm_ptr, &dst_local, src_local->fraction);

	/* check if test is valid for the local machine */
	EXPECT_TRUE((0 <= local_tm_ptr->tm_isdst) &&
		(0 < local_tm_ptr->tm_isdst) == dst_in_effect(src_local));

	/* compare source local timestamp to that UTC'd by the driver and
	 * localtime'd back above by the test */
	ASSERT_TRUE(memcmp(src_local, &dst_local, sizeof(dst_local)) == 0);
}

TEST_F(ConvertC2SQL_Timestamp_DST, Timestamp_no_DST_Local2UTC_TS)
{
	timestamp_local_to_utc(&no_dst_local, FALSE);
}

TEST_F(ConvertC2SQL_Timestamp_DST, Timestamp_DST_Local2UTC_TS)
{
	timestamp_local_to_utc(&dst_local, FALSE);
}

TEST_F(ConvertC2SQL_Timestamp_DST, Timestamp_no_DST_Local2UTC_WChar)
{
	timestamp_local_to_utc(&no_dst_local, TRUE);
}

TEST_F(ConvertC2SQL_Timestamp_DST, Timestamp_DST_Local2UTC_WChar)
{
	timestamp_local_to_utc(&dst_local, TRUE);
}

} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
