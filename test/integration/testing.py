#
# Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
# or more contributor license agreements. Licensed under the Elastic License;
# you may not use this file except in compliance with the Elastic License.
#

import datetime
import hashlib
import unittest
import re
import struct
import ctypes
import urllib3
import decimal

from elasticsearch import Elasticsearch
from data import TestData, BATTERS_TEMPLATE

DRIVER_NAME = "Elasticsearch Driver"

class Testing(unittest.TestCase):

	_uid = None
	_data = None
	_dsn = None
	_dsn_api_key = None
	_pyodbc = None
	_catalog = None

	def __init__(self, es, test_data, catalog, dsn=None):
		super().__init__()
		uid, pwd = es.credentials()
		api_key = es.api_key()
		es_url = urllib3.util.parse_url(es.base_url())

		self._uid = uid
		self._data = test_data
		self._catalog = catalog

		conn_str_no_cred = "Driver={%s};Server=%s;Port=%s;Secure=%s;" % (DRIVER_NAME, es_url.host, es_url.port,
				"1" if es_url.scheme.lower() == "https" else "0")
		conn_str = conn_str_no_cred + "UID=%s;PWD=%s;" % (uid, pwd)
		api_key_avp = "APIKey={%s};" % api_key
		if dsn:
			if "Driver=" not in dsn:
				self._dsn = conn_str + dsn
				self._dsn_api_key = conn_str_no_cred + api_key_avp
			else:
				self._dsn = dsn
				self._dsn_api_key = api_key_avp + dsn
		else:
			self._dsn = conn_str
			self._dsn_api_key = api_key_avp + conn_str_no_cred

		print("Using   DSN: '%s'." % self._dsn)
		print("API-Key-DSN: '%s'." % self._dsn_api_key)

		# only import pyODBC if running tests (vs. for instance only loading test data in ES)
		import pyodbc
		self._pyodbc = pyodbc

	def _reconstitute_csv(self, index_name):
		with self._pyodbc.connect(self._dsn, autocommit=True) as cnxn:
			csv = u""
			cols = self._data.csv_attributes(index_name)[1]
			fields = '"' + "\",\"".join(cols) + '"'
			with cnxn.execute("select %s from %s" % (fields, index_name)) as curs:
				csv += u",".join(cols)
				csv += u"\n"
				for row in curs:
					vals = []
					for val in row:
						if val is None:
							val=""
						elif isinstance(val, datetime.datetime):
							val = val.isoformat() + "Z"
						else:
							val = str(val)
						vals.append(val)
					csv += u",".join(vals)
					csv += u"\n"

			return csv

	def _as_csv(self, index_name):
		print("Reconstituting CSV from index '%s'." % index_name)
		csv = self._reconstitute_csv(index_name)

		md5 = hashlib.md5()
		md5.update(bytes(csv, "utf-8"))
		csv_md5 = self._data.csv_attributes(index_name)[0]
		if md5.hexdigest() != csv_md5:
			print("Reconstituded CSV of index '%s':\n%s" % (index_name, csv))
			raise Exception("reconstituted CSV differs from original for index '%s'" % index_name)

	def _count_all(self, index_name):
		print("Counting records in index '%s.'" % index_name)
		cnt = 0
		with self._pyodbc.connect(self._dsn, autocommit=True) as cnxn:
			with cnxn.execute("select 1 from %s" % index_name) as curs:
				while curs.fetchone():
					cnt += 1
		csv_lines = self._data.csv_attributes(index_name)[2]
		if cnt != csv_lines:
			raise Exception("index '%s' only contains %s documents (vs original %s CSV lines)" %
					(index_name, cnt, csv_lines))

	def _clear_cursor(self, index_name):
		conn_str = self._dsn + ";MaxFetchSize=5"
		with self._pyodbc.connect(conn_str, autocommit=True) as cnxn:
			with cnxn.execute("select 1 from %s limit 10" % index_name) as curs:
				for i in range(3): # must be lower than MaxFetchSize, so no next page be requested
					curs.fetchone()
				# reuse the statment (a URL change occurs)
				with curs.execute("select 1 from %s" % index_name) as curs2:
					curs2.fetchall()
		# no exception raised -> passed

	def _select_columns(self, index_name, columns):
		print("Selecting columns '%s' from index '%s'." % (columns, index_name))
		with self._pyodbc.connect(self._dsn, autocommit=True) as cnxn:
			stmt = "select %s from %s" % (columns, index_name)
			with cnxn.execute(stmt) as curs:
				cnt = 0
				while curs.fetchone():
					cnt += 1 # no exception -> success
				print("Selected %s rows from %s." % (cnt, index_name))

	def _check_info(self, attr, expected):
		with self._pyodbc.connect(self._dsn, autocommit=True) as cnxn:
			value = cnxn.getinfo(attr)
			self.assertEqual(value, expected)

	# tables(table=None, catalog=None, schema=None, tableType=None)
	def _catalog_tables(self, no_table_type_as=""):
		with self._pyodbc.connect(self._dsn, autocommit=True) as cnxn:
			curs = cnxn.cursor()

			# enumerate catalogs
			res = curs.tables("", "%", "", no_table_type_as).fetchall()
			self.assertEqual(len(res), 1)
			for i in range(0,10):
				self.assertEqual(res[0][i], None if i else self._catalog)
			#self.assertEqual(res, [tuple([self._catalog] + [None for i in range(9)])]) # XXX?

			# enumerate table types
			res = curs.tables("", "", "", "%").fetchall()
			self.assertEqual(len(res), 2)
			for i in range(0,10):
				self.assertEqual(res[0][i], None if i != 3 else 'TABLE')
				self.assertEqual(res[1][i], None if i != 3 else 'VIEW')

			# enumerate tables of selected type in catalog
			res = curs.tables(catalog="%", tableType="TABLE,VIEW").fetchall()
			res_tables = [row[2] for row in res]
			have_tables = [getattr(TestData, attr) for attr in dir(TestData)
					if attr.endswith("_INDEX") and type(getattr(TestData, attr)) == str]
			for table in have_tables:
				self.assertTrue(table in res_tables)

	# columns(table=None, catalog=None, schema=None, column=None)
	# use_surrogate: pyodbc seems to not reliably null-terminate the catalog and/or table name string,
	# despite indicating so.
	def _catalog_columns(self, use_catalog=False, use_surrogate=True):
		with self._pyodbc.connect(self._dsn, autocommit=True) as cnxn:
			curs = cnxn.cursor()
			if not use_surrogate:
				res = curs.columns(table=TestData.BATTERS_INDEX, \
						catalog=self._catalog if use_catalog else None).fetchall()
			else:
				if use_catalog:
					stmt = "SYS COLUMNS CATALOG '%s' TABLE LIKE '%s' ESCAPE '\\' LIKE '%%' ESCAPE '\\'" % \
						(self._catalog, TestData.BATTERS_INDEX)
				else:
					stmt = "SYS COLUMNS TABLE LIKE '%s' ESCAPE '\\' LIKE '%%' ESCAPE '\\'" % TestData.BATTERS_INDEX
				res = curs.execute(stmt)
			cols_have = [row[3] for row in res]
			cols_have.sort()
			cols_expect = [k for k in BATTERS_TEMPLATE["mappings"]["properties"].keys()]
			cols_expect.sort()
			self.assertEqual(cols_have, cols_expect)


	# pyodbc doesn't support INTERVAL types; when installing an "output converter", it asks the ODBC driver for the
	# binary format and currently, this is the same as a wchar_t string for INTERVALs.
	# Also, just return None for data type 0 -- NULL
	def _install_output_converters(self, cnxn):
		wchar_sz = ctypes.sizeof(ctypes.c_wchar)
		if wchar_sz == ctypes.sizeof(ctypes.c_ushort):
			unit = "H"
		elif wchar_sz == ctypes.sizeof(ctypes.c_uint32):
			unit = "I"
		else:
			raise Exception("unsupported wchar_t size")

		# wchar_t to python string
		def _convert_interval(value):
			cnt = len(value)
			assert(cnt % wchar_sz == 0)
			cnt //= wchar_sz
			ret = ""
			fmt = "=" + str(cnt) + unit
			for c in struct.unpack(fmt, value):
				ret += chr(c)
			return ret

		for x in range(101, 114): # INTERVAL types IDs
			cnxn.add_output_converter(x, _convert_interval)

		def _convert_null(value):
			return None
		cnxn.add_output_converter(0, _convert_null) # NULL type ID

	# produce an instance of the 'data_type' out of the 'data_val' string
	def _type_to_instance(self, data_type, data_val):
		# Change the value read in the tests to type and format of the result expected to be
		# returned by driver.
		if data_type == "null":
			instance = None
		elif data_type.startswith("bool"):
			instance = data_val.lower() == "true"
		elif data_type in ["byte", "short", "integer"]:
			instance = int(data_val)
		elif data_type == "long":
			instance = int(data_val.strip("lL"))
		elif data_type == "unsigned_long":
			# test uses "UNSIGNED_LONG_MAX", a BigInteger instance
			if data_val == "UNSIGNED_LONG_MAX":
				data_val = 18446744073709551615
			# pyodbc will fail to handle large values unless provided as Decimal
			instance = decimal.Decimal(data_val)
		elif data_type == "double":
			instance = float(data_val)
		elif data_type == "float":
			instance = float(data_val.strip("fF"))
			# reduce precision, py's float is a double
			instance = ctypes.c_float(instance).value
		elif data_type in ["datetime", "date", "time"]:
			fmt = "%H:%M:%S"
			fmt = "%Y-%m-%dT" + fmt
			# no explicit second with microseconds directive??
			if "." in data_val:
				fmt += ".%f"
			# always specify the timezone so that local-to-UTC conversion can take place
			fmt += "%z"
			val = data_val
			if data_type == "time":
				# parse Time as a Datetime, since some tests uses the ES/SQL-specific
				# Time-with-timezone which then needs converting to UTC (as the driver does).
				# and this conversion won't work for strptime()'ed Time values, as this uses
				# year 1900, not UTC convertible.
				val = "1970-02-02T" + val
			# strptime() won't recognize Z as Zulu/UTC
			val = val.replace("Z", "+00:00")
			instance = datetime.datetime.strptime(val, fmt)
			# if local time is provided, change it to UTC (as the driver does)
			try:
				timestamp = instance.timestamp()
				if data_type != "datetime":
					# The microsecond component only makes sense with Timestamp/Datetime with
					# ODBC (the TIME_STRUCT lacks a fractional second field).
					timestamp = int(timestamp)
				instance = instance.utcfromtimestamp(timestamp)
			except OSError:
				# The value can't be UTC converted, since the test uses Datetime years before
				# 1970 => convert it to timestamp w/o timezone.
				instance = datetime.datetime(instance.year, instance.month, instance.day,
						instance.hour, instance.minute, instance.second, instance.microsecond)

			if data_type == "date":
				instance = instance.date()
			elif data_type == "time":
				instance = instance.time()
		else:
			instance = data_val

		return instance

	def _proto_tests(self):
		tests = self._data.proto_tests()
		with self._pyodbc.connect(self._dsn, autocommit=True) as cnxn:
			self._install_output_converters(cnxn)
			try:
				for t in tests:
					(query, col_name, data_type, data_val, cli_val, disp_size) = t
					# print("T: %s, %s, %s, %s, %s, %s" % (query, col_name, data_type, data_val, cli_val, disp_size))

					if data_val != cli_val: # INTERVAL tests
						assert(query.lower().startswith("select interval"))
						# extract the literal value (`INTERVAL -'1 1' -> `-1 1``)
						expect = re.match("[^-]*(-?\s*'[^']*').*", query).groups()[0]
						expect = expect.replace("'", "")
						# filter out tests with fractional seconds:
						# https://github.com/elastic/elasticsearch/issues/41635
						if re.search("\d*\.\d+", expect):
							continue
						# intervals not supported as params; PyODBC has no interval type support
						# https://github.com/elastic/elasticsearch/issues/45915
						params = []
					else: # non-INTERVAL tests
						assert(data_type.lower() == data_type)
						# Change the value read in the tests to type and format of the result expected to be
						# returned by driver.
						expect = self._type_to_instance(data_type, data_val)

						if data_type.lower() == "null":
							query += " WHERE ? IS NULL"
							params = [expect]
						else:
							if data_type.lower() == "time":
								if col_name.find("+") <= 0:
									# ODBC's TIME_STRUCT lacks fractional component -> strip it away
									col_name = re.sub(r"(\d{2})\.\d+", "\\1", col_name)
									query += " WHERE %s = ?" % col_name
									params = [expect]
								else: # it's a time with offset
									# TIE_STRUCT lacks offset component -> perform the simple SELECT
									params = []
							else:
								query += " WHERE %s = ?" % col_name
								params = [expect]
						# print("Query: %s" % query)

					last_ex = None
					with cnxn.execute(query, *params) as curs:
						try:
							self.assertEqual(curs.rowcount, 1)
							res = curs.fetchone()[0]
							if data_type == "float":
								# PyODBC will fetch a REAL/float as a double =>  reduce precision
								res = ctypes.c_float(res).value
							self.assertEqual(res, expect)
						except Exception as e:
							print(e)
							last_ex = e

					if last_ex:
						raise last_ex

			finally:
				cnxn.clear_output_converters()

	def _api_key_test(self):
		with self._pyodbc.connect(self._dsn_api_key, autocommit=True) as cnxn:
			with cnxn.execute("SELECT 1") as curs:
				self.assertEqual(curs.fetchone()[0], 1)


	def perform(self):
		self._check_info(self._pyodbc.SQL_USER_NAME, self._uid)
		self._check_info(self._pyodbc.SQL_DATABASE_NAME, self._catalog)

		self._proto_tests()
		self._api_key_test()

		if self._data.has_csv_attributes():
			# simulate catalog querying as apps do in ES/GH#40775 do
			self._catalog_tables(no_table_type_as = "")
			self._catalog_tables(no_table_type_as = None)
			self._catalog_columns(use_catalog = False, use_surrogate = True)
			self._catalog_columns(use_catalog = True, use_surrogate = True)

			self._as_csv(TestData.LIB_TEST_INDEX)
			self._as_csv(TestData.EMP_TEST_INDEX)
			self._as_csv(TestData.LOGS_UL_TEST_INDEX)

			self._count_all(TestData.CALCS_INDEX)
			self._count_all(TestData.STAPLES_INDEX)
			self._count_all(TestData.BATTERS_INDEX)

			self._clear_cursor(TestData.LIBRARY_INDEX)

			self._select_columns(TestData.FLIGHTS_INDEX, "*")
			self._select_columns(TestData.ECOMMERCE_INDEX, "*")
			self._select_columns(TestData.LOGS_INDEX, "*")

		print("Tests successful.")

# vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=118 :
