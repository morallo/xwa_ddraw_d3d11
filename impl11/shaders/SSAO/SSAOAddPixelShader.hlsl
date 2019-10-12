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

// The Bent Normals buffer
Texture2D texBent : register(t4);
SamplerState samplerBent : register(s4);

// The Normals buffer
Texture2D texNormal : register(t5);
SamplerState samplerNormal : register(s5);

// We're reusing the same constant buffer used to blur bloom; but here
// we really only use the amplifyFactor to upscale the SSAO buffer (if
// it was rendered at half the resolution, for instance)
cbuffer ConstantBuffer : register(b2)
{
	float pixelSizeX, pixelSizeY, white_point, amplifyFactor;
	// 16 bytes
	float bloomStrength, uvStepSize, saturationStrength;
	uint enableSSAO;
	// 32 bytes
	uint enableBentNormals;
	float norm_weight, unused2, unused3;
};

cbuffer ConstantBuffer : register(b4)
{
	matrix projEyeMatrix;
	matrix viewMatrix;
	matrix fullViewMatrix;
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
	//float3 bentN    = texBent.Sample(samplerBent, input_uv_sub).xyz;
	float4 bloom		= texBloom.Sample(samplerBloom, input.uv);
	float3 ssao		= texSSAO.Sample(samplerSSAO, input_uv_sub).rgb;
	float3 ssaoMask = texSSAOMask.Sample(samplerSSAOMask, input.uv).xyz;
	float  mask     = max(dot(0.333, bloom.xyz), ssaoMask);

	//float3 light  = normalize(float3(1, 1, -0.5));
	//light = mul(viewMatrix, float4(light, 0)).xyz;
	//float BN = dot(bentN, bentN);
	//float diffuse = (BN > 0.1) ? white_point * saturate(dot(bentN, light)) : 1.0f;
	//float ambient = 0.2;

	//if (enableBentNormals) color = saturate((ambient + diffuse) * color);
	//ssao = enableSSAO ? ssao : 1.0f;
	
	float3 mult_layer = lerp(color * ssao, color, mask);
	return float4(mult_layer, 1);
	//return float4(ssaoMask, 1);

	// Let's use SSAO to also lighten some areas:
	//float3 screen_layer = 1 - (1 - color) * (1 - ssao * white_point);
	//float3 mix = lerp(mult_layer, screen_layer, ssao.r);
	//return float4(mix, 1);
	
	//float3 HSV = RGBtoHSV(color);
	//HSV.z *= ssao.r;
	//HSV.z = ssao.r;
	//color = HSVtoRGB(HSV);
	//return float4(color, 1);
}

/*
float4 main(PixelShaderInput input) : SV_TARGET
{
	float2 input_uv_sub = input.uv * amplifyFactor;
	float3 color = texture0.Sample(sampler0, input.uv).xyz;
	float3 bentN = texBent.Sample(samplerBent, input_uv_sub).xyz;
	float3 Normal = texNormal.Sample(samplerNormal, input.uv).xyz;
	float4 bloom = texBloom.Sample(samplerBloom, input.uv);
	float4 ssdo = texSSAO.Sample(samplerSSAO, input_uv_sub);
	float  ssaoMask = texSSAOMask.Sample(samplerSSAOMask, input.uv).x;
	float  mask = max(dot(0.333, bloom.xyz), ssaoMask);
	
	float3 light = normalize(float3(1, 1, -0.5));
	//light = mul(viewMatrix, float4(light, 0)).xyz;
	//float light_occlusion = 1 - saturate(dot(float4(light, 1), ssdo));
	float ssao = (enableSSAO) ? lerp(ssdo.r, 1, mask) : 1;
	float diffuse = (enableBentNormals) ? saturate(dot(bentN, light)) : 1;
	diffuse *= white_point * ssao;
	//float diffuse = white_point * saturate(dot(Normal, light));
	float ambient = 0.2;

	color = saturate((ambient + diffuse) * color);
	//ssao = enableSSAO ? ssao : 1.0f;
	float3 mult_layer = lerp(color * ssao, color, mask);
	return float4(mult_layer, 1);
	//return float4(color, 1);

	// Let's use SSAO to also lighten some areas:
	//float3 screen_layer = 1 - (1 - color) * (1 - ssao * white_point);
	//float3 mix = lerp(mult_layer, screen_layer, ssao.r);
	//return float4(mix, 1);

	//float3 HSV = RGBtoHSV(color);
	//HSV.z *= ssao.r;
	//HSV.z = ssao.r;
	//color = HSVtoRGB(HSV);
	//return float4(color, 1);
}
*/