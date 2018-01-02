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

#ifndef __INFO_H__
#define __INFO_H__

#include "sqlext.h"



SQLRETURN EsSQLGetInfoW(SQLHDBC ConnectionHandle,
		SQLUSMALLINT InfoType, 
		_Out_writes_bytes_opt_(BufferLength) SQLPOINTER InfoValue,
		SQLSMALLINT BufferLength,
		_Out_opt_ SQLSMALLINT *StringLengthPtr);

SQLRETURN EsSQLGetDiagFieldW(
		SQLSMALLINT HandleType, 
		SQLHANDLE Handle,
		SQLSMALLINT RecNumber,
		SQLSMALLINT DiagIdentifier,
		_Out_writes_opt_(_Inexpressible_(BufferLength)) SQLPOINTER DiagInfoPtr,
		SQLSMALLINT BufferLength,
		_Out_opt_ SQLSMALLINT *StringLengthPtr);

SQLRETURN EsSQLGetDiagRecW(
		SQLSMALLINT HandleType,
		SQLHANDLE Handle,
		SQLSMALLINT RecNumber,
		_Out_writes_opt_(6) SQLWCHAR *Sqlstate,
		SQLINTEGER *NativeError,
		_Out_writes_opt_(BufferLength) SQLWCHAR *MessageText,
		SQLSMALLINT BufferLength,
		_Out_opt_ SQLSMALLINT *TextLength);

SQLRETURN EsSQLGetFunctions(SQLHDBC ConnectionHandle,
		SQLUSMALLINT FunctionId, 
		_Out_writes_opt_(_Inexpressible_("Buffer length pfExists points to depends on fFunction value.")) SQLUSMALLINT *Supported);


#endif /* __INFO_H__ */

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
