/*
 * Simple shader to add SSAO + Bloom-Mask + Color Buffer
 * Copyright 2019, Leo Reyes.
 * Licensed under the MIT license. See LICENSE.txt
 *
 * The bloom-mask is *not* the final bloom buffer -- this mask is output by the pixel
 * shader and it will be used later to compute proper bloom. Here we use this mask to
 * disable areas of the SSAO buffer that should be bright.
 */
#include "..\HSV.h"

// The color buffer
Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

// The bloom mask buffer
Texture2D texBloom : register(t1);
SamplerState samplerBloom : register(s1);

// The SSAO buffer
Texture2D texSSAO : register(t2);
SamplerState samplerSSAO : register(s2);

// The SSAO mask
Texture2D texSSAOMask : register(t3);
SamplerState samplerSSAOMask : register(s3);

// We're reusing the same constant buffer used to blur bloom; but here
// we really only use the amplifyFactor to upscale the SSAO buffer (if
// it was rendered at half the resolution, for instance)
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
	float2 input_uv_sub = input.uv * amplifyFactor;
	float3 color		= texture0.Sample(sampler0, input.uv).xyz;
	float4 bloom		= texBloom.Sample(samplerBloom, input.uv);
	float3 ssao		= texSSAO.Sample(samplerSSAO, input_uv_sub).rgb;
	float  ssaoMask = texSSAOMask.Sample(samplerSSAOMask, input.uv).x;
	float  mask     = max(dot(0.333, bloom.xyz), ssaoMask);
	
	return float4(lerp(color * ssao, color, mask), 1);
	
	/*float3 HSV = RGBtoHSV(color);
	HSV.z *= ssao.r;
	color = HSVtoRGB(HSV);
	return float4(color, 1);*/
}