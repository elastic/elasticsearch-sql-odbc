/*
 * ELASTICSEARCH CONFIDENTIAL
 * __________________
 *
 *  [2018] Elasticsearch Incorporated. All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of Elasticsearch Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Elasticsearch Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Elasticsearch Incorporated.
 */

#ifndef __HANDLES_H__
#define __HANDLES_H__

#include  <curl/curl.h>

#include "ujdecode.h"

#include "error.h"
#include "log.h"

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
	SQLUINTEGER version; /* versions defined as UL (see SQL_OV_ODBC3) */
	/* diagnostic/state keeping */
	esodbc_diag_st diag;
	// TODO: connections
} esodbc_env_st;

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
	esodbc_env_st *env;
	/* diagnostic/state keeping */
	esodbc_diag_st diag;
	SQLUINTEGER timeout;
	struct {
		size_t max; /* max fetch size */
		char *str; /* as string */
		char slen; /* string's length (w/o terminator) */
	} fetch;

	// FIXME: placeholder; used if connection has been established or not
	// TODO: PROTO
	void *conn;

	SQLTCHAR *connstr; /* connection string */ // TODO: IDNA?
	CURL *curl; /* cURL handle */
	char *wbuf; /* write buffer for the answer */
	size_t wlen; /* size of wbuf */
	size_t wpos; /* current write position in the wbuf */
	size_t wmax; /* maximum lenght (bytes) that wbuf can grow to */

	/* "the catalog is a database", "For a single-tier driver, the catalog
	 * might be a directory" */
	SQLTCHAR *catalog; 
	// TODO: statements?
	
	/* options */
	SQLULEN metadata_id; // default: SQL_FALSE
	SQLULEN async_enable; // default: SQL_ASYNC_ENABLE_OFF
} esodbc_dbc_st;

/* forward declarations */
struct struct_desc;
struct struct_stmt;

typedef struct desc_rec {
	/* back ref to owning descriptor */
	struct struct_desc *desc;

	/* record fields */
	SQLSMALLINT		concise_type;
	SQLSMALLINT		type; /* SQL_C_<type> -> AxD, SQL_<type> -> IxD */
	SQLSMALLINT		datetime_interval_code;

	SQLPOINTER		data_ptr; /* array, if .array_size > 1 */

	SQLTCHAR		*base_column_name; /* read-only */
	SQLTCHAR		*base_table_name; /* r/o */
	SQLTCHAR		*catalog_name; /* r/o */
	SQLTCHAR		*label; /* r/o */
	SQLTCHAR		*literal_prefix; /* r/o */
	SQLTCHAR		*literal_suffix; /* r/o */
	SQLTCHAR		*local_type_name; /* r/o */
	SQLTCHAR		*name;
	SQLTCHAR		*schema_name; /* r/o */
	SQLTCHAR		*table_name; /* r/o */
	SQLTCHAR		*type_name; /* r/o */

	SQLLEN			*indicator_ptr; /* array, if .array_size > 1 */
	SQLLEN			*octet_length_ptr; /* array, if .array_size > 1 */

	SQLLEN			display_size;
	SQLLEN			octet_length;

	SQLULEN			length;

	SQLINTEGER		auto_unique_value;
	SQLINTEGER		case_sensitive;
	SQLINTEGER		datetime_interval_precision;
	SQLINTEGER		num_prec_radix;

	SQLSMALLINT		fixed_prec_scale;
	SQLSMALLINT		nullable; /* r/o */
	SQLSMALLINT		parameter_type;
	SQLSMALLINT		precision;
	SQLSMALLINT		rowver;
	SQLSMALLINT		scale;
	SQLSMALLINT		searchable;
	SQLSMALLINT		unnamed;
	SQLSMALLINT		usigned;
	SQLSMALLINT		updatable;
	/* /record fields */
} desc_rec_st;


typedef enum {
	DESC_TYPE_ANON, /* SQLAllocHandle()'ed */
	DESC_TYPE_ARD,
	DESC_TYPE_IRD,
	DESC_TYPE_APD,
	DESC_TYPE_IPD,
} desc_type_et;

typedef struct struct_desc {
	/* back ref to owning statement */
	struct struct_stmt *stmt;

	esodbc_diag_st diag;
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
	desc_rec_st *recs;
} esodbc_desc_st;


typedef struct struct_resultset {
	char *buff; /* buffer containing the answer to the last request in a STM */
	size_t blen; /* lenght of the answer */

	void *state; /* top UJSON decoder state */
	void *rows_iter; /* UJSON array with the result set */

	size_t nrows; /* (count of) number of rows in current result set */
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
	esodbc_dbc_st *dbc;
	esodbc_diag_st diag;

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
} esodbc_stmt_st;


/* leave the timeout to default value (0: don't timeout, pos: seconds) */
#define ESODBC_TIMEOUT_DEFAULT		-1
// FIXME: review@alpha
/* TODO: should there be a max? */
#define ESODBC_MAX_ROW_ARRAY_SIZE	128
#define ESODBC_DEF_ARRAY_SIZE		1
/* max cols or args to bind */
#define ESODBC_MAX_DESC_COUNT		128
/* values for SQL_ATTR_MAX_LENGTH statement attribute */
#define ESODBC_UP_MAX_LENGTH		0 // USHORT_MAX
#define ESODBC_LO_MAX_LENGTH		0
/* max number of rows to request from server */
#define ESODBC_DEF_FETCH_SIZE		0 // no fetch size
/* prepare a STMT for a new SQL operation.
 * To be used with catalog functions, that can be all called with same stmt */
#define ESODBC_SQL_CLOSE			((SQLUSMALLINT)-1)


SQLRETURN update_rec_count(esodbc_desc_st *desc, SQLSMALLINT new_count);
desc_rec_st* get_record(esodbc_desc_st *desc, SQLSMALLINT rec_no, BOOL grow);


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

/* wraper of RET_CDIAG, compatible with any defined handle */
#define RET_HDIAG(_hp/*handle ptr*/, _s/*tate*/, _t/*char text*/, _c/*ode*/) \
	RET_CDIAG(&(_hp)->diag, _s, _t, _c)
/* similar to RET_HDIAG, but only post the state */
#define RET_HDIAGS(_hp/*handle ptr*/, _s/*tate*/) \
	RET_DIAG(&(_hp)->diag, _s, NULL, 0)
/* copy the diagnostics from one handler to the other */
#define HDIAG_COPY(_s, _d)	(_s)->diag = (_d)->diag
/* set a diagnostic to a(ny) handle */
#define SET_HDIAG(_hp/*handle ptr*/, _s/*tate*/, _t/*char text*/, _c/*ode*/) \
	post_diagnostic(&(_hp)->diag, _s, MK_TSTR(_t), _c)

/* return the code associated with the given state (and debug-log) */
#define RET_STATE(_s)	\
	do { \
		SQLRETURN _r = esodbc_errors[_s].retcode; \
		SQLTCHAR *_c = esodbc_errors[_s].code; \
		DBG("returning state "LTPD", code %d.", _c, _r); \
		return _r; \
	} while (0)

#define RET_NOT_IMPLEMENTED	\
	do { \
		ERR("not implemented.");\
		return SQL_ERROR; \
	} while (0)

#define STMT_HAS_RESULTSET(stmt)	((stmt)->rset.buff != NULL)
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
