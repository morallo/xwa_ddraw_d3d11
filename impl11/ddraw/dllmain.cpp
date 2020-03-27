// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes (c) 2019

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <Windows.h>
#include <objbase.h>

#include <stdio.h>
#include <vector>
#include "Vectors.h"
#include "Matrices.h"
#include "config.h"

#include "XWAObject.h"

extern PlayerDataEntry* PlayerDataTable;
extern uint32_t* g_playerIndex;
extern uint32_t *g_rawFOVDist; // raw FOV dist(dword int), copy of one of the six values hard-coded with the resolution slots, which are what xwahacker edits
extern float *g_fRawFOVDist; // FOV dist(float), same value as above
extern float *g_cachedFOVDist; // cached FOV dist / 512.0 (float), seems to be used for some sprite processing
auto mouseLook = (__int8*)0x77129C;
extern float g_fDefaultFOVDist;

extern int g_KeySet;
extern float g_fMetricMult, g_fAspectRatio, g_fConcourseAspectRatio;

#ifdef DBG_VR
extern bool g_bFixSkyBox, g_bSkipGUI, g_bSkipText, g_bSkipSkyBox;
extern bool g_bDo3DCapture, g_bStart3DCapture;
bool g_bCapture2DOffscreenBuffer = false;
bool g_bDumpDebug = false;

//extern bool g_bDumpSpecificTex;
//extern int g_iDumpSpecificTexIdx;
void IncreaseCockpitThreshold(float Delta);
void IncreaseNoDrawBeforeIndex(int Delta);
void IncreaseNoDrawAfterIndex(int Delta);
//void IncreaseNoExecIndices(int DeltaBefore, int DeltaAfter);
//void IncreaseZOverride(float Delta);
void IncreaseSkipNonZBufferDrawIdx(int Delta);
void IncreaseNoDrawAfterHUD(int Delta);
#endif

// Debug functions
void log_debug(const char *format, ...);
void DumpGlobalLights();

typedef struct float4_struct {
	float x, y, z, w;
} float4;

void Normalize(float4 *Vector) {
	float x = Vector->x;
	float y = Vector->y;
	float z = Vector->z;
	float L = sqrt(x*x + y*y + z*z);
	if (L < 0.001) L = 1.0f;

	Vector->x = x / L;
	Vector->y = y / L;
	Vector->z = z / L;
}

void PrintVector(const Vector4 &Vector) {
	log_debug("[DBG] Vector: %0.3f, %0.3f, %0.3f",
		Vector.x, Vector.y, Vector.z);
}

extern bool g_bDisableBarrelEffect, g_bEnableVR, g_bResetHeadCenter, g_bBloomEnabled, g_bAOEnabled, g_bCustomFOVApplied;
//extern bool g_bLeftKeyDown, g_bRightKeyDown, g_bUpKeyDown, g_bDownKeyDown, g_bUpKeyDownShift, g_bDownKeyDownShift;
extern bool g_bDirectSBSInitialized, g_bSteamVRInitialized, g_bClearHUDBuffers, g_bDCManualActivate;
// extern bool g_bDumpBloomBuffers, 
extern bool g_bDumpSSAOBuffers, g_bEnableSSAOInShader, g_bEnableIndirectSSDO; // g_bEnableBentNormalsInShader;
extern bool g_bShowSSAODebug, g_bShowNormBufDebug, g_bFNEnable, g_bShadowEnable, g_bGlobalSpecToggle, g_bToggleSkipDC;
extern Vector4 g_LightVector[2];
extern float g_fSpecIntensity, g_fSpecBloomIntensity, g_fFocalDist, g_fFakeRoll;

extern bool bFreePIEAlreadyInitialized, g_bDCIgnoreEraseCommands;
void ShutdownFreePIE();

// DEBUG
enum HyperspacePhaseEnum;
extern float g_fHyperTimeOverride;
extern int g_iHyperStateOverride;
// DEBUG
extern bool g_bKeybExitHyperspace;
extern bool g_bFXAAEnabled;

// ACTIVE COCKPIT
extern Vector4 g_contOriginWorldSpace; // , g_contOriginViewSpace;
extern bool g_bActiveCockpitEnabled, g_bACActionTriggered, g_bACTriggerState;
extern float g_fLPdebugPointOffset;
extern bool g_bDumpLaserPointerDebugInfo;

extern Vector3 g_LaserPointDebug;

HWND ThisWindow = 0;
WNDPROC OldWindowProc = 0;

// User-facing functions
void ResetVRParams(); // Restores default values for the view params
void SaveVRParams();
void LoadVRParams();

void IncreaseIPD(float Delta);
void IncreaseScreenScale(float Delta); // Changes overall zoom
void IncreaseFocalDist(float Delta);   // Changes overall zoom after matrix projection
//void IncreasePostProjScale(float Delta);
void ToggleZoomOutMode();
void IncreaseZoomOutScale(float Delta);
void IncreaseHUDParallax(float Delta);
void IncreaseTextParallax(float Delta);
void IncreaseFloatingGUIParallax(float Delta);
void ToggleCockpitPZHack();
void IncreaseSkipNonZBufferDrawIdx(int Delta);

// Lens distortion
void IncreaseLensK1(float Delta);
void IncreaseLensK2(float Delta);

bool InitDirectSBS();
bool ShutDownDirectSBS();

// SteamVR
#include <headers/openvr.h>
extern bool g_bSteamVREnabled, g_bSteamVRInitialized, g_bUseSteamVR;
extern vr::IVRSystem *g_pHMD;
extern vr::IVRScreenshots *g_pVRScreenshots;
bool InitSteamVR();
void ShutDownSteamVR();
void ApplyFOV(float FOV);

/*
 * Save the current FOV and metric multiplier to an external file
 */
void SaveFocalLength() {
	FILE *file;
	int error = 0;

	try {
		error = fopen_s(&file, "./FocalLength.cfg", "wt");
	}
	catch (...) {
		log_debug("[DBG] [FOV] Could not save FocalLength.cfg");
	}

	if (error != 0) {
		log_debug("[DBG] [FOV] Error %d when saving FocalLength.cfg", error);
		return;
	}

	fprintf(file, "; The focal length is measured in pixels. This parameter can be modified without\n");
	fprintf(file, "; VR, so, technically, it's not a 'VRParam'\n");
	fprintf(file, "focal_length = %0.6f\n", *g_fRawFOVDist);
	fclose(file);
}

void LoadFocalLength() {
	log_debug("[DBG] [FOV] Loading FocalLength...");
	FILE *file;
	int error = 0;

	try {
		error = fopen_s(&file, "./FocalLength.cfg", "rt");
	}
	catch (...) {
		log_debug("[DBG] [FOV] Could not load FocalLength.cfg");
	}

	if (error != 0) {
		log_debug("[DBG] [FOV] Error %d when loading FocalLength.cfg", error);
		return;
	}

	char buf[160], param[80], svalue[80];
	int param_read_count = 0;
	float fValue = 0.0f;

	while (fgets(buf, 160, file) != NULL) {
		// Skip comments and blank lines
		if (buf[0] == ';' || buf[0] == '#')
			continue;
		if (strlen(buf) == 0)
			continue;

		if (sscanf_s(buf, "%s = %s", param, 80, svalue, 80) > 0) {
			fValue = (float)atof(svalue);
			if (_stricmp(param, "focal_length") == 0) {
				ApplyFOV(fValue);
				log_debug("[DBG] [FOV] Applied FOV: %0.6f", fValue);
			}
		}
	}
	fclose(file);
}

void ApplyFOV(float FOV) 
{
	*g_fRawFOVDist = FOV;
	*g_cachedFOVDist = *g_fRawFOVDist / 512.0f;
	*g_rawFOVDist = (uint32_t)*g_fRawFOVDist;
}

void IncreaseFOV(float delta) 
{
	*g_fRawFOVDist += delta;
	*g_cachedFOVDist = *g_fRawFOVDist / 512.0f;
	*g_rawFOVDist = (uint32_t)*g_fRawFOVDist;
	log_debug("[DBG] [FOV] rawFOV: %d, fRawFOV: %0.6f, cachedFOV: %0.6f",
		*g_rawFOVDist, *g_fRawFOVDist, *g_cachedFOVDist);
}

void IncreaseMetricMult(float delta) 
{
	g_fMetricMult += delta;
	if (g_fMetricMult < 0.1f)
		g_fMetricMult = 0.1f;
	log_debug("[DBG] [FOV] g_fMetricMult: %0.3f", g_fMetricMult);
}

void IncreaseAspectRatio(float delta) {
	g_fAspectRatio += delta;
	g_fConcourseAspectRatio += delta;
	log_debug("[DBG] [FOV] Aspect Ratio: %0.6f, Concourse Aspect Ratio: %0.6f", g_fAspectRatio, g_fConcourseAspectRatio);
}

#define MAX_ACTION_LEN 10
void RunAction(WORD *action) {
	// Scan codes from: http://www.philipstorr.id.au/pcbook/book3/scancode.htm
	// Scan codes: https://www.win.tue.nl/~aeb/linux/kbd/scancodes-1.html
	// Based on code from: https://stackoverflow.com/questions/18647053/sendinput-not-equal-to-pressing-key-manually-on-keyboard-in-c
	// Virtual key codes: https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
	// How to send extended scan codes
	// https://stackoverflow.com/questions/36972524/winapi-extended-keyboard-scan-codes/36976260#36976260
	// https://stackoverflow.com/questions/26283738/how-to-use-extended-scancodes-in-sendinput
	INPUT input[MAX_ACTION_LEN];
	bool bEscapedAction = (action[0] == 0xe0);

	if (action[0] == 0) // void action, skip
		return;

	// Copy & initialize the scan codes
	int i = 0, j = bEscapedAction ? 1 : 0;
	while (action[j] && j < MAX_ACTION_LEN) {
		input[i].ki.wScan = action[j];
		input[i].type = INPUT_KEYBOARD;
		input[i].ki.time = 0;
		input[i].ki.wVk = 0;
		input[i].ki.dwExtraInfo = 0;
		input[i].ki.dwFlags = KEYEVENTF_SCANCODE;
		if (bEscapedAction)
			input[i].ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
		i++; j++;
	}

	j = bEscapedAction ? 1 : 0;
	while (action[j] && j < MAX_ACTION_LEN) {
		input[i].ki.wScan = action[j];
		input[i].type = INPUT_KEYBOARD;
		input[i].ki.time = 0;
		input[i].ki.wVk = 0;
		input[i].ki.dwExtraInfo = 0;
		input[i].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
		if (bEscapedAction)
			input[i].ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
		i++; j++;
	}

	// Send keydown/keyup events in one go: (this is the only way I found to enable the arrow/escaped keys)
	SendInput(i, input, sizeof(INPUT));
}

LRESULT CALLBACK MyWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	bool AltKey   = (GetAsyncKeyState(VK_MENU)		& 0x8000) == 0x8000;
	bool CtrlKey  = (GetAsyncKeyState(VK_CONTROL)	& 0x8000) == 0x8000;
	bool ShiftKey = (GetAsyncKeyState(VK_SHIFT)		& 0x8000) == 0x8000;
	bool UpKey	  = (GetAsyncKeyState(VK_UP)			& 0x8000) == 0x8000;
	bool DownKey  = (GetAsyncKeyState(VK_DOWN)		& 0x8000) == 0x8000;
	bool LeftKey  = (GetAsyncKeyState(VK_LEFT)		& 0x8000) == 0x8000;
	bool RightKey = (GetAsyncKeyState(VK_RIGHT)		& 0x8000) == 0x8000;

	/*
	if (AltKey && !CtrlKey && !ShiftKey) {
		if (LeftKey) {
			g_fFakeRoll += 1.0f;
			return 0;
		}
		else if (RightKey) {
			g_fFakeRoll -= 1.0f;
			return 0;
		}
	}
	*/

	switch (uMsg)
	{
	case WM_KEYUP: {
		// Ctrl + Alt + Shift
		if (AltKey && CtrlKey && ShiftKey) {
			switch (wParam) {
			case 0xbb:
				IncreaseZoomOutScale(0.1f);
				return 0;
			case 0xbd:
				IncreaseZoomOutScale(-0.1f);
				return 0;

			case VK_RIGHT:
				switch (g_KeySet) {
				case 1:
					g_LightVector[0].x += 0.1f;
					g_LightVector[0].normalize();
					PrintVector(g_LightVector[0]);
					break;
				case 2:
					IncreaseFOV(50.0f);
					SaveFocalLength();
					break;
				case 3:
					g_LaserPointDebug.x += 0.1f;
					log_debug("[DBG] g_LaserPointDebug: %0.3f, %0.3f, %0.3f",
						g_LaserPointDebug.x, g_LaserPointDebug.y, g_LaserPointDebug.z);
					break;
				}

				//g_contOriginWorldSpace.x += 0.02f;

				/*
				g_fHyperTimeOverride += 0.1f;
				if (g_fHyperTimeOverride > 2.5f)
					g_fHyperTimeOverride = 2.5f;
				if (g_fHyperTimeOverride >= 1.5f)
					g_iHyperStateOverride = 3; // HYPER_EXIT
				else
					g_iHyperStateOverride = 4; // POST_HYPER_EXIT
				log_debug("[DBG] State: %d, g_fHyperTimeOverride: %0.3f", g_iHyperStateOverride, g_fHyperTimeOverride);
				*/
				return 0;
			case VK_LEFT:
				switch (g_KeySet) {
				case 1:
					g_LightVector[0].x -= 0.1f;
					g_LightVector[0].normalize();
					PrintVector(g_LightVector[0]);
					break;
				case 2:
					IncreaseFOV(-50.0f);
					SaveFocalLength();
					break;
				case 3:
					g_LaserPointDebug.x -= 0.1f;
					log_debug("[DBG] g_LaserPointDebug: %0.3f, %0.3f, %0.3f",
						g_LaserPointDebug.x, g_LaserPointDebug.y, g_LaserPointDebug.z);
					break;
				}

				//g_contOriginWorldSpace.x -= 0.02f;

				/*
				g_fHyperTimeOverride -= 0.1f;
				if (g_fHyperTimeOverride < 0.0f)
					g_fHyperTimeOverride = 0.0f;
				if (g_fHyperTimeOverride >= 1.5f)
					g_iHyperStateOverride = 3; // HYPER_EXIT
				else
					g_iHyperStateOverride = 4; // POST_HYPER_EXIT
				log_debug("[DBG] State: %d, g_fHyperTimeOverride: %0.3f", g_iHyperStateOverride, g_fHyperTimeOverride);
				*/
				return 0;

			case VK_UP:
				switch (g_KeySet) {
				case 1:
					g_LightVector[0].y += 0.1f;
					g_LightVector[0].normalize();
					PrintVector(g_LightVector[0]);
					break;
				case 2:
					IncreaseMetricMult(0.1f);
					SaveVRParams();
					break;
				case 3:
					g_LaserPointDebug.y += 0.1f;
					log_debug("[DBG] g_LaserPointDebug: %0.3f, %0.3f, %0.3f",
						g_LaserPointDebug.x, g_LaserPointDebug.y, g_LaserPointDebug.z);
					break;
				}

				//g_contOriginWorldSpace.y += 0.02f;
				return 0;
			case VK_DOWN:
				switch (g_KeySet) {
				case 1:
					g_LightVector[0].y -= 0.1f;
					g_LightVector[0].normalize();
					PrintVector(g_LightVector[0]);
					break;
				case 2:
					IncreaseMetricMult(-0.1f);
					SaveVRParams();
					break;
				case 3:
					g_LaserPointDebug.y -= 0.1f;
					log_debug("[DBG] g_LaserPointDebug: %0.3f, %0.3f, %0.3f",
						g_LaserPointDebug.x, g_LaserPointDebug.y, g_LaserPointDebug.z);
					break;
				}

				//g_contOriginWorldSpace.y -= 0.02f;
				return 0;
			}
		}

		// Ctrl + Alt
		if (AltKey && CtrlKey && !ShiftKey) {
			switch (wParam) {

#if DBG_VR
			case 'C':
				log_debug("[DBG] Capture 3D");
				//g_bDo3DCapture = true;
				g_bStart3DCapture = true;
				return 0;

			case 'D':
				// Force a refresh of the display width
				g_bDisplayWidth = true;
				return 0;
			case 'D':
				log_debug("[DBG] Dumping specific texture");
				g_bDumpSpecificTex = true;
				return 0;
				/*case 'T':
					g_bSkipText = !g_bSkipText;
					if (g_bSkipText)
						log_debug("[DBG] Skipping Text GUI");
					else
						log_debug("[DBG] Drawing Text GUI");*/
						/*
						g_bSkipAfterTargetComp = !g_bSkipAfterTargetComp;
						if (g_bSkipAfterTargetComp)
							log_debug("[DBG] Skipping draw calls after TargetComp");
						else
							log_debug("[DBG] NOT Skipping draw calls after TargetComp");
						return 0;
						*/
			case 'F':
				g_bSkipSkyBox = !g_bSkipSkyBox;
				if (g_bFixSkyBox)
					log_debug("[DBG] Skip Skybox ON");
				else
					log_debug("[DBG] Skip Skybox OFF");
				return 0;

			case 'G':
				g_bSkipGUI = !g_bSkipGUI;
				log_debug("[DBG] g_bSkipGUI: %d", g_bSkipGUI);
				return 0;
#endif
				// Ctrl + Alt + Key
				// DEBUG
			case 'D':
				g_bShowSSAODebug = !g_bShowSSAODebug;
				//log_debug("[DBG] g_bShowSSAODebug: %d", g_bShowSSAODebug);
				return 0;
			case 'F':
				g_config.FXAAEnabled = !g_config.FXAAEnabled;
				return 0;
			case 'X':
				g_bDumpSSAOBuffers = true;
				return 0;
			//case 'G':
			//	DumpGlobalLights();
			//	return 0;
			case 'G':
				g_bDumpLaserPointerDebugInfo = true;
				return 0;
				// DEBUG
			case 'P':
				g_bEnableIndirectSSDO = !g_bEnableIndirectSSDO;
				return 0;
			case 'I':
				g_bShadowEnable = !g_bShadowEnable;
				log_debug("[DBG] Shadows Enabled: %d", g_bShadowEnable);
				return 0;
			case 'A':
				g_bBloomEnabled = !g_bBloomEnabled;
				return 0;
			// Ctrl+Alt+O
			case 'O':
				g_bAOEnabled = !g_bAOEnabled;
				return 0;
			case 'N':
				// Toggle Normal Mapping
				g_bFNEnable = !g_bFNEnable;
				return 0;

			case 'B':
				g_bDisableBarrelEffect = !g_bDisableBarrelEffect;
				SaveVRParams();
				return 0;
			case 'R':
				ResetVRParams();
				return 0;

			case 'V':
				// We can't just switch to VR mode if the game was started with no VR support
				if (!g_bEnableVR) {
					if (g_bSteamVRInitialized || g_bDirectSBSInitialized)
						g_bEnableVR = true;
				}
				else { // VR mode can always be disabled
					g_bEnableVR = !g_bEnableVR;
				}
				return 0;

			case 'S':
				SaveVRParams();
				return 0;
			//case 'T':
			//	g_bToggleSkipDC = !g_bToggleSkipDC;
			//	return 0;
			case 'L':
				// Force the re-application of the focal_length
				g_bCustomFOVApplied = false;
				LoadVRParams();
				return 0;
			case 'H':
				//ToggleCockpitPZHack();
				g_bDCIgnoreEraseCommands = !g_bDCIgnoreEraseCommands;
				return 0;
			case 'W':
				g_bGlobalSpecToggle = !g_bGlobalSpecToggle;
				/*
				if (g_fSpecIntensity > 0.5f) {
					g_fSpecIntensity = 0.0f;
					g_fSpecBloomIntensity = 0.0f;
				} 
				else {
					g_fSpecIntensity = 1.0f;
					g_fSpecBloomIntensity = 1.25f;
				}
				*/
				return 0;
			case 'E':
				g_bEnableSSAOInShader = !g_bEnableSSAOInShader;
				return 0;
			/*
			case 'Q':
				g_bActiveCockpitEnabled = !g_bActiveCockpitEnabled;
				return 0;
			*/

			/*
			
			*/

			case 0xbb:
				IncreaseScreenScale(0.1f);
				return 0;
			case 0xbd:
				IncreaseScreenScale(-0.1f);
				return 0;

			case VK_UP:
				IncreaseLensK1(0.1f);
				SaveVRParams();
				//g_contOriginWorldSpace.z += 0.04f;
				return 0;
			case VK_DOWN:
				IncreaseLensK1(-0.1f);
				SaveVRParams();
				//g_contOriginWorldSpace.z -= 0.04f;
				return 0;
			case VK_LEFT:
				IncreaseLensK2(-0.1f);
				SaveVRParams();
				/*g_fLPdebugPointOffset -= 0.05f;
				if (g_fLPdebugPointOffset < 0.0f)
					g_fLPdebugPointOffset = 0.0f;*/
				//log_debug("[DBG] [AC] g_fDebugZCenter: %0.4f", g_fDebugZCenter);
				//IncreaseFocalDist(-0.1f);
				return 0;
			case VK_RIGHT:
				IncreaseLensK2(0.1f);
				SaveVRParams();
				//g_fLPdebugPointOffset += 0.05f;
				//log_debug("[DBG] [AC] g_fDebugZCenter: %0.4f", g_fDebugZCenter);
				//IncreaseFocalDist(0.1f);
				return 0;
			}
		}

		// Ctrl
		if (CtrlKey && !AltKey && !ShiftKey) {
			switch (wParam) {
			case 'Z':
				ToggleZoomOutMode();
				return 0;
			// Ctrl+O
			//case 'O':
			//	g_bAOEnabled = !g_bAOEnabled;
			//	return 0;

			/*
			case 'K': {
				WORD action[MAX_ACTION_LEN];
				action[0] = 0x46; // Scroll Lock
				action[1] = 0x0;
				log_debug("[DBG] Sending Scroll Lock");
				RunAction(action);
				return 0;
			}
			*/

			// Ctrl+K --> Toggle Mouse Look
			case 'K': {
				*mouseLook = !*mouseLook;
				return 0;
			}

			case 'P':
				if (g_bUseSteamVR && g_pVRScreenshots != NULL) {
					static int scrCounter = 0;
					char prevFileName[80], scrFileName[80];
					sprintf_s(prevFileName, 80, "./preview%d", scrCounter);
					sprintf_s(scrFileName, 80, "./screenshot%d", scrCounter);

					vr::ScreenshotHandle_t scr;
					vr::EVRScreenshotError error = g_pVRScreenshots->TakeStereoScreenshot(&scr, prevFileName, scrFileName);
					if (error)
						log_debug("[DBG] error %d when taking SteamVR screenshot", error);
					else
						log_debug("[DBG] Screenshot %d taken", scrCounter);
					scrCounter++;
				}
				else {
					log_debug("[DBG] !g_bUseSteamVR || g_pVRScreenshots is NULL");
				}
				break;

#if DBR_VR
			case 'X':
				g_bCapture2DOffscreenBuffer = true;
				return 0;
			case 'D':
				g_bDumpDebug = !g_bDumpDebug;
				return 0;
#endif

			// Ctrl + Up/Down
			case VK_UP:
				switch (g_KeySet) {
				case 1:
					g_LightVector[0].z += 0.1f;
					g_LightVector[0].normalize();
					PrintVector(g_LightVector[0]);
					break;
				case 2:
					IncreaseScreenScale(0.1f);
					//IncreasePostProjScale(0.1f);
					SaveVRParams();
					break;
				case 3:
					g_LaserPointDebug.z += 0.1f;
					log_debug("[DBG] g_LaserPointDebug: %0.3f, %0.3f, %0.3f",
						g_LaserPointDebug.x, g_LaserPointDebug.y, g_LaserPointDebug.z);
					break;
				}
				return 0;
				// Ctrl + Down
			case VK_DOWN:
				switch (g_KeySet) {
				case 1:
					g_LightVector[0].z -= 0.1f;
					g_LightVector[0].normalize();
					PrintVector(g_LightVector[0]);
					break;
				case 2:
					IncreaseScreenScale(-0.1f);
					//IncreasePostProjScale(-0.1f);
					SaveVRParams();
					break;
				case 3:
					g_LaserPointDebug.z -= 0.1f;
					log_debug("[DBG] g_LaserPointDebug: %0.3f, %0.3f, %0.3f",
						g_LaserPointDebug.x, g_LaserPointDebug.y, g_LaserPointDebug.z);
					break;
				}
				return 0;
			case VK_LEFT:
				switch (g_KeySet) {
				case 2:
					IncreaseAspectRatio(-0.05f);
					SaveVRParams();
					break;
				}
				return 0;
			case VK_RIGHT:
				switch (g_KeySet) {
				case 2:
					IncreaseAspectRatio(0.05f);
					SaveVRParams();
					break;
				}
				return 0;

			}
		}

		// Ctrl + Shift
		if (CtrlKey && !AltKey && ShiftKey) {
			switch (wParam) {
#if DBG_VR
				/*
				case 0xbb:
					//IncreaseNoExecIndices(0, 1);
					return 0;
				case 0xbd:
					//IncreaseNoExecIndices(0, -1);
					return 0;
				*/
#endif

			case VK_UP:
				IncreaseFloatingGUIParallax(0.05f);
				return 0;
			case VK_DOWN:
				IncreaseFloatingGUIParallax(-0.05f);
				return 0;
			case VK_LEFT:
				IncreaseTextParallax(-0.05f);
				return 0;
			case VK_RIGHT:
				IncreaseTextParallax(0.05f);
				return 0;
			}
		}

		// Shift
		if (ShiftKey && !AltKey && !CtrlKey) {
			switch (wParam) {
			case VK_LEFT:
				IncreaseHUDParallax(-0.1f);
				return 0;
			case VK_RIGHT:
				IncreaseHUDParallax(0.1f);
				return 0;

				/*
				case VK_UP:
					g_bUpKeyDownShift = false;
					g_bUpKeyDown = false;
					return 0;
				case VK_DOWN:
					g_bDownKeyDown = false;
					g_bDownKeyDownShift = false;
					return 0;
				*/
			}
		}

		// Plain key: no Shift, Ctrl, Alt
		if (!ShiftKey && !AltKey && !CtrlKey) {
			switch (wParam) {
			case VK_SPACE:
				// Exit hyperspace
				g_bKeybExitHyperspace = true;

				// ACTIVE COCKPIT:
				// Use the spacebar to activate the cursor on the current active element
				if (g_bActiveCockpitEnabled) {
					//g_bACActionTriggered = true;
					g_bACTriggerState = false;
				}
				break;

			case VK_OEM_PERIOD:
				if (g_bUseSteamVR)
					g_pHMD->ResetSeatedZeroPose();
				g_bResetHeadCenter = true;

				//g_contOriginWorldSpace.set(0.0f, 0.0f, 0.05f, 1);
				g_fFakeRoll = 0.0f;
				//g_contOriginViewSpace = g_contOriginWorldSpace;
				g_LaserPointDebug.x = 0.0f;
				g_LaserPointDebug.y = 0.0f;
				g_LaserPointDebug.z = 0.0f;
				break;
			}
		}

		// Alt doesn't seem to work by itself: wParam is messed up. See the beginning
		// of this function for an example of how to handle the Alt key properly
		/*
		if (!ShiftKey && AltKey && !CtrlKey) {
			switch (wParam) {
			case VK_LEFT:
				log_debug("[DBG] [AC] Alt+Left");
				return 0;
			case VK_RIGHT:
				log_debug("[DBG] [AC] Alt+Right");
				return 0;
			}
		}
		*/
		
		break;
	}

	case WM_KEYDOWN: {
		// Plain key: no Shift, Ctrl, Alt
		if (!ShiftKey && !AltKey && !CtrlKey) {
			switch (wParam) {
			case VK_SPACE:
				if (g_bActiveCockpitEnabled)
					g_bACTriggerState = true;
				return 0;
			}

		}

		/*
		// Shift
		if (ShiftKey && !AltKey && !CtrlKey) {
			switch (wParam) {
			case VK_UP:
				g_bUpKeyDownShift = true;
				return 0;
			case VK_DOWN:
				g_bDownKeyDownShift = true;
				return 0;
			}
		}
		*/
	}
	}
	
	// Call the previous WindowProc handler
	return CallWindowProc(OldWindowProc, hwnd, uMsg, wParam, lParam);
}

bool ReplaceWindowProc(HWND hwnd)
{
	ThisWindow = hwnd;
	OldWindowProc = (WNDPROC )SetWindowLong(ThisWindow, GWL_WNDPROC, (LONG )MyWindowProc);
	if (OldWindowProc != NULL)
		return true;
	return false;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		log_debug("[DBG] **********************");
		log_debug("[DBG] Initializing VR ddraw.dll");
		// Initialize the libraries needed to dump DirectX Textures
		CoInitialize(NULL);
		// Load vrparams.cfg if present
		LoadVRParams();
		// Initialize SteamVR
		if (g_bSteamVREnabled && !g_bSteamVRInitialized) {
			g_bUseSteamVR = InitSteamVR();
			log_debug("[DBG] g_bUseSteamVR: %d", g_bUseSteamVR);
			// Do not attempt initalization again, no matter what happened
			g_bSteamVRInitialized = true;
		}
		else if (g_bEnableVR && !g_bUseSteamVR) {
			log_debug("[DBG] Initializing DirectSBS mode");
			InitDirectSBS();
			g_bDirectSBSInitialized = true;
		}
		g_fDefaultFOVDist = *g_fRawFOVDist;
		log_debug("[DBG] [FOV] Default FOV Dist: %0.3f", g_fDefaultFOVDist);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		if (g_bUseSteamVR)
			ShutDownSteamVR();
		else if (g_bEnableVR)
			ShutDownDirectSBS();
		// Generic FreePIE shutdown, just in case...
		if (bFreePIEAlreadyInitialized)
			ShutdownFreePIE();
		break;
	}

	return TRUE;
}
