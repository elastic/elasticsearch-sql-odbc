/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

extern "C" {
#include "queries.h"
} // extern C

#include <gtest/gtest.h>


namespace test {

class Queries : public ::testing::Test {
};

TEST_F(Queries, attach_error_sql) {
#undef SRC_STR
#undef SRC_AID1
#undef SRC_AID2
#define SRC_AID1	"parsing_exception"
#define SRC_AID2	"line 1:8: identifiers must not etc."
#define SRC_AID3	400
#define SRC_STR	\
"{\
  \"error\": {\
    \"root_cause\": [\
      {\
        \"type\": \"" SRC_AID1 "\",\
        \"reason\": \"" SRC_AID2 "\"\
      }\
    ],\
    \"type\": \"parsing_exception\",\
    \"reason\": \"line 1:8: identifiers must not start with a digit; please use double quotes\"\
  },\
  \"status\": " STR(SRC_AID3) "\
}"

	esodbc_stmt_st stmt;
	SQLRETURN ret;
	cstr_st body = CSTR_INIT(SRC_STR);

	memset(&stmt, 0, sizeof(stmt));
	ret = attach_error(&stmt, &body, 400);
	ASSERT_EQ(ret, SQL_ERROR);

	ASSERT_EQ(stmt.hdr.diag.state, SQL_STATE_HY000);
	ASSERT_STREQ((wchar_t *)stmt.hdr.diag.text,
			(wchar_t *)MK_WPTR(SRC_AID1 ": " SRC_AID2));
	ASSERT_EQ(stmt.hdr.diag.native_code, SRC_AID3);
}

TEST_F(Queries, attach_error_non_sql) {
#undef SRC_STR
#undef SRC_AID1
#undef SRC_AID2
#undef SRC_AID3
#define SRC_AID1	"parsing_exception"
#define SRC_AID2	"line 1:8: identifiers must not etc."
#define SRC_AID3	400
#define SRC_STR	\
"{\
  \"error\": {\
    \"root_cause\": [\
      {\
        \"tyXXXpe\": \"" SRC_AID1 "\",\
        \"reason\": \"" SRC_AID2 "\"\
      }\
    ],\
    \"type\": \"parsing_exception\",\
    \"reason\": \"line 1:8: identifiers must not start with a digit; please use double quotes\"\
  },\
  \"status\": " STR(SRC_AID3) "\
}"

	esodbc_stmt_st stmt;
	SQLRETURN ret;
	cstr_st body = CSTR_INIT(SRC_STR);

	memset(&stmt, 0, sizeof(stmt));
	ret = attach_error(&stmt, &body, 400);
	ASSERT_EQ(ret, SQL_ERROR);

	ASSERT_EQ(stmt.hdr.diag.state, SQL_STATE_08S01);
	ASSERT_STREQ((wchar_t *)stmt.hdr.diag.text,
			(wchar_t *)MK_WPTR(SRC_STR));
	ASSERT_EQ(stmt.hdr.diag.native_code, SRC_AID3);
}


} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
