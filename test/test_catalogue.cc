/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

extern "C" {
#include "catalogue.h"
}

#include "connected_dbc.h"
#include <gtest/gtest.h>


namespace test {

class Catalogue : public ::testing::Test, public ConnectedDBC {
};

TEST_F(Catalogue, Statistics) {
	ret = SQLStatistics(stmt, /* irrelevant: */NULL, 0, NULL, 0, NULL, 0, 0, 0);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	SQLSMALLINT cols;
	ret = SQLNumResultCols(stmt, &cols);
	ASSERT_EQ(cols, /* nr of columns expected */13);
	SQLLEN rows;
	ret = SQLRowCount(stmt, &rows);
	ASSERT_EQ(rows, /* no rows */0);
}

TEST_F(Catalogue, SpecialColumns) {
	ret = SQLSpecialColumns(stmt, /*irrelv:*/0, NULL, 0, NULL, 0, NULL, 0, 0, 0);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	SQLSMALLINT cols;
	ret = SQLNumResultCols(stmt, &cols);
	ASSERT_EQ(cols, /* nr of columns expected */8);
	SQLLEN rows;
	ret = SQLRowCount(stmt, &rows);
	ASSERT_EQ(rows, /* no rows */0);
}

TEST_F(Catalogue, ForeignKeys) {
	ret = SQLForeignKeys(stmt, /* irrelevant: :*/NULL, 0, NULL, 0, NULL, 0,
			NULL, 0, NULL, 0, NULL, 0);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	SQLSMALLINT cols;
	ret = SQLNumResultCols(stmt, &cols);
	ASSERT_EQ(cols, /* nr of columns expected */14);
	SQLLEN rows;
	ret = SQLRowCount(stmt, &rows);
	ASSERT_EQ(rows, /* no rows */0);
}

TEST_F(Catalogue, PrimaryKeys) {
	ret = SQLPrimaryKeys(stmt, /* irrelevant: :*/NULL, 0, NULL, 0, NULL, 0);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	SQLSMALLINT cols;
	ret = SQLNumResultCols(stmt, &cols);
	ASSERT_EQ(cols, /* nr of columns expected */6);
	SQLLEN rows;
	ret = SQLRowCount(stmt, &rows);
	ASSERT_EQ(rows, /* no rows */0);
}

TEST_F(Catalogue, Columns_update_varchar_defs) {
	const char response[] = "{"
		"\"columns\":["
			"{\"name\":\"TABLE_CAT\", \"type\":\"keyword\"},"
			"{\"name\":\"TABLE_SCHEM\", \"type\":\"keyword\"},"
			"{\"name\":\"TABLE_NAME\", \"type\":\"keyword\"},"
			"{\"name\":\"COLUMN_NAME\", \"type\":\"keyword\"},"
			"{\"name\":\"DATA_TYPE\", \"type\":\"integer\"},"
			"{\"name\":\"TYPE_NAME\", \"type\":\"keyword\"},"
			"{\"name\":\"COLUMN_SIZE\", \"type\":\"integer\"},"
			"{\"name\":\"BUFFER_LENGTH\", \"type\":\"integer\"},"
			"{\"name\":\"DECIMAL_DIGITS\", \"type\":\"integer\"},"
			"{\"name\":\"NUM_PREC_RADIX\", \"type\":\"integer\"},"
			"{\"name\":\"NULLABLE\", \"type\":\"integer\"},"
			"{\"name\":\"REMARKS\", \"type\":\"keyword\"},"
			"{\"name\":\"COLUMN_DEF\", \"type\":\"keyword\"},"
			"{\"name\":\"SQL_DATA_TYPE\", \"type\":\"integer\"},"
			"{\"name\":\"SQL_DATETIME_SUB\", \"type\":\"integer\"},"
			"{\"name\":\"CHAR_OCTET_LENGTH\", \"type\":\"integer\"},"
			"{\"name\":\"ORDINAL_POSITION\", \"type\":\"integer\"},"
			"{\"name\":\"IS_NULLABLE\", \"type\":\"keyword\"}"
		"],"
		"\"rows\":["
			"[\"some_catalog\",null,\"some_table\",null,12,\"TEXT\",2147483647,"
				"2147483647,null,null,1,null,null,12,0,2147483647,1,\"YES\"],"
			"[\"some_catalog\",null,\"some_table\",null,12,\"KEYWORD\",32766,"
				"2147483647,null,null,1,null,null,12,0,2147483647,2,\"YES\"]"
		"]"
	"}";

	/* setting this attribute should not affect the outcome */
	ret = SQLSetStmtAttr(stmt, SQL_ATTR_MAX_LENGTH, (SQLPOINTER)INT_MAX,
			SQL_IS_UINTEGER);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	prepareStatement(response);

	/* needs to be lower than ESODBC_MAX_KEYWORD_PRECISION (~32k) */
#	define VARCHAR_LIMIT 333
	HDRH(stmt)->dbc->varchar_limit = VARCHAR_LIMIT;
	HDRH(stmt)->dbc->varchar_limit_str = WSTR_INIT(STR(VARCHAR_LIMIT));

	ret = update_varchar_defs(STMH(stmt));
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLFetch(stmt);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	SQLSMALLINT col_idx[] = {
		SQLCOLS_IDX_COLUMN_SIZE,
		SQLCOLS_IDX_BUFFER_LENGTH,
		SQLCOLS_IDX_CHAR_OCTET_LENGTH
	};
	SQLINTEGER val;
	SQLLEN len_ind;
	for (size_t i = 0; i < sizeof(col_idx)/sizeof(col_idx[0]); i ++) {
		ret = SQLGetData(stmt, col_idx[i], SQL_C_SLONG, &val,
				/*ignored*/0, &len_ind);	
		ASSERT_TRUE(SQL_SUCCEEDED(ret));
		ASSERT_NE(len_ind, SQL_NULL_DATA);
		ASSERT_EQ(val, VARCHAR_LIMIT);
	}

	/* above SYS COLUMNS result contains wrong (integer) type for the following
	 * columns that should be short */
	const SQLUSMALLINT short_cols[] = {
		SQLCOLS_IDX_DATA_TYPE,
		SQLCOLS_IDX_DECIMAL_DIGITS,
		SQLCOLS_IDX_NUM_PREC_RADIX,
		SQLCOLS_IDX_NULLABLE,
		SQLCOLS_IDX_SQL_DATA_TYPE,
		SQLCOLS_IDX_SQL_DATETIME_SUB
	};
	SQLWCHAR col_name[128];
	SQLSMALLINT col_name_len, sql_type, scale, nullable;
	SQLULEN col_size;
	for (size_t i = 0; i < sizeof(short_cols)/sizeof(short_cols[0]); i ++) {
		ret = SQLDescribeCol(stmt, short_cols[i], col_name, sizeof(col_name),
				&col_name_len, &sql_type, &col_size, &scale, &nullable);
		ASSERT_TRUE(SQL_SUCCEEDED(ret));
		ASSERT_EQ(sql_type, SQL_SMALLINT);
	}

	HDRH(stmt)->dbc->varchar_limit = 0;
	memset(&HDRH(stmt)->dbc->varchar_limit_str, 0, sizeof(wstr_st));

#	undef VARCHAR_LIMIT
}

} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
