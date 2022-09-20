#include "common.h"
#include "SharedMem.h"

void log_debug(const char *format, ...);

void InitSharedMem() {

	g_pSharedDataCockpitLook = g_SharedMemCockpitLook.GetMemoryPointer();
	if (g_pSharedDataCockpitLook == NULL)
		log_debug("[DBG][SharedMem] Could not load CockpitLook shared data ptr");

	g_pSharedDataTgSmush = g_SharedMemTgSmush.GetMemoryPointer();
	if (g_pSharedDataTgSmush == NULL)
		log_debug("[DBG][SharedMem] Could not load TgSmush shared data ptr");

	g_pSharedDataJoystick = g_SharedMemJoystick.GetMemoryPointer();
	if (g_pSharedDataTgSmush == NULL)
		log_debug("[DBG][SharedMem] Could not load Joystick shared data ptr");
}


SharedMemDataCockpitLook* g_pSharedDataCockpitLook = nullptr;
SharedMemDataTgSmush* g_pSharedDataTgSmush = nullptr;
SharedMemDataJoystick* g_pSharedDataJoystick = nullptr;

SharedMem<SharedMemDataCockpitLook> g_SharedMemCockpitLook(SHARED_MEM_NAME_COCKPITLOOK, true, false);
SharedMem<SharedMemDataTgSmush> g_SharedMemTgSmush(SHARED_MEM_NAME_TGSMUSH, true, true);
SharedMem<SharedMemDataJoystick> g_SharedMemJoystick(SHARED_MEM_NAME_JOYSTICK, true, false);