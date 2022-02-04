// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes, 2019
#include "XwaD3dCommon.hlsl"
#include "VertexShaderCBuffer.h"
#include "VertexShaderMatrixCB.h"
#include "shader_common.h"
#include "metric_common.h"

struct VertexShaderInput
{
	float4 pos		: POSITION;
	float4 color		: COLOR0;
	float4 specular : COLOR1;
	float2 tex		: TEXCOORD;
};

struct PixelShaderInput
{
	float4 pos      : SV_POSITION;
	float4 color    : COLOR0;
	float2 tex      : TEXCOORD0;
	float4 pos3D    : COLOR1;
	float4 normal   : NORMAL; // hook_normals.dll populates this field
};

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;
	float sz = input.pos.z;
	float w = 1.0 / input.pos.w;
	float3 temp;

	temp = float3(input.pos.xy, w);
	temp.z *= mr_FOVscale * mr_z_metric_mult;
	// temp.z is now metric 3D minus mr_cur_metric_scale

	// Override the depth of this element if z_override is set
	if (mult_z_override > -0.1)
		temp.z *= mult_z_override / mr_cur_metric_scale;
	if (z_override > -0.1)
		temp.z = z_override / mr_cur_metric_scale;

	float FOVscaleZ = mr_FOVscale / temp.z;

	// Normalize into the 0.0..1.0 range
	temp.xy *= float2(2.0 * viewportScale.x, -2.0 * viewportScale.y);
	temp.xy += float2(-1.0, 1.0);
	// temp.xy is now in the range [-1..1]

	temp.xy = temp.xy / FOVscaleZ * float2(mr_aspect_ratio, 1.0);
	temp.y -= temp.z * mr_y_center / mr_FOVscale;
	// temp.xyz is now "straight" 3D up to a scale factor

	temp *= mr_cur_metric_scale;
	// temp.xyz is now fully-metric 3D.

	if (apply_uv_comp)
		temp.xy *= mv_vr_vertexbuf_aspect_ratio_comp;
	temp.xy *= scale_override; // Scale GUI objects around the screen center, like the triangle pointer

	// The back-projection into 3D is now very simple:
	//float3 P = float3(temp.z * temp.xy / DEFAULT_FOCAL_DIST_VR, temp.z);
	float3 P = temp;

	// Write the reconstructed 3D into the output so that it gets interpolated:
	output.pos3D = float4(P, 1);

	// Adjust the coordinate system for SteamVR:
	P.z = -P.z;

	// Apply head position and project 3D --> 2D
	if (bPreventTransform < 0.5f) {
		if (bFullTransform < 0.5f)
			output.pos = mul(viewMatrix, float4(P, 1));
		else
			output.pos = mul(fullViewMatrix, float4(P, 1));
	}
	else {
		// bPreventTransform == true
		// The HUD should not be transformed so that it's possible to aim properly
		// This case is specifically to keep the aiming HUD centered so that it can still be used
		// to aim the lasers. Here, we ignore all translations to keep the HUD in the right spot.
		float4x4 compViewMatrix = viewMatrix;
		compViewMatrix._m03_m13_m23 = 0;
		output.pos = mul(compViewMatrix, float4(P, 1));
	}
	output.pos = mul(projEyeMatrix, output.pos);

	// Use the original sz; but compensate with the new w so that it stays perspective-correct:
	output.pos.z = sz * output.pos.w;
	// NOTE: The use of this w coming from the perspective matrix may have fixed the ghosting effect
	//		 in the Pimax. Better not to use the original w coming from the game.
	if (sz_override > -0.1)
		output.pos.z = sz_override * output.pos.w;

	output.color  = input.color.zyxw;
	output.normal = input.specular;
	output.tex    = input.tex;
	return output;
}
