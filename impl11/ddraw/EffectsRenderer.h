#pragma once

#include <vector>
#include <ScreenGrab.h>
#include <wincodec.h>

#include "globals.h"
#include "common.h"
#include "commonVR.h"

#include "XwaD3dRendererHook.h"
#include "D3dRenderer.h"
#include "DeviceResources.h"
#include "Direct3DTexture.h"
#include "Matrices.h"
#include "joystick.h"

// ****************************************************
// External variables
// ****************************************************
extern bool g_bIsTargetHighlighted, g_bPrevIsTargetHighlighted, g_bAOEnabled;
extern bool g_bExplosionsDisplayedOnCurrentFrame, g_bEnableVR, g_bProceduralLava;
extern bool g_bIsScaleableGUIElem, g_bScaleableHUDStarted, g_bIsTrianglePointer;
extern float g_fSSAOAlphaOfs, g_fFloatingGUIObjDepth;

// ****************************************************
// Debug variables
// ****************************************************
extern bool g_bDumpSSAOBuffers;
extern int g_iD3DExecuteCounter, g_iD3DExecuteCounterSkipHi, g_iD3DExecuteCounterSkipLo;

// ****************************************************
// Temporary, we may not need these here
// ****************************************************
extern bool g_bPrevIsScaleableGUIElem, g_bStartedGUI;
extern bool g_bIsFloating3DObject, g_bPrevIsFloatingGUI3DObject;
extern bool g_bTargetCompDrawn;
extern bool g_bEnableAnimations;
extern HyperspacePhaseEnum g_HyperspacePhaseFSM;

bool IsInsideTriangle(Vector2 P, Vector2 A, Vector2 B, Vector2 C, float *u, float *v);
Matrix4 ComputeLightViewMatrix(int idx, Matrix4 &Heading, bool invert);

class EffectsRenderer : public D3dRenderer
{
protected:
	bool _bLastTextureSelectedNotNULL, _bLastLightmapSelectedNotNULL, _bIsLaser, _bIsCockpit;
	bool _bIsGunner, _bIsExplosion, _bIsBlastMark, _bHasMaterial, _bDCIsTransparent, _bDCElemAlwaysVisible;
	bool _bModifiedShaders, _bModifiedPixelShader, _bModifiedBlendState, _bModifiedSamplerState;
	bool _bIsNoisyHolo, _bWarheadLocked, _bIsTargetHighlighted, _bIsHologram, _bRenderingLightingEffect;
	bool _bCockpitConstantsCaptured, _bExternalCamera, _bCockpitDisplayed, _bIsTransparentCall;
	bool _bShadowsRenderedInCurrentFrame, _bJoystickTransformReady; // _bThrottleTransformReady, _bThrottleRotAxisToZPlusReady;
	bool _bHangarShadowsRenderedInCurrentFrame;
	D3dConstants _CockpitConstants;
	XwaTransform _CockpitWorldView;
	Direct3DTexture *_lastTextureSelected = nullptr;
	Direct3DTexture *_lastLightmapSelected = nullptr;
	std::vector<DrawCommand> _LaserDrawCommands;
	std::vector<DrawCommand> _TransparentDrawCommands;
	std::vector<DrawCommand> _ShadowMapDrawCommands;
	Matrix4 _joystickMeshTransform;
	Matrix4 _throttleMeshTransform;
	AABB _hangarShadowAABB;
	Matrix4 _hangarShadowMapRotation;

	VertexShaderCBuffer _oldVSCBuffer;
	PixelShaderCBuffer _oldPSCBuffer;
	DCPixelShaderCBuffer _oldDCPSCBuffer;
	ComPtr<ID3D11Buffer> _oldVSConstantBuffer;
	ComPtr<ID3D11Buffer> _oldPSConstantBuffer;
	ComPtr<ID3D11ShaderResourceView> _oldVSSRV[3];
	ComPtr<ID3D11ShaderResourceView> _oldPSSRV[13];
	ComPtr<ID3D11VertexShader> _oldVertexShader;
	ComPtr<ID3D11PixelShader> _oldPixelShader;
	ComPtr<ID3D11SamplerState> _oldPSSamplers[2];
	ComPtr<ID3D11RenderTargetView> _oldRTVs[8];
	ComPtr<ID3D11DepthStencilView> _oldDSV;
	ComPtr<ID3D11DepthStencilState> _oldDepthStencilState;
	ComPtr<ID3D11BlendState> _oldBlendState;
	ComPtr<ID3D11InputLayout> _oldInputLayout;
	ComPtr<ID3D11Buffer> _oldVertexBuffer, _oldIndexBuffer;
	D3D11_PRIMITIVE_TOPOLOGY _oldTopology;
	UINT _oldStencilRef, _oldSampleMask;
	FLOAT _oldBlendFactor[4];
	UINT _oldStride, _oldOffset, _oldIOffset;
	DXGI_FORMAT _oldFormat;
	D3D11_VIEWPORT _oldViewports[2];
	UINT _oldNumViewports = 2;

	ComPtr<ID3D11ComputeShader> _RTShadowCS;


	void OBJDumpD3dVertices(const SceneCompData *scene, const Matrix4 &A);
	HRESULT QuickSetZWriteEnabled(BOOL Enabled);
	void EnableTransparency();
	void EnableHoloTransparency();
	inline ID3D11RenderTargetView *SelectOffscreenBuffer(bool bIsMaskable, bool bSteamVRRightEye=false);
	Matrix4 GetShadowMapLimits(Matrix4 L, float *OBJrange, float *OBJminZ);
	Matrix4 GetShadowMapLimits(const AABB &aabb, float *OBJrange, float *OBJminZ);

	void SaveContext();
	void RestoreContext();

public:
	EffectsRenderer();
	virtual void CreateShaders();
	virtual void SceneBegin(DeviceResources* deviceResources);
	virtual void SceneEnd();
	virtual void MainSceneHook(const SceneCompData* scene);
	virtual void HangarShadowSceneHook(const SceneCompData* scene);
	virtual void RenderScene();
	virtual void UpdateTextures(const SceneCompData* scene);
	
	// State Management
	void DoStateManagement(const SceneCompData* scene);
	void ApplyMaterialProperties();
	void ApplySpecialMaterials();
	void ApplyBloomSettings();
	void ApplyDiegeticCockpit();
	void ApplyMeshTransform();
	void DCCaptureMiniature();
	// Returns true if the current draw call needs to be skipped
	bool DCReplaceTextures();
	virtual void ExtraPreprocessing();
	void AddLaserLights(const SceneCompData* scene);
	void ApplyProceduralLava();
	void ApplyGreebles();
	void ApplyAnimatedTextures(int objectId, bool bInstanceEvent);
	void ApplyNormalMapping();

	// Raytracing
	void ApplyRTShadows();

	// Per-texture, per-instance effects
	CraftInstance *ObjectIDToCraftInstance(int objectId);
	InstanceEvent *ObjectIDToInstanceEvent(int objectId, uint32_t materialId);

	// Deferred rendering
	void RenderLasers();
	void RenderTransparency();
	void RenderCockpitShadowMap();
	void RenderHangarShadowMap();
	void StartCascadedShadowMap();
	void RenderCascadedShadowMap();
	void EndCascadedShadowMap();
	void RenderDeferredDrawCalls();
};

extern EffectsRenderer *g_effects_renderer;

extern D3dRenderer *g_current_renderer;