#!/usr/bin/env python3

# Copyright © 2019-2020  Stefano Marsili, <stemars@gmx.ch>
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

# File:   frag_checktidy.py

# Use this command to check a project with clang-tidy.

import sys
import os
import subprocess

# Expects following global variables being defined (example swapper):

#  g_sSourceProjectName = "swapper"
#  g_oSubPrjs = ["libstmm-swapper", "libstmm-swapper-xml", "swapper"]
#
#  import os
#
#  g_sScriptDirPath = os.path.dirname(os.path.abspath(__file__))

# NOTE! g_sScriptDirPath is the project's "scripts" dir, not the "scripts/priv" dir
# and being executed with:

#
#  sTempX1X2X3 = g_sScriptDirPath + "/../share/python/frag_checktidy.py"
#
#  exec(compile(source=open(sTempX1X2X3).read(), filename=sTempX1X2X3, mode="exec"))


def callTidy(sSubDir):

	if sSubDir != "":
		os.chdir(sSubDir)

	if sSubDir != "":
		sProject = sSubDir
	else:
		sProject = g_sSourceProjectName
	print("clang-tidy started for project: " + sProject )

	sTidyOutput = "build/" + sProject + "_tidy.log"
	sCmd = ("clang-tidy -checks='.*' -p build/compile_commands.json  src/*.cc"
			" >" + sTidyOutput)
	subprocess.check_call(sCmd, shell=True)
	#
	if os.path.getsize(sTidyOutput) > 0:
		raise RuntimeError("Error: " + sTidyOutput + " not empty")

	print("clang-tidy finished project " + sProject + ". Output file: " + sTidyOutput)

	if sSubDir != "":
		os.chdir("..")

def main():

	try:
		callArgParser("Clang tidy")
	except NameError:
		import argparse
		oParser = argparse.ArgumentParser(description="Clang tidy")
		oArgs = oParser.parse_args()

	os.chdir(g_sScriptDirPath)
	os.chdir("..")
	#
	if len(g_oSubPrjs) == 0:
		callTidy("")
	else:
		for sSubPrj in g_oSubPrjs:
			callTidy(sSubPrj)

if __name__ == "__main__":
	main()

