/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include "util.h" /* includes windows.h, needed for odbcinst.h */
#include <odbcinst.h>

#include "dsn.h"
#include "log.h"
#include "connect.h"
#include "info.h"
#include "util.h"


void TEST_API init_dsn_attrs(esodbc_dsn_attrs_st *attrs)
{
	size_t i;
	wstr_st *wstr;

	memset(attrs, 0, sizeof(*attrs));

	for (i = 0; i < ESODBC_DSN_ATTRS_COUNT; i ++) {
		wstr = &((wstr_st *)attrs)[i];
		wstr->str = &attrs->buff[i * ESODBC_DSN_MAX_ATTR_LEN];
	}
}

/* used with LWSTR() macro, so invoked twice, but it is:
 * - executed only if the message level is low enough (i.e. it's logged);
 * - more compact this way. */
static inline wstr_st *mask_pwd(wstr_st *attr, wstr_st *val)
{
	static wstr_st subst = WSTR_INIT(ESODBC_PWD_VAL_SUBST);
	return EQ_CASE_WSTR(attr, &MK_WSTR(ESODBC_DSN_PWD)) ? &subst : val;
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
		{&MK_WSTR(ESODBC_DSN_CLOUD_ID), &attrs->cloud_id},
		{&MK_WSTR(ESODBC_DSN_SERVER), &attrs->server},
		{&MK_WSTR(ESODBC_DSN_PORT), &attrs->port},
		{&MK_WSTR(ESODBC_DSN_SECURE), &attrs->secure},
		{&MK_WSTR(ESODBC_DSN_CA_PATH), &attrs->ca_path},
		{&MK_WSTR(ESODBC_DSN_TIMEOUT), &attrs->timeout},
		{&MK_WSTR(ESODBC_DSN_FOLLOW), &attrs->follow},
		{&MK_WSTR(ESODBC_DSN_CATALOG), &attrs->catalog},
		{&MK_WSTR(ESODBC_DSN_PACKING), &attrs->packing},
		{&MK_WSTR(ESODBC_DSN_COMPRESSION), &attrs->compression},
		{&MK_WSTR(ESODBC_DSN_MAX_FETCH_SIZE), &attrs->max_fetch_size},
		{&MK_WSTR(ESODBC_DSN_MAX_BODY_SIZE_MB), &attrs->max_body_size},
		{&MK_WSTR(ESODBC_DSN_APPLY_TZ), &attrs->apply_tz},
		{&MK_WSTR(ESODBC_DSN_EARLY_EXEC), &attrs->early_exec},
		{&MK_WSTR(ESODBC_DSN_SCI_FLOATS), &attrs->sci_floats},
		{&MK_WSTR(ESODBC_DSN_VARCHAR_LIMIT), &attrs->varchar_limit},
		{&MK_WSTR(ESODBC_DSN_MFIELD_LENIENT), &attrs->mfield_lenient},
		{&MK_WSTR(ESODBC_DSN_ESC_PVA), &attrs->auto_esc_pva},
		{&MK_WSTR(ESODBC_DSN_IDX_INC_FROZEN), &attrs->idx_inc_frozen},
		{&MK_WSTR(ESODBC_DSN_PROXY_ENABLED), &attrs->proxy_enabled},
		{&MK_WSTR(ESODBC_DSN_PROXY_TYPE), &attrs->proxy_type},
		{&MK_WSTR(ESODBC_DSN_PROXY_HOST), &attrs->proxy_host},
		{&MK_WSTR(ESODBC_DSN_PROXY_PORT), &attrs->proxy_port},
		{&MK_WSTR(ESODBC_DSN_PROXY_AUTH_ENA), &attrs->proxy_auth_enabled},
		{&MK_WSTR(ESODBC_DSN_PROXY_AUTH_UID), &attrs->proxy_auth_uid},
		{&MK_WSTR(ESODBC_DSN_PROXY_AUTH_PWD), &attrs->proxy_auth_pwd},
		{&MK_WSTR(ESODBC_DSN_TRACE_ENABLED), &attrs->trace_enabled},
		{&MK_WSTR(ESODBC_DSN_TRACE_FILE), &attrs->trace_file},
		{&MK_WSTR(ESODBC_DSN_TRACE_LEVEL), &attrs->trace_level},
		{NULL, NULL}
	};
	int ret;

	assert(sizeof(map)/sizeof(*iter) - /* {NULL,NULL} terminator */1 ==
		ESODBC_DSN_ATTRS_COUNT);

	if (ESODBC_DSN_MAX_ATTR_LEN < value->cnt) {
		ERR("attribute value length too large: %zu; max=%zu.", value->cnt,
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
					LWSTR(iter->kw), LWSTR(mask_pwd(iter->kw, value)),
					LWSTR(mask_pwd(iter->kw, iter->val)));
				return DSN_NOT_OVERWRITTEN;
			}
			INFO("keyword '" LWPDL "' already assigned: "
				"overwriting previous `" LWPDL "` with new `" LWPDL "`.",
				LWSTR(iter->kw), LWSTR(mask_pwd(iter->kw, iter->val)),
				LWSTR(mask_pwd(iter->kw, value)));
			ret = DSN_OVERWRITTEN;
		} else {
			INFO("keyword '" LWPDL "' new assignment: `" LWPDL "`.",
				LWSTR(iter->kw), LWSTR(mask_pwd(iter->kw, value)));
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
BOOL TEST_API parse_connection_string(esodbc_dsn_attrs_st *attrs,
	SQLWCHAR *szConnStrIn, SQLSMALLINT cchConnStrIn)
{

	SQLWCHAR *pos;
	SQLWCHAR *end;
	wstr_st keyword, value;

	/* parse and assign attributes in connection string */
	pos = szConnStrIn;
	/* if NTS, parse till skip_ws() encounters the \0 */
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
			continue; /* empty values are acceptable */
		}

		if (! parse_token(TRUE, &pos, end, &value)) {
			ERR("failed to parse value at position %zd",
				pos - szConnStrIn);
			return FALSE;
		}

		DBG("read connection string attribute: `" LWPDL "` = `" LWPDL "`.",
			LWSTR(&keyword), LWSTR(mask_pwd(&keyword, &value)));
		if (assign_dsn_attr(attrs, &keyword, &value, TRUE) < 0) {
			ERRN("failed to assign keyword `" LWPDL "` with val `" LWPDL "`.",
				LWSTR(&keyword), LWSTR(&value));
			return FALSE;
		}
	}

	return TRUE;
}

BOOL TEST_API parse_00_list(esodbc_dsn_attrs_st *attrs, SQLWCHAR *list00)
{
	SQLWCHAR *pos;
	size_t cnt;

	for (pos = (SQLWCHAR *)list00; *pos; pos += cnt + 1) {
		cnt = wcslen(pos);

		if (SHRT_MAX < cnt) {
			ERR("invalid list length (%zu).", cnt);
			return FALSE;
		}
		if (! parse_connection_string(attrs, pos, (SQLSMALLINT)cnt)) {
			ERR("failed parsing list entry `" LWPDL "`.", cnt, pos);
			return FALSE;
		}
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

long TEST_API write_00_list(esodbc_dsn_attrs_st *attrs,
	SQLWCHAR *list00, size_t cnt00)
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
		{&MK_WSTR(ESODBC_DSN_CLOUD_ID), &attrs->cloud_id},
		{&MK_WSTR(ESODBC_DSN_SERVER), &attrs->server},
		{&MK_WSTR(ESODBC_DSN_PORT), &attrs->port},
		{&MK_WSTR(ESODBC_DSN_SECURE), &attrs->secure},
		{&MK_WSTR(ESODBC_DSN_CA_PATH), &attrs->ca_path},
		{&MK_WSTR(ESODBC_DSN_TIMEOUT), &attrs->timeout},
		{&MK_WSTR(ESODBC_DSN_FOLLOW), &attrs->follow},
		{&MK_WSTR(ESODBC_DSN_CATALOG), &attrs->catalog},
		{&MK_WSTR(ESODBC_DSN_PACKING), &attrs->packing},
		{&MK_WSTR(ESODBC_DSN_COMPRESSION), &attrs->compression},
		{&MK_WSTR(ESODBC_DSN_MAX_FETCH_SIZE), &attrs->max_fetch_size},
		{&MK_WSTR(ESODBC_DSN_MAX_BODY_SIZE_MB), &attrs->max_body_size},
		{&MK_WSTR(ESODBC_DSN_APPLY_TZ), &attrs->apply_tz},
		{&MK_WSTR(ESODBC_DSN_EARLY_EXEC), &attrs->early_exec},
		{&MK_WSTR(ESODBC_DSN_SCI_FLOATS), &attrs->sci_floats},
		{&MK_WSTR(ESODBC_DSN_VARCHAR_LIMIT), &attrs->varchar_limit},
		{&MK_WSTR(ESODBC_DSN_MFIELD_LENIENT), &attrs->mfield_lenient},
		{&MK_WSTR(ESODBC_DSN_ESC_PVA), &attrs->auto_esc_pva},
		{&MK_WSTR(ESODBC_DSN_IDX_INC_FROZEN), &attrs->idx_inc_frozen},
		{&MK_WSTR(ESODBC_DSN_PROXY_ENABLED), &attrs->proxy_enabled},
		{&MK_WSTR(ESODBC_DSN_PROXY_TYPE), &attrs->proxy_type},
		{&MK_WSTR(ESODBC_DSN_PROXY_HOST), &attrs->proxy_host},
		{&MK_WSTR(ESODBC_DSN_PROXY_PORT), &attrs->proxy_port},
		{&MK_WSTR(ESODBC_DSN_PROXY_AUTH_ENA), &attrs->proxy_auth_enabled},
		{&MK_WSTR(ESODBC_DSN_PROXY_AUTH_UID), &attrs->proxy_auth_uid},
		{&MK_WSTR(ESODBC_DSN_PROXY_AUTH_PWD), &attrs->proxy_auth_pwd},
		{&MK_WSTR(ESODBC_DSN_TRACE_ENABLED), &attrs->trace_enabled},
		{&MK_WSTR(ESODBC_DSN_TRACE_FILE), &attrs->trace_file},
		{&MK_WSTR(ESODBC_DSN_TRACE_LEVEL), &attrs->trace_level},
		{NULL, NULL}
	};
	size_t pos;
	BOOL add_braces;

	/* check that the esodbc_dsn_attrs_st stays in sync with the above */
	assert(sizeof(map)/sizeof(*iter) - /* {NULL,NULL} terminator */1 ==
		ESODBC_DSN_ATTRS_COUNT);


	for (iter = &map[0], pos = 0; iter->val; iter ++) {
		if (! iter->val->cnt) {
			continue;
		}
		/* the braces aren't really needed in a doubly-null-terminated list,
		 * but would make a conversion to a "normal" (`;` or `|` separated)
		 * connection string easy */
		add_braces = needs_braces(iter->val);
		if (cnt00 - /*final \0*/1 < pos + iter->kw->cnt + /*`=`*/1 +
			(add_braces ? 2 : 0) + iter->val->cnt) {
			ERR("not enough room in destination buffer.");
			return -1;
		}
		/* copy keyword */
		memcpy(list00 + pos, iter->kw->str, iter->kw->cnt * sizeof(*list00));
		pos += iter->kw->cnt;
		/* copy attribute value separator (`=`) and brace if needed */
		list00[pos ++] = L'=';
		if (add_braces) {
			list00[pos ++] = L'{';
		}
		/* copy value */
		memcpy(list00 + pos, iter->val->str, iter->val->cnt * sizeof(*list00));
		pos += iter->val->cnt;
		/* close any open brace */
		if (add_braces) {
			list00[pos ++] = L'}';
		}
		/* close current attribute */
		list00[pos ++] = L'\0';
	}
	assert(pos < cnt00);
	list00[pos ++] = L'\0';

	return (long)pos;
}

void log_installer_err()
{
	DWORD ecode;
	SQLWCHAR buff[SQL_MAX_MESSAGE_LENGTH];
	WORD msg_len;
	int i = 0;

	while (SQL_SUCCEEDED(SQLInstallerError(++ i, &ecode, buff,
				sizeof(buff)/sizeof(buff[0]), &msg_len))) {
		ERR("#%i: errcode=%d: " LWPDL ".", i, ecode,
			msg_len/sizeof(*buff), buff);
	}
}

size_t copy_installer_errors(wchar_t *err_buff, size_t eb_max)
{
	DWORD ecode;
	SQLWCHAR buff[SQL_MAX_MESSAGE_LENGTH];
	WORD msg_len;
	int i, res;
	size_t pos;

	i = 0;
	pos = 0;
	while (SQL_SUCCEEDED(SQLInstallerError(++ i, &ecode, buff,
				sizeof(buff)/sizeof(buff[0]), &msg_len))) {
		/* Note: msg_len is actually count of chars, current doc is wrong */
		/* if message is larger than buffer, the untruncated size is returned*/
		if (sizeof(buff)/sizeof(buff[0]) <= msg_len) {
			msg_len = sizeof(buff)/sizeof(buff[0]) ;
		}

		assert(pos <= eb_max);
		DBG("error #%d: " LWPD " [%d].", i, buff, msg_len);
		res = swprintf(err_buff + pos, eb_max - pos, L"%.*s. (code: %d)\n",
				msg_len, buff, ecode);
		if (res < 0) {
			ERR("failed to copy: #%i: `" LWPDL "` (%d).", i, msg_len, buff,
				ecode);
			/* allow execution to continue, though */
		} else {
			pos += (size_t)res;
			if (pos <= msg_len) {
				WARN("reached error buffer end (%zu) before copying all "
					"errors.", eb_max);
				break;
			}
		}
	}
	return pos;
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
 * Reads the DSN in the system (User/System section config mode dependent,
 * SQLSetConfigMode()/~Get~).
 * Returns positive on success, 0 on local failure, -1 on system failure.
 */
int load_system_dsn(esodbc_dsn_attrs_st *attrs, BOOL overwrite)
{
	int res;
	SQLWCHAR buff[ESODBC_DSN_MAX_ATTR_LEN], *pos;
	SQLWCHAR kbuff[sizeof(attrs->buff)/sizeof(attrs->buff[0])];
	wstr_st keyword, value;

	DBG("loading attributes for DSN `" LWPDL "`.", LWSTR(&attrs->dsn));

	/* load available key *names* first;
	 * it's another doubly null-terminated list (w/o values). */
	res = SQLGetPrivateProfileStringW(attrs->dsn.str, NULL, MK_WPTR(""),
			kbuff, sizeof(kbuff)/sizeof(kbuff[0]), MK_WPTR(SUBKEY_ODBC));
	if (res < 0) {
		ERR("failed to query for DSN registry keys.");
		return -1;
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
			return -1;
		} else {
			value.cnt = (size_t)res;
			value.str = buff;
		}
		/* assign it to the config */
		DBG("read DSN attribute: `" LWPDL "` = `" LWPDL "`.",
			LWSTR(&keyword), LWSTR(mask_pwd(&keyword, &value)));
		/* assign attributes not yet given in the list */
		if (assign_dsn_attr(attrs, &keyword, &value, overwrite) < 0) {
			ERR("keyword '" LWPDL "' couldn't be assigned.", LWSTR(&keyword));
			return 0;
		}
	}

	return 1;
}


BOOL write_system_dsn(esodbc_dsn_attrs_st *new_attrs,
	esodbc_dsn_attrs_st *old_attrs)
{
	struct {
		wstr_st *kw;
		wstr_st *new;
		wstr_st *old;
	} *iter, map[] = {
		/* Driver */
		{
			&MK_WSTR(ESODBC_DSN_DESCRIPTION), &new_attrs->description,
			old_attrs ? &old_attrs->description : NULL
		},
		/* DSN */
		{
			&MK_WSTR(ESODBC_DSN_PWD), &new_attrs->pwd,
			old_attrs ? &old_attrs->pwd : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_UID), &new_attrs->uid,
			old_attrs ? &old_attrs->uid : NULL
		},
		/* SAVEILE */
		/* FILEDSN */
		{
			&MK_WSTR(ESODBC_DSN_CLOUD_ID), &new_attrs->cloud_id,
			old_attrs ? &old_attrs->cloud_id : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_SERVER), &new_attrs->server,
			old_attrs ? &old_attrs->server : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_PORT), &new_attrs->port,
			old_attrs ? &old_attrs->port : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_SECURE), &new_attrs->secure,
			old_attrs ? &old_attrs->secure : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_CA_PATH), &new_attrs->ca_path,
			old_attrs ? &old_attrs->ca_path : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_TIMEOUT), &new_attrs->timeout,
			old_attrs ? &old_attrs->timeout : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_FOLLOW), &new_attrs->follow,
			old_attrs ? &old_attrs->follow : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_CATALOG), &new_attrs->catalog,
			old_attrs ? &old_attrs->catalog : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_PACKING), &new_attrs->packing,
			old_attrs ? &old_attrs->packing : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_COMPRESSION), &new_attrs->compression,
			old_attrs ? &old_attrs->packing : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_MAX_FETCH_SIZE), &new_attrs->max_fetch_size,
			old_attrs ? &old_attrs->max_fetch_size : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_MAX_BODY_SIZE_MB), &new_attrs->max_body_size,
			old_attrs ? &old_attrs->max_body_size : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_APPLY_TZ), &new_attrs->apply_tz,
			old_attrs ? &old_attrs->apply_tz : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_EARLY_EXEC), &new_attrs->early_exec,
			old_attrs ? &old_attrs->early_exec : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_SCI_FLOATS), &new_attrs->sci_floats,
			old_attrs ? &old_attrs->sci_floats : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_VARCHAR_LIMIT), &new_attrs->varchar_limit,
			old_attrs ? &old_attrs->varchar_limit : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_MFIELD_LENIENT),
			&new_attrs->mfield_lenient,
			old_attrs ? &old_attrs->mfield_lenient : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_ESC_PVA),
			&new_attrs->auto_esc_pva,
			old_attrs ? &old_attrs->auto_esc_pva : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_IDX_INC_FROZEN),
			&new_attrs->idx_inc_frozen,
			old_attrs ? &old_attrs->idx_inc_frozen : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_PROXY_ENABLED), &new_attrs->proxy_enabled,
			old_attrs ? &old_attrs->proxy_enabled : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_PROXY_TYPE), &new_attrs->proxy_type,
			old_attrs ? &old_attrs->proxy_type : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_PROXY_HOST), &new_attrs->proxy_host,
			old_attrs ? &old_attrs->proxy_host : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_PROXY_PORT), &new_attrs->proxy_port,
			old_attrs ? &old_attrs->proxy_port : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_PROXY_AUTH_ENA),
			&new_attrs->proxy_auth_enabled,
			old_attrs ? &old_attrs->proxy_auth_enabled : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_PROXY_AUTH_UID), &new_attrs->proxy_auth_uid,
			old_attrs ? &old_attrs->proxy_auth_uid : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_PROXY_AUTH_PWD), &new_attrs->proxy_auth_pwd,
			old_attrs ? &old_attrs->proxy_auth_pwd : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_TRACE_ENABLED), &new_attrs->trace_enabled,
			old_attrs ? &old_attrs->trace_enabled : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_TRACE_FILE), &new_attrs->trace_file,
			old_attrs ? &old_attrs->trace_file : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_TRACE_LEVEL), &new_attrs->trace_level,
			old_attrs ? &old_attrs->trace_level : NULL
		},
		{NULL, NULL, NULL}
	};

	/* check that the esodbc_dsn_attrs_st stays in sync with the above */
	assert(sizeof(map)/sizeof(map[0]) /* {NULL,NULL, NULL} terminator */- 1
		/*Driver,DSN,SAVEFILE,FILEDSN*/+ 4 == ESODBC_DSN_ATTRS_COUNT);

	for (iter = &map[0]; iter->kw; iter ++) {
		if (iter->old) {
			if (EQ_WSTR(iter->new, iter->old)) {
				DBG("DSN `" LWPDL "` attribute " LWPDL " maintained "
					"value `" LWPDL "`.", LWSTR(&new_attrs->dsn),
					LWSTR(iter->kw), LWSTR(mask_pwd(iter->kw, iter->new)));
				continue;
			}
			if (! SQLWritePrivateProfileStringW(new_attrs->dsn.str,
					iter->kw->str,
					/* "If this argument is NULL, the key pointed to by the
					 * lpszEntry argument is deleted." */
					iter->new->cnt ? iter->new->str : NULL,
					MK_WPTR(SUBKEY_ODBC))) {
				ERR("failed to write key `" LWPDL "` with value `" LWPDL "`.",
					LWSTR(iter->kw), LWSTR(mask_pwd(iter->kw, iter->new)));
				return FALSE;
			}
			INFO("DSN `" LWPDL "` attribute " LWPDL " set to `" LWPDL "`%s.",
				LWSTR(&new_attrs->dsn), LWSTR(iter->kw),
				LWSTR(mask_pwd(iter->kw, iter->new)),
				iter->new->cnt ? "" : " (deleted)");
		} else if (iter->new->cnt) {
			if (! SQLWritePrivateProfileStringW(new_attrs->dsn.str,
					iter->kw->str, iter->new->str, MK_WPTR(SUBKEY_ODBC))) {
				ERR("failed to write key `" LWPDL "` with value `" LWPDL "`.",
					LWSTR(iter->kw), LWSTR(mask_pwd(iter->kw, iter->new)));
				return FALSE;
			}
			INFO("DSN `" LWPDL "` attribute " LWPDL " set to `" LWPDL "`.",
				LWSTR(&new_attrs->dsn), LWSTR(iter->kw),
				LWSTR(mask_pwd(iter->kw, iter->new)));
		}
	}
	return TRUE;
}

/* build a connection string to be written in the DSN files */
long TEST_API write_connection_string(esodbc_dsn_attrs_st *attrs,
	SQLWCHAR *szConnStrOut, SQLSMALLINT cchConnStrOutMax)
{
	int n, braces;
	size_t pos;
	wchar_t *format;
	struct {
		wstr_st *val;
		wstr_st *kw;
	} *iter, map[] = {
		{&attrs->driver, &MK_WSTR(ESODBC_DSN_DRIVER)},
		{&attrs->description, &MK_WSTR(ESODBC_DSN_DESCRIPTION)},
		{&attrs->dsn, &MK_WSTR(ESODBC_DSN_DSN)},
		{&attrs->pwd, &MK_WSTR(ESODBC_DSN_PWD)},
		{&attrs->uid, &MK_WSTR(ESODBC_DSN_UID)},
		{&attrs->savefile, &MK_WSTR(ESODBC_DSN_SAVEFILE)},
		{&attrs->filedsn, &MK_WSTR(ESODBC_DSN_FILEDSN)},
		{&attrs->cloud_id, &MK_WSTR(ESODBC_DSN_CLOUD_ID)},
		{&attrs->server, &MK_WSTR(ESODBC_DSN_SERVER)},
		{&attrs->port, &MK_WSTR(ESODBC_DSN_PORT)},
		{&attrs->secure, &MK_WSTR(ESODBC_DSN_SECURE)},
		{&attrs->ca_path, &MK_WSTR(ESODBC_DSN_CA_PATH)},
		{&attrs->timeout, &MK_WSTR(ESODBC_DSN_TIMEOUT)},
		{&attrs->follow, &MK_WSTR(ESODBC_DSN_FOLLOW)},
		{&attrs->catalog, &MK_WSTR(ESODBC_DSN_CATALOG)},
		{&attrs->packing, &MK_WSTR(ESODBC_DSN_PACKING)},
		{&attrs->compression, &MK_WSTR(ESODBC_DSN_COMPRESSION)},
		{&attrs->max_fetch_size, &MK_WSTR(ESODBC_DSN_MAX_FETCH_SIZE)},
		{&attrs->max_body_size, &MK_WSTR(ESODBC_DSN_MAX_BODY_SIZE_MB)},
		{&attrs->apply_tz, &MK_WSTR(ESODBC_DSN_APPLY_TZ)},
		{&attrs->early_exec, &MK_WSTR(ESODBC_DSN_EARLY_EXEC)},
		{&attrs->sci_floats, &MK_WSTR(ESODBC_DSN_SCI_FLOATS)},
		{&attrs->varchar_limit, &MK_WSTR(ESODBC_DSN_VARCHAR_LIMIT)},
		{&attrs->mfield_lenient, &MK_WSTR(ESODBC_DSN_MFIELD_LENIENT)},
		{&attrs->auto_esc_pva, &MK_WSTR(ESODBC_DSN_ESC_PVA)},
		{&attrs->idx_inc_frozen, &MK_WSTR(ESODBC_DSN_IDX_INC_FROZEN)},
		{&attrs->proxy_enabled, &MK_WSTR(ESODBC_DSN_PROXY_ENABLED)},
		{&attrs->proxy_type, &MK_WSTR(ESODBC_DSN_PROXY_TYPE)},
		{&attrs->proxy_host, &MK_WSTR(ESODBC_DSN_PROXY_HOST)},
		{&attrs->proxy_port, &MK_WSTR(ESODBC_DSN_PROXY_PORT)},
		{&attrs->proxy_auth_enabled, &MK_WSTR(ESODBC_DSN_PROXY_AUTH_ENA)},
		{&attrs->proxy_auth_uid, &MK_WSTR(ESODBC_DSN_PROXY_AUTH_UID)},
		{&attrs->proxy_auth_pwd, &MK_WSTR(ESODBC_DSN_PROXY_AUTH_PWD)},
		{&attrs->trace_enabled, &MK_WSTR(ESODBC_DSN_TRACE_ENABLED)},
		{&attrs->trace_file, &MK_WSTR(ESODBC_DSN_TRACE_FILE)},
		{&attrs->trace_level, &MK_WSTR(ESODBC_DSN_TRACE_LEVEL)},
		{NULL, NULL}
	};

	/* check that the esodbc_dsn_attrs_st stays in sync with the above */
	assert(sizeof(map)/sizeof(*iter) - /* {NULL,NULL} terminator */1 ==
		ESODBC_DSN_ATTRS_COUNT);
	assert(0 <= cchConnStrOutMax);

	for (iter = &map[0], pos = 0; iter->val; iter ++) {
		if (iter->val->cnt) {
			braces = needs_braces(iter->val) ? 2 : 0;
			if (cchConnStrOutMax && szConnStrOut) {
				/* is there still room in the buffer? */
				if ((SQLSMALLINT)pos < cchConnStrOutMax) {
					if (braces) {
						format = WPFWP_LDESC "={" WPFWP_LDESC "};";
					} else {
						format = WPFWP_LDESC "=" WPFWP_LDESC ";";
					}
					errno = 0;
					n = swprintf(szConnStrOut + pos, cchConnStrOutMax - pos,
							format, LWSTR(iter->kw), LWSTR(iter->val));
					/* on buffer too small, swprintf() will 0-terminate it,
					 * return negative, but not set errno. */
					if (n < 0) {
						if (errno != 0) {
							ERRN("failed to print connection string (keyword: "
								LWPDL ", room: %hd, position: %zu).",
								LWSTR(iter->kw), cchConnStrOutMax, pos);
							return -1;
						}
						assert(szConnStrOut[cchConnStrOutMax - 1] == L'\0');
					}
				}
			}
			/* update the write position with what would be written (not what
			 * has been), since the untruncated length needs to always be
			 * returned to the app */
			pos += iter->kw->cnt + /*`=`*/1 +
				iter->val->cnt + braces + /*`;`*/1;
		}
	}

#ifndef NDEBUG /* don't print the PWD */
	DBG("new connection string: `" LWPD "`; out len: %zu.", szConnStrOut, pos);
#endif /* NDEBUG */
	assert(pos < LONG_MAX);
	return (long)pos;
}

/* assign defaults where not assigned and applicable */
void assign_dsn_defaults(esodbc_dsn_attrs_st *attrs)
{
	int res = 0;

	if (! attrs->cloud_id.cnt) { /* the Cloud ID will provide these attrs */
		res |= assign_dsn_attr(attrs,
				&MK_WSTR(ESODBC_DSN_SERVER), &MK_WSTR(ESODBC_DEF_SERVER),
				/*overwrite?*/FALSE);
		res |= assign_dsn_attr(attrs,
				&MK_WSTR(ESODBC_DSN_PORT), &MK_WSTR(ESODBC_DEF_PORT),
				/*overwrite?*/FALSE);
		res |= assign_dsn_attr(attrs,
				&MK_WSTR(ESODBC_DSN_SECURE), &MK_WSTR(ESODBC_DEF_SECURE),
				/*overwrite?*/FALSE);
	}
	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_TIMEOUT), &MK_WSTR(ESODBC_DEF_TIMEOUT),
			/*overwrite?*/FALSE);
	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_FOLLOW), &MK_WSTR(ESODBC_DEF_FOLLOW),
			/*overwrite?*/FALSE);

	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_PACKING), &MK_WSTR(ESODBC_DEF_PACKING),
			/*overwrite?*/FALSE);
	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_COMPRESSION), &MK_WSTR(ESODBC_DEF_COMPRESSION),
			/*overwrite?*/FALSE);
	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_MAX_FETCH_SIZE),
			&MK_WSTR(ESODBC_DEF_FETCH_SIZE),
			/*overwrite?*/FALSE);
	res |= assign_dsn_attr(attrs, &MK_WSTR(ESODBC_DSN_MAX_BODY_SIZE_MB),
			&MK_WSTR(ESODBC_DEF_MAX_BODY_SIZE_MB),
			/*overwrite?*/FALSE);

	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_APPLY_TZ), &MK_WSTR(ESODBC_DEF_APPLY_TZ),
			/*overwrite?*/FALSE);
	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_EARLY_EXEC), &MK_WSTR(ESODBC_DEF_EARLY_EXEC),
			/*overwrite?*/FALSE);
	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_SCI_FLOATS), &MK_WSTR(ESODBC_DEF_SCI_FLOATS),
			/*overwrite?*/FALSE);
	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_VARCHAR_LIMIT),
			&MK_WSTR(ESODBC_DEF_VARCHAR_LIMIT), /*overwrite?*/FALSE);
	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_MFIELD_LENIENT),
			&MK_WSTR(ESODBC_DEF_MFIELD_LENIENT), /*overwrite?*/FALSE);
	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_ESC_PVA),
			&MK_WSTR(ESODBC_DEF_ESC_PVA), /*overwrite?*/FALSE);
	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_IDX_INC_FROZEN),
			&MK_WSTR(ESODBC_DEF_IDX_INC_FROZEN), /*overwrite?*/FALSE);

	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_PROXY_ENABLED),
			&MK_WSTR(ESODBC_DEF_PROXY_ENABLED), /*overwrite?*/FALSE);
	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_PROXY_AUTH_ENA),
			&MK_WSTR(ESODBC_DEF_PROXY_AUTH_ENA), /*overwrite?*/FALSE);

	/* default: no trace file */
	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_TRACE_ENABLED),
			&MK_WSTR(ESODBC_DEF_TRACE_ENABLED), /*overwrite?*/FALSE);
	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_TRACE_LEVEL),
			&MK_WSTR(ESODBC_DEF_TRACE_LEVEL), /*overwrite?*/FALSE);

	assert(0 <= res);
}


int validate_dsn(esodbc_dsn_attrs_st *attrs, const wchar_t *dsn_str,
	wchar_t *err_out, size_t eo_max, BOOL on_connect)
{
	int ret;
	esodbc_dbc_st dbc;

	if (! dsn_str) {
		ERR("invalid NULL DSN string received.");
		/* internal error, no user-relevant message */
		return ESODBC_DSN_ISNULL_ERROR;
	}
#ifdef ESODBC_DSN_API_WITH_00_LIST
	/* this won't be "complete" if using 00-list */
#ifndef NDEBUG /* don't print the PWD */
	DBG("received DSN string starting with: `" LWPD "`.", dsn_str);
#endif /* NDEBUG */
	if (! parse_00_list(attrs, (SQLWCHAR *)dsn_str)) {
#else
#ifndef NDEBUG /* don't print the PWD */
	DBG("received DSN string: `" LWPD "`.", dsn_str);
#endif /* NDEBUG */
	if (! parse_connection_string(attrs, (SQLWCHAR *)dsn_str, SQL_NTS)) {
#endif /* ESODBC_DSN_API_WITH_00_LIST */
		ERR("failed to parse received DSN string.");
		swprintf(err_out, eo_max, L"DSN string parsing error.");
		return ESODBC_DSN_INVALID_ERROR;
	}

	/*
	 * check on the minimum DSN set requirements
	 */
	if (! (attrs->server.cnt | attrs->cloud_id.cnt))  {
		ERR("received empty server name and cloud ID");
		swprintf(err_out, eo_max,
			L"Server hostname and Cloud ID cannot be both empty.");
		return ESODBC_DSN_INVALID_ERROR;
	}
	if (!on_connect && !attrs->dsn.cnt) {
		ERR("received empty DSN name");
		swprintf(err_out, eo_max, L"DSN name cannot be empty.");
		return ESODBC_DSN_INVALID_ERROR;
	}

	/* fill in whatever's missing */
	assign_dsn_defaults(attrs);

	init_dbc(&dbc, NULL);
	ret = on_connect ? do_connect(&dbc, attrs) : config_dbc(&dbc, attrs);

	if (! SQL_SUCCEEDED(ret)) {
		ret = EsSQLGetDiagFieldW(SQL_HANDLE_DBC, &dbc, /*rec#*/1,
				SQL_DIAG_MESSAGE_TEXT, err_out, (SQLSMALLINT)eo_max,
				/*written len*/NULL/*err_out is 0-term'd*/);
		/* function should not fail with given params. */
		assert(SQL_SUCCEEDED(ret));
		ERR("test DBC %s failed: " LWPD ".",
			on_connect ? "connection" : "configuration", err_out);
		ret = ESODBC_DSN_GENERIC_ERROR;
	} else {
		ret = ESODBC_DSN_NO_ERROR; // 0
	}

	cleanup_dbc(&dbc);
	return ret;
}

static int test_connect(void *arg, const wchar_t *dsn_str,
	wchar_t *err_out, size_t eo_max, unsigned int _)
{
	esodbc_dsn_attrs_st attrs;

	assert(! arg); /* change santinel */
	init_dsn_attrs(&attrs);
	return validate_dsn(&attrs, dsn_str, err_out, eo_max, /*on conn*/TRUE);
}

/*
 * Return:
 *  < 0: error
 *  ==0: user cancels
 *  > 0: success
 */
int prompt_user_config(HWND hwnd, BOOL on_conn, esodbc_dsn_attrs_st *attrs,
	driver_callback_ft save_cb)
{
	int ret;
	wchar_t dsn_str[sizeof(attrs->buff)/sizeof(*attrs->buff)];

#if 0
	if (write_00_list(attrs, (SQLWCHAR *)dsn_str,
			sizeof(dsn_str)/sizeof(*dsn_str)) <= 0) {
#else
	if (write_connection_string(attrs, (SQLWCHAR *)dsn_str,
			sizeof(dsn_str)/sizeof(*dsn_str)) <= 0) {
#endif
		ERR("failed to serialize attributes into a DSN string.");
		return FALSE;
	}

	ret = EsOdbcDsnEdit(hwnd, on_conn, dsn_str, &test_connect, NULL,
			save_cb, attrs);
	DBG("DSN editor returned: %d.", ret);
	if (ret < 0) {
		ERR("failed to bring up the GUI; code:%d.", ret);
	}
	return ret;
}


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
