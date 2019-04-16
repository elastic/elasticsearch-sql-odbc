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

TEST_F(Util, metadata_id_escape)
{
	SQLWCHAR dst_buff[1024];
	wstr_st dst = {dst_buff, 0};
	wstr_st src, exp;

	src = exp = WSTR_INIT("test"); // no escaping needed
	ASSERT_FALSE(metadata_id_escape(&src, &dst, FALSE));
	ASSERT_TRUE(EQ_WSTR(&dst, &exp));

	src = WSTR_INIT("x\\x"); // not an actual escaping -> escape
	exp = WSTR_INIT("x\\\\x");
	ASSERT_TRUE(metadata_id_escape(&src, &dst, FALSE));
	ASSERT_TRUE(EQ_WSTR(&dst, &exp));

	src = exp = WSTR_INIT("x\\_"); // already escaped `_`
	ASSERT_FALSE(metadata_id_escape(&src, &dst, FALSE));
	ASSERT_TRUE(EQ_WSTR(&dst, &exp));

	src = exp = WSTR_INIT("x\\%"); // already escaped `%`
	ASSERT_FALSE(metadata_id_escape(&src, &dst, FALSE));
	ASSERT_TRUE(EQ_WSTR(&dst, &exp));

	src = exp = WSTR_INIT("\\\\"); // `\\`, no force
	ASSERT_FALSE(metadata_id_escape(&src, &dst, FALSE));
	ASSERT_TRUE(EQ_WSTR(&dst, &exp));

	src = WSTR_INIT("\\\\"); // `\\`, force
	exp = WSTR_INIT("\\\\\\\\");
	ASSERT_TRUE(metadata_id_escape(&src, &dst, TRUE));
	ASSERT_TRUE(EQ_WSTR(&dst, &exp));

	src = exp = WSTR_INIT("\\\\\\"); // `\\\`
	ASSERT_FALSE(metadata_id_escape(&src, &dst, FALSE));
	ASSERT_TRUE(EQ_WSTR(&dst, &exp));

	src = exp = WSTR_INIT("\\"); // stand-alone `\`
	ASSERT_FALSE(metadata_id_escape(&src, &dst, FALSE));
	ASSERT_TRUE(EQ_WSTR(&dst, &exp));

	src = WSTR_INIT("\\"); // stand-alone `\`, but forced
	exp = WSTR_INIT("\\\\");
	ASSERT_TRUE(metadata_id_escape(&src, &dst, TRUE));
	ASSERT_TRUE(EQ_WSTR(&dst, &exp));

	src = exp = WSTR_INIT("%"); // stand-alone `\`
	ASSERT_FALSE(metadata_id_escape(&src, &dst, FALSE));
	ASSERT_TRUE(EQ_WSTR(&dst, &exp));

	src = WSTR_INIT("%"); // stand-alone `%`, but forced
	exp = WSTR_INIT("\\%");
	ASSERT_TRUE(metadata_id_escape(&src, &dst, TRUE));
	ASSERT_TRUE(EQ_WSTR(&dst, &exp));

	src = exp = WSTR_INIT("_"); // stand-alone `_`
	ASSERT_FALSE(metadata_id_escape(&src, &dst, FALSE));
	ASSERT_TRUE(EQ_WSTR(&dst, &exp));

	src = WSTR_INIT("_"); // stand-alone `_`, but forced
	exp = WSTR_INIT("\\_");
	ASSERT_TRUE(metadata_id_escape(&src, &dst, TRUE));
	ASSERT_TRUE(EQ_WSTR(&dst, &exp));

	src = exp = WSTR_INIT(""); // empty string
	ASSERT_FALSE(metadata_id_escape(&src, &dst, TRUE));
	ASSERT_TRUE(EQ_WSTR(&dst, &exp));
}

} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
