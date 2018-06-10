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

class ConvertSQL2C_Boolean : public ::testing::Test, public ConnectedDBC {

  protected:
    SQLRETURN ret;
    SQLLEN ind_len = SQL_NULL_DATA;

  ConvertSQL2C_Boolean() {
  }

  virtual ~ConvertSQL2C_Boolean() {
  }

  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  void prepareStatement(const SQLWCHAR *sql, const char *json_answer) {
    char *answer = STRDUP(json_answer);
    ASSERT_TRUE(answer != NULL);
    ret =  ATTACH_ANSWER(stmt, answer, strlen(answer));
    ASSERT_TRUE(SQL_SUCCEEDED(ret));
    ret = ATTACH_SQL(stmt, sql, wcslen(sql));
    ASSERT_TRUE(SQL_SUCCEEDED(ret));
  }
};


TEST_F(ConvertSQL2C_Boolean, Boolean2Numeric) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "true"
#define SQL "CAST(" SQL_VAL " AS SQLNUMERIC)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"boolean\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(MK_WPTR(SQL), json_answer);

  SQL_NUMERIC_STRUCT ns;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_NUMERIC, &ns, sizeof(ns), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(ns));
  EXPECT_EQ(ns.sign, 1);
  EXPECT_EQ(ns.scale, 0);
  assert(sizeof(ns.val) == 16);
  EXPECT_EQ(*(uint64_t *)ns.val, 1L);
  EXPECT_EQ(((uint64_t *)ns.val)[1], 0L);
}

TEST_F(ConvertSQL2C_Boolean, Boolean2STinyInt) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "true"
#define SQL "CAST(" SQL_VAL " AS SQLNUMERIC)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"boolean\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(MK_WPTR(SQL), json_answer);

  SQLCHAR c;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_STINYINT, &c, sizeof(c), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(c));
  EXPECT_EQ(c, 1);
}

TEST_F(ConvertSQL2C_Boolean, Boolean2UShort) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "false"
#define SQL "CAST(" SQL_VAL " AS SQLNUMERIC)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"boolean\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(MK_WPTR(SQL), json_answer);

  SQLUSMALLINT si;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_USHORT, &si, sizeof(si), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(si));
  EXPECT_EQ(si, 0);
}

TEST_F(ConvertSQL2C_Boolean, Boolean2SBigInt) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "true"
#define SQL "CAST(" SQL_VAL " AS SQLNUMERIC)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"boolean\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(MK_WPTR(SQL), json_answer);

  SQLBIGINT bi;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_SBIGINT, &bi, sizeof(bi), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(bi));
  EXPECT_EQ(bi, 1);
}

TEST_F(ConvertSQL2C_Boolean, Boolean2Double) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "true"
#define SQL "CAST(" SQL_VAL " AS SQLNUMERIC)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"boolean\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(MK_WPTR(SQL), json_answer);

  SQLDOUBLE dbl;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_DOUBLE, &dbl, sizeof(dbl), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(dbl));
  EXPECT_EQ(dbl, 1.0);
}

} // test namespace

