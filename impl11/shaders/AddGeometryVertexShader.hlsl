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
	float4 normal   : COLOR1;
	float2 tex		: TEXCOORD;
};

struct PixelShaderInput
{
	float4 pos		: SV_POSITION;
	float4 color	: COLOR0;
	float4 normal	: COLOR1;
	float2 tex		: TEXCOORD;
};

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;
	if (bFullTransform < 0.5)
	{
		// Regular Vertex Shader
		// This is similar to the regular vertex shader; but here we expect normalized (-1..1)
		// 2D screen coordinates, so we don't need to multiply by vpScale:
		output.pos = float4(input.pos.xyz, 1.0);
	}
	else
	{
		/*
		The reason we're back-projecting to 3D here is because I haven't figured out
		how to apply FOVscale and y_center properly in 3D; but the answer should be
		below now: it's just a matter of massaging the formulae.
		*/
		// Back-project into 3D. In this case, the w component has the original Z value.
		float3 temp = input.pos.xyw;
		temp.x *= aspect_ratio;
		// TODO: Check that the addition of DEFAULT_FOCAL_DIST didn't change this shader
		float3 P = float3(temp.z * temp.xy / DEFAULT_FOCAL_DIST, temp.z);
		// Project again
		P.z = -P.z;
		output.pos = mul(projEyeMatrix, float4(P, 1.0));
		// DirectX will divide output.pos by output.pos.w, so we don't need to do it here,
		// plus we probably can't do it here in the case of SteamVR anyway...

		// The following value is like the depth-buffer value. Setting it to 2 will remove
		// this point
		output.pos.z = 0.0f; // It's OK to ovewrite z after the projection, I'm doing this in SBSVertexShader already
	}

	output.color = input.color.zyxw;
	output.tex = input.tex;
	output.normal = input.normal;
	return output;
}
