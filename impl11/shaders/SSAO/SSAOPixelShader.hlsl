// Based on from: https://www.gamedev.net/articles/programming/graphics/a-simple-and-practical-approach-to-ssao-r2753/
// Adapted to XWA by Leo Reyes.
// Licensed under the MIT license. See LICENSE.txt

// The random normals buffer
Texture2D    texRand  : register(t0);
SamplerState sampRand : register(s0);

// The 3D position buffer (linear X,Y,Z)
Texture2D    texPos  : register(t1);
SamplerState sampPos : register(s1);

// The normal buffer
Texture2D    texNorm  : register(t2);
SamplerState sampNorm : register(s2);

// The color buffer
Texture2D    texColor  : register(t3);
SamplerState sampColor : register(s3);

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 ssao        : SV_TARGET0; // The SSAO output itself
	float4 bentNormal  : SV_TARGET1; // Bent normal map output
};

cbuffer ConstantBuffer : register(b3)
{
	float screenSizeX, screenSizeY, scale, bias;
	// 16 bytes
	float intensity, sample_radius, black_level;
	uint iterations;
	// 32 bytes
	uint z_division, unused1, unused2, unused3;
	// 48 bytes
};

inline float3 getPosition(in float2 uv) {
	return texPos.Sample(sampPos, uv).xyz;
}

inline float3 getNormal(in float2 uv) {
	return texNorm.Sample(sampNorm, uv).xyz;
}

inline float2 getRandom(in float2 uv) {
	return normalize(texRand.Sample(sampRand, 
		float2(screenSizeX, screenSizeY) * uv / float2(64, 64)).xy * 2.0f - 1.0f);
}

/*
These settings yield a nice effect:

sample_radius = 0.1
intensity = 2.0
scale = 0.005
*/
inline float3 doAmbientOcclusion(in float2 uv, in float2 uv_offset, in float3 P, in float3 Normal)
{
	//float3 color   = texColor.Sample(sampColor, tcoord + uv).xyz;
	// diff: Vector from current pos (p) to sampled neighbor
	float3 occluder = getPosition(uv + uv_offset);
	float3 diff     = occluder - P;
	//float zdist     = P.z - occluder.z;
	// L: Distance from current pos (P) to the occluder
	const float  L = length(diff);
	// v: Normalized (occluder - P) vector
	const float3 v = diff / L;
	const float  d = L * scale;
	return intensity * max(0.0, dot(Normal, v) - bias) / (1.0 + d);
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.ssao = float4(1, 1, 1, 1);
	output.bentNormal = float4(0, 0, 0, 0);

	const float2 vec[4] = { float2(1, 0), float2(-1, 0), float2(0, 1), float2(0, -1) };
	float3 p = getPosition(input.uv);
	float3 n = getNormal(input.uv);
	float2 rand = getRandom(input.uv);
	float3 ao = float3(0.0, 0.0, 0.0);
	float radius = sample_radius;
	if (z_division)
		radius /= p.z;

	// SSAO Calculation
	//int iterations = 4;
	[unroll]
	for (uint j = 0; j < iterations; ++j)
	{
		float2 coord1 = reflect(vec[j], rand) * radius;
		float2 coord2 = float2(coord1.x*0.7071 - coord1.y*0.7071, 
							   coord1.x*0.7071 + coord1.y*0.7071);

		ao += doAmbientOcclusion(input.uv, coord1 * 0.25, p, n);
		ao += doAmbientOcclusion(input.uv, coord2 * 0.5,  p, n);
		ao += doAmbientOcclusion(input.uv, coord1 * 0.75, p, n);
		ao += doAmbientOcclusion(input.uv, coord2, p, n);
	}

	ao = 1 - ao / ((float)iterations * 4.0);
	output.ssao.xyz *= lerp(black_level, ao, ao);
	return output;
}
