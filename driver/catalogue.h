/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#ifndef __CATALOGUE_H__
#define __CATALOGUE_H__

#include "error.h"
#include "handles.h"


SQLSMALLINT fetch_server_attr(esodbc_dbc_st *dbc, SQLINTEGER attr_id,
	SQLWCHAR *dest, SQLSMALLINT room);
BOOL TEST_API set_current_catalog(esodbc_dbc_st *dbc, wstr_st *catalog);


SQLRETURN EsSQLStatisticsW(
	SQLHSTMT           hstmt,
	_In_reads_opt_(cchCatalogName) SQLWCHAR    *szCatalogName,
	SQLSMALLINT        cchCatalogName,
	_In_reads_opt_(cchSchemaName) SQLWCHAR     *szSchemaName,
	SQLSMALLINT        cchSchemaName,
	_In_reads_opt_(cchTableName) SQLWCHAR      *szTableName,
	SQLSMALLINT        cchTableName,
	SQLUSMALLINT       fUnique,
	SQLUSMALLINT       fAccuracy);
SQLRETURN  EsSQLTablesW(
	SQLHSTMT StatementHandle,
	_In_reads_opt_(NameLength1) SQLWCHAR *CatalogName,
	SQLSMALLINT NameLength1,
	_In_reads_opt_(NameLength2) SQLWCHAR *SchemaName,
	SQLSMALLINT NameLength2,
	_In_reads_opt_(NameLength3) SQLWCHAR *TableName,
	SQLSMALLINT NameLength3,
	_In_reads_opt_(NameLength4) SQLWCHAR *TableType,
	SQLSMALLINT NameLength4);

SQLRETURN EsSQLColumnsW
(
	SQLHSTMT           hstmt,
	_In_reads_opt_(cchCatalogName) SQLWCHAR    *szCatalogName,
	SQLSMALLINT        cchCatalogName,
	_In_reads_opt_(cchSchemaName) SQLWCHAR     *szSchemaName,
	SQLSMALLINT        cchSchemaName,
	_In_reads_opt_(cchTableName) SQLWCHAR      *szTableName,
	SQLSMALLINT        cchTableName,
	_In_reads_opt_(cchColumnName) SQLWCHAR     *szColumnName,
	SQLSMALLINT        cchColumnName
);

SQLRETURN EsSQLSpecialColumnsW
(
	SQLHSTMT           hstmt,
	SQLUSMALLINT       fColType,
	_In_reads_opt_(cchCatalogName) SQLWCHAR    *szCatalogName,
	SQLSMALLINT        cchCatalogName,
	_In_reads_opt_(cchSchemaName) SQLWCHAR     *szSchemaName,
	SQLSMALLINT        cchSchemaName,
	_In_reads_opt_(cchTableName) SQLWCHAR      *szTableName,
	SQLSMALLINT        cchTableName,
	SQLUSMALLINT       fScope,
	SQLUSMALLINT       fNullable
);

SQLRETURN EsSQLForeignKeysW(
	SQLHSTMT           hstmt,
	_In_reads_opt_(cchPkCatalogName) SQLWCHAR    *szPkCatalogName,
	SQLSMALLINT        cchPkCatalogName,
	_In_reads_opt_(cchPkSchemaName) SQLWCHAR     *szPkSchemaName,
	SQLSMALLINT        cchPkSchemaName,
	_In_reads_opt_(cchPkTableName) SQLWCHAR      *szPkTableName,
	SQLSMALLINT        cchPkTableName,
	_In_reads_opt_(cchFkCatalogName) SQLWCHAR    *szFkCatalogName,
	SQLSMALLINT        cchFkCatalogName,
	_In_reads_opt_(cchFkSchemaName) SQLWCHAR     *szFkSchemaName,
	SQLSMALLINT        cchFkSchemaName,
	_In_reads_opt_(cchFkTableName) SQLWCHAR      *szFkTableName,
	SQLSMALLINT        cchFkTableName);

SQLRETURN EsSQLPrimaryKeysW(
	SQLHSTMT           hstmt,
	_In_reads_opt_(cchCatalogName) SQLWCHAR    *szCatalogName,
	SQLSMALLINT        cchCatalogName,
	_In_reads_opt_(cchSchemaName) SQLWCHAR     *szSchemaName,
	SQLSMALLINT        cchSchemaName,
	_In_reads_opt_(cchTableName) SQLWCHAR      *szTableName,
	SQLSMALLINT        cchTableName);

#endif /* __CATALOGUE_H__ */


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=78 : */
