/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include <gtest/gtest.h>
#include "connected_dbc.h"

#include <string.h>


namespace test {

class BindParam : public ::testing::Test, public ConnectedDBC {
};


TEST_F(BindParam, UnbindColapse) {
	prepareStatement();

	SQLSMALLINT val = 1;
	for (int i = 1; i <= 3; i ++) {
		ret = SQLBindParameter(stmt, /*param nr*/i, SQL_PARAM_INPUT, SQL_C_SSHORT,
				ESODBC_SQL_BOOLEAN, /*size*/0, /*decdigits*/0, &val, sizeof(val),
				/*IndLen*/NULL);
		ASSERT_TRUE(SQL_SUCCEEDED(ret));
	}

	SQLHDESC apd;
	ret = SQLGetStmtAttr(stmt, SQL_ATTR_APP_PARAM_DESC, &apd, SQL_IS_POINTER,
			/*str-len-ptr*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	for (int i  = 2; i <= 3; i ++) {
		/* unbind 2nd and 3rd param (by setting smth else than the data ptr) */
		ret = SQLSetDescField(apd, /*rec no*/i, SQL_DESC_DATA_PTR,
				/*val*/NULL, SQL_IS_POINTER);
		ASSERT_TRUE(SQL_SUCCEEDED(ret));
	}

	/* param count should have now be updated to 1 */
	SQLSMALLINT count;
	ret = SQLGetDescField(apd, /*rec no, ignored*/0, SQL_DESC_COUNT, &count,
			SQL_IS_SMALLINT, /*str-len-ptr*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ASSERT_EQ(count, 1);
}

/* Linked Servers behavior */
TEST_F(BindParam, NoBind) {
	prepareStatement();

	SQLHDESC apd;
	ret = SQLGetStmtAttr(stmt, SQL_ATTR_APP_PARAM_DESC, &apd, SQL_IS_POINTER,
			/*str-len-ptr*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLSetStmtAttr(stmt, SQL_ATTR_PARAM_BIND_TYPE, (SQLPOINTER)0x1,
			SQL_IS_INTEGER);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ret = SQLSetStmtAttr(stmt, SQL_ATTR_PARAM_BIND_TYPE, NULL, SQL_IS_INTEGER);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLSetStmtAttr(stmt, SQL_ATTR_PARAM_BIND_OFFSET_PTR, (SQLPOINTER)0x1,
			SQL_IS_INTEGER);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ret = SQLSetStmtAttr(stmt, SQL_ATTR_PARAM_BIND_OFFSET_PTR, NULL,
			SQL_IS_INTEGER);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLSetDescField(apd, /*rec no*/1, SQL_DESC_OCTET_LENGTH_PTR,
			(SQLPOINTER)0x1, SQL_IS_POINTER);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ret = SQLSetDescField(apd, /*rec no*/1, SQL_DESC_OCTET_LENGTH_PTR, NULL,
			SQL_IS_POINTER);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLSetStmtAttr(stmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER)1,
			SQL_IS_INTEGER);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest(/* no params */NULL);
}

TEST_F(BindParam, InvalidBind) {
	prepareStatement();

	SQLHDESC apd;
	ret = SQLGetStmtAttr(stmt, SQL_ATTR_APP_PARAM_DESC, &apd, SQL_IS_POINTER,
			/*str-len-ptr*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	ret = SQLSetDescField(apd, /*rec no*/1, SQL_DESC_OCTET_LENGTH_PTR,
			(SQLPOINTER)0x1, SQL_IS_POINTER);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ret = SQLSetDescField(apd, /*rec no*/1, SQL_DESC_OCTET_LENGTH_PTR, NULL,
			SQL_IS_POINTER);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	SQLSMALLINT val = 1;
	ret = SQLBindParameter(stmt, /*param nr*/2, SQL_PARAM_INPUT, SQL_C_SSHORT,
			ESODBC_SQL_BOOLEAN, /*size*/0, /*decdigits*/0, &val, sizeof(val),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	cstr_st actual = {NULL, 0};
	ret = serialize_statement((esodbc_stmt_st *)stmt, &actual);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
}

TEST_F(BindParam, BindTwo) {
	prepareStatement();

	SQLSMALLINT v1 = 1;
	ret = SQLBindParameter(stmt, /*param nr*/1, SQL_PARAM_INPUT, SQL_C_SSHORT,
			ESODBC_SQL_BOOLEAN, /*size*/0, /*decdigits*/0, &v1, sizeof(v1),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	SQLSMALLINT v0 = 0;
	ret = SQLBindParameter(stmt, /*param nr*/2, SQL_PARAM_INPUT, SQL_C_SSHORT,
			ESODBC_SQL_BOOLEAN, /*size*/0, /*decdigits*/0, &v0, sizeof(v0),
			/*IndLen*/NULL);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));

	assertRequest("[{\"type\": \"BOOLEAN\", \"value\": true}, "
			"{\"type\": \"BOOLEAN\", \"value\": false}]");
}

} // test namespace

