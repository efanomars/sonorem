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
 * File:   testWaitingState.cxx
 */

#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

#include "sonomodel.h"
#include "util.h"

#include "mainloopfixture.h"
#include "testutil.h"

#include <fspropfaker/fspropfaker.h>

#include <gtkmm.h>


namespace sono
{

using std::shared_ptr;
using std::unique_ptr;
using std::make_unique;

namespace testing
{

TEST_CASE("Sonorem, SonoModel")
{
	// !!!  Initializing gtk is needed because Glib::init(), probably also called when creating the main loop,  isn't enough !!!
	Glib::RefPtr<Gtk::Application> refApp;
	try {
		//
		refApp = Gtk::Application::create("com.efanomars.sonorem-testWaitingState-SonoModel");
	} catch (const std::runtime_error& oErr) {
		std::cout << "Error: " << oErr.what() << '\n';
		REQUIRE(false);
		return; //--------------------------------------------------------------
	}

	const std::string sMountName = "sonoremtest";
	const std::string sFsFolderPath = "/tmp/sonorem-waits/waits-base";
	const std::string sMountPath = "/tmp/sonorem-waits/waits-mount";
	const std::string sLogFilePath = "/tmp/sonorem-waits/waits.log";
	std::string sResult;
	std::string sError;
	bool bOk = execCmd("rm -rf /tmp/sonorem-waits", sResult, sError);
	if (! bOk) {
		std::cout << "Could not remove folder: " << sError << '\n';
	}
	REQUIRE(bOk);
	makePath(sFsFolderPath);
	if (! sMountPath.empty()) {
		makePath(sMountPath);
	}

// std::cout << "2222 " << '\n';
	auto oResult = fspf::FsPropFaker::create(sMountName, sFsFolderPath, sMountPath, sLogFilePath);
	auto& refFaker = oResult.m_refFaker;
	sError = std::move(oResult.m_sError);
	REQUIRE(refFaker);
	REQUIRE(sError.empty());
//std::cout << "Mount path is " << refFaker->getMountPath() << '\n';

	const auto nBlockSize = refFaker->getBlockSize();

	constexpr int64_t nMegaByte = fspf::FsPropFaker::s_nMegaByteBytes;
	refFaker->setFakeDiskFreeSizeInBlocks(7 * nMegaByte / nBlockSize);
	::sleep(1);

	unique_ptr<SonoModel> refSonoModel;
	auto oInitModel = [&]()
	{
		SonoModel::Init oInit;
		oInit.m_bAutoStart = true;
		oInit.m_bExcludeAllMountNames = true;
		oInit.m_bRfkillBluetoothOff = true;
		oInit.m_bRfkillWifiOff = true;
		oInit.m_sRecordingDirPath = sMountPath;
		oInit.m_nMaxFileSizeBytes = 1000 * nMegaByte;
		oInit.m_nMaxRecordingDurationSeconds = 10;
		oInit.m_nMinFreeSpaceBytes = 10 * nMegaByte;
		refSonoModel = std::make_unique<SonoModel>([](const std::string&){});
		sError = refSonoModel->init(std::move(oInit));
		if (! sError.empty()) {
			std::cout << "Could not create model: " << sError << '\n';
		}
		REQUIRE(sError.empty());
	};
	MainLoopFixture oMainLoop;
	const int32_t nTestIntervalMillisec = 100;
	int32_t nProgress = 0;
	oMainLoop.run([&]() -> bool
	{
		const bool bContinue = true;
		++nProgress;
		if (nProgress == 1) {
			oInitModel();
		} else if (nProgress == 2) {
			REQUIRE(refSonoModel->getState() == SonoModel::STATE_STOPPED);
			refSonoModel->startRecording();
		} else if (nProgress == 3) {
			REQUIRE(refSonoModel->getState() == SonoModel::STATE_WAITING_FOR_SPACE);
		} else if (nProgress == 4) {
			refSonoModel->stopRecording();
		} else if (nProgress == 5) {
			REQUIRE(refSonoModel->getState() == SonoModel::STATE_STOPPED);
			refFaker->setFakeDiskFreeSizeInBlocks(20 * nMegaByte / nBlockSize);
		} else if (nProgress == 6) {
			::sleep(SonoModel::s_nCheckWaitingForFreeSpaceSeconds);
			refSonoModel->startRecording();
		} else if (nProgress == 7) {
			REQUIRE(refSonoModel->getState() == SonoModel::STATE_RECORDING);
		} else {
			refSonoModel.reset();
			sError = refFaker->unmount();
			REQUIRE(sError.empty());
			return ! bContinue;
		}
		return bContinue;
	}, nTestIntervalMillisec);

}

} // namespace testing

} // namespace sono
