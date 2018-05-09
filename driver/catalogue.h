/*
 * ELASTICSEARCH CONFIDENTIAL
 * __________________
 *
 *  [2014] Elasticsearch Incorporated. All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of Elasticsearch Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Elasticsearch Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Elasticsearch Incorporated.
 */

#ifndef __CATALOGUE_H__
#define __CATALOGUE_H__

#include "error.h"
#include "handles.h"


SQLSMALLINT copy_current_catalog(esodbc_dbc_st *dbc, SQLWCHAR *dest,
		SQLSMALLINT room);


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
    _In_reads_opt_(cchCatalogName) SQLWCHAR*    szCatalogName,
    SQLSMALLINT        cchCatalogName,
    _In_reads_opt_(cchSchemaName) SQLWCHAR*     szSchemaName,
    SQLSMALLINT        cchSchemaName,
    _In_reads_opt_(cchTableName) SQLWCHAR*      szTableName,
    SQLSMALLINT        cchTableName,
    _In_reads_opt_(cchColumnName) SQLWCHAR*     szColumnName,
    SQLSMALLINT        cchColumnName
);

SQLRETURN EsSQLSpecialColumnsW
(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fColType,
    _In_reads_opt_(cchCatalogName) SQLWCHAR*    szCatalogName,
    SQLSMALLINT        cchCatalogName,
    _In_reads_opt_(cchSchemaName) SQLWCHAR*     szSchemaName,
    SQLSMALLINT        cchSchemaName,
    _In_reads_opt_(cchTableName) SQLWCHAR*      szTableName,
    SQLSMALLINT        cchTableName,
    SQLUSMALLINT       fScope,
    SQLUSMALLINT       fNullable
);

SQLRETURN EsSQLForeignKeysW(
		SQLHSTMT           hstmt,
		_In_reads_opt_(cchPkCatalogName) SQLWCHAR*    szPkCatalogName,
		SQLSMALLINT        cchPkCatalogName,
		_In_reads_opt_(cchPkSchemaName) SQLWCHAR*     szPkSchemaName,
		SQLSMALLINT        cchPkSchemaName,
		_In_reads_opt_(cchPkTableName) SQLWCHAR*      szPkTableName,
		SQLSMALLINT        cchPkTableName,
		_In_reads_opt_(cchFkCatalogName) SQLWCHAR*    szFkCatalogName,
		SQLSMALLINT        cchFkCatalogName,
		_In_reads_opt_(cchFkSchemaName) SQLWCHAR*     szFkSchemaName,
		SQLSMALLINT        cchFkSchemaName,
		_In_reads_opt_(cchFkTableName) SQLWCHAR*      szFkTableName,
		SQLSMALLINT        cchFkTableName);

SQLRETURN SQL_API EsSQLPrimaryKeysW(
		SQLHSTMT           hstmt,
		_In_reads_opt_(cchCatalogName) SQLWCHAR*    szCatalogName,
		SQLSMALLINT        cchCatalogName,
		_In_reads_opt_(cchSchemaName) SQLWCHAR*     szSchemaName,
		SQLSMALLINT        cchSchemaName,
		_In_reads_opt_(cchTableName) SQLWCHAR*      szTableName,
		SQLSMALLINT        cchTableName);

#endif /* __CATALOGUE_H__ */


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
