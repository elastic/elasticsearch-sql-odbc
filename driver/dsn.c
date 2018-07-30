/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include "util.h" /* includes windows.h, needed for odbcinst.h */
#include <odbcinst.h>

#include "dsn.h"
#include "log.h"

#define ODBC_REG_SUBKEY_PATH	"SOFTWARE\\ODBC\\ODBC.INI"
#define REG_HKLM				"HKEY_LOCAL_MACHINE"
#define REG_HKCU				"HKEY_CURRENT_USER"

/*
 * returns:
 *   positive on keyword match;
 *   0 on keyword mismatch;
 *   negative on error;
 */
int assign_dsn_attr(esodbc_dsn_attrs_st *attrs,
	wstr_st *keyword, wstr_st *value, BOOL overwrite, BOOL duplicate)
{
	struct {
		wstr_st *kw;
		wstr_st *val;
	} *iter, map[] = {
		{&MK_WSTR(ESODBC_DSN_DRIVER), &attrs->driver},
		{&MK_WSTR(ESODBC_DSN_DESCRIPTION), &attrs->description},
		{&MK_WSTR(ESODBC_DSN_DSN), &attrs->dsn},
		{&MK_WSTR(ESODBC_DSN_PWD), &attrs->pwd},
		{&MK_WSTR(ESODBC_DSN_UID), &attrs->uid},
		{&MK_WSTR(ESODBC_DSN_SAVEFILE), &attrs->savefile},
		{&MK_WSTR(ESODBC_DSN_FILEDSN), &attrs->filedsn},
		{&MK_WSTR(ESODBC_DSN_SERVER), &attrs->server},
		{&MK_WSTR(ESODBC_DSN_PORT), &attrs->port},
		{&MK_WSTR(ESODBC_DSN_SECURE), &attrs->secure},
		{&MK_WSTR(ESODBC_DSN_TIMEOUT), &attrs->timeout},
		{&MK_WSTR(ESODBC_DSN_FOLLOW), &attrs->follow},
		{&MK_WSTR(ESODBC_DSN_CATALOG), &attrs->catalog},
		{&MK_WSTR(ESODBC_DSN_PACKING), &attrs->packing},
		{&MK_WSTR(ESODBC_DSN_MAX_FETCH_SIZE), &attrs->max_fetch_size},
		{&MK_WSTR(ESODBC_DSN_MAX_BODY_SIZE_MB), &attrs->max_body_size},
		{&MK_WSTR(ESODBC_DSN_TRACE_FILE), &attrs->trace_file},
		{&MK_WSTR(ESODBC_DSN_TRACE_LEVEL), &attrs->trace_level},
		{NULL, NULL}
	};

	for (iter = &map[0]; iter->kw; iter ++) {
		if (! EQ_CASE_WSTR(iter->kw, keyword)) {
			continue;
		}
		/* it's a match: has it been assigned already? */
		if (iter->val->cnt) {
			if (! overwrite) {
				INFO("keyword '" LWPDL "' already assigned; "
					"ignoring new `" LWPDL "`, keeping previous `" LWPDL "`.",
					LWSTR(iter->kw), LWSTR(value), LWSTR(iter->val));
				return 1;
			}
			INFO("keyword '" LWPDL "' already assigned: "
				"overwriting previous `" LWPDL "` with new `" LWPDL "`.",
				LWSTR(iter->kw), LWSTR(iter->val), LWSTR(value));
		}
		if (duplicate) {
			if (iter->val->str) {
				free(iter->val->str);
			}
			if (! (iter->val->str = _wcsdup(value->str))) {
				ERR("OOM duplicating wstring of len %zu.", value->cnt);
				return -1;
			} else {
				iter->val->cnt = value->cnt;
			}
		} else {
			*iter->val = *value;
		}
		return 1;
	}

	/* entry not directly relevant to driver config */
	WARN("keyword `" LWPDL "` (with value `" LWPDL "`) not DSN config "
		"specific, so not assigned.", LWSTR(keyword), LWSTR(value));
	return 0;
}

/*
 * Advance position in string, skipping white space.
 * if exended is true, `;` will be treated as white space too.
 */
static SQLWCHAR *skip_ws(SQLWCHAR **pos, SQLWCHAR *end, BOOL extended)
{
	while (*pos < end) {
		switch(**pos) {
			case ' ':
			case '\t':
			case '\r':
			case '\n':
				(*pos)++;
				break;

			case '\0':
				return NULL;

			case ';':
				if (extended) {
					(*pos)++;
					break;
				}
			// no break;

			default:
				return *pos;
		}
	}

	/* end of string reached */
	return NULL;
}

/*
 * Parse a keyword or a value.
 * Within braces, any character is allowed, safe for \0 (i.e. no "bla{bla\0"
 * is supported as keyword or value).
 * Braces within braces are allowed.
 */
static BOOL parse_token(BOOL is_value, SQLWCHAR **pos, SQLWCHAR *end,
	wstr_st *token)
{
	BOOL brace_escaped = FALSE;
	int open_braces = 0;
	SQLWCHAR *start = *pos;
	BOOL stop = FALSE;

	while (*pos < end && (! stop)) {
		switch (**pos) {
			case '\\':
				if (! is_value) {
					ERR("keywords and data source names cannot contain "
						"the backslash.");
					return FALSE;
				}
				(*pos)++;
				break;

			case ' ':
			case '\t':
			case '\r':
			case '\n':
				if (open_braces || is_value) {
					(*pos)++;
				} else {
					stop = TRUE;
				}
				break;

			case '=':
				if (open_braces || is_value) {
					(*pos)++;
				} else {
					stop = TRUE;
				}
				break;

			case ';':
				if (open_braces) {
					(*pos)++;
				} else if (is_value) {
					stop  = TRUE;
				} else {
					ERR("';' found while parsing keyword");
					return FALSE;
				}
				break;

			case '\0':
				if (open_braces) {
					ERR("null terminator found while within braces");
					return FALSE;
				} else if (! is_value) {
					ERR("null terminator found while parsing keyword.");
					return FALSE;
				} /* else: \0 used as delimiter of value string */
				stop = TRUE;
				break;

			case '{':
				if (*pos == start) {
					open_braces ++;
				} else if (open_braces) {
					ERR("token started with opening brace, so can't use "
						"inner braces");
					/* b/c: `{foo{ = }}bar} = val`; `foo{bar}baz` is fine */
					return FALSE;
				}
				(*pos)++;
				break;

			case '}':
				if (open_braces) {
					open_braces --;
					brace_escaped = TRUE;
					stop = TRUE;
				}
				(*pos)++;
				break;

			default:
				(*pos)++;
		}
	}

	if (open_braces) {
		ERR("string finished with open braces.");
		return FALSE;
	}

	token->str = start + (brace_escaped ? 1 : 0);
	token->cnt = (*pos - start) - (brace_escaped ? 2 : 0);
	return TRUE;
}

static SQLWCHAR *parse_separator(SQLWCHAR **pos, SQLWCHAR *end)
{
	if (*pos < end) {
		if (**pos == '=') {
			(*pos)++;
			return *pos;
		}
	}
	return NULL;
}

/*
 * - "keywords and attribute values that contain the characters []{}(),;?*=!@
 *   not enclosed with braces should be avoided"; => allowed.
 * - "value of the DSN keyword cannot consist only of blanks and should not
 *   contain leading blanks";
 * - "keywords and data source names cannot contain the backslash (\)
 *   character.";
 * - "value enclosed with braces ({}) containing any of the characters
 *   []{}(),;?*=!@ is passed intact to the driver.";
 *
 *  foo{bar}=baz=foo; => "foo{bar}" = "baz=foo"
 *
 *  * `=` is delimiter, unless within {}
 *  * `{` and `}` allowed within {}
 *  * brances need to be returned to out-str;
 */
BOOL parse_connection_string(esodbc_dsn_attrs_st *attrs,
	SQLWCHAR *szConnStrIn, SQLSMALLINT cchConnStrIn, BOOL duplicate)
{

	SQLWCHAR *pos;
	SQLWCHAR *end;
	wstr_st keyword, value;

	/* parse and assign attributes in connection string */
	pos = szConnStrIn;
	end = pos + (cchConnStrIn == SQL_NTS ? SHRT_MAX : cchConnStrIn);

	while (skip_ws(&pos, end, TRUE)) {
		if (! parse_token(FALSE, &pos, end, &keyword)) {
			ERR("failed to parse keyword at position %zd",
				pos - szConnStrIn);
			return FALSE;
		}

		if (! skip_ws(&pos, end, FALSE)) {
			return FALSE;
		}

		if (! parse_separator(&pos, end)) {
			ERR("failed to parse separator (`=`) at position %zd",
				pos - szConnStrIn);
			return FALSE;
		}

		if (! skip_ws(&pos, end, FALSE)) {
			return FALSE;
		}

		if (! parse_token(TRUE, &pos, end, &value)) {
			ERR("failed to parse value at position %zd",
				pos - szConnStrIn);
			return FALSE;
		}

		DBG("read connection string attribute: `" LWPDL "` = `" LWPDL "`.",
			LWSTR(&keyword), LWSTR(&value));
		if (assign_dsn_attr(attrs, &keyword, &value, TRUE, duplicate) < 0) {
			ERRN("failed to assign keyword `" LWPDL "` with val `" LWPDL "`.",
				LWSTR(&keyword), LWSTR(&value));
			return FALSE;
		}
	}

	return TRUE;
}

BOOL parse_00_list(esodbc_dsn_attrs_st *attrs, SQLWCHAR *list00)
{
	SQLWCHAR *pos;
	size_t cnt;

	for (pos = (SQLWCHAR *)list00; *pos; pos += cnt + 1) {
		cnt = wcslen(pos);

		if (SHRT_MAX < cnt) {
			ERR("invalid list lenght (%zu).", cnt);
			return FALSE;
		}
		if (! parse_connection_string(attrs, pos, (SQLSMALLINT)cnt,
				/*duplicate?*/TRUE)) {
			ERR("failed parsing list entry `" LWPDL "`.", cnt, pos);
			return FALSE;
		}
	}
	return TRUE;
}

static void log_installer_err()
{
	DWORD ecode;
	SQLWCHAR buff[SQL_MAX_MESSAGE_LENGTH];
	WORD msg_len;
	int i = 0;

	while (SQL_SUCCEEDED(SQLInstallerError(++ i, &ecode, buff,
				sizeof(buff)/sizeof(buff[0]), &msg_len))) {
		ERR("#%i: errcode=%d: " LWPDL ".", i, ecode, msg_len, buff);
	}
}

void free_dsn_attrs(esodbc_dsn_attrs_st *attrs)
{
	size_t i;
	SQLWCHAR *ptrs[] = {
		attrs->driver.str,
		attrs->description.str,
		attrs->dsn.str,
		attrs->pwd.str,
		attrs->uid.str,
		attrs->savefile.str,
		attrs->filedsn.str,
		attrs->server.str,
		attrs->port.str,
		attrs->secure.str,
		attrs->timeout.str,
		attrs->follow.str,
		attrs->catalog.str,
		attrs->packing.str,
		attrs->max_fetch_size.str,
		attrs->max_body_size.str,
		attrs->trace_file.str,
		attrs->trace_level.str,
	};

	if (! attrs) {
		return;
	}

	for (i = 0; i < sizeof(ptrs)/sizeof(ptrs[0]); i ++) {
		if (ptrs[i]) {
			free(ptrs[i]);
		}
	}
	free(attrs);
}

/*
 * Checks if a DSN entry with the given name exists already.
 * Returns:
 * . negative on failure
 * . 0 on false
 * . positive on true.
 */
int system_dsn_exists(wstr_st *dsn)
{
	int res;
	SQLWCHAR kbuff[MAX_REG_VAL_NAME];

	/* '\' can't be a key name */
	res = SQLGetPrivateProfileStringW(dsn->str, NULL, MK_WPTR("\\"),
			kbuff, sizeof(kbuff)/sizeof(kbuff[0]), MK_WPTR(SUBKEY_ODBC));
	if (res < 0) {
		ERR("failed to query for DSN registry keys.");
		log_installer_err();
		return -1;
	}
	assert(0 < res);
	return kbuff[0] != MK_WPTR('\\');

}
/*
 * Reads the system entries for a DSN given into a doubly null-terminated
 * attributes list.
 *
 * The defaults are always filled in.
 *
 * The list - as received by ConfigDSN() - seems to only contain the 'DSN'
 * keyword, though the documentation mentions a full list. However, if a full
 * is provided, the values are going to be taken into account, but possibly
 * overwritten by registry entries (which theoretically should be the same
 * anyways).
 */
esodbc_dsn_attrs_st *load_system_dsn(SQLWCHAR *list00)
{
	esodbc_dsn_attrs_st *attrs;
	int res;
	SQLWCHAR buff[MAX_REG_DATA_SIZE], *pos;
	SQLWCHAR kbuff[MAX_REG_VAL_NAME *
						 (sizeof(esodbc_dsn_attrs_st)/sizeof(wstr_st))];
	wstr_st keyword, value;

	attrs = calloc(1, sizeof(esodbc_dsn_attrs_st));
	if (! attrs) {
		ERR("OOM allocating config attrs (%zu).", sizeof(esodbc_dsn_attrs_st));
		return NULL;
	}
	if (! parse_00_list(attrs, list00)) {
		ERR("failed to parse doubly null-terminated attributes "
			"list `" LWPD "`.", list00);
		goto err;
	}
	/* ConfigDSN() requirement */
	if (attrs->driver.cnt) {
		ERR("function can not accept '" ESODBC_DSN_DRIVER "' keyword.");
		goto err;
	}

	if (attrs->dsn.cnt) {
		DBG("loading attributes for DSN `" LWPDL "`.", LWSTR(&attrs->dsn));

		/* load available key names first;
		 * it's another doubly null-terminated list. */
		res = SQLGetPrivateProfileStringW(attrs->dsn.str, NULL, MK_WPTR(""),
				kbuff, sizeof(kbuff)/sizeof(kbuff[0]), MK_WPTR(SUBKEY_ODBC));
		if (res < 0) {
			ERR("failed to query for DSN registry keys.");
			log_installer_err();
			goto err;
		}
		/* for each key name, read its value and add it to 'attrs' */
		for (pos = kbuff; *pos; pos += keyword.cnt + 1) {
			keyword.str = pos;
			keyword.cnt = wcslen(pos);

			if (EQ_CASE_WSTR(&keyword, &MK_WSTR(ESODBC_DSN_DRIVER))) {
				/* skip the 'Driver' keyword */
				continue;
			}

			res = SQLGetPrivateProfileStringW(attrs->dsn.str, pos, MK_WPTR(""),
					buff, sizeof(buff)/sizeof(buff[0]), MK_WPTR(SUBKEY_ODBC));
			if (res < 0) {
				ERR("failed to query value for DSN registry key `" LWPDL "`.",
					LWSTR(&keyword));
				log_installer_err();
				goto err;
			} else {
				value.cnt = (size_t)res;
				value.str = buff;
			}
			/* assign it to the config */
			DBG("read DSN attribute: `" LWPDL "` = `" LWPDL "`.",
				LWSTR(&keyword), LWSTR(&value));
			if (assign_dsn_attr(attrs, &keyword, &value,
					/*overwrite?*/TRUE, /*duplicate?*/TRUE) < 0) {
				ERR("keyword '" LWPDL "' couldn't be assigned.",
					LWSTR(&keyword));
				goto err;
			}
		}
	}

	if (! assign_dsn_defaults(attrs, /*duplicate?*/TRUE)) {
		ERR("OOM assigning defaults");
		goto err;
	}

	return attrs;
err:
	free_dsn_attrs(attrs);
	return NULL;
}

BOOL write_system_dsn(esodbc_dsn_attrs_st *attrs, BOOL create_new)
{
	struct {
		wstr_st *kw;
		wstr_st *val;
	} *iter, map[] = {
		/* Driver */
		{&MK_WSTR(ESODBC_DSN_DESCRIPTION), &attrs->description},
		/* DSN */
		{&MK_WSTR(ESODBC_DSN_PWD), &attrs->pwd},
		{&MK_WSTR(ESODBC_DSN_UID), &attrs->uid},
		/* SAVEILE */
		/* FILEDSN */
		{&MK_WSTR(ESODBC_DSN_SERVER), &attrs->server},
		{&MK_WSTR(ESODBC_DSN_PORT), &attrs->port},
		{&MK_WSTR(ESODBC_DSN_SECURE), &attrs->secure},
		{&MK_WSTR(ESODBC_DSN_TIMEOUT), &attrs->timeout},
		{&MK_WSTR(ESODBC_DSN_FOLLOW), &attrs->follow},
		{&MK_WSTR(ESODBC_DSN_CATALOG), &attrs->catalog},
		{&MK_WSTR(ESODBC_DSN_PACKING), &attrs->packing},
		{&MK_WSTR(ESODBC_DSN_MAX_FETCH_SIZE), &attrs->max_fetch_size},
		{&MK_WSTR(ESODBC_DSN_MAX_BODY_SIZE_MB), &attrs->max_body_size},
		{&MK_WSTR(ESODBC_DSN_TRACE_FILE), &attrs->trace_file},
		{&MK_WSTR(ESODBC_DSN_TRACE_LEVEL), &attrs->trace_level},
		{NULL, NULL}
	};

	if (create_new) {
		if (! SQLValidDSNW(attrs->dsn.str)) {
			ERR("invalid DSN value `" LWPDL "`.", LWSTR(&attrs->dsn));
			return FALSE;
		}
		INFO("creating new DSN `" LWPDL "` for driver ` " LWPDL " `.",
			LWSTR(&attrs->dsn), LWSTR(&attrs->driver));
		if (! SQLWriteDSNToIniW(attrs->dsn.str, attrs->driver.str)) {
			ERR("failed to add DSN `" LWPDL "` for driver ` " LWPDL " ` to "
				".INI.", LWSTR(&attrs->dsn), LWSTR(&attrs->driver));
			return FALSE;
		}
	}

	for (iter = &map[0]; iter->kw; iter ++) {
		if (! iter->val->cnt) {
			DBG("value `" LWPDL "` not provisioned.", LWSTR(iter->kw));
			continue;
		}
		if (! SQLWritePrivateProfileStringW(attrs->dsn.str,
				iter->kw->str, iter->val->str, MK_WPTR(SUBKEY_ODBC))) {
			ERR("failed to write key `" LWPDL "` with value `" LWPDL "`.",
				LWSTR(iter->kw), LWSTR(iter->val));
			return FALSE;
		}
		DBG("key `" LWPDL "` with value `" LWPDL "` written to " SUBKEY_ODBC
			".", LWSTR(iter->kw), LWSTR(iter->val));
	}
	return TRUE;
}

static inline BOOL needs_braces(wstr_st *token)
{
	int i;

	for (i = 0; i < token->cnt; i ++) {
		switch(token->str[i]) {
			case ' ':
			case '\t':
			case '\r':
			case '\n':
			case '=':
			case ';':
				return TRUE;
		}
	}
	return FALSE;
}

/* build a connection string to be written in the DSN files */
BOOL write_connection_string(esodbc_dsn_attrs_st *attrs,
	SQLWCHAR *szConnStrOut, SQLSMALLINT cchConnStrOutMax,
	SQLSMALLINT *pcchConnStrOut)
{
	int n, braces;
	size_t pos;
	wchar_t *format;
	struct {
		wstr_st *val;
		char *kw;
	} *iter, map[] = {
		{&attrs->driver, ESODBC_DSN_DRIVER},
		/* Description */
		{&attrs->dsn, ESODBC_DSN_DSN},
		{&attrs->pwd, ESODBC_DSN_PWD},
		{&attrs->uid, ESODBC_DSN_UID},
		{&attrs->savefile, ESODBC_DSN_SAVEFILE},
		{&attrs->filedsn, ESODBC_DSN_FILEDSN},
		{&attrs->server, ESODBC_DSN_SERVER},
		{&attrs->port, ESODBC_DSN_PORT},
		{&attrs->secure, ESODBC_DSN_SECURE},
		{&attrs->timeout, ESODBC_DSN_TIMEOUT},
		{&attrs->follow, ESODBC_DSN_FOLLOW},
		{&attrs->catalog, ESODBC_DSN_CATALOG},
		{&attrs->packing, ESODBC_DSN_PACKING},
		{&attrs->max_fetch_size, ESODBC_DSN_MAX_FETCH_SIZE},
		{&attrs->max_body_size, ESODBC_DSN_MAX_BODY_SIZE_MB},
		{&attrs->trace_file, ESODBC_DSN_TRACE_FILE},
		{&attrs->trace_level, ESODBC_DSN_TRACE_LEVEL},
		{NULL, NULL}
	};

	for (iter = &map[0], pos = 0; iter->val; iter ++) {
		if (iter->val->cnt) {
			braces = needs_braces(iter->val) ? 2 : 0;
			if (cchConnStrOutMax && szConnStrOut) {
				/* swprintf will fail if formated string would overrun the
				 * buffer size */
				if (cchConnStrOutMax - pos < iter->val->cnt + braces) {
					/* indicate that we've reached buffer limits: only account
					 * for how long the string would be */
					cchConnStrOutMax = 0;
					pos += iter->val->cnt + braces;
					continue;
				}
				if (braces) {
					format = WPFCP_DESC "={" WPFWP_LDESC "};";
				} else {
					format = WPFCP_DESC "=" WPFWP_LDESC ";";
				}
				n = swprintf(szConnStrOut + pos, cchConnStrOutMax - pos,
						format, iter->kw, LWSTR(iter->val));
				if (n < 0) {
					ERRN("failed to outprint connection string (space "
						"left: %d; needed: %d).", cchConnStrOutMax - pos,
						iter->val->cnt);
					return FALSE;
				} else {
					pos += n;
				}
			} else {
				/* simply increment the counter, since the untruncated length
				 * need to be returned to the app */
				pos += iter->val->cnt + braces;
			}
		}
	}

	*pcchConnStrOut = (SQLSMALLINT)pos;

	DBG("Output connection string: `" LWPD "`; out len: %d.",
		szConnStrOut, pos);
	return TRUE;
}

/* calling function must mem-manage also in failure case */
BOOL assign_dsn_defaults(esodbc_dsn_attrs_st *attrs, BOOL duplicate)
{
	/* assign defaults where not assigned and applicable */
	if (assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_SERVER), &MK_WSTR(ESODBC_DEF_SERVER),
			/*overwrite?*/FALSE, duplicate) < 0) {
		return FALSE;
	}
	if (assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_PORT), &MK_WSTR(ESODBC_DEF_PORT),
			/*overwrite?*/FALSE, duplicate) < 0) {
		return FALSE;
	}
	if (assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_SECURE), &MK_WSTR(ESODBC_DEF_SECURE),
			/*overwrite?*/FALSE, duplicate) < 0) {
		return FALSE;
	}
	if (assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_TIMEOUT), &MK_WSTR(ESODBC_DEF_TIMEOUT),
			/*overwrite?*/FALSE, duplicate) < 0) {
		return FALSE;
	}
	if (assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_FOLLOW), &MK_WSTR(ESODBC_DEF_FOLLOW),
			/*overwrite?*/FALSE, duplicate) < 0) {
		return FALSE;
	}

	if (assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_PACKING), &MK_WSTR(ESODBC_DEF_PACKING),
			/*overwrite?*/FALSE, duplicate) < 0) {
		return FALSE;
	}
	if (assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_MAX_FETCH_SIZE),
			&MK_WSTR(ESODBC_DEF_FETCH_SIZE),
			/*overwrite?*/FALSE, duplicate) < 0) {
		return FALSE;
	}
	if (assign_dsn_attr(attrs, &MK_WSTR(ESODBC_DSN_MAX_BODY_SIZE_MB),
			&MK_WSTR(ESODBC_DEF_MAX_BODY_SIZE_MB),
			/*overwrite?*/FALSE, duplicate) < 0) {
		return FALSE;
	}

	/* default: no trace file */
	if (assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_TRACE_LEVEL), &MK_WSTR(ESODBC_DEF_TRACE_LEVEL),
			/*overwrite?*/FALSE, duplicate) < 0) {
		return FALSE;
	}

	return TRUE;
}


#if defined(_WIN32) || defined (WIN32)
/*
 * Reads system registry for ODBC DSN subkey named in attrs->dsn.
 * TODO: use odbccp32.dll's SQLGetPrivateProfileString() & co. instead of
 * direct registry access:
 * https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetprivateprofilestring-function
 */
BOOL read_system_info(esodbc_dsn_attrs_st *attrs, TCHAR *buff)
{
	HKEY hkey;
	BOOL ret = FALSE;
	const char *ktree;
	DWORD valsno, i, j; /* number of values in subkey */
	DWORD maxvallen; /* len of longest value name */
	DWORD maxdatalen; /* len of longest data (buffer) */
	TCHAR val[MAX_REG_VAL_NAME];
	DWORD vallen;
	DWORD valtype;
	BYTE *d;
	DWORD datalen;
	tstr_st tval, tdata;

	if (swprintf(val, sizeof(val)/sizeof(val[0]), WPFCP_DESC "\\" WPFWP_LDESC,
			ODBC_REG_SUBKEY_PATH, LWSTR(&attrs->dsn)) < 0) {
		ERRN("failed to print registry key path.");
		return FALSE;
	}
	/* try accessing local user's config first, if that fails, systems' */
	if (RegOpenKeyEx(HKEY_CURRENT_USER, val, /*options*/0, KEY_READ,
			&hkey) != ERROR_SUCCESS) {
		INFO("failed to open registry key `" REG_HKCU "\\" LWPD "`.",
			val);
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, val, /*options*/0, KEY_READ,
				&hkey) != ERROR_SUCCESS) {
			INFO("failed to open registry key `" REG_HKLM "\\" LWPD "`.",
				val);
			goto end;
		} else {
			ktree = REG_HKLM;
		}
	} else {
		ktree = REG_HKCU;
	}

	if (RegQueryInfoKey(hkey, /*key class*/NULL, /*len of key class*/NULL,
			/*reserved*/NULL, /*no subkeys*/NULL, /*longest subkey*/NULL,
			/*longest subkey class name*/NULL, &valsno, &maxvallen,
			&maxdatalen, /*sec descr*/NULL,
			/*update time */NULL) != ERROR_SUCCESS) {
		ERRN("Failed to query registery key info for path `%s\\" LWPD
			"`.", ktree, val);
		goto end;
	} else {
		DBG("Subkey '%s\\" LWPD "': vals: %d, lengthiest name: %d, "
			"lengthiest data: %d.", ktree, val, valsno, maxvallen,
			maxdatalen);
		// malloc buffers?
		if (MAX_REG_VAL_NAME < maxvallen)
			BUG("value name buffer too small (%d), needed: %dB.",
				MAX_REG_VAL_NAME, maxvallen);
		if (MAX_REG_DATA_SIZE < maxdatalen)
			BUG("value data buffer too small (%d), needed: %dB.",
				MAX_REG_DATA_SIZE, maxdatalen);
		/* connection could still succeeded, so carry on */
	}

	for (i = 0, j = 0; i < valsno; i ++) {
		vallen = sizeof(val) / sizeof(val[0]);
		datalen = MAX_REG_DATA_SIZE;
		d = (BYTE *)&buff[j * datalen];
		if (RegEnumValue(hkey, i, val, &vallen, /*reserved*/NULL, &valtype,
				d, &datalen) != ERROR_SUCCESS) {
			ERR("failed to read register subkey value.");
			goto end;
		}
		if (valtype != REG_SZ) {
			INFO("unused register values of type %d -- skipping.",
				valtype);
			continue;
		}
		tval = (tstr_st) {
			val, vallen
		};
		tdata = (tstr_st) {
			(SQLTCHAR *)d, datalen
		};
		// TODO: duplicating is needed, but leaks! this needs a rewrite.
		if (assign_dsn_attr(attrs, &tval, &tdata, FALSE, /*dup?*/TRUE) < 0) {
			ERR("failed to assign reg entry `" LTPDL "`: `" LTPDL "`.",
				LTSTR(&tval), LTSTR(&tdata));
			goto end;
		}
		// else if == 0: entry not directly relevant to driver config
		j ++;
		DBG("reg entry`" LTPDL "`: `" LTPDL "` assigned.",
			LTSTR(&tval), LTSTR(&tdata));
	}


	ret = TRUE;
end:
	RegCloseKey(hkey);

	return ret;
}
#else /* defined(_WIN32) || defined (WIN32) */
#error "unsupported platform" /* TODO */
#endif /* defined(_WIN32) || defined (WIN32) */


// asks user for the config data
BOOL prompt_user_config(HWND hwndParent, esodbc_dsn_attrs_st *attrs,
	/* disable non-connect-related controls? */
	BOOL disable_nonconn)
{
	TRACE;
	if (assign_dsn_attr(attrs, &MK_WSTR(ESODBC_DSN_DSN),
			&MK_WSTR("Elasticsearch ODBC Sample DSN"), FALSE, TRUE) <= 0) {
		//&MK_WSTR("Elasticsearch ODBC Sample DSN"), TRUE, TRUE) <= 0) {
		return FALSE;
	}
#if 1
	if (assign_dsn_attr(attrs, &MK_WSTR(ESODBC_DSN_TRACE_LEVEL),
			&MK_WSTR("INFO"), TRUE, TRUE) <= 0) {
		//&MK_WSTR("Elasticsearch ODBC Sample DSN2"), TRUE, TRUE) <= 0) {
		return FALSE;
	}
#endif
	return TRUE;
}

// asks user if we should overwrite the existing DSN
// returns:
// . negative on failure
// . 0 on false
// . positive on true
int prompt_user_overwrite(HWND hwndParent, wstr_st *dsn)
{
	TRACE;
	return 1;
}


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
