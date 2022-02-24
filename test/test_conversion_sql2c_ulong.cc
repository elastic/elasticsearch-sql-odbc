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

class ConvertSQL2C_ULong : public ::testing::Test, public ConnectedDBC {
};


TEST_F(ConvertSQL2C_ULong, ULong2Char) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "18446744073709551615"
#define SQL "CAST(" SQL_VAL " AS TEXT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
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

TEST_F(ConvertSQL2C_ULong, ULong2WChar) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "18446744073709551614"
#define SQL "CAST(" SQL_VAL " AS W-TEXT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
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


TEST_F(ConvertSQL2C_ULong, ULong2Byte) {

#undef SQL_VAL
#undef SQL
#define SQL_RAW 127
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS TINYINT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
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

TEST_F(ConvertSQL2C_ULong, ULong2UByte) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 255
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS UTINYINT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
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

TEST_F(ConvertSQL2C_ULong, ULong2UByte_Fail_Range) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 256
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS UTINYINT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
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
  ASSERT_FALSE(SQL_SUCCEEDED(ret));
  assertState(L"22003");
}

TEST_F(ConvertSQL2C_ULong, ULong2Short) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 16384
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS SHORT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLSMALLINT sshrt;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_SHORT, &sshrt, sizeof(sshrt),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(sshrt));
  EXPECT_EQ(sshrt, SQL_RAW);
}

TEST_F(ConvertSQL2C_ULong, ULong2UShort) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 32768
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS USHORT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLUSMALLINT ushrt;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_USHORT, &ushrt, sizeof(ushrt),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(ushrt));
  EXPECT_EQ(ushrt, SQL_RAW);
}

TEST_F(ConvertSQL2C_ULong, ULong2Short_Fail_Range) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 16385
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS SHORT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLSMALLINT sshrt;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_UTINYINT, &sshrt, sizeof(sshrt),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_FALSE(SQL_SUCCEEDED(ret));
  assertState(L"22003");
}

TEST_F(ConvertSQL2C_ULong, ULong2Int) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 2147483647
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS INT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLINTEGER sint;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_LONG, &sint, sizeof(sint),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(sint));
  EXPECT_EQ(sint, SQL_RAW);
}

TEST_F(ConvertSQL2C_ULong, ULong2UInt) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 4294967295
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS UINT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLUINTEGER uint;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_ULONG, &uint, sizeof(uint),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(uint));
  EXPECT_EQ(uint, SQL_RAW);
}

TEST_F(ConvertSQL2C_ULong, ULong2UInt_Fail_Range) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 4294967296
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS SHORT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLUINTEGER uint;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_ULONG, &uint, sizeof(uint),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_FALSE(SQL_SUCCEEDED(ret));
  assertState(L"22003");
}

TEST_F(ConvertSQL2C_ULong, ULong2Long) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 9223372036854775807
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS LONG)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLBIGINT slong;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_SBIGINT, &slong, sizeof(slong),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(slong));
  EXPECT_EQ(slong, SQL_RAW);
}

TEST_F(ConvertSQL2C_ULong, ULong2ULong) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 18446744073709551615
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS ULONG)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLUBIGINT ulong;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_UBIGINT, &ulong, sizeof(ulong),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(ulong));
  EXPECT_EQ(ulong, SQL_RAW);
}

TEST_F(ConvertSQL2C_ULong, ULong2Long_Fail_Range) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 9223372036854775808
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS LONG)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(json_answer);

  SQLBIGINT slong;
  ret = SQLBindCol(stmt, /*col#*/1, SQL_C_SBIGINT, &slong, sizeof(slong),
      &ind_len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  ret = SQLFetch(stmt);
  ASSERT_FALSE(SQL_SUCCEEDED(ret));
  assertState(L"22003");
}

TEST_F(ConvertSQL2C_ULong, ULong2Bit) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 1
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS BIT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
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

TEST_F(ConvertSQL2C_ULong, ULong2Bit_Fail_Range) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 2
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS BIT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
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
  ASSERT_FALSE(SQL_SUCCEEDED(ret));
  assertState(L"22003");
}

TEST_F(ConvertSQL2C_ULong, ULong2Numeric) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 0xDeadBeef
#define SQL_VAL "3735928559" // 0xdeadbeef
#define SQL "CAST(" SQL_VAL " AS SQLNUMERIC)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
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

TEST_F(ConvertSQL2C_ULong, ULong2Float) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 123456
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS FLOAT)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
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

TEST_F(ConvertSQL2C_ULong, ULong2Double) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 18446744073709551615
#define SQL_VAL STR(SQL_RAW)
#define SQL "CAST(" SQL_VAL " AS DOUBLE)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
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

TEST_F(ConvertSQL2C_ULong, ULong2Binary) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 0xBeefed
#define SQL_VAL "12513261" // 0xbeefed
#define SQL "CAST(" SQL_VAL " AS BINARY)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
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


TEST_F(ConvertSQL2C_ULong, ULong2Binary_Fail_Range) {

#undef SQL_RAW
#undef SQL_VAL
#undef SQL
#define SQL_RAW 0xBeefed
#define SQL_VAL "12513261" // 0xbeefed
#define SQL "CAST(" SQL_VAL " AS BINARY)"

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"unsigned_long\"}\
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

