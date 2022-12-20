// Copyright (c) 2016-2018 Reimar Döffinger
// Licensed under the MIT license. See LICENSE.txt

#include "common.h"
#include "config.h"

#include <mmsystem.h>
#include <xinput.h>

#include "XWAObject.h"
#include "SharedMem.h"
#include "joystick.h"

extern PlayerDataEntry *PlayerDataTable;
extern uint32_t *g_playerIndex;

// Gimbal Lock Fix
bool g_bEnableGimbalLockFix = true, g_bGimbalLockActive = false, g_bGimbalLockDebugMode = false;

// How much roll is applied when the ship is doing yaw. Set this to 0
// to have a completely flat yaw, as in the YT-series ships.
float g_fRollFromYawScale = -0.8f;

// Acceleration and deceleration rates in degrees per second (this is the 2nd derivative)
float g_fYawAccelRate_s   = 10.0f;
float g_fPitchAccelRate_s = 10.0f;
float g_fRollAccelRate_s  = 10.0f;

// The maximum turn rate (in degrees per second) for each axis (1st derivative)
float g_fMaxYawRate_s   = 70.0f;
float g_fMaxPitchRate_s = 70.0f;
float g_fMaxRollRate_s  = 100.0f;

float g_fTurnRateScaleThr_0   = 0.3f;
float g_fTurnRateScaleThr_100 = 0.6f;
float g_fMaxTurnAccelRate_s   = 3.0f;

// The normalized [-1..1] rudder read from the joystick
float g_fJoystickRudder = 0.0f;

int g_iMouseCenterX = 256, g_iMouseCenterY = 256;
float g_fMouseRangeX = 256.0f, g_fMouseRangeY = 256.0f;

extern bool g_bRendering3D;

#pragma comment(lib, "winmm")
#pragma comment(lib, "XInput9_1_0")

#undef min
#undef max
#include <algorithm>

inline float clamp(float val, float min, float max)
{
	if (val < min) val = min;
	if (val > max) val = max;
	return val;
}

// timeGetTime emulation.
// if it is called in a tight loop it will call Sleep()
// prevents high CPU usage due to busy loop
DWORD emulGetTime()
{
	static DWORD oldtime;
	static DWORD count;
	DWORD time = timeGetTime();
	if (time != oldtime)
	{
		oldtime = time;
		count = 0;
	}
	// Trigger value and sleep value derived by trial and error.
	// Trigger values down to 10 with sleep value 8 do not seem to affect performance
	// even at 60 FPS, and trigger values up to 1000 with sleep value 1 still seem effective
	// to reduce CPU load on fast modern computers.
	if (++count >= 20)
	{
		Sleep(2);
		time = timeGetTime();
		count = 0;
	}
	return time;
}

static int needsJoyEmul()
{
	JOYCAPS caps = {};
	if (joyGetDevCaps(0, &caps, sizeof(caps)) != JOYERR_NOERROR ||
	    !(caps.wCaps & JOYCAPS_HASZ) || caps.wNumAxes <= 2 ||
	    // Assume for now that wMid == 0x45e means "Gamepad"
	    // Steam controller: pid == 0x28e
	    // Xbox controller: pid == 0x2ff
	    caps.wMid == 0x45e)
	{
		// Probably the joystick is just an emulation from a gamepad.
		// Rather try to use it as gamepad directly then.
		XINPUT_STATE state;
		if (XInputGetState(0, &state) == ERROR_SUCCESS) return 2;
	}
	UINT cnt = joyGetNumDevs();
	for (unsigned i = 0; i < cnt; ++i)
	{
		JOYINFOEX jie;
		memset(&jie, 0, sizeof(jie));
		jie.dwSize = sizeof(jie);
		jie.dwFlags = JOY_RETURNALL;
		UINT res = joyGetPosEx(0, &jie);
		if (res == JOYERR_NOERROR)
			return 0;
	}
	return 1;
}

UINT WINAPI emulJoyGetNumDevs(void)
{
	if (g_config.JoystickEmul < 0) {
		g_config.JoystickEmul = needsJoyEmul();
	}
	if (!g_config.JoystickEmul) {
		return joyGetNumDevs();
	}
	return 1;
}

static UINT joyYmax, joyZmax, joyZmin;

UINT WINAPI emulJoyGetDevCaps(UINT_PTR joy, struct tagJOYCAPSA *pjc, UINT size)
{
	if (!g_config.JoystickEmul) {
		UINT res = joyGetDevCaps(joy, pjc, size);
		if (g_config.InvertYAxis && joy == 0 && pjc && size == 0x194) joyYmax = pjc->wYmax;
		if (joy == 0 && pjc && size == 0x194) {
			joyZmax = pjc->wZmax;
			joyZmin = pjc->wZmin;
			if (joyZmin > joyZmax) {
				UINT temp = joyZmin;
				joyZmin = joyZmax;
				joyZmax = temp;
			}
		}
		return res;
	}
	if (joy != 0) return MMSYSERR_NODRIVER;
	if (size != 0x194) return MMSYSERR_INVALPARAM;
	memset(pjc, 0, size);
	if (g_config.JoystickEmul == 2) {
		pjc->wXmax = 65536;
		pjc->wYmax = 65535;
		pjc->wZmax = 255;
		pjc->wRmax = 65536;
		pjc->wUmax = 65536;
		pjc->wVmax = 255;
		pjc->wNumButtons = 14;
		pjc->wMaxButtons = 14;
		pjc->wNumAxes = 6;
		pjc->wMaxAxes = 6;
		pjc->wCaps = JOYCAPS_HASZ | JOYCAPS_HASR | JOYCAPS_HASU | JOYCAPS_HASV | JOYCAPS_HASPOV | JOYCAPS_POV4DIR;
		return JOYERR_NOERROR;
	}
	pjc->wXmax = 512;
	pjc->wYmax = 512;
	pjc->wZmax = 512;
	pjc->wRmax = 512;
	pjc->wNumButtons = 5;
	pjc->wMaxButtons = 5;
	// wNumAxes should probably stay at 2 here because needsJoyEmul() compares against
	// 2 num axes to decide whether or not to use XInput.
	pjc->wNumAxes = 2;
	pjc->wMaxAxes = 2;
	return JOYERR_NOERROR;
}

static DWORD lastGetPos;

static const DWORD povmap[16] = {
	JOY_POVCENTERED, // nothing
	JOY_POVFORWARD, // DPAD_UP
	JOY_POVBACKWARD, // DPAD_DOWN
	JOY_POVCENTERED, // up and down
	JOY_POVLEFT, // DPAD_LEFT
	(270 + 45) * 100, // left and up
	(180 + 45) * 100, // down and left
	JOY_POVLEFT, // left and up and down
	JOY_POVRIGHT, // DPAD_RIGHT
	45 * 100, // up and right
	(90 + 45) * 100, // right and down
	JOY_POVRIGHT, // right and up and down

	// As from start, but  with both right and left pressed
	JOY_POVCENTERED, // nothing
	JOY_POVFORWARD, // DPAD_UP
	JOY_POVBACKWARD, // DPAD_DOWN
	JOY_POVCENTERED, // up and down
};

UINT WINAPI emulJoyGetPosEx(UINT joy, struct joyinfoex_tag *pji)
{
	if (!g_config.JoystickEmul) {
		UINT res = joyGetPosEx(joy, pji);
		if (g_config.InvertYAxis && joyYmax > 0) pji->dwYpos = joyYmax - pji->dwYpos;

		// Only swap the X-Z joystick axes if the flag is set and we're not in the gunner turret
		if (!PlayerDataTable[*g_playerIndex].gunnerTurretActive && g_config.SwapJoystickXZAxes) {
			DWORD X = pji->dwXpos;
			pji->dwXpos = pji->dwRpos;
			pji->dwRpos = X;
		}

		if (g_config.InvertThrottle) pji->dwZpos = (joyZmax - pji->dwZpos) + joyZmin;
		// WARNING: Do not use these when the Joystick hook is present
		// dwXpos: x-axis goes from 0 (left) to 32767 (center) to 65535 (right)
		// dwYpos: y-axis goes from 0 (up)   to 32767 (center) to 65535 (down)
		// dwRpos: z-axis goes from 0 (left) to 32767 (center) to 65535 (right)
		//log_debug("[DBG] dwXpos: %d, dwYpos: %d, dwZpos: %d, dwRpos: %d",
		//	pji->dwXpos, pji->dwYpos, pji->dwZpos, pji->dwRpos);
		if (g_bGimbalLockActive)
		{
			if (g_pSharedDataJoystick != NULL) {
				float normYaw   = 2.0f * (pji->dwXpos / 65535.0f - 0.5f);
				float normPitch = 2.0f * (pji->dwYpos / 65535.0f - 0.5f);
				float normRoll  = 2.0f * (pji->dwRpos / 65535.0f - 0.5f);
				g_pSharedDataJoystick->JoystickYaw = normYaw;
				g_pSharedDataJoystick->JoystickPitch = normPitch;
				g_fJoystickRudder = normRoll;
			}
			// Nullify the joystick input
			pji->dwXpos = 32767;
			pji->dwYpos = 32767;
			pji->dwRpos = 32767;
		}
		return res;
	}

	if (joy != 0) return MMSYSERR_NODRIVER;
	if (pji->dwSize != 0x34) return MMSYSERR_INVALPARAM;
	// XInput
	if (g_config.JoystickEmul == 2) {
		XINPUT_STATE state;
		XInputGetState(0, &state);
		pji->dwFlags = JOY_RETURNALL;
		pji->dwXpos = state.Gamepad.sThumbLX + 32768;
		pji->dwYpos = state.Gamepad.sThumbLY + 32768;
		if (!g_config.InvertYAxis) pji->dwYpos = 65536 - pji->dwYpos;
		// The 65536 value breaks XWA with in-game invert Y axis option
		pji->dwYpos = std::min(pji->dwYpos, DWORD(65535));
		if (g_config.XInputTriggerAsThrottle)
		{
			pji->dwZpos = g_config.XInputTriggerAsThrottle & 1 ? state.Gamepad.bLeftTrigger : state.Gamepad.bRightTrigger;
		}
		pji->dwRpos = state.Gamepad.sThumbRX + 32768;
		pji->dwUpos = state.Gamepad.sThumbRY + 32768;
		pji->dwVpos = state.Gamepad.bLeftTrigger;
		pji->dwButtons = 0;
		// Order matches XBox One controller joystick emulation default
		// A, B, X, Y first
		pji->dwButtons |= (state.Gamepad.wButtons & 0xf000) >> 12;
		// Shoulder buttons next
		pji->dwButtons |= (state.Gamepad.wButtons & 0x300) >> 4;
		// start and back, for some reason in flipped order compared to XINPUT
		pji->dwButtons |= (state.Gamepad.wButtons & 0x10) << 3;
		pji->dwButtons |= (state.Gamepad.wButtons & 0x20) << 1;
		// Thumb buttons
		pji->dwButtons |= (state.Gamepad.wButtons & 0xc0) << 2;
		// Triggers last, they are not mapped in the joystick emulation
		if (g_config.XInputTriggerAsThrottle != 1 &&
			state.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) pji->dwButtons |= 0x400;
		if (g_config.XInputTriggerAsThrottle != 2 &&
			state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) pji->dwButtons |= 0x800;
		// These are not defined by XINPUT, and can't be remapped by the
		// XWA user interface as they will be buttons above 12, but map them just in case
		pji->dwButtons |= (state.Gamepad.wButtons & 0xc00) << 2;

		// XWA can map only 12 buttons, so map dpad to POV
		pji->dwPOV = povmap[state.Gamepad.wButtons & 15];

		pji->dwButtonNumber = 0;
		for (int i = 0; i < 32; i++)
		{
			if ((pji->dwButtons >> i) & 1) ++pji->dwButtonNumber;
		}
		return JOYERR_NOERROR;
	}
	DWORD now = GetTickCount();
	// Assume we started a new game
	if ((now - lastGetPos) > 5000)
	{
		SetCursorPos(g_iMouseCenterX, g_iMouseCenterY);
		GetAsyncKeyState(VK_LBUTTON);
		GetAsyncKeyState(VK_RBUTTON);
		GetAsyncKeyState(VK_MBUTTON);
		GetAsyncKeyState(VK_XBUTTON1);
		GetAsyncKeyState(VK_XBUTTON2);
	}
	lastGetPos = now;
	POINT pos;
	GetCursorPos(&pos);

	pji->dwXpos = 256; // This is the center position for this axis
	pji->dwYpos = 256;
	pji->dwZpos = 256;
	pji->dwRpos = 256;

	// Mouse input
	{
		if (!g_bGimbalLockActive && !g_bGimbalLockDebugMode)
		{
			// This is the original code by Reimar
			pji->dwXpos = static_cast<DWORD>(std::min(256.0f + (pos.x - 240.0f) * g_config.MouseSensitivity, 512.0f));
			pji->dwYpos = static_cast<DWORD>(std::min(256.0f + (pos.y - 240.0f) * g_config.MouseSensitivity, 512.0f));
		}
		else if (g_bRendering3D || g_bGimbalLockDebugMode)
		{
			const float deltaX = clamp((pos.x - g_iMouseCenterX) * g_config.MouseSensitivity / g_fMouseRangeX, -1.0f, 1.0f);
			const float deltaY = clamp((pos.y - g_iMouseCenterY) * g_config.MouseSensitivity / g_fMouseRangeY, -1.0f, 1.0f);
			SetCursorPos(g_iMouseCenterX, g_iMouseCenterY);
			pji->dwXpos = static_cast<DWORD>((deltaX + 1.0f) / 2.0f * 512.0f);
			pji->dwYpos = static_cast<DWORD>((deltaY + 1.0f) / 2.0f * 512.0f);

			if (g_bGimbalLockDebugMode) {
				log_debug("[DBG] Mouse pos: %d, %d, delta: %0.3f, %0.3f",
					pos.x, pos.y, deltaX, deltaY);
			}
		}

		pji->dwButtons = 0;
		pji->dwButtonNumber = 0;
		if (GetAsyncKeyState(VK_LBUTTON)) {
			pji->dwButtons |= 1;
			++pji->dwButtonNumber;
		}
		if (GetAsyncKeyState(VK_RBUTTON)) {
			pji->dwButtons |= 2;
			++pji->dwButtonNumber;
		}
		if (GetAsyncKeyState(VK_MBUTTON)) {
			pji->dwButtons |= 4;
			++pji->dwButtonNumber;
		}
		if (GetAsyncKeyState(VK_XBUTTON1)) {
			pji->dwButtons |= 8;
			++pji->dwButtonNumber;
		}
		if (GetAsyncKeyState(VK_XBUTTON2)) {
			pji->dwButtons |= 16;
			++pji->dwButtonNumber;
		}
	}

	/*
	bool AltKey = (GetAsyncKeyState(VK_MENU) & 0x8000) == 0x8000;
	pji->dwRpos = 256; // This is the center position for this axis
	if (AltKey) {
		//log_debug("[DBG] AltKey Pressed");
		if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
			pji->dwRpos = static_cast<DWORD>(std::max(256 - 256 * g_config.KbdSensitivity, 0.0f));
		}
		if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
			pji->dwRpos = static_cast<DWORD>(std::min(256 + 256 * g_config.KbdSensitivity, 512.0f));
		}
	}
	else {
		if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
			pji->dwXpos = static_cast<DWORD>(std::max(256 - 256 * g_config.KbdSensitivity, 0.0f));
		}
		if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
			pji->dwXpos = static_cast<DWORD>(std::min(256 + 256 * g_config.KbdSensitivity, 512.0f));
		}
	}
	log_debug("[DBG] dwXpos,dwRpos: %d, %d", pji->dwZpos, pji->dwRpos);
	*/
	
	// dwZpos is the throttle
	if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
		pji->dwXpos = static_cast<DWORD>(std::max(256 - 256 * g_config.KbdSensitivity, 0.0f));
	}
	if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
		pji->dwXpos = static_cast<DWORD>(std::min(256 + 256 * g_config.KbdSensitivity, 512.0f));
	}
	
	if (GetAsyncKeyState(VK_UP) & 0x8000) {
		pji->dwYpos = static_cast<DWORD>(std::max(256 - 256 * g_config.KbdSensitivity, 0.0f));
	}
	if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
		pji->dwYpos = static_cast<DWORD>(std::min(256 + 256 * g_config.KbdSensitivity, 512.0f));
	}
	if (g_config.InvertYAxis) pji->dwYpos = 512 - pji->dwYpos;
	// Normalize each axis to the range -1..1:
	float normYaw   = 2.0f * (pji->dwXpos / 512.0f - 0.5f);
	float normPitch = 2.0f * (pji->dwYpos / 512.0f - 0.5f);
	if (g_pSharedDataJoystick != NULL) {
		g_pSharedDataJoystick->JoystickYaw = normYaw;
		g_pSharedDataJoystick->JoystickPitch = normPitch;
	}

	if (g_bGimbalLockActive)
	{
		// Nullify the joystick input, but only if we're inside the cockpit
		// (that's what g_bGimbalLockActive does)
		pji->dwXpos = 256;
		pji->dwYpos = 256;
		pji->dwZpos = 256;
		pji->dwRpos = 256;
	}

	return JOYERR_NOERROR;
}
