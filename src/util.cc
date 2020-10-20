/*
 * Copyright © 2020  Stefano Marsili, <stemars@gmx.ch>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>
 */
/*
 * File:   util.cc
 */

#include "util.h"

#include <string>
#include <iostream>
#include <cassert>
#include <stdexcept>
#include <chrono>

#include <array>
#include <string.h>
#include <stdlib.h>

#include <errno.h>
#include <sys/stat.h>

namespace sono
{

// Modified from https://stackoverflow.com/questions/675039/how-can-i-create-directory-tree-in-c-linux
typedef struct stat Stat;

namespace Private
{
static std::string makeDir(const char* p0Dir) noexcept
{
	Stat oStat;

	if (::stat(p0Dir, &oStat) != 0) {
		// Directory does not exist. EEXIST for race condition.
		const auto nRet = ::mkdir(p0Dir, 0755); // rwx r-x r-x
		if (nRet != 0) {
			if (errno != EEXIST) {
				return std::string{"Error: Could not create directory " + std::string(p0Dir)};
			}
		}
	} else if (!S_ISDIR(oStat.st_mode)) {
		return std::string{"Error: " + std::string(p0Dir) + " not a directory"};
	}
	return "";
}
}
std::string makePath(const std::string& sPath) noexcept
{
	std::string sWorkPath = sPath;
	std::string::size_type nBasePos = 0;
	do {
		const auto nNewPos = sWorkPath.find('/', nBasePos);
		if (nNewPos == std::string::npos) {
			auto sError = Private::makeDir(sWorkPath.c_str());
			if (! sError.empty()) {
				return sError;
			}
			break; // do ------
		} else if (nBasePos != nNewPos) {
			// not root or double slash
			sWorkPath[nNewPos] = '\0';
			auto sError = Private::makeDir(sWorkPath.c_str());
			if (! sError.empty()) {
				return sError;
			}
			sWorkPath[nNewPos] = '/';
		}
		nBasePos = nNewPos + 1;
	} while (true);
	return "";
}

static std::vector<std::string> strSplit(const std::string& sStr, const std::string& sSeparator, bool bRemoveEmpty) noexcept
{
	std::vector<std::string> aTokens;
	std::string sToken;
	std::size_t nCurPos = 0;
	const std::size_t nStrLen = sStr.length();
	const std::size_t nSeparatorLen = sSeparator.length();
	while (nCurPos < nStrLen) {
		const std::size_t nSepPos = [&]()
		{
			if (nSeparatorLen == 0) {
				return sStr.find_first_of(" \t", nCurPos);
			} else {
				return sStr.find(sSeparator, nCurPos);
			}
		}();
		if (nSepPos == std::string::npos) {
			sToken = sStr.substr(nCurPos, nStrLen - nCurPos);
			nCurPos = nStrLen;
		} else {
			sToken = sStr.substr(nCurPos, nSepPos - nCurPos);
			nCurPos = nSepPos + ((nSeparatorLen == 0) ? 1 : nSeparatorLen);
		}
		if (! (bRemoveEmpty && sToken.empty())) {
			aTokens.push_back(sToken);
		}
	}
	return aTokens;
}
std::vector<std::string> strSplit(const std::string& sStr, const std::string& sSeparator) noexcept
{
	return strSplit(sStr, sSeparator, false);
}
std::vector<std::string> strSplit(const std::string& sStr) noexcept
{
	return strSplit(sStr, "", true);
}

std::string strStrip(const std::string& sStr) noexcept
{
	std::string sRes;
	std::string::const_iterator it1 = sStr.begin();
	do {
		if (it1 == sStr.end()) {
			return sRes;
		}
		if (! std::isspace(*it1)) {
			break;
		}
		++it1;
	} while (true);
	std::string::const_iterator it2 = sStr.end();
	do {
		--it2;
		if (! std::isspace(*it2)) {
			break;
		}
	} while (true);

	do {
		sRes.append(1, *it1);
		if (it1 == it2) {
			break;
		}
		++it1;
	} while (true);
	return sRes;
}
std::string strToInt32(const std::string& sStr, int32_t& nRes) noexcept
{
	try {
		nRes = std::stoi(sStr);
	} catch (std::invalid_argument& ) {
		return "Invalid number";
	} catch (std::out_of_range& ) {
		return "Invalid number";
	}
	return "";
}

std::string getEnvString(const char* p0Name) noexcept
{
	const char* p0Value = ::secure_getenv(p0Name);
	std::string sValue{(p0Value == nullptr) ? "" : p0Value};
	return sValue;
}

bool execCmd(const char* sCmd, std::string& sResult, std::string& sError) noexcept
{
	::fflush(nullptr);
	sError.clear();
	sResult.clear();
	std::array<char, 128> aBuffer;
	FILE* pFile = ::popen(sCmd, "r");
	if (pFile == nullptr) {
		sError = std::string("Error: popen() failed: ") + ::strerror(errno) + "(" + std::to_string(errno) + ")";
		return false; //--------------------------------------------------------
	}
	while (!::feof(pFile)) {
		if (::fgets(aBuffer.data(), sizeof(aBuffer), pFile) != nullptr) {
			sResult += aBuffer.data();
		}
	}
	const auto nRet = ::pclose(pFile);
	return (nRet == 0);
}

} // namespace sono
