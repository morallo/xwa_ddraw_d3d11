#pragma once

#include "commonVR.h"
#include "effects.h"
#include "active_cockpit.h"
#include "dynamic_cockpit.h"
#include "shadow_mapping.h"

// METRIC RECONSTRUCTION:
extern bool g_bYCenterHasBeenFixed;

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

extern int g_KeySet; // Default setting: let users adjust the FOV, I can always override this with the "key_set" SSAO.cfg param

// Counter for calls to DrawIndexed() (This helps us know where were are in the rendering process)
// Gets reset everytime the backbuffer is presented and is increased only after BOTH the left and
// right images have been rendered.
extern int g_iDrawCounter, g_iNoDrawBeforeIndex, g_iNoDrawAfterIndex, g_iDrawCounterAfterHUD;
// Similar to the above, but only get incremented after each Execute() is finished.
extern int g_iNoExecBeforeIndex, g_iNoExecAfterIndex, g_iNoDrawAfterHUD;
// The Skybox cannot be detected using g_iExecBufCounter anymore when using the hook_d3d because the whole frame is
// rendered with a single Execute call
//extern int g_iSkyBoxExecIndex = DEFAULT_SKYBOX_INDEX; // This gives us the threshold for the Skybox
// g_iSkyBoxExecIndex is compared against g_iExecBufCounter to determine when the SkyBox is rendered
// This is important because in XWAU the SkyBox is *not* rendered at infinity and causes a lot of
// visual contention if not handled properly.
extern bool g_bIsSkyBox, g_bPrevIsSkyBox, g_bSkyBoxJustFinished;

extern bool g_bFixSkyBox; // Fix the skybox (send it to infinity: use original vertices without parallax)
extern bool g_bSkipSkyBox;
extern bool g_bStartedGUI; // Set to false at the beginning of each frame. Set to true when the GUI has begun rendering.
extern bool g_bPrevStartedGUI; // Keeps the last value of g_bStartedGUI -- helps detect when the GUI is about to be rendered.
extern bool g_bIsScaleableGUIElem; // Set to false at the beginning of each frame. Set to true when the scaleable GUI has begun rendering.
extern bool g_bPrevIsScaleableGUIElem; // Keeps the last value of g_bIsScaleableGUIElem -- helps detect when scaleable GUI is about to start rendering.
extern bool g_bScaleableHUDStarted; // Becomes true as soon as the scaleable HUD is about to be rendered. Reset to false at the end of each frame
extern bool g_bSkipGUI; // Skip non-skybox draw calls with disabled Z-Buffer
extern bool g_bSkipText; // Skips text draw calls
extern bool g_bSkipAfterTargetComp; // Skip all draw calls after the targetting computer has been drawn
extern bool g_bTargetCompDrawn; // Becomes true after the targetting computer has been drawn
extern bool g_bPrevIsFloatingGUI3DObject; // Stores the last value of g_bIsFloatingGUI3DObject -- useful to detect when the targeted craft is about to be drawn
extern bool g_bIsFloating3DObject; // true when rendering the targeted 3D object.
extern bool g_bIsTrianglePointer, g_bLastTrianglePointer;
extern bool g_bIsPlayerObject, g_bPrevIsPlayerObject, g_bHyperspaceEffectRenderedOnCurrentFrame;
extern bool g_bIsTargetHighlighted; // True if the target can  be fired upon, gets reset every frame
extern bool g_bPrevIsTargetHighlighted; // The value of g_bIsTargetHighlighted for the previous frame

//bool g_bLaserBoxLimitsUpdated = false; // Set to true whenever the laser/ion charge limit boxes are updated
extern unsigned int g_iFloatingGUIDrawnCounter;
extern int g_iPresentCounter, g_iNonZBufferCounter, g_iSkipNonZBufferDrawIdx;
extern float g_fZBracketOverride; // 65535 is probably the maximum Z value in XWA
extern bool g_bResetDC;

// DS2 Effects
extern int g_iReactorExplosionCount;


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
extern bool g_bKeybExitHyperspace;
extern Vector4 g_TempLightColor[2], g_TempLightVector[2];

extern bool g_bFXAAEnabled;

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

/*********************************************************/
// DYNAMIC COCKPIT
extern char g_sCurrentCockpit[128];
extern DCHUDRegions g_DCHUDRegions;
extern DCElemSrcBoxes g_DCElemSrcBoxes;
extern dc_element g_DCElements[MAX_DC_SRC_ELEMENTS];

extern float g_fCoverTextureBrightness;
extern float g_fDCBrightness;
extern int g_iNumDCElements;
extern move_region_coords g_DCMoveRegions;
extern float g_fCurInGameWidth, g_fCurInGameHeight, g_fCurInGameAspectRatio, g_fCurScreenWidth, g_fCurScreenHeight, g_fCurScreenWidthRcp, g_fCurScreenHeightRcp;
extern bool g_bDCManualActivate, g_bDCApplyEraseRegionCommands, g_bReRenderMissilesNCounterMeasures;
extern bool g_bGlobalDebugFlag, g_bInhibitCMDBracket;
extern bool g_bHUDVisibleOnStartup;
extern bool g_bCompensateFOVfor1920x1080;
extern bool g_bDCWasClearedOnThisFrame;
extern int g_iHUDOffscreenCommandsRendered;
extern bool g_bEdgeEffectApplied;
extern int g_WindowWidth, g_WindowHeight;
extern float4 g_DCTargetingColor, g_DCWireframeLuminance;
extern float4 g_DCTargetingIFFColors[6];
extern float g_DCWireframeContrast;
extern float g_fReticleScale;
extern Vector2 g_SubCMDBracket; // Populated in XwaDrawBracketHook for the sub-CMD bracket when the enhanced 2D renderer is on
// HOLOGRAMS
extern float g_fDCHologramFadeIn, g_fDCHologramFadeInIncr, g_fDCHologramTime;
extern bool g_bDCHologramsVisible, g_bDCHologramsVisiblePrev;

// DEBUG vars
extern Vector3 g_debug_v0, g_debug_v1, g_debug_v2;
extern bool g_bDumpLaserPointerDebugInfo;
extern Vector3 g_LPdebugPoint;
extern float g_fLPdebugPointOffset, g_fDebugYCenter;
/*
 * Dumps the vertices in the current instruction to the given file after back-projecting them
 * into 3D space.
 * After dumping solid polygons, I realized the back-project code skews the Y axis... considerably.
 */

extern bool g_bDumpOBJEnabled;
extern FILE* g_DumpOBJFile;
extern int g_iDumpOBJFaceIdx, g_iDumpOBJIdx;
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

// MATERIALS
extern bool g_bReloadMaterialsEnabled;
extern Material g_DefaultGlobalMaterial;
extern std::vector<ColorLightPair> g_TextureVector;
/*
 * Used to store a list of textures for fast lookup. For instance, all suns must
 * have their associated lights reset after jumping through hyperspace; and all
 * textures with materials can be placed here so that material properties can be
 * applied while flying.
 */
extern std::vector<Direct3DTexture*> g_AuxTextureVector;

// DS2 Effects
extern int g_iReactorExplosionCount;

extern int* s_XwaGlobalLightsCount;
extern XwaGlobalLight* s_XwaGlobalLights;
extern Matrix4 g_CurrentHeadingViewMatrix;

extern bool g_bExternalHUDEnabled, g_bEdgeDetectorEnabled, g_bStarDebugEnabled;

extern float g_f2DYawMul, g_f2DPitchMul, g_f2DRollMul;
extern TrackerType g_TrackerType;

extern bool g_bAOEnabled;

extern float g_fCurInGameHeight;

extern float g_fDefaultFOVDist;
extern float g_fDebugFOVscale, g_fDebugYCenter;
extern float g_fCurrentShipFocalLength, g_fCurrentShipLargeFocalLength, g_fReticleScale;
extern bool g_bYCenterHasBeenFixed;
extern bool g_bTogglePostPresentHandoff;

extern uint32_t* g_rawFOVDist; /* = (uint32_t *)0x91AB6C*/ // raw FOV dist(dword int), copy of one of the six values hard-coded with the resolution slots, which are what xwahacker edits
extern float* g_fRawFOVDist; /*= (float *)0x8B94CC;*/ // FOV dist(float), same value as above
extern float* g_cachedFOVDist; /*= (float *)0x8B94BC;*/ // cached FOV dist / 512.0 (float), seems to be used for some sprite processing

extern float g_fCurrentShipFocalLength, g_fCurrentShipLargeFocalLength, g_fVR_FOV;
extern float g_fDebugFOVscale, g_fDebugYCenter;
extern bool g_bCustomFOVApplied, g_bLastFrameWasExterior;
extern float g_fRealHorzFOV, g_fRealVertFOV;

