#pragma once

#include "EffectsCommon.h"
#include "DynamicCockpit.h"
#include "SteamVR.h"
#include <wincodec.h>


// ACTIVE COCKPIT
#define MAX_AC_COORDS_PER_TEXTURE 64
#define MAX_AC_TEXTURES_PER_COCKPIT 16
#define MAX_AC_ACTION_LEN 8 // WORDs (scan codes) used to specify an action
#define AC_HOLOGRAM_FAKE_VK_CODE 0x01 // Internal AC code to toggle the holograms
#define AC_JOYBUTTON1_FAKE_VK_CODE 0x02 // Internal AC code to emulate joystick button clicks
#define AC_JOYBUTTON2_FAKE_VK_CODE 0x03
#define AC_JOYBUTTON3_FAKE_VK_CODE 0x04
#define AC_JOYBUTTON4_FAKE_VK_CODE 0x05
#define AC_JOYBUTTON5_FAKE_VK_CODE 0x06

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

struct ACJoyEmulSettings
{
	bool  joystickEnabled;
	bool  throttleEnabled;
	// Internally, the left-hand index is always 0 and the right hand is at index 1
	// We use these indices to enable handedness:
	int   joyHandIdx;
	int   thrHandIdx;
	float joyHalfRangeX;
	float joyHalfRangeZ;
	float deadZonePerc;
	float thrHalfRange;

	ACJoyEmulSettings()
	{
		joystickEnabled = false;
		throttleEnabled = true;

		joyHandIdx    = 1;
		thrHandIdx    = 0;

		joyHalfRangeX = 0.075f;
		joyHalfRangeZ = 0.075f;
		deadZonePerc  = 0.1f;
		thrHalfRange  = 0.07f;
	}
};
extern ACJoyEmulSettings g_ACJoyEmul;

struct ACJoyMapping
{
	WORD action[VRButtons::MAX][MAX_AC_ACTION_LEN];
};
extern ACJoyMapping g_ACJoyMappings[2];

// Used to tell which controller and button are used in VR for activating AC controls
struct ACPointerData
{
	int contIdx;
	int button;
	float mouseSpeedX;
	float mouseSpeedY;

	ACPointerData()
	{
		contIdx = 0; // Left controller
		button  = VRButtons::TRIGGER;
		mouseSpeedX = 3.0f;
		mouseSpeedY = 3.0f;
	}
};
extern ACPointerData g_ACPointerData;

// DEBUG vars
extern Vector3 g_debug_v0, g_debug_v1, g_debug_v2;
extern bool g_bDumpLaserPointerDebugInfo;
extern Vector3 g_LPdebugPoint;
extern float g_fLPdebugPointOffset;
// DEBUG vars

bool LoadIndividualACParams(char* sFileName);
void CockpitNameToACParamsFile(char* CockpitName, char* sFileName, int iFileNameSize);
void TranslateACAction(WORD* scanCodes, char* action, bool* bIsACActivator);
void DisplayACAction(WORD* scanCodes);
int isInVector(char* name, ac_element* ac_elements, int num_elems);
void ACRunAction(WORD* action, struct joyinfoex_tag* pji = nullptr);
bool IsContinousAction(WORD* action);
