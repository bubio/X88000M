#include <SDL3/SDL.h>

#ifdef X88000_SDL3_HAS_CORE

#include "StdHeader.h"
#include "PC88.h"
#include "X88Utility.h"
#include "X88ScreenDrawer.h"
#include "X88DiskImageMemory.h"
#include "ParallelNull.h"
#include "ParallelPR201.h"
#include "X88PrinterDrawer.h"

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
#include <queue>
#include <time.h>
#include <sys/stat.h>

#ifdef X88000_SDL3_HAS_CORE

namespace {

// Resolve the path to NotoSansJP-Regular.ttf.
// macOS: <BasePath>/../Resources/fonts/
// Windows/Linux: <BasePath>/fonts/
std::string ResolveFontPath()
{
	const char* pBase = SDL_GetBasePath();
	if (!pBase) return std::string();

	std::string sBase(pBase);
#ifdef __APPLE__
	std::string sPath = sBase + "../Resources/fonts/NotoSansJP-Regular.ttf";
#else
	std::string sPath = sBase + "fonts/NotoSansJP-Regular.ttf";
#endif
	FILE* fpt = fopen(sPath.c_str(), "rb");
	if (fpt) { fclose(fpt); return sPath; }
	return std::string();
}

std::vector<std::string> g_vRomSearchDir;
CPC88 g_pc88;
CX88ScreenDrawer g_screenDrawer;
CParallelNull g_parallelNull;
CParallelPR201 g_parallelPR201;
int g_nParallelDevice = 0; // 0=Null, 1=PR201
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

const char kMainWindowTitle[] = "X88000M";

void UpdateWindowTitle(SDL_Window* pWindow, bool bCoreReady, bool bPauseEmulation);
void DisableRendererVSync(SDL_Renderer* pRenderer, const char* pszRendererName);
int GetFrameExecuteClock();
// Forward declarations for helpers used by ImGui draw functions further up the file.
void SetMediaStatus(const std::string& strStatus);
bool MountDiskImageByIndex(int nDrive, int nDiskImageIndex);
void EjectDiskImageFromDrive(int nDrive);
std::string GetDriveDiskImageLabel(int nDrive);
void EraseAllDiskImages();
int  FindDriveHoldingDiskIndex(int nDiskImageIndex);
void RequestOpenDiskOnlyDialog(SDL_Window* pWindow, int nDrive);
void RequestOpenTapeOnlyDialog(SDL_Window* pWindow);

void DisableRendererVSync(SDL_Renderer* pRenderer, const char* pszRendererName)
{
	if ((pRenderer != NULL) && !SDL_SetRenderVSync(pRenderer, 0)) {
		fprintf(stderr,
			"[warn] SDL_SetRenderVSync(%s) failed: %s\n",
			(pszRendererName != NULL)? pszRendererName: "renderer",
			SDL_GetError());
	}
}

int GetFrameExecuteClock()
{
	const int nBaseClockMHz = CPC88::GetBaseClock();
	return ((nBaseClockMHz > 0)? nBaseClockMHz: 4) * 1000000 / 60;
}

////////////////////////////////////////////////////////////
// Environment settings (BASIC mode, base clock, dip-switches)
//
// These settings are persisted under the legacy [option] section of
// X88000.ini so the SDL3 frontend stays compatible with the existing
// core option keys.

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
	// Hi-resolution
	std::string strHireso = settings.GetSectionString(SECTION_OPTION, "hireso", "");
	if (!strHireso.empty()) {
		CPC88::SetHiresolution(ParseBoolEntry(strHireso, false));
	}
	// Option font (overseas mode)
	std::string strOptFont = settings.GetSectionString(SECTION_OPTION, "optionfont", "");
	if (!strOptFont.empty()) {
		CPC88::SetOptionFont(ParseBoolEntry(strOptFont, false));
	}
	// Interlace
	std::string strInterlace = settings.GetSectionString(SECTION_OPTION, "interlace", "");
	if (!strInterlace.empty()) {
		bool bInterlace = ParseBoolEntry(strInterlace, false);
		CX88ScreenDrawer::SetInterlace(bInterlace);
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
	bool bHiResolution;
	bool bOptionFont;
	int  nBoostLimiter;
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
		bHiResolution(false),
		bOptionFont(false),
		nBoostLimiter(0),
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
	view.bHiResolution = CPC88::IsHiresolution();
	view.bOptionFont   = CPC88::IsOptionFont();
	view.nBoostLimiter = atoi(settings.GetSectionString(SECTION_OPTION, "boostlim", "0").c_str());
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

			if (ImGui::Checkbox("Hi-resolution (24 KHz)", &view.bHiResolution)) {
				CPC88::SetHiresolution(view.bHiResolution);
				settings.SetSectionString(SECTION_OPTION, "hireso", BoolToOnOff(view.bHiResolution));
				bRequestReset = true;
			}

			int nBoostLim = view.nBoostLimiter;
			const char* aszBoostLimLabels[] = {
				"Off", "25%", "50%", "75%", "150%", "200%", "300%", "400%"
			};
			const int anBoostLimValues[] = {
				0, 25, 50, 75, 150, 200, 300, 400
			};
			int nBoostLimIdx = 0;
			for (int i = 0; i < 8; i++) {
				if (anBoostLimValues[i] == nBoostLim) { nBoostLimIdx = i; break; }
			}
			if (ImGui::Combo("Boost limiter", &nBoostLimIdx, aszBoostLimLabels, 8)) {
				view.nBoostLimiter = anBoostLimValues[nBoostLimIdx];
				char szBuf[16];
				snprintf(szBuf, sizeof(szBuf), "%d", view.nBoostLimiter);
				settings.SetSectionString(SECTION_OPTION, "boostlim", szBuf);
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
			if (ImGui::Checkbox("Option font (overseas mode)", &view.bOptionFont)) {
				CPC88::SetOptionFont(view.bOptionFont);
				settings.SetSectionString(SECTION_OPTION, "optionfont", BoolToOnOff(view.bOptionFont));
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
				CX88ScreenDrawer::SetInterlace(view.bInterlace);
				CPC88::Z80Main().SetGVRamUpdate(true);
				settings.SetSectionString(SECTION_OPTION, "interlace", BoolToOnOff(view.bInterlace));
			}
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
					std::string strName = NX88Utility::ConvSJIStoUTF8(pDisk->GetImageName());
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
			// Mnemonic at current PC (legacy format: 0XXXXH  MNEMONIC)
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

				// Register display: legacy 3-line horizontal layout
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

// IME character paste support — ported from CX88000::m_awIMECharTable
// Each entry encodes a key matrix position:
//   bits 11-8: shift flag (0x01=shift, 0x10=kana)
//   bits 7-4:  row in key matrix
//   bits 2-0:  column in key matrix
uint16_t g_awIMECharTable[] = {
	// 0x20-0x2F: Marks (space ! " # $ % & ' ( ) * + , - . /)
	0x0096, 0x0161, 0x0162, 0x0163, 0x0164, 0x0165, 0x0166, 0x0167,
	0x0170, 0x0171, 0x0172, 0x0173, 0x0074, 0x0057, 0x0075, 0x0076,
	// 0x30-0x3F: Numeric (0-9 : ; < = > ?)
	0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
	0x0070, 0x0071, 0x0072, 0x0073, 0x0174, 0x0157, 0x0175, 0x0176,
	// 0x40-0x5F: Upper Alpha (@ A-Z [ \ ] ^ _)
	0x0020, 0x0121, 0x0122, 0x0123, 0x0124, 0x0125, 0x0126, 0x0127,
	0x0130, 0x0131, 0x0132, 0x0133, 0x0134, 0x0135, 0x0136, 0x0137,
	0x0140, 0x0141, 0x0142, 0x0143, 0x0144, 0x0145, 0x0146, 0x0147,
	0x0150, 0x0151, 0x0152, 0x0053, 0x0054, 0x0055, 0x0056, 0x0177,
	// 0x60-0x7F: Lower Alpha (` a-z { | } ~ DEL)
	0x0120, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
	0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
	0x0050, 0x0051, 0x0052, 0x0153, 0x0154, 0x0155, 0x0156, 0x0000,
	// 0x80-0x9F: (unused, padding to keep kana at correct offset)
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	// 0xA0-0xDF: Kana (half-width katakana, indexed as btIME-0x40)
	// Index 0x60=space, 0x61-0x9F=kana chars, 0x9E=dakuten, 0x9F=handakuten
	0x0000, 0x1175, 0x1153, 0x1155, 0x1174, 0x1176, 0x1160, 0x1163,
	0x1125, 0x1164, 0x1165, 0x1166, 0x1167, 0x1170, 0x1171, 0x1152,
	0x1054, 0x1063, 0x1025, 0x1064, 0x1065, 0x1066, 0x1044, 0x1027,
	0x1030, 0x1072, 0x1022, 0x1050, 0x1024, 0x1042, 0x1040, 0x1023,
	0x1041, 0x1021, 0x1052, 0x1047, 0x1043, 0x1045, 0x1031, 0x1061,
	0x1074, 0x1033, 0x1026, 0x1046, 0x1062, 0x1056, 0x1057, 0x1032,
	0x1036, 0x1055, 0x1076, 0x1035, 0x1067, 0x1070, 0x1071, 0x1037,
	0x1034, 0x1075, 0x1073, 0x1077, 0x1060, 0x1051, 0x1020, 0x1053,
	// 0xA0: Return (table index 0xA0 = 160)
	0x0017,
	// 0xA1: Tab (table index 0xA1 = 161)
	0x00A0
};

std::queue<uint16_t> g_queueIMEChar;
int g_nIMECharPhase = 0;
// Phase 0: set shift/kana modifiers, key OFF (1 frame)
// Phase 1: key ON + modifiers (2 frames)
// Phase 2: all keys OFF (1 frame), advance to next char

enum {
	IME_KEY_RETURN = 0x0017,
	IME_KEY_TAB    = 0x00A0
};

void AddPasteText(const char* pszText)
{
	if (!pszText) return;
	for (; *pszText != '\0'; pszText++) {
		uint8_t ch = (uint8_t)*pszText;
		if (ch >= 0x20 && ch <= 0x7E) {
			g_queueIMEChar.push(g_awIMECharTable[ch - 0x20]);
		} else if (ch == 0x0A) {
			// LF → Return key
			g_queueIMEChar.push(IME_KEY_RETURN);
		} else if (ch == 0x0D) {
			// CR → Return key (skip if followed by LF to avoid double return)
			if (*(pszText + 1) != 0x0A) {
				g_queueIMEChar.push(IME_KEY_RETURN);
			}
		} else if (ch == 0x09) {
			// Tab
			g_queueIMEChar.push(IME_KEY_TAB);
		}
	}
}

// Screenshot save dialog result — may be called from a non-main thread.
std::string g_strPendingScreenshotPath;
std::mutex  g_mtxScreenshot;

void SDLCALL OnScreenshotPathSelected(void* userdata, const char* const* filelist, int filter)
{
	(void)userdata;
	(void)filter;
	if (filelist && filelist[0]) {
		std::lock_guard<std::mutex> lock(g_mtxScreenshot);
		g_strPendingScreenshotPath = filelist[0];
	}
}

bool DoSaveScreenshot(const std::string& fstrPath)
{
	if (!g_bScreenDrawerReady) return false;
	// Build ARGB pixel buffer (reuse the same logic as UploadCoreFrameToTexture)
	std::vector<uint32_t> vPixels(640 * 400);
	if (CPC88::Z80Main().IsVABScreenActive()) {
		uint8_t* pRgb = CX88ScreenDrawer::GetScreenDataBits2();
		if (!pRgb) return false;
		for (int n = 0; n < 640*400; n++) {
			vPixels[n] = 0xFF000000U
				| (uint32_t(pRgb[n*3+0]) << 16)
				| (uint32_t(pRgb[n*3+1]) << 8)
				| uint32_t(pRgb[n*3+2]);
		}
	} else {
		uint8_t* pIdx = CX88ScreenDrawer::GetScreenDataBits();
		if (!pIdx) return false;
		SX88Color* pCT = CX88ScreenDrawer::GetColorTable();
		for (int n = 0; n < 640*400; n++) {
			uint8_t p = pIdx[n];
			vPixels[n] = 0xFF000000U
				| (uint32_t(pCT[p].red >> 8) << 16)
				| (uint32_t(pCT[p].green >> 8) << 8)
				| uint32_t(pCT[p].blue >> 8);
		}
	}
	SDL_Surface* pSurf = SDL_CreateSurfaceFrom(
		640, 400, SDL_PIXELFORMAT_ARGB8888, &vPixels[0], 640 * 4);
	if (!pSurf) return false;
	bool bOK = SDL_SaveBMP(pSurf, fstrPath.c_str());
	SDL_DestroySurface(pSurf);
	return bOK;
}

bool DoCopyScreenText()
{
	// Keep the legacy clipboard text extraction behavior from X88000.cpp.
	int nWidth = CPC88::Crtc().IsWidth80() ? 80 : 40;
	int nHeight = CPC88::Crtc().IsHeight25() ? 25 : 20;
	std::string strText;
	int nOfs = 0;
	for (int y = 0; y < nHeight; y++) {
		int nSpace = 0;
		for (int x = 0; x < nWidth; x++) {
			uint8_t btChar = CX88ScreenDrawer::GetTextChar(nOfs);
			uint8_t btAttr = CX88ScreenDrawer::GetTextAttr(nOfs);
			bool bSkip = ((btAttr & CX88ScreenDrawer::TATTR_GRAPHIC) != 0)
				|| (btChar <= 0x20)
				|| ((btChar > 0x7E) && (btChar < 0xA1))
				|| (btChar > 0xDF);
			if (bSkip) {
				nSpace++;
			} else {
				for (; nSpace > 0; nSpace--) {
					strText += ' ';
				}
				// ASCII printable range only (skip half-width kana for now)
				if (btChar >= 0x21 && btChar <= 0x7E) {
					strText += (char)btChar;
				}
			}
			nOfs += (nWidth <= 40) ? 2 : 1;
		}
		strText += '\n';
	}
	// Remove trailing blank lines
	while (strText.size() >= 2
		&& strText[strText.size()-1] == '\n'
		&& strText[strText.size()-2] == '\n')
	{
		strText.erase(strText.size()-1);
	}
	return SDL_SetClipboardText(strText.c_str());
}

// Export folder dialog result — may be called from a non-main thread.
struct SRamExportRequest {
	bool bActive;
	bool bMainRam0;
	bool bMainRam1;
	bool bFastTVRam;
	bool bSlowTVRam;
	bool bGVRam0;
	bool bGVRam1;
	bool bGVRam2;
	bool bSubRam;
	bool bExRam0;
	bool bExRam1;
	bool bFastTVRamUse;
	bool bSubDisabled;
};

std::string g_strPendingExportDir;
std::mutex  g_mtxExportDir;
bool g_bExportFolderDialogResolved = false;
bool g_bExportFolderDialogAccepted = false;
std::string g_strLastExportDir;
std::string g_strRamExportStatus;
SRamExportRequest g_ramExportRequest = {};

std::string GetDefaultFolderDialogDir()
{
#ifdef X88_PLATFORM_WINDOWS
	const char* pProfile = getenv("USERPROFILE");
	if (pProfile && *pProfile) {
		return std::string(pProfile) + "\\Documents\\";
	}
#else
	const char* pHome = getenv("HOME");
	if (pHome && *pHome) {
		return std::string(pHome) + "/Documents/";
	}
#endif
	return "./";
}

void SDLCALL OnExportFolderSelected(void* userdata, const char* const* filelist, int filter)
{
	(void)userdata;
	(void)filter;
	std::lock_guard<std::mutex> lock(g_mtxExportDir);
	g_bExportFolderDialogResolved = true;
	g_bExportFolderDialogAccepted = false;
	g_strPendingExportDir.clear();
	if (filelist && filelist[0]) {
		std::string strDir = filelist[0];
		char cLast = strDir[strDir.size() - 1];
		if (!strDir.empty() && cLast != '/'
#ifdef X88_PLATFORM_WINDOWS
			&& cLast != '\\'
#endif
		) {
			strDir += '/';
		}
		g_strPendingExportDir = strDir;
		g_bExportFolderDialogAccepted = true;
	}
}

std::string ExportSelectedRam(const std::string& strExportDir,
	const SRamExportRequest& req)
{
	char szTime[64];
	{
		time_t t = time(NULL);
		struct tm* pTm = localtime(&t);
		strftime(szTime, sizeof(szTime),
			"X88000M_RAM_%Y%m%d_%H%M%S", pTm);
	}
	std::string fstrDir = strExportDir + szTime + "/";
#ifdef X88_PLATFORM_WINDOWS
	_mkdir(fstrDir.c_str());
#else
	mkdir(fstrDir.c_str(), 0755);
#endif

	int nExported = 0;
	auto WriteFile = [&](const char* pszName, const void* pData, size_t nSize) -> bool {
		std::string fstrPath = fstrDir + pszName;
		FILE* fpt = NX88Utility::Fopen_UTF8(fstrPath.c_str(), "wb");
		if (fpt == NULL) {
			return false;
		}
		bool bOK = (fwrite(pData, nSize, 1, fpt) == 1);
		fclose(fpt);
		return bOK;
	};

	if (req.bMainRam0) {
		if (WriteFile("main0.ram", CPC88::Z80Main().GetMainRamPtr(), 0x8000)) {
			nExported++;
		}
	}
	if (req.bMainRam1) {
		std::string fstrPath = fstrDir + "main1.ram";
		FILE* fpt = NX88Utility::Fopen_UTF8(fstrPath.c_str(), "wb");
		if (fpt != NULL) {
			bool bOK = false;
			if (req.bFastTVRamUse) {
				bOK = (fwrite(CPC88::Z80Main().GetMainRamPtr() + 0x8000,
						0x7000, 1, fpt) == 1) &&
					(fwrite(CPC88::Z80Main().GetFastTVRamPtr(),
						0x1000, 1, fpt) == 1);
			} else {
				bOK = (fwrite(CPC88::Z80Main().GetMainRamPtr() + 0x8000,
						0x8000, 1, fpt) == 1);
			}
			fclose(fpt);
			if (bOK) {
				nExported++;
			}
		}
	}
	if (req.bFastTVRam && !req.bFastTVRamUse) {
		if (WriteFile("fast_tv.ram", CPC88::Z80Main().GetFastTVRamPtr(), 0x1000)) {
			nExported++;
		}
	}
	if (req.bSlowTVRam && req.bFastTVRamUse) {
		if (WriteFile("slow_tv.ram",
			CPC88::Z80Main().GetMainRamPtr() + 0xF000, 0x1000))
		{
			nExported++;
		}
	}
	if (req.bGVRam0) {
		if (WriteFile("gv0.ram", CPC88::Z80Main().GetGVRamPtr(0), 0x4000)) {
			nExported++;
		}
	}
	if (req.bGVRam1) {
		if (WriteFile("gv1.ram", CPC88::Z80Main().GetGVRamPtr(1), 0x4000)) {
			nExported++;
		}
	}
	if (req.bGVRam2) {
		if (WriteFile("gv2.ram", CPC88::Z80Main().GetGVRamPtr(2), 0x4000)) {
			nExported++;
		}
	}
	if (req.bSubRam && !req.bSubDisabled) {
		if (WriteFile("sub.ram", CPC88::Z80Sub().GetSubRamPtr(), 0x4000)) {
			nExported++;
		}
	}
	if (req.bExRam0) {
		if (WriteFile("ex0.ram", CPC88::Z80Main().GetExRamPtr(0), 0x8000 * 4)) {
			nExported++;
		}
	}
	if (req.bExRam1) {
		if (WriteFile("ex1.ram", CPC88::Z80Main().GetExRamPtr(1), 0x8000 * 4)) {
			nExported++;
		}
	}

	char szMsg[256];
	snprintf(szMsg, sizeof(szMsg),
		"Exported %d file(s) to %s", nExported, fstrDir.c_str());
	return szMsg;
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

		if (g_strLastExportDir.empty()) {
			g_strLastExportDir = GetDefaultFolderDialogDir();
		}
		ImGui::BeginDisabled(g_ramExportRequest.bActive);
		if (ImGui::Button("Export")) {
			g_ramExportRequest.bActive = true;
			g_ramExportRequest.bMainRam0 = bMainRam0;
			g_ramExportRequest.bMainRam1 = bMainRam1;
			g_ramExportRequest.bFastTVRam = bFastTVRam;
			g_ramExportRequest.bSlowTVRam = bSlowTVRam;
			g_ramExportRequest.bGVRam0 = bGVRam0;
			g_ramExportRequest.bGVRam1 = bGVRam1;
			g_ramExportRequest.bGVRam2 = bGVRam2;
			g_ramExportRequest.bSubRam = bSubRam;
			g_ramExportRequest.bExRam0 = bExRam0;
			g_ramExportRequest.bExRam1 = bExRam1;
			g_ramExportRequest.bFastTVRamUse = bFastTVRamUse;
			g_ramExportRequest.bSubDisabled = bSubDisabled;
			SDL_ShowOpenFolderDialog(
				OnExportFolderSelected, NULL,
				NULL, g_strLastExportDir.c_str(), false);
		}
		ImGui::EndDisabled();

		ImGui::EndDisabled();

		if (g_ramExportRequest.bActive) {
			ImGui::TextDisabled("Waiting for destination folder...");
		}
		if (!g_strRamExportStatus.empty()) {
			ImGui::TextWrapped("%s", g_strRamExportStatus.c_str());
		}
		if (!bDebugMode || !bStopped) {
			ImGui::TextDisabled("(stop debugger first)");
		}
	}
	ImGui::End();
}

// Forward declaration
#ifdef X88000_SDL3_HAS_IMGUI
void ApplyTintStyle();
#endif

// ---- Printer Preview ----

struct SPrinterPreview {
	SDL_Window*    pWindow;
	SDL_Renderer*  pRenderer;
#ifdef X88000_SDL3_HAS_IMGUI
	ImGuiContext*  pImGuiCtx;
#endif
	SDL_WindowID   nWindowID;
	bool           bOpen;
	SDL_Texture*   pTexture;
	int nTexW;
	int nTexH;
	int nPage;
	int nZoom; // 0-4: BASE_DPI << nZoom
	bool bDirty;
};

void InitPrinterPreview(SPrinterPreview& pp)
{
	pp.pWindow = NULL;
	pp.pRenderer = NULL;
#ifdef X88000_SDL3_HAS_IMGUI
	pp.pImGuiCtx = NULL;
#endif
	pp.nWindowID = 0;
	pp.bOpen = false;
	pp.pTexture = NULL;
	pp.nTexW = 0;
	pp.nTexH = 0;
	pp.nPage = 0;
	pp.nZoom = 2; // default 72 DPI
	pp.bDirty = true;
}

bool OpenPrinterWindow(SPrinterPreview& pp, ImGuiContext* pMainCtx)
{
	if (pp.bOpen) return true;

	pp.pWindow = SDL_CreateWindow("X88000M Printer Preview",
		600, 700, SDL_WINDOW_RESIZABLE);
	if (!pp.pWindow) return false;

	pp.pRenderer = SDL_CreateRenderer(pp.pWindow, NULL);
	if (!pp.pRenderer) {
		SDL_DestroyWindow(pp.pWindow);
		pp.pWindow = NULL;
		return false;
	}
	DisableRendererVSync(pp.pRenderer, "printer preview");
	pp.nWindowID = SDL_GetWindowID(pp.pWindow);

#ifdef X88000_SDL3_HAS_IMGUI
	pp.pImGuiCtx = ImGui::CreateContext();
	ImGui::SetCurrentContext(pp.pImGuiCtx);
	ImGuiIO& prtIO = ImGui::GetIO();
	prtIO.IniFilename = NULL;
	prtIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// Load font
	{
		std::string sFontPath = ResolveFontPath();
		if (!sFontPath.empty()) {
			ImFontConfig fontCfg;
			fontCfg.OversampleH = 2;
			fontCfg.OversampleV = 1;
			prtIO.Fonts->AddFontFromFileTTF(
				sFontPath.c_str(), 20.0f, &fontCfg,
				prtIO.Fonts->GetGlyphRangesJapanese());
		} else {
			prtIO.Fonts->AddFontDefault();
		}
	}

	ImGui::StyleColorsDark();
	ApplyTintStyle();
	ImGui_ImplSDL3_InitForSDLRenderer(pp.pWindow, pp.pRenderer);
	ImGui_ImplSDLRenderer3_Init(pp.pRenderer);

	ImGui::SetCurrentContext(pMainCtx);
#endif

	pp.bOpen = true;
	pp.bDirty = true;
	return true;
}

void ClosePrinterWindow(SPrinterPreview& pp, ImGuiContext* pMainCtx)
{
	if (!pp.bOpen) return;

#ifdef X88000_SDL3_HAS_IMGUI
	ImGui::SetCurrentContext(pp.pImGuiCtx);
	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext(pp.pImGuiCtx);
	ImGui::SetCurrentContext(pMainCtx);
	pp.pImGuiCtx = NULL;
#endif

	if (pp.pTexture) {
		SDL_DestroyTexture(pp.pTexture);
		pp.pTexture = NULL;
	}
	SDL_DestroyRenderer(pp.pRenderer);
	SDL_DestroyWindow(pp.pWindow);
	pp.pRenderer = NULL;
	pp.pWindow = NULL;
	pp.bOpen = false;
}

// Scanline fill for CG character polygons.
// Fills multiple polygons (even-odd rule) into the pixel buffer.
// Polygon points are in a local coordinate system; pFnMapX/pFnMapY map
// to destination pixel coordinates.
static void FillCGPolygons(
	uint32_t* pPixels, int nTexW, int nTexH,
	const int anPoints[64][2],
	const int anPointCounts[16],
	int nPolygonCount,
	int xDst, int yDst, int cxDst, int cyDst,
	uint32_t color)
{
	if (nPolygonCount <= 0) return;
	if (cxDst <= 0 || cyDst <= 0) return;

	// Transform all points to destination space up-front.
	int axDst[64], ayDst[64];
	int nTotal = 0;
	for (int p = 0; p < nPolygonCount; p++) nTotal += anPointCounts[p];
	if (nTotal > 64) nTotal = 64;
	for (int i = 0; i < nTotal; i++) {
		axDst[i] = xDst + (anPoints[i][0] * cxDst + 8) / 16;
		ayDst[i] = yDst + (anPoints[i][1] * cyDst + 12) / 24;
	}

	// Compute Y bounds.
	int yMin = ayDst[0], yMax = ayDst[0];
	for (int i = 1; i < nTotal; i++) {
		if (ayDst[i] < yMin) yMin = ayDst[i];
		if (ayDst[i] > yMax) yMax = ayDst[i];
	}
	if (yMin < 0) yMin = 0;
	if (yMax >= nTexH) yMax = nTexH - 1;
	if (yMin > yMax) return;

	for (int y = yMin; y <= yMax; y++) {
		int aXCross[64];
		int nCross = 0;
		int nBase = 0;
		for (int p = 0; p < nPolygonCount; p++) {
			int nCount = anPointCounts[p];
			for (int i = 0; i < nCount; i++) {
				int i0 = nBase + i;
				int i1 = nBase + ((i + 1) % nCount);
				int y0 = ayDst[i0];
				int y1 = ayDst[i1];
				if (y0 == y1) continue;
				int yLo = y0 < y1 ? y0 : y1;
				int yHi = y0 < y1 ? y1 : y0;
				// Use half-open interval [yLo, yHi) to avoid
				// double-counting vertices.
				if (y < yLo || y >= yHi) continue;
				int x0 = axDst[i0];
				int x1 = axDst[i1];
				int xCross = x0 + (int)((int64_t)(x1 - x0) * (y - y0) / (y1 - y0));
				if (nCross < 64) aXCross[nCross++] = xCross;
			}
			nBase += nCount;
		}
		// Sort crossings.
		for (int i = 1; i < nCross; i++) {
			int v = aXCross[i];
			int j = i - 1;
			while (j >= 0 && aXCross[j] > v) {
				aXCross[j + 1] = aXCross[j];
				j--;
			}
			aXCross[j + 1] = v;
		}
		// Fill even-odd pairs.
		for (int i = 0; i + 1 < nCross; i += 2) {
			int xA = aXCross[i];
			int xB = aXCross[i + 1];
			if (xA < 0) xA = 0;
			if (xB > nTexW) xB = nTexW;
			for (int x = xA; x < xB; x++) {
				pPixels[y * nTexW + x] = color;
			}
		}
	}
}

void RebuildPrinterTexture(SPrinterPreview& pp,
	SDL_Renderer* pRenderer, CParallelPrinter& printer)
{
	enum { BASE_DPI = 18 };
	int nDrawDPI = BASE_DPI << pp.nZoom;
	int nPrinterDPI = printer.GetDPI();
	int nPaperW = printer.GetPaperWidth();
	int nPaperH = printer.GetPaperHeight();
	if (nPaperW <= 0 || nPaperH <= 0) return;

	int nTexW = (nPaperW * nDrawDPI + nPrinterDPI - 1) / nPrinterDPI;
	int nTexH = (nPaperH * nDrawDPI + nPrinterDPI - 1) / nPrinterDPI;
	if (nTexW < 1) nTexW = 1;
	if (nTexH < 1) nTexH = 1;

	// Limit texture size
	if (nTexW > 4096) nTexW = 4096;
	if (nTexH > 4096) nTexH = 4096;

	// Rebuild pixel buffer
	int nPixels = nTexW * nTexH;
	std::vector<uint32_t> vPixels(nPixels, 0xFFFFFFFF); // white

	float fScaleX = (float)nTexW / nPaperW;
	float fScaleY = (float)nTexH / nPaperH;

	// Draw paper border (black outline)
	for (int x = 0; x < nTexW; x++) {
		vPixels[x] = 0xFF000000;
		vPixels[(nTexH-1) * nTexW + x] = 0xFF000000;
	}
	for (int y = 0; y < nTexH; y++) {
		vPixels[y * nTexW] = 0xFF000000;
		vPixels[y * nTexW + nTexW - 1] = 0xFF000000;
	}

	// Draw printer head (red rectangle)
	int nPage = pp.nPage;
	if (nPage == printer.GetCurrentPage()) {
		int nHeadX = printer.GetHeadX();
		int nHeadY = printer.GetHeadY();
		int nHeadH = printer.GetHeadHeight();
		int nPaperLeft = 0, nPaperTop = 0;
		nPaperLeft = printer.GetPaperLeft();
	nPaperTop = printer.GetPaperTop();

		int px0 = (int)((nHeadX - nPaperLeft) * fScaleX);
		int py0 = (int)((nHeadY - nPaperTop) * fScaleY);
		int px1 = px0 + (int)(2 * fScaleX);
		int py1 = py0 + (int)(nHeadH * fScaleY);
		if (px0 < 0) px0 = 0;
		if (py0 < 0) py0 = 0;
		if (px1 > nTexW) px1 = nTexW;
		if (py1 > nTexH) py1 = nTexH;
		for (int y = py0; y < py1; y++) {
			for (int x = px0; x < px1; x++) {
				vPixels[y * nTexW + x] = 0xFFFF4444; // red
			}
		}
	}

	// Draw sprocket holes for continuous paper
	int nSelectedPaper = printer.GetSelectedPaper();
	if (nSelectedPaper == CParallelPrinter::PAPER_C10 ||
		nSelectedPaper == CParallelPrinter::PAPER_C15)
	{
		int nGap = nPrinterDPI / 2;
		int nDiam = (nPrinterDPI * 40) / 254;
		int nRadius = (int)(nDiam * fScaleX / 2);
		if (nRadius < 1) nRadius = 1;
		// Left and right columns
		for (int side = 0; side < 2; side++) {
			int cx = side == 0
				? (int)(nDiam * fScaleX / 2)
				: nTexW - 1 - (int)(nDiam * fScaleX / 2);
			for (int yDot = nGap/2; yDot < nPaperH; yDot += nGap) {
				int cy = (int)(yDot * fScaleY);
				// Draw filled circle (simple midpoint)
				for (int dy = -nRadius; dy <= nRadius; dy++) {
					for (int dx = -nRadius; dx <= nRadius; dx++) {
						if (dx*dx + dy*dy <= nRadius*nRadius) {
							int px = cx + dx;
							int py = cy + dy;
							if (px >= 0 && px < nTexW && py >= 0 && py < nTexH) {
								vPixels[py * nTexW + px] = 0xFF000000;
							}
						}
					}
				}
			}
		}
	}

	// Draw printer objects (images and text)
	int nPaperLeft = 0, nPaperTop = 0;
	nPaperLeft = printer.GetPaperLeft();
	nPaperTop = printer.GetPaperTop();

	if (nPage >= 0 && nPage < (int)printer.size()) {
		CPrinterPage* pPage = *(printer.begin() + nPage);
		for (std::list<CPrinterObject*>::const_iterator it = pPage->begin();
			it != pPage->end(); ++it)
		{
			CPrinterObject* pObj = *it;
			if (pObj->GetObjectType() == CPrinterObject::POBJ_IMAGE) {
				const CPrinterImageObject* pImg =
					(const CPrinterImageObject*)pObj;
				int ox = (int)((pImg->GetX() - nPaperLeft) * fScaleX);
				int oy = (int)((pImg->GetY() - nPaperTop) * fScaleY);
				int ow = (int)(pImg->GetWidth() * fScaleX);
				int oh = (int)(pImg->GetHeight() * fScaleY);
				if (ow < 1) ow = 1;
				if (oh < 1) oh = 1;
				int srcW = pImg->GetDotCX();
				int srcH = pImg->GetDotCY();
				for (int dy = 0; dy < oh; dy++) {
					int sy = srcH > 0 ? (dy * srcH / oh) : 0;
					for (int dx = 0; dx < ow; dx++) {
						int sx = srcW > 0 ? (dx * srcW / ow) : 0;
						if (pImg->GetPixel(sx, sy) == 0) {
							int px = ox + dx;
							int py = oy + dy;
							if (px >= 0 && px < nTexW && py >= 0 && py < nTexH) {
								vPixels[py * nTexW + px] = 0xFF000000;
							}
						}
					}
				}
			} else if (pObj->GetObjectType() == CPrinterObject::POBJ_TEXT) {
				const CPrinterTextObject* pTxt =
					(const CPrinterTextObject*)pObj;
				int ox = (int)((pTxt->GetX() - nPaperLeft) * fScaleX);
				int oy = (int)((pTxt->GetY() - nPaperTop) * fScaleY);
				int charH = pTxt->GetCharHeight();
				int oh = (int)(charH * fScaleY);
				if (oh < 1) oh = 1;
				int nLen = pTxt->GetTextLength();
				int xCur = ox;
				int nGaijiIndex = 0;

				for (int i = 0; i < nLen; i++) {
					int cw = (int)(pTxt->GetCharWidth(i) * fScaleX);
					if (cw < 1) cw = 1;
					uint16_t wText = pTxt->GetText()[i];
					int nCharType = pTxt->GetCharType(i);

					if (nCharType == CPrinterTextObject::CHAR_CG) {
						int anPoints[64][2];
						int anPointCounts[16];
						int nPolygonCount = 0, nTotalPts = 0;
						CX88PrinterDrawer::GetCGCharacterData(
							wText, anPoints, anPointCounts,
							nPolygonCount, nTotalPts);
						if (nPolygonCount > 0) {
							FillCGPolygons(
								&vPixels[0], nTexW, nTexH,
								anPoints, anPointCounts, nPolygonCount,
								xCur, oy, cw, oh, 0xFF000000);
						}
						xCur += cw + (int)(pTxt->GetRightGap(i) * fScaleX);
						continue;
					}

					if (nCharType == CPrinterTextObject::CHAR_GAIJI) {
						if (nGaijiIndex < pTxt->GetGaijiCount()) {
							const CPrinterGaiji* pGaiji =
								pTxt->GetGaiji(nGaijiIndex);
							if (pGaiji && pGaiji->IsValidData()) {
								int srcW = pGaiji->GetDotCX();
								int srcH = pGaiji->GetDotCY();
								for (int dy = 0; dy < oh; dy++) {
									int sy = srcH > 0 ? (dy * srcH / oh) : 0;
									for (int dx = 0; dx < cw; dx++) {
										int sx = srcW > 0 ? (dx * srcW / cw) : 0;
										if (pGaiji->GetPixel(sx, sy) == 0) {
											int px = xCur + dx;
											int py = oy + dy;
											if (px >= 0 && px < nTexW && py >= 0 && py < nTexH) {
												vPixels[py * nTexW + px] = 0xFF000000;
											}
										}
									}
								}
							}
						}
						nGaijiIndex++;
						xCur += cw + (int)(pTxt->GetRightGap(i) * fScaleX);
						continue;
					}

					if (wText <= 0x20) {
						xCur += cw + (int)(pTxt->GetRightGap(i) * fScaleX);
						continue;
					}

					bool bIsPrintableAscii =
						(nCharType == CPrinterTextObject::CHAR_ASCII ||
						 nCharType == CPrinterTextObject::CHAR_ANK) &&
						wText >= 0x21 && wText <= 0x7E;
					if (bIsPrintableAscii) {
						// Render using embedded 5x7 bitmap font, scaled to cell
						static const uint8_t s_font5x7[][7] = {
							{0x20,0x20,0x20,0x00,0x20,0x00,0x00}, // ! 0x21
							{0x50,0x50,0x00,0x00,0x00,0x00,0x00}, // "
							{0x50,0xF8,0x50,0xF8,0x50,0x00,0x00}, // #
							{0x20,0x78,0xA0,0x70,0x28,0xF0,0x20}, // $
							{0xC8,0xD0,0x20,0x58,0x98,0x00,0x00}, // %
							{0x40,0xA0,0x40,0xA8,0x50,0x00,0x00}, // &
							{0x20,0x20,0x00,0x00,0x00,0x00,0x00}, // '
							{0x10,0x20,0x20,0x20,0x20,0x20,0x10}, // (
							{0x40,0x20,0x20,0x20,0x20,0x20,0x40}, // )
							{0x00,0x50,0x20,0x50,0x00,0x00,0x00}, // *
							{0x00,0x20,0x70,0x20,0x00,0x00,0x00}, // +
							{0x00,0x00,0x00,0x00,0x20,0x20,0x40}, // ,
							{0x00,0x00,0x70,0x00,0x00,0x00,0x00}, // -
							{0x00,0x00,0x00,0x00,0x00,0x20,0x00}, // .
							{0x08,0x10,0x10,0x20,0x40,0x40,0x80}, // /
							{0x70,0x88,0x98,0xA8,0xC8,0x88,0x70}, // 0
							{0x20,0x60,0x20,0x20,0x20,0x20,0x70}, // 1
							{0x70,0x88,0x08,0x30,0x40,0x80,0xF8}, // 2
							{0x70,0x88,0x08,0x30,0x08,0x88,0x70}, // 3
							{0x10,0x30,0x50,0x90,0xF8,0x10,0x10}, // 4
							{0xF8,0x80,0xF0,0x08,0x08,0x88,0x70}, // 5
							{0x30,0x40,0x80,0xF0,0x88,0x88,0x70}, // 6
							{0xF8,0x08,0x10,0x20,0x20,0x20,0x20}, // 7
							{0x70,0x88,0x88,0x70,0x88,0x88,0x70}, // 8
							{0x70,0x88,0x88,0x78,0x08,0x10,0x60}, // 9
							{0x00,0x20,0x00,0x00,0x20,0x00,0x00}, // :
							{0x00,0x20,0x00,0x00,0x20,0x20,0x40}, // ;
							{0x08,0x10,0x20,0x40,0x20,0x10,0x08}, // <
							{0x00,0x00,0x70,0x00,0x70,0x00,0x00}, // =
							{0x40,0x20,0x10,0x08,0x10,0x20,0x40}, // >
							{0x70,0x88,0x08,0x10,0x20,0x00,0x20}, // ?
							{0x70,0x88,0xB8,0xA8,0xB8,0x80,0x70}, // @
							{0x20,0x50,0x88,0x88,0xF8,0x88,0x88}, // A
							{0xF0,0x88,0x88,0xF0,0x88,0x88,0xF0}, // B
							{0x70,0x88,0x80,0x80,0x80,0x88,0x70}, // C
							{0xF0,0x88,0x88,0x88,0x88,0x88,0xF0}, // D
							{0xF8,0x80,0x80,0xF0,0x80,0x80,0xF8}, // E
							{0xF8,0x80,0x80,0xF0,0x80,0x80,0x80}, // F
							{0x70,0x88,0x80,0xB8,0x88,0x88,0x70}, // G
							{0x88,0x88,0x88,0xF8,0x88,0x88,0x88}, // H
							{0x70,0x20,0x20,0x20,0x20,0x20,0x70}, // I
							{0x38,0x10,0x10,0x10,0x10,0x90,0x60}, // J
							{0x88,0x90,0xA0,0xC0,0xA0,0x90,0x88}, // K
							{0x80,0x80,0x80,0x80,0x80,0x80,0xF8}, // L
							{0x88,0xD8,0xA8,0xA8,0x88,0x88,0x88}, // M
							{0x88,0xC8,0xA8,0xA8,0x98,0x88,0x88}, // N
							{0x70,0x88,0x88,0x88,0x88,0x88,0x70}, // O
							{0xF0,0x88,0x88,0xF0,0x80,0x80,0x80}, // P
							{0x70,0x88,0x88,0x88,0xA8,0x90,0x68}, // Q
							{0xF0,0x88,0x88,0xF0,0xA0,0x90,0x88}, // R
							{0x70,0x88,0x80,0x70,0x08,0x88,0x70}, // S
							{0xF8,0x20,0x20,0x20,0x20,0x20,0x20}, // T
							{0x88,0x88,0x88,0x88,0x88,0x88,0x70}, // U
							{0x88,0x88,0x88,0x88,0x50,0x50,0x20}, // V
							{0x88,0x88,0x88,0xA8,0xA8,0xD8,0x88}, // W
							{0x88,0x88,0x50,0x20,0x50,0x88,0x88}, // X
							{0x88,0x88,0x50,0x20,0x20,0x20,0x20}, // Y
							{0xF8,0x08,0x10,0x20,0x40,0x80,0xF8}, // Z
							{0x70,0x40,0x40,0x40,0x40,0x40,0x70}, // [
							{0x80,0x40,0x40,0x20,0x10,0x10,0x08}, // backslash
							{0x70,0x10,0x10,0x10,0x10,0x10,0x70}, // ]
							{0x20,0x50,0x88,0x00,0x00,0x00,0x00}, // ^
							{0x00,0x00,0x00,0x00,0x00,0x00,0xF8}, // _
							{0x40,0x20,0x10,0x00,0x00,0x00,0x00}, // `
							{0x00,0x00,0x70,0x08,0x78,0x88,0x78}, // a
							{0x80,0x80,0xF0,0x88,0x88,0x88,0xF0}, // b
							{0x00,0x00,0x70,0x80,0x80,0x80,0x70}, // c
							{0x08,0x08,0x78,0x88,0x88,0x88,0x78}, // d
							{0x00,0x00,0x70,0x88,0xF8,0x80,0x70}, // e
							{0x30,0x48,0x40,0xF0,0x40,0x40,0x40}, // f
							{0x00,0x00,0x78,0x88,0x78,0x08,0x70}, // g
							{0x80,0x80,0xB0,0xC8,0x88,0x88,0x88}, // h
							{0x20,0x00,0x60,0x20,0x20,0x20,0x70}, // i
							{0x10,0x00,0x30,0x10,0x10,0x90,0x60}, // j
							{0x80,0x80,0x90,0xA0,0xC0,0xA0,0x90}, // k
							{0x60,0x20,0x20,0x20,0x20,0x20,0x70}, // l
							{0x00,0x00,0xD0,0xA8,0xA8,0xA8,0x88}, // m
							{0x00,0x00,0xB0,0xC8,0x88,0x88,0x88}, // n
							{0x00,0x00,0x70,0x88,0x88,0x88,0x70}, // o
							{0x00,0x00,0xF0,0x88,0xF0,0x80,0x80}, // p
							{0x00,0x00,0x78,0x88,0x78,0x08,0x08}, // q
							{0x00,0x00,0xB0,0xC8,0x80,0x80,0x80}, // r
							{0x00,0x00,0x78,0x80,0x70,0x08,0xF0}, // s
							{0x40,0x40,0xF0,0x40,0x40,0x48,0x30}, // t
							{0x00,0x00,0x88,0x88,0x88,0x98,0x68}, // u
							{0x00,0x00,0x88,0x88,0x88,0x50,0x20}, // v
							{0x00,0x00,0x88,0xA8,0xA8,0xA8,0x50}, // w
							{0x00,0x00,0x88,0x50,0x20,0x50,0x88}, // x
							{0x00,0x00,0x88,0x88,0x78,0x08,0x70}, // y
							{0x00,0x00,0xF8,0x10,0x20,0x40,0xF8}, // z
							{0x18,0x20,0x20,0x40,0x20,0x20,0x18}, // {
							{0x20,0x20,0x20,0x20,0x20,0x20,0x20}, // |
							{0xC0,0x20,0x20,0x10,0x20,0x20,0xC0}, // }
							{0x00,0x00,0x48,0xA8,0x90,0x00,0x00}, // ~
						};
						int idx = wText - 0x21;
						if (idx >= 0 && idx < (int)(sizeof(s_font5x7)/sizeof(s_font5x7[0]))) {
							float fsx = (float)cw / 5.0f;
							float fsy = (float)oh / 7.0f;
							for (int gy = 0; gy < 7; gy++) {
								for (int gx = 0; gx < 5; gx++) {
									if (s_font5x7[idx][gy] & (0x80 >> gx)) {
										// Fill scaled pixel
										int px0 = xCur + (int)(gx * fsx);
										int py0 = oy + (int)(gy * fsy);
										int px1 = xCur + (int)((gx+1) * fsx);
										int py1 = oy + (int)((gy+1) * fsy);
										if (px1 <= px0) px1 = px0 + 1;
										if (py1 <= py0) py1 = py0 + 1;
										for (int py = py0; py < py1 && py < nTexH; py++) {
											if (py < 0) continue;
											for (int px = px0; px < px1 && px < nTexW; px++) {
												if (px < 0) continue;
												vPixels[py * nTexW + px] = 0xFF000000;
											}
										}
									}
								}
							}
						}
					} else {
						// Non-renderable (kanji/kana/other): small filled block
						int blockH = oh * 2 / 3;
						if (blockH < 1) blockH = 1;
						int blockW = cw > 1 ? cw - 1 : 1;
						int blockTop = oy + (oh - blockH) / 2;
						for (int dy = 0; dy < blockH; dy++) {
							int py = blockTop + dy;
							if (py < 0 || py >= nTexH) continue;
							for (int dx = 0; dx < blockW; dx++) {
								int px = xCur + dx;
								if (px >= 0 && px < nTexW) {
									vPixels[py * nTexW + px] = 0xFF404040;
								}
							}
						}
					}
					xCur += cw + (int)(pTxt->GetRightGap(i) * fScaleX);
				}
				// Underline
				if (pTxt->GetLineType() & CPrinterTextObject::LINE_UNDER) {
					int lineY = oy + oh - 1;
					if (lineY >= 0 && lineY < nTexH) {
						for (int dx = ox; dx < xCur && dx < nTexW; dx++) {
							if (dx >= 0) vPixels[lineY * nTexW + dx] = 0xFF000000;
						}
					}
				}
				// Overline
				if (pTxt->GetLineType() & CPrinterTextObject::LINE_UPPER) {
					if (oy >= 0 && oy < nTexH) {
						for (int dx = ox; dx < xCur && dx < nTexW; dx++) {
							if (dx >= 0) vPixels[oy * nTexW + dx] = 0xFF000000;
						}
					}
				}
			}
		}
	}

	// Upload to texture
	if (pp.pTexture && (pp.nTexW != nTexW || pp.nTexH != nTexH)) {
		SDL_DestroyTexture(pp.pTexture);
		pp.pTexture = NULL;
	}
	if (!pp.pTexture) {
		pp.pTexture = SDL_CreateTexture(pRenderer,
			SDL_PIXELFORMAT_ARGB8888,
			SDL_TEXTUREACCESS_STREAMING,
			nTexW, nTexH);
	}
	if (pp.pTexture) {
		SDL_UpdateTexture(pp.pTexture, NULL, &vPixels[0], nTexW * 4);
	}
	pp.nTexW = nTexW;
	pp.nTexH = nTexH;
	pp.bDirty = false;
}

static const char* s_aszPaperNames[] = {
	"None", "10\" Continuous", "15\" Continuous",
	"A5 Portrait", "A5 Landscape",
	"A4 Portrait", "A4 Landscape",
	"A3 Portrait", "A3 Landscape",
	"B5 Portrait", "B5 Landscape",
	"B4 Portrait", "B4 Landscape",
	"Postcard Portrait", "Postcard Landscape"
};

void DrawPrinterPreviewContent(SPrinterPreview& pp)
{
	CParallelPrinter* pPrinter = NULL;
	if (g_nParallelDevice == 1) {
		pPrinter = &g_parallelPR201;
	}

	// Menu bar
	if (ImGui::BeginMainMenuBar()) {
		if (pPrinter) {
			int nPaper = pPrinter->GetSelectedPaper();
			ImGui::SetNextItemWidth(180);
			if (ImGui::Combo("##Paper", &nPaper, s_aszPaperNames, 15)) {
				pPrinter->SelectPaper(nPaper);
				pp.bDirty = true;
			}
			ImGui::SameLine();
			bool bCenter = pPrinter->IsPaperCentering();
			if (ImGui::Checkbox("Center", &bCenter)) {
				pPrinter->SetPaperCentering(bCenter);
				pp.bDirty = true;
			}
			ImGui::Separator();

			int nPageCount = (int)pPrinter->size();
			if (pp.nPage >= nPageCount && nPageCount > 0) {
				pp.nPage = nPageCount - 1;
				pp.bDirty = true;
			}
			ImGui::Text("Page %d/%d", nPageCount > 0 ? pp.nPage + 1 : 0, nPageCount);
			if (ImGui::Button("<") && pp.nPage > 0) {
				pp.nPage--;
				pp.bDirty = true;
			}
			ImGui::SameLine(0, 2);
			if (ImGui::Button(">") && pp.nPage < nPageCount - 1) {
				pp.nPage++;
				pp.bDirty = true;
			}
			ImGui::Separator();

			if (ImGui::Button("-") && pp.nZoom > 0) {
				pp.nZoom--;
				pp.bDirty = true;
			}
			ImGui::SameLine(0, 2);
			if (ImGui::Button("+") && pp.nZoom < 4) {
				pp.nZoom++;
				pp.bDirty = true;
			}
			ImGui::SameLine();
			ImGui::Text("%d%%", (100 * (18 << pp.nZoom)) / pPrinter->GetDPI());
			ImGui::Separator();

			if (ImGui::Button("Feed")) {
				pPrinter->Flush();
				pp.bDirty = true;
			}
			if (ImGui::Button("Del")) {
				if (nPageCount > 0) {
					pPrinter->DeletePage(pp.nPage);
					if (pp.nPage >= (int)pPrinter->size() && pp.nPage > 0)
						pp.nPage--;
					pp.bDirty = true;
				}
			}
			if (ImGui::Button("Copy Text")) {
				if (nPageCount > 0) {
					CX88PrinterDrawer drawer;
					std::string strText;
					drawer.ExtractText(pPrinter, pp.nPage, strText, false);
					SDL_SetClipboardText(strText.c_str());
				}
			}
			if (ImGui::Button("Reset")) {
				pPrinter->Initialize();
				pPrinter->Reset();
				pp.nPage = 0;
				pp.bDirty = true;
			}
		} else {
			ImGui::TextDisabled("No printer connected");
		}
		ImGui::EndMainMenuBar();
	}

	if (!pPrinter) return;

	// Check dirty flag
	if (pPrinter->IsDirty()) {
		pPrinter->SetDirty(false);
		pp.bDirty = true;
	}
	if (pp.bDirty && pPrinter->GetPaperWidth() > 0) {
		RebuildPrinterTexture(pp, pp.pRenderer, *pPrinter);
	}

	// Preview: fill remaining window area with scrollable image
	if (pp.pTexture && pp.nTexW > 0 && pp.nTexH > 0) {
		float fMenuH = ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y;
		ImGui::SetNextWindowPos(ImVec2(0, fMenuH));
		ImGui::SetNextWindowSize(ImVec2(
			ImGui::GetMainViewport()->Size.x,
			ImGui::GetMainViewport()->Size.y - fMenuH));
		ImGui::Begin("##Preview", NULL,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_HorizontalScrollbar |
			ImGuiWindowFlags_NoSavedSettings);
		ImGui::Image((ImTextureID)(intptr_t)pp.pTexture,
			ImVec2((float)pp.nTexW, (float)pp.nTexH));
		ImGui::End();
	}
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
		FILE* fpt = NX88Utility::Fopen_UTF8(fstrPath.c_str(), "rb");
		if (fpt != NULL) {
			return fpt;
		}
#ifdef X88_PLATFORM_UNIX
		if (fstrUpperName != strName) {
			fstrPath = fstrDir + fstrUpperName;
			fpt = NX88Utility::Fopen_UTF8(fstrPath.c_str(), "rb");
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

std::string g_strPendingDebugLogDir;
std::mutex  g_mtxDebugLogDir;
bool g_bDebugLogFolderDialogResolved = false;
bool g_bDebugLogFolderDialogAccepted = false;
bool g_bStartDebugLogAfterFolderPick = false;
std::string g_strLastDebugLogDir;

void SDLCALL OnDebugLogFolderSelected(void* userdata, const char* const* filelist, int filter)
{
	(void)userdata;
	(void)filter;
	std::lock_guard<std::mutex> lock(g_mtxDebugLogDir);
	g_bDebugLogFolderDialogResolved = true;
	g_bDebugLogFolderDialogAccepted = false;
	g_strPendingDebugLogDir.clear();
	if (filelist && filelist[0]) {
		g_strPendingDebugLogDir = EnsureTrailingSlash(filelist[0]);
		g_bDebugLogFolderDialogAccepted = true;
	}
}

bool StartDebugLog(const std::string& fstrDir)
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
	std::string fstrDir2 = fstrDir.empty()
		? GetDefaultFolderDialogDir()
		: EnsureTrailingSlash(fstrDir);
	fstrPath = fstrDir2 + szTime;
	g_pfDebugLog = NX88Utility::Fopen_UTF8(fstrPath.c_str(), "at");
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
	style.Colors[ImGuiCol_TitleBg]          = ImVec4(tintDim.x, tintDim.y, tintDim.z, 0.2f);
	style.Colors[ImGuiCol_TitleBgActive]    = ImVec4(tint.x, tint.y, tint.z, 0.4f);
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
	DisableRendererVSync(dw.pRenderer, "debug window");

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
		std::string sFontPath = ResolveFontPath();
		if (!sFontPath.empty()) {
			ImFontConfig fontCfg;
			fontCfg.OversampleH = 2;
			fontCfg.OversampleV = 1;
			dbgIO.Fonts->AddFontFromFileTTF(
				sFontPath.c_str(), 20.0f, &fontCfg,
				dbgIO.Fonts->GetGlyphRangesJapanese());
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
		FILE* fTest = NX88Utility::Fopen_UTF8(dw.strIniPath.c_str(), "r");
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
	std::string strName = NX88Utility::ConvSJIStoUTF8(pDiskImage->GetImageName());
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

// Memory image load — async dialog result
std::string g_strPendingMemoryImage;
std::mutex  g_mtxMemoryImage;

void SDLCALL OnMemoryImageDialogResult(
	void* pUserData,
	const char* const* ppszFileList,
	int)
{
	(void)pUserData;
	if (ppszFileList && ppszFileList[0]) {
		std::lock_guard<std::mutex> lock(g_mtxMemoryImage);
		g_strPendingMemoryImage = ppszFileList[0];
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
	(void)bCoreReady;
	(void)bPauseEmulation;
	SDL_SetWindowTitle(pWindow, kMainWindowTitle);
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

#ifdef X88_PLATFORM_WINDOWS
	const char* pszAppData = getenv("APPDATA");
	if ((pszAppData != NULL) && (*pszAppData != '\0')) {
		RegisterRomSearchDir(std::string(pszAppData) + "\\X88000M");
	}
#elif defined(__APPLE__)
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
	g_parallelPR201.Initialize();
	g_parallelPR201.Reset();
	if (!ProbeRomAvailability()) {
		return false;
	}
	CPC88::Initialize();
	CPC88::Reset();
	// Interlace is applied later via ApplyEnvSettingsFromIni; default off.
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
		SX88Color* pColorTable = g_screenDrawer.GetColorTable();
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
	bool bShowBeepStatsWindow = false;
	bool bShowEnvWindow = false;
	bool bShowDiskWindow = false;
	bool bShowTapeWindow = false;
	SPrinterPreview printerPreview;
	InitPrinterPreview(printerPreview);
	SDebugWindow dbgWin;
	InitDebugWindowStruct(dbgWin);
	bool bBoostMode = false;
	SEnvSettingsView envView;
	std::vector<uint32_t> vArgbBuffer;
	const Uint64 nPerfFreq = SDL_GetPerformanceFrequency();
	const Uint64 nFrameTicks = (nPerfFreq > 0)? (nPerfFreq / 60U): 0U;
	char szMediaPath[1024];
	szMediaPath[0] = '\0';
	int nSelectedDrive = 0;

	CSdl3Settings settings;
	settings.Load();
	envView.nBoostLimiter =
		atoi(settings.GetSectionString(SECTION_OPTION, "boostlim", "0").c_str());

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
		g_audio.SetMasterVolume(atoi(settings.GetSectionString(SECTION_OPTION, "mastervolume", "50").c_str()));
		g_audio.SetBeepVolume(atoi(settings.GetSectionString(SECTION_OPTION, "beepvolume", "50").c_str()));
		g_audio.SetPcgVolume (atoi(settings.GetSectionString(SECTION_OPTION, "pcgvolume",  "50").c_str()));
		g_audio.SetBeepMute  (ParseBoolEntry(settings.GetSectionString(SECTION_OPTION, "beepmute", "off"), false));
		g_audio.SetPcgMute   (ParseBoolEntry(settings.GetSectionString(SECTION_OPTION, "pcgmute",  "off"), false));
	}
#endif

	int nInitialWindowW = settings.GetInt("window.width", 712);
	int nInitialWindowH = settings.GetInt("window.height", 428);
	if (nInitialWindowW < 320) { nInitialWindowW = 320; }
	if (nInitialWindowH < 240) { nInitialWindowH = 240; }
	int nInitialWindowX = settings.GetInt("window.x", SDL_WINDOWPOS_CENTERED);
	int nInitialWindowY = settings.GetInt("window.y", SDL_WINDOWPOS_CENTERED);
	SDL_Window* pWindow = SDL_CreateWindow(
		kMainWindowTitle,
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
	DisableRendererVSync(pRenderer, "main window");

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

	// Load Noto Sans JP so that Japanese text renders correctly.
	// Falls back to the built-in ASCII font if the file is missing.
	{
		std::string sFontPath = ResolveFontPath();
		if (!sFontPath.empty()) {
			ImFontConfig fontCfg;
			fontCfg.OversampleH = 2;
			fontCfg.OversampleV = 1;
			io.Fonts->AddFontFromFileTTF(
				sFontPath.c_str(), 20.0f, &fontCfg,
				io.Fonts->GetGlyphRangesJapanese());
		} else {
			fprintf(stderr,
				"[warn] font not found — falling back to "
				"built-in font (no Japanese glyphs)\n");
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

	// Emulation FPS measurement (counts UpdateCoreFrame() calls per second window).
	int      nEmuFrameCount = 0;
	uint64_t nEmuFpsWindowStart = SDL_GetTicks();
	float    fEmuFps = 0.0f;
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
			} else if (printerPreview.bOpen && evtWinID == printerPreview.nWindowID) {
				ImGui::SetCurrentContext(printerPreview.pImGuiCtx);
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
			// Printer window close
			if (evt.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED
				&& printerPreview.bOpen
				&& evt.window.windowID == printerPreview.nWindowID)
			{
				ClosePrinterWindow(printerPreview, pMainImGuiCtx);
			}
#endif
				if (evt.type == SDL_EVENT_QUIT) {
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
		// Process pending screenshot save
		{
			std::lock_guard<std::mutex> lock(g_mtxScreenshot);
			if (!g_strPendingScreenshotPath.empty()) {
				DoSaveScreenshot(g_strPendingScreenshotPath);
				g_strPendingScreenshotPath.clear();
			}
		}
		// Process pending RAM export folder selection
		{
			bool bResolved = false;
			bool bAccepted = false;
			std::string strExportDir;
			{
				std::lock_guard<std::mutex> lock(g_mtxExportDir);
				if (g_bExportFolderDialogResolved) {
					bResolved = true;
					bAccepted = g_bExportFolderDialogAccepted;
					strExportDir = g_strPendingExportDir;
					g_bExportFolderDialogResolved = false;
					g_bExportFolderDialogAccepted = false;
					g_strPendingExportDir.clear();
				}
			}
			if (bResolved && g_ramExportRequest.bActive) {
				if (bAccepted) {
					g_strLastExportDir = strExportDir;
					g_strRamExportStatus =
						ExportSelectedRam(strExportDir, g_ramExportRequest);
				}
				g_ramExportRequest.bActive = false;
			}
		}
		// Process pending debug log folder selection
		{
			bool bResolved = false;
			bool bAccepted = false;
			std::string strDebugLogDir;
			{
				std::lock_guard<std::mutex> lock(g_mtxDebugLogDir);
				if (g_bDebugLogFolderDialogResolved) {
					bResolved = true;
					bAccepted = g_bDebugLogFolderDialogAccepted;
					strDebugLogDir = g_strPendingDebugLogDir;
					g_bDebugLogFolderDialogResolved = false;
					g_bDebugLogFolderDialogAccepted = false;
					g_strPendingDebugLogDir.clear();
				}
			}
			if (bResolved && g_bStartDebugLogAfterFolderPick) {
				if (bAccepted) {
					g_strLastDebugLogDir = strDebugLogDir;
					StartDebugLog(strDebugLogDir);
				}
				g_bStartDebugLogAfterFolderPick = false;
			}
		}
		// Process pending memory image load
		{
			std::lock_guard<std::mutex> lock(g_mtxMemoryImage);
			if (!g_strPendingMemoryImage.empty()) {
				CPC88::LoadMemoryImage(g_strPendingMemoryImage);
				g_strPendingMemoryImage.clear();
			}
		}
		// Flush printer buffer periodically (~every 6 frames ≈ 100ms)
		if (bCoreReady && g_nParallelDevice == 1) {
			static int nPrinterFlushCount = 0;
			if (++nPrinterFlushCount >= 6) {
				nPrinterFlushCount = 0;
				g_parallelPR201.Flush();
			}
		}
		if (bCoreReady && !bPauseEmulation) {
			if (!g_queueIMEChar.empty()) {
				// Paste text with the same 2ms-granularity key injection as the legacy frontend.
				// by splitting frame execution into EXECUTE_UNIT_TIME chunks.
				enum { EXEC_UNIT = 2 }; // ms, matches CX88000::EXECUTE_UNIT_TIME
				enum { IME_WAIT = 200 }; // ms per character
				int nClockPerUnit = CPC88::GetBaseClock() * 1000 * EXEC_UNIT;
				int nUnitsPerFrame = (1000/60) / EXEC_UNIT; // ~8 units per frame
				for (int nUnit = 0; nUnit < nUnitsPerFrame && !g_queueIMEChar.empty(); nUnit++) {
					CPC88::Z80Main().ClearKeyMatrics();
					uint16_t wKey = g_queueIMEChar.front();
					// Set modifier keys
					CPC88::Z80Main().SetKeyMatrics(
						0x08, 6, (wKey & 0x0100) != 0);
					CPC88::Z80Main().SetKeyMatrics(
						0x08, 5, (wKey & 0x1000) != 0);
					// Press actual key during middle portion of wait
					if (g_nIMECharPhase >= IME_WAIT/4 &&
						g_nIMECharPhase < (IME_WAIT*3)/4)
					{
						CPC88::Z80Main().SetKeyMatrics(
							(wKey >> 4) & 0x0F, wKey & 0x07, true);
					}
					g_nIMECharPhase += EXEC_UNIT;
					if (g_nIMECharPhase >= IME_WAIT) {
						g_nIMECharPhase = 0;
						g_queueIMEChar.pop();
					}
					if (!CPC88::IsDebugMode()) {
						CPC88::Execute(nClockPerUnit);
					}
				}
				// Run remaining clocks if any
				if (g_queueIMEChar.empty()) {
					// Restore normal keyboard
					CPC88::Z80Main().ClearKeyMatrics();
				}
			} else {
				// Normal keyboard: only when main window has focus
				SDL_WindowFlags nMainFlags = SDL_GetWindowFlags(pWindow);
				const int nFrameExecClock = GetFrameExecuteClock();
				if (nMainFlags & SDL_WINDOW_INPUT_FOCUS) {
					UpdateKeyMatricsFromSDL();
				}
				if (!CPC88::IsDebugMode()) {
					CPC88::Execute(nFrameExecClock);
				} else if (!CPC88::IsDebugStopped()) {
					CPC88::DebugExecute(nFrameExecClock);
				}
			}
			UpdateCoreFrame();
			UploadCoreFrameToTexture(pFrameTexture, vArgbBuffer);
			++nEmuFrameCount;
		}
		{
			uint64_t nFpsNow = SDL_GetTicks();
			uint64_t nElapsed = nFpsNow - nEmuFpsWindowStart;
			if (nElapsed >= 500) {
				fEmuFps = (float)nEmuFrameCount * 1000.0f / (float)nElapsed;
				nEmuFrameCount = 0;
				nEmuFpsWindowStart = nFpsNow;
			}
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

					// BASIC Mode quick switch (matches the legacy submenu layout).
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
					if (ImGui::MenuItem("Boost Mode", NULL, bBoostMode, bCoreReady)) {
						bBoostMode = !bBoostMode;
					}
					ImGui::Separator();

					if (ImGui::BeginMenu("Clipboard", bCoreReady)) {
						if (ImGui::MenuItem("Save Screenshot (BMP)...")) {
							const SDL_DialogFileFilter aFilter[] = {
								{ "BMP Image", "bmp" }
							};
							SDL_ShowSaveFileDialog(
								OnScreenshotPathSelected, NULL,
								pWindow, aFilter, 1, NULL);
						}
						if (ImGui::MenuItem("Copy Screen Text")) {
							DoCopyScreenText();
						}
						if (ImGui::MenuItem("Paste Text")) {
							char* pszClip = SDL_GetClipboardText();
							if (pszClip) {
								AddPasteText(pszClip);
								SDL_free(pszClip);
							}
						}
						ImGui::EndMenu();
					}

					if (ImGui::BeginMenu("Parallel Port", bCoreReady)) {
						if (ImGui::MenuItem("Null Device", NULL, g_nParallelDevice == 0)) {
							if (g_nParallelDevice != 0) {
								g_nParallelDevice = 0;
								g_parallelNull.Initialize();
								g_parallelNull.Reset();
								CPC88::Z80Main().SetParallelDevice(g_parallelNull);
							}
						}
						if (ImGui::MenuItem("PC-PR201 Printer", NULL, g_nParallelDevice == 1)) {
							if (g_nParallelDevice != 1) {
								g_nParallelDevice = 1;
								g_parallelPR201.Initialize();
								g_parallelPR201.Reset();
								CPC88::Z80Main().SetParallelDevice(g_parallelPR201);
							}
						}
						ImGui::Separator();
						if (ImGui::MenuItem("Printer Preview...", NULL, printerPreview.bOpen)) {
							if (printerPreview.bOpen) {
								ClosePrinterWindow(printerPreview, pMainImGuiCtx);
							} else {
								OpenPrinterWindow(printerPreview, pMainImGuiCtx);
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
					if (ImGui::MenuItem("Reset Window Size")) {
						SDL_SetWindowSize(pWindow, 712, 428);
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Quit X88000M")) {
						bRunning = false;
					}
					ImGui::EndMenu();
				}

				// ----- Media menu -----
				if (ImGui::BeginMenu("Media")) {
#ifdef X88000_SDL3_HAS_CORE
					if (ImGui::MenuItem("Open Media...", "Ctrl+O", false, bCoreReady)) {
						RequestOpenMediaDialog(pWindow, -1);
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Disk Manager...", NULL, bShowDiskWindow, bCoreReady)) {
						bShowDiskWindow = !bShowDiskWindow;
					}
					if (ImGui::MenuItem("Tape Manager...", NULL, bShowTapeWindow, bCoreReady)) {
						bShowTapeWindow = !bShowTapeWindow;
					}
					if (ImGui::MenuItem("Load Memory Image...", NULL, false, bCoreReady)) {
						static const SDL_DialogFileFilter s_aMemFilters[] = {
							{"Memory Image", "n80"},
							{"All Files", "*"}
						};
						SDL_ShowOpenFileDialog(
							OnMemoryImageDialogResult, NULL,
							pWindow, s_aMemFilters,
							(int)(sizeof(s_aMemFilters)/sizeof(s_aMemFilters[0])),
							NULL, false);
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
#ifdef X88_PLATFORM_WINDOWS
								std::string::size_type nBSlash = fpath.rfind('\\');
								if (nBSlash != std::string::npos &&
									(nSlash == std::string::npos || nBSlash > nSlash))
								{
									nSlash = nBSlash;
								}
#endif
								std::string strDir = (nSlash != std::string::npos)
									? fpath.substr(0, nSlash + 1) : "./";
								dbgWin.strIniPath = strDir + "imgui.ini";
							}
							OpenDebugWindow(dbgWin, pMainImGuiCtx, settings);
						}
					}
					ImGui::Separator();

					if (ImGui::MenuItem("BEEP Stats...", NULL, bShowBeepStatsWindow)) {
						bShowBeepStatsWindow = !bShowBeepStatsWindow;
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
#ifdef X88000M_VERSION
					ImGui::Text("Version %s (based on X88000 1.5.3)", X88000M_VERSION);
#endif
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
			// Printer preview is drawn in its own window below.
			// Debug panels are drawn in the separate debug window below.
#endif
			if (bShowBeepStatsWindow) {
				if (ImGui::Begin("BEEP Stats", &bShowBeepStatsWindow)) {
					static CSdl3AudioOutput::SBeepStats sPrev = {};
					static uint64_t sPrevPerf = 0;
					static double   sBit5Rate = 0.0;
					static double   sBit7Rate = 0.0;
					static double   sWriteRate = 0.0;
					CSdl3AudioOutput::SBeepStats cur = g_audio.GetBeepStats();
					uint64_t nNow = SDL_GetPerformanceCounter();
					uint64_t nFreq = SDL_GetPerformanceFrequency();
					if (sPrevPerf != 0 && nFreq > 0) {
						double dElapsed = (double)(nNow - sPrevPerf)
							/ (double)nFreq;
						if (dElapsed >= 0.25) {
							sBit5Rate = (double)(cur.nBit5Transitions
								- sPrev.nBit5Transitions) / dElapsed;
							sBit7Rate = (double)(cur.nBit7Transitions
								- sPrev.nBit7Transitions) / dElapsed;
							sWriteRate = (double)(cur.nWriteCount
								- sPrev.nWriteCount) / dElapsed;
							sPrev = cur;
							sPrevPerf = nNow;
						}
					} else {
						sPrev = cur;
						sPrevPerf = nNow;
					}
					ImGui::Text("Port 40h writes: %llu total (%.1f /s)",
						(unsigned long long)cur.nWriteCount, sWriteRate);
					ImGui::Text("bit5 (BEEP gate): %s   transitions %llu (%.1f /s  -> %.1f Hz square)",
						cur.bCurBit5? "HIGH": "LOW",
						(unsigned long long)cur.nBit5Transitions,
						sBit5Rate, sBit5Rate * 0.5);
					ImGui::Text("bit7 (SING):      %s   transitions %llu (%.1f /s  -> %.1f Hz square)",
						cur.bCurBit7? "HIGH": "LOW",
						(unsigned long long)cur.nBit7Transitions,
						sBit7Rate, sBit7Rate * 0.5);
					ImGui::TextUnformatted(
						"A full square-wave cycle needs 2 transitions,"
						" so audible pitch = transitions/s / 2.");
				}
				ImGui::End();
			}
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
			static bool     s_bPanelLeft = ParseBoolEntry(
				settings.GetSectionString(SECTION_OPTION, "sidepanel_left", "off"), false);
#ifdef X88000_SDL3_HAS_CORE
			// Side panel (72px wide, on left or right of the 640px emu area)
			{
				static uint64_t s_nLastClockTick = 0;
				static int      s_nClock = 4;
				static int      s_nBasicMode = CPC88Z80Main::BASICMODE_N88V2;
				static bool     s_bHighSpeed = true;
				// LED hold: keep lit for 100ms after last access
				static uint64_t s_anFddLitUntil[CPC88Fdc::DRIVE_MAX] = {};
				static int      s_nMasterVol = atoi(
					settings.GetSectionString(SECTION_OPTION, "mastervolume", "50").c_str());

				uint64_t nNow = SDL_GetTicks();
				if (nNow - s_nLastClockTick >= 2000 || s_nLastClockTick == 0) {
					s_nLastClockTick = nNow;
					s_nClock     = CPC88::GetBaseClock();
					s_nBasicMode = CPC88::GetBasicMode();
					s_bHighSpeed = CPC88::IsHighSpeedMode();
				}

				for (int i = 0; i < CPC88Fdc::DRIVE_MAX; ++i) {
					if (CPC88::Fdc().IsDriveAccessing(i)) s_anFddLitUntil[i] = nNow + 100;
				}

				const char* pszBasic = "N88-V2";
				switch (s_nBasicMode) {
				case CPC88Z80Main::BASICMODE_N:
					pszBasic = "N-BASIC"; break;
				case CPC88Z80Main::BASICMODE_N88V1:
					pszBasic = s_bHighSpeed ? "N88-V1H" : "N88-V1S"; break;
				case CPC88Z80Main::BASICMODE_N88V2:
					pszBasic = "N88-V2";  break;
				case CPC88Z80Main::BASICMODE_N80V1:
					pszBasic = "N80-V1";  break;
				case CPC88Z80Main::BASICMODE_N80V2:
					pszBasic = "N80-V2";  break;
				}

				// Match panel height to the letterboxed emu screen height
				float fLogicalW  = ImGui::GetIO().DisplaySize.x;
				float fLogicalH  = ImGui::GetIO().DisplaySize.y;
				float fAvailW    = fLogicalW - 72.0f;
				float fAvailH    = fLogicalH - fMenuBarH;
				float fEmuScale  = (fAvailW / 640.0f < fAvailH / 400.0f)
					? fAvailW / 640.0f : fAvailH / 400.0f;
				float fEmuH      = 400.0f * fEmuScale;
				float fPanelY    = fMenuBarH + (fAvailH - fEmuH) * 0.5f;
				float fPanelX    = s_bPanelLeft ? 0.0f : (fLogicalW - 72.0f);
				ImGui::SetNextWindowPos(ImVec2(fPanelX, fPanelY));
				ImGui::SetNextWindowSize(ImVec2(72.0f, fEmuH));
				ImGui::Begin("##sidepanel", nullptr,
					ImGuiWindowFlags_NoResize       |
					ImGuiWindowFlags_NoMove         |
					ImGuiWindowFlags_NoTitleBar     |
					ImGuiWindowFlags_NoScrollbar    |
					ImGuiWindowFlags_NoBringToFrontOnFocus);

				ImDrawList* dl = ImGui::GetWindowDrawList();
				const float fLedW = 16.0f;
				const float fLedH =  8.0f;
				const ImU32 colOn    = IM_COL32(220, 50, 50, 255);
				const ImU32 colOff   = IM_COL32(80, 20, 20, 255);
				const ImU32 colNoEqu = IM_COL32(35, 12, 12, 255);

				// Center a string in the panel window
				auto TextCentered = [](const char* s) {
					float fW = ImGui::CalcTextSize(s).x;
					ImGui::SetCursorPosX((ImGui::GetWindowWidth() - fW) * 0.5f);
					ImGui::TextUnformatted(s);
				};

				// Panel side toggle: move button to the opposite side
				// (<< when on right, >> when on left)
				{
					const char* pszLabel = s_bPanelLeft ? ">>" : "<<";
					const float fBtnW = 32.0f;
					ImGui::SetCursorPosX((ImGui::GetWindowWidth() - fBtnW) * 0.5f);
					if (ImGui::Button(pszLabel, ImVec2(fBtnW, 0))) {
						s_bPanelLeft = !s_bPanelLeft;
						settings.SetSectionString(SECTION_OPTION,
							"sidepanel_left", BoolToOnOff(s_bPanelLeft));
					}
				}
				ImGui::Separator();

				// BASIC mode
				TextCentered(pszBasic);
				ImGui::Separator();

				// Clock (updated every 2s)
				char szClock[12];
				snprintf(szClock, sizeof(szClock), "%dMHz", s_nClock);
				TextCentered(szClock);
				ImGui::Separator();

				// Drive section header + 4 LEDs (LED and number vertically centered)
				TextCentered("Drive");
				{
					const float fGap   = 4.0f;
					const float fLineH = ImGui::GetTextLineHeight();
					for (int i = 0; i < CPC88Fdc::DRIVE_MAX; ++i) {
						bool bLit   = nNow <= s_anFddLitUntil[i];
						bool bEquip = CPC88::Fdc().IsDriveEquip(i);
						ImU32 col   = bLit ? colOn : (bEquip ? colOff : colNoEqu);

						char szNum[4];
						snprintf(szNum, sizeof(szNum), "%d", i + 1);
						float fNumW   = ImGui::CalcTextSize(szNum).x;
						float fRowW   = fLedW + fGap + fNumW;
						float fStartX = (ImGui::GetWindowWidth() - fRowW) * 0.5f;

						// Draw LED rectangle centered vertically on the text line
						ImGui::SetCursorPosX(fStartX);
						ImVec2 pScreen = ImGui::GetCursorScreenPos();
						float fLedY = pScreen.y + (fLineH - fLedH) * 0.5f;
						dl->AddRectFilled(
							ImVec2(pScreen.x,         fLedY),
							ImVec2(pScreen.x + fLedW, fLedY + fLedH),
							col);

						// Draw number on the same row, past the LED
						ImGui::SetCursorPosX(fStartX + fLedW + fGap);
						ImGui::TextUnformatted(szNum);
					}
				}
				ImGui::Separator();

				// Volume — label / current value / fixed-height vertical slider
				TextCentered("Vol");
				char szVol[8];
				snprintf(szVol, sizeof(szVol), "%d", s_nMasterVol);
				TextCentered(szVol);
				{
					const float fSliderW = 16.0f;
					const float fSliderH = 72.0f;
					ImGui::SetCursorPosX(ImGui::GetCursorPosX()
						+ (ImGui::GetContentRegionAvail().x - fSliderW) * 0.5f);
					if (ImGui::VSliderInt("##vol", ImVec2(fSliderW, fSliderH),
							&s_nMasterVol, 0, 100, "")) {
						g_audio.SetMasterVolume(s_nMasterVol);
						char szBuf[8];
						snprintf(szBuf, sizeof(szBuf), "%d", s_nMasterVol);
						settings.SetSectionString(SECTION_OPTION, "mastervolume", szBuf);
					}
				}

				ImGui::Separator();
				TextCentered("FPS");
				char szFps[16];
				snprintf(szFps, sizeof(szFps), "%.1f", fEmuFps);
				TextCentered(szFps);

				ImGui::End();
			}
#endif // X88000_SDL3_HAS_CORE
			ImGui::Render();
#endif

		SDL_SetRenderDrawColor(pRenderer, 18, 24, 30, 255);
		SDL_RenderClear(pRenderer);

			if (bCoreReady) {
				int nWindowW = 0;
				int nWindowH = 0;
				SDL_GetRenderOutputSize(pRenderer, &nWindowW, &nWindowH);
#ifdef X88000_SDL3_HAS_IMGUI
				// Offset below the ImGui menu bar; 72px on one side is reserved for side panel
				float fScale = SDL_GetWindowDisplayScale(pWindow);
				float fMenuPx = fMenuBarH * fScale;
				float fPanelPx = 72.0f * fScale;
				int nEmuW = nWindowW - (int)fPanelPx;
				SDL_FRect rctDst = CalcLetterboxRect(
					nEmuW, (int)(nWindowH - fMenuPx), 640, 400);
				rctDst.y += fMenuPx;
				if (s_bPanelLeft) rctDst.x += fPanelPx;
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
					ImGui::BeginDisabled(!bDebugMode || g_bStartDebugLogAfterFolderPick);
					bool bLogging = IsDebugLogging();
					if (ImGui::MenuItem("Record Execution Log", NULL, bLogging)) {
						if (bLogging) {
							EndDebugLog();
						} else {
							if (g_strLastDebugLogDir.empty()) {
								g_strLastDebugLogDir = GetDefaultFolderDialogDir();
							}
							g_bStartDebugLogAfterFolderPick = true;
							SDL_ShowOpenFolderDialog(
								OnDebugLogFolderSelected, NULL,
								dbgWin.pWindow, g_strLastDebugLogDir.c_str(), false);
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

			// Build initial layout on first frame (matching the legacy panel arrangement).
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

		// === Printer preview window render pass ===
		if (printerPreview.bOpen && bCoreReady) {
			ImGui::SetCurrentContext(printerPreview.pImGuiCtx);
			ImGui_ImplSDLRenderer3_NewFrame();
			ImGui_ImplSDL3_NewFrame();
			ImGui::NewFrame();

			DrawPrinterPreviewContent(printerPreview);

			ImGui::Render();
			SDL_SetRenderDrawColor(printerPreview.pRenderer, 30, 30, 30, 255);
			SDL_RenderClear(printerPreview.pRenderer);
			ImGui_ImplSDLRenderer3_RenderDrawData(
				ImGui::GetDrawData(), printerPreview.pRenderer);
			SDL_RenderPresent(printerPreview.pRenderer);

			ImGui::SetCurrentContext(pMainImGuiCtx);
		}
#endif

		if (!bBoostMode && nFrameTicks > 0) {
			// Normal mode: run at 1x speed (60 FPS pacing)
			nNextFrameTick += nFrameTicks;
			Uint64 nNow = SDL_GetPerformanceCounter();
			if (nNow < nNextFrameTick) {
				Uint64 nWaitPerf = nNextFrameTick - nNow;
				Uint64 nWaitMs = (nWaitPerf * 1000U) / nPerfFreq;
				if (nWaitMs > 0) {
					SDL_Delay((Uint32)nWaitMs);
				}
			} else if ((nNow - nNextFrameTick) > nFrameTicks * 4U) {
				nNextFrameTick = nNow;
			}
		} else if (bBoostMode && nFrameTicks > 0) {
			// Boost mode: run as fast as possible, limited by boost limiter.
			// Limiter value is a percentage of normal speed (e.g. 200 = 2x).
			// 0 = unlimited.
			int nLim = envView.nBoostLimiter;
			if (nLim > 0) {
				Uint64 nLimitedTicks = nFrameTicks * 100U / (Uint64)nLim;
				nNextFrameTick += nLimitedTicks;
				Uint64 nNow = SDL_GetPerformanceCounter();
				if (nNow < nNextFrameTick) {
					Uint64 nWaitMs = ((nNextFrameTick - nNow) * 1000U) / nPerfFreq;
					if (nWaitMs > 0) {
						SDL_Delay((Uint32)nWaitMs);
					}
				} else if ((nNow - nNextFrameTick) > nLimitedTicks * 4U) {
					nNextFrameTick = nNow;
				}
			}
		}
	}

#ifdef X88000_SDL3_HAS_IMGUI
	// Close sub-windows before destroying main ImGui context.
	if (printerPreview.bOpen) {
		ClosePrinterWindow(printerPreview, pMainImGuiCtx);
	}
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
