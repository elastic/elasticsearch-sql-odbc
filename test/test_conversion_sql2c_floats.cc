/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <gtest/gtest.h>
#include "connected_dbc.h"

#include <string.h>
#include <math.h>

/* placeholders; will be undef'd and redef'd */
#define SQL_SCALE
#define SQL_RAW
#define SQL_VAL
#define SQL /* attached for troubleshooting purposes */

namespace test {

class ConvertSQL2C_Floats : public ::testing::Test, public ConnectedDBC {
};


TEST_F(ConvertSQL2C_Floats, ScaledFloat2Char_scale_default) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "0.98765432100123456789" //20 fractional digits
#define SQL "CAST(" SQL_VAL " AS SCALED_FLOAT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"scaled_float\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLCHAR buff[sizeof(SQL_VAL)];
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, &buff, sizeof(buff), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len , /*0.*/2 + /* max ES/SQL double scale */19);
  //std::cerr << buff << std::endl;
  EXPECT_EQ(memcmp(buff, SQL_VAL, /*0.*/2+/*x64 dbl precision*/15), 0);
}


TEST_F(ConvertSQL2C_Floats, Float2Char_scale_default) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "0.98765432109876543219" //20 fractional digits
#define SQL "CAST(" SQL_VAL " AS DOUBLE)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"float\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLCHAR buff[sizeof(SQL_VAL)];
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, &buff, sizeof(buff), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len , /*0.*/2 + /* max ES/SQL double scale */7);
}


TEST_F(ConvertSQL2C_Floats, Float2Char_scale_1) {

#undef SQL_SCALE
#undef SQL_VAL
#undef SQL
#define SQL_SCALE 1
#define SQL_VAL "0.9"
#define SQL "CAST(" SQL_VAL " AS DOUBLE)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"float\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  // set scale
  SQLHDESC ard;
  ret = SQLGetStmtAttr(stmt, SQL_ATTR_APP_ROW_DESC, &ard, 0, NULL);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  ret = SQLSetDescField(ard, /*col#*/1, SQL_DESC_SCALE, (SQLPOINTER)SQL_SCALE,
      0);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  SQLCHAR buff[sizeof(SQL_VAL)];
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, &buff, sizeof(buff), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len , sizeof(SQL_VAL) - 1);
  EXPECT_STREQ((char/*4gtest*/*)buff, SQL_VAL);
}


TEST_F(ConvertSQL2C_Floats, Float2WChar) {

#undef SQL_SCALE
#undef SQL_VAL
#undef SQL
#define SQL_SCALE 3
#define SQL_VAL "-128.998"
#define SQL "CAST(" SQL_VAL " AS FLOAT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"float\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  // set scale
  SQLHDESC ard;
  ret = SQLGetStmtAttr(stmt, SQL_ATTR_APP_ROW_DESC, &ard, 0, NULL);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  ret = SQLSetDescField(ard, /*col#*/1, SQL_DESC_SCALE, (SQLPOINTER)SQL_SCALE,
      0);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  SQLWCHAR wbuff[sizeof(SQL_VAL)];
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_WCHAR, &wbuff, sizeof(wbuff),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len / sizeof(*wbuff), sizeof(SQL_VAL) - /*\0*/1);
  EXPECT_STREQ((wchar_t/*4gtest*/*)wbuff, MK_WPTR(SQL_VAL));
}


TEST_F(ConvertSQL2C_Floats, Float2Char) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "-128.998"
#define SQL "CAST(" SQL_VAL " AS FLOAT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"float\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLCHAR buff[sizeof(SQL_VAL)];
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, &buff, sizeof(buff), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  // GE here, since the default scale is high, so more digits would be avail
  EXPECT_GE(ind_len / sizeof(*buff), sizeof(SQL_VAL) - /*\0*/1);
  double sql_val = atof((char *)SQL_VAL);
  double buff2dbl = atof((char *)buff);
  EXPECT_LE(fabs(sql_val - buff2dbl), .001);
}


TEST_F(ConvertSQL2C_Floats, Float2WChar_dotzero) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "0.0"
#define SQL "CAST(" SQL_VAL " AS FLOAT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"float\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLWCHAR wbuff[sizeof(SQL_VAL)];
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_WCHAR, &wbuff, sizeof(wbuff),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len / sizeof(*wbuff), sizeof(SQL_VAL) - /*\0*/1);
  EXPECT_STREQ((wchar_t/*4gtest*/*)wbuff, MK_WPTR(SQL_VAL));
}


TEST_F(ConvertSQL2C_Floats, Float2TinyInt) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW -128.998
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS FLOAT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"float\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLSCHAR val;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_TINYINT, &val, sizeof(val),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(val));
  EXPECT_LE((SQLSCHAR)SQL_RAW, val);
}


TEST_F(ConvertSQL2C_Floats, Float2UShort) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 65535.0 // USHRT_MAX .0
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS FLOAT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"float\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLUSMALLINT val;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_USHORT, &val, sizeof(val), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(val));
  EXPECT_EQ((SQLUSMALLINT)SQL_RAW, val);
}


TEST_F(ConvertSQL2C_Floats, Float2Long) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW -2147483648.99 // INT32_MIN .99
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS FLOAT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"float\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLINTEGER val;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_LONG, &val, sizeof(val), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(val));
  EXPECT_LE((SQLINTEGER)SQL_RAW, val);
}


TEST_F(ConvertSQL2C_Floats, Double2Float) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW -2147483648.99 // INT32_MIN .99
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS DOUBLE)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"double\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLREAL val;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_FLOAT, &val, sizeof(val), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(val));
  EXPECT_LE(SQL_RAW, val);
}


TEST_F(ConvertSQL2C_Floats, HalfFloat2Bit_fail_22003) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW -.1
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS DOUBLE)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"half_float\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLCHAR val;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_BIT, &val, sizeof(val), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_FALSE(SQL_SUCCEEDED(ret));
  assertState(L"22003");
}


TEST_F(ConvertSQL2C_Floats, HalfFloat2Bit_truncate_01S07) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 1.1
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS DOUBLE)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"half_float\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLCHAR val;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_BIT, &val, sizeof(val), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  assertState(L"01S07");
  ASSERT_EQ(val, 1);
}


TEST_F(ConvertSQL2C_Floats, HalfFloat2Bit) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 1.0
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS DOUBLE)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"half_float\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLCHAR val;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_BIT, &val, sizeof(val), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  assertState(NULL);
  ASSERT_EQ(val, 1);
}


TEST_F(ConvertSQL2C_Floats, Double2Numeric) {

#undef SQL_SCALE
#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_SCALE 3
#define SQL_PREC 5
#define SQL_RAW 25.212
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS DOUBLE)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"double\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  //
  //set scale
  //
  SQLHDESC ard;
  ret = SQLGetStmtAttr(stmt, SQL_ATTR_APP_ROW_DESC, &ard, 0, NULL);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLSetDescField(ard, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_C_NUMERIC, 0);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  ret = SQLSetDescField(ard, 1, SQL_DESC_PRECISION, (SQLPOINTER)SQL_PREC, 0);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  ret = SQLSetDescField(ard, 1, SQL_DESC_SCALE, (SQLPOINTER)SQL_SCALE, 0);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  ret = SQLSetDescField(ard, /*col#*/1, SQL_DESC_OCTET_LENGTH_PTR,
      (SQLPOINTER)&ind_len, 0);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  SQL_NUMERIC_STRUCT val;
  ret = SQLSetDescField(ard, 1, SQL_DESC_DATA_PTR, (SQLPOINTER) &val, 0);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(val));
  EXPECT_EQ(val.sign, 1);
  EXPECT_EQ(val.scale, SQL_SCALE);
  EXPECT_EQ(val.precision, SQL_PREC);
  EXPECT_EQ(memcmp(val.val, "|b", 2), 0);
}


TEST_F(ConvertSQL2C_Floats, Double2Binary) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW -123.001
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS DOUBLE)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"double\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLDOUBLE val = 0;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_DOUBLE, &val, sizeof(val), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(val));
  EXPECT_EQ(val, SQL_RAW);
}


} // test namespace

