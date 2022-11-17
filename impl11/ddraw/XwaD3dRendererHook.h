#pragma once

#include "xwa_structures.h"

class DeviceResources;

#pragma pack(push, 1)

#pragma pack(pop)

enum RendererType
{
	RendererType_Unknown,
	RendererType_Main,
	RendererType_Shadow,
};

extern RendererType g_rendererType;

enum BVHBuilderType
{
	BVHBuilderType_BVH2,
	BVHBuilderType_QBVH,
	BVHBuilderType_FastQBVH,
	BVHBuilderType_MAX,
};

extern BVHBuilderType g_BVHBuilderType;
extern char* g_sBVHBuilderTypeNames[BVHBuilderType_MAX];

void D3dRendererInitialize();
void D3dRendererUninitialize();
void D3dRendererSceneBegin(DeviceResources* deviceResources);
void D3dRendererSceneEnd();
void D3dRendererFlightStart();
void D3dRenderLasersHook(int A4);
void D3dRenderMiniatureHook(int A4, int A8, int AC, int A10, int A14);
void D3dRenderHyperspaceLinesHook(int A4);
void D3dRendererMainHook(SceneCompData* scene);
void D3dRendererShadowHook(SceneCompData* scene);
void D3dRendererOptLoadHook(int handle);
void D3dRendererOptNodeHook(OptHeader* optHeader, int nodeIndex, SceneCompData* scene);
