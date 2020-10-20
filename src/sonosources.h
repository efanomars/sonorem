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
 * File:   sonosources.h
 */

#ifndef SONO_SONO_SOURCES_H
#define SONO_SONO_SOURCES_H

#include <glibmm.h>

#include <sigc++/sigc++.h>

#include <string>

#include <stdint.h>

namespace sono
{

/* For polling pipe read events */
class PipeInputSource : public Glib::Source
{
public:
	explicit PipeInputSource(int32_t nPipeFD) noexcept;
	// Closes file descriptor on destruction
	virtual ~PipeInputSource() noexcept;

	// A source can have only one callback type, that is the slot given as parameter
	sigc::connection connect(const sigc::slot<void, bool, const std::string&>& oSlot) noexcept;

	inline int32_t getPipeFD() const noexcept { return m_nPipeFD; }
protected:
	bool prepare(int& nTimeout) noexcept override;
	bool check() noexcept override;
	bool dispatch(sigc::slot_base* oSlot) noexcept override;

private:
	int32_t m_nPipeFD;
	std::string m_sErrorStr;
	//
	Glib::PollFD m_oReadPollFD; // The file descriptor is open until destructor is called
	//
	std::string m_sReceiveBuffer;
private:
	PipeInputSource() = delete;
	PipeInputSource(const PipeInputSource& oSource) = delete;
	PipeInputSource& operator=(const PipeInputSource& oSource) = delete;
};

} // namespace sono

#endif /* SONO_SONO_SOURCES_H */
