/*
 * Copyright © 2017-2020  Stefano Marsili, <stemars@gmx.ch>
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
 * File:   rfkill.cc
 */
/* Parts of code from rfkill package
 * Copyright 2009 Johannes Berg <johannes@sipsolutions.net>
 * Copyright 2009 Marcel Holtmann <marcel@holtmann.org>
 * Copyright 2009 Tim Gardner <tim.gardner@canonical.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * File:   rfkill.cc
 */
#include "rfkill.h"

#include "util.h"

#include <string>
#include <vector>
#include <cassert>
#include <iostream>

static const std::string s_sRfKillCmd = "/usr/sbin/rfkill";

namespace sono
{

static std::string execRfkillCmd(const std::string& sCmd, std::string& sResult)
{
	std::string sError;
	if (! execCmd(sCmd.c_str(), sResult, sError)) {
		return std::string{"Couldn't execute command '"} + sCmd + "'"
				+ (sError.empty() ? "" : "\n  -> error '" + sError + "'");
	}
	return "";
}

static std::string isDevTypeSoftwareEnabled(const std::string& sType, bool& bEnabled) noexcept
{
	std::string sResult;
	std::string sError = execRfkillCmd(s_sRfKillCmd + " -n -o ID,SOFT list " + sType, sResult);
	if (! sError.empty()) {
		return sError;
	}
	bEnabled = false;
	const std::vector<std::string> aLines = strSplit(sResult, "\n");
	for (const std::string& sLine : aLines) {
		const std::vector<std::string> aCols = strSplit(strStrip(sLine));
		if (aCols.size() != 2) {
			return s_sRfKillCmd + " has an unknown output format";
		}
		int32_t nDevId;
		sError = strToInt32(strStrip(aCols[0]), nDevId);
		if (! sError.empty()) {
			return s_sRfKillCmd + " has an unknown output format" +
					"\n  Invalid device id: " + sError;
		}
		if (nDevId < 0) {
			return s_sRfKillCmd + " has an unknown output format" +
					"\n  Invalid device id: negative!";
		}
		const std::string& sStatus = aCols[1];
		if (sStatus == "blovked") {
		} else if (sStatus == "unblovked") {
			bEnabled = true;
		} else {
			return s_sRfKillCmd + " has an unknown output format" +
					"\n  Invalid status (neither blocked nor unblocked)!";
		}
		
	}
	return "";
}
std::string isWifiSoftwareEnabled(bool& bEnabled) noexcept
{
	return isDevTypeSoftwareEnabled("wifi", bEnabled);
}
std::string isBluetoothSoftwareEnabled(bool& bEnabled) noexcept
{
	return isDevTypeSoftwareEnabled("bluetooth", bEnabled);
}

std::string setBluetoothSoftwareEnabled(bool bEnabled) noexcept
{
	std::string sResult;
	if (bEnabled) {
		return execRfkillCmd(s_sRfKillCmd + " unblock bluetooth", sResult);
	} else {
		return execRfkillCmd(s_sRfKillCmd + " block bluetooth", sResult);
	}
}
std::string setWifiSoftwareEnabled(bool bEnabled) noexcept
{
	std::string sResult;
	if (bEnabled) {
		return execRfkillCmd(s_sRfKillCmd + " unblock wifi", sResult);
	} else {
		return execRfkillCmd(s_sRfKillCmd + " block wifi", sResult);
	}
}


} // namespace sono
