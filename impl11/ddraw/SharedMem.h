#pragma once

#include <windows.h>
#include "SharedMemTemplate.h"

struct SharedMemDataCockpitLook {
	// Offset added to the current POV when VR is active. This is controlled by ddraw
	float POVOffsetX, POVOffsetY, POVOffsetZ;
	// Euler angles, in degrees, for the current camera matrix coming from SteamVR. This is
	// written by CockpitLookHook:
	float Yaw, Pitch, Roll;
	// Positional tracking data. Written by CockpitLookHook:
	float X, Y, Z;
	// Flag to indicate that the reticle needs setup, to inhibit tracking and avoid roll messing with
	// the pips positions.
	// Set to 0 by ddraw when the game starts a mission (in OnSizeChanged())
	// Set to 1 by a hook in the SetupReticle() XWA function.
	int bIsReticleSetup;
};

struct SharedMemDataJoystick
{
	// Joystick's position, written by the joystick hook, or by the joystick emulation code.
	// These values are normalized in the range -1..1
	float JoystickYaw, JoystickPitch, JoystickRoll;
	// If the gimbal lock fix is active, the following will disable the joystick (default: false)
	// Set by ddraw
	bool GimbalLockFixActive;
	// Set by ddraw when joystick emulation is enabled
	bool JoystickEmulationEnabled;
	// Set by the joystick hook
	bool JoystickHookPresent;

	SharedMemDataJoystick() {
		JoystickYaw = 0.0f;
		JoystickPitch = 0.0f;
		JoystickRoll = 0.0f;
		GimbalLockFixActive = false;
		JoystickEmulationEnabled = false;
		JoystickHookPresent = false;
	}
};

struct SharedMemDataTgSmush
{
	int videoFrameIndex;
	int videoFrameWidth;
	int videoFrameHeight;
	int videoDataLength;
	char* videoDataPtr;
};

void InitSharedMem();

const auto SHARED_MEM_NAME_COCKPITLOOK = L"Local\\CockpitLookHook";
const auto SHARED_MEM_NAME_TGSMUSH = L"Local\\TgSmushVideo";
const auto SHARED_MEM_NAME_JOYSTICK = L"Local\\JoystickFfHook";

extern SharedMem<SharedMemDataCockpitLook> g_SharedMemCockpitLook;
extern SharedMem<SharedMemDataTgSmush> g_SharedMemTgSmush;
extern SharedMem<SharedMemDataJoystick> g_SharedMemJoystick;

extern SharedMemDataCockpitLook* g_pSharedDataCockpitLook;
extern SharedMemDataTgSmush* g_pSharedDataTgSmush;
extern SharedMemDataJoystick* g_pSharedDataJoystick;