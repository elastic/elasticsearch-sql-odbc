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
#include "connect.h"

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
	buff[0] = MK_WPTR('0') + ESODBC_ODBC_INTERFACE_CONFORMANCE;
	if (! SQLWritePrivateProfileStringW(lpszDriver,
			MK_WPTR(VAL_NAME_APILEVEL),
			buff, MK_WPTR(SUBKEY_ODBCINST))) {
		ERR("failed to add value `" VAL_NAME_APILEVEL "`.");
		return FALSE;
	}

	memset(buff, 0, sizeof(buff));
	init_dbc(&dbc, NULL);
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
			/* nothing to do, the vales in ODBCINST.INI are removed along with
			 * the subkey by caller, when needed and same happens with DSNs
			 * under ODBC.INI  */
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
#	undef _DSN_END_MARKER
}

static int save_dsn_cb(void *arg, const wchar_t *list00,
	wchar_t *err_out, size_t eo_max, unsigned int flags)
{
	size_t cnt;
	int res;
	esodbc_dsn_attrs_st attrs;
	esodbc_dbc_st dbc;
	BOOL create_new, remove_old = FALSE;
	esodbc_dsn_attrs_st *old_attrs = (esodbc_dsn_attrs_st *)arg;
	wstr_st old_dsn = old_attrs->dsn;

	if (! list00) {
		ERR("invalid NULL 00-list received.");
		return ESODBC_DSN_ISNULL_ERROR;
	}

	init_dsn_attrs(&attrs);
#ifdef ESODBC_DSN_API_WITH_00_LIST
	if (! parse_00_list(&attrs, (SQLWCHAR *)list00)) {
#else
	if (! parse_connection_string(&attrs, (SQLWCHAR *)list00,
			(SQLSMALLINT)wcslen(list00))) {
#endif /* ESODBC_DSN_API_WITH_00_LIST */
		ERR("failed to parse received 00-list.");
		return ESODBC_DSN_INVALID_ERROR;
	}

	/* 
	 * validate the DSN set 
	 */
	if (! (attrs.dsn.cnt & attrs.server.cnt)) {
		ERR("DSN name (" LWPDL ") and server address (" LWPDL ") must not be"
				" empty.", LWSTR(&attrs.dsn), LWSTR(&attrs.server));
		return ESODBC_DSN_INVALID_ERROR;
	}
	init_dbc(&dbc, NULL);
	if (! SQL_SUCCEEDED(config_dbc(&dbc, &attrs))) {
		ERR("test DBC configuration failed.");
		return ESODBC_DSN_INVALID_ERROR;
	}

	/* is it a brand new DSN name or has the DSN name changed? */
	DBG("old DSN: `" LWPDL "`, new DSN: `" LWPDL "`.",
		LWSTR(&old_dsn), LWSTR(&attrs.dsn));
	if ((! old_dsn.cnt) || (! EQ_CASE_WSTR(&old_dsn, &attrs.dsn))) {
		/* check if target DSN (new or old) already exists */
		res = system_dsn_exists(&attrs.dsn);
		if (res < 0) {
			cnt = copy_installer_errors(err_out, eo_max);
			ERR("failed to check if DSN `" LWPDL "` already exists: "
				LWPDL ".", LWSTR(&attrs.dsn), cnt, err_out);
			goto err;
		} else if (res) {
			DBG("overwrite confirmed? %s!", flags & ESODBC_DSN_OVERWRITE_FLAG
				? "yes" : "no");
			if (! (flags & ESODBC_DSN_OVERWRITE_FLAG)) {
				return ESODBC_DSN_EXISTS_ERROR;
			}
		}
		/* if an old DSN exists, delete it */
		remove_old = !!old_dsn.cnt;
		create_new = TRUE;
	} else {
		create_new = FALSE;
	}

	/* create or update the DSN */
	if (! write_system_dsn(&attrs, create_new)) {
		cnt = copy_installer_errors(err_out, eo_max);
		ERR("failed to add DSN to the system: " LWPDL ".", cnt, err_out);
		goto err;
	}

	/* only remove old if new is succesfully created (even though the
	 * documentation says otherwise). */
	if (remove_old) {
		assert(old_dsn.cnt);
		if (! SQLRemoveDSNFromIniW(old_dsn.str)) {
			cnt = copy_installer_errors(err_out, eo_max);
			ERR("failed to remove old DSN ` " LWPDL "`: " LWPDL ".",
				LWSTR(&old_dsn), cnt, err_out);
		} else {
			DBG("removed now renamed DSN `" LWPDL "`.", LWSTR(&old_dsn));
		}
	}

	return 0;
err:
	SQLPostInstallerError(ODBC_ERROR_REQUEST_FAILED, NULL);
	return ESODBC_DSN_GENERIC_ERROR;
}


BOOL SQL_API ConfigDSNW(
	HWND     hwndParent,
	WORD     fRequest,
	LPCWSTR   lpszDriver,
	LPCWSTR   lpszAttributes)
{
	esodbc_dsn_attrs_st attrs;
	wstr_st driver;
	int res;
	DWORD ierror = 0;

	TRACE4(_IN, "phWW", hwndParent, fRequest, lpszDriver, lpszAttributes);

	init_dsn_attrs(&attrs);

	/* assign the Driver name; this is not the value of the Driver key in the
	 * registry (i.e. the path to the DLL), which is actually skipped when
	 * loading the config. */
	driver = (wstr_st) {
		(SQLWCHAR *)lpszDriver,
		wcslen(lpszDriver)
	};

	/* If there's a DSN in reveived attributes, load the config from the
	 * registry. Otherwise, populate a new config with defaults. */
	if (! load_system_dsn(&attrs, (SQLWCHAR *)lpszAttributes)) {
		ERR("failed to load system DSN for driver ` " LWPD " ` and "
			"attributes `" LWPDL "`.", LWSTR(&driver), lpszAttributes);
		return FALSE;
	}
	res = assign_dsn_attr(&attrs, &MK_WSTR(ESODBC_DSN_DRIVER), &driver,
			/*overwrite?*/FALSE);
	assert(0 < res);

	switch (fRequest) {
		case ODBC_CONFIG_DSN:
		case ODBC_ADD_DSN:
			res = prompt_user_config(hwndParent, FALSE, &attrs, save_dsn_cb);
			if (res < 0) {
				ierror = ODBC_ERROR_REQUEST_FAILED;
				ERR("failed getting user values.");
			}
			break;

		case ODBC_REMOVE_DSN:
			if (! SQLRemoveDSNFromIniW(attrs.dsn.str)) {
				ierror = ODBC_ERROR_REQUEST_FAILED;
				ERR("failed to remove driver ` " LWPD " ` with "
					"attributes `" LWPDL "`.", LWSTR(&driver), lpszAttributes);
			} else {
				INFO("removed DSN `" LWPDL "` from the system.",
					LWSTR(&attrs.dsn));
			}
			break;

		default:
			ERR("unexpected configuration request type: %hd.", fRequest);
			ierror = ODBC_ERROR_INVALID_REQUEST_TYPE;
	}

	if (ierror) {
		SQLPostInstallerError(ODBC_ERROR_REQUEST_FAILED, NULL);
	}

	TRACE5(_OUT, "dphWW", ierror, hwndParent, fRequest, lpszDriver,
		lpszAttributes);
	return ierror == 0;
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
