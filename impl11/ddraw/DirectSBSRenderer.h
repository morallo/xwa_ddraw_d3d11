#pragma once

#include "EffectsRenderer.h"

class DirectSBSRenderer : public EffectsRenderer {
public:
	DirectSBSRenderer();
	virtual void SceneBegin(DeviceResources* deviceResources);
	virtual void SceneEnd();
	virtual void RenderScene();
	virtual void CreateShaders();

protected:
	ComPtr<ID3D11VertexShader> _vertexShaderVR;
	ComPtr<ID3D11VertexShader> _shadowVertexShaderVR;
};

extern DirectSBSRenderer g_directsbs_renderer;
