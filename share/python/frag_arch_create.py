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

# File:   frag_arch_create.py

# Creates arch PKGBUILD.

import sys
import os
import stat
import shutil
import subprocess
from datetime import date
import re
from tempfile import mkdtemp
from tempfile import mkstemp
import datetime
import email
import email.utils

# Expects following global variables being defined (example stmm-input):

#  g_sSourceProjectName = "stmm-input"
#  g_sSubPrj = "libstmm-input"
#
#  g_sScriptDirPath = os.path.dirname(os.path.abspath(__file__))

# and being executed with:

#  import os
#
#  sTempX1X2X3 = g_sScriptDirPath + "/../share/python/frag_arch_create.py"
#
#  exec(compile(source=open(sTempX1X2X3).read(), filename=sTempX1X2X3, mode="exec"))


g_sProjectArchDir = "arch_" + g_sSourceProjectName

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

#==============================================================================
# sSubPrjPath: any of the subproject directories
def transformFile(sFileName, sSourceFileDir, sDestFileDir, sPrivScriptDirPath, sSubPrjPath
				, sDebSrcVersion, nDebRevision, sChangelogDate
				, sAuthorFullName, sAuthorEMail, sWebsite, sWebsiteSection
				, sTarballName, sTarballSha256):
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
	#
	sParams = sParams + ' -D STMMI_TARBALL_NAME="' + sTarballName + '"'
	sParams = sParams + ' -D STMMI_TARBALL_SHA256="' + sTarballSha256 + '"'
	sParams = sParams + ' -D STMMI_SOURCE_PROJECT="' + g_sSourceProjectName + '"'
	subprocess.check_call("cmake " + sParams + " -P " + sPrivScriptDirPath + "/debian_src_versions.cmake", shell=True)
	if bDoFileName:
		os.close(oFd1)
		os.close(oFd2)
		os.remove(sTmpPath1)
		os.remove(sTmpPath2)

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

#==============================================================================
def main():

	import argparse
	oParser = argparse.ArgumentParser(description="Create ARCH PKGBUILD.\n"
												"  If not defined environment variable DEBFULLNAME is set to '"
												+ g_sDefaultPackagerFullName + "',\n"
												"  and DEBEMAIL is set to '" + g_sDefaultPackagerEMail + "'"
						, formatter_class=argparse.RawDescriptionHelpFormatter)
	oParser.add_argument("--version", help="the arch package version (overrides CHANGES)", metavar='VERSION'\
						, default="", dest="sDebSrcVersion")
	oParser.add_argument("--revision", help="the arch revision number (Default: 1)", metavar='REVISION'\
						, default="1", dest="sDebRevision")
	oParser.add_argument("--tarball-name", help="The sources tarball", metavar='TARBALL'\
						, default="", dest="sTarballName")
	oParser.add_argument("--tarball-sha256", help="The sources checksum (Default: SKIP)", metavar='SHA256'\
						, default="SKIP", dest="sTarballSha256")
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
	sChangelogDate = email.utils.format_datetime(oChangelogDate)
	#print("sChangelogDate = " + sChangelogDate)

	#os.chdir(sScriptDirPath)
	#print("sScriptDirPath = " + sScriptDirPath)

	sSubPrjPath = sSourceDirPath
	if g_sSubPrj != "":
		sSubPrjPath = sSubPrjPath + "/" + g_sSubPrj

	sRawPkgName = g_sSourceProjectName + "-" + sDebSrcVersion

	sArchOrigPath = sPrivScriptDirPath + "/arch.orig"

	os.chdir(sParentDirPath)

	os.mkdir(g_sProjectArchDir, 0o775)
	#os.chdir(g_sSourceProjectName)
	aDirOrigFileNames = os.listdir(sArchOrigPath)
	for sEntry in aDirOrigFileNames:
		sEntryPath = sArchOrigPath + "/" + sEntry
		if os.path.isdir(sEntryPath):
			shutil.copytree(sEntryPath, sEntry, copy_function=shutil.copy)
		elif sEntry[-3:] == ".in":
			transformFile(sEntry[:-3], sArchOrigPath, g_sProjectArchDir
						, sPrivScriptDirPath, sSubPrjPath, sDebSrcVersion, nDebRevision, sChangelogDate
						, sAuthorFullName, sAuthorEMail, oArgs.sWebsite, oArgs.sWebsiteSection
						, oArgs.sTarballName, oArgs.sTarballSha256)
		else:
			shutil.copyfile(sEntryPath, g_sProjectArchDir + "/" + sEntry)

if __name__ == "__main__":
	main()

