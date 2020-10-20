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
 * File:   sonosources.cc
 */

#include "sonosources.h"

//#include <cassert>
//#include <iostream>
#include <cstring>

#include <unistd.h>

namespace sono
{

static const constexpr int32_t s_nMaxLineSize = 8192;

PipeInputSource::PipeInputSource(int32_t nPipeFD) noexcept
: m_nPipeFD(nPipeFD)
{
//std::cout << "PipeInputSource::PipeInputSource()" << '\n';
	static_assert(sizeof(int) <= sizeof(int32_t), "");
	static_assert(false == FALSE, "");
	static_assert(true == TRUE, "");
	//
	m_oReadPollFD.set_fd(nPipeFD);
	m_oReadPollFD.set_events(Glib::IO_IN | Glib::IO_HUP | Glib::IO_ERR | Glib::IO_NVAL);
	add_poll(m_oReadPollFD);
	set_can_recurse(false);
}
PipeInputSource::~PipeInputSource() noexcept
{
//std::cout << "PipeInputSource::~PipeInputSource()" << '\n';
	if (m_nPipeFD >= 0) {
		::close(m_nPipeFD);
	}
}
sigc::connection PipeInputSource::connect(const sigc::slot<void, bool, const std::string&>& oSlot) noexcept
{
	if (m_nPipeFD < 0) {
		// Error, return an empty connection
		return sigc::connection();
	}
	return connect_generic(oSlot);
}
bool PipeInputSource::prepare(int& nTimeout) noexcept
{
	nTimeout = -1;

	return false;
}
bool PipeInputSource::check() noexcept
{
	bool bRet = false;

	if ((m_oReadPollFD.get_revents() & (Glib::IO_IN | Glib::IO_HUP | Glib::IO_ERR | Glib::IO_NVAL)) != 0) {
		bRet = true;
	}

	return bRet;
}
bool PipeInputSource::dispatch(sigc::slot_base* p0Slot) noexcept
{
//std::cout << "PipeInputSource::dispatch" << '\n';
	const bool bContinue = true;

	if (p0Slot == nullptr) {
		// Shouldn't happen
		return bContinue;
	}

	const auto nIoEvents = m_oReadPollFD.get_revents();
	const bool bClientHangedUp = ((nIoEvents & Glib::IO_HUP) != 0);
	const bool bPipeError = ((nIoEvents & (Glib::IO_ERR | Glib::IO_NVAL)) != 0);
	if (bPipeError) {
		// some sort of problem ... close pipe
		(*static_cast<sigc::slot<void, bool, const std::string&>*>(p0Slot))(true, "Pipe error");
		return ! bContinue; // -------------------------------------------------
	}
	auto oSendBuffer = [&]()
	{
		if (! m_sReceiveBuffer.empty()) {
			(*static_cast<sigc::slot<void, bool, const std::string&>*>(p0Slot))(false, m_sReceiveBuffer);
		}
	};
	if ((nIoEvents & G_IO_IN) == 0) {
		if (bClientHangedUp) {
			oSendBuffer();
			return ! bContinue; // ---------------------------------------------
		}
		return bContinue; // ---------------------------------------------------
	}
	char aStreamBuf[s_nMaxLineSize];
	const int32_t nBufLen = sizeof aStreamBuf;
	const int32_t nReadLen = ::read(m_oReadPollFD.get_fd(), aStreamBuf, nBufLen);
	if (nReadLen == 0) {
		// EOF: can a pipe cause it?
		oSendBuffer();
		return ! bContinue; //--------------------------------------------------
	} else if (nReadLen < 0) {
		// Since this read originated from a epoll it's not supposed to fail
		return ! bContinue; //--------------------------------------------------
	}
	// find lines
	char* p0CurPos = aStreamBuf;
	int32_t nRestLen = nReadLen;
	do {
		auto p0NL = static_cast<char*>(std::memchr(p0CurPos, '\n', nRestLen));
		if (p0NL == nullptr) {
			m_sReceiveBuffer += std::string(p0CurPos, nRestLen);
			break; // do -----
		}
		//           111111111122222
		// 0123456789012345678901234
		// xxxxxxxxxxxxxxxx0yyyyyyyy
		// nRestLen = 25, nAttach = 16 => nRestLen = 8
		const int32_t nAttach = p0NL - p0CurPos;
		nRestLen = nRestLen - 1 - nAttach;
		m_sReceiveBuffer += std::string(p0CurPos, nAttach);
		//
		oSendBuffer();
		//
		m_sReceiveBuffer.clear();
		p0CurPos = p0NL + 1;
	} while (nRestLen > 0);
	return bContinue;
}


} // namespace sono
