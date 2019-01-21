#
# Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
# or more contributor license agreements. Licensed under the Elastic License;
# you may not use this file except in compliance with the Elastic License.
#

import pyodbc
import datetime
import hashlib

from elasticsearch import AUTH_PASSWORD
from data import CSV_MD5, CSV_HEADER, CSV_LINES, LIBRARY_INDEX, EMPLOYEES_INDEX, CALCS_INDEX, STAPLES_INDEX

CONNECT_STRING = 'Driver={Elasticsearch Driver};UID=elastic;PWD=%s;Secure=0' % AUTH_PASSWORD

def reconstitute_csv(index_name):
	with pyodbc.connect(CONNECT_STRING) as cnxn:
		cnxn.setencoding(encoding='utf-8')
		csv = u""
		cols = CSV_HEADER[index_name]
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

def test_index_as_csv(index_name):
	csv = reconstitute_csv(index_name)

	md5 = hashlib.md5()
	md5.update(bytes(csv, "utf-8"))
	if md5.hexdigest() != CSV_MD5[index_name]:
		print("Reconstituded CSV of index '%s':\n%s" % (index_name, csv))
		raise Exception("reconstituted CSV differs from original for index '%s'" % index_name)

def test_index_count_all(index_name):
	with pyodbc.connect(CONNECT_STRING) as cnxn:
		cnt = 0
		with cnxn.execute("select 1 from %s" % index_name) as curs:
			while curs.fetchone():
				cnt += 1
		if cnt != CSV_LINES[index_name]:
			raise Exception("index '%s' only contains %s documents (vs original %s CSV lines)" %
					(index_name, cnt, CSV_LINES[index_name]))

def run_tests():
	test_index_as_csv(LIBRARY_INDEX)
	test_index_as_csv(EMPLOYEES_INDEX)
	test_index_count_all(CALCS_INDEX)
	test_index_count_all(STAPLES_INDEX)

# vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=118 :
