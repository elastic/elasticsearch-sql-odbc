/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

extern "C" {
#include "dsn.h"
} // extern C

#include <gtest/gtest.h>


namespace test {

class Dsn : public ::testing::Test {
};

TEST_F(Dsn, parse_write_00_list) {
#undef SRC_STR
#define SRC_STR	\
		"Driver={Elasticsearch Driver}\0" \
		"Description={Some description}\0" \
		"DSN=Data_Source_Name\0" \
		"PWD=password\0" \
		"UID=user_id\0" \
		"SAVEFILE=C:\\Temp\\Data_Source_Name.dsn\0" \
		"FILEDSN=C:\\Temp\\Data_Source_Name.dsn\0" \
		"Server=::1\0" \
		"Port=9200\0" \
		"Secure=4\0" \
		"CAPath=C:\\Temp\\Data_Source_Name.pem\0" \
		"Timeout=1000\0" \
		"Follow=1\0" \
		"Catalog=\0" \
		"Packing=JSON\0" \
		"MaxFetchSize=10000\0" \
		"MaxBodySizeMB=100\0" \
		"TraceFile=C:\\Temp\\Data_Source_Name.log\0" \
		"TraceLevel=DEBUG\0" \
		"\0"


	esodbc_dsn_attrs_st attrs;
	SQLWCHAR *src = MK_WPTR(SRC_STR);
	SQLWCHAR dst[sizeof(attrs.buff)/sizeof(*attrs.buff)];
	long written;

	init_dsn_attrs(&attrs);
	ASSERT_TRUE(parse_00_list(&attrs, src));
	written = write_00_list(&attrs, dst, sizeof(dst)/sizeof(*dst));
	ASSERT_TRUE(0 < written);

	ASSERT_TRUE(memcmp(src, dst, written) == 0);
}


TEST_F(Dsn, parse_write_connection_string) {
#undef SRC_STR
#define SRC_STR	\
		"Driver={Elasticsearch Driver};" \
		"Description={Some description};" \
		"DSN=Data_Source_Name;" \
		"PWD=password;" \
		"UID=user_id;" \
		"SAVEFILE=C:\\Temp\\Data_Source_Name.dsn;" \
		"FILEDSN=C:\\Temp\\Data_Source_Name.dsn;" \
		"Server=::1;" \
		"Port=9200;" \
		"Secure=4;" \
		"CAPath=C:\\Temp\\Data_Source_Name.pem;" \
		"Timeout=;" \
		"Follow=;" \
		"Catalog=;" \
		"Packing=JSON;" \
		"MaxFetchSize=10000;" \
		"MaxBodySizeMB=100;" \
		"TraceFile=C:\\Temp\\Data_Source_Name.log;" \
		"TraceLevel=DEBUG;"


	esodbc_dsn_attrs_st attrs;
	wstr_st src = WSTR_INIT(SRC_STR);
	SQLWCHAR dst[sizeof(attrs.buff)/sizeof(*attrs.buff)];
	long written;

	init_dsn_attrs(&attrs);
	ASSERT_TRUE(parse_connection_string(&attrs, src.str,
				(SQLSMALLINT)src.cnt));
	written = write_connection_string(&attrs, dst,
			(SQLSMALLINT)(sizeof(dst)/sizeof(*dst)));
	ASSERT_TRUE(0 < written);

	ASSERT_TRUE(memcmp(src.str, dst, written) == 0);
}

TEST_F(Dsn, write_connection_string_null_str_out) {
#undef SRC_STR
#define SRC_STR	\
		"Driver={Elasticsearch Driver};" \
		"Description={Some description};" \
		"DSN=Data_Source_Name;" \
		"PWD=password;" \
		"UID=user_id;" \
		"SAVEFILE=C:\\Temp\\Data_Source_Name.dsn;" \
		"FILEDSN=C:\\Temp\\Data_Source_Name.dsn;" \
		"Server=::1;" \
		"Port=9200;" \
		"Secure=4;" \
		"CAPath=C:\\Temp\\Data_Source_Name.pem;" \
		"Timeout=;" \
		"Follow=;" \
		"Catalog=;" \
		"Packing=JSON;" \
		"MaxFetchSize=10000;" \
		"MaxBodySizeMB=100;" \
		"TraceFile=C:\\Temp\\Data_Source_Name.log;" \
		"TraceLevel=DEBUG;"


	esodbc_dsn_attrs_st attrs;
	wstr_st src = WSTR_INIT(SRC_STR);
	SQLWCHAR dst[sizeof(attrs.buff)/sizeof(*attrs.buff)];
	long written, counted;

	init_dsn_attrs(&attrs);
	ASSERT_TRUE(parse_connection_string(&attrs, src.str,
				(SQLSMALLINT)src.cnt));
	written = write_connection_string(&attrs, dst,
			(SQLSMALLINT)(sizeof(dst)/sizeof(*dst)));
	ASSERT_TRUE(0 < written);
	counted = write_connection_string(&attrs, NULL, 0);
	ASSERT_EQ(written, counted);
}


} // test namespace

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
