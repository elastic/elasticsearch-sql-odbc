/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#ifndef __HANDLES_H__
#define __HANDLES_H__

#include <curl/curl.h>
#include <ujdecode.h>

#include "error.h"
#include "defs.h"
#include "log.h"
#include "tinycbor.h"

/* forward declarations */
struct struct_env;
struct struct_dbc;
struct struct_stmt;
struct struct_desc;

/*
 * header structure, embedded in all API handles
 * Note: must remain the first declared member.
 */
typedef struct struct_hheader { /* handle header */
	/* SQL_HANDLE_ENV / _DBC / _STMT / _DESC  */
	SQLSMALLINT type;
	/* diagnostic/state keeping */
	esodbc_diag_st diag;
	/* ODBC API multi-threading exclusive lock */
	esodbc_mutex_lt mutex;
	/* back reference to "parent" structure (in type hierarchy) */
	union {
		struct struct_env *env;
		struct struct_dbc *dbc;
		struct struct_stmt *stmt;
		void *parent;
	};
	/* logging helpers */
	wstr_st typew; /* ENV/DBC/STMT/DESC as w-string */
	esodbc_filelog_st *log; /* logger: owned by a DBC; ENV uses global */
} esodbc_hhdr_st;

/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/environment-handles :
 * """
 * The environment's state
 * The current environment-level diagnostics
 * The handles of connections currently allocated on the environment
 * The current settings of each environment attribute
 * """
 */
typedef struct struct_env {
	esodbc_hhdr_st hdr;
	SQLUINTEGER version; /* versions defined as UL (see SQL_OV_ODBC3) */
} esodbc_env_st;

/* meta data types (same for both SQL_C_<t> and SQL_<t> types) */
typedef enum {
	METATYPE_UNKNOWN = 0,
	METATYPE_EXACT_NUMERIC,
	METATYPE_FLOAT_NUMERIC,
	METATYPE_STRING,
	METATYPE_BIN,
	METATYPE_DATE_TIME,
	METATYPE_INTERVAL_WSEC,
	METATYPE_INTERVAL_WOSEC,
	METATYPE_BIT,
	METATYPE_UID,
	METATYPE_MAX // SQL_C_DEFAULT, ESODBC_SQL_NULL
} esodbc_metatype_et;

/* Structure mapping one ES/SQL data type. */
typedef struct elasticsearch_type {
	/* fields of one row returned  in response to 'SYS TYPES' query */
	wstr_st			type_name;
	SQLSMALLINT		data_type; /* maps to rec's .concise_type member */
	SQLINTEGER		column_size;
	wstr_st			literal_prefix;
	wstr_st			literal_suffix;
	wstr_st			create_params;
	SQLSMALLINT		nullable;
	SQLSMALLINT		case_sensitive;
	SQLSMALLINT		searchable;
	SQLSMALLINT		unsigned_attribute;
	SQLSMALLINT		fixed_prec_scale;
	SQLSMALLINT		auto_unique_value;
	wstr_st			local_type_name;
	SQLSMALLINT		minimum_scale;
	SQLSMALLINT		maximum_scale;
	SQLSMALLINT		sql_data_type; /* :-> rec's .type member */
	SQLSMALLINT		sql_datetime_sub; /* :-> rec's .datetime_interval_code */
	SQLINTEGER		num_prec_radix;
	SQLSMALLINT		interval_precision;

	/* number of SYS TYPES result columns mapped over the above members */
#define ESODBC_TYPES_COLUMNS	19

	/* SQL C type driver mapping of ES' data_type; this is derived from
	 * .type_name, rathern than .(sql_)data_type (sice the name is the
	 * "unique" key and sole identifier in general queries results). */
	SQLSMALLINT			c_concise_type;
	/* There should be no need for a supplemental 'sql_c_type': if
	 * rec.datetime_interval_code == 0, then this member would equal the
	 * concise one (above); else, rec.type will contain the right value
	 * already (i.e. they'd be the same for SQL and SQL C data types). */

	/* helper member, to characterize the type */
	esodbc_metatype_et	meta_type;
	SQLLEN				display_size;
	/* convenience C-string conversion of type_name */
	cstr_st				type_name_c;
} esodbc_estype_st;


/*
 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/connection-handles :
 * """
 * The state of the connection
 * The current connection-level diagnostics
 * The handles of statements and descriptors currently allocated on the
 * connection
 * The current settings of each connection attribute
 * """
 */
typedef struct struct_dbc {
	esodbc_hhdr_st hdr;

	wstr_st dsn; /* data source name SQLGetInfo(SQL_DATA_SOURCE_NAME) */
	wstr_st server; /* ~ name; requested with SQLGetInfo(SQL_SERVER_NAME) */
	wstr_st catalog; /* cached value; checked against if app setting it */
	wstr_st srv_ver; /* server version: SQLGetInfo(SQL_DBMS_VER) */

	cstr_st proxy_url;
	cstr_st proxy_uid;
	cstr_st proxy_pwd;
	cstr_st url; /* SQL URL (posts) */
	cstr_st close_url; /* SQL close URL (posts) */
	cstr_st root_url; /* root URL (gets) */
	enum {
		ESODBC_SEC_NONE = 0,
		ESODBC_SEC_USE_SSL, /* 1 */
		ESODBC_SEC_CHECK_CA,
		ESODBC_SEC_CHECK_HOST,
		ESODBC_SEC_CHECK_REVOKE, /* 4 */
		ESODBC_SEC_MAX /* meta */
	} secure;
	cstr_st ca_path;

	cstr_st uid;
	cstr_st pwd;
	SQLUINTEGER timeout;
	BOOL follow;
	struct {
		size_t max; /* max fetch size */
		char *str; /* as string */
		char slen; /* string's length (w/o terminator) */
	} fetch;
	BOOL pack_json; /* should JSON be used in REST bodies? (vs. CBOR) */
	enum {
		ESODBC_CMPSS_OFF = 0,
		ESODBC_CMPSS_ON,
		ESODBC_CMPSS_AUTO,
	} compression;
	BOOL apply_tz; /* should the times be converted from UTC to local TZ? */
	BOOL early_exec; /* should prepared, non-param queries be exec'd early? */
	enum {
		ESODBC_FLTS_DEFAULT = 0,
		ESODBC_FLTS_SCIENTIFIC,
		ESODBC_FLTS_AUTO,
	} sci_floats; /* floats printing on conversion */
	BOOL mfield_lenient; /* 'field_multi_value_leniency' request param */
	BOOL idx_inc_frozen; /* 'field_multi_value_leniency' request param */
	BOOL auto_esc_pva; /* auto-escape PVA args in catalog functions */

	esodbc_estype_st *es_types; /* array with ES types */
	SQLULEN no_types; /* number of types in array */
	/* maximum precision/length of types using same SQL data type ID */
	esodbc_estype_st *max_varchar_type; /* pointer to TEXT type */
	esodbc_estype_st *max_float_type; /* pointer to DOUBLE type */
	/* configuration imposed lengths for the ES/SQL string types */
	SQLUINTEGER varchar_limit;
	wstr_st varchar_limit_str; /* convenience w-string of varchar limit */

	CURL *curl; /* cURL handle */
	CURLcode curl_err;
	char curl_err_buff[CURL_ERROR_SIZE];
	enum {
		ESODBC_CURL_NONE = 0, /* init value */
		ESODBC_CURL_QUERY,
		ESODBC_CURL_CLOSE,
		ESODBC_CURL_ROOT
	} crr_url; /* curl is set to 'url', 'close_url' or 'root_url' (above) */
	char *abuff; /* buffer holding the answer */
	size_t alen; /* size of abuff */
	size_t apos; /* current write position in the abuff */
	size_t amax; /* maximum length (bytes) that abuff can grow to */
	esodbc_mutex_lt curl_mux; /* mutex for above 'networking' members */

	/* window handler */
	HWND hwin;

	/* options */
	SQLULEN metadata_id; // default: SQL_FALSE
} esodbc_dbc_st;

typedef struct desc_rec {
	/* back ref to owning descriptor */
	struct struct_desc	*desc;

	/* helper member, to characterize the type */
	esodbc_metatype_et	meta_type;

	/* pointer to the ES/SQL type in DBC array
	 * need to be set for records in IxD descriptors */
	esodbc_estype_st	*es_type;

	/* IRD reference copy of respective protocol value */
	union {
		UJObject json;
		CborValue cbor;
	} i_val;

	/*
	 * record fields
	 */
	/* following record fields have been moved into es_type:
	 * display_size, literal_prefix, literal_suffix, local_type_name,
	 * type_name, auto_unique_value, case_sensitive, fixed_prec_scale,
	 * nullable, searchable, usigned  */
	/* record types (SQL_<t> for IxD, or SQL_C_<t> for AxD) */
	SQLSMALLINT		concise_type;
	SQLSMALLINT		type;
	SQLSMALLINT		datetime_interval_code;

	SQLPOINTER		data_ptr; /* array, if .array_size > 1 */

	wstr_st			base_column_name; /* read-only */
	wstr_st			base_table_name; /* r/o */
	wstr_st			catalog_name; /* r/o */
	wstr_st			label; /* r/o */ //alias?
	wstr_st			name;
	wstr_st			schema_name; /* r/o */
	wstr_st			table_name; /* r/o */

	SQLLEN			*indicator_ptr; /* array, if .array_size > 1 */
	SQLLEN			*octet_length_ptr; /* array, if .array_size > 1 */

	SQLLEN			octet_length;
	SQLULEN			length;

	SQLINTEGER		datetime_interval_precision; /*TODO: -> es_type? */
	SQLINTEGER		num_prec_radix; /*TODO: -> es_type? */

	SQLSMALLINT		parameter_type;
	/* "number of digits for an exact numeric type, the number of bits in the
	 * mantissa (binary precision) for an approximate numeric type, or the
	 * numbers of digits in the fractional seconds component "*/
	/* Intervals: "the number of decimal digits allowed in the fractional part
	 * of the seconds value" */
	SQLSMALLINT		precision;
	SQLSMALLINT		rowver;
	SQLSMALLINT		scale;
	SQLSMALLINT		unnamed;
	SQLSMALLINT		updatable;
} esodbc_rec_st;


typedef enum {
	DESC_TYPE_ANON, /* SQLAllocHandle()'ed */
	DESC_TYPE_ARD,
	DESC_TYPE_IRD,
	DESC_TYPE_APD,
	DESC_TYPE_IPD,
} desc_type_et;

/* type is for an application descriptor */
#define DESC_TYPE_IS_APPLICATION(_dtype) \
	(_dtype == DESC_TYPE_ARD || _dtype == DESC_TYPE_APD)
/* type is for an implementation descriptor */
#define DESC_TYPE_IS_IMPLEMENTATION(_dtype) \
	(_dtype == DESC_TYPE_IRD || _dtype == DESC_TYPE_IPD)
/* type is for a record descriptor */
#define DESC_TYPE_IS_RECORD(_dtype) \
	(_dtype == DESC_TYPE_ARD || _dtype == DESC_TYPE_IRD)
/* type is for a parameter descriptor */
#define DESC_TYPE_IS_PARAMETER(_dtype) \
	(_dtype == DESC_TYPE_APD || _dtype == DESC_TYPE_IPD)


typedef struct struct_desc {
	esodbc_hhdr_st hdr;

	desc_type_et type; /* APD, IPD, ARD, IRD */

	/* header fields */
	/* descriptor was allocated automatically by the driver or explicitly by
	 * the application */
	SQLSMALLINT		alloc_type;
	/* ARDs: the number of rows returned by each call to SQLFetch* = the rowset
	 * APDs: the number of values for each parameter. */
	SQLULEN			array_size;
	SQLUSMALLINT	*array_status_ptr;
	SQLLEN			*bind_offset_ptr;
	SQLINTEGER		bind_type; /* row/col */
	SQLSMALLINT		count; /* of recs */
	SQLULEN			*rows_processed_ptr;
	/* /header fields */

	/* array of records of .count cardinality */
	esodbc_rec_st *recs;
} esodbc_desc_st;

/* the ES/SQL type must be set for implementation descriptor records */
#define ASSERT_IXD_HAS_ES_TYPE(_rec) \
	assert(DESC_TYPE_IS_IMPLEMENTATION(_rec->desc->type) && _rec->es_type)

struct resultset_cbor {
	cstr_st curs; /* ES'es cursor */
	BOOL curs_allocd; /* curs.str is allocated (and reassembled) */
	CborValue rows_obj; /* top object rows container (EsSQLRowCount()) */
	CborValue rows_iter; /* iterator over received rows; refs req's body */
	wstr_st cols_buff /* columns descriptions; refs allocated chunk */;
};

struct resultset_json {
	wstr_st curs; /* ES'es cursor; refs UJSON4C 'state' */
	void *state; /* top UJSON decoder state */
	UJObject rows_obj; /* top object rows container (EsSQLRowCount()) */
	void *rows_iter; /* UJSON iterator with the rows in result set */
	UJObject row_array; /* UJSON object for current row */
};

typedef struct struct_resultset {
	long code; /* HTTP code of last response */
	cstr_st body; /* HTTP body of last answer to a statement */

	BOOL pack_json; /* the server could send a JSON answer for a CBOR req. */
	union {
		struct resultset_cbor cbor;
		struct resultset_json json;
	} pack;

	size_t vrows; /* (count of) visited rows in current result set  */
} resultset_st;

#define STMT_HAS_CURSOR(_stmt)	\
	((_stmt)->rset.pack_json ? \
		(_stmt)->rset.pack.json.curs.cnt : \
		(_stmt)->rset.pack.cbor.curs.cnt)

/*
 * "The fields of an IRD have a default value only after the statement has
 * been prepared or executed and the IRD has been populated, not when the
 * statement handle or descriptor has been allocated. Until the IRD has been
 * populated, any attempt to gain access to a field of an IRD will return an
 * error."
 */

typedef struct struct_stmt {
	esodbc_hhdr_st hdr;

	/* cache UTF8 JSON serialized SQL: can be (re)used with varying params */
	cstr_st u8sql;

	/* pointers to the current descriptors */
	esodbc_desc_st *ard;
	esodbc_desc_st *ird;
	esodbc_desc_st *apd;
	esodbc_desc_st *ipd;
	/* initial implicit descriptors allocated with the statement */
	esodbc_desc_st i_ard;
	esodbc_desc_st i_ird;
	esodbc_desc_st i_apd;
	esodbc_desc_st i_ipd;

	/* options */
	SQLULEN bookmarks; //default: SQL_UB_OFF
	SQLULEN metadata_id; // default: copied from connection
	/* "the maximum amount of data that the driver returns from a character or
	 * binary column" */
	SQLULEN max_length;
	/* "number of seconds to wait for an SQL statement to execute before
	 * returning to the application." */
	SQLULEN query_timeout;

	/* [current] result set (= one page from ES/SQL; can contain a cursor) */
	resultset_st rset;
	/* count of result sets fetched */
	size_t nset;
	/* total visited rows (SUM(resultset.vrows)) <=> SQL_ATTR_ROW_NUMBER */
	size_t tv_rows;
	/* SQL data types conversion to SQL C compatibility (IRD.SQL -> ARD.C) */
	enum {
		CONVERSION_VIOLATION = -2, /* specs disallowed */
		CONVERSION_UNSUPPORTED, /* ES/driver not supported */
		CONVERSION_UNCHECKED, /* 0 */
		CONVERSION_SUPPORTED,
		CONVERSION_SKIPPED, /* used with driver's meta queries */
	} sql2c_conversion;
	/* early execution */
	BOOL early_executed;

	/* SQLGetData state members */
	SQLINTEGER gd_col; /* current column to get from, if positive */
	SQLINTEGER gd_ctype; /* current target type */
	SQLLEN gd_offt; /* position in source buffer */

} esodbc_stmt_st;

/* reset statment's result set count and number of visited rows */
#define STMT_ROW_CNT_RESET(_stmt)	\
	do {  \
		(_stmt)->nset = 0; \
		(_stmt)->tv_rows = 0; \
	} while (0)

/* SQLGetData() state reset */
#define STMT_GD_RESET(_stmt)		\
	do { \
		_stmt->gd_col = -1; \
		_stmt->gd_ctype = 0; \
		_stmt->gd_offt = 0; \
	} while (0)
/* is currently a SQLGetData() call being serviced? */
#define STMT_GD_CALLING(_stmt)		(0 <= _stmt->gd_col)

SQLRETURN update_rec_count(esodbc_desc_st *desc, SQLSMALLINT new_count);
SQLSMALLINT count_bound(esodbc_desc_st *desc);
esodbc_rec_st *get_record(esodbc_desc_st *desc, SQLSMALLINT rec_no, BOOL grow);
void dump_record(esodbc_rec_st *rec);

void init_dbc(esodbc_dbc_st *dbc, SQLHANDLE InputHandle);

esodbc_desc_st *getdata_set_ard(esodbc_stmt_st *stmt, esodbc_desc_st *gd_ard,
	SQLUSMALLINT colno, esodbc_rec_st *recs, SQLUSMALLINT count);
void getdata_reset_ard(esodbc_stmt_st *stmt, esodbc_desc_st *ard,
	SQLUSMALLINT colno, esodbc_rec_st *recs, SQLUSMALLINT count);

void concise_to_type_code(SQLSMALLINT concise, SQLSMALLINT *type,
	SQLSMALLINT *code);
esodbc_metatype_et concise_to_meta(SQLSMALLINT concise_type,
	desc_type_et desc_type);

SQLRETURN EsSQLAllocHandle(SQLSMALLINT HandleType,
	SQLHANDLE InputHandle, _Out_ SQLHANDLE *OutputHandle);
SQLRETURN EsSQLFreeHandle(SQLSMALLINT HandleType, SQLHANDLE Handle);
SQLRETURN EsSQLFreeStmt(SQLHSTMT StatementHandle, SQLUSMALLINT Option);

SQLRETURN EsSQLSetEnvAttr(SQLHENV EnvironmentHandle,
	SQLINTEGER Attribute,
	_In_reads_bytes_opt_(StringLength) SQLPOINTER Value,
	SQLINTEGER StringLength);
SQLRETURN EsSQLGetEnvAttr(SQLHENV EnvironmentHandle, SQLINTEGER Attribute,
	_Out_writes_(_Inexpressible_(BufferLength)) SQLPOINTER Value,
	SQLINTEGER BufferLength, _Out_opt_ SQLINTEGER *StringLength);


SQLRETURN EsSQLSetStmtAttrW(
	SQLHSTMT           StatementHandle,
	SQLINTEGER         Attribute,
	SQLPOINTER         ValuePtr,
	SQLINTEGER         BufferLength);
SQLRETURN EsSQLGetStmtAttrW(
	SQLHSTMT     StatementHandle,
	SQLINTEGER   Attribute,
	SQLPOINTER   ValuePtr,
	SQLINTEGER   BufferLength,
	SQLINTEGER  *StringLengthPtr);

SQLRETURN EsSQLGetDescFieldW(
	SQLHDESC        DescriptorHandle,
	SQLSMALLINT     RecNumber,
	SQLSMALLINT     FieldIdentifier,
	_Out_writes_opt_(_Inexpressible_(BufferLength))
	SQLPOINTER      ValuePtr,
	SQLINTEGER      BufferLength,
	SQLINTEGER      *StringLengthPtr);
SQLRETURN EsSQLGetDescRecW(
	SQLHDESC        DescriptorHandle,
	SQLSMALLINT     RecNumber,
	_Out_writes_opt_(BufferLength)
	SQLWCHAR        *Name,
	_Out_opt_
	SQLSMALLINT     BufferLength,
	_Out_opt_
	SQLSMALLINT     *StringLengthPtr,
	_Out_opt_
	SQLSMALLINT     *TypePtr,
	_Out_opt_
	SQLSMALLINT     *SubTypePtr,
	_Out_opt_
	SQLLEN          *LengthPtr,
	_Out_opt_
	SQLSMALLINT     *PrecisionPtr,
	_Out_opt_
	SQLSMALLINT     *ScalePtr,
	_Out_opt_
	SQLSMALLINT     *NullablePtr);

/* use with RecNumber for header fields */
#define NO_REC_NR	-1

SQLRETURN EsSQLSetDescFieldW(
	SQLHDESC        DescriptorHandle,
	SQLSMALLINT     RecNumber,
	SQLSMALLINT     FieldIdentifier,
	SQLPOINTER      Value,
	SQLINTEGER      BufferLength);
SQLRETURN EsSQLSetDescRec(
	SQLHDESC DescriptorHandle,
	SQLSMALLINT RecNumber,
	SQLSMALLINT Type,
	SQLSMALLINT SubType,
	SQLLEN Length,
	SQLSMALLINT Precision,
	SQLSMALLINT Scale,
	_Inout_updates_bytes_opt_(Length) SQLPOINTER Data,
	_Inout_opt_ SQLLEN *StringLength,
	_Inout_opt_ SQLLEN *Indicator);


/*
 * Macros to convert ODBC API generic handles into implementation types.
 */
#define ENVH(_h)	((esodbc_env_st *)(_h))
#define DBCH(_h)	((esodbc_dbc_st *)(_h))
#define STMH(_h)	((esodbc_stmt_st *)(_h))
#define DSCH(_h)	((esodbc_desc_st *)(_h))
/* this will only work if member stays first in handles (see struct decl). */
#define HDRH(_h)	((esodbc_hhdr_st *)(_h))

/*
 * Locking macros for ODBC API calls.
 */
#define HND_LOCK(_h)	ESODBC_MUX_LOCK(&HDRH(_h)->mutex)
#define HND_TRYLOCK(_h)	ESODBC_MUX_TRYLOCK(&HDRH(_h)->mutex)
#define HND_UNLOCK(_h)	ESODBC_MUX_UNLOCK(&HDRH(_h)->mutex)


/* post state into the diagnostic and return state's return code */
#define RET_HDIAG(_hp/*handle ptr*/, _s/*tate*/, _t/*char text*/, _c/*ode*/) \
	return post_diagnostic(_hp, _s, MK_WPTR(_t), _c)
/* similar to RET_HDIAG, but only post the state */
#define RET_HDIAGS(_hp/*handle ptr*/, _s/*tate*/) \
	return post_diagnostic(_hp, _s, NULL, 0)
/* copy the diagnostics from one handle to the other */
#define HDIAG_COPY(_s, _d)	(_d)->hdr.diag = (_s)->hdr.diag
/* set a diagnostic to a(ny) handle */
#define SET_HDIAG(_hp/*handle ptr*/, _s/*tate*/, _t/*char text*/, _c/*ode*/) \
	post_diagnostic(_hp, _s, MK_WPTR(_t), _c)
/* reset handle diagnostic state */
#define RESET_HDIAG(_hp/*handle ptr*/) \
	init_diagnostic(&HDRH(dbc)->diag)

/* return the code associated with the given state (and debug-log) */
#define RET_STATE(_s)	\
	do { \
		assert(_s < SQL_STATE_MAX); \
		return esodbc_errors[_s].retcode; \
	} while (0)

#define STMT_HAS_RESULTSET(stmt)	((stmt)->rset.body.str != NULL)
#define STMT_FORCE_NODATA(stmt)		(stmt)->rset.body.cnt = (size_t)-1
#define STMT_NODATA_FORCED(stmt)	((stmt)->rset.body.cnt == (size_t)-1)
/* "An application can unbind the data buffer for a column but still have a
 * length/indicator buffer bound for the column" */
#define REC_IS_BOUND(rec)			( \
	(rec)->data_ptr != NULL || \
	(rec)->indicator_ptr != NULL || \
	(rec)->octet_length_ptr != NULL)

/*
 * Logging with handle
 */

#define LOGH(hnd, lvl, werrn, fmt, ...) \
	_LOG(HDRH(hnd)->log, lvl, werrn, "[" LWPDL "@0x%p] " fmt, \
		LWSTR(&HDRH(hnd)->typew), hnd, __VA_ARGS__)

#define ERRNH(hnd, fmt, ...)	LOGH(hnd, LOG_LEVEL_ERR, 1, fmt, __VA_ARGS__)
#define ERRH(hnd, fmt, ...)		LOGH(hnd, LOG_LEVEL_ERR, 0, fmt, __VA_ARGS__)
#define WARNH(hnd, fmt, ...)	LOGH(hnd, LOG_LEVEL_WARN, 0, fmt, __VA_ARGS__)
#define INFOH(hnd, fmt, ...)	LOGH(hnd, LOG_LEVEL_INFO, 0, fmt, __VA_ARGS__)
#define DBGH(hnd, fmt, ...)		LOGH(hnd, LOG_LEVEL_DBG, 0, fmt, __VA_ARGS__)

#define BUGH(hnd, fmt, ...) \
	do { \
		ERRH(hnd, "[BUG] " fmt, __VA_ARGS__); \
		assert(0); \
	} while (0)



#endif /* __HANDLES_H__ */

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
