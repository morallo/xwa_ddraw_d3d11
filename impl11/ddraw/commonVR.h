#pragma once

#include <Windows.h>
#include <stdio.h>
#include "common.h"
#include "utils.h"
#include "config.h"
#include "commonVR.h"
#include "SteamVR.h"
#include <headers/openvr.h>
#include "FreePIE.h"
#include "Vectors.h"
#include "Matrices.h"

extern const float PI;
extern const float DEG2RAD;
extern const float RAD_TO_DEG;
extern const float DEFAULT_IPD; // Ignored in SteamVR mode.
extern const float IPD_SCALE_FACTOR;
extern Matrix4 g_EyeMatrixLeftInv, g_EyeMatrixRightInv;
extern Matrix4 g_projLeft, g_projRight;
extern Matrix4 g_FullProjMatrixLeft, g_FullProjMatrixRight, g_viewMatrix;
extern Vector3 g_headCenter; // The head's center: this value should be re-calibrated whenever we set the headset
extern bool g_bResetHeadCenter; // Reset the head center on startup
extern float g_fVR_FOV;
extern float g_fIPD;
extern float g_fHalfIPD;
// Set to true in PrimarySurface Present 2D (Flip)
extern bool g_bInTechRoom;

void EvaluateIPD(float NewIPD);
void IncreaseIPD(float Delta);

typedef enum {
	TRACKER_NONE,
	TRACKER_FREEPIE,
	TRACKER_STEAMVR,
	TRACKER_TRACKIR
} TrackerType;
