// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes, 2019

Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

struct PixelShaderInput
{
	float4 pos   : SV_POSITION;
	float4 color : COLOR0;
	float2 tex   : TEXCOORD0;
	float3 pos3D : TEXCOORD1;
};

struct PixelShaderOutput
{
	float4 color		: SV_TARGET0;
	float4 diffuse	: SV_TARGET1;
	float4 bloom		: SV_TARGET2;
	float4 pos3D		: SV_TARGET3;
	float4 normal	: SV_TARGET4;
	float4 ssaoMask : SV_TARGET5;
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.color		= input.color;
	output.diffuse  = 0;
	output.bloom		= 0;
	output.pos3D		= 0;
	output.normal	= 0;
	output.ssaoMask = 1;
	return output;
}
