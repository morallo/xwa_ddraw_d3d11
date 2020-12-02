#pragma once

#include "commonVR.h"
#include "effects.h"
#include "active_cockpit.h"
#include "dynamic_cockpit.h"
#include "DeviceResources.h"

extern bool g_bCockpitPZHackEnabled;
extern bool g_bOverrideAspectRatio;
extern bool g_bEnableVR;
extern TrackerType g_TrackerType;

extern bool g_bDisableBarrelEffect, g_bEnableVR, g_bResetHeadCenter, g_bBloomEnabled, g_bAOEnabled, g_bCustomFOVApplied;
extern bool g_bDirectSBSInitialized, g_bSteamVRInitialized, g_bClearHUDBuffers, g_bDCManualActivate, g_bGlobalDebugFlag;
extern bool g_bDumpSSAOBuffers, g_bEnableSSAOInShader, g_bEnableIndirectSSDO, g_bResetDC, g_bProceduralSuns, g_bEnableHeadLights;
extern bool g_bShowSSAODebug, g_bShowNormBufDebug, g_bFNEnable, g_bShadowEnable, g_bGlobalSpecToggle, g_bToggleSkipDC;
extern Vector4 g_LightVector[2];
extern float g_fSpecIntensity, g_fSpecBloomIntensity, g_fFocalDist, g_fFakeRoll;

extern bool g_bHDREnabled, g_bEdgeDetectorEnabled;
extern float g_fHDRWhitePoint;

extern bool g_bRendering3D; // Used to distinguish between 2D (Concourse/Menus) and 3D rendering (main in-flight game)
// g_fZOverride is activated when it's greater than -0.9f, and it's used for bracket rendering so that 
// objects cover the brackets. In this way, we avoid visual contention from the brackets.

extern bool g_bZoomOut, g_bZoomOutInitialState;

// VR
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
extern vr::IVRSystem* g_pHMD;
extern int g_iFreePIESlot, g_iSteamVR_Remaining_ms, g_iSteamVR_VSync_ms;
extern Matrix4 g_FullProjMatrixLeft, g_FullProjMatrixRight;


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

// BLOOM
extern bool /* g_bDumpBloomBuffers, */ g_bDCManualActivate;
extern BloomConfig g_BloomConfig;

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
extern float g_fMoireOffsetDir, g_fMoireOffsetInd;



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

// DS2 Effects
extern int g_iReactorExplosionCount;

extern int* s_XwaGlobalLightsCount;
extern XwaGlobalLight* s_XwaGlobalLights;
extern Matrix4 g_CurrentHeadingViewMatrix;

extern bool g_bExternalHUDEnabled, g_bEdgeDetectorEnabled, g_bStarDebugEnabled;

extern float g_f2DYawMul, g_f2DPitchMul, g_f2DRollMul;
extern TrackerType g_TrackerType;


extern bool g_bAOEnabled;


