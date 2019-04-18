#
# Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
# or more contributor license agreements. Licensed under the Elastic License;
# you may not use this file except in compliance with the Elastic License.
#

import pyodbc
import datetime
import hashlib
import unittest

from elasticsearch import Elasticsearch
from data import TestData, BATTERS_TEMPLATE

UID = "elastic"
CONNECT_STRING = 'Driver={Elasticsearch Driver};UID=%s;PWD=%s;Secure=0;' % (UID, Elasticsearch.AUTH_PASSWORD)
CATALOG = "elasticsearch"

class Testing(unittest.TestCase):

	_data = None
	_dsn = None

	def __init__(self, test_data, dsn=None):
		super().__init__()
		self._data = test_data
		self._dsn = dsn if dsn else CONNECT_STRING
		print("Using DSN: '%s'." % self._dsn)

	def _reconstitute_csv(self, index_name):
		with pyodbc.connect(self._dsn) as cnxn:
			cnxn.autocommit = True
			csv = u""
			cols = self._data.csv_attributes(index_name)[1]
			fields = ",".join(cols)
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
		with pyodbc.connect(self._dsn) as cnxn:
			cnxn.autocommit = True
			with cnxn.execute("select 1 from %s" % index_name) as curs:
				while curs.fetchone():
					cnt += 1
		csv_lines = self._data.csv_attributes(index_name)[2]
		if cnt != csv_lines:
			raise Exception("index '%s' only contains %s documents (vs original %s CSV lines)" %
					(index_name, cnt, csv_lines))

	def _clear_cursor(self, index_name):
		conn_str = self._dsn + ";MaxFetchSize=5"
		with pyodbc.connect(conn_str) as cnxn:
			cnxn.autocommit = True
			with cnxn.execute("select 1 from %s limit 10" % index_name) as curs:
				for i in range(3): # must be lower than MaxFetchSize, so no next page be requested
					curs.fetchone()
				# reuse the statment (a URL change occurs)
				with curs.execute("select 1 from %s" % index_name) as curs2:
					curs2.fetchall()
		# no exception raised -> passed

	def _select_columns(self, index_name, columns):
		print("Selecting columns '%s' from index '%s'." % (columns, index_name))
		with pyodbc.connect(self._dsn) as cnxn:
			cnxn.autocommit = True
			stmt = "select %s from %s" % (columns, index_name)
			with cnxn.execute(stmt) as curs:
				cnt = 0
				while curs.fetchone():
					cnt += 1 # no exception -> success
				print("Selected %s rows from %s." % (cnt, index_name))

	def _check_info(self, attr, expected):
		with pyodbc.connect(self._dsn) as cnxn:
			cnxn.autocommit = True
			value = cnxn.getinfo(attr)
			self.assertEqual(value, expected)

	# tables(table=None, catalog=None, schema=None, tableType=None)
	def _catalog_tables(self, no_table_type_as=""):
		with pyodbc.connect(self._dsn) as cnxn:
			cnxn.autocommit = True
			curs = cnxn.cursor()

			# enumerate catalogs
			res = curs.tables("", "%", "", no_table_type_as).fetchall()
			self.assertEqual(len(res), 1)
			for i in range(0,10):
				self.assertEqual(res[0][i], None if i else CATALOG)
			#self.assertEqual(res, [tuple([CATALOG] + [None for i in range(9)])]) # XXX?

			# enumerate table types
			res = curs.tables("", "", "", "%").fetchall()
			self.assertEqual(len(res), 2)
			for i in range(0,10):
				self.assertEqual(res[0][i], None if i != 3 else 'BASE TABLE')
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
		with pyodbc.connect(self._dsn) as cnxn:
			cnxn.autocommit = True
			curs = cnxn.cursor()
			if not use_surrogate:
				res = curs.columns(table=TestData.BATTERS_INDEX, catalog=CATALOG if use_catalog else None).fetchall()
			else:
				if use_catalog:
					stmt = "SYS COLUMNS CATALOG '%s' TABLE LIKE '%s' ESCAPE '\\' LIKE '%%' ESCAPE '\\'" % \
						(CATALOG, TestData.BATTERS_INDEX)
				else:
					stmt = "SYS COLUMNS TABLE LIKE '%s' ESCAPE '\\' LIKE '%%' ESCAPE '\\'" % TestData.BATTERS_INDEX
				res = curs.execute(stmt)
			cols_have = [row[3] for row in res]
			cols_have.sort()
			cols_expect = [k for k in BATTERS_TEMPLATE["mappings"]["properties"].keys()]
			cols_expect.sort()
			self.assertEqual(cols_have, cols_expect)

	def perform(self):
		self._check_info(pyodbc.SQL_USER_NAME, UID)
		self._check_info(pyodbc.SQL_DATABASE_NAME, CATALOG)

		# simulate catalog querying as apps do in ES/GH#40775 do
		self._catalog_tables(no_table_type_as = "")
		self._catalog_tables(no_table_type_as = None)
		self._catalog_columns(use_catalog = False, use_surrogate = True)
		self._catalog_columns(use_catalog = True, use_surrogate = True)

		self._as_csv(TestData.LIBRARY_INDEX)
		self._as_csv(TestData.EMPLOYEES_INDEX)

		self._count_all(TestData.CALCS_INDEX)
		self._count_all(TestData.STAPLES_INDEX)
		self._count_all(TestData.BATTERS_INDEX)

		self._clear_cursor(TestData.LIBRARY_INDEX)

		self._select_columns(TestData.FLIGHTS_INDEX, "*")
		self._select_columns(TestData.ECOMMERCE_INDEX, "*")
		self._select_columns(TestData.LOGS_INDEX, "*")

		print("Tests successful.")

# vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=118 :
