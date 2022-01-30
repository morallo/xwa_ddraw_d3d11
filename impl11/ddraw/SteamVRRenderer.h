#pragma once

#include "EffectsRenderer.h"

class SteamVRRenderer : public VRRenderer {
public:
	SteamVRRenderer();
	virtual void SceneBegin(DeviceResources* deviceResources);
	virtual void SceneEnd();
	virtual void RenderScene();
	virtual void CreateShaders();

protected:
	ComPtr<ID3D11VertexShader> _VRVertexShader;
};

extern SteamVRRenderer g_steamvr_renderer;
