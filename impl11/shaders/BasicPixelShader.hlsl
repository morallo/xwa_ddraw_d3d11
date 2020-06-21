// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes (c) 2019

Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

// MainShadersCBuffer
cbuffer ConstantBuffer : register(b0)
{
	float scale, aspect_ratio, parallax, brightness;
	float use_3D, unused0, unused1, unused2;
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	float4 texelColor = texture0.Sample(sampler0, input.tex);

	return float4(brightness * texelColor.xyz, 1.0f);
}
