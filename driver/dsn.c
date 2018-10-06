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

#define ODBC_REG_SUBKEY_PATH	"SOFTWARE\\ODBC\\ODBC.INI"
#define REG_HKLM				"HKEY_LOCAL_MACHINE"
#define REG_HKCU				"HKEY_CURRENT_USER"


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
		{&MK_WSTR(ESODBC_DSN_CA_PATH), &attrs->ca_path},
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
BOOL TEST_API parse_connection_string(esodbc_dsn_attrs_st *attrs,
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
			continue; /* empty values are acceptable */
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

BOOL TEST_API parse_00_list(esodbc_dsn_attrs_st *attrs, SQLWCHAR *list00)
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
		{&MK_WSTR(ESODBC_DSN_SERVER), &attrs->server},
		{&MK_WSTR(ESODBC_DSN_PORT), &attrs->port},
		{&MK_WSTR(ESODBC_DSN_SECURE), &attrs->secure},
		{&MK_WSTR(ESODBC_DSN_CA_PATH), &attrs->ca_path},
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

static void log_installer_err()
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
 * Reads the system entries for a DSN given into a doubly null-terminated
 * attributes list.
 *
 * The list - as received by ConfigDSN() - only contains the 'DSN' keyword,
 * though it could contain other attributes too. These are going to be taken
 * into account, but possibly overwritten by registry entries (which
 * theoretically should be the same anyways).
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

	/* Provide the user no defaults atp. (i.e. start from empty config). The
	 * GUI will provide some defaults (ex. 9200) and config is validated
	 * before saving. => no assign_dsn_defaults(attrs); */

	return TRUE;
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
			&MK_WSTR(ESODBC_DSN_MAX_FETCH_SIZE), &new_attrs->max_fetch_size,
			old_attrs ? &old_attrs->max_fetch_size : NULL
		},
		{
			&MK_WSTR(ESODBC_DSN_MAX_BODY_SIZE_MB), &new_attrs->max_body_size,
			old_attrs ? &old_attrs->max_body_size : NULL
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
	assert(sizeof(map)/sizeof(*iter) /* {NULL,NULL, NULL} terminator */-1
		/*Driver,DSN,SAVEFILE,FILEDSN*/-4 == ESODBC_DSN_ATTRS_COUNT);

	for (iter = &map[0]; iter->kw; iter ++) {
		if (iter->old) {
			if (EQ_WSTR(iter->new, iter->old)) {
				DBG("DSN `" LWPDL "` attribute " LWPDL " maintained "
					"value `" LWPDL "`.", LWSTR(&new_attrs->dsn),
					LWSTR(iter->kw), LWSTR(iter->new));
				continue;
			}
			if (! SQLWritePrivateProfileStringW(new_attrs->dsn.str,
					iter->kw->str,
					/* "If this argument is NULL, the key pointed to by the
					 * lpszEntry argument is deleted." */
					iter->new->cnt ? iter->new->str : NULL,
					MK_WPTR(SUBKEY_ODBC))) {
				ERR("failed to write key `" LWPDL "` with value `" LWPDL "`.",
					LWSTR(iter->kw), LWSTR(iter->new));
				return FALSE;
			}
			INFO("DSN `" LWPDL "` attribute " LWPDL " set to `" LWPDL "`%s.",
				LWSTR(&new_attrs->dsn), LWSTR(iter->kw), LWSTR(iter->new),
				iter->new->cnt ? "" : " (deleted)");
		} else if (iter->new->cnt) {
			if (! SQLWritePrivateProfileStringW(new_attrs->dsn.str,
					iter->kw->str, iter->new->str, MK_WPTR(SUBKEY_ODBC))) {
				ERR("failed to write key `" LWPDL "` with value `" LWPDL "`.",
					LWSTR(iter->kw), LWSTR(iter->new));
				return FALSE;
			}
			INFO("DSN `" LWPDL "` attribute " LWPDL " set to `" LWPDL "`.",
				LWSTR(&new_attrs->dsn), LWSTR(iter->kw), LWSTR(iter->new));
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
		{&attrs->server, &MK_WSTR(ESODBC_DSN_SERVER)},
		{&attrs->port, &MK_WSTR(ESODBC_DSN_PORT)},
		{&attrs->secure, &MK_WSTR(ESODBC_DSN_SECURE)},
		{&attrs->ca_path, &MK_WSTR(ESODBC_DSN_CA_PATH)},
		{&attrs->timeout, &MK_WSTR(ESODBC_DSN_TIMEOUT)},
		{&attrs->follow, &MK_WSTR(ESODBC_DSN_FOLLOW)},
		{&attrs->catalog, &MK_WSTR(ESODBC_DSN_CATALOG)},
		{&attrs->packing, &MK_WSTR(ESODBC_DSN_PACKING)},
		{&attrs->max_fetch_size, &MK_WSTR(ESODBC_DSN_MAX_FETCH_SIZE)},
		{&attrs->max_body_size, &MK_WSTR(ESODBC_DSN_MAX_BODY_SIZE_MB)},
		{&attrs->trace_file, &MK_WSTR(ESODBC_DSN_TRACE_FILE)},
		{&attrs->trace_level, &MK_WSTR(ESODBC_DSN_TRACE_LEVEL)},
		{NULL, NULL}
	};

	/* check that the esodbc_dsn_attrs_st stays in sync with the above */
	assert(sizeof(map)/sizeof(*iter) - /* {NULL,NULL} terminator */1 ==
		ESODBC_DSN_ATTRS_COUNT);

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
					format = WPFWP_LDESC "={" WPFWP_LDESC "};";
				} else {
					format = WPFWP_LDESC "=" WPFWP_LDESC ";";
				}
				n = swprintf(szConnStrOut + pos, cchConnStrOutMax - pos,
						format, LWSTR(iter->kw), LWSTR(iter->val));
				if (n < 0) {
					ERRN("failed to outprint connection string (space "
						"left: %d; needed: %d).", cchConnStrOutMax - pos,
						iter->val->cnt);
					return -1;
				} else {
					pos += n;
				}
			} else {
				/* simply increment the counter, since the untruncated length
				 * needs to be returned to the app */
				pos += iter->kw->cnt + /*`=`*/1 +
					iter->val->cnt + braces + /*`;`*/1;
			}
		}
	}

	DBG("Output connection string: `" LWPD "`; out len: %d.",
		szConnStrOut, pos);
	assert(pos < LONG_MAX);
	return (long)pos;
}

/* assign defaults where not assigned and applicable */
void assign_dsn_defaults(esodbc_dsn_attrs_st *attrs)
{
	int res = 0;

	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_SERVER), &MK_WSTR(ESODBC_DEF_SERVER),
			/*overwrite?*/FALSE);
	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_PORT), &MK_WSTR(ESODBC_DEF_PORT),
			/*overwrite?*/FALSE);
	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_SECURE), &MK_WSTR(ESODBC_DEF_SECURE),
			/*overwrite?*/FALSE);
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
			&MK_WSTR(ESODBC_DSN_MAX_FETCH_SIZE),
			&MK_WSTR(ESODBC_DEF_FETCH_SIZE),
			/*overwrite?*/FALSE);
	res |= assign_dsn_attr(attrs, &MK_WSTR(ESODBC_DSN_MAX_BODY_SIZE_MB),
			&MK_WSTR(ESODBC_DEF_MAX_BODY_SIZE_MB),
			/*overwrite?*/FALSE);

	/* default: no trace file */
	res |= assign_dsn_attr(attrs,
			&MK_WSTR(ESODBC_DSN_TRACE_LEVEL), &MK_WSTR(ESODBC_DEF_TRACE_LEVEL),
			/*overwrite?*/FALSE);

	assert(0 <= res);
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

int validate_dsn(esodbc_dsn_attrs_st *attrs, const wchar_t *dsn_str,
	wchar_t *err_out, size_t eo_max, BOOL try_connect)
{
	int ret;
	esodbc_dbc_st dbc;

	if (! dsn_str) {
		ERR("invalid NULL DSN string received.");
		return ESODBC_DSN_ISNULL_ERROR;
	} else {
#ifdef ESODBC_DSN_API_WITH_00_LIST
		/* this won't be "complete" if using 00-list */
		DBG("received DSN string starting with: `" LWPD "`.", dsn_str);
#else /* ESODBC_DSN_API_WITH_00_LIST */
		DBG("received DSN string: `" LWPD "`.", dsn_str);
#endif /* ESODBC_DSN_API_WITH_00_LIST */
	}

#ifdef ESODBC_DSN_API_WITH_00_LIST
	if (! parse_00_list(attrs, (SQLWCHAR *)dsn_str)) {
#else
	if (! parse_connection_string(attrs, (SQLWCHAR *)dsn_str, SQL_NTS)) {
#endif /* ESODBC_DSN_API_WITH_00_LIST */
		ERR("failed to parse received DSN string.");
		return ESODBC_DSN_INVALID_ERROR;
	}

	/*
	 * validate the DSN set
	 */
	if (! (attrs->dsn.cnt && attrs->server.cnt)) {
		ERR("DSN name (" LWPDL ") and server address (" LWPDL ") cannot be"
			" empty strings.", LWSTR(&attrs->dsn), LWSTR(&attrs->server));
		return ESODBC_DSN_INVALID_ERROR;
	} else {
		/* fill in whatever's missing */
		assign_dsn_defaults(attrs);
	}

	init_dbc(&dbc, NULL);
	if (try_connect) {
		ret = do_connect(&dbc, attrs);
		if (! SQL_SUCCEEDED(ret)) {
			ret = EsSQLGetDiagFieldW(SQL_HANDLE_DBC, &dbc, /*rec#*/1,
					SQL_DIAG_MESSAGE_TEXT, err_out, (SQLSMALLINT)eo_max,
					/*written len*/NULL/*err_out is 0-term'd*/);
			/* function should not fail with given params. */
			assert(SQL_SUCCEEDED(ret));
			ERR("test DBC connection failed: " LWPD ".", err_out);
			ret = ESODBC_DSN_GENERIC_ERROR;
		} else {
			ret = ESODBC_DSN_NO_ERROR; // 0
		}
	} else {
		init_dbc(&dbc, NULL);
		ret = config_dbc(&dbc, attrs);
		if (! SQL_SUCCEEDED(ret)) {
			ERR("test DBC configuration failed.");
			ret = ESODBC_DSN_INVALID_ERROR;
		} else {
			ret = ESODBC_DSN_NO_ERROR; // 0
		}
	}
	cleanup_dbc(&dbc);
	return ret;

}

static int test_connect(void *arg, const wchar_t *dsn_str,
	wchar_t *err_out, size_t eo_max, unsigned int _)
{
	esodbc_dsn_attrs_st attrs;
	esodbc_dbc_st dbc;
	SQLRETURN res;
	int ret;

	assert(! arg); /* change santinel */
	if (! dsn_str) {
		ERR("invalid NULL DSN string received.");
		return ESODBC_DSN_ISNULL_ERROR;
	} else {
		DBG("received DSN string: `" LWPD "`.", dsn_str);
	}

	init_dsn_attrs(&attrs);
#ifdef ESODBC_DSN_API_WITH_00_LIST
	if (! parse_00_list(&attrs, (SQLWCHAR *)dsn_str)) {
#else
	if (! parse_connection_string(&attrs, (SQLWCHAR *)dsn_str,
			(SQLSMALLINT)wcslen(dsn_str))) {
#endif /* ESODBC_DSN_API_WITH_00_LIST */
		ERR("failed to parse received DSN string.");
		return ESODBC_DSN_INVALID_ERROR;
	}
	/* fill in whatever's missing */
	assign_dsn_defaults(&attrs);

	init_dbc(&dbc, NULL);
	res = do_connect(&dbc, &attrs);
	if (! SQL_SUCCEEDED(res)) {
		res = EsSQLGetDiagFieldW(SQL_HANDLE_DBC, &dbc, /*rec#*/1,
				SQL_DIAG_MESSAGE_TEXT, err_out, (SQLSMALLINT)eo_max,
				/*written len*/NULL/*err_out is 0-term'd*/);
		/* function should not fail with given params. */
		assert(SQL_SUCCEEDED(res));
		ret = ESODBC_DSN_GENERIC_ERROR;
	} else {
		ret = 0;
	}

	cleanup_dbc(&dbc);
	return ret;
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
	if (ret < 0) {
		ERR("failed to bring up the GUI; code:%d.", ret);
	}
	return ret;
}


/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
