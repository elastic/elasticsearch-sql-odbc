#
# Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
# or more contributor license agreements. Licensed under the Elastic License;
# you may not use this file except in compliance with the Elastic License.
#

import requests
import io
import csv
import json
import time
import hashlib

from elasticsearch import AUTH_PASSWORD, REQ_TIMEOUT, ES_PORT

REQ_AUTH = ("elastic", AUTH_PASSWORD)

TABLEAU_DATASET_BASE_URL = "https://raw.githubusercontent.com/elastic/connector-plugin-sdk/120fe213c4bce30d9424c155fbd9b2ad210239e0/tests/datasets/TestV1/"

CALCS_TEMPLATE =\
	{
		"index_patterns": "calcs*",
		"settings": {
			"number_of_shards": 1
		},
		"mappings": {
			"_doc": {
				"properties": {
					"key": {"type": "keyword"},
					"num0": {"type": "double"},
					"num1": {"type": "double"},
					"num2": {"type": "double"},
					"num3": {"type": "double"},
					"num4": {"type": "double"},
					"str0": {"type": "keyword"},
					"str1": {"type": "keyword"},
					"str2": {"type": "keyword"},
					"str3": {"type": "keyword"},
					"int0": {"type": "integer"},
					"int1": {"type": "integer"},
					"int2": {"type": "integer"},
					"int3": {"type": "integer"},
					"bool0": {"type": "boolean"},
					"bool1": {"type": "boolean"},
					"bool2": {"type": "boolean"},
					"bool3": {"type": "boolean"},
					"date0": {"type": "date"},
					"date1": {"type": "date"},
					"date2": {"type": "date"},
					"date3": {"type": "date"},
					"time0": {"type": "date", "format": "yyyy-MM-dd' 'HH:mm:ss"},
					"time1": {"type": "date", "format": "hour_minute_second"},
					"datetime0": {"type": "date", "format": "yyyy-MM-dd' 'HH:mm:ss"},
					"datetime1": {"type": "keyword"},
					"zzz": {"type": "keyword"}
				}
			}
		}
	}


CALCS_PIPELINE =\
	{
		"description": "Parsing the Calcs lines",
		"processors": [
			{
				"script": {
					"lang": "painless",
					"source":
						"""
						ctx.bool0 = ctx.bool0 == "0" ? false : true;
						ctx.bool1 = ctx.bool1 == "0" ? false : true;
						ctx.bool2 = ctx.bool2 == "0" ? false : true;
						ctx.bool3 = ctx.bool3 == "0" ? false : true;
						"""
				}
			}
		]
	}

CALCS_FILE = "Calcs_headers.csv"
CALCS_INDEX = "calcs"

STAPLES_TEMPLATE =\
	{
		"index_patterns": "staples*",
		"settings": {
			"number_of_shards": 1
		},
		"mappings": {
			"_doc": {
				"properties": {
					"Item Count": {"type": "integer"},
					"Ship Priority": {"type": "keyword"},
					"Order Priority": {"type": "keyword"},
					"Order Status": {"type": "keyword"},
					"Order Quantity": {"type": "double"},
					"Sales Total": {"type": "double"},
					"Discount": {"type": "double"},
					"Tax Rate": {"type": "double"},
					"Ship Mode": {"type": "keyword"},
					"Fill Time": {"type": "double"},
					"Gross Profit": {"type": "double"},
					"Price": {"type": "double"},
					"Ship Handle Cost": {"type": "double"},
					"Employee Name": {"type": "keyword"},
					"Employee Dept": {"type": "keyword"},
					"Manager Name": {"type": "keyword"},
					"Employee Yrs Exp": {"type": "double"},
					"Employee Salary": {"type": "double"},
					"Customer Name": {"type": "keyword"},
					"Customer State": {"type": "keyword"},
					"Call Center Region": {"type": "keyword"},
					"Customer Balance": {"type": "double"},
					"Customer Segment": {"type": "keyword"},
					"Prod Type1": {"type": "keyword"},
					"Prod Type2": {"type": "keyword"},
					"Prod Type3": {"type": "keyword"},
					"Prod Type4": {"type": "keyword"},
					"Product Name": {"type": "keyword"},
					"Product Container": {"type": "keyword"},
					"Ship Promo": {"type": "keyword"},
					"Supplier Name": {"type": "keyword"},
					"Supplier Balance": {"type": "double"},
					"Supplier Region": {"type": "keyword"},
					"Supplier State": {"type": "keyword"},
					"Order ID": {"type": "keyword"},
					"Order Year": {"type": "integer"},
					"Order Month": {"type": "integer"},
					"Order Day": {"type": "integer"},
					"Order Date": {"type": "date", "format": "yyyy-MM-dd' 'HH:mm:ss"},
					"Order Quarter": {"type": "keyword"},
					"Product Base Margin": {"type": "double"},
					"Product ID": {"type": "keyword"},
					"Receive Time": {"type": "double"},
					"Received Date": {"type": "date", "format": "yyyy-MM-dd' 'HH:mm:ss"},
					"Ship Date": {"type": "date", "format": "yyyy-MM-dd' 'HH:mm:ss"},
					"Ship Charge": {"type": "double"},
					"Total Cycle Time": {"type": "double"},
					"Product In Stock": {"type": "keyword"},
					"PID": {"type": "integer"},
					"Market Segment": {"type": "keyword"}
				}
			}
		}
	}

STAPLES_FILE = "Staples_utf8_headers.csv"
STAPLES_INDEX = "staples"

ES_DATASET_BASE_URL = "https://raw.githubusercontent.com/elastic/elasticsearch/6857d305270be3d987689fda37cc84b7bc18fbb3/x-pack/plugin/sql/qa/src/main/resources/"
LIBRARY_FILE = "library.csv"
LIBRARY_INDEX = "library"
EMPLOYEES_FILE = "employees.csv"
EMPLOYEES_INDEX = "employees"

# python seems to slow down when operating on multiple long strings?
BATCH_SIZE = 500

# how many docs to wait to become availalbe when waiting on indexing
MIN_INDEXED_DOCS = 10

# MD5 checksums of fetched CSVs
CSV_MD5 = {}
# the headers of fetched CSVs
CSV_HEADER = {}
# number of lines, header excluded
CSV_LINES = {}


def csv_to_json_docs(csv_text):
	stream = io.StringIO(csv_text)
	reader = csv.reader(stream, delimiter=',', quotechar='"')
	
	json_docs = []
	header_row = next(reader)
	cnt = 0
	for row in reader:
		doc = {}
		for index, item in enumerate(row):
			if item:
				doc[header_row[index]] = item
		json_docs.append(doc)
		cnt += 1
		#if 10 < cnt:
		#	break
	#print("items: %s" % cnt)
	return json_docs


def _docs_to_ndjson(docs, index_string):
	ndjson = ""
	for doc in docs:
		ndjson += index_string + "\n"
		ndjson += json.dumps(doc) + "\n"
	return ndjson

def docs_to_ndjson(index_name, docs):
	index_json = {"index": {"_index": index_name, "_type": "_doc"}}
	index_string = json.dumps(index_json)

	ndjsons = []
	for i in range(0, len(docs), BATCH_SIZE):
		ndjson = _docs_to_ndjson(docs[i : i + BATCH_SIZE], index_string)
		ndjsons.append(ndjson)
	
	return ndjsons if 1 < len(ndjsons) else ndjsons[0]


def csv_to_ndjson(csv_text, index_name):
	stream = io.StringIO(csv_text)
	reader = csv.reader(stream, delimiter=',', quotechar='"')

	index_string = '{"index": {"_index": "%s", "_type": "_doc"}}' % index_name
	ndjson = ""

	header_row = next(reader)
	for row in reader:
		doc_string = ""
		for index, item in enumerate(row):
			if item:
				doc_string += '"' + header_row[index] + '":"' + item + '",'
		if doc_string:
			doc_string = doc_string[ : -1] # trim last `, `
			doc_string = "{" + doc_string + "}"
			ndjson += index_string + "\n"
			ndjson += doc_string + "\n"

	return ndjson

def register_md5(index_name, text, encoding):
	md5 = hashlib.md5()
	md5.update(bytes(text, encoding))
	CSV_MD5[index_name] = md5.hexdigest()

def register_header(index_name, text):
	stream = io.StringIO(text)
	header = stream.readline()
	CSV_HEADER[index_name] = header.strip().split(",")

def remote_csv_as_ndjson(url, index_name):
	req = requests.get(url, timeout = REQ_TIMEOUT)
	if req.status_code != 200:
		raise Exception("failed to fetch %s with code %s" % (url, req.status_code))

	# TODO: CSV to NDJSON takes long for Staples with json, ujson and no-njson conversion versions: try multiprocessing
	# (the no-json version takes even more.. why?)
	#ndjson = csv_to_ndjson(req.text, index_name)
	docs = csv_to_json_docs(req.text)
	ndjson = docs_to_ndjson(index_name, docs)

	register_md5(index_name, req.text, req.encoding)
	register_header(index_name, req.text)
	CSV_LINES[index_name] = len(docs)

	return ndjson

def post_ndjson(ndjson, index_name, pipeline_name=None):
	url = "http://localhost:%s/%s/_doc/_bulk" % (ES_PORT, index_name)
	if pipeline_name:
		url += "?pipeline=%s" % pipeline_name
	with requests.post(url, data=ndjson, headers = {"Content-Type": "application/x-ndjson"}, auth=REQ_AUTH) as req:
		if req.status_code != 200:
			raise Exception("bulk POST to %s failed with code: %s (content: %s)" % (index_name, req.status_code,
				req.text))
		reply = json.loads(req.text)
		if reply["errors"]:
			raise Exception("bulk POST to %s failed with content: %s" % (index_name, req.text))

def prepare_tableau_load(file_name, index_name, index_template):
	url = TABLEAU_DATASET_BASE_URL + file_name
	ndjson = remote_csv_as_ndjson(url, index_name)

	with requests.put("http://localhost:%s/_template/%s_template" % (ES_PORT, index_name), json=index_template,
			auth=REQ_AUTH) as req:
		if req.status_code != 200:
			raise Exception("PUT %s template failed with code: %s (content: %s)" % (index_name, req.status_code,
				req.text))

	return ndjson

def wait_for_results(index_name):
	hits = 0
	waiting_since = time.time()
	while hits < MIN_INDEXED_DOCS:
		url = "http://elastic:%s@localhost:%s/%s/_search" % (AUTH_PASSWORD, ES_PORT, index_name)
		req = requests.get(url, timeout = REQ_TIMEOUT)
		if req.status_code != 200:
			raise Exception("failed to _search %s: code: %s, content: %s" % (index_name, req.status_code, req.text))
		answer = json.loads(req.text)
		hits = answer["hits"]["total"]["value"]
		time.sleep(.25)
		if REQ_TIMEOUT < time.time() - waiting_since:
			raise Exception("index '%s' has less than %s documents indexed" % (index_name, MIN_INDEXED_DOCS))

def index_tableau_calcs(skip_indexing):
	ndjson = prepare_tableau_load(CALCS_FILE, CALCS_INDEX, CALCS_TEMPLATE)

	with requests.put("http://localhost:%s/_ingest/pipeline/parse_%s" % (ES_PORT, CALCS_INDEX), json=CALCS_PIPELINE,
			auth=REQ_AUTH) as req:
		if req.status_code != 200:
			raise Exception("PUT %s pipeline failed with code: %s (content: %s) " % (CALCS_INDEX, req.status_code,
				req.text))

	if not skip_indexing:
		post_ndjson(ndjson, CALCS_INDEX, "parse_" + CALCS_INDEX)
		wait_for_results(CALCS_INDEX)

def index_tableau_staples(skip_indexing):
	ndjsons = prepare_tableau_load(STAPLES_FILE, STAPLES_INDEX, STAPLES_TEMPLATE)
	assert(isinstance(ndjsons, list))
	if not skip_indexing:
		for ndjson in ndjsons:
			post_ndjson(ndjson, STAPLES_INDEX)
		wait_for_results(STAPLES_INDEX)


def index_es_library(skip_indexing):
	ndjson = remote_csv_as_ndjson(ES_DATASET_BASE_URL + LIBRARY_FILE, LIBRARY_INDEX)
	if not skip_indexing:
		post_ndjson(ndjson, LIBRARY_INDEX)
		wait_for_results(LIBRARY_INDEX)

def index_es_employees(skip_indexing):
	ndjson = remote_csv_as_ndjson(ES_DATASET_BASE_URL + EMPLOYEES_FILE, EMPLOYEES_INDEX)
	if not skip_indexing:
		post_ndjson(ndjson, EMPLOYEES_INDEX)
		wait_for_results(EMPLOYEES_INDEX)

def index_test_data(skip_indexing=False): #skip_indexing: load CSV and fill metas (CSV_* global dicts)
	index_tableau_calcs(skip_indexing)
	index_tableau_staples(skip_indexing)
	index_es_library(skip_indexing)
	index_es_employees(skip_indexing)

# vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=118 :
