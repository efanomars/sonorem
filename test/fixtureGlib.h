/*
 * Copyright © 2020  Stefano Marsili, <stemars@gmx.ch>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>
 */
/*
 * File:   fixtureGlib.h
 */

#ifndef SONO_TESTING_FIXTURE_GLIB_H
#define SONO_TESTING_FIXTURE_GLIB_H

#include "fixtureTestBase.h"

#include <gtkmm.h>

#include <iostream>
#include <cassert>

namespace sono
{

namespace testing
{

class GlibFixture : public TestBaseFixture
{
protected:
	void setup() override
	{
		// !!!  Initializing gtk is needed because Glib::init(),
		// !!!  probably also called when creating the main loop,
		// !!!  isn't enough
		try {
			//
			m_refApp = Gtk::Application::create("com.efanomars.sonorem-test-fixture");
		} catch (const std::runtime_error& oErr) {
			std::cout << "Error: " << oErr.what() << '\n';
			assert(false);
			return; //----------------------------------------------------------
		}
		
	}
	void teardown() override
	{
	}
public:
	Glib::RefPtr<Gtk::Application> m_refApp;
};

} // namespace testing

} // namespace sono

#endif /* SONO_TESTING_FIXTURE_GLIB_H */
