#pragma once

#include "EffectsCommon.h"
#include "DynamicCockpit.h"
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

// ACTIVE COCKPIT
extern bool g_bActiveCockpitEnabled;
extern Vector4 g_contOriginWorldSpace; // This is the origin of the controller in 3D, in world-space coords
extern Vector4 g_controllerForwardVector; // Forward direction in the controller's frame of reference
extern Vector4 g_contDirWorldSpace; // This is the direction in which the controller is pointing in world-space coords
extern Vector3 g_LaserPointer3DIntersection;
extern float g_fBestIntersectionDistance;
extern int g_iBestIntersTexIdx; // The index into g_ACElements where the intersection occurred
extern bool g_bFreePIEControllerButtonDataAvailable;
extern ac_element g_ACElements[MAX_AC_TEXTURES_PER_COCKPIT];
extern int g_iLaserDirSelector;
extern int g_iNumACElements;
extern bool g_bACActionTriggered, g_bACLastTriggerState, g_bACTriggerState;
extern bool g_bFreePIEInitialized, g_bFreePIEControllerButtonDataAvailable;
extern float g_fLaserPointerLength;
extern int g_iFreePIESlot, g_iFreePIEControllerSlot;

// DEBUG vars
extern Vector3 g_debug_v0, g_debug_v1, g_debug_v2;
extern bool g_bDumpLaserPointerDebugInfo;
extern Vector3 g_LPdebugPoint;
extern float g_fLPdebugPointOffset;
// DEBUG vars

bool LoadIndividualACParams(char* sFileName);
void CockpitNameToACParamsFile(char* CockpitName, char* sFileName, int iFileNameSize);
void TranslateACAction(WORD* scanCodes, char* action);
void DisplayACAction(WORD* scanCodes);
int isInVector(char* name, ac_element* ac_elements, int num_elems);
