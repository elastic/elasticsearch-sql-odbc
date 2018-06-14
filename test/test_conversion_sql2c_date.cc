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

class ConvertSQL2C_Date : public ::testing::Test, public ConnectedDBC {

  protected:
    DATE_STRUCT ds;

  void prepareAndBind(const char *json_answer) {
    prepareStatement(json_answer);

    ret = SQLBindCol(stmt, /*col#*/1, SQL_C_TYPE_DATE, &ds, sizeof(ds),
        &ind_len);
    ASSERT_TRUE(SQL_SUCCEEDED(ret));
  }
};


/* ES/SQL 'date' is actually 'timestamp' */
TEST_F(ConvertSQL2C_Date, Timestamp2Date) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "2345-01-23T00:00:00Z"
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

  EXPECT_EQ(ind_len, sizeof(ds));
  EXPECT_EQ(ds.year, 2345);
  EXPECT_EQ(ds.month, 1);
  EXPECT_EQ(ds.day, 23);
}

TEST_F(ConvertSQL2C_Date, Timestamp2Date_truncate) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "   2345-01-23T12:34:56.789Z  "
#define SQL "CAST(" SQL_VAL "AS DATE)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"CAST(" SQL ")\", \"type\": \"date\"}\
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

  EXPECT_EQ(ind_len, sizeof(ds));
  EXPECT_EQ(ds.year, 2345);
  EXPECT_EQ(ds.month, 1);
  EXPECT_EQ(ds.day, 23);
}


TEST_F(ConvertSQL2C_Date, Date2Date) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "2345-01-23"
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

  EXPECT_EQ(ind_len, sizeof(ds));
  EXPECT_EQ(ds.year, 2345);
  EXPECT_EQ(ds.month, 1);
  EXPECT_EQ(ds.day, 23);
}


TEST_F(ConvertSQL2C_Date, Time2Date_22018) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "10:10:10.1010"
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
  ASSERT_FALSE(SQL_SUCCEEDED(ret));
  assertState(L"22018");
}


TEST_F(ConvertSQL2C_Date, Integer2Date_violation_07006) {

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

