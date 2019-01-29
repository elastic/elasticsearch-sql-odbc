#!/usr/bin/python3
#
# Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
# or more contributor license agreements. Licensed under the Elastic License;
# you may not use this file except in compliance with the Elastic License.
#

# Required modules: pyodbc psutil requests

import argparse
import os, sys, re, time

from elasticsearch import Elasticsearch #spawn_elasticsearch, reset_elasticsearch, es_is_listening, AUTH_PASSWORD
from data import TestData
from install import Installer
from testing import Testing


def ites(args):
	es = Elasticsearch()

	# create a running instance of Elasticsearch if needed
	if not args.pre_staged:
		if args.es_reset:
			es_dir = os.path.abspath(args.es_reset)
			es.reset(es_dir)
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
	elif not es.is_listening(Elasticsearch.AUTH_PASSWORD):
		raise Exception("no running prestaged Elasticsearch instance found.")
	else:
		print("Using pre-staged Elasticsearch.")

	# add test data into it
	if args.reindex or not (args.skip_indexing and args.skip_tests):
		data = TestData(TestData.MODE_NOINDEX if args.skip_indexing else TestData.MODE_REINDEX if args.reindex else \
				TestData.MODE_INDEX)
		data.load()

	# install the driver
	if args.driver:
		driver_path = os.path.abspath(args.driver)
		installer = Installer(driver_path)
		installer.install(args.ephemeral)

	# run the tests
	if not args.skip_tests:
		assert(data is not None)
		tests = Testing(data)
		tests.perform()

def main():
	parser = argparse.ArgumentParser(description='Integration Testing with Elasticsearch.')

	stage_grp = parser.add_mutually_exclusive_group()
	stage_grp.add_argument("-r", "--root-dir", help="Root directory to [temporarily] stage Elasticsearch into.")
	stage_grp.add_argument("-s", "--es-reset", help="Path to an already configured Elasticsearch folder to "
			"use; data directory content will be removed; 'ephemeral' will be ignored.")
	stage_grp.add_argument("-p", "--pre-staged", help="Use a pre-staged and running Elasticsearch instance",
			action="store_true", default=False)

	parser.add_argument("-d", "--driver", help="The path to the driver file to test; if not provided, the driver "
			"is assumed to have been installed.")
	parser.add_argument("-e", "--ephemeral", help="Remove the staged Elasticsearch and installed driver after testing"
			" if test is succesful.", action="store_true", default=False)
	parser.add_argument("-t", "--skip-tests", help="Skip running the tests.", action="store_true", default=False)

	idx_grp = parser.add_mutually_exclusive_group()
	idx_grp.add_argument("-i", "--skip-indexing", help="Skip indexing test data.", action="store_true", default=False)
	idx_grp.add_argument("-x", "--reindex", help="Drop indices if any and (re)index test data.",
			action="store_true", default=False)

	parser.add_argument("-v", "--version", help="Elasticsearch version to test against; read from the driver file by "
			"default.")

	args = parser.parse_args()
	if not (args.root_dir or args.es_reset or args.pre_staged):
		parser.error("no Elasticsearch instance or root/staged directory provided.")

	if not (args.driver or args.version or args.es_reset or args.pre_staged):
		parser.error("don't know what Elasticsearch version to test against.")

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
