#!/usr/bin/env python3

# Copyright © 2020  Stefano Marsili, <stemars@gmx.ch>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public
# License along with this program; if not, see <http://www.gnu.org/licenses/>

# File:   testall.py

# - compiles and installs the project contained in this source package
#   for both Debug and Release configurations
# - runs all tests
# - makes sure no line in the code starts with std::cout
# - optionally calls clang-tidy

import sys
import os
import subprocess


def testAll(sBuildType, sInstallDir, sSudo):

	subprocess.check_call("./scripts/uninstall_sonorem.py -y --installdir {}  {}".format(sInstallDir, sSudo).split())

	subprocess.check_call("./scripts/install_sonorem.py -b {} -t On  --installdir {}  {}"\
			.format(sBuildType, sInstallDir, sSudo).split())

	os.chdir("build")
	subprocess.check_call("make test", shell=True)
	os.chdir("..")



def checkTidy():
	#
	print("-------checkTidy ")
	subprocess.check_call("./scripts/priv/checktidy.py".split())
	

def main():

	import argparse
	oParser = argparse.ArgumentParser(description="Uninstall, compile, reinstall and test the sonorem project")
	oParser.add_argument("-y", "--no-prompt", help="no prompt comfirmation", action="store_true"\
						, default=False, dest="bNoPrompt")
	oParser.add_argument("--installdir", help="install dir (default=/usr/local)", metavar='INSTALLDIR'\
						, default="/usr/local", dest="sInstallDir")
	oParser.add_argument("--no-sudo", help="don't use sudo to (un)install", action="store_true"\
						, default=False, dest="bDontSudo")
	oParser.add_argument("-b", "--buildtype", help="build type (default=Both)", choices=['Debug', 'Release', 'Both']\
						, default="Both", dest="sBuildType")
	oParser.add_argument("--tidy", help="apply clang-tidy to source code", action="store_true"\
						, default=False, dest="bTidy")
	oArgs = oParser.parse_args()

	while not oArgs.bNoPrompt:
		sAnswer = input("Are you sure? (yes/no) >").strip()
		if sAnswer.casefold() == "yes":
			break
		elif sAnswer.casefold() == "no":
			return #-----------------------------------------------------------

	print("-----------------------------")
	print("testall.py (sonorem) started!")
	print("-----------------------------")

	sInstallDir = os.path.abspath(os.path.expanduser(oArgs.sInstallDir))

	sScriptDir = os.path.dirname(os.path.abspath(__file__))
	os.chdir(sScriptDir)
	os.chdir("..")

	subprocess.check_call("./share/python/checkcode.py".split())

	if oArgs.bDontSudo:
		sSudo = "--no-sudo"
	else:
		sSudo = ""

	if oArgs.sBuildType == "Both":
		testAll("Debug", sInstallDir, sSudo)
		if oArgs.bTidy:
			checkTidy()
		#
		testAll("Release", sInstallDir, sSudo)
		if oArgs.bTidy:
			checkTidy()
	else:
		testAll(oArgs.sBuildType, sInstallDir, sSudo)
		if oArgs.bTidy:
			checkTidy()

	print("-------------------------------------------")
	print("testall.py (sonorem) finished successfully!")
	print("-------------------------------------------")


if __name__ == "__main__":
	main()

