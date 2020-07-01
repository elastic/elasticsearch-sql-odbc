#
# Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
# or more contributor license agreements. Licensed under the Elastic License;
# you may not use this file except in compliance with the Elastic License.
#

import requests
import urllib3
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
from subprocess import PIPE
from urllib.parse import urlparse

ARTIF_URL = "https://artifacts-api.elastic.co/v1/versions"
ES_PROJECT = "elasticsearch"
PACKAGING = "zip"
ES_ARCH = "-windows-x86_64"

K1 = 1024
M1 = K1 * K1


class Elasticsearch(object):

	TERM_TIMEOUT = 5 # how long to wait for processes to die (before KILLing)
	REQ_TIMEOUT = 20 # default GET request timeout
	ES_PORT = 9200
	ES_CREDENTIALS = ("elastic", "elastic") # user, pwd
	ES_BASE_URL = "http://localhost:%s" % ES_PORT
	ES_START_TIMEOUT = 60 # how long to wait for Elasticsearch to come online
	ES_401_RETRIES = 8 # how many "starting" 401 answers to accept before giving up (.5s waiting inbetween)

	_offline_dir = None
	_credentials = None
	_base_url = None

	_req = None

	def __init__(self, offline_dir=None, url=None):
		self._offline_dir = offline_dir
		if not url:
			self._port = self.ES_PORT
			self._base_url = self.ES_BASE_URL
			self._credentials = self.ES_CREDENTIALS
		else:
			u = urlparse(url)
			self._port = u.port if u.port else self.ES_PORT
			self._base_url = "%s://%s:%s" % (u.scheme, u.hostname, self._port)
			self._credentials = (u.username, u.password) if u.username else self.ES_CREDENTIALS
		print("Using Elasticsearch instance at %s, credentials (%s, %s)" % (self._base_url, self._credentials[0],
			"*" * len(self._credentials[1])))

		self._req = requests.session()
		self._req.verify = False
		urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

	@staticmethod
	def elasticsearch_distro_filename(version):
		return "%s-%s%s.%s" % (ES_PROJECT, version, ES_ARCH, PACKAGING)

	def credentials(self):
		return self._credentials

	def base_url(self):
		return self._base_url

	def _latest_build(self, version):
		req = self._req.get(ARTIF_URL, timeout=self.REQ_TIMEOUT)
		vers = req.json()["versions"]
		if version not in vers:
			raise Exception("version %s not found; available: %s" % (version, vers))

		builds_url = ARTIF_URL + "/" + version + "/builds"
		req = self._req.get(builds_url, timeout=self.REQ_TIMEOUT)
		builds = req.json()["builds"]

		# file name to download
		file_name = Elasticsearch.elasticsearch_distro_filename(version)

		# Determine the most recent build
		latest_et = datetime(1, 1, 1) # latest end_time
		latest_url = None
		# requests.async: no Windows support
		for build in builds:
			build_url = builds_url + "/" + build
			req = self._req.get(build_url, timeout=self.REQ_TIMEOUT)
			build_json = req.json()["build"]
			dt_et = datetime.strptime(build_json["end_time"], "%a, %d %b %Y %X %Z")
			if latest_et < dt_et:
				latest_et = dt_et
				latest_url = build_json["projects"][ES_PROJECT]["packages"][file_name]["url"]
				# break # (TODO: is this always the first entry?)

		if not latest_url:
			raise Exception("no projects reported as built")

		print("- latest version of %s, built on %s, available under: %s" % (ES_PROJECT, latest_et, latest_url))
		return latest_url

	def _download_build(self, url, dest_dir):
		print("- downloading %s: " % url, end='')
		size = 0
		req = self._req.get(url, stream=True, timeout=self.REQ_TIMEOUT)
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

	def _fetch_elasticsearch(self, version, dest_dir):
		if self._offline_dir:
			print("Copying Elasticsearch from %s. " % self._offline_dir)
			file_name = Elasticsearch.elasticsearch_distro_filename(version)
			src_path = os.path.join(self._offline_dir, file_name)
			if not os.path.isfile(src_path):
				raise Exception("Can't find %s in offline dir %s" % (file_name, self._offline_dir))
			dest_file = os.path.join(dest_dir, file_name)
			shutil.copyfile(src_path, dest_file)
		else:
			print("Fetching Elasticsearch from %s: " % ARTIF_URL)
			start_ts = time.time()
			url = self._latest_build(version)
			delta = time.time() - start_ts
			print("- latest build URL determined in: %.2f sec." % delta)

			start_ts = time.time()
			dest_file, size = self._download_build(url, dest_dir)
			delta = time.time() - start_ts
			print("- build of size %.2fMB downloaded in %.2f sec (%.2f Mbps)." % (size/M1, delta,
				(8 * size)/(M1 * delta)))

		return dest_file

	def _update_es_yaml(self, es_dir, append=True):
		yaml = os.path.join(es_dir, "config", "elasticsearch.yml")
		with open(yaml, mode="a" if append else "w", newline="\n") as f:
			f.write("#\n# ODBC Integration Test\n#\n")
			f.write("xpack.security.enabled: True\n")
			f.write("http.port: %s\n" % self._port) # don't bind on next avail port
			f.write("cluster.routing.allocation.disk.threshold_enabled: False\n")

	@staticmethod
	def _stop_es(es_proc):
		children = es_proc.children()
		for child in children:
			child.terminate() # == kill on Win
		children.append(es_proc)
		try:
			gone, alive = psutil.wait_procs(children, timeout=Elasticsearch.TERM_TIMEOUT)
			for child in alive:
				child.kill()
		except Exception as e:
			# just a warning: the batch starting ES will die after java.exe exits and psutil can throw an exception
			# about the (.bat) process no longer existing
			print("WARN: while killing ES: %s." % e)

		print("Elasticsearch stopped.")

	def _start_elasticsearch(self, es_dir):
		start_script = os.path.join(es_dir, "bin", "elasticsearch")
		if os.name == "nt":
			start_script += ".bat"
			creationflags = subprocess.CREATE_NEW_PROCESS_GROUP
		else:
			creationflags = 0

		if self.is_listening():
			raise Exception("an Elasticsearch instance is already running")

		print("Starting Elasticsearch with: %s" % start_script)
		# don't daemonize to get the start logs inlined with those that this app generates
		es_proc = psutil.Popen(start_script, close_fds=True, creationflags=creationflags)
		atexit.register(Elasticsearch._stop_es, es_proc)
		# it takes ES a few seconds to start: don't parse output, just wait till it's online
		waiting_since = time.time()
		failures = 0
		while es_proc.returncode is None:
			try:
				if self.is_listening():
					break
			except Exception as e:
				failures += 1
				# it seems that on a "fortunate" timing, ES will return a 401 when just starting, even if no
				# authentication is enabled at this point: try to give it more time to start
				if self.ES_401_RETRIES < failures:
					raise e
			time.sleep(.5)
			if self.ES_START_TIMEOUT < time.time() - waiting_since:
				raise Exception("Elasticsearch failed to start in %s seconds" % self.ES_START_TIMEOUT)
			es_proc.poll()

		print("Elasticsearch online.")

	def _gen_passwords(self, es_dir):
		pwd_script = os.path.join(es_dir, "bin", "elasticsearch-setup-passwords")
		if os.name == "nt":
			pwd_script += ".bat"
		with psutil.Popen([pwd_script, "auto", "-b"], stdout=PIPE, stderr=PIPE, universal_newlines=True) as p:
			lines = ""
			for line in p.stdout.readlines():
				lines += line
				if "PASSWORD elastic =" in line:
					return line.split("=")[1].strip()
			raise Exception("password generation failed with:\nOUT:%s\nERR: %s" % (lines if lines else p.stdout.read(),
				p.stderr.read()))

	def _enable_xpack(self, es_dir):
		# setup passwords to random generated ones first...
		pwd = self._gen_passwords(es_dir)
		# ...then change passwords, easier to restart with failed tests
		req = self._req.post("%s/_security/user/_password" % self._base_url, auth=(self._credentials[0], pwd),
				json={"password": self._credentials[1]})
		if req.status_code != 200:
			raise Exception("attempt to change elastic's password failed with code %s" % req.status_code)
		# kibana too (debug convenience)
		req = self._req.post("%s/_security/user/kibana/_password" % self._base_url, auth=self._credentials,
				json={"password": self._credentials[1]})
		if req.status_code != 200:
			print("ERROR: kibana user password change failed with code: %s" % req.status_code)

		# start trial mode
		url = "%s/_license/start_trial?acknowledge=true" % self._base_url
		failures = 0
		while True:
			req = self._req.post(url, auth=self._credentials, timeout=self.REQ_TIMEOUT)
			if req.status_code == 200:
				# TODO: check content?
				break
			print("starting of trial failed (#%s) with status: %s, text: %s" % (failures, req.status_code, req.text))
			failures += 1
			if self.ES_401_RETRIES < failures:
				raise Exception("starting of trial failed with status: %s, text: %s" % (req.status_code, req.text))
			time.sleep(.5)

	def spawn(self, version, root_dir=None, ephemeral=False):
		stage_dir = tempfile.mkdtemp(suffix=".ITES", dir=root_dir)
		if ephemeral:
			atexit.register(shutil.rmtree, stage_dir, ignore_errors=True)

		es_build = self._fetch_elasticsearch(version, stage_dir)
		print("ES fetched to: %s" % es_build)

		with zipfile.ZipFile(es_build) as z:
			z.extractall(stage_dir)

		es_dir = es_build[: es_build.rfind('.')] # strip the extension
		es_dir = es_dir[: -len(ES_ARCH)] # strip the arch tag
		self._update_es_yaml(es_dir)

		self._start_elasticsearch(es_dir)
		self._enable_xpack(es_dir)

	def reset(self, es_dir, reset_conf=False):
		# make sure there's no instance running
		try:
			if self.is_listening():
				raise Exception()
		except:
			raise Exception("port %s is active; if Elasticsearch is running it needs to be shut down first" %
					self._port)

		data_path = os.path.join(es_dir, "data")
		if os.path.isdir(data_path):
			for folder in os.listdir(data_path):
				path = os.path.join(data_path, folder)
				if os.path.isdir(path):
					shutil.rmtree(path)
				else:
					os.remove(path)

		if reset_conf:
			self._update_es_yaml(es_dir, False)
		self._start_elasticsearch(es_dir)
		self._enable_xpack(es_dir)

	def cluster_name(self, fail_on_non200=True):
		try:
			resp = self._req.get(self._base_url, auth=self._credentials, timeout=self.REQ_TIMEOUT)
		except (requests.Timeout, requests.ConnectionError):
			return None
		if resp.status_code != 200:
			if fail_on_non200:
				raise Exception("unexpected ES response code received: %s" % resp.status_code)
			else:
				return ""
		if "cluster_name" not in resp.json():
			raise Exception("unexpected ES answer received: %s" % resp.text)
		return resp.json().get("cluster_name")

	def is_listening(self):
		return self.cluster_name(False) is not None

# vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=118 :
