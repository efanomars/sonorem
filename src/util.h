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
 * File:   util.h
 */

#ifndef SONO_UTIL_H
#define SONO_UTIL_H

#include <string>
#include <vector>

namespace sono
{

/* Create the path if it doesn't exist.
 * @param sPath The path.
 * @return Empty string if no error, otherwise error.
 */
std::string makePath(const std::string& sPath) noexcept;

std::string strStrip(const std::string& sStr) noexcept;

std::vector<std::string> strSplit(const std::string& sStr, const std::string& sSeparator) noexcept;
std::vector<std::string> strSplit(const std::string& sStr) noexcept;

std::string strToInt32(const std::string& sStr, int32_t& nRes) noexcept;

std::string getEnvString(const char* p0Name) noexcept;

bool execCmd(const char* sCmd, std::string& sResult, std::string& sError) noexcept;

} // namespace sono

#endif /* SONO_UTIL_H */
