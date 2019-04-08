/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */
#ifndef __CONVERT_H__
#define __CONVERT_H__

#include "error.h"
#include "handles.h"

void TEST_API convert_init();

SQLRETURN set_param_decdigits(esodbc_rec_st *irec,
	SQLUSMALLINT param_no, SQLSMALLINT decdigits);
SQLSMALLINT get_param_decdigits(esodbc_rec_st *irec);
SQLRETURN set_param_size(esodbc_rec_st *irec,
	SQLUSMALLINT param_no, SQLULEN size);
SQLULEN get_param_size(esodbc_rec_st *irec);

inline void *deferred_address(SQLSMALLINT field_id, size_t pos,
	esodbc_rec_st *rec);

SQLRETURN convertability_check(esodbc_stmt_st *stmt, SQLINTEGER idx,
	int *conv_code);

/*
 * SQL -> C SQL
 */

SQLRETURN sql2c_string(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, const wchar_t *wstr, size_t chars_0);
SQLRETURN sql2c_longlong(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, long long ll);
SQLRETURN sql2c_double(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, double dbl);
/*
 * SQL C -> SQL
 */
inline SQLRETURN c2sql_null(esodbc_rec_st *arec,
	esodbc_rec_st *irec, char *dest, size_t *len);
SQLRETURN c2sql_boolean(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, char *dest, size_t *len);
SQLRETURN c2sql_number(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, double *min, double *max, BOOL fixed, char *dest,
	size_t *len);
SQLRETURN c2sql_varchar(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, char *dest, size_t *len);
SQLRETURN c2sql_timestamp(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, char *dest, size_t *len);
SQLRETURN c2sql_interval(esodbc_rec_st *arec, esodbc_rec_st *irec,
	SQLULEN pos, char *dest, size_t *len);

#endif /* __CONVERT_H__ */
