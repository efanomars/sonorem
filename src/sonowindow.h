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
 * File:   sonowindow.h
 */

#ifndef SONO_SONO_WINDOW_H
#define SONO_SONO_WINDOW_H

#include "sonomodel.h"

#include <stmm-input/event.h>
#include <stmm-input/devicemanager.h>

#include <gtkmm.h>

#include <string>

#include <stdint.h>

namespace sono
{

class SonoWindow : public Gtk::Window
{
public:
	SonoWindow(SonoModel& oModel, bool bVerbose, bool bDebug, const std::string& sSpeechApp, bool bKeepOnTop
				, const shared_ptr<stmi::DeviceManager>& refDM, const std::string& sTitle) noexcept;

	void logToWindow(const std::string& sStr) noexcept;
private:
	void log(const std::string& sStr) noexcept;

	void autoStart() noexcept;

	void onSigIsActiveChanged() noexcept;
	bool isWindowActive() const noexcept;

	void startRecording() noexcept;
	void stopRecording() noexcept;
	void unmountNonBusy() noexcept;
	void tellStatus() noexcept;
	bool resetStatusCounter() noexcept;
	void tellString(const std::string& sStr) noexcept;
	std::string getSizeStringFromBytes(int64_t nSizeBytes, bool bLongUnit) const noexcept;
	std::string getTimeStringFromSeconds(int64_t nRecordingElapsedSeconds) const noexcept;

	void onNotebookSwitchPage(Gtk::Widget*, guint nPageNum) noexcept;
	void onButtonStartRecording() noexcept;
	void onButtonStopRecording() noexcept;
	void onButtonUnmountNonBusy() noexcept;
	void onButtonTellState() noexcept;

	void onStmiEvent(const shared_ptr<stmi::Event>& refEvent) noexcept;
	void regenerateDevicesList() noexcept;
	std::string getCapabilityClassId(const stmi::Capability::Class& oClass) const noexcept;

	void quitSignal() noexcept;
	void quitNow() noexcept;
	void mountsChanged() noexcept;
	void stateChangedSignal() noexcept;
	void refreshState() noexcept;
	void regenerateMountsList() noexcept;

	bool toTheFront() noexcept;

private:
	friend struct DebugCtx<SonoWindow>;

	SonoModel& m_oModel;
	std::function<void(const std::string&)>& m_oLogger;

	bool m_bVerbose;
	mutable int32_t m_nDebugCtxDepth;
	std::string m_sSpeechApp;

	shared_ptr<stmi::DeviceManager> m_refDM;

	static constexpr const int32_t s_nTotPages = 5;
	int32_t m_aPageIndex[s_nTotPages];

	//Gtk::Box* m_p0VBoxWhole = nullptr;

	//Gtk::Notebook* m_p0NotebookChoices = nullptr;

		static constexpr const int32_t s_nTabMain = 0;
		//Gtk::Label* m_p0TabLabelMain;
		//Gtk::Box* m_p0VBoxMain = nullptr;

			//Gtk::Grid* m_p0GridState = nullptr;
				//Gtk::Label* m_p0LabelState = nullptr;
				Gtk::Entry* m_p0EntryState = nullptr;
				//Gtk::Label* m_p0LabelRecordingFilePath = nullptr;
				Gtk::Entry* m_p0EntryRecordingFilePath = nullptr;
				//Gtk::Label* m_p0LabelRecordingFileSize = nullptr;
				Gtk::Entry* m_p0EntryRecordingFileSize = nullptr;
				//Gtk::Label* m_p0LabelFreeDiskSpace = nullptr;
				Gtk::Entry* m_p0EntryFreeDiskSpace = nullptr;
				//Gtk::Label* m_p0LabelCopyingFilePath = nullptr;
				Gtk::Entry* m_p0EntryCopyingFilePath = nullptr;
				//Gtk::Label* m_p0LabelSyncingFilePath = nullptr;
				Gtk::Entry* m_p0EntrySyncingFilePath = nullptr;
				//Gtk::Label* m_p0LabelRemovingFilePath = nullptr;
				Gtk::Entry* m_p0EntryRemovingFilePath = nullptr;
				//Gtk::Label* m_p0LabelNrToBeCopiedFiles = nullptr;
				Gtk::Entry* m_p0EntryNrToBeCopiedFiles = nullptr;
				//Gtk::Label* m_p0LabelNrToBeSyncedFiles = nullptr;
				Gtk::Entry* m_p0EntryNrToBeSyncedFiles = nullptr;
				//Gtk::Label* m_p0LabelNrToBeRemovedFiles = nullptr;
				Gtk::Entry* m_p0EntryNrToBeRemovedFiles = nullptr;
				//Gtk::Label* m_p0LabelUnmountingRootPath = nullptr;
				Gtk::Entry* m_p0EntryUnmountingRootPath = nullptr;

		static constexpr const int32_t s_nTabMounts = 1;
		//Gtk::Label* m_p0TabLabelMounts = nullptr;
		//Gtk::Box* m_p0VBoxMounts = nullptr;
			//Gtk::ScrolledWindow* m_p0ScrolledMounts = nullptr;
				Gtk::TreeView* m_p0TreeViewMounts = nullptr;

		static constexpr const int32_t s_nTabInputs = 2;
		//Gtk::Label* m_p0TabLabelInputs = nullptr;
		//Gtk::Box* m_p0VBoxInputs = nullptr;
			//Gtk::ScrolledWindow* m_p0ScrolledInputs = nullptr;
				Gtk::TreeView* m_p0TreeViewInputs = nullptr;

		static constexpr const int32_t s_nTabLog = 3;
		//Gtk::Label* m_p0TabLabelLog = nullptr;
		//Gtk::ScrolledWindow* m_p0ScrolledLog = nullptr;
			Gtk::TextView* m_p0TextViewLog = nullptr;

		static constexpr const int32_t s_nTabInfo = 4;
		//Gtk::Label* m_p0TabLabelInfo = nullptr;
		//Gtk::ScrolledWindow* m_p0ScrolledInfo = nullptr;
			//Gtk::Label* m_p0LabelInfoText = nullptr;

	//Gtk::Box* m_p0HBoxStateControl = nullptr;
		Gtk::Button* m_p0ButtonStartRecording = nullptr;
		Gtk::Button* m_p0ButtonStopRecording = nullptr;
		Gtk::Button* m_p0ButtonUnmountNonBusy = nullptr;
		Gtk::Button* m_p0ButtonTellState = nullptr;

	class InputsColumns : public Gtk::TreeModel::ColumnRecord
	{
	public:
		InputsColumns() noexcept
		{
			add(m_oColDeviceName); add(m_oColCapabilityClassId); add(m_oColCapabilityId);
			add(m_oColHiddenDeviceId);
		}
		Gtk::TreeModelColumn<Glib::ustring> m_oColDeviceName;
		Gtk::TreeModelColumn<Glib::ustring> m_oColCapabilityClassId;
		Gtk::TreeModelColumn<int32_t> m_oColCapabilityId;
		Gtk::TreeModelColumn<int32_t> m_oColHiddenDeviceId;
	};
	InputsColumns m_oInputsColumns;
	Glib::RefPtr<Gtk::TreeStore> m_refTreeModelInputs;

	class MountsColumns : public Gtk::TreeModel::ColumnRecord
	{
	public:
		MountsColumns() noexcept
		{
			add(m_oColMountName); add(m_oColMountRoot); add(m_oColMountUUID);
			add(m_oMountFreeSpace);
		}
		Gtk::TreeModelColumn<Glib::ustring> m_oColMountName;
		Gtk::TreeModelColumn<Glib::ustring> m_oColMountRoot;
		Gtk::TreeModelColumn<Glib::ustring> m_oColMountUUID;
		Gtk::TreeModelColumn<int64_t> m_oMountFreeSpace;
	};
	MountsColumns m_oMountsColumns;
	Glib::RefPtr<Gtk::TreeStore> m_refTreeModelMounts;


	Glib::RefPtr<Gtk::TextBuffer> m_refTextBufferLog;
	Glib::RefPtr<Gtk::TextBuffer::Mark> m_refTextBufferMarkBottom;
	int32_t m_nTextBufferLogTotLines;
	static constexpr const int32_t s_nTextBufferLogMaxLines = 500;

	Glib::RefPtr<Gtk::TextBuffer> m_refTextBufferInfo;

	bool m_bRegenerateDevicesInProgress = false;
	shared_ptr<stmi::EventListener> m_refEventListener;

	bool m_bRegenerateMountsInProgress = false;

	bool m_bQuitting = false;

	static constexpr const int32_t s_nInitialWindowSizeW = 400;
	static constexpr const int32_t s_nInitialWindowSizeH = 600;

	int32_t m_nStatusCounter;
	int32_t m_nSubStatusCounter;
	sigc::connection m_oStatusConn;
private:
	SonoWindow() = delete;
};

} // namespace sono

#endif /* SONO_SONO_WINDOW_H */

