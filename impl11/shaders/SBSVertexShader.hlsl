// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes, 2019
#include "shader_common.h"
#include "metric_common.h"

// VertexShaderCBuffer
cbuffer ConstantBuffer : register(b0)
{
	float4 vpScale;
	float aspect_ratio, cockpit_threshold, z_override, sz_override;
	float mult_z_override, bPreventTransform, bFullTransform, metric_mult;
	//float post_proj_scale, vsunused0, vsunused1, vsunused2;
};

// VertexShaderMatrixCB
cbuffer ConstantBuffer : register(b1)
{
	matrix projEyeMatrix;
	matrix viewMatrix;
	matrix fullViewMatrix;
};

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
	float4 normal   : NORMAL; // hook_normals.dll populates this field
};

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;
	float sz = input.pos.z;
	float w = 1.0 / input.pos.w;

	/*
	float3 temp = input.pos.xyz;
	// Normalize into the -0.5..0.5 range
	temp.xy *= vpScale.xy;
	temp.xy -= 0.5;

	// Apply the scale in 2D coordinates before back-projecting. This is
	// either g_fGlobalScale or g_fGUIElemScale (used to zoom-out the HUD
	// so that it's readable)
	//temp.xy *= vpScale.w * vpScale.z * float2(aspect_ratio, 1);
	temp.xy *= vpScale.w * vpScale.z * float2(aspect_ratio, 1);
	temp.z = METRIC_SCALE_FACTOR * metric_mult * w; // METRIC_SCALE_FACTOR was determined empirically
	// temp.z = w; // This setting provides a really nice depth for distant objects; but the cockpit is messed up
	*/

	float3 temp = float3(input.pos.xy, w);
	temp.z *= mr_FOVscale * mr_z_metric_mult;
	// temp.z is now metric 3D

	// Override the depth of this element if z_override is set
	if (mult_z_override > -0.1)
		temp.z *= mult_z_override; // * mr_cur_metric_scale ?
	if (z_override > -0.1)
		temp.z = z_override; // * mr_cur_metric_scale ?

	float FOVscaleZ = mr_FOVscale / temp.z;

	// Normalize into the 0.0..1.0 range
	temp.xy *= float2(2.0 * vpScale.x, -2.0 * vpScale.y);
	temp.xy += float2(-1.0, 1.0);
	// temp.xy is now in the range [-1..1]

	temp.xy = temp.xy / FOVscaleZ * float2(mr_aspect_ratio, 1.0);
	temp.y -= temp.z * mr_y_center / mr_FOVscale;
	// temp.xyz is now "straight" 3D up to a scale factor

	temp *= mr_cur_metric_scale;
	// temp.xyz is now fully-metric 3D.

	// The back-projection into 3D is now very simple:
	//float3 P = float3(temp.z * temp.xy / DEFAULT_FOCAL_DIST_VR, temp.z);
	float3 P = temp;

	// Write the reconstructed 3D into the output so that it gets interpolated:
	//output.pos3D = float4(P.x, -P.y, P.z, 1);
	output.pos3D = float4(P, 1);

	// Adjust the coordinate system for SteamVR:
	//P.yz = -P.yz;
	P.z = -P.z;
	
	// Apply head position and project 3D --> 2D
	if (bPreventTransform < 0.5f) {
		if (bFullTransform < 0.5f)
			output.pos = mul(viewMatrix, float4(P, 1));
		else
			output.pos = mul(fullViewMatrix, float4(P, 1));
	} else {
		// The HUD should not be transformed so that it's possible to aim properly
		// This case is specifically to keep the aiming HUD centered so that it can still be used
		// to aim the lasers. Here, we ignore all translations to keep the HUD in the right spot.
		float4x4 compViewMatrix = viewMatrix;
		compViewMatrix._m03_m13_m23 = 0;
		output.pos = mul(compViewMatrix, float4(P, 1));
	}
	output.pos = mul(projEyeMatrix, output.pos);
	//output.pos.xy *= post_proj_scale; // I think this caused double images/blurry vision
	//output.pos.w = 1.0f;

	// Stereoscopy boost -- probably not worth it, needs to be able to distinguish the skybox and the HUD is amplified too
	/*
	if (pz > cockpit_threshold) {
		float w_orig = output.pos.w;
		metric_scale = 1;
		temp.xy *= vpScale.w * vpScale.z * float2(aspect_ratio, 1);
		temp.z = metric_scale * w; // This value was determined empirically so that the X-Wing cockpit had a reasonably metric size
		// temp.z = w; // This setting provides a really nice depth for distant objects.
		if (mult_z_override > -0.1)
			temp.z *= mult_z_override;
		if (z_override > -0.1)
			temp.z = z_override;
		float3 P = float3(temp.z * temp.xy, temp.z);
		// Adjust the coordinate system for SteamVR:
		P.y = -P.y;
		P.z = -P.z;
		// Apply head position and project 3D --> 2D
		output.pos = mul(viewMatrix, float4(P, 1));
		output.pos = mul(projEyeMatrix, output.pos);
		output.pos *= w_orig / output.pos.w;
	}
	*/

	// For some weird reason the following line also provides perspective-correct texturing;
	// but it's using the old sz and w:
	//output.pos.z = sz * w;

	// Use the original sz; but compensate with the new w so that it stays perspective-correct:
	output.pos.z = sz * output.pos.w;
	// NOTE: The use of this w coming from the perspective matrix may have fixed the ghosting effect
	//		 in the Pimax. Better not to use the original w coming from the game.
	if (sz_override > -0.1)
		output.pos.z = sz_override;

	output.color  = input.color.zyxw;
	output.tex	  = input.tex;
	output.normal = input.specular;
	return output;
}
