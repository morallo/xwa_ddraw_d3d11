// Based on Pascal Gilcher's MXAO implementation and on
// the following:
// https://www.gamedev.net/articles/programming/graphics/a-simple-and-practical-approach-to-ssao-r2753/
// Adapted for XWA by Leo Reyes.
// Licensed under the MIT license. See LICENSE.txt

// The random normals buffer
Texture2D    texRand  : register(t0);
SamplerState sampRand : register(s0);

// The Foreground 3D position buffer (linear X,Y,Z)
Texture2D    texPos   : register(t1);
SamplerState sampPos  : register(s1);

// The Background 3D position buffer (linear X,Y,Z)
Texture2D    texPos2  : register(t2);
SamplerState sampPos2 : register(s2);

// The normal buffer
Texture2D    texNorm   : register(t3);
SamplerState sampNorm  : register(s3);

// The color buffer
Texture2D    texColor  : register(t4);
SamplerState sampColor : register(s4);

#define INFINITY_Z 10000

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 ssao        : SV_TARGET0;
	//float4 bentNormal  : SV_TARGET1; // Bent normal map output
};

cbuffer ConstantBuffer : register(b3)
{
	float screenSizeX, screenSizeY, scale, bias;
	// 16 bytes
	float intensity, sample_radius, black_level;
	uint iterations;
	// 32 bytes
	uint z_division;
	float area, falloff, power;
	// 48 bytes
};

//interface IPosition {
//	float3 getPosition(in float2 uv);
//};

//class ForegroundPos : IPosition {
float3 getPositionFG(in float2 uv) {
	return texPos.Sample(sampPos, uv).xyz;
}
//};

//class BackgroundPos : IPosition {
float3 getPositionBG(in float2 uv) {
	return texPos2.Sample(sampPos2, uv).xyz;
}
//};

inline float3 getNormal(in float2 uv) {
	return texNorm.Sample(sampNorm, uv).xyz;
}

inline float2 getRandom(in float2 uv) {
	// The float2(64, 64) in the following expression comes from the size of the
	// random texture
	return normalize(texRand.Sample(sampRand, 
		float2(screenSizeX, screenSizeY) * uv / float2(64, 64)).xy * 2.0f - 1.0f);
}

//inline float3 getRandom(in float2 uv) {
//	return normalize(texRand.Sample(sampRand,
//		float3(screenSizeX, screenSizeY) * uv / float2(64, 64)).xyz * 2.0f - 1.0f);
//}

/*
These settings yield a nice effect:

sample_radius = 0.1
intensity = 2.0
scale = 0.005
*/
inline float3 doAmbientOcclusion(bool FG, in float2 uv, in float2 uv_offset, in float3 P, in float3 Normal /*, inout float3 BentNormal */)
{
	//float3 color   = texColor.Sample(sampColor, tcoord + uv).xyz;
	//float3 occluderNormal = getNormal(uv + uv_offset).xyz;
	float3 occluder = FG ? getPositionFG(uv + uv_offset) : getPositionBG(uv + uv_offset);
	// diff: Vector from current pos (p) to sampled neighbor
	float3 diff     = occluder - P;
	//float zdist     = -diff.z;
	// L: Distance from current pos (P) to the occluder
	const float  L = length(diff);
	// v: Normalized (occluder - P) vector
	const float3 v = diff / L;
	const float  d = L * scale;
	//if (zdist < 0) BentNormal += v;
	//BentNormal += v;
	return intensity * pow(max(0.0, dot(Normal, v) - bias), power) / (1.0 + d);
	//return step(falloff, zdist) * (1.0 - smoothstep(falloff, area, zdist));
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.ssao = float4(1, 1, 1, 1);
	//float3 dummy = float3(0, 0, 0);
	
	//const float2 vec[4] = { float2(1, 0), float2(-1, 0), float2(0, 1), float2(0, -1) };
	float3 P1 = getPositionFG(input.uv);
	float3 P2 = getPositionBG(input.uv);
	float3 n = getNormal(input.uv);
	//float3 bentNormal = n;
	//float2 rand = getRandom(input.uv);
	float3 ao = float3(0.0, 0.0, 0.0);
	float radius = sample_radius;
	float3 p;
	bool FGFlag;
	
	if (P1.z < P2.z) {
		p = P1;
		FGFlag = true;
	} else {
		p = P2;
		FGFlag = false;
	}
	
	// Early exit: do not compute SSAO for objects at infinity
	if (p.z > INFINITY_Z) return output;

	// Enable perspective-correct radius
	if (z_division) 	radius /= p.z;

	float sample_jitter = dot(floor(input.pos.xy % 4 + 0.1), float2(0.0625, 0.25)) + 0.0625;
	float2 sample_uv, sample_direction;
	sincos(2.3999632 * 16 * sample_jitter, sample_direction.x, sample_direction.y); //2.3999632 * 16
	sample_direction *= radius;

	// SSAO Calculation
	//int iterations = 4;
	[loop]
	for (uint j = 0; j < 4 * iterations; j++)
	{
		sample_uv = input.uv + sample_direction.xy * (j + sample_jitter);
		sample_direction.xy = mul(sample_direction.xy, float2x2(0.76465, -0.64444, 0.64444, 0.76465)); //cos/sin 2.3999632 * 16 
		ao += doAmbientOcclusion(FGFlag, sample_uv, float2(0, 0), p, n);

		/*
		float2 coord1 = reflect(vec[j], rand) * radius;
		float2 coord2 = float2(coord1.x*0.7071 - coord1.y*0.7071, 
							   coord1.x*0.7071 + coord1.y*0.7071);

		ao += doAmbientOcclusion(FGFlag, input.uv, coord1 * 0.25, p, n);
		ao += doAmbientOcclusion(FGFlag, input.uv, coord2 * 0.5,  p, n);
		ao += doAmbientOcclusion(FGFlag, input.uv, coord1 * 0.75, p, n);
		ao += doAmbientOcclusion(FGFlag, input.uv, coord2, p, n);
		*/
	}

	ao = 1 - ao / ((float)iterations * 4.0);
	output.ssao.xyz *= lerp(black_level, ao, ao);
	//output.bentNormal = float4(normalize(bentNormal), 1);
	return output;
}
