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

class ConvertSQL2C_Ints : public ::testing::Test, public ConnectedDBC {
};


TEST_F(ConvertSQL2C_Ints, Byte2Char) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "-128"
#define SQL "CAST(" SQL_VAL " AS TEXT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"byte\"}\
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

  EXPECT_EQ(ind_len, sizeof(SQL_VAL) - /*\0*/1);
  EXPECT_STREQ((char/*4gtest*/*)buff, SQL_VAL);
}


TEST_F(ConvertSQL2C_Ints, Byte2Char_null_buff_len) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "-100"
#define SQL "CAST(" SQL_VAL " AS TEXT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"byte\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, NULL, 0, &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(SQL_VAL) - /*\0*/1);
}


TEST_F(ConvertSQL2C_Ints, Short2WChar) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "-128"
#define SQL "CAST(" SQL_VAL " AS W-TEXT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"short\"}\
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

  EXPECT_EQ(ind_len, sizeof(SQLWCHAR) * (sizeof(SQL_VAL) - /*\0*/1));
  EXPECT_STREQ(wbuff, MK_WPTR(SQL_VAL));
}

TEST_F(ConvertSQL2C_Ints, Long2Char_truncate_22003) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "12345678"
#define SQL "CAST(" SQL_VAL " AS TEXT)"

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

  SQLCHAR buff[sizeof(SQL_VAL)/2];
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, &buff, sizeof(buff), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_FALSE(SQL_SUCCEEDED(ret));
  assertState(L"22003");
}


TEST_F(ConvertSQL2C_Ints, Long2Char_zero_copy) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "12345678"
#define SQL "CAST(" SQL_VAL " AS TEXT)"

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

  SQLCHAR buff[sizeof(SQL_VAL)] = {0};
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_CHAR, &buff, 0, &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  EXPECT_EQ(ind_len, sizeof(SQL_VAL) - 1);
  EXPECT_EQ(buff[0], 0); /* nothing copied, since 0 buff size indicated */
}


TEST_F(ConvertSQL2C_Ints, Short2Byte) {

#undef SQL_VAL
#undef SQL
#define SQL_RAW -128
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS SHORT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"short\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLSCHAR schar;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_STINYINT, &schar, sizeof(schar),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(schar));
  EXPECT_EQ(schar, SQL_RAW);
}


TEST_F(ConvertSQL2C_Ints, Short2Byte_truncate_22003) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW -129
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS SHORT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"short\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLSCHAR schar;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_STINYINT, &schar, sizeof(schar),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_FALSE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_FALSE(SQL_SUCCEEDED(ret));
  assertState(L"22003");
}


TEST_F(ConvertSQL2C_Ints, Short2UByte) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 255
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS SHORT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"short\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLCHAR uchar;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_UTINYINT, &uchar, sizeof(uchar),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(uchar));
  EXPECT_EQ(uchar, SQL_RAW);
}


TEST_F(ConvertSQL2C_Ints, Byte2UShort) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 255
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS SHORT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"byte\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLUSMALLINT ushort;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_USHORT, &ushort, sizeof(ushort),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(ushort));
  EXPECT_EQ(ushort, SQL_RAW);
}


TEST_F(ConvertSQL2C_Ints, Byte2UShort_negative2unsigned) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW -1
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS SHORT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"short\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLUSMALLINT ushort;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_USHORT, &ushort, sizeof(ushort),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(ushort));
  EXPECT_EQ(ushort, USHRT_MAX);
}


TEST_F(ConvertSQL2C_Ints, Integer2Null_fail_HY009) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 256
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS INTEGER)"

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

  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_ULONG, NULL, 0, &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_FALSE(SQL_SUCCEEDED(ret));
  assertState(L"HY009");
}


TEST_F(ConvertSQL2C_Ints, Long2BigInt_signed_min) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW LLONG_MIN
//#define SQL_VAL STR(SQL_RAW)
#ifdef _WIN64
#define SQL_VAL "-9223372036854775808" // 0x8000000000000000
#else /* _WIN64 */
#define SQL_VAL "-2147483648"
#endif /* _WIN64 */
#define SQL "CAST(" SQL_VAL " AS INTEGER)"

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

  SQLBIGINT bi;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_SBIGINT, &bi, sizeof(bi), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(bi));
  EXPECT_EQ(bi, SQL_RAW);
}


TEST_F(ConvertSQL2C_Ints, Long2UBigInt_signed_max) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW LLONG_MAX
//#define SQL_VAL STR(SQL_RAW)
#ifdef _WIN64
#define SQL_VAL "9223372036854775807" // 0x7fffffffffffffff
#else /* _WIN64 */
#define SQL_VAL "2147483647"
#endif /* _WIN64 */
#define SQL "CAST(" SQL_VAL " AS INTEGER)"

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

  SQLUBIGINT ubi;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_UBIGINT, &ubi, sizeof(ubi),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(ubi));
  EXPECT_EQ(ubi, SQL_RAW);
}

TEST_F(ConvertSQL2C_Ints, Long2Numeric) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 0xDeadBeef
#define SQL_VAL "3735928559" // 0xdeadbeef
#define SQL "CAST(" SQL_VAL " AS SQLNUMERIC)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"long\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQL_NUMERIC_STRUCT ns;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_NUMERIC, &ns, sizeof(ns), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(ns));
  EXPECT_EQ(ns.sign, 1);
  EXPECT_EQ(ns.scale, 0);
  EXPECT_EQ(ns.precision, sizeof(SQL_VAL) - 1);
  assert(sizeof(ns.val) == 16);
  long long expected = SQL_RAW;
#if REG_DWORD != REG_DWORD_LITTLE_ENDIAN
  expected = _byteswap_ulong(expected);
#endif /* LE */
  EXPECT_EQ(*(uint64_t *)ns.val, expected);
  EXPECT_EQ(((uint64_t *)ns.val)[1], 0L);
}


TEST_F(ConvertSQL2C_Ints, Long2Bit) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 1
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS INTEGER)"

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

  SQLCHAR bit;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_BIT, &bit, sizeof(bit),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(bit));
  EXPECT_EQ(bit, SQL_RAW);
}


TEST_F(ConvertSQL2C_Ints, Long2Float) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 123456
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS INTEGER)"

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

  SQLREAL real;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_FLOAT, &real, sizeof(real),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(real));
  EXPECT_EQ(real, SQL_RAW); /* float equality should hold for casts */
}


TEST_F(ConvertSQL2C_Ints, Long2Double) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW -9223372036854775807 // 0x7fffffffffffffff
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS INTEGER)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"long\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLDOUBLE dbl;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_DOUBLE, &dbl, sizeof(dbl), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(dbl));
  EXPECT_EQ(dbl, SQL_RAW); /* float equality should hold for casts */
}


TEST_F(ConvertSQL2C_Ints, Long2Binary) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 0xBeefed
#define SQL_VAL "12513261" // 0xbeefed
#define SQL "CAST(" SQL_VAL " AS INTEGER)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"long\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLBIGINT bin = LLONG_MAX;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_BINARY, &bin, sizeof(bin), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, /*min aligned size for the value*/4);
  if (4 < sizeof(bin)) {
    bin &= 0x00000000ffffffff; /* the driver has only writtten 4 bytes */
  }
  EXPECT_EQ(bin, SQL_RAW);
}


TEST_F(ConvertSQL2C_Ints, Long2Binary_fail_22003) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 0xBeefed
#define SQL_VAL "12513261" // 0xbeefed
#define SQL "CAST(" SQL_VAL " AS INTEGER)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"long\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLSMALLINT bin;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_BINARY, &bin, sizeof(bin), &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_FALSE(SQL_SUCCEEDED(ret));
  assertState(L"22003");
}


} // test namespace

