/*
 * TODO:
 * - There's (apparently) no disparity between the left and right images
 * - Hangar shadows need a custom render method
 * - The only way to enable SteamVR is by hard-coding the current renderer in XwaD3dRendererHook.cpp
 */
#include "SteamVRRenderer.h"

#ifdef _DEBUG
#include "../Debug/XwaD3dVRVertexShader.h"
#else
#include "../Release/XwaD3dVRVertexShader.h"
#endif


// ****************************************************
// External variables
// ****************************************************

//************************************************************************
// SteamVR Renderer
//************************************************************************

SteamVRRenderer g_steamvr_renderer;

SteamVRRenderer::SteamVRRenderer() : VRRenderer() {
}

void SteamVRRenderer::CreateShaders()
{
	ID3D11Device* device = _deviceResources->_d3dDevice;

	VRRenderer::CreateShaders();

	device->CreateVertexShader(g_XwaD3dVRVertexShader, sizeof(g_XwaD3dVRVertexShader), nullptr, &_VRVertexShader);
}

void SteamVRRenderer::CreateStates()
{
	ID3D11Device* device = _deviceResources->_d3dDevice;

	VRRenderer::CreateStates();
}

void SteamVRRenderer::SceneBegin(DeviceResources* deviceResources)
{
	VRRenderer::SceneBegin(deviceResources);
}

void SteamVRRenderer::SceneEnd()
{
	VRRenderer::SceneEnd();
}

void SteamVRRenderer::ExtraPreprocessing()
{
	VRRenderer::ExtraPreprocessing();
}

void SteamVRRenderer::MainSceneHook(const SceneCompData* scene)
{
	auto &resources = _deviceResources;

	VRRenderer::MainSceneHook(scene);

	D3D11_VIEWPORT viewport;
	viewport.Width = (float)resources->_backbufferWidth;
	viewport.Height = (float)resources->_backbufferHeight;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	resources->InitViewport(&viewport);
	resources->InitVertexShader(_VRVertexShader);
}

void SteamVRRenderer::RenderScene()
{
	auto &resources = _deviceResources;
	auto &context = resources->_d3dDeviceContext;

	// TODO: Implement instanced rendering so that we issue only one draw call to
	// render both eyes.

	// ****************************************************************************
	// Render the left image
	// ****************************************************************************
	{
		ID3D11RenderTargetView *rtvs[6] = {
			SelectOffscreenBuffer(_bIsCockpit || _bIsGunner /* || bIsReticle */),
			resources->_renderTargetViewBloomMask.Get(),
			resources->_renderTargetViewDepthBuf.Get(), 
			// The normals hook should not be allowed to write normals for light textures. This is now implemented
			// in XwaD3dPixelShader
			_deviceResources->_renderTargetViewNormBuf.Get(),
			// Blast Marks are confused with glass because they are not shadeless; but they have transparency
			_bIsBlastMark ? NULL : resources->_renderTargetViewSSAOMask.Get(),
			_bIsBlastMark ? NULL : resources->_renderTargetViewSSMask.Get(),
		};
		context->OMSetRenderTargets(6, rtvs, resources->_depthStencilViewL.Get());

		// Set the left projection matrix
		g_VSMatrixCB.projEye = g_FullProjMatrixLeft;
		// The viewMatrix is set at the beginning of the frame
		resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);

		context->DrawIndexed(_trianglesCount * 3, 0, 0);
	}

	// ****************************************************************************
	// Render the right image
	// ****************************************************************************
	{
		ID3D11RenderTargetView *rtvs[6] = {
			SelectOffscreenBuffer(_bIsCockpit || _bIsGunner /* || bIsReticle */, true),
			resources->_renderTargetViewBloomMaskR.Get(),
			resources->_renderTargetViewDepthBufR.Get(),
			// The normals hook should not be allowed to write normals for light textures. This is now implemented
			// in XwaD3dPixelShader
			_deviceResources->_renderTargetViewNormBufR.Get(),
			// Blast Marks are confused with glass because they are not shadeless; but they have transparency
			_bIsBlastMark ? NULL : resources->_renderTargetViewSSAOMaskR.Get(),
			_bIsBlastMark ? NULL : resources->_renderTargetViewSSMaskR.Get(),
		};
		context->OMSetRenderTargets(6, rtvs, resources->_depthStencilViewR.Get());

		// Set the right projection matrix
		g_VSMatrixCB.projEye = g_FullProjMatrixRight;
		// The viewMatrix is set at the beginning of the frame
		resources->InitVSConstantBufferMatrix(resources->_VSMatrixBuffer.GetAddressOf(), &g_VSMatrixCB);

		context->DrawIndexed(_trianglesCount * 3, 0, 0);
	}

//out:
	g_iD3DExecuteCounter++;
}
