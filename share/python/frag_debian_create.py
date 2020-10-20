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

# File:   frag_debian_create.py

# Creates debian source package, binary packages and binary dist zip.

import sys
import os
import stat
import shutil
import subprocess
from datetime import date
from tempfile import mkstemp
import email.utils

# Expects following global variables being defined (example stmm-input):

#  g_sSourceProjectName = "stmm-input"
#  # The subprojects
#  g_oSubPrjs = ["libstmm-input", "libstmm-input-base", "libstmm-input-dl"\
#				, "libstmm-input-ev", "libstmm-input-fake", "libstmm-input-gtk", "libstmm-input-gtk-dm"\
#				, "stmm-input-plugins"]
#  # The binaries containing shared object libraries for runtime
#  g_oBinLibs = ["libstmm-input", "libstmm-input-base", "libstmm-input-dl"\
#				, "libstmm-input-ev", "libstmm-input-gtk", "libstmm-input-gtk-dm"]
#  # The binaries containing headers, documents and shared object libraries for building
#  g_oDevLibs = ["libstmm-input-dev", "libstmm-input-base-dev", "libstmm-input-dl-dev"\
#				, "libstmm-input-ev-dev", "libstmm-input-gtk-dev", "libstmm-input-gtk-dm-dev"\
#				, "libstmm-input-fake-dev"\
#				, "libstmm-input-doc"]
#  g_oAllLibs = g_oBinLibs + g_oDevLibs
#  # The binaries containing executables
#  g_oBinExes = ["stmm-input-plugins"]
#  g_oAllPkgs = g_oAllLibs + g_oBinExes
#
#  g_sScriptDirPath = os.path.dirname(os.path.abspath(__file__))

# and being executed with:

#  import os
#
#  sTempX1X2X3 = g_sScriptDirPath + "/../share/python/frag_debian_create.py"
#
#  exec(compile(source=open(sTempX1X2X3).read(), filename=sTempX1X2X3, mode="exec"))


g_sDefaultWebsite = "https://www.efanomars.com"

g_sDefaultPackagerFullName = "Stefano Marsili"
g_sDefaultPackagerEMail = "efanomars@gmx.ch"

try:
	g_sPackagerFullName = os.environ["DEBFULLNAME"]
except KeyError:
	#raise RuntimeError("Couldn't get environment variable DEBFULLNAME")
	g_sPackagerFullName = g_sDefaultPackagerFullName
	try:
		os.environ["DEBFULLNAME"] = g_sPackagerFullName
	except KeyError:
		raise RuntimeError("Couldn't set environment variable DEBFULLNAME to: " + g_sPackagerFullName)

try:
	g_sPackagerEMail = os.environ["DEBEMAIL"]
except KeyError:
	#raise RuntimeError("Couldn't get environment variable DEBEMAIL")
	g_sPackagerEMail = g_sDefaultPackagerEMail
	try:
		os.environ["DEBEMAIL"] = g_sPackagerEMail
	except KeyError:
		raise RuntimeError("Couldn't set environment variable DEBEMAIL to: " + g_sPackagerEMail)


# -----SNIPPET-BEGIN--GetInfoFromChangedFile-----
import re
import datetime
import email.utils

g_aMonthNames = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
g_sMonthNames = ""
for sMonthName in g_aMonthNames:
	g_sMonthNames = g_sMonthNames + sMonthName + "|"
g_sMonthNames = g_sMonthNames[:-1]
#
g_aWeekDayAbbrs = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
g_sWeekDayAbbrs = ""
for sWeekDayAbbr in g_aWeekDayAbbrs:
	g_sWeekDayAbbrs = g_sWeekDayAbbrs + sWeekDayAbbr + "|"
g_sWeekDayAbbrs = g_sWeekDayAbbrs[:-1]
#
# Example: Wed, 07 Nov 2018 07:23:39 +0100
g_sDateCommonRegex = "(" + g_sWeekDayAbbrs + "),"\
				" ([0-3][0-9]) (" + g_sMonthNames + ") ([0-9][0-9][0-9][0-9])"\
				" ([0-2][0-9]:[0-5][0-9]:[0-5][0-9]) ([\+-][0-9][0-9][0-9][0-9])"

# Example: 1.3.2
g_sVersionCommonRegex = "([0-9]+\.)+[0-9]+"

g_sVersionRegex = "^" + g_sVersionCommonRegex + "$"
g_oVersionPatt = re.compile(g_sVersionRegex)

g_sDateRegex = "^" + g_sDateCommonRegex + "$"
g_oDatePatt = re.compile(g_sDateRegex)

g_sChangeDateRegex = "^ -- (.*) <(.*)>  (" + g_sDateCommonRegex + ")[ ]*$"
g_oChangeDatePatt = re.compile(g_sChangeDateRegex)

#==============================================================================
# sPrjName: ex. stmm-input or libstmm-input-gtk
# returns (sLastVersion, oDate, sText, sAuthorFullName, sAuthorEMail)
def getLastVersionAndDateFromChanged(sPrjName, sChangedFilePath, nSkip):
	sChangeEntryRegex = "^" + sPrjName + " \((" + g_sVersionCommonRegex + ")\) .*$"
	oChangeEntryPatt = re.compile(sChangeEntryRegex)

	sLastVersion = ""
	oDate = datetime.datetime.now()
	try:
		oF = open(sChangedFilePath, 'r')
	except PermissionError:
		return ("", oDate, "", "", "") #---------------
	sText = ""
	bWithinEntry = False
	for sLine in oF:
		oMatch = oChangeEntryPatt.search(sLine)
		if oMatch:
			if bWithinEntry:
				raise RuntimeError("Changes file '" + sChangedFilePath + "' faulty: entry start without end")
			if nSkip < 0:
				raise RuntimeError("Changes file '" + sChangedFilePath + "' faulty: entry start without end")
			sLastVersion = oMatch.group(1);
			bWithinEntry = True
		else:
			oMatch = g_oChangeDatePatt.search(sLine)
			if oMatch:
				if not bWithinEntry:
					raise RuntimeError("Changes file '" + sChangedFilePath + "' faulty: entry end without start")
				bWithinEntry = False
				nSkip = nSkip -1
				#
				sAuthorFullName = oMatch.group(1)
				sAuthorEMail = oMatch.group(2)
				sDateTimeTz = oMatch.group(3)
				#print("sAuthorFullName=" + sAuthorFullName)
				#print("sAuthorEMail=" + sAuthorEMail)
				#print("sDateTimeTz=" + sDateTimeTz)
				try:
					oDate = email.utils.parsedate_to_datetime(sDateTimeTz)
				except:
					raise RuntimeError("Changes file '" + sChangedFilePath + "' faulty: date and time incorrect")
				#
				if nSkip < 0:
					break
		if bWithinEntry:
			if sText != "":
				sText = sText + "\n"
			sText = sText + sLine
	else:
		if bWithinEntry:
			raise RuntimeError("Changes file '" + sChangedFilePath + "' faulty: entry start without end")
		return ("", oDate, "", "" , "") #---------------

	return (sLastVersion, oDate, sText, sAuthorFullName, sAuthorEMail)
#-----SNIPPET-END----GetInfoFromChangedFile-----

#==============================================================================
def stringIsAllDigitsOrEmpty(sStr):
	for c in sStr:
		if not (c in "0123456789"):
			return False
	return True

#==============================================================================
# sSubPrjPath: any of the subproject directories
def transformFile(sFileName, sSourceFileDir, sDestFileDir, sPrivScriptDirPath, sSubPrjPath\
				, sDebSrcVersion, nDebRevision, sChangelogDate
				, sAuthorFullName, sAuthorEMail, sWebsite, sWebsiteSection):
	bDoFileName = ("@" in sFileName)
	if bDoFileName:
		(oFd1, sTmpPath1) = mkstemp(prefix=g_sSourceProjectName + "_1", text=True)
		(oFd2, sTmpPath2) = mkstemp(prefix=g_sSourceProjectName + "_2", text=True)
	else:
		sTmpPath1 = ""
		sTmpPath2 = ""
	sParams =            '-D STMMI_DEBIAN_SRC_FILENAME="' + sFileName + '"'
	sParams = sParams + ' -D STMMI_DEBIAN_SRC_FROM_DIR="' + sSourceFileDir + '"'
	sParams = sParams + ' -D STMMI_DEBIAN_SRC_TO_DIR="' + sDestFileDir + '"'
	sParams = sParams + ' -D STMMI_DEBIAN_SRC_TMP_1="' + sTmpPath1 + '"'
	sParams = sParams + ' -D STMMI_DEBIAN_SRC_TMP_2="' + sTmpPath2 + '"'
	sParams = sParams + ' -D STMMI_PROJECT_SOURCE_DIR="' + sSubPrjPath + '"'
	sParams = sParams + ' -D STMMI_DEBIAN_SRC_VERSION="' + sDebSrcVersion + '"'
	sParams = sParams + ' -D STMMI_DEBIAN_REVISION="' + str(nDebRevision) + '"'
	sParams = sParams + ' -D STMMI_DEBIAN_CHANGELOG_DATE="' + sChangelogDate + '"'
	sParams = sParams + ' -D STMMI_PACKAGER_FULLNAME="' + g_sPackagerFullName + '"'
	sParams = sParams + ' -D STMMI_PACKAGER_EMAIL="' + g_sPackagerEMail + '"'
	sParams = sParams + ' -D STMMI_AUTHOR_FULLNAME="' + sAuthorFullName + '"'
	sParams = sParams + ' -D STMMI_AUTHOR_EMAIL="' + sAuthorEMail + '"'
	sParams = sParams + ' -D STMMI_WEBSITE="' + sWebsite + '"'
	sWebsiteAndSection = sWebsite
	if sWebsiteSection != "":
		sWebsiteAndSection += "/" + sWebsiteSection
	sParams = sParams + ' -D STMMI_WEBSITE_SECTION="' + sWebsiteAndSection + '"'
	sParams = sParams + ' -D STMMI_AT="@"'
	sCmd = "cmake " + sParams
	sCmd += " -P " + sPrivScriptDirPath + "/debian_src_versions.cmake"
	subprocess.check_call(sCmd, shell=True)
	if bDoFileName:
		os.close(oFd1)
		os.close(oFd2)
		os.remove(sTmpPath1)
		os.remove(sTmpPath2)

#==============================================================================
def main():
	bHasDevLibs = (len(g_oDevLibs) > 0)

	import argparse
	oParser = argparse.ArgumentParser(description="Create DEBIAN source package, binary packages and dist zip.\n"
												"  If not defined environment variable DEBFULLNAME is set to '"
												+ g_sDefaultPackagerFullName + "',\n"
												"  and DEBEMAIL is set to '" + g_sDefaultPackagerEMail + "'"
						, formatter_class=argparse.RawDescriptionHelpFormatter)
	oParser.add_argument("--version", help="the debian source package version (overrides CHANGES)", metavar='VERSION'\
						, default="", dest="sDebSrcVersion")
	oParser.add_argument("--revision", help="the debian revision number (Default: 1)", metavar='REVISION'\
						, default="1", dest="sDebRevision")
	oParser.add_argument("--date", help="the changelog date and time (overrides CHANGES)", metavar='DATE'\
						, default="", dest="sChangelogDate")
	oParser.add_argument("--builddir", help="the dir where everything should be built", metavar='BUILDPATH'\
						, default="", dest="sBuildPath")
	oParser.add_argument("--no-source", help="don't create the source", action="store_true"\
						, default=False, dest="bDontSource")
	oParser.add_argument("--no-binaries", help="don't create the binaries", action="store_true"\
						, default=False, dest="bDontBinaries")
	oParser.add_argument("--no-dist", help="don't create dist tar.gz", action="store_true"\
						, default=False, dest="bDontDist")
	oParser.add_argument("--website", help="The website (Default: " + g_sDefaultWebsite + ")", metavar='WEBSITE'\
						, default=g_sDefaultWebsite, dest="sWebsite")
	oParser.add_argument("--website-section", help="The website (Default: games)", metavar='SECTION'\
						, default="games", dest="sWebsiteSection")
	oArgs = oParser.parse_args()

	sDebSrcVersion = oArgs.sDebSrcVersion
	if (sDebSrcVersion != "") and not g_oVersionPatt.search(sDebSrcVersion):
		raise RuntimeError("Version must satisfy regex '" + g_sVersionRegex + "'")

	try:
		nDebRevision = int(oArgs.sDebRevision)
	except:
		raise RuntimeError("Revision must be number >= 1")
	if nDebRevision < 1: 
		raise RuntimeError("Revision must be number >= 1")

	sChangelogDate = oArgs.sChangelogDate
	if (sChangelogDate != "") and not g_oDatePatt.search(sChangelogDate):
		raise RuntimeError("Date must satisfy regex '" + g_sDateRegex + "'")

	sBuildPath = os.path.abspath(os.path.expanduser(oArgs.sBuildPath))
	if not oArgs.bDontSource:
		if os.path.exists(sBuildPath):
			if not os.path.isdir(sBuildPath):
				raise RuntimeError("BUILDPATH " + sBuildPath + " is not a directory")
			if len(os.listdir(sBuildPath)) > 0:
				raise RuntimeError("BUILDPATH " + sBuildPath + " is not empty")
		else:
			os.mkdir(sBuildPath, 0o775)

	sScriptDirPath = g_sScriptDirPath #os.path.dirname(os.path.abspath(__file__))
	sPrivScriptDirPath = sScriptDirPath  + "/priv"
	sSourceDirPath = os.path.dirname(sScriptDirPath)
	sParentDirPath = os.path.dirname(sSourceDirPath)

	os.chdir(sSourceDirPath)
	#print("sSourceDirPath = " + sSourceDirPath)

	oRes = getLastVersionAndDateFromChanged(g_sSourceProjectName, "CHANGES", 0)
	(sLastVersion, oChangelogDate, sUnusedText, sAuthorFullName, sAuthorEMail) = oRes
	if sLastVersion == "":
		raise RuntimeError("CHANGES file not found or has no entry")
	if sDebSrcVersion == "":
		sDebSrcVersion = sLastVersion
	if sChangelogDate == "":
		sChangelogDate = email.utils.format_datetime(oChangelogDate)
	#print("sChangelogDate = " + sChangelogDate)

	os.chdir(sScriptDirPath)
	#print("sScriptDirPath = " + sScriptDirPath)

	sRawPkgName = g_sSourceProjectName + "-" + sDebSrcVersion
	sTarPkgName = g_sSourceProjectName + "_" + sDebSrcVersion + ".orig.tar.gz"

	if not oArgs.bDontSource:
		sTempTarName = sRawPkgName + "_tmp"
		sTempTarPkgName = sTempTarName + ".tar.gz"
		subprocess.check_call("./sourcepackage.py -t --filename=" + sTempTarName, shell=True)

		shutil.move(sParentDirPath + "/" + sTempTarPkgName, sBuildPath + "/" + sTarPkgName, copy_function=shutil.copy)

	os.chdir(sBuildPath)

	if not oArgs.bDontSource:
		subprocess.check_call("tar mxvzf " + sTarPkgName, shell=True)

		os.rename(g_sSourceProjectName, sRawPkgName)

	sRawSrcPkgPath = sBuildPath + "/" + sRawPkgName
	sDebianOrigPath = sRawSrcPkgPath + "/scripts/priv/debian.orig"

	os.chdir(sRawSrcPkgPath)

	if not oArgs.bDontSource:
		os.mkdir("debian", 0o775)
		#
		if len(g_oAllPkgs) == 1:
			sSubPrjPath = sSourceDirPath
		else:
			# transformFile needs this
			sSubPrjPath = sSourceDirPath + "/" + g_oSubPrjs[0]
			# multi binary debian source package, assumes
			# each subproject has its own subdirectory
			# for each a xxx.changelog is created
			for sCurPkg in g_oAllPkgs:
				if sCurPkg[-4:] == "-doc":
					continue
				# Calculate the cmake version string
				sSoName = sCurPkg
				if sSoName[:3] == "lib":
					sSoName = sSoName[3:]
				if sSoName[-4:] == "-dev":
					sSoName = sSoName[:-4]
				sPkgVersionPrefix = sSoName.upper().replace("-", "_")
				print(sPkgVersionPrefix)
				sPkgChangelog = sCurPkg + ".changelog"
				sPkgChangelogIn = sPkgChangelog + ".in"
				oPkgChangelogFile = open(sRawSrcPkgPath + "/debian/" + sPkgChangelogIn, 'w')
				oPkgChangelogFile.write(g_sSourceProjectName + " (@" + sPkgVersionPrefix + "_MAJOR_VERSION@.@"\
										+ sPkgVersionPrefix + "_MINOR_VERSION@-@STMMI_DEBIAN_REVISION@) unstable; urgency=low\n\n")
				oPkgChangelogFile.write("  * This is not an official Debian distro package\n")
				oPkgChangelogFile.write("    but was created using Debian tools.\n")
				oPkgChangelogFile.write("    Please check the source package CHANGES file.\n")
				oPkgChangelogFile.write("    This file is used to set the version of " + sCurPkg + ".\n\n")
				oPkgChangelogFile.write(" -- @STMMI_AUTHOR_FULLNAME@ <@STMMI_AUTHOR_EMAIL@>  @STMMI_DEBIAN_CHANGELOG_DATE@\n")
				oPkgChangelogFile.close()
				transformFile(sPkgChangelog, sRawSrcPkgPath + "/debian", sRawSrcPkgPath + "/debian"\
							, sPrivScriptDirPath, sSubPrjPath, sDebSrcVersion, nDebRevision, sChangelogDate
							, sAuthorFullName, sAuthorEMail, oArgs.sWebsite, oArgs.sWebsiteSection)
				os.remove(sRawSrcPkgPath + "/debian/" + sPkgChangelogIn)

		aDirOrigFileNames = os.listdir(sDebianOrigPath)
		for sEntry in aDirOrigFileNames:
			sEntryPath = sDebianOrigPath + "/" + sEntry
			if os.path.isdir(sEntryPath):
				shutil.copytree(sEntryPath, "debian/" + sEntry, copy_function=shutil.copy)
			elif sEntry[-3:] == ".in":
				transformFile(sEntry[:-3], sDebianOrigPath, "debian"\
							, sPrivScriptDirPath, sSubPrjPath, sDebSrcVersion, nDebRevision, sChangelogDate
							, sAuthorFullName, sAuthorEMail, oArgs.sWebsite, oArgs.sWebsiteSection)
			else:
				shutil.copyfile(sEntryPath, "debian/" + sEntry)
				if sEntry == "rules":
					oStat = os.stat("debian/" + sEntry)
					os.chmod("debian/" + sEntry, oStat.st_mode | stat.S_IXUSR)

	if not oArgs.bDontBinaries:
		g_sPackagerEMail = g_sDefaultPackagerEMail
		sDebianBuildCmd = "DEBFULLNAME=\"" + g_sPackagerFullName + "\" DEBEMAIL=\"" + g_sPackagerEMail + "\" "\
						+ "dpkg-buildpackage -us -uc"
		subprocess.check_call(sDebianBuildCmd, shell=True)

	if not oArgs.bDontDist:
		os.chdir(sBuildPath)
		# map from package name without soversion to deb filename and package name with soversion
		oBinToDebs = {sCurPkg : ("", "") for sCurPkg in g_oAllPkgs}
		#
		aDirFileNames = os.listdir()
		sDebBins = ""
		for sFileName in aDirFileNames:
			#print("sFileName = '" + sFileName + "'")
			if sFileName[-4:] == ".deb":
				nPos = sFileName.find("_")
				sPkgName = sFileName[:nPos]
				#print("  sPkgName = " + sPkgName)

				if (sPkgName[-4:] == "-dev") or (sPkgName[-4:] == "-doc"):
					sBinName = sPkgName
					if not sBinName in oBinToDebs:
						raise RuntimeError("Internal error unknown package: " + sBinName)
					oBinToDebs[sBinName] = (sFileName, sPkgName)
					sDebBins += "./" + sFileName + " "
				elif not (sPkgName[-7:] == "-dbgsym"):
					sBinName = sPkgName
					for sCurBinLib in g_oBinLibs:
						if sPkgName[:len(sCurBinLib)] == sCurBinLib:
							sRest = sPkgName[len(sCurBinLib):]
							if stringIsAllDigitsOrEmpty(sRest):
								sBinName = sCurBinLib
								break # for sCurBinLib
					else:
						if not (sBinName in g_oBinExes):
							raise RuntimeError("Internal error unknown package: " + sPkgName)
					oBinToDebs[sBinName] = (sFileName, sPkgName)
					sDebBins += "./" + sFileName + " "
		#

		sShaBang = "#!/bin/sh\n"
		sBashOpt = """
OPTIND=1
nodevel=1
while getopts "h?d" opt; do
    case "$opt" in
    h|\?)
        echo "USAGE: $0 [-d]"
        echo "  -d    include devel packages"
        exit 0
        ;;
    d)  nodevel=0
        ;;
    esac
done
shift $((OPTIND-1))
[ "${1:-}" = "--" ] && shift
if [ "x$@x" != "xx" ]; then
    echo "Illegal parameter(s) '$@'"
    exit 0
fi
"""
		sIfDevel = """
if [ "$nodevel" -eq 0 ]; then
"""
		sElseDevel = """
else
"""
		sEndifDevel = """
fi
"""

		sBinInstallSh = "install-bin.sh"
		oBinInstallShFile = open(sBinInstallSh, 'w')
		oBinInstallShFile.write(sShaBang)
		if bHasDevLibs:
			oBinInstallShFile.write(sBashOpt)

		oBinInstallShFile.write("sudo dpkg -i \\\n")
		for sPkg in g_oAllPkgs:
			(sFileName, sPkgName) = oBinToDebs[sPkg]
			if sFileName == "":
				raise RuntimeError("No deb file found for " + sPkg)
			if (sPkg in g_oBinLibs) or (sPkg in g_oBinExes):
				oBinInstallShFile.write("  " + sFileName + " \\\n")
		oBinInstallShFile.write("#")

		if bHasDevLibs:
			oBinInstallShFile.write(sIfDevel)
			oBinInstallShFile.write("    sudo dpkg -i \\\n")
			for sPkg in g_oAllPkgs:
				(sFileName, sPkgName) = oBinToDebs[sPkg]
				if not ((sPkg in g_oBinLibs) or (sPkg in g_oBinExes)):
					oBinInstallShFile.write("      " + sFileName + " \\\n")
			oBinInstallShFile.write("    #")
			oBinInstallShFile.write(sEndifDevel)

		oStat = os.fstat(oBinInstallShFile.fileno())
		os.fchmod(oBinInstallShFile.fileno(), oStat.st_mode | stat.S_IXUSR)
		oBinInstallShFile.close()

		g_oAllPkgs.reverse()

		sBinUninstallSh = "uninstall-bin.sh"
		oBinUninstallShFile = open(sBinUninstallSh, 'w')
		oBinUninstallShFile.write(sShaBang)
		if bHasDevLibs:
			oBinUninstallShFile.write(sBashOpt)

		if bHasDevLibs:
			oBinUninstallShFile.write(sIfDevel)
			oBinUninstallShFile.write("    sudo dpkg -r \\\n")
			for sPkg in g_oAllPkgs:
				(sFileName, sPkgName) = oBinToDebs[sPkg]
				if not ((sPkg in g_oBinLibs) or (sPkg in g_oBinExes)):
					oBinUninstallShFile.write("      " + sPkgName + " \\\n")
			oBinUninstallShFile.write("    #")
			oBinUninstallShFile.write(sEndifDevel)

		oBinUninstallShFile.write("sudo dpkg -r \\\n")
		for sPkg in g_oAllPkgs:
			(sFileName, sPkgName) = oBinToDebs[sPkg]
			if (sPkg in g_oBinLibs) or (sPkg in g_oBinExes):
				oBinUninstallShFile.write("  " + sPkgName + " \\\n")
		oBinUninstallShFile.write("#")

		oStat = os.fstat(oBinUninstallShFile.fileno())
		os.fchmod(oBinUninstallShFile.fileno(), oStat.st_mode | stat.S_IXUSR)
		oBinUninstallShFile.close()

		sBinDistFileName = g_sSourceProjectName + "-" + sDebSrcVersion + "-" + str(nDebRevision) + "-dist.tar.gz"
		#
		sReadmeFileName = "README"
		oReadmeFile = open(sReadmeFileName, 'w')
		oReadmeFile.write(sBinDistFileName + "\n")
		oReadmeFile.write(("=" * len(sBinDistFileName)) + "\n\n")
		oReadmeFile.write("To install the DEB binary packages please use\n\n")
		oReadmeFile.write("  $ ./" + sBinInstallSh + "\n\n")
		oReadmeFile.write("from this directory\n\n")
		oReadmeFile.write("To uninstall them use command\n\n")
		oReadmeFile.write("  $ ./" + sBinUninstallSh + "\n")
		oReadmeFile.close()

		sCmd = ("tar -zcf  {} -v {} {} {} {} --transform='s,^\\.,{},'").format(
											sBinDistFileName, sDebBins\
											, "./" + sBinInstallSh\
											, "./" + sBinUninstallSh\
											, "./" + sReadmeFileName\
											, sRawPkgName)
		#print(sCmd)
		subprocess.check_call(sCmd, shell=True)
		print("Created bin dist zip: " + sBinDistFileName)

		os.remove(sReadmeFileName)
		#
		os.remove(sBinInstallSh)
		os.remove(sBinUninstallSh)

if __name__ == "__main__":
	main()

