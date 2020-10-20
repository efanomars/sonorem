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
 * File:   sonomodel.cc
 */

#include "sonomodel.h"

#include "util.h"
#include "rfkill.h"

#include <giomm.h>
#include <glibmm.h>

#include <string>
#include <cassert>
#include <iostream>
#include <memory>
#include <algorithm>
#include <iterator>

#include <signal.h>
#include <wait.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

namespace sono
{

static constexpr int32_t s_nMillionBytes = 1000 * 1000;

static constexpr int32_t s_nMountAfterMillisec = 2000;
static constexpr int32_t s_nCheckWaitingChildMillisec = 500;
static constexpr int32_t s_nCheckToBeCopiedRecordingsSeconds = 17;
static constexpr int32_t s_nCheckToBeSyncedRecordingsSeconds = 19;
static constexpr int32_t s_nCheckToBeRemovedRecordingsSeconds = 15;

static constexpr int32_t s_nCheckRecordingMaxFileSizeSeconds = 11;

static constexpr int32_t s_nUpdateMountsFreeSpaceSeconds = 47;
static constexpr int32_t s_nCheckSonoremQuitFileSeconds = 59;
static constexpr int32_t s_nUnmountAfterStoppedSeconds = 30;

static constexpr double s_fMountFreeSpaceToMaxRecordingSizeRatio = 1.2;

static const std::string s_sMountFileExtTagName = "name";
static const std::string s_sMountFileExtTagFolder = "folder";
static const std::string s_sMountFileExtTagExclude = "excl";
static const std::string s_sFileExtStopRecording = "stop";
static const std::string s_sFileExtStartRecording = "start"; // secret
static const std::string s_sFileExtUnmountNonBusyDirty = "umount"; // secret
static const std::string s_sFileExtTellStatus = "tell"; // secret
static const std::string s_sFileExtQuitProgram = "quit"; // secret
static const std::string s_sFileExtDontRfkillWifi = "wifi";
static const std::string s_sFileExtDontRfkillBluetooth = "bluetooth";

const std::string SonoModel::s_sRecordingProgram = "rec";
const std::string SonoModel::s_sRecordingDefaultFileExt = "ogg";
const int32_t SonoModel::s_nMaxSonoremNameLen = 30;

static const std::string s_sSyncProgram = "sync";

static constexpr int16_t s_nSignalToInterruptChildren = SIGINT;
static constexpr int16_t s_nSignalToTerminateChildren = SIGTERM;


SonoModel::RecordingData::RecordingData(SonoModel* p0This, Glib::Pid&& oPid, int nRecordingCoutFd, int nRecordingCerrFd) noexcept
{
	assert(p0This != nullptr);
	DebugCtx<SonoModel> oCtx(p0This, "RecordingData::RecordingData");

	m_oRecordingPid = std::move(oPid);
	//
	m_nCurrentRecordingSizeBytes = 0;
	m_nCurrentRecordingLastSizeBytes = -1;
	//
	const int32_t nStartCheckingSeconds = std::max(0, p0This->m_oInit.m_nMaxRecordingDurationSeconds - 3);
	//
	m_oRecordingTimedOutConn = Glib::signal_timeout().connect_seconds(
											sigc::mem_fun(*p0This, &SonoModel::checkRecordingTimedOut)
											, nStartCheckingSeconds);
	//
	if (p0This->m_oInit.m_bDebug) {
		m_refRecordingCout = Glib::RefPtr<PipeInputSource>(new PipeInputSource(nRecordingCoutFd));
		m_oRecordingCoutConn = m_refRecordingCout->connect(sigc::mem_fun(*p0This, &SonoModel::onRecordingCout));
		m_refRecordingCout->attach();
		m_refRecordingCerr = Glib::RefPtr<PipeInputSource>(new PipeInputSource(nRecordingCerrFd));
		m_oRecordingCerrConn = m_refRecordingCerr->connect(sigc::mem_fun(*p0This, &SonoModel::onRecordingCerr));
		m_refRecordingCerr->attach();
	}
}
SonoModel::RecordingData::~RecordingData() noexcept
{
	m_oRecordingTimedOutConn.disconnect();
	m_oRecordingCoutConn.disconnect();
	m_oRecordingCerrConn.disconnect();
}
////////////////////////////////////////////////////////////////////////////////
SonoModel::SyncingData::SyncingData(SonoModel* p0This, std::string&& sSyncingFileName, std::string&& sSyncingFilePath
									, Glib::Pid&& oPid, int nSyncingCoutFd, int nSyncingCerrFd) noexcept
: m_sSyncingFileName(std::move(sSyncingFileName))
, m_sSyncingFilePath(std::move(sSyncingFilePath))
, m_oSyncingPid(std::move(oPid))
{
	assert(p0This != nullptr);
	DebugCtx<SonoModel> oCtx(p0This, "SyncingData::SyncingData");
	//
	if (p0This->m_oInit.m_bDebug) {
		m_refSyncingCout = Glib::RefPtr<PipeInputSource>(new PipeInputSource(nSyncingCoutFd));
		m_oSyncingCoutConn = m_refSyncingCout->connect(sigc::mem_fun(*p0This, &SonoModel::onSyncingCout));
		m_refSyncingCout->attach();
		m_refSyncingCerr = Glib::RefPtr<PipeInputSource>(new PipeInputSource(nSyncingCerrFd));
		m_oSyncingCerrConn = m_refSyncingCerr->connect(sigc::mem_fun(*p0This, &SonoModel::onSyncingCerr));
		m_refSyncingCerr->attach();
	}
}
SonoModel::SyncingData::~SyncingData() noexcept
{
	m_oSyncingCoutConn.disconnect();
	m_oSyncingCerrConn.disconnect();
}

////////////////////////////////////////////////////////////////////////////////
SonoModel::~SonoModel() noexcept
{
	if (! m_sCurrentRecordingFilePath.empty()) {
		interruptRecordingProcess();
	}
	if (m_refAsyncCopyCancellable) {
		m_refAsyncCopyCancellable->cancel();
	}
	if (m_refAsyncRemoveCancellable) {
		m_refAsyncRemoveCancellable->cancel();
	}
	if (m_refAsyncUnmountCancellable) {
		m_refAsyncUnmountCancellable->cancel();
	}
	for (auto& oPair : m_aWaitingRecPids) {
		::waitpid(oPair.first, nullptr, 0);
	}
	if (m_refSyncingData) {
		::waitpid(m_refSyncingData->m_oSyncingPid, nullptr, 0);
	}
	if (! m_sSyncingMountRootPath.empty()) {
		maybeTerminateSyncingProcess();
	}
}

std::function<void(const std::string&)>& SonoModel::getLogger() noexcept
{
	return m_oLogger;
}

static bool isUniqueInstance() noexcept
{
	const std::string sCmd = "ps -eo pid,comm | grep \" sonorem\" | grep -v " + std::to_string(::getpid());
	std::string sResult;
	std::string sError;
	if (execCmd(sCmd.c_str(), sResult, sError)) {
		sResult = strStrip(sResult);
		if ((! sResult.empty()) && (sResult.substr(sResult.size() - 1 - 7) == " sonorem")) {
			return false; //----------------------------------------------------
		}
	}
	return true;
}
void SonoModel::pickupLeftoverToBeCopiedRecordings() noexcept
{
	Glib::Dir oDir(m_oInit.m_sRecordingDirPath);
	for (const auto& sFileName : oDir) {
		const std::string sFilePath = m_oInit.m_sRecordingDirPath + "/" + sFileName;
		if (Glib::file_test(sFilePath, Glib::FILE_TEST_IS_DIR)) {
			continue;
		}
		if (! matchRecordingFileName(sFileName)) {
			continue;
		}
		if (m_oInit.m_bVerbose) {
			m_oLogger("Picked up leftover recording: " + sFilePath);
		}
		m_aToBeCopiedRecordings.push_back(sFilePath);
	}
}
int64_t SonoModel::getFileSizeBytes(const std::string& sPath) const noexcept
{
	auto refFile = Gio::File::create_for_path(sPath);
	return getFileSizeBytes(*(refFile.operator->()));
}
int64_t SonoModel::getFileSizeBytes(Gio::File& oFile) const noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::getFileSizeBytes");

	Glib::RefPtr<Gio::FileInfo> refFileInfo;
	try {
		refFileInfo = oFile.query_info(G_FILE_ATTRIBUTE_STANDARD_SIZE);
	} catch (const Gio::Error& oErr) {
		m_oLogger("Failed query_info: " + oErr.what());
		return -1; //-----------------------------------------------------------
	}
	//std::uint64_t n64FreeB = refFileInfo->get_size();
	auto sFree = refFileInfo->get_attribute_as_string(G_FILE_ATTRIBUTE_STANDARD_SIZE);
	std::uint64_t n64FreeB;
	try {
		n64FreeB = std::stoull(sFree);
	} catch (const std::invalid_argument& /*oErr*/) {
		n64FreeB = refFileInfo->get_attribute_uint64(G_FILE_ATTRIBUTE_STANDARD_SIZE);
	} catch (const std::out_of_range& /*oErr*/) {
		assert(false);
		return -1; //-----------------------------------------------------------
	}
	return static_cast<int64_t>(n64FreeB);
}

int64_t SonoModel::getFsFreeMB(const std::string& sPath) const noexcept
{
	auto refFile = Gio::File::create_for_path(sPath);
	return getFsFreeMB(*(refFile.operator->()));
}
int64_t SonoModel::getFsFreeMB(Gio::File& oFile) const noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::getFsFreeMB");

	Glib::RefPtr<Gio::FileInfo> refFileInfo;
	try {
		refFileInfo = oFile.query_filesystem_info(G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
	} catch (const Gio::Error& oErr) {
		m_oLogger("Failed query_filesystem_info: " + oErr.what());
		return -1; //-----------------------------------------------------------
	}
	if (! refFileInfo) {
		const std::string sErr = "Failed query_filesystem_info: try creating a Gtk::Application object"
								"\ninstead of Glib::init() which apparently isn't enough";
		std::cerr << sErr << '\n';
		m_oLogger(sErr);
		return -1; //-----------------------------------------------------------
	}
	auto sFree = refFileInfo->get_attribute_as_string(G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
	std::uint64_t n64FreeB;
	try {
		n64FreeB = std::stoull(sFree);
	} catch (const std::invalid_argument& /*oErr*/) {
		n64FreeB = refFileInfo->get_attribute_uint64(G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
	} catch (const std::out_of_range& /*oErr*/) {
		assert(false);
		return -1; //-----------------------------------------------------------
	}
	const int64_t nFreeMB = n64FreeB / s_nMillionBytes;
	return nFreeMB;
}

static bool strIsSonoremName(const std::string& sStr) noexcept
{
	if (sStr.find_first_not_of("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_") != std::string::npos) {
		return false;
	}
	if (sStr.empty()) {
		return false;
	}
	if ((sStr[0] == '-') || (sStr[0] == '_')) {
		return false;
	}
	const auto nSize = sStr.size();
	if (nSize > SonoModel::s_nMaxSonoremNameLen) {
		return false;
	}
	if ((sStr[nSize - 1] == '-') || (sStr[nSize - 1] == '_')) {
		return false;
	}
	return true;
}
static bool strIsSonoremFolder(const std::string& sStr) noexcept
{
	return strIsSonoremName(sStr);
}
static bool strIsUUID(const std::string& sStr) noexcept
{
	//           1         2         3
	// 0123456789012345678901234567890123456789
	// 1a773d12-3afd-4cf6-b8ab-601d5e3281e2
	const int32_t nSize = static_cast<int32_t>(sStr.size());
	if (nSize != 8 + 1 + 4 + 1 + 4 + 1 + 4 + 1 + 12) {
		return false;
	}
	for (int32_t nIdx = 0; nIdx < nSize; ++nIdx) {
		const auto c = sStr[nIdx];
		if ((nIdx == 8) || (nIdx == 13) || (nIdx == 18) || (nIdx == 23)) {
			if (c != '-') {
				return false;
			}
		} else {
			if ((c >= '0') && (c <= '9')) {
			} else if ((c >= 'A') && (c <= 'F')) {
			} else if ((c >= 'a') && (c <= 'f')) {
			} else {
				return false;
			}
		}
	}
	return true;
}
static std::string getMountUUID(Gio::Mount& oMount) noexcept
{
//std::cout << "getMountUUID" << '\n';
	std::string sUUID = oMount.get_uuid();
	if (! sUUID.empty()) {
		return sUUID; //--------------------------------------------------------
	}
	auto refVolume = oMount.get_volume();
	if (refVolume) {
		sUUID = refVolume->get_uuid();
		if (! sUUID.empty()) {
			return sUUID; //----------------------------------------------------
		}
	}
	auto refRootFile = oMount.get_root();
	if (! refRootFile) {
		return ""; //-----------------------------------------------------------
	}
	const std::string sRootPath = refRootFile->get_path();
	const std::string sBaseName = Glib::filename_display_basename(sRootPath);
	if (! strIsUUID(sBaseName)) {
		return ""; //-----------------------------------------------------------
	}
	return sBaseName;
}
bool SonoModel::isAutoStart() const noexcept
{
	return m_oInit.m_bAutoStart;
}
bool SonoModel::isMountExcluded(Gio::Mount& oMount, const std::string& sName, const std::string& sRootPath) noexcept
{
	DebugCtx<SonoModel> oCtx(this, " SonoModel::isMountExcluded");

	if (m_oInit.m_bExcludeAllMountNames) {
		return true;
	}
	if (oMount.is_shadowed()) {
		return true;
	}
	if (sonoremFileExists(sRootPath, s_sMountFileExtTagExclude)) {
		return true;
	}
	const auto itFind = std::find(m_oInit.m_aExclMountNames.begin(), m_oInit.m_aExclMountNames.end(), sName);
	if (itFind != m_oInit.m_aExclMountNames.end()) {
		return true;
	}
	// ex. sRootPath = "/"          m_sRecordingDirPath="/home/pinco/Downloads" is subpath
	// ex. sRootPath = "/media/xx"  m_sRecordingDirPath="/home/pinco/Downloads" is not subpath
	// ex. sRootPath = "/home"      m_sRecordingDirPath="/home/pinco/Downloads" is a better subpath
	const int32_t nRootPathLen = static_cast<int32_t>(sRootPath.size());
	if (m_oInit.m_sRecordingDirPath.substr(0, nRootPathLen) == sRootPath) {
		// it is a sub-path: skip
		return true;
	}

	auto refDrive = oMount.get_drive();
	if ((! refDrive) || (! refDrive->is_removable()) || (! refDrive->is_media_removable())) {
		// not a usb stick
		return true;
	}
	return false;
}
void SonoModel::getMountOverridables(const std::string& sRootPath, std::string& sName, std::string& sFolder) noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::getMountOverridables");

	const std::string sNameFilePath = sRootPath + "/sonorem." + s_sMountFileExtTagName;
	if (Glib::file_test(sNameFilePath, Glib::FILE_TEST_EXISTS)) {
		const std::string sCmd = "cat " + sNameFilePath;
		std::string sResult;
		std::string sError;
		if (execCmd(sCmd.c_str(), sResult, sError)) {
			sResult = strStrip(sResult);
			if (strIsSonoremName(sResult)) {
				m_oLogger("Renamed mount '" + sName + "' to '" + sResult + "'");
				sName = sResult;
			} else if (m_oInit.m_bVerbose) {
				m_oLogger("Renaming mount error: invalid characters");
			}
		}
	}
	const std::string sFolderFilePath = sRootPath + "/sonorem." + s_sMountFileExtTagFolder;
	if (Glib::file_test(sFolderFilePath, Glib::FILE_TEST_EXISTS)) {
		const std::string sCmd = "cat " + sFolderFilePath;
		std::string sResult;
		std::string sError;
		if (execCmd(sCmd.c_str(), sResult, sError)) {
			sResult = strStrip(sResult);
			if (strIsSonoremFolder(sResult)) {
				sFolder = sResult;
				const std::string sFolderPath = sRootPath + "/" + sFolder;
				if (Glib::file_test(sFolderPath, Glib::FILE_TEST_EXISTS)
						&& Glib::file_test(sFolderPath, Glib::FILE_TEST_IS_DIR)) {
					m_oLogger("Mount folder of '" + sName + "' is '" + sFolder + "/'");
				} else {
					m_oLogger("Setting mount folder error: " + sFolderPath + " not found or not a directory");
				}
			} else if (m_oInit.m_bVerbose) {
				m_oLogger("Setting mount folder error: invalid characters");
			}
		}
	}
}
bool SonoModel::sonoremFileExists(const std::string& sRootPath, const std::string& sFileExt) noexcept
{
	return Glib::file_test(sRootPath + "/sonorem." + sFileExt, Glib::FILE_TEST_EXISTS);
}

std::string SonoModel::init(Init&& oInit) noexcept
{
	if (oInit.m_bDebug) {
		m_nDebugCtxDepth = 0;
	}

	DebugCtx<SonoModel> oCtx(this, "SonoModel::init");

	assert(! oInit.m_sRecordingDirPath.empty());
	assert(m_oInit.m_sRecordingDirPath.empty()); // Called more than once
	//
	if (! isUniqueInstance()) {
		const std::string sErr = "An instance of sonorem is already running";
		return sErr; //---------------------------------------------------------
	}
	//
	m_oInit = std::move(oInit);

	m_nRecordingFsFreeMB = getFsFreeMB(m_oInit.m_sRecordingDirPath);

	pickupLeftoverToBeCopiedRecordings();
	//
	m_refVolumeMonitor = Gio::VolumeMonitor::get();
	//
	m_refVolumeMonitor->signal_mount_added().connect(
								sigc::mem_fun(*this, &SonoModel::onMountAdded) );
	m_refVolumeMonitor->signal_mount_removed().connect(
								sigc::mem_fun(*this, &SonoModel::onMountRemoved) );
    m_refVolumeMonitor->signal_mount_changed().connect(
								sigc::mem_fun(*this, &SonoModel::onMountChanged) );
	//
	m_refVolumeMonitor->signal_volume_added().connect(
								sigc::mem_fun(*this, &SonoModel::onVolumeAdded) );
	m_refVolumeMonitor->signal_volume_removed().connect(
								sigc::mem_fun(*this, &SonoModel::onVolumeRemoved) );
	//
	std::vector<Glib::RefPtr<Gio::Mount>> aMounts = m_refVolumeMonitor->get_mounts();
	for (auto& refMount : aMounts) {
		Gio::Mount& oMount = *(refMount.operator->());
		auto refRootFile = oMount.get_root();
		std::string sRootPath = refRootFile->get_path();
		std::string sName = oMount.get_name();
		std::string sUUID = getMountUUID(oMount);
		std::string sFolder;

		auto oOverrideOptionToFalse = [&](bool& bOption, const std::string& sOption, const std::string& sFileExt, const std::string& sComment)
		{
			if (bOption && sonoremFileExists(sRootPath, sFileExt)) {
				m_oLogger("Overriding " + sOption + " option: " + sComment);
				bOption = false;
			}
		};
		oOverrideOptionToFalse(m_oInit.m_bAutoStart, "--auto", s_sFileExtStopRecording, "no autostart!");
		//
		auto oForcingOption = [&](bool& bOption, const std::string& sOption, const std::string& sFileExt, const std::string& sComment)
		{
			if ((! bOption) && sonoremFileExists(sRootPath,  sFileExt)) {
				m_oLogger("Forcing " + sOption + " option: " + sComment);
				bOption = true;
			}
		};
		oForcingOption(m_oInit.m_bRfkillWifiOn, "--wifi-on", s_sFileExtDontRfkillWifi, "starting wifi!");
		oForcingOption(m_oInit.m_bRfkillBluetoothOn, "--bluetooth-on", s_sFileExtDontRfkillBluetooth, "starting bluetooth!");
		//
		m_oLogger("Found mount with root path: " + sRootPath + " (name: " + sName + ")");
		//
		getMountOverridables(sRootPath, sName, sFolder);
		//
		if (isMountExcluded(oMount, sName, sRootPath)) {
			m_oLogger("       EXCLUDED!");
			continue;
		}
		const int64_t nMountFreeSpace = getFsFreeMB(*(refRootFile.operator->()));
		if (nMountFreeSpace < 0) {
			m_oLogger("  Could not get free space of fs: " + sRootPath);
			m_oLogger("       EXCLUDED!");
			continue;
		}
		m_aMountInfos.emplace_back();
		MountInfo& oMountInfo = m_aMountInfos.back();
		oMountInfo.m_sName = std::move(sName);
		oMountInfo.m_sFolder = std::move(sFolder);
		oMountInfo.m_sRootPath = std::move(sRootPath);
		oMountInfo.m_sUUID = std::move(sUUID);
		oMountInfo.m_nFreeMB = nMountFreeSpace;
	}
	//
	sortMounts();
	//
	if (m_oInit.m_bVerbose) {
		m_oLogger("Initialized model");
		m_oLogger("  Max. recording file duration (seconds): " + std::to_string(m_oInit.m_nMaxRecordingDurationSeconds ));
		m_oLogger("  Max. recording file size (bytes):       " + std::to_string(m_oInit.m_nMaxFileSizeBytes));
		m_oLogger("  Min. free space on main disk (bytes):   " + std::to_string(m_oInit.m_nMinFreeSpaceBytes));
	}
	//
	m_sSonoremQuitFilePath = m_oInit.m_sRecordingDirPath + "/sonorem." + s_sFileExtQuitProgram;
	//
	Glib::signal_timeout().connect(sigc::mem_fun(*this, &SonoModel::checkWaitingChild), s_nCheckWaitingChildMillisec);
	Glib::signal_timeout().connect_seconds(sigc::mem_fun(*this, &SonoModel::checkToBeCopiedRecordings)
										, std::min(s_nCheckToBeCopiedRecordingsSeconds, m_oInit.m_nMaxRecordingDurationSeconds));
	Glib::signal_timeout().connect_seconds(sigc::mem_fun(*this, &SonoModel::checkToBeSyncedRecordings)
										, std::min(s_nCheckToBeSyncedRecordingsSeconds, m_oInit.m_nMaxRecordingDurationSeconds));
	Glib::signal_timeout().connect_seconds(sigc::mem_fun(*this, &SonoModel::checkToBeRemovedRecordings)
										, std::min(s_nCheckToBeRemovedRecordingsSeconds, m_oInit.m_nMaxRecordingDurationSeconds));
	// The following also updates m_nCurrentRecordingSizeBytes
	Glib::signal_timeout().connect_seconds(sigc::mem_fun(*this, &SonoModel::checkRecordingMaxFileSize), s_nCheckRecordingMaxFileSizeSeconds);
	//
	Glib::signal_timeout().connect_seconds(sigc::mem_fun(*this, &SonoModel::updateMountsFreeSpace), s_nUpdateMountsFreeSpaceSeconds);
	//
	Glib::signal_timeout().connect_seconds(sigc::mem_fun(*this, &SonoModel::checkSonoremQuitFile), s_nCheckSonoremQuitFileSeconds);

	std::string sError;
	if (m_oInit.m_bRfkillWifiOn) {
		sError = setWifiSoftwareEnabled(true);
	} else if (m_oInit.m_bRfkillWifiOff) {
		sError = setWifiSoftwareEnabled(false);
	}
	if (m_oInit.m_bRfkillBluetoothOn) {
		sError = setBluetoothSoftwareEnabled(true);
	} else if (m_oInit.m_bRfkillBluetoothOff) {
		sError = setBluetoothSoftwareEnabled(false);
	}
	if (! sError.empty()) {
		m_oLogger(sError);
	}
	return "";
}
SonoModel::STATE SonoModel::getState() const noexcept
{
	return m_eState;
}

const std::string& SonoModel::getRecordingDirPath() const noexcept
{
	return m_oInit.m_sRecordingDirPath;
}
int64_t SonoModel::getRecordingFsFreeMB() const noexcept
{
	return m_nRecordingFsFreeMB;
}
const std::string& SonoModel::getRecordingFileExt() const noexcept
{
	return m_oInit.m_sRecordingFileExt;
}

void SonoModel::onVolumeAdded(const Glib::RefPtr<Gio::Volume>& refVolume) noexcept
{
//std::cout << "Volume added: " << refVolume->get_name() << '\n';
//std::cout << "        UUID: " << refVolume->get_uuid() << '\n';
//if (refVolume->get_activation_root()) {
//std::cout << "        root: " << refVolume->get_activation_root()->get_path() << '\n';
//}
	Glib::signal_timeout().connect_once(sigc::bind(
					sigc::mem_fun(*this, &SonoModel::onVolumeAddedOut), refVolume.operator->()), s_nMountAfterMillisec);
}
void SonoModel::onVolumeAddedOut(Gio::Volume* p0Volume) noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::onVolumeAddedOut");

	std::vector<Glib::RefPtr<Gio::Volume>> aVolumes = m_refVolumeMonitor->get_volumes();
	for (auto& refVolume : aVolumes) {
		if (refVolume.operator->() == p0Volume) {
			if (! refVolume->get_mount()) {
				refVolume->mount();
			}
		}
	}
}
void SonoModel::onVolumeRemoved(const Glib::RefPtr<Gio::Volume>& refVolume) noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::onVolumeRemoved");

	auto refMount = refVolume->get_mount();
	if (! refMount) {
		return;
	}
	auto refRootFile = refMount->get_root();
	const std::string sRootPath = refRootFile->get_path();
	if (sRootPath == m_sUnmountingMountPath) {
		assert(m_refAsyncUnmountCancellable);
		m_oLogger("Canceling unmount of " + m_sUnmountingMountPath);
		m_refAsyncUnmountCancellable->cancel();
		m_refAsyncUnmountCancellable.reset();
	}
}

void SonoModel::onMountAdded(const Glib::RefPtr<Gio::Mount>& refMount) noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::onMountAdded");

	Gio::Mount& oMount = *(refMount.operator->());
	auto refRootFile = oMount.get_root();
	std::string sName = oMount.get_name();
	std::string sUUID = getMountUUID(oMount);
	std::string sFolder;
	std::string sRootPath = refRootFile->get_path();

	m_oLogger("Mount added: " + sRootPath);
	if (! sUUID.empty()) {
		m_oLogger("       UUID: " + sRootPath);
	}
	//
	bool bQuitting = false;
	if (sonoremFileExists(sRootPath, s_sFileExtQuitProgram)) {
		bQuitting = true;
	} else if (sonoremFileExists(sRootPath, s_sFileExtTellStatus)) {
		m_oTellStatusSignal.emit();
	}
	bool bStopped = false;
	if ((m_eState != STATE_STOPPED)
			&& (bQuitting || sonoremFileExists(sRootPath, s_sFileExtStopRecording))) {
		//
		stopRecording();
		//
		m_oLogger("Stopped recording because detected sonorem." + (bQuitting ? s_sFileExtQuitProgram : s_sFileExtStopRecording)
					+ "\n  on mount " + sRootPath);
		bStopped = true;
	}
	if ((! bQuitting) && (! bStopped) && (m_eState != STATE_RECORDING)
				&& (sonoremFileExists(sRootPath, s_sFileExtStartRecording))) {
		startRecording();
	}
	if (sonoremFileExists(sRootPath, s_sFileExtUnmountNonBusyDirty)) {
		m_oLogger("Unmounting because detected sonorem." + s_sFileExtUnmountNonBusyDirty
					+ (bStopped ? "\n  in " + std::to_string(s_nUnmountAfterStoppedSeconds) + " seconds" : ""));
		// if stopped or quitting wait a little bit for the recording to possibly be moved to mount
		Glib::signal_timeout().connect_seconds_once(sigc::mem_fun(*this, &SonoModel::unmountNonBusy), bStopped ? s_nUnmountAfterStoppedSeconds : 0);
	}
	//
	if (bQuitting) {
		Glib::signal_timeout().connect_seconds_once(sigc::mem_fun(*this, &SonoModel::sonoremQuit), s_nCheckSonoremQuitFileSeconds);
	}
	//
	getMountOverridables(sRootPath, sName, sFolder);
	//
	if (isMountExcluded(oMount, sName, sRootPath)) {
		m_oLogger("       EXCLUDED!");
		return; //--------------------------------------------------------------
	}

//std::cout << "Mount added: " << refMount->get_name() << '\n';
//std::cout << "       UUID: " << sUUID << '\n';
	const int64_t nMountFreeSpace =  getFsFreeMB(*(refRootFile.operator->()));
	if (nMountFreeSpace < 0) {
		m_oLogger("  Could not get free space of fs: " + sRootPath);
		m_oLogger("       EXCLUDED!");
		return; //--------------------------------------------------------------
	}

	m_aMountInfos.emplace_back();
	MountInfo& oMountInfo = m_aMountInfos.back();
	oMountInfo.m_sRootPath = std::move(sRootPath);
	oMountInfo.m_sName = std::move(sName);
	oMountInfo.m_sFolder = std::move(sFolder);
	oMountInfo.m_sUUID = std::move(sUUID);

	oMountInfo.m_nFreeMB = nMountFreeSpace;
	//
	sortMounts();
	//
	m_oMountsChangedSignal.emit();
}
void SonoModel::sortMounts() noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::sortMounts");

	if (m_aMountInfos.empty()) {
		return;
	}
	auto itBegin = [&]()
	{
		if (m_sCopyingToMountRootPath.empty()) {
			return m_aMountInfos.begin();
		} else {
			assert(m_aMountInfos[0].m_sRootPath == m_sCopyingToMountRootPath);
			return ++m_aMountInfos.begin();
		}
	}();
	std::sort(itBegin, m_aMountInfos.end(), [&](const MountInfo& oMI1, const MountInfo& oMI2)
	{
		if (oMI1.m_bUnmounting != oMI2.m_bUnmounting) {
			// favor non unmounting mounts
			return oMI2.m_bUnmounting;
		}
		if (oMI1.m_nFailedCopyAttempts != oMI2.m_nFailedCopyAttempts) {
			// favor mounts with no failed copy attempts
			return (oMI1.m_nFailedCopyAttempts < oMI2.m_nFailedCopyAttempts);
		}
		if (oMI1.m_bDirty != oMI2.m_bDirty) {
			// favor the dirty mount if it still has enough space
			if (oMI1.m_bDirty) {
				return oMI1.m_nFreeMB * s_nMillionBytes >= m_oInit.m_nMaxFileSizeBytes * s_fMountFreeSpaceToMaxRecordingSizeRatio;
			} else {
				return oMI2.m_nFreeMB * s_nMillionBytes >= m_oInit.m_nMaxFileSizeBytes * s_fMountFreeSpaceToMaxRecordingSizeRatio;
			}
		}
		return (oMI1.m_nFreeMB > oMI2.m_nFreeMB);
	});
}
void SonoModel::onMountRemoved(const Glib::RefPtr<Gio::Mount>& refMount) noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::onMountRemoved");

//std::cout << "Mount removed: " << refMount->get_name() << '\n';
//std::cout << "         UUID: " << getMountUUID(*(refMount.operator->())) << '\n';
	const std::string sUUID = getMountUUID(*(refMount.operator->()));
	auto refRootFile = refMount->get_root();
	const std::string sRootPath = refRootFile->get_path();
	m_oLogger("Mount removed: " + sRootPath);
	if (! sUUID.empty()) {
		m_oLogger("         UUID: " + sUUID);
	}
	const int32_t nMountIdx = getMountIdxFromRootPath(sRootPath);
	if (nMountIdx < 0) {
		return; //----------------------------------------------------
	}
	if (sRootPath == m_sCopyingToMountRootPath) {
		assert(m_refAsyncCopyCancellable);
		m_oLogger("Canceling copying of " + m_sCopyingFileName);
		m_refAsyncCopyCancellable->cancel();
		m_refAsyncCopyCancellable.reset();
	}
	if (sRootPath == m_sSyncingMountRootPath) {
		// do nothing, hopefully the sync process will exit
		// with an error or a signal without hanging
		// so that checkToBeSyncedRecordings() can catch it
	}
	m_aMountInfos.erase(m_aMountInfos.begin() + nMountIdx);
	m_oMountsChangedSignal.emit();
}
void SonoModel::onMountChanged(const Glib::RefPtr<Gio::Mount>& refMount) noexcept
{
//std::cout << "Mount changed: " << refMount->get_name() << '\n';
//std::cout << "         UUID: " << getMountUUID(*(refMount.operator->())) << '\n';
	const std::string sUUID = getMountUUID(*(refMount.operator->()));
	auto refRootFile = refMount->get_root();
	const std::string sRootPath = refRootFile->get_path();
	m_oLogger("Mount changed: " + sRootPath);
	if (! sUUID.empty()) {
		m_oLogger("         UUID: " + sUUID);
	}
	m_oMountsChangedSignal.emit();
}
bool SonoModel::updateMountsFreeSpace() noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::updateMountsFreeSpace");

	const bool bContinue = true;
	bool bChanged = false;
	for (auto itMountInfo = m_aMountInfos.begin(); itMountInfo != m_aMountInfos.end(); ++itMountInfo) {
		auto& oMountInfo = *itMountInfo;
		if (oMountInfo.m_bUnmounting) {
			continue;
		}
		if (oMountInfo.m_sRootPath == m_sCopyingToMountRootPath) {
			// don't disturb the copying
			continue;
		}
		const int64_t nFreeMB = getFsFreeMB(oMountInfo.m_sRootPath);
		if (nFreeMB != oMountInfo.m_nFreeMB) {
			oMountInfo.m_nFreeMB = nFreeMB;
			bChanged = true;
		}
	}
	if (bChanged) {
		m_oMountsChangedSignal.emit();
	}
	return bContinue;
}
const std::vector<SonoModel::MountInfo>& SonoModel::getMountInfos() const noexcept
{
	return m_aMountInfos;
}
bool SonoModel::recordingFsHasFreeSpace() noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::recordingFsHasFreeSpace");

	m_nRecordingFsFreeMB = getFsFreeMB(m_oInit.m_sRecordingDirPath);
//std::cout << "startRecording() m_nRecordingFsFreeMB = " << m_nRecordingFsFreeMB << '\n';
	if (m_nRecordingFsFreeMB * s_nMillionBytes < m_oInit.m_nMinFreeSpaceBytes) {
		// change state to waiting for space
		m_oWaitingForFreeSpaceConn = Glib::signal_timeout().connect_seconds(sigc::mem_fun(*this
											, &SonoModel::checkWaitingForFreeSpace), s_nCheckWaitingForFreeSpaceSeconds);
		m_oLogger("Not enough free space for recording.");
		if (m_aToBeCopiedRecordings.empty()) {
			m_oLogger("Please free some memory first.");
		} else {
			m_oLogger("Insert a usb memory stick to free space.");
		}
		m_eState = STATE_WAITING_FOR_SPACE;
		m_oStateChangedSignal.emit();
		return false; //--------------------------------------------------------
	}
	return true;
}

void SonoModel::startRecording() noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::startRecording");

	if (m_eState == STATE_RECORDING) {
		m_oLogger("Already recording");
		return; //--------------------------------------------------------------
	}
	if (m_eState == STATE_WAITING_FOR_SPACE) {
		m_oLogger("Recording as soon as space freed");
		return; //--------------------------------------------------------------
	}
	// Check there is enough space on disk
	if (! recordingFsHasFreeSpace()) {
		assert(m_eState == STATE_WAITING_FOR_SPACE);
		return; //--------------------------------------------------------------
	}

	if (! launchRecordingProcess()) {
		return; //--------------------------------------------------------------
	}

	m_oLogger("Started recording to " + m_sCurrentRecordingFilePath);
	m_oStartedRecordingTime = Glib::DateTime::create_now_local();
	m_eState = STATE_RECORDING;
	m_oStateChangedSignal.emit();
}
std::string SonoModel::getNowString() noexcept
{
	Glib::DateTime oNow = Glib::DateTime::create_now_local();
	// 20200721-150854
	return oNow.format("%Y%m%d-%H%M%S");
}
std::string SonoModel::getRecordingFileName(const std::string& sNow) noexcept
{
	return m_oInit.m_sPreString + std::move(sNow) + "." + m_oInit.m_sRecordingFileExt;
}
bool SonoModel::matchRecordingFileName(const std::string& sFileName) noexcept
{
	// pre20200721-150854.ogg
	const auto nFileNameSize = sFileName.size();
	const auto nPreSize = m_oInit.m_sPreString.size();
	const auto nFileExtSize = m_oInit.m_sRecordingFileExt.size();
	if (nFileNameSize != nPreSize + 15 + 1 + nFileExtSize) {
		return false;
	}
	if (sFileName.substr(0, nPreSize) != m_oInit.m_sPreString) {
		return false;
	}
	if (sFileName.substr(nFileNameSize - nFileExtSize) != m_oInit.m_sRecordingFileExt) {
		return false;
	}
	if (sFileName[nPreSize + 8] != '-') {
		return false;
	}
	if (sFileName[nPreSize + 15] != '.') {
		return false;
	}
	for (int32_t nIdx = 0; nIdx < 8; ++nIdx) {
		auto c = sFileName[nPreSize + nIdx];
		if ((c < '0') || (c > '9')) {
			return false;
		}
	}
	for (int32_t nIdx = 8 + 1; nIdx < 8 + 1 + 6; ++nIdx) {
		auto c = sFileName[nPreSize + nIdx];
		if ((c < '0') || (c > '9')) {
			return false;
		}
	}
	return true;
}
std::string SonoModel::getShortNowString() noexcept
{
	Glib::DateTime oNow = Glib::DateTime::create_now_local();
	return oNow.format("%H%M%S");
}
std::string SonoModel::getDurationInSecondsAsString(int64_t nDurationSeconds) noexcept
{
	const int64_t nHours = nDurationSeconds / 60 / 60;
	const int64_t nMinutes = nDurationSeconds / 60 - nHours * 60;
	const int64_t nSeconds = nDurationSeconds - nHours * 60 * 60 - nMinutes * 60;
	std::string sRes = std::to_string(nHours);
	sRes += ":";
	if (nMinutes < 10) {
		sRes += "0";
	}
	sRes += std::to_string(nMinutes);
	sRes += ":";
	if (nSeconds < 10) {
		sRes += "0";
	}
	sRes += std::to_string(nSeconds);
	return sRes;
}
bool SonoModel::launchRecordingProcess() noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::launchRecordingProcess");

	static int32_t s_nCounter = 0;
	++s_nCounter;
	const std::string sNow = getNowString();
	const std::string sFile = getRecordingFileName(sNow);
	const std::string sCurrentRecordingFilePath = m_oInit.m_sRecordingDirPath + "/" + sFile;
	//
	std::vector<std::string> aArgv;
	aArgv.reserve(5);
	aArgv.push_back(s_sRecordingProgram); // TODO create a fake-rec that simulates rec by increasing file size very quickly
	aArgv.push_back("--comment");
	aArgv.push_back("\"" + sNow + "_" + std::to_string(s_nCounter) + "\"");
	aArgv.push_back("-c");
	aArgv.push_back("2");
	aArgv.push_back("-q");
	aArgv.push_back(sCurrentRecordingFilePath);
	aArgv.push_back("trim");
	aArgv.push_back("0");
	aArgv.push_back(getDurationInSecondsAsString(m_oInit.m_nMaxRecordingDurationSeconds));

	Glib::Pid oPid;
	int nRecordingCoutFd;
	int nRecordingCerrFd;
	try {
		Glib::spawn_async_with_pipes(m_oInit.m_sRecordingDirPath, aArgv, Glib::SPAWN_SEARCH_PATH | Glib::SPAWN_DO_NOT_REAP_CHILD
						, Glib::SlotSpawnChildSetup(), &oPid, nullptr
						, (m_oInit.m_bDebug ? &nRecordingCoutFd : nullptr)
						, (m_oInit.m_bDebug ? &nRecordingCerrFd : nullptr));
	} catch (const Glib::SpawnError& oErr) {
		m_oLogger("Error spawning '" + s_sRecordingProgram + "': " + oErr.what());
		return false; //--------------------------------------------------------
	}
	m_sCurrentRecordingFilePath = sCurrentRecordingFilePath;
	m_refRecordingData = std::make_unique<RecordingData>(this, std::move(oPid), nRecordingCoutFd, nRecordingCerrFd);
	//m_oStartedCurrentRecordingTime = Glib::DateTime::create_now_local();
	//
	return true;
}
void SonoModel::onRecordingCout(bool bError, const std::string sLine) noexcept
{
	if (bError) {
		m_oLogger("Error reading " + s_sRecordingProgram + " output: " + sLine);
		return; //--------------------------------------------------------------
	}
	m_oLogger(s_sRecordingProgram + ": " + sLine);
}
void SonoModel::onRecordingCerr(bool bError, const std::string sLine) noexcept
{
	if (bError) {
		m_oLogger("Error reading " + s_sRecordingProgram + " cerr output: " + sLine);
		return; //--------------------------------------------------------------
	}
	m_oLogger(s_sRecordingProgram + " (E): " + sLine);
}
void SonoModel::stopRecording() noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::stopRecording");

	if (m_eState == STATE_STOPPED) {
		m_oLogger("Already stopped");
		return;
	}
	if (! m_sCurrentRecordingFilePath.empty()) {
		m_oLogger("Stopped recording to " + m_sCurrentRecordingFilePath);
		assert(m_eState == STATE_RECORDING);
		// kills process and puts recording in queue to be copied to a mount
		interruptRecordingProcess();
		m_sCurrentRecordingFilePath.clear();
		m_refRecordingData.reset();
	} else {
		assert((m_eState == STATE_WAITING_FOR_SPACE) || ! m_aWaitingRecPids.empty());
		m_oLogger("Stopped recording");
		m_oWaitingForFreeSpaceConn.disconnect();
	}
	m_eState = STATE_STOPPED;

	m_oStateChangedSignal.emit();
}
int32_t SonoModel::getMountIdxFromRootPath(const std::string& sMountRootPath) noexcept
{
	const auto itFind = std::find_if(m_aMountInfos.begin(), m_aMountInfos.end(), [&](const MountInfo& oMI)
	{
		return sMountRootPath == oMI.m_sRootPath;
	});
	if (itFind != m_aMountInfos.end()) {
		return static_cast<int32_t>(std::distance(m_aMountInfos.begin(), itFind));
	} else {
		return -1;
	}
}
Glib::RefPtr<Gio::Mount> SonoModel::getGioMountFromRootPath(const std::string& sMountRootPath) noexcept
{
	std::vector<Glib::RefPtr<Gio::Mount>> aMounts = m_refVolumeMonitor->get_mounts();
	Glib::RefPtr<Gio::Mount> refMount;
	for (auto& refCurMount : aMounts) {
		auto refRootFile = refCurMount->get_root();
		std::string sRootPath = refRootFile->get_path();
		if (sRootPath == sMountRootPath) {
			refMount = refCurMount;
		}
	}
	return refMount;
}
void SonoModel::unmountNonBusy() noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::unmountNonBusy");

	if (! m_sUnmountingMountPath.empty()) {
		// just one at a time
		return; //--------------------------------------------------------------
	}
//std::cout << "SonoModel::unmountNonBusy 0" << '\n';
	if (m_aMountInfos.empty()) {
		// No mounts
		return; //--------------------------------------------------------------
	}
	// find the first ready to be unmounted
	for (MountInfo& oMountInfo : m_aMountInfos) {
		if (oMountInfo.m_bUnmounting || ! oMountInfo.m_bDirty) {
			continue;
		}
		if (oMountInfo.m_sRootPath == m_sCopyingToMountRootPath) {
			// copying a file skip
			continue;
		}
		if (oMountInfo.m_sRootPath == m_sSyncingMountRootPath) {
			// syncing a file skip
			continue;
		}
		// start unmounting
		auto refMount = getGioMountFromRootPath(oMountInfo.m_sRootPath);
		if (! refMount) {
			// No mounts
			m_oLogger("Internal error: mount not found " + oMountInfo.m_sRootPath);
			return; //----------------------------------------------------------
		}
		m_sUnmountingMountPath = oMountInfo.m_sRootPath;

		m_oLogger("Started unmounting " + m_sUnmountingMountPath);
		m_refAsyncUnmountCancellable = Gio::Cancellable::create();
		try {
			refMount->unmount(Glib::RefPtr<Gio::MountOperation>{}
							, sigc::mem_fun(*this, &SonoModel::onAsyncUnmountReady)
							, m_refAsyncUnmountCancellable
							, Gio::MOUNT_UNMOUNT_NONE);
			//
			m_refUnmountingMount = refMount;
			oMountInfo.m_bUnmounting = true;
			m_oStateChangedSignal.emit();
			break; // for ----------------------------
		} catch (const Gio::Error& oErr) {
			m_oLogger("Failed unmounting of " + m_sUnmountingMountPath
						+ "\n   error: " + oErr.what());
			m_sUnmountingMountPath.clear();
			m_refAsyncUnmountCancellable.reset();
		}
	}
}
void SonoModel::onAsyncUnmountReady(Glib::RefPtr<Gio::AsyncResult>& refResult) noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::onAsyncUnmountReady");

	assert(! m_sUnmountingMountPath.empty());
	assert(m_refUnmountingMount);
	//
	std::string sPreLine;
	bool bOk = false;
	try {
		bOk = m_refUnmountingMount->unmount_finish(refResult);
	} catch (const Gio::Error& oErr) {
		sPreLine = "! unmount_finish failed " + oErr.what() + "\n";
	}
	//
	if (bOk) {
		//
		const int32_t nIdx = getMountIdxFromRootPath(m_sUnmountingMountPath);
		if (nIdx >= 0) {
			m_aMountInfos.erase(m_aMountInfos.begin() + nIdx);
		}
		m_oLogger("Finished unmounting " + m_sUnmountingMountPath);
	} else  {
		m_oLogger(sPreLine + "Error unmounting " + m_sUnmountingMountPath);
	}
	//
	m_refUnmountingMount.reset();
	m_refAsyncUnmountCancellable.reset();
	m_sUnmountingMountPath.clear();

	m_oStateChangedSignal.emit();

	// now trigger the next ready to be unmounted mount
	Glib::signal_timeout().connect_once(sigc::mem_fun(*this, &SonoModel::onAsyncUnmountNext), 0);
}
void SonoModel::onAsyncUnmountNext() noexcept
{
	unmountNonBusy();
}
bool SonoModel::checkWaitingForFreeSpace() noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::checkWaitingForFreeSpace");

	const bool bContinue = true;

	assert(m_eState == STATE_WAITING_FOR_SPACE);

	m_nRecordingFsFreeMB = getFsFreeMB(m_oInit.m_sRecordingDirPath);

	if (m_nRecordingFsFreeMB * s_nMillionBytes < m_oInit.m_nMinFreeSpaceBytes) {
		// still not enough space
		return bContinue; //----------------------------------------------------
	}
	if (! launchRecordingProcess()) {
		return bContinue; //--------------------------------------------
	}

	m_oLogger("Restarted recording to " + m_sCurrentRecordingFilePath);
	m_oStartedRecordingTime = Glib::DateTime::create_now_local();
	m_eState = STATE_RECORDING;
	m_oStateChangedSignal.emit();

	return ! bContinue;
}

void SonoModel::interruptRecordingProcess() noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::interruptRecordingProcess");

	assert(! m_sCurrentRecordingFilePath.empty());
	assert(m_refRecordingData);
	//
	::kill(m_refRecordingData->m_oRecordingPid, s_nSignalToInterruptChildren);
	//
	bool bAlreadyPresent = false;
	for (auto& oPair : m_aWaitingRecPids) {
		if (oPair.second == m_sCurrentRecordingFilePath) {
			// This happens when the process was about to exit anyway because
			// time limit reached
			bAlreadyPresent = true;
			break;
		}
	}
	if (! bAlreadyPresent) {
		m_aWaitingRecPids.push_back(std::make_pair(m_refRecordingData->m_oRecordingPid, m_sCurrentRecordingFilePath));
	}
//std::cout << "killed Pid: " << m_oRecordingPid << '\n';
}
bool SonoModel::checkWaitingChild() noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::checkWaitingChild");

	const bool bContinue = true;
//std::cout << "checkWaitingChild()" << '\n';
	if (m_aWaitingRecPids.empty()) {
		return bContinue;
	}
	bool bSignalStateChanged = false;
	auto itPair = m_aWaitingRecPids.begin();
	while (itPair != m_aWaitingRecPids.end()) {
		auto& oPair = *itPair;
		int nWaitStatus;
		auto nRet = ::waitpid(oPair.first, &nWaitStatus, WNOHANG);
		if (nRet == 0) {
			// not terminated yet
			++itPair;
		} else if (nRet < 0) {
			m_oLogger("Error waiting " + std::string{::strerror(errno)});
			++itPair;
		} else {
			const std::string& sRecordingPath = oPair.second;
			if (WIFEXITED(nWaitStatus)) {
				//
				if (m_oInit.m_bDebug) {
					m_oLogger("Recording has finished with exit status: " + std::to_string(nWaitStatus));
				}
			} else if (WIFSIGNALED(nWaitStatus)) {
				const int32_t nTermSig = WTERMSIG(nWaitStatus);
				if ((nTermSig != s_nSignalToInterruptChildren) || m_oInit.m_bDebug) {
					m_oLogger("Recording terminated by signal " + std::to_string(nTermSig));
				}
			}
			if (sRecordingPath == m_sCurrentRecordingFilePath) {
				m_sCurrentRecordingFilePath.clear();
				m_refRecordingData.reset();
			}
			// triggers copying to mount
			m_aToBeCopiedRecordings.push_back(sRecordingPath);
			itPair = m_aWaitingRecPids.erase(itPair);
			bSignalStateChanged = true;
		}
	}
	if (m_eState == STATE_RECORDING) {
		if (m_aWaitingRecPids.empty()) {
			// keep recording
			if (! recordingFsHasFreeSpace()) {
				assert(m_eState == STATE_WAITING_FOR_SPACE);
				return bContinue; //--------------------------------------------
			}
			//
			if (! launchRecordingProcess()) {
				return bContinue; //--------------------------------------------
			}
			//
			m_oLogger("Recording switched to " + m_sCurrentRecordingFilePath);
			bSignalStateChanged = true;
		}
	}
	if (bSignalStateChanged) {
		m_oStateChangedSignal.emit();
	}
	return bContinue;
}
bool SonoModel::checkRecordingMaxFileSize() noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::checkRecordingMaxFileSize");

	const bool bContinue = true;
	if (m_sCurrentRecordingFilePath.empty()) {
		assert(!m_refRecordingData);
		return bContinue; //----------------------------------------------------
	}
	// Note that the size has to stall for two checks to trigger a ::waitpid
	assert(m_refRecordingData);
	auto& oRD = *m_refRecordingData;
	const int64_t nNewLastSize = oRD.m_nCurrentRecordingSizeBytes;
	oRD.m_nCurrentRecordingSizeBytes = getFileSizeBytes(m_sCurrentRecordingFilePath);
	if (oRD.m_nCurrentRecordingLastSizeBytes == oRD.m_nCurrentRecordingSizeBytes) {
		if (m_oInit.m_bDebug) {
			m_oLogger("Recording size has stalled: " + m_sCurrentRecordingFilePath);
		}
		// value has stalled, check whether child still alive
		int nWaitStatus;;
		auto nRet = ::waitpid(oRD.m_oRecordingPid, &nWaitStatus, WNOHANG);
		if (nRet == 0) {
			// not terminated yet, keep going
		} else if (nRet < 0) {
			m_oLogger("Error waiting " + std::string{::strerror(errno)});
		} else {
			// the recording process has terminated already
			if (WIFEXITED(nWaitStatus)) {
				//
				if (m_oInit.m_bDebug) {
					m_oLogger("Reason: finished with exit status: " + std::to_string(nWaitStatus));
				}
			} else if (WIFSIGNALED(nWaitStatus)) {
				const int32_t nTermSig = WTERMSIG(nWaitStatus);
				if ((nTermSig != s_nSignalToInterruptChildren) || m_oInit.m_bDebug) {
					m_oLogger("Reason: terminated by signal " + std::to_string(nTermSig));
				}
			}
			if (m_oInit.m_bDebug) {
				m_oLogger("Reason: it terminated");
			}
			m_sCurrentRecordingFilePath.clear();
			m_refRecordingData.reset();
			//
			m_oStateChangedSignal.emit();
			//
			if (m_eState == STATE_RECORDING) {
				// keep recording
				if (! recordingFsHasFreeSpace()) {
					assert(m_eState == STATE_WAITING_FOR_SPACE);
					return bContinue; //----------------------------------------
				}
				//
				if (! launchRecordingProcess()) {
					return bContinue; //----------------------------------------
				}
				//
				m_oLogger("Recording switched to " + m_sCurrentRecordingFilePath);
				m_oStateChangedSignal.emit();
			}
		}
		return bContinue; //----------------------------------------------------
	}
	oRD.m_nCurrentRecordingLastSizeBytes = nNewLastSize;
	if (oRD.m_nCurrentRecordingSizeBytes > m_oInit.m_nMaxFileSizeBytes) {
		if (m_oInit.m_bDebug) {
			m_oLogger("Recording has reached size limit: " + m_sCurrentRecordingFilePath);
		}
		interruptRecordingProcess();
		m_sCurrentRecordingFilePath.clear();
		m_refRecordingData.reset();
	}
	m_oStateChangedSignal.emit();
	return bContinue;
}
bool SonoModel::checkRecordingTimedOut() noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::checkRecordingTimedOut");

	#ifndef NDEBUG
	for (auto& oPair : m_aWaitingRecPids) {
		if (oPair.second == m_sCurrentRecordingFilePath) {
			// size check already killed the recording but
			// it's also supposed to disconnect this signal
			assert(false);
		}
	}
	#endif //NDEBUG
	//
	assert(! m_sCurrentRecordingFilePath.empty());
	assert(m_refRecordingData);
	m_aWaitingRecPids.push_back(std::make_pair(m_refRecordingData->m_oRecordingPid, m_sCurrentRecordingFilePath));
	return false; // connect once
}


bool SonoModel::checkToBeCopiedRecordings() noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::checkToBeCopiedRecordings");

	const bool bContinue = true;
	if (! m_sCopyingToMountRootPath.empty()) {
		// copy operation already in progress
		return bContinue; //----------------------------------------------------
	}
	if (m_aToBeCopiedRecordings.empty()) {
		return bContinue; //----------------------------------------------------
	}
	const std::string sRecordingFilePath = m_aToBeCopiedRecordings[0];
	//
	if (m_aMountInfos.empty()) {
		// There is nowhere to copy recording
		return bContinue; //----------------------------------------------------
	}
	// The first mount should already be the best
	auto& oMountInfo = m_aMountInfos[0];
	if (oMountInfo.m_nFailedCopyAttempts >= MountInfo::s_nFailedCopyAttemptsToBlacklist) {
		return bContinue; //----------------------------------------------------
	}
	if (oMountInfo.m_bUnmounting) {
		// There is nowhere to copy recording, shouldn't happen
		return bContinue; //----------------------------------------------------
	}
	//
	assert(! m_refCopyingFile);
	m_refCopyingFile = Gio::File::create_for_path(sRecordingFilePath);
	const int64_t nCurrentSizeBytes = getFileSizeBytes(*(m_refCopyingFile.operator->()));
	if (nCurrentSizeBytes < 1) {
		m_oLogger("Can't get size of file " + sRecordingFilePath);
		return bContinue; //----------------------------------------------------
	}
	if (1.0 * s_nMillionBytes * oMountInfo.m_nFreeMB < s_fMountFreeSpaceToMaxRecordingSizeRatio * nCurrentSizeBytes) {
		// Not enough space on mount
		return bContinue; //----------------------------------------------------
	}
	//
	m_sCopyingToMountRootPath = oMountInfo.m_sRootPath;
	m_sCopyingFileName = m_refCopyingFile->get_basename();
	const std::string sCopyingFolderPath = m_sCopyingToMountRootPath + (oMountInfo.m_sFolder.empty() ? "" : "/" + oMountInfo.m_sFolder);
	auto refDest = Gio::File::create_for_path(sCopyingFolderPath + "/" + m_sCopyingFileName);
	//
	m_oLogger("Started copying " + m_sCopyingFileName + " to " + sCopyingFolderPath);
	m_refAsyncCopyCancellable = Gio::Cancellable::create();
	try {
		m_refCopyingFile->copy_async(refDest, sigc::mem_fun(*this, &SonoModel::onAsyncCopyProgress)
									, sigc::mem_fun(*this, &SonoModel::onAsyncCopyReady)
									, m_refAsyncCopyCancellable
									, Gio::FILE_COPY_OVERWRITE, Glib::PRIORITY_LOW);
	} catch (const Gio::Error& oErr) {
		m_oLogger("Failed to copy file " + sRecordingFilePath
				+ "\n  error: " + oErr.what());
		m_sCopyingToMountRootPath.clear();
		m_sCopyingFileName.clear();
		m_refAsyncCopyCancellable.clear();
		m_refCopyingFile.reset();
		return bContinue; //----------------------------------------------------
	}
	m_oStateChangedSignal.emit();
	return bContinue;
}
void SonoModel::onAsyncCopyProgress(goffset /*nCurrentBytes*/, goffset /*nTotalBytes*/) noexcept
{
	//TODO
}
void SonoModel::onAsyncCopyReady(Glib::RefPtr<Gio::AsyncResult>& refResult) noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::onAsyncCopyReady");

	assert(! m_aToBeCopiedRecordings.empty());

	bool bSortMounts = false;

	std::string sPreLine;
	bool bOk = false;
	try {
		bOk = m_refCopyingFile->copy_finish(refResult);
	} catch (const Gio::Error& oErr) {
		sPreLine = "! copy_finish failed: " + oErr.what() + "\n";
	}
	const int32_t nMountIdx = getMountIdxFromRootPath(m_sCopyingToMountRootPath);
	if (bOk) {
		//
		assert(m_aToBeCopiedRecordings[0] == m_oInit.m_sRecordingDirPath + "/" + m_sCopyingFileName);
		m_aToBeCopiedRecordings.erase(m_aToBeCopiedRecordings.begin());
		//
		std::string sCopyingFolderPath;
		if (nMountIdx >= 0) {
			// While copying the first mount cannot be sorted elsewhere
			assert(nMountIdx == 0);
			auto& oMountInfo = m_aMountInfos[nMountIdx];
			assert(! oMountInfo.m_bUnmounting);
			assert(oMountInfo.m_sRootPath == m_sCopyingToMountRootPath);
			oMountInfo.m_bDirty = true;
			auto refMount = getGioMountFromRootPath(m_sCopyingToMountRootPath);
			if (! refMount) {
				m_oLogger("Internal error: mount not found " + oMountInfo.m_sRootPath);
			} else {
				auto refRootFile = refMount->get_root();
				oMountInfo.m_nFreeMB = getFsFreeMB(*(refRootFile.operator->()));
			}
			//
			m_aToBeSyncedRecordings.push_back(std::make_pair(m_sCopyingToMountRootPath, m_sCopyingFileName));
			//
			const bool bWasFailing = (oMountInfo.m_nFailedCopyAttempts > 0);
			//
			oMountInfo.m_nFailedCopyAttempts = 0;
			//
			sCopyingFolderPath = m_sCopyingToMountRootPath + (oMountInfo.m_sFolder.empty() ? "" : "/" + oMountInfo.m_sFolder);
			m_oLogger("Finished copying " + m_sCopyingFileName + " to " + sCopyingFolderPath);
			//
			if (bWasFailing) {
				m_oLogger("Promoting " + m_sCopyingToMountRootPath);
				//
				bSortMounts = true;
			}
		} else {
			// Can't sync a file to a mount that has been removed
			// this shouldn't happen since a canceled copy results in an error
			m_oLogger("Internal error copying " + m_sCopyingFileName + " to " + m_sCopyingToMountRootPath);
		}
	} else  {
		m_oLogger(sPreLine + "Error copying " + m_sCopyingFileName + " to " + m_sCopyingToMountRootPath);
		//
		// When a mount is removed the copy is canceled with an error
		if (nMountIdx >= 0) {
			auto& oMountInfo = m_aMountInfos[0];
			++oMountInfo.m_nFailedCopyAttempts;
			if (oMountInfo.m_nFailedCopyAttempts == MountInfo::s_nFailedCopyAttemptsToBlacklist) {
				m_oLogger("Demoting " + m_sCopyingToMountRootPath);
				//
				bSortMounts = true;
			}
		}
	}
	//
	m_refAsyncCopyCancellable.reset();
	m_sCopyingToMountRootPath.clear();
	m_sCopyingFileName.clear();
	m_refCopyingFile.reset();

	if (bSortMounts) {
		sortMounts();
	}

	m_oStateChangedSignal.emit();
}


bool SonoModel::checkToBeSyncedRecordings() noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::checkToBeSyncedRecordings");

	const bool bContinue = true;
	if (! m_sSyncingMountRootPath.empty()) {
		assert(m_refSyncingData);
		// Beware! At this point m_sSyncingMountRootPath could identify
		// a mount that was already removed!
		SyncingData& oSD = *(m_refSyncingData.operator->());
		int nWaitStatus;
		auto nRet = ::waitpid(oSD.m_oSyncingPid, &nWaitStatus, WNOHANG);
		if (nRet == 0) {
			// not terminated yet
			maybeTerminateSyncingProcess();
			return bContinue; //------------------------------------------------
		}
		if (nRet < 0) {
			m_oLogger("Error waiting '" + s_sSyncProgram + "': " + std::string{::strerror(errno)});
			// keep waiting
			// maybe after 3 errors consider the child dead?
			return bContinue; //------------------------------------------------
		}
		bool bRemoveSource = true;
		if (WIFEXITED(nWaitStatus)) {
			//
			const int32_t nIdx = getMountIdxFromRootPath(m_sSyncingMountRootPath);
			if (nIdx < 0) {
				// Mount no longer there.
				// Alas sync program doesn't necessarily fail when a mount is removed
				// while syncing a file.
				// Consider the sync failed so that the file can be picked up for
				// another copying process by the next startup.
				m_oLogger("Syncing finished on no longer existing mount: " + m_sSyncingMountRootPath);
				bRemoveSource = false;
			} else if (! Glib::file_test(m_refSyncingData->m_sSyncingFilePath, Glib::FILE_TEST_EXISTS)) {
				m_oLogger("Syncing finished but file '" + m_refSyncingData->m_sSyncingFileName + "'"
							+ "\n  doesn't exist on mount: " + m_sSyncingMountRootPath);
				bRemoveSource = false;
			}
		} else if (WIFSIGNALED(nWaitStatus)) {
			const int32_t nTermSig = WTERMSIG(nWaitStatus);
			if ((nTermSig != s_nSignalToInterruptChildren) || m_oInit.m_bDebug) {
				m_oLogger("Syncing terminated by signal " + std::to_string(nTermSig));
			}
			bRemoveSource = false;
		}
		//
		if (bRemoveSource) {
			m_oLogger("Finished syncing " + oSD.m_sSyncingFilePath);
			//
			m_aToBeRemovedRecordings.push_back(m_oInit.m_sRecordingDirPath + "/" + oSD.m_sSyncingFileName);
		}
		//
		m_sSyncingMountRootPath.clear();
		m_refSyncingData.reset();
	}
	if (m_aToBeSyncedRecordings.empty()) {
		return bContinue; //----------------------------------------------------
	}
	const auto& oPair = m_aToBeSyncedRecordings[0];
	std::string sSyncingMountRootPath = oPair.first;
	std::string sSyncingFileName = oPair.second;
	const int32_t nIdx = getMountIdxFromRootPath(sSyncingMountRootPath);
	if (nIdx < 0) {
		// The mount was removed, nothing to sync
		m_aToBeSyncedRecordings.erase(m_aToBeSyncedRecordings.begin());
		return bContinue; //----------------------------------------------------
	}
	const MountInfo& oMountInfo = m_aMountInfos[nIdx];
	const std::string sSyncingFolderPath = sSyncingMountRootPath + (oMountInfo.m_sFolder.empty() ? "" : "/" + oMountInfo.m_sFolder);
	std::string sSyncingFilePath = sSyncingFolderPath + "/" + sSyncingFileName;

	if (! launchSyncingProcess(std::move(sSyncingMountRootPath), std::move(sSyncingFileName), std::move(sSyncingFilePath))) {
		return bContinue; //----------------------------------------------------
	}
	//
	m_aToBeSyncedRecordings.erase(m_aToBeSyncedRecordings.begin());
	//
	return bContinue;
}
bool SonoModel::launchSyncingProcess(std::string&& sSyncingMountRootPath, std::string&& sSyncingFileName
									, std::string&& sSyncingFilePath) noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::launchSyncingProcess");

	std::vector<std::string> aArgv;
	aArgv.reserve(3);
	aArgv.push_back(s_sSyncProgram);
	aArgv.push_back("-d");
	aArgv.push_back(sSyncingFilePath);
	Glib::Pid oPid;
	int nSyncingCoutFd;
	int nSyncingCerrFd;
	try {
		Glib::spawn_async_with_pipes(m_oInit.m_sRecordingDirPath, aArgv, Glib::SPAWN_SEARCH_PATH | Glib::SPAWN_DO_NOT_REAP_CHILD
						, Glib::SlotSpawnChildSetup(), &oPid, nullptr
						, (m_oInit.m_bDebug ? &nSyncingCoutFd : nullptr)
						, (m_oInit.m_bDebug ? &nSyncingCerrFd : nullptr));
	} catch (const Glib::SpawnError& oErr) {
		m_oLogger("Error spawning '" + s_sSyncProgram + "': " + oErr.what());
		return false; //--------------------------------------------------------
	}
	assert(m_sSyncingMountRootPath.empty());
	m_sSyncingMountRootPath = std::move(sSyncingMountRootPath);
	m_refSyncingData = std::make_unique<SyncingData>(this, std::move(sSyncingFileName), std::move(sSyncingFilePath)
													, std::move(oPid), nSyncingCoutFd, nSyncingCerrFd);
	SyncingData& oSD = *(m_refSyncingData.operator->());

	m_oLogger("Started syncing of " + oSD.m_sSyncingFilePath);

	auto nPrio = ::getpriority(PRIO_PROCESS, oSD.m_oSyncingPid);
	// lower priority of process running sync
	auto nRet = ::setpriority(PRIO_PROCESS, oSD.m_oSyncingPid, nPrio + 1);
	if (nRet < 0) {
		m_oLogger("Error setting priority of " + s_sSyncProgram);
	}
	return true;
}
void SonoModel::maybeTerminateSyncingProcess() noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::maybeTerminateSyncingProcess");

	assert(m_refSyncingData);
	const int32_t nIdx = getMountIdxFromRootPath(m_sSyncingMountRootPath);
	if (nIdx < 0) {
		// The mount was already removed, apparently the sync process
		// is hanging ... kill it explicitely
		if (m_oInit.m_bVerbose) {
			m_oLogger("Mount was remved while syncing!");
			m_oLogger("Terminating " + s_sSyncProgram + " of " + m_refSyncingData->m_sSyncingFilePath);
		}
		::kill(m_refSyncingData->m_oSyncingPid, s_nSignalToTerminateChildren);
	}
}
void SonoModel::onSyncingCout(bool bError, const std::string sLine) noexcept
{
	if (bError) {
		m_oLogger("Error reading " + s_sSyncProgram + " output: " + sLine);
		return; //--------------------------------------------------------------
	}
	m_oLogger(s_sSyncProgram + ": " + sLine);
}
void SonoModel::onSyncingCerr(bool bError, const std::string sLine) noexcept
{
	if (bError) {
		m_oLogger("Error reading " + s_sSyncProgram + " cerr output: " + sLine);
		return; //--------------------------------------------------------------
	}
	m_oLogger(s_sSyncProgram + " (E): " + sLine);
}


bool SonoModel::checkToBeRemovedRecordings() noexcept
{
	DebugCtx<SonoModel> oCtx(this, "SonoModel::checkToBeRemovedRecordings");

	const bool bContinue = true;
	if (! m_sRemovingFileName.empty()) {
		// remove operation already in progress
		return bContinue; //----------------------------------------------------
	}
	if (m_aToBeRemovedRecordings.empty()) {
		return bContinue; //----------------------------------------------------
	}
	const std::string sRecordingFilePath = m_aToBeRemovedRecordings[0];

	assert(! m_refRemovingFile);
	m_refRemovingFile = Gio::File::create_for_path(sRecordingFilePath);
	m_sRemovingFileName = m_refRemovingFile->get_basename();
	// now remove copied file
	m_oLogger("Started removing " + m_sRemovingFileName);
	m_refAsyncRemoveCancellable = Gio::Cancellable::create();
	try {
		m_refRemovingFile->remove_async(sigc::mem_fun(*this, &SonoModel::onAsyncRemoveReady)
										, m_refAsyncRemoveCancellable
										, Glib::PRIORITY_LOW);
		m_oStateChangedSignal.emit();
	} catch (const Gio::Error& oErr) {
		m_oLogger("Failed to remove file " + m_sRemovingFileName
				+ "\n  error: " + oErr.what());
	}
	return bContinue;
}

void SonoModel::onAsyncRemoveReady(Glib::RefPtr<Gio::AsyncResult>& refResult) noexcept
{
	std::string sPreLine;
	bool bOk = false;
	try {
		bOk = m_refRemovingFile->remove_finish(refResult);
	} catch (const Gio::Error& oErr) {
		sPreLine = "! remove_finish " + oErr.what() + "\n";
	}
	if (bOk) {
		//
		m_aToBeRemovedRecordings.erase(m_aToBeRemovedRecordings.begin());
		//
		m_oLogger("Finished removing " + m_sRemovingFileName);
	} else  {
		m_oLogger(sPreLine + "Error removing " + m_sRemovingFileName);
	}
	m_refAsyncRemoveCancellable.reset();
	m_sRemovingFileName.clear();
	m_refRemovingFile.reset();

	m_oStateChangedSignal.emit();
}

bool SonoModel::checkSonoremQuitFile() noexcept
{
	const bool bContinue = true;
	if (Glib::file_test(m_sSonoremQuitFilePath, Glib::FILE_TEST_EXISTS)) {
		if (m_oInit.m_bVerbose) {
			m_oLogger("Detected file " + m_sSonoremQuitFilePath + ": quitting ...");
		}
		sonoremQuit();
	}
	return bContinue;
}
void SonoModel::sonoremQuit() noexcept
{
	m_oQuitSignal.emit();
}


const std::string& SonoModel::getRecordingFilePath() const noexcept
{
	return m_sCurrentRecordingFilePath;
}
std::string SonoModel::getCopyingFromFilePath() const noexcept
{
	return (m_sCopyingFileName.empty() ? "" : m_oInit.m_sRecordingDirPath + "/" + m_sCopyingFileName);
}
std::string SonoModel::getCopyingToFilePath() const noexcept
{
	return (m_sCopyingFileName.empty() ? "" : m_sCopyingToMountRootPath + "/" + m_sCopyingFileName);
}
std::string SonoModel::getSyncingFilePath() const noexcept
{
	return (m_sSyncingMountRootPath.empty() ? "" : m_refSyncingData->m_sSyncingFilePath);
}
std::string SonoModel::getRemovingFilePath() const noexcept
{
	return (m_sRemovingFileName.empty() ? "" : m_oInit.m_sRecordingDirPath + "/" + m_sRemovingFileName);
}
const std::string& SonoModel::getUnmountingMountRootPath() const noexcept
{
	return m_sUnmountingMountPath;
}
int64_t SonoModel::getRecordingSizeBytes() const noexcept
{
	if (m_refRecordingData) {
		return m_refRecordingData->m_nCurrentRecordingSizeBytes;
	} else {
		return 0;
	}
}
int64_t SonoModel::getRecordingElapsedSeconds() const noexcept
{
	if (m_sCurrentRecordingFilePath.empty()) {
		return -1;
	}
	constexpr int64_t nMicrosecondsInASecond = 1000000;
	return Glib::DateTime::create_now_local().difference(m_oStartedRecordingTime) / nMicrosecondsInASecond;
}
int32_t SonoModel::getNrWaitingForKilledProcesses() const noexcept
{
	return static_cast<int32_t>(m_aWaitingRecPids.size());
}
int32_t SonoModel::getNrToBeCopiedRecordings() const noexcept
{
	return static_cast<int32_t>(m_aToBeCopiedRecordings.size());
}
int32_t SonoModel::getNrToBeRemovedRecordings() const noexcept
{
	return static_cast<int32_t>(m_aToBeRemovedRecordings.size());
}
int32_t SonoModel::getNrToBeSyncedRecordings() const noexcept
{
	return static_cast<int32_t>(m_aToBeSyncedRecordings.size());
}

} // namespace sono
