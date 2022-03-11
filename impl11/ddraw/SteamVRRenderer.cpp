/*
 * TODO:
 * - The hangar backdrop "swims" sometimes.
 *	 Maybe the backdrops swim when cockpit roll happens?
 * - Do we need to pay extra attention to all the deferred calls? (transparency, lasers...)
 * - Engine Glow (Z-Fighting)
 * - There are no explosions! (Z-Fighting?)
 * - Shadows are messed up when there's camera roll, this is probably also true for the pancake version
 * - External camera is kind of broken
 * - Sometimes the background "swims" while in the hangar
 * - Some HUD elements "swim", including the brackets, but not sure when -- seems to happen con cockpit camera roll
 * - Tech Room?
 * - The DS2 mission is messed up...
 */
#include "SteamVRRenderer.h"

#ifdef _DEBUG
#include "../Debug/XwaD3dVertexShaderVR.h"
#include "../Debug/XwaD3dShadowVertexShaderVR.h"
#else
#include "../Release/XwaD3dVertexShaderVR.h"
#include "../Release/XwaD3dShadowVertexShaderVR.h"
#endif


// ****************************************************
// External variables
// ****************************************************

//************************************************************************
// SteamVR Renderer
//************************************************************************

SteamVRRenderer g_steamvr_renderer;

SteamVRRenderer::SteamVRRenderer() : EffectsRenderer() {
}

void SteamVRRenderer::CreateShaders()
{
	ID3D11Device* device = _deviceResources->_d3dDevice;

	EffectsRenderer::CreateShaders();

	device->CreateVertexShader(g_XwaD3dVertexShaderVR, sizeof(g_XwaD3dVertexShaderVR), nullptr, &_vertexShaderVR);
	device->CreateVertexShader(g_XwaD3dShadowVertexShaderVR, sizeof(g_XwaD3dShadowVertexShaderVR), nullptr, &_shadowVertexShaderVR);
}

void SteamVRRenderer::SceneBegin(DeviceResources* deviceResources)
{
	EffectsRenderer::SceneBegin(deviceResources);
}

void SteamVRRenderer::SceneEnd()
{
	EffectsRenderer::SceneEnd();
}

void SteamVRRenderer::RenderScene()
{
	if (_deviceResources->_displayWidth == 0 || _deviceResources->_displayHeight == 0)
	{
		return;
	}

	if (g_rendererType == RendererType_Shadow)
		// Using the _shadowVertexShaderVR is too expensive: we end up rendering the same scene 4 times.
		// Instead, let's do nothing here and just use the new hangar soft shadow system.
		// resources->InitVertexShader(_shadowVertexShaderVR);
		return;

	auto &resources = _deviceResources;
	auto &context = resources->_d3dDeviceContext;

	unsigned short scissorLeft = *(unsigned short*)0x07D5244;
	unsigned short scissorTop = *(unsigned short*)0x07CA354;
	unsigned short scissorWidth = *(unsigned short*)0x08052B8;
	unsigned short scissorHeight = *(unsigned short*)0x07B33BC;
	float scaleX = _viewport.Width / _deviceResources->_displayWidth;
	float scaleY = _viewport.Height / _deviceResources->_displayHeight;
	D3D11_RECT scissor{};
	scissor.left = (LONG)(_viewport.TopLeftX + scissorLeft * scaleX + 0.5f);
	scissor.top = (LONG)(_viewport.TopLeftY + scissorTop * scaleY + 0.5f);
	scissor.right = scissor.left + (LONG)(scissorWidth * scaleX + 0.5f);
	scissor.bottom = scissor.top + (LONG)(scissorHeight * scaleY + 0.5f);
	_deviceResources->InitScissorRect(&scissor);

	// TODO: Implement instanced rendering so that we issue only one draw call to
	// render both eyes.
	
	// Regular VR path
	resources->InitVertexShader(_vertexShaderVR);

	D3D11_VIEWPORT viewport;
	viewport.Width = (float)resources->_backbufferWidth;
	viewport.Height = (float)resources->_backbufferHeight;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.MinDepth = D3D11_MIN_DEPTH;
	viewport.MaxDepth = D3D11_MAX_DEPTH;
	resources->InitViewport(&viewport);

	// Ensure that we're not overriding regular depth in this path
	g_VSCBuffer.z_override = -1.0f;
	g_VSCBuffer.sz_override = -1.0f;
	g_VSCBuffer.mult_z_override = -1.0f;
	// Add extra depth to Floating GUI elements
	if (g_bIsFloating3DObject) {
		_bModifiedShaders = true;
		g_VSCBuffer.z_override += g_fFloatingGUIObjDepth;
	}
	resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);

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