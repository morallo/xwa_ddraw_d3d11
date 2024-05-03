/*
 * Simple blur for SSAO
 * Copyright 2019, Leo Reyes
 * Licensed under the MIT license. See LICENSE.txt
 */

#ifdef INSTANCED_RENDERING
 // The SSAO Buffer
Texture2DArray SSAOTex : register(t0);
SamplerState SSAOSampler : register(s0);

// The Depth/Position Buffer
Texture2DArray DepthTex : register(t1);
SamplerState DepthSampler : register(s1);

// The Normal Buffer
Texture2DArray NormalTex : register(t2);
SamplerState NormalSampler : register(s2);
#else
 // The SSAO Buffer
Texture2D SSAOTex : register(t0);
SamplerState SSAOSampler : register(s0);

// The Depth/Position Buffer
Texture2D DepthTex : register(t1);
SamplerState DepthSampler : register(s1);

// The Normal Buffer
Texture2D NormalTex : register(t2);
SamplerState NormalSampler : register(s2);
#endif

struct BlurData {
	float3 pos;
	float3 normal;
};

// I'm reusing the constant buffer from the bloom blur shader; but
// we're only using the amplifyFactor here.
cbuffer ConstantBuffer : register(b2)
{
	float pixelSizeX, pixelSizeY, unused0, amplifyFactor;
	// 16 bytes
	float bloomStrength, uvStepSize, saturationStrength;
	uint unused1;
	// 32 bytes
	uint unused2;
	float unused3, depth_weight, unused4;
	// 48 bytes
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
#ifdef INSTANCED_RENDERING
	uint   viewId : SV_RenderTargetArrayIndex;
#endif
};

struct PixelShaderOutput
{
	float4 ssao : SV_TARGET0;
};

float compute_spatial_tap_weight(in BlurData center, in BlurData tap)
{
	if (tap.pos.z > 30000.0) return 0;
	const float depth_term = saturate(depth_weight - abs(tap.pos.z - center.pos.z));
	const float normal_term = saturate(dot(tap.normal.xyz, center.normal.xyz) * 16 - 15);
	return depth_term * normal_term;
}

#define BLUR_SAMPLES 8
PixelShaderOutput main(PixelShaderInput input) {
	static const float2 offsets[16] =
	{
		float2(1.5, 0.5), float2(-1.5, -0.5), float2(-0.5, 1.5), float2(0.5, -1.5),
		float2(1.5, 2.5), float2(-1.5, -2.5), float2(-2.5, 1.5), float2(2.5, -1.5),
		float2(-1.5, 0.5), float2(1.5, -0.5), float2(0.5, 1.5), float2(-0.5, -1.5),
		float2(-1.5, 2.5), float2(1.5, -2.5), float2(2.5, 1.5), float2(-2.5, -1.5),
	};

	float2 cur_offset, cur_offset_scaled;
	float2 pixelSize = float2(pixelSizeX, pixelSizeY);
	float2 input_uv_scaled = input.uv * amplifyFactor;
	float  blurweight = 0, tap_weight;
	float3 tap_ssao, ssao_sum, ssao_sum_noweight;
	BlurData center, tap;
#ifdef INSTANCED_RENDERING
	center.pos = DepthTex.Sample(DepthSampler, float3(input.uv, input.viewId)).xyz;
#else
	center.pos = DepthTex.Sample(DepthSampler, input.uv).xyz;
#endif

	PixelShaderOutput output;
	output.ssao = float4(0, 0, 0, 1);

#ifdef INSTANCED_RENDERING
	ssao_sum = SSAOTex.Sample(SSAOSampler, float3(input_uv_scaled, input.viewId)).xyz;
	center.normal = NormalTex.Sample(NormalSampler, float3(input.uv, input.viewId)).xyz;
#else
	ssao_sum = SSAOTex.Sample(SSAOSampler, input_uv_scaled).xyz;
	center.normal = NormalTex.Sample(NormalSampler, input.uv).xyz;
#endif
	blurweight = 1;
	ssao_sum_noweight = ssao_sum;

	[unroll]
	for (int i = 0; i < BLUR_SAMPLES; i++)
	{
		cur_offset = pixelSize * offsets[i];
		cur_offset_scaled = amplifyFactor * cur_offset;
#ifdef INSTANCED_RENDERING
		tap_ssao = SSAOTex.Sample(SSAOSampler, float3(input_uv_scaled + cur_offset_scaled, input.viewId)).xyz;
		tap.pos = DepthTex.Sample(DepthSampler, float3(input.uv + cur_offset, input.viewId)).xyz;
		tap.normal = NormalTex.Sample(NormalSampler, float3(input.uv + cur_offset, input.viewId)).xyz;
#else
		tap_ssao = SSAOTex.Sample(SSAOSampler, input_uv_scaled + cur_offset_scaled).xyz;
		tap.pos = DepthTex.Sample(DepthSampler, input.uv + cur_offset).xyz;
		tap.normal = NormalTex.Sample(NormalSampler, input.uv + cur_offset).xyz;
#endif
		tap_weight = compute_spatial_tap_weight(center, tap);

		blurweight += tap_weight;
		ssao_sum += tap_ssao * tap_weight;
		ssao_sum_noweight += tap_ssao;
	}

	ssao_sum /= blurweight;
	ssao_sum_noweight /= BLUR_SAMPLES;

	output.ssao = float4(lerp(ssao_sum, ssao_sum_noweight, blurweight < 2), 1);
	return output;
}
