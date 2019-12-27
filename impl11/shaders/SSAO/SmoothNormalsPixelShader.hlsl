/*
 * Simple shader to smooth the normal buffer
 * Copyright 2019, Leo Reyes.
 * Licensed under the MIT license. See LICENSE.txt
 */

// The 3D position buffer (linear X,Y,Z) (maybe later I'll make this a
// depth-aware smooth shader?)
Texture2D    posTex : register(t0);
SamplerState posSampler : register(s0);

// The normal buffer (values are in the -1..1 range)
Texture2D    normTex : register(t1);
SamplerState normSampler : register(s1);

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 normal : SV_TARGET0;
	float4 color  : SV_TARGET1; // DEBUG -- Remove later
};

// We're reusing the constant buffer from the bloom shader here
cbuffer ConstantBuffer : register(b2)
{
	float pixelSizeX, pixelSizeY, unused1, amplifyFactor;
	// 16 bytes
	float bloomStrength, uvStepSize, saturationStrength, unused2;
	// 32 bytes
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float3 N = float3(0, 0, 0);
	float2 delta = uvStepSize * float2(pixelSizeX, pixelSizeY);
	float2 uv_outer = input.uv - 3 * delta;
	float2 uv_inner;

	[unroll]
	for (int i = -3; i <= 3; i++) {
		uv_inner = uv_outer;
		[unroll]
		for (int j = -3; j <= 3; j++) {
			N += normTex.Sample(normSampler, uv_inner).xyz;
			uv_inner.x += delta.x;
		}
		uv_outer.y += delta.y;
	}
	N = normalize(N);
	output.normal = float4(N, 1);
	output.color = float4(N * 0.5 + 0.5, 1); // DEBUG
	return output;
}
