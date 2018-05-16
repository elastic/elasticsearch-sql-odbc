/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

#ifndef __HANDLES_H__
#define __HANDLES_H__

#include  <curl/curl.h>

#include "ujdecode.h"

#include "error.h"
#include "defs.h"

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
	/* back reference to "parent" structure (in type hierarchy) */
	union {
		struct struct_env *env;
		struct struct_dbc *dbc;
		struct struct_stmt *stmt;
		void *parent;
	};
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
	// TODO?: connections
} esodbc_env_st;

/* meta data types (same for both SQL_C_<t> and SQL_<t> types) */
typedef enum {
	METATYPE_UNKNOWN = 0,
	METATYPE_EXACT_NUMERIC,
	METATYPE_FLOAT_NUMERIC,
	METATYPE_STRING,
	METATYPE_BIN,
	METATYPE_DATETIME,
	METATYPE_INTERVAL_WSEC,
	METATYPE_INTERVAL_WOSEC,
	METATYPE_BIT,
	METATYPE_UID,
	METATYPE_MAX // SQL_C_DEFAULT
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
#define ESODBC_TYPES_MEMBERS	19

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

	wstr_st dsn; /* data source name */
	char *url;
	SQLUINTEGER timeout;
	BOOL follow;
	struct {
		size_t max; /* max fetch size */
		char *str; /* as string */
		char slen; /* string's length (w/o terminator) */
	} fetch;
	BOOL pack_json; /* should JSON be used in REST bodies? (vs. CBOR) *///TODO

	esodbc_estype_st *es_types; /* array with ES types */
	SQLULEN no_types; /* number of types in array */

	CURL *curl; /* cURL handle */
	char *abuff; /* buffer holding the answer */
	size_t alen; /* size of abuff */
	size_t apos; /* current write position in the abuff */
	size_t amax; /* maximum length (bytes) that abuff can grow to */

	/* window handler */
	HWND hwin;
	/* "the catalog is a database", "For a single-tier driver, the catalog
	 * might be a directory" */
	SQLWCHAR *catalog; 
	// TODO: statement list?

	/* options */
	SQLULEN metadata_id; // default: SQL_FALSE
	SQLULEN async_enable; // default: SQL_ASYNC_ENABLE_OFF
	SQLUINTEGER txn_isolation; // default: SQL_TXN_*
} esodbc_dbc_st;

typedef struct desc_rec {
	/* back ref to owning descriptor */
	struct struct_desc	*desc;

	/* helper member, to characterize the type */
	esodbc_metatype_et	meta_type;

	/* pointer to the ES/SQL type in DBC array
	 * need to be set for records in IxD descriptors */
	esodbc_estype_st	*es_type;

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

	/* TODO: move all SQLWCHARs to wstr_st */
	SQLWCHAR		*base_column_name; /* read-only */
	SQLWCHAR		*base_table_name; /* r/o */
	SQLWCHAR		*catalog_name; /* r/o */
	SQLWCHAR		*label; /* r/o */ //alias?
	SQLWCHAR		*name;
	SQLWCHAR		*schema_name; /* r/o */
	SQLWCHAR		*table_name; /* r/o */

	SQLLEN			*indicator_ptr; /* array, if .array_size > 1 */
	SQLLEN			*octet_length_ptr; /* array, if .array_size > 1 */

	SQLLEN			octet_length;
	SQLULEN			length;

	SQLINTEGER		datetime_interval_precision; /*TODO: -> es_type? */
	SQLINTEGER		num_prec_radix; /*TODO: -> es_type? */

	SQLSMALLINT		parameter_type;
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

	/* array of records of .count cardinality
	 * TODO: list? binding occurs seldomly, compared to execution, tho. */
	esodbc_rec_st *recs;
} esodbc_desc_st;

/* the ES/SQL type must be set for implementation descriptor records */
#define ASSERT_IXD_HAS_ES_TYPE(_rec) \
	assert(DESC_TYPE_IS_IMPLEMENTATION(_rec->desc->type) && _rec->es_type)


typedef struct struct_resultset {
	long code; /* code of last response */
	char *buff; /* buffer containing the answer to the last request in a STM */
	size_t blen; /* length of the answer */

	void *state; /* top UJSON decoder state */
	void *rows_iter; /* UJSON array with the result set */
	const wchar_t *ecurs; /* Elastic's cursor object */
	size_t eccnt; /* cursor char count */

	size_t nrows; /* (count of) rows in current result set */
	size_t vrows; /* (count of) visited rows in current result set  */
	size_t frows; /* (count of) fetched rows across *entire* result set  */

	 SQLULEN array_pos; /* position in ARD array to write_in/resume_at */
} resultset_st;

/*
 * "The fields of an IRD have a default value only after the statement has
 * been prepared or executed and the IRD has been populated, not when the
 * statement handle or descriptor has been allocated. Until the IRD has been
 * populated, any attempt to gain access to a field of an IRD will return an
 * error."
 */

typedef struct struct_stmt {
	esodbc_hhdr_st hdr;

	/* cache SQL, can be used with varying params */
	char *u8sql; /* UTF8 JSON serialized buffer */
	size_t sqllen; /* byte len of SQL statement */

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
	SQLULEN async_enable; // default: copied from connection
	/* "the maximum amount of data that the driver returns from a character or
	 * binary column" */
	SQLULEN max_length; 

	/* result set */
	resultset_st rset;
	/* SQL data types conversion to SQL C compatibility (IRD.SQL -> ARD.C) */
	enum {
		CONVERSION_VIOLATION = -1,
		CONVERSION_UNCHECKED, /* 0 */
		CONVERSION_SUPPORTED,
		CONVERSION_SKIPPED, /* used with driver's meta queries */
	} sql2c_conversion;
} esodbc_stmt_st;



SQLRETURN update_rec_count(esodbc_desc_st *desc, SQLSMALLINT new_count);
esodbc_rec_st* get_record(esodbc_desc_st *desc, SQLSMALLINT rec_no, BOOL grow);
void dump_record(esodbc_rec_st *rec);

/* TODO: move to some utils.h */
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
SQLRETURN SQL_API EsSQLGetEnvAttr(SQLHENV EnvironmentHandle,
		SQLINTEGER Attribute, 
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


#define ENVH(_h)	((esodbc_env_st *)(_h))
#define DBCH(_h)	((esodbc_dbc_st *)(_h))
#define STMH(_h)	((esodbc_stmt_st *)(_h))
#define DSCH(_h)	((esodbc_desc_st *)(_h))
/* this will only work if member stays first in handles (see struct decl). */
#define HDRH(_h)	((esodbc_hhdr_st *)(_h))


/* wraper of RET_CDIAG, compatible with any defined handle */
#define RET_HDIAG(_hp/*handle ptr*/, _s/*tate*/, _t/*char text*/, _c/*ode*/) \
	RET_CDIAG(&(_hp)->hdr.diag, _s, _t, _c)
/* similar to RET_HDIAG, but only post the state */
#define RET_HDIAGS(_hp/*handle ptr*/, _s/*tate*/) \
	RET_DIAG(&(_hp)->hdr.diag, _s, NULL, 0)
/* copy the diagnostics from one handle to the other */
#define HDIAG_COPY(_s, _d)	(_s)->hdr.diag = (_d)->hdr.diag
/* set a diagnostic to a(ny) handle */
#define SET_HDIAG(_hp/*handle ptr*/, _s/*tate*/, _t/*char text*/, _c/*ode*/) \
	post_diagnostic(&(_hp)->hdr.diag, _s, MK_WPTR(_t), _c)

/* return the code associated with the given state (and debug-log) */
#define RET_STATE(_s)	\
	do { \
		assert(_s < SQL_STATE_MAX); \
		return esodbc_errors[_s].retcode; \
	} while (0)

#define STMT_HAS_RESULTSET(stmt)	((stmt)->rset.buff != NULL)
#define STMT_FORCE_NODATA(stmt)		(stmt)->rset.blen = (size_t)-1
#define STMT_NODATA_FORCED(stmt)	((stmt)->rset.blen == (size_t)-1)
/* "An application can unbind the data buffer for a column but still have a
 * length/indicator buffer bound for the column" */
#define REC_IS_BOUND(rec)			( \
		(rec)->data_ptr != NULL || \
		(rec)->indicator_ptr != NULL || \
		(rec)->octet_length_ptr != NULL)


/* TODO: this is inefficient: add directly into ujson4c lib (as .size of
 * ArrayItem struct, inc'd in arrayAddItem()) or local utils file. Only added
 * here to be accessible with the statement resultset member. */
static inline size_t UJArraySize(UJObject obj)
{
	UJObject _u; /* unused */
	size_t size = 0;
	void *iter = UJBeginArray(obj);
	if (iter) {
		while (UJIterArray(&iter, &_u))
			size ++;
	}
	return size;
}

#endif /* __HANDLES_H__ */

/* vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 : */
