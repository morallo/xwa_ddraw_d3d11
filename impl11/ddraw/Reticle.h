#pragma once

#include <vector>
#include "Vectors.h"
#include "DynamicCockpit.h"

extern float g_fYCenter, g_fFOVscale;
extern Vector2 g_ReticleCentroid;
extern Box g_ReticleCenterLimits;
extern bool g_bTriggerReticleCapture, g_bYCenterHasBeenFixed;

extern std::vector<char*> Reticle_ResNames;
extern std::vector<char*> ReticleCenter_ResNames;
extern std::vector<char*> CustomReticleCenter_ResNames;
extern char g_sLaserLockedReticleResName[80];
extern std::vector<char*> CustomWLight_LL_ResNames;
extern std::vector<char*> CustomWLight_ML_ResNames;
extern std::vector<char*> CustomWLight_MR_ResNames;
extern std::vector<char*> CustomWLight_RR_ResNames;

int ReticleIndexToHUDSlot(int ReticleIndex);
void ReticleIndexToHUDresname(int ReticleIndex, char* out_str, int out_str_len);
void ClearCustomReticleCenterResNames();
bool LoadReticleTXTFile(char* sFileName);
void LoadCustomReticle(char* sCurrentCockpit);
