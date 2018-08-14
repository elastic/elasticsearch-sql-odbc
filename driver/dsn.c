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


void init_dsn_attrs(esodbc_dsn_attrs_st *attrs)
{
	size_t i;
	wstr_st *wstr;

	memset(attrs, 0, sizeof(*attrs));

	for (i = 0; i < ESODBC_DSN_ATTRS_COUNT; i ++) {
		wstr = &((wstr_st *)attrs)[i];
		wstr->str = &attrs->buff[i * ESODBC_DSN_MAX_ATTR_LEN];
	}
}

#define DSN_NOT_MATCHED		0
#define DSN_NOT_OVERWRITTEN	1
#define DSN_ASSIGNED		2
#define DSN_OVERWRITTEN		3
/*
 * returns:
 *   positive on keyword match:
 *     DSN_ASSIGNED for assignment on blank,
 *     DSN_OVERWRITTEN for assignment over value,
 *     DSN_NOT_OVERWRITTEN for skipped assignment due to !overwrite;
 *   0 on keyword mismatch;
 *   negative on error;
 */
int assign_dsn_attr(esodbc_dsn_attrs_st *attrs,
	wstr_st *keyword, wstr_st *value, BOOL overwrite)
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
	int ret;

	if (ESODBC_DSN_MAX_ATTR_LEN < value->cnt) {
		ERR("attribute value lenght too large: %zu; max=%zu.", value->cnt,
			ESODBC_DSN_MAX_ATTR_LEN);
		return -1;
	}

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
				return DSN_NOT_OVERWRITTEN;
			}
			INFO("keyword '" LWPDL "' already assigned: "
				"overwriting previous `" LWPDL "` with new `" LWPDL "`.",
				LWSTR(iter->kw), LWSTR(iter->val), LWSTR(value));
			ret = DSN_OVERWRITTEN;
		} else {
			INFO("keyword '" LWPDL "' new assignment: `" LWPDL "`.",
				LWSTR(iter->kw), LWSTR(value));
			ret = DSN_ASSIGNED;
		}
		memcpy(iter->val->str, value->str, value->cnt * sizeof(*value->str));
		iter->val->cnt = value->cnt;
		return ret;
	}

	/* entry not directly relevant to driver config */
	WARN("keyword `" LWPDL "` (with value `" LWPDL "`) not DSN config "
		"specific, so not assigned.", LWSTR(keyword), LWSTR(value));
	return DSN_NOT_MATCHED;
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
	SQLWCHAR *szConnStrIn, SQLSMALLINT cchConnStrIn)
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
		if (assign_dsn_attr(attrs, &keyword, &value, TRUE) < 0) {
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
		if (! parse_connection_string(attrs, pos, (SQLSMALLINT)cnt)) {
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
	SQLWCHAR kbuff[ESODBC_DSN_MAX_ATTR_LEN];

	/* '\' can't be a key name */
	res = SQLGetPrivateProfileStringW(dsn->str, NULL, MK_WPTR("\\"),
			kbuff, sizeof(kbuff)/sizeof(kbuff[0]), MK_WPTR(SUBKEY_ODBC));
	if (res < 0) {
		ERR("failed to query for DSN registry keys.");
		log_installer_err();
		return -1;
	}
	/* subkey can be found, but have nothing beneath => res == 0 */
	return (! res) || (kbuff[0] != MK_WPTR('\\'));

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
BOOL load_system_dsn(esodbc_dsn_attrs_st *attrs, SQLWCHAR *list00)
{
	int res;
	SQLWCHAR buff[ESODBC_DSN_MAX_ATTR_LEN], *pos;
	SQLWCHAR kbuff[sizeof(attrs->buff)/sizeof(attrs->buff[0])];
	wstr_st keyword, value;

	if (! parse_00_list(attrs, list00)) {
		ERR("failed to parse doubly null-terminated attributes "
			"list `" LWPD "`.", list00);
		return FALSE;
	}
	/* ConfigDSN() requirement */
	if (attrs->driver.cnt) {
		ERR("function can not accept '" ESODBC_DSN_DRIVER "' keyword.");
		return FALSE;
	}

	if (attrs->dsn.cnt) {
		DBG("loading attributes for DSN `" LWPDL "`.", LWSTR(&attrs->dsn));

		/* load available key *names* first;
		 * it's another doubly null-terminated list (w/o values). */
		res = SQLGetPrivateProfileStringW(attrs->dsn.str, NULL, MK_WPTR(""),
				kbuff, sizeof(kbuff)/sizeof(kbuff[0]), MK_WPTR(SUBKEY_ODBC));
		if (res < 0) {
			ERR("failed to query for DSN registry keys.");
			log_installer_err();
			return FALSE;
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
				return FALSE;
			} else {
				value.cnt = (size_t)res;
				value.str = buff;
			}
			/* assign it to the config */
			DBG("read DSN attribute: `" LWPDL "` = `" LWPDL "`.",
				LWSTR(&keyword), LWSTR(&value));
			/* assign attributes not yet given in the 00-list */
			if (assign_dsn_attr(attrs, &keyword, &value, /*over?*/FALSE) < 0) {
				ERR("keyword '" LWPDL "' couldn't be assigned.",
					LWSTR(&keyword));
				return FALSE;
			}
		}
	}

	if (! assign_dsn_defaults(attrs)) {
		ERR("OOM assigning defaults");
		return FALSE;
	}

	return TRUE;
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
	size_t i;

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

BOOL assign_dsn_defaults(esodbc_dsn_attrs_st *attrs)
{
	/* assign defaults where not assigned and applicable */
	if (assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_SERVER), &MK_WSTR(ESODBC_DEF_SERVER),
			/*overwrite?*/FALSE) < 0) {
		return FALSE;
	}
	if (assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_PORT), &MK_WSTR(ESODBC_DEF_PORT),
			/*overwrite?*/FALSE) < 0) {
		return FALSE;
	}
	if (assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_SECURE), &MK_WSTR(ESODBC_DEF_SECURE),
			/*overwrite?*/FALSE) < 0) {
		return FALSE;
	}
	if (assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_TIMEOUT), &MK_WSTR(ESODBC_DEF_TIMEOUT),
			/*overwrite?*/FALSE) < 0) {
		return FALSE;
	}
	if (assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_FOLLOW), &MK_WSTR(ESODBC_DEF_FOLLOW),
			/*overwrite?*/FALSE) < 0) {
		return FALSE;
	}

	if (assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_PACKING), &MK_WSTR(ESODBC_DEF_PACKING),
			/*overwrite?*/FALSE) < 0) {
		return FALSE;
	}
	if (assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_MAX_FETCH_SIZE),
			&MK_WSTR(ESODBC_DEF_FETCH_SIZE),
			/*overwrite?*/FALSE) < 0) {
		return FALSE;
	}
	if (assign_dsn_attr(attrs, &MK_WSTR(ESODBC_DSN_MAX_BODY_SIZE_MB),
			&MK_WSTR(ESODBC_DEF_MAX_BODY_SIZE_MB),
			/*overwrite?*/FALSE) < 0) {
		return FALSE;
	}

	/* default: no trace file */
	if (assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_TRACE_LEVEL), &MK_WSTR(ESODBC_DEF_TRACE_LEVEL),
			/*overwrite?*/FALSE) < 0) {
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
BOOL read_system_info(esodbc_dsn_attrs_st *attrs)
{
	HKEY hkey;
	BOOL ret = FALSE;
	const char *ktree;
	DWORD valsno/* number of values in subkey */, i, res;
	DWORD maxvallen; /* len of longest value name */
	DWORD maxdatalen; /* len of longest data (buffer) */
	DWORD vallen;
	DWORD valtype;
	DWORD datalen;
	SQLWCHAR val_buff[ESODBC_DSN_MAX_ATTR_LEN];
	wstr_st val_name = {val_buff, 0};
	SQLWCHAR data_buff[ESODBC_DSN_MAX_ATTR_LEN];
	wstr_st val_data = {data_buff, 0};

	if (swprintf(val_buff, sizeof(val_buff)/sizeof(val_buff[0]),
			WPFCP_DESC "\\" WPFWP_LDESC,
			ODBC_REG_SUBKEY_PATH, LWSTR(&attrs->dsn)) < 0) {
		ERRN("failed to print registry key path.");
		return FALSE;
	}
	/* try accessing local user's config first, if that fails, systems' */
	if (RegOpenKeyExW(HKEY_CURRENT_USER, val_buff, /*options*/0, KEY_READ,
			&hkey) != ERROR_SUCCESS) {
		INFO("failed to open registry key `" REG_HKCU "\\" LWPD "`.",
			val_buff);
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, val_buff, /*options*/0, KEY_READ,
				&hkey) != ERROR_SUCCESS) {
			INFO("failed to open registry key `" REG_HKLM "\\" LWPD "`.",
				val_buff);
			goto end;
		} else {
			ktree = REG_HKLM;
		}
	} else {
		ktree = REG_HKCU;
	}

	if (RegQueryInfoKeyW(hkey, /*key class*/NULL, /*len of key class*/NULL,
			/*reserved*/NULL, /*no subkeys*/NULL, /*longest subkey*/NULL,
			/*longest subkey class name*/NULL, &valsno, &maxvallen,
			&maxdatalen, /*sec descr*/NULL,
			/*update time */NULL) != ERROR_SUCCESS) {
		ERRN("Failed to query registery key info for path `%s\\" LWPD
			"`.", ktree, val_buff);
		goto end;
	} else {
		DBG("Subkey '%s\\" LWPD "': vals: %d, lengthiest name: %d, "
			"lengthiest data: %d.", ktree, val_buff, valsno, maxvallen,
			maxdatalen);
		if (sizeof(val_buff)/sizeof(val_buff[0]) < maxvallen) {
			WARN("value name buffer too small (%d), needed: %dB.",
				sizeof(val_buff)/sizeof(val_buff[0]), maxvallen);
		}
		if (sizeof(data_buff)/sizeof(data_buff[0]) < maxdatalen) {
			WARN("value data buffer too small (%d), needed: %dB.",
				sizeof(data_buff)/sizeof(data_buff[0]), maxdatalen);
		}
		/* the registry might contain other, non connection-related strings,
		 * so these conditions are not necearily an error. */
	}

	for (i = 0; i < valsno; i ++) {
		vallen = sizeof(val_buff) / sizeof(val_buff[0]);
		datalen = sizeof(data_buff);

		if (RegEnumValueW(hkey, i, val_buff, &vallen, /*reserved*/NULL, &valtype,
				(BYTE *)data_buff, &datalen) != ERROR_SUCCESS) {
			ERR("failed to read register subkey value.");
			goto end;
		}
		/* vallen doesn't count the \0 */
		val_name.cnt = vallen;
		if (valtype != REG_SZ) {
			INFO("skipping register value `" LWPDL "` of type %d.",
				LWSTR(&val_name), valtype);
			continue;
		}
		if (datalen <= 0) {
			INFO("skipping value `" LWPDL "` with empty data.",
				LWSTR(&val_name));
			continue;
		}
		assert(datalen % sizeof(SQLWCHAR) == 0);
		/* datalen counts all bytes returned, so including the \0 */
		val_data.cnt = datalen/sizeof(SQLWCHAR) - /*\0*/1;

		if ((res = assign_dsn_attr(attrs, &val_name, &val_data,
						/*overwrite?*/FALSE)) < 0) {
			ERR("failed to assign reg entry `" LTPDL "`: `" LTPDL "`.",
				LTSTR(&val_name), LTSTR(&val_data));
			goto end;
		} else if (res == DSN_ASSIGNED) {
			DBG("reg entry`" LTPDL "`: `" LTPDL "` assigned.",
				LTSTR(&val_name), LTSTR(&val_data));
		}
		/* if == 0: entry not directly relevant to driver config
		 * if == DSN_NOT_OVERWRITTEN: entry provisioned in the conn str. */
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
// returns:
// . negative: on failure
// . 0: user canceled
// . positive: user provided input
int prompt_user_config(HWND hwndParent, esodbc_dsn_attrs_st *attrs,
	/* disable non-connect-related controls? */
	BOOL disable_nonconn)
{
	static int attempts = 0;

	if (! hwndParent) {
		INFO("no window handler provided -- configuration skipped.");
		return 1;
	}
	TRACE;

	if (assign_dsn_attr(attrs, &MK_WSTR(ESODBC_DSN_DSN),
			&MK_WSTR("My Elasticsearch ODBC DSN"), FALSE) <= 0) {
		//&MK_WSTR("My Elasticsearch ODBC DSN"), TRUE, TRUE) <= 0) {
		return -1;
	}
#if 1
	if (assign_dsn_attr(attrs, &MK_WSTR(ESODBC_DSN_TRACE_LEVEL),
			&MK_WSTR("INFO"), FALSE) <= 0) {
		return -1;
	}
#endif
	if (1 < attempts ++) {
		/* prevent infinite loops */
		return 0;
	}

	if (SQL_MAX_DSN_LENGTH < attrs->dsn.cnt) {
		ERR("DSN name longer than max (%d).", SQL_MAX_DSN_LENGTH);
	}

	return 1;
}

// asks user if we should overwrite the existing DSN
// returns:
// . negative on failure
// . 0 on false
// . positive on true
int prompt_user_overwrite(HWND hwndParent, wstr_st *dsn)
{
	if (! hwndParent) {
		INFO("no window handler provided -- forcing overwrite.");
		return 1;
	}
	TRACE;
	return 1;
}


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
