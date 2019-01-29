#
# Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
# or more contributor license agreements. Licensed under the Elastic License;
# you may not use this file except in compliance with the Elastic License.
#

import os
import time
import psutil
import ctypes
import sys
import atexit

from elasticsearch import Elasticsearch

class Installer(object):
	_driver_path = None

	def __init__(self, path):
		self._driver_path = path

		bitness_py = 32 if sys.maxsize <= 2**32 else 64
		bitness_driver = 32 if "-x86.msi" in path else 64
		if bitness_py != bitness_driver:
			print("WARNING: bitness misalignment between interpreter (%s) and driver (%s): testing will likely fail" %
					(bitness_py, bitness_driver))

	def _install_driver_win(self, ephemeral):
		with psutil.Popen(["msiexec.exe", "/i", self._driver_path, "/norestart", "/quiet"]) as p:
			waiting_since = time.time()
			while p.poll() is None:
				time.sleep(.3)
				if Elasticsearch.TERM_TIMEOUT < time.time() - waiting_since:
					try: p.kill()
					except: pass
					raise Exception("installer killed after %s seconds" % Elasticsearch.TERM_TIMEOUT)
			assert(p.returncode is not None)
			if p.returncode:
				if not ctypes.windll.shell32.IsUserAnAdmin():
					print("WARNING: running as non-admin -- likely the failure cause, if user lacks appropriate "
							"privileges")
				raise Exception("driver installation failed with code: %s (see "\
						"https://docs.microsoft.com/en-us/windows/desktop/msi/error-codes)." % p.returncode)
			print("Driver installed (%s)." % self._driver_path)
			if ephemeral:
				atexit.register(psutil.Popen, ["msiexec.exe", "/x", self._driver_path, "/norestart", "/quiet"])

	def install(self, ephemeral):
		if os.name == "nt":
			return self._install_driver_win(ephemeral)
		else:
			raise Exception("unsupported OS: %s" % os.name)



# vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=118 :
