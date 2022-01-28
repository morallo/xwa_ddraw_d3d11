#pragma once

#include "EffectsRenderer.h"

class SteamVRRenderer : public VRRenderer {
public:
	SteamVRRenderer();
	virtual void SceneBegin(DeviceResources* deviceResources);
	virtual void SceneEnd();
	virtual void MainSceneHook(const SceneCompData* scene);
	virtual void RenderScene();
	virtual void ExtraPreprocessing();
	virtual void CreateShaders();
	virtual void CreateStates();

protected:
	ComPtr<ID3D11VertexShader> _VRVertexShader;
};

extern SteamVRRenderer g_steamvr_renderer;
