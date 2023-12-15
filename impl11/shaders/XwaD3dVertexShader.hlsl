#include "XwaD3dCommon.hlsl"

Buffer<float3> g_vertices : register(t0);
Buffer<float3> g_normals : register(t1);
Buffer<float2> g_textureCoords : register(t2);
Buffer<float3> g_tangents : register(t3);

// OPTMeshTransformCBuffer
cbuffer ConstantBuffer : register(b8) {
	matrix MeshTransform;
	// 64 bytes
};

struct VertexShaderInput
{
	uint4 index : POSITION;
};


//#define ORIGINAL_D3D_RENDERER_SHADERS
#ifdef ORIGINAL_D3D_RENDERER_SHADERS
// This is the original PixelShaderInput used with the D3DRendererHook
struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float4 position : COLOR2;
	float4 normal : COLOR1;
	float2 tex : TEXCOORD;
	float4 color : COLOR0;
};

// This is the original Vertex Shader for the D3DRendererHook
PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;

	float3 v = g_vertices[input.index.x];
	float3 n = g_normals[input.index.y];
	float2 t = g_textureCoords[input.index.z];
	int c = input.index.w;

	output.pos = mul(float4(v, 1.0f), transformWorldView);
	output.pos = TransformProjection(output.pos.xyz);

	output.position = float4(v, 1.0f);
	//output.normal = mul(float4(n, 1.0f), transformWorldView); // Original version
	output.normal = mul(float4(n, 0.0f), transformWorldView); // Fixed version
	output.tex = t;

	output.color = float4(1.0f, 1.0f, 1.0f, 0.0f);

	//if (c == 0)
	if (renderType == 0)
	{
		output.color = ComputeColorLight(v, n);
	}

	return output;
}
#else
struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 pos3D  : COLOR1;
	float4 normal : NORMAL;
	float2 tex    : TEXCOORD;
	//float4 color  : COLOR0;
	//float4 tangent : TANGENT;
};

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;

	// The vertices and normals are in the object OPT coordinate system
	float3 v = g_vertices[input.index.x];
	float3 n = g_normals[input.index.y];
	float2 t = g_textureCoords[input.index.z];
	//float3 T = g_tangents[input.index.y];
	//int c = input.index.w; // This isn't used even in the original code above

	// The 3D coordinates we get here are in "OPT scale". The conversion factor from
	// OPT to meters is 1/40.96. We need to compensate that somewhere, but we can't
	// do it in the transformWorldView matrix because the depth calculations are done
	// in the original (OPT) coordinate system. In other words, the depth will be messed
	// up. The compensation must be done later, when we write the coordinates to pos3D below
	v = mul(float4(v, 1.0f), MeshTransform).xyz;
	output.pos3D = mul(float4(v, 1.0f), transformWorldView);
	output.pos = TransformProjection(output.pos3D.xyz);
	output.pos3D *= 0.024414; // 1/40.96, OPT to metric conversion factor
	output.pos3D.y = -output.pos3D.y; // Further conversion of the coordinate system (Y+ is up)
	// At this point, output.pos3D is metric, X+ is right, Y+ is up and Z+ is forward (away from the camera)

	n = mul(float4(n, 0.0f), MeshTransform).xyz;
	//T = mul(float4(T, 0.0f), MeshTransform).xyz;
	output.normal = mul(float4(n, 0.0f), transformWorldView);
	//output.tangent = mul(float4(T, 0.0f), transformWorldView);
	output.tex = t;
	//output.color = float4(1.0f, 1.0f, 1.0f, 0.0f);

	return output;
}
#endif