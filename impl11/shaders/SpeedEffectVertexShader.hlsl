// Copyright (c) 2020 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// This shader is used to render the speed effect particles

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

	// Regular Vertex Shader
	output.pos.xy = (input.pos.xy * vpScale.xy + float2(-1.0, 1.0)) * vpScale.z;
	output.pos.z  = input.pos.z;
	output.pos.w  = 1.0f;

	output.color = input.color.zyxw;
	output.tex   = input.tex;
	return output;
}
