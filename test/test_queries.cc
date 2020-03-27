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

	cstr_st body = CSTR_INIT(SRC_STR);
	SQLWCHAR *pos, *prev;

	ret = attach_error(stmt, &body, /*is JSON*/TRUE, 400);
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

	cstr_st body = CSTR_INIT(SRC_STR);
	SQLWCHAR *pos, *prev;

	ret = attach_error(stmt, &body, /*is JSON*/TRUE, 400);
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

TEST_F(Queries, cbor_serialize_alloc_body) {
	const size_t bsz = 64;
	wchar_t buff[bsz];
	wstr_st select = WSTR_INIT("SELECT '");
	cstr_st body = {(SQLCHAR *)buff, /*same size as the Q: force realloc*/bsz};

	/* construct a valid query, though irrelevant for the test a.t.p. */
	wmemset(buff, L'*', bsz);
	wmemcpy(buff, select.str, select.cnt);
	buff[bsz - 2] = L'\'';
	buff[bsz - 1] = L'\0';

	DBCH(dbc)->pack_json = FALSE;
	ASSERT_TRUE(SQL_SUCCEEDED(attach_sql(STMH(stmt), (SQLWCHAR *)buff,
					bsz - 1)));
	ASSERT_TRUE(SQL_SUCCEEDED(serialize_statement(STMH(stmt), &body)));
	ASSERT_NE((void *)body.str, (void *)buff);
	free(body.str);
}

TEST_F(Queries, SQLNativeSql) {
#undef SRC_STR
#define SRC_STR	"SELECT 1"
	SQLWCHAR *src = MK_WPTR(SRC_STR);
	SQLWCHAR buff[sizeof(SRC_STR)];
	SQLINTEGER written;

	ret = SQLNativeSql(dbc, src, SQL_NTSL, buff, sizeof(buff)/sizeof(*buff),
			&written);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	assertState(SQL_HANDLE_DBC, NULL);
	ASSERT_STREQ(buff, src);
}

TEST_F(Queries, SQLNativeSql_truncate) {
#undef SRC_STR
#define SRC_STR	"SELECT 1"
	SQLWCHAR *src = MK_WPTR(SRC_STR);
	SQLWCHAR buff[sizeof(SRC_STR) - /*0-term*/1];
	SQLINTEGER written;

	ret = SQLNativeSql(dbc, src, SQL_NTSL, buff, sizeof(buff)/sizeof(*buff),
			&written);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	assertState(SQL_HANDLE_DBC, MK_WPTR("01004"));
	ASSERT_TRUE(wcsncmp(buff, src, sizeof(buff)/sizeof(*buff) - 1) == 0);
}

TEST_F(Queries, SQLNumParams_no_markers) {
	prepareStatement(MK_WPTR("SELECT 1"));
	SQLRETURN ret;
	SQLSMALLINT params = -1;
	ret = SQLNumParams(stmt, &params);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(params, 0);
}

TEST_F(Queries, SQLNumParams_markers) {
	prepareStatement(MK_WPTR("SELECT * FROM foo WHERE bar = ? AND baz = ?"));
	SQLRETURN ret;
	SQLSMALLINT params = -1;
	ret = SQLNumParams(stmt, &params);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(params, 2);
}

TEST_F(Queries, SQLNumParams_quoted) {
	prepareStatement(MK_WPTR("foo ' ?' ?"));
	SQLRETURN ret;
	SQLSMALLINT params = -1;
	ret = SQLNumParams(stmt, &params);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(params, 1);
}

TEST_F(Queries, SQLNumParams_quoted_escaped) {
	prepareStatement(MK_WPTR("foo ' \\?' ? ?"));
	SQLRETURN ret;
	SQLSMALLINT params = -1;
	ret = SQLNumParams(stmt, &params);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(params, 2);
}

TEST_F(Queries, SQLNumParams_escaped) {
	prepareStatement(MK_WPTR("foo  \\? ?"));
	SQLRETURN ret;
	SQLSMALLINT params = -1;
	ret = SQLNumParams(stmt, &params);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(params, 1);
}

TEST_F(Queries, SQLNumParams_invalid) {
	prepareStatement(MK_WPTR("foo  '\\? ?"));
	SQLRETURN ret;
	SQLSMALLINT params;
	ret = SQLNumParams(stmt, &params);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
}

TEST_F(Queries, SQLNumParams_duplicates) {
	prepareStatement(MK_WPTR("foo  ??? ?"));
	SQLRETURN ret;
	SQLSMALLINT params = -1;
	ret = SQLNumParams(stmt, &params);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(params, 1);
}

TEST_F(Queries, SQLNumParams_duplicates_escape) {
	prepareStatement(MK_WPTR("foo ? \\??? ??? ? ??"));
	SQLRETURN ret;
	SQLSMALLINT params = -1;
	ret = SQLNumParams(stmt, &params);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(params, 2);
}

TEST_F(Queries, SQLDescribeCol_wchar) {

#	define COL_NAME "SQLDescribeCol_wchar"

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" COL_NAME "\", \"type\": \"text\"}\
  ],\
  \"rows\": [\
    [\"bar\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	SQLWCHAR col_name[sizeof(COL_NAME)];
	SQLSMALLINT col_name_len, sql_type, scale, nullable;
	SQLULEN col_size;
	ret = SQLDescribeCol(stmt, /*col#*/1, col_name, sizeof(col_name),
			&col_name_len, &sql_type, &col_size, &scale, &nullable);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(col_name_len, sizeof(COL_NAME) - 1);
	ASSERT_STREQ(col_name, MK_WPTR(COL_NAME));
	ASSERT_EQ(sql_type, ES_WVARCHAR_SQL);
	ASSERT_EQ(col_size, INT_MAX);
	ASSERT_EQ(nullable, SQL_NULLABLE_UNKNOWN);
}

TEST_F(Queries, SQLDescribeCol_char) {

#	undef COL_NAME
#	define COL_NAME "SQLDescribeCol_char"

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" COL_NAME "\", \"type\": \"IP\"}\
  ],\
  \"rows\": [\
    [\"1.2.3.4\"]\
  ]\
}\
";
	prepareStatement(json_answer);

	SQLWCHAR col_name[sizeof(COL_NAME)];
	SQLSMALLINT col_name_len, sql_type, scale, nullable;
	SQLULEN col_size;
	ret = SQLDescribeCol(stmt, /*col#*/1, col_name, sizeof(col_name),
			&col_name_len, &sql_type, &col_size, &scale, &nullable);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(col_name_len, sizeof(COL_NAME) - 1);
	ASSERT_STREQ(col_name, MK_WPTR(COL_NAME));
	ASSERT_EQ(sql_type, ES_VARCHAR_SQL);
	ASSERT_EQ(col_size, 45);
	ASSERT_EQ(nullable, SQL_NULLABLE_UNKNOWN);
}

TEST_F(Queries, SQLDescribeCol_varchar_lim) {

#	undef COL_NAME
#	define COL_NAME "SQLDescribeCol_varchar_lim"
#	define VARCHAR_LIMIT 333
#	define CONN_STR CONNECT_STRING "VarcharLimit=" STR(VARCHAR_LIMIT) ";"

	const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" COL_NAME "\", \"type\": \"text\"},\
    {\"name\": \"binary\", \"type\": \"binary\"}\
  ],\
  \"rows\": [\
    [\"foo\", \"binary\"]\
  ]\
}\
";

	/* set varchar limit: this is set onto the varchar types structures, so the
	 * DBC needs "reconnecting" and the corresponding DSN param set */
	ret = SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	assert(SQL_SUCCEEDED(ret));
	ret = SQLFreeHandle(SQL_HANDLE_DBC, dbc);
	assert(SQL_SUCCEEDED(ret));
	ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
	assert(SQL_SUCCEEDED(ret));
	cstr_st types = {0};
	types.str = (SQLCHAR *)strdup(SYSTYPES_ANSWER);
	assert(types.str != NULL);
	types.cnt = sizeof(SYSTYPES_ANSWER) - 1;
	ret = SQLDriverConnect(dbc, (SQLHWND)&types, (SQLWCHAR *)CONN_STR,
			sizeof(CONN_STR) / sizeof(CONN_STR[0]) - 1, NULL, 0, NULL,
			ESODBC_SQL_DRIVER_TEST);
	assert(SQL_SUCCEEDED(ret));
	ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
	assert(SQL_SUCCEEDED(ret));
	assert(stmt != NULL);

	prepareStatement(json_answer);

	SQLWCHAR col_name[sizeof(COL_NAME)];
	SQLSMALLINT col_name_len, sql_type, scale, nullable;
	SQLULEN col_size;
	/* text column */
	ret = SQLDescribeCol(stmt, /*col#*/1, col_name, sizeof(col_name),
			&col_name_len, &sql_type, &col_size, &scale, &nullable);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(col_name_len, sizeof(COL_NAME) - 1);
	ASSERT_STREQ(col_name, MK_WPTR(COL_NAME));
	ASSERT_EQ(sql_type, ES_WVARCHAR_SQL);
	ASSERT_EQ(col_size, VARCHAR_LIMIT); /* limit enforced */
	ASSERT_EQ(nullable, SQL_NULLABLE_UNKNOWN);

	/* binary column */
	ret = SQLDescribeCol(stmt, /*col#*/2, col_name, sizeof(col_name),
			&col_name_len, &sql_type, &col_size, &scale, &nullable);
	ASSERT_TRUE(SQL_SUCCEEDED(ret));
	ASSERT_EQ(col_size, INT_MAX); /* binary col's length must not be affected */
}

} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
