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
 * File:   fixtureTestBase.h
 */

#ifndef SONO_TESTING_FIXTURE_TEST_BASE_H
#define SONO_TESTING_FIXTURE_TEST_BASE_H

#include <type_traits>

namespace sono
{

namespace testing
{

class TestBaseFixture
{
public:
	TestBaseFixture() = default;
protected:
	virtual void setup() {}
	virtual void teardown() {}
};

template<class Base>
class STFX : public Base
{
public:
	STFX()
	{
		static_assert(std::is_base_of<TestBaseFixture, Base>::value, "");
		setup();
	}
	~STFX()
	{
		teardown();
	}
protected:
	void setup() override
	{
		Base::setup();
	}
	void teardown() override
	{
		Base::teardown();
	}
};

} // namespace testing

} // namespace sono

#endif /* SONO_TESTING_FIXTURE_TEST_BASE_H */
