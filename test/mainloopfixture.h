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
 * File:   mainloopfixture.h
 */

#ifndef SONOREM_MAIN_LOOP_FIXTURE_H_
#define SONOREM_MAIN_LOOP_FIXTURE_H_

#include <glibmm.h>

#include <string>
#include <cassert>
#include <iostream>

namespace sono
{
namespace testing
{

class MainLoopFixture
{
public:
	/** Constructor.
	 * Creates Glib::MainLoop.
	 */
	MainLoopFixture()
	{
		m_refML = Glib::MainLoop::create();
	}

	/** Run the main loop until check function returns false.
	 * @param oCF The function to call each nCheckMillisec milliseconds.
	 * @param nCheckMillisec Interval in milliseconds.
	 */
	template<typename CF>
	void run(CF oCF, int32_t nCheckMillisec)
	{
		assert(nCheckMillisec > 0);

		Glib::signal_timeout().connect([&]() -> bool
		{
			const bool bContinue = oCF();
			if (!bContinue) {
				m_refML->quit();
			}
			return bContinue;
		}, nCheckMillisec);
		m_refML->run();
	}
private:
	Glib::RefPtr<Glib::MainLoop> m_refML;
};

} // namespace testing
} // namespace sono

#endif	/* SONOREM_MAIN_LOOP_FIXTURE_H_ */

