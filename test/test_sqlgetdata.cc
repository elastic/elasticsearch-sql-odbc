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

class GetData : public ::testing::Test, public ConnectedDBC {
};


TEST_F(GetData, String2Char_chunked) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "12345678901234567890"
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

	SQLCHAR buff[7];

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, sizeof(buff), &ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS_WITH_INFO);
	EXPECT_EQ(ind_len, sizeof(SQL_VAL) - /*\0*/1 -
			0 * (sizeof(buff) - /*truncation \0*/1));
	EXPECT_STREQ((char *)buff, "123456");

	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, sizeof(buff), &ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS_WITH_INFO);
	EXPECT_EQ(ind_len, sizeof(SQL_VAL) - /*\0*/1 -
			1 * (sizeof(buff) - /*truncation \0*/1));
	EXPECT_STREQ((char *)buff, "789012");

	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, sizeof(buff), &ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS_WITH_INFO);
	EXPECT_EQ(ind_len, sizeof(SQL_VAL) - /*\0*/1 -
			2 * (sizeof(buff) - /*truncation \0*/1));
	EXPECT_STREQ((char *)buff, "345678");

	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, sizeof(buff), &ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS);
	EXPECT_EQ(ind_len, sizeof(SQL_VAL) - /*\0*/1 -
			3 * (sizeof(buff) - /*truncation \0*/1));
	EXPECT_STREQ((char *)buff, "90");
	
	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, sizeof(buff), &ind_len);
	ASSERT_EQ(ret, SQL_NO_DATA);
}

TEST_F(GetData, String2Char_whole) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "12345678901234567890"
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

	SQLCHAR buff[2 * sizeof(SQL_VAL)];

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, sizeof(buff), &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	EXPECT_EQ(ind_len, sizeof(SQL_VAL) - /*\0*/1);
	EXPECT_STREQ((char *)buff, SQL_VAL);

	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, sizeof(buff), &ind_len);
	ASSERT_EQ(ret, SQL_NO_DATA);
}


TEST_F(GetData, String2Char_zero_copy) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "12345678901234567890"
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

	SQLCHAR buff[2 * sizeof(SQL_VAL)];

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, 0, &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	EXPECT_EQ(ind_len, sizeof(SQL_VAL) - /*\0*/1);
}


TEST_F(GetData, String2WChar_chunked) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "12345678901234567890"
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

	SQLWCHAR buff[7];

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS_WITH_INFO);
	EXPECT_EQ(ind_len/sizeof(*buff), sizeof(SQL_VAL) - /*\0*/1 -
			0 * (sizeof(buff)/sizeof(*buff) - /*truncation \0*/1));
	EXPECT_STREQ((wchar_t *)buff, MK_WPTR("123456"));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS_WITH_INFO);
	EXPECT_EQ(ind_len/sizeof(*buff), sizeof(SQL_VAL) - /*\0*/1 -
			1 * (sizeof(buff)/sizeof(*buff) - /*truncation \0*/1));
	EXPECT_STREQ((wchar_t *)buff, MK_WPTR("789012"));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS_WITH_INFO);
	EXPECT_EQ(ind_len/sizeof(*buff), sizeof(SQL_VAL) - /*\0*/1 -
			2 * (sizeof(buff)/sizeof(*buff) - /*truncation \0*/1));
	EXPECT_STREQ((wchar_t *)buff, MK_WPTR("345678"));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS);
	EXPECT_EQ(ind_len/sizeof(*buff), sizeof(SQL_VAL) - /*\0*/1 -
			3 * (sizeof(buff)/sizeof(*buff) - /*truncation \0*/1));
	EXPECT_STREQ((wchar_t *)buff, MK_WPTR("90"));
	
	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
}


TEST_F(GetData, String2WChar_whole) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "12345678901234567890"
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

	SQLWCHAR buff[2 * sizeof(SQL_VAL)];

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	EXPECT_EQ(ind_len/sizeof(*buff), sizeof(SQL_VAL) - /*\0*/1);
	EXPECT_STREQ((wchar_t *)buff, MK_WPTR(SQL_VAL));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_NO_DATA);
}


TEST_F(GetData, String2WChar_zero_copy) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "12345678901234567890"
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

	SQLWCHAR buff[2 * sizeof(SQL_VAL)];

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, 0, &ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	EXPECT_EQ(ind_len/sizeof(*buff), sizeof(SQL_VAL) - /*\0*/1);
}

TEST_F(GetData, String2SLong) {

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
	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_SLONG, &val, /*ignored*/0,
			&ind_len);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	EXPECT_EQ(ind_len, sizeof(val));
	EXPECT_EQ(val, SQL_RAW);
	
	ret = SQLGetData(stmt, /*col*/1, SQL_C_SLONG, &val, /*ignored*/0,
			&ind_len);
	/* XXX: not sure what should actually be returned in this fixed-data
	 * subsequent call case -- for now the driver services the request
	 * succesfully every time */
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	//ASSERT_EQ(ret, SQL_NO_DATA);
}


TEST_F(GetData, SLong2WChar_chunked) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "-1234567890"
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

	SQLWCHAR buff[5];
	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS_WITH_INFO);
	EXPECT_EQ(ind_len, (sizeof(SQL_VAL) - 1) * sizeof(*buff));
	EXPECT_STREQ((wchar_t *)buff, MK_WPTR("-123"));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS_WITH_INFO);
	EXPECT_EQ(ind_len, (sizeof(SQL_VAL) - 1) * sizeof(*buff) -
			1 * (sizeof(buff) - /*\0*/sizeof(*buff)));
	EXPECT_STREQ((wchar_t *)buff, MK_WPTR("4567"));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS);
	EXPECT_EQ(ind_len, (sizeof(SQL_VAL) - 1) * sizeof(*buff) -
			2 * (sizeof(buff) - /*\0*/sizeof(*buff)));
	EXPECT_STREQ((wchar_t *)buff, MK_WPTR("890"));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_NO_DATA);
}


TEST_F(GetData, SLong2WChar_whole) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "-1234567890"
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

	SQLWCHAR buff[2 * sizeof(SQL_VAL)];
	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS);
	EXPECT_EQ(ind_len, (sizeof(SQL_VAL) - 1) * sizeof(*buff));
	EXPECT_STREQ((wchar_t *)buff, MK_WPTR(SQL_VAL));
}


TEST_F(GetData, SLong2Char_chunked) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "-1234567890"
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

	SQLCHAR buff[5];
	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS_WITH_INFO);
	EXPECT_EQ(ind_len, (sizeof(SQL_VAL) - 1) * sizeof(*buff));
	EXPECT_STREQ((char *)buff, MK_CPTR("-123"));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS_WITH_INFO);
	EXPECT_EQ(ind_len, (sizeof(SQL_VAL) - 1) * sizeof(*buff) -
			1 * (sizeof(buff) - /*\0*/sizeof(*buff)));
	EXPECT_STREQ((char *)buff, MK_CPTR("4567"));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS);
	EXPECT_EQ(ind_len, (sizeof(SQL_VAL) - 1) * sizeof(*buff) -
			2 * (sizeof(buff) - /*\0*/sizeof(*buff)));
	EXPECT_STREQ((char *)buff, MK_CPTR("890"));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_NO_DATA);
}

TEST_F(GetData, SLong2Char_whole) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "-1234567890"
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

	SQLCHAR buff[sizeof(SQL_VAL)];
	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS);
	EXPECT_EQ(ind_len, sizeof(SQL_VAL) - 1);
	EXPECT_STREQ((char *)buff, SQL_VAL);
}


TEST_F(GetData, ScaledFloat2WChar_chunked) {

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

	SQLWCHAR buff[7];
	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS_WITH_INFO);
	EXPECT_EQ(ind_len , (/*0.*/2 + /* max ES/SQL double scale */15
			- 0 * (sizeof(buff)/sizeof(SQLWCHAR) - 1)) * sizeof(SQLWCHAR));
	EXPECT_STREQ((wchar_t *)buff, MK_WPTR("0.9876"));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS_WITH_INFO);
	EXPECT_EQ(ind_len , (/*0.*/2 + /* max ES/SQL double scale */15
			- 1 * (sizeof(buff)/sizeof(SQLWCHAR) - 1)) * sizeof(SQLWCHAR));
	EXPECT_STREQ((wchar_t *)buff, MK_WPTR("543210"));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS);
	EXPECT_EQ(ind_len , (/*0.*/2 + /* max ES/SQL double scale */15
			- 2 * (sizeof(buff)/sizeof(SQLWCHAR) - 1)) * sizeof(SQLWCHAR));
	EXPECT_STREQ((wchar_t *)buff, MK_WPTR("01234"));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_NO_DATA);
}

TEST_F(GetData, ScaledFloat2WChar_whole) {

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

	SQLWCHAR buff[sizeof(SQL_VAL)];
	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_WCHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS);
	EXPECT_EQ(ind_len , (/*0.*/2 + /* max ES/SQL double scale */15)
			* sizeof(SQLWCHAR));
	/* TODO: convert the value in the test and use it for comparison.
	 * The below value is what the SQL_VAL converts to */
	EXPECT_EQ(wmemcmp(buff, MK_WPTR(SQL_VAL), /*0.*/2+/*dbl*/15), 0);
}


TEST_F(GetData, ScaledFloat2Char_chunked) {

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

	SQLCHAR buff[7];
	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS_WITH_INFO);
	EXPECT_EQ(ind_len , /*0.*/2 + /* max ES/SQL double scale */15
			- 0 * (sizeof(buff) - 1));
	EXPECT_STREQ((char *)buff, "0.9876");

	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS_WITH_INFO);
	EXPECT_EQ(ind_len , /*0.*/2 + /* max ES/SQL double scale */15
			- 1 * (sizeof(buff) - 1));
	EXPECT_STREQ((char *)buff, "543210");

	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS);
	EXPECT_EQ(ind_len , /*0.*/2 + /* max ES/SQL double scale */15
			- 2 * (sizeof(buff) - 1));
	EXPECT_STREQ((char *)buff, "01234");

	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_NO_DATA);
}

TEST_F(GetData, ScaledFloat2Char_whole) {

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
	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLGetData(stmt, /*col*/1, SQL_C_CHAR, buff, sizeof(buff),
			&ind_len);
	ASSERT_EQ(ret, SQL_SUCCESS);
	EXPECT_EQ(ind_len , /*0.*/2 + /* max ES/SQL double scale */15);
	//std::cerr << buff << std::endl;
	EXPECT_EQ(memcmp(buff, SQL_VAL, /*0.*/2+/*x64 dbl precision*/15), 0);
}


} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
