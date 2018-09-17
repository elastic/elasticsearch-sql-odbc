/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

extern "C" {
#include "queries.h"
} // extern C

#include "connected_dbc.h"
#include <gtest/gtest.h>


namespace test {

class Queries : public ::testing::Test, public ConnectedDBC {
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
    \"type\": \"" SRC_AID1 "\",\
    \"reason\": \"" SRC_AID2 "\"\
  },\
  \"status\": " STR(SRC_AID3) "\
}"

	SQLRETURN ret;
	cstr_st body = CSTR_INIT(SRC_STR);
	SQLWCHAR *pos, *prev;

	ret = attach_error(stmt, &body, 400);
	ASSERT_EQ(ret, SQL_ERROR);

	ASSERT_EQ(HDRH(stmt)->diag.state, SQL_STATE_HY000);
	/* skip over ODBC's error message format prefix */
	for (pos = HDRH(stmt)->diag.text; pos; pos = wcschr(prev, L']')) {
		prev = pos + 1;
	}
	ASSERT_TRUE(prev != NULL);
	ASSERT_STREQ((wchar_t *)prev, (wchar_t *)MK_WPTR(SRC_AID1 ": " SRC_AID2));
	ASSERT_EQ(HDRH(stmt)->diag.native_code, SRC_AID3);
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
    \"root_cause\": \"" SRC_AID1 "\", \
    \"tyXXXpe\": \"" SRC_AID1 "\",\
    \"reason\": \"" SRC_AID2 "\"\
  },\
  \"status\": " STR(SRC_AID3) "\
}"

	SQLRETURN ret;
	cstr_st body = CSTR_INIT(SRC_STR);
	SQLWCHAR *pos, *prev;

	ret = attach_error(stmt, &body, 400);
	ASSERT_EQ(ret, SQL_ERROR);

	ASSERT_EQ(HDRH(stmt)->diag.state, SQL_STATE_08S01);
	/* skip over ODBC's error message format prefix */
	for (pos = HDRH(stmt)->diag.text; pos; pos = wcschr(prev, L']')) {
		prev = pos + 1;
	}
	ASSERT_TRUE(prev != NULL);
	ASSERT_STREQ(prev, (wchar_t *)MK_WPTR(SRC_STR));
	ASSERT_EQ(HDRH(stmt)->diag.native_code, SRC_AID3);
}


} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
