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

// The diffuse buffer
Texture2D texDiff : register(t1);
SamplerState samplerDiff : register(s1);

// The bloom mask buffer
Texture2D texBloom : register(t2);
SamplerState samplerBloom : register(s2);

// The SSAO buffer
Texture2D texSSAO : register(t3);
SamplerState samplerSSAO : register(s3);

// The SSAO mask
Texture2D texSSAOMask : register(t4);
SamplerState samplerSSAOMask : register(s4);

// The Bent Normals buffer
Texture2D texBent : register(t5);
SamplerState samplerBent : register(s5);

// The Normals buffer
Texture2D texNormal : register(t6);
SamplerState samplerNormal : register(s6);

// We're reusing the same constant buffer used to blur bloom; but here
// we really only use the amplifyFactor to upscale the SSAO buffer (if
// it was rendered at half the resolution, for instance)
cbuffer ConstantBuffer : register(b2)
{
	float pixelSizeX, pixelSizeY, unused0, amplifyFactor;
	// 16 bytes
	float bloomStrength, uvStepSize, saturationStrength;
	uint enableSSAO;
	// 32 bytes
	uint enableBentNormals;
	float unused1, depth_weight;
	uint debug;
};

cbuffer ConstantBuffer : register(b4)
{
	matrix projEyeMatrix;
	matrix viewMatrix;
	matrix fullViewMatrix;
	float4 LightVector;
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	float2 input_uv_sub = input.uv * amplifyFactor;
	float3 color = texture0.Sample(sampler0, input.uv).xyz;
	float4 diffuse = texDiff.Sample(samplerDiff, input.uv);
	float3 bentN = texBent.Sample(samplerBent, input_uv_sub).xyz;
	float3 Normal = texNormal.Sample(samplerNormal, input.uv).xyz;
	float4 bloom = texBloom.Sample(samplerBloom, input.uv);
	float3 ssao = texSSAO.Sample(samplerSSAO, input_uv_sub).rgb;
	float3 ssaoMask = texSSAOMask.Sample(samplerSSAOMask, input.uv).xyz;
	float  mask = max(dot(0.333, bloom.xyz), dot(0.333, ssaoMask));
	
	// TODO: Make the ambient component configurable
	ssao = 0.15 + ssao; // Add the ambient component 
	ssao = lerp(ssao, 1, mask);
	return float4(color * ssao, 1);

	//color = saturate((ambient + diffuse) * color);
	//ssao = enableSSAO ? ssao : 1.0f;

	/*ssao = lerp(ssao, 1, mask);
	if (!enableBentNormals) 	bentN = 0;
	color = color * diffuse.xyz * ssao + bentN;
	return float4(color, 1);*/

	return float4(ssao, 1);

	//return float4(color * ssao, 1);
	//return float4(saturate(color * ssao + bentN * light_factor), 1);
	//return float4(ssao, 1);

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
