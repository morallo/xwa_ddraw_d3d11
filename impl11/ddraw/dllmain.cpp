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
extern float *g_hudScale;

extern float g_fDefaultFOVDist;
extern float g_fDebugFOVscale, g_fDebugYCenter;
extern float g_fCurrentShipFocalLength, g_fReticleScale;
extern bool g_bYCenterHasBeenFixed;
// Current window width and height
int g_WindowWidth, g_WindowHeight;

extern int g_KeySet;
extern float g_fMetricMult, g_fAspectRatio, g_fConcourseAspectRatio, g_fCockpitTranslationScale;
extern bool g_bTriggerReticleCapture;

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
extern bool g_bDirectSBSInitialized, g_bSteamVRInitialized, g_bClearHUDBuffers, g_bDCManualActivate, g_bGlobalDebugFlag;
extern bool g_bDumpSSAOBuffers, g_bEnableSSAOInShader, g_bEnableIndirectSSDO, g_bResetDC, g_bProceduralSuns, g_bEnableHeadLights;
extern bool g_bShowSSAODebug, g_bShowNormBufDebug, g_bFNEnable, g_bShadowEnable, g_bGlobalSpecToggle, g_bToggleSkipDC;
extern Vector4 g_LightVector[2];
extern float g_fSpecIntensity, g_fSpecBloomIntensity, g_fFocalDist, g_fFakeRoll;

extern bool g_bHDREnabled;
extern float g_fHDRWhitePoint;

extern bool bFreePIEAlreadyInitialized, g_bDCIgnoreEraseCommands, g_bEnableLaserLights;
void ShutdownFreePIE();

// DEBUG
enum HyperspacePhaseEnum;
extern float g_fHyperTimeOverride;
extern int g_iHyperStateOverride;
extern float g_fOBJ_Z_MetricMult, g_fOBJGlobalMetricMult;
// DEBUG
extern bool g_bKeybExitHyperspace;
extern bool g_bFXAAEnabled;

// ACTIVE COCKPIT
extern Vector4 g_contOriginWorldSpace; // , g_contOriginViewSpace;
extern bool g_bActiveCockpitEnabled, g_bACActionTriggered, g_bACTriggerState;
extern float g_fLPdebugPointOffset;
extern bool g_bDumpLaserPointerDebugInfo;

extern Vector3 g_LaserPointDebug;

// SHADOW MAPPING
extern float g_fShadowMapAngleY, g_fShadowMapAngleX, g_fShadowMapDepthTrans, g_fShadowMapScale;
extern bool g_bShadowMapEnable, g_bShadowMapDebug, g_bShadowMapEnablePCSS, g_bShadowMapHardwarePCF;

HWND g_ThisWindow = 0;
WNDPROC OldWindowProc = 0;

// User-facing functions
void ResetVRParams(); // Restores default values for the view params
void SaveVRParams();
void LoadVRParams();
void ComputeHyperFOVParams();

void IncreaseIPD(float Delta);
void IncreaseScreenScale(float Delta); // Changes overall zoom
//void IncreaseFocalDist(float Delta);   // Changes overall zoom after matrix projection
//void IncreasePostProjScale(float Delta);
void IncreaseSMZFactor(float Delta);
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
extern bool g_bSteamVREnabled, g_bSteamVRInitialized, g_bUseSteamVR, g_bEnableSteamVR_QPC;
extern vr::IVRSystem *g_pHMD;
extern vr::IVRScreenshots *g_pVRScreenshots;
bool InitSteamVR();
void ShutDownSteamVR();
void ApplyFocalLength(float focal_length);
bool UpdateXWAHackerFOV();
void CycleFOVSetting();
float ComputeRealVertFOV();
float ComputeRealHorzFOV();
float RealVertFOVToRawFocalLength(float real_FOV);

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

	//fprintf(file, "; The focal length is measured in pixels. This parameter can be modified without\n");
	//fprintf(file, "; VR, so, technically, it's not a 'VRParam'\n");
	// Let's not write the focal length in pixels anymore. It doesn't make any sense to
	// anyone and it's only useful internally. We'll continue to read it, but let's start
	// using something sensible
	//fprintf(file, "focal_length = %0.6f\n", *g_fRawFOVDist);
	// Save the *real* vert FOV
	fprintf(file, "; The FOV is measured in degrees. This is the actual vertical FOV.\n");
	if (!g_bEnableVR) {
		fprintf(file, "; This FOV is used when the game is in non-VR mode.\n");
		fprintf(file, "real_FOV = %0.3f\n", ComputeRealVertFOV());
	}
	else {
		fprintf(file, "; This FOV is used only when the game is in VR mode.\n");
		fprintf(file, "VR_FOV = %0.3f\n", ComputeRealVertFOV());
	}
	fclose(file);
}

bool LoadFocalLength() {
	log_debug("[DBG] [FOV] Loading FocalLength...");
	FILE *file;
	int error = 0;
	bool bApplied = false;

	try {
		error = fopen_s(&file, "./FocalLength.cfg", "rt");
	}
	catch (...) {
		log_debug("[DBG] [FOV] Could not load FocalLength.cfg");
	}

	if (error != 0) {
		log_debug("[DBG] [FOV] Error %d when loading FocalLength.cfg", error);
		return bApplied;
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
				ApplyFocalLength(fValue);
				log_debug("[DBG] [FOV] Applied FOV: %0.3f", fValue);
				bApplied = true;
			}
			else if (_stricmp(param, "real_FOV") == 0 && !g_bEnableVR) {
				float RawFocalLength = RealVertFOVToRawFocalLength(fValue);
				ApplyFocalLength(RawFocalLength);
				log_debug("[DBG] [FOV] Applied Real FOV: %0.3f", RawFocalLength);
				bApplied = true;
			}
			else if (_stricmp(param, "VR_FOV") == 0 && g_bEnableVR) {
				float RawFocalLength = RealVertFOVToRawFocalLength(fValue);
				ApplyFocalLength(RawFocalLength);
				log_debug("[DBG] [FOV] Applied VR FOV: %0.3f", RawFocalLength);
				bApplied = true;
			}
		}
	}
	fclose(file);
	return bApplied;
}

void ApplyFocalLength(float focal_length)
{
	log_debug("[DBG] [FOV] Old FOV: %0.3f, Applying: %0.3f", *g_fRawFOVDist, focal_length);
	*g_fRawFOVDist = focal_length;
	*g_cachedFOVDist = *g_fRawFOVDist / 512.0f;
	*g_rawFOVDist = (uint32_t)*g_fRawFOVDist;
	// Force recomputation of the y center:
	g_bYCenterHasBeenFixed = false;
	ComputeHyperFOVParams();
}

void IncreaseFOV(float delta) 
{
	*g_fRawFOVDist += delta;
	*g_cachedFOVDist = *g_fRawFOVDist / 512.0f;
	*g_rawFOVDist = (uint32_t)*g_fRawFOVDist;
	log_debug("[DBG] [FOV] rawFOV: %d, fRawFOV: %0.6f, cachedFOV: %0.6f",
		*g_rawFOVDist, *g_fRawFOVDist, *g_cachedFOVDist);
	// Force recomputation of the y center:
	g_bYCenterHasBeenFixed = false;
	ComputeHyperFOVParams();
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
					// First try to update the DC file. If that fails, then save the regular focal length
					if (!UpdateXWAHackerFOV())
						SaveFocalLength();
					break;
				case 3:
					g_LaserPointDebug.x += 0.1f;
					log_debug("[DBG] g_LaserPointDebug: %0.3f, %0.3f, %0.3f",
						g_LaserPointDebug.x, g_LaserPointDebug.y, g_LaserPointDebug.z);
					break;
				case 4:
					g_fDebugFOVscale += 0.01f;
					log_debug("[DBG] g_fDebugFOVscale: %0.3f", g_fDebugFOVscale);
					ComputeHyperFOVParams();
					//(*g_hudScale) += 0.1f;
					//log_debug("[DBG] g_hudScale: %0.3f", *g_hudScale);
					break;
				case 5:
					g_fCockpitTranslationScale += 0.0005f;
					log_debug("[DBG] g_fCockpitTranslationScale: %0.6f", g_fCockpitTranslationScale);
					break;
				case 6:
					g_fShadowMapAngleY += 7.5f;
					log_debug("[DBG] [SHW] g_fLightMapAngleY: %0.3f", g_fShadowMapAngleY);
					break;
				case 8:
					g_fOBJGlobalMetricMult += 0.05f;
					log_debug("[DBG] g_fOBJGlobalMetricMult: %0.3f", g_fOBJGlobalMetricMult);
					break;
				case 9:
					g_fReticleScale += 0.1f;
					log_debug("[DBG] g_fReticleScale: %0.3f", g_fReticleScale);
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
					// First try to update the DC file. If that fails, then save the regular focal length
					if (!UpdateXWAHackerFOV())
						SaveFocalLength();
					break;
				case 3:
					g_LaserPointDebug.x -= 0.1f;
					log_debug("[DBG] g_LaserPointDebug: %0.3f, %0.3f, %0.3f",
						g_LaserPointDebug.x, g_LaserPointDebug.y, g_LaserPointDebug.z);
					break;
				case 4:
					g_fDebugFOVscale -= 0.01f;
					log_debug("[DBG] g_fDebugFOVscale: %0.3f", g_fDebugFOVscale);
					ComputeHyperFOVParams();
					//(*g_hudScale) -= 0.1f;
					//log_debug("[DBG] g_hudScale: %0.3f", *g_hudScale);
					break;
				case 5:
					g_fCockpitTranslationScale -= 0.0005f;
					log_debug("[DBG] g_fCockpitTranslationScale: %0.6f", g_fCockpitTranslationScale);
					break;
				case 6:
					g_fShadowMapAngleY -= 7.5f;
					log_debug("[DBG] [SHW] g_fLightMapAngleY: %0.3f", g_fShadowMapAngleY);
					break;
				case 8:
					g_fOBJGlobalMetricMult -= 0.05f;
					log_debug("[DBG] g_fOBJGlobalMetricMult: %0.3f", g_fOBJGlobalMetricMult);
					break;
				case 9:
					g_fReticleScale -= 0.1f;
					if (g_fReticleScale < 0.1f)
						g_fReticleScale = 0.1f;
					log_debug("[DBG] g_fReticleScale: %0.3f", g_fReticleScale);
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
				case 4:
					g_fDebugYCenter += 0.01f;
					log_debug("[DBG] g_fDebugYCenter: %0.3f", g_fDebugYCenter);
					ComputeHyperFOVParams();
					break;
				case 6:
					g_fShadowMapAngleX += 7.5f;
					log_debug("[DBG] [SHW] g_fLightMapAngleX: %0.3f", g_fShadowMapAngleX);
					break;

					/*g_fLightMapDistance += 1.0f;
					log_debug("[DBG] [SHW] g_fLightMapDistance: %0.3f", g_fLightMapDistance);
					break;*/
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
				case 4:
					g_fDebugYCenter -= 0.01f;
					log_debug("[DBG] g_fDebugYCenter: %0.3f", g_fDebugYCenter);
					ComputeHyperFOVParams();
					break;
				case 6:
					g_fShadowMapAngleX -= 7.5f;
					log_debug("[DBG] [SHW] g_fLightMapAngleX: %0.3f", g_fShadowMapAngleX);
					break;

					/*g_fLightMapDistance -= 1.0f;
					log_debug("[DBG] [SHW] g_fLightMapDistance: %0.3f", g_fLightMapDistance);
					break;*/
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
			case 'H':
				g_bHDREnabled = !g_bHDREnabled;
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
			//case 'H':
				//ToggleCockpitPZHack();
				//g_bDCIgnoreEraseCommands = !g_bDCIgnoreEraseCommands;
				//g_bGlobalDebugFlag = !g_bGlobalDebugFlag;
				//return 0;
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

			// Ctrl+Alt + Key
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
			case 'H':
				g_bEnableHeadLights = !g_bEnableHeadLights;
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

			// Ctrl+D --> Toggle dynamic lights
			case 'D': {
				g_bEnableLaserLights = !g_bEnableLaserLights;
				return 0;
			}

			// Ctrl+K --> Toggle Mouse Look
			case 'K': {
				*mouseLook = !*mouseLook;
				return 0;
			}

			case 'R': {
				//g_bResetDC = true;
				//g_bProceduralSuns = !g_bProceduralSuns;
				g_bShadowMapDebug = !g_bShadowMapDebug;
				return 0;
			}
			case 'T': {
				g_bShadowMapEnablePCSS = !g_bShadowMapEnablePCSS;
				return 0;
			}
			case 'S': {
				g_bShadowMapEnable = !g_bShadowMapEnable;
				return 0;
			}
			case 'E': {
				g_bShadowMapHardwarePCF = !g_bShadowMapHardwarePCF;
				log_debug("[DBG] [SHW] g_bShadowMapHardwarePCF: %d", g_bShadowMapHardwarePCF);
				return 0;
			}

			case 'F': {
				CycleFOVSetting();
				return 0;
			}
			case 'W': {
				// DEBUG: Toggle keyboard joystick emulation
				if (g_config.KbdSensitivity > 0.0f)
					g_config.KbdSensitivity = 0.0f;
				else
					g_config.KbdSensitivity = 1.0f;
				log_debug("[DBG] Keyboard enabled: %d", (bool)g_config.KbdSensitivity);
				return 0;
			}
			case 'V':
			{
				//g_bEnableSteamVR_QPC = !g_bEnableSteamVR_QPC;
				//log_debug("[DBG] [QPC] g_bEnableSteamVR_QPC: %d", g_bEnableSteamVR_QPC);
				g_bTriggerReticleCapture = true;
				g_bCustomFOVApplied = false; // Force reapplication/recomputation of FOV
				return 0;
			}

			// Ctrl+L is the landing gear

			// Ctrl+P SteamVR screenshot (doesn't seem to work terribly well, though...)
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

			// Ctrl + Up
			case VK_UP:
				switch (g_KeySet) {
				case 1:
					g_LightVector[0].z += 0.1f;
					g_LightVector[0].normalize();
					PrintVector(g_LightVector[0]);
					break;
				case 2:
					IncreaseScreenScale(0.1f);
					//IncreaseSMZFactor(0.025f); // DEBUG, remove this later
					SaveVRParams();
					break;
				case 3:
					g_LaserPointDebug.z += 0.1f;
					log_debug("[DBG] g_LaserPointDebug: %0.3f, %0.3f, %0.3f",
						g_LaserPointDebug.x, g_LaserPointDebug.y, g_LaserPointDebug.z);
					break;
				case 6:
					g_fShadowMapScale += 0.1f;
					log_debug("[DBG] [SHW] g_fLightMapScale: %0.3f", g_fShadowMapScale);
					break;
				case 7:
					g_fHDRWhitePoint *= 2.0f;
					log_debug("[DBG] white point: %0.3f", g_fHDRWhitePoint);
					break;
				case 8:
					g_fOBJ_Z_MetricMult += 5.0f;
					log_debug("[DBG] g_fOBJMetricMult: %0.3f", g_fOBJ_Z_MetricMult);
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
					//IncreaseSMZFactor(-0.025f); // DEBUG, remove this later
					SaveVRParams();
					break;
				case 3:
					g_LaserPointDebug.z -= 0.1f;
					log_debug("[DBG] g_LaserPointDebug: %0.3f, %0.3f, %0.3f",
						g_LaserPointDebug.x, g_LaserPointDebug.y, g_LaserPointDebug.z);
					break;
				case 6:
					g_fShadowMapScale -= 0.1f;
					if (g_fShadowMapScale < 0.1f) g_fShadowMapScale = 0.2f;
					log_debug("[DBG] [SHW] g_fLightMapScale: %0.3f", g_fShadowMapScale);
					break;
				case 7:
					g_fHDRWhitePoint /= 2.0f;
					if (g_fHDRWhitePoint < 0.125f)
						g_fHDRWhitePoint = 0.125f;
					log_debug("[DBG] white point: %0.3f", g_fHDRWhitePoint);
					break;
				case 8:
					g_fOBJ_Z_MetricMult -= 5.0f;
					log_debug("[DBG] g_fOBJMetricMult: %0.3f", g_fOBJ_Z_MetricMult);
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
				g_fShadowMapAngleX = 0.0f;
				g_fShadowMapAngleY = 0.0f;
				g_fShadowMapScale = 1.0f;
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
	RECT rect;
	g_ThisWindow = hwnd;
	OldWindowProc = (WNDPROC )SetWindowLong(g_ThisWindow, GWL_WNDPROC, (LONG )MyWindowProc);
	if (OldWindowProc != NULL) {
		// SetProcessDPIAware(); // Reimar suggested using this to fix the DPI
		GetWindowRect(g_ThisWindow, &rect);
		log_debug("[DBG] RECT size: (%d, %d)-(%d, %d)", rect.left, rect.top, rect.right, rect.bottom);
		g_WindowWidth = rect.right - rect.left;
		g_WindowHeight = rect.bottom - rect.top;
		return true;
	}
	
	return false;
}

#include "XwaDrawTextHook.h"
#include "XwaDrawRadarHook.h"
#include "XwaDrawBracketHook.h"

bool IsXwaExe()
{
	char filename[4096];

	if (GetModuleFileName(nullptr, filename, sizeof(filename)) == 0)
	{
		return false;
	}

	int length = strlen(filename);

	if (length < 17)
	{
		return false;
	}

	return _stricmp(filename + length - 17, "xwingalliance.exe") == 0;
}

void PatchWithValue(uint32_t address, unsigned char value, int size) {
	DWORD old, dummy;
	BOOL res;
	res = VirtualProtect((void *)address, 1, PAGE_READWRITE, &old);
	if (res) {
		//log_debug("[DBG] Patching address 0x%x", address);
		memset((unsigned char *)address, value, size);
		VirtualProtect((void *)address, 1, old, &dummy);
	}
	else
		log_debug("[DBG] Could not patch address 0x%x", address);
}

void LoadPOVOffsets() {
	log_debug("[DBG] [POV] Loading POVOffsets.cfg...");
	FILE *file;
	char buf[160], param[80], svalue[80];
	float fValue;
	int slot, x, y, z, num_params, entries_applied = 0, error = 0;
	const char *CraftTableBase = (char *)0x5BB480;
	const int16_t EntrySize = 0x3DB, POVOffset = 0x238;
	// 0x5BB480 + (n-1) * 0x3DB + 0x238

	// POV Offsets will only be applied if VR is enabled
	if (!g_bEnableVR)
		return;

	try {
		error = fopen_s(&file, "./POVOffsets.cfg", "rt");
	}
	catch (...) {
		log_debug("[DBG] [POV] Could not load POVOffsets.cfg");
	}

	if (error != 0) {
		log_debug("[DBG] [POV] Error %d when loading POVOffsets.cfg", error);
		return;
	}

	while (fgets(buf, 160, file) != NULL) {
		// Skip comments and blank lines
		if (buf[0] == ';' || buf[0] == '#')
			continue;
		if (strlen(buf) == 0)
			continue;

		if (strstr(buf, "=") != NULL) {
			if (sscanf_s(buf, "%s = %s", param, 80, svalue, 80) > 0) {
				fValue = (float)atof(svalue);
				if (_stricmp(param, "apply_custom_VR_POVs") == 0) {
					bool bValue = (bool)fValue;
					log_debug("[DBG] [POV] POVOffsets Enabled: %d", bValue);
					if (!bValue) 
					{
						log_debug("[DBG] [POV] POVOffsets will NOT be applied");
						goto out;
					}
				}
			}
		}
		else {
			num_params = sscanf_s(buf, "%d %d %d %d", &slot, &x, &z, &y);
			if (num_params == 4) {
				int16_t *pov = (int16_t *)(CraftTableBase + (slot - 1) * EntrySize + POVOffset);
				// Y, Z, X
				*pov += y; pov++;
				*pov += z; pov++;
				*pov += x;
				entries_applied++;
			}
		}
	}

out:
	fclose(file);
	log_debug("[DBG] [POV] %d POV entries modified", entries_applied);
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
		
		if (IsXwaExe())
		{
			if (g_config.Text2DRendererEnabled) 
			{
				// RenderCharHook
				*(unsigned char*)(0x00450A47 + 0x00) = 0xE8;
				*(int*)(0x00450A47 + 0x01) = (int)RenderCharHook - (0x00450A47 + 0x05);

				// ComputeMetricsHook
				*(unsigned char*)(0x00510385 + 0x00) = 0xE8;
				*(int*)(0x00510385 + 0x01) = (int)ComputeMetricsHook - (0x00510385 + 0x05);
			}
		
			if (g_config.Radar2DRendererEnabled) 
			{
				// DrawRadarHook
				*(int*)(0x00434977 + 0x06) = (int)DrawRadarHook;
				*(int*)(0x00434995 + 0x06) = (int)DrawRadarSelectedHook;

				// This hook will need more work in VR mode (back-project coords into 3D and then project again
				// into 2D twice -- once for each eye -- then select the left/right offscreen buffers...); for now,
				// let's only enable this hook if we didn't start in VR mode:
				if (!g_bEnableVR) {
					// DrawBracketInFlightHook
					*(unsigned char*)(0x00503D46 + 0x00) = 0xE8;
					*(int*)(0x00503D46 + 0x01) = (int)DrawBracketInFlightHook - (0x00503D46 + 0x05);

					// DrawBracketInFlightHook CMD
					*(unsigned char*)(0x00478E44 + 0x00) = 0xE8;
					*(int*)(0x00478E44 + 0x01) = (int)DrawBracketInFlightHook - (0x00478E44 + 0x05);
				}
				// DrawBracketMapHook
				*(unsigned char*)(0x00503CFE + 0x00) = 0xE8;
				*(int*)(0x00503CFE + 0x01) = (int)DrawBracketMapHook - (0x00503CFE + 0x05);
			}

			// Remove the text next to the triangle pointer
			// At offset 072B4A, replace BF48BD6800 with 9090909090.
			// Offset 072B4A corresponds to address 0047374A
			if (!g_config.TriangleTextEnabled)
			{
				DWORD old, dummy;
				BOOL res;
				res = VirtualProtect((void *)0x047374A, 5, PAGE_READWRITE, &old);
				if (res) {
					log_debug("[DBG] Patching address 0x047374A");
					//memset((unsigned char *)0x047374A, 0x90, 5);
					unsigned char *ptr = (unsigned char *)0x047374A;
					for (int i = 0; i < 5; i++) {
						//log_debug("[DBG] 0x%x", *(ptr + i));
						*(ptr + i) = 0x90; // NOP
					}
					if (VirtualProtect((void *)0x047374A, 5, old, &dummy)) {
						log_debug("[DBG] Address 0x047374A patched!");
						if (FlushInstructionCache(GetModuleHandle(NULL), NULL, 0))
							log_debug("[DBG] Instructions flushed");
					}
				}
				else
					log_debug("[DBG] Could not get address 0x047374A");
			}

			// Remove the triangle pointer completely:
			// At offset 06B970 (address 0046C570), replace 83 with C3.
			if (!g_config.TrianglePointerEnabled) {
				DWORD old, dummy;
				BOOL res;
				res = VirtualProtect((void *)0x046C570, 1, PAGE_READWRITE, &old);
				if (res) {
					log_debug("[DBG] Patching address 0x046C570");
					//memset((unsigned char *)0x046C570, 0xC3, 1);
					unsigned char *ptr = (unsigned char *)0x046C570;
					for (int i = 0; i < 1; i++) {
						*(ptr + i) = 0xC3; // RET (?)
					}
					if (VirtualProtect((void *)0x046C570, 1, old, &dummy)) {
						log_debug("[DBG] Address 0x046C570 patched!");
						if (FlushInstructionCache(GetModuleHandle(NULL), NULL, 0))
							log_debug("[DBG] Instructions flushed");
					}
				}
				else
					log_debug("[DBG] Could not get address 0x046C570");
			}

			/*
				Remove the text next to the triangle pointer; but leave the triangle:

				// disable text
				At offset 06BE26 (address 0046CA26), replace E88585FCFF with 9090909090.
				At offset 06C092 (address 0046CC92), replace E81983FCFF with 9090909090.
				At offset 06C515 (address 0046D115), replace E8967EFCFF with 9090909090.

				// disable text digits
				At offset 06C0D4 (address 0046CCD4), replace E84785FCFF with 9090909090.
				At offset 06C10C (address 0046CD0C), replace E80F85FCFF with 9090909090.
				At offset 06C563 (address 0046D163), replace E8B880FCFF with 9090909090.
				At offset 06C59B (address 0046D19B), replace E88080FCFF with 9090909090.

				// disable dot
				At offset 06C0EB (address 0046CCEB), replace FF15B09D7C00 with 909090909090.
				At offset 06C57A (address 0046D17A), replace FF15B09D7C00 with 909090909090.
			*/
			if (g_config.SimplifiedTrianglePointer) {
				log_debug("[DBG] Applying SimplifiedTrianglePointer patch");
				// At offset 072B4A(address 0047374A), replace BF48BD6800 with 90 90 90 90 90. (5 bytes)
				//PatchWithValue(0x047374A, 0x90, 5); <-- This will remove the triangle; but also the FG name in the target box!

				// Disable Text
				// At offset 06BE26(address 0046CA26), replace E88585FCFF with 90 90 90 90 90. (5 bytes)
				PatchWithValue(0x046CA26, 0x90, 5);
				// At offset 06C092(address 0046CC92), replace E81983FCFF with 9090909090.
				PatchWithValue(0x046CC92, 0x90, 5);
				// At offset 06C515(address 0046D115), replace E8967EFCFF with 9090909090.
				PatchWithValue(0x046D115, 0x90, 5);

				// Disable text digits
				// At offset 06C0D4(address 0046CCD4), replace E84785FCFF with 90 90 90 90 90.
				PatchWithValue(0x046CCD4, 0x90, 5);
				// At offset 06C10C(address 0046CD0C), replace E80F85FCFF with 90 90 90 90 90.
				PatchWithValue(0x046CD0C, 0x90, 5);
				// At offset 06C563(address 0046D163), replace E8B880FCFF with 90 90 90 90 90.
				PatchWithValue(0x046D163, 0x90, 5);
				// At offset 06C59B(address 0046D19B), replace E88080FCFF with 90 90 90 90 90.
				PatchWithValue(0x046D19B, 0x90, 5);

				// disable dot
				// At offset 06C0EB(address 0046CCEB), replace FF15B09D7C00 with 90 90 90 90 90 90. (6 bytes)
				PatchWithValue(0x046CCEB, 0x90, 6);
				// At offset 06C57A(address 0046D17A), replace FF15B09D7C00 with 90 90 90 90 90 90.
				PatchWithValue(0x046D17A, 0x90, 6);
				// Flush the instruction cache
				if (FlushInstructionCache(GetModuleHandle(NULL), NULL, 0))
					log_debug("[DBG] Instructions flushed");
			}

			if (g_config.MusicSyncFix)
			{
				/*
					// Patch to fix the music when ProcessAffinityCore = 0
					At offset 191F44, replace 0F84 with 90E9.
					At offset 192015, replace 75 with EB.
				*/
				uint32_t BASE_ADDR = 0x400C00;
				// At offset 191F44, replace 0F84 with 90E9.
				//log_debug("[DBG] [PATCH] Before: 0x191F44: %X%X", *(uint8_t *)(BASE_ADDR + 0x191F44), *(uint8_t *)(BASE_ADDR + 0x191F44 + 1));
				//log_debug("[DBG] [PATCH] Before: 0x192015: %X", *(uint8_t *)(BASE_ADDR + 0x192015));
				
				PatchWithValue(BASE_ADDR + 0x191F44, 0x90, 1);
				PatchWithValue(BASE_ADDR + 0x191F44 + 1, 0xE9, 1);
				// At offset 192015, replace 75 with EB.
				PatchWithValue(BASE_ADDR + 0x192015, 0xEB, 1);
				log_debug("[DBG] [PATCH] Music Sync Fix Applied");
				//log_debug("[DBG] [PATCH] After: 0x191F44: %X%X", *(uint8_t *)(BASE_ADDR + 0x191F44), *(uint8_t *)(BASE_ADDR + 0x191F44 + 1));
				//log_debug("[DBG] [PATCH] After: 0x192015: %X", *(uint8_t *)(BASE_ADDR + 0x192015));
			}

			{
				//short *POV_Y0 = (short *)(0x5BB480 + 0x238); // = 0x5BB6B8, 0x5BB480 + 0x32 = Craft name
				//short *POV_Z0 = (short *)(0x5BB480 + 0x23A);
				//short *POV_X0 = (short *)(0x5BB480 + 0x23C);
				//log_debug("[DBG] [POV] X,Z,Y: %d, %d, %d", *POV_X0, *POV_Z0, *POV_Y0);
				//*POV_Z0 += 10; // Moves the POV up
				//*POV_Y0 += 5;  // Moves the POV forward
				//*POV_Y0 += 10; // This makes the X-Wing look good in VR
				LoadPOVOffsets();

				// DEBUG
				/*
				const char *CraftTableBase = (char *)0x5BB480;
				const int16_t EntrySize = 0x3DB, POVOffset = 0x238;
				for (int i = 1; i <= 3; i++) {
					int16_t *pov = (int16_t *)(CraftTableBase + (i - 1) * EntrySize + POVOffset);
					log_debug("[DBG] [POV] (%d): %d, %d, %d",
						i, *pov, *(pov+1), *(pov+2));
				}
				*/
			}
		}
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
		exit(0);
		break;
	}

	return TRUE;
}
