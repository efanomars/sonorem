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
 * File:   sonodevicemanager.h
 */

#ifndef SONO_SONO_DEVICE_MANAGER_H
#define SONO_SONO_DEVICE_MANAGER_H

#include <stmm-input-base/parentdevicemanager.h>
#include <stmm-input-base/childdevicemanager.h>

#include <string>
#include <vector>
#include <memory>

namespace sono
{

using std::shared_ptr;

class SonoDeviceManager : public stmi::ParentDeviceManager
{
public:
	static bool create(const std::string& sAppName, shared_ptr<SonoDeviceManager>& refSonoDM) noexcept;
protected:
	SonoDeviceManager() = default;
	void init(const std::vector< shared_ptr<ChildDeviceManager> >& aChildDeviceManager) noexcept;
};

} // namespace sono

#endif /* SONO_SONO_DEVICE_MANAGER_H */

