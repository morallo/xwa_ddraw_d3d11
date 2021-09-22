#include "reticle.h"

#include "common.h"
#include <vector>

/*
hook_reticle mapping:
Reticle_5 = 5
Reticle_6 = 6
Reticle_7 = 7
Reticle_8 = 8
Reticle_9 = 9
Reticle_10 = 10
Reticle_15 = 15
Reticle_16 = 16
Reticle_17 = 17
Reticle_18 = 18
Reticle_19 = 19
Reticle_20 = 20
Reticle_21 = 21
Reticle_22 = 22
*/
std::vector<char*> Reticle_ResNames = {
	"dat,12000,500,",  // 0xdcb8e4f4, // Main Laser reticle.
	"dat,12000,600,",  // 0x0793c7d6, // Semi circles that indicate target is ready to be fired upon.
	"dat,12000,700,",  // 0xa4870ab3, // Main Warhead reticle.
	"dat,12000,800,",  // 0x756c8f81, // Warhead semi-circles that indicate lock is being acquired.
	"dat,12000,900,",  // 0x6acc3e3a, // Green dot for next laser available.
	"dat,12000,1000,", // 0x19f6f5a2, // Next laser available to fire.
	"dat,12000,1500,", // 0x1c5e0b86, // Laser warning indicator, left.
	"dat,12000,1600,", // 0xc54d8171, // Laser warning indicator, mid-left.
	"dat,12000,1700,", // 0xf4388255, // Laser warning indicator, mid-right.
	"dat,12000,1800,", // 0xee802582, // Laser warning indicator, right.
	"dat,12000,1900,", // 0x671e8041, // Warhead top indicator, left.
	"dat,12000,2000,", // 0x6cd5d81f, // Warhead top indicator, mid-left,right
	"dat,12000,2100,", // 0x6cd5d81f, // Warhead top indicator, mid-left,right
	"dat,12000,2200,", // 0xc33a94b3, // Warhead top indicator, right.
};

std::vector<char*> ReticleCenter_ResNames = {
	"dat,12000,500,",  // 0xdcb8e4f4, // Main Laser reticle.
	"dat,12000,700,",  // 0xa4870ab3, // Main Warhead reticle.
};

std::vector<char*> CustomReticleCenter_ResNames;
char g_sLaserLockedReticleResName[80] = { 0 };
std::vector<char*> CustomWLight_LL_ResNames;
std::vector<char*> CustomWLight_ML_ResNames;
std::vector<char*> CustomWLight_MR_ResNames;
std::vector<char*> CustomWLight_RR_ResNames;

/*
 Converts an index number as specified in the custom reticle files to a slot index
 as stored in HUD.dat.
*/
int ReticleIndexToHUDSlot(int ReticleIndex) {
	// I hate this stupid numbering system
	// The reticles start at 5151
	// Every 14 slots, the group number will increase
	// Every 100th slot, the group number will *also* increase, because, hey, why the hell not?
	// This leads to this very stupid and complex algorithm:
	int sub_slot = (ReticleIndex - 51) / 14;
	int super_slot = (ReticleIndex / 100);
	int slot_mod = (ReticleIndex % 100);
	int slot = 5100 + 100 * sub_slot + 100 * super_slot + slot_mod;
	log_debug("[DBG] [RET] index: %d, sub_slot: %d, super_slot: %d, slot_mod: %d slot: %d",
		ReticleIndex, sub_slot, super_slot, slot_mod, slot);
	return slot;
}

/*
 Converts an index number as specified in the custom reticle files to a HUD resname
 in the form "dat,12000,<slot>,"
*/
void ReticleIndexToHUDresname(int ReticleIndex, char* out_str, int out_str_len) {
	int slot = ReticleIndexToHUDSlot(ReticleIndex);
	sprintf_s(out_str, out_str_len, "dat,12000,%d,", slot);
}

void ClearCustomReticleCenterResNames() {
	for (char* res : CustomReticleCenter_ResNames) {
		if (res != NULL) {
			delete[] res;
			res = NULL;
		}
	}
	CustomReticleCenter_ResNames.clear();
	g_sLaserLockedReticleResName[0] = 0;
}

void ClearCustomWarningLightsResNames() {
	for (char* res : CustomWLight_LL_ResNames) {
		if (res != NULL) {
			delete[] res;
			res = NULL;
		}
	}
	CustomWLight_LL_ResNames.clear();
}

bool LoadReticleTXTFile(char* sFileName) {
	log_debug("[DBG] [RET] Loading Reticle Text file...");
	FILE* file;
	int error = 0;

	try {
		error = fopen_s(&file, sFileName, "rt");
	}
	catch (...) {
		log_debug("[DBG] [RET] Could not load %s", sFileName);
	}

	if (error != 0) {
		log_debug("[DBG] [RET] Error %d when loading %s", error, sFileName);
		return false;
	}

	// The reticle file exists, let's parse it
	char buf[160], param[80], svalue[80], * ResName;
	int param_read_count = 0;
	int iValue = 0;

	ClearCustomReticleCenterResNames();
	while (fgets(buf, 160, file) != NULL) {
		// Skip comments and blank lines
		if (buf[0] == ';')
			continue;
		if (strlen(buf) == 0)
			continue;

		if (sscanf_s(buf, "%s = %s", param, 80, svalue, 80) > 0) {
			iValue = atoi(svalue);
			if (_stricmp(param, "Reticle_5") == 0) { // Laser reticle
				log_debug("[DBG] [RET] Reticle_5: %d", iValue);
				ResName = new char[80];
				ReticleIndexToHUDresname(iValue, ResName, 80);
				CustomReticleCenter_ResNames.push_back(ResName);
				log_debug("[DBG] [RET] Custom Reticle Center: %s Added", ResName);
			}
			else if (_stricmp(param, "Reticle_6") == 0) { // Highlighted laser reticle ("locked" laser)
				log_debug("[DBG] [RET] Reticle_6: %d", iValue);
				ResName = new char[80];
				ReticleIndexToHUDresname(iValue, ResName, 80);
				strcpy_s(g_sLaserLockedReticleResName, 80, ResName);
				log_debug("[DBG] [RET] Laser Highlight Reticle Center: %s", ResName);
			}
			else if (_stricmp(param, "Reticle_7") == 0) { // Warhead reticle
				log_debug("[DBG] [RET] Reticle_7: %d", iValue);
				ResName = new char[80];
				ReticleIndexToHUDresname(iValue, ResName, 80);
				CustomReticleCenter_ResNames.push_back(ResName);
				log_debug("[DBG] [RET] Custom Warhead Reticle Center: %s Added", ResName);
			}
			// Warning Lights are indices 15,16,17,18 for the regular reticle and
			// indices 19,20,21,22 for the warhead reticle
			// Left-Left Warning Lights
			else if (_stricmp(param, "Reticle_15") == 0 || _stricmp(param, "Reticle_19") == 0) { // Warhead reticle
				log_debug("[DBG] [RET] Reticle_15|19: %d", iValue);
				ResName = new char[80];
				ReticleIndexToHUDresname(iValue, ResName, 80);
				CustomWLight_LL_ResNames.push_back(ResName);
				log_debug("[DBG] [RET] Custom WLight LL: %s Added", ResName);
			}
			// Mid-Left Warning Lights
			else if (_stricmp(param, "Reticle_16") == 0 || _stricmp(param, "Reticle_20") == 0) { // Warhead reticle
				log_debug("[DBG] [RET] Reticle_16|20: %d", iValue);
				ResName = new char[80];
				ReticleIndexToHUDresname(iValue, ResName, 80);
				CustomWLight_ML_ResNames.push_back(ResName);
				log_debug("[DBG] [RET] Custom WLight ML: %s Added", ResName);
			}
			// Mid-Right Warning Lights
			else if (_stricmp(param, "Reticle_17") == 0 || _stricmp(param, "Reticle_21") == 0) { // Warhead reticle
				log_debug("[DBG] [RET] Reticle_17|21: %d", iValue);
				ResName = new char[80];
				ReticleIndexToHUDresname(iValue, ResName, 80);
				CustomWLight_MR_ResNames.push_back(ResName);
				log_debug("[DBG] [RET] Custom WLight MR: %s Added", ResName);
			}
			// RIght-Right Warning Lights
			else if (_stricmp(param, "Reticle_18") == 0 || _stricmp(param, "Reticle_22") == 0) { // Warhead reticle
				log_debug("[DBG] [RET] Reticle_18|22: %d", iValue);
				ResName = new char[80];
				ReticleIndexToHUDresname(iValue, ResName, 80);
				CustomWLight_RR_ResNames.push_back(ResName);
				log_debug("[DBG] [RET] Custom WLight RR: %s Added", ResName);
			}
		}
	}
	fclose(file);
	return true;
}

void LoadCustomReticle(char* sCurrentCockpit) {
	char sFileName[80], sCurrent[80];
	int len;
	ClearCustomReticleCenterResNames();

	strcpy_s(sCurrent, sCurrentCockpit);
	len = strlen(sCurrent);
	// Remove the "Cockpit" from the name:
	sCurrent[len - 7] = 0;
	// Look in the Reticle.txt file
	snprintf(sFileName, 80, "./FlightModels/%sReticle.txt", sCurrent);
	log_debug("[DBG] [RET] Loading file: %s", sFileName);
	if (LoadReticleTXTFile(sFileName))
		return;

	log_debug("[DBG] [RET] Could not load %s, searching the INI file for custom reticles", sFileName);

	// Look in the INI file
	snprintf(sFileName, 80, "./FlightModels/%s.ini", sCurrent);
	log_debug("[DBG] [RET] Loading file: %s", sFileName);
	if (!LoadReticleTXTFile(sFileName)) {
		log_debug("[DBG] [RET] Could not load %s. No custom reticles for this cockpit", sFileName);
	}
}