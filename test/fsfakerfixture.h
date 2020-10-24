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
 * File:   fsfakerfixture.h
 */

#ifndef SONOREM_FS_FAKER_FIXTURE_H_
#define SONOREM_FS_FAKER_FIXTURE_H_

#include "util.h"
#include "testutil.h"

#include <fspropfaker.h>

#include <memory>
#include <string>
#include <cassert>
#include <iostream>

#include <string.h>
#include <limits.h>
#include <stdlib.h>

namespace sono
{
namespace testing
{

using std::shared_ptr;

class FsFakerFixture
{
public:
	/** Constructor.
	 * Creates fspf::FsPropFaker
	 */
	FsFakerFixture(const std::string& sMountName)
	{
		m_sMountName = sMountName;
		static const char* s_p0Template = "/tmp/fsfakerfixtureXXXXXX";
		char sDirPath[PATH_MAX];
		::strcpy(sDirPath, s_p0Template);
		char* p0DirPath = ::mkdtemp(sDirPath);
		if (p0DirPath == nullptr) {
			std::cout << "Temporary folder " << p0DirPath << " could not be created" << '\n';
			assert(false);
			return;
		}
		m_sBasePath = std::string{p0DirPath};

		const std::string sFsFolderDir = "realfs";
		const std::string sMountDir = "fakemount";
		const std::string sLogFile = "fsfake.log";
		std::string sResult;
		std::string sError;
		bool bOk = execCmd(std::string("rm -rf " + m_sBasePath).c_str(), sResult, sError);
		if (! bOk) {
			std::cout << "Could not remove folder: " << sError << '\n';
		}
		assert(bOk);
		m_sFsFolderPath = m_sBasePath + "/" + sFsFolderDir;
		m_sMountPath = m_sBasePath + "/" + sMountDir;
		m_sLogFilePath = m_sBasePath + "/" + sLogFile;
		makePath(m_sFsFolderPath);
		makePath(m_sMountPath);

		auto oResult = fspf::FsPropFaker::create(m_sMountName, m_sFsFolderPath, m_sMountPath, m_sLogFilePath);
		m_refFaker = std::move(oResult.m_refFaker);
		assert(m_refFaker);
		sError = std::move(oResult.m_sError);
		assert(sError.empty());
	}

	const std::string& getMountName() const
	{
		return m_sMountName;
	}

	const std::string& getRealFsPath() const
	{
		return m_sFsFolderPath;
	}
	const std::string& getFakeFsPath() const
	{
		return m_sMountPath;
	}
	const std::string& getLogFilePath() const
	{
		return m_sLogFilePath;
	}
private:
	std::string m_sMountName;
	std::string m_sBasePath;
	std::string m_sFsFolderPath;
	std::string m_sMountPath;
	std::string m_sLogFilePath;
public:
	shared_ptr<fspf::FsPropFaker> m_refFaker;
};

} // namespace testing
} // namespace sono

#endif	/* SONOREM_FS_FAKER_FIXTURE_H_ */

