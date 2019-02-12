#
# Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
# or more contributor license agreements. Licensed under the Elastic License;
# you may not use this file except in compliance with the Elastic License.
#

import os
import time
import psutil
from subprocess import PIPE
import ctypes
import sys
import atexit
import tempfile

from elasticsearch import Elasticsearch

DRIVER_BASE_NAME = "Elasticsearch ODBC Driver"
WMIC_UNINSTALL_TIMOUT = 30 # it can take quite long

class Installer(object):
	_driver_path = None
	_driver_name = None

	def __init__(self, path):
		self._driver_path = path

		bitness_py = 32 if sys.maxsize <= 2**32 else 64
		bitness_driver = 32 if "-x86.msi" in path else 64
		if bitness_py != bitness_driver:
			print("WARNING: bitness misalignment between interpreter (%s) and driver (%s): testing will likely fail" %
					(bitness_py, bitness_driver))

		self._driver_name = "%s (%sbit)" % (DRIVER_BASE_NAME, bitness_driver)

	def _install_driver_win(self, ephemeral):
		# remove any old driver (of tested bitness)
		name_filter = "name = '%s'" % self._driver_name
		with psutil.Popen(["wmic", "/INTERACTIVE:OFF", "product", "where", name_filter, "call", "uninstall"],
				stdout=PIPE, stderr=PIPE, universal_newlines=True) as p:
			try:
				p.wait(WMIC_UNINSTALL_TIMOUT)
			except psutil.TimeoutExpired as e:
				try: p.kill()
				except: pass
				raise Exception("wmic uninstallation killed after %s seconds" % WMIC_UNINSTALL_TIMOUT)

			assert(p.returncode is not None)
			out, err = p.communicate()
			if (out): print(out)
			if (err): print(err)
			if p.returncode:
				print("ERROR: driver wmic-uninstallation failed.")
			else:
				print("INFO: an old driver was %s." % ("uninstalled" if ("ReturnValue = 0" in out) else "not found"))


		# get a file name to log the installation into
		(fd, log_name) = tempfile.mkstemp(suffix="-installer.log")
		os.close(fd)
		if ephemeral:
			atexit.register(os.remove, log_name)
		# install the new driver
		with psutil.Popen(["msiexec.exe", "/i", self._driver_path, "/norestart", "/quiet", "/l*vx", log_name]) as p:
			try:
				p.wait(Elasticsearch.TERM_TIMEOUT)
			except psutil.TimeoutExpired as e:
				try: p.kill()
				except: pass
				raise Exception("installer killed after %s seconds" % Elasticsearch.TERM_TIMEOUT)

			assert(p.returncode is not None)
			if p.returncode:
				if not ctypes.windll.shell32.IsUserAnAdmin():
					print("WARNING: running as non-admin -- likely the failure cause, if user lacks appropriate "
							"privileges")
				try:
					with open(log_name, "rb") as f:
						print("Failed installation log (%s):\n" % log_name)
						text = f.read()[2:] # skip the BOM to be able to decode the text
						print(text.decode("utf-16"))
				except Exception as e:
					print("ERROR: failed to read log of failed intallation: %s" % e)

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
