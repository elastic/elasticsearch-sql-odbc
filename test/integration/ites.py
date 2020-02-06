#!/usr/bin/python3
#
# Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
# or more contributor license agreements. Licensed under the Elastic License;
# you may not use this file except in compliance with the Elastic License.
#

# Required modules: pyodbc psutil requests

import argparse
import os, sys, re, time

from elasticsearch import Elasticsearch
from data import TestData
from install import Installer
from testing import Testing


def ites(args):
	es = Elasticsearch(args.offline_dir, args.url)

	# create a running instance of Elasticsearch if needed
	if args.url is None:
		es_reset = args.es_full_reset or args.es_reset
		if es_reset:
			es_dir = os.path.abspath(es_reset)
			es.reset(es_dir, args.es_full_reset is not None)
		else:
			assert(args.root_dir)
			root_dir = os.path.abspath(args.root_dir)
			if not args.version:
				toks = args.driver.split("-")
				for i in range(3, len(toks)):
					if re.match("\d+\.\d+\.\d+", toks[-i]):
						version = "-".join(toks[len(toks) - i : -2])
			else:
				version = args.version
			if not version:
				raise Exception("failed to determine Elasticsearch version to test against (params: driver: %s, "
						"version: %s)" % (args.driver, args.version))

			es.spawn(version, root_dir, args.ephemeral)
	elif not es.is_listening():
		raise Exception("no running prestaged Elasticsearch instance found.")
	else:
		print("Using pre-staged Elasticsearch.")

	# add test data into it
	if args.reindex or not (args.skip_indexing and args.skip_tests):
		if args.skip_indexing:
			test_mode = TestData.MODE_NOINDEX
		elif args.reindex:
			test_mode = TestData.MODE_REINDEX
		else:
			test_mode = TestData.MODE_INDEX

		data = TestData(es, test_mode, args.offline_dir)
		data.load()

	# install the driver
	if args.driver:
		driver_path = os.path.abspath(args.driver)
		installer = Installer(driver_path)
		installer.install(args.ephemeral)

	# run the tests
	if not args.skip_tests:
		assert(data is not None)
		cluster_name = es.cluster_name()
		assert(len(cluster_name))
		if args.dsn:
			Testing(es, data, cluster_name, args.dsn).perform()
		else:
			Testing(es, data, cluster_name, "Packing=JSON;Compression=on;").perform()
			Testing(es, data, cluster_name, "Packing=CBOR;Compression=off;").perform()

def main():
	parser = argparse.ArgumentParser(description='Integration Testing with Elasticsearch.')

	stage_grp = parser.add_mutually_exclusive_group()
	stage_grp.add_argument("-r", "--root-dir", help="Root directory to [temporarily] stage Elasticsearch into.")
	stage_grp.add_argument("-s", "--es-reset", help="Path to an already configured Elasticsearch folder to "
			"use; data directory content will be removed; 'ephemeral' will be ignored.", dest="ES_DIR")
	stage_grp.add_argument("-S", "--es-full-reset", help="Path to the Elasticsearch folder; config file and data "
			"directory content will be removed; 'ephemeral' will be ignored.", dest="ES_DIR")
	stage_grp.add_argument("-p", "--url", help="Use a pre-staged and running Elasticsearch instance. If no URL is "
			"provided, %s is assumed." % Elasticsearch.ES_BASE_URL, nargs="?", const="")

	parser.add_argument("-d", "--driver", help="The path to the driver file to test; if not provided, the driver "
			"is assumed to have been installed.")
	parser.add_argument("-c", "--dsn", help="The full or partial connection string to use with a preinstalled "
			"driver; if the provided string contains the name under which the driver to test is registered, it will "
			"be used as such; otherwise it will be appended as additional parameters to a pre-configured DSN.")
	parser.add_argument("-o", "--offline_dir", help="The directory path holding the files to copy the test data from, "
			"as opposed to downloading them.")
	parser.add_argument("-e", "--ephemeral", help="Remove the staged Elasticsearch and installed driver after testing"
			" if test is successful.", action="store_true", default=False)
	parser.add_argument("-t", "--skip-tests", help="Skip running the tests.", action="store_true", default=False)

	idx_grp = parser.add_mutually_exclusive_group()
	idx_grp.add_argument("-i", "--skip-indexing", help="Skip indexing test data.", action="store_true", default=False)
	idx_grp.add_argument("-x", "--reindex", help="Drop indices if any and (re)index test data.",
			action="store_true", default=False)

	parser.add_argument("-v", "--version", help="Elasticsearch version to test against; read from the driver file by "
			"default.")

	args = parser.parse_args()
	if not (args.root_dir or args.es_reset or args.es_full_reset or args.url is not None):
		parser.error("no Elasticsearch instance or root/staged directory provided.")

	if not (args.driver or args.version or args.es_reset or args.es_full_reset or args.url is not None):
		parser.error("don't know what Elasticsearch version to test against.")

	if args.driver and args.dsn and "Driver=" in args.dsn:
		parser.error("driver specified both by -d/--driver and -c/--dsn arguments")

	try:
		started_at = time.time()

		ites(args)

		print("Testing took %.2f seconds." % (time.time() - started_at))
		sys.exit(0)
	except Exception as e:
		print("FAILURE: %s" % e)
		raise e

if __name__== "__main__":
	main()

# vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=118 :
