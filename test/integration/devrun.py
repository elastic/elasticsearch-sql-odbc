#!/usr/bin/python3
#
# Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
# or more contributor license agreements. Licensed under the Elastic License;
# you may not use this file except in compliance with the Elastic License.
#

import pyodbc
import base64
import time
import re
import decimal

#conn_str = 'DSN=::1-7;'
#conn_str = 'DSN=10.0.2.2-32;'
#conn_str = 'DSN=cloud.dev;'
#conn_str = 'DSN=cloud.dev.elastic;'
#conn_str = 'DSN=pime;'
conn_str = 'DSN=10.0.2.2-dev-msi-sys;Compression=0' # x32
#conn_str = 'DSN=10.0.2.2-dev-install-sys' # x32
#conn_str = 'DSN=10.0.2.2-dev-sys' # x64
#conn_str = 'Driver=EOD7;'
#conn_str = 'Driver={Elasticsearch Driver};'
#conn_str = 'Driver={Elasticsearch ODBC};'

#conn_str += 'UID=elastic;PWD=elastic;Secure=0;'
#conn_str += 'MaxFetchSize=1;MaxBodySizeMB=100;'
#conn_str += 'ApplyTZ=1;'
#conn_str += 'ScientitficFloats=0;'
#conn_str += 'Packing=CBOR;'
#conn_str += 'Packing=JSON;'

#conn_str += 'UID=odbc;PWD=elastic;'
#cloud_id = "7879474408f44e9ca98d1597e659dfe2.eu-central-1.aws.cloud.es.io:9243"
#cloud_id = "eu-central-1.aws.cloud.es.io:9243$7879474408f44e9ca98d1597e659dfe2"
#cloud_id = "eu-central-1.aws.cloud.es.io:9243$7879474408f44e9ca98d1597e659dfe2$foobar$barbar$"
#cloud_id = base64.b64encode(bytes(cloud_id, "utf8")).decode("utf8")
#cloud_id = ":%s" % cloud_id
#conn_str += 'CloudID={%s}' % cloud_id

print("Connection String: %s" % conn_str)

def count_locally_table(conn, table_name):
	with conn.execute("select * from %s" % table_name) as curs:
		x = curs.fetchall()
		print(len(x))

def exec_stmt(conn, stmt, echo=True, params=[]):
	started_at = time.time()
	with conn.execute(stmt, *params) as curs:
		x = curs.fetchall()
		if echo:
			print(x)
	print("Statement `%s`\n -- records: %s, took: %ss." % (re.sub("\s+", " ", stmt), len(x), time.time() - started_at))

def exec_interval_stmt(conn, stmt):
	import struct
	import ctypes
	wchar_sz = ctypes.sizeof(ctypes.c_wchar)
	if wchar_sz == ctypes.sizeof(ctypes.c_ushort):
		unit = "H"
	elif wchar_sz == ctypes.sizeof(ctypes.c_uint32):
		unit = "I"
	else:
		raise Exception("unsupported wchar_t size")

	def convert(value):
		cnt = len(value)
		assert(cnt % wchar_sz == 0)
		cnt //= wchar_sz
		fmt = "=" + str(cnt) + unit
		ret = ""
		for c in struct.unpack(fmt, value):
			ret += chr(c)
		return ret

	for x in range(101, 114):
		conn.add_output_converter(x, convert)
	ret = exec_stmt(conn, stmt)
	conn.clear_output_converters()
	return ret

def group0(conn):
	print("Current user: `%s`." % conn.getinfo(pyodbc.SQL_USER_NAME))
	print("Current catalog: `%s`." % conn.getinfo(pyodbc.SQL_DATABASE_NAME))
	exec_stmt(conn, "SELECT CURRENT_TIMESTAMP() as today")
	#exec_stmt(conn, "select 1 from library")
	#exec_stmt(conn, "select * from kibana_sample_data_logs limit 10000", False)
	#exec_stmt(conn, "select * from batters", False)
	#exec_stmt(conn, "select 1, pg_typeof(pay_by_quarter), pay_by_quarter[1], pay_by_quarter from sal_emp limit 1")
	#exec_stmt(conn, """
	#		SELECT referer FROM "kibana_sample_data_logs" WHERE referer like '%mark-kelly%' group by referer""",
	#		False)
	#exec_stmt(conn, "select min(birth_date), max(birth_date) from employees") #FIXME TODO
	exec_stmt(conn, "select * from arrays limit 3")

def group1(conn):
	exec_stmt(conn, "select cast('0.98765432100123456789' as double)")
	exec_stmt(conn, "select convert(now(), time)")
	exec_stmt(conn, "SELECT CAST(-1234567890123.34 AS FLOAT)")
	exec_stmt(conn, "SELECT CAST(INTERVAL '163:59.163' MINUTE TO SECOND AS STRING)")
	exec_stmt(conn, "SELECT CAST('2001:0db8:0000:0000:0000:ff00:0042:8329' AS IP)")

	exec_interval_stmt(conn, "SELECT INTERVAL '326' YEAR")
	exec_interval_stmt(conn, "SELECT INTERVAL '163:59.163' MINUTE TO SECOND")
	exec_interval_stmt(conn, "SELECT INTERVAL '1 1:1' day TO minute")
	exec_interval_stmt(conn, "SELECT INTERVAL '163 12:39:59.163' DAY TO SECOND")
	exec_interval_stmt(conn, "SELECT INTERVAL -'163 23:39:56.23' DAY TO SECOND")
	exec_interval_stmt(conn, "SELECT INTERVAL '163 12' DAY TO HOUR")
	exec_interval_stmt(conn, "SELECT INTERVAL '520' DAY")
	exec_interval_stmt(conn, "SELECT INTERVAL '163' MINUTE")

	curs = conn.cursor()
	#print(dir(curs))
	# tables(table=None, catalog=None, schema=None, tableType=None)
	#print(curs.tables("", "%", "", "").fetchall())
	#print(curs.tables(catalog="%", tableType="TABLE,VIEW").fetchall())
	#print(curs.tables("", "", "", "%").fetchall())
	#print(curs.columns(table="batters").fetchone())
	##print(curs.columns(table="batters", catalog="distribution_run").fetchone()) # returns None/valid value randomly

# fields only --(gradually)--> fn.s only
def group2(conn):
	limit = 90000
	#limit = 1000
	#limit = 30000
	limit = 120000
	limit = 200000

	grp_by = "GROUP BY 1, 2, 3, 4, 5"
	#grp_by = ""

	exec_stmt(conn, \
	"""
	SELECT day AS "dy_@timestamp_ok",
	  "fgarmin"."gpn" AS "gpn",
	  month AS "mn_@timestamp_ok",
	  quarter AS "qr_@timestamp_ok",
	  year AS "yr_@timestamp_ok"
	FROM "fgarmin"
	%s
	LIMIT %s
	""" % (grp_by, limit))

	exec_stmt(conn, \
	"""
	SELECT {fn CONVERT({fn TRUNCATE({fn EXTRACT(DAY FROM "fgarmin"."@timestamp")},0)}, SQL_BIGINT)} AS "dy_@timestamp_ok",
	  "fgarmin"."gpn" AS "gpn",
	  month AS "mn_@timestamp_ok",
	  quarter AS "qr_@timestamp_ok",
	  year AS "yr_@timestamp_ok"
	FROM "fgarmin"
	%s
	LIMIT %s
	""" % (grp_by, limit))

	exec_stmt(conn, \
	"""
	SELECT {fn CONVERT({fn TRUNCATE({fn EXTRACT(DAY FROM "fgarmin"."@timestamp")},0)}, SQL_BIGINT)} AS "dy_@timestamp_ok",
	  "fgarmin"."gpn" AS "gpn",
	  {fn CONVERT({fn TRUNCATE({fn EXTRACT(MONTH FROM "fgarmin"."@timestamp")},0)}, SQL_BIGINT)} AS "mn_@timestamp_ok",
	  quarter AS "qr_@timestamp_ok",
	  year AS "yr_@timestamp_ok"
	FROM "fgarmin"
	%s
	LIMIT %s
	""" % (grp_by, limit))

	exec_stmt(conn, \
	"""
	SELECT {fn CONVERT({fn TRUNCATE({fn EXTRACT(DAY FROM "fgarmin"."@timestamp")},0)}, SQL_BIGINT)} AS "dy_@timestamp_ok",
	  "fgarmin"."gpn" AS "gpn",
	  {fn CONVERT({fn TRUNCATE({fn EXTRACT(MONTH FROM "fgarmin"."@timestamp")},0)}, SQL_BIGINT)} AS "mn_@timestamp_ok",
	  {fn QUARTER("fgarmin"."@timestamp")} AS "qr_@timestamp_ok",
	  year AS "yr_@timestamp_ok"
	FROM "fgarmin"
	%s
	LIMIT %s
	""" % (grp_by, limit))

	exec_stmt(conn, \
	"""
	SELECT {fn CONVERT({fn TRUNCATE({fn EXTRACT(DAY FROM "fgarmin"."@timestamp")},0)}, SQL_BIGINT)} AS "dy_@timestamp_ok",
	  "fgarmin"."gpn" AS "gpn",
	  {fn CONVERT({fn TRUNCATE({fn EXTRACT(MONTH FROM "fgarmin"."@timestamp")},0)}, SQL_BIGINT)} AS "mn_@timestamp_ok",
	  {fn QUARTER("fgarmin"."@timestamp")} AS "qr_@timestamp_ok",
	  {fn CONVERT({fn TRUNCATE({fn EXTRACT(YEAR FROM "fgarmin"."@timestamp")},0)}, SQL_BIGINT)} AS "yr_@timestamp_ok"
	FROM "fgarmin"
	%s
	LIMIT %s
	""" % (grp_by, limit))

# GPN + increasingly imbricated fn.s
def group3(conn):
	limit = 90000
	#limit = 1000
	#limit = 30000
	limit = 120000
	limit = 200000

	grp_by = "GROUP BY 1, 2"
	#grp_by = ""

	exec_stmt(conn, \
	"""
	SELECT day AS "dy_@timestamp_ok",
	  "fgarmin"."gpn" AS "gpn"
	FROM "fgarmin"
	%s
	LIMIT %s
	""" % (grp_by, limit))

	exec_stmt(conn, \
	"""
	SELECT {fn CONVERT({fn TRUNCATE({fn EXTRACT(DAY FROM "fgarmin"."@timestamp")},0)}, SQL_BIGINT)} AS "dy_@timestamp_ok",
	  "fgarmin"."gpn" AS "gpn"
	FROM "fgarmin"
	%s
	LIMIT %s
	""" % (grp_by, limit))

	exec_stmt(conn, \
	"""
	SELECT {fn TRUNCATE({fn EXTRACT(DAY FROM "fgarmin"."@timestamp")},0)} AS "dy_@timestamp_ok",
	  "fgarmin"."gpn" AS "gpn"
	FROM "fgarmin"
	%s
	LIMIT %s
	""" % (grp_by, limit))

	exec_stmt(conn, \
	"""
	SELECT {fn EXTRACT(DAY FROM "fgarmin"."@timestamp")} AS "dy_@timestamp_ok",
	  "fgarmin"."gpn" AS "gpn"
	FROM "fgarmin"
	%s
	LIMIT %s
	""" % (grp_by, limit))

# SQL fn.s +: 1)GPN vs. 2)FLOOR
def group4(conn):
	limit = 200000

	grp_by = "GROUP BY 1, 2"
	#grp_by = ""

	exec_stmt(conn, \
	"""
	SELECT {fn CONVERT({fn TRUNCATE({fn EXTRACT(DAY FROM "fgarmin"."@timestamp")},0)}, SQL_BIGINT)} AS "dy_@timestamp_ok",
	  "fgarmin"."gpn" AS "gpn"
	FROM "fgarmin"
	%s
	LIMIT %s
	""" % (grp_by, limit))

	exec_stmt(conn, \
	"""
	SELECT {fn CONVERT({fn TRUNCATE({fn EXTRACT(DAY FROM "fgarmin"."@timestamp")},0)}, SQL_BIGINT)} AS "dy_@timestamp_ok",
	  "fgarmin"."floor" AS "floor"
	FROM "fgarmin"
	%s
	LIMIT %s
	""" % (grp_by, limit))

# SQL fn.s only
def group5(conn):
	limit = 200000

	exec_stmt(conn, \
	"""
	SELECT {fn CONVERT({fn TRUNCATE({fn EXTRACT(DAY FROM "fgarmin"."@timestamp")},0)}, SQL_BIGINT)} AS "dy_@timestamp_ok",
	{fn CONVERT({fn TRUNCATE({fn EXTRACT(MONTH FROM "fgarmin"."@timestamp")},0)}, SQL_BIGINT)} AS "mn_@timestamp_ok",
	{fn QUARTER("fgarmin"."@timestamp")} AS "qr_@timestamp_ok",
	{fn CONVERT({fn TRUNCATE({fn EXTRACT(YEAR FROM "fgarmin"."@timestamp")},0)}, SQL_BIGINT)} AS "yr_@timestamp_ok"
	FROM "fgarmin"
	GROUP BY 1, 2, 3, 4
	LIMIT %s
	""" % (limit))

# GPN +: 1) field vs. 2) fn
def group6(conn):
	limit = 400000

	grp_by = "GROUP BY 1, 2"
	#grp_by = ""

	exec_stmt(conn, \
	"""
	SELECT day AS "dy_@timestamp_ok",
	  "fgarmin"."gpn" AS "gpn"
	FROM "fgarmin"
	%s
	LIMIT %s
	""" % (grp_by, limit))

	exec_stmt(conn, \
	"""
	SELECT {fn EXTRACT(DAY FROM "fgarmin"."@timestamp")} AS "dy_@timestamp_ok",
	  "fgarmin"."gpn" AS "gpn"
	FROM "fgarmin"
	%s
	LIMIT %s
	""" % (grp_by, limit))

def group7(conn):
	exec_stmt(conn, "select * from staples", False)

def group8(conn):
	#c = conn.execute("select * from employees where first_name = ?", "Bojan")
	#c = conn.execute("select * from employees where first_name = ?", None)
	#print("RES: %s", c.fetchall())
	c = conn.execute("select * from employees where emp_no = ? or first_name = ? or first_name = ?", 10001, "Bojan",
			None)
	print("RES: %s", c.fetchall())

def group9(conn):
	exec_stmt(conn, "SELECT '%s'" % ("*" * 4000), False)

preconnect={109:"foobar"}
with pyodbc.connect(conn_str, attrs_before=preconnect) as conn:
	conn.autocommit = True

	#group0(conn)
	#group1(conn)
	#group2(conn)
	#group3(conn)
	#group4(conn)
	#group5(conn)
	#group6(conn)
	#group7(conn)
	#group8(conn)
	#exec_stmt(conn, "SELECT CAST(1234.34 AS REAL)")
	#exec_stmt(conn, "SELECT CAST(1234.34 AS DOUBLE)")
	#group9(conn)
	#exec_stmt(conn, "SELECT * FROM cust ORDER BY ulong ASC")
	#exec_stmt(conn, "SELECT * FROM test_emp LIMIT 2")
	#exec_stmt(conn, "SELECT * FROM test_logs_ul LIMIT 20")
	exec_stmt(conn, "SELECT 18446744073709551615 WHERE 18446744073709551615 = ?", True,
			[decimal.Decimal(18446744073709551615)])

#if __name__== "__main__":
#	main()

# vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=118 :
