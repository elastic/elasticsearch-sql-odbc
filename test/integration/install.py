#
# Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
# or more contributor license agreements. Licensed under the Elastic License;
# you may not use this file except in compliance with the Elastic License.
#

import os
import time
import psutil
import subprocess
import ctypes
import sys
import atexit
import tempfile

from elasticsearch import Elasticsearch

DRIVER_BASE_NAME = "Elasticsearch ODBC Driver"
# Uninstallation can take quite a long time (over 10s)
INSTALLATION_TIMEOUT = 60

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

	@staticmethod
	def uninstall_driver_win(driver_name):
		print("Uninstalling any existing '%s' driver." % driver_name)
		name_filter = "name = '%s'" % driver_name
		with psutil.Popen(["wmic", "/INTERACTIVE:OFF", "product", "where", name_filter, "call", "uninstall"],
				stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True) as p:
			try:
				p.wait(INSTALLATION_TIMEOUT)
			except psutil.TimeoutExpired:
				print("ERROR: driver installation timed out: host state check recommended!")
				raise Exception("wmic-uninstallation didn't finish after %s seconds." % INSTALLATION_TIMEOUT)

			assert(p.returncode is not None)
			out, err = p.communicate()
			if (out): print(out)
			if (err): print(err)
			if p.returncode:
				print("ERROR: driver wmic-uninstallation failed with code: %s." % p.returncode)
			else:
				print("INFO: an existing '%s' driver was %s." % (driver_name,
					"uninstalled" if ("ReturnValue = 0" in out) else "not found"))

	def _install_driver_win(self, ephemeral):
		# remove any old driver (of tested bitness)
		Installer.uninstall_driver_win(self._driver_name)

		# get a file name to log the installation into
		(fd, log_name) = tempfile.mkstemp(suffix="-installer.log")
		os.close(fd)
		if ephemeral:
			atexit.register(os.remove, log_name)
		# install the new driver
		with psutil.Popen(["msiexec.exe", "/i", self._driver_path, "/norestart", "/quiet", "/l*vx", log_name]) as p:
			try:
				p.wait(INSTALLATION_TIMEOUT)
			except psutil.TimeoutExpired as e:
				print("ERROR: driver uninstallation timed out: host state check recommended!")
				raise Exception("msi-installer didn't finish after %s seconds" % INSTALLATION_TIMEOUT)

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
				# wmic-uninstall rather than msiexec /x: the Win restart manager will (sometimes?) detect the current
				# parent (python) process as holding a file handler with the uninstalled .msi (which is actually not
				# the case) and will signal it (along with the build.bat, when launched by it), effectively stopping
				# the unit testing.
				atexit.register(Installer.uninstall_driver_win, self._driver_name)

	def install(self, ephemeral):
		if os.name == "nt":
			return self._install_driver_win(ephemeral)
		else:
			raise Exception("unsupported OS: %s" % os.name)

# vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=118 :
