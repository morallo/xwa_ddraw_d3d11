// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes (c) 2019

// The color buf (for DEBUG purposes right now)
Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

// The 3D position buffer (linear X,Y,Z)
Texture2D    texPos  : register(t1);
SamplerState sampPos : register(s1);

// The normal buffer
Texture2D    texNorm  : register(t2);
SamplerState sampNorm : register(s2);

// The random normals buffer
Texture2D    texRand  : register(t3);
SamplerState sampRand : register(s3);

//static float METRIC_SCALE_FACTOR = 25.0;

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct PixelShaderOutput
{
	//float4 color : SV_TARGET0; // For debugging purposes, remove later
	//float4 ssao  : SV_TARGET1; // The SSAO output itself
	float4 color  : SV_TARGET0; // The SSAO output itself
};

cbuffer ConstantBuffer : register(b3)
{
	float screenSizeX, screenSizeY, scale, bias;
	// 16 bytes
	float intensity, sample_radius;
	uint iterations, unused2;
	// 32 bytes
};

inline float3 getPosition(in float2 uv) {
	return texPos.Sample(sampPos, uv).xyz;
}

inline float3 getNormal(in float2 uv) {
	return texNorm.Sample(sampNorm, uv).xyz;
}

inline float2 getRandom(in float2 uv) {
	return normalize(texRand.Sample(sampler0, 
		float2(screenSizeX, screenSizeY) * uv / float2(64, 64)).xy * 2.0f - 1.0f);
}

// From: https://www.gamedev.net/articles/programming/graphics/a-simple-and-practical-approach-to-ssao-r2753/
float doAmbientOcclusion(in float2 tcoord, in float2 uv, in float3 p, in float3 cnorm)
{
	float3 diff = getPosition(tcoord + uv) - p;
	const float3 v = normalize(diff);
	const float d = length(diff) * scale;
	return max(0.0, dot(cnorm, v) - bias) * (1.0 / (1.0 + d)) * intensity;
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.color = texture0.Sample(sampler0, input.uv);
	//output.ssao = float4(1, 1, 1, 1);

	const float2 vec[4] = { float2(1,0),float2(-1,0), float2(0,1),float2(0,-1) };
	float3 p = getPosition(input.uv);
	float3 n = getNormal(input.uv);
	float2 rand = getRandom(input.uv);
	float ao = 0.0f;
	float radius = sample_radius / p.z;
	//float radius = sample_radius;

	// SSAO Calculation
	//int iterations = 4;
	[unroll]
	for (int j = 0; j < iterations; ++j)
	{
		float2 coord1 = reflect(vec[j], rand) * radius;
		float2 coord2 = float2(coord1.x*0.707 - coord1.y*0.707, 
							   coord1.x*0.707 + coord1.y*0.707);

		ao += doAmbientOcclusion(input.uv, coord1 * 0.25, p, n);
		ao += doAmbientOcclusion(input.uv, coord2 * 0.5,  p, n);
		ao += doAmbientOcclusion(input.uv, coord1 * 0.75, p, n);
		ao += doAmbientOcclusion(input.uv, coord2, p, n);
	}

	ao /= (float)iterations * 4.0;
	//output.color.xyz *= (1 - ao);
	output.color = float4(float3(1, 1, 1) * (1 - ao), 1);
	return output;
}
