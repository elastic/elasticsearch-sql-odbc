/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

extern "C" {
#include "util.h"
#include "handles.h"
} // extern C

#include <gtest/gtest.h>


namespace test {

class Util : public ::testing::Test {
};

TEST_F(Util, wstr_to_utf8_null_dst) {
#undef SRC_STR
#define SRC_STR	"abcd"
	wstr_st src = WSTR_INIT(SRC_STR);
	cstr_st *dst;

	dst = wstr_to_utf8(&src, NULL);
	ASSERT_TRUE(dst != NULL);
	ASSERT_EQ(dst->cnt, sizeof(SRC_STR) - 1);
	ASSERT_EQ(dst->str[dst->cnt], '\0');
	ASSERT_STREQ((char *)SRC_STR, (char *)dst->str);
	free(dst);
}

TEST_F(Util, wstr_to_utf8_empty_src) {
#undef SRC_STR
#define SRC_STR	""
	wstr_st src = WSTR_INIT(SRC_STR);
	cstr_st *dst;

	dst = wstr_to_utf8(&src, NULL);
	ASSERT_TRUE(dst != NULL);
	ASSERT_EQ(dst->cnt, sizeof(SRC_STR) - 1);
	ASSERT_EQ(dst->str[dst->cnt], '\0');
	ASSERT_STREQ((char *)SRC_STR, (char *)dst->str);
	free(dst);
}

TEST_F(Util, wstr_to_utf8_unicode) {
#undef SRC_STR
#undef SRC_AID
#define SRC_STR	"XäXüXßX"
#define SRC_AID	"X\xC3\xA4X\xC3\xBCX\xC3\x9FX"
	wstr_st src;
	cstr_st dst;
	src.str = MK_WPTR(SRC_STR);
	src.cnt = sizeof(MK_WPTR(SRC_STR))/sizeof(SQLWCHAR) - 1;

	ASSERT_EQ(&dst, wstr_to_utf8(&src, &dst));
	ASSERT_EQ(dst.cnt, sizeof(SRC_AID) - 1);
	ASSERT_EQ(dst.str[dst.cnt], '\0');
	ASSERT_STREQ((char *)SRC_STR, (char *)dst.str);
	free(dst.str);
}

TEST_F(Util, wstr_to_utf8_nts_not_counted) {
#undef SRC_STR
#define SRC_STR	"abcd"
	wstr_st src = WSTR_INIT(SRC_STR);
	cstr_st dst;

	ASSERT_EQ(&dst, wstr_to_utf8(&src, &dst));
	ASSERT_EQ(dst.cnt, sizeof(SRC_STR) - 1);
	ASSERT_EQ(dst.str[dst.cnt], '\0');
	ASSERT_STREQ((char *)SRC_STR, (char *)dst.str);
	free(dst.str);
}

TEST_F(Util, wstr_to_utf8_nts_counted) {
#undef SRC_STR
#define SRC_STR	"abcd"
	wstr_st src = WSTR_INIT(SRC_STR);
	src.cnt ++;
	cstr_st dst;

	ASSERT_EQ(&dst, wstr_to_utf8(&src, &dst));
	ASSERT_EQ(dst.cnt, sizeof(SRC_STR));
	ASSERT_EQ(dst.str[dst.cnt - 1], '\0');
	ASSERT_STREQ((char *)SRC_STR, (char *)dst.str);
	free(dst.str);
}

TEST_F(Util, wstr_to_utf8_no_nts) {
#undef SRC_STR
#undef SRC_AID
#define SRC_AID	"abcd"
#define SRC_STR	"XXX" SRC_AID "XXX"
	wstr_st src = WSTR_INIT(SRC_STR);
	src.str += 3; /*`XXX`*/
	src.cnt = sizeof(SRC_AID) - 1;
	cstr_st dst;

	ASSERT_EQ(&dst, wstr_to_utf8(&src, &dst));
	ASSERT_EQ(dst.cnt, sizeof(SRC_AID) - 1);
	ASSERT_EQ(dst.str[dst.cnt], '\0');
	ASSERT_STREQ((char *)SRC_AID, (char *)dst.str);
	free(dst.str);
}

TEST_F(Util, write_wstr_invalid_avail) {
	wstr_st src = WSTR_INIT(SRC_STR);
	SQLWCHAR dst[sizeof(SRC_STR)];
	esodbc_env_st env = {0};
	SQLSMALLINT used;

	SQLRETURN ret = write_wstr(&env, dst, &src, sizeof(*dst) + 1, &used);
	ASSERT_FALSE(SQL_SUCCEEDED(ret));
}

TEST_F(Util, write_wstr_0_avail) {
	wstr_st src = WSTR_INIT(SRC_STR);
	SQLWCHAR dst[sizeof(SRC_STR)];
	esodbc_env_st env = {0};
	SQLSMALLINT used;

	SQLRETURN ret = write_wstr(&env, dst, &src, /*avail*/0, &used);
	assert(SQL_SUCCEEDED(ret));
	ASSERT_EQ(used, src.cnt * sizeof(*dst));
}

TEST_F(Util, write_wstr_trunc) {
	wstr_st src = WSTR_INIT(SRC_STR);
	SQLWCHAR dst[sizeof(SRC_STR)];
	esodbc_env_st env = {0};
	SQLSMALLINT used;

	SQLRETURN ret = write_wstr(&env, dst, &src,
			(SQLSMALLINT)((src.cnt - 1) * sizeof(*dst)), &used);
	assert(SQL_SUCCEEDED(ret));
	ASSERT_EQ(used, src.cnt * sizeof(*dst));
	ASSERT_EQ(dst[src.cnt - 2], L'\0');
	ASSERT_EQ(wcsncmp(src.str, dst, src.cnt - 2), 0);
}

TEST_F(Util, write_wstr_copy) {
	wstr_st src = WSTR_INIT(SRC_STR);
	SQLWCHAR dst[sizeof(SRC_STR)];
	esodbc_env_st env = {0};
	SQLSMALLINT used;

	SQLRETURN ret = write_wstr(&env, dst, &src, (SQLSMALLINT)sizeof(dst),
			&used);
	assert(SQL_SUCCEEDED(ret));
	ASSERT_EQ(used, src.cnt * sizeof(*dst));
	ASSERT_EQ(dst[src.cnt], L'\0');
	ASSERT_EQ(wcscmp(src.str, dst), 0);
}

TEST_F(Util, ascii_c2w2c)
{
#undef SRC_STR
#define SRC_STR	"abcd"
	SQLCHAR *test = (SQLCHAR *)SRC_STR;
	SQLWCHAR wbuff[2 * sizeof(SRC_STR)] = {(SQLWCHAR)-1};
	SQLCHAR cbuff[2 * sizeof(SRC_STR)] = {(SQLCHAR)-1};
	int c2w, w2c;

	c2w = ascii_c2w(test, wbuff, sizeof(wbuff)/sizeof(*wbuff));
	w2c = ascii_w2c(wbuff, cbuff, sizeof(cbuff));
	ASSERT_EQ(c2w, w2c);
	ASSERT_STREQ((char *)test, (char *)cbuff);
}

TEST_F(Util, ascii_c2w_add_0term)
{
#undef SRC_STR
#define SRC_STR	"abcd"
	SQLCHAR *test = (SQLCHAR *)SRC_STR;
	SQLWCHAR wbuff[sizeof(SRC_STR)] = {(SQLWCHAR)-1};
	SQLWCHAR *wtest = MK_WPTR(SRC_STR);

	ASSERT_EQ(ascii_c2w(test, wbuff, sizeof(SRC_STR) - 1), sizeof(SRC_STR));
	ASSERT_STREQ((wchar_t *)wbuff, (wchar_t *)wtest);
}

} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
