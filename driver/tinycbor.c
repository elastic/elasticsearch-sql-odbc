/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#include "tinycbor.h"
#include "defs.h"

/* Vars for track keeping of thread-local UTF8-UTF16 conversion (buffers
 * allocated by cbor_value_get_utf16_wstr()).
 * Note: these can't be freed per thread, since
 * DllMain(DLL_THREAD_ATTACH/DLL_THREAD_DETACH) is optional (and apps are
 * inconsistent even calling attach-detach for same thread). */
static wchar_t **u16buffs = NULL;
static size_t u16buff_cnt = 0;
static esodbc_mutex_lt u16buff_mux = ESODBC_MUX_SINIT;

/* advance an iterator of an "entered" JSON-sytle map to the value for the
 * given key, if that exists */
CborError cbor_map_advance_to_key(CborValue *it, const char *key,
	size_t key_len, CborValue *val)
{
	CborError res;
	const char *buffptr;
	size_t bufflen;

	while (! cbor_value_at_end(it)) {
		/* skip all tags */
		if ((res = cbor_value_skip_tag(it)) != CborNoError) {
			return res;
		}
		/* if current key is a string, get its name */
		if (cbor_value_is_text_string(it)) {
			res = cbor_value_get_string_chunk(it, &buffptr, &bufflen);
			if (res != CborNoError) {
				return res;
			}
			/* this assumes an ASCII key (which is the case with ES' info, but
			 * not generally valid for CBOR/JSON-style maps) */
			if (bufflen != key_len || strncasecmp(key, buffptr, key_len)) {
				/* skip all tags */
				if ((res = cbor_value_skip_tag(it)) != CborNoError) {
					return res;
				}
				/* advance past param's value */
				if ((res = cbor_value_advance(it)) != CborNoError) {
					return res;
				}
				continue;
			}
		}
		/* found it! is there anything following it? */
		/* TODO: does this check the entire obj or just the container?? */
		if (cbor_value_at_end(it)) {
			return CborErrorTooFewItems;
		}
		*val = *it;
		return CborNoError;
	}
	/* key not found */
	val->type = CborInvalidType;
	return CborNoError;
}

CborError cbor_map_lookup_keys(CborValue *map, size_t cnt,
	const char **keys, const size_t *lens, CborValue **objs, BOOL drain)
{
	CborError res;
	CborValue it;
	const char *buffptr;
	size_t bufflen;
	size_t i, found;

	assert(cbor_value_is_map(map));
	if ((res = cbor_value_enter_container(map, &it)) != CborNoError) {
		return res;
	}

	/* mark all out values invalid since only the found keys are going to be
	 * returned as valid */
	for (i = 0; i < cnt; i ++) {
		objs[i]->type = CborInvalidType;
	}

	found = 0;
	while ((! cbor_value_at_end(&it)) && (found < cnt)) {
		/* skip all tags */
		if ((res = cbor_value_skip_tag(&it)) != CborNoError) {
			return res;
		}
		/* is current key is a string, get its name */
		if (cbor_value_is_text_string(&it)) {
			res = cbor_value_get_string_chunk(&it, &buffptr, &bufflen);
			if (res != CborNoError) {
				return res;
			}
			// TODO: binary search on ordered keys?
			for (i = 0; i < cnt; i ++) {
				/* this assumes an ASCII key (which is the case with ES' info,
				 * but not generally valid for CBOR/JSON-like maps) */
				if (bufflen == lens[i] &&
					strncasecmp(keys[i], buffptr, lens[i]) == 0) {
					*objs[i] = it;
					found ++;
					break;
				}
			}
		}

		/* skip all tags */
		if ((res = cbor_value_skip_tag(&it)) != CborNoError) {
			return res;
		}
		/* advance past param's value */
		if ((res = cbor_value_advance(&it)) != CborNoError) {
			return res;
		}
	}

	if (drain) {
		while (! cbor_value_at_end(&it)) {
			if ((res = cbor_value_advance(&it)) != CborNoError) {
				return res;
			}
		}

		return cbor_value_leave_container(map, &it);
	} else {
		return CborNoError;
	}
}

CborError cbor_container_count(CborValue cont, size_t *count)
{
	CborError res;
	CborValue it;
	size_t cnt = 0;

	assert(cbor_value_is_container(&cont));

	if ((res = cbor_value_enter_container(&cont, &it)) != CborNoError) {
		return res;
	}
	while (! cbor_value_at_end(&it)) {
		if (! cbor_value_is_tag(&it)) {
			cnt ++;
		}
		if ((res = cbor_value_advance(&it)) != CborNoError) {
			return res;
		}
	}
	*count = cnt;
	return CborNoError;
}

// XXX cbor_get_map_count() should also be useful
CborError cbor_get_array_count(CborValue arr, size_t *count)
{
	assert(cbor_value_is_array(&arr));

	return cbor_value_is_length_known(&arr) ?
		cbor_value_get_array_length(&arr, count) :
		cbor_container_count(arr, count);
}

CborError cbor_container_is_empty(CborValue cont, BOOL *empty)
{
	CborError res;
	CborValue it;

	assert(cbor_value_is_container(&cont));

	if ((res = cbor_value_enter_container(&cont, &it)) != CborNoError) {
		return res;
	}
	/* skip all tags */
	if ((res = cbor_value_skip_tag(&it)) != CborNoError) {
		return res;
	}
	*empty = cbor_value_at_end(&it) || (! cbor_value_is_valid(&it));
	return CborNoError;
}

static BOOL enlist_utf16_buffer(wchar_t *old, wchar_t *new)
{
	wchar_t **r;
	size_t i;

	if (! old) {
		/* new entry must be inserted into list */
		ESODBC_MUX_LOCK(&u16buff_mux);
		r = realloc(u16buffs, (u16buff_cnt + 1) * sizeof(wchar_t *));
		if (r) {
			u16buffs = r;
			u16buffs[u16buff_cnt ++] = new;
		}
		ESODBC_MUX_UNLOCK(&u16buff_mux);
	} else {
		ESODBC_MUX_LOCK(&u16buff_mux);
		r = NULL;
		for (i = 0; i < u16buff_cnt; i ++) {
			if (u16buffs[i] == old) {
				r = &u16buffs[i];
				u16buffs[i] = new;
				break;
			}
		}
		ESODBC_MUX_UNLOCK(&u16buff_mux);
	}

	return !!r;
}

void tinycbor_cleanup()
{
	size_t i;
	for (i = 0; i < u16buff_cnt; i ++) {
		free(u16buffs[i]);
	}
	if (i) {
		free(u16buffs);
	}
}

/* Fetches and converts a(n always UTF8) text string to UTF16 wide char.
 * Uses a dynamically allocated thread-local buffer. */
CborError cbor_value_get_utf16_wstr(CborValue *it, wstr_st *utf16)
{
	static thread_local wstr_st wbuff = {.str = NULL, .cnt = (size_t)-1};
	wstr_st r; /* reallocated */
	cstr_st mb_str; /* multibyte string */
	CborError res;
	int n;

	assert(cbor_value_is_text_string(it));
	/* get the multibyte string to convert */
	res = cbor_value_get_string_chunk(it, &mb_str.str, &mb_str.cnt);
	if (res != CborNoError) {
		return res;
	}
	/* attempt string conversion */
	while ((n = U8MB_TO_U16WC(mb_str.str, mb_str.cnt, wbuff.str,
					wbuff.cnt)) <= 0) {
		/* U8MB_TO_U16WC will return error (though not set it with
		 * SetLastError()) for empty source strings */
		if (! mb_str.cnt) {
			utf16->cnt = 0;
			utf16->str = NULL;
			return CborNoError;
		}
		/* is this a non-buffer related error? (like decoding) */
		if ((! WAPI_ERR_EBUFF()) && wbuff.str) {
			return CborErrorInvalidUtf8TextString;
		} /* else: buffer hasn't yet been allocated or is too small */
		/* what's the minimum space needed? */
		if ((n = U8MB_TO_U16WC(mb_str.str, mb_str.cnt, NULL, 0)) < 0) {
			return CborErrorInvalidUtf8TextString;
		}
		/* double scratchpad size until exceeding min needed space.
		 * condition on equality, to allow for a 0-term */
		for (r.cnt = wbuff.cnt < (size_t)-1 ? wbuff.cnt :
				ESODBC_BODY_BUF_START_SIZE; r.cnt <= (size_t)n; r.cnt *= 2) {
			;
		}
		if (! (r.str = realloc(wbuff.str, r.cnt))) {
			return CborErrorOutOfMemory;
		}
		if (! enlist_utf16_buffer(wbuff.str, r.str)) {
			/* it should only fail on 1st allocation per-thread */
			assert(! wbuff.str);
			free(r.str);
			return CborErrorOutOfMemory;
		} else {
			wbuff = r;
		}
		DBG("new UTF8/16 conv. buffer @0x%p, size %zu.", wbuff.str, wbuff.cnt);
	}

	/* U8MB_TO_U16WC() will only convert the 0-term if counted in input*/
	wbuff.str[n] = '\0'; /* set, but not counted */
	utf16->str = wbuff.str;
	utf16->cnt = n;
	return CborNoError;
}

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
