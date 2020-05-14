// Copyright (c) 2020 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// This shader is used to render the speed effect particles
#include "ShadertoyCBuffer.h"

// VertexShaderCBuffer
cbuffer ConstantBuffer : register(b0)
{
	float4 vpScale;
	float aspect_ratio, cockpit_threshold, z_override, sz_override;
	float mult_z_override, bPreventTransform, bFullTransform, metric_mult;
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
	float4 pos		: SV_POSITION;
	float4 color	: COLOR0;
	float2 tex		: TEXCOORD;
};

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;

	// SBS/SteamVR:
	//output.pos = mul(projEyeMatrix, input.pos);

	// VS from the DC HUD shader:
	//output.pos.x = (input.pos.x - 0.5) * 2.0;
	//output.pos.y = -(input.pos.y - 0.5) * 2.0;
	//output.pos.z = input.pos.z;
	//output.pos.w = 1.0f;

	if (bFullTransform < 0.5) 
	{
		// Regular Vertex Shader
		//output.pos.xy = (input.pos.xy * vpScale.xy + float2(-1.0, 1.0)) * vpScale.z;
		// This is similar to the regular vertex shader; but here we expect normalized (-1..1)
		// 2D screen coordinates, so we don't need to multiply by vpScale:
		//output.pos.xy = input.pos.xy * float2(1.0 / aspect_ratio, 1.0);
		output.pos.xy = input.pos.xy;
		//output.pos.xy = (input.pos.xy * float2(1.0 / aspect_ratio, 1.0)) + float2(0.0, y_center);
		//output.pos.xy *= FOVscale;
		output.pos.z = input.pos.z;
		output.pos.w = 1.0f;
	}
	else 
	{
		/*
		output.pos = mul(projEyeMatrix, float4(input.pos.xyz, 1.0));
		output.pos.xyz /= output.pos.w;
		output.pos.x *= aspect_ratio;
		// Apply FOVscale and y_center
		//output.pos.xy = FOVscale * output.pos.xy + float2(0.0, y_center);
		
		*/

		/*
		The reason we're back-projecting to 3D here is because I haven't figured out
		how to apply FOVscale and y_center properly in 3D; but the answer should be
		below now: it's just a matter of massaging the formulae.
		*/
		// Back-project into 3D
		float3 temp = (input.pos.xyw);
		temp.x *= aspect_ratio;
		float3 P = float3(temp.z * temp.xy, temp.z);
		// Project again
		P.z = -P.z;
		output.pos = mul(projEyeMatrix, float4(P, 1.0));
		output.pos.xyz /= output.pos.w;
		// The following value is like the depth-buffer value. Setting it to 2 will remove
		// this point
		output.pos.z = 0.0f;
		// We're already providing 2D coords here, so we can set w = 1
		output.pos.w = 1.0f;
	}

	output.color = input.color.zyxw;
	output.tex   = input.tex;
	return output;
}
