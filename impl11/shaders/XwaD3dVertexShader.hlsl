
#include "XwaD3dCommon.hlsl"

Buffer<float3> g_vertices : register(t0);
Buffer<float3> g_normals : register(t1);
Buffer<float2> g_textureCoords : register(t2);

struct VertexShaderInput
{
	int4 index : POSITION;
};

/*
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
	output.normal = mul(float4(n, 1.0f), transformWorldView);
	output.tex = t;

	output.color = float4(1.0f, 1.0f, 1.0f, 0.0f);

	//if (c == 0)
	if (renderType == 0)
	{
		output.color = ComputeColorLight(v, n);
	}

	return output;
}
*/

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 pos3D  : COLOR2;
	float4 normal : COLOR1;
	float2 tex    : TEXCOORD;
	//float4 color  : COLOR0;
};

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;

	// Vertices in world coordinates (?)
	float3 v = g_vertices[input.index.x];
	// Normals in world coordinates (?)
	float3 n = g_normals[input.index.y];
	float2 t = g_textureCoords[input.index.z];
	//int c = input.index.w; // This isn't used even in the original code above

	// The 3D coordinates are way too big. About 27.5 times too big, in fact. We need
	// to compensate that somewhere, but we can't do it in the transformWorldView matrix
	// because the depth calculations are done in the original coordinate system. In
	// other words, the depth will be messed up. The compensation must be done later,
	// when we write the coordinates to pos3D below
	output.pos3D = mul(float4(v, 1.0f), transformWorldView);
	output.pos = TransformProjection(output.pos3D.xyz);
	output.pos3D *= 0.0363636; // Approx 1/27.5

	output.normal = mul(float4(n, 0.0f), transformWorldView);
	output.tex = t;
	//output.color = float4(1.0f, 1.0f, 1.0f, 0.0f);

	/*
	if (renderType == 0)
	{
		// Gouraud Shading, we're not going to use that in the Effects branch.
		output.color = ComputeColorLight(v, n);
	}
	*/
	// Note: All the other render types are probably shadeless

	return output;
}
