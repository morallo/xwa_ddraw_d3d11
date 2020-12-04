#pragma once

#include "dynamic_cockpit.h"
#include <wincodec.h>


// ACTIVE COCKPIT
#define MAX_AC_COORDS_PER_TEXTURE 64
#define MAX_AC_TEXTURES_PER_COCKPIT 16
#define MAX_AC_ACTION_LEN 8 // WORDs (scan codes) used to specify an action
#define AC_HOLOGRAM_FAKE_VK_CODE 0x01 // Internal AC code to toggle the holograms
typedef struct ac_uv_coords_struct {
	uvfloat4 area[MAX_AC_COORDS_PER_TEXTURE];
	WORD action[MAX_AC_COORDS_PER_TEXTURE][MAX_AC_ACTION_LEN]; // List of scan codes
	char action_name[MAX_AC_COORDS_PER_TEXTURE][16]; // For debug purposes only, remove later
	int numCoords;
} ac_uv_coords;

typedef struct ac_element_struct {
	ac_uv_coords coords;
	//int idx; // "Back pointer" into the g_ACElements array
	char name[MAX_TEXTURE_NAME];
	bool bActive, bNameHasBeenTested;
	short width, height; // DEBUG, remove later
} ac_element;

extern Vector4 g_contOriginWorldSpace; // This is the origin of the controller in 3D, in world-space coords
extern Vector4 g_contDirWorldSpace; // This is the direction in which the controller is pointing in world-space coords
extern Vector4 g_contOriginViewSpace; // This is the origin of the controller in 3D, in view-space coords
extern Vector4 g_contDirViewSpace; // The direction in which the controller is pointing, in view-space coords
extern Vector3 g_LaserPointer3DIntersection;
extern float g_fBestIntersectionDistance;
extern float g_fContMultiplierX, g_fContMultiplierY, g_fContMultiplierZ;
extern int g_iBestIntersTexIdx; // The index into g_ACElements where the intersection occurred
extern bool g_bActiveCockpitEnabled, g_bACActionTriggered, g_bACLastTriggerState, g_bACTriggerState;
extern bool g_bOriginFromHMD, g_bCompensateHMDRotation, g_bCompensateHMDPosition, g_bFullCockpitTest;
extern bool g_bFreePIEControllerButtonDataAvailable;
extern ac_element g_ACElements[MAX_AC_TEXTURES_PER_COCKPIT];
extern int g_iNumACElements, g_iLaserDirSelector;
// DEBUG vars
extern Vector3 g_debug_v0, g_debug_v1, g_debug_v2;
extern bool g_bDumpLaserPointerDebugInfo;
extern Vector3 g_LPdebugPoint;
extern float g_fLPdebugPointOffset;
// DEBUG vars

// ACTIVE COCKPIT
extern ac_element g_ACElements[MAX_AC_TEXTURES_PER_COCKPIT];
extern int g_iNumACElements;
extern bool g_bActiveCockpitEnabled;
bool LoadIndividualACParams(char* sFileName);
void CockpitNameToACParamsFile(char* CockpitName, char* sFileName, int iFileNameSize);
void TranslateACAction(WORD* scanCodes, char* action);
void DisplayACAction(WORD* scanCodes);
