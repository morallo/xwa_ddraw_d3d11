#pragma once

#include <vector>

extern float g_fYCenter, g_fFOVscale;
extern Vector2 g_ReticleCentroid;
extern Box g_ReticleCenterLimits;
extern bool g_bTriggerReticleCapture, g_bYCenterHasBeenFixed;

extern std::vector<char*> Reticle_ResNames;
extern std::vector<char*> ReticleCenter_ResNames;
extern std::vector<char*> CustomReticleCenter_ResNames;


int ReticleIndexToHUDSlot(int ReticleIndex);
void ReticleIndexToHUDresname(int ReticleIndex, char* out_str, int out_str_len);
void ClearCustomReticleCenterResNames();
bool LoadReticleTXTFile(char* sFileName);
void LoadCustomReticle(char* sCurrentCockpit);
