/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#ifndef __DSN_H__
#define __DSN_H__

#include "EsOdbcDsnBinding.h"
#include "util.h"
#include "defs.h"

#define SUBKEY_ODBCINST		"ODBCINST.INI"
#define SUBKEY_ODBC			"ODBC.INI"

/* attribute keywords used in connection strings */
#define ESODBC_DSN_DRIVER			"Driver"
#define ESODBC_DSN_DESCRIPTION		"Description"
#define ESODBC_DSN_DSN				"DSN"
#define ESODBC_DSN_PWD				"PWD"
#define ESODBC_DSN_UID				"UID"
#define ESODBC_DSN_SAVEFILE			"SAVEFILE"
#define ESODBC_DSN_FILEDSN			"FILEDSN"
#define ESODBC_DSN_CLOUD_ID			"CloudID"
#define ESODBC_DSN_SERVER			"Server"
#define ESODBC_DSN_PORT				"Port"
#define ESODBC_DSN_SECURE			"Secure"
#define ESODBC_DSN_CA_PATH			"CAPath"
#define ESODBC_DSN_TIMEOUT			"Timeout"
#define ESODBC_DSN_FOLLOW			"Follow"
#define ESODBC_DSN_CATALOG			"Catalog"
#define ESODBC_DSN_PACKING			"Packing"
#define ESODBC_DSN_COMPRESSION		"Compression"
#define ESODBC_DSN_MAX_FETCH_SIZE	"MaxFetchSize"
#define ESODBC_DSN_MAX_BODY_SIZE_MB	"MaxBodySizeMB"
#define ESODBC_DSN_APPLY_TZ			"ApplyTZ"
#define ESODBC_DSN_EARLY_EXEC		"EarlyExecution"
#define ESODBC_DSN_SCI_FLOATS		"ScientificFloats"
#define ESODBC_DSN_VARCHAR_LIMIT	"VarcharLimit"
#define ESODBC_DSN_MFIELD_LENIENT	"MultiFieldLenient"
#define ESODBC_DSN_ESC_PVA			"AutoEscapePVA"
#define ESODBC_DSN_IDX_INC_FROZEN	"IndexIncludeFrozen"
#define ESODBC_DSN_PROXY_ENABLED	"ProxyEnabled"
#define ESODBC_DSN_PROXY_TYPE		"ProxyType"
#define ESODBC_DSN_PROXY_HOST		"ProxyHost"
#define ESODBC_DSN_PROXY_PORT		"ProxyPort"
#define ESODBC_DSN_PROXY_AUTH_ENA	"ProxyAuthEnabled"
#define ESODBC_DSN_PROXY_AUTH_UID	"ProxyAuthUID"
#define ESODBC_DSN_PROXY_AUTH_PWD	"ProxyAuthPWD"
#define ESODBC_DSN_TRACE_ENABLED	"TraceEnabled"
#define ESODBC_DSN_TRACE_FILE		"TraceFile"
#define ESODBC_DSN_TRACE_LEVEL		"TraceLevel"

/* Packing values */
#define ESODBC_DSN_PACK_JSON		"JSON"
#define ESODBC_DSN_PACK_CBOR		"CBOR"
/* Compression values */
#define ESODBC_DSN_CMPSS_AUTO		"auto"
#define ESODBC_DSN_CMPSS_ON			"on"
#define ESODBC_DSN_CMPSS_OFF		"off"
/* Floats printing */
#define ESODBC_DSN_FLTS_DEF			"default"
#define ESODBC_DSN_FLTS_SCI			"scientific"
#define ESODBC_DSN_FLTS_AUTO		"auto"

/* stucture to collect all attributes in a connection string */
typedef struct {
	wstr_st driver;
	wstr_st description;
	wstr_st dsn;
	wstr_st pwd;
	wstr_st uid;
	wstr_st savefile;
	wstr_st filedsn;
	wstr_st cloud_id;
	wstr_st server;
	wstr_st port;
	wstr_st secure;
	wstr_st ca_path;
	wstr_st timeout;
	wstr_st follow;
	wstr_st catalog;
	wstr_st packing;
	wstr_st compression;
	wstr_st max_fetch_size;
	wstr_st max_body_size;
	wstr_st apply_tz;
	wstr_st early_exec;
	wstr_st sci_floats;
	wstr_st varchar_limit;
	wstr_st mfield_lenient;
	wstr_st auto_esc_pva;
	wstr_st idx_inc_frozen;
	wstr_st proxy_enabled;
	wstr_st proxy_type;
	wstr_st proxy_host;
	wstr_st proxy_port;
	wstr_st proxy_auth_enabled;
	wstr_st proxy_auth_uid;
	wstr_st proxy_auth_pwd;
	wstr_st trace_enabled;
	wstr_st trace_file;
	wstr_st trace_level;
#define ESODBC_DSN_ATTRS_COUNT	36

	SQLWCHAR buff[ESODBC_DSN_ATTRS_COUNT * ESODBC_DSN_MAX_ATTR_LEN];
	/* DSN reading/writing functions are passed a SQLSMALLINT length param */
#if SHRT_MAX < ESODBC_DSN_ATTRS_COUNT * ESODBC_DSN_MAX_ATTR_LEN
#error "attrs buffer too large"
#endif
} esodbc_dsn_attrs_st;


/* assign_dsn_attr() success codes. */
#define DSN_NOT_MATCHED		0
#define DSN_NOT_OVERWRITTEN	1
#define DSN_ASSIGNED		2
#define DSN_OVERWRITTEN		3

void TEST_API init_dsn_attrs(esodbc_dsn_attrs_st *attrs);
void assign_dsn_defaults(esodbc_dsn_attrs_st *attrs);
BOOL assign_dsn_attr(esodbc_dsn_attrs_st *attrs,
	wstr_st *keyword, wstr_st *value, BOOL overwrite);

BOOL TEST_API parse_00_list(esodbc_dsn_attrs_st *attrs, SQLWCHAR *list00);
long TEST_API write_00_list(esodbc_dsn_attrs_st *attrs,
	SQLWCHAR *list00, size_t cnt00);

/* "system" from "system information" (cf. SQLDriverConnect), not as
 * in User/System DSN */
int system_dsn_exists(wstr_st *dsn);
BOOL load_system_dsn(esodbc_dsn_attrs_st *attrs, BOOL overwrite);
BOOL write_system_dsn(esodbc_dsn_attrs_st *crr, esodbc_dsn_attrs_st *old);

BOOL TEST_API parse_connection_string(esodbc_dsn_attrs_st *attrs,
	SQLWCHAR *szConnStrIn, SQLSMALLINT cchConnStrIn);
long TEST_API write_connection_string(esodbc_dsn_attrs_st *attrs,
	SQLWCHAR *szConnStrOut, SQLSMALLINT cchConnStrOutMax);

void log_installer_err();
size_t copy_installer_errors(wchar_t *err_buff, size_t eb_max);
int validate_dsn(esodbc_dsn_attrs_st *attrs, const wchar_t *dsn_str,
	wchar_t *err_out, size_t eo_max, BOOL try_connect);
int prompt_user_config(HWND hwnd, BOOL on_conn, esodbc_dsn_attrs_st *attrs,
	driver_callback_ft save_cb);

/* Uncomment to enable 00-list format (vs. connection string,
 * `;`/`|`-separated) at the interface with the GUI API.
 * The .NET framework has an ODBC connection string parser, so the that format
 * will be used on Windows. */
//#define ESODBC_DSN_API_WITH_00_LIST

#endif /* __DSN_H__ */


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
