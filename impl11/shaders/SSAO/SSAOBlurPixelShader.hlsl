/*
 * Simple blur for SSAO
 * Copyright 2019, Leo Reyes
 * Licensed under the MIT license. See LICENSE.txt
 */

// The SSAO Buffer
Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

// I'm reusing the constant buffer from the bloom blur shader; but
// we're only using the amplifyFactor here.
cbuffer ConstantBuffer : register(b2)
{
	float pixelSizeX, pixelSizeY, unused1, amplifyFactor;
	// 16 bytes
	float bloomStrength, uvStepSize, saturationStrength, unused2;
	// 32 bytes
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	float2 input_uv = input.uv * amplifyFactor;
	float3 color = float3(0, 0, 0);
	float2 delta = uvStepSize * float2(pixelSizeX, pixelSizeY);
	float2 uv_outer = input_uv - delta;
	float2 uv_inner;

	[unroll]
	for (int i = -2; i <= 2; i++) {
		uv_inner = uv_outer;
		[unroll]
		for (int j = -2; j <= 2; j++) {
			color += texture0.Sample(sampler0, uv_inner).xyz;
			uv_inner.x += delta.x;
		}
		uv_outer.y += delta.y;
	}
	return float4(color / 25, 1);
}