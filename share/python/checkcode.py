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

# File:   checkcode.py

# Checks whether there are still debug comments in the source code.

import sys
import os
import re
import subprocess


g_oPattStdCout = re.compile("^std::cout <<")
g_oPattStdCassert = re.compile("#include <cassert>")

def pathHasToBeSkipped(sPathFile):
	if "/build/" in sPathFile:
		return True
	if "/nbproject/" in sPathFile:
		return True
	if "/thirdparty/" in sPathFile:
		return True
	if "/stuff/" in sPathFile:
		return True
	if "/.git/" in sPathFile:
		return True
	return False


def checkSourceFileCout(sPathFile):
	if pathHasToBeSkipped(sPathFile):
		return
	print("Checking: " + sPathFile)
	try:
		oF = open(sPathFile, 'r')
	except PermissionError:
		return
	nLine = 0
	for sLine in oF:
		if g_oPattStdCout.search(sLine):
			raise RuntimeError("Found '^std::cout <<' in file: " + sPathFile + " at line: " + str(nLine))
		nLine += 1

def checkSourceFileCassert(sPathFile):
	if pathHasToBeSkipped(sPathFile):
		return
	print("Checking: " + sPathFile)
	try:
		oF = open(sPathFile, 'r')
	except PermissionError:
		return
	for sLine in oF:
		if g_oPattStdCassert.search(sLine):
			raise RuntimeError("Found '#include <cassert>' in header file: " + sPathFile)

def checkPython3Lint(sPathFile):
	if pathHasToBeSkipped(sPathFile):
		return
	sFile = os.path.basename(sPathFile)
	if sFile[:5] == "frag_":
		return
	print("Checking: " + sPathFile)
	sCmd = 'pylint3 -E "' + sPathFile + '"'
	oProc = subprocess.Popen(sCmd, stdout=subprocess.PIPE, shell=True)
	(sOut, sErr) = oProc.communicate()
	sResult = sOut.decode().strip()
	#oRes = subprocess.run(sCmd, shell=True, check=True, capture_output=True)
	#sResult = str(oRes.stdout.decode()).strip()
	if sResult != "":
		raise RuntimeError("Python error in file: " + sPathFile)


def checkCurDir():
	aEntries = os.listdir()
	for sEntry in aEntries:
		if sEntry[:1] == '.':
			# don't consider hidden files
			continue
		sPathEntry = os.path.abspath(sEntry)
		if os.path.islink(sPathEntry):
			# don't follow symbolic links
			continue
		elif os.path.isdir(sPathEntry):
			os.chdir(sEntry)
			checkCurDir()
			os.chdir("..")
		else:
			(sRoot, sExt) = os.path.splitext(sEntry)
			bIsHeader = ((sExt == ".h") or (sExt == ".hh") or (sExt == ".hpp") or (sExt == ".hxx"))
			if (sExt == ".cc") or (sExt == ".cpp") or (sExt == ".cxx") or (sExt == ".c++")\
					or bIsHeader or (sExt == ".cc.in"):
				checkSourceFileCout(sPathEntry)
				#if bIsHeader:
				#	checkSourceFileCassert(sPathEntry)
			elif sExt == ".py":
				checkPython3Lint(sPathEntry)

def main():

	sScriptDir = os.path.dirname(os.path.abspath(__file__))

	os.chdir(sScriptDir)
	os.chdir("../..")

	checkCurDir()

if __name__ == "__main__":
	main()
