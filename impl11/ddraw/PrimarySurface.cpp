// Copyright (c) 2014 J�r�my Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes (c) 2019

#include <ScreenGrab.h>
#include <wincodec.h>

#include "common.h"
#include "DeviceResources.h"
#include "PrimarySurface.h"
#include "BackbufferSurface.h"
#include "FrontbufferSurface.h"
#include "FreePIE.h"
#include "Matrices.h"
#include "Direct3DTexture.h"
//#include "XWAFramework.h"
#include "XwaDrawTextHook.h"
#include "XwaDrawRadarHook.h"
#include "XwaDrawBracketHook.h"

#include "../../Hook_XWACockpitLook/SteamVR.h"

#define DBG_MAX_PRESENT_LOGS 0

const float DEG2RAD = 3.141593f / 180.0f;

#include <vector>

#include "XWAObject.h"
extern PlayerDataEntry* PlayerDataTable;
ObjectEntry **objects = (ObjectEntry **)0x7B33C4;
CraftDefinitionEntry *CraftDefinitionTable = (CraftDefinitionEntry *)0x005BB480; // 32 Entries
extern uint32_t* g_playerIndex;
const auto mouseLook_Y = (int*)0x9E9624;
const auto mouseLook_X = (int*)0x9E9620;
const auto numberOfPlayersInGame = (int*)0x910DEC;
extern uint32_t *g_playerInHangar;
bool g_bPrevPlayerInHangar = false;
#define GENERIC_POV_SCALE 44.0f
// These values match MXvTED exactly:
const short *g_POV_Y0 = (short *)(0x5BB480 + 0x238);
const short *g_POV_Z0 = (short *)(0x5BB480 + 0x23A);
const short *g_POV_X0 = (short *)(0x5BB480 + 0x23C);
// Floating-point version of the POV (plus Y is inverted):
const float *g_POV_X = (float *)(0x8B94E0 + 0x20D);
const float *g_POV_Y = (float *)(0x8B94E0 + 0x211);
const float *g_POV_Z = (float *)(0x8B94E0 + 0x215);
const auto g_FlightSurfaceHeight = (DWORD*)0x07D4B6C;
auto g_hudScale = (float *)0x06002B8;

extern int *s_XwaGlobalLightsCount;
extern XwaGlobalLight* s_XwaGlobalLights;
extern Matrix4 g_CurrentHeadingViewMatrix;

extern bool g_bExternalHUDEnabled, g_bEdgeDetectorEnabled, g_bStarDebugEnabled;

extern float g_f2DYawMul, g_f2DPitchMul, g_f2DRollMul;
extern TrackerType g_TrackerType;

/*
dword& s_V0x09C6E38 = *(dword*)0x009C6E38;
When the value is different of 0xFFFF, the player craft is in a hangar.
*/
extern uint32_t *g_rawFOVDist; /* = (uint32_t *)0x91AB6C*/ // raw FOV dist(dword int), copy of one of the six values hard-coded with the resolution slots, which are what xwahacker edits
extern float *g_fRawFOVDist; /*= (float *)0x8B94CC;*/ // FOV dist(float), same value as above
extern float *g_cachedFOVDist; /*= (float *)0x8B94BC;*/ // cached FOV dist / 512.0 (float), seems to be used for some sprite processing

extern float g_fYCenter, g_fFOVscale;
extern Vector2 g_ReticleCentroid;
extern Box g_ReticleCenterLimits;
extern bool g_bTriggerReticleCapture, g_bYCenterHasBeenFixed;

extern float *g_fRawFOVDist, g_fCurrentShipFocalLength, g_fCurrentShipLargeFocalLength, g_fVR_FOV;
extern float g_fDebugFOVscale, g_fDebugYCenter;
extern bool g_bCustomFOVApplied, g_bLastFrameWasExterior;
extern float g_fRealHorzFOV, g_fRealVertFOV;
bool g_bMetricParamsNeedReapply = false;
bool LoadFocalLength();
void ApplyFocalLength(float focal_length);
void SaveFocalLength();
Matrix4 g_ReflRotX;

// Text Rendering
TimedMessage g_TimedMessages[MAX_TIMED_MESSAGES];

void ZToDepthRHW(float Z, float *sz, float *rhw);
void GetCraftViewMatrix(Matrix4 *result);
void DisplayBox(char *name, Box box);
Vector3 project(Vector3 pos3D, Matrix4 viewMatrix, Matrix4 projEyeMatrix /*, float *sx, float *sy */);
inline void backProject(float sx, float sy, float rhw, Vector3 *P);
inline void backProjectMetric(float sx, float sy, float rhw, Vector3 *P);
inline Vector3 projectMetric(Vector3 pos3D, Matrix4 viewMatrix, Matrix4 projEyeMatrix, bool bForceNonVR = false);
inline Vector3 projectToInGameCoords(Vector3 pos3D, Matrix4 viewMatrix, Matrix4 projEyeMatrix);
inline Vector3 projectToInGameOrPostProcCoordsMetric(Vector3 pos3D, Matrix4 viewMatrix, Matrix4 projEyeMatrix, bool bForceNonVR = false);
bool rayTriangleIntersect(
	const Vector3 &orig, const Vector3 &dir,
	const Vector3 &v0, const Vector3 &v1, const Vector3 &v2,
	float &t, Vector3 &P, float &u, float &v);

extern HyperspacePhaseEnum g_HyperspacePhaseFSM;
extern short g_fLastCockpitCameraYaw, g_fLastCockpitCameraPitch;
extern int g_lastCockpitXReference, g_lastCockpitYReference, g_lastCockpitZReference;
extern float g_fHyperShakeRotationSpeed, g_fHyperLightRotationSpeed, g_fHyperspaceRand;
extern float g_fCockpitCameraYawOnFirstHyperFrame, g_fCockpitCameraPitchOnFirstHyperFrame, g_fCockpitCameraRollOnFirstHyperFrame;
extern float g_fHyperTimeOverride; // DEBUG, remove later
extern int g_iHyperStateOverride; // DEBUG, remove later
extern bool g_bHyperDebugMode; // DEBUG -- needed to fine-tune the effect, won't be able to remove until I figure out an automatic way to setup the effect
extern bool g_bHyperspaceFirstFrame; // Set to true on the first frame of hyperspace, reset to false at the end of each frame
extern bool g_bClearedAuxBuffer, g_bExecuteBufferLock;
extern bool g_bHyperHeadSnapped, g_bHyperspaceEffectRenderedOnCurrentFrame;
extern int g_iHyperExitPostFrames;
bool g_bKeybExitHyperspace = false;
extern Vector4 g_TempLightColor[2], g_TempLightVector[2];

// DYNAMIC COCKPIT
extern dc_element g_DCElements[];
extern int g_iNumDCElements;
extern DCHUDRegions g_DCHUDRegions;
extern move_region_coords g_DCMoveRegions;
extern char g_sCurrentCockpit[128];
extern bool g_bDCApplyEraseRegionCommands, g_bReRenderMissilesNCounterMeasures;
extern bool g_bEdgeEffectApplied, g_bDCHologramsVisible;
extern float g_fReticleScale;
extern DCElemSrcBoxes g_DCElemSrcBoxes;
//float g_fReticleOfsX = 0.0f;
//float g_fReticleOfsY = 0.0f;
//extern bool g_bInhibitCMDBracket; // Used in XwaDrawBracketHook
//extern float g_fXWAScale;

extern Vector2 g_TriangleCentroid;
extern float g_fTrianglePointerDist;

// ACTIVE COCKPIT
extern bool g_bActiveCockpitEnabled, g_bACActionTriggered, g_bACLastTriggerState, g_bACTriggerState;
extern bool g_bFreePIEInitialized, g_bOriginFromHMD, g_bCompensateHMDRotation, g_bCompensateHMDPosition, g_bFreePIEControllerButtonDataAvailable;
extern Vector4 g_contOriginWorldSpace, g_contOriginViewSpace, g_contDirWorldSpace, g_contDirViewSpace;
extern Vector3 g_LaserPointer3DIntersection;
extern float g_fBestIntersectionDistance, g_fLaserPointerLength;
extern int g_iFreePIESlot, g_iFreePIEControllerSlot;
extern float g_fContMultiplierX, g_fContMultiplierY, g_fContMultiplierZ, g_fFakeRoll;
extern int g_iBestIntersTexIdx;
extern ac_element g_ACElements[MAX_AC_TEXTURES_PER_COCKPIT];
extern int g_iNumACElements, g_iLaserDirSelector;

// DEBUG vars
extern Vector3 g_debug_v0, g_debug_v1, g_debug_v2;
extern bool g_bDumpLaserPointerDebugInfo;
extern Vector3 g_LPdebugPoint;
extern float g_fLPdebugPointOffset, g_fDebugYCenter;
// DEBUG vars

extern int g_iNaturalConcourseAnimations, g_iHUDOffscreenCommandsRendered;
extern bool g_bIsTrianglePointer, g_bLastTrianglePointer, g_bFixedGUI, g_bFloatingAimingHUD;
extern bool g_bYawPitchFromMouseOverride, g_bIsSkyBox, g_bPrevIsSkyBox, g_bSkyBoxJustFinished;
extern bool g_bIsPlayerObject, g_bPrevIsPlayerObject, g_bSwitchedToGUI;
extern bool g_bIsTargetHighlighted, g_bPrevIsTargetHighlighted;

// SPEED SHADER EFFECT
extern bool g_bHyperspaceTunnelLastFrame, g_bHyperspaceLastFrame;
extern bool g_bEnableSpeedShader, g_bEnableAdditionalGeometry;
extern float g_fSpeedShaderScaleFactor, g_fSpeedShaderParticleSize, g_fSpeedShaderMaxIntensity, g_fSpeedShaderTrailSize, g_fSpeedShaderParticleRange;
extern float g_fCockpitTranslationScale;
extern int g_iSpeedShaderMaxParticles;
Vector4 g_prevFs(0, 0, 0, 0), g_prevUs(0, 0, 0, 0);
D3DTLVERTEX g_SpeedParticles2D[MAX_SPEED_PARTICLES * 12];

// SHADOW MAPPING
extern ShadowMappingData g_ShadowMapping;
extern bool g_bShadowMapEnable, g_bShadowMapDebug, g_bShadowMappingInvertCameraMatrix, g_bShadowMapEnablePCSS;
extern float g_fShadowMapScale, g_fShadowMapAngleX, g_fShadowMapAngleY, g_fShadowMapDepthTrans;
extern float SHADOW_OBJ_SCALE, SHADOW_OBJ_SCALE_Y, SHADOW_OBJ_SCALE_Z;
extern std::vector<Vector4> g_OBJLimits;
bool g_bShadowMapHardwarePCF = false;
extern XWALightInfo g_XWALightInfo[MAX_XWA_LIGHTS];
extern Vector3 g_SunCentroids[MAX_XWA_LIGHTS];
extern Vector2 g_SunCentroids2D[MAX_XWA_LIGHTS];
extern int g_iNumSunCentroids;

extern VertexShaderCBuffer g_VSCBuffer;
extern PixelShaderCBuffer g_PSCBuffer;
extern DCPixelShaderCBuffer g_DCPSCBuffer;
extern ShadowMapVertexShaderMatrixCB g_ShadowMapVSCBuffer;
extern MetricReconstructionCB g_MetricRecCBuffer;
extern float g_fAspectRatio, g_fGlobalScale, g_fBrightness, g_fGUIElemsScale, g_fHUDDepth, g_fFloatingGUIDepth;
extern float g_fCurScreenWidth, g_fCurScreenHeight, g_fCurInGameAspectRatio, g_fCurScreenWidthRcp, g_fCurScreenHeightRcp;
extern float g_fCurInGameWidth, g_fCurInGameHeight, g_fMetricMult;
extern int g_WindowWidth, g_WindowHeight;
extern D3D11_VIEWPORT g_nonVRViewport;



void InGameToScreenCoords(UINT left, UINT top, UINT width, UINT height, float x, float y, float *x_out, float *y_out);
void ScreenCoordsToInGame(float left, float top, float width, float height, float x, float y, float *x_out, float *y_out);
void GetScreenLimitsInUVCoords(float *x0, float *y0, float *x1, float *y1, bool UseNonVR=false);

#include <headers/openvr.h>
#if !defined(XWA_MATH_CONSTANTS)
#define XWA_MATH_CONSTANTS 1
const float PI = 3.141592f;
const float RAD_TO_DEG = 180.0f / PI;
#endif
extern float g_fPitchMultiplier, g_fYawMultiplier, g_fRollMultiplier;
extern float g_fYawOffset, g_fPitchOffset;
extern float g_fPosXMultiplier, g_fPosYMultiplier, g_fPosZMultiplier;
extern float g_fMinPositionX, g_fMaxPositionX;
extern float g_fMinPositionY, g_fMaxPositionY;
extern float g_fMinPositionZ, g_fMaxPositionZ;
extern float g_fFrameTimeRemaining;
extern float g_fSteamVRMirrorWindow3DScale, g_fSteamVRMirrorWindowAspectRatio;
extern Vector3 g_headCenter;
extern bool g_bResetHeadCenter, g_bSteamVRPosFromFreePIE, g_bReshadeEnabled, g_bSteamVRDistortionEnabled, g_bSteamVRYawPitchRollFromMouseLook;
extern vr::IVRSystem *g_pHMD;
extern int g_iFreePIESlot, g_iSteamVR_Remaining_ms, g_iSteamVR_VSync_ms;
extern Matrix4 g_FullProjMatrixLeft, g_FullProjMatrixRight;
bool g_bTogglePostPresentHandoff = false, g_bInTechRoom = false, g_bSteamVRMirrorWindowLeftEye = true;

// LASER LIGHTS
extern SmallestK g_LaserList;
extern bool g_bEnableLaserLights, g_bEnableHeadLights;
Vector3 g_LaserPointDebug(0.0f, 0.0f, 0.0f);
Vector3 g_HeadLightsPosition(0.0f, 0.0f, 20.0f), g_HeadLightsColor(0.85f, 0.85f, 0.90f);
float g_fHeadLightsAmbient = 0.05f, g_fHeadLightsDistance = 5000.0f, g_fHeadLightsAngleCos = 0.25f; // Approx cos(75)
bool g_bHeadLightsAutoTurnOn = true;

// Bloom
extern bool /* g_bDumpBloomBuffers, */ g_bDCManualActivate;
extern BloomConfig g_BloomConfig;
float g_fBloomLayerMult[8] = {
	1.000f, // 0
	1.025f, // 1
	1.030f, // 2
	1.035f, // 3
	1.045f, // 4
	1.055f, // 5
	1.070f, // 6
	1.100f, // 7
};
float g_fBloomSpread[8] = {
	2.0f, // 0
	3.0f, // 1
	4.0f, // 2
	4.0f, // 3
	4.0f, // 4
	4.0f, // 5
	4.0f, // 6
	4.0f, // 7
};
int g_iBloomPasses[8] = {
	1, 1, 1, 1, 1, 1, 1, 1
};

//extern FILE *colorFile, *lightFile;

// SSAO
extern SSAOTypeEnum g_SSAO_Type;
extern float g_fSSAOZoomFactor, g_fSSAOZoomFactor2, g_fSSAOWhitePoint, g_fNormWeight, g_fNormalBlurRadius;
extern int g_iSSDODebug, g_iSSAOBlurPasses;
extern bool g_bBlurSSAO, g_bDepthBufferResolved, g_bOverrideLightPos;
extern bool g_bShowSSAODebug, g_bEnableIndirectSSDO, g_bFNEnable, g_bShadowEnable;
extern bool g_bDumpSSAOBuffers, g_bEnableSSAOInShader, g_bEnableBentNormalsInShader;
extern Vector4 g_LightVector[2];
extern Vector4 g_LightColor[2];
extern float g_fViewYawSign, g_fViewPitchSign;
float g_fMoireOffsetDir = 0.02f, g_fMoireOffsetInd = 0.1f;

// V0x00782848
DWORD *XwaGlobalLightsCount = (DWORD *)0x00782848;

// V0x007D4FA0
XwaGlobalLight *XwaGlobalLights = (XwaGlobalLight *)0x007D4FA0; // Maximum 8 lights

struct XwaVector3
{
	/* 0x0000 */ float x;
	/* 0x0004 */ float y;
	/* 0x0008 */ float z;
};

// S0x0000002
struct XwaMatrix3x3
{
	/* 0x0000 */ float _11;
	/* 0x0004 */ float _12;
	/* 0x0008 */ float _13;
	/* 0x000C */ float _21;
	/* 0x0010 */ float _22;
	/* 0x0014 */ float _23;
	/* 0x0018 */ float _31;
	/* 0x001C */ float _32;
	/* 0x0020 */ float _33;
};

// S0x0000012
struct XwaTransform
{
	/* 0x0000 */ XwaVector3 Position;
	/* 0x000C */ XwaMatrix3x3 Rotation;
};

void ShowMatrix4(const Matrix4 &mat, char *name);

// S0x0000001
// L00439B30
void XwaVector3Transform(XwaVector3* A4, const XwaMatrix3x3* A8)
{
	float A4_00 = A8->_31 * A4->z + A8->_21 * A4->y + A8->_11 * A4->x;
	float A4_04 = A8->_32 * A4->z + A8->_22 * A4->y + A8->_12 * A4->x;
	float A4_08 = A8->_33 * A4->z + A8->_23 * A4->y + A8->_13 * A4->x;

	A4->x = A4_00;
	A4->y = A4_04;
	A4->z = A4_08;
}

void DumpXwaTransform(char *name, XwaTransform *t) {
	log_debug("[DBG] ********************");
	log_debug("[DBG] %s", name);
	log_debug("[DBG] Rotation:");
	log_debug("[DBG] %0.3f, %0.3f, %0.3f", t->Rotation._11, t->Rotation._12, t->Rotation._13);
	log_debug("[DBG] %0.3f, %0.3f, %0.3f", t->Rotation._21, t->Rotation._22, t->Rotation._23);
	log_debug("[DBG] %0.3f, %0.3f, %0.3f", t->Rotation._31, t->Rotation._32, t->Rotation._33);
	log_debug("[DBG] Translation:");
	log_debug("[DBG] %0.3f, %0.3f, %0.3f", t->Position.x, t->Position.y, t->Position.z);
	log_debug("[DBG] ********************");
}

void DumpGlobalLights()
{
	std::ostringstream str;

	int s_XwaCurrentSceneCompData = *(int*)0x009B6D02;
	int s_XwaSceneCompDatasOffset = *(int*)0x009B6CF8;

	XwaTransform* ViewTransform = (XwaTransform*)(s_XwaSceneCompDatasOffset + s_XwaCurrentSceneCompData * 284 + 0x0008);
	XwaTransform* WorldTransform = (XwaTransform*)(s_XwaSceneCompDatasOffset + s_XwaCurrentSceneCompData * 284 + 0x0038);

	//DumpXwaTransform("ViewTransform", ViewTransform);
	//DumpXwaTransform("WorldTransform", WorldTransform);

	for (int i = 0; i < *s_XwaGlobalLightsCount; i++)
	//int i = 0;
	{
		log_debug("[DBG] ***************************");
		//str << std::endl;
		//log_debug("[DBG] light[%d], pos: (%0.3f, %0.3f, %0.3f)",
		//	i, s_XwaGlobalLights[i].PositionX, s_XwaGlobalLights[i].PositionY, s_XwaGlobalLights[i].PositionZ);
		log_debug("[DBG] light[%d], dir: [%0.3f, %0.3f, %0.3f]",
			i, s_XwaGlobalLights[i].DirectionX, s_XwaGlobalLights[i].DirectionY, s_XwaGlobalLights[i].DirectionZ);

		XwaVector3 viewDir = { s_XwaGlobalLights[i].DirectionX, s_XwaGlobalLights[i].DirectionY, s_XwaGlobalLights[i].DirectionZ };
		XwaVector3Transform(&viewDir, &ViewTransform->Rotation);
		log_debug("[DBG] light[%d], viewDir: [%0.3f, %0.3f, %0.3f]", i, viewDir.x,  viewDir.y, viewDir.z);

		XwaVector3 worldDir = { s_XwaGlobalLights[i].DirectionX, s_XwaGlobalLights[i].DirectionY, s_XwaGlobalLights[i].DirectionZ };
		XwaVector3Transform(&worldDir, &WorldTransform->Rotation);
		log_debug("[DBG] light[%d], worldDir: [%0.3f, %0.3f, %0.3f]", i, worldDir.x, worldDir.y, worldDir.z);

		XwaVector3 worlViewDir = { s_XwaGlobalLights[i].DirectionX, s_XwaGlobalLights[i].DirectionY, s_XwaGlobalLights[i].DirectionZ };
		XwaVector3Transform(&worlViewDir, &WorldTransform->Rotation);
		XwaVector3Transform(&worlViewDir, &ViewTransform->Rotation);
		log_debug("[DBG] light[%d], worldViewDir: [%0.3f, %0.3f, %0.3f]", 
			i, worlViewDir.x, worlViewDir.y, worlViewDir.z);
		log_debug("[DBG] ***************************");
	}

	//LogText(str.str());
}

inline float lerp(float x, float y, float s) {
	return x + s * (y - x);
}

/*
 * Convert a rotation matrix to a normalized quaternion.
 * From: http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
 */
vr::HmdQuaternionf_t rotationToQuaternion(vr::HmdMatrix34_t m) {
	float tr = m.m[0][0] + m.m[1][1] + m.m[2][2];
	vr::HmdQuaternionf_t q;

	if (tr > 0) {
		float S = sqrt(tr + 1.0f) * 2.0f; // S=4*qw 
		q.w = 0.25f * S;
		q.x = (m.m[2][1] - m.m[1][2]) / S;
		q.y = (m.m[0][2] - m.m[2][0]) / S;
		q.z = (m.m[1][0] - m.m[0][1]) / S;
	}
	else if ((m.m[0][0] > m.m[1][1]) && (m.m[0][0] > m.m[2][2])) {
		float S = sqrt(1.0f + m.m[0][0] - m.m[1][1] - m.m[2][2]) * 2.0f; // S=4*qx 
		q.w = (m.m[2][1] - m.m[1][2]) / S;
		q.x = 0.25f * S;
		q.y = (m.m[0][1] + m.m[1][0]) / S;
		q.z = (m.m[0][2] + m.m[2][0]) / S;
	}
	else if (m.m[1][1] > m.m[2][2]) {
		float S = sqrt(1.0f + m.m[1][1] - m.m[0][0] - m.m[2][2]) * 2.0f; // S=4*qy
		q.w = (m.m[0][2] - m.m[2][0]) / S;
		q.x = (m.m[0][1] + m.m[1][0]) / S;
		q.y = 0.25f * S;
		q.z = (m.m[1][2] + m.m[2][1]) / S;
	}
	else {
		float S = sqrt(1.0f + m.m[2][2] - m.m[0][0] - m.m[1][1]) * 2.0f; // S=4*qz
		q.w = (m.m[1][0] - m.m[0][1]) / S;
		q.x = (m.m[0][2] + m.m[2][0]) / S;
		q.y = (m.m[1][2] + m.m[2][1]) / S;
		q.z = 0.25f * S;
	}
	float Q = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
	q.x /= Q;
	q.y /= Q;
	q.z /= Q;
	q.w /= Q;
	return q;
}

/*
 * From http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToMatrix/index.htm
 */
vr::HmdMatrix33_t quatToMatrix(vr::HmdQuaternionf_t q) {
	vr::HmdMatrix33_t m;
	float sqw = q.w*q.w;
	float sqx = q.x*q.x;
	float sqy = q.y*q.y;
	float sqz = q.z*q.z;

	// invs (inverse square length) is only required if quaternion is not already normalised
	float invs = 1 / (sqx + sqy + sqz + sqw);
	m.m[0][0] = (sqx - sqy - sqz + sqw) * invs; // since sqw + sqx + sqy + sqz =1/invs*invs
	m.m[1][1] = (-sqx + sqy - sqz + sqw) * invs;
	m.m[2][2] = (-sqx - sqy + sqz + sqw) * invs;

	float tmp1 = q.x*q.y;
	float tmp2 = q.z*q.w;
	m.m[1][0] = 2.0f * (tmp1 + tmp2)*invs;
	m.m[0][1] = 2.0f * (tmp1 - tmp2)*invs;

	tmp1 = q.x*q.z;
	tmp2 = q.y*q.w;
	m.m[2][0] = 2.0f * (tmp1 - tmp2)*invs;
	m.m[0][2] = 2.0f * (tmp1 + tmp2)*invs;
	tmp1 = q.y*q.z;
	tmp2 = q.x*q.w;
	m.m[2][1] = 2.0f * (tmp1 + tmp2)*invs;
	m.m[1][2] = 2.0f * (tmp1 - tmp2)*invs;
	return m;
}

/*
   From: http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/index.htm
   yaw: left = +90, right = -90
   pitch: up = +90, down = -90
   roll: left = +90, right = -90

   if roll > 90, the axis will swap pitch and roll; but why would anyone do that?
*/
void quatToEuler(vr::HmdQuaternionf_t q, float *yaw, float *roll, float *pitch) {
	float test = q.x*q.y + q.z*q.w;

	if (test > 0.499f) { // singularity at north pole
		*yaw = 2 * atan2(q.x, q.w);
		*pitch = PI / 2.0f;
		*roll = 0;
		return;
	}
	if (test < -0.499f) { // singularity at south pole
		*yaw = -2 * atan2(q.x, q.w);
		*pitch = -PI / 2.0f;
		*roll = 0;
		return;
	}
	float sqx = q.x*q.x;
	float sqy = q.y*q.y;
	float sqz = q.z*q.z;
	*yaw = atan2(2.0f * q.y*q.w - 2.0f * q.x*q.z, 1.0f - 2.0f * sqy - 2.0f * sqz);
	*pitch = asin(2.0f * test);
	*roll = atan2(2.0f * q.x*q.w - 2.0f * q.y*q.z, 1.0f - 2.0f * sqx - 2.0f * sqz);
}

/*
 * From: http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/index.htm
 */
vr::HmdQuaternionf_t eulerToQuat(float yaw, float pitch, float roll) {
	vr::HmdQuaternionf_t q;
	// Assuming the angles are in radians.
	float c1 = cos(yaw / 2.0f);
	float s1 = sin(yaw / 2.0f);
	float c2 = cos(pitch / 2.0f);
	float s2 = sin(pitch / 2.0f);
	float c3 = cos(roll / 2.0f);
	float s3 = sin(roll / 2.0f);
	float c1c2 = c1 * c2;
	float s1s2 = s1 * s2;
	q.w = c1c2 * c3 - s1s2 * s3;
	q.x = c1c2 * s3 + s1s2 * c3;
	q.y = s1 * c2*c3 + c1 * s2*s3;
	q.z = c1 * s2*c3 - s1 * c2*s3;
	return q;
}

Matrix3 HmdMatrix34toMatrix3(const vr::HmdMatrix34_t &mat) {
	Matrix3 matrixObj(
		mat.m[0][0], mat.m[1][0], mat.m[2][0],
		mat.m[0][1], mat.m[1][1], mat.m[2][1],
		mat.m[0][2], mat.m[1][2], mat.m[2][2]
	);
	return matrixObj;
}

Matrix3 HmdMatrix33toMatrix3(const vr::HmdMatrix33_t &mat) {
	Matrix3 matrixObj(
		mat.m[0][0], mat.m[1][0], mat.m[2][0],
		mat.m[0][1], mat.m[1][1], mat.m[2][1],
		mat.m[0][2], mat.m[1][2], mat.m[2][2]
	);
	return matrixObj;
}

void ShowXWAMatrix(const XwaTransform &m) {
	log_debug("[DBG] -----------------------------");
	log_debug("[DBG] %0.4f, %0.4f, %0.4f, %0.4f", m.Rotation._11, m.Rotation._12, m.Rotation._13, m.Position.x);
	log_debug("[DBG] %0.4f, %0.4f, %0.4f, %0.4f", m.Rotation._21, m.Rotation._22, m.Rotation._23, m.Position.y);
	log_debug("[DBG] %0.4f, %0.4f, %0.4f, %0.4f", m.Rotation._31, m.Rotation._32, m.Rotation._33, m.Position.z);
	log_debug("[DBG] -----------------------------");
}

void ShowXWAVector(char *msg, const XwaVector3 &v) {
	log_debug("[DBG] %s [%0.4f, %0.4f, %0.4f]", msg, v.x, v.y, v.z);
}

/*
void ComputeRotationMatrixFromXWAView(Vector4 *light, int num_lights) {
	Vector4 tmpL[2], T, B, N;
	// Compute the full rotation
	float yaw   = PlayerDataTable[*g_playerIndex].yaw   / 65536.0f * 360.0f;
	float pitch = PlayerDataTable[*g_playerIndex].pitch / 65536.0f * 360.0f;
	float roll  = PlayerDataTable[*g_playerIndex].roll  / 65536.0f * 360.0f;

	// To test how (x,y,z) is aligned with either the Y+ or Z+ axis, just multiply rotMatrixPitch * rotMatrixYaw * (x,y,z)
	Matrix4 rotMatrixFull, rotMatrixYaw, rotMatrixPitch, rotMatrixRoll;
	rotMatrixFull.identity();
	rotMatrixYaw.identity();   rotMatrixYaw.rotateY(-yaw);
	rotMatrixPitch.identity(); rotMatrixPitch.rotateX(-pitch);
	rotMatrixRoll.identity();  rotMatrixRoll.rotateY(-roll); // Z or Y?

	// rotMatrixYaw aligns the orientation with the y-z plane (x --> 0)
	// rotMatrixPitch * rotMatrixYaw aligns the orientation with y+ (x --> 0 && z --> 0)
	// so the remaining rotation must be around the y axis (?)
	// DEBUG, x = z, y = x, z = y;
	// The yaw is indeed the y-axis rotation, it goes from -180 to 0 to 180.
	// When pitch == 90, the craft is actually seeing the horizon
	// When pitch == 0, the craft is looking towards the sun
	// When entering hyperspace, yaw,pitch,raw = (0,90,0), so that's probably the "home" orientation: level and looking forward
	// New approach: let's build a TBN system here to avoid the gimbal lock problem
	float cosTheta, cosPhi, sinTheta, sinPhi;
	cosTheta = cos(yaw * DEG2RAD), sinTheta = sin(yaw * DEG2RAD);
	cosPhi = cos(pitch * DEG2RAD), sinPhi = sin(pitch * DEG2RAD);
	N.z = cosTheta * sinPhi;
	N.x = sinTheta * sinPhi;
	N.y = cosPhi;
	N.w = 0;

	// This transform chain will always transform (x,y,z) into (0, 1, 0):
	// To make an orthonormal basis, we need x+ and z+
	N = rotMatrixPitch * rotMatrixYaw * N;
	B.x = 0; B.y = 0; B.z = 1; B.w = 0;
	T.x = 1; T.y = 0; T.z = 0; T.w = 0;
	// In this space we now need to rotate B and T around the Y axis by "roll" degrees
	B = rotMatrixRoll * B;
	T = rotMatrixRoll * T;
	// Our new basis is T,B,N; but we need to invert the yaw/pitch rotation we applied
	rotMatrixFull = rotMatrixPitch * rotMatrixYaw;
	rotMatrixFull.invert();
	T = rotMatrixFull * T;
	B = rotMatrixFull * B;
	N = rotMatrixFull * N;
	// Our TBN basis is now in absolute coordinates
	rotMatrixFull.identity();
	rotMatrixFull.set(
		T.x, B.x, N.x, 0,
		T.y, B.y, N.y, 0,
		T.z, B.z, N.z, 0,
		0, 0, 0, 1
	);
	// We further rotate the light vectors by 180 degrees so that Y+ points up:
	Matrix4 rot;
	rot.rotateX(180.0f);
	rotMatrixFull = rotMatrixFull * rot;

	for (int i = 0; i < num_lights; i++)
		tmpL[i] = rotMatrixFull * g_LightVector[i];

	// tmpL is now the light's direction in view space. We need to further transform this
	// to compensate for camera rotation

	// TODO: Switch between cockpit and external cameras -- apply the external camera rotation
	float viewYaw, viewPitch;
	if (PlayerDataTable[*g_playerIndex].externalCamera) {
		viewYaw   = PlayerDataTable[*g_playerIndex].cameraYaw   / 65536.0f * 360.0f;
		viewPitch = PlayerDataTable[*g_playerIndex].cameraPitch / 65536.0f * 360.0f;
	}
	else {
		viewYaw   = PlayerDataTable[*g_playerIndex].cockpitCameraYaw   / 65536.0f * 360.0f;
		viewPitch = PlayerDataTable[*g_playerIndex].cockpitCameraPitch / 65536.0f * 360.0f;
	}
	Matrix4 viewMatrixYaw, viewMatrixPitch, viewMatrixFull;
	viewMatrixYaw.identity();
	viewMatrixPitch.identity();
	viewMatrixYaw.rotateY(g_fViewYawSign   * viewYaw);
	viewMatrixYaw.rotateX(g_fViewPitchSign * viewPitch);
	viewMatrixFull = viewMatrixPitch * viewMatrixYaw;
	for (int i = 0; i < num_lights; i++)
		light[i] = viewMatrixFull * tmpL[i];

	//log_debug("[DBG] [AO] ypr: (%0.3f, %0.3f, %0.3f); sph: [%0.3f, %0.3f, %0.3f], pos: [%0.3f, %0.3f, %0.3f]",
	//	yaw, pitch, roll, x, y, z, tmp.x, tmp.y, tmp.z);
	//log_debug("[DBG] [AO] ypr: (%0.3f, %0.3f, %0.3f); T: [%0.3f, %0.3f, %0.3f], B: [%0.3f, %0.3f, %0.3f], N: [%0.3f, %0.3f, %0.3f]",
	//	yaw, pitch, roll, T.x, T.y, T.z, B.x, B.y, B.z, N.x, N.y, N.z);
	//log_debug("[DBG] [AO] ypr: (%0.3f, %0.3f, %0.3f); light: [%0.3f, %0.3f, %0.3f]",
	//	yaw, pitch, roll, light->x, light->y, light->z);
	//log_debug("[DBG] [AO] ypr: (%0.3f, %0.3f, %0.3f); camYP: (%0.3f, %0.3f), light: [%0.3f, %0.3f, %0.3f]",
	//	yaw, pitch, roll, viewYaw, viewPitch, light->x, light->y, light->z);
	// DEBUG

	//log_debug("[DBG] [AO] ypr: (%0.3f, %0.3f, %0.3f), L: [%0.3f, %0.3f, %0.3f]",
	//	yaw, pitch, roll, light->x, light->y, light->z);

	//log_debug("[DBG] [AO] ypr: (%0.3f, %0.3f, %0.3f); light: [%0.3f, %0.3f, %0.3f]",
	//	yaw, pitch, roll, tmplight.x, tmplight.y, tmplight.z);

	//log_debug("[DBG] [AO] XwaGlobalLightsCount: %d", *XwaGlobalLightsCount);
}
*/

void GetFakeYawPitchRollFromKeyboard(float *yaw, float *pitch, float *roll) {
	static float fake_yaw = 0.0f, fake_pitch = 0.0f, fake_roll = 0.0f;
	bool LeftKey = (GetAsyncKeyState(VK_LEFT) & 0x8000) == 0x8000;
	bool RightKey = (GetAsyncKeyState(VK_RIGHT) & 0x8000) == 0x8000;
	bool UpKey = (GetAsyncKeyState(VK_UP) & 0x8000) == 0x8000;
	bool DownKey = (GetAsyncKeyState(VK_DOWN) & 0x8000) == 0x8000;

	if (LeftKey)
		fake_yaw -= 1.0f;
	if (RightKey)
		fake_yaw += 1.0f;

	if (UpKey)
		fake_pitch += 1.0f;
	if (DownKey)
		fake_pitch -= 1.0f;

	*yaw = fake_yaw;
	*pitch = fake_pitch;
	*roll = fake_roll;
}

/*
 * Returns the current yaw, pitch in PlayerDataTable in degrees
 */
void GetFakeYawPitchRollFromMouseLook(float *yaw, float *pitch, float *roll) {
	*yaw   = -(float)PlayerDataTable[*g_playerIndex].cockpitCameraYaw   / 65536.0f * 360.0f;
	*pitch =  (float)PlayerDataTable[*g_playerIndex].cockpitCameraPitch / 65536.0f * 360.0f;
	*roll  =  0.0f;
}

struct MainVertex
{
	float pos[2];
	float tex[2];
	MainVertex() {}
	MainVertex(float x, float y, float tx, float ty) {
		pos[0] = x; pos[1] = y;
		tex[0] = tx; tex[1] = ty;
	}
};

// Barrel Effect
BarrelPixelShaderCBuffer g_BarrelPSCBuffer;
extern float g_fLensK1, g_fLensK2, g_fLensK3;

// Main Pixel Shader constant buffer
MainShadersCBuffer			g_MSCBuffer;
// Constant Buffers
BloomPixelShaderCBuffer		g_BloomPSCBuffer;
SSAOPixelShaderCBuffer		g_SSAO_PSCBuffer;
PSShadingSystemCB			g_ShadingSys_PSBuffer;
extern ShadertoyCBuffer		g_ShadertoyBuffer;
extern LaserPointerCBuffer	g_LaserPointerBuffer;
extern bool g_bBloomEnabled, g_bAOEnabled, g_bApplyXWALightsIntensity, g_bProceduralSuns, g_b3DSunPresent, g_b3DSkydomePresent;
extern float g_fBloomAmplifyFactor, g_fHangarAmbient, g_fGlobalAmbient;
extern float g_fSpecIntensity, g_fSpecBloomIntensity, g_fXWALightsSaturation, g_fXWALightsIntensity;

extern float g_fHDRLightsMultiplier, g_fHDRWhitePoint;
extern bool g_bHDREnabled;
bool g_bGlobalSpecToggle = true;
//extern float g_fFlareAspectMult;

extern float g_fConcourseScale, g_fConcourseAspectRatio;
extern int   g_iDraw2DCounter;
extern bool  g_bStartedGUI, g_bPrevStartedGUI, g_bIsScaleableGUIElem, g_bPrevIsScaleableGUIElem, g_bScaleableHUDStarted, g_bDynCockpitEnabled;
extern bool g_bDCWasClearedOnThisFrame;
extern bool  g_bEnableVR, g_bDisableBarrelEffect;
extern bool  g_bDumpGUI;
extern int   g_iDrawCounter, g_iExecBufCounter, g_iPresentCounter, g_iNonZBufferCounter, g_iDrawCounterAfterHUD;
extern bool  g_bTargetCompDrawn, g_bPrevIsFloatingGUI3DObject, g_bIsFloating3DObject;
extern unsigned int g_iFloatingGUIDrawnCounter;

bool g_bRendering3D = false; // Set to true when the system is about to render in 3D

// SteamVR
extern DWORD g_FullScreenWidth, g_FullScreenHeight;
extern Matrix4 g_viewMatrix;
extern VertexShaderMatrixCB g_VSMatrixCB;

extern bool g_bStart3DCapture, g_bDo3DCapture;
extern FILE *g_HackFile;
#ifdef DBR_VR
extern bool g_bCapture2DOffscreenBuffer;
#endif

/* SteamVR HMD */
#include <headers/openvr.h>
extern float g_fOBJ_Z_MetricMult, g_fOBJGlobalMetricMult, g_fOBJCurMetricScale;
extern vr::IVRSystem *g_pHMD;
extern vr::IVRCompositor *g_pVRCompositor;
extern bool g_bSteamVREnabled, g_bUseSteamVR;
extern uint32_t g_steamVRWidth, g_steamVRHeight;
extern vr::TrackedDevicePose_t g_rTrackedDevicePose;
void *g_pSurface = NULL;

void GetSteamVRPositionalData(float* yaw, float* pitch, float* roll, float* x, float* y, float* z, Matrix3* rotMatrix)
{
	vr::TrackedDeviceIndex_t unDevice = vr::k_unTrackedDeviceIndex_Hmd;
	if (!g_pHMD->IsTrackedDeviceConnected(unDevice)) {
		//log_debug("[DBG] HMD is not connected");
		return;
	}

	vr::VRControllerState_t state;
	if (g_pHMD->GetControllerState(unDevice, &state, sizeof(state)))
	{
		vr::TrackedDevicePose_t trackedDevicePoseArray[vr::k_unMaxTrackedDeviceCount];
		vr::TrackedDevicePose_t trackedDevicePose;
		vr::HmdMatrix34_t poseMatrix;
		vr::HmdQuaternionf_t q;
		vr::ETrackedDeviceClass trackedDeviceClass = vr::VRSystem()->GetTrackedDeviceClass(unDevice);

		if (g_bRendering3D) {
			//Get the last tracking pose obtained by CockpitLook and used to render the frame in the .exe
			GetLastSteamVRPose(&trackedDevicePose);
			vr::VRCompositor()->WaitGetPoses(NULL, vr::k_unMaxTrackedDeviceCount, NULL, 0);
		}
		else {
			// We are probably in 2D mode, CockpitLook is not working. Get the poses here.
			vr::VRCompositor()->WaitGetPoses(trackedDevicePoseArray, vr::k_unMaxTrackedDeviceCount, NULL, 0);
			trackedDevicePose = trackedDevicePoseArray[vr::k_unTrackedDeviceIndex_Hmd];
		}
		//vr::VRCompositor()->GetLastPoses(trackedDevicePoseArray, vr::k_unMaxTrackedDeviceCount, NULL, 0);

		poseMatrix = trackedDevicePose.mDeviceToAbsoluteTracking;  // This matrix contains all positional and rotational data.
		q = rotationToQuaternion(poseMatrix);
		quatToEuler(q, yaw, pitch, roll);

		*x = poseMatrix.m[0][3];
		*y = poseMatrix.m[1][3];
		*z = poseMatrix.m[2][3];
		*rotMatrix = HmdMatrix34toMatrix3(poseMatrix);
	}
}

float ComputeRealVertFOV() {
	return 2.0f * atan2(0.5f * g_fCurInGameHeight, *g_fRawFOVDist) / DEG2RAD;
}

float ComputeRealHorzFOV() {
	return 2.0f * atan2(0.5f * g_fCurInGameWidth, *g_fRawFOVDist) / DEG2RAD;
}

float RealVertFOVToRawFocalLength(float real_FOV_deg) {
	return 0.5f * g_fCurInGameHeight / tan(0.5f * real_FOV_deg * DEG2RAD);
}

/**
 * Compute FOVscale and y_center for the hyperspace effect (and others that may need the FOVscale)
 */
void ComputeHyperFOVParams() {
	float y_center_raw, FOVscale_raw;
	// Find the current center of the screen after it has been displaced by the cockpit camera.
	// We only care about the y-coordinate, as we're going to use it along with the reticle to
	// compute y_center.

	// To find the y-center of the screen, we're going to use math. The tangent of the current
	// pitch can give us the information right away. This is closely related to the FOV and
	// is easy to see if you draw the focal length and the in-game height to form a right triangle.
	float pitch = (float)PlayerDataTable[*g_playerIndex].cockpitCameraPitch / 65536.0f * 2.0f * PI;
	float H = *g_fRawFOVDist * tan(pitch); // This will give us the height, in pixels, measured from the center of the screen
	H += g_fCurInGameHeight / 2.0f;
	log_debug("[DBG] [FOV] Screen Y-Center: %0.3f, ReticleCentroid: %0.3f, pitch: %0.3f, ", H, g_ReticleCentroid.y, pitch / DEG2RAD);
	if (g_ReticleCentroid.y > -1.0f) {
		// *sigh* for whatever stupid reason, sometimes we can have ReticleCentroid.y == 0.0 while looking straight ahead (pitch == 0)
		// This situation completely destroys the calculation below, so we need to make sure that the camera pitch and the centroid
		// are consistent.
		// Another way to solve this problem is to prevent this calculation until the reticle centroid is close-ish to the center
		// of the screen
		float y_dist_from_screen_center = (float)fabs(g_ReticleCentroid.y - g_fCurInGameHeight / 2.0f);
		if (y_dist_from_screen_center < g_fCurInGameHeight / 2.0f) {
			// The reticle center visible this frame and it's not close to the edge of the screen.
			// We can use it to compute y_center...

			// The formula to compute y_center seems to be:
			// (in-game-center - HUD_center) / in-game-height * 2.0f * comp_factor.
			// The in-game-center has to be computer properly if the cockpit isn't facing forward
			y_center_raw = 2.0f * (H - g_ReticleCentroid.y) / g_fCurInGameHeight;
			log_debug("[DBG] [FOV] HUD_center to y_center: %0.3f", y_center_raw);
			// We can stop looking for the reticle center now:
			g_bYCenterHasBeenFixed = true;
		}
		else
			log_debug("[DBG] [FOV] RETICLE COULD NOT BE USED COMPUTE Y_CENTER. WILL RETRY. Frame: %d", g_iPresentCounter);
		// If the reticle center can't be used to compute y_center, then g_bYCenterHasBeenFixed will stay false, and we'll
		// come back to this path on the next frame where a reticle is visible.
	}
	
	if (!g_bYCenterHasBeenFixed) {
		// Provide a default value if we couldn't compute y_center:
		y_center_raw = 153.0f / g_fCurInGameHeight;
		//y_center_raw = 0.0f; // I can't do this because the cockpits look wrong in the hangar. They look skewed
	}
	FOVscale_raw = 2.0f * *g_fRawFOVDist / g_fCurInGameHeight;

	// Compute the aspect-ratio fix factors
	float g_fWindowAspectRatio = max(1.0f, (float)g_WindowWidth / (float)g_WindowHeight);
	if (g_bUseSteamVR)
		g_fWindowAspectRatio = max(1.0f, (float)g_steamVRWidth / (float)g_steamVRHeight);
	// The point where fixed and non-fixed params are about the same is given by the window aspect ratio
	bool bFixFactors = g_fCurInGameAspectRatio > g_fWindowAspectRatio;
	// The compensation factor is given by the ratio between the window aspect ratio and the in-game's 
	// aspect ratio. In other words, the size of the display window will stretch the view, so we
	// have to compensate for that.
	float comp_factor = bFixFactors ? g_fWindowAspectRatio / g_fCurInGameAspectRatio : 1.0f;

	log_debug("[DBG] [FOV] y_center raw: %0.3f, FOVscale raw: %0.3f, W/H: %0.0f, %0.0f, a/r: %0.3f, FIX: %d, comp_factor: %0.3f",
		y_center_raw, FOVscale_raw, g_fCurInGameWidth, g_fCurInGameHeight, g_fCurInGameAspectRatio, bFixFactors, comp_factor);

	// Compute the compensated FOV and y_center:
	g_fFOVscale = comp_factor * FOVscale_raw;
	g_fYCenter  = comp_factor * y_center_raw;

	// Store the global FOVscale, y_center in the CB:
	g_ShadertoyBuffer.FOVscale = g_fFOVscale;
	g_ShadertoyBuffer.y_center = g_fYCenter;
	
	// If PreserveAspectRatio is 0, then we need to apply further compensation because the image
	// will further stretch in either the X or Y axis.
	g_ShadertoyBuffer.preserveAspectRatioComp[0] = 1.0f;
	g_ShadertoyBuffer.preserveAspectRatioComp[1] = 1.0f;
	// The VR modes behave just like PreserveAspectRatio = 0. The funny thing is that
	// it will stretch the image in the same amount as when running in non-VR mode.
	// This means that the aspect ratio of the in-game resolution should be an integer
	// multiple of the Display/SteamVR window to avoid distortion.
	if (!g_config.AspectRatioPreserved || g_bEnableVR)
	{
		float RealWindowAspectRatio = (float)g_WindowWidth / (float)g_WindowHeight;
		// In SteamVR mode, the must compensate against the SteamVR window size
		if (g_bUseSteamVR)
			RealWindowAspectRatio = (float)g_steamVRWidth / (float)g_steamVRHeight;
		
		if (RealWindowAspectRatio > g_fCurInGameAspectRatio) {
			// The display window is going to stretch the image horizontally, so we need
			// to shrink the x axis:
			if (RealWindowAspectRatio > 1.0f) // Make sure we shrink. If we divide by a value lower than 1, we'll stretch!
				g_ShadertoyBuffer.preserveAspectRatioComp[0] = g_fCurInGameAspectRatio / RealWindowAspectRatio;
			else
				g_ShadertoyBuffer.preserveAspectRatioComp[0] = RealWindowAspectRatio / g_fCurInGameAspectRatio;
		}
		else {
			// The display window is going to stretch the image vertically, so we need
			// to shrink the y axis:
			if (g_fCurInGameAspectRatio > 1.0f)
				g_ShadertoyBuffer.preserveAspectRatioComp[1] = RealWindowAspectRatio / g_fCurInGameAspectRatio;
			else
				g_ShadertoyBuffer.preserveAspectRatioComp[1] = g_fCurInGameAspectRatio / RealWindowAspectRatio;
		}

		log_debug("[DBG] [FOV] Real Window a/r: %0.3f, preserveA/R-Comp: %0.3f, %0.3f",
			RealWindowAspectRatio, g_ShadertoyBuffer.preserveAspectRatioComp[0], g_ShadertoyBuffer.preserveAspectRatioComp[1]);
	}

	// Compute the *real* vertical and horizontal FOVs:
	g_fRealVertFOV = ComputeRealVertFOV();
	g_fRealHorzFOV = ComputeRealHorzFOV();
	// Compute the metric scale factor conversion
	g_fOBJCurMetricScale = g_fCurInGameHeight * g_fOBJGlobalMetricMult / (SHADOW_OBJ_SCALE * 3200.0f);

	// Populate the metric reconstruction CB. Noticeably, the Metric Rec values need the raw
	// FOVscale and y_center, not the fixed ones. Making a 3D OBJ crosshairs as additional geometry
	// also places the crosshairs at the right spot (the crosshairs need to be at (0,1,100))
	g_MetricRecCBuffer.mr_FOVscale = FOVscale_raw;
	g_MetricRecCBuffer.mr_y_center = y_center_raw;
	g_MetricRecCBuffer.mr_cur_metric_scale = g_fOBJCurMetricScale;
	g_MetricRecCBuffer.mr_aspect_ratio = g_fCurInGameAspectRatio;
	g_MetricRecCBuffer.mr_z_metric_mult = g_fOBJ_Z_MetricMult;
	g_MetricRecCBuffer.mr_shadow_OBJ_scale = SHADOW_OBJ_SCALE;
	g_MetricRecCBuffer.mr_screen_aspect_ratio = g_fCurScreenWidth / g_fCurScreenHeight;
	//g_MetricRecCBuffer.mr_vr_aspect_ratio_comp[0] = 1.0f;
	//g_MetricRecCBuffer.mr_vr_aspect_ratio_comp[1] = 1.0f;
	g_MetricRecCBuffer.mv_vr_vertexbuf_aspect_ratio_comp[0] = 1.0f;
	g_MetricRecCBuffer.mv_vr_vertexbuf_aspect_ratio_comp[1] = 1.0f;
	g_MetricRecCBuffer.mr_vr_aspect_ratio = 1.0f;
	if (g_bEnableVR) {
		if (g_bUseSteamVR) {
			g_MetricRecCBuffer.mr_vr_aspect_ratio = (float)g_steamVRHeight / (float)g_steamVRWidth;
		}
		else {
			// This is the DirectSBS mode. I don't have a reliable way to get the resolution of the 
			// HMD device (which could be any cell phone + Google Cardboard for all we know). Instead,
			// we need to trust that the user will set the current desktop resolution to the HMD's 
			// resolution. So let's use the desktop's resolution to compensate for aspect ratio in
			// SBS mode:
			g_MetricRecCBuffer.mr_vr_aspect_ratio = (float)g_WindowHeight / (float)g_WindowWidth;
		}
	}

	if (g_bEnableVR) {
		if (g_bUseSteamVR) {
			if (g_config.AspectRatioPreserved) {
				g_MetricRecCBuffer.mv_vr_vertexbuf_aspect_ratio_comp[0] = 1.0f / g_ShadertoyBuffer.preserveAspectRatioComp[0];
				g_MetricRecCBuffer.mv_vr_vertexbuf_aspect_ratio_comp[1] = 1.0f / g_ShadertoyBuffer.preserveAspectRatioComp[1];
			}
		}
		else {
			// DirectSBS path
			g_MetricRecCBuffer.mv_vr_vertexbuf_aspect_ratio_comp[0] = 1.0f / g_ShadertoyBuffer.preserveAspectRatioComp[0];
			g_MetricRecCBuffer.mv_vr_vertexbuf_aspect_ratio_comp[1] = 1.0f / g_ShadertoyBuffer.preserveAspectRatioComp[1];
		}
	}

	// We just modified the Metric Reconstruction parameters, let's reapply them
	g_bMetricParamsNeedReapply = true;

	log_debug("[DBG] [FOV] Final y_center: %0.3f, FOV_Scale: %0.6f, RealVFOV: %0.2f, RealHFOV: %0.2f, mr_aspect_ratio: %0.3f, Frame: %d",
		g_ShadertoyBuffer.y_center, g_ShadertoyBuffer.FOVscale, g_fRealVertFOV, g_fRealHorzFOV, 
		g_MetricRecCBuffer.mr_aspect_ratio, g_iPresentCounter);

	// DEBUG
	//static bool bFirstTime = true;
	//if (bFirstTime) {
	//	g_fDebugFOVscale = g_ShadertoyBuffer.FOVscale;
	//	//g_fDebugYCenter = g_ShadertoyBuffer.y_center;
	//	bFirstTime = false;
	//}
	//g_fFOVscale = g_fDebugFOVscale;
	//g_ShadertoyBuffer.FOVscale = g_fDebugFOVscale;
	//
	//g_fYCenter = g_fDebugYCenter;
	//g_ShadertoyBuffer.y_center = g_fDebugYCenter;
	////g_MetricRecCBuffer.mr_y_center = g_fDebugYCenter;
	////log_debug("[DBG] [FOV] g_fDebugYCenter: %0.3f", g_fDebugYCenter);
	//log_debug("[DBG] [FOV] g_fDebugYCenter: %0.3f, g_fDebugFOVscale: %0.6f", g_fDebugYCenter, g_fDebugFOVscale);
	// DEBUG
}

// void capture()
//#ifdef DBR_VR
void PrimarySurface::capture(int time_delay, ComPtr<ID3D11Texture2D> buffer, const wchar_t *filename)
{
	bool bDoCapture = false;
	HRESULT hr = D3D_OK;

	hr = DirectX::SaveWICTextureToFile(this->_deviceResources->_d3dDeviceContext.Get(),
		buffer.Get(), GUID_ContainerFormatJpeg, filename);

	if (SUCCEEDED(hr))
		log_debug("[DBG] CAPTURED (1)!");
	else
		log_debug("[DBG] NOT captured, hr: %d", hr);
}
//#endif

PrimarySurface::PrimarySurface(DeviceResources* deviceResources, bool hasBackbufferAttached)
{
	this->_refCount = 1;
	this->_deviceResources = deviceResources;

	this->_hasBackbufferAttached = hasBackbufferAttached;

	if (this->_hasBackbufferAttached)
	{
		*this->_backbufferSurface.GetAddressOf() = new BackbufferSurface(this->_deviceResources);
		this->_deviceResources->_backbufferSurface = this->_backbufferSurface;
	}

	this->_flipFrames = 0;

	InitHeadingMatrix();
}

PrimarySurface::~PrimarySurface()
{
	if (this->_hasBackbufferAttached)
	{
		for (ULONG ref = this->_backbufferSurface->AddRef(); ref > 1; ref--)
		{
			this->_backbufferSurface->Release();
		}
	}

	if (this->_deviceResources->_primarySurface == this)
	{
		this->_deviceResources->_primarySurface = nullptr;
	}
}

HRESULT PrimarySurface::QueryInterface(
	REFIID riid,
	LPVOID* obp
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tE_NOINTERFACE");
	LogText(str.str());
#endif

	return E_NOINTERFACE;
}

ULONG PrimarySurface::AddRef()
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

	this->_refCount++;

#if LOGGER
	str.str("");
	str << "\t" << this->_refCount;
	LogText(str.str());
#endif

	return this->_refCount;
}

ULONG PrimarySurface::Release()
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

	this->_refCount--;

#if LOGGER
	str.str("");
	str << "\t" << this->_refCount;
	LogText(str.str());
#endif

	if (this->_refCount == 0)
	{
		delete this;
		return 0;
	}

	return this->_refCount;
}

HRESULT PrimarySurface::AddAttachedSurface(
	LPDIRECTDRAWSURFACE lpDDSAttachedSurface
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::AddOverlayDirtyRect(
	LPRECT lpRect
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::Blt(
	LPRECT lpDestRect,
	LPDIRECTDRAWSURFACE lpDDSrcSurface,
	LPRECT lpSrcRect,
	DWORD dwFlags,
	LPDDBLTFX lpDDBltFx
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	str << tostr_RECT(lpDestRect);

	if (lpDDSrcSurface == this->_deviceResources->_backbufferSurface)
	{
		str << " BackbufferSurface";
	}
	else
	{
		str << " " << lpDDSrcSurface;
	}

	str << tostr_RECT(lpSrcRect);

	if ((dwFlags & DDBLT_COLORFILL) != 0 && lpDDBltFx != nullptr)
	{
		str << " " << (void*)lpDDBltFx->dwFillColor;
	}

	LogText(str.str());
#endif

	if (dwFlags & DDBLT_COLORFILL)
	{
		if (lpDDBltFx == nullptr)
		{
			return DDERR_INVALIDPARAMS;
		}

		return DD_OK;
	}

	if (lpDDSrcSurface != nullptr)
	{
		if (lpDDSrcSurface == this->_deviceResources->_backbufferSurface)
		{
			return this->Flip(NULL, 0);
		}
	}

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::BltBatch(
	LPDDBLTBATCH lpDDBltBatch,
	DWORD dwCount,
	DWORD dwFlags
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::BltFast(
	DWORD dwX,
	DWORD dwY,
	LPDIRECTDRAWSURFACE lpDDSrcSurface,
	LPRECT lpSrcRect,
	DWORD dwTrans
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	str << " " << dwX << " " << dwY;

	if (lpDDSrcSurface == nullptr)
	{
		str << " NULL";
	}
	else if (lpDDSrcSurface == this->_deviceResources->_backbufferSurface)
	{
		str << " BackbufferSurface";
	}
	else
	{
		str << " " << lpDDSrcSurface;
	}

	str << tostr_RECT(lpSrcRect);

	if (dwTrans & DDBLTFAST_SRCCOLORKEY)
	{
		str << " SRCCOLORKEY";
	}

	if (dwTrans & DDBLTFAST_DESTCOLORKEY)
	{
		str << " DESTCOLORKEY";
	}

	if (dwTrans & DDBLTFAST_WAIT)
	{
		str << " WAIT";
	}

	if (dwTrans & DDBLTFAST_DONOTWAIT)
	{
		str << " DONOTWAIT";
	}

	LogText(str.str());
#endif

	if (lpDDSrcSurface != nullptr)
	{
		if (lpDDSrcSurface == this->_deviceResources->_backbufferSurface)
		{
			return this->Flip(this->_deviceResources->_backbufferSurface, 0);
		}
	}

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::DeleteAttachedSurface(
	DWORD dwFlags,
	LPDIRECTDRAWSURFACE lpDDSAttachedSurface
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::EnumAttachedSurfaces(
	LPVOID lpContext,
	LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::EnumOverlayZOrders(
	DWORD dwFlags,
	LPVOID lpContext,
	LPDDENUMSURFACESCALLBACK lpfnCallback
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

/*
 * Applies the barrel distortion effect on the 2D window (Concourse, menus, etc).
 */
void PrimarySurface::barrelEffect2D(int iteration) {
	/*
	We need to avoid resolving the offscreen buffer multiple times. It's probably easier to
	skip method this altogether if we already rendered all this in the first iteration.
	*/
	if (iteration > 0)
		return;

	D3D11_VIEWPORT viewport{};
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	// Temporarily disable ZWrite: we won't need it for the barrel effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	float screen_res_x = (float)resources->_backbufferWidth;
	float screen_res_y = (float)resources->_backbufferHeight;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	// Set the vertex buffer
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_postProcessVertBuffer.GetAddressOf(), &stride, &offset);

	// Set Primitive Topology
	// This is probably an opportunity for an optimization: let's use the same topology everywhere?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	// At this point, offscreenBuffer contains the image that would be rendered to the screen by
	// copying to the backbuffer. So, resolve the offscreen buffer into offscreenBuffer2 to use it
	// as input, so that we can render the image twice.
	context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
		0, BACKBUFFER_FORMAT);

#ifdef DBG_VR
	if (g_bCapture2DOffscreenBuffer) {
		static int frame = 0;
		if (frame == 0) {
			log_debug("[DBG] [Capture] display Width, Height: %d, %d",
				this->_deviceResources->_displayWidth, this->_deviceResources->_displayHeight);
			log_debug("[DBG] [Capture] backBuffer Width, Height: %d, %d",
				this->_deviceResources->_backbufferWidth, this->_deviceResources->_backbufferHeight);
		}
		wchar_t filename[120];
		swprintf_s(filename, 120, L"c:\\temp\\offscreenBuf-%d-A.jpg", frame++);
		capture(0, this->_deviceResources->_offscreenBuffer2, filename);
		if (frame >= 40)
			g_bCapture2DOffscreenBuffer = false;
	}
#endif

	// Render the barrel effect
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = screen_res_x;
	viewport.Height = screen_res_y;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;

	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(), 0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices
	resources->InitVertexShader(resources->_mainVertexShader);
	resources->InitPixelShader(resources->_barrelPixelShader);
	
	// Set the lens distortion constants for the barrel shader
	resources->InitPSConstantBufferBarrel(resources->_barrelConstantBuffer.GetAddressOf(), g_fLensK1, g_fLensK2, g_fLensK3);

	context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
	ID3D11RenderTargetView *rtvs[5] = {
		resources->_renderTargetViewPost.Get(),
		NULL, NULL, NULL, NULL,
	};
	context->OMSetRenderTargets(5, rtvs, NULL);
	//context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceView.GetAddressOf());
	resources->InitPSShaderResourceView(resources->_offscreenAsInputShaderResourceView);

	resources->InitViewport(&viewport);
	context->Draw(6, 0);

	// Restore the original rendertargetview
	context->OMSetRenderTargets(1, this->_deviceResources->_renderTargetView.GetAddressOf(), this->_deviceResources->_depthStencilViewL.Get());
}

/*
 * Applies the barrel distortion effect on the 3D window.
 * Resolves the offscreenBuffer into offscreenBufferAsInput
 * Renders to offscreenBufferPost
 */
void PrimarySurface::barrelEffect3D() {
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	// Set the vertex buffer
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_postProcessVertBuffer.GetAddressOf(), &stride, &offset);

	// Set Primitive Topology
	// Opportunity for optimization? Make all draw calls use the same topology?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);
	
	// Temporarily disable ZWrite: we won't need it for the barrel effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);
	
	float screen_res_x = (float)resources->_backbufferWidth;
	float screen_res_y = (float)resources->_backbufferHeight;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	// Create a new viewport to render the offscreen buffer as a texture
	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = (float)0;
	viewport.TopLeftY = (float)0;
	viewport.Width = (float)screen_res_x;
	viewport.Height = (float)screen_res_y;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	
	context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
		0, BACKBUFFER_FORMAT);

	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(), 0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices
	resources->InitVertexShader(resources->_mainVertexShader);
	resources->InitPixelShader(resources->_barrelPixelShader);
	
	// Set the lens distortion constants for the barrel shader
	resources->InitPSConstantBufferBarrel(resources->_barrelConstantBuffer.GetAddressOf(), g_fLensK1, g_fLensK2, g_fLensK3);

	// Clear the render target
	context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
	ID3D11RenderTargetView *rtvs[5] = {
		resources->_renderTargetViewPost.Get(),
		NULL, NULL, NULL, NULL,
	};
	context->OMSetRenderTargets(5, rtvs, NULL);
	context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceView.GetAddressOf());
	resources->InitViewport(&viewport);
	context->IASetInputLayout(resources->_mainInputLayout);
	context->Draw(6, 0);

#ifdef DBG_VR
	if (g_bCapture2DOffscreenBuffer) {
		static int frame = 0;
		wchar_t filename[120];

		swprintf_s(filename, 120, L"c:\\temp\\offscreenBuf-%d.jpg", frame);
		capture(0, this->_deviceResources->_offscreenBuffer, filename);

		swprintf_s(filename, 120, L"c:\\temp\\offscreenBuf2-%d.jpg", frame);
		capture(0, this->_deviceResources->_offscreenBuffer2, filename);

		swprintf_s(filename, 120, L"c:\\temp\\offscreenBuf3-%d.jpg", frame);
		capture(0, this->_deviceResources->_offscreenBuffer3, filename);

		frame++;
		if (frame >= 5)
			g_bCapture2DOffscreenBuffer = false;
	}
#endif

	// Restore previous rendertarget, etc
	resources->InitInputLayout(resources->_inputLayout);
	context->OMSetRenderTargets(1, this->_deviceResources->_renderTargetView.GetAddressOf(), 
		this->_deviceResources->_depthStencilViewL.Get());
}

/*
 * Applies the barrel distortion effect on the 3D window for SteamVR (so each image is
 * independent and the singleBarrelPixelShader is used.
 * Input: _offscreenBuffer and _offscreenBufferR
 * Output: _offscreenBufferPost and _offscreenBufferPostR
 */
void PrimarySurface::barrelEffectSteamVR() {
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	// Set the vertex buffer
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_postProcessVertBuffer.GetAddressOf(), &stride, &offset);

	// Set Primitive Topology
	// Opportunity for optimization? Make all draw calls use the same topology?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	// Temporarily disable ZWrite: we won't need it for the barrel effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	// Create a new viewport to render the offscreen buffer as a texture
	D3D11_VIEWPORT viewport{};
	float screen_res_x = (float)resources->_backbufferWidth;
	float screen_res_y = (float)resources->_backbufferHeight;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (float)screen_res_x;
	viewport.Height = (float)screen_res_y;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	resources->InitViewport(&viewport);

	// Resolve both images
	context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
		0, BACKBUFFER_FORMAT);
	context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR,
		0, BACKBUFFER_FORMAT);

#ifdef DBG_VR
	if (g_bCapture2DOffscreenBuffer) {
		static int frame = 0;
		wchar_t filename[120];

		log_debug("[DBG] Capturing buffers");
		swprintf_s(filename, 120, L"c:\\temp\\offscreenBuf-%d.jpg", frame);
		capture(0, this->_deviceResources->_offscreenBuffer, filename);

		swprintf_s(filename, 120, L"c:\\temp\\offscreenBufAsInput-%d.jpg", frame);
		capture(0, this->_deviceResources->_offscreenBufferAsInput, filename);

		frame++;
		if (frame >= 1)
			g_bCapture2DOffscreenBuffer = false;
	}
#endif

	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(), 0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices
	resources->InitVertexShader(resources->_mainVertexShader);
	resources->InitPixelShader(resources->_singleBarrelPixelShader);

	// Set the lens distortion constants for the barrel shader
	resources->InitPSConstantBufferBarrel(resources->_barrelConstantBuffer.GetAddressOf(), g_fLensK1, g_fLensK2, g_fLensK3);

	// Clear the render targets
	context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
	context->ClearRenderTargetView(resources->_renderTargetViewPostR, bgColor);
	context->IASetInputLayout(resources->_mainInputLayout);

	ID3D11RenderTargetView *rtvs[5] = {
		resources->_renderTargetViewPost.Get(),
		NULL, NULL, NULL, NULL,
	};
	context->OMSetRenderTargets(5, rtvs, NULL);
	//context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceView.GetAddressOf());
	resources->InitPSShaderResourceView(resources->_offscreenAsInputShaderResourceView);
	context->Draw(6, 0);

	rtvs[0] = resources->_renderTargetViewPostR.Get();
	context->OMSetRenderTargets(5, rtvs, NULL);
	//context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceViewR.GetAddressOf());
	resources->InitPSShaderResourceView(resources->_offscreenAsInputShaderResourceViewR);
	context->Draw(6, 0);

#ifdef DBG_VR
	if (g_bCapture2DOffscreenBuffer) {
		static int frame = 0;
		wchar_t filename[120];

		swprintf_s(filename, 120, L"c:\\temp\\offscreenBufR-%d.jpg", frame);
		capture(0, resources->_offscreenBufferR, filename);

		swprintf_s(filename, 120, L"c:\\temp\\offscreenBufAsInputR-%d.jpg", frame);
		capture(0, resources->_offscreenBufferAsInputR, filename);

		swprintf_s(filename, 120, L"c:\\temp\\offscreenBufPostR-%d.jpg", frame);
		capture(0, resources->_offscreenBufferPostR, filename);

		frame++;
		if (frame >= 1)
			g_bCapture2DOffscreenBuffer = false;
	}
#endif

	// Restore previous rendertarget, etc
	resources->InitInputLayout(resources->_inputLayout);
	context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(),
		resources->_depthStencilViewL.Get());
}

/*
 * When rendering for SteamVR, we may be rendering at a resolution that is different
 * from the current screen resolution, so some stretching will happen in the mirror
 * window. We need to resize the offscreenBuffer before presenting it.
 * Bonus points: Adjust the FOV to the original (~50) to avoid showing the distortion
 * caused by the larger FOV used in most headsets.
 * Input: _offscreenBuffer
 * Output: _steamVRPresentBuffer
 */
void PrimarySurface::resizeForSteamVR(int iteration, bool is_2D) {
	/*
	We need to avoid resolving the offscreen buffer multiple times. It's probably easier to
	skip method this altogether if we already rendered all this in the first iteration.
	*/
	if (iteration > 0)
		return;

	D3D11_VIEWPORT viewport{};
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	float screen_res_x = (float)g_WindowWidth;
	float screen_res_y = (float)g_WindowHeight;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	// Set the vertex buffer
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_mainVertexBuffer.GetAddressOf(), &stride, &offset);
	resources->InitIndexBuffer(resources->_mainIndexBuffer, false);

	// Set Primitive Topology
	// This is probably an opportunity for an optimization: let's use the same topology everywhere?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	// Temporarily disable ZWrite: we won't need it here
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	// At this point, offscreenBuffer contains the image that would be rendered to the screen by
	// copying to the backbuffer. So, resolve the offscreen buffer into offscreenBufferAsInput to
	// use it as input in the shader
	if (g_bSteamVRMirrorWindowLeftEye) {
		context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
			0, BACKBUFFER_FORMAT);
	}
	else {
		context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR,
			0, BACKBUFFER_FORMAT);
	}

#ifdef DBG_VR
	if (g_bCapture2DOffscreenBuffer) {
		static int frame = 0;
		if (frame == 0) {
			log_debug("[DBG] [Capture] display Width, Height: %d, %d",
				this->_deviceResources->_displayWidth, this->_deviceResources->_displayHeight);
			log_debug("[DBG] [Capture] backBuffer Width, Height: %d, %d",
				this->_deviceResources->_backbufferWidth, this->_deviceResources->_backbufferHeight);
			log_debug("[DBG] screen_res_x,y: %0.1f, %0.1f", screen_res_x, screen_res_y);
			log_debug("[DBG] g_steamVRWidth,Height: %d, %d", g_steamVRWidth, g_steamVRHeight);
			log_debug("[DBG] scale, newWidth,Height: %0.3f, (%0.3f, %0.3f)", scale, newWidth, newHeight);
		}
		wchar_t filename[120];
		swprintf_s(filename, 120, L"c:\\temp\\offscreenBuf-%d-A.jpg", frame++);
		capture(0, this->_deviceResources->_offscreenBufferAsInput, filename);
		if (frame >= 1)
			g_bCapture2DOffscreenBuffer = false;
	}
#endif

	// The viewport has to be in the range (0,0)-(g_steamVRWidth,g_steamVRHeight) or the image
	// will be cropped. This is because the backbuffer has those same dimensions.
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width    = (float)g_steamVRWidth;
	viewport.Height   = (float)g_steamVRHeight;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	resources->InitViewport(&viewport);
	
	//float RealWindowAspectRatio = (float)g_WindowWidth / (float)g_WindowHeight;
	float steamVR_aspect_ratio = (float)g_steamVRWidth / (float)g_steamVRHeight;
	float window_factor_x = (float)g_steamVRWidth / (float)g_WindowWidth;
	// If the display window has height > width, we can't make the the y axis smaller to compensate.
	// Instead, I'm expanding the x-axis to keep the original aspect ratio. That'll cause the image
	// to become bigger too, so we'll shrink the image by adjusting the scale as well:
	float window_factor_y = (float)g_WindowHeight / (float)g_steamVRHeight;
	float scale = 1.0f / window_factor_y;

	float window_scale_x = (float)g_WindowWidth / (float)g_steamVRWidth;
	float window_scale_y = (float)g_WindowHeight / (float)g_steamVRHeight;
	float window_scale = min(window_scale_x, window_scale_y);
	scale *= window_scale;
	
	float aspect_ratio = 1.0f;
	if (g_bRendering3D) {
		if (g_fSteamVRMirrorWindowAspectRatio > 0.01f)
			aspect_ratio = g_fSteamVRMirrorWindowAspectRatio;
		else
			// The loading mission screen will take this path, so it will render with the wrong aspect ratio. I have no idea how to fix this :P
			aspect_ratio = 1.0f / steamVR_aspect_ratio * window_factor_x * window_factor_y;
		// Looks like the Reticle gets stretched when PreserveAspectRatio = 0... but only in the mirror window?
		/*
		if (!g_config.AspectRatioPreserved) {
			float RealWindowAspectRatio = steamVR_aspect_ratio;
			if (RealWindowAspectRatio > g_fCurInGameAspectRatio)
				// The display window is going to stretch the image horizontally, so we need
				// to shrink the x axis:
				// Make sure we shrink. If we divide by a value lower than 1, we'll stretch!
				aspect_ratio *= RealWindowAspectRatio > 1.0f ? g_fCurInGameAspectRatio / RealWindowAspectRatio : RealWindowAspectRatio / g_fCurInGameAspectRatio;
			else
				// The display window is going to stretch the image vertically, so we need
				// to shrink the y axis:
				aspect_ratio *= g_fCurInGameAspectRatio > 1.0f ? RealWindowAspectRatio / g_fCurInGameAspectRatio : g_fCurInGameAspectRatio / RealWindowAspectRatio;
		}
		*/
	}
	else
		// The 2D image is already rendered with g_fConcourseAspectRatio. We need to undo it and that's why we add 1/g_fConcourseAspectRatio below:
		aspect_ratio = (1.0f / g_fConcourseAspectRatio) * g_fCurInGameAspectRatio / steamVR_aspect_ratio * window_factor_x * window_factor_y;

	// We have a problem here: the CB for the VS and PS are the same (_mainShadersConstantBuffer), so
	// we have to use the same settings on both.
	resources->InitPSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
		0.0f, aspect_ratio, scale, 1.0f, g_bRendering3D ? g_fSteamVRMirrorWindow3DScale : 1.0f);
	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
		0.0f, aspect_ratio, scale, 1.0f, 0.0f); // Don't use 3D projection matrices
	resources->InitVertexShader(resources->_mainVertexShader);
	resources->InitPixelShader(resources->_steamVRMirrorPixelShader);

	context->ClearRenderTargetView(resources->_renderTargetViewSteamVRResize, bgColor);
	context->OMSetRenderTargets(1, resources->_renderTargetViewSteamVRResize.GetAddressOf(),
		resources->_depthStencilViewL.Get());
	if (g_bSteamVRMirrorWindowLeftEye) {
		resources->InitPSShaderResourceView(resources->_offscreenAsInputShaderResourceView);
	}
	else {
		resources->InitPSShaderResourceView(resources->_offscreenAsInputShaderResourceViewR);
	}
	context->DrawIndexed(6, 0, 0);

#ifdef DBG_VR
	if (g_bCapture2DOffscreenBuffer) {
		static int frame = 0;
		wchar_t filename[120];
		swprintf_s(filename, 120, L"c:\\temp\\offscreenBuf-%d.jpg", frame);
		capture(0, this->_deviceResources->_offscreenBuffer, filename);

		swprintf_s(filename, 120, L"c:\\temp\\offscreenBufAsInput-%d.jpg", frame);
		capture(0, this->_deviceResources->_offscreenBufferAsInput, filename);

		swprintf_s(filename, 120, L"c:\\temp\\steamVRPresentBuf-%d.jpg", frame);
		capture(0, this->_deviceResources->_steamVRPresentBuffer, filename);

		log_debug("[DBG] viewport: (%f,%f)-(%f,%f)", viewport.TopLeftX, viewport.TopLeftY,
			viewport.Width, viewport.Height);

		frame++;
		if (frame >= 0)
			g_bCapture2DOffscreenBuffer = false;
	}
#endif

	// Restore previous rendertarget, etc
	context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(),
		resources->_depthStencilViewL.Get());
}

 /*
  * Applies a single bloom effect pass. Depending on the input:
  * pass = 0: Initial horizontal blur pass from the bloom mask.
  * pass = 1: Vertical pass from internal temporary ping-pong buffer.
  * pass = 2: Horizontal pass from internal temporary ping-pong buffer.
  * pass = 3: Final combine pass (deprecated, used for 32-bit UNORM mode)
  * pass = 4: Linear add between current bloom pass and bloomSum
  * pass = 5: Final combine between bloomSum and offscreenBuffer
  *
  * For pass 0, the input texture must be resolved already to 
  * _offscreenBufferAsInputReshadeMask, _offscreenBufferAsInputReshadeMaskR
  */
void PrimarySurface::BloomBasicPass(int pass, float fZoomFactor) {
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	// Set the vertex buffer... we probably need another vertex buffer here
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_postProcessVertBuffer.GetAddressOf(), &stride, &offset);

	// Set Primitive Topology
	// Opportunity for optimization? Make all draw calls use the same topology?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	// Temporarily disable ZWrite: we won't need it for this effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	// Create a new viewport to render the offscreen buffer as a texture
	float screen_res_x = (float)resources->_backbufferWidth;
	float screen_res_y = (float)resources->_backbufferHeight;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	if (pass < 3) {
		viewport.Width  = screen_res_x / fZoomFactor;
		viewport.Height = screen_res_y / fZoomFactor;
	} else { // The final pass should be performed at full resolution
		viewport.Width  = screen_res_x;
		viewport.Height = screen_res_y;
	}

	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	resources->InitViewport(&viewport);

	// Set the constant buffers
	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
		0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices
	context->IASetInputLayout(resources->_mainInputLayout);
	resources->InitVertexShader(resources->_mainVertexShader);

	// The input texture must be resolved already to
	// _offscreenBufferAsInputReshadeMask, _offscreenBufferAsInputReshadeMaskR
	switch (pass) {
		case 0: 	// Horizontal Gaussian Blur
			// Input: _offscreenAsInputReshadeSRV
			// Output _bloomOutput1
			resources->InitPixelShader(resources->_bloomHGaussPS);
			context->PSSetShaderResources(0, 1, resources->_offscreenAsInputBloomMaskSRV.GetAddressOf());
			context->ClearRenderTargetView(resources->_renderTargetViewBloom1, bgColor);
			context->OMSetRenderTargets(1, resources->_renderTargetViewBloom1.GetAddressOf(), NULL);
			break;
		case 1: // Vertical Gaussian Blur
			// Input:  _bloomOutput1
			// Output: _bloomOutput2
			resources->InitPixelShader(resources->_bloomVGaussPS);
			context->PSSetShaderResources(0, 1, resources->_bloomOutput1SRV.GetAddressOf());
			context->ClearRenderTargetView(resources->_renderTargetViewBloom2, bgColor);
			context->OMSetRenderTargets(1, resources->_renderTargetViewBloom2.GetAddressOf(), NULL);
			break;
		case 2: // Horizontal Gaussian Blur
			// Input:  _bloomOutput2
			// Output: _bloomOutput1
			resources->InitPixelShader(resources->_bloomHGaussPS);
			context->PSSetShaderResources(0, 1, resources->_bloomOutput2SRV.GetAddressOf());
			context->ClearRenderTargetView(resources->_renderTargetViewBloom1, bgColor);
			context->OMSetRenderTargets(1, resources->_renderTargetViewBloom1.GetAddressOf(), NULL);
			break;
		case 3: // Final pass to combine the bloom texture with the offscreenBuffer
			// Input:  _bloomOutput2, _offscreenBufferAsInput
			// Output: _offscreenBuffer (_bloomOutput1?)
			resources->InitPixelShader(resources->_bloomCombinePS);
			context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
				0, BACKBUFFER_FORMAT);
			context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceView.GetAddressOf());
			context->PSSetShaderResources(1, 1, resources->_bloomOutput2SRV.GetAddressOf());
			context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(), NULL);
			break;
		case 4:
			// Input: _bloomOutput2, _bloomSum
			// Output: _bloomOutput1
			resources->InitPixelShader(resources->_bloomBufferAddPS);
			context->PSSetShaderResources(0, 1, resources->_bloomOutput2SRV.GetAddressOf());
			context->PSSetShaderResources(1, 1, resources->_bloomOutputSumSRV.GetAddressOf());
			context->ClearRenderTargetView(resources->_renderTargetViewBloom1, bgColor);
			context->OMSetRenderTargets(1, resources->_renderTargetViewBloom1.GetAddressOf(), NULL);
			break;
		case 5: // Final pass to combine the bloom accumulated texture with the offscreenBuffer
			// Input:  _bloomSum, _offscreenBufferAsInput
			// Output: _offscreenBuffer
			resources->InitPixelShader(resources->_bloomCombinePS);
			context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
				0, BACKBUFFER_FORMAT);
			context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceView.GetAddressOf());
			context->PSSetShaderResources(1, 1, resources->_bloomOutputSumSRV.GetAddressOf());
			/*
			ID3D11RenderTargetView *rtvs[2] = {
				resources->_renderTargetView.Get(),
				resources->_renderTargetViewBloom1.Get()
			};
			context->OMSetRenderTargets(2, rtvs, NULL);
			*/
			context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(), NULL);
			break;
	}
	context->Draw(6, 0);

	// Draw the right image when SteamVR is enabled
	if (g_bUseSteamVR) {
		switch (pass) {
		case 0: 	// Prepass
			// Input: _offscreenAsInputReshadeSRV_R
			// Output _bloomOutput1R
			resources->InitPixelShader(resources->_bloomHGaussPS);
			context->PSSetShaderResources(0, 1, resources->_offscreenAsInputBloomMaskSRV_R.GetAddressOf());
			context->ClearRenderTargetView(resources->_renderTargetViewBloom1R, bgColor);
			context->OMSetRenderTargets(1, resources->_renderTargetViewBloom1R.GetAddressOf(), NULL);
			break;
		case 1: // Vertical Gaussian Blur
			// Input:  _bloomOutput1R
			// Output: _bloomOutput2R
			resources->InitPixelShader(resources->_bloomVGaussPS);
			context->PSSetShaderResources(0, 1, resources->_bloomOutput1SRV_R.GetAddressOf());
			context->ClearRenderTargetView(resources->_renderTargetViewBloom2R, bgColor);
			context->OMSetRenderTargets(1, resources->_renderTargetViewBloom2R.GetAddressOf(), NULL);
			break;
		case 2: // Horizontal Gaussian Blur
			// Input:  _bloomOutput2R
			// Output: _bloomOutput1R
			resources->InitPixelShader(resources->_bloomHGaussPS);
			context->PSSetShaderResources(0, 1, resources->_bloomOutput2SRV_R.GetAddressOf());
			context->ClearRenderTargetView(resources->_renderTargetViewBloom1R, bgColor);
			context->OMSetRenderTargets(1, resources->_renderTargetViewBloom1R.GetAddressOf(), NULL);
			break;
		case 3: // Final pass to combine the bloom texture with the backbuffer
			// Input:  _bloomOutput2R, _offscreenBufferAsInputR
			// Output: _offscreenBufferR
			resources->InitPixelShader(resources->_bloomCombinePS);
			context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR,
				0, BACKBUFFER_FORMAT);
			context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceViewR.GetAddressOf());
			context->PSSetShaderResources(1, 1, resources->_bloomOutput2SRV_R.GetAddressOf());
			context->OMSetRenderTargets(1, resources->_renderTargetViewR.GetAddressOf(), NULL);
			break;
		case 4:
			// Input: _bloomOutput2R, _bloomSumR
			// Output: _bloomOutput1R
			resources->InitPixelShader(resources->_bloomBufferAddPS);
			context->PSSetShaderResources(0, 1, resources->_bloomOutput2SRV_R.GetAddressOf());
			context->PSSetShaderResources(1, 1, resources->_bloomOutputSumSRV_R.GetAddressOf());
			context->ClearRenderTargetView(resources->_renderTargetViewBloom1R, bgColor);
			context->OMSetRenderTargets(1, resources->_renderTargetViewBloom1R.GetAddressOf(), NULL);
			break;
		case 5: // Final pass to combine the bloom accumulated texture with the offscreenBuffer
			// Input:  _bloomSumR, _offscreenBufferAsInputR
			// Output: _offscreenBufferR
			resources->InitPixelShader(resources->_bloomCombinePS);
			context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR,
				0, BACKBUFFER_FORMAT);
			context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceViewR.GetAddressOf());
			context->PSSetShaderResources(1, 1, resources->_bloomOutputSumSRV_R.GetAddressOf());
			/*
			ID3D11RenderTargetView *rtvs[2] = {
				resources->_renderTargetViewR.Get(),
				resources->_renderTargetViewBloom1R.Get()
			};
			context->OMSetRenderTargets(2, rtvs, NULL);
			*/
			context->OMSetRenderTargets(1, resources->_renderTargetViewR.GetAddressOf(), NULL);
			break;
		}

		context->Draw(6, 0);
	}

	// Restore previous rendertarget, etc
	// TODO: Is this really needed?
	viewport.Width  = screen_res_x;
	viewport.Height = screen_res_y;
	resources->InitViewport(&viewport);
	resources->InitInputLayout(resources->_inputLayout);
	context->OMSetRenderTargets(1, this->_deviceResources->_renderTargetView.GetAddressOf(),
		this->_deviceResources->_depthStencilViewL.Get());
}

/*
 * Performs a full bloom blur pass to build a pyramidal bloom. This function
 * calls BloomBasicPass internally several times.
 * Input: Bloom mask (_offscreenBufferAsInputReshadeMask, _offscreenBufferAsInputReshadeMaskR)
 * Output: 
 *		_offscreenBuffer (with accumulated bloom)
 *		_offscreenBufferAsInputReshadeMask, _offscreenBufferAsInputReshadeMaskR (blurred and downsampled from this pass)
 */
void PrimarySurface::BloomPyramidLevelPass(int PyramidLevel, int AdditionalPasses, float fZoomFactor, bool debug=false) {
	auto &resources = this->_deviceResources;
	auto &context = resources->_d3dDeviceContext;
	float fPixelScale = g_fBloomSpread[PyramidLevel];
	float fFirstPassZoomFactor = fZoomFactor / 2.0f;

	// The textures are always going to be g_fCurScreenWidth x g_fCurScreenHeight; but the step
	// size will be twice as big in the next pass due to the downsample, so we have to compensate
	// with a zoom factor:
	g_BloomPSCBuffer.pixelSizeX			= fPixelScale * g_fCurScreenWidthRcp  / fFirstPassZoomFactor;
	g_BloomPSCBuffer.pixelSizeY			= fPixelScale * g_fCurScreenHeightRcp / fFirstPassZoomFactor;
	g_BloomPSCBuffer.amplifyFactor		= 1.0f / fFirstPassZoomFactor;
	g_BloomPSCBuffer.bloomStrength		= g_fBloomLayerMult[PyramidLevel];
	g_BloomPSCBuffer.saturationStrength = g_BloomConfig.fSaturationStrength;
	g_BloomPSCBuffer.uvStepSize			= g_BloomConfig.uvStepSize1;
	//g_BloomPSCBuffer.uvStepSize			= 1.5f;
	resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);

	// DEBUG
	/*if (g_iPresentCounter == 100 || g_bDumpBloomBuffers) {
		wchar_t filename[80];
		swprintf_s(filename, 80, L"c:\\temp\\_offscreenInputBloomMask-%d.jpg", PyramidLevel);
		DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferAsInputBloomMask, GUID_ContainerFormatJpeg, filename);
	}*/
	// DEBUG

	// Initial Horizontal Gaussian Blur from Masked Buffer. input: reshade mask, output: bloom1
	// This pass will downsample the image according to fViewportDivider:
	BloomBasicPass(0, fZoomFactor);
	// DEBUG
	/*if (g_iPresentCounter == 100 || g_bDumpBloomBuffers) {
		wchar_t filename[80];
		swprintf_s(filename, 80, L"c:\\temp\\_bloom1-pass0-level-%d.jpg", PyramidLevel);
		DirectX::SaveWICTextureToFile(context, resources->_bloomOutput1, GUID_ContainerFormatJpeg, filename);
	}*/
	// DEBUG

	// Second Vertical Gaussian Blur: adjust the pixel size since this image was downsampled in
	// the previous pass:
	g_BloomPSCBuffer.pixelSizeX		= fPixelScale * g_fCurScreenWidthRcp / fZoomFactor;
	g_BloomPSCBuffer.pixelSizeY		= fPixelScale * g_fCurScreenHeightRcp / fZoomFactor;
	// The UVs should now go to half the original range because the image was downsampled:
	g_BloomPSCBuffer.amplifyFactor	= 1.0f / fZoomFactor;
	resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);
	// Vertical Gaussian Blur. input: bloom1, output: bloom2
	BloomBasicPass(1, fZoomFactor);
	// DEBUG
	/*if (g_iPresentCounter == 100 || g_bDumpBloomBuffers) {
		wchar_t filename[80];
		swprintf_s(filename, 80, L"c:\\temp\\_bloom2-pass1-level-%d.jpg", PyramidLevel);
		DirectX::SaveWICTextureToFile(context, resources->_bloomOutput2, GUID_ContainerFormatJpeg, filename);
	}*/
	// DEBUG

	for (int i = 0; i < AdditionalPasses; i++) {
		// Alternating between 2.0 and 1.5 avoids banding artifacts
		//g_BloomPSCBuffer.uvStepSize = (i % 2 == 0) ? 2.0f : 1.5f;
		//g_BloomPSCBuffer.uvStepSize = 1.5f + (i % 3) * 0.7f;
		g_BloomPSCBuffer.uvStepSize = (i % 2 == 0) ? g_BloomConfig.uvStepSize2 : g_BloomConfig.uvStepSize1;
		resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);
		// Horizontal Gaussian Blur. input: bloom2, output: bloom1
		BloomBasicPass(2, fZoomFactor);
		// Vertical Gaussian Blur. input: bloom1, output: bloom2
		BloomBasicPass(1, fZoomFactor);
	}
	// The blur output will *always* be in bloom2, let's copy it to the bloom masks to reuse it for the
	// next pass:
	context->CopyResource(resources->_offscreenBufferAsInputBloomMask, resources->_bloomOutput2);
	if (g_bUseSteamVR)
		context->CopyResource(resources->_offscreenBufferAsInputBloomMaskR, resources->_bloomOutput2R);

	// DEBUG
	/*if (g_iPresentCounter == 100 || g_bDumpBloomBuffers) {
		wchar_t filename[80];
		swprintf_s(filename, 80, L"c:\\temp\\_bloom2Buffer-Level-%d.jpg", PyramidLevel);
		DirectX::SaveWICTextureToFile(context, resources->_bloomOutput2, GUID_ContainerFormatJpeg, filename);
	}*/
	// DEBUG

	g_BloomPSCBuffer.amplifyFactor = 1.0f / fZoomFactor;
	resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);
	
	// Combine. input: offscreenBuffer (will be resolved), bloom2; output: offscreenBuffer/bloom1
	//BloomBasicPass(3, fZoomFactor);

	// Accummulate the bloom buffer, input: bloom2, bloomSum; output: bloom1
	BloomBasicPass(4, fZoomFactor);

	// Copy _bloomOutput1 over _bloomOutputSum
	context->CopyResource(resources->_bloomOutputSum, resources->_bloomOutput1);
	if (g_bUseSteamVR)
		context->CopyResource(resources->_bloomOutputSumR, resources->_bloomOutput1R);

	// DEBUG
	/*if (g_iPresentCounter == 100 || g_bDumpBloomBuffers) {
		wchar_t filename[80];
		swprintf_s(filename, 80, L"c:\\temp\\_bloomOutputSum-Level-%d.jpg", PyramidLevel);
		DirectX::SaveWICTextureToFile(context, resources->_bloomOutputSum, GUID_ContainerFormatJpeg, filename);
	}*/
	// DEBUG

	// To make this step compatible with the rest of the code, we need to copy the results
	// to offscreenBuffer and offscreenBufferR (in SteamVR mode).
	/*context->CopyResource(resources->_offscreenBuffer, resources->_bloomOutput1);
	if (g_bUseSteamVR)
		context->CopyResource(resources->_offscreenBufferR, resources->_bloomOutput1R);*/
}

void PrimarySurface::ClearBox(uvfloat4 box, D3D11_VIEWPORT *viewport, D3DCOLOR clearColor) {
	HRESULT hr;
	auto& resources = _deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	D3D11_BLEND_DESC blendDesc{};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = resources->InitBlendState(nullptr, &blendDesc);

	// Set the vertex buffer
	// D3DCOLOR seems to be AARRGGBB
	D3DTLVERTEX vertices[6];
	vertices[0].sx = box.x0; vertices[0].sy = box.y0; vertices[0].sz = 0.98f; vertices[0].rhw = 34.0f; vertices[0].color = clearColor;
	vertices[1].sx = box.x1; vertices[1].sy = box.y0; vertices[1].sz = 0.98f; vertices[1].rhw = 34.0f; vertices[1].color = clearColor;
	vertices[2].sx = box.x1; vertices[2].sy = box.y1; vertices[2].sz = 0.98f; vertices[2].rhw = 34.0f; vertices[2].color = clearColor;

	vertices[3].sx = box.x1; vertices[3].sy = box.y1; vertices[3].sz = 0.98f; vertices[3].rhw = 34.0f; vertices[3].color = clearColor;
	vertices[4].sx = box.x0; vertices[4].sy = box.y1; vertices[4].sz = 0.98f; vertices[4].rhw = 34.0f; vertices[4].color = clearColor;
	vertices[5].sx = box.x0; vertices[5].sy = box.y0; vertices[5].sz = 0.98f; vertices[5].rhw = 34.0f; vertices[5].color = clearColor;

	D3D11_MAPPED_SUBRESOURCE map;
	hr = context->Map(resources->_clearHUDVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
	if (SUCCEEDED(hr)) {
		memcpy(map.pData, vertices, sizeof(D3DTLVERTEX) * 6);
		context->Unmap(resources->_clearHUDVertexBuffer, 0);
	}

	D3D11_DEPTH_STENCIL_DESC zDesc;
	// Temporarily disable ZWrite: we won't need it to display the HUD
	ComPtr<ID3D11DepthStencilState> depthState;
	zDesc.DepthEnable = FALSE;
	zDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	zDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	zDesc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &zDesc);

	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitRasterizerState(resources->_rasterizerState);

	// Change the shaders
	resources->InitVertexShader(resources->_passthroughVertexShader);
	resources->InitPixelShader(resources->_pixelShaderClearBox);
	// Change the render target
	// Set three RTVs: Foreground HUD, Background HUD and HUD Text
	ID3D11RenderTargetView *rtvs[3] = {
		resources->_renderTargetViewDynCockpitAsInput.Get(),
		resources->_renderTargetViewDynCockpitAsInputBG.Get(),
		resources->_DCTextAsInputRTV.Get(),
	};
	context->OMSetRenderTargets(3, rtvs, NULL);

	// Set the viewport
	resources->InitViewport(viewport);
	// Set the vertex buffer (map the vertices from the box)
	UINT stride = sizeof(D3DTLVERTEX);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_clearHUDVertexBuffer.GetAddressOf(), &stride, &offset);
	// Draw
	context->Draw(6, 0);
}

int PrimarySurface::ClearHUDRegions() {
	D3D11_VIEWPORT viewport = { 0 };
	int num_regions_erased = 0;
	
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width    = (float )_deviceResources->_backbufferWidth;
	viewport.Height   = (float )_deviceResources->_backbufferHeight;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	viewport.MaxDepth = D3D11_MAX_DEPTH;

	int size = g_iNumDCElements;
	for (int i = 0; i < size; i++) {
		dc_element *dc_elem = &g_DCElements[i];
		if (g_sCurrentCockpit[0] != 0 && !dc_elem->bNameHasBeenTested)
		{
			if (strstr(dc_elem->name, g_sCurrentCockpit) != NULL) {
				dc_elem->bActive = true;
				dc_elem->bNameHasBeenTested = true;
				//log_debug("[DBG] [DC] ACTIVATED: '%s'", dc_elem->name);
			}
		}

		// Only clear HUD regions for active dc_elements
		if (!dc_elem->bActive)
			continue;

		if (dc_elem->num_erase_slots == 0)
			continue;

		for (int j = 0; j < dc_elem->num_erase_slots; j++) {
			int erase_slot = dc_elem->erase_slots[j];
			DCHUDRegion *dcSrcBox = &g_DCHUDRegions.boxes[erase_slot];
			if (dcSrcBox->bLimitsComputed) {
				ClearBox(dcSrcBox->erase_coords, &viewport, 0x0);
				num_regions_erased++;
			}
		}
	}
	return num_regions_erased;
}

/*
 * Renders the HUD foreground and background and applies the move_region 
 * commands if DC is enabled
 */
void PrimarySurface::DrawHUDVertices() {
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;
	D3D11_VIEWPORT viewport;
	HRESULT hr;

	D3D11_BLEND_DESC blendDesc{};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = resources->InitBlendState(nullptr, &blendDesc);

	// We don't need to clear the current vertex and pixel constant buffers.
	// Since we've just finished rendering 3D, they should contain values that
	// can be reused. So let's just overwrite the values that we need.
	g_VSCBuffer.aspect_ratio      =  g_fAspectRatio;
	g_VSCBuffer.z_override        = -1.0f;
	g_VSCBuffer.sz_override       = -1.0f;
	g_VSCBuffer.mult_z_override   = -1.0f;
	g_VSCBuffer.apply_uv_comp     =  g_bEnableVR;
	g_VSCBuffer.bPreventTransform =  0.0f;
	g_VSCBuffer.bFullTransform    =  0.0f;
	if (g_bEnableVR) {
		g_VSCBuffer.viewportScale[0] = 1.0f / resources->_displayWidth;
		g_VSCBuffer.viewportScale[1] = 1.0f / resources->_displayHeight;
	}
	else {
		g_VSCBuffer.viewportScale[0] =  2.0f / resources->_displayWidth;
		g_VSCBuffer.viewportScale[1] = -2.0f / resources->_displayHeight;
	}
	// Reduce the scale for GUI elements, except for the HUD
	g_VSCBuffer.viewportScale[3]  = g_fGUIElemsScale;
	// Enable/Disable the fixed GUI
	g_VSCBuffer.bFullTransform	  = g_bFixedGUI ? 1.0f : 0.0f;
	// Since the HUD is all rendered on a flat surface, we lose the vrparams that make the 3D object
	// and text float
	g_VSCBuffer.z_override		  = g_fFloatingGUIDepth;
	g_VSCBuffer.scale_override	  = g_fGUIElemsScale;

	g_PSCBuffer.brightness		  = 1.0f;
	g_PSCBuffer.bUseCoverTexture  = 0;
	
	// Add the move_regions commands.
	int numCoords = 0;
	if (g_bDynCockpitEnabled) 
	{
		for (int i = 0; i < g_DCMoveRegions.numCoords; i++) {
			int region_slot = g_DCMoveRegions.region_slot[i];
			// Skip invalid src slots
			if (region_slot < 0)
				continue;
			// Skip regions if their limits haven't been computed
			if (!g_DCHUDRegions.boxes[region_slot].bLimitsComputed) {
				//log_debug("[DBG] [DC] Skipping move_region command for slot %d", region_slot);
				continue;
			}
			// Fetch the source uv coords:
			g_DCPSCBuffer.src[numCoords] = g_DCHUDRegions.boxes[region_slot].erase_coords;
			// Fetch the destination uv coords:
			g_DCPSCBuffer.dst[numCoords] = g_DCMoveRegions.dst[i];
			numCoords++;
		}
	}
	g_PSCBuffer.DynCockpitSlots = numCoords;

	resources->InitPSConstantBuffer3D(resources->_PSConstantBuffer.GetAddressOf(), &g_PSCBuffer);
	resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);
	resources->InitPSConstantBufferDC(resources->_PSConstantBufferDC.GetAddressOf(), &g_DCPSCBuffer);

	UINT stride = sizeof(D3DTLVERTEX);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_HUDVertexBuffer.GetAddressOf(), &stride, &offset);
	resources->InitInputLayout(resources->_inputLayout);
	if (g_bEnableVR)
		resources->InitVertexShader(resources->_sbsVertexShader);
	else
		// The original code used _vertexShader:
		resources->InitVertexShader(resources->_vertexShader);
	resources->InitPixelShader(resources->_pixelShaderHUD);
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitRasterizerState(resources->_rasterizerState);

	// Temporarily disable ZWrite: we won't need it to display the HUD
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	// Don't clear the render target, the offscreenBuffer already has the 3D render in it
	// Render the left image
	if (g_bUseSteamVR)
		context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(), NULL);
	else
		context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(), NULL);
	// VIEWPORT-LEFT
	if (g_bEnableVR) {
		if (g_bUseSteamVR)
			viewport.Width = (float)resources->_backbufferWidth;
		else
			viewport.Width = (float)resources->_backbufferWidth / 2.0f;
	}
	else // Non-VR path
		viewport.Width = (float)resources->_backbufferWidth;
	viewport.Height = (float)resources->_backbufferHeight;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	resources->InitViewport(&viewport);
	// Set the left projection matrix
	g_VSMatrixCB.projEye = g_FullProjMatrixLeft;
	// The viewMatrix is set at the beginning of the frame
	resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);
	// Set the HUD foreground, background and Text textures:
	ID3D11ShaderResourceView *srvs[3] = {
		resources->_offscreenAsInputDynCockpitSRV.Get(),
		resources->_offscreenAsInputDynCockpitBG_SRV.Get(),
		resources->_DCTextSRV.Get()
	};
	context->PSSetShaderResources(0, 3, srvs);
	// Draw the Left Image
	//if (RenderHUD)
		context->Draw(6, 0);

	if (!g_bEnableVR) // Shortcut for the non-VR path
		//goto out;
		return;

	// Render the right image
	if (g_bUseSteamVR)
		context->OMSetRenderTargets(1, resources->_renderTargetViewR.GetAddressOf(), NULL);
	else
		context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(), NULL);

	// VIEWPORT-RIGHT
	if (g_bUseSteamVR) {
		viewport.Width = (float)resources->_backbufferWidth;
		viewport.TopLeftX = 0.0f;
	}
	else {
		viewport.Width = (float)resources->_backbufferWidth / 2.0f;
		viewport.TopLeftX = 0.0f + viewport.Width;
	}
	viewport.Height = (float)resources->_backbufferHeight;
	viewport.TopLeftY = 0.0f;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	resources->InitViewport(&viewport);
	// Set the right projection matrix
	g_VSMatrixCB.projEye = g_FullProjMatrixRight;
	resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);
	// Draw the Right Image
	//if (RenderHUD)
		context->Draw(6, 0);

	/*
out:
	// Restore the state
	ID3D11ShaderResourceView *null_srvs[3] = { NULL, NULL, NULL };
	context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(), resources->_depthStencilViewL.Get());
	context->PSSetShaderResources(0, 3, null_srvs);
	resources->InitViewport(&g_nonVRViewport);
	resources->InitVertexShader(resources->_vertexShader);
	resources->InitPixelShader(resources->_pixelShaderTexture);
	resources->InitRasterizerState(resources->_rasterizerState);
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_inputLayout);
	//UINT stride = sizeof(D3DTLVERTEX);
	//UINT offset = 0;
	//resources->InitVertexBuffer(_vertexBuffer.GetAddressOf(), &stride, &offset);
	*/
}

void PrimarySurface::ComputeNormalsPass(float fZoomFactor) {
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	// Set the constants used by the ComputeNormals shader
	float fPixelScale = 0.5f, fFirstPassZoomFactor = 1.0f;
	g_BloomPSCBuffer.pixelSizeX = fPixelScale * g_fCurScreenWidthRcp  / fFirstPassZoomFactor;
	g_BloomPSCBuffer.pixelSizeY = fPixelScale * g_fCurScreenHeightRcp / fFirstPassZoomFactor;
	g_BloomPSCBuffer.amplifyFactor = 1.0f / fFirstPassZoomFactor;
	g_BloomPSCBuffer.uvStepSize = 1.0f;
	resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);

	// Set the vertex buffer... we probably need another vertex buffer here
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_postProcessVertBuffer.GetAddressOf(), &stride, &offset);

	// Set Primitive Topology
	// Opportunity for optimization? Make all draw calls use the same topology?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	// Temporarily disable ZWrite: we won't need it for this effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	// Create a new viewport to render the offscreen buffer as a texture
	float screen_res_x = (float)resources->_backbufferWidth;
	float screen_res_y = (float)resources->_backbufferHeight;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width  = screen_res_x / (1.0f * fZoomFactor);
	viewport.Height = screen_res_y / (1.0f * fZoomFactor);

	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	resources->InitViewport(&viewport);

	// Set the constant buffers
	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
		0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices
	context->IASetInputLayout(resources->_mainInputLayout);
	resources->InitVertexShader(resources->_mainVertexShader);

	// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
	// Input: _depthAsInput
	// Output _normBuf
	//resources->InitPixelShader(resources->_computeNormalsPS);
	context->PSSetShaderResources(0, 1, resources->_depthBufSRV.GetAddressOf());
	context->ClearRenderTargetView(resources->_renderTargetViewNormBuf, bgColor);
	//ID3D11RenderTargetView *rtvs[2] = {
	//			resources->_renderTargetView.Get(), // DEBUG purposes only, remove later
	//			resources->_renderTargetViewNormBuf.Get()
	//};
	//context->OMSetRenderTargets(2, rtvs, NULL);
	context->OMSetRenderTargets(1, resources->_renderTargetViewNormBuf.GetAddressOf(), NULL);
	context->Draw(6, 0);

	// Draw the right image when SteamVR is enabled
	if (g_bUseSteamVR) {
		// TODO: Check that this works in SteamVR
		// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
		// Input: _depthAsInputR
		// Output _normBufR
		context->PSSetShaderResources(0, 1, resources->_depthBufSRV_R.GetAddressOf());
		context->ClearRenderTargetView(resources->_renderTargetViewNormBufR, bgColor);
		context->OMSetRenderTargets(1, resources->_renderTargetViewNormBufR.GetAddressOf(), NULL);
		context->Draw(6, 0);
	}

	// Restore previous rendertarget, etc
	// TODO: Is this really needed?
	viewport.Width = screen_res_x;
	viewport.Height = screen_res_y;
	resources->InitViewport(&viewport);
	resources->InitInputLayout(resources->_inputLayout);
	context->OMSetRenderTargets(1, this->_deviceResources->_renderTargetView.GetAddressOf(),
		this->_deviceResources->_depthStencilViewL.Get());
}

/*
void PrimarySurface::SmoothNormalsPass(float fZoomFactor) {
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;

	// Set the constants used by the ComputeNormals shader
	//float fPixelScale = 0.5f, fFirstPassZoomFactor = 1.0f;
	//g_BloomPSCBuffer.pixelSizeX = fPixelScale * g_fCurScreenWidthRcp / fFirstPassZoomFactor;
	//g_BloomPSCBuffer.pixelSizeY = fPixelScale * g_fCurScreenHeightRcp / fFirstPassZoomFactor;
	//g_BloomPSCBuffer.amplifyFactor = 1.0f / fFirstPassZoomFactor;
	//g_BloomPSCBuffer.uvStepSize = 1.0f;
	//resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);

	// Create the VertexBuffer if necessary
	if (resources->_barrelEffectVertBuffer == nullptr) {
		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

		vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		vertexBufferDesc.ByteWidth = sizeof(MainVertex) * ARRAYSIZE(g_BarrelEffectVertices);
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.CPUAccessFlags = 0;
		vertexBufferDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA vertexBufferData;

		ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
		vertexBufferData.pSysMem = g_BarrelEffectVertices;
		device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, resources->_barrelEffectVertBuffer.GetAddressOf());
	}
	// Set the vertex buffer... we probably need another vertex buffer here
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_barrelEffectVertBuffer.GetAddressOf(), &stride, &offset);

	// Set Primitive Topology
	// Opportunity for optimization? Make all draw calls use the same topology?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	// Temporarily disable ZWrite: we won't need it for this effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	// Create a new viewport to render the offscreen buffer as a texture
	float screen_res_x = (float)resources->_backbufferWidth;
	float screen_res_y = (float)resources->_backbufferHeight;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width  = screen_res_x / (1.0f * fZoomFactor);
	viewport.Height = screen_res_y / (1.0f * fZoomFactor);

	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	resources->InitViewport(&viewport);

	// Set the constant buffers
	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
		0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices
	context->IASetInputLayout(resources->_mainInputLayout);
	resources->InitVertexShader(resources->_mainVertexShader);

	float fPixelScale = 2.0f, fFirstPassZoomFactor = fZoomFactor / 2.0f;
	g_BloomPSCBuffer.pixelSizeX = fPixelScale * g_fCurScreenWidthRcp / fZoomFactor;
	g_BloomPSCBuffer.pixelSizeY = fPixelScale * g_fCurScreenHeightRcp / fZoomFactor;
	g_BloomPSCBuffer.amplifyFactor = 1.0f / fZoomFactor;
	g_BloomPSCBuffer.uvStepSize = 1.0f;
	//g_BloomPSCBuffer.enableSSAO = g_bEnableSSAOInShader;
	//g_BloomPSCBuffer.enableBentNormals = g_bEnableBentNormalsInShader;
	//g_BloomPSCBuffer.norm_weight = g_fNormWeight;
	g_BloomPSCBuffer.depth_weight = g_SSAO_PSCBuffer.max_dist;
	resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);

	// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
	// Input: _depthAsInput
	// Output _normBuf
	resources->InitPixelShader(resources->_computeNormalsPS);
	// Copy bentBuf <- normBuf temporarily...
	context->CopyResource(resources->_bentBuf, resources->_normBuf);
	ID3D11ShaderResourceView *srvs[2] = {
		resources->_depthBufSRV.Get(),
		resources->_bentBufSRV.Get()
	};
	//ID3D11RenderTargetView *rtvs[2] = {
	//			resources->_renderTargetView.Get(), // DEBUG purposes only, remove later
	//			resources->_renderTargetViewNormBuf.Get()
	//};
	//context->OMSetRenderTargets(2, rtvs, NULL);
	//context->OMSetRenderTargets(1, resources->_renderTargetViewNormBuf.GetAddressOf(), NULL);
	context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(), NULL); // DEBUG

	context->PSSetShaderResources(0, 2, srvs);
	context->ClearRenderTargetView(resources->_renderTargetViewNormBuf, bgColor);
	
	context->Draw(6, 0);

	// Draw the right image when SteamVR is enabled
	if (g_bUseSteamVR) {
		// TODO: Check that this works in SteamVR
		// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
		// Input: _depthAsInputR
		// Output _normBufR
		context->PSSetShaderResources(0, 1, resources->_depthBufSRV_R.GetAddressOf());
		context->ClearRenderTargetView(resources->_renderTargetViewNormBufR, bgColor);
		context->OMSetRenderTargets(1, resources->_renderTargetViewNormBufR.GetAddressOf(), NULL);
		context->Draw(6, 0);
	}

	// Restore previous rendertarget, etc
	// TODO: Is this really needed?
	viewport.Width = screen_res_x;
	viewport.Height = screen_res_y;
	resources->InitViewport(&viewport);
	resources->InitInputLayout(resources->_inputLayout);
	context->OMSetRenderTargets(1, this->_deviceResources->_renderTargetView.GetAddressOf(),
		this->_deviceResources->_depthStencilViewL.Get());
}
*/

/*
 * Sets the lights in the constant buffer used in the New Shading System.
 */
void PrimarySurface::SetLights(float fSSDOEnabled) {
	const auto &resources = this->_deviceResources;
	Vector4 light;
	int i;

	if (g_bHeadLightsAutoTurnOn) {
		if (*s_XwaGlobalLightsCount == 0) {
			//log_debug("[DBG] AUTO Turning headlights ON");
			g_bEnableHeadLights = true;
		}
		else {
			//log_debug("[DBG] AUTO Turning headlights OFF");
			g_bEnableHeadLights = false;
		}
	}

	/*
	if (g_bOverrideLightPos) {
		for (int i = 0; i < 2; i++) {
			light[i].x = g_LightVector[i].x;
			light[i].y = g_LightVector[i].y;
			light[i].z = g_LightVector[i].z;
		}
	}
	else
		ComputeRotationMatrixFromXWAView(light, 2);
	*/

	// Use the heading matrix to move the lights
	//Matrix4 H = GetCurrentHeadingViewMatrix();
	// In skirmish mode, the light with the highest intensity seems to be in index 1 and it matches the 
	// position of the Sun/Nebula. However, in regular missions, the main light source seems to come from
	// index 0 and index 1 is a secondary light like a big planet or nebula or similar.
	// We need to find the light with the highest intensity and use that for SSDO
	float maxIntensity = -1.0;
	int maxIdx = -1, maxLights = min(MAX_XWA_LIGHTS, *s_XwaGlobalLightsCount);
	float cur_ambient = g_ShadingSys_PSBuffer.ambient;
	//if (*s_XwaGlobalLightsCount == 0)
	//	log_debug("[DBG] NO GLOBAL LIGHTS!");
	if (g_bDumpSSAOBuffers) {
		log_debug("[DBG] s_XwaGlobalLightsCount: %d", *s_XwaGlobalLightsCount);
		log_file("[DBG] s_XwaGlobalLightsCount: %d, maxLights: %d\n", *s_XwaGlobalLightsCount, maxLights);
		DumpGlobalLights();
	}

	if (g_HyperspacePhaseFSM != HS_HYPER_TUNNEL_ST)
	{
		for (int i = 0; i < maxLights; i++)
		{
			Vector4 col;
			float intensity, value;
			Vector4 xwaLight = Vector4(
				s_XwaGlobalLights[i].PositionX / 32768.0f,
				s_XwaGlobalLights[i].PositionY / 32768.0f,
				s_XwaGlobalLights[i].PositionZ / 32768.0f,
				0.0f);

			light = g_CurrentHeadingViewMatrix * xwaLight;
			light.z = -light.z; // Once more we invert Z because normal mapping has Z+ pointing to the camera
			//log_debug("[DBG] light, I: %0.3f, [%0.3f, %0.3f, %0.3f]",
			//	s_XwaGlobalLights[i].Intensity, light[0].x, light[0].y, light[0].z);
			g_ShadingSys_PSBuffer.LightVector[i].x = light.x;
			g_ShadingSys_PSBuffer.LightVector[i].y = light.y;
			g_ShadingSys_PSBuffer.LightVector[i].z = light.z;
			g_ShadingSys_PSBuffer.LightVector[i].w = 0.0f;

			col.x = s_XwaGlobalLights[i].ColorR;
			col.y = s_XwaGlobalLights[i].ColorG;
			col.z = s_XwaGlobalLights[i].ColorB;
			col.w = 0.0f;

			intensity = s_XwaGlobalLights[i].Intensity;
			// Compute the value: use Luma to approximate it
			value = 0.299f * col.x + 0.587f * col.y + 0.114f * col.z;
			// Normalize the colors if the intensity is above 1
			if (intensity > 1.0f)
				intensity = value;
			else if (g_bApplyXWALightsIntensity)
				// Tone down the current color according to its intensity (?)
				col *= intensity;

			// Change the saturation of the lights. The idea here is that we're
			// using a vector gray = vec3(value) and then computing col - gray, and then
			// we interpolate between col and gray depending on the saturation setting
			col.x = value + g_fXWALightsSaturation * (col.x - value);
			col.y = value + g_fXWALightsSaturation * (col.y - value);
			col.z = value + g_fXWALightsSaturation * (col.z - value);

			g_ShadingSys_PSBuffer.LightColor[i].x = g_fXWALightsIntensity * col.x;
			g_ShadingSys_PSBuffer.LightColor[i].y = g_fXWALightsIntensity * col.y;
			g_ShadingSys_PSBuffer.LightColor[i].z = g_fXWALightsIntensity * col.z;
			g_ShadingSys_PSBuffer.LightColor[i].w = intensity;

			if (g_ShadingSys_PSBuffer.HDREnabled) {
				g_ShadingSys_PSBuffer.LightColor[i].x *= g_fHDRLightsMultiplier;
				g_ShadingSys_PSBuffer.LightColor[i].y *= g_fHDRLightsMultiplier;
				g_ShadingSys_PSBuffer.LightColor[i].z *= g_fHDRLightsMultiplier;
			}

			// Keep track of the light with the highest intensity
			if (intensity > maxIntensity) {
				maxIntensity = intensity;
				maxIdx = i;
			}

			if (g_bDumpSSAOBuffers)
			{
				log_debug("[DBG] light[%d], I: %0.3f: i: %0.3f, m1C: %0.3f, V:[%0.3f, %0.3f, %0.3f], COL: (%0.3f, %0.3f, %0.3f), col: (%0.3f, %0.3f, %0.3f)",
					i, s_XwaGlobalLights[i].Intensity, intensity, s_XwaGlobalLights[i].XwaGlobalLight_m1C,
					g_ShadingSys_PSBuffer.LightVector[i].x, g_ShadingSys_PSBuffer.LightVector[i].y, g_ShadingSys_PSBuffer.LightVector[i].z,
					s_XwaGlobalLights[i].ColorR, s_XwaGlobalLights[i].ColorG, s_XwaGlobalLights[i].ColorB,
					g_ShadingSys_PSBuffer.LightColor[i].x, g_ShadingSys_PSBuffer.LightColor[i].y, g_ShadingSys_PSBuffer.LightColor[i].z
				);
				log_file("[DBG] light[%d], I: %0.3f: i: %0.3f, m1C: %0.3f, V:[%0.3f, %0.3f, %0.3f], COL: (%0.3f, %0.3f, %0.3f), col: (%0.3f, %0.3f, %0.3f)\n",
					i, s_XwaGlobalLights[i].Intensity, intensity, s_XwaGlobalLights[i].XwaGlobalLight_m1C,
					g_ShadingSys_PSBuffer.LightVector[i].x, g_ShadingSys_PSBuffer.LightVector[i].y, g_ShadingSys_PSBuffer.LightVector[i].z,
					s_XwaGlobalLights[i].ColorR, s_XwaGlobalLights[i].ColorG, s_XwaGlobalLights[i].ColorB,
					g_ShadingSys_PSBuffer.LightColor[i].x, g_ShadingSys_PSBuffer.LightColor[i].y, g_ShadingSys_PSBuffer.LightColor[i].z
				);
			}
		}
		if (g_bDumpSSAOBuffers)
			log_file("[DBG] maxIdx: %d, maxIntensity: %0.3f\n\n", maxIdx, maxIntensity);
		g_ShadingSys_PSBuffer.LightCount  = maxLights;
		g_ShadingSys_PSBuffer.MainLight.x = g_ShadingSys_PSBuffer.LightVector[maxIdx].x;
		g_ShadingSys_PSBuffer.MainLight.y = g_ShadingSys_PSBuffer.LightVector[maxIdx].y;
		g_ShadingSys_PSBuffer.MainLight.z = g_ShadingSys_PSBuffer.LightVector[maxIdx].z;
		g_ShadingSys_PSBuffer.MainColor   = g_ShadingSys_PSBuffer.LightColor[maxIdx];

		// If the headlights are on, then replace the main light with the headlight
		if (g_bEnableHeadLights) {
			Vector4 headLightDir(0.0, 0.0, 1.0, 0.0);
			Vector4 headLightPos(g_HeadLightsPosition.x, g_HeadLightsPosition.y, g_HeadLightsPosition.z, 1.0);
			Matrix4 ViewMatrix;
			// TODO: I think we also need to transform the position of the light...
			GetCockpitViewMatrixSpeedEffect(&ViewMatrix, true);
			// We're going to need another vector for the headlights direction... let's shift all
			// the light vectors one spot to the right and place the direction on the first slot
			for (int i = 0; i < maxLights; i++)
				if (i + 1 < MAX_XWA_LIGHTS)
				{
					g_ShadingSys_PSBuffer.LightVector[i + 1] = g_ShadingSys_PSBuffer.LightVector[i];
					g_ShadingSys_PSBuffer.LightColor[i + 1] = g_ShadingSys_PSBuffer.LightColor[i];
				}
			maxLights = min(MAX_XWA_LIGHTS, maxLights + 1);
			// Transform the headlights' direction and position:
			headLightDir = ViewMatrix * headLightDir;
			ViewMatrix.transpose();
			headLightPos = ViewMatrix * headLightPos;
			headLightPos.z = -headLightPos.z; // Shaders expect Z to be negative so that Z+ points towards the camera
			//log_debug("[DBG] headLightDir: %0.3f, %0.3f, %0.3f", headLightDir.x, headLightDir.y, headLightDir.z);
			//log_debug("[DBG] headLightPos: %0.3f, %0.3f, %0.3f", headLightPos.x, headLightPos.y, headLightPos.z);
			g_ShadingSys_PSBuffer.LightVector[0].x = headLightDir.x;
			g_ShadingSys_PSBuffer.LightVector[0].y = headLightDir.y;
			g_ShadingSys_PSBuffer.LightVector[0].z = headLightDir.z;

			// DEBUG: Remove all lights!
			//g_ShadingSys_PSBuffer.LightCount = 0;
			// DEBUG
			g_ShadingSys_PSBuffer.ambient = g_fHeadLightsAmbient;
			g_ShadingSys_PSBuffer.MainLight.x = headLightPos.x;
			g_ShadingSys_PSBuffer.MainLight.y = headLightPos.y;
			g_ShadingSys_PSBuffer.MainLight.z = headLightPos.z;

			g_ShadingSys_PSBuffer.MainColor.x = g_HeadLightsColor.x;
			g_ShadingSys_PSBuffer.MainColor.y = g_HeadLightsColor.y;
			g_ShadingSys_PSBuffer.MainColor.z = g_HeadLightsColor.z;
			g_ShadingSys_PSBuffer.MainColor.w = g_fHeadLightsDistance;

			g_ShadingSys_PSBuffer.headlights_angle_cos = g_fHeadLightsAngleCos;
		}
	}
	else 
	{
		// Hyper tunnel lights
		g_ShadingSys_PSBuffer.LightCount = 2;
		for (int i = 0; i < 2; i++) {
			g_ShadingSys_PSBuffer.LightVector[i].x = g_LightVector[i].x;
			g_ShadingSys_PSBuffer.LightVector[i].y = g_LightVector[i].y;
			g_ShadingSys_PSBuffer.LightVector[i].z = g_LightVector[i].z;

			g_ShadingSys_PSBuffer.LightColor[i].x = g_LightColor[i].x;
			g_ShadingSys_PSBuffer.LightColor[i].y = g_LightColor[i].y;
			g_ShadingSys_PSBuffer.LightColor[i].z = g_LightColor[i].z;
		}
		g_ShadingSys_PSBuffer.MainLight.x = g_ShadingSys_PSBuffer.LightVector[0].x;
		g_ShadingSys_PSBuffer.MainLight.y = g_ShadingSys_PSBuffer.LightVector[0].y;
		g_ShadingSys_PSBuffer.MainLight.z = g_ShadingSys_PSBuffer.LightVector[0].z;
		g_ShadingSys_PSBuffer.MainColor   = g_ShadingSys_PSBuffer.LightColor[0];
	}

	if (g_bEnableLaserLights) {
		// DEBUG
		/*
		g_ShadingSys_PSBuffer.num_lasers = 1;
		g_ShadingSys_PSBuffer.LightPoint[0].x =  g_LaserPointDebug.x;
		g_ShadingSys_PSBuffer.LightPoint[0].y =  g_LaserPointDebug.y;
		g_ShadingSys_PSBuffer.LightPoint[0].z = -g_LaserPointDebug.z;

		g_ShadingSys_PSBuffer.LightPointColor[0].x = 1.0f;
		g_ShadingSys_PSBuffer.LightPointColor[0].y = 0.0f;
		g_ShadingSys_PSBuffer.LightPointColor[0].z = 0.0f;
		*/
		// DEBUG
		int num_lasers = g_LaserList._size;
		g_ShadingSys_PSBuffer.num_lasers = num_lasers;
		for (i = 0; i < num_lasers; i++) {
			// Set the lights from the lasers
			g_ShadingSys_PSBuffer.LightPoint[i].x =  g_LaserList._elems[i].P.x;
			g_ShadingSys_PSBuffer.LightPoint[i].y =  g_LaserList._elems[i].P.y;
			g_ShadingSys_PSBuffer.LightPoint[i].z = -g_LaserList._elems[i].P.z;

			g_ShadingSys_PSBuffer.LightPointColor[i].x = g_LaserList._elems[i].col.x;
			g_ShadingSys_PSBuffer.LightPointColor[i].y = g_LaserList._elems[i].col.y;
			g_ShadingSys_PSBuffer.LightPointColor[i].z = g_LaserList._elems[i].col.z;
		}
	}
	else
		g_ShadingSys_PSBuffer.num_lasers = 0;
	
	/*
	if (g_bDumpSSAOBuffers) 
	{
		log_debug("[DBG] LightCount: %d, maxIdx: %d", g_ShadingSys_PSBuffer.LightCount, maxIdx);
		for (uint32_t i = 0; i < g_ShadingSys_PSBuffer.LightCount; i++) {
			log_debug("[DBG] light[%d], I: %0.3f: V:[%0.3f, %0.3f, %0.3f], col: (%0.3f, %0.3f, %0.3f)",
				i, g_ShadingSys_PSBuffer.LightColor[i].w,
				g_ShadingSys_PSBuffer.LightVector[i].x, g_ShadingSys_PSBuffer.LightVector[i].y, g_ShadingSys_PSBuffer.LightVector[i].z,
				g_ShadingSys_PSBuffer.LightColor[i].x, g_ShadingSys_PSBuffer.LightColor[i].y, g_ShadingSys_PSBuffer.LightColor[i].z
			);
		}
	}
	*/

	// TODO:
	g_ShadingSys_PSBuffer.ambient = *g_playerInHangar ? g_fHangarAmbient : g_fGlobalAmbient;
	g_ShadingSys_PSBuffer.ssdo_enabled = fSSDOEnabled;
	g_ShadingSys_PSBuffer.sso_disable = g_bEnableSSAOInShader ? 0.0f : 1.0f;
	g_ShadingSys_PSBuffer.HDREnabled = g_bHDREnabled;
	g_ShadingSys_PSBuffer.HDR_white_point = g_fHDRWhitePoint;
	resources->InitPSConstantShadingSystem(resources->_shadingSysBuffer.GetAddressOf(), &g_ShadingSys_PSBuffer);
	if (g_bEnableHeadLights)
		g_ShadingSys_PSBuffer.ambient = cur_ambient;
}

void PrimarySurface::SSAOPass(float fZoomFactor) {
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;
	
	// Set the vertex buffer... we probably need another vertex buffer here
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_postProcessVertBuffer.GetAddressOf(), &stride, &offset);

	// Set Primitive Topology
	// Opportunity for optimization? Make all draw calls use the same topology?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	// Temporarily disable ZWrite: we won't need it for this effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	// Create a new viewport to render the offscreen buffer as a texture
	float screen_res_x = (float)resources->_backbufferWidth;
	float screen_res_y = (float)resources->_backbufferHeight;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = screen_res_x / fZoomFactor;
	viewport.Height = screen_res_y / fZoomFactor;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	resources->InitViewport(&viewport);

	// Set the constant buffers
	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
		0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices

	// Set the SSAO pixel shader constant buffer
	g_SSAO_PSCBuffer.screenSizeX = g_fCurScreenWidth;
	g_SSAO_PSCBuffer.screenSizeY = g_fCurScreenHeight;
	g_SSAO_PSCBuffer.fn_enable   = g_bFNEnable;
	g_SSAO_PSCBuffer.debug		 = g_bShowSSAODebug;
	g_SSAO_PSCBuffer.enable_dist_fade = (float)g_b3DSkydomePresent;
	resources->InitPSConstantBufferSSAO(resources->_ssaoConstantBuffer.GetAddressOf(), &g_SSAO_PSCBuffer);

	// Set the layout
	context->IASetInputLayout(resources->_mainInputLayout);
	resources->InitVertexShader(resources->_mainVertexShader);

	// Set the lights and set the Shading System Constant Buffer
	g_ShadingSys_PSBuffer.spec_intensity       = g_bGlobalSpecToggle ? g_fSpecIntensity : 0.0f;
	g_ShadingSys_PSBuffer.spec_bloom_intensity = g_bGlobalSpecToggle ? g_fSpecBloomIntensity : 0.0f;
	SetLights(0.0f);

	// SSAO Computation, Left Image
	// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
	// Input: _depthBuf, _depthBuf2, _normBuf, _offscreenBuf (resolved here)
	// Output _ssaoBuf, _bentBuf
	{
		// Resolve offscreenBuf
		context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
			0, BACKBUFFER_FORMAT);
		ID3D11ShaderResourceView *srvs_pass1[3] = {
			resources->_depthBufSRV.Get(),
			resources->_normBufSRV.Get(),
			resources->_offscreenAsInputShaderResourceView
		};
		resources->InitPixelShader(resources->_ssaoPS);
		if (!g_bBlurSSAO && g_bShowSSAODebug) {
			ID3D11RenderTargetView *rtvs[1] = {
				resources->_renderTargetView.Get(),
			};
			context->ClearRenderTargetView(resources->_renderTargetView, bgColor);
			context->OMSetRenderTargets(1, rtvs, NULL);
			context->PSSetShaderResources(0, 4, srvs_pass1);
			context->Draw(6, 0);
			goto out1;
		}
		else {
			ID3D11RenderTargetView *rtvs[1] = {
				resources->_renderTargetViewSSAO.Get(),
			};
			context->ClearRenderTargetView(resources->_renderTargetViewSSAO, bgColor);
			context->OMSetRenderTargets(1, rtvs, NULL);
			context->PSSetShaderResources(0, 3, srvs_pass1);
			context->Draw(6, 0);
		}
	}

	// Setup the constant buffers to upscale the buffers
	// The textures are always going to be g_fCurScreenWidth x g_fCurScreenHeight; but the step
	// size will be twice as big in the next pass due to the downsample, so we have to compensate
	// with a zoom factor:
	float fPixelScale = fZoomFactor;
	g_BloomPSCBuffer.pixelSizeX			= fPixelScale * g_fCurScreenWidthRcp;
	g_BloomPSCBuffer.pixelSizeY			= fPixelScale * g_fCurScreenHeightRcp;
	g_BloomPSCBuffer.amplifyFactor		= 1.0f / fZoomFactor;
	g_BloomPSCBuffer.uvStepSize			= 1.0f;
	g_BloomPSCBuffer.depth_weight		= g_SSAO_PSCBuffer.max_dist;
	resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);

	// SSAO Blur, Left Image
	// input: _bloomOutput1SRV (with a copy of the ssaoBuf), depthBuf, normBuf
	// output: ssaoBuf
	if (g_bBlurSSAO) {
		resources->InitPixelShader(resources->_ssaoBlurPS);
		// Copy the SSAO buffer to bloom1(HDR) -- we'll use it as temp buffer
		// to blur the SSAO buffer
		context->CopyResource(resources->_bloomOutput1, resources->_ssaoBuf);
		context->ClearRenderTargetView(resources->_renderTargetViewSSAO.Get(), bgColor);
		ID3D11ShaderResourceView *srvs[3] = {
				resources->_bloomOutput1SRV.Get(),
				resources->_depthBufSRV.Get(),
				resources->_normBufSRV.Get(),
		};
		
		ID3D11RenderTargetView *rtvs[1] = {
			resources->_renderTargetViewSSAO.Get(),
		};
		context->OMSetRenderTargets(1, rtvs, NULL);
		context->PSSetShaderResources(0, 3, srvs);
		context->Draw(6, 0);
	}

	// Final combine, Left Image
	{
		// input: offscreenAsInput (resolved here), bloomMask, ssaoBuf
		// output: offscreenBuf
		// Reset the viewport for the final SSAO combine
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = screen_res_x;
		viewport.Height = screen_res_y;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		resources->InitViewport(&viewport);

		ID3D11ShaderResourceView *null_srvs4[4] = { NULL, NULL, NULL, NULL };
		context->PSSetShaderResources(0, 4, null_srvs4);
		// ssaoBuf was bound as an RTV, so let's bind the RTV first to unbind ssaoBuf
		// so that it can be used as an SRV
		ID3D11RenderTargetView *rtvs[5] = {
			resources->_renderTargetView.Get(),
			resources->_renderTargetViewBloomMask.Get(),
			NULL, NULL, NULL,
		};
		context->OMSetRenderTargets(5, rtvs, NULL);
		if (!g_bEnableHeadLights)
			resources->InitPixelShader(resources->_ssaoAddPS);
		else
			resources->InitPixelShader(resources->_headLightsSSAOPS);
		// Resolve offscreenBuf
		context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
			0, BACKBUFFER_FORMAT);
		ID3D11ShaderResourceView *srvs_pass2[9] = {
			resources->_offscreenAsInputShaderResourceView.Get(),	// Color buffer
			resources->_ssaoBufSRV.Get(),							// SSAO component
			NULL,													// SSDO Indirect
			resources->_ssaoMaskSRV.Get(),							// SSAO Mask

			resources->_depthBufSRV.Get(),							// Depth buffer
			resources->_normBufSRV.Get(),							// Normals
			NULL,													// Bent Normals
			resources->_ssMaskSRV.Get(),							// Shading System Mask buffer

			g_ShadowMapping.bEnabled ? 
				resources->_shadowMapArraySRV.Get() : NULL,			// The shadow map
		};
		context->PSSetShaderResources(0, 9, srvs_pass2);
		context->Draw(6, 0);
	}

out1:
	// Draw the right image when SteamVR is enabled
	if (g_bUseSteamVR) {
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = screen_res_x / fZoomFactor;
		viewport.Height = screen_res_y / fZoomFactor;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		resources->InitViewport(&viewport);

		// SSAO Computation, right eye
		// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
		// Input: _depthBuf, _depthBuf2, _normBuf, _offscreenBuf (resolved here)
		// Output _ssaoBuf, _bentBuf
		{
			// Resolve offscreenBuf
			context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR,
				0, BACKBUFFER_FORMAT);
			ID3D11ShaderResourceView *srvs_pass1[3] = {
				resources->_depthBufSRV_R.Get(),
				resources->_normBufSRV_R.Get(),
				resources->_offscreenAsInputShaderResourceViewR
			};
			resources->InitPixelShader(resources->_ssaoPS);
			if (!g_bBlurSSAO && g_bShowSSAODebug) {
				ID3D11RenderTargetView *rtvs[1] = {
					resources->_renderTargetViewR.Get(),
				};
				context->ClearRenderTargetView(resources->_renderTargetViewR, bgColor);
				context->OMSetRenderTargets(1, rtvs, NULL);
				context->PSSetShaderResources(0, 3, srvs_pass1);
				context->Draw(6, 0);
				goto out2;
			}
			else {
				ID3D11RenderTargetView *rtvs[1] = {
					resources->_renderTargetViewSSAO_R.Get(),
				};
				context->ClearRenderTargetView(resources->_renderTargetViewSSAO_R, bgColor);
				context->OMSetRenderTargets(1, rtvs, NULL);
				context->PSSetShaderResources(0, 3, srvs_pass1);
				context->Draw(6, 0);
			}
		}

		// Setup the constant buffers to upscale the buffers
		// The textures are always going to be g_fCurScreenWidth x g_fCurScreenHeight; but the step
		// size will be twice as big in the next pass due to the downsample, so we have to compensate
		// with a zoom factor:
		float fPixelScale = fZoomFactor;
		g_BloomPSCBuffer.pixelSizeX		= fPixelScale * g_fCurScreenWidthRcp;
		g_BloomPSCBuffer.pixelSizeY		= fPixelScale * g_fCurScreenHeightRcp;
		g_BloomPSCBuffer.amplifyFactor	= 1.0f / fZoomFactor;
		g_BloomPSCBuffer.uvStepSize		= 1.0f;
		g_BloomPSCBuffer.depth_weight	= g_SSAO_PSCBuffer.max_dist;
		resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);

		// SSAO Blur, Right Image
		// input: _bloomOutput1SRV (with a copy of the ssaoBufR), depthBufR, normBufR
		// output: ssaoBufR
		if (g_bBlurSSAO)
		{
			resources->InitPixelShader(resources->_ssaoBlurPS);
			// Copy the SSAO buffer to bloom1(HDR) -- we'll use it as temp buffer
			// to blur the SSAO buffer
			context->CopyResource(resources->_bloomOutput1, resources->_ssaoBufR);
			context->ClearRenderTargetView(resources->_renderTargetViewSSAO_R.Get(), bgColor);
			ID3D11ShaderResourceView *srvs[3] = {
					resources->_bloomOutput1SRV.Get(),
					resources->_depthBufSRV_R.Get(),
					resources->_normBufSRV_R.Get(),
			};
			
			ID3D11RenderTargetView *rtvs[1] = {
				resources->_renderTargetViewSSAO_R.Get(),
			};
			context->OMSetRenderTargets(1, rtvs, NULL);
			context->PSSetShaderResources(0, 3, srvs);
			context->Draw(6, 0);
		}

		// Final compositing, Right Image
		{
			// input: offscreenAsInputR (resolved here), bloomMaskR, ssaoBufR
			// output: offscreenBufR
			// Reset the viewport for the final SSAO combine
			viewport.TopLeftX = 0.0f;
			viewport.TopLeftY = 0.0f;
			viewport.Width = screen_res_x;
			viewport.Height = screen_res_y;
			viewport.MaxDepth = D3D11_MAX_DEPTH;
			viewport.MinDepth = D3D11_MIN_DEPTH;
			resources->InitViewport(&viewport);
	
			ID3D11ShaderResourceView *null_srvs4[4] = { NULL, NULL, NULL, NULL };
			context->PSSetShaderResources(0, 4, null_srvs4);
			// ssaoBuf was bound as an RTV, so let's bind the RTV first to unbind ssaoBuf
			// so that it can be used as an SRV
			ID3D11RenderTargetView *rtvs[5] = {
				resources->_renderTargetViewR.Get(),
				resources->_renderTargetViewBloomMaskR.Get(),
				NULL, NULL, NULL,
			};
			context->OMSetRenderTargets(5, rtvs, NULL);
			if (!g_bEnableHeadLights)
				resources->InitPixelShader(resources->_ssaoAddPS);
			else
				resources->InitPixelShader(resources->_headLightsSSAOPS);
			// Resolve offscreenBuf
			context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR,
				0, BACKBUFFER_FORMAT);
			ID3D11ShaderResourceView *srvs_pass2[9] = {
				resources->_offscreenAsInputShaderResourceViewR.Get(),	// Color buffer
				resources->_ssaoBufSRV_R.Get(),							// SSAO component
				NULL,													// SSDO Indirect
				resources->_ssaoMaskSRV_R.Get(),						// SSAO Mask

				resources->_depthBufSRV_R.Get(),						// Depth buffer
				resources->_normBufSRV_R.Get(),							// Normals
				NULL,													// Bent Normals
				resources->_ssMaskSRV_R.Get(),							// Shading System Mask

				g_ShadowMapping.bEnabled ?
					resources->_shadowMapArraySRV.Get() : NULL,			// The shadow map
			};
			context->PSSetShaderResources(0, 9, srvs_pass2);
			context->Draw(6, 0);
		}
	}

out2:
	// Restore previous rendertarget, etc
	// TODO: Is this really needed?
	viewport.Width = screen_res_x;
	viewport.Height = screen_res_y;
	resources->InitViewport(&viewport);
	resources->InitInputLayout(resources->_inputLayout);
	context->OMSetRenderTargets(1, this->_deviceResources->_renderTargetView.GetAddressOf(),
		this->_deviceResources->_depthStencilViewL.Get());
}

void PrimarySurface::SSDOPass(float fZoomFactor, float fZoomFactor2) {
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;
	float x0, y0, x1, y1;

	GetScreenLimitsInUVCoords(&x0, &y0, &x1, &y1);
	g_SSAO_PSCBuffer.x0 = x0;
	g_SSAO_PSCBuffer.y0 = y0;
	g_SSAO_PSCBuffer.x1 = x1;
	g_SSAO_PSCBuffer.y1 = y1;
	g_SSAO_PSCBuffer.enable_dist_fade = (float)g_b3DSkydomePresent;
	g_ShadingSys_PSBuffer.spec_intensity = g_bGlobalSpecToggle ? g_fSpecIntensity : 0.0f;
	g_ShadingSys_PSBuffer.spec_bloom_intensity = g_bGlobalSpecToggle ? g_fSpecBloomIntensity : 0.0f;

	// Set the vertex buffer... we probably need another vertex buffer here
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_postProcessVertBuffer.GetAddressOf(), &stride, &offset);

	// Set Primitive Topology
	// Opportunity for optimization? Make all draw calls use the same topology?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	// Temporarily disable ZWrite: we won't need it for this effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	/*
	// Set the proper blending state
	D3D11_BLEND_DESC blendDesc{};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	//blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	//blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_COLOR;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_DEST_COLOR;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	//blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_DEST_ALPHA;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	resources->InitBlendState(nullptr, &blendDesc);
	*/

	// Create a new viewport to render the offscreen buffer as a texture
	float screen_res_x = (float)resources->_backbufferWidth;
	float screen_res_y = (float)resources->_backbufferHeight;
	float black[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width    = screen_res_x / fZoomFactor;
	viewport.Height   = screen_res_y / fZoomFactor;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	resources->InitViewport(&viewport);
	
	// Set the lights and set the Shading System Constant Buffer
	SetLights(1.0f);

#ifdef DEATH_STAR
	static float iTime = 0.0f;
	g_DeathStarBuffer.iMouse[0] = 0;
	g_DeathStarBuffer.iMouse[1] = 0;
	g_DeathStarBuffer.iTime = iTime;
	g_DeathStarBuffer.iResolution[0] = g_fCurScreenWidth;
	g_DeathStarBuffer.iResolution[1] = g_fCurScreenHeight;
	resources->InitPSConstantBufferDeathStar(resources->_deathStarConstantBuffer.GetAddressOf(), &g_DeathStarBuffer);
	iTime += 0.1f;
#endif

	// Set the Vertex Shader Constant buffers
	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
		0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices
	// Set the SSDO Pixel Shader constant buffer
	g_SSAO_PSCBuffer.screenSizeX   = g_fCurScreenWidth;
	g_SSAO_PSCBuffer.screenSizeY   = g_fCurScreenHeight;
	g_SSAO_PSCBuffer.amplifyFactor = 1.0f / fZoomFactor;
	g_SSAO_PSCBuffer.fn_enable     = g_bFNEnable;
	//g_SSAO_PSCBuffer.shadow_enable = g_ShadowMapping.bEnabled;
	g_SSAO_PSCBuffer.moire_offset  = g_fMoireOffsetDir;
	//g_SSAO_PSCBuffer.debug		   = g_bShowSSAODebug;
	resources->InitPSConstantBufferSSAO(resources->_ssaoConstantBuffer.GetAddressOf(), &g_SSAO_PSCBuffer);

	// Set the layout
	context->IASetInputLayout(resources->_mainInputLayout);
	resources->InitVertexShader(resources->_mainVertexShader);
	
	// SSDO Direct Lighting, Left Image
	// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
	// Input: _randBuf, _depthBuf, _depthBuf2, _normBuf, _offscreenBuf (resolved here)
	// Output _ssaoBuf, _bentBuf
	{
		// Resolve offscreenBuf
		context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
			0, BACKBUFFER_FORMAT);
		ID3D11ShaderResourceView *srvs_pass1[5] = {
			resources->_depthBufSRV.Get(),
			resources->_normBufSRV.Get(),
			resources->_offscreenAsInputShaderResourceView.Get(),
			resources->_ssaoMaskSRV.Get(),
			resources->_offscreenAsInputBloomMaskSRV.Get(),
		};
		//if (g_SSAO_Type == SSO_BENT_NORMALS)
			//resources->InitPixelShader(resources->_ssdoDirectBentNormalsPS);
			//resources->InitPixelShader(resources->_deathStarPS);
		//else
		resources->InitPixelShader(resources->_ssdoDirectPS);
		//resources->InitPixelShader(resources->_ssaoPS); // Should be _ssdoDirectPS; but this will also work here

		//context->ClearRenderTargetView(resources->_renderTargetViewEmissionMask.Get(), black);
		if (g_bShowSSAODebug && !g_bBlurSSAO && !g_bEnableIndirectSSDO) {
			ID3D11RenderTargetView *rtvs[2] = {
				resources->_renderTargetView.Get(),
				resources->_renderTargetViewBentBuf.Get(),
				//resources->_renderTargetViewEmissionMask.Get(),
			};
			context->ClearRenderTargetView(resources->_renderTargetView, black);
			context->ClearRenderTargetView(resources->_renderTargetViewBentBuf, black);
			context->OMSetRenderTargets(2, rtvs, NULL);
			context->PSSetShaderResources(0, 5, srvs_pass1);
			context->Draw(6, 0);
			goto out1;
		}
		else {
			ID3D11RenderTargetView *rtvs[2] = {
				resources->_renderTargetViewSSAO.Get(),
				resources->_renderTargetViewBentBuf.Get(),
				//resources->_renderTargetViewEmissionMask.Get(),
			};
			context->ClearRenderTargetView(resources->_renderTargetViewSSAO, black);
			context->ClearRenderTargetView(resources->_renderTargetViewBentBuf, black);
			context->OMSetRenderTargets(2, rtvs, NULL);
			context->PSSetShaderResources(0, 5, srvs_pass1);
			context->Draw(6, 0);
		}
	}
	
	// Setup the constant buffers to upscale the buffers
	// The textures are always going to be g_fCurScreenWidth x g_fCurScreenHeight; but the step
	// size will be twice as big in the next pass due to the downsample, so we have to compensate
	// with a zoom factor:
	float fPixelScale = fZoomFactor;
	g_BloomPSCBuffer.pixelSizeX			= fPixelScale * g_fCurScreenWidthRcp;
	g_BloomPSCBuffer.pixelSizeY			= fPixelScale * g_fCurScreenHeightRcp;
	g_BloomPSCBuffer.amplifyFactor		= 1.0f / fZoomFactor;
	g_BloomPSCBuffer.uvStepSize			= 1.0f;
	g_BloomPSCBuffer.depth_weight		= g_SSAO_PSCBuffer.max_dist;
	// The SSDO component currently encodes the AO component in the Y channel. If
	// the SSDO buffer is displayed directly, it's hard to understand. So, instead, we
	// have to code the following logic:
	// If SSDO Indirect is disabled, then enable debug mode in the blur shader. This mode
	// returns ssao.xxx so that the SSDO direct component can be visualized properly
	// If SSDO Indirect is enabled, then we can't return ssao.xxx, because that'll wipe
	// out the AO component needed for SSDO Indirect. In that case we just pass along the
	// debug flag directly -- but the SSDO Direct buffer will be unreadable
	if (g_bShowSSAODebug) {
		if (!g_bEnableIndirectSSDO)
			g_BloomPSCBuffer.debug = 1;
		else
			g_BloomPSCBuffer.debug = g_SSAO_PSCBuffer.debug;
	}
	else
		g_BloomPSCBuffer.debug = 0;
	resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);

	// SSDO Direct Blur, Left Image
	// input: bloomOutput1 (with a copy of the ssaoBuf), depthBuf, bentBufR (with a copy of bentBuf), normBuf
	// output: ssaoBuf, bentBuf
	if (g_bBlurSSAO) 
		for (int i = 0; i < g_iSSAOBlurPasses; i++) {
			resources->InitPixelShader(resources->_ssdoBlurPS);
			// Copy the SSAO buffer to offscreenBufferAsInput/bloomOutput1(HDR) -- we'll use it as temp buffer
			// to blur the SSAO buffer
			//context->CopyResource(resources->_offscreenBufferAsInput, resources->_ssaoBuf);
			context->CopyResource(resources->_bloomOutput1, resources->_ssaoBuf);
			//context->CopyResource(resources->_offscreenBufferAsInput, resources->_ssEmissionMask);
			// Here I'm reusing bentBufR as a temporary buffer for bentBuf, in the SteamVR path I'll do
			// the opposite. This is just to avoid having to make a temporary buffer to blur the bent normals.
			context->CopyResource(resources->_bentBufR, resources->_bentBuf);
			// Clear the destination buffers: the blur will re-populate them
			context->ClearRenderTargetView(resources->_renderTargetViewSSAO.Get(), black);
			context->ClearRenderTargetView(resources->_renderTargetViewBentBuf.Get(), black);
			ID3D11ShaderResourceView *srvs[4] = {
					//resources->_offscreenAsInputShaderResourceView.Get(), // LDR
					resources->_bloomOutput1SRV.Get(), // HDR
					resources->_depthBufSRV.Get(),
					resources->_normBufSRV.Get(),
					resources->_bentBufSRV_R.Get(),
					//resources->_offscreenAsInputShaderResourceView.Get(), // emission mask
			};
			if (g_bShowSSAODebug && i == g_iSSAOBlurPasses - 1 && !g_bEnableIndirectSSDO && 	g_SSAO_Type != SSO_BENT_NORMALS) {
				context->ClearRenderTargetView(resources->_renderTargetView, black);
				// DirectX doesn't like mixing MSAA and non-MSAA RTVs. _renderTargetView is MSAA; but renderTargetViewBentBuf is not.
				// If both are set and MSAA is active, nothing gets rendered -- even with a NULL depth stencil.
				// The solution here is to avoid setting the renderTargetViewBentBuf if MSAA is active
				// Alternatively, we could change renderTargetViewBentBuf to be MSAA too, just like I did for ssMaskMSAA, etc; but... eh...
				ID3D11RenderTargetView *rtvs[2] = {
					resources->_renderTargetView.Get(), // resources->_renderTargetViewSSAO.Get(),
					resources->_useMultisampling ? NULL : resources->_renderTargetViewBentBuf.Get(),
					//resources->_useMultisampling ? NULL : resources->_renderTargetViewEmissionMask.Get(),
				};
				context->OMSetRenderTargets(2, rtvs, NULL);
				context->PSSetShaderResources(0, 4, srvs);
				// DEBUG: Enable the following line to display the bent normals (it will also blur the bent normals buffer
				//context->PSSetShaderResources(0, 1, resources->_bentBufSRV.GetAddressOf());
				// DEBUG: Enable the following line to display the normals
				//context->PSSetShaderResources(0, 1, resources->_normBufSRV.GetAddressOf());
				// DEBUG
				context->Draw(6, 0);
				goto out1;
			}
			else {
				ID3D11RenderTargetView *rtvs[2] = {
					resources->_renderTargetViewSSAO.Get(),
					resources->_renderTargetViewBentBuf.Get(),
					//resources->_renderTargetViewEmissionMask.Get(),
				};
				context->OMSetRenderTargets(2, rtvs, NULL);
				context->PSSetShaderResources(0, 4, srvs);
				context->Draw(6, 0);
			}
		}

	// SSDO Indirect Lighting, Left Image

	// Clear the Indirect SSDO buffer -- we have to do this regardless of whether the indirect
	// illumination is computed or not.
	context->ClearRenderTargetView(resources->_renderTargetViewSSAO_R.Get(), black);

	// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
	// Input: _depthBuf, _depthBuf2, _normBuf, offscreenAsInput (with a copy of _ssaoBuf -- not anymore! Should I fix this?)
	// Output _ssaoBufR
	if (g_bEnableIndirectSSDO)
	{
		resources->InitPixelShader(resources->_ssdoIndirectPS);
		// Set the SSDO pixel shader constant buffer
		//g_SSAO_PSCBuffer.screenSizeX = g_fCurScreenWidth  / fZoomFactor; // Not used in the shader
		//g_SSAO_PSCBuffer.screenSizeY = g_fCurScreenHeight / fZoomFactor; // Not used in the shader
		g_SSAO_PSCBuffer.amplifyFactor  = 1.0f / fZoomFactor;
		g_SSAO_PSCBuffer.amplifyFactor2 = 1.0f / fZoomFactor2;
		g_SSAO_PSCBuffer.moire_offset   = g_fMoireOffsetInd;
		g_SSAO_PSCBuffer.fn_enable      = g_bFNEnable;
		resources->InitPSConstantBufferSSAO(resources->_ssaoConstantBuffer.GetAddressOf(), &g_SSAO_PSCBuffer);

		// Copy the SSAO buffer to offscreenBufferAsInput -- this is the accumulated
		// color + SSDO buffer
		// Not anymore! The accumulation of color + SSDO is now done in the Add shader.
		// What happens if I just send the regular color buffer instead?
		//context->CopyResource(resources->_offscreenBufferAsInput/_bloomOutput1, resources->_ssaoBuf);

		// Resolve offscreenBuf, we need the original color buffer and it may have been overwritten
		// in the previous steps
		context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
			0, BACKBUFFER_FORMAT);
		ID3D11ShaderResourceView *srvs[3] = {
			resources->_depthBufSRV.Get(),  // FG Depth Buffer
			resources->_normBufSRV.Get(),   // Normal Buffer
			resources->_offscreenAsInputShaderResourceView.Get(), // Color Buffer
			//resources->_ssaoBufSRV.Get(),   // Direct SSDO from the previous pass
		};
		
		// DEBUG
		if (g_bShowSSAODebug && !g_bBlurSSAO && g_bEnableIndirectSSDO) {
			context->ClearRenderTargetView(resources->_renderTargetView, black);
			ID3D11RenderTargetView *rtvs[1] = {
				resources->_renderTargetView.Get(),
			};
			context->OMSetRenderTargets(1, rtvs, NULL);
		}
		// DEBUG
		else {
			// _renderTargetViewSSAO_R is used for the LEFT eye (and viceversa). THIS IS NOT A BUG
			context->ClearRenderTargetView(resources->_renderTargetViewSSAO_R, black);
			ID3D11RenderTargetView *rtvs[1] = {
				resources->_renderTargetViewSSAO_R.Get(),
			};
			context->OMSetRenderTargets(1, rtvs, NULL);
		}
		context->PSSetShaderResources(0, 3, srvs);
		context->Draw(6, 0);
	}

	// Blur the Indirect SSDO buffer
	if (g_bEnableIndirectSSDO && g_bBlurSSAO) {
		resources->InitPixelShader(resources->_ssdoBlurPS);
		for (int i = 0; i < g_iSSAOBlurPasses; i++) {
			// Copy the SSDO Indirect Buffer (ssaoBufR) to offscreenBufferAsInput -- we'll use it as temp buffer
			// to blur the SSAO buffer
			context->CopyResource(resources->_bloomOutput1, resources->_ssaoBufR); // _ssaoBufR is 16bit floating point, so we use bloomOutput as temp buffer
			// Here I'm reusing bentBufR as a temporary buffer for bentBuf, in the SteamVR path I'll do
			// the opposite. This is just to avoid having to make a temporary buffer to blur the bent normals.
			//context->CopyResource(resources->_bentBufR, resources->_bentBuf);
			// Clear the destination buffers: the blur will re-populate them
			context->ClearRenderTargetView(resources->_renderTargetViewSSAO_R.Get(), black);
			ID3D11ShaderResourceView *srvs[3] = {
					//resources->_offscreenAsInputShaderResourceView.Get(), // ssaoBufR
					resources->_bloomOutput1SRV.Get(), // ssaoBufR HDR
					resources->_depthBufSRV.Get(),
					resources->_normBufSRV.Get(),
					//resources->_bentBufSRV_R.Get(),
			};
			if (g_bShowSSAODebug && i == g_iSSAOBlurPasses - 1) {
				context->ClearRenderTargetView(resources->_renderTargetView, black);
				//context->OMSetRenderTargets(1, resources->_renderTargetViewSSAO_R.GetAddressOf(), NULL);
				ID3D11RenderTargetView *rtvs[2] = {
					//resources->_renderTargetViewSSAO_R.Get(),
					resources->_renderTargetView.Get(), // resources->_renderTargetViewBentBuf.Get(),
					NULL,
				};
				context->OMSetRenderTargets(2, rtvs, NULL);
				context->PSSetShaderResources(0, 3, srvs);
				// DEBUG: Enable the following line to display the bent normals (it will also blur the bent normals buffer
				//context->PSSetShaderResources(0, 1, resources->_bentBufSRV.GetAddressOf());
				// DEBUG: Enable the following line to display the normals
				//context->PSSetShaderResources(0, 1, resources->_normBufSRV.GetAddressOf());
				context->Draw(6, 0);
				goto out1;
			}
			else {
				context->ClearRenderTargetView(resources->_renderTargetViewSSAO_R, black);
				ID3D11RenderTargetView *rtvs[2] = {
					resources->_renderTargetViewSSAO_R.Get(),
					NULL,
				};
				context->OMSetRenderTargets(2, rtvs, NULL);
				context->PSSetShaderResources(0, 3, srvs);
				context->Draw(6, 0);
			}
		}
	}
	
	// Final combine, Left Image
	{
		// DEBUG: REMOVE LATER: WE DON'T NEED TO SET THIS BUFFER ON EVERY FRAME!
		//g_BloomPSCBuffer.debug = g_SSAO_PSCBuffer.debug;
		//resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);
		// DEBUG

		// input: offscreenAsInput (resolved here), bloomMask (removed?), ssaoBuf
		// output: offscreenBuf, bloomMask?
		//if (g_SSAO_Type == SSO_BENT_NORMALS)
			//resources->InitPixelShader(resources->_hyperspacePS);
#ifndef DEATH_STAR
			//resources->InitPixelShader(resources->_ssdoAddBentNormalsPS);
#else
			resources->InitPixelShader(resources->_deathStarPS);
#endif
		if (!g_bEnableHeadLights)
			resources->InitPixelShader(resources->_ssdoAddPS);
		else
			resources->InitPixelShader(resources->_headLightsPS);
			
		// Reset the viewport for the final SSAO combine
		viewport.TopLeftX	= 0.0f;
		viewport.TopLeftY	= 0.0f;
		viewport.Width		= screen_res_x;
		viewport.Height		= screen_res_y;
		viewport.MaxDepth	= D3D11_MAX_DEPTH;
		viewport.MinDepth	= D3D11_MIN_DEPTH;
		resources->InitViewport(&viewport);
		// ssaoBuf was bound as an RTV, so let's bind the RTV first to unbind ssaoBuf
		// so that it can be used as an SRV
		ID3D11RenderTargetView *rtvs[5] = {
			resources->_renderTargetView.Get(),			 // MSAA
			resources->_renderTargetViewBloomMask.Get(), // MSAA
			NULL, //resources->_renderTargetViewBentBuf.Get(), // DEBUG REMOVE THIS LATER! Non-MSAA
			NULL, NULL,
		};
		context->OMSetRenderTargets(5, rtvs, NULL);
		// Resolve offscreenBuf
		context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
			0, BACKBUFFER_FORMAT);

		ID3D11ShaderResourceView *srvs_pass2[9] = {
			resources->_offscreenAsInputShaderResourceView.Get(),	// Color buffer
			resources->_ssaoBufSRV.Get(),							// SSDO Direct Component
			resources->_ssaoBufSRV_R.Get(),							// SSDO Indirect
			resources->_ssaoMaskSRV.Get(),							// SSAO Mask

			resources->_depthBufSRV.Get(),							// Depth buffer
			resources->_normBufSRV.Get(),							// Normals buffer
			resources->_bentBufSRV.Get(),							// Bent Normals
			resources->_ssMaskSRV.Get(),							// Shading System Mask buffer

			g_ShadowMapping.bEnabled ? 
				resources->_shadowMapArraySRV.Get() : NULL,			// The shadow map
		};
		context->PSSetShaderResources(0, 9, srvs_pass2);
		context->Draw(6, 0);
	}

	/*******************************************************************************
	 START PROCESS FOR THE RIGHT EYE, STEAMVR MODE
	 *******************************************************************************/
out1:
	// Draw the right image when SteamVR is enabled
	if (g_bUseSteamVR) {
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width    = screen_res_x / fZoomFactor;
		viewport.Height   = screen_res_y / fZoomFactor;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		resources->InitViewport(&viewport);

		// SSDO Direct Lighting, right eye
		// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
		// Input: _randBuf, _depthBuf, _depthBuf2, _normBuf, _offscreenBuf (resolved here)
		// Output _ssaoBuf, _bentBuf
		{
			// Resolve offscreenBuf
			context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR,
				0, BACKBUFFER_FORMAT);
			ID3D11ShaderResourceView *srvs_pass1[5] = {
				resources->_depthBufSRV_R.Get(),
				resources->_normBufSRV_R.Get(),
				resources->_offscreenAsInputShaderResourceViewR.Get(),
				resources->_ssaoMaskSRV_R.Get(),
				resources->_offscreenAsInputBloomMaskSRV_R.Get(),
			};
			
			resources->InitPixelShader(resources->_ssdoDirectPS);
			if (g_bShowSSAODebug && !g_bBlurSSAO && !g_bEnableIndirectSSDO) {
				ID3D11RenderTargetView *rtvs[2] = {
					resources->_renderTargetViewR.Get(),
					resources->_renderTargetViewBentBufR.Get(),
				};
				context->ClearRenderTargetView(resources->_renderTargetViewR, black);
				context->ClearRenderTargetView(resources->_renderTargetViewBentBufR, black);
				context->OMSetRenderTargets(2, rtvs, NULL);
				context->PSSetShaderResources(0, 5, srvs_pass1);
				context->Draw(6, 0);
				goto out2;
			}
			else {
				ID3D11RenderTargetView *rtvs[2] = {
					resources->_renderTargetViewSSAO_R.Get(),
					resources->_renderTargetViewBentBufR.Get()
				};
				context->ClearRenderTargetView(resources->_renderTargetViewSSAO_R, black);
				context->ClearRenderTargetView(resources->_renderTargetViewBentBufR, black);
				context->OMSetRenderTargets(2, rtvs, NULL);
				context->PSSetShaderResources(0, 5, srvs_pass1);
				context->Draw(6, 0);
			}
		}

		// Setup the constant buffers to upscale the buffers
		// The textures are always going to be g_fCurScreenWidth x g_fCurScreenHeight; but the step
		// size will be twice as big in the next pass due to the downsample, so we have to compensate
		// with a zoom factor:
		float fPixelScale = fZoomFactor;
		g_BloomPSCBuffer.pixelSizeX			= fPixelScale * g_fCurScreenWidthRcp;
		g_BloomPSCBuffer.pixelSizeY			= fPixelScale * g_fCurScreenHeightRcp;
		g_BloomPSCBuffer.amplifyFactor		= 1.0f / fZoomFactor;
		g_BloomPSCBuffer.uvStepSize			= 1.0f;
		g_BloomPSCBuffer.depth_weight		= g_SSAO_PSCBuffer.max_dist;
		// The SSDO component currently encodes the AO component in the Y channel. If
		// the SSDO buffer is displayed directly, it's hard to understand. So, instead, we
		// have to code the following logic:
		// If SSDO Indirect is disabled, then enable debug mode in the blur shader. This mode
		// returns ssao.xxx so that the SSDO direct component can be visualized properly
		// If SSDO Indirect is enabled, then we can't return ssao.xxx, because that'll wipe
		// out the AO component needed for SSDO Indirect. In that case we just pass along the
		// debug flag directly -- but the SSDO Direct buffer will be unreadable
		if (g_bShowSSAODebug) {
			if (!g_bEnableIndirectSSDO)
				g_BloomPSCBuffer.debug = 1;
			else
				g_BloomPSCBuffer.debug = g_SSAO_PSCBuffer.debug;
		}
		else
			g_BloomPSCBuffer.debug = 0;
		resources->InitPSConstantBufferBloom(resources->_bloomConstantBuffer.GetAddressOf(), &g_BloomPSCBuffer);

		// SSDO Direct Blur, Right Image
		// input: bloomOutput1 (with a copy of the ssaoBufR), depthBufR, bentBuf (with a copy of bentBufR), normBufR
		// output: ssaoBufR, bentBufR
		if (g_bBlurSSAO)
			for (int i = 0; i < g_iSSAOBlurPasses; i++) {
				resources->InitPixelShader(resources->_ssdoBlurPS);
				// Copy the SSAO buffer to offscreenBufferAsInput/_bloomOutput1(HDR) -- we'll use it as temp buffer
				// to blur the SSAO buffer
				context->CopyResource(resources->_bloomOutput1, resources->_ssaoBufR);
				// Here I'm reusing bentBuf as a temporary buffer for bentBufR
				// This is just to avoid having to make a temporary buffer to blur the bent normals.
				context->CopyResource(resources->_bentBuf, resources->_bentBufR);
				// Clear the destination buffers: the blur will re-populate them
				context->ClearRenderTargetView(resources->_renderTargetViewSSAO_R.Get(), black);
				context->ClearRenderTargetView(resources->_renderTargetViewBentBufR.Get(), black);
				ID3D11ShaderResourceView *srvs[4] = {
						//resources->_offscreenAsInputShaderResourceViewR.Get(), // LDR
						resources->_bloomOutput1SRV.Get(), // HDR
						resources->_depthBufSRV_R.Get(),
						resources->_normBufSRV_R.Get(),
						resources->_bentBufSRV.Get(),
				};
				if (g_bShowSSAODebug && i == g_iSSAOBlurPasses - 1 && !g_bEnableIndirectSSDO) {
					context->ClearRenderTargetView(resources->_renderTargetViewR, black);
					// Don't mix MSAA and non-MSAA RTVs:
					ID3D11RenderTargetView *rtvs[2] = {
						resources->_renderTargetViewR.Get(), // resources->_renderTargetViewSSAO_R.Get(),
						resources->_useMultisampling ? NULL : resources->_renderTargetViewBentBufR.Get(),
					};
					context->OMSetRenderTargets(2, rtvs, NULL);
					context->PSSetShaderResources(0, 4, srvs);
					// DEBUG: Enable the following line to display the bent normals (it will also blur the bent normals buffer
					//context->PSSetShaderResources(0, 1, resources->_bentBufSRV_R.GetAddressOf());
					// DEBUG: Enable the following line to display the normals
					//context->PSSetShaderResources(0, 1, resources->_normBufSRV_R.GetAddressOf());
					context->Draw(6, 0);
					goto out2;
				}
				else {
					ID3D11RenderTargetView *rtvs[2] = {
						resources->_renderTargetViewSSAO_R.Get(),
						resources->_renderTargetViewBentBufR.Get()
					};
					context->OMSetRenderTargets(2, rtvs, NULL);
					context->PSSetShaderResources(0, 4, srvs);
					context->Draw(6, 0);
				}
			}
	
		// SSDO Indirect Lighting, Right Image

		// Clear the Indirect SSDO buffer -- we have to do this regardless of whether the indirect
		// illumination is computed or not. For the right image, the indirect illumination buffer
		// is ssaoBufR:
		context->ClearRenderTargetView(resources->_renderTargetViewSSAO.Get(), black);

		// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
		// Input: _depthBuf, _depthBuf2, _normBuf, offscreenAsInput (with a copy of _ssaoBuf -- not anymore! Should I fix this?)
		// Output _ssaoBufR
		if (g_bEnableIndirectSSDO)
		{
			resources->InitPixelShader(resources->_ssdoIndirectPS);
			// Set the SSDO pixel shader constant buffer
			//g_SSAO_PSCBuffer.screenSizeX = g_fCurScreenWidth  / fZoomFactor; // Not used in the shader
			//g_SSAO_PSCBuffer.screenSizeY = g_fCurScreenHeight / fZoomFactor; // Not used in the shader
			g_SSAO_PSCBuffer.amplifyFactor  = 1.0f / fZoomFactor;
			g_SSAO_PSCBuffer.amplifyFactor2 = 1.0f / fZoomFactor2;
			g_SSAO_PSCBuffer.moire_offset = g_fMoireOffsetInd;
			g_SSAO_PSCBuffer.fn_enable		= g_bFNEnable;
			resources->InitPSConstantBufferSSAO(resources->_ssaoConstantBuffer.GetAddressOf(), &g_SSAO_PSCBuffer);

			// Copy the SSAO buffer to offscreenBufferAsInput -- this is the accumulated
			// color + SSDO buffer
			// Not anymore! The accumulation of color + SSDO is now done in the Add shader.
			// What happens if I just send the regular color buffer instead?
			//context->CopyResource(resources->_offscreenBufferAsInput/_bloomOutput1, resources->_ssaoBuf);

			// Resolve offscreenBuf, we need the original color buffer and it may have been overwritten
			// in the previous steps
			context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR,
				0, BACKBUFFER_FORMAT);
			ID3D11ShaderResourceView *srvs[3] = {
				resources->_depthBufSRV_R.Get(),  // FG Depth Buffer
				resources->_normBufSRV_R.Get(),   // Normal Buffer
				resources->_offscreenAsInputShaderResourceViewR.Get(), // Color Buffer
				//resources->_ssaoBufSRV_R.Get(),   // Direct SSDO from previous pass
			};

			// DEBUG
			if (g_bShowSSAODebug && !g_bBlurSSAO && g_bEnableIndirectSSDO) {
				context->ClearRenderTargetView(resources->_renderTargetViewR, black);
				ID3D11RenderTargetView *rtvs[1] = {
					resources->_renderTargetViewR.Get(),
				};
				context->OMSetRenderTargets(1, rtvs, NULL);
			}
			// DEBUG
			else {
				// _renderTargetViewSSAO is used for the RIGHT eye (and viceversa). THIS IS NOT A BUG
				context->ClearRenderTargetView(resources->_renderTargetViewSSAO, black);
				ID3D11RenderTargetView *rtvs[1] = {
					resources->_renderTargetViewSSAO.Get(),
				};
				context->OMSetRenderTargets(1, rtvs, NULL);
			}
			context->PSSetShaderResources(0, 3, srvs);
			context->Draw(6, 0);
		}

		// Blur the Indirect SSDO buffer
		if (g_bEnableIndirectSSDO && g_bBlurSSAO) {
			resources->InitPixelShader(resources->_ssdoBlurPS);
			for (int i = 0; i < g_iSSAOBlurPasses; i++) {
				// Copy the SSDO Indirect Buffer (ssaoBuf) to offscreenBufferAsInputR/_bloomOutput1 -- we'll use it as temp buffer
				// to blur the SSAO buffer
				context->CopyResource(resources->_bloomOutput1, resources->_ssaoBuf);
				// Here I'm reusing bentBuf as a temporary buffer for bentBufR, This is just to avoid 
				// having to make a temporary buffer to blur the bent normals.
				//context->CopyResource(resources->_bentBuf, resources->_bentBufR);
				// Clear the destination buffers: the blur will re-populate them
				context->ClearRenderTargetView(resources->_renderTargetViewSSAO.Get(), black);
				//context->ClearRenderTargetView(resources->_renderTargetViewBentBufR.Get(), bgColor);
				ID3D11ShaderResourceView *srvs[3] = {
						//resources->_offscreenAsInputShaderResourceViewR.Get(), // ssaoBuf, direct lighting
						resources->_bloomOutput1SRV.Get(), // ssaoBuf, direct lighting HDR
						resources->_depthBufSRV_R.Get(),
						resources->_normBufSRV_R.Get(),
						//resources->_bentBufSRV.Get(), // with a copy of bentBufR
				};
				if (g_bShowSSAODebug && i == g_iSSAOBlurPasses - 1) {
					context->ClearRenderTargetView(resources->_renderTargetViewR, black);
					//context->OMSetRenderTargets(1, resources->_renderTargetViewSSAO.GetAddressOf(), NULL);
					ID3D11RenderTargetView *rtvs[2] = {
						//resources->_renderTargetViewSSAO.Get(),
						resources->_renderTargetViewR.Get(), // resources->_renderTargetViewBentBufR.Get(),
						NULL,
					};
					context->OMSetRenderTargets(2, rtvs, NULL);
					context->PSSetShaderResources(0, 3, srvs);
					// DEBUG: Enable the following line to display the bent normals (it will also blur the bent normals buffer
					//context->PSSetShaderResources(0, 1, resources->_bentBufSRV_R.GetAddressOf());
					// DEBUG: Enable the following line to display the normals
					//context->PSSetShaderResources(0, 1, resources->_normBufSRV_R.GetAddressOf());
					context->Draw(6, 0);
					goto out1;
				}
				else {
					ID3D11RenderTargetView *rtvs[2] = {
						resources->_renderTargetViewSSAO.Get(), // ssaoBuf is now the indirect lighting
						NULL, // bentBufR output
					};
					context->OMSetRenderTargets(2, rtvs, NULL);
					context->PSSetShaderResources(0, 3, srvs);
					context->Draw(6, 0);
				}
			}
		}

		// Final combine, Right Image
		{
			// input: offscreenAsInputR (resolved here), bloomMaskR, ssaoBufR
			// output: offscreenBufR

			if (!g_bEnableHeadLights)
				resources->InitPixelShader(resources->_ssdoAddPS);
			else
				resources->InitPixelShader(resources->_headLightsPS);
			// Reset the viewport for the final SSAO combine
			viewport.TopLeftX	= 0.0f;
			viewport.TopLeftY	= 0.0f;
			viewport.Width		= screen_res_x;
			viewport.Height		= screen_res_y;
			viewport.MaxDepth	= D3D11_MAX_DEPTH;
			viewport.MinDepth	= D3D11_MIN_DEPTH;
			resources->InitViewport(&viewport);
			// ssaoBufR was bound as an RTV, so let's bind the RTV first to unbind ssaoBufR
			// so that it can be used as an SRV
			ID3D11RenderTargetView *rtvs[5] = {
				resources->_renderTargetViewR.Get(),			  // MSAA
				resources->_renderTargetViewBloomMaskR.Get(), // MSAA
				NULL, //resources->_renderTargetViewBentBufR.Get(), // DEBUG REMOVE THIS LATER Non-MSAA
				NULL, NULL
			};
			context->OMSetRenderTargets(5, rtvs, NULL);
			// Resolve offscreenBuf
			context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR,
				0, BACKBUFFER_FORMAT);
			ID3D11ShaderResourceView *srvs_pass2[9] = {
				resources->_offscreenAsInputShaderResourceViewR.Get(),	// Color buffer
				resources->_ssaoBufSRV_R.Get(),							// SSDO Direct Component
				resources->_ssaoBufSRV.Get(),							// SSDO Indirect Component
				resources->_ssaoMaskSRV_R.Get(),						// SSAO Mask

				resources->_depthBufSRV_R.Get(),						// Depth buffer
				resources->_normBufSRV_R.Get(),							// Normals buffer
				resources->_bentBufSRV_R.Get(),							// Bent Normals
				resources->_ssMaskSRV_R.Get(),							// Shading System Mask buffer

				g_ShadowMapping.bEnabled ?
					resources->_shadowMapArraySRV.Get() : NULL,			// The shadow map
			};
			context->PSSetShaderResources(0, 9, srvs_pass2);
			context->Draw(6, 0);
		}
	}

out2:
	// Restore previous rendertarget, etc
	// TODO: Is this really needed?
	viewport.Width  = screen_res_x;
	viewport.Height = screen_res_y;
	resources->InitViewport(&viewport);
	resources->InitInputLayout(resources->_inputLayout);
	context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(),
		resources->_depthStencilViewL.Get());
}

/* Regular deferred shading with fake HDR, no SSAO */
void PrimarySurface::DeferredPass() {
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;
	float x0, y0, x1, y1;

	GetScreenLimitsInUVCoords(&x0, &y0, &x1, &y1);
	g_SSAO_PSCBuffer.x0 = x0;
	g_SSAO_PSCBuffer.y0 = y0;
	g_SSAO_PSCBuffer.x1 = x1;
	g_SSAO_PSCBuffer.y1 = y1;
	g_SSAO_PSCBuffer.enable_dist_fade = (float)g_b3DSkydomePresent;
	g_ShadingSys_PSBuffer.spec_intensity = g_bGlobalSpecToggle ? g_fSpecIntensity : 0.0f;
	g_ShadingSys_PSBuffer.spec_bloom_intensity = g_bGlobalSpecToggle ? g_fSpecBloomIntensity : 0.0f;

	// Set the vertex buffer... we probably need another vertex buffer here
	UINT stride = sizeof(MainVertex);
	UINT offset = 0;
	resources->InitVertexBuffer(resources->_postProcessVertBuffer.GetAddressOf(), &stride, &offset);

	// Set Primitive Topology
	// Opportunity for optimization? Make all draw calls use the same topology?
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	// Temporarily disable ZWrite: we won't need it for this effect
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	// Create a new viewport to render the offscreen buffer as a texture
	float screen_res_x = (float)resources->_backbufferWidth;
	float screen_res_y = (float)resources->_backbufferHeight;
	float black[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width    = screen_res_x;
	viewport.Height   = screen_res_y;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	resources->InitViewport(&viewport);

	// Set the lights and the Shading System Constant Buffer
	SetLights(0.0f);

	// Set the Vertex Shader Constant buffers
	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
		0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices
	// Set the SSDO Pixel Shader constant buffer
	g_SSAO_PSCBuffer.screenSizeX = g_fCurScreenWidth;
	g_SSAO_PSCBuffer.screenSizeY = g_fCurScreenHeight;
	g_SSAO_PSCBuffer.amplifyFactor = 1.0f;
	g_SSAO_PSCBuffer.fn_enable = g_bFNEnable;
	//g_SSAO_PSCBuffer.shadow_enable = g_ShadowMapping.bEnabled;
	g_SSAO_PSCBuffer.debug = g_bShowSSAODebug;
	resources->InitPSConstantBufferSSAO(resources->_ssaoConstantBuffer.GetAddressOf(), &g_SSAO_PSCBuffer);

	// Set the layout
	context->IASetInputLayout(resources->_mainInputLayout);
	resources->InitVertexShader(resources->_mainVertexShader);

	if (!g_bEnableHeadLights)
		resources->InitPixelShader(resources->_ssdoAddPS);
	else
		resources->InitPixelShader(resources->_headLightsPS);

	// Set the PCF sampler state
	if (g_ShadowMapping.bEnabled)
		context->PSSetSamplers(8, 1, resources->_shadowPCFSamplerState.GetAddressOf());

	// The pos/depth texture must be resolved to _depthAsInput/_depthAsInputR already
	// Deferred pass, Left Image
	{
		// input: offscreenAsInput (resolved here), normBuf
		// output: offscreenBuf, bloomMask
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width	  = screen_res_x;
		viewport.Height   = screen_res_y;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		resources->InitViewport(&viewport);
		// ssaoBuf was bound as an RTV, so let's bind the RTV first to unbind ssaoBuf
		// so that it can be used as an SRV
		ID3D11RenderTargetView *rtvs[5] = {
			resources->_renderTargetView.Get(),
			resources->_renderTargetViewBloomMask.Get(),
			NULL, // resources->_renderTargetViewBentBuf.Get(), // DEBUG REMOVE THIS LATER! 
			NULL, NULL,
		};
		context->OMSetRenderTargets(5, rtvs, NULL);
		// Resolve offscreenBuf
		context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer,
			0, BACKBUFFER_FORMAT);
		ID3D11ShaderResourceView *srvs_pass2[9] = {
			resources->_offscreenAsInputShaderResourceView.Get(),	// Color buffer
			NULL,													// Bent Normals (HDR) or SSDO Direct Component (LDR)
			NULL, //resources->_ssaoBufSRV_R.Get(),					// SSDO Indirect
			resources->_ssaoMaskSRV.Get(),							// SSAO Mask

			resources->_depthBufSRV.Get(),							// Depth buffer
			resources->_normBufSRV.Get(),							// Normals buffer
			NULL,													// Bent Normals
			resources->_ssMaskSRV.Get(),							// Shading System buffer

			g_ShadowMapping.bEnabled ?								// The shadow map
				resources->_shadowMapArraySRV.Get() : NULL,
		};
		context->PSSetShaderResources(0, 9, srvs_pass2);
		context->Draw(6, 0);
	}

	// Deferred pass, Right image
	if (g_bUseSteamVR)
	{
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width    = screen_res_x;
		viewport.Height   = screen_res_y;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		resources->InitViewport(&viewport);

		// ssaoBufR was bound as an RTV, so let's bind the RTV first to unbind ssaoBufR
		// so that it can be used as an SRV
		ID3D11RenderTargetView *rtvs[5] = {
			resources->_renderTargetViewR.Get(),
			resources->_renderTargetViewBloomMaskR.Get(),
			NULL, NULL, NULL
		};
		context->OMSetRenderTargets(5, rtvs, NULL);
		// Resolve offscreenBuf
		context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR,
			0, BACKBUFFER_FORMAT);
		ID3D11ShaderResourceView *srvs_pass2[9] = {
			resources->_offscreenAsInputShaderResourceViewR.Get(),	// Color buffer
			NULL,													// SSDO Direct Component
			NULL,													// SSDO Indirect Component
			resources->_ssaoMaskSRV_R.Get(),						// SSAO Mask

			resources->_depthBufSRV_R.Get(),						// Depth buffer
			resources->_normBufSRV_R.Get(),							// Normals buffer
			NULL,													// Bent Normals
			resources->_ssMaskSRV_R.Get(),							// Shading System buffer

			g_ShadowMapping.bEnabled ?
				resources->_shadowMapArraySRV.Get() : NULL,			// The shadow map
		};
		context->PSSetShaderResources(0, 9, srvs_pass2);
		context->Draw(6, 0);
	}

	// Restore previous rendertarget, etc
	// TODO: Is this really needed?
	viewport.Width = screen_res_x;
	viewport.Height = screen_res_y;
	resources->InitViewport(&viewport);
	resources->InitInputLayout(resources->_inputLayout);
	context->OMSetRenderTargets(1, this->_deviceResources->_renderTargetView.GetAddressOf(),
		this->_deviceResources->_depthStencilViewL.Get());
}


void InitHeadingMatrix() {
	/*
	Matrix4 rotX, refl;
	rotX.identity();
	rotX.rotateX(90.0f);
	refl.set(
		1,  0,  0,  0,
		0, -1,  0,  0,
		0,  0,  1,  0,
		0,  0,  0,  1
	);
	g_ReflRotX = refl * rotX;
	static bool bFirstTime = true;
	if (bFirstTime) {
		ShowMatrix4(g_ReflRotX, "g_ReflRotX");
	}

	// This is the matrix we want (looks like it's a simple Y-Z axis swap
		[14444][DBG] 1.000000, 0.000000, 0.000000, 0.000000
		[14444][DBG] 0.000000, 0.000000, 1.000000, 0.000000
		[14444][DBG] 0.000000, 1.000000, 0.000000, 0.000000
		[14444][DBG] 0.000000, 0.000000, 0.000000, 1.000000
	*/

	g_ReflRotX.set(
		1.0, 0.0, 0.0, 0.0, // 1st column
		0.0, 0.0, 1.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	);
}

/*
 * For a simple direction vector Fs (like a light vector), return a simple matrix that aligns
 * the given vector with (0,0,1) (if invert == false).
 */
Matrix4 GetSimpleDirectionMatrix(Vector4 Fs, bool invert) {
	Vector4 temp = Fs;
	temp.normalize(); // TODO: Check if we need to normalize every time
	// Rotate the vector around the X-axis to align it with the X-Z plane
	float AngX = atan2(temp.y, temp.z) / 0.01745f;
	float AngY = -asin(temp.x) / 0.01745f;
	Matrix4 rotX, rotY, rotFull;
	rotX.rotateX(AngX);
	rotY.rotateY(AngY);
	rotFull = rotY * rotX;
	// DEBUG
	//Vector4 debugFs = rotFull * temp;
	// The following line should always display: (0,0,1)
	//log_debug("[DBG] debugFs: %0.3f, %0.3f, %0.3f", debugFs.x, debugFs.y, debugFs.z);
	// DEBUG
	if (invert)
		rotFull.invert(); // Full inversion for now, should replace for either transpose, or just invert AngX,AngY
	return rotFull;
}

/*
 * Compute the current ship's orientation. Returns:
 * Rs: The "Right" vector in global coordinates
 * Us: The "Up" vector in global coordinates
 * Fs: The "Forward" vector in global coordinates
 * A viewMatrix that maps [Rs, Us, Fs] to the major [X, Y, Z] axes
 */
Matrix4 GetCurrentHeadingMatrix(Vector4 &Rs, Vector4 &Us, Vector4 &Fs, bool invert=false, bool debug=false)
{
	const float DEG2RAD = 3.141593f / 180;
	float yaw, pitch, roll;
	Matrix4 rotMatrixFull, rotMatrixYaw, rotMatrixPitch, rotMatrixRoll;
	Vector4 T, B, N;
	// Compute the full rotation
	yaw   = PlayerDataTable[*g_playerIndex].yaw   / 65536.0f * 360.0f;
	pitch = PlayerDataTable[*g_playerIndex].pitch / 65536.0f * 360.0f;
	roll  = PlayerDataTable[*g_playerIndex].roll  / 65536.0f * 360.0f;

	// To test how (x,y,z) is aligned with either the Y+ or Z+ axis, just multiply rotMatrixPitch * rotMatrixYaw * (x,y,z)
	//Matrix4 rotMatrixFull, rotMatrixYaw, rotMatrixPitch, rotMatrixRoll;
	rotMatrixFull.identity();
	rotMatrixYaw.identity();   rotMatrixYaw.rotateY(-yaw);
	rotMatrixPitch.identity(); rotMatrixPitch.rotateX(-pitch);
	rotMatrixRoll.identity();  rotMatrixRoll.rotateY(roll);

	// rotMatrixYaw aligns the orientation with the y-z plane (x --> 0)
	// rotMatrixPitch * rotMatrixYaw aligns the orientation with y+ (x --> 0 && z --> 0)
	// so the remaining rotation must be around the y axis (?)
	// DEBUG, x = z, y = x, z = y;
	// The yaw is indeed the y-axis rotation, it goes from -180 to 0 to 180.
	// When pitch == 90, the craft is actually seeing the horizon
	// When pitch == 0, the craft is looking towards the sun
	// New approach: let's build a TBN system here to avoid the gimbal lock problem
	float cosTheta, cosPhi, sinTheta, sinPhi;
	cosTheta = cos(yaw * DEG2RAD), sinTheta = sin(yaw * DEG2RAD);
	cosPhi = cos(pitch * DEG2RAD), sinPhi = sin(pitch * DEG2RAD);
	N.z = cosTheta * sinPhi;
	N.x = sinTheta * sinPhi;
	N.y = cosPhi;
	N.w = 0;

	// This transform chain will always transform (N.x,N.y,N.z) into (0, 1, 0)
	// To make an orthonormal basis, we need x+ and z+
	N = rotMatrixPitch * rotMatrixYaw * N;
	//log_debug("[DBG] N(DEBUG): %0.3f, %0.3f, %0.3f", N.x, N.y, N.z); // --> displays (0,1,0)
	B.x = 0; B.y = 0; B.z = -1; B.w = 0;
	T.x = 1; T.y = 0; T.z =  0; T.w = 0;
	B = rotMatrixRoll * B;
	T = rotMatrixRoll * T;
	// Our new basis is T,B,N; but we need to invert the yaw/pitch rotation we applied
	rotMatrixFull = rotMatrixPitch * rotMatrixYaw;
	rotMatrixFull.invert();
	T = rotMatrixFull * T;
	B = rotMatrixFull * B;
	N = rotMatrixFull * N;
	// Our TBN basis is now in absolute coordinates
	Fs = g_ReflRotX * N;
	Us = g_ReflRotX * B;
	Rs = g_ReflRotX * T;
	Fs.w = 0; Rs.w = 0; Us.w = 0;
	// This transform chain gets us the orientation of the craft in XWA's coord system:
	// [1,0,0] is right, [0,1,0] is forward, [0,0,1] is up
	//log_debug("[DBG] [GUN] Fs: [%0.3f, %0.3f, %0.3f] ypr: %0.3f, %0.3f, %0.3f ***********", Fs.x, Fs.y, Fs.z, yaw, pitch, roll);

	// Facing forward we get: T: [1, 0, 0], B: [0, -1, 0], N: [0, 0, -1]
	if (debug)
		log_debug("[DBG] [H] Rs: [%0.3f, %0.3f, %0.3f], Us: [%0.3f, %0.3f, %0.3f], Fs: [%0.3f, %0.3f, %0.3f]",
			Rs.x, Rs.y, Rs.z, Us.x, Us.y, Us.z, Fs.x, Fs.y, Fs.z);

	Matrix4 viewMatrix;
	if (!invert) { // Transform current ship's heading to Global Coordinates (Major Axes)
		viewMatrix = Matrix4(
			Rs.x, Us.x, Fs.x, 0,
			Rs.y, Us.y, Fs.y, 0,
			Rs.z, Us.z, Fs.z, 0,
			0, 0, 0, 1
		);
		// Rs, Us, Fs is an orthonormal basis
	} else { // Transform Global Coordinates to the Ship's Coordinate System
		viewMatrix = Matrix4(
			Rs.x, Rs.y, Rs.z, 0,
			Us.x, Us.y, Us.z, 0,
			Fs.x, Fs.y, Fs.z, 0,
			0, 0, 0, 1
		);
		// Rs, Us, Fs is an orthonormal basis
	}
	return viewMatrix;
}

/*
 * Get the combined Heading + Cockpit Camera View matrix that transforms from XWA's system to
 * PixelShader (Z+) coordinates.
 */
Matrix4 GetCurrentHeadingViewMatrix() {
	Vector4 Rs, Us, Fs;
	Matrix4 H = GetCurrentHeadingMatrix(Rs, Us, Fs, false, false);

	float viewYaw, viewPitch;
	if (PlayerDataTable[*g_playerIndex].externalCamera) {
		viewYaw   = PlayerDataTable[*g_playerIndex].cameraYaw   / 65536.0f * 360.0f;
		viewPitch = PlayerDataTable[*g_playerIndex].cameraPitch / 65536.0f * 360.0f;
	}
	else {
		viewYaw   = PlayerDataTable[*g_playerIndex].cockpitCameraYaw   / 65536.0f * 360.0f;
		viewPitch = PlayerDataTable[*g_playerIndex].cockpitCameraPitch / 65536.0f * 360.0f;
	}
	Matrix4 viewMatrixYaw, viewMatrixPitch;
	viewMatrixYaw.identity();
	viewMatrixPitch.identity();
	viewMatrixYaw.rotateY(g_fViewYawSign   * viewYaw);
	viewMatrixYaw.rotateX(g_fViewPitchSign * viewPitch);
	return viewMatrixPitch * viewMatrixYaw * H;
}

void GetCockpitViewMatrix(Matrix4 *result, bool invert=true) {
	float yaw, pitch;
	
	if (PlayerDataTable[*g_playerIndex].externalCamera) {
		yaw   = (float)PlayerDataTable[*g_playerIndex].cameraYaw   / 65536.0f * 360.0f + 180.0f;
		pitch = (float)PlayerDataTable[*g_playerIndex].cameraPitch / 65536.0f * 360.0f;
	}
	else {
		yaw   = (float)PlayerDataTable[*g_playerIndex].cockpitCameraYaw   / 65536.0f * 360.0f + 180.0f;
		pitch = (float)PlayerDataTable[*g_playerIndex].cockpitCameraPitch / 65536.0f * 360.0f;
	}

	Matrix4 rotMatrixFull, rotMatrixYaw, rotMatrixPitch, rotMatrixRoll;
	rotMatrixFull.identity();
	rotMatrixYaw.identity();   rotMatrixYaw.rotateY(yaw);
	rotMatrixPitch.identity(); rotMatrixPitch.rotateX(pitch);
	rotMatrixFull = rotMatrixPitch * rotMatrixYaw;
	if (invert)
		*result = rotMatrixFull.invert();
	else
		*result = rotMatrixFull;
}

/*
 * Returns the current camera or external matrix for the speed effect. The coord sys is slightly
 * different from the one used in the shadertoy pixel shaders, so we need a new version of
 * GetCockpitViewMatrix.
 */
void PrimarySurface::GetCockpitViewMatrixSpeedEffect(Matrix4 *result, bool invert = true) {
	
	Matrix4 rotMatrixFull;

	if (PlayerDataTable[*g_playerIndex].gunnerTurretActive)
	{
		GetGunnerTurretViewMatrixSpeedEffect(&rotMatrixFull);
	}
	else 
	{
		float yaw, pitch;

		if (PlayerDataTable[*g_playerIndex].externalCamera) {
			yaw = -(float)PlayerDataTable[*g_playerIndex].cameraYaw / 65536.0f * 360.0f;
			pitch = (float)PlayerDataTable[*g_playerIndex].cameraPitch / 65536.0f * 360.0f;
		}
		else {
			yaw = -(float)PlayerDataTable[*g_playerIndex].cockpitCameraYaw / 65536.0f * 360.0f;
			pitch = (float)PlayerDataTable[*g_playerIndex].cockpitCameraPitch / 65536.0f * 360.0f;
		}

		Matrix4 rotMatrixYaw, rotMatrixPitch, rotMatrixRoll;
		rotMatrixFull.identity();
		rotMatrixYaw.identity();   rotMatrixYaw.rotateY(yaw);
		rotMatrixPitch.identity(); rotMatrixPitch.rotateX(pitch);
		rotMatrixFull = rotMatrixPitch * rotMatrixYaw;
	}

	if (invert)
		*result = rotMatrixFull.invert();
	else
		*result = rotMatrixFull;
}

/*
 * Returns the gunner turret view matrix
 */
void GetGunnerTurretViewMatrix(Matrix4 *result) {
	// This is what the matrix looks like when looking forward:
	// F: [-0.257, 0.963, 0.080], R: [0.000, 0.083, -0.996], U: [-0.966, -0.256, -0.021]
	float factor = 32768.0f;
	/*
	short *Turret = (short *)(0x8B94E0 + 0x21E);
	Vector3 F(Turret[0] / factor, Turret[1] / factor, Turret[2] / factor);
	Vector3 R(Turret[3] / factor, Turret[4] / factor, Turret[5] / factor);
	Vector3 U(Turret[6] / factor, Turret[7] / factor, Turret[8] / factor);
	*/
	Vector3 F(
		PlayerDataTable[*g_playerIndex].gunnerTurretF[0] / factor,
		PlayerDataTable[*g_playerIndex].gunnerTurretF[1] / factor,
		PlayerDataTable[*g_playerIndex].gunnerTurretF[2] / factor
	);
	Vector3 R(
		PlayerDataTable[*g_playerIndex].gunnerTurretR[0] / factor,
		PlayerDataTable[*g_playerIndex].gunnerTurretR[1] / factor,
		PlayerDataTable[*g_playerIndex].gunnerTurretR[2] / factor
	);
	Vector3 U(
		PlayerDataTable[*g_playerIndex].gunnerTurretU[0] / factor,
		PlayerDataTable[*g_playerIndex].gunnerTurretU[1] / factor,
		PlayerDataTable[*g_playerIndex].gunnerTurretU[2] / factor
	);

	// Pointing straight at the sun or "straight up":
	// [4344] [DBG] F: [-0.015, -0.003, 1.000], R: [0.000, 1.000, 0.002], U: [-1.000, 0.000, -0.015]
	// [12316] [DBG] F: [-0.004, 0.146, 0.989], R: [0.000, 0.989, -0.146], U: [-1.000, -0.000, -0.004]
	// Pointing up again. ship looking at 90 to the right:
	// [12316] [DBG] F: [0.020, 0.001, 0.999], R: [0.984, -0.177, -0.020], U: [0.177, 0.984, -0.004]
	// Pointing up again, ship looking at 45 in the horizon:
	// [12316] [DBG] F: [0.004, 0.061, 0.998], R: [0.691, 0.721, -0.047], U: [-0.723, 0.690, -0.039]
	// Pointing up again; ship looking backwards:
	// [12316] [DBG] F: [0.040, -0.013, 0.999], R: [-0.030, -0.999, -0.011], U: [0.998, -0.030, -0.040]
	// Pointing up again, ship looking -90 left:
	// [12316] [DBG] F: [-0.060, 0.003, 0.998], R: [-0.993, 0.097, -0.060], U: [-0.097, -0.995, -0.002]

	// Pointing forward:
	// [12316] [DBG] F: [0.001, 0.996, 0.076], R: [0.090, 0.076, -0.992], U: [-0.995, 0.008, -0.090]
	// Pointing right:
	// [12316][DBG] F: [0.992, -0.101, 0.071], R : [0.069, -0.028, -0.997], U : [0.102, 0.994, -0.021]
	// Pointing backwards:
	// [12316] [DBG] F: [-0.019, -0.992, -0.116], R: [-0.179, -0.110, 0.977], U: [-0.983, 0.039, -0.176]
	// Pointing right because the ship rolled 90 deg right:
	// [12316] [DBG] F: [0.974, 0.022, 0.221], R: [-0.002, 0.995, -0.089], U: [-0.222, 0.087, 0.970]
	// Pointing left because the ship rolled 90 deg left:
	// [12316] [DBG] F: [-0.998, 0.000, 0.043], R: [-0.004, 0.995, -0.090], U: [-0.043, -0.090, -0.995]

	// Ship facing down from starting position, so that turret looks straight ahead:
	// [16224][DBG] F: [0.000, 0.997, 0.069], R : [0.000, 0.069, -0.997], U : [-1.000, 0.000, 0.000]

	Vector4 Rs, Us, Fs;
	Matrix4 Heading = GetCurrentHeadingMatrix(Rs, Us, Fs);
	//log_debug("[DBG] [GUN] (1) R: [%0.3f, %0.3f, %0.3f], U: [%0.3f, %0.3f, %0.3f], F: [%0.3f, %0.3f, %0.3f]",
	//	R.x, R.y, R.z, U.x, U.y, U.z, F.x, F.y, F.z);

	// Transform the turret's orientation into the canonical x-y-z axes:
	R = Heading * R;
	U = Heading * U;
	F = Heading * F;
	// At this point the original [R, U, F] should always map to [0,0,1], [-1,0,0], [0,1,0] and this should only
	// change if the turret moves
	//log_debug("[DBG] [GUN] (2) R: [%0.3f, %0.3f, %0.3f], U: [%0.3f, %0.3f, %0.3f], F: [%0.3f, %0.3f, %0.3f]",
	//	R.x, R.y, R.z, U.x, U.y, U.z, F.x, F.y, F.z);

	//float tpitch = atan2(F.y, F.x); // .. range?
	//float tyaw   = acos(F.z); // 0..PI
	//log_debug("[DBG] [GUN] tpitch: %0.3f, tyaw: %0.3f", 	tpitch / DEG2RAD, tyaw / DEG2RAD);

	Matrix4 viewMat = Matrix4(
		-R.x, -U.x, F.x, 0,
		-R.y, -U.y, F.y, 0,
		-R.z, -U.z, F.z, 0,
		0, 0, 0, 1
	);
	Matrix4 rotX;
	rotX.rotateX(180.0f);
	*result = rotX * viewMat.invert();
}

/*
 * Returns the gunner turret view matrix for the speed effect shaders
 */
void PrimarySurface::GetGunnerTurretViewMatrixSpeedEffect(Matrix4 *result) {
	// This is what the matrix looks like when looking forward:
	// F: [-0.257, 0.963, 0.080], R: [0.000, 0.083, -0.996], U: [-0.966, -0.256, -0.021]
	float factor = 32768.0f;
	/*
	short *Turret = (short *)(0x8B94E0 + 0x21E);
	Vector3 F(Turret[0] / factor, Turret[1] / factor, Turret[2] / factor);
	Vector3 R(Turret[3] / factor, Turret[4] / factor, Turret[5] / factor);
	Vector3 U(Turret[6] / factor, Turret[7] / factor, Turret[8] / factor);
	*/
	Vector3 F(
		PlayerDataTable[*g_playerIndex].gunnerTurretF[0] / factor, 
		PlayerDataTable[*g_playerIndex].gunnerTurretF[1] / factor, 
		PlayerDataTable[*g_playerIndex].gunnerTurretF[2] / factor
	);
	Vector3 R(
		PlayerDataTable[*g_playerIndex].gunnerTurretR[0] / factor,
		PlayerDataTable[*g_playerIndex].gunnerTurretR[1] / factor,
		PlayerDataTable[*g_playerIndex].gunnerTurretR[2] / factor
	);
	Vector3 U(
		PlayerDataTable[*g_playerIndex].gunnerTurretU[0] / factor,
		PlayerDataTable[*g_playerIndex].gunnerTurretU[1] / factor,
		PlayerDataTable[*g_playerIndex].gunnerTurretU[2] / factor
	);

	Vector4 Rs, Us, Fs;
	Matrix4 Heading = GetCurrentHeadingMatrix(Rs, Us, Fs);
	//log_debug("[DBG] [GUN] (1) R: [%0.3f, %0.3f, %0.3f], U: [%0.3f, %0.3f, %0.3f], F: [%0.3f, %0.3f, %0.3f]",
	//	R.x, R.y, R.z, U.x, U.y, U.z, F.x, F.y, F.z);

	// Transform the turret's orientation into the canonical x-y-z axes:
	R = Heading * R;
	U = Heading * U;
	F = Heading * F;
	// At this point the original [R, U, F] should always map to [0,0,1], [-1,0,0], [0,1,0] and this should only
	// change if the turret moves
	//log_debug("[DBG] [GUN] (2) R: [%0.3f, %0.3f, %0.3f], U: [%0.3f, %0.3f, %0.3f], F: [%0.3f, %0.3f, %0.3f]",
	//	R.x, R.y, R.z, U.x, U.y, U.z, F.x, F.y, F.z);

	Matrix4 viewMat = Matrix4(
		R.x, U.x, -F.x, 0,
		R.y, U.y, -F.y, 0,
		R.z, U.z, -F.z, 0,
		0, 0, 0, 1
	);
	Matrix4 rotX;
	rotX.rotateX(180.0f);
	*result = rotX * viewMat;
}

/*
 * Returns either the cockpit view matrix or the gunner turret view matrix.
 */
void GetCraftViewMatrix(Matrix4 *result) {
	const float DEG2RAD = 0.01745f;
	if (PlayerDataTable[*g_playerIndex].gunnerTurretActive)
		GetGunnerTurretViewMatrix(result);
	else
		GetCockpitViewMatrix(result);
}

/*
 * Computes Heading Difference (this is similar to cockpit inertia; but we don't care about smoothing the effect)
 * Input: The current Heading matrix H, the current forward vector Fs
 * Output: The X,Y,Z displacement
 */
/*
void ComputeHeadingDifference(const Matrix4 &H, Vector4 Fs, Vector4 Us, float fCurSpeed, int playerIndex, float *XDisp, float *YDisp, float *ZDisp) {
	const float g_fCockpitInertia = 0.35f, g_fCockpitMaxInertia = 0.2f;
	static bool bFirstFrame = true;
	static float fLastSpeed = 0.0f;
	static time_t prevT = 0;
	time_t curT = time(NULL);

	// Reset the first frame if the time between successive queries is too big: this
	// implies the game was either paused or a new mission was loaded
	bFirstFrame |= curT - prevT > 2; // Reset if +2s have elapsed
	// Skip the very first frame: there's no inertia to compute yet
	if (bFirstFrame || g_bHyperspaceTunnelLastFrame || g_bHyperspaceLastFrame)
	{
		bFirstFrame = false;
		*XDisp = *YDisp = *ZDisp = 0.0f;
		//log_debug("Resetting X/Y/ZDisp");
		// Update the previous heading vectors
		g_prevFs = Fs;
		g_prevUs = Us;
		prevT = curT;
		fLastSpeed = fCurSpeed;
		return;
	}

	Matrix4 HT = H;
	HT.transpose();
	// Multiplying the current Rs, Us, Fs with H will yield the major axes:
	//Vector4 X = HT * Rs; // --> always returns [1, 0, 0]
	//Vector4 Y = HT * Us; // --> always returns [0, 1, 0]
	//Vector4 Z = HT * Fs; // --> always returns [0, 0, 1]
	//log_debug("[DBG] X: [%0.3f, %0.3f, %0.3f], Y: [%0.3f, %0.3f, %0.3f], Z: [%0.3f, %0.3f, %0.3f]",
	//	X.x, X.y, X.z, Y.x, Y.y, Y.z, Z.x, Z.y, Z.z);

	//Vector4 X = HT * g_prevRs; // --> returns something close to [1, 0, 0]
	Vector4 Y = HT * g_prevUs; // --> returns something close to [0, 1, 0]
	Vector4 Z = HT * g_prevFs; // --> returns something close to [0, 0, 1]
	//log_debug("[DBG] X: [%0.3f, %0.3f, %0.3f], Y: [%0.3f, %0.3f, %0.3f], Z: [%0.3f, %0.3f, %0.3f]",
	//	X.x, X.y, X.z, Y.x, Y.y, Y.z, Z.x, Z.y, Z.z);
	Vector4 curFs(0, 0, 1, 0);
	Vector4 curUs(0, 1, 0, 0);
	Vector4 diffZ = curFs - Z;
	Vector4 diffY = curUs - Y;

	*XDisp = g_fCockpitInertia * diffZ.x;
	*YDisp = g_fCockpitInertia * diffZ.y;
	*ZDisp = g_fCockpitInertia * diffY.x;
	if (*XDisp < -g_fCockpitMaxInertia) *XDisp = -g_fCockpitMaxInertia; else if (*XDisp > g_fCockpitMaxInertia) *XDisp = g_fCockpitMaxInertia;
	if (*YDisp < -g_fCockpitMaxInertia) *YDisp = -g_fCockpitMaxInertia; else if (*YDisp > g_fCockpitMaxInertia) *YDisp = g_fCockpitMaxInertia;
	if (*ZDisp < -g_fCockpitMaxInertia) *ZDisp = -g_fCockpitMaxInertia; else if (*ZDisp > g_fCockpitMaxInertia) *ZDisp = g_fCockpitMaxInertia;

	// Update the previous heading smoothly, otherwise the cockpit may shake a bit
	g_prevFs = 0.1f * Fs + 0.9f * g_prevFs;
	g_prevUs = 0.1f * Us + 0.9f * g_prevUs;
	fLastSpeed = 0.1f * fCurSpeed + 0.9f * fLastSpeed;
	prevT = curT;

	//if (g_HyperspacePhaseFSM == HS_HYPER_ENTER_ST || g_HyperspacePhaseFSM == HS_INIT_ST)
	//if (g_bHyperspaceLastFrame || g_bHyperspaceTunnelLastFrame)
	//	log_debug("[%d] X/YDisp: %0.3f, %0.3f",  g_iHyperspaceFrame, *XDisp, *YDisp);
}
*/

/*
Input: _shaderToyAuxBuf (already resolved): Should hold the background (everything minus the cockpit)
	   _shaderToyBuf (already resolved): Contains the foreground (only the cockpit) when exiting hyperspace.
					 Unused in all other circumstances.
Output: Renders over _offscreenBufferPost and copies to _offscreenBuffer.
		Overwrites _offscreenBufferAsInputShaderResourceView
*/
void PrimarySurface::RenderHyperspaceEffect(D3D11_VIEWPORT *lastViewport,
	ID3D11PixelShader *lastPixelShader, Direct3DTexture *lastTextureSelected,
	ID3D11Buffer *lastVertexBuffer, UINT *lastVertexBufStride, UINT *lastVertexBufOffset)
{
	/*
	  Jedi Fallen Order sample hyperspace entry effect:
	  https://youtu.be/GLZoDkbTakg?t=197
	*/
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;
	float x0, y0, x1, y1;
	static float iTime = 0.0f, iTimeAtHyperExit = 0.0f;
	static float fLightRotationAngle = 0.0f;
	float timeInHyperspace = (float)PlayerDataTable[*g_playerIndex].timeInHyperspace;
	float iLinearTime = 0.0f; // We need a "linear" time that we can use to control the speed of the shake and light rotation
	float fShakeAmplitude = 0.0f;
	bool bBGTextureAvailable = (g_HyperspacePhaseFSM == HS_POST_HYPER_EXIT_ST);
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	D3D11_VIEWPORT viewport{};

	// Prevent rendering the hyperspace effect multiple times per frame:
	if (g_bHyperspaceEffectRenderedOnCurrentFrame)
		return;
	g_bHyperspaceEffectRenderedOnCurrentFrame = true;

	/*
		Hyperspace Data:
			2: Entering hyperspace
				max time: 489, 553
			4: Traveling through hyperspace (animation plays back at this point)
				max time: 1291, 1290
			3: Exiting hyperspace
				max time: 236, 231
	*/

	// Constants for the post-hyper-exit effect:
	const float T2 = 2.0f; // Time in seconds for the trails
	const float T2_ZOOM = 1.5f; // Time in seconds for the hyperzoom
	const float T_OVERLAP = 1.5f; // Overlap between the trails and the zoom

	static float fXRotationAngle = 0.0f, fYRotationAngle = 0.0f, fZRotationAngle = 0.0f;

	// Adjust the time according to the current hyperspace phase
	//switch (PlayerDataTable[*g_playerIndex].hyperspacePhase) 
	switch (g_HyperspacePhaseFSM)
	{
	case HS_HYPER_ENTER_ST:
		// Max internal time: ~500
		// Max shader time: 2.0 (t2)
		g_bKeybExitHyperspace = false;
		resources->InitPixelShader(resources->_hyperEntryPS);
		timeInHyperspace = timeInHyperspace / 650.0f; // 550.0f
		iTime = lerp(0.0f, 2.0f, timeInHyperspace);
		if (iTime > 2.0f) iTime = 2.0f;
		fShakeAmplitude = lerp(0.0f, 4.0f, timeInHyperspace);
		iLinearTime = iTime;
		g_ShadertoyBuffer.bloom_strength = g_BloomConfig.fHyperStreakStrength;
		break;
	case HS_HYPER_TUNNEL_ST:
	{
		/*
		From Jeremy:
		To increase the time in hyperspace:
		At offset 0029D6, replace 1205 with XXXX.
		Base is 0x0400C00 (?)

		XXXX is a hex value.
		The default value is 0x0512 which correspond to 1298 (5.5s).

		To set 10s, the value will be 2360 (10 * 236). XXXX will be 3809
		*/
		// Max internal time: ~1290 (about 5.5s, it's *tunnelTime / 236.0)
		// Max shader time: 4.0 (arbitrary)
		uint16_t *tunnelTime = (uint16_t *)(0x0400C00 + 0x0029D6); // 0x0400C00 is XWA's base process address.
		//*tunnelTime = 2360;
		resources->InitPixelShader(resources->_hyperTunnelPS);
		//timeInHyperspace = timeInHyperspace / 1290.0f;
		timeInHyperspace = timeInHyperspace / (float )(*tunnelTime); // It's better to read the hyperspace time from this offset
		iTime = lerp(0.0f, 4.0f, timeInHyperspace);

		// Rotate the lights while travelling through the hyper-tunnel:
		for (int i = 0; i < 2; i++) {
			g_LightVector[i].x = (float)cos((fLightRotationAngle + (i * 90.0f)) * 0.01745f);
			g_LightVector[i].y = (float)sin((fLightRotationAngle + (i * 90.0f)) * 0.01745f);
			g_LightVector[i].z = 0.0f;
		}
		fShakeAmplitude = lerp(4.0f, 7.0f, timeInHyperspace);
		iLinearTime = 2.0f + iTime;
		g_ShadertoyBuffer.bloom_strength = g_BloomConfig.fHyperTunnelStrength;

		if (g_config.StayInHyperspace) {
			if (!g_bKeybExitHyperspace) {
				if (PlayerDataTable[*g_playerIndex].timeInHyperspace > 900 && PlayerDataTable[*g_playerIndex].timeInHyperspace < 1000)
					PlayerDataTable[*g_playerIndex].timeInHyperspace -= 500;
			}
			else {
				PlayerDataTable[*g_playerIndex].timeInHyperspace = 1050;
				g_bKeybExitHyperspace = false;
			}
		}
		break;
	}
	case HS_HYPER_EXIT_ST:
		g_bKeybExitHyperspace = false;
		// Time is reversed here and in post-hyper-exit because this effect was originally
		// the hyper-entry in reverse; but then I changed it and I didn't "un-revert" the
		// time axis. Oh well, yet another TODO for me...
		// Max internal time: ~236
		// Max shader time: 1.5 (t2 minus a small fraction)
		resources->InitPixelShader(resources->_hyperExitPS);
		timeInHyperspace = timeInHyperspace / 200.0f;
		iTime = lerp(T2 + T2_ZOOM - T_OVERLAP, T2_ZOOM, timeInHyperspace);
		//iTime = lerp(0.0f, T2 - T_OVERLAP, timeInHyperspace);
		iTimeAtHyperExit = iTime;
		iLinearTime = 6.0f + ((T2 + T2_ZOOM - T_OVERLAP) - iTime);
		fShakeAmplitude = lerp(7.0f, 0.0f, timeInHyperspace);
		g_ShadertoyBuffer.bloom_strength = g_BloomConfig.fHyperStreakStrength;
		break;
	case HS_POST_HYPER_EXIT_ST:
		// Max internal time: MAX_POST_HYPER_EXIT_FRAMES
		// Max shader time: T2_ZOOM
		resources->InitPixelShader(resources->_hyperZoomPS); // hyperExitPS is activated after the HyperZoom shader
		timeInHyperspace = (float)g_iHyperExitPostFrames / MAX_POST_HYPER_EXIT_FRAMES;
		if (timeInHyperspace > 1.0f) timeInHyperspace = 1.0f;
		iTime = lerp(iTimeAtHyperExit, 0.0f, timeInHyperspace);
		iLinearTime = ((T2 + T2_ZOOM - T_OVERLAP) - iTimeAtHyperExit) + timeInHyperspace;
		//iTime = lerp(iTimeAtHyperExit, T2 + T2_ZOOM - T_OVERLAP, timeInHyperspace);
		g_ShadertoyBuffer.bloom_strength = g_BloomConfig.fHyperStreakStrength;
		break;
	}

	//#ifdef HYPER_OVERRIDE
		
	if (g_bHyperDebugMode) {
		//iTime = g_fHyperTimeOverride;

		static float TimeOverride = 0.0f;
		TimeOverride += 0.01f;
		if (TimeOverride > 2.0f) TimeOverride = 0.0f;
		iTime = TimeOverride;
	}
	//#endif

	fLightRotationAngle = 25.0f * iLinearTime * g_fHyperLightRotationSpeed;
	// TODO: Where am I setting the lights for the new shading model?
	//		 Am I setting the ssMask in the RTVs like I do during Execute()? Do I even need to do that?
	
	/*
	// TODO: The extra geometry shader shakes by itself when this block is enabled. Cockpit Inertia kills this
	//       effect, so we need to read that flag from CockpitLook.cfg... or we can just remove this whole block.

	fXRotationAngle = 25.0f * iLinearTime * g_fHyperShakeRotationSpeed;
	fYRotationAngle = 30.0f * iLinearTime * g_fHyperShakeRotationSpeed;
	fZRotationAngle = 35.0f * iLinearTime * g_fHyperShakeRotationSpeed;

	// Shake the cockpit a little bit:
	float fShakeX = cos(fXRotationAngle * 0.01745f);
	float fShakeZ = sin(fZRotationAngle * 0.01745f);
	float fShakeY = cos(fYRotationAngle * 0.01745f);;
	int iShakeX = (int)(fShakeAmplitude * fShakeX);
	int iShakeY = (int)(fShakeAmplitude * fShakeY);
	int iShakeZ = (int)(fShakeAmplitude * fShakeZ);

	if (*numberOfPlayersInGame == 1 && !g_bCockpitInertiaEnabled) 
	{
		PlayerDataTable[*g_playerIndex].cockpitXReference = iShakeX;
		PlayerDataTable[*g_playerIndex].cockpitYReference = iShakeY;
		PlayerDataTable[*g_playerIndex].cockpitZReference = iShakeZ;
	}
	*/

	// DEBUG Test the hyperzoom
	/*
	if (g_HyperspacePhaseFSM == HS_HYPER_EXIT_ST || g_HyperspacePhaseFSM == HS_POST_HYPER_EXIT_ST) {
		float fGetTime = iTime - T2_ZOOM + T_OVERLAP;
		log_debug("[DBG] FSM %d, iTime: %0.3f, getTime: %0.3f, bUseHyperZoom: %d, g_iHyperExitPostFrames: %d",
			g_HyperspacePhaseFSM, iTime, fGetTime, bBGTextureAvailable, g_iHyperExitPostFrames);
	}
	*/
	// DEBUG
	bool bDirectSBS = (g_bEnableVR && !g_bUseSteamVR);
	GetScreenLimitsInUVCoords(&x0, &y0, &x1, &y1);
	GetCraftViewMatrix(&g_ShadertoyBuffer.viewMat);
	g_ShadertoyBuffer.x0 = x0;
	g_ShadertoyBuffer.y0 = y0;
	g_ShadertoyBuffer.x1 = x1;
	g_ShadertoyBuffer.y1 = y1;
	g_ShadertoyBuffer.srand = g_fHyperspaceRand;
	g_ShadertoyBuffer.iTime = iTime;
	g_ShadertoyBuffer.VRmode = bDirectSBS;
	g_ShadertoyBuffer.iResolution[0] = g_fCurScreenWidth;
	g_ShadertoyBuffer.iResolution[1] = g_fCurScreenHeight;
	g_ShadertoyBuffer.hyperspace_phase = g_HyperspacePhaseFSM;
	/*if (g_bEnableVR) {
		if (g_HyperspacePhaseFSM == HS_HYPER_TUNNEL_ST)
			g_ShadertoyBuffer.iResolution[1] *= g_fAspectRatio;
	}*/
	resources->InitPSConstantBufferHyperspace(resources->_hyperspaceConstantBuffer.GetAddressOf(), &g_ShadertoyBuffer);

	//const int CAPTURE_FRAME = 75; // Hyper-entry
	//const int CAPTURE_FRAME = 150; // Tunnel

	// Pre-render: Apply the hyperzoom if necessary
	// input: shadertoyAuxBufSRV
	// output: Renders to renderTargetViewPost, then resolves the output to shadertoyAuxBuf
	if (bBGTextureAvailable)
	{
		context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
		viewport.Width  = g_fCurScreenWidth;
		viewport.Height = g_fCurScreenHeight;
		// VIEWPORT-LEFT
		if (g_bUseSteamVR) {
			viewport.Width  = (float)resources->_backbufferWidth;
			viewport.Height = (float)resources->_backbufferHeight;
		}
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		resources->InitViewport(&viewport);

		// We don't need to clear the current vertex and pixel constant buffers.
		// Since we've just finished rendering 3D, they should contain values that
		// can be reused. So let's just overwrite the values that we need.
		g_VSCBuffer.aspect_ratio      =  g_fAspectRatio;
		g_VSCBuffer.z_override        = -1.0f;
		g_VSCBuffer.sz_override       = -1.0f;
		g_VSCBuffer.mult_z_override   = -1.0f;
		g_VSCBuffer.apply_uv_comp     =  false;
		g_VSCBuffer.bPreventTransform =  0.0f;
		g_VSCBuffer.bFullTransform    =  0.0f;
		if (g_bUseSteamVR)
		{
			g_VSCBuffer.viewportScale[0] = 1.0f / resources->_displayWidth;
			g_VSCBuffer.viewportScale[1] = 1.0f / resources->_displayHeight;
		}
		else
		{
			// non-VR and DirectSBS cases:
			g_VSCBuffer.viewportScale[0] =  2.0f / resources->_displayWidth;
			g_VSCBuffer.viewportScale[1] = -2.0f / resources->_displayHeight;
		}

		// Since the HUD is all rendered on a flat surface, we lose the vrparams that make the 3D object
		// and text float
		g_VSCBuffer.z_override  = 65535.0f;
		g_VSCBuffer.scale_override = 1.0f;

		// Set the left projection matrix (the viewMatrix is set at the beginning of the frame)
		g_VSMatrixCB.projEye = g_FullProjMatrixLeft;

		resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);
		resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);

		UINT stride = sizeof(D3DTLVERTEX), offset = 0;
		resources->InitVertexBuffer(resources->_hyperspaceVertexBuffer.GetAddressOf(), &stride, &offset);
		resources->InitInputLayout(resources->_inputLayout);
		if (g_bUseSteamVR)
			resources->InitVertexShader(resources->_sbsVertexShader);
		else
			// The original (non-VR) code used _vertexShader:
			resources->InitVertexShader(resources->_vertexShader);

		resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Set the RTV:
		context->OMSetRenderTargets(1, resources->_renderTargetViewPost.GetAddressOf(), NULL);
		// Set the SRV:
		context->PSSetShaderResources(0, 1, resources->_shadertoyAuxSRV.GetAddressOf());
		context->Draw(6, 0);
		context->ResolveSubresource(resources->_shadertoyAuxBuf, 0, resources->_offscreenBufferPost, 0, BACKBUFFER_FORMAT);

		// Render the right image
		if (g_bUseSteamVR)
		{
			context->ClearRenderTargetView(resources->_renderTargetViewPostR, bgColor);
			// VIEWPORT-RIGHT
			viewport.TopLeftX = 0.0f;
			viewport.TopLeftY = 0.0f;
			viewport.Width = (float)resources->_backbufferWidth;
			viewport.Height = (float)resources->_backbufferHeight;
			viewport.MinDepth = D3D11_MIN_DEPTH;
			viewport.MaxDepth = D3D11_MAX_DEPTH;
			resources->InitViewport(&viewport);
			// Set the right projection matrix
			g_VSMatrixCB.projEye = g_FullProjMatrixRight;
			resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);

			context->OMSetRenderTargets(1, resources->_renderTargetViewPostR.GetAddressOf(), NULL);
			// Set the SRV:
			context->PSSetShaderResources(0, 1, resources->_shadertoyAuxSRV_R.GetAddressOf());
			context->Draw(6, 0);
			context->ResolveSubresource(resources->_shadertoyAuxBufR, 0, resources->_offscreenBufferPostR, 0, BACKBUFFER_FORMAT);
		}

		// Activate the hyperExitPS
		resources->InitPixelShader(resources->_hyperExitPS);
	}
	// End of hyperzoom

	// First render: Render the hyperspace effect itself
	// input: None
	// output: renderTargetViewPost
	{
		// Set the new viewport (a full quad covering the full screen)
		viewport.Width  = g_fCurScreenWidth;
		viewport.Height = g_fCurScreenHeight;
		// VIEWPORT-LEFT
		if (g_bEnableVR) {
			if (g_bUseSteamVR)
				viewport.Width = (float)resources->_backbufferWidth;
			else
				viewport.Width = (float)resources->_backbufferWidth / 2.0f;
		}
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		resources->InitViewport(&viewport);

		// We don't need to clear the current vertex and pixel constant buffers.
		// Since we've just finished rendering 3D, they should contain values that
		// can be reused. So let's just overwrite the values that we need.
		g_VSCBuffer.aspect_ratio		=  g_fAspectRatio;
		g_VSCBuffer.z_override			= -1.0f;
		g_VSCBuffer.sz_override			= -1.0f;
		g_VSCBuffer.mult_z_override		= -1.0f;
		g_VSCBuffer.apply_uv_comp       = false;
		g_VSCBuffer.bPreventTransform	=  0.0f;
		g_VSCBuffer.bFullTransform		=  0.0f;
		if (g_bEnableVR) 
		{
			g_VSCBuffer.viewportScale[0] = 1.0f / resources->_displayWidth;
			g_VSCBuffer.viewportScale[1] = 1.0f / resources->_displayHeight;
		}
		else
		{
			g_VSCBuffer.viewportScale[0] =  2.0f / resources->_displayWidth;
			g_VSCBuffer.viewportScale[1] = -2.0f / resources->_displayHeight;
		}
		//g_VSCBuffer.viewportScale[3] = 1.0f;
		//g_VSCBuffer.viewportScale[3] = g_fGlobalScale;

		// Since the HUD is all rendered on a flat surface, we lose the vrparams that make the 3D object
		// and text float
		g_VSCBuffer.z_override  = 65535.0f;
		g_VSCBuffer.scale_override = 1.0f;

		// Set the left projection matrix (the viewMatrix is set at the beginning of the frame)
		g_VSMatrixCB.projEye = g_FullProjMatrixLeft;
		resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);
		resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);

		UINT stride = sizeof(D3DTLVERTEX), offset = 0;
		resources->InitVertexBuffer(resources->_hyperspaceVertexBuffer.GetAddressOf(), &stride, &offset);
		resources->InitInputLayout(resources->_inputLayout);
		if (g_bEnableVR)
			resources->InitVertexShader(resources->_sbsVertexShader);
		else
			// The original (non-VR) code used _vertexShader:
			resources->InitVertexShader(resources->_vertexShader);
		resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
		// Set the RTV:
		ID3D11RenderTargetView *rtvs[1] = {
			resources->_renderTargetViewPost.Get(), // Render to offscreenBufferPost instead of offscreenBuffer
		};
		context->OMSetRenderTargets(1, rtvs, NULL);
		context->Draw(6, 0);

		// Render the right image
		if (g_bEnableVR) {
			// VIEWPORT-RIGHT
			if (g_bUseSteamVR) {
				context->ClearRenderTargetView(resources->_renderTargetViewPostR, bgColor);
				viewport.Width = (float)resources->_backbufferWidth;
				viewport.TopLeftX = 0.0f;
			}
			else {
				viewport.Width = (float)resources->_backbufferWidth / 2.0f;
				viewport.TopLeftX = (float)viewport.Width;
			}
			viewport.Height = (float)resources->_backbufferHeight;
			viewport.TopLeftY = 0.0f;
			viewport.MinDepth = D3D11_MIN_DEPTH;
			viewport.MaxDepth = D3D11_MAX_DEPTH;
			resources->InitViewport(&viewport);
			// Set the right projection matrix
			g_VSMatrixCB.projEye = g_FullProjMatrixRight;
			resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);

			if (g_bUseSteamVR)
				context->OMSetRenderTargets(1, resources->_renderTargetViewPostR.GetAddressOf(), NULL);
			else
				context->OMSetRenderTargets(1, resources->_renderTargetViewPost.GetAddressOf(), NULL);
			context->Draw(6, 0);
		}
	}
	
	// DEBUG
	/*if (g_iPresentCounter == CAPTURE_FRAME)
	{
		DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferPost, GUID_ContainerFormatJpeg,
			L"C:\\Temp\\_offscreenBufferPost-0.jpg");
	}*/
	// DEBUG

	// Second render: compose the cockpit over the previous effect
	{
		// Reset the viewport for non-VR mode:
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width	  = g_fCurScreenWidth;
		viewport.Height	  = g_fCurScreenHeight;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		resources->InitViewport(&viewport);

		// Reset the vertex shader to regular 2D post-process
		// Set the Vertex Shader Constant buffers
		resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
			0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices

		// Set/Create the VertexBuffer and set the topology, etc
		UINT stride = sizeof(MainVertex), offset = 0;
		resources->InitVertexBuffer(resources->_postProcessVertBuffer.GetAddressOf(), &stride, &offset);
		resources->InitInputLayout(resources->_mainInputLayout);
		resources->InitVertexShader(resources->_mainVertexShader);

		// Reset the UV limits for this shader
		GetScreenLimitsInUVCoords(&x0, &y0, &x1, &y1);
		g_ShadertoyBuffer.x0 = x0;
		g_ShadertoyBuffer.y0 = y0;
		g_ShadertoyBuffer.x1 = x1;
		g_ShadertoyBuffer.y1 = y1;
		g_ShadertoyBuffer.iResolution[0] = g_fCurScreenWidth;
		g_ShadertoyBuffer.iResolution[1] = g_fCurScreenHeight;
		resources->InitPSConstantBufferHyperspace(resources->_hyperspaceConstantBuffer.GetAddressOf(), &g_ShadertoyBuffer);

		resources->InitPixelShader(resources->_hyperComposePS);
		// Clear all the render target views
		ID3D11RenderTargetView *rtvs_null[5] = {
			NULL, // Main RTV
			NULL, // Bloom
			NULL, // Depth
			NULL, // Norm Buf
			NULL, // SSAO Mask
		};
		context->OMSetRenderTargets(5, rtvs_null, NULL);

		// DEBUG
		/*
		if (g_iPresentCounter == CAPTURE_FRAME)
		{
			DirectX::SaveWICTextureToFile(context, resources->_shadertoyAuxBuf, GUID_ContainerFormatJpeg,
				L"C:\\Temp\\_shadertoyAuxBuf.jpg");
			DirectX::SaveWICTextureToFile(context, resources->_shadertoyBuf, GUID_ContainerFormatJpeg,
				L"C:\\Temp\\_shadertoyBuf.jpg");
			DirectX::SaveWICTextureToFile(context, resources->_offscreenBuffer, GUID_ContainerFormatJpeg,
				L"C:\\Temp\\_offscreenBuf-1.jpg");
			DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferPost, GUID_ContainerFormatJpeg,
				L"C:\\Temp\\_offscreenBufferPost-1.jpg");
		}
		*/
		// DEBUG

		// The output from the previous effect will be in offscreenBufferPost, so let's resolve it
		// to _offscreenBufferAsInput to re-use in the next step:
		context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBufferPost, 0, BACKBUFFER_FORMAT);
		if (g_bUseSteamVR)
			context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferPostR, 0, BACKBUFFER_FORMAT);

		context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
		if (!g_bReshadeEnabled) {
			ID3D11RenderTargetView *rtvs[1] = {
				resources->_renderTargetViewPost.Get(),
			};
			context->OMSetRenderTargets(1, rtvs, NULL);
		}
		else {
			ID3D11RenderTargetView *rtvs[5] = {
				resources->_renderTargetViewPost.Get(), // Render to offscreenBufferPost instead of offscreenBuffer
				resources->_renderTargetViewBloomMask.Get(),
				NULL, // Depth
				NULL, // Norm Buf
				NULL, // SSAO Mask
			};
			context->OMSetRenderTargets(5, rtvs, NULL);
		}
		// Set the SRVs:
		ID3D11ShaderResourceView *srvs[3] = {
			resources->_shadertoySRV.Get(),		// Foreground (cockpit)
			resources->_shadertoyAuxSRV.Get(),  // Background
			resources->_offscreenAsInputShaderResourceView.Get(), // Previous effect (trails or tunnel)
		};
		context->PSSetShaderResources(0, 3, srvs);
		// TODO: Handle SteamVR cases
		context->Draw(6, 0);

		// Post-process the right image
		if (g_bUseSteamVR) {
			context->ClearRenderTargetView(resources->_renderTargetViewPostR, bgColor);
			if (!g_bReshadeEnabled) {
				ID3D11RenderTargetView *rtvs[1] = {
					resources->_renderTargetViewPostR.Get(),
				};
				context->OMSetRenderTargets(1, rtvs, NULL);
			}
			else {
				ID3D11RenderTargetView *rtvs[5] = {
					resources->_renderTargetViewPostR.Get(), // Render to offscreenBufferPost instead of offscreenBuffer
					resources->_renderTargetViewBloomMaskR.Get(),
					NULL, // Depth
					NULL, // Norm Buf
					NULL, // SSAO Mask
				};
				context->OMSetRenderTargets(5, rtvs, NULL);
			}
			// Set the SRVs:
			ID3D11ShaderResourceView *srvs[3] = {
				resources->_shadertoySRV_R.Get(),		// Foreground (cockpit)
				resources->_shadertoyAuxSRV_R.Get(),  // Background
				resources->_offscreenAsInputShaderResourceViewR.Get(), // Previous effect (trails or tunnel)
			};
			context->PSSetShaderResources(0, 3, srvs);
			// TODO: Handle SteamVR cases
			context->Draw(6, 0);
		}
	}

	
	// DEBUG
	/*if (g_iPresentCounter == CAPTURE_FRAME) {
		DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferPost, GUID_ContainerFormatJpeg,
			L"C:\\Temp\\_offscreenBufferPost-2.jpg");
	}*/
	// DEBUG
	

//out:
	// Copy the result (_offscreenBufferPost) to the _offscreenBuffer so that it gets displayed
	context->CopyResource(resources->_offscreenBuffer, resources->_offscreenBufferPost);
	if (g_bUseSteamVR)
		context->CopyResource(resources->_offscreenBufferR, resources->_offscreenBufferPostR);

	// Restore the original state: VertexBuffer, Shaders, Topology, Z-Buffer state, etc...
	resources->InitViewport(lastViewport);
	// TODO: None of these functions will actually *apply* any changes if they don't internally see
	//       any difference. The fix is to use a proper InitXXX() above to update the internal state
	//	     of these functions.
	if (lastTextureSelected != NULL) {
		lastTextureSelected->_refCount++;
		context->PSSetShaderResources(0, 1, lastTextureSelected->_textureView.GetAddressOf());
		lastTextureSelected->_refCount--;
	}
	resources->InitInputLayout(resources->_inputLayout);
	if (g_bEnableVR)
		this->_deviceResources->InitVertexShader(resources->_sbsVertexShader);
	else
		this->_deviceResources->InitVertexShader(resources->_vertexShader);
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitPixelShader(lastPixelShader);
	resources->InitRasterizerState(resources->_rasterizerState);
	if (lastVertexBuffer != NULL)
		resources->InitVertexBuffer(&lastVertexBuffer, lastVertexBufStride, lastVertexBufOffset);
	resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);
	resources->InitPSConstantBuffer3D(resources->_PSConstantBuffer.GetAddressOf(), &g_PSCBuffer);
}

void PrimarySurface::RenderFXAA()
{
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;
	float x0, y0, x1, y1;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	D3D11_VIEWPORT viewport{};

	// We only need the iResolution field from the shadertoy CB in the FXAA shader
	// ... and maybe the UV limits; but that's it. I think we can safely re-use the CB
	GetScreenLimitsInUVCoords(&x0, &y0, &x1, &y1);
	g_ShadertoyBuffer.x0 = x0;
	g_ShadertoyBuffer.y0 = y0;
	g_ShadertoyBuffer.x1 = x1;
	g_ShadertoyBuffer.y1 = y1;
	g_ShadertoyBuffer.iResolution[0] = g_fCurScreenWidth;
	g_ShadertoyBuffer.iResolution[1] = g_fCurScreenHeight;
	resources->InitPSConstantBufferHyperspace(resources->_hyperspaceConstantBuffer.GetAddressOf(), &g_ShadertoyBuffer);

	// Reset the viewport for non-VR mode:
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width    = g_fCurScreenWidth;
	viewport.Height   = g_fCurScreenHeight;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	resources->InitViewport(&viewport);

	// Reset the vertex shader to regular 2D post-process
	// Set the Vertex Shader Constant buffers
	resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
		0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices

	// Set/Create the VertexBuffer and set the topology, etc
	UINT stride = sizeof(MainVertex), offset = 0;
	resources->InitVertexBuffer(resources->_postProcessVertBuffer.GetAddressOf(), &stride, &offset);
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitInputLayout(resources->_mainInputLayout);

	resources->InitVertexShader(resources->_mainVertexShader);
	resources->InitPixelShader(resources->_fxaaPS);
	// Clear all the render target views
	ID3D11RenderTargetView *rtvs_null[5] = {
		NULL, // Main RTV
		NULL, // Bloom
		NULL, // Depth
		NULL, // Norm Buf
		NULL, // SSAO Mask
	};
	context->OMSetRenderTargets(5, rtvs_null, NULL);

	// Do we need to resolve the offscreen buffer?
	context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer, 0, BACKBUFFER_FORMAT);
	if (g_bUseSteamVR)
		context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR, 0, BACKBUFFER_FORMAT);
	context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);

	ID3D11RenderTargetView *rtvs[1] = {
		resources->_renderTargetViewPost.Get(),
	};
	context->OMSetRenderTargets(1, rtvs, NULL);
	// Set the SRVs:
	ID3D11ShaderResourceView *srvs[1] = {
		resources->_offscreenAsInputShaderResourceView.Get(),
	};
	context->PSSetShaderResources(0, 1, srvs);
	// TODO: Handle SteamVR cases
	context->Draw(6, 0);

	// Post-process the right image
	if (g_bUseSteamVR) {
		context->ClearRenderTargetView(resources->_renderTargetViewPostR, bgColor);
		ID3D11RenderTargetView *rtvs[1] = {
			resources->_renderTargetViewPostR.Get(),
		};
		context->OMSetRenderTargets(1, rtvs, NULL);

		// Set the SRVs:
		ID3D11ShaderResourceView *srvs[1] = {
			resources->_offscreenAsInputShaderResourceViewR.Get(),
		};
		context->PSSetShaderResources(0, 1, srvs);
		// TODO: Handle SteamVR cases
		context->Draw(6, 0);
	}

	// Copy the result (_offscreenBufferPost) to the _offscreenBuffer so that it gets displayed
	context->CopyResource(resources->_offscreenBuffer, resources->_offscreenBufferPost);
	if (g_bUseSteamVR)
		context->CopyResource(resources->_offscreenBufferR, resources->_offscreenBufferPostR);

	// Restore previous rendertarget, etc
	resources->InitInputLayout(resources->_inputLayout); // Not sure this is really needed
}

void PrimarySurface::RenderStarDebug()
{
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;
	bool bDirectSBS = (g_bEnableVR && !g_bUseSteamVR);
	float x0, y0, x1, y1;
	D3D11_VIEWPORT viewport;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	const bool bExternalView = PlayerDataTable[*g_playerIndex].externalCamera;

	GetScreenLimitsInUVCoords(&x0, &y0, &x1, &y1);
	GetCraftViewMatrix(&g_ShadertoyBuffer.viewMat);
	g_ShadertoyBuffer.x0 = x0;
	g_ShadertoyBuffer.y0 = y0;
	g_ShadertoyBuffer.x1 = x1;
	g_ShadertoyBuffer.y1 = y1;
	//g_ShadertoyBuffer.iTime = 0;
	//g_ShadertoyBuffer.VRmode = bDirectSBS;
	g_ShadertoyBuffer.VRmode = g_bEnableVR ? (bDirectSBS ? 1 : 2) : 0; // 0 = non-VR, 1 = SBS, 2 = SteamVR
	g_ShadertoyBuffer.iResolution[0] = g_fCurScreenWidth;
	g_ShadertoyBuffer.iResolution[1] = g_fCurScreenHeight;
	g_ShadertoyBuffer.y_center = bExternalView ? 0.0f : g_fYCenter;
	g_ShadertoyBuffer.FOVscale = g_fFOVscale;

	// DEBUG: Display the light at index i:
	int i = 1;
	Vector4 xwaLight = Vector4(
		s_XwaGlobalLights[i].PositionX / 32768.0f,
		s_XwaGlobalLights[i].PositionY / 32768.0f,
		s_XwaGlobalLights[i].PositionZ / 32768.0f,
		0.0f);
	// Convert the XWA light direction into viewspace coordinates:
	Vector4 light = g_CurrentHeadingViewMatrix * xwaLight;

	Vector3 L(light.x, light.y, light.z);
	L *= 65536.0f;
	Vector3 p = projectMetric(L, g_viewMatrix, g_FullProjMatrixLeft);
	//log_debug("[DBG] p: %0.3f, %0.3f", p.x, p.y);
	g_ShadingSys_PSBuffer.MainLight.x = p.x;
	g_ShadingSys_PSBuffer.MainLight.y = g_bEnableVR ? -p.y : p.y;
	g_ShadingSys_PSBuffer.MainLight.z = light.z;
	resources->InitPSConstantShadingSystem(resources->_shadingSysBuffer.GetAddressOf(), &g_ShadingSys_PSBuffer);

	resources->InitPSConstantBufferHyperspace(resources->_hyperspaceConstantBuffer.GetAddressOf(), &g_ShadertoyBuffer);
	resources->InitPixelShader(resources->_starDebugPS);

	context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer, 0, BACKBUFFER_FORMAT);
	if (g_bUseSteamVR)
		context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR, 0, BACKBUFFER_FORMAT);

	// Render the star centroid
	{
		// Set the new viewport (a full quad covering the full screen)
		viewport.Width = g_fCurScreenWidth;
		viewport.Height = g_fCurScreenHeight;
		// VIEWPORT-LEFT
		if (g_bEnableVR) {
			if (g_bUseSteamVR)
				viewport.Width = (float)resources->_backbufferWidth;
			else
				viewport.Width = (float)resources->_backbufferWidth / 2.0f;
		}
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		resources->InitViewport(&viewport);

		// We don't need to clear the current vertex and pixel constant buffers.
		// Since we've just finished rendering 3D, they should contain values that
		// can be reused. So let's just overwrite the values that we need.
		g_VSCBuffer.aspect_ratio = g_fAspectRatio;
		g_VSCBuffer.z_override = -1.0f;
		g_VSCBuffer.sz_override = -1.0f;
		g_VSCBuffer.mult_z_override = -1.0f;
		g_VSCBuffer.apply_uv_comp = false;
		g_VSCBuffer.bPreventTransform = 0.0f;
		g_VSCBuffer.bFullTransform = 0.0f;
		if (g_bEnableVR)
		{
			g_VSCBuffer.viewportScale[0] = 1.0f / resources->_displayWidth;
			g_VSCBuffer.viewportScale[1] = 1.0f / resources->_displayHeight;
		}
		else
		{
			g_VSCBuffer.viewportScale[0] = 2.0f / resources->_displayWidth;
			g_VSCBuffer.viewportScale[1] = -2.0f / resources->_displayHeight;
		}

		// These are the same settings we used previously when rendering the HUD during a draw() call.
		// We use these settings to place the HUD at different depths
		g_VSCBuffer.z_override = g_fHUDDepth;
		// The Aiming HUD is now visible in external view using the exterior hook, let's put it at
		// infinity
		if (bExternalView)
			g_VSCBuffer.z_override = 65536.0f;
		if (g_bFloatingAimingHUD)
			g_VSCBuffer.bPreventTransform = 1.0f;

		// Set the left projection matrix (the viewMatrix is set at the beginning of the frame)
		g_VSMatrixCB.projEye = g_FullProjMatrixLeft;
		resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);
		resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);

		//resources->FillReticleVertexBuffer(g_fCurInGameWidth, g_fCurInGameHeight);
		UINT stride = sizeof(D3DTLVERTEX), offset = 0;
		resources->InitVertexBuffer(resources->_hyperspaceVertexBuffer.GetAddressOf(), &stride, &offset);
		resources->InitInputLayout(resources->_inputLayout);
		if (g_bEnableVR)
			resources->InitVertexShader(resources->_sbsVertexShader);
		else
			resources->InitVertexShader(resources->_vertexShader);

		resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
		// Set the RTV:
		ID3D11RenderTargetView *rtvs[1] = {
			resources->_renderTargetViewPost.Get(), // Render to offscreenBufferPost instead of offscreenBuffer
		};
		context->OMSetRenderTargets(1, rtvs, NULL);
		// Set the SRVs:
		ID3D11ShaderResourceView *srvs[2] = {
			resources->_offscreenAsInputShaderResourceView.Get(),
			resources->_ReticleSRV.Get(),
		};
		context->PSSetShaderResources(0, 2, srvs);
		context->Draw(6, 0);

		// Render the right image
		if (g_bEnableVR) {
			// VIEWPORT-RIGHT
			if (g_bUseSteamVR) {
				context->ClearRenderTargetView(resources->_renderTargetViewPostR, bgColor);
				viewport.Width = (float)resources->_backbufferWidth;
				viewport.TopLeftX = 0.0f;
			}
			else {
				viewport.Width = (float)resources->_backbufferWidth / 2.0f;
				viewport.TopLeftX = (float)viewport.Width;
			}
			viewport.Height = (float)resources->_backbufferHeight;
			viewport.TopLeftY = 0.0f;
			viewport.MinDepth = D3D11_MIN_DEPTH;
			viewport.MaxDepth = D3D11_MAX_DEPTH;
			resources->InitViewport(&viewport);
			// Set the right projection matrix
			g_VSMatrixCB.projEye = g_FullProjMatrixRight;
			resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);

			if (g_bUseSteamVR) {
				context->OMSetRenderTargets(1, resources->_renderTargetViewPostR.GetAddressOf(), NULL);
				// Set the SRVs:
				ID3D11ShaderResourceView *srvs[2] = {
					resources->_offscreenAsInputShaderResourceViewR.Get(),
					resources->_ReticleSRV.Get(),
				};
				context->PSSetShaderResources(0, 2, srvs);
			}
			else {
				context->OMSetRenderTargets(1, resources->_renderTargetViewPost.GetAddressOf(), NULL);
				// Set the SRVs:
				ID3D11ShaderResourceView *srvs[2] = {
					resources->_offscreenAsInputShaderResourceView.Get(),
					resources->_ReticleSRV.Get(),
				};
				context->PSSetShaderResources(0, 2, srvs);
			}
			context->Draw(6, 0);
		}

		if (!g_bEnableVR)
			goto out;

		// Second render: If VR mode is enabled, then compose the previous render with the offscreen buffer
		{
			// Reset the viewport for non-VR mode, post-proc viewport (cover the whole screen)
			viewport.TopLeftX = 0.0f;
			viewport.TopLeftY = 0.0f;
			viewport.Width = g_fCurScreenWidth;
			viewport.Height = g_fCurScreenHeight;
			viewport.MaxDepth = D3D11_MAX_DEPTH;
			viewport.MinDepth = D3D11_MIN_DEPTH;
			resources->InitViewport(&viewport);

			// Reset the vertex shader to regular 2D post-process
			// Set the Vertex Shader Constant buffers
			resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
				0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices

			// Set/Create the VertexBuffer and set the topology, etc
			UINT stride = sizeof(MainVertex), offset = 0;
			resources->InitVertexBuffer(resources->_postProcessVertBuffer.GetAddressOf(), &stride, &offset);
			resources->InitInputLayout(resources->_mainInputLayout);
			resources->InitVertexShader(resources->_mainVertexShader);

			// Reset the UV limits for this shader
			GetScreenLimitsInUVCoords(&x0, &y0, &x1, &y1);
			g_ShadertoyBuffer.x0 = x0;
			g_ShadertoyBuffer.y0 = y0;
			g_ShadertoyBuffer.x1 = x1;
			g_ShadertoyBuffer.y1 = y1;
			g_ShadertoyBuffer.iResolution[0] = g_fCurScreenWidth;
			g_ShadertoyBuffer.iResolution[1] = g_fCurScreenHeight;
			resources->InitPSConstantBufferHyperspace(resources->_hyperspaceConstantBuffer.GetAddressOf(), &g_ShadertoyBuffer);
			resources->InitPixelShader(resources->_addGeomComposePS);

			// The output from the previous effect will be in offscreenBufferPost, so let's resolve it
			// to _shadertoyBuf to use it now:
			context->ResolveSubresource(resources->_shadertoyBuf, 0, resources->_offscreenBufferPost, 0, BACKBUFFER_FORMAT);
			if (g_bUseSteamVR)
				context->ResolveSubresource(resources->_shadertoyBufR, 0, resources->_offscreenBufferPostR, 0, BACKBUFFER_FORMAT);

			context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
			ID3D11RenderTargetView *rtvs[1] = {
				resources->_renderTargetViewPost.Get(), // Render to offscreenBufferPost instead of offscreenBuffer
			};
			context->OMSetRenderTargets(1, rtvs, NULL);
			// Set the SRVs:
			ID3D11ShaderResourceView *srvs[2] = {
				resources->_offscreenAsInputShaderResourceView.Get(), // The current render
				resources->_shadertoySRV.Get(),	 // The effect rendered in the previous pass
			};
			context->PSSetShaderResources(0, 2, srvs);
			context->Draw(6, 0);

			// TODO: Post-process the right image in SteamVR
			if (g_bUseSteamVR) {
				context->ClearRenderTargetView(resources->_renderTargetViewPostR, bgColor);
				ID3D11RenderTargetView *rtvs[1] = {
					resources->_renderTargetViewPostR.Get(),
				};
				context->OMSetRenderTargets(1, rtvs, NULL);
				// Set the SRVs:
				ID3D11ShaderResourceView *srvs[2] = {
					resources->_offscreenAsInputShaderResourceViewR.Get(), // The current render
					resources->_shadertoySRV_R.Get(),  // The effect rendered in the previous pass
				};
				context->PSSetShaderResources(0, 2, srvs);
				context->Draw(6, 0);
			}
		}
	}

out:
	// Copy the result (_offscreenBufferPost) to the _offscreenBuffer so that it gets displayed
	context->CopyResource(resources->_offscreenBuffer, resources->_offscreenBufferPost);
	if (g_bUseSteamVR)
		context->CopyResource(resources->_offscreenBufferR, resources->_offscreenBufferPostR);

	// Restore previous rendertarget, etc
	resources->InitInputLayout(resources->_inputLayout); // Not sure this is really needed
}

/*
 * The large FOV needed for VR causes the reticle to become huge as well. This shader is being reused
 * to resize the reticle to a more reasonable size in VR. It may also help in reducing the distortion
 * that happens when looking near the edges of the screen in VR mode.
 * Render an aiming reticle in cockpit/external view. Made obsolete by Jeremy's exterior hook, but
 * good for debugging the FOVscale and y_center params -- and good for displaying the lights as
 * well.
 */
void PrimarySurface::RenderExternalHUD()
{
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;
	bool bDirectSBS = (g_bEnableVR && !g_bUseSteamVR);
	float x0, y0, x1, y1;
	D3D11_VIEWPORT viewport;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	const bool bExternalView = PlayerDataTable[*g_playerIndex].externalCamera;
	const bool bReticleInvisible = g_ReticleCentroid.x < 0.0f || g_ReticleCentroid.y < 0.0f;
	const bool bTriangleInvisible = g_TriangleCentroid.x < 0.0f || g_TriangleCentroid.y < 0.0f;

	// The reticle centroid is not visible and the triangle pointer isn't visible: nothing to do
	if (bReticleInvisible && bTriangleInvisible)
		return;

	// DEBUG
	//g_MetricRecCBuffer.mr_debug_value = g_fDebugFOVscale;
	//resources->InitVSConstantBufferMetricRec(resources->_metricRecVSConstantBuffer.GetAddressOf(), &g_MetricRecCBuffer);
	//resources->InitPSConstantBufferMetricRec(resources->_metricRecPSConstantBuffer.GetAddressOf(), &g_MetricRecCBuffer);
	// DEBUG

	//float sz, rhw;
	//ZToDepthRHW(g_fHUDDepth, &sz, &rhw);
	//log_debug("[DBG] sz: %0.3f, rhw: %0.3f", sz, rhw);

	// g_ReticleCentroid is in in-game coordinates. For the shader, we need to transform that into UVs:
	//float x = g_ReticleCentroid.x / g_fCurInGameWidth, y = g_ReticleCentroid.y / g_fCurInGameHeight;
	float x, y;
	InGameToScreenCoords((UINT)g_nonVRViewport.TopLeftX, (UINT)g_nonVRViewport.TopLeftY,
		(UINT)g_nonVRViewport.Width, (UINT)g_nonVRViewport.Height, g_ReticleCentroid.x, g_ReticleCentroid.y, &x, &y);
	x /= g_fCurScreenWidth;
	y /= g_fCurScreenHeight;

	//log_debug("[DBG] g_ReticleCentroid, in-game coords: %0.3f, %0.3f", g_ReticleCentroid.x, g_ReticleCentroid.y);
	//log_debug("[DBG] ReticleCentroid UV: %0.3f, %0.3f, coords: %0.3f, %0.3f", x, y, x * g_fCurScreenWidth, y * g_fCurScreenHeight);

	// SunCoords[0].xy: Reticle Centroid
	// SunCoords[0].z: Inverse reticle scale
	// SunCoords[0].w: Reticle visible (0 off, 1 on)

	// SunCoords[1].x: Triangle pointer angle
	// SunCoords[1].y: Triangle pointer displacement
	// SunCoords[1].z: Triangle pointer scale (animated)
	// SunCoords[1].w: Render triangle pointer (0 off, 1 on)

	// Send the reticle centroid to the shader:
	g_ShadertoyBuffer.SunCoords[0].x = x;
	g_ShadertoyBuffer.SunCoords[0].y = y;
	g_ShadertoyBuffer.SunCoords[0].z = 1.0f / g_fReticleScale;
	g_ShadertoyBuffer.SunCoords[0].w = (float)(!bReticleInvisible);

	/*static float ang = 0.0f;
	ang += 6.0f * 0.01745f;
	if (ang > PI * 2.0f)
		ang -= (PI * 2.0f);*/
	static float time = 0.0f;
	time += 0.1f;
	if (time > 1.0f) time = 0.0f;
	// Convert the triangle centroid to screen coords:
	//InGameToScreenCoords((UINT)g_nonVRViewport.TopLeftX, (UINT)g_nonVRViewport.TopLeftY,
	//	(UINT)g_nonVRViewport.Width, (UINT)g_nonVRViewport.Height, g_TriangleCentroid.x, g_TriangleCentroid.y, &x, &y);
	//Vector2 TriangleCentroidSC(x, y);
	g_TriangleCentroid -= 0.5f * Vector2(g_fCurInGameWidth, g_fCurInGameHeight);
	// Not sure if multiplying by preserveAspectRatioComp is necessary
	//g_TriangleCentroidSC.x *= g_ShadertoyBuffer.preserveAspectRatioComp[0];
	//g_TriangleCentroidSC.y *= g_ShadertoyBuffer.preserveAspectRatioComp[1];
	float ang = PI + atan2(g_TriangleCentroid.y, g_TriangleCentroid.x);
	g_ShadertoyBuffer.SunCoords[1].x = ang;
	g_ShadertoyBuffer.SunCoords[1].y = g_fTrianglePointerDist;
	g_ShadertoyBuffer.SunCoords[1].z = 0.1f + 0.05f * time;
	g_ShadertoyBuffer.SunCoords[1].w = (float)(!bTriangleInvisible);

	GetScreenLimitsInUVCoords(&x0, &y0, &x1, &y1);
	GetCraftViewMatrix(&g_ShadertoyBuffer.viewMat);
	g_ShadertoyBuffer.x0 = x0;
	g_ShadertoyBuffer.y0 = y0;
	g_ShadertoyBuffer.x1 = x1;
	g_ShadertoyBuffer.y1 = y1;
	//g_ShadertoyBuffer.iTime = 0;
	//g_ShadertoyBuffer.VRmode = bDirectSBS;
	g_ShadertoyBuffer.VRmode = g_bEnableVR ? (bDirectSBS ? 1 : 2) : 0; // 0 = non-VR, 1 = SBS, 2 = SteamVR
	g_ShadertoyBuffer.iResolution[0] = g_fCurScreenWidth;
	g_ShadertoyBuffer.iResolution[1] = g_fCurScreenHeight;
	g_ShadertoyBuffer.y_center = bExternalView ? 0.0f : g_fYCenter;
	g_ShadertoyBuffer.FOVscale = g_fFOVscale;
	
	/*/
	// DEBUG: Display light at index i:
	int i = 1;
	Vector4 xwaLight = Vector4(
		s_XwaGlobalLights[i].PositionX / 32768.0f,
		s_XwaGlobalLights[i].PositionY / 32768.0f,
		s_XwaGlobalLights[i].PositionZ / 32768.0f,
		0.0f);
	// Convert the XWA light direction into viewspace coordinates:
	Vector4 light = g_CurrentHeadingViewMatrix * xwaLight;
	
	Vector3 L(light.x, light.y, light.z);
	L *= 65536.0f;
	Vector3 p = projectMetric(L, g_viewMatrix, g_FullProjMatrixLeft);
	//log_debug("[DBG] p: %0.3f, %0.3f", p.x, p.y);
	g_ShadingSys_PSBuffer.MainLight.x = p.x;
	g_ShadingSys_PSBuffer.MainLight.y = g_bEnableVR ? -p.y : p.y;
	g_ShadingSys_PSBuffer.MainLight.z = light.z;
	resources->InitPSConstantShadingSystem(resources->_shadingSysBuffer.GetAddressOf(), &g_ShadingSys_PSBuffer);
	*/
	
	resources->InitPSConstantBufferHyperspace(resources->_hyperspaceConstantBuffer.GetAddressOf(), &g_ShadertoyBuffer);
	resources->InitPixelShader(resources->_externalHUDPS);

	context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer, 0, BACKBUFFER_FORMAT);
	if (g_bUseSteamVR)
		context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR, 0, BACKBUFFER_FORMAT);
	// Resolve the Reticle buffer
	if (g_bEnableVR) {
		context->ResolveSubresource(resources->_ReticleBufAsInput, 0, resources->_ReticleBufMSAA, 0, BACKBUFFER_FORMAT);
		if (g_bDumpSSAOBuffers) {
			DirectX::SaveWICTextureToFile(context, resources->_ReticleBufAsInput, GUID_ContainerFormatPng,
				L"C:\\Temp\\_reticleBufAsInput.png");
		}
	}

	// Render the external HUD
	{
		// Set the new viewport (a full quad covering the full screen)
		viewport.Width  = g_fCurScreenWidth;
		viewport.Height = g_fCurScreenHeight;
		// VIEWPORT-LEFT
		if (g_bEnableVR) {
			if (g_bUseSteamVR)
				viewport.Width = (float)resources->_backbufferWidth;
			else
				viewport.Width = (float)resources->_backbufferWidth / 2.0f;
		}
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		resources->InitViewport(&viewport);

		// We don't need to clear the current vertex and pixel constant buffers.
		// Since we've just finished rendering 3D, they should contain values that
		// can be reused. So let's just overwrite the values that we need.
		g_VSCBuffer.aspect_ratio      =  g_fAspectRatio;
		g_VSCBuffer.z_override        = -1.0f;
		g_VSCBuffer.sz_override       = -1.0f;
		g_VSCBuffer.mult_z_override   = -1.0f;
		g_VSCBuffer.apply_uv_comp     =  false;
		g_VSCBuffer.bPreventTransform =  0.0f;
		g_VSCBuffer.bFullTransform    =  0.0f;
		if (g_bEnableVR)
		{
			g_VSCBuffer.viewportScale[0] = 1.0f / resources->_displayWidth;
			g_VSCBuffer.viewportScale[1] = 1.0f / resources->_displayHeight;
		}
		else
		{
			g_VSCBuffer.viewportScale[0] =  2.0f / resources->_displayWidth;
			g_VSCBuffer.viewportScale[1] = -2.0f / resources->_displayHeight;
		}

		// These are the same settings we used previously when rendering the HUD during a draw() call.
		// We use these settings to place the HUD at different depths
		g_VSCBuffer.z_override = g_fHUDDepth;
		// The Aiming HUD is now visible in external view using the exterior hook, let's put it at
		// infinity
		if (bExternalView)
			g_VSCBuffer.z_override = 65536.0f;
		if (g_bFloatingAimingHUD)
			g_VSCBuffer.bPreventTransform = 1.0f;

		// Set the left projection matrix (the viewMatrix is set at the beginning of the frame)
		g_VSMatrixCB.projEye = g_FullProjMatrixLeft;
		resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);
		resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);

		//resources->FillReticleVertexBuffer(g_fCurInGameWidth, g_fCurInGameHeight);
		UINT stride = sizeof(D3DTLVERTEX), offset = 0;
		resources->InitVertexBuffer(resources->_hyperspaceVertexBuffer.GetAddressOf(), &stride, &offset);
		//resources->InitVertexBuffer(resources->_reticleVertexBuffer.GetAddressOf(), &stride, &offset);
		resources->InitInputLayout(resources->_inputLayout);
		if (g_bEnableVR)
			resources->InitVertexShader(resources->_sbsVertexShader);
		else
			resources->InitVertexShader(resources->_vertexShader);

		resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
		// Set the RTV:
		ID3D11RenderTargetView *rtvs[1] = {
			resources->_renderTargetViewPost.Get(), // Render to offscreenBufferPost instead of offscreenBuffer
		};
		context->OMSetRenderTargets(1, rtvs, NULL);
		// Set the SRVs:
		ID3D11ShaderResourceView *srvs[2] = {
			resources->_offscreenAsInputShaderResourceView.Get(),
			resources->_ReticleSRV.Get(),
		};
		context->PSSetShaderResources(0, 2, srvs);
		context->Draw(6, 0);

		// Render the right image
		if (g_bEnableVR) {
			// VIEWPORT-RIGHT
			if (g_bUseSteamVR) {
				context->ClearRenderTargetView(resources->_renderTargetViewPostR, bgColor);
				viewport.Width = (float)resources->_backbufferWidth;
				viewport.TopLeftX = 0.0f;
			}
			else {
				viewport.Width = (float)resources->_backbufferWidth / 2.0f;
				viewport.TopLeftX = (float)viewport.Width;
			}
			viewport.Height = (float)resources->_backbufferHeight;
			viewport.TopLeftY = 0.0f;
			viewport.MinDepth = D3D11_MIN_DEPTH;
			viewport.MaxDepth = D3D11_MAX_DEPTH;
			resources->InitViewport(&viewport);
			// Set the right projection matrix
			g_VSMatrixCB.projEye = g_FullProjMatrixRight;
			resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);

			if (g_bUseSteamVR) {
				context->OMSetRenderTargets(1, resources->_renderTargetViewPostR.GetAddressOf(), NULL);
				// Set the SRVs:
				ID3D11ShaderResourceView *srvs[2] = {
					resources->_offscreenAsInputShaderResourceViewR.Get(),
					resources->_ReticleSRV.Get(),
				};
				context->PSSetShaderResources(0, 2, srvs);
			}
			else {
				context->OMSetRenderTargets(1, resources->_renderTargetViewPost.GetAddressOf(), NULL);
				// Set the SRVs:
				ID3D11ShaderResourceView *srvs[2] = {
					resources->_offscreenAsInputShaderResourceView.Get(),
					resources->_ReticleSRV.Get(),
				};
				context->PSSetShaderResources(0, 2, srvs);
			}
			context->Draw(6, 0);
		}

		if (!g_bEnableVR)
			goto out;

		if (g_bDumpSSAOBuffers) {
			DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferPost, GUID_ContainerFormatJpeg,
				L"C:\\Temp\\_offscreenBufPostExternalHUD.jpg");
		}

		// Second render: If VR mode is enabled, then compose the previous render with the offscreen buffer
		{
			// Reset the viewport for non-VR mode, post-proc viewport (cover the whole screen)
			viewport.TopLeftX = 0.0f;
			viewport.TopLeftY = 0.0f;
			viewport.Width    = g_fCurScreenWidth;
			viewport.Height   = g_fCurScreenHeight;
			viewport.MaxDepth = D3D11_MAX_DEPTH;
			viewport.MinDepth = D3D11_MIN_DEPTH;
			resources->InitViewport(&viewport);

			// Reset the vertex shader to regular 2D post-process
			// Set the Vertex Shader Constant buffers
			resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
				0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices

			// Set/Create the VertexBuffer and set the topology, etc
			UINT stride = sizeof(MainVertex), offset = 0;
			resources->InitVertexBuffer(resources->_postProcessVertBuffer.GetAddressOf(), &stride, &offset);
			resources->InitInputLayout(resources->_mainInputLayout);
			resources->InitVertexShader(resources->_mainVertexShader);

			// Reset the UV limits for this shader
			GetScreenLimitsInUVCoords(&x0, &y0, &x1, &y1);
			g_ShadertoyBuffer.x0 = x0;
			g_ShadertoyBuffer.y0 = y0;
			g_ShadertoyBuffer.x1 = x1;
			g_ShadertoyBuffer.y1 = y1;
			g_ShadertoyBuffer.iResolution[0] = g_fCurScreenWidth;
			g_ShadertoyBuffer.iResolution[1] = g_fCurScreenHeight;
			resources->InitPSConstantBufferHyperspace(resources->_hyperspaceConstantBuffer.GetAddressOf(), &g_ShadertoyBuffer);
			resources->InitPixelShader(resources->_addGeomComposePS);

			// The output from the previous effect will be in offscreenBufferPost, so let's resolve it
			// to _shadertoyBuf to use it now:
			context->ResolveSubresource(resources->_shadertoyBuf, 0, resources->_offscreenBufferPost, 0, BACKBUFFER_FORMAT);
			if (g_bUseSteamVR)
				context->ResolveSubresource(resources->_shadertoyBufR, 0, resources->_offscreenBufferPostR, 0, BACKBUFFER_FORMAT);

			context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
			ID3D11RenderTargetView *rtvs[1] = {
				resources->_renderTargetViewPost.Get(), // Render to offscreenBufferPost instead of offscreenBuffer
			};
			context->OMSetRenderTargets(1, rtvs, NULL);
			// Set the SRVs:
			ID3D11ShaderResourceView *srvs[2] = {
				resources->_offscreenAsInputShaderResourceView.Get(), // The current render
				resources->_shadertoySRV.Get(),	 // The effect rendered in the previous pass
			};
			context->PSSetShaderResources(0, 2, srvs);
			context->Draw(6, 0);

			// TODO: Post-process the right image in SteamVR
			if (g_bUseSteamVR) {
				context->ClearRenderTargetView(resources->_renderTargetViewPostR, bgColor);
				ID3D11RenderTargetView *rtvs[1] = {
					resources->_renderTargetViewPostR.Get(),
				};
				context->OMSetRenderTargets(1, rtvs, NULL);
				// Set the SRVs:
				ID3D11ShaderResourceView *srvs[2] = {
					resources->_offscreenAsInputShaderResourceViewR.Get(), // The current render
					resources->_shadertoySRV_R.Get(),  // The effect rendered in the previous pass
				};
				context->PSSetShaderResources(0, 2, srvs);
				context->Draw(6, 0);
			}
		}
	}

out:
	// Copy the result (_offscreenBufferPost) to the _offscreenBuffer so that it gets displayed
	context->CopyResource(resources->_offscreenBuffer, resources->_offscreenBufferPost);
	if (g_bUseSteamVR)
		context->CopyResource(resources->_offscreenBufferR, resources->_offscreenBufferPostR);

	// Restore previous rendertarget, etc
	resources->InitInputLayout(resources->_inputLayout); // Not sure this is really needed
}

inline void ProjectSpeedPoint(const Matrix4 &ViewMatrix, D3DTLVERTEX *particles, int idx)
{
	const float FOVFactor = g_ShadertoyBuffer.FOVscale;
	const float y_center = g_ShadertoyBuffer.y_center;
	//const float y_center = 0.0f;
	Vector4 P;
	P.x = particles[idx].sx;
	P.y = particles[idx].sy;
	P.z = particles[idx].sz;
	P.w = 1.0f;
	// Transform the point with the view matrix
	P = ViewMatrix * P;
	// Project to 2D
	P.x /= g_fAspectRatio; // TODO: Should this be g_fCurInGameAspectRatio?
	particles[idx].sx = FOVFactor * (P.x / P.z);
	particles[idx].sy = FOVFactor * (P.y / P.z) + y_center;
	particles[idx].sz = 0.0f; // We need to do this or the point will be clipped by DX. Setting it to 2.0 will clip it
	particles[idx].rhw = P.z; // Only used in VR to back-project (Ignored in non-VR mode)
	/*}
	else {
		// In VR, we leave the point in 3D, and we change the coordinates to match SteamVR's coord sys
		// This code won't work well until I figure out how to apply FOVscale and y_center in VR mode.
		particles[idx].sx =  P.x;
		particles[idx].sy =  P.y;
		particles[idx].sz = -P.z;
	}
	*/
}

inline void PrimarySurface::AddSpeedPoint(const Matrix4 &ViewMatrix, D3DTLVERTEX *particles,
	Vector4 Q, float zdisp, int ofs, float craft_speed)
{
	D3DTLVERTEX sample;
	// For VR we need to make the particles a little bigger or they'll be almost invisible:
	const float part_size = g_bEnableVR ? g_fSpeedShaderParticleSize * 2.0f : g_fSpeedShaderParticleSize;
	int j = ofs;
	//const float FOVFactor = g_ShadertoyBuffer.FOVscale;

	sample.sz = 0.0f;
	sample.rhw = 0.0f;
	// For VR we need a higher intensity or the effect will be almost invisible
	float gray = g_bEnableVR ? 1.0f : g_fSpeedShaderMaxIntensity * min(craft_speed, 1.0f);
	// Move the cutoff point a little above speed 0: we want the particles to disappear
	// a little before the craft stops.
	gray -= 0.1f;
	if (gray < 0.0f) gray = 0.0f;
	// The color is RRGGBB, so this value gets encoded in the blue component:
	// Disable the following line so that particles are still displayed when parked.
	sample.color = (uint32_t)(gray * 255.0f);
	// DEBUG: Do not fade speed particles
	//sample.color = 0xFFFFFFFF;
	// DEBUG

	// top
	particles[j] = sample;
	particles[j].sx = Q.x;
	particles[j].sy = Q.y - part_size;
	particles[j].sz = Q.z;
	particles[j].tu = -1.0;
	particles[j].tv =  1.0;
	ProjectSpeedPoint(ViewMatrix, particles, j);
	j++;

	particles[j] = sample;
	particles[j].sx = Q.x;
	particles[j].sy = Q.y;
	particles[j].sz = Q.z - part_size;
	particles[j].tu = -1.0;
	particles[j].tv = -1.0;
	ProjectSpeedPoint(ViewMatrix, particles, j);
	j++;

	particles[j] = sample;
	particles[j].sx = Q.x;
	particles[j].sy = Q.y;
	particles[j].sz = Q.z + zdisp;
	particles[j].tu = 1.0;
	particles[j].tv = 1.0;
	ProjectSpeedPoint(ViewMatrix, particles, j);
	j++;

	// bottom
	particles[j] = sample;
	particles[j].sx = Q.x;
	particles[j].sy = Q.y + part_size;
	particles[j].sz = Q.z;
	particles[j].tu =  1.0;
	particles[j].tv = -1.0;
	ProjectSpeedPoint(ViewMatrix, particles, j);
	j++;

	particles[j] = sample;
	particles[j].sx = Q.x;
	particles[j].sy = Q.y;
	particles[j].sz = Q.z - part_size;
	particles[j].tu = -1.0;
	particles[j].tv = -1.0;
	ProjectSpeedPoint(ViewMatrix, particles, j);
	j++;

	particles[j] = sample;
	particles[j].sx = Q.x;
	particles[j].sy = Q.y;
	particles[j].sz = Q.z + zdisp;
	particles[j].tu = 1.0;
	particles[j].tv = 1.0;
	ProjectSpeedPoint(ViewMatrix, particles, j);
	j++;


	// left
	particles[j] = sample;
	particles[j].sx = Q.x - part_size;
	particles[j].sy = Q.y;
	particles[j].sz = Q.z;
	particles[j].tu = -1.0;
	particles[j].tv =  1.0;
	ProjectSpeedPoint(ViewMatrix, particles, j);
	j++;

	particles[j] = sample;
	particles[j].sx = Q.x;
	particles[j].sy = Q.y;
	particles[j].sz = Q.z - part_size;
	particles[j].tu = -1.0;
	particles[j].tv = -1.0;
	ProjectSpeedPoint(ViewMatrix, particles, j);
	j++;

	particles[j] = sample;
	particles[j].sx = Q.x;
	particles[j].sy = Q.y;
	particles[j].sz = Q.z + zdisp;
	particles[j].tu = 1.0;
	particles[j].tv = 1.0;
	ProjectSpeedPoint(ViewMatrix, particles, j);
	j++;

	// right
	particles[j] = sample;
	particles[j].sx = Q.x + part_size;
	particles[j].sy = Q.y;
	particles[j].sz = Q.z;
	particles[j].tu = 1.0;
	particles[j].tv = -1.0;
	ProjectSpeedPoint(ViewMatrix, particles, j);
	j++;

	particles[j] = sample;
	particles[j].sx = Q.x;
	particles[j].sy = Q.y;
	particles[j].sz = Q.z - part_size;
	particles[j].tu = -1.0;
	particles[j].tv = -1.0;
	ProjectSpeedPoint(ViewMatrix, particles, j);
	j++;

	particles[j] = sample;
	particles[j].sx = Q.x;
	particles[j].sy = Q.y;
	particles[j].sz = Q.z + zdisp;
	particles[j].tu = 1.0;
	particles[j].tv = 1.0;
	ProjectSpeedPoint(ViewMatrix, particles, j);
	j++;

}

void PrimarySurface::RenderSpeedEffect()
{
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;
	bool bDirectSBS = (g_bEnableVR && !g_bUseSteamVR);
	float x0, y0, x1, y1;
	D3D11_VIEWPORT viewport;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	int NumParticleVertices = 0, NumParticles = 0;
	const bool bExternalView = PlayerDataTable[*g_playerIndex].externalCamera;
	static float ZTimeDisp[MAX_SPEED_PARTICLES] = { 0 };
	float craft_speed = PlayerDataTable[*g_playerIndex].currentSpeed / g_fSpeedShaderScaleFactor;

	/*
	viewMode1 and viewMode2 both become 18 in external view and 0 in cockpit view.
	They don't change when fly-by camera is active
	log_debug("[DBG] RelatedToCamera: %d, viewMode1: %d viewMode2: %d",
		PlayerDataTable[*g_playerIndex]._RelatedToCamera_,
		PlayerDataTable[*g_playerIndex].viewMode1,
		PlayerDataTable[*g_playerIndex].viewMode2);
	*/

	Vector4 Rs, Us, Fs;
	Matrix4 ViewMatrix, HeadingMatrix = GetCurrentHeadingMatrix(Rs, Us, Fs, false);
	GetCockpitViewMatrixSpeedEffect(&ViewMatrix, false);
	// Apply the roll in VR mode:
	if (g_bEnableVR)
		ViewMatrix = g_VSMatrixCB.viewMat * ViewMatrix;

	GetScreenLimitsInUVCoords(&x0, &y0, &x1, &y1);
	g_ShadertoyBuffer.x0 = x0;
	g_ShadertoyBuffer.y0 = y0;
	g_ShadertoyBuffer.x1 = x1;
	g_ShadertoyBuffer.y1 = y1;
	g_ShadertoyBuffer.y_center = bExternalView ? 0.0f : g_fYCenter;
	g_ShadertoyBuffer.VRmode = bDirectSBS;
	g_ShadertoyBuffer.iResolution[0] = g_fCurScreenWidth;
	g_ShadertoyBuffer.iResolution[1] = g_fCurScreenHeight;
	//g_ShadertoyBuffer.craft_speed = craft_speed;
	// The A-Wing's max speed seems to be 270
	//log_debug("[DBG] speed: %d", PlayerDataTable[*g_playerIndex].currentSpeed);
	// g_ShadertoyBuffer.FOVscale must be set! We'll need it for this shader

	resources->InitPixelShader(resources->_speedEffectPS);
	resources->InitPSConstantBufferHyperspace(resources->_hyperspaceConstantBuffer.GetAddressOf(), &g_ShadertoyBuffer);

	context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer, 0, BACKBUFFER_FORMAT);
	if (g_bUseSteamVR)
		context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR, 0, BACKBUFFER_FORMAT);

	// Update the position of the particles, project them to 2D and add them to the vertex buffer
	{
		// If the range is big, we also need to increase the speed of the particles. The default
		// range size is 10.0f, the following factor will accelerate the particles if the range
		// is bigger than that.
		const float SPEED_CONST = g_fSpeedShaderParticleRange / 10.0f;
		Vector4 QH, QT, RH, RT;
		for (int i = 0; i < MAX_SPEED_PARTICLES; i++) {
			float zdisp = 0.0f;
			// Transform the particles from worldspace to craftspace (using the craft's heading):
			QH = HeadingMatrix * g_SpeedParticles[i];
			// Update the position in craftspace. In this coord system,
			// Forward is always Z+, so we just have to translate points
			// along the Z axis.
			QH.z -= ZTimeDisp[i];
			// ZTimeDisp has to be updated like this to avoid the particles from moving
			// backwards when we brake
			ZTimeDisp[i] += 0.0166f * SPEED_CONST * craft_speed;
			// This is the Head-to-tail displacement along the z-axis:
			zdisp = craft_speed * g_fSpeedShaderTrailSize;

			// Transform the current particle into viewspace
			// Q is the head of the particle, P is the tail:
			QT = QH; QT.z += zdisp;
			RH = ViewMatrix * QH; // Head
			RT = ViewMatrix * QT; // Tail
			// If the particle is behind the camera, or outside the clipping space, then
			// compute a new position for it. We need to test both the head and the tail
			// of the particle, or we'll get ugly artifacts when looking back.
			if (
				// Clip the head
				RH.z < 1.0f || RH.z > g_fSpeedShaderParticleRange ||
				RH.x < -g_fSpeedShaderParticleRange || RH.x > g_fSpeedShaderParticleRange ||
				RH.y < -g_fSpeedShaderParticleRange || RH.y > g_fSpeedShaderParticleRange ||
				// Clip the tail
				RT.z < 1.0f || RT.z > g_fSpeedShaderParticleRange ||
				RT.x < -g_fSpeedShaderParticleRange || RT.x > g_fSpeedShaderParticleRange ||
				RT.y < -g_fSpeedShaderParticleRange || RT.y > g_fSpeedShaderParticleRange
				)
			{
				// Compute a new random position for this particle
				float x = (((float)rand() / RAND_MAX) - 0.5f);
				float y = (((float)rand() / RAND_MAX) - 0.5f);
				float z = (((float)rand() / RAND_MAX) - 0.5f);
				// We multiply x and y by 0.5 so that most of the particles appear in the direction
				// of travel... or that's the idea anyway
				g_SpeedParticles[i].x = x * g_fSpeedShaderParticleRange * 0.5f;
				g_SpeedParticles[i].y = y * g_fSpeedShaderParticleRange * 0.5f;
				g_SpeedParticles[i].z = z * g_fSpeedShaderParticleRange;
				ZTimeDisp[i] = 0.0f;
			}
			else
			{
				// Transform the current point with the camera matrix, project it and
				// add it to the vertex buffer
				if (NumParticles < g_iSpeedShaderMaxParticles)
				{
					AddSpeedPoint(ViewMatrix, g_SpeedParticles2D, QH, zdisp, NumParticleVertices, craft_speed);
					NumParticleVertices += 12;
					NumParticles++;
				}
			}
		}

		D3D11_MAPPED_SUBRESOURCE map;
		HRESULT hr = context->Map(resources->_speedParticlesVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		if (SUCCEEDED(hr))
		{
			size_t length = sizeof(D3DTLVERTEX) * NumParticleVertices;
			memcpy(map.pData, g_SpeedParticles2D, length);
			context->Unmap(resources->_speedParticlesVertexBuffer, 0);
		}

		UINT stride = sizeof(D3DTLVERTEX), offset = 0;
		resources->InitVertexBuffer(resources->_speedParticlesVertexBuffer.GetAddressOf(), &stride, &offset);
		resources->InitInputLayout(resources->_inputLayout);
		resources->InitVertexShader(resources->_speedEffectVS);
		resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// First render: Render the speed effect
	// input: None
	// output: renderTargetViewPost
	{
		if (g_bEnableVR) 
		{
			// This should be the same viewport used in the Execute() function
			// Set the new viewport (a full quad covering the full screen)
			// VIEWPORT-LEFT
			if (g_bUseSteamVR)
				viewport.Width = (float)resources->_backbufferWidth;
			else
				viewport.Width = (float)resources->_backbufferWidth / 2.0f;
			viewport.Height   = g_fCurScreenHeight;
			viewport.TopLeftX = 0.0f;
			viewport.TopLeftY = 0.0f;
			viewport.MinDepth = D3D11_MIN_DEPTH;
			viewport.MaxDepth = D3D11_MAX_DEPTH;
			resources->InitViewport(&viewport);
		}
		else
			resources->InitViewport(&g_nonVRViewport);

		// We don't need to clear the current vertex and pixel constant buffers.
		// Since we've just finished rendering 3D, they should contain values that
		// can be reused. So let's just overwrite the values that we need.
		g_VSCBuffer.aspect_ratio		=  g_bEnableVR ? g_fAspectRatio : g_fCurInGameAspectRatio;
		g_VSCBuffer.z_override			= -1.0f;
		g_VSCBuffer.sz_override			= -1.0f;
		g_VSCBuffer.mult_z_override		= -1.0f;
		g_VSCBuffer.apply_uv_comp       =  false;
		g_VSCBuffer.bPreventTransform	=  0.0f;
		if (g_bEnableVR)
		{
			g_VSCBuffer.bFullTransform = 1.0f; // Setting bFullTransform tells the VS to use VR projection matrices
			g_VSCBuffer.viewportScale[0] = 1.0f / resources->_displayWidth;
			g_VSCBuffer.viewportScale[1] = 1.0f / resources->_displayHeight;
		}
		else
		{
			g_VSCBuffer.bFullTransform = 0.0f; // Setting bFullTransform tells the VS to use VR projection matrices
			g_VSCBuffer.viewportScale[0] =  2.0f / resources->_displayWidth;
			g_VSCBuffer.viewportScale[1] = -2.0f / resources->_displayHeight;
		}

		// Set the left projection matrix (the viewMatrix is set at the beginning of the frame)
		g_VSMatrixCB.projEye = g_FullProjMatrixLeft;
		resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);
		resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);

		context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
		// Set the RTV:
		ID3D11RenderTargetView *rtvs[1] = {
			resources->_renderTargetViewPost.Get(), // Render to offscreenBufferPost instead of offscreenBuffer
		};
		context->OMSetRenderTargets(1, rtvs, NULL);
		context->Draw(NumParticleVertices, 0);

		if (g_bEnableVR) {
			// VIEWPORT-RIGHT
			if (g_bUseSteamVR) {
				context->ClearRenderTargetView(resources->_renderTargetViewPostR, bgColor);
				viewport.Width = (float)resources->_backbufferWidth;
				viewport.TopLeftX = 0.0f;
			}
			else {
				viewport.Width = (float)resources->_backbufferWidth / 2.0f;
				viewport.TopLeftX = (float)viewport.Width;
			}
			viewport.Height = (float)resources->_backbufferHeight;
			viewport.TopLeftY = 0.0f;
			viewport.MinDepth = D3D11_MIN_DEPTH;
			viewport.MaxDepth = D3D11_MAX_DEPTH;
			resources->InitViewport(&viewport);
			// Set the right projection matrix
			g_VSMatrixCB.projEye = g_FullProjMatrixRight;
			resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);

			if (g_bUseSteamVR)
				context->OMSetRenderTargets(1, resources->_renderTargetViewPostR.GetAddressOf(), NULL);
			else
				context->OMSetRenderTargets(1, resources->_renderTargetViewPost.GetAddressOf(), NULL);
			context->Draw(NumParticleVertices, 0);
		}
	}

	// Second render: compose the cockpit over the previous effect
	{
		// Reset the viewport for non-VR mode, post-proc viewport (cover the whole screen)
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width    = g_fCurScreenWidth;
		viewport.Height   = g_fCurScreenHeight;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		resources->InitViewport(&viewport);

		// Reset the vertex shader to regular 2D post-process
		// Set the Vertex Shader Constant buffers
		resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
			0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices

		// Set/Create the VertexBuffer and set the topology, etc
		UINT stride = sizeof(MainVertex), offset = 0;
		resources->InitVertexBuffer(resources->_postProcessVertBuffer.GetAddressOf(), &stride, &offset);
		resources->InitInputLayout(resources->_mainInputLayout);
		resources->InitVertexShader(resources->_mainVertexShader);

		// Reset the UV limits for this shader
		GetScreenLimitsInUVCoords(&x0, &y0, &x1, &y1);
		g_ShadertoyBuffer.x0 = x0;
		g_ShadertoyBuffer.y0 = y0;
		g_ShadertoyBuffer.x1 = x1;
		g_ShadertoyBuffer.y1 = y1;
		g_ShadertoyBuffer.iResolution[0] = g_fCurScreenWidth;
		g_ShadertoyBuffer.iResolution[1] = g_fCurScreenHeight;
		resources->InitPSConstantBufferHyperspace(resources->_hyperspaceConstantBuffer.GetAddressOf(), &g_ShadertoyBuffer);

		resources->InitPixelShader(resources->_speedEffectComposePS);
		// Clear all the render target views
		ID3D11RenderTargetView *rtvs_null[5] = {
			NULL, // Main RTV
			NULL, // Bloom
			NULL, // Depth
			NULL, // Norm Buf
			NULL, // SSAO Mask
		};
		context->OMSetRenderTargets(5, rtvs_null, NULL);

		// DEBUG
		/*
		if (g_iPresentCounter == CAPTURE_FRAME)
		{
			DirectX::SaveWICTextureToFile(context, resources->_shadertoyAuxBuf, GUID_ContainerFormatJpeg,
				L"C:\\Temp\\_shadertoyAuxBuf.jpg");
			DirectX::SaveWICTextureToFile(context, resources->_shadertoyBuf, GUID_ContainerFormatJpeg,
				L"C:\\Temp\\_shadertoyBuf.jpg");
			DirectX::SaveWICTextureToFile(context, resources->_offscreenBuffer, GUID_ContainerFormatJpeg,
				L"C:\\Temp\\_offscreenBuf-1.jpg");
			DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferPost, GUID_ContainerFormatJpeg,
				L"C:\\Temp\\_offscreenBufferPost-1.jpg");
		}
		*/
		// DEBUG

		// The output from the previous effect will be in offscreenBufferPost, so let's resolve it
		// to _shadertoyBuf to use it now:
		context->ResolveSubresource(resources->_shadertoyBuf, 0, resources->_offscreenBufferPost, 0, BACKBUFFER_FORMAT);
		if (g_bUseSteamVR)
			context->ResolveSubresource(resources->_shadertoyBufR, 0, resources->_offscreenBufferPostR, 0, BACKBUFFER_FORMAT);

		context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
		ID3D11RenderTargetView *rtvs[1] = {
			resources->_renderTargetViewPost.Get(), // Render to offscreenBufferPost instead of offscreenBuffer
		};
		context->OMSetRenderTargets(1, rtvs, NULL);
		// Set the SRVs:
		ID3D11ShaderResourceView *srvs[3] = {
			resources->_offscreenAsInputShaderResourceView.Get(), // The current render
			resources->_shadertoySRV.Get(),	 // The effect rendered in the previous pass
			resources->_depthBufSRV.Get(),   // The depth buffer
		};
		context->PSSetShaderResources(0, 3, srvs);
		// TODO: Handle SteamVR cases
		context->Draw(6, 0);

		// TODO: Post-process the right image in SteamVR
		if (g_bUseSteamVR) {
			context->ClearRenderTargetView(resources->_renderTargetViewPostR, bgColor);
			ID3D11RenderTargetView *rtvs[1] = {
				resources->_renderTargetViewPostR.Get(),
			};
			context->OMSetRenderTargets(1, rtvs, NULL);
			// Set the SRVs:
			ID3D11ShaderResourceView *srvs[3] = {
				resources->_offscreenAsInputShaderResourceViewR.Get(), // The current render
				resources->_shadertoySRV_R.Get(),  // The effect rendered in the previous pass
				resources->_depthBufSRV_R.Get(),   // The depth buffer
			};
			context->PSSetShaderResources(0, 3, srvs);
			// TODO: Handle SteamVR cases
			context->Draw(6, 0);
		}
	}

	// Copy the result (_offscreenBufferPost) to the _offscreenBuffer so that it gets displayed
	context->CopyResource(resources->_offscreenBuffer, resources->_offscreenBufferPost);
	if (g_bUseSteamVR)
		context->CopyResource(resources->_offscreenBufferR, resources->_offscreenBufferPostR);

	// Restore previous rendertarget, etc
	resources->InitInputLayout(resources->_inputLayout); // Not sure this is really needed
}

inline D3DCOLOR PrimarySurface::EncodeNormal(Vector3 N)
{
	N.normalize();
	uint32_t x = (uint32_t)(255.0f * (0.5f * N.x + 0.5f));
	uint32_t y = (uint32_t)(255.0f * (0.5f * N.y + 0.5f));
	uint32_t z = (uint32_t)(255.0f * (0.5f * N.z + 0.5f));
	D3DCOLOR col = (x << 24) | (y << 16) | (z << 8);
	return col;
}

/*
 * Adds geometry and returns the number of vertices added
 */
inline int PrimarySurface::AddGeometry(const Matrix4 &ViewMatrix, D3DTLVERTEX *particles,
	Vector4 Q, float zdisp, int ofs)
{
	D3DTLVERTEX sample;
	const float part_size = 0.025f;
	int j = ofs;

	sample.sz = 0.0f;
	sample.rhw = 0.0f;
	// AARRGGBB
	sample.color = 0xFFFFFF00;

	// y- is down
	// Fun fact: in external view, the origin is exactly on top of the aiming reticle

	// top-left
	particles[j] = sample;
	particles[j].sx = Q.x - part_size;
	particles[j].sy = Q.y + part_size;
	particles[j].sz = Q.z;
	particles[j].tu = -1.0;
	particles[j].tv =  1.0;
	particles[j].specular = EncodeNormal(Vector3(0.0f, 0.0f, -1.0f));
	ProjectSpeedPoint(ViewMatrix, particles, j);
	j++;

	// top-right
	particles[j] = sample;
	particles[j].sx = Q.x + part_size;
	particles[j].sy = Q.y + part_size;
	particles[j].sz = Q.z;
	particles[j].tu = 1.0;
	particles[j].tv = 1.0;
	particles[j].specular = EncodeNormal(Vector3(0.0f, 0.0f, -1.0f));
	ProjectSpeedPoint(ViewMatrix, particles, j);
	j++;

	// center
	particles[j] = sample;
	particles[j].sx = Q.x;
	particles[j].sy = Q.y;
	particles[j].sz = Q.z;
	particles[j].tu = 0.0;
	particles[j].tv = 0.0;
	particles[j].specular = EncodeNormal(Vector3(0.0f, 0.0f, -1.0f));
	ProjectSpeedPoint(ViewMatrix, particles, j);
	j++;

	return j - ofs;
}

/*
 * Computes a matrix that can be used to render OBJs modeled in Blender over the OPTs.
 * OBJs have to be pre-scaled by (1.64,1.64,-1.64). It also returns the current
 * HeadingMatrix and the current CockpitCamera matrix.
 *
 * Computes a matrix that transforms points in Modelspace -- as in the framework
 * used to model the OPTs -- into Viewspace 3D.
 *
 * This Viewspace 3D coord sys is *not* directly compatible with backprojected 3D 
 * -- as in the coord sys created during (SBS)VertexShader backprojection -- but
 * a further Viewspace 3D --> 2D projection that compensates for FOVscale and y_center
 * will project into XWA 2D (see AddGeometryVertexShader).
 *
 * In other words, things modeled in Blender over the OPT can be added to the current
 * cockpit using this matrix.
 *
 * The transform chain is:
 *
 * -POV --> CockpitCamera --> -CockpitTranslation
 *
 * In Matrix form:
 *
 * Result = -CockpitTranslation * CockpitCamera * (-POV)
 *
 * In other words:
 *
 * "Translate POV to origin, apply current cockpit camera, apply current cockpit translation"
 *
 * After applying this matrix, do the following to project to XWA 2D:
 *
 * P.x /= sm_aspect_ratio;
 * P.x = FOVscale * (P.x / P.z);
 * P.y = FOVscale * (P.y / P.z) + y_center;
 *
 * This function is used in RenderAdditionalGeometry() and RenderShadowMapOBJ().
 */
Matrix4 PrimarySurface::ComputeAddGeomViewMatrix(Matrix4 *HeadingMatrix, Matrix4 *CockpitMatrix)
{
	Vector4 Rs, Us, Fs, T;
	Matrix4 ViewMatrix, Translation;
	*HeadingMatrix = GetCurrentHeadingMatrix(Rs, Us, Fs, false);
	GetCockpitViewMatrixSpeedEffect(CockpitMatrix, false);
	ViewMatrix = *CockpitMatrix;

	// Apply the roll in VR mode:
	if (g_bEnableVR)
		ViewMatrix = g_VSMatrixCB.viewMat * ViewMatrix;

	// Translate the model so that the origin is at the POV and then scale it
	Matrix4 POVTrans;
	g_ShadowMapVSCBuffer.POV.x = -(*g_POV_X) / g_ShadowMapping.POV_XY_FACTOR;
	g_ShadowMapVSCBuffer.POV.y = -(*g_POV_Z) / g_ShadowMapping.POV_XY_FACTOR;
	g_ShadowMapVSCBuffer.POV.z =  (*g_POV_Y) / g_ShadowMapping.POV_Z_FACTOR; // For some reason, depth is inverted w.r.t POV_Y0
	POVTrans.translate(g_ShadowMapVSCBuffer.POV.x, g_ShadowMapVSCBuffer.POV.y, g_ShadowMapVSCBuffer.POV.z);
	ViewMatrix = ViewMatrix * POVTrans;

	if (g_bDumpSSAOBuffers) {
		log_debug("[DBG] [SHW] X/W HP: %d, %d, %d",
			CraftDefinitionTable[0].LaserEndingHardpoint[0],
			CraftDefinitionTable[0].LaserEndingHardpoint[1],
			CraftDefinitionTable[0].LaserEndingHardpoint[2]);
		log_debug("[DBG] [SHW] Y/W HP: %d, %d, %d",
			CraftDefinitionTable[1].LaserEndingHardpoint[0],
			CraftDefinitionTable[1].LaserEndingHardpoint[1],
			CraftDefinitionTable[1].LaserEndingHardpoint[2]);

		//log_debug("[DBG] object->y: %d", (*objects)[PlayerDataTable[*g_playerIndex].objectIndex].y);
		//log_debug("[DBG] objects: 0x%x, g_objectPtr: 0x%x", objects, *g_objectPtr);
		//log_debug("[DBG] objectIndex: %d, g_FlightSurfaceHeight: %d, cameraY: %d",
			//PlayerDataTable[*g_playerIndex].objectIndex,
			//objects[PlayerDataTable[*g_playerIndex].objectIndex].y, 
			//g_FlightSurfaceHeight, // Not relevant
			//PlayerDataTable[*g_playerIndex].cameraY); // This looks like the absolute pos of the camera

		//log_debug("[DBG] [SHW] POV0: %0.3f, %0.3f, %0.3f", (float)*g_POV_X0, (float)*g_POV_Z0, (float)*g_POV_Y0);
		log_debug("[DBG] [SHW] POV (Original): %0.3f, %0.3f, %0.3f", *g_POV_X, *g_POV_Z, *g_POV_Y);
		log_debug("[DBG] [SHW] Using POV: %0.3f, %0.3f, %0.3f",
			g_ShadowMapVSCBuffer.POV.x, g_ShadowMapVSCBuffer.POV.y, g_ShadowMapVSCBuffer.POV.z);
	}

	// Add the CockpitRef translation
	T.x = PlayerDataTable[*g_playerIndex].cockpitXReference * g_fCockpitTranslationScale;
	T.y = PlayerDataTable[*g_playerIndex].cockpitYReference * g_fCockpitTranslationScale;
	T.z = PlayerDataTable[*g_playerIndex].cockpitZReference * g_fCockpitTranslationScale;

	T.w = 0.0f;
	T = *HeadingMatrix * T; // The heading matrix is needed to convert the translation into the correct frame
	Translation.translate(-T.x, -T.y, -T.z);
	ViewMatrix = ViewMatrix * Translation;
	return ViewMatrix;
}

void PrimarySurface::RenderAdditionalGeometry()
{
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;
	float x0, y0, x1, y1;
	D3D11_VIEWPORT viewport;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	int NumParticleVertices = 0, NumParticles = 0;
	const bool bExternalView = PlayerDataTable[*g_playerIndex].externalCamera;
	Matrix4 HeadingMatrix, CockpitMatrix;

	Matrix4 ViewMatrix = ComputeAddGeomViewMatrix(&HeadingMatrix, &CockpitMatrix);
	
	GetScreenLimitsInUVCoords(&x0, &y0, &x1, &y1);
	g_ShadertoyBuffer.x0 = x0;
	g_ShadertoyBuffer.y0 = y0;
	g_ShadertoyBuffer.x1 = x1;
	g_ShadertoyBuffer.y1 = y1;
	g_ShadertoyBuffer.y_center = bExternalView ? 0.0f : g_fYCenter;
	g_ShadertoyBuffer.iResolution[0] = g_fCurScreenWidth;
	g_ShadertoyBuffer.iResolution[1] = g_fCurScreenHeight;

	resources->InitPixelShader(resources->_addGeomPS);
	resources->InitPSConstantBufferHyperspace(resources->_hyperspaceConstantBuffer.GetAddressOf(), &g_ShadertoyBuffer);

	context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer, 0, BACKBUFFER_FORMAT);
	if (g_bUseSteamVR)
		context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR, 0, BACKBUFFER_FORMAT);

	// Update the position of the particles, project them to 2D and add them to the vertex buffer
	/*
	{
		Vector4 QH, QT, RH, RT;
		for (int i = 0; i < 1; i++) {
			// Transform the particles from worldspace to craftspace (using the craft's heading):
			//QH = HeadingMatrix * g_SpeedParticles[i];
			QH.x = 0.0f;
			QH.y = 0.0f;
			QH.z = 1.0f;

			// Transform the current particle into viewspace
			RH = ViewMatrix * QH;

			// Clip the geometry
			if (RH.z > 0.1f && RH.z < g_fSpeedShaderParticleRange &&
				RH.x > -g_fSpeedShaderParticleRange && RH.x < g_fSpeedShaderParticleRange &&
				RH.y > -g_fSpeedShaderParticleRange && RH.y < g_fSpeedShaderParticleRange
			   )
			{
				// Transform the current point with the camera matrix, project it and
				// add it to the vertex buffer
				if (NumParticleVertices < MAX_SPEED_PARTICLES)
					NumParticleVertices += AddGeometry(ViewMatrix, g_SpeedParticles2D, QH, 0.0f, NumParticleVertices);
			}
		}

		D3D11_MAPPED_SUBRESOURCE map;
		HRESULT hr = context->Map(resources->_speedParticlesVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		if (SUCCEEDED(hr))
		{
			size_t length = sizeof(D3DTLVERTEX) * NumParticleVertices;
			memcpy(map.pData, g_SpeedParticles2D, length);
			context->Unmap(resources->_speedParticlesVertexBuffer, 0);
		}

		UINT stride = sizeof(D3DTLVERTEX), offset = 0;
		resources->InitVertexBuffer(resources->_speedParticlesVertexBuffer.GetAddressOf(), &stride, &offset);
		resources->InitInputLayout(resources->_inputLayout);
		resources->InitVertexShader(resources->_addGeomVS);
		resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}
	*/

	// Render the Shadow Map OBJ as additional geometry
	UINT stride = sizeof(D3DTLVERTEX), offset = 0;
	resources->InitVertexBuffer(resources->_shadowVertexBuffer.GetAddressOf(), &stride, &offset);
	resources->InitIndexBuffer(resources->_shadowIndexBuffer.Get(), false);
	resources->InitInputLayout(resources->_inputLayout);
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitVertexShader(resources->_addGeomVS);

	// Set the ViewMatrix
	g_ShadowMapVSCBuffer.Camera = ViewMatrix;
	g_ShadowMapVSCBuffer.sm_aspect_ratio = g_fCurInGameAspectRatio;
	resources->InitVSConstantBufferShadowMap(resources->_shadowMappingVSConstantBuffer.GetAddressOf(), &g_ShadowMapVSCBuffer);
	resources->InitVSConstantBufferHyperspace(resources->_hyperspaceConstantBuffer.GetAddressOf(), &g_ShadertoyBuffer);

	// First render: Render the additional geometry
	// input: None
	// output: renderTargetViewPost
	{
		if (g_bEnableVR)
		{
			// This should be the same viewport used in the Execute() function
			// Set the new viewport (a full quad covering the full screen)
			// VIEWPORT-LEFT
			if (g_bUseSteamVR)
				viewport.Width = (float)resources->_backbufferWidth;
			else
				viewport.Width = (float)resources->_backbufferWidth / 2.0f;
			viewport.Height = g_fCurScreenHeight;
			viewport.TopLeftX = 0.0f;
			viewport.TopLeftY = 0.0f;
			viewport.MinDepth = D3D11_MIN_DEPTH;
			viewport.MaxDepth = D3D11_MAX_DEPTH;
			resources->InitViewport(&viewport);
		}
		else
			resources->InitViewport(&g_nonVRViewport);

		// We don't need to clear the current vertex and pixel constant buffers.
		// Since we've just finished rendering 3D, they should contain values that
		// can be reused. So let's just overwrite the values that we need.
		g_VSCBuffer.aspect_ratio		=  g_fAspectRatio;
		g_VSCBuffer.z_override			= -1.0f;
		g_VSCBuffer.sz_override			= -1.0f;
		g_VSCBuffer.mult_z_override		= -1.0f;
		g_VSCBuffer.apply_uv_comp       =  false;
		g_VSCBuffer.bPreventTransform	=  0.0f;
		if (g_bEnableVR)
		{
			g_VSCBuffer.bFullTransform   = 1.0f; // Setting bFullTransform tells the VS to use VR projection matrices
			g_VSCBuffer.viewportScale[0] = 1.0f / resources->_displayWidth;
			g_VSCBuffer.viewportScale[1] = 1.0f / resources->_displayHeight;
		}
		else
		{
			g_VSCBuffer.bFullTransform   =  0.0f; // Setting bFullTransform tells the VS to use VR projection matrices
			g_VSCBuffer.viewportScale[0] =  2.0f / resources->_displayWidth;
			g_VSCBuffer.viewportScale[1] = -2.0f / resources->_displayHeight;
		}

		// Set the left projection matrix (the viewMatrix is set at the beginning of the frame)
		g_VSMatrixCB.projEye = g_FullProjMatrixLeft;
		resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);
		resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);

		context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
		// Set the RTV:
		ID3D11RenderTargetView *rtvs[1] = {
			resources->_renderTargetViewPost.Get(), // Render to offscreenBufferPost instead of offscreenBuffer
		};
		context->OMSetRenderTargets(1, rtvs, NULL);
		context->DrawIndexed(g_ShadowMapping.NumIndices, 0, 0); // Draw OBJ

		// Render the additional geometry on the right eye
		if (g_bEnableVR) {
			// VIEWPORT-RIGHT
			if (g_bUseSteamVR) {
				context->ClearRenderTargetView(resources->_renderTargetViewPostR, bgColor);
				viewport.Width = (float)resources->_backbufferWidth;
				viewport.TopLeftX = 0.0f;
			}
			else {
				viewport.Width = (float)resources->_backbufferWidth / 2.0f;
				viewport.TopLeftX = (float)viewport.Width;
			}
			viewport.Height = (float)resources->_backbufferHeight;
			viewport.TopLeftY = 0.0f;
			viewport.MinDepth = D3D11_MIN_DEPTH;
			viewport.MaxDepth = D3D11_MAX_DEPTH;
			resources->InitViewport(&viewport);
			// Set the right projection matrix
			g_VSMatrixCB.projEye = g_FullProjMatrixRight;
			resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);

			if (g_bUseSteamVR)
				context->OMSetRenderTargets(1, resources->_renderTargetViewPostR.GetAddressOf(), NULL);
			else
				context->OMSetRenderTargets(1, resources->_renderTargetViewPost.GetAddressOf(), NULL);
			context->DrawIndexed(g_ShadowMapping.NumIndices, 0, 0); // Draw OBJ
		}
	}

	/*
	if (g_bDumpSSAOBuffers)
	{
		DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferPost, GUID_ContainerFormatPng, L"C:\\Temp\\_addGeom.png");
	}
	*/

	// Second render: compose
	{
		// Reset the viewport for non-VR mode, post-proc viewport (cover the whole screen)
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width    = g_fCurScreenWidth;
		viewport.Height   = g_fCurScreenHeight;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		resources->InitViewport(&viewport);

		// Reset the vertex shader to regular 2D post-process
		// Set the Vertex Shader Constant buffers
		resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
			0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices

		// Set/Create the VertexBuffer and set the topology, etc
		UINT stride = sizeof(MainVertex), offset = 0;
		resources->InitVertexBuffer(resources->_postProcessVertBuffer.GetAddressOf(), &stride, &offset);
		resources->InitInputLayout(resources->_mainInputLayout);
		resources->InitVertexShader(resources->_mainVertexShader);

		// Reset the UV limits for this shader
		GetScreenLimitsInUVCoords(&x0, &y0, &x1, &y1);
		g_ShadertoyBuffer.x0 = x0;
		g_ShadertoyBuffer.y0 = y0;
		g_ShadertoyBuffer.x1 = x1;
		g_ShadertoyBuffer.y1 = y1;
		g_ShadertoyBuffer.iResolution[0] = g_fCurScreenWidth;
		g_ShadertoyBuffer.iResolution[1] = g_fCurScreenHeight;
		resources->InitPSConstantBufferHyperspace(resources->_hyperspaceConstantBuffer.GetAddressOf(), &g_ShadertoyBuffer);

		resources->InitPixelShader(resources->_addGeomComposePS);
		// Clear all the render target views
		ID3D11RenderTargetView *rtvs_null[5] = {
			NULL, // Main RTV
			NULL, // Bloom
			NULL, // Depth
			NULL, // Norm Buf
			NULL, // SSAO Mask
		};
		context->OMSetRenderTargets(5, rtvs_null, NULL);

		// DEBUG
		/*
		if (g_iPresentCounter == CAPTURE_FRAME)
		{
			DirectX::SaveWICTextureToFile(context, resources->_shadertoyAuxBuf, GUID_ContainerFormatJpeg,
				L"C:\\Temp\\_shadertoyAuxBuf.jpg");
			DirectX::SaveWICTextureToFile(context, resources->_shadertoyBuf, GUID_ContainerFormatJpeg,
				L"C:\\Temp\\_shadertoyBuf.jpg");
			DirectX::SaveWICTextureToFile(context, resources->_offscreenBuffer, GUID_ContainerFormatJpeg,
				L"C:\\Temp\\_offscreenBuf-1.jpg");
			DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferPost, GUID_ContainerFormatJpeg,
				L"C:\\Temp\\_offscreenBufferPost-1.jpg");
		}
		*/
		// DEBUG

		// The output from the previous effect will be in offscreenBufferPost, so let's resolve it
		// to _shadertoyBuf to use it now:
		context->ResolveSubresource(resources->_shadertoyBuf, 0, resources->_offscreenBufferPost, 0, BACKBUFFER_FORMAT);
		if (g_bUseSteamVR)
			context->ResolveSubresource(resources->_shadertoyBufR, 0, resources->_offscreenBufferPostR, 0, BACKBUFFER_FORMAT);

		context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
		ID3D11RenderTargetView *rtvs[1] = {
			resources->_renderTargetViewPost.Get(), // Render to offscreenBufferPost instead of offscreenBuffer
		};
		context->OMSetRenderTargets(1, rtvs, NULL);
		// Set the SRVs:
		ID3D11ShaderResourceView *srvs[2] = {
			resources->_offscreenAsInputShaderResourceView.Get(), // The current render
			resources->_shadertoySRV.Get(),	 // The effect rendered in the previous pass
			//resources->_depthBufSRV.Get(),   // The depth buffer
		};
		context->PSSetShaderResources(0, 2, srvs);
		// TODO: Handle SteamVR cases
		context->Draw(6, 0);

		// TODO: Post-process the right image in SteamVR
		if (g_bUseSteamVR) {
			context->ClearRenderTargetView(resources->_renderTargetViewPostR, bgColor);
			ID3D11RenderTargetView *rtvs[1] = {
				resources->_renderTargetViewPostR.Get(),
			};
			context->OMSetRenderTargets(1, rtvs, NULL);
			// Set the SRVs:
			ID3D11ShaderResourceView *srvs[2] = {
				resources->_offscreenAsInputShaderResourceViewR.Get(), // The current render
				resources->_shadertoySRV_R.Get(),  // The effect rendered in the previous pass
				//resources->_depthBufSRV_R.Get(),   // The depth buffer
			};
			context->PSSetShaderResources(0, 2, srvs);
			// TODO: Handle SteamVR cases
			context->Draw(6, 0);
		}
	}

	// Copy the result (_offscreenBufferPost) to the _offscreenBuffer so that it gets displayed
	context->CopyResource(resources->_offscreenBuffer, resources->_offscreenBufferPost);
	if (g_bUseSteamVR)
		context->CopyResource(resources->_offscreenBufferR, resources->_offscreenBufferPostR);

	// Restore previous rendertarget, etc
	resources->InitInputLayout(resources->_inputLayout); // Not sure this is really needed
}

/*
 * Computes the Parallel Projection Matrix from the point of view of the current light.
 * Since this is a parallel projection, the result is a Rotation Matrix that aligns the
 * ViewSpace coordinates so that the origin is now the current light looking at the
 * previous ViewSpace origin.
 */
Matrix4 PrimarySurface::ComputeLightViewMatrix(int idx, Matrix4 &Heading, bool invert)
{
	Matrix4 L;
	L.identity();

	Vector4 xwaLight = Vector4(
		s_XwaGlobalLights[idx].PositionX / 32768.0f,
		s_XwaGlobalLights[idx].PositionY / 32768.0f,
		s_XwaGlobalLights[idx].PositionZ / 32768.0f,
		0.0f);

	xwaLight = Heading * xwaLight;
	xwaLight.normalize();
	//log_debug("[DBG] [SHW] xwaLight: %0.3f, %0.3f, %0.3f", xwaLight.x, xwaLight.y, xwaLight.z);

	// Rotate the light vector so that it lies in the X-Z plane
	//Vector4 temp;
	Matrix4 rotX, rotY, rot;
	float pitch = atan2(xwaLight.y, xwaLight.z);
	rotX.rotateX(pitch / DEG2RAD);
	//temp = rotX * xwaLight;
	// temp lies now in the X-Z plane, so Y is always zero:
	//log_debug("[DBG] [SHW] temp: %0.3f, %0.3f, %0.3f", temp.x, temp.y, temp.z);

	// Rotate the vector around the Y axis so that x --> 0
	rotY.rotateY(-asin(xwaLight.x) / DEG2RAD);
	//temp = rotY * temp;
	rot = rotY * rotX;
	//temp = rot * xwaLight;
	// x and y should now be 0, with z = 1 all the time:
	//log_debug("[DBG] [SHW] temp: %0.3f, %0.3f, %0.3f", temp.x, temp.y, temp.z);

	// It is now easy to build a TBN matrix:
	Vector4 R(1.0f, 0.0f, 0.0f, 0.0f);
	Vector4 U(0.0f, 1.0f, 0.0f, 0.0f);
	Vector4 F(0.0f, 0.0f, -1.0f, 0.0f); // We use -1 here so that the light points towards the origin

	// Invert the rotation chain:
	rot.transpose();
	// Invert the TBN system
	R = rot * R;
	U = rot * U;
	F = rot * F;
	// Build a TBN matrix in ViewSpace
	if (!invert)
		L.set(
			R.x, U.x, F.x, 0,
			R.y, U.y, F.y, 0,
			R.z, U.z, F.z, 0,
			0, 0, 0, 1
		);
	else
		L.set(
			R.x, R.y, R.z, 0,
			U.x, U.y, U.z, 0,
			F.x, F.y, F.z, 0,
			0, 0, 0, 1
		);

	return L;
}

void IncreaseSMZFactor(float Delta) {
	g_ShadowMapVSCBuffer.sm_z_factor += Delta;
	log_debug("[DBG] [SHW] sm_z_factor: %0.3f, FOVscale: %0.3f, FOVDist: %0.3f",
		g_ShadowMapVSCBuffer.sm_z_factor, g_ShadowMapVSCBuffer.sm_FOVscale, *g_fRawFOVDist);
}

/*
 * Using the current 3D box limits loaded in g_OBJLimits, compute the 2D/Z-Depth limits
 * needed to center the Shadow Map depth buffer.
 */
Matrix4 PrimarySurface::GetShadowMapLimits(Matrix4 L, float *OBJrange, float *OBJminZ) {
	float minx = 100000.0f, maxx = -100000.0f;
	float miny = 100000.0f, maxy = -100000.0f;
	float minz = 100000.0f, maxz = -100000.0f;
	float cx, cy, sx, sy;
	Matrix4 S, T;
	Vector4 P, Q;
	//FILE *file = NULL;

	//if (g_bDumpSSAOBuffers)
	//	fopen_s(&file, "./Limits.OBJ", "wt");

	for (Vector4 X : g_OBJLimits) {
		// This transform chain should be the same we apply in ShadowMapVS.hlsl

		// OBJ-3D to camera view
		P = g_ShadowMapVSCBuffer.Camera * X;
		
		if (!g_bEnableVR) {
			// Project the point. The P.z here is OBJ-3D plus Camera transform
			P.x /= g_ShadowMapVSCBuffer.sm_aspect_ratio;
			P.x = g_ShadowMapVSCBuffer.sm_FOVscale * (P.x / P.z);
			P.y = g_ShadowMapVSCBuffer.sm_FOVscale * (P.y / P.z) + g_ShadowMapVSCBuffer.sm_y_center;

			// The point is now in DirectX 2D coord sys (-1..1). The depth of the point is in P.z
			// The OBJ-2D should match XWA 2D at this point. Let's back-project so that
			// they're in the same coord sys

			// Non-VR back-projection
			P.x *= g_VSCBuffer.viewportScale[2] * g_ShadowMapVSCBuffer.sm_aspect_ratio;
			P.y *= g_VSCBuffer.viewportScale[2] * g_ShadowMapVSCBuffer.sm_aspect_ratio;
			P.z *= g_ShadowMapVSCBuffer.sm_z_factor;

			Q.x = P.z * P.x / (float)DEFAULT_FOCAL_DIST;
			Q.y = P.z * P.y / (float)DEFAULT_FOCAL_DIST;
			Q.z = P.z;
			Q.w = 1.0f;
		}
		else {
			/*
			// VR back-projection. The factor of 2.0 below is because in non-VR viewPortScale is multiplied by 2;
			// but in VR mode, we multiply by 1, so we have to compensate for that.
			P.x *= g_VSCBuffer.viewportScale[2] * g_VSCBuffer.viewportScale[3] / 2.0f * g_ShadowMapVSCBuffer.sm_aspect_ratio;
			P.y *= g_VSCBuffer.viewportScale[2] * g_VSCBuffer.viewportScale[3] / 2.0f;
			P.z *= g_ShadowMapVSCBuffer.sm_z_factor * g_fMetricMult;

			// TODO: Verify that the use of DEFAULT_FOCAL_DIST didn't change the stereoscopy in VR
			Q.x = P.z * P.x / (float)DEFAULT_FOCAL_DIST_VR;
			Q.y = P.z * P.y / (float)DEFAULT_FOCAL_DIST_VR;
			Q.z = P.z;
			Q.w = 1.0f;
			*/

			// Remove the scale (1.64) we added when loading the OBJ since we need fully-metric
			// coords for the VR path:
			Q.x = P.x / g_MetricRecCBuffer.mr_shadow_OBJ_scale;
			Q.y = P.y / g_MetricRecCBuffer.mr_shadow_OBJ_scale;
			Q.z = P.z / g_MetricRecCBuffer.mr_shadow_OBJ_scale;
			Q.w = 1.0f;
		}

		// The point is now in XWA 3D, with the POV at the origin.
		// let's apply the light transform, but keep points in metric 3D
		P = L * Q;

		// Update the limits
		if (P.x < minx) minx = P.x; 
		if (P.y < miny) miny = P.y; 
		if (P.z < minz) minz = P.z; 
		
		if (P.x > maxx) maxx = P.x;
		if (P.y > maxy) maxy = P.y;
		if (P.z > maxz) maxz = P.z;

		//if (g_bDumpSSAOBuffers)
		//	fprintf(file, "v %0.6f %0.6f %0.6f\n", P.x, P.y, P.z);
	}
	/*
	if (g_bDumpSSAOBuffers) {
		fprintf(file, "\n");
		fprintf(file, "f 1 2 3\n");
		fprintf(file, "f 1 3 4\n");

		fprintf(file, "f 5 6 7\n");
		fprintf(file, "f 5 7 8\n");
		
		fprintf(file, "f 1 5 6\n");
		fprintf(file, "f 1 6 2\n");

		fprintf(file, "f 4 8 7\n");
		fprintf(file, "f 4 7 3\n");
		fflush(file);
		fclose(file);
	}
	*/
	
	// Compute the centroid
	cx = (minx + maxx) / 2.0f;
	cy = (miny + maxy) / 2.0f;
	//cz = (minz + maxz) / 2.0f;
	//cz = minz;
	
	// Compute the scale
	sx = 1.95f / (maxx - minx); // Map to -0.975..0.975
	sy = 1.95f / (maxy - miny); // Map to -0.975..0.975
	// Having an anisotropic scale provides a better usage of the shadow map. However
	// it also distorts the shadow map, making it harder to debug.
	// release
	float s = min(sx, sy);
	//sz = 1.8f / (maxz - minz); // Map to -0.9..0.9
	//sz = 1.0f / (maxz - minz);

	// We want to map xy to the origin; but we want to map Z to 0..0.98, so that Z = 1.0 is at infinity
	// Translate the points so that the centroid is at the origin
	T.translate(-cx, -cy, 0.0f);
	// Scale around the origin so that the xyz limits are [-0.9..0.9]
	if (g_ShadowMapping.bAnisotropicMapScale)
		S.scale(sx, sy, 1.0f); // Anisotropic scale: better use of the shadow map
	else
		S.scale(s, s, 1.0f); // Isotropic scale: better for debugging.

	*OBJminZ = minz;
	*OBJrange = maxz - minz;
	
	if (g_bDumpSSAOBuffers) {
		log_debug("[DBG] [SHW] min-x,y,z: %0.3f, %0.3f, %0.3f, max-x,y,z: %0.3f, %0.3f, %0.3f",
			minx, miny, minz, maxx, maxy, maxz);
		log_debug("[DBG] [SHW] cx,cy: %0.3f, %0.3f, sx,sy,s: %0.3f, %0.3f, %0.3f",
			cx, cy, sx, sy, s);
		log_debug("[DBG] [SHW] maxz: %0.3f, OBJminZ: %0.3f, OBJrange: %0.3f",
			maxz, *OBJminZ, *OBJrange);
		log_debug("[DBG] [SHW] sm_z_factor: %0.6f, FOVDistScale: %0.3f",
			g_ShadowMapVSCBuffer.sm_z_factor, g_ShadowMapping.FOVDistScale);
	}
	return S * T;
}

/*
 * For each Sun Centroid stored in the previous frame, check if they match any of
 * XWA's lights. If there's a match, we have found the global sun and we can stop
 * computing shadows from the other lights.
 * For each light that doesn't match, if the light isn't near the edge of the screen,
 * then we know that this light doesn't correspond to a sun, so we can stop computing
 * shadows for that light.
 */
void PrimarySurface::TagXWALights()
{
	int NumTagged = 0;
	// Get the screen limits, we'll need them to tell when a light is visible on the screen
	float x0, y0, x1, y1;
	bool bExternal = PlayerDataTable[*g_playerIndex].externalCamera;
	// Don't tag anything in external view (I don't know if y_center needs to be used)
	if (bExternal)
		return;
	GetScreenLimitsInUVCoords(&x0, &y0, &x1, &y1, true);

	// Check all the lights to see if they match any sun centroid
	for (int LightIdx = 0; LightIdx < *s_XwaGlobalLightsCount; LightIdx++)
	{
		// Skip lights that have been tagged. If we have tagged everything, then
		// we can stop tagging lights in future frames
		if (g_XWALightInfo[LightIdx].bTagged) {
			NumTagged++;
			if (NumTagged >= *s_XwaGlobalLightsCount) {
				log_debug("[DBG] [SHW] All lights have been tagged. Will set bAllLightsTagged to stop tagging lights");
				g_ShadowMapping.bAllLightsTagged = true;
				return;
			}
			continue;
		}

		Vector4 xwaLight = Vector4(
			s_XwaGlobalLights[LightIdx].PositionX / 32768.0f,
			s_XwaGlobalLights[LightIdx].PositionY / 32768.0f,
			s_XwaGlobalLights[LightIdx].PositionZ / 32768.0f,
			0.0f);
		// Convert the XWA light direction into viewspace coordinates:
		Vector4 light = g_CurrentHeadingViewMatrix * xwaLight;

		// Only test lights in front of the camera:
		if (light.z <= 0.0f)
			continue;

		// Project this light to screen coords to see if it's visible
		Vector3 L(light.x, light.y, light.z);
		L *= 65536.0f;
		Vector3 p = projectMetric(L, g_viewMatrix, g_FullProjMatrixLeft, true);
		// p is now in post-proc UV coords. If it's within (x0,y0)-(x1,y1) then this
		// light is visible on the screen
		/*if (p.x < x0 + 0.05f || p.x > x1 - 0.05f ||
			p.y < y0 + 0.05f || p.y > y1 - 0.05f)
			continue;*/
		bool Visible = x0 + 0.05f < p.x && p.x < x1 - 0.05f && 
			           y0 + 0.05f < p.y && p.y < y1 - 0.05f;
		if (!Visible)
			continue;

		// If we reach this point, it means this light is visible on the screen
		for (int CentroidIdx = 0; CentroidIdx < g_iNumSunCentroids; CentroidIdx++)
		{
			float X, Y, Z;
			Vector3 c, SunCentroid = g_SunCentroids[CentroidIdx];

			// Compensate point...
			X = SunCentroid.x;
			//Y = SunCentroid.y + bExternal ? 0.0f : g_ShadertoyBuffer.y_center;
			Y = SunCentroid.y + g_ShadertoyBuffer.y_center;
			Z = SunCentroid.z;

			c.set(X, Y, Z);
			c.normalize();
			light.normalize();
			// Here we're taking the dot product between the light's direction and the compensated
			// centroid. Note that a centroid c = [0,0,0] is directly in the middle of the screen.
			// So the vertical FOV should help us predict if the compensated centroid is visible
			// in the screen or not.
			float dot = c.x*light.x + c.y*light.y + c.z*light.z;
			/*log_debug("[DBG] [SHW] sun vector: [%0.3f, %0.3f, %0.3f], Centroid: [%0.3f, %0.3f, %0.3f]",
				c.x, c.y, c.z, X, Y, Z);
			log_debug("[DBG] [SHW] light: [%0.3f, %0.3f, %0.3f], dot: %0.3f, SUN: %d",
				light.x, light.y, light.z, dot, dot > 0.98f);*/

			if (dot > 0.98f)
			{
				// Associate an XWA light to this texture and stop checking
				log_debug("[DBG] [SHW] Sun Found: Light %d, dot: %0.3f", LightIdx, dot);
				log_debug("[DBG] [SHW] centr: [%0.3f, %0.3f, %0.3f], light: [%0.3f, %0.3f, %0.3f]",
					c.x, c.y, c.z, light.x, light.y, light.z);

				if (g_ShadowMapping.bMultipleSuns) 
				{
					// There may be missions with multiple suns, so, let's tag this one as a sun
					// but let's keep tagging.
					g_XWALightInfo[LightIdx].bIsSun = true;
					g_XWALightInfo[LightIdx].bTagged = true;
					break;
				}
				else {
					// At this point, I probably only want one sun casting shadows. So, as soon as we find our sun, we
					// can stop tagging and shut down all the other lights as shadow casters.
					for (int j = 0; j < *s_XwaGlobalLightsCount; j++)
					{
						g_XWALightInfo[j].bIsSun = (j == LightIdx); // LightIdx is the current light being tagged
						g_XWALightInfo[j].bTagged = true;
					}
					g_ShadowMapping.bAllLightsTagged = true; // We found the global sun, stop tagging lights
					return;
				}

			}
		}

		// If we reach this point, the light is visible and it has been tested against all
		// centroids. If the light was not tagged, then it's not a sun
		if (!g_XWALightInfo[LightIdx].bTagged && NumTagged < *s_XwaGlobalLightsCount - 1) // Keep at least one light as shadow caster
		{
			// If we reach this point, then the light hasn't been tagged, it's clearly visible 
			// on the screen and it's not a sun
			g_XWALightInfo[LightIdx].bTagged = true;
			g_XWALightInfo[LightIdx].bIsSun = false;
			log_debug("[DBG] [SHW] Light: %d is *NOT* a Sun", LightIdx);
			log_debug("[DBG] [SHW] light: %0.3f, %0.3f, %0.3f", light.x, light.y, light.z);
			log_debug("[DBG] [SHW] p: %0.3f, %0.3f", p.x, p.y);
			log_debug("[DBG] [SHW] limits: (%0.3f, %0.3f)-(%0.3f, %0.3f)", x0, y0, x1, y1);
			log_debug("[DBG] [SHW] g_bCustomFOVApplied: %d, Frame: %d, VR mode: %d",
				g_bCustomFOVApplied, g_iPresentCounter, g_bEnableVR);
			ShowMatrix4(g_CurrentHeadingViewMatrix, "g_CurrentHeadingViewMatrix");
		}

		/*
		// If the light is close enough to the center of the screen and hasn't been tagged
		// then we know it's not a sun.
		if (!g_XWALightInfo[LightIdx].bTagged && 
			NumTagged < *s_XwaGlobalLightsCount - 1) // Keep at least one light as shadow caster
		{
			// dot_light_center_of_screen = dot([0,0,1], light) = light.z
			// The following gives us the angle between the center of the screen and the light:
			float light_rad = acos(light.z); // dot product of the light's dir and the forward view
			//float light_ang = light_rad / DEG2RAD;
			//log_debug("[DBG] [SHW] light_ang[%d]: %0.3f, dot: %0.3f", i, light_rad / DEG2RAD, light.z);
			float MinRealFOV = min(g_fRealVertFOV, g_fRealHorzFOV);
			const float range = 0.6f;
			float RealHalfFOV = MinRealFOV / 2.0f * range;
			// We divide by 2.0 because the angle is measured with respect to the screen's center. The other
			// factor is added to make sure the light isn't too close to the edge of the screen (in this case,
			// we want the light to be at least halfway into the center before comparing). If we compare
			// the lights too close to the edges, we risk misclassifying true lights as non-Suns because
			// there's a small error margin when comparing lights with sun centroids
			// I've noticed that sometimes light_ang is exactly 0. This is just ridiculous and I think it's
			// happening because the lights or the transform hasn't been set properly yet. To prevent
			// false-tagging lights, I'm adding the "light_rad > 0.01f" test below.
			if (light_rad / DEG2RAD < RealHalfFOV)
			{
				// If we reach this point, then the light hasn't been tagged, it's clearly visible 
				// on the screen and it's not a sun:
				g_XWALightInfo[LightIdx].bTagged = true;
				g_XWALightInfo[LightIdx].bIsSun = false;
				log_debug("[DBG] [SHW] Light: %d is *NOT* a Sun", LightIdx);
				log_debug("[DBG] [SHW] MinRealFOV: %0.3f, light_ang: %0.3f, range: %0.3f",
					MinRealFOV, light_rad / DEG2RAD, range);
				log_debug("[DBG] [SHW] light: %0.3f, %0.3f, %0.3f", light.x, light.y, light.z);
				log_debug("[DBG] [SHW] g_bCustomFOVApplied: %d, Frame: %d, VR mode: %d",
					g_bCustomFOVApplied, g_iPresentCounter, g_bEnableVR);
				ShowMatrix4(g_CurrentHeadingViewMatrix, "g_CurrentHeadingViewMatrix");
			}
		}
		*/
	} // for LightIdx
}

void PrimarySurface::RenderShadowMapOBJ()
{
	auto &resources = this->_deviceResources;
	auto &device = resources->_d3dDevice;
	auto &context = resources->_d3dDeviceContext;
	Matrix4 HeadingMatrix, CockpitMatrix;
	Matrix4 T, Ry, Rx, S;

	// Tag the XWA global lights unless everything has been tagged
	// Looks like the very first frame cannot be used to tag anything: the lights are probably not in
	// the right places and the FOV hasn't been computed, so let's wait until the FOV has been computed
	// to tag anything and we've finished rendering a few frames
	if (g_bCustomFOVApplied && !g_ShadowMapping.bAllLightsTagged && g_iPresentCounter > 5)
		TagXWALights();

	// Display debug information on g_XWALightInfo (are the lights tagged, are they suns?)
	if (g_bDumpSSAOBuffers) {
		log_debug("[DBG] [SHW] RealVFOV: %0.3f, RealHFOV: %0.3f", g_fRealVertFOV / DEG2RAD, g_fRealHorzFOV / DEG2RAD);
		for (int j = 0; j < *s_XwaGlobalLightsCount; j++)
		{
			log_debug("[DBG] [SHW] light[%d], bIsSun: %d, bTagged: %d, black_level: %0.3f",
				j, g_XWALightInfo[j].bIsSun, g_XWALightInfo[j].bTagged, g_ShadowMapVSCBuffer.sm_black_levels[j]);
		}
	}

	// Fade all non-sun lights
	for (int j = 0; j < *s_XwaGlobalLightsCount; j++) 
	{
		if (g_ShadowMapVSCBuffer.sm_black_levels[j] >= 0.95f)
			continue;

		// If this light has been tagged and isn't a sun, then fade it!
		if (g_XWALightInfo[j].bTagged && !g_XWALightInfo[j].bIsSun) {
			//g_XWALightInfo[j].fadeout += 0.01f;
			g_ShadowMapVSCBuffer.sm_black_levels[j] += 0.01f;
			if (g_ShadowMapVSCBuffer.sm_black_levels[j] > 0.95f)
				log_debug("[DBG] [SHW] Light %d FADED", j);
		}
	}

	//Matrix4 T1, T2;
	//Matrix4 S;
	//T1.translate(0.0f, -g_ShadowMapVSCBuffer.sm_y_center, 0.0f);
	//T2.translate(0.0f,  g_ShadowMapVSCBuffer.sm_y_center, 0.0f);
	//S.scale(1.0f/g_ShadowMapVSCBuffer.sm_aspect_ratio, 1.0f, 1.0f);
	//S.scale(g_ShadowMapVSCBuffer.sm_aspect_ratio, 1.0f, 1.0f);

	// Enable ZWrite: we'll need it for the ShadowMap
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = TRUE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	desc.DepthFunc = D3D11_COMPARISON_LESS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	/*
	// Set the rasterizer state to enable
	D3D11_RASTERIZER_DESC rsDesc;
	static ID3D11RasterizerState *rstate = NULL;
	rsDesc.CullMode = D3D11_CULL_NONE;
	rsDesc.FillMode = D3D11_FILL_SOLID;
	rsDesc.FrontCounterClockwise = TRUE;
	rsDesc.DepthBias = g_ShadowMapping.DepthBias;
	rsDesc.DepthBiasClamp = g_ShadowMapping.DepthBiasClamp;
	rsDesc.SlopeScaledDepthBias = g_ShadowMapping.SlopeScaledDepthBias;
	rsDesc.DepthClipEnable = TRUE;
	rsDesc.ScissorEnable = FALSE;
	rsDesc.MultisampleEnable = FALSE;
	rsDesc.AntialiasedLineEnable = FALSE;
	if (rstate == NULL || g_bDumpSSAOBuffers) {
		if (rstate != NULL)
			rstate->Release();
		device->CreateRasterizerState(&rsDesc, &rstate);
	}
	resources->InitRasterizerState(rstate);
	*/

	// Init the Viewport
	resources->InitViewport(&g_ShadowMapping.ViewPort);

	// Set the Vertex and Pixel Shaders
	resources->InitVertexShader(resources->_shadowMapVS);
	resources->InitPixelShader(resources->_shadowMapPS);

	// Set the vertex and index buffers
	UINT stride = sizeof(D3DTLVERTEX), ofs = 0;
	resources->InitVertexBuffer(resources->_shadowVertexBuffer.GetAddressOf(), &stride, &ofs);
	resources->InitIndexBuffer(resources->_shadowIndexBuffer.Get(), false);

	// Set the input layout
	resources->InitInputLayout(resources->_inputLayout);
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// TODO: Is the HeadingMatrix and H the same thing? Looks like the answer is "Not really!"
	//       Even transposing HeadMatrix doesn't return the same matrix as GetCurrentHeadingViewMatrix
	// Compute the OBJ-to-ViewSpace ViewMatrix
	g_ShadowMapVSCBuffer.Camera = ComputeAddGeomViewMatrix(&HeadingMatrix, &CockpitMatrix);
	// TODO: Should I use y_center here? The lights don't seem to rotate quite well...
	// Use the heading matrix to move the lights
	//Matrix4 H = GetCurrentHeadingViewMatrix();

	g_ShadowMapVSCBuffer.sm_aspect_ratio = g_fCurInGameAspectRatio; // g_VSCBuffer.aspect_ratio;
	g_ShadowMapVSCBuffer.sm_FOVscale = g_ShadertoyBuffer.FOVscale;
	g_ShadowMapVSCBuffer.sm_y_center = g_ShadertoyBuffer.y_center;
	g_ShadowMapVSCBuffer.sm_PCSS_enabled = g_bShadowMapEnablePCSS;
	g_ShadowMapVSCBuffer.sm_z_factor = g_ShadowMapping.FOVDistScale / *g_fRawFOVDist;
	g_ShadowMapVSCBuffer.sm_resolution = (float)g_ShadowMapping.ShadowMapSize;
	g_ShadowMapVSCBuffer.sm_hardware_pcf = g_bShadowMapHardwarePCF;
	// Select either the SW or HW bias depending on which setting is enabled
	g_ShadowMapVSCBuffer.sm_bias = g_bShadowMapHardwarePCF ? g_ShadowMapping.hw_pcf_bias : g_ShadowMapping.sw_pcf_bias;
	g_ShadowMapVSCBuffer.sm_enabled = g_bShadowMapEnable;
	g_ShadowMapVSCBuffer.sm_debug = g_bShadowMapDebug;
	g_ShadowMapVSCBuffer.sm_VR_mode = g_bEnableVR;

	// Compute all the lightWorldMatrices and their OBJrange/minZ's first:
	for (int idx = 0; idx < *s_XwaGlobalLightsCount; idx++)
	{
		float range, minZ;
		// Don't bother computing shadow maps for lights with a high black
		// level
		if (g_ShadowMapVSCBuffer.sm_black_levels[idx] > 0.95f)
			continue;

		// Compute the LightView (Parallel Projection) Matrix
		Matrix4 L = ComputeLightViewMatrix(idx, g_CurrentHeadingViewMatrix, false);
		Matrix4 ST = GetShadowMapLimits(L, &range, &minZ);

		////g_ShadowMapVSCBuffer.lightWorldMatrix = T * S * Rx * Ry * CockpitMatrix.transpose();
		////g_ShadowMapVSCBuffer.lightWorldMatrix = T * S * L * CockpitMatrix.transpose();
		//g_ShadowMapVSCBuffer.lightWorldMatrix = T * S * L;
		g_ShadowMapVSCBuffer.lightWorldMatrix[idx] = ST * L;
		g_ShadowMapVSCBuffer.OBJrange[idx] = range;
		g_ShadowMapVSCBuffer.OBJminZ[idx] = minZ;

		// Render each light to its own shadow map
		g_ShadowMapVSCBuffer.light_index = idx;

		// Initialize the Constant Buffer
		// T * R does rotation first, then translation: so the object rotates around the origin
		// and then gets pushed away along the Z axis
		/*
		S.scale(g_fShadowMapScale, g_fShadowMapScale, 1.0f);
		Rx.rotateX(g_fShadowMapAngleX);
		Ry.rotateY(g_fShadowMapAngleY);
		T.translate(0, 0, g_fShadowMapDepthTrans);
		*/
		
		// Set the constant buffer
		resources->InitVSConstantBufferShadowMap(resources->_shadowMappingVSConstantBuffer.GetAddressOf(), &g_ShadowMapVSCBuffer);

		// Clear the Shadow Map DSV (I may have to update this later for the hyperspace state)
		context->ClearDepthStencilView(resources->_shadowMapDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
		// Set the Shadow Map DSV
		context->OMSetRenderTargets(0, 0, resources->_shadowMapDSV.Get());
		// Render the Shadow Map
		context->DrawIndexed(g_ShadowMapping.NumIndices, 0, 0);

		// Copy the shadow map to the right slot in the array
		context->CopySubresourceRegion(resources->_shadowMapArray, D3D11CalcSubresource(0, idx, 1), 0, 0, 0, 
			resources->_shadowMap, D3D11CalcSubresource(0, 0, 1), NULL);
	}

	// Set the Shadow Mapping Constant Buffer for the Pixel Shader as well
	resources->InitPSConstantBufferShadowMap(resources->_shadowMappingPSConstantBuffer.GetAddressOf(), &g_ShadowMapVSCBuffer);
}

/*
 * In VR mode, the centroid of the sun texture is a new vertex in metric
 * 3D space. We need to project it to the vertex buffer that is used to
 * render the flare (the hyperspace vertex buffer, so it's placed at
 * ~22km away). This projection yields a uv-coordinate in this distant
 * vertex buffer. Then we need to project this distant point to post-proc
 * screen coordinates. The screen-space coords (u,v) have (0,0) at the
 * center of the screen. The output u is further multiplied by the aspect
 * ratio to put the flare exactly on top of the sun.
 * This function projects the 3D centroid to post-proc coords.
 */
void PrimarySurface::ProjectCentroidToPostProc(Vector3 Centroid, float *u, float *v) {
	/*
	 * The following circus happens because the vertex buffer we're using to render the flare
	 * is at ~22km away. We want it there, so that it's at "infinity". So we need to trace a ray
	 * from the camera through the Centroid and then extend that until it intersects the vertex
	 * buffer at that depth. Then we compute the UV coords of the intersection and use that for
	 * the shader. This should avoid visual artifacts in SteamVR because we're using an actual 3D
	 * canvas (the vertex buffer at ~21km) that gets transformed with the regular projection matrices.
	 */
	float U0, U1, U2;
	float V0, V1, V2;
	Vector3 v0, v1, v2, P, orig = Vector3(0.0f, 0.0f, 0.0f), dir;
	float t, tu, tv;
	// The depth of the hyperspace buffer is: 
	float hb_rhw_depth = 0.000863f;
	//g_ShadertoyBuffer.iResolution[1] *= g_fAspectRatio;

	/*
	// Back-project to the depth of the vertex buffer we're using here:
	backProject(QL.x, QL.y, hb_rhw_depth, &QL);
	backProject(QR.x, QR.y, hb_rhw_depth, &QR);
	log_debug("[DBG] Centroid: %0.3f, %0.3f, %0.3f --> Q: %0.3f, %0.3f, %0.3f",
		Centroid.x, Centroid.y, Centroid.z, QL.x, QL.y, QL.z);
	*/

	// Convert the centroid into a direction coming from the origin:
	dir = Centroid;
	dir.normalize();

	// Test the first triangle that makes up the hyperspace VB for an intersection:
	backProjectMetric(0.0f, 0.0f, hb_rhw_depth, &v0);
	backProjectMetric(g_fCurInGameWidth, 0.0f, hb_rhw_depth, &v1);
	backProjectMetric(0.0f, g_fCurInGameHeight, hb_rhw_depth, &v2);
	if (rayTriangleIntersect(orig, dir, v0, v1, v2, t, P, tu, tv)) {
		U0 = 0.0f; U1 = 1.0f; U2 = 0.0f;
		V0 = 0.0f; V1 = 0.0f; V2 = 1.0f;
	}
	else 
	{
		// Test the second triangle that makes up the hyperspace VB for an intersection:
		backProjectMetric(g_fCurInGameWidth, 0.0f, hb_rhw_depth, &v0);
		backProjectMetric(g_fCurInGameWidth, g_fCurInGameHeight, hb_rhw_depth, &v1);
		backProjectMetric(0.0f, g_fCurInGameHeight, hb_rhw_depth, &v2);
		rayTriangleIntersect(orig, dir, v0, v1, v2, t, P, tu, tv);
		U0 = 1.0f; U1 = 1.0f; U2 = 0.0f;
		V0 = 0.0f; V1 = 1.0f; V2 = 1.0f;
	}
	// Interpolate the texture UV using the barycentric (tu, tv) coords:
	*u = tu * U0 + tv * U1 + (1.0f - tu - tv) * U2;
	*v = tu * V0 + tv * V1 + (1.0f - tu - tv) * V2;
	// Change the range to -1..1:
	*u = 2.0f * *u - 1.0f;
	*v = 2.0f * *v - 1.0f;
	// Apply the aspect ratio
	*u *= g_fCurScreenWidth / g_fCurScreenHeight;
}

void PrimarySurface::RenderSunFlare()
{
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;
	bool bDirectSBS = (g_bEnableVR && !g_bUseSteamVR);
	float x0, y0, x1, y1;
	D3D11_VIEWPORT viewport;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	static float iTime = 0.0f;
	Vector3 Centroid, QL[MAX_SUN_FLARES], QR[MAX_SUN_FLARES];
	Vector2 Q[MAX_SUN_FLARES];
	const bool bExternalView = PlayerDataTable[*g_playerIndex].externalCamera;

	iTime += 0.01f;
	GetScreenLimitsInUVCoords(&x0, &y0, &x1, &y1);
	g_ShadertoyBuffer.x0 = x0;
	g_ShadertoyBuffer.y0 = y0;
	g_ShadertoyBuffer.x1 = x1;
	g_ShadertoyBuffer.y1 = y1;
	g_ShadertoyBuffer.iTime = iTime;
	g_ShadertoyBuffer.y_center = bExternalView ? 0.0f : g_fYCenter;
	g_ShadertoyBuffer.VRmode = g_bEnableVR;
	g_ShadertoyBuffer.iResolution[0] = g_fCurScreenWidth;
	g_ShadertoyBuffer.iResolution[1] = g_fCurScreenHeight;
	// g_ShadertoyBuffer.FOVscale must be set! We'll need it for this shader
	if (g_bEnableVR) {
		float u, v;
		if (bDirectSBS)
			g_ShadertoyBuffer.iResolution[0] = g_fCurScreenWidth / 2.0f;

		// This is a first step towards having multiple flares; but more work is needed
		// because flares are blocked depending on stuff that lies in front of them, so
		// we can't render all flares on the same go. It has to be done one by one, eye
		// by eye. So, at this point, even though we have an array of flares, it's all
		// hard-coded to only render flare 0 in the shaders.
		// Project all flares to post-proc coords in the hyperspace vertex buffer:
		for (int i = 0; i < g_ShadertoyBuffer.SunFlareCount; i++) {
			Centroid.x = g_ShadertoyBuffer.SunCoords[i].x;
			Centroid.y = g_ShadertoyBuffer.SunCoords[i].y;
			Centroid.z = g_ShadertoyBuffer.SunCoords[i].z;
			// To keep TagXwaLights() working in non-VR space even when VR is enabled,
			// the centroid is always stored in the nonVR coord sys. For VR, we need to 
			// flip the Y coord:
			if (g_bEnableVR) Centroid.y = -Centroid.y;
			/*
			ProjectCentroidToPostProc(Centroid, &u, &v);
			*/
			
			// The Sun Centroid is in in-game coords, convert to the range [-1..1]:
			u = (g_SunCentroids2D[i].x / g_fCurInGameWidth - 0.5f) * 2.0f;
			v = (g_SunCentroids2D[i].y / g_fCurInGameHeight - 0.5f) * 2.0f;
			if (bDirectSBS) {
				v *= g_fCurScreenHeight / (g_fCurScreenWidth / 2.0f);
			}
			else {
				// SteamVR mode
				if (g_steamVRWidth < g_steamVRHeight)
					v *= (float)g_steamVRHeight / (float)g_steamVRWidth;
				else
					u *= (float)g_steamVRWidth / (float)g_steamVRHeight;
			}

			// Overwrite the centroid with the new 2D coordinates for the left/right images.
			// In VR mode, these UVs are *the same* for both eyes because they are defined
			// with respect to distant the hyperspace VB. This is *not* a bug.
			g_ShadertoyBuffer.SunCoords[i].x = u;
			g_ShadertoyBuffer.SunCoords[i].y = v;

			// Also project the centroid to 2D directly -- we'll need that during the compose pass
			// to mask the flare. In VR mode, projectToInGameCoordsMetric produces post-proc
			// coords, not in-game coords
			QL[i] = projectToInGameOrPostProcCoordsMetric(Centroid, g_viewMatrix, g_FullProjMatrixLeft);
			QR[i] = projectToInGameOrPostProcCoordsMetric(Centroid, g_viewMatrix, g_FullProjMatrixRight);
			//log_debug("[DBG] QL: %0.3f, %0.3f", QL[i].x, QL[i].y);
		}
	}
	// Set the shadertoy constant buffer:
	resources->InitPSConstantBufferHyperspace(resources->_hyperspaceConstantBuffer.GetAddressOf(), &g_ShadertoyBuffer);
	resources->InitPixelShader(resources->_sunFlareShaderPS);

	context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer, 0, BACKBUFFER_FORMAT);
	if (g_bUseSteamVR)
		context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR, 0, BACKBUFFER_FORMAT);

#ifdef GENMIPMAPS_DEBUG
	// DEBUG
	// An easy way to check if the mip maps are being generated is by calling GenerateMipMaps here and then sampling
	// a higher level in the sun flare shader.
	context->GenerateMips(resources->_offscreenAsInputShaderResourceView);
#endif

	// Render the Sun Flare
	// output: _offscreenBufferPost, _offscreenBufferPostR
	// The non-VR case produces a fully-finished image.
	// The VR case produces only the flare on these buffers. The flare must be composed on top of the image in a further step
	// The VR flare is rendered on a distant VB buffer. The uv coords in this buffer are the same regardless of the
	// view because their frame of reference is the same VB buffer
	{
		// Set the new viewport (a full quad covering the full screen)
		viewport.Width  = g_fCurScreenWidth;
		viewport.Height = g_fCurScreenHeight;
		// VIEWPORT-LEFT
		if (g_bEnableVR) {
			if (g_bUseSteamVR)
				viewport.Width = (float)resources->_backbufferWidth;
			else
				viewport.Width = (float)resources->_backbufferWidth / 2.0f;
		}
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		resources->InitViewport(&viewport);

		// We don't need to clear the current vertex and pixel constant buffers.
		// Since we've just finished rendering 3D, they should contain values that
		// can be reused. So let's just overwrite the values that we need.
		g_VSCBuffer.aspect_ratio		=  g_fAspectRatio;
		g_VSCBuffer.z_override			= -1.0f;
		g_VSCBuffer.sz_override			= -1.0f;
		g_VSCBuffer.mult_z_override		= -1.0f;
		g_VSCBuffer.apply_uv_comp       =  false;
		g_VSCBuffer.bPreventTransform	=  0.0f;
		g_VSCBuffer.bFullTransform		=  0.0f;
		if (g_bEnableVR)
		{
			g_VSCBuffer.viewportScale[0] = 1.0f / resources->_displayWidth;
			g_VSCBuffer.viewportScale[1] = 1.0f / resources->_displayHeight;
		}
		else
		{
			g_VSCBuffer.viewportScale[0] =  2.0f / resources->_displayWidth;
			g_VSCBuffer.viewportScale[1] = -2.0f / resources->_displayHeight;
		}

		// Since the HUD is all rendered on a flat surface, we lose the vrparams that make the 3D object
		// and text float
		g_VSCBuffer.z_override = 65535.0f;
		g_VSCBuffer.scale_override = 1.0f;

		// Set the left projection matrix (the viewMatrix is set at the beginning of the frame)
		g_VSMatrixCB.projEye = g_FullProjMatrixLeft;
		resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);
		resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);

		UINT stride = sizeof(D3DTLVERTEX), offset = 0;
		resources->InitVertexBuffer(resources->_hyperspaceVertexBuffer.GetAddressOf(), &stride, &offset);
		resources->InitInputLayout(resources->_inputLayout);
		if (g_bEnableVR)
			resources->InitVertexShader(resources->_sbsVertexShader);
		else
			resources->InitVertexShader(resources->_vertexShader);
		resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
		// Set the RTV:
		ID3D11RenderTargetView *rtvs[1] = {
			resources->_renderTargetViewPost.Get(), // Render to offscreenBufferPost instead of offscreenBuffer
		};
		context->OMSetRenderTargets(1, rtvs, NULL);
		// Set the SRVs:
		ID3D11ShaderResourceView *srvs[2] = {
			resources->_offscreenAsInputShaderResourceView.Get(),
			resources->_depthBufSRV.Get(),
		};
		context->PSSetShaderResources(0, 2, srvs);
		context->Draw(6, 0);

		// Render the right image
		if (g_bEnableVR)
		{
			// VIEWPORT-RIGHT
			if (g_bUseSteamVR) {
				context->ClearRenderTargetView(resources->_renderTargetViewPostR, bgColor);
				viewport.Width = (float)resources->_backbufferWidth;
				viewport.TopLeftX = 0.0f;
			}
			else {
				viewport.Width = (float)resources->_backbufferWidth / 2.0f;
				viewport.TopLeftX = (float)viewport.Width;
			}
			viewport.Height = (float)resources->_backbufferHeight;
			viewport.TopLeftY = 0.0f;
			viewport.MinDepth = D3D11_MIN_DEPTH;
			viewport.MaxDepth = D3D11_MAX_DEPTH;
			resources->InitViewport(&viewport);
			// Set the right projection matrix
			g_VSMatrixCB.projEye = g_FullProjMatrixRight;
			resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);

			if (g_bUseSteamVR) {
				context->OMSetRenderTargets(1, resources->_renderTargetViewPostR.GetAddressOf(), NULL);
				// Set the SRVs:
				ID3D11ShaderResourceView *srvs[2] = {
					resources->_offscreenAsInputShaderResourceViewR.Get(),
					resources->_depthBufSRV_R.Get(),
				};
				context->PSSetShaderResources(0, 2, srvs);
			}
			else {
				// DirectSBS case
				context->OMSetRenderTargets(1, resources->_renderTargetViewPost.GetAddressOf(), NULL);
				// Set the SRVs:
				ID3D11ShaderResourceView *srvs[2] = {
					resources->_offscreenAsInputShaderResourceView.Get(),
					resources->_depthBufSRV.Get(),
				};
				context->PSSetShaderResources(0, 2, srvs);
			}
			context->Draw(6, 0);
		}
	}
	
	if (g_bDumpSSAOBuffers) {
		DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferPost, GUID_ContainerFormatPng, L"C:\\Temp\\_sunFlare.png");
		if (g_bUseSteamVR)
			DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferPostR, GUID_ContainerFormatPng, L"C:\\Temp\\_sunFlareR.png");
	}

	// Post-process: compose the flare on top of the offscreen buffers
	if (g_bEnableVR)
	{
		// Reset the viewport for non-VR mode:
		viewport.TopLeftX	= 0.0f;
		viewport.TopLeftY	= 0.0f;
		viewport.Width		= g_fCurScreenWidth;
		viewport.Height		= g_fCurScreenHeight;
		viewport.MaxDepth	= D3D11_MAX_DEPTH;
		viewport.MinDepth	= D3D11_MIN_DEPTH;
		resources->InitViewport(&viewport);

		// Reset the vertex shader to regular 2D post-process
		// Set the Vertex Shader Constant buffers
		resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(),
			0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices

		// Set/Create the VertexBuffer and set the topology, etc
		UINT stride = sizeof(MainVertex), offset = 0;
		resources->InitVertexBuffer(resources->_postProcessVertBuffer.GetAddressOf(), &stride, &offset);
		resources->InitInputLayout(resources->_mainInputLayout);
		resources->InitVertexShader(resources->_mainVertexShader);

		// Reset the UV limits for this shader
		GetScreenLimitsInUVCoords(&x0, &y0, &x1, &y1);
		g_ShadertoyBuffer.x0 = x0;
		g_ShadertoyBuffer.y0 = y0;
		g_ShadertoyBuffer.x1 = x1;
		g_ShadertoyBuffer.y1 = y1;
		g_ShadertoyBuffer.VRmode = bDirectSBS;
		g_ShadertoyBuffer.iResolution[0] = g_fCurScreenWidth;
		g_ShadertoyBuffer.iResolution[1] = g_fCurScreenHeight;
		// Project the centroid to the left/right eyes
		for (int i = 0; i < g_ShadertoyBuffer.SunFlareCount; i++) {
			//QL = projectToInGameCoords(Centroid, g_viewMatrix, g_FullProjMatrixLeft);
			g_ShadertoyBuffer.SunCoords[i].x = QL[i].x;
			g_ShadertoyBuffer.SunCoords[i].y = QL[i].y;
		}
		resources->InitPSConstantBufferHyperspace(resources->_hyperspaceConstantBuffer.GetAddressOf(), &g_ShadertoyBuffer);
		resources->InitPixelShader(resources->_sunFlareComposeShaderPS);

		// The output from the previous effect will be in offscreenBufferPost, so let's resolve it
		// to _shaderToyBuf to re-use in the next step:
		context->ResolveSubresource(resources->_shadertoyBuf, 0, resources->_offscreenBufferPost, 0, BACKBUFFER_FORMAT);
		context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
		if (g_bUseSteamVR) {
			context->ResolveSubresource(resources->_shadertoyBufR, 0, resources->_offscreenBufferPostR, 0, BACKBUFFER_FORMAT);
			context->ClearRenderTargetView(resources->_renderTargetViewPostR, bgColor);
		}
		
		ID3D11RenderTargetView *rtvs[1] = {
			resources->_renderTargetViewPost.Get(), // Render to offscreenBufferPost instead of offscreenBuffer
		};
		context->OMSetRenderTargets(1, rtvs, NULL);
		// Set the SRVs:
		ID3D11ShaderResourceView *srvs[3] = {
			resources->_offscreenAsInputShaderResourceView.Get(),
			resources->_shadertoySRV.Get(),
			resources->_depthBufSRV.Get(),
		};
		context->PSSetShaderResources(0, 3, srvs);
		context->Draw(6, 0);

		// Post-process the right image
		if (g_bUseSteamVR) {
			//QR = projectToInGameCoords(Centroid, g_viewMatrix, g_FullProjMatrixRight);
			for (int i = 0; i < g_ShadertoyBuffer.SunFlareCount; i++) {
				g_ShadertoyBuffer.SunCoords[i].x = QR[i].x;
				g_ShadertoyBuffer.SunCoords[i].y = QR[i].y;
			}
			resources->InitPSConstantBufferHyperspace(resources->_hyperspaceConstantBuffer.GetAddressOf(), &g_ShadertoyBuffer);
			ID3D11RenderTargetView *rtvs[1] = {
				resources->_renderTargetViewPostR.Get(),
			};
			context->OMSetRenderTargets(1, rtvs, NULL);
			// Set the SRVs:
			ID3D11ShaderResourceView *srvs[3] = {
				resources->_offscreenAsInputShaderResourceViewR.Get(),
				resources->_shadertoySRV_R.Get(),
				resources->_depthBufSRV_R.Get(),
			};
			context->PSSetShaderResources(0, 3, srvs);
			context->Draw(6, 0);
		}
	}

	// Copy the result (_offscreenBufferPost) to the _offscreenBuffer so that it gets displayed
	context->CopyResource(resources->_offscreenBuffer, resources->_offscreenBufferPost);
	if (g_bUseSteamVR)
		context->CopyResource(resources->_offscreenBufferR, resources->_offscreenBufferPostR);

	// Restore previous rendertarget, etc
	resources->InitInputLayout(resources->_inputLayout); // Not sure this is really needed
}

void DisplayACAction(WORD *scanCodes);

/*
 * Executes the action defined by "action" as per the Active Cockpit
 * definitions.
 */
void ACRunAction(WORD *action) {
	// Scan codes from: http://www.philipstorr.id.au/pcbook/book3/scancode.htm
	// Scan codes: https://www.win.tue.nl/~aeb/linux/kbd/scancodes-1.html
	// Based on code from: https://stackoverflow.com/questions/18647053/sendinput-not-equal-to-pressing-key-manually-on-keyboard-in-c
	// Virtual key codes: https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
	// How to send extended scan codes
	// https://stackoverflow.com/questions/36972524/winapi-extended-keyboard-scan-codes/36976260#36976260
	// https://stackoverflow.com/questions/26283738/how-to-use-extended-scancodes-in-sendinput
	INPUT input[MAX_AC_ACTION_LEN];
	bool bEscapedAction = (action[0] == 0xe0);

	if (action[0] == 0) { // void action, skip
		//log_debug("[DBG] [AC] Skipping VOID action");
		return;
	}

	// Special internal action: these actions don't need to synthesize any input
	if (action[0] == 0xFF) {
		switch (action[1]) {
		case AC_HOLOGRAM_FAKE_VK_CODE:
			g_bDCHologramsVisible = !g_bDCHologramsVisible;
			return;
		}
		return;
	}

	//if (bEscapedAction)
	//	log_debug("[DBG] [AC] Executing escaped code");
	//log_debug("[DBG] [AC] Running action: ");
	//DisplayACAction(action);

	// Copy & initialize the scan codes
	int i = 0, j = bEscapedAction ? 1 : 0;
	while (action[j] && j < MAX_AC_ACTION_LEN) {
		input[i].ki.wScan = action[j];
		input[i].type = INPUT_KEYBOARD;
		input[i].ki.time = 0;
		input[i].ki.wVk = 0;
		input[i].ki.dwExtraInfo = 0;
		input[i].ki.dwFlags = KEYEVENTF_SCANCODE;
		if (bEscapedAction) {
			input[i].ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
			//input[i].ki.dwExtraInfo = GetMessageExtraInfo();
		}
		i++; j++;
	}

	j = bEscapedAction ? 1 : 0;
	while (action[j] && j < MAX_AC_ACTION_LEN) {
		input[i].ki.wScan = action[j];
		input[i].type = INPUT_KEYBOARD;
		input[i].ki.time = 0;
		input[i].ki.wVk = 0;
		input[i].ki.dwExtraInfo = 0;
		input[i].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
		if (bEscapedAction) {
			input[i].ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
			//input[i].ki.dwExtraInfo = GetMessageExtraInfo();
		}
		i++; j++;
	}

	// Send keydown/keyup events in one go: (this is the only way I found to enable the arrow/escaped keys)
	SendInput(i, input, sizeof(INPUT));
}

/*
 * Input: offscreenBuffer (resolved here)
 * Output: offscreenBufferPost
 */
void PrimarySurface::RenderLaserPointer(D3D11_VIEWPORT *lastViewport,
	ID3D11PixelShader *lastPixelShader, Direct3DTexture *lastTextureSelected,
	ID3D11Buffer *lastVertexBuffer, UINT *lastVertexBufStride, UINT *lastVertexBufOffset)
{
	auto& resources = this->_deviceResources;
	auto& device = resources->_d3dDevice;
	auto& context = resources->_d3dDeviceContext;
	float x0, y0, x1, y1;
	static float iTime = 0.0f;
	float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	//static Vector3 referencePos = Vector3(0, 0, 0);
	D3D11_VIEWPORT viewport{};
	// The viewport covers the *whole* screen, including areas that were not rendered during the first pass
	// because this is a post-process effect

	context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer, 0, BACKBUFFER_FORMAT);
	if (g_bUseSteamVR)
		context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR, 0, BACKBUFFER_FORMAT);

	resources->InitPixelShader(resources->_laserPointerPS);

	g_LaserPointerBuffer.DirectSBSEye = -1;
	if (g_bEnableVR && !g_bUseSteamVR)
		g_LaserPointerBuffer.DirectSBSEye = 1;

	GetScreenLimitsInUVCoords(&x0, &y0, &x1, &y1);
	//GetCraftViewMatrix(&g_LaserPointerBuffer.viewMat);
	g_LaserPointerBuffer.x0 = x0;
	g_LaserPointerBuffer.y0 = y0;
	g_LaserPointerBuffer.x1 = x1;
	g_LaserPointerBuffer.y1 = y1;
	g_LaserPointerBuffer.iResolution[0] = g_fCurScreenWidth;
	g_LaserPointerBuffer.iResolution[1] = g_fCurScreenHeight;
	g_LaserPointerBuffer.FOVscale = g_ShadertoyBuffer.FOVscale;
	g_LaserPointerBuffer.TriggerState = g_bACTriggerState;
	// Let's fix the aspect ratio of the laser pointer in non-VR mode:
	g_LaserPointerBuffer.lp_aspect_ratio[0] = g_bEnableVR ? 1.0f : g_MetricRecCBuffer.mr_screen_aspect_ratio;
	g_LaserPointerBuffer.lp_aspect_ratio[1] = 1.0f;
	// Detect triggers:
	if (g_bACLastTriggerState && !g_bACTriggerState)
		g_bACActionTriggered = true;
	g_bACLastTriggerState = g_bACTriggerState;

	// g_viewMatrix contains the camera Roll, nothing more right now
	bool bProjectContOrigin = (g_contOriginViewSpace[2] >= 0.001f);
	Vector3 contOriginDisplay = Vector3(g_contOriginViewSpace.x, g_contOriginViewSpace.y, g_contOriginViewSpace.z);
	Vector3 intersDisplay, pos2D;

	// Project the controller's position:
	Matrix4 viewMatrix = g_viewMatrix;
	viewMatrix.invert();
	if (bProjectContOrigin) {
		pos2D = projectMetric(contOriginDisplay, viewMatrix, g_FullProjMatrixLeft /*, NULL, NULL*/);
		g_LaserPointerBuffer.contOrigin[0] = pos2D.x;
		g_LaserPointerBuffer.contOrigin[1] = pos2D.y;
		g_LaserPointerBuffer.bContOrigin = 1;
		/* if (g_bDumpLaserPointerDebugInfo) {
			log_debug("[DBG] [AC] contOrigin: (%0.3f, %0.3f, %0.3f) --> (%0.3f, %0.3f)", 
				g_contOrigin.x, g_contOrigin.y, g_contOrigin.y,
				p.x, p.y);
		} */
	}
	else
		g_LaserPointerBuffer.bContOrigin = 0;
	
	// Project the intersection to 2D:
	if (g_LaserPointerBuffer.bIntersection) {
		intersDisplay = g_LaserPointer3DIntersection;
		if (g_LaserPointerBuffer.bDebugMode) {
			Vector3 q;
			q = projectMetric(g_debug_v0, viewMatrix, g_FullProjMatrixLeft /*, NULL, NULL*/); g_LaserPointerBuffer.v0[0] = q.x; g_LaserPointerBuffer.v0[1] = q.y;
			q = projectMetric(g_debug_v1, viewMatrix, g_FullProjMatrixLeft /*, NULL, NULL*/); g_LaserPointerBuffer.v1[0] = q.x; g_LaserPointerBuffer.v1[1] = q.y;
			q = projectMetric(g_debug_v2, viewMatrix, g_FullProjMatrixLeft /*, NULL, NULL*/); g_LaserPointerBuffer.v2[0] = q.x; g_LaserPointerBuffer.v2[1] = q.y;
		}
	} 
	else {
		// Make a fake intersection just to help the user move around the cockpit
		intersDisplay.x = contOriginDisplay.x + g_fLaserPointerLength * g_contDirViewSpace.x;
		intersDisplay.y = contOriginDisplay.y + g_fLaserPointerLength * g_contDirViewSpace.y;
		intersDisplay.z = contOriginDisplay.z + g_fLaserPointerLength * g_contDirViewSpace.z;
	}
	// Project the intersection point
	pos2D = projectMetric(intersDisplay, viewMatrix, g_FullProjMatrixLeft /*, NULL, NULL*/);
	g_LaserPointerBuffer.intersection[0] = pos2D.x;
	g_LaserPointerBuffer.intersection[1] = pos2D.y;

	g_LaserPointerBuffer.bHoveringOnActiveElem = 0;
	// If there was an intersection, find the action and execute it.
	// (I don't think this code needs to be here; but I put it here with the rest of the render function)
	if (g_LaserPointerBuffer.bIntersection && g_iBestIntersTexIdx > -1 && g_iBestIntersTexIdx < g_iNumACElements)
	{
		ac_uv_coords *coords = &(g_ACElements[g_iBestIntersTexIdx].coords);
		float u = g_LaserPointerBuffer.uv[0];
		float v = g_LaserPointerBuffer.uv[1];
		float u0 = u, v0 = v;

		// Fix negative UVs (yes, some OPTs may have negative UVs)
		while (u < 0.0f) u += 1.0f;
		while (v < 0.0f) v += 1.0f;
		// Fix UVs beyond 1
		while (u > 1.0f) u -= 1.0f;
		while (v > 1.0f) v -= 1.0f;

		for (int i = 0; i < coords->numCoords; i++) {
			if (coords->area[i].x0 <= u && u <= coords->area[i].x1 &&
				coords->area[i].y0 <= v && v <= coords->area[i].y1)
			{
				g_LaserPointerBuffer.bHoveringOnActiveElem = 1;
				if (g_bACActionTriggered) {
					short width = g_ACElements[g_iBestIntersTexIdx].width;
					short height = g_ACElements[g_iBestIntersTexIdx].height;
					
					/*log_debug("[DBG} *************");
					log_debug("[DBG] [AC] g_iBestIntersTexIdx: %d", g_iBestIntersTexIdx);
					log_debug("[DBG] [AC] Texture name: %s", g_ACElements[g_iBestIntersTexIdx].name);
					log_debug("[DBG] [AC] numCoords: %d", g_ACElements[g_iBestIntersTexIdx].coords.numCoords);
					log_debug("[DBG] [AC] Running action: [%s]", coords->action_name[i]);
					log_debug("[DBG] [AC] uv coords: (%0.3f, %0.3f)-(%0.3f, %0.3f)",
						coords->area[i].x0, coords->area[i].y0,
						coords->area[i].x1, coords->area[i].y1);
					log_debug("[DBG] [AC] laser uv (raw): (%0.3f, %0.3f)", u0, v0);
					log_debug("[DBG] [AC] laser uv: (%0.3f, %0.3f)-(%d, %d)",
						u, v, (short)(width * u), (short)(height * v));*/
					
					// Run the action itself
					ACRunAction(coords->action[i]);
					//log_debug("[DBG} *************");
				}
				break;
			}
		}
	}
	g_bACActionTriggered = false;

	// Temporarily disable ZWrite: we won't need it for post-proc
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = FALSE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);

	// Dump some debug info to see what's happening with the intersection
	if (g_bDumpLaserPointerDebugInfo) {
		Vector3 pos3D = Vector3(g_LaserPointer3DIntersection.x, g_LaserPointer3DIntersection.y, g_LaserPointer3DIntersection.z);
		Vector3 p = projectMetric(pos3D, g_viewMatrix, g_FullProjMatrixLeft /*, NULL, NULL*/);
		bool bIntersection = g_LaserPointerBuffer.bIntersection;
		log_debug("[DBG] [AC] bIntersection: %d", bIntersection);
		if (bIntersection) {
			short width = g_iBestIntersTexIdx > -1 ? g_ACElements[g_iBestIntersTexIdx].width : 0;
			short height = g_iBestIntersTexIdx > -1 ? g_ACElements[g_iBestIntersTexIdx].height : 0;
			log_debug("[DBG] [AC] g_iBestIntersTexIdx: %d", g_iBestIntersTexIdx);
			if (g_iBestIntersTexIdx > -1)
				log_debug("[DBG] [AC] Texture: %s", g_ACElements[g_iBestIntersTexIdx].name);
			else
				log_debug("[DBG] [AC] NULL Texture name (-1 index)");
			log_debug("[DBG] [AC] intersection: (%0.3f, %0.3f, %0.3f) --> (%0.3f, %0.3f)",
				pos3D.x, pos3D.y, pos3D.z, p.x, p.y);
			log_debug("[DBG] [AC] laser uv: (%0.3f, %0.3f)-(%d, %d)",
				g_LaserPointerBuffer.uv[0], g_LaserPointerBuffer.uv[1],
				(short)(width * g_LaserPointerBuffer.uv[0]), (short)(height * g_LaserPointerBuffer.uv[1]));
			
		}
		log_debug("[DBG] [AC] g_contOrigin: (%0.3f, %0.3f, %0.3f)", g_contOriginViewSpace.x, g_contOriginViewSpace.y, g_contOriginViewSpace.z);
		log_debug("[DBG] [AC] g_contDirection: (%0.3f, %0.3f, %0.3f)", g_contDirViewSpace.x, g_contDirViewSpace.y, g_contDirViewSpace.z);
		log_debug("[DBG] [AC] Triangle, best t: %0.3f: ", g_fBestIntersectionDistance);
		log_debug("[DBG] [AC] v0: (%0.3f, %0.3f)", g_LaserPointerBuffer.v0[0], g_LaserPointerBuffer.v0[1]);
		log_debug("[DBG] [AC] v1: (%0.3f, %0.3f)", g_LaserPointerBuffer.v1[0], g_LaserPointerBuffer.v1[1]);
		log_debug("[DBG] [AC] v2: (%0.3f, %0.3f)", g_LaserPointerBuffer.v2[0], g_LaserPointerBuffer.v2[1]);

		/*
		FILE *file = NULL;
		fopen_s(&file, "./test-tri-inters.obj", "wt");
		fprintf(file, "# Triangle\n");
		fprintf(file, "v %0.6f %0.6f %0.6f\n", g_debug_v0.x, g_debug_v0.y, g_debug_v0.z);
		fprintf(file, "v %0.6f %0.6f %0.6f\n", g_debug_v1.x, g_debug_v1.y, g_debug_v1.z);
		fprintf(file, "v %0.6f %0.6f %0.6f\n", g_debug_v2.x, g_debug_v2.y, g_debug_v2.z);
		fprintf(file, "\n# Origin\n");
		fprintf(file, "v %0.6f %0.6f %0.6f\n", g_contOriginViewSpace.x, g_contOriginViewSpace.y, g_contOriginViewSpace.z);
		fprintf(file, "\n# Intersection\n");
		fprintf(file, "v %0.6f %0.6f %0.6f\n", g_LaserPointer3DIntersection.x, g_LaserPointer3DIntersection.y, g_LaserPointer3DIntersection.z);
		fprintf(file, "\n# Triangle\n");
		fprintf(file, "f 1 2 3\n");
		fprintf(file, "# Line\n");
		fprintf(file, "f 4 5\n");
		fclose(file);
		log_debug("[DBG] [AC] test-tri-inters.obj dumped");
		*/

		//g_bDumpLaserPointerDebugInfo = false;
	}

	{
		//context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
		// Set the new viewport (a full quad covering the full screen)
		viewport.Width  = g_fCurScreenWidth;
		viewport.Height = g_fCurScreenHeight;
		// VIEWPORT-LEFT
		if (g_bEnableVR) {
			if (g_bUseSteamVR)
				viewport.Width = (float)resources->_backbufferWidth;
			else
				viewport.Width = (float)resources->_backbufferWidth / 2.0f;
		}
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.MinDepth = D3D11_MIN_DEPTH;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		resources->InitViewport(&viewport);

		// Set the vertex buffer
		UINT stride = sizeof(MainVertex);
		UINT offset = 0;
		resources->InitVertexBuffer(resources->_postProcessVertBuffer.GetAddressOf(), &stride, &offset);
		resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		resources->InitInputLayout(resources->_mainInputLayout);
		
		resources->InitVSConstantBuffer2D(resources->_mainShadersConstantBuffer.GetAddressOf(), 0.0f, 1.0f, 1.0f, 1.0f, 0.0f); // Do not use 3D projection matrices
		resources->InitPSConstantBufferLaserPointer(resources->_laserPointerConstantBuffer.GetAddressOf(), &g_LaserPointerBuffer);
		resources->InitVertexShader(resources->_mainVertexShader);

		context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
		// Set the RTV:
		ID3D11RenderTargetView *rtvs[1] = {
			resources->_renderTargetViewPost.Get(), // Render to offscreenBufferPost instead of offscreenBuffer
		};
		context->OMSetRenderTargets(1, rtvs, NULL);
		// Set the SRV:
		context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceView.GetAddressOf());
		context->Draw(6, 0);

		// Render the right image
		if (g_bEnableVR) {
			if (bProjectContOrigin) {
				pos2D = projectMetric(contOriginDisplay, viewMatrix, g_FullProjMatrixRight /*, NULL, NULL*/);
				g_LaserPointerBuffer.contOrigin[0] = pos2D.x;
				g_LaserPointerBuffer.contOrigin[1] = pos2D.y;
				g_LaserPointerBuffer.bContOrigin = 1;
			}
			else
				g_LaserPointerBuffer.bContOrigin = 0;

			// Project the intersection to 2D:
			pos2D = projectMetric(intersDisplay, viewMatrix, g_FullProjMatrixRight /*, NULL, NULL*/);
			g_LaserPointerBuffer.intersection[0] = pos2D.x;
			g_LaserPointerBuffer.intersection[1] = pos2D.y;

			if (g_LaserPointerBuffer.bIntersection && g_LaserPointerBuffer.bDebugMode) {
				Vector3 q;
				q = projectMetric(g_debug_v0, viewMatrix, g_FullProjMatrixRight /*, NULL, NULL*/); g_LaserPointerBuffer.v0[0] = q.x; g_LaserPointerBuffer.v0[1] = q.y;
				q = projectMetric(g_debug_v1, viewMatrix, g_FullProjMatrixRight /*, NULL, NULL*/); g_LaserPointerBuffer.v1[0] = q.x; g_LaserPointerBuffer.v1[1] = q.y;
				q = projectMetric(g_debug_v2, viewMatrix, g_FullProjMatrixRight /*, NULL, NULL*/); g_LaserPointerBuffer.v2[0] = q.x; g_LaserPointerBuffer.v2[1] = q.y;
			}

			// VIEWPORT-RIGHT
			if (g_bUseSteamVR) {
				context->ClearRenderTargetView(resources->_renderTargetViewPostR, bgColor);
				viewport.Width = (float)resources->_backbufferWidth;
				viewport.TopLeftX = 0.0f;
			}
			else {
				viewport.Width = (float)resources->_backbufferWidth / 2.0f;
				viewport.TopLeftX = (float)viewport.Width;
				g_LaserPointerBuffer.DirectSBSEye = 2;
			}
			viewport.Height = (float)resources->_backbufferHeight;
			viewport.TopLeftY = 0.0f;
			viewport.MinDepth = D3D11_MIN_DEPTH;
			viewport.MaxDepth = D3D11_MAX_DEPTH;
			resources->InitViewport(&viewport);

			resources->InitPSConstantBufferLaserPointer(resources->_laserPointerConstantBuffer.GetAddressOf(), &g_LaserPointerBuffer);

			if (g_bUseSteamVR) {
				context->OMSetRenderTargets(1, resources->_renderTargetViewPostR.GetAddressOf(), NULL);
				context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceViewR.GetAddressOf());
			} 
			else {
				context->OMSetRenderTargets(1, resources->_renderTargetViewPost.GetAddressOf(), NULL);
				context->PSSetShaderResources(0, 1, resources->_offscreenAsInputShaderResourceView.GetAddressOf());
			}
			context->Draw(6, 0);
		}
	}

	context->CopyResource(resources->_offscreenBuffer, resources->_offscreenBufferPost);
	if (g_bUseSteamVR)
		context->CopyResource(resources->_offscreenBufferR, resources->_offscreenBufferPostR);

	// Restore previous rendertarget, etc
	resources->InitInputLayout(resources->_inputLayout);
	context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(),
		resources->_depthStencilViewL.Get());

	// Don't restore anything...
	// I wonder if this is causing the bug that makes the menu go black? Maybe I need to restore
	// the 2D vertex buffer here?
	//goto out;

	/*
	// Restore the original state: VertexBuffer, Shaders, Topology, Z-Buffer state, etc...
	resources->InitViewport(lastViewport);
	if (lastTextureSelected != NULL) {
		lastTextureSelected->_refCount++;
		context->PSSetShaderResources(0, 1, lastTextureSelected->_textureView.GetAddressOf());
		lastTextureSelected->_refCount--;
	}
	resources->InitInputLayout(resources->_inputLayout);
	if (g_bEnableVR)
		this->_deviceResources->InitVertexShader(resources->_sbsVertexShader);
	else
		this->_deviceResources->InitVertexShader(resources->_vertexShader);
	resources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	resources->InitPixelShader(lastPixelShader);
	resources->InitRasterizerState(resources->_rasterizerState);
	if (lastVertexBuffer != NULL)
		resources->InitVertexBuffer(&lastVertexBuffer, lastVertexBufStride, lastVertexBufOffset);
	resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);
	resources->InitPSConstantBuffer3D(resources->_PSConstantBuffer.GetAddressOf(), &g_PSCBuffer);
out:
*/
}

void ProcessFreePIEGamePad(uint32_t axis0, uint32_t axis1, uint32_t buttonsPressed) {
	static uint32_t lastButtonsPressed = 0x0;
	WORD events[6];

	/*
	Vertical Axis: 
	Axis0: Up --> -1, Down --> 1
	Up --> 0, Down --> 254, Center --> 127

	Horizontal Axis:
	Axis1: Right --> -1, Left --> 1
	Right --> 0, Left --> 254, Center --> 127
	*/
	//if (axis0 != 127) log_debug("[DBG] axis0: %d", axis0); 
	//if (axis1 != 127) log_debug("[DBG] axis1: %d", axis1);

	// Gamepad Joystick Up: Increase speed
	if (axis0 < 100) { 
		// Send a [+] keypress for as long as this key is depressed
		events[0] = 0x0D;
		events[1] = 0x0;
		ACRunAction(events);
	} 

	// Gamepad Joystick Down: Decrease speed
	if (axis0 > 130) { 
		// Send a [-] keypress for as long as this key is depressed
		events[0] = 0x0C;
		events[1] = 0x0;
		ACRunAction(events);
	}
	
	// Down button: reset view (period key)
	if (!(buttonsPressed & 0x02) && (lastButtonsPressed & 0x02)) {
		events[0] = 0x34; // period key
		events[1] = 0x0;
		ACRunAction(events);
	}
	
	// Up Button: Trigger
	if (buttonsPressed & 0x04) 
		g_bACTriggerState = true;
	else
		g_bACTriggerState = false;

	// Left button: 1/3 thrust "[" key
	if (!(buttonsPressed & 0x01) && (lastButtonsPressed & 0x01)) {
		events[0] = 0x1A;
		events[1] = 0x0;
		ACRunAction(events);
	}

	// Right: Match speed with target [Enter]
	if (!(buttonsPressed & 0x08) && (lastButtonsPressed & 0x08)) {
		events[0] = 0x1C;
		events[1] = 0x0;
		ACRunAction(events);
	}

	lastButtonsPressed = buttonsPressed;
}

/*
 * Compute the current view matrices from SteamVR or FreePIE and store them in
 * g_VSMatrixCB.viewMat, and g_VSMatrixCB.fullViewMat = rotMatrixFull.
 * This function also updates the laser pointer (this is mostly "TO-DO" at the moment)
 */
void UpdateViewMatrix()
{
	float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
	float x = 0.0f, y = 0.0f, z = 0.0f;
	static float home_yaw = 0.0f, home_pitch = 0.0f, home_roll = 0.0f;

	// Enable roll (formerly this was 6dof)
	if (g_bUseSteamVR) {
		Matrix3 rotMatrix;

		GetSteamVRPositionalData(&yaw, &pitch, &roll, &x, &y, &z, &rotMatrix);
		yaw   *= RAD_TO_DEG * g_fYawMultiplier;
		pitch *= RAD_TO_DEG * g_fPitchMultiplier;
		roll  *= RAD_TO_DEG * g_fRollMultiplier;

		// DEBUG
		if (g_bSteamVRYawPitchRollFromMouseLook)
			GetFakeYawPitchRollFromMouseLook(&yaw, &pitch, &roll);
		// DEBUG

		yaw   += g_fYawOffset;
		pitch += g_fPitchOffset;

		// Compute the full rotation
		Matrix4 rotMatrixFull, rotMatrixYaw, rotMatrixPitch, rotMatrixRoll;
		rotMatrixFull.identity();
		rotMatrixYaw.identity();   rotMatrixYaw.rotateY(-yaw);
		rotMatrixPitch.identity(); rotMatrixPitch.rotateX(-pitch);
		rotMatrixRoll.identity();  rotMatrixRoll.rotateZ(roll);
		rotMatrixFull = rotMatrixRoll * rotMatrixPitch * rotMatrixYaw;

		// Transform the absolute head position into a relative position. This is
		// needed because the game will apply the yaw/pitch on its own. So, we need
		// to undo the yaw/pitch transformation by computing the inverse of the
		// rotation matrix. Fortunately, rotation matrices can be inverted with a
		// simple transpose.
		rotMatrix.invert();

		g_viewMatrix.identity();
		g_viewMatrix.rotateZ(roll);
		
		// viewMat is not a full transform matrix: it's only RotZ
		// because the cockpit hook already applies the yaw/pitch rotation
		g_VSMatrixCB.viewMat = g_viewMatrix;
		g_VSMatrixCB.fullViewMat = rotMatrixFull;
	}
	else 
	{
		// non-VR and DirectSBS modes, read the roll and position from FreePIE
		//static Vector4 headCenterPos(0, 0, 0, 0);

		// Read yaw/pitch/roll from FreePIE
		if (g_iFreePIESlot > -1 && ReadFreePIE(g_iFreePIESlot)) {
			if (g_bResetHeadCenter) {
				home_yaw   = g_FreePIEData.yaw;
				home_pitch = g_FreePIEData.pitch;
				home_roll  = g_FreePIEData.roll;
				g_bResetHeadCenter = false;
			}
			yaw   =  (g_FreePIEData.yaw   - home_yaw)   * g_fYawMultiplier;
			pitch = -(g_FreePIEData.pitch - home_pitch) * g_fPitchMultiplier;
			roll  =  (g_FreePIEData.roll  - home_roll)  * g_fRollMultiplier;

		}

		if (g_bYawPitchFromMouseOverride) {
			// If FreePIE could not be read, then get the yaw/pitch from the mouse:
			yaw   =  (float)PlayerDataTable[*g_playerIndex].cockpitCameraYaw / 32768.0f * 180.0f;
			pitch = -(float)PlayerDataTable[*g_playerIndex].cockpitCameraPitch / 32768.0f * 180.0f;
		}

		if (g_bEnableVR) {
			// If we're rendering 2D, then the PlayerDataTable camera will not be updated by the mouse hook,
			// so we need to use the yaw,pitch,roll coming from FreePIE. But if we're doing 3D rendering, then
			// the hook should've updated the cockpit/external camera and we can read it here:
			if (g_bRendering3D && !g_playerInHangar) {
				if (PlayerDataTable[*g_playerIndex].externalCamera) {
					yaw   = (float)PlayerDataTable[*g_playerIndex].cameraYaw / 65536.0f * 360.0f;
					pitch = (float)PlayerDataTable[*g_playerIndex].cameraPitch / 65536.0f * 360.0f;
				}
				else {
					yaw   = (float)PlayerDataTable[*g_playerIndex].cockpitCameraYaw / 65536.0f * 360.0f;
					pitch = (float)PlayerDataTable[*g_playerIndex].cockpitCameraPitch / 65536.0f * 360.0f;
				}
			}

			// Compute the rotation matrices with the current yaw, pitch, roll:
			Matrix4 rotMatrixFull, rotMatrixYaw, rotMatrixPitch, rotMatrixRoll;
			rotMatrixFull.identity();
			rotMatrixYaw.identity();   rotMatrixYaw.rotateY(-yaw);
			rotMatrixPitch.identity(); rotMatrixPitch.rotateX(-pitch);
			rotMatrixRoll.identity();  rotMatrixRoll.rotateZ(roll);

			// For the fixed GUI, yaw has to be like this:
			rotMatrixFull.rotateY(yaw);
			rotMatrixFull = rotMatrixRoll * rotMatrixPitch * rotMatrixFull;
			rotMatrixYaw  = rotMatrixPitch * rotMatrixYaw;
			// Can we avoid computing the matrix inverse?
			rotMatrixYaw.invert();

			g_viewMatrix.identity();
			g_viewMatrix.rotateZ(roll);
			//g_viewMatrix.rotateZ(roll + 30.0f * headPosFromKeyboard[0]); // HACK to enable roll-through keyboard

			// viewMat is not a full transform matrix: it's only RotZ + Translation
			// because the cockpit hook already applies the yaw/pitch rotation
			g_VSMatrixCB.viewMat = g_viewMatrix;
			g_VSMatrixCB.fullViewMat = rotMatrixFull;
		}
	}

	if (g_bResetHeadCenter)
		g_bResetHeadCenter = false;

	// At this point, yaw, pitch, roll contain the data read from either SteamVR or FreePIE.
	// We can use these values to apply head tracking to the hangar.
	// The reason we have to do this here is because the hangar does not activate the regular
	// mouse look hook.
	if ((g_TrackerType == TRACKER_FREEPIE || g_TrackerType == TRACKER_STEAMVR) && *g_playerInHangar) {
		const bool bExternalCamera = PlayerDataTable[*g_playerIndex].externalCamera;
		
		// For the DirectSBS mode we need to invert the yaw:
		if (g_TrackerType == TRACKER_FREEPIE)
			yaw = -yaw;

		if (bExternalCamera) {
			PlayerDataTable[*g_playerIndex].cameraYaw = -(short )((yaw / 360.0f) * 65535.0f);
			PlayerDataTable[*g_playerIndex].cameraPitch = (short)((pitch / 360.0f) * 65535.0f);
		}
		else {
			PlayerDataTable[*g_playerIndex].cockpitCameraYaw = -(short)((yaw / 360.0f) * 65535.0f);
			PlayerDataTable[*g_playerIndex].cockpitCameraPitch = (short)((pitch / 360.0f) * 65535.0f);
		}
	}

	/*
	// Read the controller's position from FreePIE
	if (g_iFreePIEControllerSlot > -1 && ReadFreePIE(g_iFreePIEControllerSlot)) {
		if (g_bResetHeadCenter && !g_bOriginFromHMD) {
			headCenterPos[0] = g_FreePIEData.x;
			headCenterPos[1] = g_FreePIEData.y;
			headCenterPos[2] = g_FreePIEData.z;
		}
		g_contOriginWorldSpace.x = g_FreePIEData.x - headCenterPos.x;
		g_contOriginWorldSpace.y = g_FreePIEData.y - headCenterPos.y;
		g_contOriginWorldSpace.z = g_FreePIEData.z - headCenterPos.z;
		g_contOriginWorldSpace.z = -g_contOriginWorldSpace.z; // The z-axis is inverted w.r.t. the FreePIE tracker
		if (g_bFreePIEControllerButtonDataAvailable) {
			uint32_t buttonsPressed = *((uint32_t*)&(g_FreePIEData.yaw));
			uint32_t axis0 = *((uint32_t*)&g_FreePIEData.pitch);
			uint32_t axis1 = *((uint32_t*)&g_FreePIEData.roll);
			ProcessFreePIEGamePad(axis0, axis1, buttonsPressed);
			//g_bACTriggerState = buttonsPressed != 0x0;
		}
	}
	*/

	// Update the laser pointer position.
	g_contOriginViewSpace = g_contOriginWorldSpace;
	g_contDirViewSpace = g_contDirWorldSpace;
	// In VR mode, the y-axis of the laser pointer is flipped with respect to the non-VR path:
	if (g_bEnableVR)
		g_contOriginViewSpace.y = -g_contOriginViewSpace.y;
}

HRESULT PrimarySurface::Flip(
	LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride,
	DWORD dwFlags
	)
{
	static uint64_t frame, lastFrame = 0;
	static float seconds;

#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;

	if (lpDDSurfaceTargetOverride == nullptr)
	{
		str << " NULL";
	}
	else if (lpDDSurfaceTargetOverride == this->_deviceResources->_backbufferSurface)
	{
		str << " BackbufferSurface";
	}
	else if (lpDDSurfaceTargetOverride == this->_deviceResources->_frontbufferSurface)
	{
		str << " FrontbufferSurface";
	}
	else
	{
		str << " " << lpDDSurfaceTargetOverride;
	}

	if (dwFlags & DDFLIP_WAIT)
	{
		str << " WAIT";
	}

	LogText(str.str());
#endif

	auto &resources = this->_deviceResources;
	auto &context = resources->_d3dDeviceContext;
	auto &device = resources->_d3dDevice;
	this->_deviceResources->sceneRenderedEmpty = this->_deviceResources->sceneRendered == false;
	this->_deviceResources->sceneRendered = false;
	bool bHyperspaceFirstFrame = g_bHyperspaceFirstFrame; // Used to clear the shadowMap DSVs *after* they're used

	if (this->_deviceResources->sceneRenderedEmpty && this->_deviceResources->_frontbufferSurface != nullptr && this->_deviceResources->_frontbufferSurface->wasBltFastCalled)
	{
		if (!g_bHyperspaceFirstFrame) {
			context->ClearRenderTargetView(resources->_renderTargetView, resources->clearColor);
			context->ClearRenderTargetView(resources->_shadertoyRTV, resources->clearColorRGBA);
		}
		context->ClearRenderTargetView(resources->_renderTargetViewPost, resources->clearColorRGBA);
		if (g_bUseSteamVR) {
			if (!g_bHyperspaceFirstFrame) {
				context->ClearRenderTargetView(resources->_renderTargetViewR, resources->clearColor);
				context->ClearRenderTargetView(resources->_shadertoyRTV_R, resources->clearColorRGBA);
			}
			context->ClearRenderTargetView(resources->_renderTargetViewPostR, resources->clearColorRGBA);
		}

		if (!g_bHyperspaceFirstFrame) {
			context->ClearDepthStencilView(resources->_depthStencilViewL, D3D11_CLEAR_DEPTH, resources->clearDepth, 0);
			if (g_bUseSteamVR)
				context->ClearDepthStencilView(resources->_depthStencilViewR, D3D11_CLEAR_DEPTH, resources->clearDepth, 0);
		}
	}

	/* Present 2D content */
	if (lpDDSurfaceTargetOverride != nullptr)
	{
		if (lpDDSurfaceTargetOverride == this->_deviceResources->_frontbufferSurface)
		{
			HRESULT hr;

			if (FAILED(hr = this->_deviceResources->_backbufferSurface->BltFast(0, 0, this->_deviceResources->_frontbufferSurface, nullptr, 0)))
				return hr;

			return this->Flip(this->_deviceResources->_backbufferSurface, 0);
		}

		/* Present 2D content */
		if (lpDDSurfaceTargetOverride == this->_deviceResources->_backbufferSurface)
		{
			g_bInTechRoom = (g_iDrawCounter > 0);
			g_iDrawCounter = 0;

			//if (bInTechRoom) log_debug("[DBG] IN TECH ROOM");
			// If we don't have the metric params ready by the time the Tech Room is presented,
			// then nothing will show up, so it's better to initialize the params (with default
			// values) just in case we go into the tech room before we load any mission.
			// This is only needed in VR mode:
			if (g_bEnableVR && g_bInTechRoom)
			{
				//float y_center_temp = g_MetricRecCBuffer.mr_y_center;
				*g_fRawFOVDist = 256.0f;
				*g_cachedFOVDist = *g_fRawFOVDist / 512.0f;
				*g_rawFOVDist = (uint32_t)*g_fRawFOVDist;
				//g_bYCenterHasBeenFixed = false; // Force the recomputation of YCenter when a mission loads
				g_bCustomFOVApplied = false;

				// Compute the metric scale factor conversion
				g_fOBJCurMetricScale = g_fCurInGameHeight * g_fOBJGlobalMetricMult / (SHADOW_OBJ_SCALE * 3200.0f);
				
				g_MetricRecCBuffer.mr_y_center = 0.0f;
				g_MetricRecCBuffer.mr_cur_metric_scale = g_fOBJCurMetricScale;
				g_MetricRecCBuffer.mr_aspect_ratio = g_fCurInGameAspectRatio;
				g_MetricRecCBuffer.mr_z_metric_mult = g_fOBJ_Z_MetricMult;
				g_MetricRecCBuffer.mr_shadow_OBJ_scale = SHADOW_OBJ_SCALE;

				// We need to set these CBs here if the Tech Room is displayed before any mission has set the FOV params
				resources->InitVSConstantBufferMetricRec(resources->_metricRecVSConstantBuffer.GetAddressOf(), &g_MetricRecCBuffer);
				resources->InitPSConstantBufferMetricRec(resources->_metricRecPSConstantBuffer.GetAddressOf(), &g_MetricRecCBuffer);
				// Restore the previous y_center:
				//g_MetricRecCBuffer.mr_y_center = y_center_temp;
				//log_debug("[DBG] Initializing 2D Metric Params");
			}

			// Read yaw,pitch,roll from SteamVR/FreePIE and apply the rotation to the 2D content
			// on the next frame
			UpdateViewMatrix(); // VR in TechRoom
#ifdef DISABLED
			//if (g_bEnableVR)
			{
				float yaw, pitch, roll, x,y,z;
				Matrix3 rotMatrix;

				if (g_bUseSteamVR) {
					GetSteamVRPositionalData(&yaw, &pitch, &roll, &x, &y, &z, &rotMatrix);
					// We need to multiply the yaw and pitch in SteamVR mode to invert the rotation
					yaw   *= RAD_TO_DEG * g_fYawMultiplier * -1.0f;
					pitch *= RAD_TO_DEG * g_fPitchMultiplier * -1.0f;
					roll  *= RAD_TO_DEG * g_fRollMultiplier;
					yaw   += g_fYawOffset;
					pitch += g_fPitchOffset;
				}
				else {
					// DirectSBS case
					static float home_yaw = 0.0f, home_pitch = 0.0f, home_roll = 0.0f;
					// Read yaw/pitch/roll from FreePIE
					if (g_iFreePIESlot > -1 && ReadFreePIE(g_iFreePIESlot)) {
						if (g_bResetHeadCenter) {
							home_yaw   = g_FreePIEData.yaw;
							home_pitch = g_FreePIEData.pitch;
							home_roll  = g_FreePIEData.roll;
							g_bResetHeadCenter = false;
						}
						yaw   = (g_FreePIEData.yaw   - home_yaw)   * g_fYawMultiplier;
						pitch = (g_FreePIEData.pitch - home_pitch) * g_fPitchMultiplier;
						roll  = (g_FreePIEData.roll  - home_roll)  * g_fRollMultiplier;
					}
				}

				// DEBUG
				/*
				GetFakeYawPitchRollFromKeyboard(&yaw, &pitch, &roll);
				*/
				// DEBUG

				// Compute the full rotation
				Matrix4 rotMatrixFull, rotMatrixYaw, rotMatrixPitch, rotMatrixRoll;
				rotMatrixFull.identity();
				rotMatrixYaw.identity();   rotMatrixYaw.rotateY(g_f2DYawMul * yaw);
				rotMatrixPitch.identity(); rotMatrixPitch.rotateX(g_f2DPitchMul * pitch);
				rotMatrixRoll.identity();  rotMatrixRoll.rotateZ(g_f2DRollMul * roll);
				rotMatrixFull = rotMatrixRoll * rotMatrixPitch * rotMatrixYaw;

				g_VSMatrixCB.fullViewMat = rotMatrixFull;
			}
#endif

			if (this->_deviceResources->_frontbufferSurface == nullptr)
			{
				if (FAILED(this->_deviceResources->RenderMain(this->_deviceResources->_backbufferSurface->_buffer, this->_deviceResources->_displayWidth, this->_deviceResources->_displayHeight, this->_deviceResources->_displayBpp)))
					return DDERR_GENERIC;
			}
			else
			{
				const unsigned short colorKey = 0x8080;

				int width = 640;
				int height = 480;

				int topBlack = 0;

				if (this->_deviceResources->_displayBpp == 2)
				{
					int w = this->_deviceResources->_displayWidth;
					int x = (this->_deviceResources->_displayWidth - width) / 2;
					int y = (this->_deviceResources->_displayHeight - height) / 2;
					unsigned short* buffer = (unsigned short*)this->_deviceResources->_backbufferSurface->_buffer + y * w + x;

					for (int i = 0; i < height / 2; i++)
					{
						if (buffer[i * w] != colorKey)
							break;

						topBlack++;
					}
				}
				else
				{
					const unsigned int colorKey32 = convertColorB5G6R5toB8G8R8X8(colorKey);

					int w = this->_deviceResources->_displayWidth;
					int x = (this->_deviceResources->_displayWidth - width) / 2;
					int y = (this->_deviceResources->_displayHeight - height) / 2;
					unsigned int* buffer = (unsigned int*)this->_deviceResources->_backbufferSurface->_buffer + y * w + x;

					for (int i = 0; i < height / 2; i++)
					{
						if (buffer[i * w] != colorKey32)
							break;

						topBlack++;
					}
				}

				if (topBlack < height / 2)
				{
					height -= topBlack * 2;
				}

				RECT rc;
				rc.left = (this->_deviceResources->_displayWidth - width) / 2;
				rc.top = (this->_deviceResources->_displayHeight - height) / 2;
				rc.right = rc.left + width;
				rc.bottom = rc.top + height;

				int length = width * height * this->_deviceResources->_displayBpp;
				char* buffer = new char[length];

				copySurface(buffer, width, height, this->_deviceResources->_displayBpp, this->_deviceResources->_backbufferSurface->_buffer, this->_deviceResources->_displayWidth, this->_deviceResources->_displayHeight, this->_deviceResources->_displayBpp, 0, 0, &rc, false);

				HRESULT hr = this->_deviceResources->RenderMain(buffer, width, height, this->_deviceResources->_displayBpp);

				delete[] buffer;

				if (FAILED(hr))
					return DDERR_GENERIC;
			}

			HRESULT hr;

			/* Present Concourse, ESC Screen Menu */
			if (this->_deviceResources->_swapChain)
			{
				/*
				// Resolve the bloom mask
				if (g_bBloomEnabled) {
					context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMask, 0,
						resources->_offscreenBufferBloomMask, 0, BLOOM_BUFFER_FORMAT);
					if (g_bUseSteamVR)
						context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMaskR, 0,
							resources->_offscreenBufferBloomMaskR, 0, BLOOM_BUFFER_FORMAT);
				}

				if (g_bDumpSSAOBuffers) {
					DirectX::SaveWICTextureToFile(context, resources->_offscreenBuffer, GUID_ContainerFormatJpeg,
						L"C:\\Temp\\_offscreenBuf.jpg");
					DirectX::SaveDDSTextureToFile(context, resources->_offscreenBufferAsInputBloomMask, L"C:\\Temp\\_bloomMask2D-1.dds");
					//DirectX::SaveDDSTextureToFile(context, resources->_bentBuf, L"C:\\Temp\\_bentBuf.dds");
					DirectX::SaveDDSTextureToFile(context, resources->_depthBuf, L"C:\\Temp\\_depthBuf2D.dds");
					//DirectX::SaveWICTextureToFile(context, resources->_bentBuf, GUID_ContainerFormatJpeg, L"C:\\Temp\\_bentBuf2D.jpg");
					DirectX::SaveDDSTextureToFile(context, resources->_ssaoBuf, L"C:\\Temp\\_ssaoBuf2D.dds");
					DirectX::SaveWICTextureToFile(context, resources->_ssaoBufR, GUID_ContainerFormatJpeg, L"C:\\Temp\\_ssaoBufR2D.jpg");
					DirectX::SaveDDSTextureToFile(context, resources->_normBuf, L"C:\\Temp\\_normBuf2D.dds");
					DirectX::SaveDDSTextureToFile(context, resources->_ssaoMask, L"C:\\Temp\\_ssaoMask2D.dds");
					DirectX::SaveDDSTextureToFile(context, resources->_ssMask, L"C:\\Temp\\_ssMask2D.dds");
				}

				if (g_bAOEnabled) {
					switch (g_SSAO_Type) {
					case SSO_AMBIENT:
						SSAOPass(g_fSSAOZoomFactor);
						// Resolve the bloom mask again: SSDO can modify this mask
						context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMask, 0,
							resources->_offscreenBufferBloomMask, 0, BLOOM_BUFFER_FORMAT);
						if (g_bUseSteamVR)
							context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMaskR, 0,
								resources->_offscreenBufferBloomMaskR, 0, BLOOM_BUFFER_FORMAT);
						break;
					case SSO_DIRECTIONAL:
					case SSO_BENT_NORMALS:
						SSDOPass(g_fSSAOZoomFactor, g_fSSAOZoomFactor2);
						// Resolve the bloom mask again: SSDO can modify this mask
						context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMask, 0,
							resources->_offscreenBufferBloomMask, 0, BLOOM_BUFFER_FORMAT);
						if (g_bUseSteamVR)
							context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMaskR, 0,
								resources->_offscreenBufferBloomMaskR, 0, BLOOM_BUFFER_FORMAT);
						break;
					case SSO_DEFERRED:
						DeferredPass();
						// Resolve the bloom mask again: SSDO can modify this mask
						context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMask, 0,
							resources->_offscreenBufferBloomMask, 0, BLOOM_BUFFER_FORMAT);
						if (g_bUseSteamVR)
							context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMaskR, 0,
								resources->_offscreenBufferBloomMaskR, 0, BLOOM_BUFFER_FORMAT);
						break;
					}
				}

				if (g_bDumpSSAOBuffers) {
					DirectX::SaveDDSTextureToFile(context, resources->_offscreenBufferAsInputBloomMask, L"C:\\Temp\\_bloomMask2D-2.dds");
				}

				// Apply the Bloom effect
				if (g_bBloomEnabled) {
					// _offscreenBufferAsInputBloomMask is resolved earlier, before the SSAO pass because
					// SSAO uses that mask to prevent applying SSAO on bright areas

					// We need to set the blend state properly for Bloom, or else we might get
					// different results when brackets are rendered because they alter the 
					// blend state
					D3D11_BLEND_DESC blendDesc{};
					blendDesc.AlphaToCoverageEnable = FALSE;
					blendDesc.IndependentBlendEnable = FALSE;
					blendDesc.RenderTarget[0].BlendEnable = TRUE;
					blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
					blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
					blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
					blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
					blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
					blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
					blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
					hr = resources->InitBlendState(nullptr, &blendDesc);

					// Temporarily disable ZWrite: we won't need it to display Bloom
					D3D11_DEPTH_STENCIL_DESC desc;
					ComPtr<ID3D11DepthStencilState> depthState;
					desc.DepthEnable = FALSE;
					desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
					desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
					desc.StencilEnable = FALSE;
					resources->InitDepthStencilState(depthState, &desc);

					// Initialize the accummulator buffer
					float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
					context->ClearRenderTargetView(resources->_renderTargetViewBloomSum, bgColor);
					if (g_bUseSteamVR)
						context->ClearRenderTargetView(resources->_renderTargetViewBloomSumR, bgColor);

					float fScale = 2.0f;
					for (int i = 1; i <= g_BloomConfig.iNumPasses; i++) {
						int AdditionalPasses = g_iBloomPasses[i] - 1;
						// Zoom level 2.0f with only one pass tends to show artifacts unless
						// the spread is set to 1
						BloomPyramidLevelPass(i, AdditionalPasses, fScale);
						fScale *= 2.0f;
					}

					// TODO: Check the statement below, I'm not sure it's current anymore (?)
					// If SSAO is not enabled, then we can merge the bloom buffer with the offscreen buffer
					// here. Otherwise, we'll merge it along with the SSAO buffer later.
					// Add the accumulated bloom with the offscreen buffer
					// Input: _bloomSum, _offscreenBufferAsInput
					// Output: _offscreenBuffer
					BloomBasicPass(5, 1.0f);

					// DEBUG
					if (g_bDumpSSAOBuffers) {
						DirectX::SaveDDSTextureToFile(context, resources->_bloomOutput1, L"C:\\Temp\\_bloomOutput2D.dds");
						DirectX::SaveDDSTextureToFile(context, resources->_offscreenBuffer, L"C:\\Temp\\_offscreenBuffer-Bloom2D.dds");
					}
					// DEBUG
				}
				*/

				UINT rate = 25 * this->_deviceResources->_refreshRate.Denominator;
				UINT numerator = this->_deviceResources->_refreshRate.Numerator + this->_flipFrames;
				UINT interval = numerator / rate;
				this->_flipFrames = numerator % rate;
				// DEBUG
				//static bool bDisplayInterval = true;
				// DEBUG

				interval = max(interval, 1);
				// DEBUG
				/*if (bDisplayInterval) {
					log_debug("[DBG] Original interval: %d", interval);
				}*/
				// DEBUG

				hr = DD_OK;

				interval = g_iNaturalConcourseAnimations ? interval : 1;
				if (g_iNaturalConcourseAnimations > 1)
					interval = g_iNaturalConcourseAnimations;
				
				// DEBUG
				/*if (bDisplayInterval) {
					log_debug("[DBG] g_iNaturalConcourseAnimations: %d, Final interval: %d", g_iNaturalConcourseAnimations, interval);
					bDisplayInterval = false;
				}*/
				// DEBUG

				for (UINT i = 0; i < interval; i++)
				{
					// In the original code the offscreenBuffer is simply resolved into the backBuffer.
					// this->_deviceResources->_d3dDeviceContext->ResolveSubresource(this->_deviceResources->_backBuffer, 0, this->_deviceResources->_offscreenBuffer, 0, BACKBUFFER_FORMAT);

					if (!g_bDisableBarrelEffect && g_bEnableVR && !g_bUseSteamVR) {
						// Barrel effect enabled for DirectSBS mode
						barrelEffect2D(i);
						context->ResolveSubresource(resources->_backBuffer, 0, resources->_offscreenBufferPost, 0, BACKBUFFER_FORMAT);
					}
					else {
						// In SteamVR mode this will display the left image:
						if (g_bUseSteamVR) {
							resizeForSteamVR(0, true);
							context->ResolveSubresource(resources->_backBuffer, 0, resources->_steamVRPresentBuffer, 0, BACKBUFFER_FORMAT);
						}
						else {
							// Non-VR path
							context->ResolveSubresource(resources->_backBuffer, 0, resources->_offscreenBuffer, 0, BACKBUFFER_FORMAT);
						}
					}
					
					// Enable capturing 3D objects even when rendering 2D. This happens in the Tech Library
#ifdef DBG_VR
					if (g_bStart3DCapture && !g_bDo3DCapture) {
						g_bDo3DCapture = true;
					}
					else if (g_bStart3DCapture && g_bDo3DCapture) {
						g_bDo3DCapture = false;
						g_bStart3DCapture = false;
						fclose(g_HackFile);
						g_HackFile = NULL;
					}
#endif

					// Reset the 2D draw counter -- that'll help us increase the parallax for the Tech Library
					g_iDraw2DCounter = 0;				

					if (g_bUseSteamVR) {
						vr::EVRCompositorError error = vr::VRCompositorError_None;
						vr::Texture_t leftEyeTexture = { this->_deviceResources->_offscreenBuffer.Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto };
						vr::Texture_t rightEyeTexture = { this->_deviceResources->_offscreenBufferR.Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto };
						error = g_pVRCompositor->Submit(vr::Eye_Left, &leftEyeTexture);
						error = g_pVRCompositor->Submit(vr::Eye_Right, &rightEyeTexture);
					}
					
					g_bRendering3D = false;
					if (g_bDumpSSAOBuffers) {
						/*
						if (colorFile != NULL) {
							fflush(colorFile);
							fclose(colorFile);
							colorFile = NULL;
						}

						if (lightFile != NULL) {
							fflush(lightFile);
							fclose(lightFile);
							lightFile = NULL;
						}
						*/

						g_bDumpSSAOBuffers = false;
					}

					//g_HyperspacePhaseFSM = HS_INIT_ST; // Resetting the hyperspace state when presenting a 2D image messes up the state
					// This is because the user can press [ESC] to display the menu while in hyperspace and that's a 2D present.
					// Present 2D
					if (FAILED(hr = this->_deviceResources->_swapChain->Present(g_iNaturalConcourseAnimations, 0)))
					{
						static bool messageShown = false;
						
						if (!messageShown)
						{
							MessageBox(nullptr, _com_error(hr).ErrorMessage(), __FUNCTION__, MB_ICONERROR);
						}

						messageShown = true;

						hr = DDERR_SURFACELOST;
						break;
					}

					if (g_bUseSteamVR) {
						if (g_bTogglePostPresentHandoff)
							g_pVRCompositor->PostPresentHandoff();
					}
				}

				// Clear the RTVs for the next frame
				{
					// We're about to switch from 3D to 2D rendering.
					// Let's clear the render target for the next iteration or we'll get multiple images during
					// the animation
					auto &context = this->_deviceResources->_d3dDeviceContext;
					float bgColor[4] = { 0, 0, 0, 0 };
					context->ClearRenderTargetView(resources->_renderTargetView, bgColor);
					if (g_bUseSteamVR) {
						context->ClearRenderTargetView(resources->_renderTargetViewR, bgColor);
						//context->ClearRenderTargetView(resources->_renderTargetViewSteamVRResize, bgColor);
					}
				}

			}
			else
			{
				hr = DD_OK;
			}

			if (this->_deviceResources->_frontbufferSurface != nullptr)
			{
				if (this->_deviceResources->_frontbufferSurface->wasBltFastCalled)
				{
					memcpy(this->_deviceResources->_frontbufferSurface->_buffer2, this->_deviceResources->_frontbufferSurface->_buffer, this->_deviceResources->_frontbufferSurface->_bufferSize);
				}

				this->_deviceResources->_frontbufferSurface->wasBltFastCalled = false;
			}

			return hr;
		}
	}
	/* Present 3D content */
	else
	{
		HRESULT hr;
		auto &resources = this->_deviceResources;
		auto &context = resources->_d3dDeviceContext;
		auto &device = resources->_d3dDevice;
		const bool bExternalCamera = PlayerDataTable[*g_playerIndex].externalCamera;
		const bool bCockpitDisplayed = PlayerDataTable[*g_playerIndex].cockpitDisplayed;
		const bool bMapOFF = PlayerDataTable[*g_playerIndex].mapState == 0;
		const int cameraObjIdx = PlayerDataTable[*g_playerIndex].cameraFG;
		int FlyByCameraTime = PlayerDataTable[*g_playerIndex].FlyByCameraTime;

		// This moves the external camera when in the hangar:
		//static __int16 yaw = 0;
		//PlayerDataTable[*g_playerIndex].cameraYaw   += 40 * *mouseLook_X;
		//PlayerDataTable[*g_playerIndex].cameraPitch += 15 * *mouseLook_Y;
		//yaw += 600;
		//log_debug("[DBG] roll: %0.3f", PlayerDataTable[*g_playerIndex].roll / 32768.0f * 180.0f);
		/*
		if (*g_playerInHangar) 
		{
			static int yaw = 0;
			yaw += 256;
			log_debug("[DBG] yaw: %0.3f", (float)yaw / 65536.0f);
			if (bExternalCamera) {
				PlayerDataTable[*g_playerIndex].cameraYaw = yaw;
			}
			else {
				PlayerDataTable[*g_playerIndex].cockpitCameraYaw = yaw;
			}

			if (yaw > 65535)
				yaw -= 65535;
		}
		*/

		if (this->_deviceResources->_swapChain)
		{
			hr = DD_OK;

			// Clear the DC RTVs -- we should've done this during Execute(); but if the GUI was disabled
			// then we didn't do it!
			if (!g_bDCWasClearedOnThisFrame) {
				float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				context->ClearRenderTargetView(resources->_renderTargetViewDynCockpit, bgColor);
				context->ClearRenderTargetView(resources->_renderTargetViewDynCockpitBG, bgColor);
				context->ClearRenderTargetView(resources->_DCTextRTV, bgColor);
				g_bDCWasClearedOnThisFrame = true;
				//log_debug("[DBG] DC Clearing RTVs because GUI was off");
			}

			if (g_bDumpSSAOBuffers) {
				DirectX::SaveWICTextureToFile(context, resources->_ReticleBufMSAA, GUID_ContainerFormatPng,
					L"C:\\Temp\\_reticleBufMSAA.png");
			}

			//this->RenderBracket(); // Don't render the bracket yet, wait until all the shading has been applied
			// We can render the enhanced radar and text now; because they will go to DCTextBuf -- not directly to the screen
			if (g_config.Radar2DRendererEnabled)
				this->RenderRadar();

			if (g_config.Text2DRendererEnabled) {
				/*
				short x = 250, y = 260;
				x = AddText("Hello ", 0, x, y, 0x5555FF);
				x = AddText("Weird ", 1, x, y, 0x5555FF);
				x = AddText("World ", 2, x, y, 0x5555FF);
				*/
				//AddCenteredText("Hello World", FONT_LARGE_IDX, 260, 0x5555FF);
				// The following text gets captured as part of the missile count DC element:
				//AddCenteredText("XXXXXXXXXXXXXXXXXXXXXXXX", FONT_LARGE_IDX, 17, 0x5555FF);
				this->RenderText();
			}

			// If we didn't render any GUI/HUD elements so far, then here's the place where we do some management
			// of the state that is normally done in Execute():
			// We capture the frame-so-far into shadertoyAuxBuf -- this is needed for the hyperspace effect.
			if (!g_bSwitchedToGUI) 
			{
				g_bSwitchedToGUI = true;
				// Capture the background/current-frame-so-far for the new hyperspace effect; but only if we're
				// not travelling through hyperspace:
				if (g_bHyperDebugMode || g_HyperspacePhaseFSM == HS_INIT_ST || g_HyperspacePhaseFSM == HS_POST_HYPER_EXIT_ST)
				{
					g_fLastCockpitCameraYaw   = PlayerDataTable[*g_playerIndex].cockpitCameraYaw;
					g_fLastCockpitCameraPitch = PlayerDataTable[*g_playerIndex].cockpitCameraPitch;
					g_lastCockpitXReference	  = PlayerDataTable[*g_playerIndex].cockpitXReference;
					g_lastCockpitYReference   = PlayerDataTable[*g_playerIndex].cockpitYReference;
					g_lastCockpitZReference   = PlayerDataTable[*g_playerIndex].cockpitZReference;

					context->ResolveSubresource(resources->_shadertoyAuxBuf, 0, resources->_offscreenBuffer, 0, BACKBUFFER_FORMAT);
					if (g_bUseSteamVR)
						context->ResolveSubresource(resources->_shadertoyAuxBufR, 0, resources->_offscreenBufferR, 0, BACKBUFFER_FORMAT);
				}
			}

			// TODO: The g_bShadowMapEnable was added later to be able to toggle the shadows with a hotkey
			//	     Either remove the multiplicity of "enable" variables or get rid of the hotkey.
			g_ShadowMapping.bEnabled = g_bShadowMapEnable;
			g_ShadowMapVSCBuffer.sm_enabled = g_bShadowMapEnable;
			// Shadow Mapping is disabled when the we're in external view or traveling through hyperspace.
			// Maybe also disable it if the cockpit is hidden
			// Render the Shadow Map
			if (g_ShadowMapping.bEnabled && g_ShadowMapping.bUseShadowOBJ && 
				!bExternalCamera && bCockpitDisplayed && g_HyperspacePhaseFSM == HS_INIT_ST)
			{
				RenderShadowMapOBJ();

				// Restore the previous viewport, etc
				resources->InitViewport(&g_nonVRViewport);
				resources->InitVertexShader(resources->_vertexShader);
				//resources->InitPixelShader(lastPixelShader);
				// Restore the previous index and vertex buffers?
				context->OMSetRenderTargets(0, 0, resources->_depthStencilViewL.Get());
				resources->InitRasterizerState(resources->_rasterizerState);
			}
			else {
				// We need to tell the pixel shaders not to sample the shadow map if it wasn't rendered:
				g_ShadowMapVSCBuffer.sm_enabled = false;
				resources->InitPSConstantBufferShadowMap(resources->_shadowMappingPSConstantBuffer.GetAddressOf(), &g_ShadowMapVSCBuffer);
			}

			// Render the hyperspace effect if necessary
			{
				// Render the new hyperspace effect
				if (g_HyperspacePhaseFSM != HS_INIT_ST)
				{
					UINT vertexBufferStride = sizeof(D3DTLVERTEX), vertexBufferOffset = 0;
					// Preconditions: shadertoyAuxBuf has a copy of the offscreen buffer (the background, if applicable)
					//				  shadertoyBuf has a copy of the cockpit
					if (resources->_useMultisampling) {
						context->ResolveSubresource(resources->_shadertoyBuf, 0, resources->_shadertoyBufMSAA, 0, BACKBUFFER_FORMAT);
						if (g_bUseSteamVR)
							context->ResolveSubresource(resources->_shadertoyBufR, 0, resources->_shadertoyBufMSAA_R, 0, BACKBUFFER_FORMAT);
					}

					/*
					if (g_bDumpSSAOBuffers) {
						DirectX::SaveDDSTextureToFile(context, resources->_shadertoyBuf, L"C:\\Temp\\_shadertoyBuf.dds");
						DirectX::SaveDDSTextureToFile(context, resources->_shadertoyAuxBuf, L"C:\\Temp\\_shadertoyAuxBuf.dds");
					}
					*/

					// This is the right spot to render the post-hyper-exit effect: we've captured the current offscreenBuffer into
					// shadertoyAuxBuf and we've finished rendering the cockpit/foreground too.
					RenderHyperspaceEffect(&g_nonVRViewport, resources->_pixelShaderTexture, NULL, NULL, &vertexBufferStride, &vertexBufferOffset);
				}
			}

			// Resolve the Bloom, Normals and SSMask before the SSAO and Bloom effects.
			if (g_bReshadeEnabled) {
				// Resolve whatever is in the _offscreenBufferBloomMask into _offscreenBufferAsInputBloomMask, and
				// do the same for the right (SteamVR) image -- I'll worry about the details later.
				// _offscreenBufferAsInputReshade was previously resolved during Execute() -- right before any GUI is rendered
				context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMask, 0,
					resources->_offscreenBufferBloomMask, 0, BLOOM_BUFFER_FORMAT);
				if (g_bUseSteamVR)
					context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMaskR, 0,
						resources->_offscreenBufferBloomMaskR, 0, BLOOM_BUFFER_FORMAT);

				// Resolve the normals, ssaoMask and ssMask buffers if necessary
				// Notice how we don't care about the g_bDepthBufferResolved flag here, we just resolve and move on
				if (resources->_useMultisampling) {
					context->ResolveSubresource(resources->_normBuf, 0, resources->_normBufMSAA, 0, AO_DEPTH_BUFFER_FORMAT);
					context->ResolveSubresource(resources->_ssaoMask, 0, resources->_ssaoMaskMSAA, 0, AO_MASK_FORMAT);
					context->ResolveSubresource(resources->_ssMask, 0, resources->_ssMaskMSAA, 0, AO_MASK_FORMAT);
					if (g_bUseSteamVR) {
						context->ResolveSubresource(resources->_normBufR, 0, resources->_normBufMSAA_R, 0, AO_DEPTH_BUFFER_FORMAT);
						context->ResolveSubresource(resources->_ssaoMaskR, 0, resources->_ssaoMaskMSAA_R, 0, AO_MASK_FORMAT);
						context->ResolveSubresource(resources->_ssMaskR, 0, resources->_ssMaskMSAA_R, 0, AO_MASK_FORMAT);
					}
				} 
				// if MSAA is not enabled, then the RTVs already populated normBuf, ssaoMask and ssMask, 
				// so nothing to do here!
			}

			// AO must (?) be computed before the bloom shader -- or at least output to a different buffer
			// Render the Deferred, SSAO or SSDO passes
			if (g_bAOEnabled) {
				if (!g_bDepthBufferResolved) {
					// If the depth buffer wasn't resolved during the regular Execute() then resolve it here.
					// This may happen if there is no GUI (all HUD indicators were switched off) or the
					// external camera is active.
					context->ResolveSubresource(resources->_depthBufAsInput, 0, resources->_depthBuf, 0, AO_DEPTH_BUFFER_FORMAT);
					if (g_bUseSteamVR)
						context->ResolveSubresource(resources->_depthBufAsInputR, 0,
							resources->_depthBufR, 0, AO_DEPTH_BUFFER_FORMAT);
				}

#ifdef GENMIPMAPS
				context->GenerateMips(resources->_depthBufSRV);
				if (g_bUseSteamVR)
					context->GenerateMips(resources->_depthBufSRV_R);
#endif

				// We need to set the blend state properly for SSAO
				D3D11_BLEND_DESC blendDesc{};
				blendDesc.AlphaToCoverageEnable = FALSE;
				blendDesc.IndependentBlendEnable = FALSE;
				blendDesc.RenderTarget[0].BlendEnable = TRUE;
				blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
				hr = resources->InitBlendState(nullptr, &blendDesc);

				// Temporarily disable ZWrite: we won't need it to display SSAO
				D3D11_DEPTH_STENCIL_DESC desc;
				ComPtr<ID3D11DepthStencilState> depthState;
				desc.DepthEnable = FALSE;
				desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
				desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
				desc.StencilEnable = FALSE;
				resources->InitDepthStencilState(depthState, &desc);

				//resources->_d3dDevice->render
				//D3D11_SAMPLER_DESC oldSamplerDesc = this->_renderStates->GetSamplerDesc();
				D3D11_SAMPLER_DESC samplerDesc;
				samplerDesc.Filter = resources->_useAnisotropy ? D3D11_FILTER_ANISOTROPIC : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
				samplerDesc.MaxAnisotropy = resources->_useAnisotropy ? resources->GetMaxAnisotropy() : 1;
				samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
				samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
				samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
				samplerDesc.MipLODBias = 0.0f;
				samplerDesc.MinLOD = 0;
				samplerDesc.MaxLOD = FLT_MAX;
				samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
				samplerDesc.BorderColor[0] = 0.0f;
				samplerDesc.BorderColor[1] = 0.0f;
				samplerDesc.BorderColor[2] = 0.0f;
				samplerDesc.BorderColor[3] = 0.0f;
				ComPtr<ID3D11SamplerState> tempSampler;
				hr = resources->_d3dDevice->CreateSamplerState(&samplerDesc, &tempSampler);
				context->PSSetSamplers(1, 1, &tempSampler);

				if (g_bDumpSSAOBuffers) {
					DirectX::SaveDDSTextureToFile(context, resources->_normBuf, L"C:\\Temp\\_normBuf.dds");
					DirectX::SaveDDSTextureToFile(context, resources->_depthBufAsInput, L"C:\\Temp\\_depthBuf.dds");
					DirectX::SaveDDSTextureToFile(context, resources->_offscreenBufferAsInputBloomMask, L"C:\\Temp\\_bloomMask1.dds");
					DirectX::SaveWICTextureToFile(context, resources->_offscreenBuffer, GUID_ContainerFormatJpeg,
						L"C:\\Temp\\_offscreenBuf.jpg");
					DirectX::SaveDDSTextureToFile(context, resources->_ssaoMask, L"C:\\Temp\\_ssaoMask.dds");
					DirectX::SaveDDSTextureToFile(context, resources->_ssMask, L"C:\\Temp\\_ssMask.dds");
					log_debug("[DBG] [AO] Captured debug buffers");
				}

				// Input: depthBufAsInput (already resolved during Execute())
				// Output: normalsBuf
				//SmoothNormalsPass(1.0f);

				// Input: depthBuf, normBuf, randBuf
				// Output: _bloom1
				switch (g_SSAO_Type) {
					case SSO_AMBIENT:
						SSAOPass(g_fSSAOZoomFactor);
						// Resolve the bloom mask again: SSDO can modify this mask
						context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMask, 0,
							resources->_offscreenBufferBloomMask, 0, BLOOM_BUFFER_FORMAT);
						if (g_bUseSteamVR)
							context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMaskR, 0,
								resources->_offscreenBufferBloomMaskR, 0, BLOOM_BUFFER_FORMAT);
						break;
					case SSO_DIRECTIONAL:
					case SSO_BENT_NORMALS:
						SSDOPass(g_fSSAOZoomFactor, g_fSSAOZoomFactor2);
						// Resolve the bloom mask again: SSDO can modify this mask
						context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMask, 0,
							resources->_offscreenBufferBloomMask, 0, BLOOM_BUFFER_FORMAT);
						if (g_bUseSteamVR)
							context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMaskR, 0,
								resources->_offscreenBufferBloomMaskR, 0, BLOOM_BUFFER_FORMAT);
						break;
					case SSO_DEFERRED:
						DeferredPass();
						// Resolve the bloom mask again: the deferred pass can modify this mask
						context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMask, 0,
							resources->_offscreenBufferBloomMask, 0, BLOOM_BUFFER_FORMAT);
						if (g_bUseSteamVR)
							context->ResolveSubresource(resources->_offscreenBufferAsInputBloomMaskR, 0,
								resources->_offscreenBufferBloomMaskR, 0, BLOOM_BUFFER_FORMAT);
						break;
				}

				if (g_bDumpSSAOBuffers) {
					//DirectX::SaveWICTextureToFile(context, resources->_offscreenBuffer, GUID_ContainerFormatJpeg, L"C:\\Temp\\_offscreenBuffer.jpg");
					DirectX::SaveDDSTextureToFile(context, resources->_offscreenBufferAsInputBloomMask, L"C:\\Temp\\_bloomMask2.dds");
					//DirectX::SaveDDSTextureToFile(context, resources->_bentBuf, L"C:\\Temp\\_bentBuf.dds");
					DirectX::SaveWICTextureToFile(context, resources->_bentBuf, GUID_ContainerFormatJpeg, L"C:\\Temp\\_bentBuf.jpg");
					DirectX::SaveDDSTextureToFile(context, resources->_ssaoBuf, L"C:\\Temp\\_ssaoBuf.dds");
					//DirectX::SaveWICTextureToFile(context, resources->_ssaoBufR, GUID_ContainerFormatJpeg, L"C:\\Temp\\_ssaoBufR.jpg");
					DirectX::SaveDDSTextureToFile(context, resources->_ssaoBufR, L"C:\\Temp\\_ssaoBufR.dds");
					DirectX::SaveDDSTextureToFile(context, resources->_normBuf, L"C:\\Temp\\_normBuf.dds");
					if (g_ShadowMapping.bEnabled) {
						//context->CopyResource(resources->_shadowMapDebug, resources->_shadowMap);
						wchar_t wFileName[80];
						for (int i = 0; i < *s_XwaGlobalLightsCount; i++) {
							context->CopySubresourceRegion(resources->_shadowMapDebug, D3D11CalcSubresource(0, 0, 1), 0, 0, 0,
								resources->_shadowMapArray, D3D11CalcSubresource(0, i, 1), NULL);
							swprintf_s(wFileName, 80, L"c:\\Temp\\_shadowMap%d.dds", i);
							DirectX::SaveDDSTextureToFile(context, resources->_shadowMapDebug, wFileName);
						}
						
						/*
						context->CopyResource(resources->_shadowMapDebug, resources->_shadowMap);
						DirectX::SaveDDSTextureToFile(context, resources->_shadowMapDebug, L"c:\\Temp\\_shadowMap.dds");
						*/
					}
					//DirectX::SaveWICTextureToFile(context, resources->_shadertoyAuxBuf, GUID_ContainerFormatJpeg, L"C:\\Temp\\_shadertoyAuxBuf.jpg");
					//DirectX::SaveWICTextureToFile(context, resources->_shadertoyAuxBuf, GUID_ContainerFormatJpeg, L"C:\\Temp\\_shadertoyAuxBuf.jpg");
				}
			}

			// Render the sun flare (if applicable)
			if (g_bReshadeEnabled && g_bProceduralSuns && !g_b3DSunPresent && !g_b3DSkydomePresent &&
				g_ShadertoyBuffer.SunFlareCount > 0 && g_ShadertoyBuffer.flare_intensity > 0.01f)
			{
				// We need to set the blend state properly for Bloom, or else we might get
				// different results when brackets are rendered because they alter the 
				// blend state
				D3D11_BLEND_DESC blendDesc{};
				blendDesc.AlphaToCoverageEnable = FALSE;
				blendDesc.IndependentBlendEnable = FALSE;
				blendDesc.RenderTarget[0].BlendEnable = TRUE;
				blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
				hr = resources->InitBlendState(nullptr, &blendDesc);

				// Temporarily disable ZWrite: we won't need it for post-proc effects
				D3D11_DEPTH_STENCIL_DESC desc;
				ComPtr<ID3D11DepthStencilState> depthState;
				desc.DepthEnable = FALSE;
				desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
				desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
				desc.StencilEnable = FALSE;
				resources->InitDepthStencilState(depthState, &desc);

				RenderSunFlare();
			}

			//if (g_bDumpSSAOBuffers) {
				//DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferPost, GUID_ContainerFormatPng, L"C:\\Temp\\_offscreenBufferPost.png");
				//DirectX::SaveDDSTextureToFile(context, resources->_offscreenBufferPost, L"C:\\Temp\\_offscreenBufferPost.dds");
			//}

			// Apply FXAA
			if (g_config.FXAAEnabled)
			{
				// _offscreenBufferAsInputBloomMask is resolved earlier, before the SSAO pass because
				// SSAO uses that mask to prevent applying SSAO on bright areas

				// We need to set the blend state properly for Bloom, or else we might get
				// different results when brackets are rendered because they alter the 
				// blend state
				D3D11_BLEND_DESC blendDesc{};
				blendDesc.AlphaToCoverageEnable = FALSE;
				blendDesc.IndependentBlendEnable = FALSE;
				blendDesc.RenderTarget[0].BlendEnable = TRUE;
				blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
				hr = resources->InitBlendState(nullptr, &blendDesc);

				// Temporarily disable ZWrite: we won't need it to display Bloom
				D3D11_DEPTH_STENCIL_DESC desc;
				ComPtr<ID3D11DepthStencilState> depthState;
				desc.DepthEnable = FALSE;
				desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
				desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
				desc.StencilEnable = FALSE;
				resources->InitDepthStencilState(depthState, &desc);

				//UINT vertexBufferStride = sizeof(D3DTLVERTEX), vertexBufferOffset = 0;
				RenderFXAA();
			}

			// Render the enhanced bracket after all the shading has been applied.
			if (g_config.Radar2DRendererEnabled && !g_bEnableVR)
				this->RenderBracket();

			// Draw the reticle on top of everything else
			if (g_bExternalHUDEnabled || g_bEnableVR)
			{
				// We need to set the blend state properly for Bloom, or else we might get
				// different results when brackets are rendered because they alter the 
				// blend state
				D3D11_BLEND_DESC blendDesc{};
				blendDesc.AlphaToCoverageEnable = FALSE;
				blendDesc.IndependentBlendEnable = FALSE;
				blendDesc.RenderTarget[0].BlendEnable = TRUE;
				blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
				hr = resources->InitBlendState(nullptr, &blendDesc);

				// Temporarily disable ZWrite: we won't need it to display Bloom
				D3D11_DEPTH_STENCIL_DESC desc;
				ComPtr<ID3D11DepthStencilState> depthState;
				desc.DepthEnable = FALSE;
				desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
				desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
				desc.StencilEnable = FALSE;
				resources->InitDepthStencilState(depthState, &desc);

				RenderExternalHUD();
			}

			if (g_bStarDebugEnabled)
			{
				// We need to set the blend state properly for Bloom, or else we might get
				// different results when brackets are rendered because they alter the 
				// blend state
				D3D11_BLEND_DESC blendDesc{};
				blendDesc.AlphaToCoverageEnable = FALSE;
				blendDesc.IndependentBlendEnable = FALSE;
				blendDesc.RenderTarget[0].BlendEnable = TRUE;
				blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
				hr = resources->InitBlendState(nullptr, &blendDesc);

				// Temporarily disable ZWrite: we won't need it to display Bloom
				D3D11_DEPTH_STENCIL_DESC desc;
				ComPtr<ID3D11DepthStencilState> depthState;
				desc.DepthEnable = FALSE;
				desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
				desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
				desc.StencilEnable = FALSE;
				resources->InitDepthStencilState(depthState, &desc);

				RenderStarDebug();
			}

			// Apply the speed shader
			// Adding g_bCustomFOVApplied to condition below prevents this effect from getting rendered 
			// on the first frame (sometimes it can happen and it's quite visible/ugly)
			if (g_bCustomFOVApplied && g_bEnableSpeedShader && !*g_playerInHangar && bMapOFF &&
				(!bExternalCamera || (bExternalCamera && *g_playerIndex == cameraObjIdx)) &&
				(g_HyperspacePhaseFSM == HS_INIT_ST || g_HyperspacePhaseFSM == HS_POST_HYPER_EXIT_ST) &&
				FlyByCameraTime == 0)
			{
				// We need to set the blend state properly for Bloom, or else we might get
				// different results when brackets are rendered because they alter the 
				// blend state
				D3D11_BLEND_DESC blendDesc{};
				blendDesc.AlphaToCoverageEnable = FALSE;
				blendDesc.IndependentBlendEnable = FALSE;
				blendDesc.RenderTarget[0].BlendEnable = TRUE;
				blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
				hr = resources->InitBlendState(nullptr, &blendDesc);

				// Temporarily disable ZWrite: we won't need it to display Bloom
				D3D11_DEPTH_STENCIL_DESC desc;
				ComPtr<ID3D11DepthStencilState> depthState;
				desc.DepthEnable = FALSE;
				desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
				desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
				desc.StencilEnable = FALSE;
				resources->InitDepthStencilState(depthState, &desc);

				context->ResolveSubresource(resources->_offscreenBufferAsInput, 0, resources->_offscreenBuffer, 0, BACKBUFFER_FORMAT);
				if (g_bUseSteamVR)
					context->ResolveSubresource(resources->_offscreenBufferAsInputR, 0, resources->_offscreenBufferR, 0, BACKBUFFER_FORMAT);
				RenderSpeedEffect();
			}

			// Render the additional geometry
			// Adding g_bCustomFOVApplied to condition below prevents this effect from getting rendered 
			// on the first frame (sometimes it can happen and it's quite visible/ugly
			if (g_bCustomFOVApplied && g_bEnableAdditionalGeometry)
			{
				// We need to set the blend state properly for Bloom, or else we might get
				// different results when brackets are rendered because they alter the 
				// blend state
				D3D11_BLEND_DESC blendDesc{};
				blendDesc.AlphaToCoverageEnable = FALSE;
				blendDesc.IndependentBlendEnable = FALSE;
				blendDesc.RenderTarget[0].BlendEnable = TRUE;
				blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
				hr = resources->InitBlendState(nullptr, &blendDesc);

				// Temporarily disable ZWrite: we won't need it to display Bloom
				D3D11_DEPTH_STENCIL_DESC desc;
				ComPtr<ID3D11DepthStencilState> depthState;
				desc.DepthEnable = FALSE;
				desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
				desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
				desc.StencilEnable = FALSE;
				resources->InitDepthStencilState(depthState, &desc);

				RenderAdditionalGeometry();
			}

			// Apply the Bloom effect
			if (g_bBloomEnabled) {
				// _offscreenBufferAsInputBloomMask is resolved earlier, before the SSAO pass because
				// SSAO uses that mask to prevent applying SSAO on bright areas

				// We need to set the blend state properly for Bloom, or else we might get
				// different results when brackets are rendered because they alter the 
				// blend state
				D3D11_BLEND_DESC blendDesc{};
				blendDesc.AlphaToCoverageEnable = FALSE;
				blendDesc.IndependentBlendEnable = FALSE;
				blendDesc.RenderTarget[0].BlendEnable = TRUE;
				blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
				blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
				blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
				blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
				hr = resources->InitBlendState(nullptr, &blendDesc);

				// Temporarily disable ZWrite: we won't need it to display Bloom
				D3D11_DEPTH_STENCIL_DESC desc;
				ComPtr<ID3D11DepthStencilState> depthState;
				desc.DepthEnable = FALSE;
				desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
				desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
				desc.StencilEnable = FALSE;
				resources->InitDepthStencilState(depthState, &desc);

				//int HyperspacePhase = PlayerDataTable[*g_playerIndex].hyperspacePhase;
				//bool bHyperStreaks = (HyperspacePhase == 2) || (HyperspacePhase == 3);
				bool bHyperStreaks = g_HyperspacePhaseFSM == HS_HYPER_ENTER_ST ||
					g_HyperspacePhaseFSM == HS_HYPER_EXIT_ST ||
					g_HyperspacePhaseFSM == HS_POST_HYPER_EXIT_ST;

				// DEBUG
				//static int CaptureCounter = 1;
				// DEBUG

				// Initialize the accummulator buffer
				float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				context->ClearRenderTargetView(resources->_renderTargetViewBloomSum, bgColor);
				if (g_bUseSteamVR)
					context->ClearRenderTargetView(resources->_renderTargetViewBloomSumR, bgColor);

				// DEBUG
				/* if (g_iPresentCounter == 100 || g_bDumpBloomBuffers) {
					wchar_t filename[80];

					swprintf_s(filename, 80, L".\\_offscreenBufferBloomMask-%d.jpg", CaptureCounter);
					DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferAsInputBloomMask, GUID_ContainerFormatJpeg, filename);
					swprintf_s(filename, 80, L".\\_offscreenBufferBloomMask-%d.dds", CaptureCounter);
					DirectX::SaveDDSTextureToFile(context, resources->_offscreenBufferAsInputBloomMask, filename);


					swprintf_s(filename, 80, L".\\_offscreenBuffer-%d.jpg", CaptureCounter);
					DirectX::SaveWICTextureToFile(context, resources->_offscreenBuffer, GUID_ContainerFormatJpeg, filename);
					swprintf_s(filename, 80, L".\\_offscreenBuffer-%d.dds", CaptureCounter);
					DirectX::SaveDDSTextureToFile(context, resources->_offscreenBuffer, filename);
				} */
				// DEBUG

				{
					float fScale = 2.0f;
					for (int i = 1; i <= g_BloomConfig.iNumPasses; i++) {
						int AdditionalPasses = g_iBloomPasses[i] - 1;
						// Zoom level 2.0f with only one pass tends to show artifacts unless
						// the spread is set to 1
						BloomPyramidLevelPass(i, AdditionalPasses, fScale);
						fScale *= 2.0f;
					}
				}

				//if (!g_bAOEnabled) {
					// TODO: Check the statement below, I'm not sure it's current anymore (?)
					// If SSAO is not enabled, then we can merge the bloom buffer with the offscreen buffer
					// here. Otherwise, we'll merge it along with the SSAO buffer later.
					// Add the accumulated bloom with the offscreen buffer
					// Input: _bloomSum, _offscreenBufferAsInput
					// Output: _offscreenBuffer
					BloomBasicPass(5, 1.0f);
				//}

				// DEBUG
				/*if (g_iPresentCounter == 100 || g_bDumpBloomBuffers) {
					wchar_t filename[80];

					swprintf_s(filename, 80, L".\\_bloomMask-Final-%d.jpg", CaptureCounter);
					DirectX::SaveWICTextureToFile(context, resources->_bloomOutput1, GUID_ContainerFormatJpeg, filename);
					swprintf_s(filename, 80, L".\\_bloomMask-Final-%d.dds", CaptureCounter);
					DirectX::SaveDDSTextureToFile(context, resources->_bloomOutput1, filename);

					swprintf_s(filename, 80, L".\\_offscreenBuffer-Final-%d.jpg", CaptureCounter);
					DirectX::SaveWICTextureToFile(context, resources->_offscreenBuffer, GUID_ContainerFormatJpeg, filename);
					swprintf_s(filename, 80, L".\\_offscreenBuffer-Final-%d.dds", CaptureCounter);
					DirectX::SaveDDSTextureToFile(context, resources->_offscreenBuffer, filename);

					CaptureCounter++;
					g_bDumpBloomBuffers = false;
				}*/
				// DEBUG
			}

			// Draw the HUD
			/*
			if ((g_bDCManualActivate || bExteriorCamera) && (g_bDynCockpitEnabled || g_bReshadeEnabled) && 
				g_iHUDOffscreenCommandsRendered && resources->_bHUDVerticesReady) 
			*/
			// For some reason we have to call this or the ESC menu will disappear...
			/*
			 * If the new Text renderer and Bloom are both enabled, then the following block will cause the landing
			 * sequence to freeze _and I don't know why!_, I suspect the ResolveSubResource from the _offscreenBuffer
			 * to the _backBuffer isn't working properly; but I don't see why that should be the case. So, instead of
			 * fixing that, I'm disabling this block when the player is in the hangar, the exterior camera is enabled,
			 * and the new 2D renderer is enabled.
			 */
			//if (!(*g_playerInHangar && bExteriorCamera))
			//if (true)
			if (!(*g_playerInHangar && bExternalCamera && g_config.Text2DRendererEnabled)) 
			{
				// If we're not in external view, then clear everything we don't want to display from the HUD
				if (g_bDynCockpitEnabled && g_bDCApplyEraseRegionCommands && !bExternalCamera)
					ClearHUDRegions();

				// DTM's Yavin map exposed a weird bug when the next if() is enabled: if the XwingCockpit.dc file
				// doesn't match the XwingCockpit.opt, then we'll erase all HUD regions; but we will not render
				// any DC elements. Pressing ESC to see the menu while flying only shows a black screen. The black
				// screen goes away if we remove the next if:
				//if (num_regions_erased < MAX_DC_REGIONS - 1)
				// I don't know why; but if we call DrawHUDVertices, or if we enable AC, the problem goes away.
				// Looks like we need the state set by DrawHUDVertices() after all...
				// ... also: we need to call DrawHUDVertices to move the HUD regions anyway (but only if there
				// are any HUD regions that weren't erased!)
				// So, we're going to call DrawHUDVertices to set the state; but skip the Draw() calls if all
				// HUD regions were erased
				//DrawHUDVertices(num_regions_erased < MAX_DC_REGIONS - 1);

				DrawHUDVertices();
			}

			// I should probably render the laser pointer before the HUD; but if I do that, then the
			// HUD gets messed up. I guess I'm destroying the state somehow
			// Render the Laser Pointer for VR
			if (g_bActiveCockpitEnabled && g_bRendering3D && !PlayerDataTable[*g_playerIndex].externalCamera)
			{
				UINT vertexBufferStride = sizeof(D3DTLVERTEX), vertexBufferOffset = 0;
				RenderLaserPointer(&g_nonVRViewport, resources->_pixelShaderTexture, NULL, NULL, &vertexBufferStride, &vertexBufferOffset);
			}
			
			if (g_bDumpLaserPointerDebugInfo)
				g_bDumpLaserPointerDebugInfo = false;

			// In the original code, the offscreenBuffer is resolved to the backBuffer
			//this->_deviceResources->_d3dDeviceContext->ResolveSubresource(this->_deviceResources->_backBuffer, 0, this->_deviceResources->_offscreenBuffer, 0, BACKBUFFER_FORMAT);

			if (g_bDumpSSAOBuffers && !g_bAOEnabled) {
				log_debug("[DBG] SSAO Disabled. Dumping buffers");
				// If SSAO is OFF we would still like to dump some information.
				DirectX::SaveWICTextureToFile(context, resources->_offscreenBuffer, GUID_ContainerFormatJpeg,
					L"C:\\Temp\\_offscreenBuf.jpg");
				DirectX::SaveDDSTextureToFile(context, resources->_offscreenBufferAsInputBloomMask, L"C:\\Temp\\_bloomMask1.dds");
				if (g_bUseSteamVR) {
					DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferR, GUID_ContainerFormatJpeg,
						L"C:\\Temp\\_offscreenBufR.jpg");
					DirectX::SaveDDSTextureToFile(context, resources->_offscreenBufferAsInputBloomMaskR, L"C:\\Temp\\_bloomMask1R.dds");
				}
			}

			// Final _offscreenBuffer --> _backBuffer copy. The offscreenBuffer SHOULD CONTAIN the fully-rendered image at this point.
			if (g_bEnableVR) {
				if (g_bUseSteamVR) {
					/*
					// The new Metric VR reconstruction shouldn't need any barrel effect distortion 
					if (!g_bDisableBarrelEffect) {
						// Do the barrel effect (_offscreenBuffer -> _offscreenBufferPost)
						barrelEffectSteamVR();
						context->CopyResource(resources->_offscreenBuffer, resources->_offscreenBufferPost);
						context->CopyResource(resources->_offscreenBufferR, resources->_offscreenBufferPostR);
					}
					*/
					// Resize the buffer to be presented (_offscreenBuffer -> _steamVRPresentBuffer)
					// TODO: Compensate for the difference in aspect ratio between SteamVR's window
					//       and the actual window size on the screen.
					resizeForSteamVR(0, false);
					// Resolve steamVRPresentBuffer to backBuffer so that it gets presented on the screen
					context->ResolveSubresource(resources->_backBuffer, 0, resources->_steamVRPresentBuffer, 0, BACKBUFFER_FORMAT);
				} 
				else { // Direct SBS mode
					if (!g_bDisableBarrelEffect) {
						barrelEffect3D();
						context->ResolveSubresource(resources->_backBuffer, 0, resources->_offscreenBufferPost, 0, BACKBUFFER_FORMAT);
					} else
						context->ResolveSubresource(resources->_backBuffer, 0, resources->_offscreenBuffer, 0, BACKBUFFER_FORMAT);
				}
			}
			else { // Non-VR mode
				// D3D11CalcSubresource(0, 0, 1),
				context->ResolveSubresource(resources->_backBuffer, 0, resources->_offscreenBuffer, 0, BACKBUFFER_FORMAT);
				//context->CopyResource(resources->_offscreenBufferPost, resources->_offscreenBuffer);
				//context->ResolveSubresource(resources->_backBuffer, 0, resources->_offscreenBufferPost, 0, BACKBUFFER_FORMAT);
				//log_debug("[DBG] (%d) offscreenBuffer --> backBuffer", g_iPresentCounter);
				if (g_bDumpSSAOBuffers)
					DirectX::SaveDDSTextureToFile(context, resources->_backBuffer, L"C:\\Temp\\_backBuffer.dds");
			}

			if (g_bDumpSSAOBuffers) 
			{
				// These values match MXvTED exactly:
				//short *POV_Y0 = (short *)(0x5BB480 + 0x238);
				//short *POV_Z0 = (short *)(0x5BB480 + 0x23A);
				//short *POV_X0 = (short *)(0x5BB480 + 0x23C);

				//*POV_X0 = 5; // This had no obvious effect

				// These are almost the same values as the above, as floats:
				// The difference is that Y has the sign inverted
				/*float *POV_X1 = (float *)(0x8B94E0 + 0x20D);
				float *POV_Y1 = (float *)(0x8B94E0 + 0x211);
				float *POV_Z1 = (float *)(0x8B94E0 + 0x215);*/

				//*POV_X1 = 5.0f; // This had no obvious effect

				// This is the same POV as the above; but Y is now -Y and it has been
				// transformed by the camera heading. So, this value changes depending
				// on the current orientation of the craft.
				// That's what Jeremy meant by "transform matrix in the MobileObject struct
				// (offset 0xC7)" <-- That's what I call the "Heading Matrix".
				// This transform matrix is *not* affected by the cockpit camera. It's just
				// the current heading.
				/*float *POV_X2 = (float *)(0x8B94E0 + 0x201);
				float *POV_Y2 = (float *)(0x8B94E0 + 0x205);
				float *POV_Z2 = (float *)(0x8B94E0 + 0x209);*/

				//log_debug("[DBG] [SHW] [POV] X0,Z0,Y0: %d, %d, %d", *g_POV_X0, *g_POV_Z0, *g_POV_Y0);
				//log_debug("[DBG] [SHW] [POV] X1,Z1,Y1: %f, %f, %f", *g_POV_X, *g_POV_Z, *g_POV_Y);
				//log_debug("[DBG] [POV] X2,Z2,Y2: %f, %f, %f", *POV_X2, *POV_Z2, *POV_Y2);

				/*
				DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferDynCockpit, GUID_ContainerFormatJpeg, L"c:\\temp\\_DC-FG-2.jpg");
				DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferDynCockpitBG, GUID_ContainerFormatJpeg, L"c:\\temp\\_DC-BG-2.jpg");
				DirectX::SaveWICTextureToFile(context, resources->_DCTextMSAA, GUID_ContainerFormatJpeg, L"c:\\temp\\_DC-Text-2.jpg");
				*/
				//log_debug("[DBG] g_playerIndex: %d, objectIndex: %d, currentTarget: %d",
				//	*g_playerIndex, PlayerDataTable[*g_playerIndex].objectIndex, PlayerDataTable[*g_playerIndex].currentTargetIndex);
				//log_debug("[DBG] mapState: %d", PlayerDataTable[*g_playerIndex].mapState);
				// Read the int value at offset 0B54 in the player struct and compare it with the player object index
				//int *pEntry = (int *)(&PlayerDataTable[*g_playerIndex] + 0x0B54);
				//log_debug("[DBG] Focus idx: %d", PlayerDataTable[*g_playerIndex].cameraFG);
			}

			// RESET CONTROL VARS, FRAME COUNTERS, CLEAR VECTORS, ETC
			{
				g_iDrawCounter = 0; // g_iExecBufCounter = 0;
				g_iNonZBufferCounter = 0; g_iDrawCounterAfterHUD = -1;
				g_iFloatingGUIDrawnCounter = 0;
				g_bTargetCompDrawn = false;
				g_bPrevIsFloatingGUI3DObject = false;
				g_bIsFloating3DObject = false;
				g_bStartedGUI = false;
				g_bPrevStartedGUI = false;
				g_bIsScaleableGUIElem = false;
				g_bPrevIsScaleableGUIElem = false;
				g_bScaleableHUDStarted = false;
				g_bIsTrianglePointer = false;
				g_bLastTrianglePointer = false;
				g_iHUDOffscreenCommandsRendered = 0;
				g_bSkyBoxJustFinished = false;
				g_bPrevIsPlayerObject = false;
				g_bIsPlayerObject = false;
				g_bLastFrameWasExterior = PlayerDataTable[*g_playerIndex].externalCamera;
				// Disable the Dynamic Cockpit whenever we're in external camera mode:
				g_bDCManualActivate = !g_bLastFrameWasExterior;
				g_bDepthBufferResolved = false;
				g_bHyperspaceEffectRenderedOnCurrentFrame = false;
				g_bSwitchedToGUI = false;
				g_ShadertoyBuffer.SunFlareCount = 0;
				g_bPrevIsTargetHighlighted = g_bIsTargetHighlighted;
				g_bIsTargetHighlighted = false;
				g_bExecuteBufferLock = false;
				g_bDCWasClearedOnThisFrame = false;
				g_bEdgeEffectApplied = false;
				g_TriangleCentroid.x = g_TriangleCentroid.y = -1.0f;
				g_iNumSunCentroids = 0; // Reset the number of sun centroids seen in this frame

				// Reset the frame counter if we just exited the hangar
				if (!(*g_playerInHangar) && g_bPrevPlayerInHangar) {
					g_iPresentCounter = 0;
					log_debug("[DBG] EXITED HANGAR");
				}
				g_bPrevPlayerInHangar = *g_playerInHangar;
				// Force recomputation of y_center at the beginning of each match, or after exiting the hangar
				if (g_iPresentCounter == 5)
					g_bYCenterHasBeenFixed = false;

				if (g_bTriggerReticleCapture) {
					//DisplayBox("Reticle Limits: ", g_ReticleCenterLimits);
					log_debug("Reticle Centroid: %0.3f, %0.3f", g_ReticleCentroid.x, g_ReticleCentroid.y);
					g_bTriggerReticleCapture = false;
				}
				
				// Increase the post-hyperspace-exit frames; but only when we're in the right state:
				if (g_HyperspacePhaseFSM == HS_POST_HYPER_EXIT_ST)
					g_iHyperExitPostFrames++;
				else
					g_iHyperExitPostFrames = 0;
				// If we just rendered the first hyperspace frame, then it's safe to clear the aux buffer:
				if (g_bHyperspaceFirstFrame && !g_bClearedAuxBuffer) {
					float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
					g_bClearedAuxBuffer = true;
					context->ClearRenderTargetView(resources->_renderTargetViewPost, bgColor);
					context->ResolveSubresource(resources->_shadertoyAuxBuf, 0, resources->_offscreenBufferPost, 0, BACKBUFFER_FORMAT);
					if (g_bUseSteamVR)
						context->CopyResource(resources->_shadertoyAuxBufR, resources->_shadertoyAuxBuf);
				}
				g_bHyperspaceFirstFrame = false;
				g_bHyperHeadSnapped = false;
				//*g_playerInHangar = 0;
				if (g_bDumpSSAOBuffers)
					g_bDumpSSAOBuffers = false;

				// Reset the laser pointer intersection
				if (g_bActiveCockpitEnabled) {
					g_LaserPointerBuffer.bIntersection = 0;
					g_fBestIntersectionDistance = 10000.0f;
					g_iBestIntersTexIdx = -1;
				}

				// Clear the laser list for the next frame
				if (g_bEnableLaserLights) {
					g_LaserList.clear();
					// If the headlights are ON, let's add one light right away near the cockpit's center:
					//if (g_bEnableHeadLights)
					//	g_LaserList.insert(g_HeadLightsPosition, g_HeadLightsColor);
				}
			}

			// Apply the custom FOV
			// I tried applying these settings on DLL load, and on the first draw call in Execute(); but they didn't work.
			// Apparently I have to wait until the first frame is fully executed in order to apply the custom FOV
			if (!g_bCustomFOVApplied) {
				log_debug("[DBG] [FOV] [Flip] Applying Custom FOV.");

				switch (g_CurrentFOVType) {
				case GLOBAL_FOV:
					// Loads Focal_Length.cfg and applies the FOV using ApplyFocalLength()
					log_debug("[DBG] [FOV] [Flip] GLOBAL_FOV Loading Focal_Length.cfg");
					if (!LoadFocalLength()) {
						if (g_bEnableVR) {
							// We couldn't load the custom FOV and we're running in VR mode, let's apply
							// the current VR FOV...
							ApplyFocalLength(RealVertFOVToRawFocalLength(g_fVR_FOV));
							// ... and save it
							SaveFocalLength();
						}
						else {
							// We couldn't load the focal length and we're not running in VR mode.
							// We still need to compute the FOVscale and y_center:
							ComputeHyperFOVParams();
						}
					}
					// else: LoadFocalLength calls ApplyFocalLength, which in turn calls ComputeHyperFOVParams, which is
					// where we compute y_center and FOVscale, so we're good.
					break;
				case XWAHACKER_FOV:
					// If the current ship's DC file has a focal length, apply it:
					if (g_fCurrentShipFocalLength > 0.0f) {
						log_debug("[DBG] [FOV] [Flip] XWAHACKER_FOV Applying DC REGULAR Focal Length: %0.3f", g_fCurrentShipFocalLength);
						ApplyFocalLength(g_fCurrentShipFocalLength);
					}
					else {
						// Loads Focal_Length.cfg and applies the FOV using ApplyFocalLength()
						log_debug("[DBG] [FOV] [Flip] XWAHACKER_FOV Loading Focal_Length.cfg");
						LoadFocalLength();
					}
					break;
				case XWAHACKER_LARGE_FOV:
					// If the current ship's DC file has a focal length, apply it:
					if (g_fCurrentShipLargeFocalLength > 0.0f) {
						log_debug("[DBG] [FOV] [Flip] XWAHACKER_LARGE_FOV Applying DC LARGE Focal Length: %0.3f", g_fCurrentShipLargeFocalLength);
						ApplyFocalLength(g_fCurrentShipLargeFocalLength);
					}
					else {
						// Loads Focal_Length.cfg and applies the FOV using ApplyFocalLength()
						log_debug("[DBG] [FOV] [Flip] XWAHACKER_LARGE_FOV Loading Focal_Length.cfg");
						LoadFocalLength();
					}
					break;
				} // switch

				g_bCustomFOVApplied = true; // Becomes false in OnSizeChanged()
				ComputeHyperFOVParams();
			}

			// Reapply the MetricRec constants if necessary
			if (g_bMetricParamsNeedReapply) {
				// This probably is a good place to set the VS constant buffer:
				resources->InitVSConstantBufferMetricRec(resources->_metricRecVSConstantBuffer.GetAddressOf(), &g_MetricRecCBuffer);
				// TODO: Do we need this CB in the pixel shaders? Maybe if we replace the Shadertoy CB settings...
				resources->InitPSConstantBufferMetricRec(resources->_metricRecPSConstantBuffer.GetAddressOf(), &g_MetricRecCBuffer);
				log_debug("[DBG] Reapplied MetricRec CBs");
				g_bMetricParamsNeedReapply = false;
			}
			// Reset the Reticle Centroid *after* it has been used by ComputeHyperFOVParams
			g_ReticleCentroid.set(-1.0f, -1.0f);

//#define HYPER_OVERRIDE 1
//#ifdef HYPER_OVERRIDE
			//g_fHyperTimeOverride += 0.025f;
			if (g_bHyperDebugMode) {
				g_fHyperTimeOverride = 1.0f;
				if (g_fHyperTimeOverride > 2.0f) // Use 2.0 for entry, 4.0 for tunnel, 2.0 for exit, 1.5 for post-hyper-exit
					g_fHyperTimeOverride = 0.0f;
			}
			/*
			// Update the state
			if (g_fHyperTimeOverride >= 2.0f)
				g_iHyperStateOverride = 3; // HYPER_EXIT
			else
				g_iHyperStateOverride = 4; // POST_HYPER_EXIT
			*/
//#endif

			// Resolve the Dynamic Cockpit FG, BG and Text buffers.
			// This step has to be done here, after we clear the HUD regions, or we'll erase
			// the contents of these buffers for the next frame
			if (g_bDynCockpitEnabled || g_bReshadeEnabled || g_config.Text2DRendererEnabled) 
			{
				context->ResolveSubresource(_deviceResources->_offscreenAsInputDynCockpit,
					0, _deviceResources->_offscreenBufferDynCockpit, 0, BACKBUFFER_FORMAT);
				context->ResolveSubresource(_deviceResources->_offscreenAsInputDynCockpitBG,
					0, _deviceResources->_offscreenBufferDynCockpitBG, 0, BACKBUFFER_FORMAT);
				context->ResolveSubresource(_deviceResources->_DCTextAsInput,
					0, _deviceResources->_DCTextMSAA, 0, BACKBUFFER_FORMAT);

				/*
				// Should this block be here? This fixes the HUD not getting cleared when the new 2D Renderer
				// Is enabled; but it breaks when it isn't
				float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				context->ClearRenderTargetView(resources->_renderTargetViewDynCockpit, bgColor);
				context->ClearRenderTargetView(resources->_renderTargetViewDynCockpitBG, bgColor);
				context->ClearRenderTargetView(resources->_DCTextRTV, bgColor);
				*/
				//log_debug("[DBG] Resolve DC RTVs.");

				/*static bool bDump = true;
				if (g_iPresentCounter == 100 && bDump) {
					DirectX::SaveWICTextureToFile(context, resources->_offscreenAsInputDynCockpit, GUID_ContainerFormatPng,
						L"C:\\Temp\\_DC-FG.png");
					DirectX::SaveWICTextureToFile(context, resources->_offscreenAsInputDynCockpitBG, GUID_ContainerFormatPng,
						L"C:\\Temp\\_DC-BG.png");
					bDump = false;
				}*/
			}

			/*
			// Clear the Shadow Map buffers
			if (bHyperspaceFirstFrame && g_ShadowMapping.Enabled) {
				context->ClearDepthStencilView(resources->_shadowMapDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
			}
			*/

			//CalculateViewMatrix();

#ifdef DBG_VR
			if (g_bStart3DCapture && !g_bDo3DCapture) {
				g_bDo3DCapture = true;
			}
			else if (g_bStart3DCapture && g_bDo3DCapture) {
				g_bDo3DCapture = false;
				g_bStart3DCapture = false;
				fclose(g_HackFile);
				g_HackFile = NULL;
			}
#endif

			// Submit images to SteamVR
			if (g_bUseSteamVR) {
				//if (!g_pHMD->GetTimeSinceLastVsync(&seconds, &frame))
				//	log_debug("[DBG] No Vsync info available");
				vr::EVRCompositorError error = vr::VRCompositorError_None;
				vr::Texture_t leftEyeTexture;
				vr::Texture_t rightEyeTexture;

				leftEyeTexture = { this->_deviceResources->_offscreenBuffer.Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto };
				rightEyeTexture = { this->_deviceResources->_offscreenBufferR.Get(), vr::TextureType_DirectX, vr::ColorSpace_Auto };

				//log_debug("[DBG] Submit 3D");
				error = g_pVRCompositor->Submit(vr::Eye_Left, &leftEyeTexture);
				error = g_pVRCompositor->Submit(vr::Eye_Right, &rightEyeTexture);
			}

			// We're about to switch to 3D rendering, update the hyperspace FSM if necessary
			if (!g_bRendering3D) {
				// We were presenting 2D content and now we're about to show 3D content. If we were in
				// hyperspace, we might need to reset the hyperspace FSM and restore any settings we
				// changed previously
				if (g_HyperspacePhaseFSM != HS_INIT_ST && PlayerDataTable[*g_playerIndex].hyperspacePhase == 0) {
					if (g_HyperspacePhaseFSM == HS_HYPER_TUNNEL_ST) {
						// Restore the previous color of the lights
						for (int i = 0; i < 2; i++) {
							memcpy(&g_LightVector[i], &g_TempLightVector[i], sizeof(Vector4));
							memcpy(&g_LightColor[i], &g_TempLightColor[i], sizeof(Vector4));
						}
					}
					g_bHyperspaceLastFrame = (g_HyperspacePhaseFSM == HS_HYPER_EXIT_ST);
					g_HyperspacePhaseFSM = HS_INIT_ST;
				}
			}
			// Make sure the hyperspace effect is off if we're back in the hangar. This is necessary to fix
			// the Holdo bug (but more changes may be needed).
			if (*g_playerInHangar)
				g_HyperspacePhaseFSM = HS_INIT_ST;
			// We're about to show 3D content, so let's set the corresponding flag
			g_bRendering3D = true;
			// Doing Present(1, 0) limits the framerate to 30fps, without it, it can go up to 60; but usually
			// stays around 45 in my system
			g_iPresentCounter++;
			//static bool bPrevPlayerInHangar = false;
			//if (bPrevPlayerInHangar && !*g_playerInHangar) {
			//	// We just exited the hanger, let's reset the present counter
			//	g_iPresentCounter = 0;
			//	log_debug("[DBG] Exited Hangar, resetting g_iPresentCounter and HUD regions");
			//}
			//bPrevPlayerInHangar = *g_playerInHangar;

			// This is Jeremy's code:
			//if (FAILED(hr = this->_deviceResources->_swapChain->Present(g_config.VSyncEnabled ? 1 : 0, 0)))
			// For VR, we probably want to disable VSync to get as much frames a possible:
			//if (FAILED(hr = this->_deviceResources->_swapChain->Present(0, 0)))
			bool bEnableVSync = g_config.VSyncEnabled;
			if (*g_playerInHangar)
				bEnableVSync = g_config.VSyncEnabledInHangar;
			// If SteamVR is on, we do NOT want to do VSync with the monitor as well: that'll kill the performance.
			if (g_bUseSteamVR)
				bEnableVSync = false;
			//log_debug("[DBG] ******************* PRESENT 3D");
			if (FAILED(hr = this->_deviceResources->_swapChain->Present(bEnableVSync ? 1 : 0, 0)))
			{
				static bool messageShown = false;
				log_debug("[DBG] Present 3D failed, hr: 0x%x", hr);
				if (!messageShown)
				{
					MessageBox(nullptr, _com_error(hr).ErrorMessage(), __FUNCTION__, MB_ICONERROR);
				}
				messageShown = true;
				hr = DDERR_SURFACELOST;
			}
			// We may still need the PostPresentHandoff here...
			if (g_bUseSteamVR && g_bTogglePostPresentHandoff) {
				g_pVRCompositor->PostPresentHandoff();
			}
		}
		else
		{
			hr = DD_OK;
		}

		return hr;
	}

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetAttachedSurface(
	LPDDSCAPS lpDDSCaps,
	LPDIRECTDRAWSURFACE FAR *lplpDDAttachedSurface
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

	if (lpDDSCaps == nullptr)
	{
#if LOGGER
		str.str("\tDDERR_INVALIDPARAMS");
		LogText(str.str());
#endif

		return DDERR_INVALIDPARAMS;
	}

	if (lpDDSCaps->dwCaps & DDSCAPS_BACKBUFFER)
	{
		if (!this->_hasBackbufferAttached)
		{
#if LOGGER
			str.str("\tDDERR_INVALIDOBJECT");
			LogText(str.str());
#endif

			return DDERR_INVALIDOBJECT;
		}

		*lplpDDAttachedSurface = this->_backbufferSurface;
		this->_backbufferSurface->AddRef();

#if LOGGER
		str.str("");
		str << "\tBackbufferSurface\t" << *lplpDDAttachedSurface;
		LogText(str.str());
#endif

		return DD_OK;
	}

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetBltStatus(
	DWORD dwFlags
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetCaps(
	LPDDSCAPS lpDDSCaps
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetClipper(
	LPDIRECTDRAWCLIPPER FAR *lplpDDClipper
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetColorKey(
	DWORD dwFlags,
	LPDDCOLORKEY lpDDColorKey
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetDC(
	HDC FAR *lphDC
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetFlipStatus(
	DWORD dwFlags
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetOverlayPosition(
	LPLONG lplX,
	LPLONG lplY
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetPalette(
	LPDIRECTDRAWPALETTE FAR *lplpDDPalette
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetPixelFormat(
	LPDDPIXELFORMAT lpDDPixelFormat
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::GetSurfaceDesc(
	LPDDSURFACEDESC lpDDSurfaceDesc
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

	if (lpDDSurfaceDesc == nullptr)
	{
#if LOGGER
		str.str("\tDDERR_INVALIDPARAMS");
		LogText(str.str());
#endif

		return DDERR_INVALIDPARAMS;
	}

	*lpDDSurfaceDesc = {};
	lpDDSurfaceDesc->dwSize = sizeof(DDSURFACEDESC);
	lpDDSurfaceDesc->dwFlags = DDSD_CAPS | DDSD_PIXELFORMAT | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH;
	lpDDSurfaceDesc->ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY;
	lpDDSurfaceDesc->ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	lpDDSurfaceDesc->ddpfPixelFormat.dwFlags = DDPF_RGB;
	lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = 16;
	lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = 0xF800;
	lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = 0x7E0;
	lpDDSurfaceDesc->ddpfPixelFormat.dwBBitMask = 0x1F;
	lpDDSurfaceDesc->dwHeight = this->_deviceResources->_displayHeight;
	lpDDSurfaceDesc->dwWidth = this->_deviceResources->_displayWidth;
	lpDDSurfaceDesc->lPitch = this->_deviceResources->_displayWidth * 2;

#if LOGGER
	str.str("");
	str << "\t" << tostr_DDSURFACEDESC(lpDDSurfaceDesc);
	LogText(str.str());
#endif

	return DD_OK;
}

HRESULT PrimarySurface::Initialize(
	LPDIRECTDRAW lpDD,
	LPDDSURFACEDESC lpDDSurfaceDesc
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::IsLost()
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::Lock(
	LPRECT lpDestRect,
	LPDDSURFACEDESC lpDDSurfaceDesc,
	DWORD dwFlags,
	HANDLE hEvent
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::ReleaseDC(
	HDC hDC
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::Restore()
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::SetClipper(
	LPDIRECTDRAWCLIPPER lpDDClipper
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::SetColorKey(
	DWORD dwFlags,
	LPDDCOLORKEY lpDDColorKey
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::SetOverlayPosition(
	LONG lX,
	LONG lY
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::SetPalette(
	LPDIRECTDRAWPALETTE lpDDPalette
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::Unlock(
	LPVOID lpSurfaceData
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::UpdateOverlay(
	LPRECT lpSrcRect,
	LPDIRECTDRAWSURFACE lpDDDestSurface,
	LPRECT lpDestRect,
	DWORD dwFlags,
	LPDDOVERLAYFX lpDDOverlayFx
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::UpdateOverlayDisplay(
	DWORD dwFlags
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

HRESULT PrimarySurface::UpdateOverlayZOrder(
	DWORD dwFlags,
	LPDIRECTDRAWSURFACE lpDDSReference
	)
{
#if LOGGER
	std::ostringstream str;
	str << this << " " << __FUNCTION__;
	LogText(str.str());
#endif

#if LOGGER
	str.str("\tDDERR_UNSUPPORTED");
	LogText(str.str());
#endif

	return DDERR_UNSUPPORTED;
}

ComPtr<IDWriteTextFormat> textFormats[3];
int fontSizes[] = { 12, 16, 10 };
D2D1_POINT_2F origin;
IDWriteFactory* pDWriteFactory_;
ID2D1Factory* pD2DFactory_;
ComPtr<IDWriteTextLayout> layouts[381];
ComPtr<ID2D1SolidColorBrush> brushes[32];
int brushColors[32] = {};
boolean font_initialized = false;

/*
  Computes the width of the given message. Use this function before adding messages to be
  displayed.
 */
short PrimarySurface::ComputeMsgWidth(char *str, int font_size_index) {
	int x = 0;
	if (!font_initialized)
		return x;

	unsigned char* fontWidths[] = { (unsigned char*)0x007D4C80, (unsigned char*)0x007D4D80, (unsigned char*)0x007D4E80 };
	for (int i = 0; str[i]; i++)
		x += fontWidths[font_size_index][(int)str[i]];
	return x;
}

/*
  Adds the given text to the buffer that is used by RenderText to display messages.
  This function requires Jeremy's new Text Renderer to work.
 
  font_size_index must be an integer in the range 0..2
 		index 0: small
 		index 1: regular
 		index 2: smallest
 
  color is in 0xRRGGBB format.
 
  x,y must be in in-game coordinates
 
  returns the x coordinate after the last char rendered
 */
short PrimarySurface::DisplayText(char *str, int font_size_index, short x, short y, uint32_t color) {
	if (!font_initialized)
		return x;

	// There are 3 font sizes (see the fontSizes global definition): 12, 16, 10
	int fontSize = fontSizes[font_size_index];
	// In XwaDrawTextHook, the fontWidths are populated using DWRITE_TEXT_METRICS and other variables.
	// Here we can just use the values computed by the hook.
	// Otherwise we would need to do the following for every ASCII char:
	/*
		DWRITE_TEXT_METRICS textMetrics;
		int layoutIndex = index * 256 + (int)character;
		s_textLayouts[layoutIndex]->GetMetrics(&textMetrics);
		w = (short)textMetrics.width;
		h = (short)textMetrics.height;
	*/
	unsigned char* fontWidths[] = { (unsigned char*)0x007D4C80, (unsigned char*)0x007D4D80, (unsigned char*)0x007D4E80 };
	XwaText xwaText;

	// Common attributes for all the chars in the string
	xwaText.color = color;
	xwaText.fontSize = fontSize;

	for (int i = 0; str[i]; i++) {
		char c = str[i];
		//RenderCharHook(x, y, 0, fontSize, c, color);

		// To speed things up a little bit, I'm not calling RenderCharHook here. Instead
		// I'm inserting the text directly into g_xwa_text. However, the code below should
		// follow RenderCharHook.
		xwaText.positionX = x;
		xwaText.positionY = y;
		xwaText.textChar = c;
		g_xwa_text.push_back(xwaText);

		x += fontWidths[font_size_index][(int)c];
	}
	return x;
}

/*
  Adds centered text at the given vertical position.
  Returns the x coordinate where the next char can be placed
 */
short PrimarySurface::DisplayCenteredText(char *str, int font_size_index, short y, uint32_t color) {
	short width = ComputeMsgWidth(str, font_size_index);
	short x = (short)(g_fCurInGameWidth / 2.0f) - width / 2;
	return DisplayText(str, font_size_index, x, y, color);
}

/*
  Adds a timed message in one of the 3 available slots, overwriting and reseting
  the timer on whatever is in that slot. The hard-coded values are screen-height
  relative and seem to work fine across different resolutions.
 */
void DisplayTimedMessage(uint32_t seconds, int row, char *msg) {
	short y_pos = (short)(g_fCurInGameHeight * (0.189f + 0.05f * row));
	g_TimedMessages[row].SetMsg(msg, seconds, y_pos, FONT_LARGE_IDX, FONT_BLUE_COLOR);
}

void PrimarySurface::RenderText()
{
	static DWORD s_displayWidth = 0;
	static DWORD s_displayHeight = 0;
	static int s_fontSizes[3] = { 12, 16, 10 };
	static ComPtr<IDWriteTextFormat> s_textFormats[3];
	static ComPtr<IDWriteTextLayout> s_textLayouts[3 * 256];
	static ComPtr<ID2D1SolidColorBrush> s_brush;
	static ComPtr<ID2D1SolidColorBrush> s_black_brush;
	static UINT s_left;
	static UINT s_top;
	static float s_scaleX;
	static float s_scaleY;
	static short s_rowSize = (short)(0.0185f * g_fCurInGameHeight);

	if (this->_deviceResources->_displayWidth != s_displayWidth || this->_deviceResources->_displayHeight != s_displayHeight)
	{
		s_displayWidth = this->_deviceResources->_displayWidth;
		s_displayHeight = this->_deviceResources->_displayHeight;

		UINT w;
		UINT h;

		if (g_config.AspectRatioPreserved)
		{
			if (this->_deviceResources->_backbufferHeight * this->_deviceResources->_displayWidth <= this->_deviceResources->_backbufferWidth * this->_deviceResources->_displayHeight)
			{
				w = this->_deviceResources->_backbufferHeight * this->_deviceResources->_displayWidth / this->_deviceResources->_displayHeight;
				h = this->_deviceResources->_backbufferHeight;
			}
			else
			{
				w = this->_deviceResources->_backbufferWidth;
				h = this->_deviceResources->_backbufferWidth * this->_deviceResources->_displayHeight / this->_deviceResources->_displayWidth;
			}
		}
		else
		{
			w = this->_deviceResources->_backbufferWidth;
			h = this->_deviceResources->_backbufferHeight;
		}

		s_left = (this->_deviceResources->_backbufferWidth - w) / 2;
		s_top = (this->_deviceResources->_backbufferHeight - h) / 2;

		s_scaleX = (float)w / (float)this->_deviceResources->_displayWidth;
		s_scaleY = (float)h / (float)this->_deviceResources->_displayHeight;

		for (int index = 0; index < 3; index++)
		{
			this->_deviceResources->_dwriteFactory->CreateTextFormat(
				g_config.TextFontFamily.c_str(),
				nullptr,
				DWRITE_FONT_WEIGHT_NORMAL,
				DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL,
				(float)s_fontSizes[index] * min(s_scaleX, s_scaleY),
				L"en-US",
				&s_textFormats[index]);

			for (int c = 33; c < 256; c++)
			{
				char t[2];
				t[0] = (char)c;
				t[1] = 0;

				std::wstring wtext = string_towstring(t);

				if (wtext.empty())
				{
					continue;
				}

				int layoutIndex = index * 256 + c;
				this->_deviceResources->_dwriteFactory->CreateTextLayout(
					wtext.c_str(),
					wtext.length(),
					s_textFormats[index],
					(FLOAT)this->_deviceResources->_backbufferWidth,
					(FLOAT)this->_deviceResources->_backbufferHeight,
					&s_textLayouts[layoutIndex]);
			}
		}

		this->_deviceResources->_d2d1RenderTarget->CreateSolidColorBrush(D2D1::ColorF(0), &s_brush);
		//this->_deviceResources->_d2d1RenderTarget->CreateSolidColorBrush(D2D1::ColorF(0xFF0000, 1.0f), &s_black_brush);
		this->_deviceResources->_d2d1RenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x0, 1.0f), &s_black_brush);
	}

	this->_deviceResources->_d2d1RenderTarget->SaveDrawingState(this->_deviceResources->_d2d1DrawingStateBlock);
	this->_deviceResources->_d2d1RenderTarget->BeginDraw();

	unsigned int brushColor = 0;
	s_brush->SetColor(D2D1::ColorF(brushColor));

	IDWriteTextLayout* layout = nullptr;

	if (!font_initialized)
	{
		for (int index = 0; index < 3; index++)
		{
			this->_deviceResources->_dwriteFactory->CreateTextFormat(
				g_config.TextFontFamily.c_str(),
				nullptr,
				DWRITE_FONT_WEIGHT_NORMAL,
				DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL,
				(float)fontSizes[index] * min(s_scaleX, s_scaleY),
				L"en-US",
				&textFormats[index]);
		}

		D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED,
			&pD2DFactory_
		);

		DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory),
			reinterpret_cast<IUnknown**>(&pDWriteFactory_)
		);

		origin = D2D1::Point2F(
			static_cast<FLOAT>(0),
			static_cast<FLOAT>(0)
		);

		for (int size_index = 0; size_index < 3; size_index++)
		{
			for (int char_index = 0; char_index < 127; char_index++)
			{
				char tm[2];
				tm[0] = char_index;
				tm[1] = 0;
				std::wstring ttext = string_towstring(tm);
				pDWriteFactory_->CreateTextLayout(
					ttext.c_str(),     
					ttext.length(),  
					textFormats[size_index],
					fontSizes[size_index] * s_scaleX,
					fontSizes[size_index] * s_scaleY,
					&layouts[size_index * 127 + char_index]
				);
			}
		}
		for (int brush_index = 0; brush_index < 32; brush_index++)
		{
			brushColors[brush_index] = -1;
		}
		font_initialized = true;
		//log_debug("[DBG] s_scaleX: %0.3f, s_scaleY: %0.3f, min: %0.3f", s_scaleX, s_scaleY, min(s_scaleX, s_scaleY));
	}

	int size_index = 0;
	for (const auto& xwaText : g_xwa_text)
	{
		if (xwaText.textChar == ' ')
		{
			continue;
		}

		IDWriteTextLayout* textLayout = nullptr;

		for (int index = 0; index < 3; index++)
		{
			if (xwaText.fontSize == s_fontSizes[index])
			{
				int layoutIndex = index * 256 + (int)(unsigned char)xwaText.textChar;
				textLayout = s_textLayouts[layoutIndex];
				break;
			}
		}

		if (!textLayout)
		{
			continue;
		}

		if (xwaText.color != brushColor)
		{
			brushColor = xwaText.color;
			s_brush->SetColor(D2D1::ColorF(brushColor));
		}

		float x = (float)s_left + (float)xwaText.positionX * s_scaleX;
		float y = (float)s_top + (float)xwaText.positionY * s_scaleY;

		this->_deviceResources->_d2d1RenderTarget->DrawTextLayout(
			D2D1_POINT_2F{ x, y },
			textLayout,
			s_brush);
	}

	// Clear the text. We'll add extra text now and render it.
	g_xwa_text.clear();
	g_xwa_text.reserve(4096);

	bool bExternalCamera = PlayerDataTable[*g_playerIndex].externalCamera;
	if (g_bDynCockpitEnabled && g_bReRenderMissilesNCounterMeasures && g_bDCApplyEraseRegionCommands &&
		!bExternalCamera) {
		// Gather the data we'll need to replace the missiles and countermeasures
		int16_t objectIndex = (int16_t)PlayerDataTable[*g_playerIndex].objectIndex;
		// Looks like objectIndex cannot be 0 or we'll crash
		//log_debug("[DBG] objectIndex: %d", objectIndex);
		if (objectIndex <= 0) goto out;
		ObjectEntry *object = &((*objects)[objectIndex]);
		if (object == NULL) goto out;
		MobileObjectEntry *mobileObject = object->MobileObjectPtr;
		if (mobileObject == NULL) goto out;
		CraftInstance *craftInstance = mobileObject->craftInstancePtr;
		if (craftInstance == NULL) goto out;

		int speed = (int)(PlayerDataTable[*g_playerIndex].currentSpeed / 2.25f);
		short throttle = (short)(100.0f * craftInstance->EngineThrottleInput / 65535.0f);
		int countL = 0, countR = 0, countermeasures = 0;
		countermeasures = craftInstance->CountermeasureAmount;
		int warheadStartIdx = craftInstance->NumberOfLasers;
		// If the secondary warheads are armed, then offset the warheadStartIdx accordingly:
		if (PlayerDataTable[*g_playerIndex].warheadArmed && PlayerDataTable[*g_playerIndex].primarySecondaryArmed)
			warheadStartIdx += 2;
		if (warheadStartIdx < 16)
			countL = craftInstance->Hardpoints[warheadStartIdx++].Count;
		if (warheadStartIdx < 16)
			countR = craftInstance->Hardpoints[warheadStartIdx++].Count;

		// The following will render a rectangle on the text buffer, which is then captured by DC. This can be used
		// to clear text right here without using more shaders.
		//D2D1_RECT_F rect = D2D1::RectF(0.0f, 100.0f, 1500.0f, 800.0f);
		//this->_deviceResources->_d2d1RenderTarget->FillRectangle(&rect, s_black_brush);

		// Clear the box for the missiles, countermeasures, speed & throttle, and add new 
		// text to replace the ones we're clearing here
		D2D1_RECT_F rect;
		DCElemSrcBox *dcElemSrcBox;
		float fx, fy;
		static short ofs = (short)(0.005f * g_fCurInGameHeight);

		dcElemSrcBox = &g_DCElemSrcBoxes.src_boxes[MISSILES_DC_ELEM_SRC_IDX];
		if (dcElemSrcBox->bComputed) {
			rect.left = g_fCurScreenWidth * dcElemSrcBox->coords.x0;
			rect.top = g_fCurScreenHeight * dcElemSrcBox->coords.y0;
			rect.right = g_fCurScreenWidth * dcElemSrcBox->coords.x1;
			rect.bottom = g_fCurScreenHeight * dcElemSrcBox->coords.y1;
			this->_deviceResources->_d2d1RenderTarget->FillRectangle(&rect, s_black_brush);
			
			ScreenCoordsToInGame(g_nonVRViewport.TopLeftX, g_nonVRViewport.TopLeftY,
				g_nonVRViewport.Width, g_nonVRViewport.Height,
				rect.left, rect.top, &fx, &fy);
			char buf[40];
			sprintf_s(buf, 40, "%d %d", countL, countR);
			DisplayText(buf, FONT_MEDIUM_IDX, (short)fx + ofs, (short)fy + ofs, 0xFFFFFF);
		}

		dcElemSrcBox = &g_DCElemSrcBoxes.src_boxes[NUM_CRAFTS_DC_ELEM_SRC_IDX];
		if (dcElemSrcBox->bComputed) {
			rect.left = g_fCurScreenWidth * dcElemSrcBox->coords.x0;
			rect.top = g_fCurScreenHeight * dcElemSrcBox->coords.y0;
			rect.right = g_fCurScreenWidth * dcElemSrcBox->coords.x1;
			rect.bottom = g_fCurScreenHeight * dcElemSrcBox->coords.y1;
			this->_deviceResources->_d2d1RenderTarget->FillRectangle(&rect, s_black_brush);
			ScreenCoordsToInGame(g_nonVRViewport.TopLeftX, g_nonVRViewport.TopLeftY,
				g_nonVRViewport.Width, g_nonVRViewport.Height,
				rect.left, rect.top, &fx, &fy);

			char buf[40];
			sprintf_s(buf, 40, "%d", countermeasures);
			DisplayText(buf, FONT_MEDIUM_IDX, (short)fx + ofs, (short)fy + ofs, 0xFFFFFF);
		}

		dcElemSrcBox = &g_DCElemSrcBoxes.src_boxes[SPEED_N_THROTTLE_DC_ELEM_SRC_IDX];
		if (dcElemSrcBox->bComputed) {
			rect.left = g_fCurScreenWidth * dcElemSrcBox->coords.x0;
			rect.top = g_fCurScreenHeight * dcElemSrcBox->coords.y0;
			rect.right = g_fCurScreenWidth * dcElemSrcBox->coords.x1;
			rect.bottom = g_fCurScreenHeight * dcElemSrcBox->coords.y1;
			this->_deviceResources->_d2d1RenderTarget->FillRectangle(&rect, s_black_brush);
			ScreenCoordsToInGame(g_nonVRViewport.TopLeftX, g_nonVRViewport.TopLeftY,
				g_nonVRViewport.Width, g_nonVRViewport.Height,
				rect.left, rect.top, &fx, &fy);

			char buf[40];
			sprintf_s(buf, 40, "SPD: %d", speed);
			DisplayText(buf, FONT_MEDIUM_IDX, (short)fx + ofs, (short)fy + ofs, 0xFFFFFF);
			sprintf_s(buf, 40, "THR: %d%%", throttle);
			DisplayText(buf, FONT_MEDIUM_IDX, (short)fx + ofs, (short)fy + ofs + s_rowSize, 0xFFFFFF);
		}
	}

out:
	// Add the custom text that will be displayed
	for (int i = 0; i < MAX_TIMED_MESSAGES; i++) {
		if (g_TimedMessages[i].IsExpired())
			continue;
		g_TimedMessages[i].Tick();
		DisplayCenteredText(g_TimedMessages[i].msg, g_TimedMessages[i].font_size_idx,
			g_TimedMessages[i].y, g_TimedMessages[i].color);
	}

	// Render the additional text
	size_index = 0;
	for (const auto& xwaText : g_xwa_text)
	{
		if (xwaText.textChar == ' ')
		{
			continue;
		}

		IDWriteTextLayout* textLayout = nullptr;

		for (int index = 0; index < 3; index++)
		{
			if (xwaText.fontSize == s_fontSizes[index])
			{
				int layoutIndex = index * 256 + (int)(unsigned char)xwaText.textChar;
				textLayout = s_textLayouts[layoutIndex];
				break;
			}
		}

		if (!textLayout)
		{
			continue;
		}

		if (xwaText.color != brushColor)
		{
			brushColor = xwaText.color;
			s_brush->SetColor(D2D1::ColorF(brushColor));
		}

		float x = (float)s_left + (float)xwaText.positionX * s_scaleX;
		float y = (float)s_top + (float)xwaText.positionY * s_scaleY;

		this->_deviceResources->_d2d1RenderTarget->DrawTextLayout(
			D2D1_POINT_2F{ x, y },
			textLayout,
			s_brush);
	}

	this->_deviceResources->_d2d1RenderTarget->EndDraw();
	this->_deviceResources->_d2d1RenderTarget->RestoreDrawingState(this->_deviceResources->_d2d1DrawingStateBlock);

	g_xwa_text.clear();
	g_xwa_text.reserve(4096);
}

void PrimarySurface::RenderRadar()
{
	static DWORD s_displayWidth = 0;
	static DWORD s_displayHeight = 0;
	static ComPtr<ID2D1SolidColorBrush> s_brush;
	static UINT s_left;
	static UINT s_top;
	static float s_scaleX;
	static float s_scaleY;

	if (this->_deviceResources->_displayWidth != s_displayWidth || this->_deviceResources->_displayHeight != s_displayHeight)
	{
		s_displayWidth = this->_deviceResources->_displayWidth;
		s_displayHeight = this->_deviceResources->_displayHeight;

		UINT w;
		UINT h;

		if (g_config.AspectRatioPreserved)
		{
			if (this->_deviceResources->_backbufferHeight * this->_deviceResources->_displayWidth <= this->_deviceResources->_backbufferWidth * this->_deviceResources->_displayHeight)
			{
				w = this->_deviceResources->_backbufferHeight * this->_deviceResources->_displayWidth / this->_deviceResources->_displayHeight;
				h = this->_deviceResources->_backbufferHeight;
			}
			else
			{
				w = this->_deviceResources->_backbufferWidth;
				h = this->_deviceResources->_backbufferWidth * this->_deviceResources->_displayHeight / this->_deviceResources->_displayWidth;
			}
		}
		else
		{
			w = this->_deviceResources->_backbufferWidth;
			h = this->_deviceResources->_backbufferHeight;
		}

		s_left = (this->_deviceResources->_backbufferWidth - w) / 2;
		s_top = (this->_deviceResources->_backbufferHeight - h) / 2;

		s_scaleX = (float)w / (float)this->_deviceResources->_displayWidth;
		s_scaleY = (float)h / (float)this->_deviceResources->_displayHeight;

		this->_deviceResources->_d2d1RenderTarget->CreateSolidColorBrush(D2D1::ColorF(0), &s_brush);
	}

	this->_deviceResources->_d2d1RenderTarget->SaveDrawingState(this->_deviceResources->_d2d1DrawingStateBlock);
	this->_deviceResources->_d2d1RenderTarget->BeginDraw();

	unsigned int brushColor = 0;
	s_brush->SetColor(D2D1::ColorF(brushColor));

	for (const auto& xwaRadar : g_xwa_radar)
	{
		unsigned short si = ((unsigned short*)0x08D9420)[xwaRadar.colorIndex];
		unsigned int esi;

		if (((bool(*)())0x0050DC50)() != 0)
		{
			unsigned short eax = si & 0x001F;
			unsigned short ecx = si & 0x7C00;
			unsigned short edx = si & 0x03E0;

			esi = (eax << 3) | (edx << 6) | (ecx << 9);
		}
		else
		{
			unsigned short eax = si & 0x001F;
			unsigned short edx = si & 0xF800;
			unsigned short ecx = si & 0x07E0;

			esi = (eax << 3) | (ecx << 5) | (edx << 8);
		}

		if (esi != brushColor)
		{
			brushColor = esi;
			s_brush->SetColor(D2D1::ColorF(brushColor));
		}

		float x = s_left + (float)xwaRadar.positionX * s_scaleX;
		float y = s_top + (float)xwaRadar.positionY * s_scaleY;

		float deltaX = 2.0f * s_scaleX;
		float deltaY = 2.0f * s_scaleY;

		this->_deviceResources->_d2d1RenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), deltaX, deltaY), s_brush);
	}

	if (g_xwa_radar_selected_positionX != -1 && g_xwa_radar_selected_positionY != -1)
	{
		s_brush->SetColor(D2D1::ColorF(D2D1::ColorF::Yellow));

		float x = s_left + (float)g_xwa_radar_selected_positionX * s_scaleX;
		float y = s_top + (float)g_xwa_radar_selected_positionY * s_scaleY;

		float deltaX = 4.0f * s_scaleX;
		float deltaY = 4.0f * s_scaleY;

		this->_deviceResources->_d2d1RenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), deltaX, deltaY), s_brush);
	}

	this->_deviceResources->_d2d1RenderTarget->EndDraw();
	this->_deviceResources->_d2d1RenderTarget->RestoreDrawingState(this->_deviceResources->_d2d1DrawingStateBlock);

	g_xwa_radar.clear();
	g_xwa_radar_selected_positionX = -1;
	g_xwa_radar_selected_positionY = -1;
}

void PrimarySurface::RenderBracket()
{
	static DWORD s_displayWidth = 0;
	static DWORD s_displayHeight = 0;
	// It's probably not necessary to have two brushes, but I don't think it hurts either and
	// I'm doing this just in case brushes can't be shared between different RTVs
	static ComPtr<ID2D1SolidColorBrush> s_brushOffscreen, s_brushDC, s_brush;
	static UINT s_left;
	static UINT s_top;
	static float s_scaleX;
	static float s_scaleY;

	if (this->_deviceResources->_displayWidth != s_displayWidth || this->_deviceResources->_displayHeight != s_displayHeight)
	{
		s_displayWidth = this->_deviceResources->_displayWidth;
		s_displayHeight = this->_deviceResources->_displayHeight;

		UINT w;
		UINT h;

		if (g_config.AspectRatioPreserved)
		{
			if (this->_deviceResources->_backbufferHeight * this->_deviceResources->_displayWidth <= this->_deviceResources->_backbufferWidth * this->_deviceResources->_displayHeight)
			{
				w = this->_deviceResources->_backbufferHeight * this->_deviceResources->_displayWidth / this->_deviceResources->_displayHeight;
				h = this->_deviceResources->_backbufferHeight;
			}
			else
			{
				w = this->_deviceResources->_backbufferWidth;
				h = this->_deviceResources->_backbufferWidth * this->_deviceResources->_displayHeight / this->_deviceResources->_displayWidth;
			}
		}
		else
		{
			w = this->_deviceResources->_backbufferWidth;
			h = this->_deviceResources->_backbufferHeight;
		}

		s_left = (this->_deviceResources->_backbufferWidth - w) / 2;
		s_top = (this->_deviceResources->_backbufferHeight - h) / 2;

		s_scaleX = (float)w / (float)this->_deviceResources->_displayWidth;
		s_scaleY = (float)h / (float)this->_deviceResources->_displayHeight;

		this->_deviceResources->_d2d1OffscreenRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0), &s_brushOffscreen);
		this->_deviceResources->_d2d1DCRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0), &s_brushDC);
	}

	this->_deviceResources->_d2d1OffscreenRenderTarget->SaveDrawingState(this->_deviceResources->_d2d1DrawingStateBlock);
	this->_deviceResources->_d2d1OffscreenRenderTarget->BeginDraw();

	this->_deviceResources->_d2d1DCRenderTarget->SaveDrawingState(this->_deviceResources->_d2d1DrawingStateBlock);
	this->_deviceResources->_d2d1DCRenderTarget->BeginDraw();

	unsigned int brushColor = 0;
	s_brushOffscreen->SetColor(D2D1::ColorF(brushColor));
	s_brushDC->SetColor(D2D1::ColorF(brushColor));

	for (const auto& xwaBracket : g_xwa_bracket)
	{
		unsigned short si = ((unsigned short*)0x08D9420)[xwaBracket.colorIndex];
		unsigned int esi;

		if (((bool(*)())0x0050DC50)() != 0)
		{
			unsigned short eax = si & 0x001F;
			unsigned short ecx = si & 0x7C00;
			unsigned short edx = si & 0x03E0;

			esi = (eax << 3) | (edx << 6) | (ecx << 9);
		}
		else
		{
			unsigned short eax = si & 0x001F;
			unsigned short edx = si & 0xF800;
			unsigned short ecx = si & 0x07E0;

			esi = (eax << 3) | (ecx << 5) | (edx << 8);
		}

		if (esi != brushColor)
		{
			brushColor = esi;
			s_brushOffscreen->SetColor(D2D1::ColorF(brushColor));
			s_brushDC->SetColor(D2D1::ColorF(brushColor));
		}

		float posX = s_left + (float)xwaBracket.positionX * s_scaleX;
		float posY = s_top + (float)xwaBracket.positionY * s_scaleY;
		float posW = (float)xwaBracket.width * s_scaleX;
		float posH = (float)xwaBracket.height * s_scaleY;
		float posSide = 0.125;

		float strokeWidth = 2.0f * min(s_scaleX, s_scaleY);

		bool fill = xwaBracket.width <= 4 || xwaBracket.height <= 4;
		// Select the DC RTV if this bracket was classified as belonging to
		// the Dynamic Cockpit display
		ID2D1RenderTarget *rtv = xwaBracket.DC ? this->_deviceResources->_d2d1DCRenderTarget : this->_deviceResources->_d2d1OffscreenRenderTarget;
		s_brush = xwaBracket.DC ? s_brushDC : s_brushOffscreen;

		if (fill)
		{
			rtv->FillRectangle(D2D1::RectF(posX, posY, posX + posW, posY + posH), s_brush);
		}
		else
		{
			// top left
			rtv->DrawLine(D2D1::Point2F(posX, posY), D2D1::Point2F(posX + posW * posSide, posY), s_brush, strokeWidth);
			rtv->DrawLine(D2D1::Point2F(posX, posY), D2D1::Point2F(posX, posY + posH * posSide), s_brush, strokeWidth);

			// top right
			rtv->DrawLine(D2D1::Point2F(posX + posW - posW * posSide, posY), D2D1::Point2F(posX + posW, posY), s_brush, strokeWidth);
			rtv->DrawLine(D2D1::Point2F(posX + posW, posY), D2D1::Point2F(posX + posW, posY + posH * posSide), s_brush, strokeWidth);

			// bottom left
			rtv->DrawLine(D2D1::Point2F(posX, posY + posH - posH * posSide), D2D1::Point2F(posX, posY + posH), s_brush, strokeWidth);
			rtv->DrawLine(D2D1::Point2F(posX, posY + posH), D2D1::Point2F(posX + posW * posSide, posY + posH), s_brush, strokeWidth);

			// bottom right
			rtv->DrawLine(D2D1::Point2F(posX + posW - posW * posSide, posY + posH), D2D1::Point2F(posX + posW, posY + posH), s_brush, strokeWidth);
			rtv->DrawLine(D2D1::Point2F(posX + posW, posY + posH - posH * posSide), D2D1::Point2F(posX + posW, posY + posH), s_brush, strokeWidth);
		}
	}

	this->_deviceResources->_d2d1DCRenderTarget->EndDraw();
	this->_deviceResources->_d2d1DCRenderTarget->RestoreDrawingState(this->_deviceResources->_d2d1DrawingStateBlock);

	this->_deviceResources->_d2d1OffscreenRenderTarget->EndDraw();
	this->_deviceResources->_d2d1OffscreenRenderTarget->RestoreDrawingState(this->_deviceResources->_d2d1DrawingStateBlock);

	// Need to set s_brush to NULL to avoid dangling references. Otherwise, we may get
	// crashes when exiting.
	s_brush = nullptr;
	g_xwa_bracket.clear();
}
