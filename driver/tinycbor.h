/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#ifndef __TINYCBOR_H__
#define __TINYCBOR_H__

#include <cbor.h>

#include "log.h"
#include "util.h"

#define JUMP_ON_CBOR_ERR(_res, _lbl, _hdl, _fmt, ...)	\
	do { \
		if (_res != CborNoError) { \
			ERRH(_hdl, "CBOR: %s -- " _fmt ".", cbor_error_string(_res), \
				__VA_ARGS__); \
			goto _lbl; \
		} \
	} while (0)

#ifndef NDEBUG
#	define ES_CBOR_PARSE_FLAGS	CborValidateStrictest
#else /* !NDEBUG */
#	define ES_CBOR_PARSE_FLAGS	CborValidateBasic
#endif /* !NDEBUG */

#define CBOR_LEN_IMMEDIATE_MAX	23 /* numeric value encoded within the hdr */
#define CBOR_OBJ_BOOL_LEN		1
#define CBOR_OBJ_HFLOAT_LEN		(/*initial byte*/1 + /* half prec. float */2)
#define CBOR_OBJ_FLOAT_LEN		(/*initial byte*/1 + /* single prec. float */4)
#define CBOR_OBJ_DOUBLE_LEN		(/*initial byte*/1 + /* double prec. float */8)

/* Calculates the length of the preamble/header of a non-nummeric serialized
 * object, where 'item_len' is:
 * - the length of a text/byte string; or
 * - the count of elements in an array; or
 * - the count of pairs in a map.
 * (Similar functionality covered by tinycbor's encode_number_no_update()
 * internal-only function.) */
static inline size_t cbor_nn_hdr_len(size_t item_len)
{
	size_t len_sz; /* size of the length field in bytes (1/2/4/8) */
	if (item_len <= CBOR_LEN_IMMEDIATE_MAX) {
		len_sz = 0;
	} else if (item_len <= UINT8_MAX) {
		len_sz = sizeof(uint8_t);
	} else if (item_len <= UINT16_MAX) {
		len_sz = sizeof(uint16_t);
	} else if (item_len <= UINT32_MAX) {
		len_sz = sizeof(uint32_t);
	} else {
		len_sz = sizeof(uint64_t);
	}
	return /*initial leading byte*/1 + len_sz;
}

#define CBOR_INT_OBJ_LEN(_val) \
	((0 <= (_val)) ? cbor_nn_hdr_len(_val) : cbor_nn_hdr_len(-1 - (_val)))


/* Calculates the serialized object length of a CBOR string (text, byte)
 * object.
 * (Similar functionality covered by tinycbor's internal only
 * encode_number_no_update() function.) */
static inline size_t cbor_str_obj_len(size_t item_len)
{
	return cbor_nn_hdr_len(item_len) + item_len;
}


/* advance an iterator of an "entered" JSON-sytle map to the value for the
 * given key, if that exists */
CborError cbor_map_advance_to_key(CborValue *it, const char *key,
	size_t key_len, CborValue *val);
/* similar to cbor_value_leave_container(), but the iterator may find itself
 * anywhere within the container (and not necessarily at the end of it). */
CborError cbor_value_exit_container(CborValue *cont, CborValue *it);
/* Looks up a number of 'cnt' objects mapped to the 'keys' of given
 * 'len[gth]s'. If a key is not found, the corresponding objects are marked
 * with an invalid type. */
CborError cbor_map_lookup_keys(CborValue *map, size_t cnt,
	const char **keys, const size_t *lens, CborValue **objs);
CborError cbor_container_count(CborValue cont, size_t *count);
CborError cbor_get_array_count(CborValue arr, size_t *count);
CborError cbor_container_is_empty(CborValue cont, BOOL *empty);

CborError cbor_value_get_utf16_wstr(CborValue *it, wstr_st *utf8);
void tinycbor_cleanup();


/* function defined in cborparser.c file "patched" in CMakeLists.txt */
CborError cbor_value_get_string_chunk(CborValue *it,
	const char **bufferptr, size_t *len);

#endif /* __TINYCBOR_H__ */
