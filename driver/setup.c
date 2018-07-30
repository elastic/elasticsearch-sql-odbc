/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */
#include "util.h" /* includes windows.h, needed for odbcinst.h */
#include <odbcinst.h>

#include "defs.h"
#include "tracing.h"
#include "log.h"
#include "info.h"
#include "dsn.h"

#define VAL_NAME_APILEVEL	"APILevel"
#define VAL_NAME_CONNECTFN	"ConnectFunctions"
#define VAL_NAME_ODBCVER	"DriverODBCVer"
#define VAL_NAME_SQLLEVEL	"SQLLevel"

/*
 * Adds vals from:
 *  https://docs.microsoft.com/en-us/sql/odbc/reference/install/driver-specification-subkeys
 */
static BOOL add_subkey_values(LPCWSTR lpszDriver)
{
	SQLWCHAR buff[sizeof("YYY")] = {0};
	esodbc_dbc_st dbc;
	SQLRETURN ret = SQL_SUCCESS;
	SQLSMALLINT supported;

	assert(ESODBC_ODBC_INTERFACE_CONFORMANCE <= 9);
	memset(buff, 0, sizeof(buff));
	buff[0] = MK_WPTR('0') + ESODBC_ODBC_INTERFACE_CONFORMANCE;
	if (! SQLWritePrivateProfileStringW(lpszDriver,
			MK_WPTR(VAL_NAME_APILEVEL),
			buff, MK_WPTR(SUBKEY_ODBCINST))) {
		ERR("failed to add value `" VAL_NAME_APILEVEL "`.");
		return FALSE;
	}

	memset(buff, 0, sizeof(buff));
	memset(&dbc, 0, sizeof(dbc));
	dbc.hdr.type = SQL_HANDLE_DBC;
	ret |= EsSQLGetFunctions(&dbc, SQL_API_SQLCONNECT, &supported);
	buff[0] = supported ? MK_WPTR('Y') : MK_WPTR('N');
	ret |= EsSQLGetFunctions(&dbc, SQL_API_SQLDRIVERCONNECT, &supported);
	buff[1] = supported ? MK_WPTR('Y') : MK_WPTR('N');
	ret |= EsSQLGetFunctions(&dbc, SQL_API_SQLBROWSECONNECT, &supported);
	buff[2] = supported ? MK_WPTR('Y') : MK_WPTR('N');
	if (! SQL_SUCCEEDED(ret)) {
		BUG("failed to read connect functions support.");
		return FALSE;
	}
	if (! SQLWritePrivateProfileStringW(lpszDriver,
			MK_WPTR(VAL_NAME_CONNECTFN),
			buff, MK_WPTR(SUBKEY_ODBCINST))) {
		ERR("failed to add value `" VAL_NAME_CONNECTFN "`.");
		return FALSE;
	}

	if (! SQLWritePrivateProfileStringW(lpszDriver,
			MK_WPTR(VAL_NAME_ODBCVER),
			MK_WPTR(ESODBC_SQL_SPEC_STRING), MK_WPTR(SUBKEY_ODBCINST))) {
		ERR("failed to add value `" VAL_NAME_APILEVEL "`.");
		return FALSE;
	}

	assert(ESODBC_SQL_CONFORMANCE <= 9);
	memset(buff, 0, sizeof(buff));
	buff[0] = MK_WPTR('0') + ESODBC_SQL_CONFORMANCE;
	if (! SQLWritePrivateProfileStringW(lpszDriver,
			MK_WPTR(VAL_NAME_SQLLEVEL),
			buff, MK_WPTR(SUBKEY_ODBCINST))) {
		ERR("failed to add value `" VAL_NAME_APILEVEL "`.");
		return FALSE;
	}

	return TRUE;
}

/* called for every installation and for last removal */
BOOL SQL_API ConfigDriverW(
	HWND    hwndParent,
	WORD    fRequest,
	LPCWSTR  lpszDriver,
	LPCWSTR  lpszArgs,
	LPWSTR   lpszMsg,
	WORD    cbMsgMax,
	WORD   *pcbMsgOut)
{
	BOOL ret = FALSE;

	TRACE7(_IN, "phWWpht", hwndParent, fRequest, lpszDriver, lpszArgs,
		lpszMsg, cbMsgMax, pcbMsgOut);

	switch (fRequest) {
		case ODBC_INSTALL_DRIVER:
			ret = add_subkey_values(lpszDriver);
			if (! ret) {
				SQLPostInstallerError(ODBC_ERROR_REQUEST_FAILED, NULL);
			}
			break;
		case ODBC_REMOVE_DRIVER:
			/* nothing to do, the vales are removed along with the subkey by
			 * caller, when needed. */
			ret = TRUE;
			break;
		default:
			ERR("unexpected configuration request type: %hd.", fRequest);
			SQLPostInstallerError(ODBC_ERROR_INVALID_REQUEST_TYPE, NULL);
			goto end;
	}
end:
	TRACE8(_OUT, "dphWWpht", ret, hwndParent, fRequest, lpszDriver, lpszArgs,
		lpszMsg, cbMsgMax, pcbMsgOut);
	return ret;
}

BOOL SQL_API ConfigDSNW(
	HWND     hwndParent,
	WORD     fRequest,
	LPCWSTR   lpszDriver,
	LPCWSTR   lpszAttributes)
{
	esodbc_dsn_attrs_st *attrs = NULL;
	wstr_st driver;
	SQLWCHAR buff[MAX_REG_VAL_NAME] = {0};
	wstr_st old_dsn = {buff, 0};
	BOOL create_new;
	int res;
	BOOL ret = FALSE;

	TRACE4(_IN, "phWW", hwndParent, fRequest, lpszDriver, lpszAttributes);

	/* If there's a DSN in reveived attributes, load the config from the
	 * registry. Otherwise, populate a new config with defaults. */
	if (! (attrs = load_system_dsn((SQLWCHAR *)lpszAttributes))) {
		ERR("failed to load system DSN for driver ` " LWPD " ` and "
			"attributes `" LWPD "`.", lpszDriver, lpszAttributes);
		goto err;
	}
	/* assign the Driver name; this is not the value of the Driver key in the
	 * registry (i.e. the path to the DLL), which is actually skipped when
	 * loading the config. */
	driver = (wstr_st) {
		(SQLWCHAR *)lpszDriver,
		wcslen(lpszDriver)
	};
	if (assign_dsn_attr(attrs, &MK_WSTR(ESODBC_DSN_DRIVER), &driver,
			/*overwrite?*/FALSE, /*duplicate?*/TRUE) <= 0) {
		ERR("failed to assign driver `" LWPDL "` attribute.", LWSTR(&driver));
		goto err;
	}

	switch (fRequest) {
		case ODBC_ADD_DSN:
		case ODBC_CONFIG_DSN:
			/* save the DSN naming, since this might be changed by the user */
			if (attrs->dsn.cnt) {
				/* attrs->dsn.cnt < MAX_REG_VAL_NAME due to load_sys_dsn() */
				wcscpy(old_dsn.str, attrs->dsn.str);
				old_dsn.cnt = attrs->dsn.cnt;
			}
			/* user-interraction loop */
			while (TRUE) {
				if (! prompt_user_config(hwndParent, attrs, FALSE)) {
					ERR("failed getting user values.");
					goto err;
				}
				/* is it a brand new DSN or has the DSN name changed? */
				if ((! old_dsn.cnt) ||
					(! EQ_CASE_WSTR(&old_dsn, &attrs->dsn))) {
					/* check if target DSN (new or old) already exists */
					res = system_dsn_exists(&attrs->dsn);
					if (res < 0) {
						ERR("failed to check if DSN `" LWPDL "` already "
							"exists.", LWSTR(&attrs->dsn));
						goto err;
					} else if (res) {
						res = prompt_user_overwrite(hwndParent, &attrs->dsn);
						if (res < 0) {
							ERR("failed to get user input.");
							goto err;
						} else if (! res) {
							/* let user chose a different DSN name */
							continue;
						}
					}
					/* if an old DSN exists, delete it */
					if (old_dsn.cnt) {
						if (! SQLRemoveDSNFromIniW(old_dsn.str)) {
							ERR("failed to remove old DSN ` " LWPDL " `.",
								LWSTR(&old_dsn));
							goto err;
						}
						DBG("removed now renamed DSN `" LWPDL "`.",
							LWSTR(&old_dsn));
					}
					create_new = TRUE;
				} else {
					create_new = FALSE;
				}
				break;
			}
			/* create or update the DSN */
			if (! write_system_dsn(attrs, create_new)) {
				ERR("failed to add DSN to the system.");
			} else {
				ret = TRUE;
			}
			break;

		case ODBC_REMOVE_DSN:
			if (! SQLRemoveDSNFromIniW(attrs->dsn.str)) {
				ERR("failed to remove driver ` " LWPD " ` with "
					"attributes `" LWPD "`.", lpszDriver, lpszAttributes);
			} else {
				INFO("removed DSN `" LWPDL "` from the system.",
					LWSTR(&attrs->dsn));
				ret = TRUE;
			}
			break;

		default:
			ERR("unexpected configuration request type: %hd.", fRequest);
			SQLPostInstallerError(ODBC_ERROR_INVALID_REQUEST_TYPE, NULL);
			goto end;
	}

err:
	if (! ret) {
		SQLPostInstallerError(ODBC_ERROR_REQUEST_FAILED, NULL);
	}
end:
	if (attrs) {
		free_dsn_attrs(attrs);
	}
	TRACE5(_OUT, "dphWW", ret, hwndParent, fRequest, lpszDriver,
		lpszAttributes);
	return ret;
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
