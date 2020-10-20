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
 * File:   sonowindow.cc
 */

#include "sonowindow.h"

#include "config.h"
#include "util.h"

#include <stmm-input/device.h>

#include <pangomm.h>
#include <sigc++/sigc++.h>

#include <algorithm>
#include <cassert>
#include <iostream>

#include <errno.h>
#include <unistd.h>

namespace sono
{

using std::shared_ptr;

static constexpr int32_t s_nStatusCounterResetMillisec = 5000;

static constexpr int32_t s_nQuitDelaySeconds = 60;

static constexpr int32_t s_nWindowToTheTopSeconds = 2;
static constexpr int32_t s_nAutoStartRecordingAfterSeconds = 1;

SonoWindow::SonoWindow(SonoModel& oModel, bool bVerbose, bool bDebug, const std::string& sSpeechApp, bool bKeepOnTop
						, const shared_ptr<stmi::DeviceManager>& refDM, const std::string& sTitle) noexcept
: Gtk::Window()
, m_oModel(oModel)
, m_oLogger(m_oModel.getLogger())
, m_bVerbose(bVerbose)
, m_nDebugCtxDepth(bDebug ? 0 : -1)
, m_sSpeechApp(sSpeechApp)
, m_refDM(refDM)
, m_nTextBufferLogTotLines(0)
, m_nStatusCounter(0)
, m_nSubStatusCounter(0)
{
	//
	set_title(sTitle);
	set_default_size(s_nInitialWindowSizeW, s_nInitialWindowSizeH);
	set_resizable(true);

	const std::string sInitialLogText = "";

	const std::string sInfoTitle = "sonorem";
	const std::string sInfoText = std::string{"\n"} +
			"Version: " + Config::getVersionString() + "\n\n"
			"Copyright © 2020  Stefano Marsili, <stemars@gmx.ch>.\n"
			"\n"
			"Use > sonorem < to record sound from a microphone for"
			" hours or even days. By default, it creates " + m_oModel.getRecordingFileExt() + " files"
			" that are automatically moved to any plugged in usb sticks,"
			" which are auto-mounted.\n"
			"This app is meant to be used on a Raspberry Pi with"
			" no display and only a mini keyboard or a joystick as an"
			" input device.\n"
			"To assess the current status you need ear-phones.\n"
			"Read the man page (man sonorem) for more information.\n"
			"";

	Glib::RefPtr<Gtk::TreeSelection> refTreeSelection;
	Pango::FontDescription oMonoFont("Mono");

	Gtk::Box* m_p0VBoxWhole = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
	Gtk::Window::add(*m_p0VBoxWhole);

	Gtk::Notebook* m_p0NotebookChoices = Gtk::manage(new Gtk::Notebook());
	m_p0VBoxWhole->pack_start(*m_p0NotebookChoices, true, true);

	Gtk::Label* m_p0TabLabelMain = Gtk::manage(new Gtk::Label("Main"));
	Gtk::Box* m_p0VBoxMain = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
	m_aPageIndex[s_nTabMain] = m_p0NotebookChoices->append_page(*m_p0VBoxMain, *m_p0TabLabelMain);
		m_p0VBoxMain->set_spacing(4);
		m_p0VBoxMain->set_border_width(5);

		Gtk::Grid* m_p0GridState = Gtk::manage(new Gtk::Grid());
		m_p0VBoxMain->pack_start(*m_p0GridState, true, true);

			int32_t nGridRow = 0;
			Gtk::Label* m_p0LabelState = Gtk::manage(new Gtk::Label("State"));
			m_p0GridState->attach(*m_p0LabelState, 0, nGridRow, 1, 1);
                m_p0LabelState->set_halign(Gtk::Align::ALIGN_START);
			//
			m_p0EntryState = Gtk::manage(new Gtk::Entry());
			m_p0GridState->attach(*m_p0EntryState, 1, nGridRow, 1, 1);
				m_p0EntryState->set_can_focus(false);
				m_p0EntryState->set_editable(false);
				m_p0EntryState->set_hexpand(true);
			//
			++nGridRow;
			Gtk::Label* m_p0LabelFreeDiskSpace = Gtk::manage(new Gtk::Label("Free space (MB):"));
			m_p0GridState->attach(*m_p0LabelFreeDiskSpace, 0, nGridRow, 1, 1);
                m_p0LabelFreeDiskSpace->set_halign(Gtk::Align::ALIGN_START);
			//
			m_p0EntryFreeDiskSpace = Gtk::manage(new Gtk::Entry());
			m_p0GridState->attach(*m_p0EntryFreeDiskSpace, 1, nGridRow, 1, 1);
				m_p0EntryFreeDiskSpace->set_can_focus(false);
				m_p0EntryFreeDiskSpace->set_editable(false);
				m_p0EntryFreeDiskSpace->set_hexpand(true);
			//
			++nGridRow;
			Gtk::Label* m_p0LabelRecordingFilePath = Gtk::manage(new Gtk::Label("Writing file:"));
			m_p0GridState->attach(*m_p0LabelRecordingFilePath, 0, nGridRow, 1, 1);
                m_p0LabelRecordingFilePath->set_halign(Gtk::Align::ALIGN_START);
			//
			m_p0EntryRecordingFilePath = Gtk::manage(new Gtk::Entry());
			m_p0GridState->attach(*m_p0EntryRecordingFilePath, 1, nGridRow, 1, 1);
				m_p0EntryRecordingFilePath->set_can_focus(false);
				m_p0EntryRecordingFilePath->set_editable(false);
				m_p0EntryRecordingFilePath->set_hexpand(true);
			//
			++nGridRow;
			Gtk::Label* m_p0LabelRecordingFileSize = Gtk::manage(new Gtk::Label("        size:"));
			m_p0GridState->attach(*m_p0LabelRecordingFileSize, 0, nGridRow, 1, 1);
                m_p0LabelRecordingFileSize->set_halign(Gtk::Align::ALIGN_START);
			//
			m_p0EntryRecordingFileSize = Gtk::manage(new Gtk::Entry());
			m_p0GridState->attach(*m_p0EntryRecordingFileSize, 1, nGridRow, 1, 1);
				m_p0EntryRecordingFileSize->set_can_focus(false);
				m_p0EntryRecordingFileSize->set_editable(false);
				m_p0EntryRecordingFileSize->set_hexpand(true);
			//
			++nGridRow;
			Gtk::Label* m_p0LabelCopyingFilePath = Gtk::manage(new Gtk::Label("Copying file:"));
			m_p0GridState->attach(*m_p0LabelCopyingFilePath, 0, nGridRow, 1, 1);
                m_p0LabelCopyingFilePath->set_halign(Gtk::Align::ALIGN_START);
			//
			m_p0EntryCopyingFilePath = Gtk::manage(new Gtk::Entry());
			m_p0GridState->attach(*m_p0EntryCopyingFilePath, 1, nGridRow, 1, 1);
				m_p0EntryCopyingFilePath->set_can_focus(false);
				m_p0EntryCopyingFilePath->set_editable(false);
				m_p0EntryCopyingFilePath->set_hexpand(true);
			//
			++nGridRow;
			Gtk::Label* m_p0LabelSyncingFilePath = Gtk::manage(new Gtk::Label("Syncing file:"));
			m_p0GridState->attach(*m_p0LabelSyncingFilePath, 0, nGridRow, 1, 1);
                m_p0LabelSyncingFilePath->set_halign(Gtk::Align::ALIGN_START);
			//
			m_p0EntrySyncingFilePath = Gtk::manage(new Gtk::Entry());
			m_p0GridState->attach(*m_p0EntrySyncingFilePath, 1, nGridRow, 1, 1);
				m_p0EntrySyncingFilePath->set_can_focus(false);
				m_p0EntrySyncingFilePath->set_editable(false);
				m_p0EntrySyncingFilePath->set_hexpand(true);
			//
			++nGridRow;
			Gtk::Label* m_p0LabelRemovingFilePath = Gtk::manage(new Gtk::Label("Removing file:"));
			m_p0GridState->attach(*m_p0LabelRemovingFilePath, 0, nGridRow, 1, 1);
                m_p0LabelRemovingFilePath->set_halign(Gtk::Align::ALIGN_START);
			//
			m_p0EntryRemovingFilePath = Gtk::manage(new Gtk::Entry());
			m_p0GridState->attach(*m_p0EntryRemovingFilePath, 1, nGridRow, 1, 1);
				m_p0EntryRemovingFilePath->set_can_focus(false);
				m_p0EntryRemovingFilePath->set_editable(false);
				m_p0EntryRemovingFilePath->set_hexpand(true);
			//
			++nGridRow;
			Gtk::Label* m_p0LabelNrToBeCopiedFiles = Gtk::manage(new Gtk::Label("To be copied:"));
			m_p0GridState->attach(*m_p0LabelNrToBeCopiedFiles, 0, nGridRow, 1, 1);
                m_p0LabelNrToBeCopiedFiles->set_halign(Gtk::Align::ALIGN_START);
			//
			m_p0EntryNrToBeCopiedFiles = Gtk::manage(new Gtk::Entry());
			m_p0GridState->attach(*m_p0EntryNrToBeCopiedFiles, 1, nGridRow, 1, 1);
				m_p0EntryNrToBeCopiedFiles->set_can_focus(false);
				m_p0EntryNrToBeCopiedFiles->set_editable(false);
				m_p0EntryNrToBeCopiedFiles->set_hexpand(true);
			//
			++nGridRow;
			Gtk::Label* m_p0LabelNrToBeSyncedFiles = Gtk::manage(new Gtk::Label("To be synced:"));
			m_p0GridState->attach(*m_p0LabelNrToBeSyncedFiles, 0, nGridRow, 1, 1);
                m_p0LabelNrToBeSyncedFiles->set_halign(Gtk::Align::ALIGN_START);
			//
			m_p0EntryNrToBeSyncedFiles = Gtk::manage(new Gtk::Entry());
			m_p0GridState->attach(*m_p0EntryNrToBeSyncedFiles, 1, nGridRow, 1, 1);
				m_p0EntryNrToBeSyncedFiles->set_can_focus(false);
				m_p0EntryNrToBeSyncedFiles->set_editable(false);
				m_p0EntryNrToBeSyncedFiles->set_hexpand(true);
			//
			++nGridRow;
			Gtk::Label* m_p0LabelNrToBeRemovedFiles = Gtk::manage(new Gtk::Label("To be removed:"));
			m_p0GridState->attach(*m_p0LabelNrToBeRemovedFiles, 0, nGridRow, 1, 1);
                m_p0LabelNrToBeRemovedFiles->set_halign(Gtk::Align::ALIGN_START);
			//
			m_p0EntryNrToBeRemovedFiles = Gtk::manage(new Gtk::Entry());
			m_p0GridState->attach(*m_p0EntryNrToBeRemovedFiles, 1, nGridRow, 1, 1);
				m_p0EntryNrToBeRemovedFiles->set_can_focus(false);
				m_p0EntryNrToBeRemovedFiles->set_editable(false);
				m_p0EntryNrToBeRemovedFiles->set_hexpand(true);
			//
			++nGridRow;
			Gtk::Label* m_p0LabelUnmountingRootPath = Gtk::manage(new Gtk::Label("Unmounting path:"));
			m_p0GridState->attach(*m_p0LabelUnmountingRootPath, 0, nGridRow, 1, 1);
                m_p0LabelUnmountingRootPath->set_halign(Gtk::Align::ALIGN_START);
			//
			m_p0EntryUnmountingRootPath = Gtk::manage(new Gtk::Entry());
			m_p0GridState->attach(*m_p0EntryUnmountingRootPath, 1, nGridRow, 1, 1);
				m_p0EntryUnmountingRootPath->set_can_focus(false);
				m_p0EntryUnmountingRootPath->set_editable(false);
				m_p0EntryUnmountingRootPath->set_hexpand(true);


	Gtk::Label* m_p0TabLabelMounts = Gtk::manage(new Gtk::Label("Mounts"));
	Gtk::Box* m_p0VBoxMounts = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
	m_aPageIndex[s_nTabMounts] = m_p0NotebookChoices->append_page(*m_p0VBoxMounts, *m_p0TabLabelMounts);

		Gtk::ScrolledWindow* m_p0ScrolledMounts = Gtk::manage(new Gtk::ScrolledWindow());
		m_p0VBoxMounts->pack_start(*m_p0ScrolledMounts, true, true);
			m_p0ScrolledMounts->set_border_width(1);
			m_p0ScrolledMounts->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_ALWAYS);

			m_refTreeModelMounts = Gtk::TreeStore::create(m_oMountsColumns);
			m_p0TreeViewMounts = Gtk::manage(new Gtk::TreeView(m_refTreeModelMounts));
			m_p0ScrolledMounts->add(*m_p0TreeViewMounts);
				m_p0TreeViewMounts->append_column("Path", m_oMountsColumns.m_oColMountRoot);
				m_p0TreeViewMounts->append_column("Free (MB)", m_oMountsColumns.m_oMountFreeSpace);
				m_p0TreeViewMounts->append_column("Name", m_oMountsColumns.m_oColMountName);
				m_p0TreeViewMounts->append_column("UUID", m_oMountsColumns.m_oColMountUUID);
				m_p0TreeViewMounts->set_can_focus(false);


	Gtk::Label* m_p0TabLabelInputs = Gtk::manage(new Gtk::Label("Inputs"));
	Gtk::Box* m_p0VBoxInputs = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
	m_aPageIndex[s_nTabInputs] = m_p0NotebookChoices->append_page(*m_p0VBoxInputs, *m_p0TabLabelInputs);

		Gtk::ScrolledWindow* m_p0ScrolledInputs = Gtk::manage(new Gtk::ScrolledWindow());
		m_p0VBoxInputs->pack_start(*m_p0ScrolledInputs, true, true);
			m_p0ScrolledInputs->set_border_width(1);
			m_p0ScrolledInputs->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_ALWAYS);

			m_refTreeModelInputs = Gtk::TreeStore::create(m_oInputsColumns);
			m_p0TreeViewInputs = Gtk::manage(new Gtk::TreeView(m_refTreeModelInputs));
			m_p0ScrolledInputs->add(*m_p0TreeViewInputs);
				m_p0TreeViewInputs->append_column("Device", m_oInputsColumns.m_oColDeviceName);
				m_p0TreeViewInputs->append_column("Class", m_oInputsColumns.m_oColCapabilityClassId);
				m_p0TreeViewInputs->append_column("Id", m_oInputsColumns.m_oColCapabilityId);
				m_p0TreeViewInputs->append_column("DevId", m_oInputsColumns.m_oColHiddenDeviceId);
				m_p0TreeViewInputs->set_can_focus(false);


	Gtk::Label* m_p0TabLabelLog = Gtk::manage(new Gtk::Label("Log"));
	Gtk::ScrolledWindow* m_p0ScrolledLog = Gtk::manage(new Gtk::ScrolledWindow());
	m_aPageIndex[s_nTabLog] = m_p0NotebookChoices->append_page(*m_p0ScrolledLog, *m_p0TabLabelLog);
		m_p0ScrolledLog->set_border_width(5);
		m_p0ScrolledLog->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_ALWAYS);

		m_p0TextViewLog = Gtk::manage(new Gtk::TextView());
		m_p0ScrolledLog->add(*m_p0TextViewLog);
			m_refTextBufferLog = Gtk::TextBuffer::create();
			m_refTextBufferMarkBottom = Gtk::TextBuffer::Mark::create("Bottom");
			m_p0TextViewLog->set_can_focus(false);
			m_p0TextViewLog->set_editable(false);
			m_p0TextViewLog->set_buffer(m_refTextBufferLog);
			m_refTextBufferLog->set_text(sInitialLogText);
			m_p0TextViewLog->override_font(oMonoFont);
			m_refTextBufferLog->add_mark(m_refTextBufferMarkBottom, m_refTextBufferLog->end());

	Gtk::Label* m_p0TabLabelInfo = Gtk::manage(new Gtk::Label("Info"));
	Gtk::ScrolledWindow* m_p0ScrolledInfo = Gtk::manage(new Gtk::ScrolledWindow());
	m_aPageIndex[s_nTabInfo] = m_p0NotebookChoices->append_page(*m_p0ScrolledInfo, *m_p0TabLabelInfo);
		m_p0ScrolledInfo->set_border_width(5);
		m_p0ScrolledInfo->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

		Gtk::Box* m_p0VBoxInfoText = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
		m_p0ScrolledInfo->add(*m_p0VBoxInfoText);
			Gtk::Label* m_p0LabelInfoTitle = Gtk::manage(new Gtk::Label(sInfoTitle));
			m_p0VBoxInfoText->pack_start(*m_p0LabelInfoTitle, false, false);
				m_p0LabelInfoTitle->set_halign(Gtk::Align::ALIGN_START);
				m_p0LabelInfoTitle->set_margin_top(3);
				m_p0LabelInfoTitle->set_margin_bottom(3);
				{
				Pango::AttrList oAttrList;
				Pango::AttrInt oAttrWeight = Pango::Attribute::create_attr_weight(Pango::WEIGHT_HEAVY);
				oAttrList.insert(oAttrWeight);
				m_p0LabelInfoTitle->set_attributes(oAttrList);
				}

			Gtk::Label* m_p0LabelInfoText = Gtk::manage(new Gtk::Label(sInfoText));
			m_p0VBoxInfoText->pack_start(*m_p0LabelInfoText, true, true);
				m_p0LabelInfoText->set_halign(Gtk::Align::ALIGN_START);
				m_p0LabelInfoText->set_valign(Gtk::Align::ALIGN_START);
				m_p0LabelInfoText->set_line_wrap(true);
				m_p0LabelInfoText->set_line_wrap_mode(Pango::WRAP_WORD_CHAR);

	Gtk::Box* m_p0HBoxStateControl = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
	m_p0VBoxWhole->pack_start(*m_p0HBoxStateControl, false, false);
		m_p0HBoxStateControl->set_spacing(6);

		m_p0ButtonStartRecording = Gtk::manage(new Gtk::Button("Start"));
		m_p0HBoxStateControl->pack_start(*m_p0ButtonStartRecording);
			m_p0ButtonStartRecording->signal_clicked().connect(
							sigc::mem_fun(*this, &SonoWindow::onButtonStartRecording) );

		m_p0ButtonStopRecording = Gtk::manage(new Gtk::Button("Stop"));
		m_p0HBoxStateControl->pack_start(*m_p0ButtonStopRecording);
			m_p0ButtonStopRecording->signal_clicked().connect(
							sigc::mem_fun(*this, &SonoWindow::onButtonStopRecording) );

		m_p0ButtonUnmountNonBusy = Gtk::manage(new Gtk::Button("Unmount"));
		m_p0HBoxStateControl->pack_start(*m_p0ButtonUnmountNonBusy);
			m_p0ButtonUnmountNonBusy->signal_clicked().connect(
							sigc::mem_fun(*this, &SonoWindow::onButtonUnmountNonBusy) );

		m_p0ButtonTellState = Gtk::manage(new Gtk::Button("Status"));
		m_p0HBoxStateControl->pack_start(*m_p0ButtonTellState);
			m_p0ButtonTellState->signal_clicked().connect(
							sigc::mem_fun(*this, &SonoWindow::onButtonTellState) );


	m_refEventListener = std::make_shared<stmi::EventListener>(
		[this](const shared_ptr<stmi::Event>& refEvent)
		{
			onStmiEvent(refEvent);
		});
	#ifndef NDEBUG
	const bool bAdded =
	#endif
	m_refDM->addEventListener(m_refEventListener);
	assert(bAdded);

	m_oModel.m_oMountsChangedSignal.connect( sigc::mem_fun(this, &SonoWindow::mountsChanged) );
	m_oModel.m_oStateChangedSignal.connect( sigc::mem_fun(this, &SonoWindow::stateChangedSignal) );
	m_oModel.m_oRecordingFsFreeMBChangedSignal.connect( sigc::mem_fun(this, &SonoWindow::stateChangedSignal) );
	m_oModel.m_oTellStatusSignal.connect( sigc::mem_fun(this, &SonoWindow::tellStatus) );
	m_oModel.m_oQuitSignal.connect( sigc::mem_fun(this, &SonoWindow::quitSignal) );

	regenerateDevicesList();
	regenerateMountsList();
	refreshState();

	show_all_children();

	if (m_oModel.isAutoStart()) {
		Glib::signal_timeout().connect_seconds_once(sigc::mem_fun(*this, &SonoWindow::autoStart), s_nAutoStartRecordingAfterSeconds);
	}
	if (bKeepOnTop) {
		Glib::signal_timeout().connect_seconds(sigc::mem_fun(*this, &SonoWindow::toTheFront), s_nWindowToTheTopSeconds);
	}
	property_is_active().signal_changed().connect(sigc::mem_fun(this, &SonoWindow::onSigIsActiveChanged));
}
void SonoWindow::onSigIsActiveChanged() noexcept
{
	if (m_nDebugCtxDepth < 0) {
		return;
	}
	if (isWindowActive()) {
		log("Window has now focus!");
	} else {
		log("Window no longer has focus!");
	}
}
bool SonoWindow::isWindowActive() const noexcept
{
	return get_realized() && get_visible() && is_active();
}
void SonoWindow::autoStart() noexcept
{
	log("Autostart recording");
	startRecording();
}
void SonoWindow::onNotebookSwitchPage(Gtk::Widget*, guint /*nPageNum*/) noexcept
{
}
void SonoWindow::onButtonStartRecording() noexcept
{
	startRecording();
}
void SonoWindow::onButtonStopRecording() noexcept
{
	stopRecording();
}
void SonoWindow::onButtonUnmountNonBusy() noexcept
{
	unmountNonBusy();
}
bool SonoWindow::toTheFront() noexcept
{
	const bool bContinue = true;
	raise();
	return bContinue;
}
void SonoWindow::onButtonTellState() noexcept
{
	tellStatus();
}
void SonoWindow::regenerateDevicesList() noexcept
{
//std::cout << "regenerateDevicesList()" << '\n';
	assert(!m_bRegenerateDevicesInProgress);
	m_bRegenerateDevicesInProgress = true;
	m_refTreeModelInputs->clear();
	const auto aDeviceIds = m_refDM->getDevices();
	for (int32_t nDeviceId : aDeviceIds) {
		auto refDevice = m_refDM->getDevice(nDeviceId);
		assert(refDevice);
		const std::string& sDeviceName = refDevice->getName();
//std::cout << "regenerateDevicesList()  sDeviceName " << sDeviceName << "   nDeviceId=" << nDeviceId << '\n';
		const auto aCapabilityIds = refDevice->getCapabilities();
		for (int32_t nCapabilityId : aCapabilityIds) {
			auto refCapability = refDevice->getCapability(nCapabilityId);
//std::cout << "regenerateDevicesList()  nCapabilityId " << nCapabilityId << '\n';
			assert(refCapability);
			const stmi::Capability::Class& oClass = refCapability->getCapabilityClass();
			assert(oClass);
			//if (oClass == stmi::PlaybackCapability::getClass()) {
			//	continue;
			//}
			const std::string sCapaClassId = getCapabilityClassId(oClass);
			//
			Gtk::TreeModel::Row oRow;
			oRow = *(m_refTreeModelInputs->append());
			oRow[m_oInputsColumns.m_oColDeviceName] = sDeviceName;
			oRow[m_oInputsColumns.m_oColCapabilityClassId] = sCapaClassId;
			oRow[m_oInputsColumns.m_oColCapabilityId] = nCapabilityId;
			oRow[m_oInputsColumns.m_oColHiddenDeviceId] = nDeviceId;
		}
	}
	m_p0TreeViewInputs->expand_all();
	m_bRegenerateDevicesInProgress = false;
}
std::string SonoWindow::getCapabilityClassId(const stmi::Capability::Class& oClass) const noexcept
{
	std::string sCapaClassId = oClass.getId();
	auto nLastPos = sCapaClassId.rfind("::");
	if (nLastPos != std::string::npos) {
		sCapaClassId.erase(0, nLastPos + 2);
	}
	return sCapaClassId;
}
void SonoWindow::onStmiEvent(const shared_ptr<stmi::Event>& refEvent) noexcept
{
	const bool bDeviceMgmt = refEvent->getCapability()->getCapabilityClass().isDeviceManagerCapability();
	if (bDeviceMgmt) {
//std::cout << "SonoWindow::onStmiEvent() " << refEvent->getEventClass().getId() << '\n';
		regenerateDevicesList();
	}
	stmi::HARDWARE_KEY eKey;
	stmi::Event::AS_KEY_INPUT_TYPE eType;
	bool bMore;
	const bool bOk = refEvent->getAsKey(eKey, eType, bMore);
	if ((!bOk) || bMore || (eType != stmi::Event::AS_KEY_PRESS)) {
		return; //--------------------------------------------------------------
	}
	if (m_bVerbose) {
		//
		const int32_t nKey = static_cast<int32_t>(eKey);
		auto refCapa = refEvent->getCapability();
		std::string sDevice;
		if (refCapa) {
			auto refDevice = refCapa->getDevice();
			if (refDevice) {
				sDevice = refDevice->getName();
			}
		}
		log("Device: " + sDevice + "  Key: " + std::to_string(nKey));
	}
	if ((eKey == stmi::HK_1) || (eKey == stmi::HK_KP1) || (eKey == stmi::HK_BTN_A)) {
		startRecording();
	} else if ((eKey == stmi::HK_2) || (eKey == stmi::HK_KP2) || (eKey == stmi::HK_BTN_B)) {
		stopRecording();
	} else if ((eKey == stmi::HK_3) || (eKey == stmi::HK_KP3) || (eKey == stmi::HK_BTN_X)) {
		unmountNonBusy();
	} else if ((eKey == stmi::HK_4) || (eKey == stmi::HK_KP4) || (eKey == stmi::HK_BTN_Y)) {
		tellStatus();
	} else if ((eKey == stmi::HK_5) || (eKey == stmi::HK_KP5) || (eKey == stmi:: HK_BTN_START)) {
		m_nStatusCounter = 3;
		tellStatus();
	}
}

void SonoWindow::quitSignal() noexcept
{
	if (m_bVerbose) {
		log("Quit signaled!");
	}
	if (m_bQuitting) {
		return;
	}
	m_bQuitting = true;
	log("Quitting in " + std::to_string(s_nQuitDelaySeconds) + " seconds");
	Glib::signal_timeout().connect_seconds_once(sigc::mem_fun(*this, &SonoWindow::quitNow), s_nQuitDelaySeconds);
}
void SonoWindow::quitNow() noexcept
{
	hide();
}
void SonoWindow::mountsChanged() noexcept
{
	m_nSubStatusCounter = -1;
	regenerateMountsList();
}
void SonoWindow::stateChangedSignal() noexcept
{
	refreshState();
}
void SonoWindow::refreshState() noexcept
{
	DebugCtx<SonoWindow> oCtx(this, "SonoWindow::refreshState");

	const SonoModel::STATE eState = m_oModel.getState();
	const bool bStopped = (eState == SonoModel::STATE_STOPPED);
	m_p0ButtonStartRecording->set_sensitive(bStopped);
	m_p0ButtonStopRecording->set_sensitive(! bStopped);
	//
	const std::string sState = [&]()
	{
		if (bStopped) {
			return "STOPPED";
		} else if (eState == SonoModel::STATE_RECORDING) {
			return "RECORDING";
		} else if (eState == SonoModel::STATE_WAITING_FOR_SPACE) {
			return "WAITING-FOR-SPACE";
		} else {
			assert(false);
			return "???";
		}
	}();
	m_p0EntryState->set_text(sState);
	//
	const std::string& sRecordingFile = m_oModel.getRecordingFilePath();
	m_p0EntryRecordingFilePath->set_text(sRecordingFile);
	//
	const int64_t nSizeBytes = m_oModel.getRecordingSizeBytes();
	if (! sRecordingFile.empty()) {
		m_p0EntryRecordingFileSize->set_text(getSizeStringFromBytes(nSizeBytes, false));
	} else {
		m_p0EntryRecordingFileSize->set_text("");
	}

	//
	const int64_t nFreeMB = m_oModel.getRecordingFsFreeMB();
	m_p0EntryFreeDiskSpace->set_text(std::to_string(nFreeMB));
	//
	const std::string sCopyingFile = m_oModel.getCopyingFromFilePath();
	m_p0EntryCopyingFilePath->set_text(sCopyingFile);
	//
	const std::string sSyncingFile = m_oModel.getSyncingFilePath();
	m_p0EntrySyncingFilePath->set_text(sSyncingFile);
	//
	const std::string sRemovingFile = m_oModel.getRemovingFilePath();
	m_p0EntryRemovingFilePath->set_text(sRemovingFile);
	//
	const int32_t nNrToBeCopied = m_oModel.getNrToBeCopiedRecordings();
	m_p0EntryNrToBeCopiedFiles->set_text(std::to_string(nNrToBeCopied));
	//
	const int32_t nNrToBeSynced = m_oModel.getNrToBeSyncedRecordings();
	m_p0EntryNrToBeSyncedFiles->set_text(std::to_string(nNrToBeSynced));
	//
	const int32_t nNrToBeRemoved = m_oModel.getNrToBeRemovedRecordings();
	m_p0EntryNrToBeRemovedFiles->set_text(std::to_string(nNrToBeRemoved));
	//
	const std::string& sUnmountingDir = m_oModel.getUnmountingMountRootPath();
	m_p0EntryUnmountingRootPath->set_text(sUnmountingDir);
}
void SonoWindow::regenerateMountsList() noexcept
{
	DebugCtx<SonoWindow> oCtx(this, "SonoWindow::regenerateMountsList");

	assert(!m_bRegenerateMountsInProgress);
	m_bRegenerateMountsInProgress = true;
	m_refTreeModelMounts->clear();
	auto& aMountInfos = m_oModel.getMountInfos();
	for (auto& oMountInfo : aMountInfos) {
		
		Gtk::TreeModel::Row oRow;
		oRow = *(m_refTreeModelMounts->append());
		oRow[m_oMountsColumns.m_oColMountName] = oMountInfo.m_sName;
		oRow[m_oMountsColumns.m_oColMountRoot] = oMountInfo.m_sRootPath;
		oRow[m_oMountsColumns.m_oColMountUUID] = oMountInfo.m_sUUID;
		oRow[m_oMountsColumns.m_oMountFreeSpace] = oMountInfo.m_nFreeMB;
	}
	m_p0TreeViewMounts->expand_all();
	m_bRegenerateMountsInProgress = false;
}

void SonoWindow::log(const std::string& sStr) noexcept
{
	m_oLogger(sStr);
}
void SonoWindow::logToWindow(const std::string& sStr) noexcept
{
	if (m_nTextBufferLogTotLines >= s_nTextBufferLogMaxLines) {
		auto itLine1 = m_refTextBufferLog->get_iter_at_line(1);
		m_refTextBufferLog->erase(m_refTextBufferLog->begin(), itLine1);
		--m_nTextBufferLogTotLines;
	}
	m_refTextBufferLog->insert(m_refTextBufferLog->end(), sStr + "\n");
	++m_nTextBufferLogTotLines;

	m_refTextBufferLog->place_cursor(m_refTextBufferLog->end());
	m_refTextBufferLog->move_mark(m_refTextBufferMarkBottom, m_refTextBufferLog->end());
	m_p0TextViewLog->scroll_to(m_refTextBufferMarkBottom, 0.1);
}

void SonoWindow::startRecording() noexcept
{
	DebugCtx<SonoWindow> oCtx{this, "SonoWindow::startRecording"};
	tellString("started recording");
	m_oModel.startRecording();

	Glib::signal_timeout().connect_once(sigc::mem_fun(*this, &SonoWindow::refreshState), 0);
}
void SonoWindow::stopRecording() noexcept
{
	tellString("stopped recording");
	m_oModel.stopRecording();

	Glib::signal_timeout().connect_once(sigc::mem_fun(*this, &SonoWindow::refreshState), 0);
}
void SonoWindow::unmountNonBusy() noexcept
{
	tellString("unmounting non busy mounts with recordings");
	m_oModel.unmountNonBusy();

	Glib::signal_timeout().connect_once(sigc::mem_fun(*this, &SonoWindow::refreshState), 0);
}
std::string SonoWindow::getSizeStringFromBytes(int64_t nSizeBytes, bool bLongUnit) const noexcept
{
	std::string sUnit;
	const int64_t nSizeInUnit = [&]() {
		if (nSizeBytes > 1000 * 1000) {
			sUnit = (bLongUnit ? "MegaBytes" : "MB");
			return nSizeBytes / (1000 * 1000);
		} else if (nSizeBytes > 1000) {
			sUnit = (bLongUnit ? "KiloBytes" : "KB");
			return nSizeBytes / 1000;
		} else {
			sUnit = (bLongUnit ? "Bytes" : "B");
			return nSizeBytes;
		}
	}();
	return std::to_string(nSizeInUnit) + " " + sUnit;
}
std::string SonoWindow::getTimeStringFromSeconds(int64_t nSeconds) const noexcept
{
	std::string sRes;
	const int32_t nSecs = nSeconds % 60;
	const int32_t nMins = (nSeconds / 60) % 60;
	const int32_t nHours = nSeconds / 60 / 60;
	if (nHours > 0) {
		sRes += std::to_string(nHours) + " hours ";
	}
	if (nMins > 0) {
		sRes += std::to_string(nMins) + " minutes ";
	}
	if (nHours == 0) {
		sRes += std::to_string(nSecs) + " seconds";
	}
	return sRes;
}

void SonoWindow::tellStatus() noexcept
{
	DebugCtx<SonoWindow> oCtx(this, "SonoWindow::tellStatus");

	m_oStatusConn.disconnect();

	const SonoModel::STATE eState = m_oModel.getState();

	switch (m_nStatusCounter) {
	case 0: {
		if (eState == SonoModel::STATE_STOPPED) {
			tellString("Stopped");
		} else if (eState == SonoModel::STATE_RECORDING) {
			tellString("Recording");
		} else if (eState == SonoModel::STATE_WAITING_FOR_SPACE) {
			tellString("Waiting for space");
		} else {
			assert(false);
		}
		if (m_bQuitting) {
			tellString("Quitting");
		}
	} break;
	case 1: {
		if (eState == SonoModel::STATE_RECORDING) {
			const int32_t nRecordingSizeBytes = m_oModel.getRecordingSizeBytes();
			tellString("File size: " + getSizeStringFromBytes(nRecordingSizeBytes, true));
			break;
		}
		++m_nStatusCounter;
	} // fallthrough
	case 2: {
		if (eState == SonoModel::STATE_RECORDING) {
			const int32_t nRecordingElapsedSeconds = m_oModel.getRecordingElapsedSeconds();
			tellString("Elapsed: " + getTimeStringFromSeconds(nRecordingElapsedSeconds));
			break;
		}
		++m_nStatusCounter;
	} // fallthrough
	case 3: {
		const int32_t nNrWaitingForKilled = m_oModel.getNrWaitingForKilledProcesses();
		if (nNrWaitingForKilled > 0) {
			tellString("Number of waiting for killed processes: " + std::to_string(nNrWaitingForKilled));
			break;
		}
		++m_nStatusCounter;
	} // fallthrough
	case 4: {
		const int32_t nNrToBeCopied = m_oModel.getNrToBeCopiedRecordings();
		if (nNrToBeCopied > 0) {
			tellString("Number of to be copied files: " + std::to_string(nNrToBeCopied));
			break;
		}
		++m_nStatusCounter;
	} // fallthrough
	case 5: {
		const int32_t nNrToBeRemoved = m_oModel.getNrToBeRemovedRecordings();
		if (nNrToBeRemoved > 0) {
			tellString("Number of to be removed files: " + std::to_string(nNrToBeRemoved));
			break;
		}
		++m_nStatusCounter;
	} // fallthrough
	case 6: {
		const int32_t nNrToBeSynced = m_oModel.getNrToBeSyncedRecordings();
		if (nNrToBeSynced > 0) {
			tellString("Number of to be synchronized files: " + std::to_string(nNrToBeSynced));
			break;
		}
		++m_nStatusCounter;
	} // fallthrough
	case 7: {
		auto& aMountInfos = m_oModel.getMountInfos();
		const auto nTotMounts = aMountInfos.size();
		tellString("Number of mounts: " + std::to_string(nTotMounts));
		m_nSubStatusCounter = 0;
		break;
	}
	case 8: {
		if (m_nSubStatusCounter < 0) {
			tellString("Mounts have changed. Restarting.");
			m_nSubStatusCounter = 0;
			--m_nStatusCounter; // stay on this case
			break;
		}
		auto& aMountInfos = m_oModel.getMountInfos();
		const int32_t nTotMounts = static_cast<int32_t>(aMountInfos.size());
		if (m_nSubStatusCounter < nTotMounts) {
			const SonoModel::MountInfo& oMI = aMountInfos[m_nSubStatusCounter];
			std::string sTell = "Mount " + std::to_string(m_nSubStatusCounter) + ": " + oMI.m_sName + ".";
			if (m_bVerbose) {
				if (oMI.m_bDirty) {
					sTell += " Is dirty.";
				}
				if (oMI.isBlacklisted()) {
					sTell += " Is blaclisted.";
				}
			}
			tellString(sTell);
			++m_nSubStatusCounter;
			--m_nStatusCounter; // stay on this case
			break;
		}
		m_nSubStatusCounter = 0;
		++m_nStatusCounter;
	} // fallthrough
	case 9: {
		const int32_t nMainFreeMB = m_oModel.getRecordingFsFreeMB();
		tellString("Free disk: " + std::to_string(nMainFreeMB) + " Megabytes");
		break;
	}
	case 10: {
		auto oNow = Glib::DateTime::create_now_local();
		const std::string sNowStr = oNow.format("%Y. %B, %e. %k hours. %M minutes");

		tellString(sNowStr);
		m_nStatusCounter = -1;
		break;
	}
	default: {
		m_nStatusCounter = -1;
	} break;
	}
	//
	++m_nStatusCounter;
	//
	m_oStatusConn = Glib::signal_timeout().connect(sigc::mem_fun(*this, &SonoWindow::resetStatusCounter), s_nStatusCounterResetMillisec);
}
bool SonoWindow::resetStatusCounter() noexcept
{
	const bool bContinue = true;
	m_nStatusCounter = 0;
	return ! bContinue; // one shot
}

void SonoWindow::tellString(const std::string& sStr) noexcept
{
	std::string sResult;
	std::string sError;
	const std::string sCmd = m_sSpeechApp + " \"" + sStr + "\"";
	execCmd(sCmd.c_str(), sResult, sError);
}

} // namespace sono
