/*
 * List of known issues:
 * - Faceted normal maps --> Not an issue with ddraw, OPTs have the wrong normals.
 * - VR
 * - Animations (including Debris)
 * - Animated Explosions (do they even work now?)
 * - Explosion Variants
 * - The Map was semi-fixed apparently as a side-effect of fixing the artifacts in the CMD.
 *   Turns out the map lines were being captured in the Dynamic Cockpit FG buffer, probably
 *   because we didn't detect the miniature being rendered when in map mode.
 *
 * New ideas that might be possible now:
 *
 * - Control the position of ships hypering in or out. Make them *snap* into place
 * - Animate the 3D cockpit. Moving joysticks, targetting computer, etc
 * - Aiming Reticle with lead
 * - Global shadow mapping?
 * - Ray-tracing? Do we have all the unculled, unclipped 3D data now?
 *		Use Intel Embree to build the BVH, one ray per pixel to compute global shadows
 *		outside the cockpit.
 * - Universal head tracking through the tranformWorldView matrix
 * - Parallax mapping: we have the OPT -> Viewspace transform matrix now!
 */
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ScreenGrab.h>
#include <wincodec.h>

#include "globals.h"
#include "common.h"
#include "commonVR.h"
#include "XwaD3dRendererHook.h"
#include "DeviceResources.h"
#include "Direct3DTexture.h"
#include "Matrices.h"
#include "joystick.h"

#ifdef _DEBUG
#include "../Debug/XwaD3dVertexShader.h"
#include "../Debug/XwaD3dPixelShader.h"
#include "../Debug/XwaD3dShadowVertexShader.h"
#include "../Debug/XwaD3dShadowPixelShader.h"
#else
#include "../Release/XwaD3dVertexShader.h"
#include "../Release/XwaD3dPixelShader.h"
#include "../Release/XwaD3dShadowVertexShader.h"
#include "../Release/XwaD3dShadowPixelShader.h"
#endif

#undef LOGGER_DUMP
#define LOGGER_DUMP 0

#pragma pack(push, 1)

struct D3dGlobalLight
{
	float direction[4];
	float color[4];
};

struct D3dLocalLight
{
	float position[4];
	float color[4];
};

struct D3dConstants
{
	float viewportScale[4];
	float projectionValue1;
	float projectionValue2;
	float projectionDeltaX;
	float projectionDeltaY;
	float floorLevel;
	float cameraPositionX;
	float cameraPositionY;
	float hangarShadowAccStartEnd;
	float s_V0x0662814;
	float s_V0x064D1A4;
	float s_V0x06626F4;
	float s_V0x063D194;
	float transformView[16];
	float transformWorldView[16];
	int globalLightsCount;
	int localLightsCount;
	int renderType;
	int renderTypeIllum;
	D3dGlobalLight globalLights[8];
	D3dLocalLight localLights[8];
};

static_assert(sizeof(D3dConstants) % 16 == 0, "D3dConstants size must be multiple of 16");

struct D3dVertex
{
	int iV;
	int iN;
	int iT;
	int c;
};

struct D3dTriangle
{
	int v1;
	int v2;
	int v3;
};

#pragma pack(pop)

#if LOGGER_DUMP

std::ofstream g_d3d_file;

void DumpFile(std::string filename)
{
	g_d3d_file.open(filename);
}

void DumpConstants(D3dConstants& constants)
{
	std::ostringstream str;
	str << std::setprecision(30);

	str << "\tConstants" << std::endl;
	str << "\t" << constants.viewportScale[0] << "; " << constants.viewportScale[1] << "; " << constants.viewportScale[2] << "; " << constants.viewportScale[3] << std::endl;
	str << "\t" << constants.projectionValue1 << std::endl;
	str << "\t" << constants.projectionValue2 << std::endl;
	str << "\t" << constants.projectionDeltaX << std::endl;
	str << "\t" << constants.projectionDeltaY << std::endl;

	str << "\t" << constants.transformWorldView[0] << "; " << constants.transformWorldView[1] << "; " << constants.transformWorldView[2] << "; " << constants.transformWorldView[3] << std::endl;
	str << "\t" << constants.transformWorldView[4] << "; " << constants.transformWorldView[5] << "; " << constants.transformWorldView[6] << "; " << constants.transformWorldView[7] << std::endl;
	str << "\t" << constants.transformWorldView[8] << "; " << constants.transformWorldView[9] << "; " << constants.transformWorldView[10] << "; " << constants.transformWorldView[11] << std::endl;
	str << "\t" << constants.transformWorldView[12] << "; " << constants.transformWorldView[13] << "; " << constants.transformWorldView[14] << "; " << constants.transformWorldView[15] << std::endl;

	g_d3d_file << str.str() << std::endl;
}

void DumpVector3(XwaVector3* vertices, int count)
{
	std::ostringstream str;
	str << std::setprecision(30);

	for (int index = 0; index < count; index++)
	{
		XwaVector3 v = vertices[index];

		str << "\tVERT" << index << ":";
		str << "\t( " << v.x << "\t; " << v.y << "\t; " << v.z << " )";
		str << std::endl;
	}

	g_d3d_file << str.str() << std::endl;
}

void DumpTextureVertices(XwaTextureVertex* vertices, int count)
{
	std::ostringstream str;
	str << std::setprecision(30);

	for (int index = 0; index < count; index++)
	{
		XwaTextureVertex v = vertices[index];

		str << "\tTEX" << index << ":";
		str << "\t( " << v.u << "\t; " << v.v << " )";
		str << std::endl;
	}

	g_d3d_file << str.str() << std::endl;
}

void DumpD3dVertices(D3dVertex* vertices, int count)
{
	std::ostringstream str;

	for (int index = 0; index < count; index++)
	{
		D3dVertex v = vertices[index];

		str << "\tTRI" << index << ":";
		str << "\t( " << v.iV << "\t; " << v.iT << " )";
		str << std::endl;
	}

	g_d3d_file << str.str() << std::endl;
}

#endif

struct DrawCommand {
	Direct3DTexture *colortex, *lighttex;
	ID3D11ShaderResourceView *vertexSRV, *normalsSRV, *texturesSRV;
	ID3D11Buffer *vertexBuffer, *indexBuffer;
	int trianglesCount;
	D3dConstants constants;
	// Extra fields needed for transparent layers
	ID3D11ShaderResourceView *SRVs[2];
	PixelShaderCBuffer PSCBuffer;
	DCPixelShaderCBuffer DCPSCBuffer;
	bool bIsCockpit, bIsGunner, bIsBlastMark;
	ComPtr<ID3D11PixelShader> pixelShader;
};

static bool g_isInRenderLasers = false;
// The following flag cannot be used reliably to tell when the miniature is being rendered: it's
// false if we're in External Camera mode even if a craft is targeted.
static bool g_isInRenderMiniature = false;
static bool g_isInRenderHyperspaceLines = false;

bool IsInsideTriangle(Vector2 P, Vector2 A, Vector2 B, Vector2 C, float *u, float *v);

// ****************************************************
// External variables
// ****************************************************
extern bool g_bIsTargetHighlighted, g_bPrevIsTargetHighlighted, g_bAOEnabled;
extern bool g_bExplosionsDisplayedOnCurrentFrame, g_bEnableVR, g_bProceduralLava;
extern bool g_bIsScaleableGUIElem, g_bScaleableHUDStarted, g_bIsTrianglePointer;
extern float g_fSSAOAlphaOfs;

// ****************************************************
// Debug variables
// ****************************************************
extern bool g_bDumpSSAOBuffers;
int g_iD3DExecuteCounter = 0, g_iD3DExecuteCounterSkipHi = -1, g_iD3DExecuteCounterSkipLo = -1;

void IncreaseD3DExecuteCounterSkipHi(int Delta) {
	g_iD3DExecuteCounterSkipHi += Delta;
	if (g_iD3DExecuteCounterSkipHi < -1)
		g_iD3DExecuteCounterSkipHi = -1;
	log_debug("[DBG] g_iD3DExecuteCounterSkip, Lo: %d, Hi: %d", g_iD3DExecuteCounterSkipLo, g_iD3DExecuteCounterSkipHi);
}

void IncreaseD3DExecuteCounterSkipLo(int Delta) {
	g_iD3DExecuteCounterSkipLo += Delta;
	if (g_iD3DExecuteCounterSkipLo < -1)
		g_iD3DExecuteCounterSkipLo = -1;
	log_debug("[DBG] g_iD3DExecuteCounterSkip, Lo: %d, Hi: %d", g_iD3DExecuteCounterSkipLo, g_iD3DExecuteCounterSkipHi);
}

// ****************************************************
// Temporary, we may not need these here
// ****************************************************
extern bool g_bPrevIsScaleableGUIElem, g_bStartedGUI;
extern bool g_bIsFloating3DObject, g_bPrevIsFloatingGUI3DObject;
extern bool g_bTargetCompDrawn;
bool g_bEnableAnimations = true;
extern HyperspacePhaseEnum g_HyperspacePhaseFSM;

Matrix4 ComputeLightViewMatrix(int idx, Matrix4 &Heading, bool invert);

// ****************************************************
// Dump to OBJ
// ****************************************************
// Set the following flag to true to enable dumping the current scene to an OBJ file
bool bD3DDumpOBJEnabled = false;
FILE *D3DDumpOBJFile = NULL, *D3DDumpLaserOBJFile = NULL;
int D3DOBJFileIdx = 0, D3DTotalVertices = 0, D3DOBJGroup = 0;
int D3DOBJLaserFileIdx = 0, D3DTotalLaserVertices = 0, D3DTotalLaserTextureVertices = 0, D3DOBJLaserGroup = 0;

void OBJDump(XwaVector3 *vertices, int count)
{
	static int obj_idx = 1;
	log_debug("[DBG] OBJDump, count: %d, obj_idx: %d", count, obj_idx);

	fprintf(D3DDumpOBJFile, "o obj-%d\n", obj_idx++);
	for (int index = 0; index < count; index++) {
		XwaVector3 v = vertices[index];
		fprintf(D3DDumpOBJFile, "v %0.6f %0.6f %0.6f\n", v.x, v.y, v.z);
	}
	fprintf(D3DDumpOBJFile, "\n");
}

inline Matrix4 XwaTransformToMatrix4(const XwaTransform &M)
{
	return Matrix4(
		M.Rotation._11, M.Rotation._12, M.Rotation._13, 0.0f,
		M.Rotation._21, M.Rotation._22, M.Rotation._23, 0.0f,
		M.Rotation._31, M.Rotation._32, M.Rotation._33, 0.0f,
		M.Position.x,   M.Position.y,   M.Position.z,   1.0f
	);
}

inline Vector4 XwaVector3ToVector4(const XwaVector3 &V)
{
	return Vector4(V.x, V.y, V.z, 1.0f);
}

inline Vector2 XwaTextureVertexToVector2(const XwaTextureVertex &V)
{
	return Vector2(V.u, V.v);
}

void OBJDumpD3dVertices(const SceneCompData *scene)
{
	std::ostringstream str;
	XwaVector3 *MeshVertices = scene->MeshVertices;
	int MeshVerticesCount = *(int*)((int)scene->MeshVertices - 8);
	static XwaVector3 *LastMeshVertices = nullptr;
	static int LastMeshVerticesCount = 0;

	if (D3DOBJGroup == 1)
		LastMeshVerticesCount = 0;

	if (LastMeshVertices != MeshVertices) {
		// This is a new mesh, dump all the vertices.
		log_debug("[DBG] Writting obj_idx: %d, MeshVerticesCount: %d, FacesCount: %d",
			D3DOBJGroup, MeshVerticesCount, scene->FacesCount);
		fprintf(D3DDumpOBJFile, "o obj-%d\n", D3DOBJGroup);
		
		Matrix4 T = XwaTransformToMatrix4(scene->WorldViewTransform);
		for (int i = 0; i < MeshVerticesCount; i++) {
			XwaVector3 v = MeshVertices[i];
			Vector4 V(v.x, v.y, v.z, 1.0f);
			V = T * V;
			// OPT to meters conversion:
			V *= OPT_TO_METERS;
			fprintf(D3DDumpOBJFile, "v %0.6f %0.6f %0.6f\n", V.x, V.y, V.z);
		}
		fprintf(D3DDumpOBJFile, "\n");
		D3DTotalVertices += LastMeshVerticesCount;
		D3DOBJGroup++;

		LastMeshVertices = MeshVertices;
		LastMeshVerticesCount = MeshVerticesCount;
	}

	// The following works alright, but it's not how things are rendered.
	for (int faceIndex = 0; faceIndex < scene->FacesCount; faceIndex++) {
		OptFaceDataNode_01_Data_Indices& faceData = scene->FaceIndices[faceIndex];
		int edgesCount = faceData.Edge[3] == -1 ? 3 : 4;
		std::string line = "f ";

		for (int vertexIndex = 0; vertexIndex < edgesCount; vertexIndex++)
		{
			// faceData.Vertex[vertexIndex] matches the vertex index data from the OPT
			line += std::to_string(faceData.Vertex[vertexIndex] + D3DTotalVertices) + " ";
		}
		fprintf(D3DDumpOBJFile, "%s\n", line.c_str());
	}
	fprintf(D3DDumpOBJFile, "\n");
}

enum RendererType
{
	RendererType_Unknown,
	RendererType_Main,
	RendererType_Shadow,
};

static RendererType g_rendererType = RendererType_Unknown;

class D3dRenderer
{
public:
	D3dRenderer();
	virtual void SceneBegin(DeviceResources* deviceResources);
	virtual void SceneEnd();
	void FlightStart();
	virtual void MainSceneHook(const SceneCompData* scene);
	void HangarShadowSceneHook(const SceneCompData* scene);
	virtual void UpdateTextures(const SceneCompData* scene);
	void UpdateMeshBuffers(const SceneCompData* scene);
	void ResizeDataVector(const SceneCompData* scene);
	void CreateDataScene(const SceneCompData* scene);
	void UpdateVertexAndIndexBuffers(const SceneCompData* scene);
	void UpdateConstantBuffer(const SceneCompData* scene);
	virtual void RenderScene();
	virtual void RenderDeferredDrawCalls();
	void BuildGlowMarks(SceneCompData* scene);
	void RenderGlowMarks();
	void Initialize();
	void CreateConstantBuffer();
	void CreateStates();
	void CreateShaders();
	void GetViewport(D3D11_VIEWPORT* viewport);
	void GetViewportScale(float* viewportScale);

protected:
	DeviceResources* _deviceResources;
	ID3D11Buffer *_lastVertexBuffer, *_lastIndexBuffer;

	int _totalVerticesCount;
	int _totalTrianglesCount;

	int _currentFaceIndex;

	int _verticesCount;
	std::vector<D3dVertex> _vertices;
	int _trianglesCount;
	std::vector<D3dTriangle> _triangles;

	std::vector<XwaD3dVertex> _glowMarksVertices;
	std::vector<XwaD3dTriangle> _glowMarksTriangles;

	bool _isInitialized;
	UINT _meshBufferInitialCount;
	std::map<int, ComPtr<ID3D11ShaderResourceView>> _meshVerticesViews;
	std::map<int, ComPtr<ID3D11ShaderResourceView>> _meshNormalsViews;
	std::map<int, ComPtr<ID3D11ShaderResourceView>> _meshTextureCoordsViews;
	XwaVector3* _lastMeshVertices;
	ID3D11ShaderResourceView* _lastMeshVerticesView;
	XwaVector3* _lastMeshVertexNormals;
	ID3D11ShaderResourceView* _lastMeshVertexNormalsView;
	XwaTextureVertex* _lastMeshTextureVertices;
	ID3D11ShaderResourceView* _lastMeshTextureVerticesView;
	std::map<int, ComPtr<ID3D11Buffer>> _vertexBuffers;
	std::map<int, ComPtr<ID3D11Buffer>> _triangleBuffers;
	std::map<int, std::pair<int, int>> _vertexCounters;
	D3dConstants _constants;
	ComPtr<ID3D11Buffer> _constantBuffer;
	ComPtr<ID3D11RasterizerState> _rasterizerState;
	ComPtr<ID3D11RasterizerState> _rasterizerStateWireframe;
	ComPtr<ID3D11SamplerState> _samplerState;
	ComPtr<ID3D11BlendState> _solidBlendState;
	ComPtr<ID3D11DepthStencilState> _solidDepthState;
	ComPtr<ID3D11BlendState> _transparentBlendState;
	ComPtr<ID3D11DepthStencilState> _transparentDepthState;
	ComPtr<ID3D11InputLayout> _inputLayout;
	ComPtr<ID3D11VertexShader> _vertexShader;
	ComPtr<ID3D11PixelShader> _pixelShader;
	ComPtr<ID3D11VertexShader> _shadowVertexShader;
	ComPtr<ID3D11PixelShader> _shadowPixelShader;
	D3D11_VIEWPORT _viewport;
};

D3dRenderer::D3dRenderer()
{
	_isInitialized = false;
	_meshBufferInitialCount = 65536;
	_lastMeshVertices = nullptr;
	_lastMeshVerticesView = nullptr;
	_lastMeshVertexNormals = nullptr;
	_lastMeshVertexNormalsView = nullptr;
	_lastMeshTextureVertices = nullptr;
	_lastMeshTextureVerticesView = nullptr;
	_constants = {};
	_viewport = {};

#if LOGGER_DUMP
	DumpFile("ddraw_d3d.txt");
#endif
}

void D3dRenderer::SceneBegin(DeviceResources* deviceResources)
{
	_deviceResources = deviceResources;

	if (!_isInitialized)
	{
		Initialize();
		_isInitialized = true;
	}

	_totalVerticesCount = 0;
	_totalTrianglesCount = 0;

	_lastMeshVertices = nullptr;
	_lastMeshVerticesView = nullptr;
	_lastMeshVertexNormals = nullptr;
	_lastMeshVertexNormalsView = nullptr;
	_lastMeshTextureVertices = nullptr;
	_lastMeshTextureVerticesView = nullptr;

	GetViewport(&_viewport);
	GetViewportScale(_constants.viewportScale);
}

void D3dRenderer::SceneEnd()
{
}

void D3dRenderer::FlightStart()
{
	_meshVerticesViews.clear();
	_meshNormalsViews.clear();
	_meshTextureCoordsViews.clear();
	_vertexBuffers.clear();
	_triangleBuffers.clear();
	_vertexCounters.clear();
}

void D3dRenderer::MainSceneHook(const SceneCompData* scene)
{
	ID3D11DeviceContext* context = _deviceResources->_d3dDeviceContext;

	ComPtr<ID3D11Buffer> oldVSConstantBuffer;
	ComPtr<ID3D11Buffer> oldPSConstantBuffer;
	ComPtr<ID3D11ShaderResourceView> oldVSSRV[3];

	context->VSGetConstantBuffers(0, 1, oldVSConstantBuffer.GetAddressOf());
	context->PSGetConstantBuffers(0, 1, oldPSConstantBuffer.GetAddressOf());
	context->VSGetShaderResources(0, 3, oldVSSRV[0].GetAddressOf());

	context->VSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());
	context->PSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());
	_deviceResources->InitRasterizerState(_rasterizerState);
	_deviceResources->InitSamplerState(_samplerState.GetAddressOf(), nullptr);

	if (scene->TextureAlphaMask == 0)
	{
		_deviceResources->InitBlendState(_solidBlendState, nullptr);
		_deviceResources->InitDepthStencilState(_solidDepthState, nullptr);
	}
	else
	{
		_deviceResources->InitBlendState(_transparentBlendState, nullptr);
		_deviceResources->InitDepthStencilState(_transparentDepthState, nullptr);
	}

	_deviceResources->InitViewport(&_viewport);
	_deviceResources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_deviceResources->InitInputLayout(_inputLayout);
	_deviceResources->InitVertexShader(_vertexShader);
	_deviceResources->InitPixelShader(_pixelShader);

	UpdateTextures(scene);
	UpdateMeshBuffers(scene);
	UpdateVertexAndIndexBuffers(scene);
	UpdateConstantBuffer(scene);
	RenderScene();

	context->VSSetConstantBuffers(0, 1, oldVSConstantBuffer.GetAddressOf());
	context->PSSetConstantBuffers(0, 1, oldPSConstantBuffer.GetAddressOf());
	context->VSSetShaderResources(0, 3, oldVSSRV[0].GetAddressOf());

#if LOGGER_DUMP
	DumpConstants(_constants);
	DumpVector3(scene->MeshVertices, *(int*)((int)scene->MeshVertices - 8));
	DumpTextureVertices(scene->MeshTextureVertices, *(int*)((int)scene->MeshTextureVertices - 8));
	DumpD3dVertices(_vertices.data(), _verticesCount);
#endif
}

void D3dRenderer::HangarShadowSceneHook(const SceneCompData* scene)
{
	ID3D11DeviceContext* context = _deviceResources->_d3dDeviceContext;

	ComPtr<ID3D11Buffer> oldVSConstantBuffer;
	ComPtr<ID3D11Buffer> oldPSConstantBuffer;
	ComPtr<ID3D11ShaderResourceView> oldVSSRV[3];

	context->VSGetConstantBuffers(0, 1, oldVSConstantBuffer.GetAddressOf());
	context->PSGetConstantBuffers(0, 1, oldPSConstantBuffer.GetAddressOf());
	context->VSGetShaderResources(0, 3, oldVSSRV[0].GetAddressOf());

	context->VSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());
	context->PSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());
	_deviceResources->InitRasterizerState(_rasterizerState);
	_deviceResources->InitSamplerState(_samplerState.GetAddressOf(), nullptr);

	_deviceResources->InitBlendState(_solidBlendState, nullptr);
	_deviceResources->InitDepthStencilState(_solidDepthState, nullptr, 0U);

	_deviceResources->InitViewport(&_viewport);
	_deviceResources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_deviceResources->InitInputLayout(_inputLayout);
	_deviceResources->InitVertexShader(_shadowVertexShader);
	_deviceResources->InitPixelShader(_shadowPixelShader);

	UpdateTextures(scene);
	UpdateMeshBuffers(scene);
	UpdateVertexAndIndexBuffers(scene);
	UpdateConstantBuffer(scene);
	RenderScene();

	context->VSSetConstantBuffers(0, 1, oldVSConstantBuffer.GetAddressOf());
	context->PSSetConstantBuffers(0, 1, oldPSConstantBuffer.GetAddressOf());
	context->VSSetShaderResources(0, 3, oldVSSRV[0].GetAddressOf());
}

void D3dRenderer::UpdateTextures(const SceneCompData* scene)
{
	const unsigned char ShipCategory_PlayerProjectile = 6;
	const unsigned char ShipCategory_OtherProjectile = 7;
	unsigned char category = scene->pObject->ShipCategory;
	bool isProjectile = category == ShipCategory_PlayerProjectile || category == ShipCategory_OtherProjectile;

	const auto XwaD3dTextureCacheUpdateOrAdd = (void(*)(XwaTextureSurface*))0x00597784;
	const auto L00432750 = (short(*)(unsigned short, short, short))0x00432750;
	const ExeEnableEntry* s_ExeEnableTable = (ExeEnableEntry*)0x005FB240;

	XwaTextureSurface* surface = nullptr;
	XwaTextureSurface* surface2 = nullptr;

	_constants.renderType = 0;
	_constants.renderTypeIllum = 0;

	if (g_isInRenderLasers)
	{
		_constants.renderType = 2;
	}

	if (isProjectile)
	{
		_constants.renderType = 2;
	}

	if (scene->D3DInfo != nullptr)
	{
		surface = scene->D3DInfo->ColorMap[0];

		if (scene->D3DInfo->LightMap[0] != nullptr)
		{
			surface2 = scene->D3DInfo->LightMap[0];
			_constants.renderTypeIllum = 1;
		}
	}
	else
	{
		const unsigned short ModelIndex_237_1000_0_ResData_LightingEffects = 237;
		L00432750(ModelIndex_237_1000_0_ResData_LightingEffects, 0x02, 0x100);
		XwaSpeciesTMInfo* esi = (XwaSpeciesTMInfo*)s_ExeEnableTable[ModelIndex_237_1000_0_ResData_LightingEffects].pData1;
		surface = (XwaTextureSurface*)esi->pData;
	}

	XwaD3dTextureCacheUpdateOrAdd(surface);

	if (surface2 != nullptr)
	{
		XwaD3dTextureCacheUpdateOrAdd(surface2);
	}

	Direct3DTexture* texture = (Direct3DTexture*)surface->D3dTexture.D3DTextureHandle;
	Direct3DTexture* texture2 = surface2 == nullptr ? nullptr : (Direct3DTexture*)surface2->D3dTexture.D3DTextureHandle;
	_deviceResources->InitPSShaderResourceView(texture->_textureView, texture2 == nullptr ? nullptr : texture2->_textureView.Get());
}

void D3dRenderer::UpdateMeshBuffers(const SceneCompData* scene)
{
	ID3D11Device* device = _deviceResources->_d3dDevice;
	ID3D11DeviceContext* context = _deviceResources->_d3dDeviceContext;

	XwaVector3* vertices = scene->MeshVertices;
	XwaVector3* normals = scene->MeshVertexNormals;
	XwaTextureVertex* textureCoords = scene->MeshTextureVertices;

	if (vertices != _lastMeshVertices)
	{
		_lastMeshVertices = vertices;
		_currentFaceIndex = 0;

		auto it = _meshVerticesViews.find((int)vertices);

		if (it != _meshVerticesViews.end())
		{
			_lastMeshVerticesView = it->second;
		}
		else
		{
			int verticesCount = *(int*)((int)vertices - 8);

			D3D11_SUBRESOURCE_DATA initialData;
			initialData.pSysMem = vertices;
			initialData.SysMemPitch = 0;
			initialData.SysMemSlicePitch = 0;

			ComPtr<ID3D11Buffer> meshVerticesBuffer;
			device->CreateBuffer(&CD3D11_BUFFER_DESC(verticesCount * sizeof(XwaVector3), D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_IMMUTABLE), &initialData, &meshVerticesBuffer);

			ID3D11ShaderResourceView* meshVerticesView;
			device->CreateShaderResourceView(meshVerticesBuffer, &CD3D11_SHADER_RESOURCE_VIEW_DESC(meshVerticesBuffer, DXGI_FORMAT_R32G32B32_FLOAT, 0, verticesCount), &meshVerticesView);

			_meshVerticesViews.insert(std::make_pair((int)vertices, meshVerticesView));
			_lastMeshVerticesView = meshVerticesView;
		}
	}

	if (normals != _lastMeshVertexNormals)
	{
		_lastMeshVertexNormals = normals;

		auto it = _meshNormalsViews.find((int)normals);

		if (it != _meshNormalsViews.end())
		{
			_lastMeshVertexNormalsView = it->second;
		}
		else
		{
			int normalsCount = *(int*)((int)normals - 8);

			D3D11_SUBRESOURCE_DATA initialData;
			initialData.pSysMem = normals;
			initialData.SysMemPitch = 0;
			initialData.SysMemSlicePitch = 0;

			ComPtr<ID3D11Buffer> meshNormalsBuffer;
			device->CreateBuffer(&CD3D11_BUFFER_DESC(normalsCount * sizeof(XwaVector3), D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_IMMUTABLE), &initialData, &meshNormalsBuffer);

			ID3D11ShaderResourceView* meshNormalsView;
			device->CreateShaderResourceView(meshNormalsBuffer, &CD3D11_SHADER_RESOURCE_VIEW_DESC(meshNormalsBuffer, DXGI_FORMAT_R32G32B32_FLOAT, 0, normalsCount), &meshNormalsView);

			_meshNormalsViews.insert(std::make_pair((int)normals, meshNormalsView));
			_lastMeshVertexNormalsView = meshNormalsView;
		}
	}

	if (textureCoords != _lastMeshTextureVertices)
	{
		_lastMeshTextureVertices = textureCoords;

		auto it = _meshTextureCoordsViews.find((int)textureCoords);

		if (it != _meshTextureCoordsViews.end())
		{
			_lastMeshTextureVerticesView = it->second;
		}
		else
		{
			int textureCoordsCount = *(int*)((int)textureCoords - 8);

			D3D11_SUBRESOURCE_DATA initialData;
			initialData.pSysMem = textureCoords;
			initialData.SysMemPitch = 0;
			initialData.SysMemSlicePitch = 0;

			ComPtr<ID3D11Buffer> meshTextureCoordsBuffer;
			device->CreateBuffer(&CD3D11_BUFFER_DESC(textureCoordsCount * sizeof(XwaTextureVertex), D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_IMMUTABLE), &initialData, &meshTextureCoordsBuffer);

			ID3D11ShaderResourceView* meshTextureCoordsView;
			device->CreateShaderResourceView(meshTextureCoordsBuffer, &CD3D11_SHADER_RESOURCE_VIEW_DESC(meshTextureCoordsBuffer, DXGI_FORMAT_R32G32_FLOAT, 0, textureCoordsCount), &meshTextureCoordsView);

			_meshTextureCoordsViews.insert(std::make_pair((int)textureCoords, meshTextureCoordsView));
			_lastMeshTextureVerticesView = meshTextureCoordsView;
		}
	}

	ID3D11ShaderResourceView* vsSSRV[3] = { _lastMeshVerticesView, _lastMeshVertexNormalsView, _lastMeshTextureVerticesView };
	context->VSSetShaderResources(0, 3, vsSSRV);
}

void D3dRenderer::ResizeDataVector(const SceneCompData* scene)
{
	size_t verticesRequiredSize = scene->FacesCount * 4;
	size_t trianglesRequiredSize = scene->FacesCount * 2;

	if (_vertices.capacity() < verticesRequiredSize)
	{
		_vertices.reserve(verticesRequiredSize * 2);
	}

	if (_triangles.capacity() < trianglesRequiredSize)
	{
		_triangles.reserve(trianglesRequiredSize * 2);
	}
}

void D3dRenderer::CreateDataScene(const SceneCompData* scene)
{
	const unsigned char ShipCategory_PlayerProjectile = 6;
	const unsigned char ShipCategory_OtherProjectile = 7;
	unsigned char category = scene->pObject->ShipCategory;
	int isProjectile = (category == ShipCategory_PlayerProjectile || category == ShipCategory_OtherProjectile) ? 1 : 0;

	D3dVertex* v = _vertices.data();
	D3dTriangle* t = _triangles.data();
	int verticesIndex = 0;
	_verticesCount = 0;
	_trianglesCount = 0;

	for (int faceIndex = 0; faceIndex < scene->FacesCount; faceIndex++)
	{
		OptFaceDataNode_01_Data_Indices& faceData = scene->FaceIndices[faceIndex];
		int edgesCount = faceData.Edge[3] == -1 ? 3 : 4;

		for (int vertexIndex = 0; vertexIndex < edgesCount; vertexIndex++)
		{
			v->iV = faceData.Vertex[vertexIndex];
			v->iN = faceData.VertexNormal[vertexIndex];
			v->iT = faceData.TextureVertex[vertexIndex];
			//v->c = isProjectile;
			v->c = 0;
			v++;
		}

		_verticesCount += edgesCount;

		// This converts quads into 2 tris if necessary
		for (int edge = 2; edge < edgesCount; edge++)
		{
			t->v1 = verticesIndex;
			t->v2 = verticesIndex + edge - 1;
			t->v3 = verticesIndex + edge;
			t++;
		}

		verticesIndex += edgesCount;
		_trianglesCount += edgesCount - 2;
	}

	_totalVerticesCount += _verticesCount;
	_totalTrianglesCount += _trianglesCount;
}

void D3dRenderer::UpdateVertexAndIndexBuffers(const SceneCompData* scene)
{
	ID3D11Device* device = _deviceResources->_d3dDevice;
	OptFaceDataNode_01_Data_Indices* faceData = scene->FaceIndices;

	ID3D11Buffer* vertexBuffer;
	ID3D11Buffer* indexBuffer;
	int verticesCount;
	int trianglesCount;

	auto itVertex = _vertexBuffers.find((int)faceData);
	auto itTriangle = _triangleBuffers.find((int)faceData);
	auto itCounters = _vertexCounters.find((int)faceData);

	if (itVertex != _vertexBuffers.end())
	{
		vertexBuffer = itVertex->second;
		indexBuffer = itTriangle->second;
		verticesCount = itCounters->second.first;
		trianglesCount = itCounters->second.second;
	}
	else
	{
		ResizeDataVector(scene);
		CreateDataScene(scene);

		D3D11_SUBRESOURCE_DATA initialData;
		initialData.SysMemPitch = 0;
		initialData.SysMemSlicePitch = 0;

		initialData.pSysMem = _vertices.data();
		device->CreateBuffer(&CD3D11_BUFFER_DESC(_verticesCount * sizeof(D3dVertex), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_IMMUTABLE), &initialData, &vertexBuffer);

		initialData.pSysMem = _triangles.data();
		device->CreateBuffer(&CD3D11_BUFFER_DESC(_trianglesCount * sizeof(D3dTriangle), D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_IMMUTABLE), &initialData, &indexBuffer);

		verticesCount = _verticesCount;
		trianglesCount = _trianglesCount;

		_vertexBuffers.insert(std::make_pair((int)faceData, vertexBuffer));
		_triangleBuffers.insert(std::make_pair((int)faceData, indexBuffer));
		_vertexCounters.insert(std::make_pair((int)faceData, std::make_pair(_verticesCount, _trianglesCount)));
	}

	UINT vertexBufferStride = sizeof(D3dVertex);
	UINT vertexBufferOffset = 0;
	_deviceResources->InitVertexBuffer(nullptr, nullptr, nullptr);
	_deviceResources->InitVertexBuffer(&vertexBuffer, &vertexBufferStride, &vertexBufferOffset);
	_deviceResources->InitIndexBuffer(nullptr, true);
	_deviceResources->InitIndexBuffer(indexBuffer, true);

	_lastVertexBuffer = vertexBuffer;
	_lastIndexBuffer = indexBuffer;
	_verticesCount = verticesCount;
	_trianglesCount = trianglesCount;

	unsigned int& s_SceneNumVerts = *(unsigned int*)0x009EAA00;
	unsigned int& s_SceneNumTris = *(unsigned int*)0x009EA96C;
	unsigned int& s_SceneNumTexChanges = *(unsigned int*)0x009EA984;
	unsigned int& s_SceneNumStateChanges = *(unsigned int*)0x009EA980;

	s_SceneNumVerts += verticesCount;
	s_SceneNumTris += trianglesCount;
	s_SceneNumStateChanges += 1;
}

void D3dRenderer::UpdateConstantBuffer(const SceneCompData* scene)
{
	ID3D11DeviceContext* context = _deviceResources->_d3dDeviceContext;

	switch (g_rendererType)
	{
	case RendererType_Main:
	default:
		_constants.projectionValue1 = *(float*)0x08B94CC;
		_constants.projectionValue2 = *(float*)0x05B46B4;
		_constants.projectionDeltaX = *(float*)0x08C1600 + *(float*)0x0686ACC;
		_constants.projectionDeltaY = *(float*)0x080ACF8 + *(float*)0x07B33C0 + *(float*)0x064D1AC;
		_constants.floorLevel = 0.0f;
		_constants.cameraPositionX = (float)(*(int*)(0x08B94E0 + *(int*)0x08C1CC8 * 0xBCF + 0xB48 + 0x00));
		_constants.cameraPositionY = (float)(*(int*)(0x08B94E0 + *(int*)0x08C1CC8 * 0xBCF + 0xB48 + 0x04));
		_constants.hangarShadowAccStartEnd = 0.0f;
		break;

	case RendererType_Shadow:
		_constants.projectionValue1 = *(float*)0x08B94CC;
		_constants.projectionValue2 = *(float*)0x05B46B4;
		_constants.projectionDeltaX = (float)(*(short*)0x08052B8 / 2) + *(float*)0x0686ACC;
		_constants.projectionDeltaY = (float)(*(short*)0x07B33BC / 2 + *(int*)0x080811C) + *(float*)0x064D1AC;
		_constants.floorLevel = *(float*)0x0686B34 - -1.5f - (float)(*(int*)(0x08B94E0 + *(int*)0x08C1CC8 * 0xBCF + 0xB48 + 0x08));
		_constants.cameraPositionX = (float)(*(int*)(0x08B94E0 + *(int*)0x08C1CC8 * 0xBCF + 0xB48 + 0x00));
		_constants.cameraPositionY = (float)(*(int*)(0x08B94E0 + *(int*)0x08C1CC8 * 0xBCF + 0xB48 + 0x04));
		_constants.hangarShadowAccStartEnd = *(short*)0x0686B38 != 0 ? 1.0f : 0.0f;
		_constants.s_V0x0662814 = *(float*)0x0662814;
		_constants.s_V0x064D1A4 = *(float*)0x064D1A4;
		_constants.s_V0x06626F4 = *(float*)0x06626F4;
		_constants.s_V0x063D194 = *(float*)0x063D194;
		break;
	}

	// transformView only seems to be used when rendering shadows in the hangar.
	_constants.transformView[0] = *(float*)0x007B4BEC;
	_constants.transformView[1] = *(float*)0x007B6FF8;
	_constants.transformView[2] = *(float*)0x007B33DC;
	_constants.transformView[3] = 0.0f;
	_constants.transformView[4] = *(float*)0x007B4BE8;
	_constants.transformView[5] = *(float*)0x007B6FF0;
	_constants.transformView[6] = *(float*)0x007B33D8;
	_constants.transformView[7] = 0.0f;
	_constants.transformView[8] = *(float*)0x007B4BF4;
	_constants.transformView[9] = *(float*)0x007B33D4;
	_constants.transformView[10] = *(float*)0x007B4BE4;
	_constants.transformView[11] = 0.0f;
	_constants.transformView[12] = 0.0f;
	_constants.transformView[13] = 0.0f;
	_constants.transformView[14] = 0.0f;
	_constants.transformView[15] = 1.0f;

	_constants.transformWorldView[0] = scene->WorldViewTransform.Rotation._11;
	_constants.transformWorldView[1] = scene->WorldViewTransform.Rotation._21;
	_constants.transformWorldView[2] = scene->WorldViewTransform.Rotation._31;
	_constants.transformWorldView[3] = scene->WorldViewTransform.Position.x;
	_constants.transformWorldView[4] = scene->WorldViewTransform.Rotation._12;
	_constants.transformWorldView[5] = scene->WorldViewTransform.Rotation._22;
	_constants.transformWorldView[6] = scene->WorldViewTransform.Rotation._32;
	_constants.transformWorldView[7] = scene->WorldViewTransform.Position.y;
	_constants.transformWorldView[8] = scene->WorldViewTransform.Rotation._13;
	_constants.transformWorldView[9] = scene->WorldViewTransform.Rotation._23;
	_constants.transformWorldView[10] = scene->WorldViewTransform.Rotation._33;
	_constants.transformWorldView[11] = scene->WorldViewTransform.Position.z;
	_constants.transformWorldView[12] = 0.0f;
	_constants.transformWorldView[13] = 0.0f;
	_constants.transformWorldView[14] = 0.0f;
	_constants.transformWorldView[15] = 1.0f;

	int s_XwaGlobalLightsCount = *(int*)0x00782848;
	XwaGlobalLight* s_XwaGlobalLights = (XwaGlobalLight*)0x007D4FA0;
	int s_XwaLocalLightsCount = *(int*)0x00782844;
	XwaLocalLight* s_XwaLocalLights = (XwaLocalLight*)0x008D42C0;

	_constants.globalLightsCount = s_XwaGlobalLightsCount;

	for (int i = 0; i < 8; i++)
	{
		_constants.globalLights[i].direction[0] = s_XwaGlobalLights[i].DirectionX;
		_constants.globalLights[i].direction[1] = s_XwaGlobalLights[i].DirectionY;
		_constants.globalLights[i].direction[2] = s_XwaGlobalLights[i].DirectionZ;
		_constants.globalLights[i].direction[3] = 1.0f;

		_constants.globalLights[i].color[0] = s_XwaGlobalLights[i].ColorR;
		_constants.globalLights[i].color[1] = s_XwaGlobalLights[i].ColorG;
		_constants.globalLights[i].color[2] = s_XwaGlobalLights[i].ColorB;
		_constants.globalLights[i].color[3] = s_XwaGlobalLights[i].Intensity;
	}

	_constants.localLightsCount = s_XwaLocalLightsCount;

	for (int i = 0; i < 8; i++)
	{
		_constants.localLights[i].position[0] = s_XwaLocalLights[i].fPositionX;
		_constants.localLights[i].position[1] = s_XwaLocalLights[i].fPositionY;
		_constants.localLights[i].position[2] = s_XwaLocalLights[i].fPositionZ;
		_constants.localLights[i].position[3] = 1.0f;

		_constants.localLights[i].color[0] = s_XwaLocalLights[i].ColorR;
		_constants.localLights[i].color[1] = s_XwaLocalLights[i].ColorG;
		_constants.localLights[i].color[2] = s_XwaLocalLights[i].ColorB;
		_constants.localLights[i].color[3] = s_XwaLocalLights[i].m18;
	}

	context->UpdateSubresource(_constantBuffer, 0, nullptr, &_constants, 0, 0);
}

void D3dRenderer::RenderScene()
{
	ID3D11DeviceContext* context = _deviceResources->_d3dDeviceContext;

	context->DrawIndexed(_trianglesCount * 3, 0, 0);
}

void D3dRenderer::RenderDeferredDrawCalls()
{

}

void D3dRenderer::BuildGlowMarks(SceneCompData* scene)
{
	_glowMarksVertices.clear();
	_glowMarksTriangles.clear();

	const auto XwaD3dTextureCacheUpdateOrAdd = (void(*)(XwaTextureSurface*))0x00597784;
	const auto XwaVector3Transform = (void(*)(XwaVector3*, const XwaMatrix3x3*))0x00439B30;
	const auto L004E8110 = (void(*)(SceneCompData*, XwaKnockoutData*))0x004E8110;

	const XwaObject* s_XwaObjects = *(XwaObject**)0x007B33C4;
	const XwaObject3D* s_XwaObjects3D = *(XwaObject3D**)0x00782854;
	const int s_XwaIsInConcourse = *(int*)0x005FFD9C;

	scene->GlowMarksCount = 0;

	if (s_XwaIsInConcourse == 0)
	{
		int objectIndex = scene->pObject - s_XwaObjects;
		XwaKnockoutData* knockout = s_XwaObjects3D[objectIndex].pKnockoutData;

		if (knockout != nullptr)
		{
			L004E8110(scene, knockout);
		}
	}

	for (int glowMarkIndex = 0; glowMarkIndex < scene->GlowMarksCount; glowMarkIndex++)
	{
		XwaGlowMark* glowMark = scene->GlowMarks[glowMarkIndex];

		for (int glowMarkItemIndex = 0; glowMarkItemIndex < glowMark->Count; glowMarkItemIndex++)
		{
			XwaGlowMarkItem* glowMarkItem = &glowMark->Items[glowMarkItemIndex];

			if (glowMarkItem->pTextureSurface == nullptr)
			{
				continue;
			}

			XwaD3dTextureCacheUpdateOrAdd(glowMarkItem->pTextureSurface);

			int baseFaceIndex = _currentFaceIndex;

			for (int faceIndex = 0; faceIndex < scene->FacesCount; faceIndex++)
			{
				unsigned short faceOn = glowMark->FaceOn[baseFaceIndex + faceIndex];

				if (faceOn == 0)
				{
					continue;
				}

				OptFaceDataNode_01_Data_Indices& faceData = scene->FaceIndices[faceIndex];
				int edgesCount = faceData.Edge[3] == -1 ? 3 : 4;
				int verticesIndex = _glowMarksVertices.size();

				for (int vertexIndex = 0; vertexIndex < edgesCount; vertexIndex++)
				{
					int iV = faceData.Vertex[vertexIndex];
					int iN = faceData.VertexNormal[vertexIndex];
					int iT = faceData.TextureVertex[vertexIndex];

					XwaD3dVertex v;
					v.x = scene->MeshVertices[iV].x;
					v.y = scene->MeshVertices[iV].y;
					v.z = scene->MeshVertices[iV].z;
					v.rhw = 0;
					v.color = glowMarkItem->Color;
					v.specular = 0;
					v.tu = glowMark->UVArray[iV].u * glowMarkItem->Size + 0.5f;
					v.tv = glowMark->UVArray[iV].v * glowMarkItem->Size + 0.5f;

					XwaVector3Transform((XwaVector3*)&v.x, &scene->WorldViewTransform.Rotation);
					v.x += scene->WorldViewTransform.Position.x;
					v.y += scene->WorldViewTransform.Position.y;
					v.z += scene->WorldViewTransform.Position.z;

					float st0 = _constants.projectionValue1 / v.z;
					v.x = v.x * st0 + _constants.projectionDeltaX;
					v.y = v.y * st0 + _constants.projectionDeltaY;
					v.z = (st0 * _constants.projectionValue2) / (abs(st0) * _constants.projectionValue2 + _constants.projectionValue1);
					v.rhw = st0;

					_glowMarksVertices.push_back(v);
				}

				for (int edge = 2; edge < edgesCount; edge++)
				{
					XwaD3dTriangle t;

					t.v1 = verticesIndex;
					t.v2 = verticesIndex + edge - 1;
					t.v3 = verticesIndex + edge;
					t.RenderStatesFlags = *(unsigned int*)0x06626F0;
					t.pTexture = &glowMarkItem->pTextureSurface->D3dTexture;

					_glowMarksTriangles.push_back(t);
				}
			}
		}
	}

	_currentFaceIndex += scene->FacesCount;
}

void D3dRenderer::RenderGlowMarks()
{
	if (_glowMarksTriangles.size() == 0)
	{
		return;
	}

	const auto XwaD3dExecuteBufferLock = (char(*)())0x00595006;
	const auto XwaD3dExecuteBufferAddVertices = (char(*)(void*, int))0x00595095;
	const auto XwaD3dExecuteBufferProcessVertices = (char(*)())0x00595106;
	const auto XwaD3dExecuteBufferAddTriangles = (char(*)(void*, int))0x00595191;
	const auto XwaD3dExecuteBufferUnlockAndExecute = (char(*)())0x005954D6;

	XwaD3dExecuteBufferLock();
	XwaD3dExecuteBufferAddVertices(_glowMarksVertices.data(), _glowMarksVertices.size());
	XwaD3dExecuteBufferProcessVertices();
	XwaD3dExecuteBufferAddTriangles(_glowMarksTriangles.data(), _glowMarksTriangles.size());
	XwaD3dExecuteBufferUnlockAndExecute();

	_glowMarksVertices.clear();
	_glowMarksTriangles.clear();
}

void D3dRenderer::Initialize()
{
	if (_deviceResources->_d3dFeatureLevel < D3D_FEATURE_LEVEL_10_0)
	{
		MessageBox(nullptr, "The D3d renderer hook requires a graphic card that supports D3D_FEATURE_LEVEL_10_0.", "X-Wing Alliance DDraw", MB_ICONWARNING);
		return;
	}

	CreateConstantBuffer();
	CreateStates();
	CreateShaders();
}

void D3dRenderer::CreateConstantBuffer()
{
	ID3D11Device* device = _deviceResources->_d3dDevice;

	// constant buffer
	device->CreateBuffer(&CD3D11_BUFFER_DESC(sizeof(D3dConstants), D3D11_BIND_CONSTANT_BUFFER), nullptr, &_constantBuffer);
}

void D3dRenderer::CreateStates()
{
	ID3D11Device* device = _deviceResources->_d3dDevice;

	D3D11_RASTERIZER_DESC rsDesc{};
	rsDesc.FillMode = g_config.WireframeFillMode ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
	rsDesc.CullMode = D3D11_CULL_NONE;
	rsDesc.FrontCounterClockwise = FALSE;
	rsDesc.DepthBias = 0;
	rsDesc.DepthBiasClamp = 0.0f;
	rsDesc.SlopeScaledDepthBias = 0.0f;
	rsDesc.DepthClipEnable = TRUE;
	rsDesc.ScissorEnable = FALSE;
	rsDesc.MultisampleEnable = FALSE;
	rsDesc.AntialiasedLineEnable = FALSE;
	device->CreateRasterizerState(&rsDesc, &_rasterizerState);

	rsDesc.FillMode = D3D11_FILL_WIREFRAME;
	device->CreateRasterizerState(&rsDesc, &_rasterizerStateWireframe);

	D3D11_SAMPLER_DESC samplerDesc;
	samplerDesc.Filter = _deviceResources->_useAnisotropy ? D3D11_FILTER_ANISOTROPIC : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.MaxAnisotropy = _deviceResources->_useAnisotropy ? _deviceResources->GetMaxAnisotropy() : 1;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = FLT_MAX;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.BorderColor[0] = 0.0f;
	samplerDesc.BorderColor[1] = 0.0f;
	samplerDesc.BorderColor[2] = 0.0f;
	samplerDesc.BorderColor[3] = 0.0f;
	device->CreateSamplerState(&samplerDesc, &_samplerState);

	D3D11_BLEND_DESC solidBlendDesc{};
	solidBlendDesc.AlphaToCoverageEnable = FALSE;
	solidBlendDesc.IndependentBlendEnable = FALSE;
	solidBlendDesc.RenderTarget[0].BlendEnable = FALSE;
	solidBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	solidBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	solidBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	solidBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	solidBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	solidBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	solidBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	device->CreateBlendState(&solidBlendDesc, &_solidBlendState);

	D3D11_DEPTH_STENCIL_DESC solidDepthDesc{};
	solidDepthDesc.DepthEnable = TRUE;
	solidDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	solidDepthDesc.DepthFunc = D3D11_COMPARISON_GREATER;
	solidDepthDesc.StencilEnable = FALSE;
	device->CreateDepthStencilState(&solidDepthDesc, &_solidDepthState);

	D3D11_BLEND_DESC transparentBlendDesc{};
	transparentBlendDesc.AlphaToCoverageEnable = FALSE;
	transparentBlendDesc.IndependentBlendEnable = FALSE;
	transparentBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	transparentBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	transparentBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	transparentBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	transparentBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	transparentBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	transparentBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	transparentBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	device->CreateBlendState(&transparentBlendDesc, &_transparentBlendState);

	D3D11_DEPTH_STENCIL_DESC transparentDepthDesc{};
	transparentDepthDesc.DepthEnable = TRUE;
	transparentDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	transparentDepthDesc.DepthFunc = D3D11_COMPARISON_GREATER;
	transparentDepthDesc.StencilEnable = FALSE;
	device->CreateDepthStencilState(&transparentDepthDesc, &_transparentDepthState);
}

void D3dRenderer::CreateShaders()
{
	ID3D11Device* device = _deviceResources->_d3dDevice;

	device->CreateVertexShader(g_XwaD3dVertexShader, sizeof(g_XwaD3dVertexShader), nullptr, &_vertexShader);

	const D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	device->CreateInputLayout(vertexLayoutDesc, ARRAYSIZE(vertexLayoutDesc), g_XwaD3dVertexShader, sizeof(g_XwaD3dVertexShader), &_inputLayout);

	device->CreatePixelShader(g_XwaD3dPixelShader, sizeof(g_XwaD3dPixelShader), nullptr, &_pixelShader);

	device->CreateVertexShader(g_XwaD3dShadowVertexShader, sizeof(g_XwaD3dShadowVertexShader), nullptr, &_shadowVertexShader);
	device->CreatePixelShader(g_XwaD3dShadowPixelShader, sizeof(g_XwaD3dShadowPixelShader), nullptr, &_shadowPixelShader);
}

void D3dRenderer::GetViewport(D3D11_VIEWPORT* viewport)
{
	UINT w;
	UINT h;

	if (g_config.AspectRatioPreserved)
	{
		if (_deviceResources->_backbufferHeight * _deviceResources->_displayWidth <= _deviceResources->_backbufferWidth * _deviceResources->_displayHeight)
		{
			w = _deviceResources->_backbufferHeight * _deviceResources->_displayWidth / _deviceResources->_displayHeight;
			h = _deviceResources->_backbufferHeight;
		}
		else
		{
			w = _deviceResources->_backbufferWidth;
			h = _deviceResources->_backbufferWidth * _deviceResources->_displayHeight / _deviceResources->_displayWidth;
		}
	}
	else
	{
		w = _deviceResources->_backbufferWidth;
		h = _deviceResources->_backbufferHeight;
	}

	UINT left = (_deviceResources->_backbufferWidth - w) / 2;
	UINT top = (_deviceResources->_backbufferHeight - h) / 2;

	viewport->TopLeftX = (float)left;
	viewport->TopLeftY = (float)top;
	viewport->Width = (float)w;
	viewport->Height = (float)h;
	viewport->MinDepth = D3D11_MIN_DEPTH;
	viewport->MaxDepth = D3D11_MAX_DEPTH;
}

void D3dRenderer::GetViewportScale(float* viewportScale)
{
	float scale;

	if (_deviceResources->_frontbufferSurface == nullptr)
	{
		scale = 1.0f;
	}
	else
	{
		if (_deviceResources->_backbufferHeight * _deviceResources->_displayWidth <= _deviceResources->_backbufferWidth * _deviceResources->_displayHeight)
		{
			scale = (float)_deviceResources->_backbufferHeight / (float)_deviceResources->_displayHeight;
		}
		else
		{
			scale = (float)_deviceResources->_backbufferWidth / (float)_deviceResources->_displayWidth;
		}

		scale *= g_config.Concourse3DScale;
	}

	viewportScale[0] = 2.0f / (float)_deviceResources->_displayWidth;
	viewportScale[1] = -2.0f / (float)_deviceResources->_displayHeight;
	viewportScale[2] = scale;
	viewportScale[3] = 0.0f;
}

static D3dRenderer g_xwa_d3d_renderer;

//************************************************************************
// Effects Renderer
//************************************************************************

class EffectsRenderer : public D3dRenderer
{
private:
	bool _bLastTextureSelectedNotNULL, _bLastLightmapSelectedNotNULL, _bIsLaser, _bIsCockpit;
	bool _bIsGunner, _bIsExplosion, _bIsBlastMark, _bHasMaterial, _bDCIsTransparent, _bDCElemAlwaysVisible;
	bool _bModifiedShaders, _bModifiedPixelShader, _bModifiedBlendState, _bModifiedSamplerState;
	bool _bIsNoisyHolo, _bWarheadLocked, _bIsTargetHighlighted, _bIsHologram, _bRenderingLightingEffect;
	bool _bCockpitConstantsCaptured, _bExternalCamera, _bCockpitDisplayed, _bIsTransparentCall;
	bool _bShadowsRenderedInCurrentFrame, _bJoystickTransformReady;
	D3dConstants _CockpitConstants;
	XwaTransform _CockpitWorldView;
	Direct3DTexture *_lastTextureSelected = nullptr;
	Direct3DTexture *_lastLightmapSelected = nullptr;
	std::vector<DrawCommand> _LaserDrawCommands;
	std::vector<DrawCommand> _TransparentDrawCommands;
	Matrix4 _joystickMeshTransform;

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

	HRESULT QuickSetZWriteEnabled(BOOL Enabled);
	void EnableTransparency();
	void EnableHoloTransparency();
	inline ID3D11RenderTargetView *SelectOffscreenBuffer(bool bIsMaskable, bool bSteamVRRightEye);
	Matrix4 GetShadowMapLimits(Matrix4 L, float *OBJrange, float *OBJminZ);

	void SaveContext();
	void RestoreContext();

public:
	EffectsRenderer();
	virtual void SceneBegin(DeviceResources* deviceResources);
	virtual void SceneEnd();
	virtual void MainSceneHook(const SceneCompData* scene);
	virtual void RenderScene();
	virtual void UpdateTextures(const SceneCompData* scene);
	// State Management
	void DoStateManagement(const SceneCompData* scene);
	void ApplyMaterialProperties();
	void ApplySpecialMaterials();
	void ApplyBloomSettings();
	void DCCaptureMiniature();
	// Returns true if the current draw call needs to be skipped
	bool DCReplaceTextures();
	virtual void ExtraPreprocessing();
	void AddLaserLights(const SceneCompData* scene);
	void ApplyProceduralLava();
	void ApplyGreebles();
	void ApplyAnimatedTextures();

	// Deferred rendering
	void RenderLasers();
	void RenderTransparency();
	void RenderShadowMap();
	void RenderDeferredDrawCalls();
};

EffectsRenderer::EffectsRenderer() : D3dRenderer() {

}

void EffectsRenderer::SceneBegin(DeviceResources* deviceResources)
{
	D3dRenderer::SceneBegin(deviceResources);

	// Reset any deferred-rendering variables here
	_LaserDrawCommands.clear();
	_TransparentDrawCommands.clear();
	_bCockpitConstantsCaptured = false;
	_bShadowsRenderedInCurrentFrame = false;
	// Initialize the joystick mesh transform on this frame
	_bJoystickTransformReady = false;
	_joystickMeshTransform.identity();
	g_OPTMeshTransformCB.MeshTransform.identity();
	_deviceResources->InitVSConstantOPTMeshTransform(
		_deviceResources->_OPTMeshTransformCB.GetAddressOf(), &g_OPTMeshTransformCB);

	if (PlayerDataTable->missionTime == 0)
		ApplyCustomHUDColor();

	// Initialize the OBJ dump file for the current frame
	if (bD3DDumpOBJEnabled && g_bDumpSSAOBuffers) {
		// Create the file if it doesn't exist
		if (D3DDumpOBJFile == NULL) {
			char sFileName[128];
			sprintf_s(sFileName, 128, "d3dcapture-%d.obj", D3DOBJFileIdx);
			fopen_s(&D3DDumpOBJFile, sFileName, "wt");
		}
		// Reset the vertex counter and group
		D3DTotalVertices = 1;
		D3DOBJGroup = 1;

		if (D3DDumpLaserOBJFile == NULL) {
			char sFileName[128];
			sprintf_s(sFileName, 128, "d3dlasers-%d.obj", D3DOBJLaserFileIdx);
			fopen_s(&D3DDumpLaserOBJFile, sFileName, "wt");
		}
		D3DTotalLaserVertices = 1;
		D3DTotalLaserTextureVertices = 1;
		D3DOBJLaserGroup = 1;
	}
}

void EffectsRenderer::SceneEnd()
{
	D3dRenderer::SceneEnd();

	// Close the OBJ dump file for the current frame
	if (bD3DDumpOBJEnabled && g_bDumpSSAOBuffers) {
		fclose(D3DDumpOBJFile); D3DDumpOBJFile = NULL;
		log_debug("[DBG] OBJ file [d3dcapture-%d.obj] written", D3DOBJFileIdx);
		D3DOBJFileIdx++;

		fclose(D3DDumpLaserOBJFile); D3DDumpLaserOBJFile = NULL;
		log_debug("[DBG] OBJ file [d3dlasers-%d.obj] written", D3DOBJLaserFileIdx);
		D3DOBJLaserFileIdx++;
	}
}

/* Function to quickly enable/disable ZWrite. */
HRESULT EffectsRenderer::QuickSetZWriteEnabled(BOOL Enabled)
{
	HRESULT hr;
	auto &resources = _deviceResources;
	auto &device = resources->_d3dDevice;

	D3D11_DEPTH_STENCIL_DESC desc;
	_solidDepthState->GetDesc(&desc);
	ComPtr<ID3D11DepthStencilState> depthState;

	desc.DepthEnable = Enabled;
	desc.DepthWriteMask = Enabled ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = Enabled ? D3D11_COMPARISON_GREATER : D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	hr = resources->_d3dDevice->CreateDepthStencilState(&desc, &depthState);
	if (SUCCEEDED(hr))
		resources->_d3dDeviceContext->OMSetDepthStencilState(depthState, 0);
	return hr;
}

inline void EffectsRenderer::EnableTransparency() {
	auto& resources = _deviceResources;
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
	resources->InitBlendState(nullptr, &blendDesc);
}

inline void EffectsRenderer::EnableHoloTransparency() {
	auto& resources = _deviceResources;
	D3D11_BLEND_DESC blendDesc{};

	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	resources->InitBlendState(nullptr, &blendDesc);
}

void EffectsRenderer::SaveContext()
{
	auto &context = _deviceResources->_d3dDeviceContext;

	_oldVSCBuffer = g_VSCBuffer;
	_oldPSCBuffer = g_PSCBuffer;
	_oldDCPSCBuffer = g_DCPSCBuffer;

	context->VSGetConstantBuffers(0, 1, _oldVSConstantBuffer.GetAddressOf());
	context->PSGetConstantBuffers(0, 1, _oldPSConstantBuffer.GetAddressOf());

	context->VSGetShaderResources(0, 3, _oldVSSRV[0].GetAddressOf());
	context->PSGetShaderResources(0, 13, _oldPSSRV[0].GetAddressOf());

	context->VSGetShader(_oldVertexShader.GetAddressOf(), nullptr, nullptr);
	context->PSGetShader(_oldPixelShader.GetAddressOf(), nullptr, nullptr);

	context->PSGetSamplers(0, 2, _oldPSSamplers[0].GetAddressOf());

	context->OMGetRenderTargets(8, _oldRTVs[0].GetAddressOf(), _oldDSV.GetAddressOf());
	context->OMGetDepthStencilState(_oldDepthStencilState.GetAddressOf(), &_oldStencilRef);
	context->OMGetBlendState(_oldBlendState.GetAddressOf(), _oldBlendFactor, &_oldSampleMask);

	context->IAGetInputLayout(_oldInputLayout.GetAddressOf());
	context->IAGetPrimitiveTopology(&_oldTopology);
	context->IAGetVertexBuffers(0, 1, _oldVertexBuffer.GetAddressOf(), &_oldStride, &_oldOffset);
	context->IAGetIndexBuffer(_oldIndexBuffer.GetAddressOf(), &_oldFormat, &_oldIOffset);

	_oldNumViewports = 2;
	context->RSGetViewports(&_oldNumViewports, _oldViewports);
}

void EffectsRenderer::RestoreContext()
{
	auto &resources = _deviceResources;
	auto &context = _deviceResources->_d3dDeviceContext;

	// Restore a previously-saved context
	g_VSCBuffer = _oldVSCBuffer;
	g_PSCBuffer = _oldPSCBuffer;
	g_DCPSCBuffer = _oldDCPSCBuffer;
	resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);
	resources->InitPSConstantBuffer3D(resources->_PSConstantBuffer.GetAddressOf(), &g_PSCBuffer);
	resources->InitPSConstantBufferDC(resources->_PSConstantBufferDC.GetAddressOf(), &g_DCPSCBuffer);

	// The hyperspace effect needs the current VS constants to work properly
	if (g_HyperspacePhaseFSM == HS_INIT_ST)
		context->VSSetConstantBuffers(0, 1, _oldVSConstantBuffer.GetAddressOf());
	context->PSSetConstantBuffers(0, 1, _oldPSConstantBuffer.GetAddressOf());

	context->VSSetShaderResources(0, 3, _oldVSSRV[0].GetAddressOf());
	context->PSSetShaderResources(0, 13, _oldPSSRV[0].GetAddressOf());

	// It's important to use the Init*Shader methods here, or the shaders won't be
	// applied sometimes.
	resources->InitVertexShader(_oldVertexShader);
	resources->InitPixelShader(_oldPixelShader);

	context->PSSetSamplers(0, 2, _oldPSSamplers[0].GetAddressOf());
	context->OMSetRenderTargets(8, _oldRTVs[0].GetAddressOf(), _oldDSV.Get());
	context->OMSetDepthStencilState(_oldDepthStencilState.Get(), _oldStencilRef);
	context->OMSetBlendState(_oldBlendState.Get(), _oldBlendFactor, _oldSampleMask);

	resources->InitInputLayout(_oldInputLayout);
	resources->InitTopology(_oldTopology);
	context->IASetVertexBuffers(0, 1, _oldVertexBuffer.GetAddressOf(), &_oldStride, &_oldOffset);
	context->IASetIndexBuffer(_oldIndexBuffer.Get(), _oldFormat, _oldIOffset);

	context->RSSetViewports(_oldNumViewports, _oldViewports);

	// Release everything. Previous calls to *Get* increase the refcount
	_oldVSConstantBuffer.Release();
	_oldPSConstantBuffer.Release();

	for (int i = 0; i < 3; i++)
		_oldVSSRV[i].Release();
	for (int i = 0; i < 13; i++)
		_oldPSSRV[i].Release();

	_oldVertexShader.Release();
	_oldPixelShader.Release();

	for (int i = 0; i < 2; i++)
		_oldPSSamplers[i].Release();

	for (int i = 0; i < 8; i++)
		_oldRTVs[i].Release();
	_oldDSV.Release();
	_oldDepthStencilState.Release();
	_oldBlendState.Release();
	_oldInputLayout.Release();
	_oldVertexBuffer.Release();
	_oldIndexBuffer.Release();
}

void EffectsRenderer::UpdateTextures(const SceneCompData* scene)
{
	const unsigned char ShipCategory_PlayerProjectile = 6;
	const unsigned char ShipCategory_OtherProjectile = 7;
	unsigned char category = scene->pObject->ShipCategory;
	bool isProjectile = category == ShipCategory_PlayerProjectile || category == ShipCategory_OtherProjectile;

	const auto XwaD3dTextureCacheUpdateOrAdd = (void(*)(XwaTextureSurface*))0x00597784;
	const auto L00432750 = (short(*)(unsigned short, short, short))0x00432750;
	const ExeEnableEntry* s_ExeEnableTable = (ExeEnableEntry*)0x005FB240;

	XwaTextureSurface* surface = nullptr;
	XwaTextureSurface* surface2 = nullptr;

	_constants.renderType = 0;
	_constants.renderTypeIllum = 0;
	_bRenderingLightingEffect = false;

	if (g_isInRenderLasers || isProjectile)
	{
		_constants.renderType = 2;
	}

	if (scene->D3DInfo != nullptr)
	{
		surface = scene->D3DInfo->ColorMap[0];

		if (scene->D3DInfo->LightMap[0] != nullptr)
		{
			surface2 = scene->D3DInfo->LightMap[0]; // surface2 is a lightmap
			_constants.renderTypeIllum = 1;
		}
	}
	else
	{
		// This is a lighting effect... I wonder which ones? Smoke perhaps?
		_bRenderingLightingEffect = true;
		//log_debug("[DBG] Rendering Lighting Effect");
		const unsigned short ModelIndex_237_1000_0_ResData_LightingEffects = 237;
		L00432750(ModelIndex_237_1000_0_ResData_LightingEffects, 0x02, 0x100);
		XwaSpeciesTMInfo* esi = (XwaSpeciesTMInfo*)s_ExeEnableTable[ModelIndex_237_1000_0_ResData_LightingEffects].pData1;
		surface = (XwaTextureSurface*)esi->pData;
	}

	XwaD3dTextureCacheUpdateOrAdd(surface);

	if (surface2 != nullptr)
	{
		XwaD3dTextureCacheUpdateOrAdd(surface2);
	}

	Direct3DTexture* texture = (Direct3DTexture*)surface->D3dTexture.D3DTextureHandle;
	Direct3DTexture* texture2 = surface2 == nullptr ? nullptr : (Direct3DTexture*)surface2->D3dTexture.D3DTextureHandle;
	_deviceResources->InitPSShaderResourceView(texture->_textureView, texture2 == nullptr ? nullptr : texture2->_textureView.Get());
	_lastTextureSelected = texture;
	_lastLightmapSelected = texture2;
}

void EffectsRenderer::DoStateManagement(const SceneCompData* scene)
{
	// ***************************************************************
	// State management begins here
	// ***************************************************************
	// The following state variables will probably need to be pruned. I suspect we don't
	// need to care about GUI/2D/HUD stuff here anymore.
	// At this point, texture and texture2 have been selected, we can check their names to see if
	// we need to apply effects. If there's a lightmap, it's going to be in texture2.
	// Most of the local flags below should now be class members, but I'll be hand
	_bLastTextureSelectedNotNULL = (_lastTextureSelected != NULL);
	_bLastLightmapSelectedNotNULL = (_lastLightmapSelected != NULL);
	_bIsBlastMark = false;
	_bIsCockpit = false;
	_bIsGunner = false;
	_bIsExplosion = false;
	_bDCIsTransparent = false;
	_bDCElemAlwaysVisible = false;
	_bIsHologram = false;
	_bIsNoisyHolo = false;
	_bWarheadLocked = PlayerDataTable[*g_playerIndex].warheadArmed && PlayerDataTable[*g_playerIndex].warheadLockState == 3;
	_bExternalCamera = PlayerDataTable[*g_playerIndex].Camera.ExternalCamera;
	_bCockpitDisplayed = PlayerDataTable[*g_playerIndex].cockpitDisplayed;

	_bIsTargetHighlighted = false;
	bool bIsGUI = false, bIsLensFlare = false;
	//bool bIsExterior = false, bIsDAT = false;
	//bool bIsActiveCockpit = false,
	//bool bIsDS2CoreExplosion = false;
	bool bIsElectricity = false, bHasMaterial = false;

	if (_bLastTextureSelectedNotNULL) {
		if (g_bDynCockpitEnabled && _lastTextureSelected->is_DynCockpitDst)
		{
			int idx = _lastTextureSelected->DCElementIndex;
			if (idx >= 0 && idx < g_iNumDCElements) {
				_bIsHologram |= g_DCElements[idx].bHologram;
				_bIsNoisyHolo |= g_DCElements[idx].bNoisyHolo;
				_bDCIsTransparent |= g_DCElements[idx].bTransparent;
				_bDCElemAlwaysVisible |= g_DCElements[idx].bAlwaysVisible;
			}
		}

		_bIsLaser = _lastTextureSelected->is_Laser || _lastTextureSelected->is_TurboLaser;
		//_bIsLaser = _constants.renderType == 2;
		g_bIsTargetHighlighted |= _lastTextureSelected->is_HighlightedReticle;
		_bIsTargetHighlighted = g_bIsTargetHighlighted || g_bPrevIsTargetHighlighted;
		if (_bIsTargetHighlighted) g_GameEvent.TargetEvent = TGT_EVT_LASER_LOCK;
		if (PlayerDataTable[*g_playerIndex].warheadArmed) {
			char state = PlayerDataTable[*g_playerIndex].warheadLockState;
			switch (state) {
				// state == 0 warhead armed, no lock
			case 2:
				g_GameEvent.TargetEvent = TGT_EVT_WARHEAD_LOCKING;
				break;
			case 3:
				g_GameEvent.TargetEvent = TGT_EVT_WARHEAD_LOCKED;
				break;
			}
		}
		//bIsLensFlare = lastTextureSelected->is_LensFlare;
		//bIsHyperspaceTunnel = lastTextureSelected->is_HyperspaceAnim;
		_bIsCockpit = _lastTextureSelected->is_CockpitTex;
		_bIsGunner = _lastTextureSelected->is_GunnerTex;
		//bIsExterior = lastTextureSelected->is_Exterior;
		//bIsDAT = lastTextureSelected->is_DAT;
		//bIsActiveCockpit = lastTextureSelected->ActiveCockpitIdx > -1;
		_bIsBlastMark = _lastTextureSelected->is_BlastMark;
		//bIsDS2CoreExplosion = lastTextureSelected->is_DS2_Reactor_Explosion;
		//bIsElectricity = lastTextureSelected->is_Electricity;
		_bHasMaterial = _lastTextureSelected->bHasMaterial;
		_bIsExplosion = _lastTextureSelected->is_Explosion;
		if (_bIsExplosion) g_bExplosionsDisplayedOnCurrentFrame = true;
	}

	//g_bPrevIsPlayerObject = g_bIsPlayerObject;
	//g_bIsPlayerObject = bIsCockpit || bIsExterior || bIsGunner;
	//const bool bIsFloatingGUI = _bLastTextureSelectedNotNULL && _lastTextureSelected->is_Floating_GUI;
	// Hysteresis detection (state is about to switch to render something different, like the HUD)
	g_bPrevIsFloatingGUI3DObject = g_bIsFloating3DObject;
	// Do *not* use g_isInRenderMiniature here, it's not reliable.
	g_bIsFloating3DObject = g_bTargetCompDrawn && _bLastTextureSelectedNotNULL &&
		!_lastTextureSelected->is_Text && !_lastTextureSelected->is_TrianglePointer &&
		!_lastTextureSelected->is_Reticle && !_lastTextureSelected->is_Floating_GUI &&
		!_lastTextureSelected->is_TargetingComp && !bIsLensFlare;

	// The GUI starts rendering whenever we detect a GUI element, or Text, or a bracket.
	// ... or not at all if we're in external view mode with nothing targeted.
	//g_bPrevStartedGUI = g_bStartedGUI;
	// Apr 10, 2020: g_bDisableDiffuse will make the reticle look white when the HUD is
	// hidden. To prevent this, I added bIsAimingHUD to g_bStartedGUI; but I don't know
	// if this breaks VR. If it does, then I need to add !bIsAimingHUD around line 6425,
	// where I'm setting fDisableDiffuse = 1.0f
	//g_bStartedGUI |= bIsGUI || bIsText || bIsBracket || bIsFloatingGUI || bIsReticle;
	// bIsScaleableGUIElem is true when we're about to render a HUD element that can be scaled down with Ctrl+Z
	g_bPrevIsScaleableGUIElem = g_bIsScaleableGUIElem;
	g_bIsScaleableGUIElem = g_bStartedGUI && !g_bIsTrianglePointer && !bIsLensFlare;
	// ***************************************************************
	// State management ends here
	// ***************************************************************
}

// Apply specific material properties for the current texture
void EffectsRenderer::ApplyMaterialProperties()
{
	if (!_bHasMaterial || !_bLastTextureSelectedNotNULL)
		return;

	auto &resources = _deviceResources;

	_bModifiedShaders = true;

	if (_lastTextureSelected->material.IsShadeless)
		g_PSCBuffer.fSSAOMaskVal = SHADELESS_MAT;
	else
		g_PSCBuffer.fSSAOMaskVal	 = _lastTextureSelected->material.Metallic * 0.5f; // Metallicity is encoded in the range 0..0.5 of the SSAOMask
	g_PSCBuffer.fGlossiness		= _lastTextureSelected->material.Glossiness;
	g_PSCBuffer.fSpecInt			= _lastTextureSelected->material.Intensity;
	g_PSCBuffer.fNMIntensity		= _lastTextureSelected->material.NMIntensity;
	g_PSCBuffer.fSpecVal			= _lastTextureSelected->material.SpecValue;
	g_PSCBuffer.fAmbient			= _lastTextureSelected->material.Ambient;

	if (_lastTextureSelected->material.AlphaToBloom) {
		_bModifiedPixelShader = true;
		_bModifiedShaders = true;
		resources->InitPixelShader(resources->_alphaToBloomPS);
		if (_lastTextureSelected->material.NoColorAlpha)
			g_PSCBuffer.special_control.ExclusiveMask = SPECIAL_CONTROL_NO_COLOR_ALPHA;
		g_PSCBuffer.fBloomStrength = _lastTextureSelected->material.EffectBloom;
	}

	// lastTextureSelected can't be a lightmap anymore, so we don't need (?) to test bIsLightTexture
	if (_lastTextureSelected->material.AlphaIsntGlass /* && !bIsLightTexture */) {
		_bModifiedPixelShader = true;
		_bModifiedShaders = true;
		g_PSCBuffer.fBloomStrength = 0.0f;
		resources->InitPixelShader(resources->_noGlassPS);
	}

	if (_lastTextureSelected->material.IsJoystick) {
		if (!_bJoystickTransformReady && g_pSharedData != NULL && g_pSharedData->bDataReady) {
			Vector3 JoystickRoot = _lastTextureSelected->material.JoystickRoot;
			float MaxYaw = _lastTextureSelected->material.JoystickMaxYaw;
			float MaxPitch = _lastTextureSelected->material.JoystickMaxPitch;
			Matrix4 T, R;

			// Translate the JoystickRoot to the origin
			T.identity(); T.translate(-JoystickRoot);
			_joystickMeshTransform = T;

			R.identity(); R.rotateY(MaxYaw * g_pSharedData->pSharedData->JoystickYaw);
			_joystickMeshTransform = R * _joystickMeshTransform;

			R.identity(); R.rotateX(MaxPitch * g_pSharedData->pSharedData->JoystickPitch);
			_joystickMeshTransform = R * _joystickMeshTransform;

			// Return the system to its original position
			T.identity(); T.translate(JoystickRoot);
			_joystickMeshTransform = T * _joystickMeshTransform;

			// We need to transpose the matrix because the Vertex Shader is expecting the
			// matrix in this format
			_joystickMeshTransform.transpose();
			// Avoid computing the transform above more than once per frame:
			_bJoystickTransformReady = true;
		}
		g_OPTMeshTransformCB.MeshTransform = _joystickMeshTransform;
	}
	else
		g_OPTMeshTransformCB.MeshTransform.identity();
	// Set the current mesh transform, regardless of what type of mesh type we're rendering
	resources->InitVSConstantOPTMeshTransform(resources->_OPTMeshTransformCB.GetAddressOf(), &g_OPTMeshTransformCB);
}

// Apply the SSAO mask/Special materials, like lasers and HUD
void EffectsRenderer::ApplySpecialMaterials()
{
	if (!_bLastTextureSelectedNotNULL)
		return;

	if (g_bIsScaleableGUIElem || /* bIsReticle || bIsText || */ g_bIsTrianglePointer ||
		_lastTextureSelected->is_Debris || _lastTextureSelected->is_GenericSSAOMasked ||
		_lastTextureSelected->is_Electricity || _bIsExplosion ||
		_lastTextureSelected->is_Smoke)
	{
		_bModifiedShaders = true;
		g_PSCBuffer.fSSAOMaskVal = SHADELESS_MAT;
		g_PSCBuffer.fGlossiness = DEFAULT_GLOSSINESS;
		g_PSCBuffer.fSpecInt = DEFAULT_SPEC_INT;
		g_PSCBuffer.fNMIntensity = 0.0f;
		g_PSCBuffer.fSpecVal = 0.0f;
		g_PSCBuffer.bIsShadeless = 1;

		g_PSCBuffer.fPosNormalAlpha = 0.0f;
	}
	else if (_lastTextureSelected->is_Debris || _lastTextureSelected->is_Trail ||
		_lastTextureSelected->is_CockpitSpark || _lastTextureSelected->is_Spark ||
		_lastTextureSelected->is_Chaff || _lastTextureSelected->is_Missile
		)
	{
		_bModifiedShaders = true;
		g_PSCBuffer.fSSAOMaskVal = PLASTIC_MAT;
		g_PSCBuffer.fGlossiness = DEFAULT_GLOSSINESS;
		g_PSCBuffer.fSpecInt = DEFAULT_SPEC_INT;
		g_PSCBuffer.fNMIntensity = 0.0f;
		g_PSCBuffer.fSpecVal = 0.0f;

		g_PSCBuffer.fPosNormalAlpha = 0.0f;
	}
	else if (_lastTextureSelected->is_Laser) {
		_bModifiedShaders = true;
		g_PSCBuffer.fSSAOMaskVal = EMISSION_MAT;
		g_PSCBuffer.fGlossiness = DEFAULT_GLOSSINESS;
		g_PSCBuffer.fSpecInt = DEFAULT_SPEC_INT;
		g_PSCBuffer.fNMIntensity = 0.0f;
		g_PSCBuffer.fSpecVal = 0.0f;
		g_PSCBuffer.bIsShadeless = 1;

		g_PSCBuffer.fPosNormalAlpha = 0.0f;
	}
}

// Apply BLOOM flags and 32-bit mode enhancements
void EffectsRenderer::ApplyBloomSettings()
{
	if (!_bLastTextureSelectedNotNULL)
		return;

	if (_bIsLaser) {
		_bModifiedShaders = true;
		g_PSCBuffer.fBloomStrength = _lastTextureSelected->is_Laser ?
			g_BloomConfig.fLasersStrength : g_BloomConfig.fTurboLasersStrength;
		g_PSCBuffer.bIsLaser = g_config.EnhanceLasers ? 2 : 1;
	}
	// Send the flag for light textures (enhance them in 32-bit mode, apply bloom)
	// TODO: Check if the animation for light textures still works
	else if (_bLastLightmapSelectedNotNULL) {
		_bModifiedShaders = true;
		int anim_idx = _lastTextureSelected->material.GetCurrentATCIndex(NULL, LIGHTMAP_ATC_IDX);
		g_PSCBuffer.fBloomStrength = _lastTextureSelected->is_CockpitTex ?
			g_BloomConfig.fCockpitStrength : g_BloomConfig.fLightMapsStrength;
		// If this is an animated light map, then use the right intensity setting
		// TODO: Make the following code more efficient
		if (anim_idx > -1) {
			AnimatedTexControl *atc = &(g_AnimatedMaterials[anim_idx]);
			g_PSCBuffer.fBloomStrength = atc->Sequence[atc->AnimIdx].intensity;
		}
		g_PSCBuffer.bIsLightTexture = g_config.EnhanceIllumination ? 2 : 1;
	}
	// Set the flag for EngineGlow and Explosions (enhance them in 32-bit mode, apply bloom)
	else if (_lastTextureSelected->is_EngineGlow) {
		_bModifiedShaders = true;
		g_PSCBuffer.fBloomStrength = g_BloomConfig.fEngineGlowStrength;
		g_PSCBuffer.bIsEngineGlow = g_config.EnhanceEngineGlow ? 2 : 1;
	}
	else if (_lastTextureSelected->is_Electricity || _bIsExplosion)
	{
		_bModifiedShaders = true;
		g_PSCBuffer.fBloomStrength = g_BloomConfig.fExplosionsStrength;
		g_PSCBuffer.bIsEngineGlow = g_config.EnhanceExplosions ? 2 : 1;
	}
	else if (_lastTextureSelected->is_LensFlare) {
		_bModifiedShaders = true;
		g_PSCBuffer.fBloomStrength = g_BloomConfig.fLensFlareStrength;
		g_PSCBuffer.bIsEngineGlow = 1;
	}
	/*
	// I believe Suns are not rendered here
	else if (bIsSun) {
		bModifiedShaders = true;
		// If there's a 3D sun in the scene, then we shouldn't apply bloom to Sun textures  they should be invisible
		g_PSCBuffer.fBloomStrength = g_b3DSunPresent ? 0.0f : g_BloomConfig.fSunsStrength;
		g_PSCBuffer.bIsEngineGlow = 1;
	} */
	else if (_lastTextureSelected->is_Spark) {
		_bModifiedShaders = true;
		g_PSCBuffer.fBloomStrength = g_BloomConfig.fSparksStrength;
		g_PSCBuffer.bIsEngineGlow = 1;
	}
	else if (_lastTextureSelected->is_CockpitSpark) {
		_bModifiedShaders = true;
		g_PSCBuffer.fBloomStrength = g_BloomConfig.fCockpitSparksStrength;
		g_PSCBuffer.bIsEngineGlow = 1;
	}
	else if (_lastTextureSelected->is_Chaff)
	{
		_bModifiedShaders = true;
		g_PSCBuffer.fBloomStrength = g_BloomConfig.fSparksStrength;
		g_PSCBuffer.bIsEngineGlow = 1;
	}
	else if (_lastTextureSelected->is_Missile)
	{
		_bModifiedShaders = true;
		g_PSCBuffer.fBloomStrength = g_BloomConfig.fMissileStrength;
		g_PSCBuffer.bIsEngineGlow = 1;
	}
	else if (_lastTextureSelected->is_SkydomeLight) {
		_bModifiedShaders = true;
		g_PSCBuffer.fBloomStrength = g_BloomConfig.fSkydomeLightStrength;
		g_PSCBuffer.bIsEngineGlow = 1;
	}
	else if (!_bLastLightmapSelectedNotNULL && _lastTextureSelected->material.GetCurrentATCIndex(NULL) > -1) {
		_bModifiedShaders = true;
		// TODO: Check if this animation still works
		int anim_idx = _lastTextureSelected->material.GetCurrentATCIndex(NULL);
		// If this is an animated light map, then use the right intensity setting
		// TODO: Make the following code more efficient
		if (anim_idx > -1) {
			AnimatedTexControl *atc = &(g_AnimatedMaterials[anim_idx]);
			g_PSCBuffer.fBloomStrength = atc->Sequence[atc->AnimIdx].intensity;
		}
	}

	// Remove Bloom for all textures with materials tagged as "NoBloom"
	if (_bHasMaterial && _lastTextureSelected->material.NoBloom)
	{
		_bModifiedShaders = true;
		g_PSCBuffer.fBloomStrength = 0.0f;
		g_PSCBuffer.bIsEngineGlow = 0;
	}
}

void EffectsRenderer::ExtraPreprocessing()
{
	// Extra processing before the draw call. VR-specific stuff, for instance
}

void EffectsRenderer::AddLaserLights(const SceneCompData* scene)
{
	XwaVector3 *MeshVertices = scene->MeshVertices;
	int MeshVerticesCount = *(int*)((int)scene->MeshVertices - 8);
	XwaTextureVertex *MeshTextureVertices = scene->MeshTextureVertices;
	int MeshTextureVerticesCount = *(int*)((int)scene->MeshTextureVertices - 8);
	Vector4 tempv0, tempv1, tempv2, P;
	Vector2 UV0, UV1, UV2, UV = _lastTextureSelected->material.LightUVCoordPos;
	Matrix4 T = XwaTransformToMatrix4(scene->WorldViewTransform);

	if (g_bDumpSSAOBuffers && bD3DDumpOBJEnabled) {
		// This is a new mesh, dump all the vertices.
		log_debug("[DBG] Writting obj_idx: %d, MeshVerticesCount: %d, TexCount: %d, FacesCount: %d",
			D3DOBJLaserGroup, MeshVerticesCount, MeshTextureVerticesCount, scene->FacesCount);
		fprintf(D3DDumpLaserOBJFile, "o obj-%d\n", D3DOBJLaserGroup);

		Matrix4 T = XwaTransformToMatrix4(scene->WorldViewTransform);
		for (int i = 0; i < MeshVerticesCount; i++) {
			XwaVector3 v = MeshVertices[i];
			Vector4 V(v.x, v.y, v.z, 1.0f);
			V = T * V;
			// OPT to meters conversion:
			V *= OPT_TO_METERS;
			fprintf(D3DDumpLaserOBJFile, "v %0.6f %0.6f %0.6f\n", V.x, V.y, V.z);
		}
		fprintf(D3DDumpLaserOBJFile, "\n");

		for (int i = 0; i < MeshTextureVerticesCount; i++) {
			XwaTextureVertex vt = MeshTextureVertices[i];
			fprintf(D3DDumpLaserOBJFile, "vt %0.3f %0.3f\n", vt.u, vt.v);
		}
		fprintf(D3DDumpLaserOBJFile, "\n");

		D3DOBJLaserGroup++;
	}

	// Here we look at the uv's of each face and see if the current triangle contains
	// the uv coord we're looking for (the default is (0.1, 0.5)). If the uv is contained,
	// then we compute the 3D point using its barycentric coords and add it to the list
	for (int faceIndex = 0; faceIndex < scene->FacesCount; faceIndex++)
	{
		OptFaceDataNode_01_Data_Indices& faceData = scene->FaceIndices[faceIndex];
		int edgesCount = faceData.Edge[3] == -1 ? 3 : 4;
		// This converts quads into 2 tris if necessary
		for (int edge = 2; edge < edgesCount; edge++)
		{
			D3dTriangle t;
			t.v1 = 0;
			t.v2 = edge - 1;
			t.v3 = edge;

			UV0 = XwaTextureVertexToVector2(MeshTextureVertices[faceData.TextureVertex[t.v1]]);
			UV1 = XwaTextureVertexToVector2(MeshTextureVertices[faceData.TextureVertex[t.v2]]);
			UV2 = XwaTextureVertexToVector2(MeshTextureVertices[faceData.TextureVertex[t.v3]]);
			tempv0 = OPT_TO_METERS * T * XwaVector3ToVector4(MeshVertices[faceData.Vertex[t.v1]]);
			tempv1 = OPT_TO_METERS * T * XwaVector3ToVector4(MeshVertices[faceData.Vertex[t.v2]]);
			tempv2 = OPT_TO_METERS * T * XwaVector3ToVector4(MeshVertices[faceData.Vertex[t.v3]]);

			if (g_bDumpSSAOBuffers && bD3DDumpOBJEnabled) {
				fprintf(D3DDumpLaserOBJFile, "f %d/%d %d/%d %d/%d\n",
					faceData.Vertex[t.v1] + D3DTotalLaserVertices, faceData.TextureVertex[t.v1] + D3DTotalLaserTextureVertices,
					faceData.Vertex[t.v2] + D3DTotalLaserVertices, faceData.TextureVertex[t.v2] + D3DTotalLaserTextureVertices,
					faceData.Vertex[t.v3] + D3DTotalLaserVertices, faceData.TextureVertex[t.v3] + D3DTotalLaserTextureVertices);
			}

			// Our coordinate system has the Y-axis inverted. See also XwaD3dVertexShader
			tempv0.y = -tempv0.y;
			tempv1.y = -tempv1.y;
			tempv2.y = -tempv2.y;

			float u, v;
			if (IsInsideTriangle(UV, UV0, UV1, UV2, &u, &v)) {
				P = tempv0 + u * (tempv2 - tempv0) + v * (tempv1 - tempv0);
				// Prevent lasers behind the camera: they will cause a very bright flash
				if (P.z > 0.01)
					g_LaserList.insert(Vector3(P.x, P.y, P.z), _lastTextureSelected->material.Light);
				//log_debug("[DBG] LaserLight: %0.1f, %0.1f, %0.1f", P.x, P.y, P.z);
			}
		}
	}

	if (g_bDumpSSAOBuffers && bD3DDumpOBJEnabled) {
		D3DTotalLaserVertices += MeshVerticesCount;
		D3DTotalLaserTextureVertices += MeshTextureVerticesCount;
		fprintf(D3DDumpLaserOBJFile, "\n");
	}
}

void EffectsRenderer::ApplyProceduralLava()
{
	static float iTime = 0.0f;
	auto &context = _deviceResources->_d3dDeviceContext;
	iTime = g_HiResTimer.global_time_s * _lastTextureSelected->material.LavaSpeed;

	_bModifiedShaders = true;
	_bModifiedPixelShader = true;
	_bModifiedSamplerState = true;

	g_ShadertoyBuffer.iTime = iTime;
	g_ShadertoyBuffer.bDisneyStyle   = _lastTextureSelected->material.LavaTiling;
	g_ShadertoyBuffer.iResolution[0] = _lastTextureSelected->material.LavaSize;
	g_ShadertoyBuffer.iResolution[1] = _lastTextureSelected->material.EffectBloom;
	// SunColor[0] --> Color
	g_ShadertoyBuffer.SunColor[0].x = _lastTextureSelected->material.LavaColor.x;
	g_ShadertoyBuffer.SunColor[0].y = _lastTextureSelected->material.LavaColor.y;
	g_ShadertoyBuffer.SunColor[0].z = _lastTextureSelected->material.LavaColor.z;
	/*
	// SunColor[1] --> LavaNormalMult
	g_ShadertoyBuffer.SunColor[1].x = lastTextureSelected->material.LavaNormalMult.x;
	g_ShadertoyBuffer.SunColor[1].y = lastTextureSelected->material.LavaNormalMult.y;
	g_ShadertoyBuffer.SunColor[1].z = lastTextureSelected->material.LavaNormalMult.z;
	// SunColor[2] --> LavaPosMult
	g_ShadertoyBuffer.SunColor[2].x = lastTextureSelected->material.LavaPosMult.x;
	g_ShadertoyBuffer.SunColor[2].y = lastTextureSelected->material.LavaPosMult.y;
	g_ShadertoyBuffer.SunColor[2].z = lastTextureSelected->material.LavaPosMult.z;

	g_ShadertoyBuffer.bDisneyStyle = lastTextureSelected->material.LavaTranspose;
	*/

	_deviceResources->InitPixelShader(_deviceResources->_lavaPS);
	// Set the noise texture and sampler state with wrap/repeat enabled.
	context->PSSetShaderResources(1, 1, _deviceResources->_grayNoiseSRV.GetAddressOf());
	// bModifiedSamplerState restores this sampler state at the end of this instruction.
	context->PSSetSamplers(1, 1, _deviceResources->_repeatSamplerState.GetAddressOf());

	// Set the constant buffer
	_deviceResources->InitPSConstantBufferHyperspace(
		_deviceResources->_hyperspaceConstantBuffer.GetAddressOf(), &g_ShadertoyBuffer);
}

void EffectsRenderer::ApplyGreebles()
{
	if (!g_bAutoGreeblesEnabled || !_bHasMaterial || _lastTextureSelected->material.GreebleDataIdx == -1)
		return;

	auto &resources = _deviceResources;
	auto &context = _deviceResources->_d3dDeviceContext;
	Material *material = &(_lastTextureSelected->material);
	GreebleData *greeble_data = &(g_GreebleData[material->GreebleDataIdx]);

	bool bIsRegularGreeble = (!_bLastLightmapSelectedNotNULL && greeble_data->GreebleTexIndex[0] != -1);
	bool bIsLightmapGreeble = (_bLastLightmapSelectedNotNULL && greeble_data->GreebleLightMapIndex[0] != -1);
	if (bIsRegularGreeble || bIsLightmapGreeble) {
		// 0x1: This greeble will use normal mapping
		// 0x2: This is a lightmap greeble
		// See PixelShaderGreeble for a full list of bits used in this effect
		uint32_t GreebleControlBits = bIsLightmapGreeble ? 0x2 : 0x0;
		_bModifiedShaders = true;
		_bModifiedPixelShader = true;

		resources->InitPixelShader(resources->_pixelShaderGreeble);
		if (bIsRegularGreeble) {
			g_PSCBuffer.GreebleMix1 = greeble_data->GreebleMix[0];
			g_PSCBuffer.GreebleMix2 = greeble_data->GreebleMix[1];

			g_PSCBuffer.GreebleDist1 = greeble_data->GreebleDist[0];
			g_PSCBuffer.GreebleDist2 = greeble_data->GreebleDist[1];

			g_PSCBuffer.GreebleScale1 = greeble_data->GreebleScale[0];
			g_PSCBuffer.GreebleScale2 = greeble_data->GreebleScale[1];

			uint32_t blendMask1 = greeble_data->GreebleTexIndex[0] != -1 ? greeble_data->greebleBlendMode[0] : 0x0;
			uint32_t blendMask2 = greeble_data->GreebleTexIndex[1] != -1 ? greeble_data->greebleBlendMode[1] : 0x0;
			if (blendMask1 == GBM_NORMAL_MAP || blendMask1 == GBM_UV_DISP_AND_NORMAL_MAP ||
				blendMask2 == GBM_NORMAL_MAP || blendMask2 == GBM_UV_DISP_AND_NORMAL_MAP)
				GreebleControlBits |= 0x1;
			if (blendMask1 == GBM_UV_DISP || blendMask1 == GBM_UV_DISP_AND_NORMAL_MAP ||
				blendMask2 == GBM_UV_DISP || blendMask2 == GBM_UV_DISP_AND_NORMAL_MAP)
				g_PSCBuffer.UVDispMapResolution = greeble_data->UVDispMapResolution;
			g_PSCBuffer.GreebleControl = (GreebleControlBits << 16) | (blendMask2 << 4) | blendMask1;

			// Load regular greebles...
			if (greeble_data->GreebleTexIndex[0] != -1)
				context->PSSetShaderResources(10, 1, &(resources->_extraTextures[greeble_data->GreebleTexIndex[0]]));
			if (greeble_data->GreebleTexIndex[1] != -1)
				context->PSSetShaderResources(11, 1, &(resources->_extraTextures[greeble_data->GreebleTexIndex[1]]));
		}
		else if (bIsLightmapGreeble) {
			g_PSCBuffer.GreebleMix1 = greeble_data->GreebleLightMapMix[0];
			g_PSCBuffer.GreebleMix2 = greeble_data->GreebleLightMapMix[1];

			g_PSCBuffer.GreebleDist1 = greeble_data->GreebleLightMapDist[0];
			g_PSCBuffer.GreebleDist2 = greeble_data->GreebleLightMapDist[1];

			g_PSCBuffer.GreebleScale1 = greeble_data->GreebleLightMapScale[0];
			g_PSCBuffer.GreebleScale2 = greeble_data->GreebleLightMapScale[1];

			uint32_t blendMask1 = greeble_data->GreebleLightMapIndex[0] != -1 ? greeble_data->greebleLightMapBlendMode[0] : 0x0;
			uint32_t blendMask2 = greeble_data->GreebleLightMapIndex[1] != -1 ? greeble_data->greebleLightMapBlendMode[1] : 0x0;
			if (blendMask1 == GBM_NORMAL_MAP || blendMask1 == GBM_UV_DISP_AND_NORMAL_MAP ||
				blendMask2 == GBM_NORMAL_MAP || blendMask2 == GBM_UV_DISP_AND_NORMAL_MAP)
				GreebleControlBits |= 0x1;
			if (blendMask1 == GBM_UV_DISP || blendMask1 == GBM_UV_DISP_AND_NORMAL_MAP ||
				blendMask2 == GBM_UV_DISP || blendMask2 == GBM_UV_DISP_AND_NORMAL_MAP)
				g_PSCBuffer.UVDispMapResolution = greeble_data->UVDispMapResolution;
			g_PSCBuffer.GreebleControl = (GreebleControlBits << 16) | (blendMask2 << 4) | blendMask1;

			// ... or load lightmap greebles
			if (greeble_data->GreebleLightMapIndex[0] != -1)
				context->PSSetShaderResources(10, 1, &(resources->_extraTextures[greeble_data->GreebleLightMapIndex[0]]));
			if (greeble_data->GreebleLightMapIndex[1] != -1)
				context->PSSetShaderResources(11, 1, &(resources->_extraTextures[greeble_data->GreebleLightMapIndex[1]]));
		}
	}
}

void EffectsRenderer::ApplyAnimatedTextures()
{
	// Do not apply animations if there's no material or there's a greeble in the current
	// texture
	if (!_bHasMaterial || _lastTextureSelected->material.GreebleDataIdx != -1)
		return;

	//if ((_bLastLightmapSelectedNotNULL && _lastTextureSelected->material.GetCurrentATCIndex(NULL, LIGHTMAP_ATC_IDX) > -1) ||
	//	(!_bLastLightmapSelectedNotNULL && _lastTextureSelected->material.GetCurrentATCIndex(NULL, TEXTURE_ATC_IDX) > -1))

	bool bIsDamageTex = false;
	int TexATCIndex = _lastTextureSelected->material.GetCurrentATCIndex(&bIsDamageTex, TEXTURE_ATC_IDX);
	int LightATCIndex = _lastTextureSelected->material.GetCurrentATCIndex(NULL, LIGHTMAP_ATC_IDX);
	// If there's no texture animation and no lightmap animation, then there's nothing to do here
	if (TexATCIndex == -1 && LightATCIndex == -1)
		return;

	auto &resources = _deviceResources;
	auto &context = _deviceResources->_d3dDeviceContext;
	const bool bRenderingDC = g_PSCBuffer.DynCockpitSlots > 0;

	_bModifiedShaders = true;
	_bModifiedPixelShader = true;

	// If we reach this point then one of LightMapATCIndex or TextureATCIndex must be > -1 or both!
	// If we're rendering a DC element, we don't want to replace the shader
	if (!bRenderingDC)
		resources->InitPixelShader(resources->_pixelShaderAnim);

	// We're not updating the Hyperspace FSM in the D3DRendererHook, we still do it in
	// Direct3DDevice::Execute. That means that we may reach this point without knowing
	// we've entered hyperspace. Let's provide a quick update here:
	g_PSCBuffer.bInHyperspace = PlayerDataTable[*g_playerIndex].hyperspacePhase != 0 || g_HyperspacePhaseFSM != HS_INIT_ST;
	g_PSCBuffer.AuxColor.x = 1.0f;
	g_PSCBuffer.AuxColor.y = 1.0f;
	g_PSCBuffer.AuxColor.z = 1.0f;
	g_PSCBuffer.AuxColor.w = 1.0f;

	g_PSCBuffer.AuxColorLight.x = 1.0f;
	g_PSCBuffer.AuxColorLight.y = 1.0f;
	g_PSCBuffer.AuxColorLight.z = 1.0f;
	g_PSCBuffer.AuxColorLight.w = 1.0f;

	int extraTexIdx = -1, extraLightIdx = -1;
	if (TexATCIndex > -1) {
		AnimatedTexControl *atc = &(g_AnimatedMaterials[TexATCIndex]);
		int idx = atc->AnimIdx;
		extraTexIdx = atc->Sequence[idx].ExtraTextureIndex;
		if (atc->BlackToAlpha)
			g_PSCBuffer.special_control.ExclusiveMask = SPECIAL_CONTROL_BLACK_TO_ALPHA;
		else if (atc->AlphaIsBloomMask)
			g_PSCBuffer.special_control.ExclusiveMask = SPECIAL_CONTROL_ALPHA_IS_BLOOM_MASK;
		else
			g_PSCBuffer.special_control.ExclusiveMask = 0;
		g_PSCBuffer.AuxColor = atc->Tint;
		g_PSCBuffer.Offset = atc->Offset;
		g_PSCBuffer.AspectRatio = atc->AspectRatio;
		g_PSCBuffer.Clamp = atc->Clamp;
		g_PSCBuffer.fBloomStrength = atc->Sequence[idx].intensity;
	}

	if (LightATCIndex > -1) {
		AnimatedTexControl *atc = &(g_AnimatedMaterials[LightATCIndex]);
		int idx = atc->AnimIdx;
		extraLightIdx = atc->Sequence[idx].ExtraTextureIndex;
		if (atc->BlackToAlpha)
			g_PSCBuffer.special_control_light.ExclusiveMask = SPECIAL_CONTROL_BLACK_TO_ALPHA;
		else if (atc->AlphaIsBloomMask)
			g_PSCBuffer.special_control_light.ExclusiveMask = SPECIAL_CONTROL_ALPHA_IS_BLOOM_MASK;
		else
			g_PSCBuffer.special_control_light.ExclusiveMask = 0;
		g_PSCBuffer.AuxColorLight = atc->Tint;
		// TODO: We might need two of these settings below, one for the regular tex and one for the lightmap
		g_PSCBuffer.Offset = atc->Offset;
		g_PSCBuffer.AspectRatio = atc->AspectRatio;
		g_PSCBuffer.Clamp = atc->Clamp;
		g_PSCBuffer.fBloomStrength = atc->Sequence[idx].intensity;
	}

	if (g_bDumpSSAOBuffers)
		log_debug("[DBG] TargetEvt: %d, HullEvent: %d", g_GameEvent.TargetEvent, g_GameEvent.HullEvent);
	//log_debug("[DBG] %s, ATCIndex: %d", lastTextureSelected->_surface->_name, ATCIndex);
	
	if (extraTexIdx > -1) {
		// Use the following when using std::vector<ID3D11ShaderResourceView*>:
		// We cannot use InitPSShaderResourceView here because that will set slots 0 and 1, thus changing
		// the DC foreground SRV
		context->PSSetShaderResources(0, 1, &(resources->_extraTextures[extraTexIdx]));

		// Force the use of damage textures if DC is on. This makes damage textures visible
		// even when no cover texture is available:
		if (bRenderingDC)
			g_DCPSCBuffer.use_damage_texture = bIsDamageTex;
	}
	// Set the animated lightmap in slot 1, but only if we're not rendering DC -- DC uses
	// that slot for something else
	if (extraLightIdx > -1 && !bRenderingDC) {
		// Use the following when using std::vector<ID3D11ShaderResourceView*>:
		context->PSSetShaderResources(1, 1, &(resources->_extraTextures[extraLightIdx]));
	}
}

void EffectsRenderer::MainSceneHook(const SceneCompData* scene)
{
	auto &context = _deviceResources->_d3dDeviceContext;
	auto &resources = _deviceResources;

	ComPtr<ID3D11Buffer> oldVSConstantBuffer;
	ComPtr<ID3D11Buffer> oldPSConstantBuffer;
	ComPtr<ID3D11ShaderResourceView> oldVSSRV[3];

	context->VSGetConstantBuffers(0, 1, oldVSConstantBuffer.GetAddressOf());
	context->PSGetConstantBuffers(0, 1, oldPSConstantBuffer.GetAddressOf());
	context->VSGetShaderResources(0, 3, oldVSSRV[0].GetAddressOf());

	context->VSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());
	context->PSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());
	_deviceResources->InitRasterizerState(_rasterizerState);
	_deviceResources->InitSamplerState(_samplerState.GetAddressOf(), nullptr);

	if (scene->TextureAlphaMask == 0)
	{
		_deviceResources->InitBlendState(_solidBlendState, nullptr);
		_deviceResources->InitDepthStencilState(_solidDepthState, nullptr);
		_bIsTransparentCall = false;
	}
	else
	{
		_deviceResources->InitBlendState(_transparentBlendState, nullptr);
		_deviceResources->InitDepthStencilState(_transparentDepthState, nullptr);
		_bIsTransparentCall = true;
	}

	_deviceResources->InitViewport(&_viewport);
	_deviceResources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_deviceResources->InitInputLayout(_inputLayout);
	_deviceResources->InitVertexShader(_vertexShader);
	_deviceResources->InitPixelShader(_pixelShader);
	ComPtr<ID3D11PixelShader> lastPixelShader = _pixelShader;

	UpdateTextures(scene);
	UpdateMeshBuffers(scene);
	UpdateVertexAndIndexBuffers(scene);
	UpdateConstantBuffer(scene);

	// Effects Management starts here.
	// Do the state management
	DoStateManagement(scene);

	// DEBUG
	// The scene pointer seems to be the same for every draw call, but the contents change.
	// scene->TextureName seems to be NULL all the time.
	// We can now have texture names associated with a specific ship instance. Meshes and faceData
	// can be rendered multiple times per frame, but we object ID is unique. We also have access to
	// the MobileObjectEntry and the CraftInstance. In other words: we can now apply effects on a
	// per-ship, per-texture basis. See the example below...
	//log_debug("[DBG] Rendering scene: 0x%x, faceData: 0x%x %s",
	//	scene, scene->FaceIndices, g_isInRenderLasers ? "LASER" : "");
	//MobileObjectEntry *pMobileObject = (MobileObjectEntry *)scene->pObject->pMobileObject;
	//log_debug("[DBG] FaceData: 0x%x, Id: %d, Species: %d, Category: %d",
	//	scene->FaceIndices, scene->pObject->ObjectId, scene->pObject->ObjectSpecies, scene->pObject->ShipCategory);
	if (g_bDumpSSAOBuffers && bD3DDumpOBJEnabled)
		log_debug("[DBG] [%s]: Mesh: 0x%x, faceData: 0x%x, Id: %d, Type: %d, Genus: %d, Player: %d",
			_bLastTextureSelectedNotNULL ? _lastTextureSelected->_name.c_str() : "(null)", scene->MeshVertices, scene->FaceIndices,
			scene->pObject->ObjectId, scene->pObject->ObjectSpecies, scene->pObject->ShipCategory, *g_playerIndex);
	
	/*
	// The preybird cockpit re-uses the same mesh, but different faceData:
	[500] [DBG] [opt,FlightModels\PreybirdFighterCockpit.opt,TEX00051,color,0]: Mesh: 0x183197a3, faceData: 0x18319bac, Id: 3, Type: 29, Genus: 0
	[500] [DBG] [opt,FlightModels\PreybirdFighterCockpit.opt,TEX00052,color,0]: Mesh: 0x183197a3, faceData: 0x18319ddd, Id: 3, Type: 29, Genus: 0
	[500] [DBG] [opt,FlightModels\PreybirdFighterCockpit.opt,TEX00054,color,0]: Mesh: 0x183197a3, faceData: 0x1831a00e, Id: 3, Type: 29, Genus: 0
	[500] [DBG] [opt,FlightModels\PreybirdFighterCockpit.opt,TEX00053,color,0]: Mesh: 0x183197a3, faceData: 0x1831a23f, Id: 3, Type: 29, Genus: 0

	// These are several different TIE Fighters: the faceData and Texname are repeated, but we see different IDs:
	[500][DBG][opt, FlightModels\TieFighter.opt, TEX00029, color, 0] : Mesh : 0x1dde3181, faceData : 0x1ddecd62, Id : 8,  Type : 5, Genus : 0
	[500][DBG][opt, FlightModels\TieFighter.opt, TEX00029, color, 0] : Mesh : 0x1dd3f0f2, faceData : 0x1dd698c0, Id : 11, Type : 5, Genus : 0
	[500][DBG][opt, FlightModels\TieFighter.opt, TEX00029, color, 0] : Mesh : 0x1dde3181, faceData : 0x1ddecd62, Id : 11, Type : 5, Genus : 0
	[500][DBG][opt, FlightModels\TieFighter.opt, TEX00029, color, 0] : Mesh : 0x1dd3f0f2, faceData : 0x1dd698c0, Id : 10, Type : 5, Genus : 0
	[500][DBG][opt, FlightModels\TieFighter.opt, TEX00029, color, 0] : Mesh : 0x1dde3181, faceData : 0x1ddecd62, Id : 10, Type : 5, Genus : 0
	[500][DBG][opt, FlightModels\TieFighter.opt, TEX00029, color, 0] : Mesh : 0x1dd3f0f2, faceData : 0x1dd698c0, Id : 12, Type : 5, Genus : 0
	[500][DBG][opt, FlightModels\TieFighter.opt, TEX00029, color, 0] : Mesh : 0x1dde3181, faceData : 0x1ddecd62, Id : 12, Type : 5, Genus : 0
	*/

	// The main 3D content is rendered here, that includes the cockpit and 3D models. But
	// there's content that is still rendered in Direct3DDevice::Execute():
	// - The backdrop, including the Suns
	// - Engine Glow
	// - The HUD, including the reticle
	// - Explosions, including the DS2 core explosion
	/*
		We have an interesting mixture of Execute() calls and Hook calls. The sequence for
		each frame, looks like this:
		[11528][DBG] * ****************** PRESENT 3D
		[11528][DBG] BeginScene <-- Old method
		[11528][DBG] SceneBegin <-- New Hook
		[11528][DBG] Execute(1) <-- Old method (the backdrop is probably rendered here)
		[17076][DBG] EffectsRenderer::MainSceneHook
		[17076][DBG] EffectsRenderer::MainSceneHook
		... a bunch of calls to MainSceneHook. Most of the 3D content is rendered here ...
		[17076][DBG] EffectsRenderer::MainSceneHook
		[11528][DBG] Execute(1) <-- The engine glow might be rendered here (?)
		[11528][DBG] Execute(1) <-- Maybe the HUD and reticle is rendered here (?)
		[11528][DBG] EndScene   <-- Old method
		[11528][DBG] SceneEnd   <-- New Hook
		[11528][DBG] * ****************** PRESENT 3D
	*/
	g_PSCBuffer = { 0 };
	g_PSCBuffer.brightness		= MAX_BRIGHTNESS;
	g_PSCBuffer.fBloomStrength	= 1.0f;
	g_PSCBuffer.fPosNormalAlpha = 1.0f;
	g_PSCBuffer.fSSAOAlphaMult	= g_fSSAOAlphaOfs;
	g_PSCBuffer.fSSAOMaskVal		= g_DefaultGlobalMaterial.Metallic * 0.5f;
	g_PSCBuffer.fGlossiness		= g_DefaultGlobalMaterial.Glossiness;
	g_PSCBuffer.fSpecInt			= g_DefaultGlobalMaterial.Intensity;  // DEFAULT_SPEC_INT;
	g_PSCBuffer.fNMIntensity		= g_DefaultGlobalMaterial.NMIntensity;
	g_PSCBuffer.AuxColor.x		= 1.0f;
	g_PSCBuffer.AuxColor.y		= 1.0f;
	g_PSCBuffer.AuxColor.z		= 1.0f;
	g_PSCBuffer.AuxColor.w		= 1.0f;
	g_PSCBuffer.AspectRatio		= 1.0f;

	// We will be modifying the regular render state from this point on. The state and the Pixel/Vertex
	// shaders are already set by this point; but if we modify them, we'll set bModifiedShaders to true
	// so that we can restore the state at the end of the draw call.
	_bModifiedShaders		= false;
	_bModifiedPixelShader	= false;
	_bModifiedBlendState		= false;
	_bModifiedSamplerState	= false;

	// Apply specific material properties for the current texture
	ApplyMaterialProperties();

	// Apply the SSAO mask/Special materials, like lasers and HUD
	ApplySpecialMaterials();

	// EARLY EXIT 1: Render the targetted craft to the Dynamic Cockpit RTVs and continue
	if (g_bDynCockpitEnabled && (g_bIsFloating3DObject || g_isInRenderMiniature)) {
		DCCaptureMiniature();
		goto out;
	}

	// Modify the state for both VR and regular game modes...

	// Maintain the k-closest lasers to the camera (but ignore the miniature lasers)
	if (g_bEnableLaserLights && _bIsLaser && _bHasMaterial && !g_bStartedGUI)
		AddLaserLights(scene);

	// Apply BLOOM flags and 32-bit mode enhancements
	// TODO: This code expects either a lightmap or a regular texture, but now we can have both at the same time
	// this will mess up all the animation logic when both the regular and lightmap layers have animations
	ApplyBloomSettings();

	// Transparent textures are currently used with DC to render floating text. However, if the erase region
	// commands are being ignored, then this will cause the text messages to be rendered twice. To avoid
	// having duplicated messages, we're removing these textures here when the erase_region commmands are
	// not being applied.
	// The above behavior is overridden if the DC element is set as "always_visible". In that case, the
	// transparent layer will remain visible even when the HUD is displayed.
	if (g_bDCManualActivate && g_bDynCockpitEnabled && _bDCIsTransparent && !g_bDCApplyEraseRegionCommands && !_bDCElemAlwaysVisible)
		goto out;

	// Dynamic Cockpit: Replace textures at run-time. Returns true if we need to skip the current draw call
	if (DCReplaceTextures())
		goto out;

	// TODO: Update the Hyperspace FSM -- but only update it exactly once per frame.
	// Looks like the code to do this update in Execute() still works. So moving on for now

	// Capture the cockpit OPT -> View transform for use in ShadowMapping later on
	if (g_bShadowMapEnable) {
		if (!_bCockpitConstantsCaptured && _bIsCockpit)
		{
			_bCockpitConstantsCaptured = true;
			_CockpitConstants = _constants;
			_CockpitWorldView = scene->WorldViewTransform;
		}
	}

	// Procedural Lava
	if (g_bProceduralLava && _bLastTextureSelectedNotNULL && _bHasMaterial && _lastTextureSelected->material.IsLava)
		ApplyProceduralLava();

	ApplyGreebles();

	if (g_bEnableAnimations)
		ApplyAnimatedTextures();

	// Additional processing for VR or similar. Not implemented in this class, but will be in
	// other subclasses.
	ExtraPreprocessing();

	// Apply the changes to the vertex and pixel shaders
	//if (bModifiedShaders) 
	{
		resources->InitPSConstantBuffer3D(resources->_PSConstantBuffer.GetAddressOf(), &g_PSCBuffer);
		resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);
		if (g_PSCBuffer.DynCockpitSlots > 0)
			resources->InitPSConstantBufferDC(resources->_PSConstantBufferDC.GetAddressOf(), &g_DCPSCBuffer);
	}

	// Dump the current scene to an OBJ file
	if (g_bDumpSSAOBuffers && bD3DDumpOBJEnabled) {
		// The coordinates are in Object (OPT) space and scale, centered on the origin.
		//OBJDump(scene->MeshVertices, *(int*)((int)scene->MeshVertices - 8));

		// This function is called once per face group. Each face group is associated with a single texture.
		// A single mesh can contain multiple face groups.
		// An OPT contains multiple meshes
		// _verticesCount has the number of vertices in the current face group
		//log_debug("[DBG] _vertices.size(): %lu, _verticesCount: %d", _vertices.size(), _verticesCount);
		OBJDumpD3dVertices(scene); // _vertices.data(), _verticesCount, _triangles.data(), _trianglesCount);
	}

	// There's a bug with the lasers: they are sometimes rendered at the start of the frame, causing them to be
	// displayed *behind* the geometry. To fix this, we're going to save all the lasers in a list and then render
	// them at the end of the frame.
	// TODO: Instead of storing draw calls, use a deferred context to record the calls and then execute it later
	if (_bIsLaser) {
		DrawCommand command;
		// There's apparently a bug in the latest D3DRendererHook ddraw: the miniature does not set the proper
		// viewport, so lasers and other projectiles that are rendered in the CMD also show in the bottom of the
		// screen. This is not a fix, but a workaround: we're going to skip rendering any such objects if we've
		// started rendering the CMD.
		// Also, it looks like we can't use g_isInRenderMiniature for this check, since that doesn't seem to work
		// in some cases; we need to use g_bIsFloating3DObject instead.
		if (g_bStartedGUI || g_bIsFloating3DObject || g_isInRenderMiniature)
			goto out;

		// Save the current draw command and skip. We'll render the lasers later
		// Save the textures
		command.colortex			= _lastTextureSelected;
		command.lighttex			= _lastLightmapSelected;
		// Save the SRVs
		command.vertexSRV		= _lastMeshVerticesView;
		command.normalsSRV		= _lastMeshVertexNormalsView;
		command.texturesSRV		= _lastMeshTextureVerticesView;
		// Save the vertex and index buffers
		command.vertexBuffer		= _lastVertexBuffer;
		command.indexBuffer		= _lastIndexBuffer;
		command.trianglesCount	= _trianglesCount;
		// Save the constants
		command.constants		= _constants;
		// Add the command to the list of deferred commands
		_LaserDrawCommands.push_back(command);

		// Do not render the laser at this moment
		goto out;
	}

	// Transparent polygons are sometimes rendered in the middle of a frame, causing them to appear
	// behind other geometry. We need to store those draw calls and render them later, near the end
	// of the frame.
	// TODO: Instead of storing draw calls, use a deferred context to record the calls and then execute it later
	if (_bIsTransparentCall) {
		DrawCommand command;
		// Save the current draw command and skip. We'll render the transparency later

		// Save the textures. The following will increase the refcount on the SRVs, we need
		// to decrease it later to avoid memory leaks
		for (int i = 0; i < 2; i++)
			command.SRVs[i] = nullptr;
		context->PSGetShaderResources(0, 2, command.SRVs);
		// Save the Vertex, Normal and UVs SRVs
		command.vertexSRV = _lastMeshVerticesView;
		command.normalsSRV = _lastMeshVertexNormalsView;
		command.texturesSRV = _lastMeshTextureVerticesView;
		// Save the vertex and index buffers
		command.vertexBuffer = _lastVertexBuffer;
		command.indexBuffer = _lastIndexBuffer;
		command.trianglesCount = _trianglesCount;
		// Save the constants
		command.constants = _constants;
		// Save extra data
		command.PSCBuffer = g_PSCBuffer;
		command.DCPSCBuffer = g_DCPSCBuffer;
		command.bIsCockpit = _bIsCockpit;
		command.bIsGunner = _bIsGunner;
		command.bIsBlastMark = _bIsBlastMark;
		command.pixelShader = resources->GetCurrentPixelShader();
		// Add the command to the list of deferred commands
		_TransparentDrawCommands.push_back(command);

		goto out;
	}

	RenderScene();

out:
	// The hyperspace effect needs the current VS constants to work properly
	if (g_HyperspacePhaseFSM == HS_INIT_ST)
		context->VSSetConstantBuffers(0, 1, oldVSConstantBuffer.GetAddressOf());
	context->PSSetConstantBuffers(0, 1, oldPSConstantBuffer.GetAddressOf());
	context->VSSetShaderResources(0, 3, oldVSSRV[0].GetAddressOf());

	if (_bModifiedPixelShader)
		resources->InitPixelShader(lastPixelShader);

	// Decrease the refcount of all the objects we queried at the prologue. (Is this
	// really necessary? They live on the stack, so maybe they are auto-released?)
	oldVSConstantBuffer.Release();
	oldPSConstantBuffer.Release();
	for (int i = 0; i < 3; i++)
		oldVSSRV[i].Release();

	/*
	if (bModifiedBlendState) {
		RestoreBlendState();
		bModifiedBlendState = false;
	}
	*/

#if LOGGER_DUMP
	DumpConstants(_constants);
	DumpVector3(scene->MeshVertices, *(int*)((int)scene->MeshVertices - 8));
	DumpTextureVertices(scene->MeshTextureVertices, *(int*)((int)scene->MeshTextureVertices - 8));
	DumpD3dVertices(_vertices.data(), _verticesCount);
#endif
}

/*
 If the game is rendering the hyperspace effect, this function will select shaderToyBuf
 when rendering the cockpit. Otherwise it will select the regular offscreenBuffer
 */
inline ID3D11RenderTargetView *EffectsRenderer::SelectOffscreenBuffer(bool bIsMaskable, bool bSteamVRRightEye = false) {
	auto& resources = this->_deviceResources;

	ID3D11RenderTargetView *regularRTV = bSteamVRRightEye ? resources->_renderTargetViewR.Get() : resources->_renderTargetView.Get();
	ID3D11RenderTargetView *shadertoyRTV = bSteamVRRightEye ? resources->_shadertoyRTV_R.Get() : resources->_shadertoyRTV.Get();
	if (g_HyperspacePhaseFSM != HS_INIT_ST && bIsMaskable)
		// If we reach this point, then the game is in hyperspace AND this is a cockpit texture
		return shadertoyRTV;
	else
		// Normal output buffer (_offscreenBuffer)
		return regularRTV;
}

// This function should only be called when the miniature (targetted craft) is being rendered.
void EffectsRenderer::DCCaptureMiniature()
{
	auto &resources = _deviceResources;
	auto &context = resources->_d3dDeviceContext;

	// The viewport for the miniature is not properly set at the moment. So lasers and
	// projectiles (?) and maybe other objects show outside the CMD. We need to avoid
	// capturing them.
	if (_bIsLaser || _lastTextureSelected->is_Missile) return;

	// Restore the non-VR dimensions:
	//float displayWidth = (float)resources->_displayWidth;
	//float displayHeight = (float)resources->_displayHeight;
	//g_VSCBuffer.viewportScale[0] =  2.0f / displayWidth;
	//g_VSCBuffer.viewportScale[1] = -2.0f / displayHeight;
	// Apply the brightness settings to the pixel shader
	g_PSCBuffer.brightness = g_fBrightness;
	_deviceResources->InitViewport(&_viewport);
	//resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);
	resources->InitPSConstantBuffer3D(resources->_PSConstantBuffer.GetAddressOf(), &g_PSCBuffer);
	_deviceResources->InitVertexShader(_vertexShader);
	_deviceResources->InitPixelShader(_pixelShader);
	// Select the proper RTV
	context->OMSetRenderTargets(1, resources->_renderTargetViewDynCockpit.GetAddressOf(),
		resources->_depthStencilViewL.Get());

	// Enable Z-Buffer since we're drawing the targeted craft
	QuickSetZWriteEnabled(TRUE);

	// Render
	context->DrawIndexed(_trianglesCount * 3, 0, 0);
	g_iHUDOffscreenCommandsRendered++;

	// Restore the regular texture, RTV, shaders, etc:
	context->PSSetShaderResources(0, 1, _lastTextureSelected->_textureView.GetAddressOf());
	context->OMSetRenderTargets(1, resources->_renderTargetView.GetAddressOf(),
		resources->_depthStencilViewL.Get());
	/*
	if (g_bEnableVR) {
		resources->InitVertexShader(resources->_sbsVertexShader);
		// Restore the right constants in case we're doing VR rendering
		g_VSCBuffer.viewportScale[0] = 1.0f / displayWidth;
		g_VSCBuffer.viewportScale[1] = 1.0f / displayHeight;
		resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);
	}
	else {
		resources->InitVertexShader(resources->_vertexShader);
	}
	*/
	// Restore the Pixel Shader constant buffers:
	g_PSCBuffer.brightness = MAX_BRIGHTNESS;
	resources->InitPSConstantBuffer3D(resources->_PSConstantBuffer.GetAddressOf(), &g_PSCBuffer);

	if (g_bDumpSSAOBuffers) {
		DirectX::SaveWICTextureToFile(context, resources->_offscreenBufferDynCockpit, GUID_ContainerFormatJpeg, L"c:\\temp\\_DC-FG-Input-Raw.jpg");
	}
}

bool EffectsRenderer::DCReplaceTextures()
{
	bool bSkip = false;
	auto &resources = _deviceResources;
	auto &context = resources->_d3dDeviceContext;

	// Dynamic Cockpit: Replace textures at run-time:
	if (!g_bDCManualActivate || !g_bDynCockpitEnabled || !_bLastTextureSelectedNotNULL || !_lastTextureSelected->is_DynCockpitDst ||
		// We should never render lightmap textures with the DC pixel shader:
		_lastTextureSelected->is_DynCockpitAlphaOverlay) {
		bSkip = false;
		goto out;
	}

	int idx = _lastTextureSelected->DCElementIndex;

	if (g_HyperspacePhaseFSM != HS_INIT_ST) {
		// If we're in hyperspace, let's set the corresponding flag before rendering DC controls
		_bModifiedShaders = true;
		g_PSCBuffer.bInHyperspace = 1;
	}

	// Check if this idx is valid before rendering
	if (idx >= 0 && idx < g_iNumDCElements) {
		dc_element *dc_element = &g_DCElements[idx];
		if (dc_element->bActive) {
			_bModifiedShaders = true;
			g_PSCBuffer.fBloomStrength = g_BloomConfig.fCockpitStrength;
			int numCoords = 0;
			for (int i = 0; i < dc_element->coords.numCoords; i++)
			{
				int src_slot = dc_element->coords.src_slot[i];
				// Skip invalid src slots
				if (src_slot < 0)
					continue;

				if (src_slot >= (int)g_DCElemSrcBoxes.src_boxes.size()) {
					//log_debug("[DBG] [DC] src_slot: %d bigger than src_boxes.size! %d",
					//	src_slot, g_DCElemSrcBoxes.src_boxes.size());
					continue;
				}

				DCElemSrcBox *src_box = &g_DCElemSrcBoxes.src_boxes[src_slot];
				// Skip src boxes that haven't been computed yet
				if (!src_box->bComputed)
					continue;

				uvfloat4 uv_src;
				uv_src.x0 = src_box->coords.x0; uv_src.y0 = src_box->coords.y0;
				uv_src.x1 = src_box->coords.x1; uv_src.y1 = src_box->coords.y1;
				g_DCPSCBuffer.src[numCoords] = uv_src;
				g_DCPSCBuffer.dst[numCoords] = dc_element->coords.dst[i];
				g_DCPSCBuffer.noisy_holo = _bIsNoisyHolo;
				g_DCPSCBuffer.transparent = _bDCIsTransparent;
				g_DCPSCBuffer.use_damage_texture = false;
				if (_bWarheadLocked)
					g_DCPSCBuffer.bgColor[numCoords] = dc_element->coords.uWHColor[i];
				else
					g_DCPSCBuffer.bgColor[numCoords] = _bIsTargetHighlighted ?
						dc_element->coords.uHGColor[i] :
						dc_element->coords.uBGColor[i];
				// The hologram property will make *all* uvcoords in this DC element
				// holographic as well:
				//bIsHologram |= (dc_element->bHologram);
				numCoords++;
			} // for
			// g_bDCHologramsVisible is a hard switch, let's use g_fDCHologramFadeIn instead to
			// provide a softer ON/OFF animation
			if (_bIsHologram && g_fDCHologramFadeIn <= 0.01f) {
				bSkip = true;
				goto out;
			}
			g_PSCBuffer.DynCockpitSlots = numCoords;
			//g_PSCBuffer.bUseCoverTexture = (dc_element->coverTexture != nullptr) ? 1 : 0;
			g_PSCBuffer.bUseCoverTexture = (resources->dc_coverTexture[idx] != nullptr) ? 1 : 0;

			// slot 0 is the cover texture
			// slot 1 is the HUD offscreen buffer
			// slot 2 is the text buffer
			context->PSSetShaderResources(1, 1, resources->_offscreenAsInputDynCockpitSRV.GetAddressOf());
			context->PSSetShaderResources(2, 1, resources->_DCTextSRV.GetAddressOf());
			// Set the cover texture:
			if (g_PSCBuffer.bUseCoverTexture) {
				//log_debug("[DBG] [DC] Setting coverTexture: 0x%x", resources->dc_coverTexture[idx].GetAddressOf());
				//context->PSSetShaderResources(0, 1, dc_element->coverTexture.GetAddressOf());
				//context->PSSetShaderResources(0, 1, &dc_element->coverTexture);
				context->PSSetShaderResources(0, 1, resources->dc_coverTexture[idx].GetAddressOf());
				//resources->InitPSShaderResourceView(resources->dc_coverTexture[idx].Get());
			}
			else
				context->PSSetShaderResources(0, 1, _lastTextureSelected->_textureView.GetAddressOf());
			//resources->InitPSShaderResourceView(lastTextureSelected->_textureView.Get());
		// No need for an else statement, slot 0 is already set to:
		// context->PSSetShaderResources(0, 1, texture->_textureView.GetAddressOf());
		// See D3DRENDERSTATE_TEXTUREHANDLE, where lastTextureSelected is set.
			if (g_PSCBuffer.DynCockpitSlots > 0) {
				_bModifiedPixelShader = true;
				if (_bIsHologram) {
					// Holograms require alpha blending to be enabled, but we also need to save the current
					// blending state so that it gets restored at the end of this draw call.
					//SaveBlendState();
					EnableHoloTransparency();
					_bModifiedBlendState = true;
					uint32_t hud_color = (*g_XwaFlightHudColor) & 0x00FFFFFF;
					//log_debug("[DBG] hud_color, border, inside: 0x%x, 0x%x", *g_XwaFlightHudBorderColor, *g_XwaFlightHudColor);
					g_ShadertoyBuffer.iTime = g_fDCHologramTime;
					g_ShadertoyBuffer.twirl = g_fDCHologramFadeIn;
					// Override the background color if the current DC element is a hologram:
					g_DCPSCBuffer.bgColor[0] = hud_color;
					resources->InitPSConstantBufferHyperspace(resources->_hyperspaceConstantBuffer.GetAddressOf(), &g_ShadertoyBuffer);
				}
				resources->InitPixelShader(_bIsHologram ? resources->_pixelShaderDCHolo : resources->_pixelShaderDC);
			}
			else if (g_PSCBuffer.bUseCoverTexture) {
				_bModifiedPixelShader = true;
				resources->InitPixelShader(resources->_pixelShaderEmptyDC);
			}
		} // if dc_element->bActive
	}

out:
	return bSkip;
}

void EffectsRenderer::RenderScene()
{
	auto &context = _deviceResources->_d3dDeviceContext;

	// This method isn't called to draw the hyperstreaks or the hypertunnel. A different
	// (unknown, maybe RenderMain?) path is taken instead.

	ID3D11RenderTargetView *rtvs[6] = {
		SelectOffscreenBuffer(_bIsCockpit || _bIsGunner /* || _bIsReticle */), // Select the main RTV
		_deviceResources->_renderTargetViewBloomMask.Get(),
		g_bAOEnabled ? _deviceResources->_renderTargetViewDepthBuf.Get() : NULL,
		// The normals hook should not be allowed to write normals for light textures. This is now implemented
		// in XwaD3dPixelShader
		_deviceResources->_renderTargetViewNormBuf.Get(),
		// Blast Marks are confused with glass because they are not shadeless; but they have transparency
		_bIsBlastMark ? NULL : _deviceResources->_renderTargetViewSSAOMask.Get(),
		_bIsBlastMark ? NULL : _deviceResources->_renderTargetViewSSMask.Get(),
	};
	context->OMSetRenderTargets(6, rtvs, _deviceResources->_depthStencilViewL.Get());

	// DEBUG: Skip draw calls if we're debugging the process
	/*
	if (g_iD3DExecuteCounterSkipHi > -1 && g_iD3DExecuteCounter > g_iD3DExecuteCounterSkipHi)
		goto out;
	if (g_iD3DExecuteCounterSkipLo > -1 && g_iD3DExecuteCounter < g_iD3DExecuteCounterSkipLo)
		goto out;
	*/

	context->DrawIndexed(_trianglesCount * 3, 0, 0);

//out:
	g_iD3DExecuteCounter++;
}

void EffectsRenderer::RenderLasers()
{
	if (_LaserDrawCommands.size() == 0)
		return;

	auto &resources = _deviceResources;
	auto &context = resources->_d3dDeviceContext;
	//log_debug("[DBG] Rendering %d deferred draw calls", _LaserDrawCommands.size());

	SaveContext();
	
	context->VSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());
	context->PSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());
	// Set the proper rastersize and depth stencil states for transparency
	_deviceResources->InitBlendState(_transparentBlendState, nullptr);
	_deviceResources->InitDepthStencilState(_transparentDepthState, nullptr);

	_deviceResources->InitViewport(&_viewport);
	_deviceResources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_deviceResources->InitInputLayout(_inputLayout);
	_deviceResources->InitVertexShader(_vertexShader);
	_deviceResources->InitPixelShader(_pixelShader);

	g_PSCBuffer = { 0 };
	g_PSCBuffer.brightness		= MAX_BRIGHTNESS;
	g_PSCBuffer.fBloomStrength	= 1.0f;
	g_PSCBuffer.fPosNormalAlpha	= 1.0f;
	g_PSCBuffer.fSSAOAlphaMult	= g_fSSAOAlphaOfs;
	g_PSCBuffer.AspectRatio		= 1.0f;

	// Laser-specific stuff from ApplySpecialMaterials():
	g_PSCBuffer.fSSAOMaskVal		= EMISSION_MAT;
	g_PSCBuffer.fGlossiness		= DEFAULT_GLOSSINESS;
	g_PSCBuffer.fSpecInt			= DEFAULT_SPEC_INT;
	g_PSCBuffer.fNMIntensity		= 0.0f;
	g_PSCBuffer.fSpecVal			= 0.0f;
	g_PSCBuffer.bIsShadeless		= 1;
	g_PSCBuffer.fPosNormalAlpha = 0.0f;
	// Laser-specific stuff from ApplyBloomSettings():
	g_PSCBuffer.fBloomStrength	= g_BloomConfig.fLasersStrength;
	//g_PSCBuffer.bIsLaser			= 2; // Enhance lasers by default

	// Flags used in RenderScene():
	_bIsCockpit		= false;
	_bIsGunner		= false;
	_bIsBlastMark	= false;
	
	// Just in case we need to do anything for VR or other alternative display devices...
	ExtraPreprocessing();

	// Apply the VS and PS constants
	resources->InitPSConstantBuffer3D(resources->_PSConstantBuffer.GetAddressOf(), &g_PSCBuffer);
	//resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);

	// Other stuff that is common in the loop below
	UINT vertexBufferStride = sizeof(D3dVertex);
	UINT vertexBufferOffset = 0;
	// Run the deferred commands
	for (DrawCommand command : _LaserDrawCommands) {
		// Set the textures
		_deviceResources->InitPSShaderResourceView(command.colortex->_textureView.Get(),
			command.lighttex == nullptr ? nullptr: command.lighttex->_textureView.Get());

		// Set the mesh buffers
		ID3D11ShaderResourceView* vsSSRV[3] = { command.vertexSRV, command.normalsSRV, command.texturesSRV };
		context->VSSetShaderResources(0, 3, vsSSRV);

		// Set the index and vertex buffers
		_deviceResources->InitVertexBuffer(nullptr, nullptr, nullptr);
		_deviceResources->InitVertexBuffer(&(command.vertexBuffer), &vertexBufferStride, &vertexBufferOffset);
		_deviceResources->InitIndexBuffer(nullptr, true);
		_deviceResources->InitIndexBuffer(command.indexBuffer, true);

		// Set the constants buffer
		context->UpdateSubresource(_constantBuffer, 0, nullptr, &(command.constants), 0, 0);

		// Set the number of triangles
		_trianglesCount = command.trianglesCount;

		// Render the deferred commands
		RenderScene();
	}

	// Clear the command list and restore the previous state
	_LaserDrawCommands.clear();
	RestoreContext();
}

void EffectsRenderer::RenderTransparency()
{
	if (_TransparentDrawCommands.size() == 0)
		return;

	auto &resources = _deviceResources;
	auto &context = resources->_d3dDeviceContext;

	SaveContext();

	context->VSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());
	context->PSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());
	// Set the proper rastersize and depth stencil states for transparency
	_deviceResources->InitBlendState(_transparentBlendState, nullptr);
	_deviceResources->InitDepthStencilState(_transparentDepthState, nullptr);

	_deviceResources->InitViewport(&_viewport);
	_deviceResources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_deviceResources->InitInputLayout(_inputLayout);
	_deviceResources->InitVertexShader(_vertexShader);

	// Just in case we need to do anything for VR or other alternative display devices...
	ExtraPreprocessing();

	// Other stuff that is common in the loop below
	UINT vertexBufferStride = sizeof(D3dVertex);
	UINT vertexBufferOffset = 0;

	// Run the deferred commands
	for (DrawCommand command : _TransparentDrawCommands) {
		g_PSCBuffer = command.PSCBuffer;
		g_DCPSCBuffer = command.DCPSCBuffer;

		// Flags used in RenderScene():
		_bIsCockpit = command.bIsCockpit;
		_bIsGunner = command.bIsGunner;
		_bIsBlastMark = command.bIsBlastMark;

		// Apply the VS and PS constants
		resources->InitPSConstantBuffer3D(resources->_PSConstantBuffer.GetAddressOf(), &g_PSCBuffer);
		//resources->InitVSConstantBuffer3D(resources->_VSConstantBuffer.GetAddressOf(), &g_VSCBuffer);
		if (g_PSCBuffer.DynCockpitSlots > 0)
			resources->InitPSConstantBufferDC(resources->_PSConstantBufferDC.GetAddressOf(), &g_DCPSCBuffer);

		// Set the textures
		_deviceResources->InitPSShaderResourceView(command.SRVs[0], command.SRVs[1]);

		// Set the mesh buffers
		ID3D11ShaderResourceView* vsSSRV[3] = { command.vertexSRV, command.normalsSRV, command.texturesSRV };
		context->VSSetShaderResources(0, 3, vsSSRV);

		// Set the index and vertex buffers
		_deviceResources->InitVertexBuffer(nullptr, nullptr, nullptr);
		_deviceResources->InitVertexBuffer(&(command.vertexBuffer), &vertexBufferStride, &vertexBufferOffset);
		_deviceResources->InitIndexBuffer(nullptr, true);
		_deviceResources->InitIndexBuffer(command.indexBuffer, true);

		// Set the constants buffer
		context->UpdateSubresource(_constantBuffer, 0, nullptr, &(command.constants), 0, 0);

		// Set the number of triangles
		_trianglesCount = command.trianglesCount;

		// Set the right pixel shader
		_deviceResources->InitPixelShader(command.pixelShader);

		// Render the deferred commands
		RenderScene();

		// Decrease the refcount of the textures
		for (int i = 0; i < 2; i++)
			if (command.SRVs[i] != nullptr) command.SRVs[i]->Release();
	}

	// Clear the command list and restore the previous state
	_TransparentDrawCommands.clear();
	RestoreContext();
}

/*
 * Using the current 3D box limits loaded in g_OBJLimits, compute the 2D/Z-Depth limits
 * needed to center the Shadow Map depth buffer. This version uses the transforms in
 * g_CockpitConstants
 */
Matrix4 EffectsRenderer::GetShadowMapLimits(Matrix4 L, float *OBJrange, float *OBJminZ) {
	float minx = 100000.0f, maxx = -100000.0f;
	float miny = 100000.0f, maxy = -100000.0f;
	float minz = 100000.0f, maxz = -100000.0f;
	float cx, cy, sx, sy;
	Matrix4 S, T;
	Vector4 P, Q;
	Matrix4 WorldView = XwaTransformToMatrix4(_CockpitWorldView);
	FILE *file = NULL;

	if (g_bDumpSSAOBuffers) {
		fopen_s(&file, "./Limits.OBJ", "wt");

		XwaTransform M = _CockpitWorldView;
		log_debug("[DBG] -----------------------------------------------");
		log_debug("[DBG] GetShadowMapLimits WorldViewTransform:");
		log_debug("[DBG] %0.3f, %0.3f, %0.3f, %0.3f",
			M.Rotation._11, M.Rotation._12, M.Rotation._13, 0.0f);
		log_debug("[DBG] %0.3f, %0.3f, %0.3f, %0.3f",
			M.Rotation._21, M.Rotation._22, M.Rotation._23, 0.0f);
		log_debug("[DBG] %0.3f, %0.3f, %0.3f, %0.3f",
			M.Rotation._31, M.Rotation._32, M.Rotation._33, 0.0f);
		log_debug("[DBG] %0.3f, %0.3f, %0.3f, %0.3f",
			M.Position.x, M.Position.y, M.Position.z, 1.0f);
		log_debug("[DBG] -----------------------------------------------");
	}

	for (Vector4 X : g_OBJLimits) {
		// This transform chain should be the same we apply in ShadowMapVS.hlsl

		// OPT to camera view transform. First transform object space into view space:
		Q = WorldView * X;
		// Now, transform OPT coords to meters:
		Q *= OPT_TO_METERS;
		// Invert the Y-axis since our coordinate system has Y+ pointing up
		Q.y = -Q.y;
		// Just make sure w = 1
		Q.w = 1.0f;

		// The point is now in metric 3D, with the POV at the origin.
		// Apply the light transform, keep the points in metric 3D.
		P = L * Q;

		// Update the limits
		if (P.x < minx) minx = P.x;
		if (P.y < miny) miny = P.y;
		if (P.z < minz) minz = P.z;

		if (P.x > maxx) maxx = P.x;
		if (P.y > maxy) maxy = P.y;
		if (P.z > maxz) maxz = P.z;

		if (g_bDumpSSAOBuffers)
			fprintf(file, "v %0.6f %0.6f %0.6f\n", P.x, P.y, P.z);
	}

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

	// Compute the centroid
	cx = (minx + maxx) / 2.0f;
	cy = (miny + maxy) / 2.0f;

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

void EffectsRenderer::RenderShadowMap()
{
	// We're still tagging the lights in PrimarySurface::TagXWALights(). Here we just render
	// the ShadowMap.

	// Shadow Mapping is disabled when the we're in external view or traveling through hyperspace.
	// Maybe also disable it if the cockpit is hidden
	if (!g_ShadowMapping.bEnabled || !g_bShadowMapEnable || !g_ShadowMapping.bUseShadowOBJ || _bExternalCamera ||
		!_bCockpitDisplayed || g_HyperspacePhaseFSM != HS_INIT_ST || !_bCockpitConstantsCaptured ||
		_bShadowsRenderedInCurrentFrame)
		return;

	auto &resources = _deviceResources;
	auto &context = resources->_d3dDeviceContext;
	SaveContext();
	
	context->VSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());
	context->PSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());

	// Enable ZWrite: we'll need it for the ShadowMap
	D3D11_DEPTH_STENCIL_DESC desc;
	ComPtr<ID3D11DepthStencilState> depthState;
	desc.DepthEnable = TRUE;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	desc.DepthFunc = D3D11_COMPARISON_LESS;
	desc.StencilEnable = FALSE;
	resources->InitDepthStencilState(depthState, &desc);
	// Solid blend state, no transparency
	resources->InitBlendState(_solidBlendState, nullptr);

	// Init the Viewport. This viewport has the dimensions of the shadowmap texture
	_deviceResources->InitViewport(&g_ShadowMapping.ViewPort);
	_deviceResources->InitTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_deviceResources->InitInputLayout(_inputLayout);

	_deviceResources->InitVertexShader(resources->_shadowMapVS);
	_deviceResources->InitPixelShader(resources->_shadowMapPS);

	// Set the vertex and index buffers
	UINT stride = sizeof(D3DTLVERTEX), ofs = 0;
	resources->InitVertexBuffer(resources->_shadowVertexBuffer.GetAddressOf(), &stride, &ofs);
	resources->InitIndexBuffer(resources->_shadowIndexBuffer.Get(), false);

	g_ShadowMapVSCBuffer.sm_PCSS_enabled = g_bShadowMapEnablePCSS;
	g_ShadowMapVSCBuffer.sm_resolution = (float)g_ShadowMapping.ShadowMapSize;
	g_ShadowMapVSCBuffer.sm_hardware_pcf = g_bShadowMapHardwarePCF;
	// Select either the SW or HW bias depending on which setting is enabled
	g_ShadowMapVSCBuffer.sm_bias = g_bShadowMapHardwarePCF ? g_ShadowMapping.hw_pcf_bias : g_ShadowMapping.sw_pcf_bias;
	g_ShadowMapVSCBuffer.sm_enabled = g_bShadowMapEnable;
	g_ShadowMapVSCBuffer.sm_debug = g_bShadowMapDebug;
	g_ShadowMapVSCBuffer.sm_VR_mode = g_bEnableVR;

	// Set the constant buffer for the VS and PS.
	resources->InitVSConstantBufferShadowMap(resources->_shadowMappingVSConstantBuffer.GetAddressOf(), &g_ShadowMapVSCBuffer);
	// The pixel shader is empty for the shadow map, but the SSAO/SSDO/Deferred PS do use these constants later
	resources->InitPSConstantBufferShadowMap(resources->_shadowMappingPSConstantBuffer.GetAddressOf(), &g_ShadowMapVSCBuffer);
	// Set the transform matrix
	context->UpdateSubresource(_constantBuffer, 0, nullptr, &(_CockpitConstants), 0, 0);

	// Compute all the lightWorldMatrices and their OBJrange/minZ's first:
	for (int idx = 0; idx < *s_XwaGlobalLightsCount; idx++)
	{
		float range, minZ;
		// Don't bother computing shadow maps for lights with a high black level
		if (g_ShadowMapVSCBuffer.sm_black_levels[idx] > 0.95f)
			continue;

		// Compute the LightView (Parallel Projection) Matrix
		Matrix4 L = ComputeLightViewMatrix(idx, g_CurrentHeadingViewMatrix, false);
		Matrix4 ST = GetShadowMapLimits(L, &range, &minZ);

		g_ShadowMapVSCBuffer.lightWorldMatrix[idx] = ST * L;
		g_ShadowMapVSCBuffer.OBJrange[idx] = range;
		g_ShadowMapVSCBuffer.OBJminZ[idx] = minZ;

		// Render each light to its own shadow map
		g_ShadowMapVSCBuffer.light_index = idx;

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

	if (g_bDumpSSAOBuffers) {
		wchar_t wFileName[80];
		for (int i = 0; i < *s_XwaGlobalLightsCount; i++) {
			context->CopySubresourceRegion(resources->_shadowMapDebug, D3D11CalcSubresource(0, 0, 1), 0, 0, 0,
				resources->_shadowMapArray, D3D11CalcSubresource(0, i, 1), NULL);
			swprintf_s(wFileName, 80, L"c:\\Temp\\_shadowMap%d.dds", i);
			DirectX::SaveDDSTextureToFile(context, resources->_shadowMapDebug, wFileName);
		}
	}
	
	RestoreContext();
	_bShadowsRenderedInCurrentFrame = true;
}

/*
 * This method is called from two places: in Direct3D::Execute() at the beginning of the HUD and
 * in PrimarySurface::Flip() before we start rendering post-proc effects. Any calls placed in this
 * method should be idempotent or they will render the same content twice.
 */
void EffectsRenderer::RenderDeferredDrawCalls()
{
	RenderShadowMap();
	RenderLasers();
	RenderTransparency();
}

static EffectsRenderer g_effects_renderer;

// TODO: Select the appropriate renderer depending on the current config
//#define ORIGINAL_D3D_RENDERER_SHADERS
#ifdef ORIGINAL_D3D_RENDERER_SHADERS
static D3dRenderer &g_current_renderer = g_xwa_d3d_renderer;
#else
static D3dRenderer &g_current_renderer = g_effects_renderer;
#endif

void RenderDeferredDrawCalls() {
	g_current_renderer.RenderDeferredDrawCalls();
}

//************************************************************************
// Hooks
//************************************************************************

void D3dRendererSceneBegin(DeviceResources* deviceResources)
{
	if (!g_config.D3dRendererHookEnabled)
	{
		return;
	}

	g_current_renderer.SceneBegin(deviceResources);
}

void D3dRendererSceneEnd()
{
	if (!g_config.D3dRendererHookEnabled)
	{
		return;
	}

	g_current_renderer.SceneEnd();
}

void D3dRendererFlightStart()
{
	if (!g_config.D3dRendererHookEnabled)
	{
		return;
	}

	g_current_renderer.FlightStart();
}

void D3dRenderLasersHook(int A4)
{
	const auto L0042BBA0 = (void(*)(int))0x0042BBA0;

	g_isInRenderLasers = true;
	L0042BBA0(A4);
	g_isInRenderLasers = false;
}

void D3dRenderMiniatureHook(int A4, int A8, int AC, int A10, int A14)
{
	const auto L00478490 = (void(*)(int, int, int, int, int))0x00478490;

	g_isInRenderMiniature = true;
	L00478490(A4, A8, AC, A10, A14);
	g_isInRenderMiniature = false;
}

void D3dRenderHyperspaceLinesHook(int A4)
{
	const auto L00480A80 = (void(*)(int))0x00480A80;

	g_isInRenderHyperspaceLines = true;
	L00480A80(A4);
	g_isInRenderHyperspaceLines = false;
}

void D3dRendererMainHook(SceneCompData* scene)
{
	if (*(int*)0x06628E0 != 0)
	{
		const auto XwaD3dExecuteBufferLock = (char(*)())0x00595006;
		const auto XwaD3dExecuteBufferAddVertices = (char(*)(void*, int))0x00595095;
		const auto XwaD3dExecuteBufferProcessVertices = (char(*)())0x00595106;
		const auto XwaD3dExecuteBufferAddTriangles = (char(*)(void*, int))0x00595191;
		const auto XwaD3dExecuteBufferUnlockAndExecute = (char(*)())0x005954D6;

		XwaD3dExecuteBufferLock();
		XwaD3dExecuteBufferAddVertices(*(void**)0x064D1A8, *(int*)0x06628E0);
		XwaD3dExecuteBufferProcessVertices();
		XwaD3dExecuteBufferAddTriangles(*(void**)0x066283C, *(int*)0x0686ABC);
		XwaD3dExecuteBufferUnlockAndExecute();

		*(int*)0x0686ABC = 0;
		*(int*)0x06628E0 = 0;
	}

	const auto L00480370 = (void(*)(const SceneCompData* scene))0x00480370;

	if (g_isInRenderHyperspaceLines)
	{
		// The hyperspace effect is super slow for some reason. Let's disable it if we're rendering the new
		// version
		if (!g_bReshadeEnabled)
			L00480370(scene);
		return;
	}

	g_rendererType = RendererType_Main;
	g_current_renderer.MainSceneHook(scene);
	g_current_renderer.BuildGlowMarks(scene);
	g_current_renderer.RenderGlowMarks();
}

void D3dRendererShadowHook(SceneCompData* scene)
{
	g_rendererType = RendererType_Shadow;
	g_current_renderer.HangarShadowSceneHook(scene);
}
