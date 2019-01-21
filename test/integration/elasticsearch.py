#
# Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
# or more contributor license agreements. Licensed under the Elastic License;
# you may not use this file except in compliance with the Elastic License.
#

import requests
import psutil

import sys
from datetime import datetime
import time
import os
import zipfile
import tempfile
import shutil
import atexit
import signal
import subprocess
from  subprocess import PIPE

ARTIF_URL = "https://artifacts-api.elastic.co/v1/versions"
ES_PROJECT = "elasticsearch"
PACKAGING = "zip"

TERM_TIMEOUT = 5 # how long to wait for processes to die (before KILLing)
REQ_TIMEOUT = 20 # default GET request timeout
ES_PORT = 9200
ES_START_TIMEOUT = 30
AUTH_PASSWORD = "elastic"

K1 = 1024
M1 = K1 * K1

def latest_build(version):
	req = requests.get(ARTIF_URL, timeout=REQ_TIMEOUT)
	vers = req.json()["versions"]
	if version not in vers:
		raise Exception("version %s not found; available: %s" % (version, vers))

	builds_url = ARTIF_URL + "/" + version + "/builds"
	req = requests.get(builds_url, timeout=REQ_TIMEOUT)
	builds = req.json()["builds"]

	# file name to download
	file_name = "%s-%s.%s" % (ES_PROJECT, version, PACKAGING)

	# Determine the most recent build
	latest_et = datetime(1, 1, 1) # latest end_time
	latest_url = None
	# requests.async: no Windows support
	for build in builds:
		build_url = builds_url + "/" + build
		req = requests.get(build_url, timeout=REQ_TIMEOUT)
		build_json = req.json()["build"]
		dt_et = datetime.strptime(build_json["end_time"], "%a, %d %b %Y %X %Z")
		if latest_et < dt_et:
			latest_et = dt_et
			latest_url = build_json["projects"][ES_PROJECT]["packages"][file_name]["url"]
			break # (TODO: is this always the first entry?)

	if not latest_url:
		raise Exception("no projects reported as built")

	print("- latest version of %s, built on %s, available under: %s" % (ES_PROJECT, latest_et, latest_url))
	return latest_url

def download_build(url, dest_dir):
	print("- downloading %s: " % url, end='')
	size = 0
	req = requests.get(url, stream=True, timeout=REQ_TIMEOUT)
	dest_file = os.path.join(dest_dir, req.url[req.url.rfind('/')+1 : ])
	with open(dest_file, "wb") as f:
		for chunk in req.iter_content(chunk_size = 32 * K1):
			if chunk:
				f.write(chunk)
				size += len(chunk)
				if size % (10 * M1) == 0:
					sys.stdout.write('.')
					sys.stdout.flush()
	print() # CRLF
	return (dest_file, size)

def fetch_elasticsearch(version, dest_dir):
	print("Fetching Elasticsearch from %s: " % ARTIF_URL)
	start_ts = time.time()
	url = latest_build(version)
	delta = time.time() - start_ts
	print("- latest build URL determined in: %.2f sec." % delta)

	start_ts = time.time()
	dest_file, size = download_build(url, dest_dir)
	delta = time.time() - start_ts
	print("- build of size %.2fMB downloaded in %.2f sec (%.2f Mbps)." % (size/M1, delta, (8 * size)/(M1 * delta)))
	return dest_file

def update_es_yaml(es_dir):
	yaml = os.path.join(es_dir, "config", "elasticsearch.yml")
	with open(yaml, mode="a", newline="\n") as f:
		f.write("#\n# ODBC Integration Test\n#\n")
		f.write("xpack.security.enabled: True\n")
		f.write("http.port: %s\n" % ES_PORT) # don't bind on next avail port
		f.write("cluster.routing.allocation.disk.threshold_enabled: False\n")

def es_is_listening(password=None):
	auth = ("elastic", password) if password else None
	try:
		req = requests.get("http://localhost:%s" % ES_PORT, auth=auth, timeout=.5)
	except requests.Timeout:
		return False
	if req.status_code != 200:
		raise Exception("unexpected ES response code received: %s" % req.status_code)
	if "You Know, for Search" not in req.text:
		raise Exception("unexpected ES answer received: %s" % req.text)
	return True

def stop_es(es_proc):
	children = es_proc.children()
	children.append(es_proc)
	for child in children:
		child.terminate()
	gone, alive = psutil.wait_procs(children, timeout=TERM_TIMEOUT)
	for child in alive:
		child.kill()

	print("Elasticsearch stopped.")


def start_elasticsearch(es_dir):
	start_script = os.path.join(es_dir, "bin", "elasticsearch")
	if os.name == "nt":
		start_script += ".bat"

	if es_is_listening():
		raise Exception("an Elasticsearch instance is already running")

	# don't daemonize to get the start logs inlined with those that this app generates
	es_proc = psutil.Popen(start_script, close_fds=True, creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
	atexit.register(stop_es, es_proc)
	# it takes ES a few seconds to start: don't parse output, just wait till it's online
	waiting_since = time.time()
	while es_proc.returncode is None:
		if es_is_listening():
			break
		time.sleep(.5)
		if ES_START_TIMEOUT < time.time() - waiting_since:
			raise Exception("Elasticsearch failed to start in %s seconds" % ES_START_TIMEOUT)
		es_proc.poll()

	print("Elasticsearch online.")

def gen_passwords(es_dir):
	pwd_script = os.path.join(es_dir, "bin", "elasticsearch-setup-passwords")
	if os.name == "nt":
		pwd_script += ".bat"
	with psutil.Popen([pwd_script, "auto", "-b"], stdout=PIPE, stderr=PIPE, universal_newlines=True) as p:
		for line in p.stdout.readlines():
			if "PASSWORD elastic =" in line:
				return line.split("=")[1].strip()
		raise Exception("password generation failed with: %s" % p.stdout.read())


def enable_xpack(es_dir):
	# start trial mode
	req = requests.post("http://localhost:%s/_license/start_trial?acknowledge=true" % ES_PORT)
	if req.status_code != 200:
		raise Exception("starting of trial failed with status: %s" % req.status_code)
	# TODO: check content?

	pwd = gen_passwords(es_dir)
	# change passwords, easier to restart with failed tests
	req = requests.post("http://localhost:%s/_security/user/_password" % ES_PORT, auth=("elastic", pwd), 
			json={"password": AUTH_PASSWORD})
	if req.status_code != 200:
		raise Exception("attempt to change elastic's password failed with code %s" % req.status_code)
	# kibana too (debug convenience)
	req = requests.post("http://localhost:%s/_security/user/kibana/_password" % ES_PORT,
			auth=("elastic", AUTH_PASSWORD), json={"password": AUTH_PASSWORD})
	if req.status_code != 200:
		print("ERROR: kibana user password change failed with code: %s" % req.status_code)

def reset_elasticsearch(es_dir):
	# make sure there's no instance running
	try:
		if es_is_listening():
			raise Exception()
	except:
		raise Exception("port %s is active; if Elasticsearch is running it needs to be shut down first" % ES_PORT)

	data_path = os.path.join(es_dir, "data")
	for folder in os.listdir(data_path):
		path = os.path.join(data_path, folder)
		if os.path.isdir(path):
			shutil.rmtree(path)
		else:
			os.remove(path)

	start_elasticsearch(es_dir)
	enable_xpack(es_dir)

def spawn_elasticsearch(version, root_dir=None, ephemeral=False):
	stage_dir = tempfile.mkdtemp(suffix=".ITES", dir=root_dir)
	if ephemeral:
		atexit.register(shutil.rmtree, stage_dir, ignore_errors=True)
	
	es_build = fetch_elasticsearch(version, stage_dir)
	print("ES fetched to: %s" % es_build)

	with zipfile.ZipFile(es_build) as z:
		z.extractall(stage_dir)

	es_dir = es_build[: es_build.rfind('.')]
	update_es_yaml(es_dir)

	start_elasticsearch(es_dir)
	enable_xpack(es_dir)

# vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=118 :
