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

class ConvertSQL2C_Time : public ::testing::Test, public ConnectedDBC {

  protected:
    TIME_STRUCT ts;

  void prepareAndBind(const char *jsonAnswer) {
    prepareStatement(jsonAnswer);

    ret = SQLBindCol(stmt, /*col#*/1, SQL_C_TYPE_TIME, &ts, sizeof(ts),
        &ind_len);
    ASSERT_TRUE(SQL_SUCCEEDED(ret));
  }
};


/* ES/SQL 'date' is actually 'timestamp' */
TEST_F(ConvertSQL2C_Time, Timestamp2Time) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "2345-01-23T12:34:56.000Z"
#define SQL "CAST(" SQL_VAL "AS DATE)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"date\"}\
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

TEST_F(ConvertSQL2C_Time, Timestamp2Time_truncate) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "   2345-01-23T12:34:56.789Z  "
#define SQL "CAST(" SQL_VAL "AS DATE)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"date\"}\
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


TEST_F(ConvertSQL2C_Time, Time2Time) {

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


TEST_F(ConvertSQL2C_Time, Time2Time_truncate) {

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


TEST_F(ConvertSQL2C_Time, Date2Time_22018) {

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


TEST_F(ConvertSQL2C_Time, Integer2Date_violation_07006) {

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

