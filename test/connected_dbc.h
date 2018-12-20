/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#ifndef __CONNECTED_DBC_H__
#define __CONNECTED_DBC_H__

extern "C" {
#if defined(_WIN32) || defined (WIN32)
#include <windows.h>
#include <tchar.h>
#endif /* _WIN32/WIN32 */

#include <sql.h>
#include <sqlext.h>

#include "queries.h"
} // extern C

#if defined(_WIN32) || defined (WIN32)
#define STRDUP	_strdup
#else /* _WIN32/WIN32 */
#define STRDUP	strdup
#endif /* _WIN32/WIN32 */

/* convenience casting macros */
#define ATTACH_ANSWER(_h, _s, _l)   attach_answer((esodbc_stmt_st *)_h, _s, _l)
#define ATTACH_SQL(_h, _s, _l)      attach_sql((esodbc_stmt_st *)_h, _s, _l)

#define ASSERT_CSTREQ(_c1, _c2) \
	do { \
		ASSERT_EQ(_c1.cnt, _c2.cnt); \
		ASSERT_EQ(strncmp((char *)_c1.str, (char *)_c2.str, _c1.cnt), 0); \
	} while (0)


class ConnectedDBC {
	protected:
		SQLHANDLE env, dbc, stmt;
		SQLRETURN ret;
		SQLLEN ind_len = SQL_NULL_DATA;


	ConnectedDBC();
	virtual ~ConnectedDBC();

	void assertState(const SQLWCHAR *state);
	void assertState(SQLSMALLINT htype, const SQLWCHAR *state);

	// use the test name as SQL (for faster logs lookup)
	void prepareStatement();
	// use an actual SQL statement (if it might be processed)
	void prepareStatement(const SQLWCHAR *sql, const char *jsonAnswer);
	// use test name as SQL and attach given answer
	void prepareStatement(const char *jsonAnswer);
};

#endif /* __CONNECTED_DBC_H__ */
