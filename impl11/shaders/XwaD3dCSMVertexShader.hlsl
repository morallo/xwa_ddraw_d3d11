/*
 * Cascaded Shadow Maps (CSM) Vertex Shader.
 */
#include "XwaD3dCommon.hlsl"
#include "shader_common.h"
#include "shadow_mapping_common.h"

Buffer<float3> g_vertices : register(t0);

// OPTMeshTransformCBuffer
cbuffer ConstantBuffer : register(b8) {
	matrix MeshTransform;
	// 64 bytes
};

struct VertexShaderInput
{
	int4 index : POSITION;
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
};

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;

	float3 v = g_vertices[input.index.x];
	// OPT (object space) to viewspace transform
	v = mul(float4(v, 1.0f), MeshTransform).xyz;
	float4 P = mul(float4(v, 1.0f), transformWorldView);
	P *= 0.024414; // 1/40.96, OPT to metric conversion factor
	P.y = -P.y; // Further conversion of the coordinate system (Y+ is up)
	// At this point, P is metric, X+ is right, Y+ is up and Z+ is forward (away from the camera)
	// P is now equivalent to pos3D in the regular vertex shader

	// We have transformed from OPT 3D to metric 3D, with the POV at the origin.
	// let's apply the light transform and project it from the light's point of view.

	// When computing shadows in SSDOAdd/SSAOAdd, the transformation rule starts at this point
	P = mul(lightWorldMatrix[light_index], P);

	// xy: Parallel projection
	// Map the useful Z depth into the range 0..0.98:
	output.pos = float4(P.xy, MetricZToDepth(light_index, P.z), 1.0);

	return output;
}
