// Based on Pascal Gilcher's MXAO implementation and on
// the following:
// https://www.gamedev.net/articles/programming/graphics/a-simple-and-practical-approach-to-ssao-r2753/
// Adapted for XWA by Leo Reyes.
// Licensed under the MIT license. See LICENSE.txt
#include "..\shader_common.h"
#include "..\SSAOPSConstantBuffer.h"

#ifdef INSTANCED_RENDERING
// The 3D position buffer (linear X,Y,Z)
Texture2DArray    texPos : register(t0);
SamplerState sampPos : register(s0);

// The normal buffer
Texture2DArray    texNorm : register(t1);
SamplerState sampNorm : register(s1);

// The color buffer
Texture2DArray    texColor : register(t2);
SamplerState sampColor : register(s2);
#else
// The 3D position buffer (linear X,Y,Z)
Texture2D texPos : register(t0);
SamplerState sampPos : register(s0);

// The normal buffer
Texture2D texNorm : register(t1);
SamplerState sampNorm : register(s1);

// The color buffer
Texture2D texColor : register(t2);
SamplerState sampColor : register(s2);
#endif

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
#ifdef INSTANCED_RENDERING
	uint viewId : SV_RenderTargetArrayIndex;
#endif
};

struct PixelShaderOutput
{
	float4 ssao : SV_TARGET0;
};

struct BlurData {
	float3 pos;
	float3 normal;
};

#ifdef INSTANCED_RENDERING
inline float3 getPosition(in float3 uv, in float level)
#else
inline float3 getPosition(in float2 uv, in float level)
#endif
{
	// The use of SampleLevel fixes the following error:
	// warning X3595: gradient instruction used in a loop with varying iteration
	// This happens because the texture is sampled within an if statement (if FGFlag then...)
	return texPos.SampleLevel(sampPos, uv, level).xyz;
}

#ifdef INSTANCED_RENDERING
inline float3 getNormal(in float3 uv, in float level)
#else
inline float3 getNormal(in float2 uv, in float level)
#endif
{
	return texNorm.Sample(sampNorm, uv, level).xyz;
}

#ifdef INSTANCED_RENDERING
inline float3 doAmbientOcclusion(in float3 sample_uv, in float3 P, in float3 Normal, in float level)
#else
inline float3 doAmbientOcclusion(in float2 sample_uv, in float3 P, in float3 Normal, in float level)
#endif
{
	float3 occluder = getPosition(sample_uv, level);
	// diff: Vector from current pos (p) to sampled neighbor
	float3 diff = occluder - P;
	const float diff_sqr = dot(diff, diff);
	// v: Normalized (occluder - P) vector
	const float3 v = diff * rsqrt(diff_sqr);
	const float max_dist_sqr = max_dist * max_dist;
	const float weight = saturate(1 - diff_sqr / max_dist_sqr);

	float ao_dot = max(0.0, dot(Normal, v) - bias);
	float ao_factor = ao_dot * weight;
	//return intensity * pow(ao_factor, power);
	//return intensity * ao_factor;
	return ao_factor; // Let's multiply by intensity only once outside the addition loop
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.ssao = 1;
#ifdef INSTANCED_RENDERING
	float3 p = getPosition(float3(input.uv, input.viewId), 0);
	float3 n = getNormal(float3(input.uv, input.viewId), 0);
#else
	float3 p = getPosition(input.uv, 0);
	float3 n = getNormal(input.uv, 0);
#endif
	float3 FakeNormal = 0;
	float3 ao = 0;
	float radius = near_sample_radius;
	bool FGFlag;

	// I believe the hook_normals hook is inverting the Z axis of the normal, so we need
	// to flip it here again to get proper SSAO
	n.z = -n.z;

	// Early exit: do not compute SSAO for objects at infinity
	if (p.z > INFINITY_Z1) return output;
	radius = lerp(near_sample_radius, far_sample_radius, saturate(p.z / 1000.0));

	// This is probably OK; but we didn't have this in the previous release, so
	// should I activate this?
	float m_offset = moire_offset * (p.z * 0.1);
	p += m_offset * n;

	// Enable perspective-correct radius
	if (z_division) 	radius /= p.z;

	float sample_jitter = dot(floor(input.pos.xy % 4 + 0.1), float2(0.0625, 0.25)) + 0.0625;
#ifdef INSTANCED_RENDERING
	float3 sample_uv;
#else
	float2 sample_uv;
#endif
	float2 sample_direction;
	const float2x2 rotMatrix = float2x2(0.76465, -0.64444, 0.64444, 0.76465); //cos/sin 2.3999632 * 16 
	sincos(2.3999632 * 16 * sample_jitter, sample_direction.x, sample_direction.y); // 2.3999632 * 16
	sample_direction *= radius;
	//float cur_radius;
	//float max_radius = radius * (float)(samples - 1 + sample_jitter);
	float miplevel = 0;

	// SSAO Calculation
	[loop]
	for (uint j = 0; j < samples; j++)
	{
		//cur_radius = radius * (j + sample_jitter);
		//miplevel = cur_radius / max_radius * 4; // Is this miplevel better than using L?
#ifdef INSTANCED_RENDERING
		sample_uv = float3(input.uv + sample_direction.xy * (j + sample_jitter), input.viewId);
#else
		sample_uv = float2(input.uv + sample_direction.xy * (j + sample_jitter));
#endif
		sample_direction.xy = mul(sample_direction.xy, rotMatrix);
		ao += doAmbientOcclusion(sample_uv, p, n, miplevel);

	}
	//ao = 1 - ao / (float)samples;
	ao = 1 - intensity * ao / (float)samples;
	output.ssao.xyz *= lerp(black_level, ao, ao);
	return output;
}
