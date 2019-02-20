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
import os

from elasticsearch import Elasticsearch

REQ_AUTH = ("elastic", Elasticsearch.AUTH_PASSWORD)

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


ES_DATASET_BASE_URL = "https://raw.githubusercontent.com/elastic/elasticsearch/6857d305270be3d987689fda37cc84b7bc18fbb3/x-pack/plugin/sql/qa/src/main/resources/"

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

class TestData(object):

	MODE_NOINDEX = 1 # load CSVs and fill metadatas (_csv_* dictionaries)
	MODE_REINDEX = 2 # drop old indices if any and index freshly
	MODE_INDEX = 3 # index data

	CALCS_FILE = "Calcs_headers.csv"
	CALCS_INDEX = "calcs"
	STAPLES_FILE = "Staples_utf8_headers.csv"
	STAPLES_INDEX = "staples"
	LIBRARY_FILE = "library.csv"
	LIBRARY_INDEX = "library"
	EMPLOYEES_FILE = "employees.csv"
	EMPLOYEES_INDEX = "employees"

	# loaded CSV attributes
	_csv_md5 = None
	_csv_header = None
	_csv_lines = None

	_offline_dir = None
	_mode = None

	def __init__(self, mode=MODE_INDEX, offline_dir=None):
		self._csv_md5 = {}
		self._csv_header = {}
		self._csv_lines = {}

		self._offline_dir = offline_dir
		self._mode = mode

	def _csv_to_json_docs(self, csv_text):
		stream = io.StringIO(csv_text)
		reader = csv.reader(stream, delimiter=',', quotechar='"')
		
		json_docs = []
		header_row = next(reader)
		# Staples CSV header ends in `"name" ` and the reader turns it into `"name "` => strip column names
		header = [col.strip() for col in header_row]
		cnt = 0
		for row in reader:
			doc = {}
			for index, item in enumerate(row):
				if item:
					doc[header[index]] = item
			json_docs.append(doc)
			cnt += 1
			#if 10 < cnt:
			#	break
		#print("items: %s" % cnt)
		return json_docs

	def _docs_to_ndjson_batch(self, docs, index_string):
		ndjson = ""
		for doc in docs:
			ndjson += index_string + "\n"
			ndjson += json.dumps(doc) + "\n"
		return ndjson

	def _docs_to_ndjson(self, index_name, docs):
		index_json = {"index": {"_index": index_name, "_type": "_doc"}}
		index_string = json.dumps(index_json)

		ndjsons = []
		for i in range(0, len(docs), BATCH_SIZE):
			ndjson = self._docs_to_ndjson_batch(docs[i : i + BATCH_SIZE], index_string)
			ndjsons.append(ndjson)
		
		return ndjsons if 1 < len(ndjsons) else ndjsons[0]


	def _register_md5(self, index_name, text, encoding):
		md5 = hashlib.md5()
		md5.update(bytes(text, encoding))
		self._csv_md5[index_name] = md5.hexdigest()

	def _register_header(self, index_name, text):
		stream = io.StringIO(text)
		header = stream.readline()
		self._csv_header[index_name] = header.strip().split(",")

	def _csv_as_ndjson(self, csv_text, encoding, index_name):
		#ndjson = csv_to_ndjson(csv_text, index_name)
		docs = self._csv_to_json_docs(csv_text)
		ndjson = self._docs_to_ndjson(index_name, docs)

		self._register_md5(index_name, csv_text, encoding)
		self._register_header(index_name, csv_text)
		self._csv_lines[index_name] = len(docs)

		return ndjson

	def _get_csv_as_ndjson(self, base_url, csv_name, index_name):
		if self._offline_dir:
			path = os.path.join(self._offline_dir, csv_name)
			with open(path, "rb") as f:
				return self._csv_as_ndjson(f.read().decode("utf-8"), "utf-8", index_name)
		else:
			assert(base_url.endswith("/"))
			url = base_url + csv_name
			req = requests.get(url, timeout = Elasticsearch.REQ_TIMEOUT)
			if req.status_code != 200:
				raise Exception("failed to fetch %s with code %s" % (url, req.status_code))
			return self._csv_as_ndjson(req.text, req.encoding, index_name)


	def _prepare_tableau_load(self, file_name, index_name, index_template):
		ndjson = self._get_csv_as_ndjson(TABLEAU_DATASET_BASE_URL, file_name, index_name)

		if self.MODE_NOINDEX < self._mode:
			with requests.put("http://localhost:%s/_template/%s_template" % (Elasticsearch.ES_PORT, index_name),
					json=index_template, auth=REQ_AUTH) as req:
				if req.status_code != 200:
					raise Exception("PUT %s template failed with code: %s (content: %s)" % (index_name,
						req.status_code, req.text))

		return ndjson

	def _post_ndjson(self, ndjson, index_name, pipeline_name=None):
		url = "http://localhost:%s/%s/_doc/_bulk" % (Elasticsearch.ES_PORT, index_name)
		if pipeline_name:
			url += "?pipeline=%s" % pipeline_name
		with requests.post(url, data=ndjson, headers = {"Content-Type": "application/x-ndjson"}, auth=REQ_AUTH) as req:
			if req.status_code != 200:
				raise Exception("bulk POST to %s failed with code: %s (content: %s)" % (index_name, req.status_code,
					req.text))
			reply = json.loads(req.text)
			if reply["errors"]:
				raise Exception("bulk POST to %s failed with content: %s" % (index_name, req.text))

	def _wait_for_results(self, index_name):
		hits = 0
		waiting_since = time.time()
		while hits < MIN_INDEXED_DOCS:
			url = "http://localhost:%s/%s/_search" % (Elasticsearch.ES_PORT, index_name)
			req = requests.get(url, timeout = Elasticsearch.REQ_TIMEOUT, auth=REQ_AUTH)
			if req.status_code != 200:
				raise Exception("failed to _search %s: code: %s, body: %s" % (index_name, req.status_code, req.text))
			answer = json.loads(req.text)
			total = answer["hits"]["total"]
			if type(total) is int:
				hits = total
			else:
				hits = answer["hits"]["total"]["value"]
			time.sleep(.25)
			if Elasticsearch.REQ_TIMEOUT < time.time() - waiting_since:
				raise Exception("index '%s' has less than %s documents indexed" % (index_name, MIN_INDEXED_DOCS))

	def _delete_if_needed(self, index_name):
		if self._mode != self.MODE_REINDEX:
			return

		url = "http://localhost:%s/%s" % (Elasticsearch.ES_PORT, index_name)
		with requests.delete(url, timeout = Elasticsearch.REQ_TIMEOUT, auth=REQ_AUTH) as req:
			if req.status_code != 200 and req.status_code != 404:
				raise Exception("Deleting index %s failed; code=%s, body: %s." %
						(index_name, req.status_code, req.text))

	def _load_tableau_calcs(self):
		ndjson = self._prepare_tableau_load(self.CALCS_FILE, self.CALCS_INDEX, CALCS_TEMPLATE)

		if self.MODE_NOINDEX < self._mode:
			self._delete_if_needed(self.CALCS_INDEX)
			with requests.put("http://localhost:%s/_ingest/pipeline/parse_%s" % (Elasticsearch.ES_PORT,
					self.CALCS_INDEX), json=CALCS_PIPELINE, auth=REQ_AUTH) as req:
				if req.status_code != 200:
					raise Exception("PUT %s pipeline failed with code: %s (content: %s) " % (self.CALCS_INDEX,
						req.status_code, req.text))

			self._post_ndjson(ndjson, self.CALCS_INDEX, "parse_" + self.CALCS_INDEX)
			self._wait_for_results(self.CALCS_INDEX)

	def _load_tableau_staples(self):
		ndjsons = self._prepare_tableau_load(self.STAPLES_FILE, self.STAPLES_INDEX, STAPLES_TEMPLATE)
		assert(isinstance(ndjsons, list))
		if self.MODE_NOINDEX < self._mode:
			self._delete_if_needed(self.STAPLES_INDEX)
			for ndjson in ndjsons:
				self._post_ndjson(ndjson, self.STAPLES_INDEX)
			self._wait_for_results(self.STAPLES_INDEX)

	def _load_elastic_library(self):
		ndjson = self._get_csv_as_ndjson(ES_DATASET_BASE_URL, self.LIBRARY_FILE, self.LIBRARY_INDEX)
		if self.MODE_NOINDEX < self._mode:
			self._delete_if_needed(self.LIBRARY_INDEX)
			self._post_ndjson(ndjson, self.LIBRARY_INDEX)
			self._wait_for_results(self.LIBRARY_INDEX)

	def _load_elastic_employees(self):
		ndjson = self._get_csv_as_ndjson(ES_DATASET_BASE_URL, self.EMPLOYEES_FILE, self.EMPLOYEES_INDEX)
		if self.MODE_NOINDEX < self._mode:
			self._delete_if_needed(self.EMPLOYEES_INDEX)
			self._post_ndjson(ndjson, self.EMPLOYEES_INDEX)
			self._wait_for_results(self.EMPLOYEES_INDEX)

	def load(self):
		self._load_tableau_calcs()
		self._load_tableau_staples()
		self._load_elastic_library()
		self._load_elastic_employees()
		print("Data %s." % ("meta-processed" if self._mode == self.MODE_NOINDEX else "reindexed" if self._mode == \
			self.MODE_REINDEX else "indexed"))

	def csv_attributes(self, csv_name):
		return (self._csv_md5[csv_name],  self._csv_header[csv_name], self._csv_lines[csv_name])

# vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=118 :
