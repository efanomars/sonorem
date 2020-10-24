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
 * File:   main.cc
 */

#include "config.h"
#include "sonowindow.h"
#include "sonomodel.h"
#include "sonodevicemanager.h"
#include "evalargs.h"
#include "util.h"

#include <stmm-input-gtk/gtkaccessor.h>

#include <stmm-input/event.h>

#include <gtkmm.h>

//#include <cassert>
#include <iostream>
#include <string>
#include <stdexcept>
#include <memory>
#include <fstream>

#include <stdint.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

namespace sono
{

static const std::string s_sHomeRelMainDir = "/Downloads";
static const std::string s_sDefaultSpeechApp = "espeak";

static constexpr int32_t s_nInitialAutostartSleepSeconds = 1;
using std::shared_ptr;
using std::unique_ptr;

static void printVersion() noexcept
{
	std::cout << Config::getVersionString() << '\n';
}
static void printUsage() noexcept
{
	std::cout << "Usage: sonorem" << '\n';
	std::cout << "Record sound to automounted volumes." << '\n';
	std::cout << "Option:" << '\n';
	std::cout << "  -h --help        Prints this message." << '\n';
	std::cout << "  -V --version     Prints version." << '\n';
	std::cout << "  -v --verbose     Show debug info." << '\n';
	std::cout << "  -a --auto        Start recording automatically when launched." << '\n';
	std::cout << "  -t --top         Each two seconds raises window to the top." << '\n';
	std::cout << "  -H --hours H     Max duration for one recorded sound file (default: 1)." << '\n';
	std::cout << "  -M --minutes M   Max duration for one recorded sound file (default: 0)." << '\n';
	std::cout << "                   Value is added to that of --hours." << '\n';
	//std::cout << "  -S --seconds S   Max duration for one recorded mp3 (default: 0)." << '\n';
	//std::cout << "                   Value is added to that of --hours." << '\n';
	std::cout << "  -m --max-file-size FILESIZE" << '\n';
	std::cout << "                   Maximum file size (default: " << SonoModel::Init{}.m_nMaxFileSizeBytes << "B)." << '\n';
	std::cout << "                   Number can be followed by B, KB, MB, GB." << '\n';
	std::cout << "                   Examples: 200MB, 1GB, 50000KB." << '\n';
	std::cout << "  -f --min-free-space MINSIZE" << '\n';
	std::cout << "                   Minimum free space needed to record (default: " << SonoModel::Init{}.m_nMinFreeSpaceBytes << "B)." << '\n';
	std::cout << "                   Must be bigger than --max-file-size." << '\n';
	std::cout << "                   Number can be followed by B, KB, MB, GB." << '\n';
	std::cout << "  -r --rec-path DIRPATH" << '\n';
	std::cout << "                   Recording directory path (default: /home/$USER" << s_sHomeRelMainDir << ")." << '\n';
	std::cout << "  -s --sound-format FMTEXT" << '\n';
	std::cout << "                   The recording`s sound format defined with its file extension." << '\n';
	std::cout << "                   The default is '" << SonoModel::s_sRecordingDefaultFileExt << "'." << '\n';
	std::cout << "                   The sound format must be supported by the '" << SonoModel::s_sRecordingProgram << "' program." << '\n';
	std::cout << "                   Examples: 'wav', 'aiff'." << '\n';
	std::cout << "  --pre STRING" << '\n';
	std::cout << "                   String to prepend to name of generated recordings (default is nothing)." << '\n';
	std::cout << "                   The string can only contain letters (A-Za-z), numbers (0-9), dashes (-)." << '\n';
	std::cout << "                   The un-prepended format of a file is for example '" << SonoModel::getNowString() << ".ogg'." << '\n';
	std::cout << "  -x --exclude-mount NAME" << '\n';
	std::cout << "                   Exclude mount name. Repeat this option to exclude more than one name." << '\n';
	std::cout << "  -p --speech-app CMD" << '\n';
	std::cout << "                   Speech app to use (default: " << s_sDefaultSpeechApp << ")." << '\n';
	std::cout << "  -l --log-dir DIRPATH" << '\n';
	std::cout << "                   Directory path where log files should be stored." << '\n';
	std::cout << "                   If the directory doesn't exist, it is created." << '\n';
	std::cout << "                   Example: \"/home/pi/logs\"." << '\n';
	std::cout << "  --wifi-on        Turn on wifi. Has precedence over --wifi-off." << '\n';
	std::cout << "  --wifi-off       Shutdown wifi, unless a file named 'sonorem.wifi' is found" << '\n';
	std::cout << "                   on a mounted stick when the program is started." << '\n';
	std::cout << "  --bluetooth-on   Turn on bluetooth. Has precedence over --bluetooth-off." << '\n';
	std::cout << "  --bluetooth-off  Shutdown bluetooth, unless a file named 'sonorem.bluetooth' is found" << '\n';
	std::cout << "                   on a mounted stick when the program is started." << '\n';
}

static int startWindow(SonoModel::Init&& oInit, const std::string& sSpeechApp, const std::string& sLogDirPath, bool bKeepOnTop) noexcept
{
	if (oInit.m_bAutoStart) {
		::sleep(s_nInitialAutostartSleepSeconds);
	}

	const Glib::ustring sAppName = "com.efanomars.sonorem";
	const Glib::ustring sWindoTitle = "sonorem " + Config::getVersionString();

	Glib::RefPtr<Gtk::Application> refApp;
	try {
		//
		refApp = Gtk::Application::create(sAppName);
	} catch (const std::runtime_error& oErr) {
		std::cout << "Error: " << oErr.what() << '\n';
		return EXIT_FAILURE; //-------------------------------------------------
	}
	//
	shared_ptr<SonoDeviceManager> refSonoDM;
	const bool bOk = SonoDeviceManager::create(sAppName, refSonoDM);
	if (! bOk) {
		return EXIT_FAILURE; //-------------------------------------------------
	}
	//
	const bool bVerbose = oInit.m_bVerbose;
	const bool bDebug = oInit.m_bDebug;
	//
	const bool bLogToFile = ! sLogDirPath.empty();
	struct LogData
	{
		std::ofstream m_oStream;
		std::ofstream::pos_type m_nLastStrPos = 0;
		std::string m_sLastStr;
		std::string m_sLastTime;
		int32_t m_nLastStrRepeated = 0;
	};
	unique_ptr<LogData> refLogData;
	const std::string sLogFilePath = sLogDirPath + "/sonorem" + SonoModel::getNowString() + ".log";
	if (bLogToFile) {
        refLogData = std::make_unique<LogData>();
		refLogData->m_oStream.open(sLogFilePath, std::ios_base::out | std::ios_base::trunc);
		if (! refLogData->m_oStream) {
			std::cerr << "Error: could not create log " << sLogFilePath << '\n';
			return EXIT_FAILURE; //---------------------------------------------
		}
		refLogData->m_oStream.close();
	}
	//
	std::string sPreWindow;
	Glib::RefPtr<SonoWindow> refWindow;
	//
	auto oLogger = [&](const std::string& sStr)
	{
		if (bVerbose) {
			std::cout << "sonorem: " << sStr << '\n';
		}
		if (bLogToFile && ! sStr.empty()) {
			refLogData->m_oStream.open(sLogFilePath, std::ios_base::out | std::ios_base::in);
			if (! refLogData->m_oStream) {
				std::cerr << "Error: could not open log " << sLogFilePath << '\n';
			} else {
				if (refLogData->m_sLastStr == sStr) {
					refLogData->m_oStream.seekp(refLogData->m_nLastStrPos);
					++(refLogData->m_nLastStrRepeated);
					refLogData->m_oStream << refLogData->m_sLastTime << " " << sStr << std::endl;
					refLogData->m_oStream << SonoModel::getShortNowString() << " " << sStr << " " << " (x" << refLogData->m_nLastStrRepeated - 1 << ")" << std::endl;
				} else {
					refLogData->m_oStream.seekp(0, std::ios_base::end);
					refLogData->m_nLastStrPos = refLogData->m_oStream.tellp();
					refLogData->m_sLastStr = sStr;
					refLogData->m_sLastTime = SonoModel::getShortNowString();
					refLogData->m_nLastStrRepeated = 1;
					refLogData->m_oStream << refLogData->m_sLastTime << " " << sStr << std::endl;
				}
				refLogData->m_oStream.close();
			}
		}
		if (refWindow) {
			if (! sPreWindow.empty()) {
				refWindow->logToWindow(sPreWindow);
				sPreWindow.clear();
			}
			refWindow->logToWindow(sStr);
		} else {
			if (! sPreWindow.empty()) {
				sPreWindow += "\n" + sStr;
			} else {
				sPreWindow = sStr;
			}
		}
	};
	//
	SonoModel oModel{std::move(oLogger)};
	const std::string sError = oModel.init(std::move(oInit));
	if (! sError.empty()) {
		std::cout << "Error: " << sError << '\n';
		return EXIT_FAILURE; //-------------------------------------------------
	}
	//
	refWindow = Glib::RefPtr<SonoWindow>(new SonoWindow(oModel, bVerbose, bDebug
														, sSpeechApp, bKeepOnTop, refSonoDM, sWindoTitle));
	//
	auto refAccessor = std::make_shared<stmi::GtkAccessor>(refWindow);
	refSonoDM->addAccessor(refAccessor);
	//
	oModel.getLogger()("Initialization successful");
	//
	const auto nRet = refApp->run(*(refWindow.operator->()));

	if (nRet != 0) {
		oModel.getLogger()("Finished with return code " + std::to_string(nRet));
	} else {
		oModel.getLogger()("Bye");
	}
	return nRet;
}

int sonoremMain(int nArgC, char** aArgV) noexcept
{
	SonoModel::Init oInit;
	bool bKeepOnTop = false;
	int32_t nHours = 0;
	int32_t nMinutes = 0;
	int32_t nSeconds = 0;
	std::string sSpeechApp;
	std::string sLogDirPath;
	//
	bool bHelp = false;
	bool bVersion = false;
	std::string sMatch;
	char* p0ArgVZeroSave = ((nArgC >= 1) ? aArgV[0] : nullptr);
	while (nArgC >= 2) {
		auto nOldArgC = nArgC;
		evalBoolArg(nArgC, aArgV, "--help", "-h", sMatch, bHelp);
		if (bHelp) {
			printUsage();
			return EXIT_SUCCESS; //---------------------------------------------
		}
		evalBoolArg(nArgC, aArgV, "--version", "-V", sMatch, bVersion);
		if (bVersion) {
			printVersion();
			return EXIT_SUCCESS; //---------------------------------------------
		}
		//
		evalBoolArg(nArgC, aArgV, "--debug", "", sMatch, oInit.m_bDebug);
		//
		evalBoolArg(nArgC, aArgV, "--verbose", "-v", sMatch, oInit.m_bVerbose);
		//
		evalBoolArg(nArgC, aArgV, "--auto", "-a", sMatch, oInit.m_bAutoStart);
		//
		evalBoolArg(nArgC, aArgV, "--wifi-on", "", sMatch, oInit.m_bRfkillWifiOn);
		//
		evalBoolArg(nArgC, aArgV, "--wifi-off", "", sMatch, oInit.m_bRfkillWifiOff);
		//
		evalBoolArg(nArgC, aArgV, "--bluetooth-on", "", sMatch, oInit.m_bRfkillBluetoothOn);
		//
		evalBoolArg(nArgC, aArgV, "--bluetooth-off", "", sMatch, oInit.m_bRfkillBluetoothOff);
		//
		evalBoolArg(nArgC, aArgV, "--auto", "-a", sMatch, oInit.m_bAutoStart);
		//
		evalBoolArg(nArgC, aArgV, "--top", "-t", sMatch, bKeepOnTop);
		//
		evalBoolArg(nArgC, aArgV, "--exclude-all-mounts", "", sMatch, oInit.m_bExcludeAllMountNames);
		//
		bool bOk = evalIntArg(nArgC, aArgV, "--hours", "-H", sMatch, nHours, 0);
		if (!bOk) {
			return EXIT_FAILURE; //---------------------------------------------
		}
		//
		bOk = evalIntArg(nArgC, aArgV, "--minutes", "-M", sMatch, nMinutes, 0);
		if (!bOk) {
			return EXIT_FAILURE; //---------------------------------------------
		}
		//
		bOk = evalIntArg(nArgC, aArgV, "--seconds", "-S", sMatch, nSeconds, 0);
		if (!bOk) {
			return EXIT_FAILURE; //---------------------------------------------
		}
		//
		bOk = evalMemSizeArg(nArgC, aArgV, "--max-file-size", "-m", sMatch, oInit.m_nMaxFileSizeBytes, 1);
		if (!bOk) {
			return EXIT_FAILURE; //---------------------------------------------
		}
		//
		bOk = evalMemSizeArg(nArgC, aArgV, "--min-free-space", "-f", sMatch, oInit.m_nMinFreeSpaceBytes, 1000);
		if (!bOk) {
			return EXIT_FAILURE; //---------------------------------------------
		}
		//
		bOk = evalDirPathArg(nArgC, aArgV, false, "--rec-path", "-r", true, sMatch, oInit.m_sRecordingDirPath);
		if (!bOk) {
			return EXIT_FAILURE; //---------------------------------------------
		}
		//
		bOk = evalDirPathArg(nArgC, aArgV, true, "--sound-format", "-s", true, sMatch, oInit.m_sRecordingFileExt);
		if (!bOk) {
			return EXIT_FAILURE; //---------------------------------------------
		}
		//
		bOk = evalDirPathArg(nArgC, aArgV, false, "--speech-app", "-p", true, sMatch, sSpeechApp);
		if (!bOk) {
			return EXIT_FAILURE; //---------------------------------------------
		}
		//
		bOk = evalDirPathArg(nArgC, aArgV, true, "--pre", "", true, sMatch, oInit.m_sPreString);
		if (!bOk) {
			return EXIT_FAILURE; //---------------------------------------------
		}
		//
		std::string sMountName;
		bOk = evalDirPathArg(nArgC, aArgV, true, "-x", "--exclude-mount", true, sMatch, sMountName);
		if (bOk) {
			if (! sMatch.empty()) {
				sMountName = strStrip(sMountName);
				if (sMountName.empty()) {
					std::cerr << "Mount name cannot be empty (" << sMatch << ")" << '\n';
					return EXIT_FAILURE; //-------------------------------------
				}
				oInit.m_aExclMountNames.push_back(std::move(sMountName));
			}
		} else {
			return EXIT_FAILURE; //---------------------------------------------
		}
		//
		bOk = evalDirPathArg(nArgC, aArgV, false, "--log-dir", "-l", true, sMatch, sLogDirPath);
		if (!bOk) {
			return EXIT_FAILURE; //---------------------------------------------
		}
		//
		if (nOldArgC == nArgC) {
			std::cerr << "Unknown argument: " << ((aArgV[1] == nullptr) ? "(null)" : std::string(aArgV[1])) << '\n';
			std::cerr << "Run with --help for details." << '\n';
			return EXIT_FAILURE; //---------------------------------------------
		}
		aArgV[0] = p0ArgVZeroSave;
	}

	if (nHours + nMinutes + nSeconds == 0) {
		nHours = 1;
	}
	oInit.m_nMaxRecordingDurationSeconds = nHours * 60 * 60 + nMinutes * 60 + nSeconds;
	//
	if (oInit.m_sRecordingDirPath.empty()) {
		const std::string sHomePath = getEnvString("HOME");
		if (sHomePath.empty()) {
			std::cerr << "Could not determine HOME path" << '\n';
			return EXIT_FAILURE; //---------------------------------------------
		}
		oInit.m_sRecordingDirPath = sHomePath + s_sHomeRelMainDir;
	}
	std::string sError = makePath(oInit.m_sRecordingDirPath);
	if (! sError.empty()) {
		std::cerr << "Could not create recordings dir path " << oInit.m_sRecordingDirPath << '\n';
		return EXIT_FAILURE; //---------------------------------------------
	}
	//
	if (! sLogDirPath.empty()) {
		sError = makePath(sLogDirPath);
		if (! sError.empty()) {
			std::cerr << "Could not create log path " << sLogDirPath << '\n';
			return EXIT_FAILURE; //---------------------------------------------
		}
	}
	//
	if (! oInit.m_sPreString.empty()) {
		const auto nPos = oInit.m_sPreString.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_");
		if (nPos != std::string::npos) {
			std::cerr << "--pre string contains unallowed characters." << '\n';
			return EXIT_FAILURE; //---------------------------------------------
		}
		if (oInit.m_sPreString.size() > 30) {
			std::cerr << "--pre string is too long." << '\n';
			return EXIT_FAILURE; //---------------------------------------------
		}
	}
	//
	if (oInit.m_sRecordingFileExt.empty()) {
		oInit.m_sRecordingFileExt = SonoModel::s_sRecordingDefaultFileExt;
	}
	//
	if (oInit.m_nMaxFileSizeBytes > oInit.m_nMinFreeSpaceBytes * 3) {
		std::cerr << "Sorry, --max-file-size cannot be bigger than a third of --min-free-space" << '\n';
		return EXIT_FAILURE; //---------------------------------------------
	}
	if (sSpeechApp.empty()) {
		sSpeechApp = s_sDefaultSpeechApp;
	}

	return startWindow(std::move(oInit), sSpeechApp, sLogDirPath, bKeepOnTop);
}

} // namespace sono

int main(int nArgC, char** aArgV)
{
	return sono::sonoremMain(nArgC, aArgV);
}

