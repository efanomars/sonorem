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
 * File:   sonomodel.h
 */

#ifndef SONO_SONO_MODEL_H
#define SONO_SONO_MODEL_H

#include "sonosources.h"

#include "debugctx.h"

#include <glibmm.h>
#include <giomm.h>

#include <sigc++/sigc++.h>

#include <string>
#include <vector>

#include <stdint.h>

namespace stmi { class DeviceManager; }

namespace sono
{

using std::shared_ptr;
using std::unique_ptr;

class SonoModel
{
public:
	template<class TLogger>
	explicit SonoModel(TLogger&& oLogger) noexcept
	: m_oLogger(std::move(oLogger))
	, m_nDebugCtxDepth(-1)
	{
	}
	~SonoModel() noexcept;

	struct Init
	{
		int32_t m_nMaxRecordingDurationSeconds = 60 * 60;
		int64_t m_nMaxFileSizeBytes = 1 * 1000 * 1000 * 1000;
		int64_t m_nMinFreeSpaceBytes = static_cast<int64_t>(2) * 1000 * 1000 * 1000;
		std::string m_sRecordingDirPath;
		std::string m_sPreString; // string prepended to recording files
		std::string m_sRecordingFileExt = "ogg"; // ogg, wav, aiff, see rec
		std::vector<std::string> m_aExclMountNames;
		bool m_bExcludeAllMountNames = false;
		bool m_bAutoStart = false;
		bool m_bRfkillWifiOn = false;
		bool m_bRfkillWifiOff = false;
		bool m_bRfkillBluetoothOn = false;
		bool m_bRfkillBluetoothOff = false;
		bool m_bVerbose = false;
		bool m_bDebug = false;
	};
	std::string init(Init&& oInit) noexcept;

	std::function<void(const std::string&)>& getLogger() noexcept;
	bool isAutoStart() const  noexcept;

	void startRecording() noexcept;
	void stopRecording() noexcept;
	void unmountNonBusy() noexcept;

	/** State STATE_WAITING_FOR_SPACE is STATE_STOPPED but waiting for
	 * the disk to have enough space to automatically switch to STATE_RECORDING.
	 */
	enum STATE
	{
		  STATE_STOPPED = 0
		, STATE_RECORDING = 1
		, STATE_WAITING_FOR_SPACE = 2
	};
	STATE getState() const noexcept;

	struct MountInfo {
		std::string m_sRootPath; // the unique key for a mount
		std::string m_sName; // some generic name given by OS, or content of sonorem.name
		std::string m_sUUID; // not always given
		std::string m_sFolder; // the root folder if empty, or content of sonorem.folder
		int64_t m_nFreeMB = 0; // currently free space
		int32_t m_nFailedCopyAttempts = 0;
		bool m_bDirty = false; // files were copied to it, needs unmount
		bool m_bUnmounting = false; // an unmount operation is going on
		static constexpr int32_t s_nFailedCopyAttemptsToBlacklist = 4;
	public:
		bool isBlacklisted() const noexcept
		{
			return (m_nFailedCopyAttempts >= s_nFailedCopyAttemptsToBlacklist);
		}
	};

	const std::string& getRecordingDirPath() const noexcept;
	int64_t getRecordingFsFreeMB() const noexcept;

	const std::string& getRecordingFilePath() const noexcept;
	std::string getCopyingFromFilePath() const noexcept;
	std::string getCopyingToFilePath() const noexcept;
	std::string getSyncingFilePath() const noexcept;
	std::string getRemovingFilePath() const noexcept;
	const std::string& getUnmountingMountRootPath() const noexcept;

	int64_t getRecordingSizeBytes() const noexcept;
	int64_t getRecordingElapsedSeconds() const noexcept;
	int32_t getNrWaitingForKilledProcesses() const noexcept;
	int32_t getNrToBeCopiedRecordings() const noexcept;
	int32_t getNrToBeSyncedRecordings() const noexcept;
	int32_t getNrToBeRemovedRecordings() const noexcept;

	const std::vector<MountInfo>& getMountInfos() const noexcept;

	/* Emits when what's returned by getMountInfos() has changed. */
	sigc::signal<void> m_oMountsChangedSignal;

	sigc::signal<void> m_oStateChangedSignal;

	sigc::signal<void> m_oRecordingFsFreeMBChangedSignal;

	sigc::signal<void> m_oTellStatusSignal;

	sigc::signal<void> m_oQuitSignal;

	static std::string getNowString() noexcept;
	static std::string getShortNowString() noexcept;
	static std::string getDurationInSecondsAsString(int64_t nDuration) noexcept;

	const std::string& getRecordingFileExt() const noexcept;

	static const std::string s_sRecordingProgram;
	static const std::string s_sRecordingDefaultFileExt;
	static const int32_t s_nMaxSonoremNameLen;

	static constexpr int32_t s_nCheckWaitingForFreeSpaceSeconds = 1;

protected:
	bool matchRecordingFileName(const std::string& sFileName) noexcept;

private:
	void initMountableVolumes() noexcept;

	bool isMountExcluded(Gio::Mount& oMount, const std::string& sName, const std::string& sRootPath) noexcept;
	bool sonoremFileExists(const std::string& sRootPath, const std::string& sFileExt) noexcept;

	int64_t getFileSizeBytes(const std::string& sPath) const noexcept;
	int64_t getFileSizeBytes(Gio::File& oFile) const noexcept;
	int64_t getFsFreeMB(const std::string& sPath) const noexcept;
	int64_t getFsFreeMB(Gio::File& oFile) const noexcept;

	void onVolumeAdded(const Glib::RefPtr<Gio::Volume>& refVolume) noexcept;
	void onVolumeAddedOut(Gio::Volume* p0Volume) noexcept;
	void onVolumeRemoved(const Glib::RefPtr<Gio::Volume>& refVolume) noexcept;

	void onMountAdded(const Glib::RefPtr<Gio::Mount>& refMount) noexcept;
	void onMountRemoved(const Glib::RefPtr<Gio::Mount>& refMount) noexcept;
	void onMountChanged(const Glib::RefPtr<Gio::Mount>& refMount) noexcept;

	void getMountOverridables(const std::string& sRootPath, std::string& sName, std::string& sFolder) noexcept;
	bool updateMountsFreeSpace() noexcept;

	Glib::RefPtr<Gio::Mount> getGioMountFromRootPath(const std::string& sMountRootPath) noexcept;
	int32_t getMountIdxFromRootPath(const std::string& sMountRootPath) noexcept;

	void sortMounts() noexcept;

	bool recordingFsHasFreeSpace() noexcept;

	bool launchRecordingProcess() noexcept;
	void interruptRecordingProcess() noexcept;
	bool checkRecordingTimedOut() noexcept;
	bool checkWaitingForFreeSpace() noexcept;
	bool checkRecordingMaxFileSize() noexcept;
	void onRecordingCout(bool bError, const std::string sLine) noexcept;
	void onRecordingCerr(bool bError, const std::string sLine) noexcept;
	bool checkWaitingChild() noexcept;
	bool checkToBeCopiedRecordings() noexcept;
	bool checkToBeSyncedRecordings() noexcept;
	bool launchSyncingProcess(std::string&& sSyncingMountRootPath, std::string&& sSyncingFileName
							, std::string&& sSyncingFilePath) noexcept;
	void maybeTerminateSyncingProcess() noexcept;
	void onSyncingCout(bool bError, const std::string sLine) noexcept;
	void onSyncingCerr(bool bError, const std::string sLine) noexcept;
	bool checkToBeRemovedRecordings() noexcept;
	bool checkSonoremQuitFile() noexcept;
	void sonoremQuit() noexcept;

	void onAsyncCopyProgress(goffset nCurrentBytes, goffset nTotalBytes) noexcept;
	void onAsyncCopyReady(Glib::RefPtr<Gio::AsyncResult>& refResult) noexcept;
	void onAsyncRemoveReady(Glib::RefPtr<Gio::AsyncResult>& refResult) noexcept;
	void onAsyncUnmountReady(Glib::RefPtr<Gio::AsyncResult>& refResult) noexcept;
	void onAsyncUnmountNext() noexcept;

	std::string getRecordingFileName(const std::string& sNow) noexcept;
	void pickupLeftoverToBeCopiedRecordings() noexcept;

private:
	friend struct DebugCtx<SonoModel>;

	std::function<void(const std::string&)> m_oLogger;
	mutable int32_t m_nDebugCtxDepth;

	Glib::DateTime m_oStartedRecordingTime;
	Init m_oInit;

	Glib::RefPtr<Gio::VolumeMonitor> m_refVolumeMonitor;

	STATE m_eState = STATE_STOPPED;
	sigc::connection m_oWaitingForFreeSpaceConn;

	// recording, copying, syncing, unmounting can go in parallel
	std::string m_sCopyingToMountRootPath; // if empty not copying
	std::string m_sCopyingFileName; // The file name being copied to m_sCopyingToMountRootPath
	Glib::RefPtr<Gio::File> m_refCopyingFile; // the source
	Glib::RefPtr<Gio::Cancellable> m_refAsyncCopyCancellable;
	//
	std::string m_sSyncingMountRootPath; // if empty not syncing
	struct SyncingData
	{
		SyncingData(SonoModel* p0This, std::string&& sSyncingFileName, std::string&& sSyncingFilePath
					, Glib::Pid&& oPid, int nSyncingCoutFd, int nSyncingCerrFd) noexcept;
		~SyncingData() noexcept;
		std::string m_sSyncingFileName; // The file name being synced on mount m_sSyncingMountRootPath
		std::string m_sSyncingFilePath; // The full path of the file being synced
		Glib::Pid m_oSyncingPid;
		Glib::RefPtr<PipeInputSource> m_refSyncingCout;
		sigc::connection m_oSyncingCoutConn;
		Glib::RefPtr<PipeInputSource> m_refSyncingCerr;
		sigc::connection m_oSyncingCerrConn;
	private:
		SyncingData() = delete;
	};
	unique_ptr<SyncingData> m_refSyncingData;
	//
	std::string m_sRemovingFileName; // The file name being removed, if empty not removing
	Glib::RefPtr<Gio::File> m_refRemovingFile;
	Glib::RefPtr<Gio::Cancellable> m_refAsyncRemoveCancellable;
	//
	std::string m_sUnmountingMountPath; // if empty not unmounting
	Glib::RefPtr<Gio::Mount> m_refUnmountingMount;
	Glib::RefPtr<Gio::Cancellable> m_refAsyncUnmountCancellable;
	//
	bool m_bCheckingFreeSpace; // The bigger the max file size the less often the checking
	bool m_bCheckingRecordingAlive; // Check at least once a minute
	bool m_bCheckingRecordingFileSize; // The bigger the max file size the less often the checking

	int64_t m_nRecordingFsFreeMB;

	std::string m_sCurrentRecordingFilePath; // if empty not recording
	struct RecordingData
	{
		RecordingData(SonoModel* p0This, Glib::Pid&& oPid, int nRecordingCoutFd, int nRecordingCerrFd) noexcept;
		~RecordingData() noexcept;
		Glib::Pid m_oRecordingPid;
		sigc::connection m_oRecordingTimedOutConn;
		Glib::RefPtr<PipeInputSource> m_refRecordingCout;
		sigc::connection m_oRecordingCoutConn;
		Glib::RefPtr<PipeInputSource> m_refRecordingCerr;
		sigc::connection m_oRecordingCerrConn;
		//sigc::connection m_oCurrentRecordingConn; // max recording size check
		int64_t m_nCurrentRecordingSizeBytes;
		int64_t m_nCurrentRecordingLastSizeBytes;
	private:
		RecordingData() = delete;
	};
	unique_ptr<RecordingData> m_refRecordingData;

	std::string m_sSonoremQuitFilePath;

	// "rec" child processes that have to finish (killed or because about to exit)
	std::vector< std::pair<Glib::Pid, std::string> > m_aWaitingRecPids; // Value: (pid, sRecordingFilePath)
	// file paths that need to be moved from main disk to a mount
	std::vector<std::string> m_aToBeCopiedRecordings;
	// (mount root path, file name) that need to be synced on a mount
	std::vector<std::pair<std::string, std::string>> m_aToBeSyncedRecordings;
	// file paths that need to be removed from main disk
	std::vector<std::string> m_aToBeRemovedRecordings;
	// the currently mounted usb sticks
	std::vector<MountInfo> m_aMountInfos;
};

} // namespace sono

#endif /* SONO_SONO_MODEL_H */

