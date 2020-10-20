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

# File:   install_sonorem.py

# Compiles and installs the sonorem application.

import sys
import os
import subprocess

def main():
	import argparse
	oParser = argparse.ArgumentParser()
	oParser.add_argument("-b", "--buildtype", help="build type (default=Release)"\
						, choices=['Debug', 'Release', 'MinSizeRel', 'RelWithDebInfo']\
						, default="Release", dest="sBuildType")
	oParser.add_argument("-t", "--tests", help="build tests (default=Cache)", choices=['On', 'Off', 'Cache']\
						, default="Cache", dest="sBuildTests")
	oParser.add_argument("--installdir", help="install prefix (default=/usr/local)", metavar='INSTALLDIR'\
						, default="/usr/local", dest="sInstallDir")
	oParser.add_argument("--no-configure", help="don't configure", action="store_true"\
						, default=False, dest="bDontConfigure")
	oParser.add_argument("--no-make", help="don't make", action="store_true"\
						, default=False, dest="bDontMake")
	oParser.add_argument("--no-install", help="don't install", action="store_true"\
						, default=False, dest="bDontInstall")
	oParser.add_argument("--no-sudo", help="don't use sudo to install", action="store_true"\
						, default=False, dest="bDontSudo")
	oParser.add_argument("--no-icons", help="don't install icons (implies --no-launcher)", action="store_true"\
						, default=False, dest="bNoIcons")
	oParser.add_argument("--no-launcher", help="don't install launcher (which needs gksu)", action="store_true"\
						, default=False, dest="bNoLauncher")
	oParser.add_argument("--no-man", help="don't install man page", action="store_true"\
						, default=False, dest="bNoMan")
	oArgs = oParser.parse_args()

	sInstallDir = os.path.abspath(os.path.expanduser(oArgs.sInstallDir))

	sScriptDir = os.path.dirname(os.path.abspath(__file__))
	#print("sScriptDir:" + sScriptDir)
	os.chdir(sScriptDir)
	os.chdir("..")
	#
	sInstallDir = "-D CMAKE_INSTALL_PREFIX=" + sInstallDir
	#print("sInstallDir:" + sInstallDir)
	#
	sBuildType = "-D CMAKE_BUILD_TYPE=" + oArgs.sBuildType
	#print("sBuildType:" + sBuildType)

	sBuildTests = "-D BUILD_TESTING="
	if oArgs.sBuildTests == "On":
		sBuildTests += "ON"
	elif oArgs.sBuildTests == "Off":
		sBuildTests += "OFF"
	else:
		sBuildTests = ""
	#print("sBuildTests:" + sBuildTests)

	if oArgs.bDontSudo:
		sSudo = ""
	else:
		sSudo = "sudo"

	sInstallIcons = "-D STMM_INSTALL_ICONS="
	if (oArgs.bNoIcons):
		sInstallIcons += "OFF"
		oArgs.bNoLauncher = True
	else:
		sInstallIcons += "ON"

	sInstallLauncher = "-D STMM_INSTALL_LAUNCHER="
	if (oArgs.bNoLauncher):
		sInstallLauncher += "OFF"
	else:
		sInstallLauncher += "ON"

	sInstallMan = "-D STMM_INSTALL_MAN_PAGE="
	if (oArgs.bNoMan):
		sInstallMan += "OFF"
	else:
		sInstallMan += "ON"

	if not os.path.isdir("build"):
		os.mkdir("build")

	os.chdir("build")

	if not oArgs.bDontConfigure:
		subprocess.check_call("cmake {} {} {} {} {} {} ..".format(sBuildType, sBuildTests, sInstallDir\
															, sInstallIcons, sInstallLauncher, sInstallMan).split())
	if not oArgs.bDontMake:
		subprocess.check_call("make $STMM_MAKE_OPTIONS", shell=True)

	if not oArgs.bDontInstall:
		try:
			sEnvDestDir = os.environ["DESTDIR"]
		except KeyError:
			sEnvDestDir = ""

		if sEnvDestDir != "":
			sEnvDestDir = "DESTDIR=" + sEnvDestDir
		subprocess.check_call("{} make install {}".format(sSudo, sEnvDestDir).split())


if __name__ == "__main__":
	main()

