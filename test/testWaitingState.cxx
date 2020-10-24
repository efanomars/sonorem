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

#include "fsfakerfixture.h"
#include "mainloopfixture.h"
#include "testutil.h"
#include "fixtureGlib.h"

#include <fspropfaker/fspropfaker.h>


namespace sono
{

using std::shared_ptr;
using std::unique_ptr;
using std::make_unique;

namespace testing
{

TEST_CASE_METHOD(STFX<GlibFixture>, "SonoModelWaitingState")
{
	FsFakerFixture oFFF("sonoremtest");

	auto& refFaker = oFFF.m_refFaker;
	const auto nBlockSize = refFaker->getBlockSize();

	// if something goes wrong you will probably need to fusermount -u manually
	std::cout << "Mount path is " << refFaker->getMountPath() << '\n';

	constexpr int64_t nMegaByte = fspf::FsPropFaker::s_nMegaByteBytes;
	refFaker->setFakeDiskFreeSizeInBlocks(7 * nMegaByte / nBlockSize);
	::sleep(1);

	class TestSonoModel : public SonoModel
	{
	public:
		using SonoModel::SonoModel;
		using SonoModel::init;
		using SonoModel::matchRecordingFileName;
	};
	std::string sError;
	unique_ptr<TestSonoModel> refSonoModel;
	auto oInitModel = [&]()
	{
		SonoModel::Init oInit;
		oInit.m_bAutoStart = true;
		oInit.m_bExcludeAllMountNames = true;
		oInit.m_bRfkillBluetoothOff = true;
		oInit.m_bRfkillWifiOff = true;
		oInit.m_sRecordingDirPath = oFFF.getFakeFsPath();
		oInit.m_nMaxFileSizeBytes = 1000 * nMegaByte;
		oInit.m_nMaxRecordingDurationSeconds = 10;
		oInit.m_nMinFreeSpaceBytes = 10 * nMegaByte;
		refSonoModel = std::make_unique<TestSonoModel>([](const std::string&){});
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
		} else if (nProgress == 8) {
			::sleep(1); // give time to rec process to start
		} else if (nProgress == 9) {
			::sleep(1); // give time to rec process to start
		} else if (nProgress == 10) {
			refSonoModel->stopRecording();
			::sleep(1); // give time to rec process to finish
		} else if (nProgress == 11) {
			::sleep(1); // give time to rec process to finish
		} else if (nProgress == 12) {
			::sleep(1); // give time to rec process to finish
			REQUIRE(refSonoModel->getNrToBeCopiedRecordings() == 1);
		} else {
			return ! bContinue;
		}
		return bContinue;
	}, nTestIntervalMillisec);

	Glib::Dir oDir(oFFF.getRealFsPath());
	int32_t nGeneratedRecordings = 0;
	for (const auto& sFileName : oDir) {
		const std::string sFilePath = oFFF.getRealFsPath() + "/" + sFileName;
		//std::cout << sFilePath << '\n';
		if (Glib::file_test(sFilePath, Glib::FILE_TEST_IS_DIR)) {
			continue;
		}
		REQUIRE(refSonoModel->matchRecordingFileName(sFileName));
		++nGeneratedRecordings;
	}
	REQUIRE(nGeneratedRecordings == 1);

	refSonoModel.reset();
	sError = refFaker->unmount();
	REQUIRE(sError.empty());
}

} // namespace testing

} // namespace sono
