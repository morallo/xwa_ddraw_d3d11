// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes, 2019
#include "XwaD3dCommon.hlsl"
#include "VertexShaderCBuffer.h"
#include "shader_common.h"
#include "metric_common.h"

Buffer<float3> g_vertices : register(t0);
Buffer<float3> g_normals : register(t1);
Buffer<float2> g_textureCoords : register(t2);

// OPTMeshTransformCBuffer
cbuffer ConstantBuffer : register(b8) {
	matrix MeshTransform;
	// 64 bytes
};

struct VertexShaderInput
{
	int4 index : POSITION;
};

// VertexShaderMatrixCB
cbuffer ConstantBuffer : register(b2)
{
	matrix projEyeMatrix;
	matrix viewMatrix;
	matrix fullViewMatrix;
};

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 pos3D  : COLOR1;
	float4 normal : NORMAL;
	float2 tex    : TEXCOORD;
	//float4 color  : COLOR0;
};

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;

	// The vertices and normals are in the object OPT coordinate system
	float3 v = g_vertices[input.index.x];
	float3 n = g_normals[input.index.y];
	float2 t = g_textureCoords[input.index.z];

	output.normal = mul(float4(n, 0.0f), transformWorldView);
	output.tex = t;

	// The 3D coordinates we get here are in "OPT scale". The conversion factor from
	// OPT to meters is 1/40.96. We need to compensate that somewhere, but we can't
	// do it in the transformWorldView matrix because the depth calculations are done
	// in the original (OPT) coordinate system. In other words, the depth will be messed
	// up. The compensation must be done later, when we write the coordinates to pos3D below
	v = mul(float4(v, 1.0f), MeshTransform).xyz;
	output.pos3D = mul(float4(v, 1.0f), transformWorldView);
	// Original non-VR 3D -> 2D projection. We still need this to have a depth buffer that is
	// consistent with the D3DRendererHook.
	// TODO: Optimization opportunity. We only need the zw components here, not a full matrix
	//		 transform.
	float4 Q = TransformProjection(output.pos3D.xyz);
	// Convert pos3D from OPT scale to metric scale
	output.pos3D *= 0.024414; // 1/40.96, OPT to metric conversion factor
	output.pos3D.y = -output.pos3D.y; // Further conversion of the coordinate system (Y+ is up)
	// At this point, output.pos3D is metric, X+ is right, Y+ is up and Z+ is forward (away from the camera)

	// Override the depth of this element if z_override is set
	// The following is probably not needed anymore, I think it's used to place the background at infinity
	// which is done in the Execute() path
	if (mult_z_override > -0.1)
		output.pos3D.z *= mult_z_override;
	// This is probably needed to render the miniature for non-DC cockpits
	if (z_override > -0.1)
		output.pos3D.z = z_override;
	// Not sure this is needed anymore:
	//if (apply_uv_comp)
	//	output.pos3D.xy *= mv_vr_vertexbuf_aspect_ratio_comp;
	//output.pos3D.xy *= scale_override; // Scale GUI objects around the screen center, like the triangle pointer

	float3 P = output.pos3D.xyz;
	// Adjust the coordinate system for SteamVR:
	P.z = -P.z;

	// Apply the head position and project 3D --> 2D
	if (bFullTransform < 0.5f)
		output.pos = mul(viewMatrix, float4(P, 1));
	else
		output.pos = mul(fullViewMatrix, float4(P, 1));
	output.pos = mul(projEyeMatrix, output.pos);

	// We need the original depth to be consistent with the rest of the draw calls in Execute().
	// Use the original Depth; but compensate with the new w so that it stays perspective-correct:

	// The original depth value would be Q.z/Q.w because the fixed pipeline always divides by w
	// even if we don't see it. For that same reason, we multiply by output.pos.w so that we keep
	// Q.z/Q.w after the pipeline divides by w.
	output.pos.z = (Q.z/Q.w) * output.pos.w;

	// NOTE: The use of this w coming from the perspective matrix may have fixed the ghosting effect
	//		 in the Pimax. Better not to use the original w coming from the game.
	if (sz_override > -0.1)
		output.pos.z = sz_override * output.pos.w;

	return output;
}
