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

class ConvertSQL2C_Interval : public ::testing::Test, public ConnectedDBC {

  protected:
    SQL_INTERVAL_STRUCT is;

  void prepareAndBind(const char *jsonAnswer) {
    prepareStatement(jsonAnswer);

    ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_HOUR, &is, sizeof(is),
        &ind_len);
    ASSERT_TRUE(SQL_SUCCEEDED(ret));
  }
};



TEST_F(ConvertSQL2C_Interval, Integer2Interval_unsupported_HYC00) {

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
  assertState(L"HYC00");
}


TEST_F(ConvertSQL2C_Interval, Integer2Interval_violation_07006) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "1.1"
#define SQL   "select " SQL_VAL

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"select " SQL "\", \"type\": \"double\"}\
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

