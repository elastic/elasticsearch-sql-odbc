/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

extern "C" {
#include "util.h"
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


} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
