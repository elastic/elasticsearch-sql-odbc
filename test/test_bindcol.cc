/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <gtest/gtest.h>
#include "connected_dbc.h"

#include <string.h>


namespace test {

class BindCol : public ::testing::Test, public ConnectedDBC {
};


TEST_F(BindCol, ColumnWise) {

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"col_name\", \"type\": \"byte\"}\
  ],\
  \"rows\": [\
    [0],[1],[2],[3],[4],[5],[6],[7],[8],[9]\
  ]\
}\
";

#define ARR_SZ	4
	SQLCHAR buff[ARR_SZ];
	SQLLEN ind_len_buff[ARR_SZ];
	SQLUSMALLINT row_stats[ARR_SZ];
	SQLULEN fetched_rows;

	ret = SQLSetStmtAttr(stmt, SQL_ATTR_ROW_BIND_TYPE,
			(SQLPOINTER)SQL_BIND_BY_COLUMN, 0);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLSetStmtAttr(stmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)ARR_SZ, 0);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLSetStmtAttr(stmt, SQL_ATTR_ROW_STATUS_PTR, row_stats, 0);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLSetStmtAttr(stmt, SQL_ATTR_ROWS_FETCHED_PTR, &fetched_rows, 0);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLBindCol(stmt, /*col#*/1, SQL_C_TINYINT, buff, sizeof(buff[0]),
			ind_len_buff);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	prepareStatement(json_answer);

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	EXPECT_EQ(fetched_rows, ARR_SZ);
	for (SQLUINTEGER i = 0; i < ARR_SZ; i ++) {
		EXPECT_EQ(buff[i], i);
		EXPECT_EQ(row_stats[i], SQL_ROW_SUCCESS);
	}

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	EXPECT_EQ(fetched_rows, ARR_SZ);
	for (SQLUINTEGER i = 0; i < ARR_SZ; i ++) {
		EXPECT_EQ(buff[i], ARR_SZ + i);
		EXPECT_EQ(row_stats[i], SQL_ROW_SUCCESS);
	}

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	EXPECT_EQ(fetched_rows, 10 - 2 * ARR_SZ);
	for (SQLUINTEGER i = 0; i < fetched_rows; i ++) {
		EXPECT_EQ(buff[i], 2 * ARR_SZ + i);
		EXPECT_EQ(row_stats[i], SQL_ROW_SUCCESS);
	}
	EXPECT_EQ(row_stats[2], SQL_ROW_NOROW);
	EXPECT_EQ(row_stats[3], SQL_ROW_NOROW);
}


} // test namespace

