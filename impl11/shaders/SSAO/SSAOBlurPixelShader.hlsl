/*
 * Simple blur for SSAO
 * Copyright 2019, Leo Reyes
 * Licensed under the MIT license. See LICENSE.txt
 */

// The SSAO Buffer
Texture2D SSAOTex : register(t0);
SamplerState SSAOsampler : register(s0);

// The Depth Buffer
Texture2D DepthTex : register(t1);
SamplerState DepthSampler : register(s1);

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

#define Z_THRESHOLD 20
#define BLUR_SIZE 1

float4 main(PixelShaderInput input) : SV_TARGET
{
	float2 input_uv_scaled = input.uv * amplifyFactor;
	float3 ssao = float3(0, 0, 0);
	//float3 ssao_sample;
	//float depth = DepthTex.Sample(DepthSampler, input.uv).z;
	//float depth_sample;
	float2 delta = uvStepSize * float2(pixelSizeX, pixelSizeY);
	float2 uv_outer_scaled = input_uv_scaled - BLUR_SIZE * delta;
	//float2 uv_outer        = input.uv - BLUR_SIZE * delta;
	float2 uv_inner_scaled;
	//float2 uv_inner;
	//float zdiff;
	int counter = 0;

	[unroll]
	for (int i = -BLUR_SIZE; i <= BLUR_SIZE; i++) {
		uv_inner_scaled = uv_outer_scaled;
		//uv_inner = uv_outer;
		[unroll]
		for (int j = -BLUR_SIZE; j <= BLUR_SIZE; j++) {
			ssao += SSAOTex.Sample(SSAOsampler, uv_inner_scaled).xyz;
			/*
			depth_sample = DepthTex.Sample(DepthSampler, uv_inner).z;
			zdiff = abs(depth_sample - depth);
			if (zdiff < Z_THRESHOLD) {
				ssao += ssao_sample;
				counter++;
			}*/
			uv_inner_scaled.x += delta.x;
			//uv_inner += delta.x;
			counter++;
		}
		uv_outer_scaled.y += delta.y;
		//uv_outer += delta.y;
	}
	return float4(ssao / (float)counter, 1);
}