/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */
#ifndef __CONVERT_H__
#define __CONVERT_H__

#include "error.h"
#include "handles.h"

void convert_init();

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
BOOL update_dst_date(struct tm *now);

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
SQLRETURN c2sql_null(esodbc_rec_st *arec,
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

#define TM_TO_TIMESTAMP_STRUCT(_tmp/*src*/, _tsp/*dst*/, frac) \
	do { \
		(_tsp)->year = (_tmp)->tm_year + 1900; \
		(_tsp)->month = (_tmp)->tm_mon + 1; \
		(_tsp)->day = (_tmp)->tm_mday; \
		(_tsp)->hour = (_tmp)->tm_hour; \
		(_tsp)->minute = (_tmp)->tm_min; \
		(_tsp)->second = (_tmp)->tm_sec; \
		(_tsp)->fraction = frac; \
	} while (0)

#define TIMESTAMP_STRUCT_TO_TM(_tsp/*src*/, _tmp/*dst*/) \
	do { \
		(_tmp)->tm_year = (_tsp)->year - 1900; \
		(_tmp)->tm_mon = (_tsp)->month - 1; \
		(_tmp)->tm_mday = (_tsp)->day; \
		(_tmp)->tm_hour = (_tsp)->hour; \
		(_tmp)->tm_min = (_tsp)->minute; \
		(_tmp)->tm_sec = (_tsp)->second; \
	} while (0)

#endif /* __CONVERT_H__ */
