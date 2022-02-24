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

class ColAttribute : public ::testing::Test, public ConnectedDBC {
};


TEST_F(ColAttribute, NumericAttributePtr_SQLSMALLINT) {
	SQLLEN pNumAttr = -1;

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

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLColAttribute(stmt, 1, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL,
			&pNumAttr);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(pNumAttr, SQL_C_DOUBLE);
}

TEST_F(ColAttribute, NumericAttributePtr_SQLLEN) {
	SQLLEN pNumAttr = -1;

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

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLColAttribute(stmt, 1, SQL_DESC_DISPLAY_SIZE, NULL, 0, NULL,
			&pNumAttr);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	// https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/display-size
	// "SQL_FLOAT, SQL_DOUBLE: 24 (a sign, 15 digits, a decimal point, the
	// letter E, a sign, and 3 digits)."
	ASSERT_EQ(pNumAttr, 24);
}

TEST_F(ColAttribute, NumericAttributePtr_SQLULEN) {
	SQLLEN pNumAttr = -1;

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

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLColAttribute(stmt, 1, SQL_DESC_LENGTH, NULL, 0, NULL, &pNumAttr);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(pNumAttr, 0);
}

TEST_F(ColAttribute, NumericAttributePtr_SQLINTEGER) {
	SQLLEN pNumAttr = -1;

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

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLColAttribute(stmt, 1, SQL_DESC_NUM_PREC_RADIX, NULL, 0, NULL,
			&pNumAttr);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	// https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgettypeinfo-function
	// "If the data type is an approximate numeric type, this column contains
	// the value 2 to indicate that COLUMN_SIZE specifies a number of bits."
	ASSERT_EQ(pNumAttr, 2);
}




} // test namespace

