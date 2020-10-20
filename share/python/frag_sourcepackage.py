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

# File:   frag_sourcepackage.py

# Creates a source package of a project including (if any) subprojects.

import sys
import os
import subprocess
from datetime import date
from tempfile import mkstemp

# Expects following global variables being defined (example stmm-input):

#  g_sProjectName = "stmm-input"
#
#  g_aProjDirs = ["libstmm-input", "libstmm-input-base", "libstmm-input-ev"
#  				, "libstmm-input-dl"
#  				, "libstmm-input-fake"
#  				, "libstmm-input-fake/examples/spinn"
#  				, "libstmm-input-gtk"
#  				, "libstmm-input-gtk-dm"
#  				, "libstmm-input-gtk-dm/examples/bare-app"
#  				, "libstmm-input-gtk-dm/examples/showevs"
#  				, "stmm-input-plugins"]
#
#  g_aExtraExcludes = []
#
#  g_sScriptDirPath = os.path.dirname(os.path.abspath(__file__))


# and being executed with:

#  import os
#
#  sTempX1X2X3 = g_sScriptDirPath + "/../share/python/frag_sourcepackage.py"
#
#  exec(compile(source=open(sTempX1X2X3).read(), filename=sTempX1X2X3, mode="exec"))


def main():
	import argparse
	oParser = argparse.ArgumentParser(description="Create a sourcepackage that can be uploaded\n"
						"  or used to create a debian source package.\n"
						"  The file type .tar.gz must not be part of the optional filename passed to this script."
						, formatter_class=argparse.RawDescriptionHelpFormatter)
	oParser.add_argument("--filename", help="The filename of the archive (default=" \
											+ g_sProjectName + "-YYYYMMDD)", metavar='FILENAME'\
						, default="", dest="sFileName")
	oParser.add_argument("-t", help="touch files", action="store_true"\
						, default=False, dest="bTouch")
	oArgs = oParser.parse_args()

	sScriptDirPath = g_sScriptDirPath # os.path.dirname(os.path.abspath(__file__))
	sSourceDirPath = os.path.dirname(sScriptDirPath)
	sSourceDir = os.path.basename(sSourceDirPath)
	sParentDirPath = os.path.dirname(sSourceDirPath)

	os.chdir(sParentDirPath)

	aExclPatterns = ["build*", "configure", "nbproject*", ".project", ".cproject", ".settings", "core" \
					, "*.geany", "*.geanywb", ".kdev*", "*.kdev*"]

	sExcludes =  " --exclude=.metadata"
	sExcludes += " --exclude=.git"
	sExcludes += " --exclude=stuff"
	sExcludes += " --exclude=core"
	if len(g_aProjDirs) > 0:
		for sProjDir in g_aProjDirs:
			for sExclPattern in aExclPatterns:
				sExcludes += " --exclude=" + os.path.join(sProjDir, sExclPattern)
		#
		aParentRelPaths = [os.path.dirname(sRelPath) for sRelPath in g_aProjDirs
									if not os.path.dirname(sRelPath) == ""]
		aParentRelPaths = list(set(aParentRelPaths))
		#
		for sRelPath in aParentRelPaths:
			sExcludes += " --exclude=" + sRelPath + ".metadata"

	else:
		for sExclPattern in aExclPatterns:
			sExcludes += " --exclude=" + sExclPattern

	for sExtraExclPattern in g_aExtraExcludes:
		sExcludes += " --exclude=" + sExtraExclPattern

	sFileName = oArgs.sFileName
	if sFileName != "":
		if ('/' in sFileName) or (' ' in sFileName):
			raise RuntimeError("Filename cannot contain characters '/', ' '")
		sTarFileName = sFileName + ".tar.gz"
	else:
		oToday = date.today()
		sToday = ("000" + str(oToday.year))[-4:] + ("0" + str(oToday.month))[-2:] + ("0" + str(oToday.day))[-2:]
		sTarFileName = ("{}-{}.tar.gz").format(g_sProjectName, sToday)

	#print("sExcludes=" + sExcludes)

	if oArgs.bTouch:
		(oFd, sTmpPath) = mkstemp(prefix= g_sProjectName + "_touch", text=True)
		sTouch = "--mtime=" + sTmpPath
		os.close(oFd)
	else:
		sTouch = ""

	sCmd = ("tar -zcf {} {} -v"
							" {}"
							" {}").format(sTarFileName, sTouch, sExcludes, sSourceDir)
	#print(sCmd)

	subprocess.check_call(sCmd.split())

	if oArgs.bTouch:
		os.remove(sTmpPath)

if __name__ == "__main__":
	main()

