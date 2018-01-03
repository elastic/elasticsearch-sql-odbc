/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include "stdio.h"

#include "connect.h"
#include "log.h"
#include "handles.h"

SQLRETURN EsSQLDriverConnectW
(
		SQLHDBC             hdbc,
		SQLHWND             hwnd,
		/* "A full connection string (see the syntax in "Comments"), a partial
		 * connection string, or an empty string" */
		_In_reads_(cchConnStrIn) SQLWCHAR* szConnStrIn,
		/* "Length of *InConnectionString, in characters if the string is
		 * Unicode, or bytes if string is ANSI or DBCS." */
		SQLSMALLINT         cchConnStrIn,
		/* "Pointer to a buffer for the completed connection string. Upon
		 * successful connection to the target data source, this buffer
		 * contains the completed connection string." */
		_Out_writes_opt_(cchConnStrOutMax) SQLWCHAR* szConnStrOut,
		/* "Length of the *OutConnectionString buffer, in characters." */
		SQLSMALLINT         cchConnStrOutMax,
		/* "Pointer to a buffer in which to return the total number of
		 * characters (excluding the null-termination character) available to
		 * return in *OutConnectionString" */
		_Out_opt_ SQLSMALLINT*        pcchConnStrOut,
		/* "Flag that indicates whether the Driver Manager or driver must
		 * prompt for more connection information" */
		SQLUSMALLINT        fDriverCompletion
)
{
	int n;
	esodbc_state_et state = SQL_STATE_HY000;

	DBG("Input connection string: '"LTPD"' (%d).", szConnStrIn, cchConnStrIn);
	if (! pcchConnStrOut) {
		ERR("null pcchConnStrOut parameter");
		RET_STATE(state);
	}

	//
	// FIXME: parse the connection string.
	//
	
	if (szConnStrOut) {
		n = swprintf(szConnStrOut, cchConnStrOutMax, L"%s;keyword=value",
				szConnStrIn);
		if (n < 0) {
			ERRN("failed to outprint connection string.");
			RET_STATE(state);
		} else {
			*pcchConnStrOut = (SQLSMALLINT)n;
			state = SQL_STATE_00000;
		}
	}

	//
	// FIXME: connect to server
	// TODO: PROTO
	//

	RET_STATE(state);
	//RET_NOT_IMPLEMENTED;
#if 0
/* Options for SQLDriverConnect */
#define SQL_DRIVER_NOPROMPT             0
#define SQL_DRIVER_COMPLETE             1
#define SQL_DRIVER_PROMPT               2
#define SQL_DRIVER_COMPLETE_REQUIRED    3
#endif
}

SQLRETURN EsSQLDisconnect(SQLHDBC ConnectionHandle)
{
	// FIXME: disconnect
	DBG("disconnecting from 0x%p", ConnectionHandle);
	RET_STATE(SQL_STATE_00000);
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
