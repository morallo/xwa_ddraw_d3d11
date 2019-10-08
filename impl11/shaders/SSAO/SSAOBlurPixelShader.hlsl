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

// The Bent Normals
Texture2D BentTex : register(t2);
SamplerState BentSampler : register(s2);

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

struct PixelShaderOutput
{
	float4 ssao : SV_TARGET0;
	float4 bent : SV_TARGET1;
};

#define Z_THRESHOLD 20
#define BLUR_SIZE 1

PixelShaderOutput main(PixelShaderInput input)
{
	float2 input_uv_scaled = input.uv * amplifyFactor;
	PixelShaderOutput output;
	output.ssao = float4(0, 0, 0, 1);
	output.bent = float4(0, 0, 0, 1);
	//float3 ssao_sample;
	//float depth = DepthTex.Sample(DepthSampler, input.uv).z;
	//float depth_sample;
	float2 delta = uvStepSize * float2(pixelSizeX, pixelSizeY);
	float2 uv_outer_scaled = input_uv_scaled; // -BLUR_SIZE * delta;
	//float2 uv_outer        = input.uv - BLUR_SIZE * delta;
	float2 uv_inner_scaled;
	//float2 uv_inner;
	//float zdiff;
	//int counter = 0;

	[unroll]
	for (int i = 0; i < 4; i++) {
		uv_inner_scaled = uv_outer_scaled;
		//uv_inner = uv_outer;
		[unroll]
		for (int j = 0; j < 4; j++) {
			output.ssao.xyz += SSAOTex.Sample(SSAOsampler, uv_inner_scaled).xyz;
			//output.bent.xyz += BentTex.Sample(BentSampler, uv_inner_scaled).xyz;
			/*
			depth_sample = DepthTex.Sample(DepthSampler, uv_inner).z;
			zdiff = abs(depth_sample - depth);
			if (zdiff < Z_THRESHOLD) {
				ssao += ssao_sample;
				counter++;
			}*/
			uv_inner_scaled.x += delta.x;
			//uv_inner += delta.x;
			//counter++;
		}
		uv_outer_scaled.y += delta.y;
		//uv_outer += delta.y;
	}
	output.ssao.xyz /= 16.0;
	//output.bent.xyz /= 16.0;
	return output;
}