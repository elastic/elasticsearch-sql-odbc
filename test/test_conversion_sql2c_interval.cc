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

class ConvertSQL2C_Interval : public ::testing::Test, public ConnectedDBC
{
	protected:
		SQL_INTERVAL_STRUCT is = {0};
		// for interval values
		SQLWCHAR wbuff[128] = {0};
		SQLCHAR buff[128] = {0};
};

TEST_F(ConvertSQL2C_Interval, Integer2Interval_year)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "2001"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_YEAR, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(ind_len, sizeof(is));

	SQL_INTERVAL_STRUCT is2 = {0};
	is2.interval_type = SQL_IS_YEAR;
	is2.intval.year_month.year = atoi(SQL_VAL);
	ASSERT_TRUE(memcmp(&is, &is2, sizeof(is)) == 0);
}

TEST_F(ConvertSQL2C_Interval, Integer2Interval_month)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "-2001"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_MONTH, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(ind_len, sizeof(is));

	SQL_INTERVAL_STRUCT is2 = {0};
	is2.interval_type = SQL_IS_MONTH;
	is2.intval.year_month.month = abs(atoi(SQL_VAL));
	is2.interval_sign = SQL_VAL[0] == '-' ? SQL_TRUE : SQL_FALSE;
	ASSERT_TRUE(memcmp(&is, &is2, sizeof(is)) == 0);
}

TEST_F(ConvertSQL2C_Interval, Integer2Interval_day)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "26"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_DAY, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(ind_len, sizeof(is));

	SQL_INTERVAL_STRUCT is2 = {0};
	is2.interval_type = SQL_IS_DAY;
	is2.intval.day_second.day = abs(atoi(SQL_VAL));
	is2.interval_sign = SQL_VAL[0] == '-' ? SQL_TRUE : SQL_FALSE;
	ASSERT_TRUE(memcmp(&is, &is2, sizeof(is)) == 0);
}

TEST_F(ConvertSQL2C_Interval, Long2Interval_hour)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "26"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"Long\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_HOUR, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(ind_len, sizeof(is));

	SQL_INTERVAL_STRUCT is2 = {0};
	is2.interval_type = SQL_IS_HOUR;
	is2.intval.day_second.hour = abs(atoi(SQL_VAL));
	is2.interval_sign = SQL_VAL[0] == '-' ? SQL_TRUE : SQL_FALSE;
	ASSERT_TRUE(memcmp(&is, &is2, sizeof(is)) == 0);
}

TEST_F(ConvertSQL2C_Interval, Short2Interval_minute)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "-26"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"Short\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_MINUTE, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(ind_len, sizeof(is));

	SQL_INTERVAL_STRUCT is2 = {0};
	is2.interval_type = SQL_IS_MINUTE;
	is2.intval.day_second.minute = abs(atoi(SQL_VAL));
	is2.interval_sign = SQL_VAL[0] == '-' ? SQL_TRUE : SQL_FALSE;
	ASSERT_TRUE(memcmp(&is, &is2, sizeof(is)) == 0);
}

TEST_F(ConvertSQL2C_Interval, Byte2Interval_second)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "-26"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"Byte\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_SECOND, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(ind_len, sizeof(is));

	SQL_INTERVAL_STRUCT is2 = {0};
	is2.interval_type = SQL_IS_SECOND;
	is2.intval.day_second.second = abs(atoi(SQL_VAL));
	is2.interval_sign = SQL_VAL[0] == '-' ? SQL_TRUE : SQL_FALSE;
	ASSERT_TRUE(memcmp(&is, &is2, sizeof(is)) == 0);
}

TEST_F(ConvertSQL2C_Interval, Float2Interval_violation_07006)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "-26"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"Float\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_SECOND, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"07006");
}

TEST_F(ConvertSQL2C_Interval, Integer2Interval_multi_violation_07006)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "1"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_DAY_TO_HOUR, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"07006");
}

TEST_F(ConvertSQL2C_Interval, Interval_day2Interval_hour_violation_07006)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "{INTERVAL '1' DAY}"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_DAY\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_HOUR, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"07006");
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_C_violation_07006)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL '1' DAY"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_HOUR, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"07006");
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_second_no_fraction)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL -'1' SECOND"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_SECOND, &is, sizeof(is),
			&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_SECOND);
	ASSERT_TRUE(is.interval_sign == SQL_TRUE);
	ASSERT_TRUE(is.intval.day_second.second == 1);
	ASSERT_TRUE(is.intval.day_second.fraction == 0);
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_second_with_fraction)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL 1.0004 SECOND"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_SECOND, &is, sizeof(is),
			&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_SECOND);
	ASSERT_TRUE(is.interval_sign == SQL_FALSE);
	ASSERT_TRUE(is.intval.day_second.second == 1);
	ASSERT_TRUE(is.intval.day_second.fraction == 400); // default precision: 6
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_second_dot_no_fraction)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL 1. SECOND"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_SECOND, &is, sizeof(is),
			&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_SECOND);
	ASSERT_TRUE(is.interval_sign == SQL_FALSE);
	ASSERT_TRUE(is.intval.day_second.second == 1);
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_second_fraction_trunc)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL 1.0123456789 SECOND"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_SECOND, &is, sizeof(is),
			&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(ret = SQL_SUCCESS_WITH_INFO);
	ASSERT_TRUE(is.interval_type == SQL_IS_SECOND);
	ASSERT_TRUE(is.interval_sign == SQL_FALSE);
	ASSERT_TRUE(is.intval.day_second.second == 1);
	ASSERT_TRUE(is.intval.day_second.fraction == 12345); // default precision: 6
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_year)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL -'1' YEAR"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_YEAR, &is, sizeof(is),
			&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_YEAR);
	ASSERT_TRUE(is.interval_sign == SQL_TRUE);
	ASSERT_TRUE(is.intval.year_month.year == 1);
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_year_to_month)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL -'22 - 11' YEAR TO MoNtH"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_YEAR_TO_MONTH, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_YEAR_TO_MONTH);
	ASSERT_TRUE(is.interval_sign == SQL_TRUE);
	ASSERT_TRUE(is.intval.year_month.year == 22);
	ASSERT_TRUE(is.intval.year_month.month == 11);
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_year_to_month_ivl_prec_22018)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL -'202 - 11' YEAR TO MoNtH"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_YEAR_TO_MONTH, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret)); // default interval precision: 2
	assertState(L"22018");
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_year_to_month_non_lead_22015)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL -'22 - 12' YEAR TO MoNtH"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_YEAR_TO_MONTH, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret)); // month is over limit of 11
	assertState(L"22015");
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_day_to_hour)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL - 22 12 day to HOUR"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_DAY_TO_HOUR, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_DAY_TO_HOUR);
	ASSERT_TRUE(is.interval_sign == SQL_TRUE);
	ASSERT_TRUE(is.intval.day_second.day == 22);
	ASSERT_TRUE(is.intval.day_second.hour == 12);
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_day_to_hour_non_lead_22015)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL - 22 24 day to HOUR"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_DAY_TO_HOUR, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22015");
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_day_to_minute)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL - 22 12:59 DAY TO MINUTE"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_DAY_TO_MINUTE, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_DAY_TO_MINUTE);
	ASSERT_TRUE(is.interval_sign == SQL_TRUE);
	ASSERT_TRUE(is.intval.day_second.day == 22);
	ASSERT_TRUE(is.intval.day_second.hour == 12);
	ASSERT_TRUE(is.intval.day_second.minute == 59);
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_day_to_minute_non_lead_22015)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "{INTERVAL - '22 12:61' DAY TO MINUTE}"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_DAY_TO_MINUTE, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22015");
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_day_to_second)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL - 22 12:59:58 DAY TO SECOND"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_DAY_TO_SECOND, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_DAY_TO_SECOND);
	ASSERT_TRUE(is.interval_sign == SQL_TRUE);
	ASSERT_TRUE(is.intval.day_second.day == 22);
	ASSERT_TRUE(is.intval.day_second.hour == 12);
	ASSERT_TRUE(is.intval.day_second.minute == 59);
	ASSERT_TRUE(is.intval.day_second.second == 58);
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_day_to_second_non_lead_22015)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL - 22 12:59:68 DAY TO SECOND"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_DAY_TO_SECOND, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22015");
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_day_to_second_fractions)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL -'25 23:59:59.999'  DAY   TO         SECOND"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_DAY_TO_SECOND, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_DAY_TO_SECOND);
	ASSERT_TRUE(is.interval_sign == SQL_TRUE);
	ASSERT_TRUE(is.intval.day_second.day == 25);
	ASSERT_TRUE(is.intval.day_second.hour == 23);
	ASSERT_TRUE(is.intval.day_second.minute == 59);
	ASSERT_TRUE(is.intval.day_second.second == 59);
	ASSERT_TRUE(is.intval.day_second.fraction == 999000); // def precision: 6
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_hour_to_minute)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL -'61:59' HOUR TO MINUTE"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_HOUR_TO_MINUTE, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_HOUR_TO_MINUTE);
	ASSERT_TRUE(is.interval_sign == SQL_TRUE);
	ASSERT_TRUE(is.intval.day_second.hour == 61);
	ASSERT_TRUE(is.intval.day_second.minute == 59);
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_hour_to_minute_non_lead_22015)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL -'61:69' HOUR TO MINUTE"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_HOUR_TO_MINUTE, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22015");
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_hour_to_second_fraction)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL -'61:59:58.999' HOUR TO SECOND"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_HOUR_TO_SECOND, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_HOUR_TO_SECOND);
	ASSERT_TRUE(is.interval_sign == SQL_TRUE);
	ASSERT_TRUE(is.intval.day_second.hour == 61);
	ASSERT_TRUE(is.intval.day_second.minute == 59);
	ASSERT_TRUE(is.intval.day_second.second == 58);
	ASSERT_TRUE(is.intval.day_second.fraction == 999000); // def precision: 6
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_hour_to_second_non_lead_22015)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL -'61:61:58.999' HOUR TO SECOND"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_HOUR_TO_SECOND, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22015");
}

TEST_F(ConvertSQL2C_Interval, Text2Interval_minute_to_second_fraction)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "INTERVAL -'61:59.999' MINUTE TO SECOND"
#	define SQL   "SELECT " SQL_VAL

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
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_MINUTE_TO_SECOND, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_MINUTE_TO_SECOND);
	ASSERT_TRUE(is.interval_sign == SQL_TRUE);
	ASSERT_TRUE(is.intval.day_second.minute == 61);
	ASSERT_TRUE(is.intval.day_second.second == 59);
	ASSERT_TRUE(is.intval.day_second.fraction == 999000); // def precision: 6
}

TEST_F(ConvertSQL2C_Interval, Iso86012Interval_year)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "P2Y"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_YEAR\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_YEAR, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_YEAR);
	ASSERT_TRUE(is.interval_sign == SQL_FALSE);
	ASSERT_TRUE(is.intval.year_month.year == 2);
}

TEST_F(ConvertSQL2C_Interval, Iso86012Interval_month)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "P-23M"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_MONTH\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_MONTH, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_MONTH);
	ASSERT_TRUE(is.interval_sign == SQL_TRUE);
	ASSERT_TRUE(is.intval.year_month.month == 23);
}

TEST_F(ConvertSQL2C_Interval, Iso86012Interval_year_to_month)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "P-2Y-3M"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_YEAR_TO_MONTH\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_YEAR_TO_MONTH, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_YEAR_TO_MONTH);
	ASSERT_TRUE(is.interval_sign == SQL_TRUE);
	ASSERT_TRUE(is.intval.year_month.year == 2);
	ASSERT_TRUE(is.intval.year_month.month == 3);
}

TEST_F(ConvertSQL2C_Interval, Iso86012Interval_day_to_hour_day_compounding)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "PT25H" // = P1DT1H
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_DAY_TO_HOUR\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_DAY_TO_HOUR, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_DAY_TO_HOUR);
	ASSERT_TRUE(is.interval_sign == SQL_FALSE);
	ASSERT_TRUE(is.intval.day_second.day == 1);
	ASSERT_TRUE(is.intval.day_second.hour == 1);
}

TEST_F(ConvertSQL2C_Interval, Iso86012Interval_day_to_hour)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "P33DT22H"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_DAY_TO_HOUR\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_DAY_TO_HOUR, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_DAY_TO_HOUR);
	ASSERT_TRUE(is.interval_sign == SQL_FALSE);
	ASSERT_TRUE(is.intval.day_second.day == 33);
	ASSERT_TRUE(is.intval.day_second.hour == 22);
}

TEST_F(ConvertSQL2C_Interval, Iso86012Interval_day_to_minute)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "P33DT22H44M"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_DAY_TO_MINUTE\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_DAY_TO_MINUTE, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_DAY_TO_MINUTE);
	ASSERT_TRUE(is.interval_sign == SQL_FALSE);
	ASSERT_TRUE(is.intval.day_second.day == 33);
	ASSERT_TRUE(is.intval.day_second.hour == 22);
	ASSERT_TRUE(is.intval.day_second.minute == 44);
}

TEST_F(ConvertSQL2C_Interval, Iso86012Interval_day_to_second)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "P33DT22H44M55.666S"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_DAY_TO_SECOND\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_DAY_TO_SECOND, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_DAY_TO_SECOND);
	ASSERT_TRUE(is.interval_sign == SQL_FALSE);
	ASSERT_TRUE(is.intval.day_second.day == 33);
	ASSERT_TRUE(is.intval.day_second.hour == 22);
	ASSERT_TRUE(is.intval.day_second.minute == 44);
	ASSERT_TRUE(is.intval.day_second.second == 55);
	ASSERT_TRUE(is.intval.day_second.fraction == 666000); // def prec: 6
}

TEST_F(ConvertSQL2C_Interval, Iso86012Interval_hour_to_minute)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "PT-22H-44M"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_HOUR_TO_MINUTE\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_HOUR_TO_MINUTE, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_HOUR_TO_MINUTE);
	ASSERT_TRUE(is.interval_sign == SQL_TRUE);
	ASSERT_TRUE(is.intval.day_second.hour == 22);
	ASSERT_TRUE(is.intval.day_second.minute == 44);
}

TEST_F(ConvertSQL2C_Interval, Iso86012Interval_hour_to_second)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "PT22H44M55.666S"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_HOUR_TO_SECOND\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_HOUR_TO_SECOND, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_HOUR_TO_SECOND);
	ASSERT_TRUE(is.interval_sign == SQL_FALSE);
	ASSERT_TRUE(is.intval.day_second.hour == 22);
	ASSERT_TRUE(is.intval.day_second.minute == 44);
	ASSERT_TRUE(is.intval.day_second.second == 55);
	ASSERT_TRUE(is.intval.day_second.fraction == 666000); // def prec: 6
}

TEST_F(ConvertSQL2C_Interval, Iso86012Interval_minute_to_second)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "PT44M.666S"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_MINUTE_TO_SECOND\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_MINUTE_TO_SECOND, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_TRUE(is.interval_type == SQL_IS_MINUTE_TO_SECOND);
	ASSERT_TRUE(is.interval_sign == SQL_FALSE);
	ASSERT_TRUE(is.intval.day_second.minute == 44);
	ASSERT_TRUE(is.intval.day_second.second == 0);
	ASSERT_TRUE(is.intval.day_second.fraction == 666000); // def prec: 6
}

TEST_F(ConvertSQL2C_Interval, Iso86012Interval_extra_field_22018)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "PT1H44M55.666S" // 1H is not a MIN-TO-SEC
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_MINUTE_TO_SECOND\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_MINUTE_TO_SECOND, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22018");
}

TEST_F(ConvertSQL2C_Interval, Iso86012Interval_invalid_char_22018)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "PT1H4x4M55.666S" // x - invalid
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_MINUTE_TO_SECOND\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_MINUTE_TO_SECOND, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22018");
}

TEST_F(ConvertSQL2C_Interval, Iso86012Interval_repeated_valid_22018)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "PT1H44MM55.666S" // 44MM, 2nd M
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_MINUTE_TO_SECOND\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_MINUTE_TO_SECOND, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22018");
}

TEST_F(ConvertSQL2C_Interval, Iso86012Interval_plus_minus_22018)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "PT-44M+55.666S" // +M -S
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_MINUTE_TO_SECOND\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_MINUTE_TO_SECOND, &is,
			sizeof(is), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
	assertState(L"22018");
}

TEST_F(ConvertSQL2C_Interval, Iso8601_year2WChar)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "P1Y"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_YEAR\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_WCHAR, wbuff, sizeof(wbuff),
			&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  EXPECT_EQ(ind_len, sizeof(SQLWCHAR) * (sizeof("1") - /*\0*/1));
	ASSERT_STREQ(wbuff, L"1");
}

TEST_F(ConvertSQL2C_Interval, Iso8601_year_to_month2WChar)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "P-1Y-2M"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_YEAR_TO_MONTH\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_WCHAR, wbuff, sizeof(wbuff),
			&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  EXPECT_EQ(ind_len, sizeof(SQLWCHAR) * (sizeof("-1-2") - /*\0*/1));
	ASSERT_STREQ(wbuff, L"-1-2");
}

TEST_F(ConvertSQL2C_Interval, Iso8601_day_to_hour2WChar)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "P1DT2h"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_DAY_TO_HOUR\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_WCHAR, wbuff, sizeof(wbuff),
			&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  EXPECT_EQ(ind_len, sizeof(SQLWCHAR) * (sizeof("1 2") - /*\0*/1));
	ASSERT_STREQ(wbuff, L"1 2");
}

TEST_F(ConvertSQL2C_Interval, Iso8601_day_to_minute2WChar)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "P1DT2H3M"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_DAY_TO_MINUTE\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_WCHAR, wbuff, sizeof(wbuff),
			&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  EXPECT_EQ(ind_len, sizeof(SQLWCHAR) * (sizeof("1 2:3") - /*\0*/1));
	ASSERT_STREQ(wbuff, L"1 2:3");
}

TEST_F(ConvertSQL2C_Interval, Iso8601_day_to_second2WChar)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "P1DT2H3M4S"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_DAY_TO_SECOND\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_WCHAR, wbuff, sizeof(wbuff),
			&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  EXPECT_EQ(ind_len, sizeof(SQLWCHAR) * (sizeof("1 2:3:4") - /*\0*/1));
	ASSERT_STREQ(wbuff, L"1 2:3:4");
}

TEST_F(ConvertSQL2C_Interval, Iso8601_hour_to_second2WChar)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "PT2H3M4.5555S"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_HOUR_TO_SECOND\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_WCHAR, wbuff, sizeof(wbuff),
			&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	SQLHDESC ard;
  ret = SQLGetStmtAttr(stmt, SQL_ATTR_APP_ROW_DESC, &ard, 0, NULL);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  ret = SQLSetDescField(ard, 1, SQL_DESC_PRECISION, (SQLPOINTER)3, 0);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
	// data ptr is reset by direct desc field setting
  ret = SQLSetDescField(ard, 1, SQL_DESC_DATA_PTR, (SQLPOINTER)wbuff, 0);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  EXPECT_EQ(ind_len, sizeof(SQLWCHAR) * (sizeof("2:3:4.555") - /*\0*/1));
	ASSERT_STREQ(wbuff, L"2:3:4.555");
}

TEST_F(ConvertSQL2C_Interval, Iso8601_minute_to_second2Char)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "PT3M4.5555S"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_MINUTE_TO_SECOND\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	SQLHDESC ard;
  ret = SQLGetStmtAttr(stmt, SQL_ATTR_APP_ROW_DESC, &ard, 0, NULL);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  ret = SQLSetDescField(ard, 1, SQL_DESC_PRECISION, (SQLPOINTER)4, 0);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
	// data ptr is reset by direct desc field setting
  ret = SQLSetDescField(ard, 1, SQL_DESC_DATA_PTR, (SQLPOINTER)buff, 0);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  EXPECT_EQ(ind_len, sizeof("3:4.5555") - /*\0*/1);
	ASSERT_STREQ((char *)buff, "3:4.5555");
}

TEST_F(ConvertSQL2C_Interval, Iso8601_hour_to_minute2Char)
{
#	undef SQL_VAL
#	undef SQL
#	define SQL_VAL "PT2H3M"
#	define SQL   "SELECT " SQL_VAL

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"INTERVAL_HOUR_TO_MINUTE\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	SQLHDESC ard;
  ret = SQLGetStmtAttr(stmt, SQL_ATTR_APP_ROW_DESC, &ard, 0, NULL);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  ret = SQLSetDescField(ard, 1, SQL_DESC_PRECISION, (SQLPOINTER)4, 0);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
	// data ptr is reset by direct desc field setting
  ret = SQLSetDescField(ard, 1, SQL_DESC_DATA_PTR, (SQLPOINTER)buff, 0);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  EXPECT_EQ(ind_len, sizeof("2:3") - /*\0*/1);
	ASSERT_STREQ((char *)buff, "2:3");
}

} // test namespace

/* set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=78 */
