#pragma once

#include <vector>
#include "common.h"
#include "XWAFramework.h"
#include "EffectsCommon.h"
#include "DynamicCockpit.h"
#include "ActiveCockpit.h"
#include "ShadowMapping.h"
#include "Reticle.h"
#include "Materials.h"

// SSAO Type
typedef enum {
	SSO_AMBIENT,
	SSO_DIRECTIONAL,
	SSO_BENT_NORMALS,
	SSO_DEFERRED, // New Shading Model
} SSAOTypeEnum;

// In order to blend the background with the hyperspace effect when exiting, we need to extend the
// effect for a few more frames. To do that, we need to track the current state of the effect and
// that's why we need a small state machine:
enum HyperspacePhaseEnum {
	HS_INIT_ST = 0,				// Initial state, we're not even in Hyperspace
	HS_HYPER_ENTER_ST = 1,		// We're entering hyperspace
	HS_HYPER_TUNNEL_ST = 2,		// Traveling through the blue Hyperspace tunnel
	HS_HYPER_EXIT_ST = 3,		// HyperExit streaks are being rendered
	HS_POST_HYPER_EXIT_ST = 4   // HyperExit streaks have finished rendering; but now we're blending with the backround
};
const int MAX_POST_HYPER_EXIT_FRAMES = 10; // I had 20 here up to version 1.1.1. Making this smaller makes the zoom faster

// xwahacker computes the FOV like this: FOV = 2.0 * atan(height/focal_length). This formula is questionable, the actual
// FOV seems to be: 2.0 * atan((height/2)/focal_length), same for the horizontal FOV. I confirmed this by geometry
// and by computing the angle between the lights and the current forward point.
// Data provided by keiranhalcyon7:
inline uint32_t* g_rawFOVDist = (uint32_t*)0x91AB6C; // raw FOV dist(dword int), copy of one of the six values hard-coded with the resolution slots, which are what xwahacker edits
inline float* g_fRawFOVDist = (float*)0x8B94CC; // FOV dist(float), same value as above
inline float* g_cachedFOVDist = (float*)0x8B94BC; // cached FOV dist / 512.0 (float), seems to be used for some sprite processing
inline float g_fDefaultFOVDist = 1280.0f; // Original FOV dist

extern std::vector<char*> Text_ResNames;
extern std::vector<char*> Floating_GUI_ResNames;
extern std::vector<char*> LaserIonEnergy_ResNames;
// List of regular GUI elements (this is not an exhaustive list). It's mostly used to detect when
// the game has started rendering the GUI
extern std::vector<char*> GUI_ResNames;
// List of common roots for the electricity names
extern std::vector<char*> Electricity_ResNames;
// List of common roots for the Explosion names
extern std::vector<char*>Explosion_ResNames;
// Smoke from explosions:
extern std::vector<char*> Smoke_ResNames;
extern std::vector<char*> Sparks_ResNames;
// List of Lens Flare effects
extern std::vector<char*> LensFlare_ResNames;
// List of Suns in the Backdrop.dat file
extern std::vector<char*> Sun_ResNames;
extern std::vector<char*> SpaceDebris_ResNames;
extern std::vector<char*> Trails_ResNames;

#define MAX_SPEED_PARTICLES 256
extern Vector4 g_SpeedParticles[MAX_SPEED_PARTICLES];

// Constant Buffers
extern BloomPixelShaderCBuffer	g_BloomPSCBuffer;
extern SSAOPixelShaderCBuffer	g_SSAO_PSCBuffer;
extern PSShadingSystemCB		g_ShadingSys_PSBuffer;
extern ShadertoyCBuffer			g_ShadertoyBuffer;
extern LaserPointerCBuffer		g_LaserPointerBuffer;

extern bool g_b3DSunPresent, g_b3DSkydomePresent;

// LASER LIGHTS
extern SmallestK g_LaserList;
extern bool g_bEnableLaserLights, g_bEnableHeadLights;
extern Vector3 g_LaserPointDebug;
extern Vector3 g_HeadLightsPosition, g_HeadLightsColor;
extern float g_fHeadLightsAmbient, g_fHeadLightsDistance, g_fHeadLightsAngleCos;
extern bool g_bHeadLightsAutoTurnOn;

bool isInVector(uint32_t crc, std::vector<uint32_t>& vector);
bool isInVector(char* name, std::vector<char*>& vector);
bool isInVector(char* OPTname, std::vector<OPTNameType>& vector);