/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <gtest/gtest.h>
#include "connected_dbc.h"

#include <string.h>

/* placeholders; will be undef'd and redef'd */
#define SQL_RAW
#define SQL_VAL
#define SQL /* attached for troubleshooting purposes */

namespace test {

class ConvertSQL2C_String : public ::testing::Test, public ConnectedDBC {
};


TEST_F(ConvertSQL2C_String, String2Char) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "abcdef"
#define SQL "CAST(" SQL_VAL " AS TEXT)"

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

  SQLCHAR buff[sizeof(SQL_VAL)];
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, &buff, sizeof(buff), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(SQL_VAL) - /*\0*/1);
  EXPECT_STREQ((char/*4gtest*/*)buff, SQL_VAL);
}


TEST_F(ConvertSQL2C_String, String2WChar) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "abcdef"
#define SQL "CAST(" SQL_VAL " AS KEYWORD)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"keyword\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLWCHAR buff[sizeof(SQL_VAL)];
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_WCHAR, &buff, sizeof(buff),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(*buff) * (sizeof(SQL_VAL) - /*\0*/1));
  EXPECT_STREQ((wchar_t/*4gtest*/*)buff, MK_WPTR(SQL_VAL));
}


TEST_F(ConvertSQL2C_String, String2Char_zero_copy) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "abcdef"
#define SQL "CAST(" SQL_VAL " AS TEXT)"

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

  SQLCHAR buff[sizeof(SQL_VAL)] = {'x'};
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, &buff, 0, &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(SQL_VAL) - /*\0*/1);
  EXPECT_EQ(buff[0], 'x');
}


TEST_F(ConvertSQL2C_String, String2SLong) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 123
#define SQL_VAL "+" STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS KEYWORD)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"keyword\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLINTEGER val;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_SLONG, &val, sizeof(val), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(val));
  EXPECT_EQ(val, SQL_RAW);
}


TEST_F(ConvertSQL2C_String, String2SLong_min) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW LONG_MIN
#ifdef _WIN32
#define SQL_VAL "-2147483648"
#else // _WIN64
#define SQL_VAL "-9223372036854775808"
#endif // _WIN64
#define SQL "CAST(" SQL_VAL " AS KEYWORD)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"keyword\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLINTEGER val;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_SLONG, &val, sizeof(val), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(val));
  EXPECT_EQ(val, SQL_RAW);
}


TEST_F(ConvertSQL2C_String, String2SLongLong_min) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW LLONG_MIN
#define SQL_VAL "-9223372036854775808"
#define SQL "CAST(" SQL_VAL " AS KEYWORD)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"keyword\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLBIGINT val;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_SBIGINT, &val, sizeof(val),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(val));
  EXPECT_EQ(val, SQL_RAW);
}


TEST_F(ConvertSQL2C_String, String2ULongLong_max) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW ULLONG_MAX
#define SQL_VAL "18446744073709551615"
#define SQL "CAST(" SQL_VAL " AS KEYWORD)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"keyword\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLUBIGINT val;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_UBIGINT, &val, sizeof(val),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(val));
  EXPECT_EQ(val, SQL_RAW);
}


TEST_F(ConvertSQL2C_String, String2ULongLong_fail_22018) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW ULLONG_MAX
#define SQL_VAL "1844674407370955161X"
#define SQL "CAST(" SQL_VAL " AS KEYWORD)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"keyword\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLUBIGINT val;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_UBIGINT, &val, sizeof(val),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_FALSE(SQL_SUCCEEDED(ret));
  assertState(L"22018");
}


TEST_F(ConvertSQL2C_String, String2ULongLong_fail_22003) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW ULLONG_MAX
#define SQL_VAL "18446744073709551616"
#define SQL "CAST(" SQL_VAL " AS KEYWORD)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"keyword\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLUBIGINT val;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_UBIGINT, &val, sizeof(val), 
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_FALSE(SQL_SUCCEEDED(ret));
  assertState(L"22003");
}


TEST_F(ConvertSQL2C_String, String2Float) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW -0.333e-33
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS KEYWORD)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"keyword\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLREAL val = .0;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_FLOAT, &val, sizeof(val), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(val));
  EXPECT_LE(abs(val - SQL_RAW), 1e-33);
}


TEST_F(ConvertSQL2C_String, String2Float_fail_22003) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW -0.333e-307
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS KEYWORD)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"keyword\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLREAL val = .0;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_FLOAT, &val, sizeof(val), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_FALSE(SQL_SUCCEEDED(ret));
  assertState(L"22003"); /* SQL_RAW fits a double, but not a float */
}

TEST_F(ConvertSQL2C_String, String2Double_fail_22003) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW -1e-3080
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS KEYWORD)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"keyword\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLDOUBLE val = 1.;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_DOUBLE, &val, sizeof(val),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_FALSE(SQL_SUCCEEDED(ret));
  assertState(L"22003"); /* SQL_RAW doen't fit a double */
}


TEST_F(ConvertSQL2C_String, String2Bit) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 1.2
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS KEYWORD)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"keyword\"}\
  ],\
  \"rows\": [\
    [\"" SQL_VAL "\"]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLCHAR val = -1;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_BIT, &val, sizeof(val), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  assertState(L"01S07");

  EXPECT_EQ(ind_len, sizeof(val));
  EXPECT_LE(val, 1);
}



} // test namespace

