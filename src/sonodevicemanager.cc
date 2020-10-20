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
 * File:   sonodevicemanager.cc
 */

#include "sonodevicemanager.h"

#include <stmm-input-gtk-dm/stmm-input-gtk-dm.h>
#include <stmm-input-gtk-bt/stmm-input-gtk-bt.h>

#include <cassert>
#include <iostream>

namespace sono
{

using std::shared_ptr;

bool SonoDeviceManager::create(const std::string& sAppName, shared_ptr<SonoDeviceManager>& refSonoDM) noexcept
{
	std::vector< shared_ptr<ChildDeviceManager> > aManagers;
	aManagers.reserve(2);

	{
	stmi::GtkDeviceManager::Init oInit;
	oInit.m_sAppName = sAppName;
	oInit.m_bEnablePlugins = true; // disables all plugins, in that it enables only an empty list
	const auto oPairDM = stmi::GtkDeviceManager::create(oInit);
	const shared_ptr<stmi::GtkDeviceManager>& refDM = oPairDM.first;
	if (! refDM) {
		std::cout << "Error: Couldn't create device manager: " << oPairDM.second << '\n';
		return false; //--------------------------------------------------------
	}
	aManagers.push_back(refDM);
	}
	//
	{
	const auto oPairDM = stmi::BtGtkDeviceManager::create(sAppName, false, std::vector<stmi::Event::Class>{});
	const shared_ptr<stmi::BtGtkDeviceManager>& refDM = oPairDM.first;
	if (! refDM) {
		std::cout << "Error: Couldn't create device manager: " << oPairDM.second << '\n';
	} else {
		aManagers.push_back(refDM);
	}
	}
	refSonoDM = shared_ptr<SonoDeviceManager>(new SonoDeviceManager());
	refSonoDM->init(aManagers);
	return true;
}
void SonoDeviceManager::init(const std::vector< shared_ptr<ChildDeviceManager> >& aChildDeviceManager) noexcept
{
	stmi::ParentDeviceManager::init(aChildDeviceManager);
}

} // namespace sono

