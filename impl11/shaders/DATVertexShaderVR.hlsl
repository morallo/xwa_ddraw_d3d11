// Copyright (c) 2022 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// This shader should only be used on DAT content that is meant to be
// in 3D space, like the engine glow, explosions and probably others.
// It will back-project the DAT input geometry into 3D space at OPT-scale
// so that it's compatible with XwaD3dVertexShader and XwaD3dVertexShaderVR.
// This is why 2D content, like brackets the HUD and the reticle *should not*
// be rendered with this shader.
#include "XwaD3dCommon.hlsl"
#include "VertexShaderCBuffer.h"
#include "VertexShaderMatrixCB.h"
#include "shader_common.h"
#include "metric_common.h"

struct VertexShaderInput
{
	float4 pos		: POSITION;
	float4 color	: COLOR0;
	float4 specular : COLOR1;
	float2 tex		: TEXCOORD;
};

struct PixelShaderInput
{
	float4 pos      : SV_POSITION;
	float4 color    : COLOR0;
	float2 tex      : TEXCOORD0;
	float4 pos3D    : COLOR1;
	float4 normal   : NORMAL;
};

float3 InverseTransformProjectionScreen(float4 input)
{
	float3 P;
	// input.xy is in screen coords (0,0)-(W,H), convert to normalized DirectX: -1..1
	P.x = (input.x * viewportScale.x) - 1.0f;
	P.y = (input.y * viewportScale.y) + 1.0f;
	// input.xy is now in the range -1..1, invert the formulas in TransformProjection:
	P.x = (P.x / viewportScale.z + 1.0f) / viewportScale.x;
	P.y = (P.y / viewportScale.z - 1.0f) / viewportScale.y;
	// Special case when w == z:
	P.z = Zfar / input.w - Zfar;
	// We can now continue inverting the formulas in TransformProjection
	float st0 = Znear / P.z;
	P.x = (P.x - DeltaX) / st0;
	P.y = (P.y - DeltaY) / st0;
	// P is now OPT-scale
	return P;
}

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;
	float sz = input.pos.z;
	float w = 1.0 / input.pos.w;
	float3 temp;

	float3 P = InverseTransformProjectionScreen(input.pos);
	// P is now OPT-scale, we can use the regular projection to get the final sz
	// TODO: We only need Q.zw

	//float4 Q = TransformProjection(P);
	float st0 = Znear / P.z;
	// DEPTH-BUFFER-CHANGE DONE
	//float Qz = (st0 * Zfar / 32) / (abs(st0) * Zfar / 32 + Znear / 3) * 0.5;
	float Qz = (st0 * Zfar / projectionParametersVS.x) / (abs(st0) * Zfar / projectionParametersVS.y + Znear * projectionParametersVS.z);
	//float Qw = 1.0f / st0;

	// Transform to metric 3D
	P *= 0.024414; // 1/40.96, OPT to metric conversion factor
	P.y = -P.y; // Further conversion of the coordinate system (Y+ is up)
	// Write the reconstructed 3D into the output so that it gets interpolated:
	output.pos3D = float4(P, 1);
	// Adjust the coordinate system for SteamVR:
	P.z = -P.z;
	// Apply head position and project 3D --> 2D
	if (bFullTransform < 0.5f)
		output.pos = mul(viewMatrix, float4(P, 1));
	else
		output.pos = mul(fullViewMatrix, float4(P, 1));
	output.pos = mul(projEyeMatrix, output.pos);

	// At this point, we should be doing:
	//
	// output.pos.z = (Qz / Qw) * output.pos.w
	//
	// Just like we do in XwaD3dVertexShaderVR. However, we're dividing Qz by
	// Qw, and then multiplying by output.pos.w and then the fixed pipeline will 
	// divide by output.pos.w again. That causes a lot of precision loss. So,
	// instead we can avoid all that by setting output.pos.w = 1. There doesn't seem
	// to be any ill effects because these are billboards and never cross the projection
	// plane, so I haven't noticed any artifacts.
	// We can't do the same (output.pos.w = 1) in XwaD3dVertexShaderVR because some
	// polygons cross the projection plane, and that causes severe artifacts.
	// Finally: Not sure if we can do this in SteamVR -- will it cause double images or
	// other artifacts?
	output.pos /= output.pos.w;
	output.pos.z = Qz;
	output.pos.w = 1.0;
	
	output.color  = input.color.zyxw;
	output.normal = input.specular;
	output.tex = input.tex;
	return output;
}
