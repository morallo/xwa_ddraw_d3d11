/*
 * Simple blur for SSAO
 * Copyright 2019, Leo Reyes
 * Licensed under the MIT license. See LICENSE.txt
 */

// The SSAO Buffer
Texture2D SSAOTex : register(t0);
SamplerState SSAOSampler : register(s0);

// The Depth Buffer
Texture2D DepthTex : register(t1);
SamplerState DepthSampler : register(s1);

// The Bent Normals
Texture2D BentTex : register(t2);
SamplerState BentSampler : register(s2);

// The Normal Buffer
Texture2D NormalTex : register(t3);
SamplerState NormalSampler : register(s3);

struct BlurData {
	float3 pos;
	float3 normal;
};

// I'm reusing the constant buffer from the bloom blur shader; but
// we're only using the amplifyFactor here.
cbuffer ConstantBuffer : register(b2)
{
	float pixelSizeX, pixelSizeY, white_point, amplifyFactor;
	// 16 bytes
	float bloomStrength, uvStepSize, saturationStrength;
	uint enableSSAO;
	// 32 bytes
	uint enableBentNormals;
	float norm_weight, depth_weight, unused3;
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 ssao : SV_TARGET0;
	float4 bent : SV_TARGET1;
};

#define Z_THRESHOLD 20
#define BLUR_SIZE 1

/*
PixelShaderOutput main_old(PixelShaderInput input)
{
	float2 input_uv_scaled = input.uv * amplifyFactor;
	PixelShaderOutput output;
	output.ssao = float4(0, 0, 0, 1);
	output.bent = float4(0, 0, 0, 1);
	//float3 ssao_sample;
	//float depth = DepthTex.Sample(DepthSampler, input.uv).z;
	//float depth_sample;
	float2 delta = amplifyFactor * uvStepSize * float2(pixelSizeX, pixelSizeY);
	//float2 delta_scaled = amplifyFactor * delta;	
	float2 delta_scaled = delta;
	float2 uv_outer_scaled = input_uv_scaled; // -BLUR_SIZE * delta;
	float2 uv_outer		   = input.uv; // -BLUR_SIZE * delta;
	float2 uv_inner_scaled;
	float2 uv_inner;
	//float zdiff;
	//int counter = 0;
	float center_depth = DepthTex.Sample(DepthSampler, input.uv).z;
	float sample_depth = 0;

	[unroll]
	for (int i = 0; i < 4; i++) {
		uv_inner_scaled = uv_outer_scaled;
		uv_inner = uv_outer;
		[unroll]
		for (int j = 0; j < 4; j++) {
			//sample_depth = DepthTex.Sample(DepthSampler, uv_inner).z;
			//if (abs(sample_depth - center_depth) < 3)
			{
				output.ssao.xyz += SSAOTex.Sample(SSAOSampler, uv_inner_scaled).xyz;
				output.bent.xyz += BentTex.Sample(BentSampler, uv_inner_scaled).xyz;
			}
			
			//depth_sample = DepthTex.Sample(DepthSampler, uv_inner).z;
			//zdiff = abs(depth_sample - depth);
			//if (zdiff < Z_THRESHOLD) {
			//	ssao += ssao_sample;
			//	counter++;
			//}
			uv_inner_scaled.x += delta_scaled.x;
			uv_inner += delta.x;
			//counter++;
		}
		uv_outer_scaled.y += delta_scaled.y;
		uv_outer += delta.y;
	}
	output.ssao.xyz /= 16.0;
	output.bent.xyz /= 16.0;
	return output;
}
*/

float compute_spatial_tap_weight(in BlurData center, in BlurData tap)
{
	if (tap.pos.z > 30000.0) return 0;
	const float depth_term  = saturate(depth_weight - abs(tap.pos.z - center.pos.z));
	return depth_term;
	//float normal_term = saturate(dot(tap.normal.xyz, center.normal.xyz) * 16 - 15);
	//return depth_term * normal_term;
	
	//float depth_term = saturate(1 - abs(tap.pos.z - center.pos.z));
	//float normal_term = pow(saturate(dot(tap.normal.xyz, center.normal.xyz)), norm_weight);
	//return normal_term;
	//return 1;
	
	//depth_term = lerp(1, 0, abs(tap.pos.z - center.pos.z) / depth_weight);
	//float depth_term = 1.0 / pow(saturate(1 - abs(tap.pos.z - center.pos.z)), depth_weight);
	//return depth_term;
}

PixelShaderOutput main(PixelShaderInput input) {
	static const float2 offsets[8] =
	{
		float2(1.5,0.5), float2(-1.5,-0.5), float2(-0.5,1.5), float2(0.5,-1.5),
		float2(1.5,2.5), float2(-1.5,-2.5), float2(-2.5,1.5), float2(2.5,-1.5)
	};
	/*static const float2 offsets[4] =
	{
		float2(1.0, 0.0),float2(-1.0, 0.0),float2(0.0, 1.0),float2(0.0, -1.0),
	};*/
	float2 cur_offset;
	float2 pixelSize = float2(pixelSizeX, pixelSizeY);
	float blurweight = 0, tap_weight;
	float2 input_uv_scaled = input.uv * amplifyFactor;
	BlurData center, tap;
	float3 tap_ssao, ssao_sum, ssao_sum_noweight;
	float3 tap_bent, bent_sum, bent_sum_noweight;
	PixelShaderOutput output;
	output.ssao = float4(0, 0, 0, 1);
	output.bent = float4(0, 0, 0, 1);
	
	ssao_sum      = SSAOTex.Sample(SSAOSampler, input_uv_scaled).xyz;
	bent_sum      = BentTex.Sample(BentSampler, input_uv_scaled).xyz;
	center.pos    = DepthTex.Sample(DepthSampler, input.uv).xyz;
	center.normal = NormalTex.Sample(NormalSampler, input.uv).xyz;
	blurweight    = 1;
	ssao_sum_noweight = ssao_sum;
	bent_sum_noweight = bent_sum;
	
	[unroll]
	for (int i = 0; i < 8; i++)
	//for (int i = 0; i < 4; i++)
	//int i = 0;
	{
		cur_offset = amplifyFactor * pixelSize * offsets[i];
		//cur_offset = pixelSize * offsets[i];
		tap_ssao   = SSAOTex.Sample(SSAOSampler, input_uv_scaled + cur_offset).xyz;
		tap_bent   = BentTex.Sample(BentSampler, input_uv_scaled + cur_offset).xyz;
		tap.pos    = DepthTex.Sample(DepthSampler, input.uv + cur_offset).xyz;
		tap.normal = NormalTex.Sample(NormalSampler, input.uv + cur_offset).xyz;
		
		tap_weight = compute_spatial_tap_weight(center, tap);
		//output.ssao = float4(tap_weight, tap_weight, tap_weight, 1);
		//return output;

		blurweight        += tap_weight;

		ssao_sum          += tap_ssao * tap_weight;
		ssao_sum_noweight += tap_ssao;
		bent_sum          += tap_bent * tap_weight;
		bent_sum_noweight += tap_bent;
	}
	
	ssao_sum /= blurweight;
	bent_sum /= blurweight;
	ssao_sum_noweight /= 8.0;
	bent_sum_noweight /= 8.0;
	
	output.ssao = float4(lerp(ssao_sum, ssao_sum_noweight, blurweight < 2), 1);
	output.bent = float4(lerp(bent_sum, bent_sum_noweight, blurweight < 2), 1);
	//output.ssao = float4(ssao_sum_noweight, 1);
	//output.bent = float4(bent_sum_noweight, 1);
	
	//output.ssao = float4(ssao_sum, 1);
	//blurweight /= 4;
	//output.ssao = float4(blurweight, blurweight, blurweight, 1);
	return output;
}