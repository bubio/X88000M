#include <SDL3/SDL.h>

#ifdef X88000_SDL3_HAS_CORE

#include "StdHeader.h"
#include "PC88.h"
#include "X88ScreenDrawer.h"
#include "X88DiskImageMemory.h"
#include "ParallelNull.h"

#include <ctype.h>
#include <stdlib.h>

#endif

#ifdef X88000_SDL3_HAS_IMGUI

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#endif

#include "Sdl3Settings.h"
#include "Sdl3AudioOutput.h"

#include <stdio.h>
#include <string>
#include <string.h>
#include <set>
#include <mutex>
#include <stdint.h>
#include <vector>
#include <time.h>
#include <sys/stat.h>

#ifdef X88000_SDL3_HAS_CORE

namespace {

std::vector<std::string> g_vRomSearchDir;
CPC88 g_pc88;
CX88ScreenDrawer g_screenDrawer;
CParallelNull g_parallelNull;
CSdl3AudioOutput g_audio;
bool g_bScreenDrawerReady = false;
std::set<CX88DiskImageMemory> g_setDiskImageMemory;
std::string g_astrDriveMediaPath[CPC88Fdc::DRIVE_MAX];
std::string g_strLastMediaStatus;
std::mutex g_mtxDialogQueue;
std::vector<std::pair<std::string, int> > g_vDialogMediaQueue;
int g_anDriveDiskIndex[CPC88Fdc::DRIVE_MAX] = {-1, -1, -1, -1};

struct SDiskFileRecord {
	std::string strPath;
	int nStartImageIndex;
	int nImageCount;
};
std::vector<SDiskFileRecord> g_vDiskFileRecords;

void UpdateWindowTitle(SDL_Window* pWindow, bool bCoreReady, bool bPauseEmulation);
// Forward declarations for helpers used by ImGui draw functions further up the file.
void SetMediaStatus(const std::string& strStatus);
bool MountDiskImageByIndex(int nDrive, int nDiskImageIndex);
void EjectDiskImageFromDrive(int nDrive);
std::string GetDriveDiskImageLabel(int nDrive);
void EraseAllDiskImages();
int  FindDriveHoldingDiskIndex(int nDiskImageIndex);
void RequestOpenDiskOnlyDialog(SDL_Window* pWindow, int nDrive);
void RequestOpenTapeOnlyDialog(SDL_Window* pWindow);

////////////////////////////////////////////////////////////
// Environment settings (BASIC mode, base clock, dip-switches)
//
// These settings are persisted under the legacy [option] section of
// X88000.ini so that the SDL3 frontend and legacy GTK frontend share
// the same configuration file.

const char SECTION_OPTION[] = "option";

// Six-way enumeration for the BASIC mode + hi-speed combination, matching
// the legacy "basic" option string values exactly.
enum {
	BASIC_CHOICE_N      = 0, // "n"
	BASIC_CHOICE_V1S    = 1, // "v1s"
	BASIC_CHOICE_V1H    = 2, // "v1h"
	BASIC_CHOICE_V2     = 3, // "v2"
	BASIC_CHOICE_N80V1  = 4, // "n80v1"
	BASIC_CHOICE_N80V2  = 5, // "n80v2"
	BASIC_CHOICE_COUNT  = 6
};

const char* BasicChoiceToString(int nChoice)
{
	switch (nChoice) {
	case BASIC_CHOICE_N:     return "n";
	case BASIC_CHOICE_V1S:   return "v1s";
	case BASIC_CHOICE_V1H:   return "v1h";
	case BASIC_CHOICE_V2:    return "v2";
	case BASIC_CHOICE_N80V1: return "n80v1";
	case BASIC_CHOICE_N80V2: return "n80v2";
	}
	return "v2";
}

const char* BasicChoiceToLabel(int nChoice)
{
	switch (nChoice) {
	case BASIC_CHOICE_N:     return "N-BASIC";
	case BASIC_CHOICE_V1S:   return "N88-V1 (S)";
	case BASIC_CHOICE_V1H:   return "N88-V1 (H)";
	case BASIC_CHOICE_V2:    return "N88-V2";
	case BASIC_CHOICE_N80V1: return "N80-V1";
	case BASIC_CHOICE_N80V2: return "N80-V2";
	}
	return "N88-V2";
}

int BasicChoiceFromString(const std::string& strValue, int nDefault)
{
	if (strValue == "n")     return BASIC_CHOICE_N;
	if (strValue == "v1s")   return BASIC_CHOICE_V1S;
	if (strValue == "v1h")   return BASIC_CHOICE_V1H;
	if (strValue == "v2")    return BASIC_CHOICE_V2;
	if (strValue == "n80v1") return BASIC_CHOICE_N80V1;
	if (strValue == "n80v2") return BASIC_CHOICE_N80V2;
	return nDefault;
}

int BasicChoiceFromMode(int nMode, bool bHighSpeed)
{
	switch (nMode) {
	case CPC88Z80Main::BASICMODE_N:     return BASIC_CHOICE_N;
	case CPC88Z80Main::BASICMODE_N88V1: return bHighSpeed? BASIC_CHOICE_V1H: BASIC_CHOICE_V1S;
	case CPC88Z80Main::BASICMODE_N88V2: return BASIC_CHOICE_V2;
	case CPC88Z80Main::BASICMODE_N80V1: return BASIC_CHOICE_N80V1;
	case CPC88Z80Main::BASICMODE_N80V2: return BASIC_CHOICE_N80V2;
	}
	return BASIC_CHOICE_V2;
}

void ApplyBasicChoice(int nChoice)
{
	int nMode = CPC88Z80Main::BASICMODE_N88V2;
	bool bHighSpeed = true;
	switch (nChoice) {
	case BASIC_CHOICE_N:     nMode = CPC88Z80Main::BASICMODE_N;     bHighSpeed = false; break;
	case BASIC_CHOICE_V1S:   nMode = CPC88Z80Main::BASICMODE_N88V1; bHighSpeed = false; break;
	case BASIC_CHOICE_V1H:   nMode = CPC88Z80Main::BASICMODE_N88V1; bHighSpeed = true;  break;
	case BASIC_CHOICE_V2:    nMode = CPC88Z80Main::BASICMODE_N88V2; bHighSpeed = true;  break;
	case BASIC_CHOICE_N80V1: nMode = CPC88Z80Main::BASICMODE_N80V1; bHighSpeed = false; break;
	case BASIC_CHOICE_N80V2: nMode = CPC88Z80Main::BASICMODE_N80V2; bHighSpeed = true;  break;
	}
	CPC88::SetBasicMode(nMode);
	CPC88::SetHighSpeedMode(bHighSpeed);
}

bool ParseBoolEntry(const std::string& strValue, bool bDefault)
{
	if ((strValue == "on") || (strValue == "1") || (strValue == "true")) {
		return true;
	}
	if ((strValue == "off") || (strValue == "0") || (strValue == "false")) {
		return false;
	}
	return bDefault;
}

const char* BoolToOnOff(bool bValue)
{
	return bValue? "on": "off";
}

// Apply persisted env settings from settings (X88000.ini, [option] section)
// to the freshly-initialized PC88 core. Must be called after CPC88::Initialize()
// and before the first CPC88::Reset().
void ApplyEnvSettingsFromIni(CSdl3Settings& settings)
{
	// BASIC mode
	std::string strBasic = settings.GetSectionString(SECTION_OPTION, "basic", "");
	if (!strBasic.empty()) {
		int nChoice = BasicChoiceFromString(strBasic, BASIC_CHOICE_V2);
		ApplyBasicChoice(nChoice);
	}
	// Base clock
	std::string strClock = settings.GetSectionString(SECTION_OPTION, "clock", "");
	if (!strClock.empty()) {
		if (strClock == "8") {
			CPC88::SetBaseClock(8);
		} else if (strClock == "4") {
			CPC88::SetBaseClock(4);
		}
	}
	// Drive count
	std::string strDrives = settings.GetSectionString(SECTION_OPTION, "drives", "");
	if (!strDrives.empty()) {
		int nDrives = atoi(strDrives.c_str());
		if ((nDrives >= 1) && (nDrives <= CPC88Fdc::DRIVE_MAX)) {
			CPC88::Fdc().SetDriveCount(nDrives);
		}
	}
	// Dip switches
	std::string strWaitemu = settings.GetSectionString(SECTION_OPTION, "waitemu", "");
	if (!strWaitemu.empty()) {
		CPC88::SetWaitEmulation(ParseBoolEntry(strWaitemu, false));
	}
	std::string strOldcompat = settings.GetSectionString(SECTION_OPTION, "oldcompat", "");
	if (!strOldcompat.empty()) {
		CPC88::SetOldCompatible(ParseBoolEntry(strOldcompat, false));
	}
	std::string strPcg = settings.GetSectionString(SECTION_OPTION, "pcg", "");
	if (!strPcg.empty()) {
		CPC88::SetPcgEnable(ParseBoolEntry(strPcg, false));
	}
}

// Volatile mirror of env settings used by the ImGui window. These are
// loaded from CPC88 / settings on first show and written back on each edit.
struct SEnvSettingsView {
	int  nBasicChoice;
	int  nBaseClock;
	int  nDriveCount;
	bool bWaitEmulation;
	bool bOldCompatible;
	bool bPcgEnable;
	int  nBeepVolume;
	bool bBeepMute;
	int  nPcgVolume;
	bool bPcgMute;
	bool bInterlace;
	int  nFrameRate;
	bool bLoaded;
	SEnvSettingsView() :
		nBasicChoice(BASIC_CHOICE_V2),
		nBaseClock(4),
		nDriveCount(2),
		bWaitEmulation(false),
		bOldCompatible(false),
		bPcgEnable(false),
		nBeepVolume(50),
		bBeepMute(false),
		nPcgVolume(50),
		bPcgMute(false),
		bInterlace(false),
		nFrameRate(20),
		bLoaded(false)
	{
	}
};

void LoadEnvSettingsView(SEnvSettingsView& view, CSdl3Settings& settings)
{
	view.nBasicChoice = BasicChoiceFromMode(
		CPC88::GetBasicMode(),
		CPC88::IsHighSpeedMode());
	view.nBaseClock   = CPC88::GetBaseClock();
	view.nDriveCount  = CPC88::Fdc().GetDriveCount();
	view.bWaitEmulation = CPC88::IsWaitEmulation();
	view.bOldCompatible = CPC88::IsOldCompatible();
	view.bPcgEnable     = CPC88::IsPcgEnable();
	// Sound + display settings have no live core counterpart yet — read from ini.
	view.nBeepVolume = atoi(settings.GetSectionString(SECTION_OPTION, "beepvolume", "50").c_str());
	view.bBeepMute   = ParseBoolEntry(settings.GetSectionString(SECTION_OPTION, "beepmute", "off"), false);
	view.nPcgVolume  = atoi(settings.GetSectionString(SECTION_OPTION, "pcgvolume", "50").c_str());
	view.bPcgMute    = ParseBoolEntry(settings.GetSectionString(SECTION_OPTION, "pcgmute", "off"), false);
	view.bInterlace  = ParseBoolEntry(settings.GetSectionString(SECTION_OPTION, "interlace", "off"), false);
	view.nFrameRate  = atoi(settings.GetSectionString(SECTION_OPTION, "framerate", "20").c_str());
	if (view.nFrameRate <= 0) {
		view.nFrameRate = 20;
	}
	view.bLoaded = true;
}

#ifdef X88000_SDL3_HAS_IMGUI

// Returns true if the user requested an emulator reset (because they changed
// a setting that requires one).
bool DrawEnvSettingsWindow(bool& bShow, SEnvSettingsView& view, CSdl3Settings& settings)
{
	if (!bShow) {
		return false;
	}
	if (!view.bLoaded) {
		LoadEnvSettingsView(view, settings);
	}
	bool bRequestReset = false;
	ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Environment", &bShow, ImGuiWindowFlags_NoCollapse)) {
		ImGui::TextUnformatted("Settings shared with X88000.ini [option] section.");
		ImGui::Separator();

		if (ImGui::CollapsingHeader("System (resets emulator)", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::TextUnformatted("BASIC mode");
			for (int n = 0; n < BASIC_CHOICE_COUNT; n++) {
				if (ImGui::RadioButton(BasicChoiceToLabel(n), view.nBasicChoice == n)) {
					if (view.nBasicChoice != n) {
						view.nBasicChoice = n;
						ApplyBasicChoice(n);
						settings.SetSectionString(SECTION_OPTION, "basic", BasicChoiceToString(n));
						bRequestReset = true;
					}
				}
				if ((n % 2) == 0) {
					ImGui::SameLine(180);
				}
			}
			ImGui::NewLine();

			ImGui::TextUnformatted("Base clock");
			ImGui::SameLine();
			if (ImGui::RadioButton("4 MHz", view.nBaseClock == 4)) {
				if (view.nBaseClock != 4) {
					view.nBaseClock = 4;
					CPC88::SetBaseClock(4);
					settings.SetSectionString(SECTION_OPTION, "clock", "4");
					bRequestReset = true;
				}
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("8 MHz", view.nBaseClock == 8)) {
				if (view.nBaseClock != 8) {
					view.nBaseClock = 8;
					CPC88::SetBaseClock(8);
					settings.SetSectionString(SECTION_OPTION, "clock", "8");
					bRequestReset = true;
				}
			}

			int nDriveCount = view.nDriveCount;
			if (ImGui::SliderInt("Drive count", &nDriveCount, 1, CPC88Fdc::DRIVE_MAX)) {
				if (nDriveCount != view.nDriveCount) {
					view.nDriveCount = nDriveCount;
					CPC88::Fdc().SetDriveCount(nDriveCount);
					char szBuf[16];
					snprintf(szBuf, sizeof(szBuf), "%d", nDriveCount);
					settings.SetSectionString(SECTION_OPTION, "drives", szBuf);
					bRequestReset = true;
				}
			}

			if (ImGui::Checkbox("Old machine compatible", &view.bOldCompatible)) {
				CPC88::SetOldCompatible(view.bOldCompatible);
				settings.SetSectionString(SECTION_OPTION, "oldcompat", BoolToOnOff(view.bOldCompatible));
				bRequestReset = true;
			}
		}

		if (ImGui::CollapsingHeader("Emulation", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::Checkbox("Wait emulation", &view.bWaitEmulation)) {
				CPC88::SetWaitEmulation(view.bWaitEmulation);
				settings.SetSectionString(SECTION_OPTION, "waitemu", BoolToOnOff(view.bWaitEmulation));
			}
			if (ImGui::Checkbox("PCG enable", &view.bPcgEnable)) {
				CPC88::SetPcgEnable(view.bPcgEnable);
				settings.SetSectionString(SECTION_OPTION, "pcg", BoolToOnOff(view.bPcgEnable));
			}
		}

		if (ImGui::CollapsingHeader("Display")) {
			int nFrameRate = view.nFrameRate;
			if (ImGui::SliderInt("Frame rate", &nFrameRate, 1, 60)) {
				if (nFrameRate != view.nFrameRate) {
					view.nFrameRate = nFrameRate;
					char szBuf[16];
					snprintf(szBuf, sizeof(szBuf), "%d", nFrameRate);
					settings.SetSectionString(SECTION_OPTION, "framerate", szBuf);
				}
			}
			if (ImGui::Checkbox("Interlace", &view.bInterlace)) {
				settings.SetSectionString(SECTION_OPTION, "interlace", BoolToOnOff(view.bInterlace));
			}
			ImGui::TextDisabled("(frame rate / interlace will be wired up in a later phase)");
		}

		if (ImGui::CollapsingHeader("Sound", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::SliderInt("Beep volume", &view.nBeepVolume, 0, 100)) {
				char szBuf[16];
				snprintf(szBuf, sizeof(szBuf), "%d", view.nBeepVolume);
				settings.SetSectionString(SECTION_OPTION, "beepvolume", szBuf);
				g_audio.SetBeepVolume(view.nBeepVolume);
			}
			if (ImGui::Checkbox("Beep mute", &view.bBeepMute)) {
				settings.SetSectionString(SECTION_OPTION, "beepmute", BoolToOnOff(view.bBeepMute));
				g_audio.SetBeepMute(view.bBeepMute);
			}
			if (ImGui::SliderInt("PCG volume", &view.nPcgVolume, 0, 100)) {
				char szBuf[16];
				snprintf(szBuf, sizeof(szBuf), "%d", view.nPcgVolume);
				settings.SetSectionString(SECTION_OPTION, "pcgvolume", szBuf);
				g_audio.SetPcgVolume(view.nPcgVolume);
			}
			if (ImGui::Checkbox("PCG mute", &view.bPcgMute)) {
				settings.SetSectionString(SECTION_OPTION, "pcgmute", BoolToOnOff(view.bPcgMute));
				g_audio.SetPcgMute(view.bPcgMute);
			}
			ImGui::TextDisabled("(YM2203/OPN output will be added in Phase C)");
		}
	}
	ImGui::End();
	return bRequestReset;
}

void DrawDiskImageManagerWindow(bool& bShow, SDL_Window* pWindow)
{
	if (!bShow) {
		return;
	}
	ImGui::SetNextWindowSize(ImVec2(560, 400), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Disk Image Manager", &bShow, ImGuiWindowFlags_NoCollapse)) {
		// Drive status panel at the top — most important info.
		if (ImGui::CollapsingHeader("Drives", ImGuiTreeNodeFlags_DefaultOpen)) {
			int nEquipped = CPC88::Fdc().GetDriveCount();
			if (nEquipped <= 0) {
				nEquipped = CPC88Fdc::DRIVE_MAX;
			}
			for (int nDrive = 0; nDrive < CPC88Fdc::DRIVE_MAX; nDrive++) {
				ImGui::PushID(nDrive);
				bool bEquipped = nDrive < nEquipped;
				ImGui::BeginDisabled(!bEquipped);
				char szLabel[64];
				snprintf(szLabel, sizeof(szLabel), "Drive %d:", nDrive+1);
				ImGui::TextUnformatted(szLabel);
				ImGui::SameLine(80);
				int nIndex = g_anDriveDiskIndex[nDrive];
				if (nIndex < 0) {
					ImGui::TextDisabled("(empty)");
				} else {
					ImGui::TextUnformatted(GetDriveDiskImageLabel(nDrive).c_str());
					ImGui::SameLine();
					if (ImGui::SmallButton("Eject")) {
						EjectDiskImageFromDrive(nDrive);
						SetMediaStatus("Ejected drive " + std::to_string(nDrive+1));
					}
				}
				ImGui::EndDisabled();
				ImGui::PopID();
			}
			if (!g_astrDriveMediaPath[0].empty() || !g_astrDriveMediaPath[1].empty()) {
				ImGui::TextDisabled("Source paths:");
				for (int n = 0; n < CPC88Fdc::DRIVE_MAX; n++) {
					if (!g_astrDriveMediaPath[n].empty()) {
						ImGui::TextWrapped("  D%d: %s", n+1, g_astrDriveMediaPath[n].c_str());
					}
				}
			}
		}

		ImGui::Separator();

		// Toolbar.
		if (ImGui::Button("Add D88 file...")) {
			RequestOpenDiskOnlyDialog(pWindow, -1);
		}
		ImGui::SameLine();
		if (ImGui::Button("Remove all images")) {
			EraseAllDiskImages();
		}
		ImGui::Separator();

		// Image catalog: list of D88 files and the disks within each.
		if (g_vDiskFileRecords.empty()) {
			ImGui::TextDisabled("(no disk image files loaded)");
		}
		for (size_t nRec = 0; nRec < g_vDiskFileRecords.size(); nRec++) {
			const SDiskFileRecord& rec = g_vDiskFileRecords[nRec];
			ImGui::PushID((int)nRec);
			char szHeader[1024];
			snprintf(szHeader, sizeof(szHeader), "%s (%d disk%s)",
				rec.strPath.c_str(), rec.nImageCount,
				rec.nImageCount == 1? "": "s");
			ImGuiTreeNodeFlags nFlags = ImGuiTreeNodeFlags_DefaultOpen;
			if (ImGui::TreeNodeEx(szHeader, nFlags)) {
				for (int nImage = 0; nImage < rec.nImageCount; nImage++) {
					int nGlobalIndex = rec.nStartImageIndex+nImage;
					CDiskImage* pDisk = CPC88::GetDiskImageCollection().GetDiskImage(nGlobalIndex);
					if (pDisk == NULL) {
						continue;
					}
					ImGui::PushID(nImage);

					std::string strLabel = "#" + std::to_string(nImage+1);
					std::string strName = pDisk->GetImageName();
					if (!strName.empty()) {
						strLabel += " — ";
						strLabel += strName;
					}
					if (pDisk->IsWriteProtected()) {
						strLabel += " [RO]";
					}
					ImGui::TextUnformatted(strLabel.c_str());
					ImGui::SameLine(280);

					int nCurDrive = FindDriveHoldingDiskIndex(nGlobalIndex);
					int nSelected = nCurDrive+1; // 0 = (none), 1..MAX = drive
					const char* apszDrives[1+CPC88Fdc::DRIVE_MAX] = {
						"(unmounted)", "Drive 1", "Drive 2", "Drive 3", "Drive 4"
					};
					if (ImGui::Combo("##mount", &nSelected, apszDrives, 1+CPC88Fdc::DRIVE_MAX)) {
						int nNewDrive = nSelected-1;
						if (nNewDrive != nCurDrive) {
							if (nCurDrive >= 0) {
								EjectDiskImageFromDrive(nCurDrive);
							}
							if (nNewDrive >= 0) {
								// If the target drive currently has another disk, eject it first.
								if (g_anDriveDiskIndex[nNewDrive] >= 0) {
									EjectDiskImageFromDrive(nNewDrive);
								}
								if (MountDiskImageByIndex(nNewDrive, nGlobalIndex)) {
									g_astrDriveMediaPath[nNewDrive] = rec.strPath;
									SetMediaStatus(
										"Mounted disk #" + std::to_string(nImage+1) +
										" to drive " + std::to_string(nNewDrive+1));
								}
							} else {
								SetMediaStatus("Unmounted disk #" + std::to_string(nImage+1));
							}
						}
					}

					ImGui::PopID();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}
	ImGui::End();
}

void DrawTapeImageManagerWindow(bool& bShow, SDL_Window* pWindow)
{
	if (!bShow) {
		return;
	}
	ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Tape Image Manager", &bShow, ImGuiWindowFlags_NoCollapse)) {
		CTapeImage& tapeLoad = CPC88::Usart().GetLoadTapeImage();

		ImGui::TextUnformatted("Loading tape (CMT in)");
		ImGui::Separator();
		bool bHasData = tapeLoad.IsExistData();
		ImGui::Text("State: %s", bHasData? "loaded": "(empty)");
		if (bHasData) {
			ImGui::Text("Data blocks: %d", tapeLoad.GetRealDataBlockCount());
			int nTotal = tapeLoad.GetTotalTick();
			int nCur   = tapeLoad.GetCounterTick();
			float fProgress = (nTotal > 0)? ((float)nCur/(float)nTotal): 0.0f;
			ImGui::ProgressBar(fProgress, ImVec2(-1, 0));
			ImGui::Text("Tick: %d / %d", nCur, nTotal);
		}

		ImGui::Spacing();
		if (ImGui::Button("Load T88/CMT...")) {
			RequestOpenTapeOnlyDialog(pWindow);
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!bHasData);
		if (ImGui::Button("Erase")) {
			tapeLoad.Erase();
			SetMediaStatus("Tape erased");
		}
		ImGui::SameLine();
		if (ImGui::Button("|<<  REW")) {
			tapeLoad.CounterREW();
		}
		ImGui::SameLine();
		if (ImGui::Button("FWD  >>|")) {
			tapeLoad.CounterFWD();
		}
		ImGui::EndDisabled();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("Tape save/recording will be added in a later phase.");
	}
	ImGui::End();
}

void DrawDebugMainWindow(bool& bShow)
{
	if (!bShow) {
		return;
	}
	ImGui::SetNextWindowSize(ImVec2(720, 0), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Debugger", NULL, ImGuiWindowFlags_NoCollapse))
	{
		bool bDebugMode = CPC88::IsDebugMode();
		bool bStopped = CPC88::IsDebugStopped();
		bool bMainCPU = CPC88::IsDebugMain();
		CZ80Adapter* pZ80A = bDebugMode ? CPC88::GetDebugAdapter() : NULL;

		// CPU target selector (Main/Sub only, no Off — closing the window exits debug)
		ImGui::TextUnformatted("Target CPU:");
		ImGui::SameLine();
		bool bSubDisabled = CPC88::IsSubSystemDisableNow();
		if (ImGui::RadioButton("Main", bDebugMode && bMainCPU)) {
			CPC88::SetDebugExecMode(CPC88::DEBUGEXEC_MAIN);
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(bSubDisabled);
		if (ImGui::RadioButton("Sub", bDebugMode && !bMainCPU)) {
			CPC88::SetDebugExecMode(CPC88::DEBUGEXEC_SUB);
		}
		ImGui::EndDisabled();

		ImGui::Separator();

		// Execution controls
		ImGui::BeginDisabled(!bDebugMode);
		if (bStopped) {
			if (ImGui::Button("Run")) {
				CPC88::SetDebugStop(false);
			}
		} else {
			if (ImGui::Button("Break")) {
				CPC88::SetDebugStop(true);
			}
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!bStopped);
		if (ImGui::Button("Step")) {
			CPC88::DebugExecuteStepTrace(CPC88::DEBUGSTEP_STEP);
		}
		ImGui::SameLine();
		if (ImGui::Button("Trace")) {
			CPC88::DebugExecuteStepTrace(CPC88::DEBUGSTEP_TRACE);
		}
		ImGui::SameLine();
		if (ImGui::Button("Step2")) {
			CPC88::DebugExecuteStepTrace(CPC88::DEBUGSTEP_STEP2);
		}
		ImGui::EndDisabled();
		ImGui::EndDisabled();

		ImGui::Separator();

		// Mnemonic and register display
		if (bDebugMode && bStopped && pZ80A != NULL) {
			// Mnemonic at current PC (GTK format: 0XXXXH  MNEMONIC)
			uint16_t wPC = CPC88::GetDebugPC();
			const char* pszMnemonic = pZ80A->GetMnemonic();
			// Expand tabs in mnemonic (tab stop = 8)
			std::string strMne;
			if (pszMnemonic) {
				int nCol = 0;
				for (const char* p = pszMnemonic; *p != '\0'; p++) {
					if (*p == '\t') {
						int nSpaces = 8 - (nCol % 8);
						for (int s = 0; s < nSpaces; s++) strMne += ' ';
						nCol += nSpaces;
					} else {
						strMne += *p;
						nCol++;
					}
				}
			}
			ImGui::Text(" 0%04XH  %s", wPC, strMne.c_str());
			ImGui::Separator();

			// Register display: GTK-style 3-line horizontal layout
			ImVec4 colLabel(0.4f, 0.4f, 0.9f, 1.0f); // blue labels

			// Line 1: F A BC DE HL IX IY
			ImGui::TextColored(colLabel, " F :"); ImGui::SameLine(0, 0);
			ImGui::Text("%c%c%c%c%c%c ",
				pZ80A->TestRegF(CZ80Adapter::C_FLAG)   ? 'C' : '-',
				pZ80A->TestRegF(CZ80Adapter::Z_FLAG)   ? 'Z' : '-',
				pZ80A->TestRegF(CZ80Adapter::P_V_FLAG) ? 'E' : 'O',
				pZ80A->TestRegF(CZ80Adapter::S_FLAG)   ? 'M' : 'P',
				pZ80A->TestRegF(CZ80Adapter::N_FLAG)   ? 'N' : '-',
				pZ80A->TestRegF(CZ80Adapter::H_FLAG)   ? 'H' : '-');
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, " A :"); ImGui::SameLine(0, 0);
			ImGui::Text("%02X ", pZ80A->RegA().Get());
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, " BC :"); ImGui::SameLine(0, 0);
			ImGui::Text("%04X ", pZ80A->RegBC().Get());
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, " DE :"); ImGui::SameLine(0, 0);
			ImGui::Text("%04X ", pZ80A->RegDE().Get());
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, " HL :"); ImGui::SameLine(0, 0);
			ImGui::Text("%04X ", pZ80A->RegHL().Get());
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, " IX :"); ImGui::SameLine(0, 0);
			ImGui::Text("%04X ", pZ80A->RegIX().Get());
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, " IY :"); ImGui::SameLine(0, 0);
			ImGui::Text("%04X", pZ80A->RegIY().Get());

			// Line 2: F' A' BC' DE' HL' SP
			ImGui::TextColored(colLabel, " F':"); ImGui::SameLine(0, 0);
			ImGui::Text("%c%c%c%c%c%c ",
				pZ80A->TestRegF2(CZ80Adapter::C_FLAG)   ? 'C' : '-',
				pZ80A->TestRegF2(CZ80Adapter::Z_FLAG)   ? 'Z' : '-',
				pZ80A->TestRegF2(CZ80Adapter::P_V_FLAG) ? 'E' : 'O',
				pZ80A->TestRegF2(CZ80Adapter::S_FLAG)   ? 'M' : 'P',
				pZ80A->TestRegF2(CZ80Adapter::N_FLAG)   ? 'N' : '-',
				pZ80A->TestRegF2(CZ80Adapter::H_FLAG)   ? 'H' : '-');
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, " A':"); ImGui::SameLine(0, 0);
			ImGui::Text("%02X ", pZ80A->RegAF2().GetHi());
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, " BC':"); ImGui::SameLine(0, 0);
			ImGui::Text("%04X ", pZ80A->RegBC2().Get());
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, " DE':"); ImGui::SameLine(0, 0);
			ImGui::Text("%04X ", pZ80A->RegDE2().Get());
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, " HL':"); ImGui::SameLine(0, 0);
			ImGui::Text("%04X ", pZ80A->RegHL2().Get());
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, " SP :"); ImGui::SameLine(0, 0);
			ImGui::Text("%04X", pZ80A->RegSP().Get());

			// Line 3: I R (BC) (DE) (HL) (SP) <EI/DI>
			ImGui::Text("    "); ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, "I :"); ImGui::SameLine(0, 0);
			ImGui::Text(" %02X  ", pZ80A->GetRegI());
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, "R :"); ImGui::SameLine(0, 0);
			ImGui::Text("%02X ", pZ80A->GetRegR());
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, "(BC):"); ImGui::SameLine(0, 0);
			ImGui::Text("%04X ", pZ80A->ReadMemoryW(pZ80A->RegBC().Get()));
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, "(DE):"); ImGui::SameLine(0, 0);
			ImGui::Text("%04X ", pZ80A->ReadMemoryW(pZ80A->RegDE().Get()));
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, "(HL):"); ImGui::SameLine(0, 0);
			ImGui::Text("%04X ", pZ80A->ReadMemoryW(pZ80A->RegHL().Get()));
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, "(SP):"); ImGui::SameLine(0, 0);
			ImGui::Text("%04X ", pZ80A->ReadMemoryW(pZ80A->RegSP().Get()));
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, "<"); ImGui::SameLine(0, 0);
			ImGui::Text("%cI", pZ80A->IsEnableInterrupt() ? 'E' : 'D');
			ImGui::SameLine(0, 0);
			ImGui::TextColored(colLabel, ">");
		} else if (bDebugMode && !bStopped) {
			uint16_t wPC = CPC88::GetDebugPC();
			ImGui::Text(" 0%04XH  Running...", wPC);
		} else {
			ImGui::TextDisabled("Select Main or Sub CPU to start debugging.");
		}
	}
	ImGui::End();
}

void DrawDisassembleWindow(bool& bShow)
{
	if (!bShow) {
		return;
	}
	static char szAddr[8] = "";
	static bool bFollowPC = true;

	ImGui::SetNextWindowSize(ImVec2(400, 360), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Disassemble", &bShow, ImGuiWindowFlags_NoCollapse)) {
		bool bDebugMode = CPC88::IsDebugMode();
		bool bStopped = CPC88::IsDebugStopped();
		CZ80Adapter* pZ80A = bDebugMode ? CPC88::GetDebugAdapter() : NULL;

		ImGui::Checkbox("Follow PC", &bFollowPC);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80);
		ImGui::InputText("Addr", szAddr, sizeof(szAddr),
			ImGuiInputTextFlags_CharsHexadecimal);

		ImGui::Separator();

		if (bDebugMode && bStopped && pZ80A != NULL) {
			uint16_t wStartAddr;
			if (!bFollowPC && szAddr[0] != '\0') {
				wStartAddr = (uint16_t)strtoul(szAddr, NULL, 16);
			} else {
				wStartAddr = CPC88::GetDebugPC();
			}

			// Save and restore PC around disassembly
			uint16_t wPCOrg = pZ80A->RegPC().Get();
			pZ80A->RegPC().Set(wStartAddr);

			uint16_t wDebugPC = CPC88::GetDebugPC();
			int nLines = 32;

			if (ImGui::BeginChild("DisasmList", ImVec2(0, 0), ImGuiChildFlags_None)) {
				for (int i = 0; i < nLines; i++) {
					uint16_t wPC = pZ80A->RegPC().Get();
					pZ80A->DisAssemble();
					const char* pszMne = pZ80A->GetMnemonic();

					// Expand tabs in mnemonic (tab stop = 8)
					std::string strMne;
					if (pszMne) {
						int nCol = 0;
						for (const char* p = pszMne; *p != '\0'; p++) {
							if (*p == '\t') {
								int nSpaces = 8 - (nCol % 8);
								for (int s = 0; s < nSpaces; s++) strMne += ' ';
								nCol += nSpaces;
							} else {
								strMne += *p;
								nCol++;
							}
						}
					}

					bool bCurrent = (wPC == wDebugPC);
					if (bCurrent) {
						ImGui::PushStyleColor(ImGuiCol_Text,
							ImVec4(1.0f, 1.0f, 0.3f, 1.0f));
					}
					ImGui::Text("%c0%04XH  %s",
						bCurrent ? '>' : ' ', wPC,
						strMne.c_str());
					if (bCurrent) {
						ImGui::PopStyleColor();
						if (bFollowPC) {
							ImGui::SetScrollHereY(0.3f);
						}
					}
				}
			}
			ImGui::EndChild();

			pZ80A->RegPC().Set(wPCOrg);
			// Restore mnemonic state
			pZ80A->DisAssemble();
			pZ80A->RegPC().Set(wPCOrg);
		} else {
			ImGui::TextDisabled("(debugger not active or running)");
		}
	}
	ImGui::End();
}

void DrawMemoryDumpWindow(bool& bShow)
{
	if (!bShow) {
		return;
	}
	static char szAddr[8] = "0000";
	static int nMemTarget = 0; // 0=Main, 1=Sub

	ImGui::SetNextWindowSize(ImVec2(400, 480), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Memory Dump", &bShow, ImGuiWindowFlags_NoCollapse)) {
		bool bDebugMode = CPC88::IsDebugMode();
		bool bStopped = CPC88::IsDebugStopped();

		ImGui::RadioButton("Main", &nMemTarget, 0);
		ImGui::SameLine();
		bool bSubDisabled = CPC88::IsSubSystemDisableNow();
		ImGui::BeginDisabled(bSubDisabled);
		ImGui::RadioButton("Sub", &nMemTarget, 1);
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80);
		ImGui::InputText("Addr", szAddr, sizeof(szAddr),
			ImGuiInputTextFlags_CharsHexadecimal);

		ImGui::Separator();

		if (bDebugMode && bStopped) {
			// Pick the adapter for the selected memory target
			CZ80Adapter* pMainA = CPC88::Z80Main().GetAdapter();
			CZ80Adapter* pSubA  = CPC88::Z80Sub().GetAdapter();
			CZ80Adapter* pMemA  = (nMemTarget == 0) ? pMainA : pSubA;
			if (pMemA == NULL) {
				pMemA = CPC88::GetDebugAdapter();
			}

			uint16_t wBase = (uint16_t)strtoul(szAddr, NULL, 16);
			wBase &= 0xFFF8; // align to 8

			int nRows = 32; // 8 bytes/row * 32 rows = 256 bytes
			if (ImGui::BeginChild("DumpList", ImVec2(0, 0), ImGuiChildFlags_None)) {
				for (int r = 0; r < nRows; r++) {
					uint16_t wRowAddr = (uint16_t)(wBase + r * 8);
					char szLine[128];
					int nPos = snprintf(szLine, sizeof(szLine),
						"0%04XH : ", wRowAddr);

					char szAscii[9];
					for (int c = 0; c < 8; c++) {
						uint8_t bt = pMemA->ReadMemory(
							(uint16_t)(wRowAddr + c));
						nPos += snprintf(szLine + nPos,
							sizeof(szLine) - nPos, "%02X ", bt);
						szAscii[c] = (bt >= 0x20 && bt <= 0x7E)
							? (char)bt : '.';
					}
					szAscii[8] = '\0';
					snprintf(szLine + nPos, sizeof(szLine) - nPos,
						": %s", szAscii);
					ImGui::TextUnformatted(szLine);
				}
			}
			ImGui::EndChild();
		} else {
			ImGui::TextDisabled("(debugger not active or running)");
		}
	}
	ImGui::End();
}

void DrawBreakpointWindow(bool& bShow)
{
	if (!bShow) {
		return;
	}
	static char szAddr[8] = "";

	ImGui::SetNextWindowSize(ImVec2(300, 280), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Breakpoints", &bShow, ImGuiWindowFlags_NoCollapse)) {
		bool bDebugMode = CPC88::IsDebugMode();

		ImGui::BeginDisabled(!bDebugMode);
		ImGui::SetNextItemWidth(80);
		ImGui::InputText("Addr", szAddr, sizeof(szAddr),
			ImGuiInputTextFlags_CharsHexadecimal);
		ImGui::SameLine();
		if (ImGui::Button("Add") && szAddr[0] != '\0') {
			uint16_t wAddr = (uint16_t)strtoul(szAddr, NULL, 16);
			CPC88::RegisterBreakPoint(wAddr);
			szAddr[0] = '\0';
		}

		ImGui::Separator();

		if (bDebugMode) {
			bool bMain = CPC88::IsDebugMain();
			std::set<uint16_t>* pSet = CPC88::GetBreakPoint(bMain);

			ImGui::Text("CPU: %s  (%d breakpoints)",
				bMain ? "Main" : "Sub", (int)pSet->size());
			ImGui::SameLine();
			if (ImGui::Button("Remove All") && !pSet->empty()) {
				pSet->clear();
			}

			if (ImGui::BeginChild("BPList", ImVec2(0, 0), ImGuiChildFlags_None)) {
				uint16_t wRemove = 0;
				bool bRemoveReq = false;
				for (std::set<uint16_t>::iterator it = pSet->begin();
					it != pSet->end(); ++it)
				{
					ImGui::PushID((int)*it);
					ImGui::Text("%04X", *it);
					ImGui::SameLine();
					if (ImGui::SmallButton("Del")) {
						wRemove = *it;
						bRemoveReq = true;
					}
					ImGui::PopID();
				}
				if (bRemoveReq) {
					CPC88::RemoveBreakPoint(wRemove);
				}
			}
			ImGui::EndChild();
		} else {
			ImGui::TextDisabled("(debugger not active)");
		}
		ImGui::EndDisabled();
	}
	ImGui::End();
}

void DrawWriteRamWindow(bool& bShow)
{
	if (!bShow) {
		return;
	}
	static char szAddr[8] = "";
	static char szData[8] = "";
	static int nTarget = 0; // 0=MainMem, 1=SubMem, 2=I/O

	ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Write RAM", &bShow, ImGuiWindowFlags_NoCollapse)) {
		bool bDebugMode = CPC88::IsDebugMode();
		bool bStopped = CPC88::IsDebugStopped();

		ImGui::BeginDisabled(!bDebugMode || !bStopped);

		ImGui::RadioButton("Main RAM", &nTarget, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Sub RAM", &nTarget, 1);
		ImGui::SameLine();
		ImGui::RadioButton("I/O", &nTarget, 2);

		ImGui::SetNextItemWidth(80);
		ImGui::InputText("Addr", szAddr, sizeof(szAddr),
			ImGuiInputTextFlags_CharsHexadecimal);
		ImGui::SetNextItemWidth(80);
		ImGui::InputText("Data (hex)", szData, sizeof(szData),
			ImGuiInputTextFlags_CharsHexadecimal);

		if (ImGui::Button("Write") && szAddr[0] != '\0' && szData[0] != '\0') {
			uint16_t wAddr = (uint16_t)strtoul(szAddr, NULL, 16);
			uint8_t btData = (uint8_t)strtoul(szData, NULL, 16);
			CZ80Adapter* pMainA = CPC88::Z80Main().GetAdapter();
			CZ80Adapter* pSubA  = CPC88::Z80Sub().GetAdapter();
			if (nTarget == 0 && pMainA) {
				pMainA->WriteMemory(wAddr, btData);
			} else if (nTarget == 1 && pSubA) {
				pSubA->WriteMemory(wAddr, btData);
			} else if (nTarget == 2) {
				CZ80Adapter* pA = CPC88::GetDebugAdapter();
				if (pA) {
					pA->WriteIO(wAddr, btData);
				}
			}
			CPC88::UpdateMnemonic();
		}

		ImGui::EndDisabled();

		if (!bDebugMode || !bStopped) {
			ImGui::TextDisabled("(stop debugger first)");
		}
	}
	ImGui::End();
}

// Export folder dialog result — may be called from a non-main thread.
std::string g_strPendingExportDir;
std::mutex  g_mtxExportDir;

void SDLCALL OnExportFolderSelected(void* userdata, const char* const* filelist, int filter)
{
	(void)userdata;
	(void)filter;
	if (filelist && filelist[0]) {
		std::string strDir = filelist[0];
		if (!strDir.empty() && strDir[strDir.size() - 1] != '/') {
			strDir += '/';
		}
		std::lock_guard<std::mutex> lock(g_mtxExportDir);
		g_strPendingExportDir = strDir;
	}
}

void DrawExportRamWindow(bool& bShow)
{
	if (!bShow) {
		return;
	}
	static bool bMainRam0 = true;
	static bool bMainRam1 = true;
	static bool bFastTVRam = false;
	static bool bSlowTVRam = false;
	static bool bGVRam0 = true;
	static bool bGVRam1 = true;
	static bool bGVRam2 = true;
	static bool bSubRam = true;
	static bool bExRam0 = false;
	static bool bExRam1 = false;
	static std::string strExportDir;
	static std::string strStatus;

	ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Export RAM", &bShow, ImGuiWindowFlags_NoCollapse)) {
		bool bDebugMode = CPC88::IsDebugMode();
		bool bStopped = CPC88::IsDebugStopped();
		bool bFastTVRamUse = CPC88::Z80Main().IsFastTVRamUse();
		bool bSubDisabled = CPC88::IsSubSystemDisableNow();
		bool bMainCPU = CPC88::IsDebugMain();

		ImGui::BeginDisabled(!bDebugMode || !bStopped);

		ImGui::TextUnformatted("RAM:");
		ImGui::Checkbox("Main RAM0 : 0000H-7FFFH", &bMainRam0);
		ImGui::Checkbox("Main RAM1 : 8000H-FFFFH", &bMainRam1);
		if (bFastTVRamUse) {
			ImGui::Checkbox("Slow Text VRAM", &bSlowTVRam);
		} else {
			ImGui::Checkbox("Fast Text VRAM", &bFastTVRam);
		}
		ImGui::Checkbox("Graphic VRAM0", &bGVRam0);
		ImGui::Checkbox("Graphic VRAM1", &bGVRam1);
		ImGui::Checkbox("Graphic VRAM2", &bGVRam2);
		ImGui::BeginDisabled(bSubDisabled);
		ImGui::Checkbox("Subsystem RAM", &bSubRam);
		ImGui::EndDisabled();
		ImGui::Checkbox("Expansion RAM0", &bExRam0);
		ImGui::Checkbox("Expansion RAM1 [VAB]", &bExRam1);

		ImGui::Separator();

		// Consume folder dialog result (thread-safe)
		{
			std::lock_guard<std::mutex> lock(g_mtxExportDir);
			if (!g_strPendingExportDir.empty()) {
				strExportDir = g_strPendingExportDir;
				g_strPendingExportDir.clear();
			}
		}
		// Folder selection
		if (strExportDir.empty()) {
			const char* pHome = getenv("HOME");
			if (pHome && *pHome) {
				strExportDir = std::string(pHome) + "/Documents/";
			} else {
				strExportDir = "./";
			}
		}
		ImGui::Text("Export to: %s", strExportDir.c_str());
		if (ImGui::Button("Browse...")) {
			SDL_ShowOpenFolderDialog(
				OnExportFolderSelected, NULL,
				NULL, strExportDir.c_str(), false);
		}
		ImGui::SameLine();
		if (ImGui::Button("Export")) {
			// Create timestamped subfolder: X88000M_RAM_YYYYMMDD_HHMMSS/
			char szTime[64];
			{
				time_t t = time(NULL);
				struct tm* pTm = localtime(&t);
				strftime(szTime, sizeof(szTime),
					"X88000M_RAM_%Y%m%d_%H%M%S", pTm);
			}
			std::string fstrDir = strExportDir + szTime + "/";
			mkdir(fstrDir.c_str(), 0755);
			int nExported = 0;
			FILE* fpt;
			std::string fstrPath;

			if (bMainRam0) {
				fstrPath = fstrDir + "main0.ram";
				fpt = fopen(fstrPath.c_str(), "wb");
				if (fpt) {
					fwrite(CPC88::Z80Main().GetMainRamPtr(),
						0x8000, 1, fpt);
					fclose(fpt);
					nExported++;
				}
			}
			if (bMainRam1) {
				fstrPath = fstrDir + "main1.ram";
				fpt = fopen(fstrPath.c_str(), "wb");
				if (fpt) {
					if (bFastTVRamUse) {
						fwrite(CPC88::Z80Main().GetMainRamPtr() + 0x8000,
							0x7000, 1, fpt);
						fwrite(CPC88::Z80Main().GetFastTVRamPtr(),
							0x1000, 1, fpt);
					} else {
						fwrite(CPC88::Z80Main().GetMainRamPtr() + 0x8000,
							0x8000, 1, fpt);
					}
					fclose(fpt);
					nExported++;
				}
			}
			if (bFastTVRam && !bFastTVRamUse) {
				fstrPath = fstrDir + "fast_tv.ram";
				fpt = fopen(fstrPath.c_str(), "wb");
				if (fpt) {
					fwrite(CPC88::Z80Main().GetFastTVRamPtr(),
						0x1000, 1, fpt);
					fclose(fpt);
					nExported++;
				}
			}
			if (bSlowTVRam && bFastTVRamUse) {
				fstrPath = fstrDir + "slow_tv.ram";
				fpt = fopen(fstrPath.c_str(), "wb");
				if (fpt) {
					fwrite(CPC88::Z80Main().GetMainRamPtr() + 0xF000,
						0x1000, 1, fpt);
					fclose(fpt);
					nExported++;
				}
			}
			if (bGVRam0) {
				fstrPath = fstrDir + "gv0.ram";
				fpt = fopen(fstrPath.c_str(), "wb");
				if (fpt) {
					fwrite(CPC88::Z80Main().GetGVRamPtr(0),
						0x4000, 1, fpt);
					fclose(fpt);
					nExported++;
				}
			}
			if (bGVRam1) {
				fstrPath = fstrDir + "gv1.ram";
				fpt = fopen(fstrPath.c_str(), "wb");
				if (fpt) {
					fwrite(CPC88::Z80Main().GetGVRamPtr(1),
						0x4000, 1, fpt);
					fclose(fpt);
					nExported++;
				}
			}
			if (bGVRam2) {
				fstrPath = fstrDir + "gv2.ram";
				fpt = fopen(fstrPath.c_str(), "wb");
				if (fpt) {
					fwrite(CPC88::Z80Main().GetGVRamPtr(2),
						0x4000, 1, fpt);
					fclose(fpt);
					nExported++;
				}
			}
			if (bSubRam && !bSubDisabled) {
				fstrPath = fstrDir + "sub.ram";
				fpt = fopen(fstrPath.c_str(), "wb");
				if (fpt) {
					fwrite(CPC88::Z80Sub().GetSubRamPtr(),
						0x4000, 1, fpt);
					fclose(fpt);
					nExported++;
				}
			}
			if (bExRam0) {
				fstrPath = fstrDir + "ex0.ram";
				fpt = fopen(fstrPath.c_str(), "wb");
				if (fpt) {
					fwrite(CPC88::Z80Main().GetExRamPtr(0),
						0x8000 * 4, 1, fpt);
					fclose(fpt);
					nExported++;
				}
			}
			if (bExRam1) {
				fstrPath = fstrDir + "ex1.ram";
				fpt = fopen(fstrPath.c_str(), "wb");
				if (fpt) {
					fwrite(CPC88::Z80Main().GetExRamPtr(1),
						0x8000 * 4, 1, fpt);
					fclose(fpt);
					nExported++;
				}
			}

			char szMsg[256];
			snprintf(szMsg, sizeof(szMsg),
				"Exported %d file(s) to %s", nExported, fstrDir.c_str());
			strStatus = szMsg;
		}

		ImGui::EndDisabled();

		if (!strStatus.empty()) {
			ImGui::TextWrapped("%s", strStatus.c_str());
		}
		if (!bDebugMode || !bStopped) {
			ImGui::TextDisabled("(stop debugger first)");
		}
	}
	ImGui::End();
}

#endif // X88000_SDL3_HAS_IMGUI

std::string EnsureTrailingSlash(const std::string& fstrPath)
{
	if (fstrPath.empty() || (fstrPath[fstrPath.length()-1] == '/')) {
		return fstrPath;
	}
	return fstrPath + "/";
}

void RegisterRomSearchDir(const std::string& fstrPath)
{
	if (fstrPath.empty()) {
		return;
	}
	std::string fstrNorm = EnsureTrailingSlash(fstrPath);
	for (size_t n = 0; n < g_vRomSearchDir.size(); n++) {
		if (g_vRomSearchDir[n] == fstrNorm) {
			return;
		}
	}
	g_vRomSearchDir.push_back(fstrNorm);
}

FILE* OpenSystemFileFromDirs(const std::string& strName)
{
	std::string fstrUpperName = strName;
	for (size_t n = 0; n < fstrUpperName.size(); n++) {
		char ch = fstrUpperName[n];
		if ((ch >= 'a') && (ch <= 'z')) {
			fstrUpperName[n] = (char)toupper(ch);
		}
	}

	for (size_t nDir = 0; nDir < g_vRomSearchDir.size(); nDir++) {
		const std::string& fstrDir = g_vRomSearchDir[nDir];
		std::string fstrPath = fstrDir + strName;
		FILE* fpt = fopen(fstrPath.c_str(), "rb");
		if (fpt != NULL) {
			return fpt;
		}
#ifdef X88_PLATFORM_UNIX
		if (fstrUpperName != strName) {
			fstrPath = fstrDir + fstrUpperName;
			fpt = fopen(fstrPath.c_str(), "rb");
			if (fpt != NULL) {
				return fpt;
			}
		}
#endif
	}
	return NULL;
}

// Debug execution log recording
FILE* g_pfDebugLog = NULL;
int   g_nDebugLogCol = 0;
enum { DEBUGLOG_COLMAX = 8 };

bool StartDebugLog()
{
	if (g_pfDebugLog != NULL) return false;
	char szTime[64];
	{
		time_t t = time(NULL);
		struct tm* pTm = localtime(&t);
		strftime(szTime, sizeof(szTime),
			"X88000M_ExecTrace_%Y%m%d_%H%M%S.log", pTm);
	}
	std::string fstrPath;
	const char* pHome = getenv("HOME");
	if (pHome && *pHome) {
		fstrPath = std::string(pHome) + "/Documents/" + szTime;
	} else {
		fstrPath = szTime;
	}
	g_pfDebugLog = fopen(fstrPath.c_str(), "at");
	if (g_pfDebugLog != NULL) {
		g_nDebugLogCol = 0;
	}
	return g_pfDebugLog != NULL;
}

bool EndDebugLog()
{
	if (g_pfDebugLog == NULL) return false;
	if (CPC88::IsDebugMode()) {
		if (g_nDebugLogCol > 0) {
			fputc('\n', g_pfDebugLog);
			g_nDebugLogCol = 0;
		}
		fprintf(g_pfDebugLog, "Log End\n\n");
	}
	fflush(g_pfDebugLog);
	fclose(g_pfDebugLog);
	g_pfDebugLog = NULL;
	return true;
}

bool IsDebugLogging()
{
	return g_pfDebugLog != NULL;
}

void OutputCoreDebugLog(int nLogMode)
{
	if (g_pfDebugLog == NULL) return;
	if (nLogMode != CPC88::DEBUGLOG_EXECUTE) {
		if (g_nDebugLogCol > 0) {
			fputc('\n', g_pfDebugLog);
			g_nDebugLogCol = 0;
		}
	}
	switch (nLogMode) {
	case CPC88::DEBUGLOG_START:
		fprintf(g_pfDebugLog, "Log Start(%s)\n",
			CPC88::IsDebugMain() ? "Main" : "Sub");
		break;
	case CPC88::DEBUGLOG_END:
		fprintf(g_pfDebugLog, "Log End\n\n");
		break;
	case CPC88::DEBUGLOG_CHANGE_CPU:
		fprintf(g_pfDebugLog, "Change CPU(%s)\n",
			CPC88::IsDebugMain() ? "Main" : "Sub");
		break;
	case CPC88::DEBUGLOG_RESET:
		fprintf(g_pfDebugLog, "Reset\n");
		break;
	case CPC88::DEBUGLOG_READ_MEMIMAGE:
		fprintf(g_pfDebugLog, "Read Memory Image\n");
		break;
	}
	if (nLogMode != CPC88::DEBUGLOG_END) {
		CZ80Adapter* pA = CPC88::GetDebugAdapter();
		if (pA) {
			if (g_nDebugLogCol > 0) {
				fputc(' ', g_pfDebugLog);
			}
			fprintf(g_pfDebugLog, "0%04XH", pA->RegPC().Get());
			if (++g_nDebugLogCol >= DEBUGLOG_COLMAX) {
				fputc('\n', g_pfDebugLog);
				g_nDebugLogCol = 0;
			}
		}
	}
}

void ApplyTintStyle()
{
	ImVec4 tint(141/255.0f, 105/255.0f, 96/255.0f, 1.0f);
	ImVec4 tintDim(tint.x * 0.6f, tint.y * 0.6f, tint.z * 0.6f, 1.0f);
	ImVec4 tintBright(
		tint.x + (1.0f - tint.x) * 0.3f,
		tint.y + (1.0f - tint.y) * 0.3f,
		tint.z + (1.0f - tint.z) * 0.3f, 1.0f);
	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_TitleBg]          = tintDim;
	style.Colors[ImGuiCol_TitleBgActive]    = tint;
	style.Colors[ImGuiCol_Header]           = ImVec4(tint.x, tint.y, tint.z, 0.5f);
	style.Colors[ImGuiCol_HeaderHovered]    = ImVec4(tint.x, tint.y, tint.z, 0.7f);
	style.Colors[ImGuiCol_HeaderActive]     = tint;
	style.Colors[ImGuiCol_Button]           = ImVec4(tint.x, tint.y, tint.z, 0.5f);
	style.Colors[ImGuiCol_ButtonHovered]    = ImVec4(tint.x, tint.y, tint.z, 0.7f);
	style.Colors[ImGuiCol_ButtonActive]     = tintBright;
	style.Colors[ImGuiCol_FrameBg]          = ImVec4(tintDim.x, tintDim.y, tintDim.z, 0.5f);
	style.Colors[ImGuiCol_FrameBgHovered]   = ImVec4(tint.x, tint.y, tint.z, 0.4f);
	style.Colors[ImGuiCol_FrameBgActive]    = ImVec4(tint.x, tint.y, tint.z, 0.6f);
	style.Colors[ImGuiCol_CheckMark]        = tintBright;
	style.Colors[ImGuiCol_SliderGrab]       = tint;
	style.Colors[ImGuiCol_SliderGrabActive] = tintBright;
	style.Colors[ImGuiCol_Tab]                    = tintDim;
	style.Colors[ImGuiCol_TabHovered]             = tint;
	style.Colors[ImGuiCol_TabSelected]            = tint;
	style.Colors[ImGuiCol_TabDimmed]              = ImVec4(tintDim.x * 0.6f, tintDim.y * 0.6f, tintDim.z * 0.6f, 1.0f);
	style.Colors[ImGuiCol_TabDimmedSelected]      = tintDim;
	style.Colors[ImGuiCol_TabSelectedOverline]    = tintBright;
	style.Colors[ImGuiCol_TabDimmedSelectedOverline] = tint;
}

// ---- Debug Window (separate SDL3 window) ----

struct SDebugWindow {
	SDL_Window*    pWindow;
	SDL_Renderer*  pRenderer;
#ifdef X88000_SDL3_HAS_IMGUI
	ImGuiContext*  pImGuiCtx;
#endif
	SDL_WindowID   nWindowID;
	bool           bOpen;
	bool           bShowDisasm;
	bool           bShowMemDump;
	bool           bShowBreakpoint;
	bool           bShowWriteRam;
	bool           bShowExportRam;
	bool           bNeedInitLayout;
	std::string    strIniPath;
};

void InitDebugWindowStruct(SDebugWindow& dw)
{
	dw.pWindow = NULL;
	dw.pRenderer = NULL;
#ifdef X88000_SDL3_HAS_IMGUI
	dw.pImGuiCtx = NULL;
#endif
	dw.nWindowID = 0;
	dw.bOpen = false;
	dw.bShowDisasm = false;
	dw.bShowMemDump = false;
	dw.bShowBreakpoint = false;
	dw.bShowWriteRam = false;
	dw.bShowExportRam = false;
	dw.bNeedInitLayout = false;
	dw.strIniPath.clear();
}

bool OpenDebugWindow(SDebugWindow& dw, ImGuiContext* pMainCtx,
	CSdl3Settings& settings)
{
	if (dw.bOpen) return true;

	int nDbgW = settings.GetInt("dbgwindow.width", 800);
	int nDbgH = settings.GetInt("dbgwindow.height", 700);
	int nDbgX = settings.GetInt("dbgwindow.x", SDL_WINDOWPOS_CENTERED);
	int nDbgY = settings.GetInt("dbgwindow.y", SDL_WINDOWPOS_CENTERED);
	if (nDbgW < 400) nDbgW = 400;
	if (nDbgH < 300) nDbgH = 300;
	dw.pWindow = SDL_CreateWindow("X88000M Debugger",
		nDbgW, nDbgH, SDL_WINDOW_RESIZABLE);
	if (dw.pWindow && nDbgX != SDL_WINDOWPOS_CENTERED) {
		SDL_SetWindowPosition(dw.pWindow, nDbgX, nDbgY);
	}
	if (!dw.pWindow) return false;

	dw.pRenderer = SDL_CreateRenderer(dw.pWindow, NULL);
	if (!dw.pRenderer) {
		SDL_DestroyWindow(dw.pWindow);
		dw.pWindow = NULL;
		return false;
	}

	dw.nWindowID = SDL_GetWindowID(dw.pWindow);

#ifdef X88000_SDL3_HAS_IMGUI
	// Create an independent ImGui context with its own font atlas,
	// because the renderer backend binds font textures per-renderer
	// and a shared atlas would not have a valid texture for this renderer.
	dw.pImGuiCtx = ImGui::CreateContext();
	ImGui::SetCurrentContext(dw.pImGuiCtx);
	ImGuiIO& dbgIO = ImGui::GetIO();
	// INI path must be set by caller via dw.strIniPath before calling.
	dbgIO.IniFilename = dw.strIniPath.empty()
		? NULL : dw.strIniPath.c_str();
	dbgIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// Load the same font for the debug context.
	{
		const char* pBase = SDL_GetBasePath();
		if (pBase) {
			std::string sFontPath = std::string(pBase)
				+ "../Resources/fonts/NotoSansJP-Regular.ttf";
			ImFontConfig fontCfg;
			fontCfg.OversampleH = 2;
			fontCfg.OversampleV = 1;
			ImFont* pFont = dbgIO.Fonts->AddFontFromFileTTF(
				sFontPath.c_str(), 20.0f, &fontCfg,
				dbgIO.Fonts->GetGlyphRangesJapanese());
			if (!pFont) {
				dbgIO.Fonts->AddFontDefault();
			}
		} else {
			dbgIO.Fonts->AddFontDefault();
		}
	}

	ImGui::StyleColorsDark();
	ApplyTintStyle();
	ImGui_ImplSDL3_InitForSDLRenderer(dw.pWindow, dw.pRenderer);
	ImGui_ImplSDLRenderer3_Init(dw.pRenderer);
	ImGui::SetCurrentContext(pMainCtx);
#endif

	dw.bOpen = true;
	dw.bShowDisasm = true;
	dw.bShowMemDump = true;
	dw.bShowBreakpoint = true;
	dw.bShowWriteRam = true;
	dw.bShowExportRam = true;
	// Only build initial layout if no saved INI exists.
	{
		FILE* fTest = fopen(dw.strIniPath.c_str(), "r");
		if (fTest) {
			fclose(fTest);
			dw.bNeedInitLayout = false;
		} else {
			dw.bNeedInitLayout = true;
		}
	}

	// Enter debug mode (Main CPU by default)
	if (!CPC88::IsDebugMode()) {
		CPC88::SetDebugExecMode(CPC88::DEBUGEXEC_MAIN);
	}
	return true;
}

void CloseDebugWindow(SDebugWindow& dw, ImGuiContext* pMainCtx,
	CSdl3Settings& settings)
{
	if (!dw.bOpen) return;

	// Persist debug window size and position
	{
		int nW = 0, nH = 0, nX = 0, nY = 0;
		SDL_GetWindowSize(dw.pWindow, &nW, &nH);
		SDL_GetWindowPosition(dw.pWindow, &nX, &nY);
		if (nW > 0 && nH > 0) {
			settings.SetInt("dbgwindow.width", nW);
			settings.SetInt("dbgwindow.height", nH);
			settings.SetInt("dbgwindow.x", nX);
			settings.SetInt("dbgwindow.y", nY);
		}
	}

#ifdef X88000_SDL3_HAS_IMGUI
	ImGui::SetCurrentContext(dw.pImGuiCtx);
	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	// DestroyContext with shared font atlas: ImGui will NOT free the atlas.
	ImGui::DestroyContext(dw.pImGuiCtx);
	ImGui::SetCurrentContext(pMainCtx);
	dw.pImGuiCtx = NULL;
#endif

	SDL_DestroyRenderer(dw.pRenderer);
	SDL_DestroyWindow(dw.pWindow);
	dw.pRenderer = NULL;
	dw.pWindow = NULL;
	dw.bOpen = false;

	// Exit debug mode
	if (CPC88::IsDebugMode()) {
		CPC88::SetDebugExecMode(CPC88::DEBUGEXEC_NONE);
	}
	if (IsDebugLogging()) {
		EndDebugLog();
	}
}

void OnCoreIntVectChanged()
{
	// TODO: route interrupt-vector changes to frontend debugger indicators.
}

void OnCoreBeepOutput(bool bBeepPort, bool bExBeepPort)
{
	g_audio.SetBeepEnabled(bBeepPort, bExBeepPort);
}

void OnCorePcgOutput(int nChannel, int nCounter)
{
	g_audio.SetPcgChannel(nChannel, nCounter);
}

void OnCoreOpnaSampleOutput(const int16_t* pSamples, int nFrames)
{
	g_audio.PushOpnSamples(pSamples, nFrames);
}

std::string ToLowerAscii(std::string strText)
{
	for (size_t n = 0; n < strText.size(); n++) {
		char ch = strText[n];
		if ((ch >= 'A') && (ch <= 'Z')) {
			strText[n] = (char)(ch-'A'+'a');
		}
	}
	return strText;
}

std::string GetLowerFileExt(const std::string& fstrPath)
{
	std::string::size_type nPos = fstrPath.find_last_of('.');
	if (nPos == std::string::npos) {
		return "";
	}
	return ToLowerAscii(fstrPath.substr(nPos));
}

void SetMediaStatus(const std::string& strStatus)
{
	g_strLastMediaStatus = strStatus;
}

int OpenDiskImageFile(
	const std::string& fstrFileName, bool& bReadOnly,
	uint8_t*& pbtData, uint32_t& dwSize)
{
	CX88DiskImageMemory dim;
	int nResult = dim.Create(fstrFileName, bReadOnly);
	if (nResult == CDiskImageFile::ERR_NOERROR) {
		bReadOnly = dim.IsReadOnly();
		pbtData = dim.GetData();
		dwSize = dim.GetSize();
		g_setDiskImageMemory.insert(dim);
	}
	return nResult;
}

int CloseDiskImageFile(uint8_t* pbtData)
{
	std::set<CX88DiskImageMemory>::iterator itInfo =
		g_setDiskImageMemory.find(CX88DiskImageMemory(pbtData));
	if (itInfo == g_setDiskImageMemory.end()) {
		return CDiskImageFile::ERR_ERROR;
	}
	int nResult = (*itInfo).Flush();
	(*itInfo).Destroy();
	g_setDiskImageMemory.erase(itInfo);
	return nResult;
}

bool MountDiskImageByIndex(int nDrive, int nDiskImageIndex)
{
	if ((nDrive < 0) || (nDrive >= CPC88Fdc::DRIVE_MAX)) {
		return false;
	}
	CDiskImage* pDiskImage = CPC88::GetDiskImageCollection().GetDiskImage(nDiskImageIndex);
	if (pDiskImage == NULL) {
		return false;
	}
	CPC88::Fdc().SetDiskImage(nDrive, pDiskImage);
	g_anDriveDiskIndex[nDrive] = nDiskImageIndex;
	return true;
}

void EjectDiskImageFromDrive(int nDrive)
{
	if ((nDrive < 0) || (nDrive >= CPC88Fdc::DRIVE_MAX)) {
		return;
	}
	CPC88::Fdc().SetDiskImage(nDrive, NULL);
	g_anDriveDiskIndex[nDrive] = -1;
	g_astrDriveMediaPath[nDrive].clear();
}

int ResolveDriveNo(int nDrive, bool bAllowAutoAssignDrive)
{
	if ((nDrive >= 0) && (nDrive < CPC88Fdc::DRIVE_MAX)) {
		return nDrive;
	}
	if (bAllowAutoAssignDrive) {
		for (int nDrive2 = 0; nDrive2 < CPC88Fdc::DRIVE_MAX; nDrive2++) {
			if (!CPC88::Fdc().IsDriveReady(nDrive2)) {
				return nDrive2;
			}
		}
		return 0;
	}
	return -1;
}

void RecordDiskFile(const std::string& fstrPath, int nStartImageIndex, int nImageCount)
{
	SDiskFileRecord rec;
	rec.strPath = fstrPath;
	rec.nStartImageIndex = nStartImageIndex;
	rec.nImageCount = nImageCount;
	g_vDiskFileRecords.push_back(rec);
}

std::string GetDriveDiskImageLabel(int nDrive)
{
	if ((nDrive < 0) || (nDrive >= CPC88Fdc::DRIVE_MAX)) {
		return "(invalid drive)";
	}
	int nImageIndex = g_anDriveDiskIndex[nDrive];
	if (nImageIndex < 0) {
		return "(empty)";
	}
	CDiskImage* pDiskImage = CPC88::GetDiskImageCollection().GetDiskImage(nImageIndex);
	if (pDiskImage == NULL) {
		return "(missing image)";
	}
	std::string strLabel = "#" + std::to_string(nImageIndex+1);
	std::string strName = pDiskImage->GetImageName();
	if (!strName.empty()) {
		strLabel += " ";
		strLabel += strName;
	}
	return strLabel;
}

bool AddMediaImage(
	const std::string& fstrFileName,
	int nDrive,
	bool bAllowAutoAssignDrive)
{
	if (fstrFileName.empty()) {
		return false;
	}
	std::string strExt = GetLowerFileExt(fstrFileName);
	if (strExt == ".t88") {
		int nResult = CPC88::Usart().GetLoadTapeImage().Load(fstrFileName);
		SetMediaStatus((nResult == CTapeImage::ERR_NOERROR)?
			"Loaded tape image: " + fstrFileName:
			"Failed to load tape image: " + fstrFileName);
		return nResult == CTapeImage::ERR_NOERROR;
	}
	if (strExt == ".cmt") {
		int nResult = CPC88::Usart().GetLoadTapeImage().LoadCMT(fstrFileName);
		SetMediaStatus((nResult == CTapeImage::ERR_NOERROR)?
			"Loaded CMT image: " + fstrFileName:
			"Failed to load CMT image: " + fstrFileName);
		return nResult == CTapeImage::ERR_NOERROR;
	}
	nDrive = ResolveDriveNo(nDrive, bAllowAutoAssignDrive);
	if (nDrive < 0) {
		SetMediaStatus("No drive specified for disk image: " + fstrFileName);
		return false;
	}
	int nBefore = CPC88::GetDiskImageCollection().GetDiskImageCount();
	if (!CPC88::GetDiskImageCollection().AddDiskImageFile(fstrFileName, false)) {
		SetMediaStatus("Failed to insert disk image: " + fstrFileName);
		return false;
	}
	int nAfter = CPC88::GetDiskImageCollection().GetDiskImageCount();
	int nAdded = nAfter-nBefore;
	if (nAdded <= 0) {
		SetMediaStatus("Failed to parse disk image(s): " + fstrFileName);
		return false;
	}
	RecordDiskFile(fstrFileName, nBefore, nAdded);
	if (!MountDiskImageByIndex(nDrive, nBefore)) {
		SetMediaStatus("Failed to mount parsed disk image: " + fstrFileName);
		return false;
	}
	g_astrDriveMediaPath[nDrive] = fstrFileName;
	if (nAdded > 1) {
		SetMediaStatus(
			"Inserted " + std::to_string(nAdded) + " images from D88 to drive " +
			std::to_string(nDrive+1) + " (using image 1).");
	} else {
		SetMediaStatus("Inserted disk image into drive " + std::to_string(nDrive+1) + ": " + fstrFileName);
	}
	return true;
}

void ParseInitialMediaArgs(int argc, char** argv)
{
	for (int nArg = 1; nArg < argc; nArg++) {
		const char* pszArg = argv[nArg];
		if ((pszArg == NULL) || (*pszArg == '\0')) {
			continue;
		}
		std::string strArg = pszArg;
		if (strArg.rfind("--disk1=", 0) == 0) {
			AddMediaImage(strArg.substr(8), 0, false);
		} else if (strArg.rfind("--disk2=", 0) == 0) {
			AddMediaImage(strArg.substr(8), 1, false);
		} else if (strArg.rfind("--tape=", 0) == 0) {
			AddMediaImage(strArg.substr(7), -1, false);
		} else if (strArg[0] != '-') {
			AddMediaImage(strArg, -1, true);
		}
	}
}

void EnqueueDialogMediaPath(const std::string& fstrPath, int nDrive)
{
	std::lock_guard<std::mutex> lock(g_mtxDialogQueue);
	g_vDialogMediaQueue.push_back(std::make_pair(fstrPath, nDrive));
}

void SDLCALL OnMediaFileDialogResult(
	void* pUserData,
	const char* const* ppszFileList,
	int)
{
	int nDrive = (int)((intptr_t)pUserData)-2;
	if (ppszFileList == NULL) {
		return;
	}
	for (int nFile = 0; ppszFileList[nFile] != NULL; nFile++) {
		EnqueueDialogMediaPath(ppszFileList[nFile], nDrive);
	}
}

void RequestOpenMediaDialog(SDL_Window* pWindow, int nDrive)
{
	static const SDL_DialogFileFilter s_aFilters[] = {
		{"Media Images", "d88;t88;cmt"},
		{"Disk Images", "d88"},
		{"Tape Images", "t88;cmt"},
		{"All Files", "*"}
	};
	SDL_ShowOpenFileDialog(
		OnMediaFileDialogResult,
		(void*)(intptr_t)(nDrive+2),
		pWindow,
		s_aFilters,
		(int)(sizeof(s_aFilters)/sizeof(s_aFilters[0])),
		NULL,
		false);
}

void RequestOpenDiskOnlyDialog(SDL_Window* pWindow, int nDrive)
{
	static const SDL_DialogFileFilter s_aFilters[] = {
		{"Disk Images", "d88"},
		{"All Files", "*"}
	};
	SDL_ShowOpenFileDialog(
		OnMediaFileDialogResult,
		(void*)(intptr_t)(nDrive+2),
		pWindow,
		s_aFilters,
		(int)(sizeof(s_aFilters)/sizeof(s_aFilters[0])),
		NULL,
		false);
}

void RequestOpenTapeOnlyDialog(SDL_Window* pWindow)
{
	static const SDL_DialogFileFilter s_aFilters[] = {
		{"Tape Images", "t88;cmt"},
		{"T88 Images", "t88"},
		{"CMT Images", "cmt"},
		{"All Files", "*"}
	};
	SDL_ShowOpenFileDialog(
		OnMediaFileDialogResult,
		(void*)(intptr_t)((-1)+2), // -1 = tape (no drive)
		pWindow,
		s_aFilters,
		(int)(sizeof(s_aFilters)/sizeof(s_aFilters[0])),
		NULL,
		false);
}

// Drop the entire disk image collection and clear all drive bindings.
// This is currently the only "remove" operation we support (matches the
// legacy "remove all" button). Per-file removal can be added later if
// needed.
void EraseAllDiskImages()
{
	for (int n = 0; n < CPC88Fdc::DRIVE_MAX; n++) {
		EjectDiskImageFromDrive(n);
	}
	CDiskImageCollection& dicDisks = CPC88::GetDiskImageCollection();
	while (dicDisks.size() > 0) {
		CDiskImageCollection::iterator itFile = dicDisks.begin();
		dicDisks.erase(itFile);
	}
	g_vDiskFileRecords.clear();
	SetMediaStatus("Removed all disk images");
}

// Find which drive (if any) currently holds the given global disk index.
int FindDriveHoldingDiskIndex(int nDiskImageIndex)
{
	for (int nDrive = 0; nDrive < CPC88Fdc::DRIVE_MAX; nDrive++) {
		if (g_anDriveDiskIndex[nDrive] == nDiskImageIndex) {
			return nDrive;
		}
	}
	return -1;
}

void ProcessQueuedMediaFromDialog(SDL_Window* pWindow, bool bCoreReady, bool bPauseEmulation)
{
	std::vector<std::pair<std::string, int> > vPending;
	{
		std::lock_guard<std::mutex> lock(g_mtxDialogQueue);
		if (g_vDialogMediaQueue.empty()) {
			return;
		}
		vPending.swap(g_vDialogMediaQueue);
	}
	for (size_t n = 0; n < vPending.size(); n++) {
		int nDrive = vPending[n].second;
		bool bAllowAutoAssign = nDrive < 0;
		AddMediaImage(vPending[n].first, nDrive, bAllowAutoAssign);
	}
	UpdateWindowTitle(pWindow, bCoreReady, bPauseEmulation);
}

void ResetCoreState()
{
	CPC88::Reset();
	SetMediaStatus("System reset");
}

void UpdateWindowTitle(SDL_Window* pWindow, bool bCoreReady, bool bPauseEmulation)
{
	if (pWindow == NULL) {
		return;
	}
	std::string strTitle = "X88000 SDL3";
	if (!bCoreReady) {
		strTitle += " [ROM not found]";
	} else if (bPauseEmulation) {
		strTitle += " [Paused]";
	}
	if (!g_strLastMediaStatus.empty()) {
		strTitle += " - ";
		strTitle += g_strLastMediaStatus;
	}
	SDL_SetWindowTitle(pWindow, strTitle.c_str());
}

void ToggleFullscreen(SDL_Window* pWindow)
{
	if (pWindow == NULL) {
		return;
	}
	bool bFullscreen = (SDL_GetWindowFlags(pWindow) & SDL_WINDOW_FULLSCREEN) != 0;
	if (bFullscreen) {
		SDL_SetWindowFullscreen(pWindow, false);
	} else {
		SDL_SetWindowFullscreen(pWindow, true);
	}
}

SDL_FRect CalcLetterboxRect(int nWindowW, int nWindowH, int nSrcW, int nSrcH)
{
	SDL_FRect rctDst;
	rctDst.x = 0.0f;
	rctDst.y = 0.0f;
	rctDst.w = (float)nWindowW;
	rctDst.h = (float)nWindowH;
	if ((nWindowW <= 0) || (nWindowH <= 0) || (nSrcW <= 0) || (nSrcH <= 0)) {
		return rctDst;
	}

	float fScaleX = (float)nWindowW/(float)nSrcW;
	float fScaleY = (float)nWindowH/(float)nSrcH;
	float fScale = (fScaleX < fScaleY)? fScaleX: fScaleY;
	float fW = (float)nSrcW*fScale;
	float fH = (float)nSrcH*fScale;
	rctDst.w = fW;
	rctDst.h = fH;
	rctDst.x = ((float)nWindowW-fW)*0.5f;
	rctDst.y = ((float)nWindowH-fH)*0.5f;
	return rctDst;
}

bool ProbeRomAvailability()
{
	static const char* s_apszProbeRomName[] = {
		"pc88.rom",
		"n88.rom",
		"8801-n88.rom"
	};
	for (size_t n = 0; n < sizeof(s_apszProbeRomName)/sizeof(s_apszProbeRomName[0]); n++) {
		FILE* fpt = OpenSystemFileFromDirs(s_apszProbeRomName[n]);
		if (fpt != NULL) {
			fclose(fpt);
			return true;
		}
	}
	return false;
}

bool InitializeCore()
{
	const char* pszRomDir = getenv("X88_ROM_DIR");
	if ((pszRomDir != NULL) && (*pszRomDir != '\0')) {
		RegisterRomSearchDir(pszRomDir);
	}
	RegisterRomSearchDir(".");

#ifdef __APPLE__
	const char* pszHome = getenv("HOME");
	if ((pszHome != NULL) && (*pszHome != '\0')) {
		RegisterRomSearchDir(
			std::string(pszHome) + "/Library/Application Support/X88000M");
	}
#endif

	CPC88::SetOutputDebugLogCallback(OutputCoreDebugLog);
	CPC88::SetSysFileOpenCallback(OpenSystemFileFromDirs);
	CDiskImageFile::SetDiskImageFileOpenCallback(OpenDiskImageFile);
	CDiskImageFile::SetDiskImageFileCloseCallback(CloseDiskImageFile);
	CPC88::Z80Main().SetIntVectChangeCallback(OnCoreIntVectChanged);
	CPC88::Z80Main().SetBeepOutputCallback(OnCoreBeepOutput);
	CPC88::Pcg().SetPcgSoundOutputCallback(OnCorePcgOutput);
	CPC88::Opna().SetSampleOutputCallback(OnCoreOpnaSampleOutput);
	CPC88::Opna().SetSampleRate(44100);
	CPC88::Z80Main().SetParallelDevice(g_parallelNull);
	g_parallelNull.Initialize();
	g_parallelNull.Reset();
	if (!ProbeRomAvailability()) {
		return false;
	}
	CPC88::Initialize();
	CPC88::Reset();
	g_bScreenDrawerReady = g_screenDrawer.Create(g_pc88, false);
	return true;
}

void ShutdownCore()
{
	CDiskImageCollection& dicDisks = CPC88::GetDiskImageCollection();
	while (dicDisks.size() > 0) {
		CDiskImageCollection::iterator itFile = dicDisks.begin();
		dicDisks.erase(itFile);
	}
	g_setDiskImageMemory.clear();
	g_vDiskFileRecords.clear();
	if (g_bScreenDrawerReady) {
		g_screenDrawer.Destroy();
		g_bScreenDrawerReady = false;
	}
}

bool UpdateCoreFrame()
{
	if (!g_bScreenDrawerReady) {
		return false;
	}
	bool bUpdate = g_screenDrawer.RatchText();
	if (bUpdate) {
		g_screenDrawer.DrawScreen();
		if (g_screenDrawer.UpdatePalette()) {
			bUpdate = true;
		}
	} else {
		bUpdate = g_screenDrawer.UpdatePalette();
	}
	if (CPC88::Z80Main().IsVABScreenActive()) {
		bUpdate = g_screenDrawer.DrawScreenVAB() || bUpdate;
	}
	return bUpdate;
}

bool UploadCoreFrameToTexture(
	SDL_Texture* pTexture,
	std::vector<uint32_t>& vArgbBuffer)
{
	if (!g_bScreenDrawerReady || (pTexture == NULL)) {
		return false;
	}
	if (vArgbBuffer.size() != 640U*400U) {
		vArgbBuffer.resize(640U*400U);
	}

	if (CPC88::Z80Main().IsVABScreenActive()) {
		uint8_t* pbtRgb = g_screenDrawer.GetScreenDataBits2();
		if (pbtRgb == NULL) {
			return false;
		}
		for (int n = 0; n < 640*400; n++) {
			uint8_t btR = pbtRgb[n*3+0];
			uint8_t btG = pbtRgb[n*3+1];
			uint8_t btB = pbtRgb[n*3+2];
			vArgbBuffer[n] = 0xFF000000U |
				(uint32_t(btR) << 16) |
				(uint32_t(btG) << 8) |
				uint32_t(btB);
		}
	} else {
		uint8_t* pbtIndex = g_screenDrawer.GetScreenDataBits();
		if (pbtIndex == NULL) {
			return false;
		}
		GdkColor* pColorTable = g_screenDrawer.GetColorTable();
		for (int n = 0; n < 640*400; n++) {
			uint8_t btPal = pbtIndex[n];
			uint8_t btR = (uint8_t)(pColorTable[btPal].red >> 8);
			uint8_t btG = (uint8_t)(pColorTable[btPal].green >> 8);
			uint8_t btB = (uint8_t)(pColorTable[btPal].blue >> 8);
			vArgbBuffer[n] = 0xFF000000U |
				(uint32_t(btR) << 16) |
				(uint32_t(btG) << 8) |
				uint32_t(btB);
		}
	}
	return SDL_UpdateTexture(pTexture, NULL, &vArgbBuffer[0], 640*4);
}

bool IsScancodePressed(const bool* pabKeyboardState, int nScancodeCount, SDL_Scancode nScancode)
{
	int nIndex = (int)nScancode;
	if ((nIndex < 0) || (nIndex >= nScancodeCount)) {
		return false;
	}
	return pabKeyboardState[nIndex];
}

void SetKeyMatrixByScancode(
	int nAddress,
	int nBit,
	const bool* pabKeyboardState,
	int nScancodeCount,
	SDL_Scancode nScancode)
{
	CPC88::Z80Main().SetKeyMatrics(
		nAddress,
		nBit,
		IsScancodePressed(pabKeyboardState, nScancodeCount, nScancode));
}

void SetKeyMatrixByScancode2(
	int nAddress,
	int nBit,
	const bool* pabKeyboardState,
	int nScancodeCount,
	SDL_Scancode nScancode1,
	SDL_Scancode nScancode2)
{
	CPC88::Z80Main().SetKeyMatrics(
		nAddress,
		nBit,
		IsScancodePressed(pabKeyboardState, nScancodeCount, nScancode1) ||
		IsScancodePressed(pabKeyboardState, nScancodeCount, nScancode2));
}

void UpdateKeyMatricsFromSDL()
{
	CPC88::Z80Main().ClearKeyMatrics();
	int nScancodeCount = 0;
	const bool* pabKeyboardState = SDL_GetKeyboardState(&nScancodeCount);
	if (pabKeyboardState == NULL) {
		return;
	}
	SDL_Keymod nModState = SDL_GetModState();
	if ((nModState & SDL_KMOD_ALT) != 0) {
		return;
	}

	bool bShift;
	if (
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_F6) ||
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_F7) ||
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_F8) ||
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_F9) ||
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_F10) ||
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_INSERT))
	{
		bShift = true;
	} else if (IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_DELETE)) {
		bShift = false;
	} else {
		bShift = (nModState & SDL_KMOD_SHIFT) != 0;
	}

	bool bUpArrow = IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_UP);
	bool bRightArrow = IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_RIGHT);
	if (CPC88::Z80Main().GetBasicMode() == CPC88Z80Main::BASICMODE_N80V1) {
		if (IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_DOWN)) {
			bShift = true;
			bUpArrow = true;
		}
		if (IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_LEFT)) {
			bShift = true;
			bRightArrow = true;
		}
	}

	SetKeyMatrixByScancode(0x00, 0, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_0);
	SetKeyMatrixByScancode(0x00, 1, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_1);
	SetKeyMatrixByScancode(0x00, 2, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_2);
	SetKeyMatrixByScancode(0x00, 3, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_3);
	SetKeyMatrixByScancode(0x00, 4, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_4);
	SetKeyMatrixByScancode(0x00, 5, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_5);
	SetKeyMatrixByScancode(0x00, 6, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_6);
	SetKeyMatrixByScancode(0x00, 7, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_7);
	SetKeyMatrixByScancode(0x01, 0, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_8);
	SetKeyMatrixByScancode(0x01, 1, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_9);
	SetKeyMatrixByScancode(0x01, 2, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_MULTIPLY);
	SetKeyMatrixByScancode(0x01, 3, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_PLUS);
	SetKeyMatrixByScancode(0x01, 4, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_EQUALS);
	SetKeyMatrixByScancode(0x01, 5, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_COMMA);
	SetKeyMatrixByScancode(0x01, 6, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_PERIOD);
	SetKeyMatrixByScancode2(0x01, 7, pabKeyboardState, nScancodeCount, SDL_SCANCODE_RETURN, SDL_SCANCODE_KP_ENTER);
	SetKeyMatrixByScancode(0x02, 0, pabKeyboardState, nScancodeCount, SDL_SCANCODE_GRAVE);
	SetKeyMatrixByScancode(0x02, 1, pabKeyboardState, nScancodeCount, SDL_SCANCODE_A);
	SetKeyMatrixByScancode(0x02, 2, pabKeyboardState, nScancodeCount, SDL_SCANCODE_B);
	SetKeyMatrixByScancode(0x02, 3, pabKeyboardState, nScancodeCount, SDL_SCANCODE_C);
	SetKeyMatrixByScancode(0x02, 4, pabKeyboardState, nScancodeCount, SDL_SCANCODE_D);
	SetKeyMatrixByScancode(0x02, 5, pabKeyboardState, nScancodeCount, SDL_SCANCODE_E);
	SetKeyMatrixByScancode(0x02, 6, pabKeyboardState, nScancodeCount, SDL_SCANCODE_F);
	SetKeyMatrixByScancode(0x02, 7, pabKeyboardState, nScancodeCount, SDL_SCANCODE_G);
	SetKeyMatrixByScancode(0x03, 0, pabKeyboardState, nScancodeCount, SDL_SCANCODE_H);
	SetKeyMatrixByScancode(0x03, 1, pabKeyboardState, nScancodeCount, SDL_SCANCODE_I);
	SetKeyMatrixByScancode(0x03, 2, pabKeyboardState, nScancodeCount, SDL_SCANCODE_J);
	SetKeyMatrixByScancode(0x03, 3, pabKeyboardState, nScancodeCount, SDL_SCANCODE_K);
	SetKeyMatrixByScancode(0x03, 4, pabKeyboardState, nScancodeCount, SDL_SCANCODE_L);
	SetKeyMatrixByScancode(0x03, 5, pabKeyboardState, nScancodeCount, SDL_SCANCODE_M);
	SetKeyMatrixByScancode(0x03, 6, pabKeyboardState, nScancodeCount, SDL_SCANCODE_N);
	SetKeyMatrixByScancode(0x03, 7, pabKeyboardState, nScancodeCount, SDL_SCANCODE_O);
	SetKeyMatrixByScancode(0x04, 0, pabKeyboardState, nScancodeCount, SDL_SCANCODE_P);
	SetKeyMatrixByScancode(0x04, 1, pabKeyboardState, nScancodeCount, SDL_SCANCODE_Q);
	SetKeyMatrixByScancode(0x04, 2, pabKeyboardState, nScancodeCount, SDL_SCANCODE_R);
	SetKeyMatrixByScancode(0x04, 3, pabKeyboardState, nScancodeCount, SDL_SCANCODE_S);
	SetKeyMatrixByScancode(0x04, 4, pabKeyboardState, nScancodeCount, SDL_SCANCODE_T);
	SetKeyMatrixByScancode(0x04, 5, pabKeyboardState, nScancodeCount, SDL_SCANCODE_U);
	SetKeyMatrixByScancode(0x04, 6, pabKeyboardState, nScancodeCount, SDL_SCANCODE_V);
	SetKeyMatrixByScancode(0x04, 7, pabKeyboardState, nScancodeCount, SDL_SCANCODE_W);
	SetKeyMatrixByScancode(0x05, 0, pabKeyboardState, nScancodeCount, SDL_SCANCODE_X);
	SetKeyMatrixByScancode(0x05, 1, pabKeyboardState, nScancodeCount, SDL_SCANCODE_Y);
	SetKeyMatrixByScancode(0x05, 2, pabKeyboardState, nScancodeCount, SDL_SCANCODE_Z);
	SetKeyMatrixByScancode(0x05, 3, pabKeyboardState, nScancodeCount, SDL_SCANCODE_LEFTBRACKET);
	CPC88::Z80Main().SetKeyMatrics(
		0x05,
		4,
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_BACKSLASH) ||
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_NONUSBACKSLASH) ||
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_INTERNATIONAL3));
	SetKeyMatrixByScancode(0x05, 5, pabKeyboardState, nScancodeCount, SDL_SCANCODE_RIGHTBRACKET);
	SetKeyMatrixByScancode(0x05, 6, pabKeyboardState, nScancodeCount, SDL_SCANCODE_EQUALS);
	SetKeyMatrixByScancode(0x05, 7, pabKeyboardState, nScancodeCount, SDL_SCANCODE_MINUS);
	SetKeyMatrixByScancode(0x06, 0, pabKeyboardState, nScancodeCount, SDL_SCANCODE_0);
	SetKeyMatrixByScancode(0x06, 1, pabKeyboardState, nScancodeCount, SDL_SCANCODE_1);
	SetKeyMatrixByScancode(0x06, 2, pabKeyboardState, nScancodeCount, SDL_SCANCODE_2);
	SetKeyMatrixByScancode(0x06, 3, pabKeyboardState, nScancodeCount, SDL_SCANCODE_3);
	SetKeyMatrixByScancode(0x06, 4, pabKeyboardState, nScancodeCount, SDL_SCANCODE_4);
	SetKeyMatrixByScancode(0x06, 5, pabKeyboardState, nScancodeCount, SDL_SCANCODE_5);
	SetKeyMatrixByScancode(0x06, 6, pabKeyboardState, nScancodeCount, SDL_SCANCODE_6);
	SetKeyMatrixByScancode(0x06, 7, pabKeyboardState, nScancodeCount, SDL_SCANCODE_7);
	SetKeyMatrixByScancode(0x07, 0, pabKeyboardState, nScancodeCount, SDL_SCANCODE_8);
	SetKeyMatrixByScancode(0x07, 1, pabKeyboardState, nScancodeCount, SDL_SCANCODE_9);
	SetKeyMatrixByScancode(0x07, 2, pabKeyboardState, nScancodeCount, SDL_SCANCODE_APOSTROPHE);
	SetKeyMatrixByScancode(0x07, 3, pabKeyboardState, nScancodeCount, SDL_SCANCODE_SEMICOLON);
	SetKeyMatrixByScancode(0x07, 4, pabKeyboardState, nScancodeCount, SDL_SCANCODE_COMMA);
	SetKeyMatrixByScancode(0x07, 5, pabKeyboardState, nScancodeCount, SDL_SCANCODE_PERIOD);
	SetKeyMatrixByScancode(0x07, 6, pabKeyboardState, nScancodeCount, SDL_SCANCODE_SLASH);
	CPC88::Z80Main().SetKeyMatrics(
		0x07,
		7,
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_NONUSBACKSLASH) ||
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_BACKSLASH));
	SetKeyMatrixByScancode(0x08, 0, pabKeyboardState, nScancodeCount, SDL_SCANCODE_HOME);
	CPC88::Z80Main().SetKeyMatrics(0x08, 1, bUpArrow);
	CPC88::Z80Main().SetKeyMatrics(0x08, 2, bRightArrow);
	CPC88::Z80Main().SetKeyMatrics(
		0x08,
		3,
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_BACKSPACE) ||
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_INSERT) ||
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_DELETE));
	SetKeyMatrixByScancode(0x08, 4, pabKeyboardState, nScancodeCount, SDL_SCANCODE_F12);
	CPC88::Z80Main().SetKeyMatrics(0x08, 5, (nModState & SDL_KMOD_NUM) != 0);
	CPC88::Z80Main().SetKeyMatrics(0x08, 6, bShift);
	CPC88::Z80Main().SetKeyMatrics(0x08, 7, (nModState & SDL_KMOD_CTRL) != 0);
	SetKeyMatrixByScancode(0x09, 0, pabKeyboardState, nScancodeCount, SDL_SCANCODE_F11);
	CPC88::Z80Main().SetKeyMatrics(
		0x09,
		1,
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_F1) ||
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_F6));
	CPC88::Z80Main().SetKeyMatrics(
		0x09,
		2,
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_F2) ||
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_F7));
	CPC88::Z80Main().SetKeyMatrics(
		0x09,
		3,
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_F3) ||
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_F8));
	CPC88::Z80Main().SetKeyMatrics(
		0x09,
		4,
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_F4) ||
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_F9));
	CPC88::Z80Main().SetKeyMatrics(
		0x09,
		5,
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_F5) ||
		IsScancodePressed(pabKeyboardState, nScancodeCount, SDL_SCANCODE_F10));
	SetKeyMatrixByScancode(0x09, 6, pabKeyboardState, nScancodeCount, SDL_SCANCODE_SPACE);
	SetKeyMatrixByScancode(0x09, 7, pabKeyboardState, nScancodeCount, SDL_SCANCODE_ESCAPE);
	SetKeyMatrixByScancode(0x0A, 0, pabKeyboardState, nScancodeCount, SDL_SCANCODE_TAB);
	SetKeyMatrixByScancode(0x0A, 1, pabKeyboardState, nScancodeCount, SDL_SCANCODE_DOWN);
	SetKeyMatrixByScancode(0x0A, 2, pabKeyboardState, nScancodeCount, SDL_SCANCODE_LEFT);
	SetKeyMatrixByScancode(0x0A, 3, pabKeyboardState, nScancodeCount, SDL_SCANCODE_END);
	SetKeyMatrixByScancode(0x0A, 4, pabKeyboardState, nScancodeCount, SDL_SCANCODE_PRINTSCREEN);
	SetKeyMatrixByScancode(0x0A, 5, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_MINUS);
	SetKeyMatrixByScancode(0x0A, 6, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_DIVIDE);
	CPC88::Z80Main().SetKeyMatrics(0x0A, 7, (nModState & SDL_KMOD_CAPS) != 0);
	SetKeyMatrixByScancode(0x0B, 0, pabKeyboardState, nScancodeCount, SDL_SCANCODE_PAGEDOWN);
	SetKeyMatrixByScancode(0x0B, 1, pabKeyboardState, nScancodeCount, SDL_SCANCODE_PAGEUP);
	SetKeyMatrixByScancode(0x0C, 0, pabKeyboardState, nScancodeCount, SDL_SCANCODE_F6);
	SetKeyMatrixByScancode(0x0C, 1, pabKeyboardState, nScancodeCount, SDL_SCANCODE_F7);
	SetKeyMatrixByScancode(0x0C, 2, pabKeyboardState, nScancodeCount, SDL_SCANCODE_F8);
	SetKeyMatrixByScancode(0x0C, 3, pabKeyboardState, nScancodeCount, SDL_SCANCODE_F9);
	SetKeyMatrixByScancode(0x0C, 4, pabKeyboardState, nScancodeCount, SDL_SCANCODE_F10);
	SetKeyMatrixByScancode(0x0C, 5, pabKeyboardState, nScancodeCount, SDL_SCANCODE_BACKSPACE);
	SetKeyMatrixByScancode(0x0C, 6, pabKeyboardState, nScancodeCount, SDL_SCANCODE_INSERT);
	SetKeyMatrixByScancode(0x0C, 7, pabKeyboardState, nScancodeCount, SDL_SCANCODE_DELETE);
	SetKeyMatrixByScancode(0x0D, 0, pabKeyboardState, nScancodeCount, SDL_SCANCODE_INTERNATIONAL4);
	SetKeyMatrixByScancode(0x0D, 1, pabKeyboardState, nScancodeCount, SDL_SCANCODE_INTERNATIONAL5);
	SetKeyMatrixByScancode(0x0D, 3, pabKeyboardState, nScancodeCount, SDL_SCANCODE_LANG5);
	SetKeyMatrixByScancode(0x0E, 0, pabKeyboardState, nScancodeCount, SDL_SCANCODE_RETURN);
	SetKeyMatrixByScancode(0x0E, 1, pabKeyboardState, nScancodeCount, SDL_SCANCODE_KP_ENTER);
	SetKeyMatrixByScancode(0x0E, 2, pabKeyboardState, nScancodeCount, SDL_SCANCODE_LSHIFT);
	SetKeyMatrixByScancode(0x0E, 3, pabKeyboardState, nScancodeCount, SDL_SCANCODE_RSHIFT);
}

} // namespace

#endif

int main(int argc, char** argv) {
	bool bCoreReady = false;
	bool bPauseEmulation = false;
	bool bShowStatusWindow = false;
	bool bShowEnvWindow = false;
	bool bShowDiskWindow = false;
	bool bShowTapeWindow = false;
	SDebugWindow dbgWin;
	InitDebugWindowStruct(dbgWin);
	SEnvSettingsView envView;
	std::vector<uint32_t> vArgbBuffer;
	const Uint64 nPerfFreq = SDL_GetPerformanceFrequency();
	const Uint64 nFrameTicks = (nPerfFreq > 0)? (nPerfFreq / 60U): 0U;
	char szMediaPath[1024];
	szMediaPath[0] = '\0';
	int nSelectedDrive = 0;

	CSdl3Settings settings;
	settings.Load();

#ifdef X88000_SDL3_HAS_CORE
	bCoreReady = InitializeCore();
	if (bCoreReady) {
		ApplyEnvSettingsFromIni(settings);
		// Re-apply reset so the env-driven dip switches take effect.
		CPC88::Reset();
		// Mount any media specified on the command line, then reset
		// once more so the BIOS sees the disk during its boot poll.
		bool bHadCliMedia = (argc > 1);
		ParseInitialMediaArgs(argc, argv);
		if (bHadCliMedia) {
			CPC88::Reset();
		}
	}
#endif

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO)) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

#ifdef X88000_SDL3_HAS_CORE
	if (g_audio.Initialize()) {
		// Apply persisted volume / mute settings.
		g_audio.SetBeepVolume(atoi(settings.GetSectionString(SECTION_OPTION, "beepvolume", "50").c_str()));
		g_audio.SetPcgVolume (atoi(settings.GetSectionString(SECTION_OPTION, "pcgvolume",  "50").c_str()));
		g_audio.SetBeepMute  (ParseBoolEntry(settings.GetSectionString(SECTION_OPTION, "beepmute", "off"), false));
		g_audio.SetPcgMute   (ParseBoolEntry(settings.GetSectionString(SECTION_OPTION, "pcgmute",  "off"), false));
	}
#endif

	int nInitialWindowW = settings.GetInt("window.width", 640);
	int nInitialWindowH = settings.GetInt("window.height", 428);
	if (nInitialWindowW < 320) { nInitialWindowW = 320; }
	if (nInitialWindowH < 240) { nInitialWindowH = 240; }
	int nInitialWindowX = settings.GetInt("window.x", SDL_WINDOWPOS_CENTERED);
	int nInitialWindowY = settings.GetInt("window.y", SDL_WINDOWPOS_CENTERED);
	SDL_Window* pWindow = SDL_CreateWindow(
		"X88000 SDL3 Frontend (Prototype)",
		nInitialWindowW,
		nInitialWindowH,
		SDL_WINDOW_RESIZABLE);
	if (pWindow && nInitialWindowX != SDL_WINDOWPOS_CENTERED) {
		SDL_SetWindowPosition(pWindow, nInitialWindowX, nInitialWindowY);
	}
	if (pWindow == NULL) {
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}
	UpdateWindowTitle(pWindow, bCoreReady, bPauseEmulation);

	// Fullscreen state is intentionally NOT restored from settings — the
	// app always starts in windowed mode. The user can toggle fullscreen
	// via Ctrl+Enter or View → Fullscreen.

	SDL_Renderer* pRenderer = SDL_CreateRenderer(pWindow, NULL);
	if (pRenderer == NULL) {
		fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
		SDL_DestroyWindow(pWindow);
		SDL_Quit();
		return 1;
	}

	SDL_Texture* pFrameTexture = SDL_CreateTexture(
		pRenderer,
		SDL_PIXELFORMAT_XRGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		640,
		400);
	if (pFrameTexture == NULL) {
		fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
		SDL_DestroyRenderer(pRenderer);
		SDL_DestroyWindow(pWindow);
		SDL_Quit();
		return 1;
	}

#ifdef X88000_SDL3_HAS_IMGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	ImGui::StyleColorsDark();
	ApplyTintStyle();

	// Load Noto Sans JP from the app bundle's Resources/fonts/ so that
	// Japanese text renders correctly in ImGui menus and dialogs. Falls
	// back to the built-in ASCII font if the file is missing.
	{
		const char* pBase = SDL_GetBasePath();  // .../Contents/MacOS/
		if (pBase) {
			std::string sFontPath = std::string(pBase)
				+ "../Resources/fonts/NotoSansJP-Regular.ttf";
			ImFontConfig fontCfg;
			fontCfg.OversampleH = 2;
			fontCfg.OversampleV = 1;
			ImFont* pFont = io.Fonts->AddFontFromFileTTF(
				sFontPath.c_str(), 20.0f, &fontCfg,
				io.Fonts->GetGlyphRangesJapanese());
			if (!pFont) {
				fprintf(stderr,
					"[warn] failed to load %s — falling back to "
					"built-in font (no Japanese glyphs)\n",
					sFontPath.c_str());
				io.Fonts->AddFontDefault();
			}
		} else {
			io.Fonts->AddFontDefault();
		}
	}

	ImGui_ImplSDL3_InitForSDLRenderer(pWindow, pRenderer);
	ImGui_ImplSDLRenderer3_Init(pRenderer);
	ImGuiContext* pMainImGuiCtx = ImGui::GetCurrentContext();
#endif

	SDL_WindowID nMainWindowID = SDL_GetWindowID(pWindow);
	bool bRunning = true;
	Uint64 nNextFrameTick = SDL_GetPerformanceCounter();
	while (bRunning) {
		SDL_Event evt;
		while (SDL_PollEvent(&evt)) {
#ifdef X88000_SDL3_HAS_IMGUI
			// Route ImGui events to the correct context by windowID.
			SDL_WindowID evtWinID = 0;
			if (evt.type >= SDL_EVENT_WINDOW_FIRST && evt.type <= SDL_EVENT_WINDOW_LAST)
				evtWinID = evt.window.windowID;
			else if (evt.type >= SDL_EVENT_KEY_DOWN && evt.type <= SDL_EVENT_KEY_UP)
				evtWinID = evt.key.windowID;
			else if (evt.type >= SDL_EVENT_MOUSE_MOTION && evt.type <= SDL_EVENT_MOUSE_WHEEL)
				evtWinID = evt.motion.windowID;
			else if (evt.type >= SDL_EVENT_TEXT_EDITING && evt.type <= SDL_EVENT_TEXT_INPUT)
				evtWinID = evt.text.windowID;

			if (dbgWin.bOpen && evtWinID == dbgWin.nWindowID) {
				ImGui::SetCurrentContext(dbgWin.pImGuiCtx);
				ImGui_ImplSDL3_ProcessEvent(&evt);
				ImGui::SetCurrentContext(pMainImGuiCtx);
			} else {
				ImGui_ImplSDL3_ProcessEvent(&evt);
			}

			// Debug window close
			if (evt.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED
				&& dbgWin.bOpen
				&& evt.window.windowID == dbgWin.nWindowID)
			{
				CloseDebugWindow(dbgWin, pMainImGuiCtx, settings);
			}
#endif
				if (evt.type == SDL_EVENT_QUIT) {
					bRunning = false;
				}
				// ESC only quits from main window
				if ((evt.type == SDL_EVENT_KEY_DOWN)
					&& (evt.key.key == SDLK_ESCAPE)
					&& (evt.key.windowID == nMainWindowID))
				{
					bRunning = false;
				}
#ifdef X88000_SDL3_HAS_CORE
				if (evt.type == SDL_EVENT_DROP_FILE) {
					// In SDL3, evt.drop.data is owned by SDL and must NOT
					// be freed by the application. It stays valid until the
					// next SDL_PumpEvents/SDL_PollEvent call.
					if ((evt.drop.data != NULL) && bCoreReady) {
						// Holding Shift while dropping forces the file into
						// drive 2; Cmd/Ctrl forces drive 1. Otherwise the
						// drive is auto-selected (and tape files always go
						// to the tape regardless of modifier).
						SDL_Keymod nMods = SDL_GetModState();
						int nTargetDrive = -1;
						bool bAutoAssign = true;
						std::string strExt = GetLowerFileExt(evt.drop.data);
						bool bIsTape = (strExt == ".t88") || (strExt == ".cmt");
						if (!bIsTape) {
							if ((nMods & SDL_KMOD_SHIFT) != 0) {
								nTargetDrive = 1;
								bAutoAssign = false;
							} else if ((nMods & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0) {
								nTargetDrive = 0;
								bAutoAssign = false;
							}
						}
						AddMediaImage(evt.drop.data, nTargetDrive, bAutoAssign);
						UpdateWindowTitle(pWindow, bCoreReady, bPauseEmulation);
					}
				}
				if ((evt.type == SDL_EVENT_KEY_DOWN) && !evt.key.repeat) {
					bool bCtrl = (evt.key.mod & SDL_KMOD_CTRL) != 0;
					bool bAlt  = (evt.key.mod & SDL_KMOD_ALT) != 0;
					bool bIsMainWin = (evt.key.windowID == nMainWindowID);
					// Ctrl shortcuts: main window only
					if (bIsMainWin) {
						if (bCtrl && (evt.key.key == SDLK_RETURN)) {
							ToggleFullscreen(pWindow);
						} else if (bCtrl && (evt.key.key == SDLK_O)) {
							RequestOpenMediaDialog(pWindow, -1);
						} else if (bCtrl && (evt.key.key == SDLK_R)) {
							ResetCoreState();
							UpdateWindowTitle(pWindow, bCoreReady, bPauseEmulation);
						} else if (bCtrl && (evt.key.key == SDLK_P)) {
							bPauseEmulation = !bPauseEmulation;
							UpdateWindowTitle(pWindow, bCoreReady, bPauseEmulation);
						} else if (bCtrl && (evt.key.key == SDLK_1)) {
							EjectDiskImageFromDrive(0);
							SetMediaStatus("Ejected drive 1");
							UpdateWindowTitle(pWindow, bCoreReady, bPauseEmulation);
						} else if (bCtrl && (evt.key.key == SDLK_2)) {
							EjectDiskImageFromDrive(1);
							SetMediaStatus("Ejected drive 2");
							UpdateWindowTitle(pWindow, bCoreReady, bPauseEmulation);
						}
					}
					// Debug keyboard accelerators: both windows
					if (bAlt && CPC88::IsDebugMode()) {
						if (evt.key.key == SDLK_F5) {
							CPC88::SetDebugStop(
								!CPC88::IsDebugStopped());
						} else if (evt.key.key == SDLK_F10
							&& CPC88::IsDebugStopped())
						{
							CPC88::DebugExecuteStepTrace(
								CPC88::DEBUGSTEP_STEP);
						} else if (evt.key.key == SDLK_F11
							&& CPC88::IsDebugStopped())
						{
							CPC88::DebugExecuteStepTrace(
								CPC88::DEBUGSTEP_TRACE);
						} else if (evt.key.key == SDLK_F12
							&& CPC88::IsDebugStopped())
						{
							CPC88::DebugExecuteStepTrace(
								CPC88::DEBUGSTEP_STEP2);
						}
					}
				}
#endif
			}

#ifdef X88000_SDL3_HAS_CORE
		ProcessQueuedMediaFromDialog(pWindow, bCoreReady, bPauseEmulation);
		if (bCoreReady && !bPauseEmulation) {
			// Only feed keyboard to emulator when main window has focus,
			// so typing in the debug window doesn't trigger emulated keys.
			SDL_WindowFlags nMainFlags = SDL_GetWindowFlags(pWindow);
			if (nMainFlags & SDL_WINDOW_INPUT_FOCUS) {
				UpdateKeyMatricsFromSDL();
			}
			if (!CPC88::IsDebugMode()) {
				CPC88::Execute(4000000/60);
			} else if (!CPC88::IsDebugStopped()) {
				CPC88::DebugExecute(4000000/60);
			}
			UpdateCoreFrame();
			UploadCoreFrameToTexture(pFrameTexture, vArgbBuffer);
		}
#endif

#ifdef X88000_SDL3_HAS_IMGUI
		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
			if (ImGui::BeginMainMenuBar()) {
				// ----- System menu -----
				if (ImGui::BeginMenu("System")) {
#ifdef X88000_SDL3_HAS_CORE
					if (ImGui::MenuItem("Reset", "Ctrl+R", false, bCoreReady)) {
						ResetCoreState();
						UpdateWindowTitle(pWindow, bCoreReady, bPauseEmulation);
					}
					if (ImGui::MenuItem("Pause", "Ctrl+P", bPauseEmulation, bCoreReady)) {
						bPauseEmulation = !bPauseEmulation;
						UpdateWindowTitle(pWindow, bCoreReady, bPauseEmulation);
					}
					ImGui::Separator();

					// BASIC Mode quick switch (matches legacy GTK submenu).
					int nCurBasicChoice = BasicChoiceFromMode(
						CPC88::GetBasicMode(),
						CPC88::IsHighSpeedMode());
					if (ImGui::BeginMenu("BASIC Mode", bCoreReady)) {
						for (int n = 0; n < BASIC_CHOICE_COUNT; n++) {
							if (ImGui::MenuItem(BasicChoiceToLabel(n), NULL, nCurBasicChoice == n)) {
								if (nCurBasicChoice != n) {
									ApplyBasicChoice(n);
									settings.SetSectionString(SECTION_OPTION, "basic", BasicChoiceToString(n));
									CPC88::Reset();
									envView.bLoaded = false;
									UpdateWindowTitle(pWindow, bCoreReady, bPauseEmulation);
								}
							}
						}
						ImGui::EndMenu();
					}

					// Base clock quick switch.
					int nCurClock = CPC88::GetBaseClock();
					if (ImGui::BeginMenu("Clock", bCoreReady)) {
						if (ImGui::MenuItem("4 MHz", NULL, nCurClock == 4)) {
							if (nCurClock != 4) {
								CPC88::SetBaseClock(4);
								settings.SetSectionString(SECTION_OPTION, "clock", "4");
								CPC88::Reset();
								envView.bLoaded = false;
								UpdateWindowTitle(pWindow, bCoreReady, bPauseEmulation);
							}
						}
						if (ImGui::MenuItem("8 MHz", NULL, nCurClock == 8)) {
							if (nCurClock != 8) {
								CPC88::SetBaseClock(8);
								settings.SetSectionString(SECTION_OPTION, "clock", "8");
								CPC88::Reset();
								envView.bLoaded = false;
								UpdateWindowTitle(pWindow, bCoreReady, bPauseEmulation);
							}
						}
						ImGui::EndMenu();
					}
					ImGui::Separator();

					if (ImGui::MenuItem("Environment Settings...", NULL, bShowEnvWindow, bCoreReady)) {
						bShowEnvWindow = !bShowEnvWindow;
						if (bShowEnvWindow) {
							envView.bLoaded = false; // re-sync with current core state
						}
					}
					ImGui::Separator();
#endif
					if (ImGui::MenuItem("Quit X88000M")) {
						bRunning = false;
					}
					ImGui::EndMenu();
				}

				// ----- Disk menu -----
				if (ImGui::BeginMenu("Disk")) {
#ifdef X88000_SDL3_HAS_CORE
					if (ImGui::MenuItem("Open Media...", "Ctrl+O", false, bCoreReady)) {
						RequestOpenMediaDialog(pWindow, -1);
					}
					if (ImGui::MenuItem("Insert to Drive 1...", NULL, false, bCoreReady)) {
						RequestOpenMediaDialog(pWindow, 0);
					}
					if (ImGui::MenuItem("Insert to Drive 2...", NULL, false, bCoreReady)) {
						RequestOpenMediaDialog(pWindow, 1);
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Disk Manager...", NULL, bShowDiskWindow, bCoreReady)) {
						bShowDiskWindow = !bShowDiskWindow;
					}
					if (ImGui::MenuItem("Tape Manager...", NULL, bShowTapeWindow, bCoreReady)) {
						bShowTapeWindow = !bShowTapeWindow;
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Eject Drive 1", "Ctrl+1", false, bCoreReady)) {
						EjectDiskImageFromDrive(0);
						SetMediaStatus("Ejected drive 1");
						UpdateWindowTitle(pWindow, bCoreReady, bPauseEmulation);
					}
					if (ImGui::MenuItem("Eject Drive 2", "Ctrl+2", false, bCoreReady)) {
						EjectDiskImageFromDrive(1);
						SetMediaStatus("Ejected drive 2");
						UpdateWindowTitle(pWindow, bCoreReady, bPauseEmulation);
					}
#endif
					ImGui::EndMenu();
				}

				// ----- Debug menu -----
				if (ImGui::BeginMenu("Debug", bCoreReady)) {
					// Open/close debug window (= enter/exit debug mode)
					if (ImGui::MenuItem("Debug Window...", NULL, dbgWin.bOpen)) {
						if (dbgWin.bOpen) {
							CloseDebugWindow(dbgWin, pMainImGuiCtx, settings);
						} else {
							// Set INI path next to X88000.ini
							if (dbgWin.strIniPath.empty()) {
								const std::string& fpath = settings.GetFilePath();
								std::string::size_type nSlash = fpath.rfind('/');
								std::string strDir = (nSlash != std::string::npos)
									? fpath.substr(0, nSlash + 1) : "./";
								dbgWin.strIniPath = strDir + "imgui.ini";
							}
							OpenDebugWindow(dbgWin, pMainImGuiCtx, settings);
						}
					}
					ImGui::Separator();

					// Audio mute controls
					bool bFmMute  = CPC88::Opna().GetFmMute();
					bool bSsgMute = CPC88::Opna().GetSsgMute();
					if (ImGui::MenuItem("Mute FM (all)", NULL, bFmMute)) {
						CPC88::Opna().SetFmMute(!bFmMute);
					}
					if (ImGui::MenuItem("Mute SSG (all)", NULL, bSsgMute)) {
						CPC88::Opna().SetSsgMute(!bSsgMute);
					}
					ImGui::Separator();
					for (int nCh = 0; nCh < 3; nCh++) {
						char szLabel[32];
						snprintf(szLabel, sizeof(szLabel), "Mute FM ch%d", nCh + 1);
						bool bChMute = CPC88::Opna().GetFmChMute(nCh);
						if (ImGui::MenuItem(szLabel, NULL, bChMute)) {
							CPC88::Opna().SetFmChMute(nCh, !bChMute);
						}
					}
					ImGui::Separator();
					for (int nCh = 0; nCh < 3; nCh++) {
						char szLabel[32];
						snprintf(szLabel, sizeof(szLabel),
							"Mute SSG ch%c", 'A' + nCh);
						bool bChMute = CPC88::Opna().GetSsgChMute(nCh);
						if (ImGui::MenuItem(szLabel, NULL, bChMute)) {
							CPC88::Opna().SetSsgChMute(nCh, !bChMute);
						}
					}
					ImGui::EndMenu();
				}

				// ----- Help menu -----
				static bool bOpenAbout = false;
				if (ImGui::BeginMenu("Help")) {
					if (ImGui::MenuItem("About X88000M...")) {
						bOpenAbout = true;
					}
					ImGui::EndMenu();
				}

				ImGui::EndMainMenuBar();

				// About modal popup — OpenPopup must be called outside the
				// menu scope so the popup ID is reachable.
				if (bOpenAbout) {
					ImGui::OpenPopup("About X88000M");
					bOpenAbout = false;
				}
				ImVec2 vCenter = ImGui::GetMainViewport()->GetCenter();
				ImGui::SetNextWindowPos(vCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
				if (ImGui::BeginPopupModal("About X88000M", NULL,
					ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
				{
					ImGui::TextUnformatted("X88000M");
					ImGui::TextUnformatted("PC-8801 emulator (macOS port of X88000)");
					ImGui::Separator();
					ImGui::TextUnformatted("Original X88000 by Manuke");
					ImGui::TextUnformatted("macOS / SDL3+ImGui port maintained by bubio");
					ImGui::Separator();
					ImGui::TextUnformatted("Frontend: SDL3 + Dear ImGui");
					ImGui::Spacing();
					if (ImGui::Button("OK", ImVec2(120, 0))) {
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
				}
			}
#ifdef X88000_SDL3_HAS_CORE
			if (bCoreReady && bShowEnvWindow) {
				if (DrawEnvSettingsWindow(bShowEnvWindow, envView, settings)) {
					CPC88::Reset();
					UpdateWindowTitle(pWindow, bCoreReady, bPauseEmulation);
				}
			}
			if (bCoreReady && bShowDiskWindow) {
				DrawDiskImageManagerWindow(bShowDiskWindow, pWindow);
			}
			if (bCoreReady && bShowTapeWindow) {
				DrawTapeImageManagerWindow(bShowTapeWindow, pWindow);
			}
			// Debug panels are drawn in the separate debug window below.
#endif
			if (bShowStatusWindow) {
				if (ImGui::Begin("Status", &bShowStatusWindow)) {
					ImGui::TextUnformatted("SDL3 + ImGui frontend");
					ImGui::Text("Core: %s", bCoreReady? "initialized": "ROM not found");
#ifdef X88000_SDL3_HAS_CORE
					ImGui::Text("Execution: %s", bPauseEmulation? "paused": "running");
					ImGui::Text("Drive1: %s", GetDriveDiskImageLabel(0).c_str());
					ImGui::Text("Path1: %s", g_astrDriveMediaPath[0].empty()? "(empty)": g_astrDriveMediaPath[0].c_str());
					ImGui::Text("Drive2: %s", GetDriveDiskImageLabel(1).c_str());
					ImGui::Text("Path2: %s", g_astrDriveMediaPath[1].empty()? "(empty)": g_astrDriveMediaPath[1].c_str());
					ImGui::InputText("Media Path", szMediaPath, sizeof(szMediaPath));
					ImGui::RadioButton("Drive 1", &nSelectedDrive, 0);
					ImGui::SameLine();
					ImGui::RadioButton("Drive 2", &nSelectedDrive, 1);
					if (ImGui::Button("Insert Disk/Tape")) {
						if (AddMediaImage(szMediaPath, nSelectedDrive, true)) {
							szMediaPath[0] = '\0';
						}
						UpdateWindowTitle(pWindow, bCoreReady, bPauseEmulation);
					}
					ImGui::SameLine();
					if (ImGui::Button("Eject Selected Drive")) {
						EjectDiskImageFromDrive(nSelectedDrive);
						SetMediaStatus("Ejected drive " + std::to_string(nSelectedDrive+1));
						UpdateWindowTitle(pWindow, bCoreReady, bPauseEmulation);
					}
					if (!g_strLastMediaStatus.empty()) {
						ImGui::TextWrapped("%s", g_strLastMediaStatus.c_str());
					}
					ImGui::Separator();
					ImGui::TextUnformatted("Shortcuts: Ctrl+R Reset, Ctrl+P Pause, Ctrl+1/2 Eject, Ctrl+Enter Fullscreen");
					ImGui::TextUnformatted("Drag & drop D88/T88/CMT into the window to load.");
					ImGui::TextUnformatted("  (Shift+drop = Drive 2, Cmd/Ctrl+drop = Drive 1, otherwise auto.)");
#endif
				}
				ImGui::End();
			}
			// Save menu bar height before Render() finalizes the frame.
			float fMenuBarH = ImGui::GetFrameHeight()
				+ ImGui::GetStyle().FramePadding.y;
			ImGui::Render();
#endif

		SDL_SetRenderDrawColor(pRenderer, 18, 24, 30, 255);
		SDL_RenderClear(pRenderer);

			if (bCoreReady) {
				int nWindowW = 0;
				int nWindowH = 0;
				SDL_GetRenderOutputSize(pRenderer, &nWindowW, &nWindowH);
#ifdef X88000_SDL3_HAS_IMGUI
				// Offset below the ImGui menu bar
				float fScale = SDL_GetWindowDisplayScale(pWindow);
				float fMenuPx = fMenuBarH * fScale;
				SDL_FRect rctDst = CalcLetterboxRect(
					nWindowW, (int)(nWindowH - fMenuPx), 640, 400);
				rctDst.y += fMenuPx;
#else
				SDL_FRect rctDst = CalcLetterboxRect(nWindowW, nWindowH, 640, 400);
#endif
				SDL_RenderTexture(pRenderer, pFrameTexture, NULL, &rctDst);
			}

#ifdef X88000_SDL3_HAS_IMGUI
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), pRenderer);
#endif

		SDL_RenderPresent(pRenderer);

		// === Debug window render pass ===
#if defined(X88000_SDL3_HAS_IMGUI) && defined(X88000_SDL3_HAS_CORE)
		if (dbgWin.bOpen && bCoreReady) {
			ImGui::SetCurrentContext(dbgWin.pImGuiCtx);
			ImGui_ImplSDLRenderer3_NewFrame();
			ImGui_ImplSDL3_NewFrame();
			ImGui::NewFrame();

			// Debug window menu bar
			if (ImGui::BeginMainMenuBar()) {
				if (ImGui::BeginMenu("Debug")) {
					// CPU target selection
					bool bDbgMain = CPC88::IsDebugMain();
					bool bDbgSub  = CPC88::IsDebugSub();
					if (ImGui::MenuItem("Main CPU Debug", NULL, bDbgMain)) {
						if (!bDbgMain) {
							CPC88::SetDebugExecMode(CPC88::DEBUGEXEC_MAIN);
						}
					}
					bool bSubDisabled = CPC88::IsSubSystemDisableNow();
					ImGui::BeginDisabled(bSubDisabled);
					if (ImGui::MenuItem("Sub CPU Debug", NULL, bDbgSub)) {
						if (!bDbgSub) {
							CPC88::SetDebugExecMode(CPC88::DEBUGEXEC_SUB);
						}
					}
					ImGui::EndDisabled();
					ImGui::Separator();

					bool bDebugMode = CPC88::IsDebugMode();
					bool bStopped = CPC88::IsDebugStopped() && bDebugMode;

					// Execute Debug toggle
					ImGui::BeginDisabled(!bDebugMode);
					bool bDbgRunning = bDebugMode && !CPC88::IsDebugStopped();
					if (ImGui::MenuItem("Execute Debug", "Alt+F5", bDbgRunning)) {
						CPC88::SetDebugStop(!CPC88::IsDebugStopped());
					}
					ImGui::EndDisabled();
					ImGui::Separator();

					// Step controls
					ImGui::BeginDisabled(!bStopped);
					if (ImGui::MenuItem("Execute Step", "Alt+F10")) {
						CPC88::DebugExecuteStepTrace(CPC88::DEBUGSTEP_STEP);
					}
					if (ImGui::MenuItem("Execute Trace", "Alt+F11")) {
						CPC88::DebugExecuteStepTrace(CPC88::DEBUGSTEP_TRACE);
					}
					if (ImGui::MenuItem("Execute Step2", "Alt+F12")) {
						CPC88::DebugExecuteStepTrace(CPC88::DEBUGSTEP_STEP2);
					}
					ImGui::EndDisabled();
					ImGui::Separator();

					// Record Execution Log
					ImGui::BeginDisabled(!bDebugMode);
					bool bLogging = IsDebugLogging();
					if (ImGui::MenuItem("Record Execution Log", NULL, bLogging)) {
						if (bLogging) {
							EndDebugLog();
						} else {
							StartDebugLog();
						}
					}
					ImGui::EndDisabled();
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Panels")) {
					ImGui::MenuItem("Disassemble", NULL, &dbgWin.bShowDisasm);
					ImGui::MenuItem("Memory Dump", NULL, &dbgWin.bShowMemDump);
					ImGui::MenuItem("Breakpoints", NULL, &dbgWin.bShowBreakpoint);
					ImGui::MenuItem("Write RAM", NULL, &dbgWin.bShowWriteRam);
					ImGui::MenuItem("Export RAM", NULL, &dbgWin.bShowExportRam);
					ImGui::EndMenu();
				}
				ImGui::EndMainMenuBar();
			}

			// Create a DockSpace covering the entire window (below menu bar)
			ImGuiID dockID = ImGui::DockSpaceOverViewport(
				0, ImGui::GetMainViewport(),
				ImGuiDockNodeFlags_PassthruCentralNode);

			// Build initial layout on first frame (matches GTK-style arrangement).
			// Once imgui.ini exists, this block is skipped and the saved
			// layout is used instead.
			if (dbgWin.bNeedInitLayout) {
				dbgWin.bNeedInitLayout = false;
				ImGui::DockBuilderRemoveNode(dockID);
				ImGui::DockBuilderAddNode(dockID,
					ImGuiDockNodeFlags_DockSpace);
				ImGui::DockBuilderSetNodeSize(dockID,
					ImGui::GetMainViewport()->Size);

				// Split: top 60% / bottom 40%
				ImGuiID idTop, idBottom;
				ImGui::DockBuilderSplitNode(dockID,
					ImGuiDir_Up, 0.60f, &idTop, &idBottom);

				// Top: left 25% (WriteRAM+ExportRAM tabs) | center 40% (Disasm) | right 35% (MemDump)
				ImGuiID idTopLeft, idTopRest;
				ImGui::DockBuilderSplitNode(idTop,
					ImGuiDir_Left, 0.25f, &idTopLeft, &idTopRest);
				ImGuiID idTopCenter, idTopRight;
				ImGui::DockBuilderSplitNode(idTopRest,
					ImGuiDir_Left, 0.53f, &idTopCenter, &idTopRight);

				// Bottom: left 65% (Debugger) | right 35% (Breakpoints)
				ImGuiID idBotLeft, idBotRight;
				ImGui::DockBuilderSplitNode(idBottom,
					ImGuiDir_Left, 0.65f, &idBotLeft, &idBotRight);

				ImGui::DockBuilderDockWindow("Write RAM", idTopLeft);
				ImGui::DockBuilderDockWindow("Export RAM", idTopLeft);
				ImGui::DockBuilderDockWindow("Disassemble", idTopCenter);
				ImGui::DockBuilderDockWindow("Memory Dump", idTopRight);
				ImGui::DockBuilderDockWindow("Debugger", idBotLeft);
				ImGui::DockBuilderDockWindow("Breakpoints", idBotRight);

				ImGui::DockBuilderFinish(dockID);
			}

			// Draw debug panels (user can dock/tab/split these freely)
			bool bShowMain = true;
			DrawDebugMainWindow(bShowMain);
			if (dbgWin.bShowDisasm) {
				DrawDisassembleWindow(dbgWin.bShowDisasm);
			}
			if (dbgWin.bShowMemDump) {
				DrawMemoryDumpWindow(dbgWin.bShowMemDump);
			}
			if (dbgWin.bShowBreakpoint) {
				DrawBreakpointWindow(dbgWin.bShowBreakpoint);
			}
			if (dbgWin.bShowWriteRam) {
				DrawWriteRamWindow(dbgWin.bShowWriteRam);
			}
			if (dbgWin.bShowExportRam) {
				DrawExportRamWindow(dbgWin.bShowExportRam);
			}

			ImGui::Render();
			SDL_SetRenderDrawColor(dbgWin.pRenderer, 30, 30, 30, 255);
			SDL_RenderClear(dbgWin.pRenderer);
			ImGui_ImplSDLRenderer3_RenderDrawData(
				ImGui::GetDrawData(), dbgWin.pRenderer);
			SDL_RenderPresent(dbgWin.pRenderer);

			ImGui::SetCurrentContext(pMainImGuiCtx);
		}
#endif

		if (nFrameTicks > 0) {
			nNextFrameTick += nFrameTicks;
			Uint64 nNow = SDL_GetPerformanceCounter();
			if (nNow < nNextFrameTick) {
				Uint64 nWaitPerf = nNextFrameTick-nNow;
				Uint64 nWaitMs = (nWaitPerf*1000U)/nPerfFreq;
				if (nWaitMs > 0) {
					SDL_Delay((Uint32)nWaitMs);
				}
			} else if ((nNow-nNextFrameTick) > nFrameTicks*4U) {
				// Avoid unbounded drift when resumed after a long stall.
				nNextFrameTick = nNow;
			}
		}
	}

#ifdef X88000_SDL3_HAS_IMGUI
	// Close debug window before destroying main ImGui context.
	if (dbgWin.bOpen) {
		CloseDebugWindow(dbgWin, pMainImGuiCtx, settings);
	}
	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
#endif

#ifdef X88000_SDL3_HAS_CORE
	// Clean up debug state before saving settings.
	if (CPC88::IsDebugMode()) {
		CPC88::SetDebugExecMode(CPC88::DEBUGEXEC_NONE);
	}
	if (IsDebugLogging()) {
		EndDebugLog();
	}
#endif

	{
		// Persist window size only when we are not currently in fullscreen,
		// so that "fullscreen at exit" doesn't overwrite the windowed size.
		// Fullscreen state itself is intentionally never persisted.
		SDL_WindowFlags nWindowFlags = SDL_GetWindowFlags(pWindow);
		bool bExitFullscreen = (nWindowFlags & SDL_WINDOW_FULLSCREEN) != 0;
		if (!bExitFullscreen) {
			int nExitW = 0, nExitH = 0;
			int nExitX = 0, nExitY = 0;
			SDL_GetWindowSize(pWindow, &nExitW, &nExitH);
			SDL_GetWindowPosition(pWindow, &nExitX, &nExitY);
			if ((nExitW > 0) && (nExitH > 0)) {
				settings.SetInt("window.width", nExitW);
				settings.SetInt("window.height", nExitH);
				settings.SetInt("window.x", nExitX);
				settings.SetInt("window.y", nExitY);
			}
		}
		settings.Save();
	}

	SDL_DestroyTexture(pFrameTexture);
	SDL_DestroyRenderer(pRenderer);
	SDL_DestroyWindow(pWindow);
#ifdef X88000_SDL3_HAS_CORE
	g_audio.Shutdown();
	ShutdownCore();
#endif
	SDL_Quit();
	return 0;
}
