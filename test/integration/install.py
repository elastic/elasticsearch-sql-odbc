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

from elasticsearch import TERM_TIMEOUT


def install_driver_win(driver_path):
	with psutil.Popen(["msiexec.exe", "/i", driver_path, "/norestart", "/quiet"]) as p:
		waiting_since = time.time()
		while p.poll() is None:
			time.sleep(.3)
			if TERM_TIMEOUT < time.time() - waiting_since:
				try: p.kill()
				except: pass
				raise Exception("installer killed after %s seconds" % TERM_TIMEOUT)
		assert(p.returncode is not None)
		if p.returncode:
			if not ctypes.windll.shell32.IsUserAnAdmin():
				print("WARNING: running as non-admin -- likely failure cause, if user lacks appropriate privileges")
			raise Exception("driver installation failed with code: %s (see "\
					"https://docs.microsoft.com/en-us/windows/desktop/msi/error-codes)." % p.returncode)
		print("Succesfully installed %s" % driver_path)
		# TODO: requires installer MSI uninstall support
		#atexit.register(psutil.Popen, ["msiexec.exe", "/x", driver_path, "/norestart", "/quiet"])

def install_driver(driver_path):
	bitness_py = 32 if sys.maxsize <= 2**32 else 64
	bitness_driver = 32 if "-x86." in driver_path else 64
	if bitness_py != bitness_driver:
		print("WARNING: bitness misalignment between interpreter (%s) and driver (%s): testing will likely fail" %
				(bitness_py, bitness_driver))

	if os.name == "nt":
		return install_driver_win(driver_path)
	else:
		raise Exception("unsupported OS: %s" % os.name)

# vim: set noet fenc=utf-8 ff=dos sts=0 sw=4 ts=4 tw=118 :
