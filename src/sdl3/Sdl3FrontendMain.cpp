#include <SDL3/SDL.h>

#ifdef X88000_SDL3_HAS_CORE

#include "StdHeader.h"
#include "PC88.h"
#include "X88ScreenDrawer.h"
#include "ParallelNull.h"

#include <ctype.h>
#include <stdlib.h>

#endif

#ifdef X88000_SDL3_HAS_IMGUI

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#endif

#include <stdio.h>
#include <string>
#include <vector>

#ifdef X88000_SDL3_HAS_CORE

namespace {

std::vector<std::string> g_vRomSearchDir;
CPC88 g_pc88;
CX88ScreenDrawer g_screenDrawer;
CParallelNull g_parallelNull;
bool g_bScreenDrawerReady = false;

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

void OutputCoreDebugLog(int)
{
	// TODO: map PC88 debug logs into SDL3 debug overlay / log pane.
}

void OnCoreIntVectChanged()
{
	// TODO: route interrupt-vector changes to frontend debugger indicators.
}

void OnCoreBeepOutput(bool, bool)
{
	// TODO: feed SDL audio backend.
}

void OnCorePcgOutput(int, int)
{
	// TODO: feed SDL audio backend.
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
	CPC88::Z80Main().SetIntVectChangeCallback(OnCoreIntVectChanged);
	CPC88::Z80Main().SetBeepOutputCallback(OnCoreBeepOutput);
	CPC88::Pcg().SetPcgSoundOutputCallback(OnCorePcgOutput);
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

int main(int, char**) {
	bool bCoreReady = false;
	std::vector<uint32_t> vArgbBuffer;
	const Uint64 nPerfFreq = SDL_GetPerformanceFrequency();
	const Uint64 nFrameTicks = (nPerfFreq > 0)? (nPerfFreq / 60U): 0U;

#ifdef X88000_SDL3_HAS_CORE
	bCoreReady = InitializeCore();
#endif

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO)) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	SDL_Window* pWindow = SDL_CreateWindow(
		"X88000 SDL3 Frontend (Prototype)",
		1024,
		768,
		0);
	if (pWindow == NULL) {
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}

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
	ImGui_ImplSDL3_InitForSDLRenderer(pWindow, pRenderer);
	ImGui_ImplSDLRenderer3_Init(pRenderer);
#endif

	bool bRunning = true;
	Uint64 nNextFrameTick = SDL_GetPerformanceCounter();
	while (bRunning) {
		SDL_Event evt;
		while (SDL_PollEvent(&evt)) {
#ifdef X88000_SDL3_HAS_IMGUI
			ImGui_ImplSDL3_ProcessEvent(&evt);
#endif
			if (evt.type == SDL_EVENT_QUIT) {
				bRunning = false;
			}
			if ((evt.type == SDL_EVENT_KEY_DOWN) &&
				(evt.key.key == SDLK_ESCAPE))
			{
				bRunning = false;
			}
		}

#ifdef X88000_SDL3_HAS_CORE
		if (bCoreReady) {
			UpdateKeyMatricsFromSDL();
			CPC88::Execute(4000000/60);
			UpdateCoreFrame();
			UploadCoreFrameToTexture(pFrameTexture, vArgbBuffer);
		}
#endif

#ifdef X88000_SDL3_HAS_IMGUI
		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
		ImGui::Begin("Status");
		ImGui::TextUnformatted("SDL3 + ImGui frontend bootstrap");
		ImGui::Text("Core: %s", bCoreReady? "initialized": "ROM not found");
		ImGui::End();
		ImGui::Render();
#endif

		SDL_SetRenderDrawColor(pRenderer, 18, 24, 30, 255);
		SDL_RenderClear(pRenderer);

		if (bCoreReady) {
			SDL_RenderTexture(pRenderer, pFrameTexture, NULL, NULL);
		}

#ifdef X88000_SDL3_HAS_IMGUI
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), pRenderer);
#endif

		SDL_RenderPresent(pRenderer);

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
	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
#endif

	SDL_DestroyTexture(pFrameTexture);
	SDL_DestroyRenderer(pRenderer);
	SDL_DestroyWindow(pWindow);
#ifdef X88000_SDL3_HAS_CORE
	ShutdownCore();
#endif
	SDL_Quit();
	return 0;
}
