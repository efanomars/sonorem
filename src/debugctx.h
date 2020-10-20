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
 * File:   debugctx.h
 */

#ifndef SONO_DEBUG_CTX_H
#define SONO_DEBUG_CTX_H

#include <string>

#include <stdint.h>

namespace sono
{

template< class TOwner >
struct DebugCtx
{
	static std::string getSpaces(int32_t nDepth)
	{
		std::string sSpaces;
		for (int32_t nCur = 0; nCur < nDepth; ++nCur) {
			sSpaces += "  ";
		}
		return sSpaces;
		
	}
	DebugCtx(const TOwner* p0Owner, const std::string& sComment)
	: m_p0Owner(p0Owner)
	, m_sComment(sComment)
	{
		if (m_p0Owner->m_nDebugCtxDepth < 0) {
			return;
		}
		m_p0Owner->m_oLogger("debug - entering: " + getSpaces(m_p0Owner->m_nDebugCtxDepth) + m_sComment);
		++(m_p0Owner->m_nDebugCtxDepth);
	}
	~DebugCtx()
	{
		if (m_p0Owner->m_nDebugCtxDepth < 0) {
			return;
		}
		--(m_p0Owner->m_nDebugCtxDepth);
		m_p0Owner->m_oLogger("debug -  exiting: " + getSpaces(m_p0Owner->m_nDebugCtxDepth) + m_sComment);
	}
private:
	DebugCtx() = delete;
private:
	const TOwner* m_p0Owner;
	std::string m_sComment;
};

} // namespace sono

#endif /* SONO_DEBUG_CTX_H */

