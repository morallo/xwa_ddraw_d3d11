// Based on Pascal Gilcher's MXAO implementation and on
// the following:
// https://www.gamedev.net/articles/programming/graphics/a-simple-and-practical-approach-to-ssao-r2753/
// Adapted for XWA by Leo Reyes.
// Licensed under the MIT license. See LICENSE.txt
#include "..\shader_common.h"
#include "..\shading_system.h"
#include "..\SSAOPSConstantBuffer.h"

// The Foreground 3D position buffer (linear X,Y,Z)
Texture2D    texPos   : register(t0);
SamplerState sampPos  : register(s0);

// The normal buffer
Texture2D    texNorm   : register(t1);
SamplerState sampNorm  : register(s1);

// The original color buffer 
Texture2D    texColor  : register(t2);
SamplerState sampColor : register(s2);

// The SSDO buffer with direct lighting from the previous pass
//Texture2D    texSSDO  : register(t3);
//SamplerState sampSSDO : register(s3);

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct PixelShaderOutput
{
	// The output of this stage should probably be another temporary RGB buffer -- not the SSAO buffer
	// ...maybe the ssaoBufR from SteamVR? (and viceversa when the right eye is being shaded?)
	float4 ssao        : SV_TARGET0;
};

inline float3 getPosition(in float2 uv, in float level) {
	// The use of SampleLevel fixes the following error:
	// warning X3595: gradient instruction used in a loop with varying iteration
	// This happens because the texture is sampled within an if statement (if FGFlag then...)
	return texPos.SampleLevel(sampPos, uv, level).xyz;
}

inline float3 getNormal(in float2 uv, in float level) {
	return texNorm.SampleLevel(sampNorm, uv, level).xyz;
}

/*
inline float3 doSSDOIndirect_ok(in float2 sample_uv, in float3 P, in float3 Normal,
	in float cur_radius_sqr, in float max_radius_sqr)
{
	if (any(sample_uv.xy < p0) ||
		any(sample_uv.xy > p1))
		return 0;

	float3 occluder = getPosition(sample_uv, 0);
	if (occluder.z >= INFINITY_Z1) return 0;

	//float miplevel = cur_radius / max_radius * 4;
	float3 L  			   = LightVector[0].xyz;
	float2 sample_uv_sub   = sample_uv * ssao_amplifyFactor;
	float3 occluder_Normal = normalize(getNormal(sample_uv, 0)); // I can probably read this normal off of the bent buffer later (?)
	float3 occluder_color  = saturate(texColor.SampleLevel(sampColor, sample_uv, 0).xyz); // I think this is supposed to be occ_diffuse * occ_color
	float3 occluder_ssdo   = texSSDO.SampleLevel(sampSSDO, sample_uv_sub, 0).xxx; // xyz
	// diff: Vector from current pos (P) to the sampled neighbor
	const float3 diff      = occluder - P;
	const float diff_sqr   = dot(diff, diff);
	// v: Normalized (occluder - P) vector
	const float3 v = diff * rsqrt(diff_sqr);
	const float ao_dot     = max(0.0, dot(Normal, v) - bias); // Adding the bias here helps remove a lot of self-occlusion
	//const float weight = saturate(1 - diff_sqr / max_dist_sqr);
	//const float radius_weight = (1 - cur_radius_sqr / (max_radius_sqr)); // ORIGINAL
	// TODO: Make ambient a configurable parameter
	//const float ambient = 0.15;
	//occluder_color = (ambient + occluder_ssdo) * occluder_color; // ORIGINAL
	float occ_diffuse = max(dot(L, occluder_Normal), 0.0);
	//occluder_color *= occluder_color; // gamma correction
	occluder_color = occ_diffuse * occluder_ssdo * occluder_color;

	// if diff.z > 0.0 --> occ_weight = 0: NO occlusion
	// if diff.z < 0.0 --> occ_weight = 1: There's occlusion
	//const float occ_weight = diff.z < 0.0 ? 1.0 : 0.0;
	const float occ_weight = diff.z < 0.0 ? ao_dot : 0.0;
	//const float occ_weight = ao_mask;

	if (ssao_debug == 25)
		return occ_weight;

	// Reference implementation:
	// indirectLight += (sampleDepth >= sample.z ? 1.0 : 0.0) * max(dot(sampleNormal, normalize(fragPos - samplePos)), 0.0) * sampleColor;

	// According to the reference SSDO implementation, we should be doing something like this:
	//return occ_weight * radius_weight * occluder_color * saturate(dot(occluder_Normal, -v));
	return occ_weight * max(dot(occluder_Normal, -v), 0.0) * occluder_color;
}
*/

inline float3 doSSDOIndirect(in float2 sample_uv, in float3 P, in float3 Normal,
	in float cur_radius_sqr, in float max_radius_sqr)
{
	if (any(sample_uv.xy < p0) || any(sample_uv.xy > p1))
		return 0;

	//float miplevel = cur_radius_sqr / max_radius_sqr * 4;
	const float miplevel = 0;
	float3 occluder = getPosition(sample_uv, miplevel);
	if (occluder.z >= INFINITY_Z1) return 0;

	
	float3 L			   = LightVector[0].xyz;
	float2 sample_uv_sub   = sample_uv * ssao_amplifyFactor;
	float3 occluder_Normal = getNormal(sample_uv, miplevel); // I can probably read this normal off of the bent buffer later (?)
	float3 occluder_color  = texColor.SampleLevel(sampColor, sample_uv, miplevel).xyz; // I think this is supposed to be occ_diffuse * occ_color
	//float3 occluder_ssdo   = texSSDO.SampleLevel(sampSSDO, sample_uv_sub, 0).xxx; // xyz
	// diff: Vector from current pos (P) to the sampled neighbor
	const float3 diff      = occluder - P;
	float diff_sqr         = dot(diff, diff);
	//  v: Normalized (occluder - P) vector: from current pos to sample, this is the "transmission" vector
	//  v: norm vector from current pos to sample
	// -v: norm vector from sample to current pos
	const float3 v         = diff * rsqrt(diff_sqr);
	const float dot_N_v    = dot(Normal, v);
	const float ao_dot     = max(0.0, dot_N_v - bias); // Adding the bias here helps remove a lot of self-occlusion
	diff_sqr = max(diff_sqr, 1.0); // Clamp diff_sqr to 1.0 to avoid singularities.

	// We are supposed to compute the angle between the sample's normal and the transmission vector
	// and the current point's normal and the transmission vector:
	// To avoid getting light from occluders that face away from P, we need to check
	// if the occluder is pointing towards P or away from it. v goes from P to the occluder, so we
	// use -v to point from the occluder towards P and so we use dot(occluder_Normal, -v):
	const float cosSi = max(dot(occluder_Normal, -v), 0.0);
	const float cosRi = max(dot_N_v, 0.0); // This guy probably doesn't help much? --> It helps if occ_weight is 1
	
	
	// We're supposed to read the color from the previous SSDO pass; but we're not
	// actually storing the color in that pass, just the obscurance. So, let's
	// do a shorthand calculation here:
	const float occ_diffuse = max(dot(occluder_Normal, L), 0.0);
	// In reality, the specular reflection coming from the occluder would contribute along the transmission
	// direction, so we should use -v as the "eye_vec"; but in practice it looks better if we use the regular
	// eye_vec
	//const float3 eye_vec  = -v;
	// We need to invert the z axis of the occluder to make it consistent with the illumination model. In our 
	// position/depth buffer, Z+ points away from the camera; but our vectors have Z+ pointing towards the camera 
	// to make them consistent with normal mapping, so, while computing spec shading, we need to invert the Z axis 
	// of the points to compute the eye_vector:
	const float3 eye_vec    = normalize(-float3(occluder.xy, -occluder.z));
	const float3 refl_vec   = normalize(reflect(-L, occluder_Normal));
	//float occ_spec  = max(dot(-v, refl_vec), 0.0); // This seems to be the most physically-correct version and looks OK
	float occ_spec = max(dot(eye_vec, refl_vec), 0.0);
	occ_spec = 5.0 * pow(occ_spec, 128.0);
	occluder_color *= occluder_color; // Gamma correction
	occluder_color  = occluder_color * (occ_diffuse + occ_spec);
	//occluder_color = saturate(occluder_color * occ_diffuse * occluder_ssdo.x);

	// if diff.z > 0.0 --> occ_weight = 0: NO occlusion
	// if diff.z < 0.0 --> occ_weight = 1: There's occlusion
	//const float occ_weight = diff.z < 0.0 ? 1.0 : 0.0;
	//const float occ_weight = 1.0; // This works fine
	//const float occ_weight   = diff.z > 0.0 ? 1.0 : 0.0; // This is the visibility function used in SSDO Direct
	//const float occ_weight = diff.z < 0.0 ? 1.0 /* ao_dot */ : 0.0; // occ_weight is our visibility function
	//const float occ_weight = ao_mask;
	//const float attenuation = cur_radius_sqr / max_radius_sqr; // Not sure this actually helps

	if (ssao_debug == 25)
		return occ_spec;
		//return occ_diffuse;

	// Reference implementation:
	// indirectLight += (sampleDepth >= sample.z ? 1.0 : 0.0) * max(dot(sampleNormal, normalize(fragPos - samplePos)), 0.0) * sampleColor;

	// According to the reference SSDO implementation, we should be doing something like this:
	//return occ_weight * radius_weight * occluder_color * saturate(dot(occluder_Normal, -v));
	return /* occ_weight * */ cosSi * cosRi * occluder_color / diff_sqr; // As == indirect_intensity and we multiply *once* outside this loop
	//return occ_weight * cosSi * cosRi * occluder_color * (cur_radius_sqr / max_radius_sqr); // As == indirect_intensity and we multiply *once* outside this loop
	//return clamp(occ_weight * cosSi * cosRi * occluder_color / diff_sqr, 0.0, 1.0); // As == indirect_intensity and we multiply *once* outside this loop
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.ssao = 0;

	const float3 p       = getPosition(input.uv, 0);
	// Early exit: do not compute SSAO for objects at infinity
	if (p.z > INFINITY_Z1) return output;
	const float3 Normal  = normalize(getNormal(input.uv, 0));
	//float3 ssdoDir = texSSDO.SampleLevel(sampSSDO, input.uv, 0).xyz;
	float3 ssdoInd = 0;
	float radius;
	output.ssao = float4(0, 0, 0, 1);

	// This apparently helps prevent z-fighting noise
	////p += 0.01 * n;
	//float m_offset = max(moire_offset, moire_offset * (p.z * moire_scale)); // ORIGINAL: 0.1 instead of moire_scale
	////p += m_offset * n;
	//p.z -= m_offset; // ORIGINAL

	const float3 SSAO_Normal = float3(Normal.xy, -Normal.z);
	float m_offset = moire_offset * (p.z * moire_scale);
	//p += m_offset * SSAO_Normal;

	// Interpolate between near_sample_radius at z == 0 and far_sample_radius at 1km+
	// We need to use saturate() here or we actually get negative numbers!
	radius = lerp(near_sample_radius, far_sample_radius, saturate(p.z / 1000.0));
	//float nm_intensity = (1 - ssao_mask) * lerp(nm_intensity_near, nm_intensity_far, saturate(p.z / 4000.0));

	float sample_jitter = dot(floor(input.pos.xy % 4 + 0.1), float2(0.0625, 0.25)) + 0.0625;
	float2 sample_uv, sample_direction;
	const float2x2 rotMatrix = float2x2(0.76465, -0.64444, 0.64444, 0.76465); //cos/sin 2.3999632 * 16 
	sincos(2.3999632 * 16 * sample_jitter, sample_direction.x, sample_direction.y); // 2.3999632 * 16
	sample_direction *= radius;
	const float max_radius = radius * (float)(samples + sample_jitter);
	const float max_radius_sqr = max_radius * max_radius;
	float cur_radius;

	// SSDO Indirect Calculation
	ssdoInd = 0;
	[loop]
	for (uint j = 0; j < samples; j++)
	{
		cur_radius = radius * (j + sample_jitter);
		sample_uv = input.uv + sample_direction.xy * (j + sample_jitter);
		sample_direction.xy = mul(sample_direction.xy, rotMatrix);
		ssdoInd += doSSDOIndirect(sample_uv, p, Normal, cur_radius * cur_radius, max_radius_sqr);
	}

	/*
	if (ssao_debug == 23) {
		output.ssao.xyz = 1.0 - ssdoDir.y;
		return output;
	}
	*/
	if (ssao_debug == 25) { // Display occ_weight
		//output.ssao.xyz = ssdoInd / (float)samples;
		//output.ssao.xz = 0;
		output.ssao = float4(ssdoInd / (float)samples, 1.0);
		return output;
	}

	ssdoInd = indirect_intensity * ssdoInd / (float)samples;
	// Start fading the effect at INFINITY_Z0 and fade out completely at INFINITY_Z1
	output.ssao = float4(lerp(ssdoInd, 0, saturate((p.z - INFINITY_Z0) / INFINITY_FADEOUT_RANGE)), 1.0);
	//output.ssao = float4(ssdoInd, 1.0);
	return output;
}

/*
// DSSDO
PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	//output.ssao = float4(1, 1, 1, 1);
	output.ssao = debug ? float4(0, 0, 0, 1) : 1;
	output.bentNormal = float4(0, 0, 0, 1);

	float3 P1 = getPosition(input.uv);
	float3 n = getNormal(input.uv);
	float3 bentNormal = 0;
	//float3 bentNormal = bentNormalInit * n; // Initialize the bentNormal with the normal
	float3 ao = 0;
	float3 p;
	float radius = sample_radius;
	bool FGFlag;
	float attenuation_angle_threshold = 0.1;
	float4 occlusion_sh2 = 0;
	const float fudge_factor_l0 = 2.0;
	const float fudge_factor_l1 = 10.0;
	const float  sh2_weight_l0 = fudge_factor_l0 * 0.28209; // 0.5 * sqrt(1.0/pi);
	const float3 sh2_weight_l1 = fudge_factor_l1 * 0.48860; // 0.5 * sqrt(3.0/pi);
	const float4 sh2_weight    = float4(sh2_weight_l1, sh2_weight_l0) / samples;

	if (P1.z < P2.z) {
		p = P1;
		FGFlag = true;
	}
	else {
		p = P2;
		FGFlag = false;
	}

	// Early exit: do not compute SSAO for objects at infinity
	if (p.z > INFINITY_Z) return output;

	float sample_jitter = dot(floor(input.pos.xy % 4 + 0.1), float2(0.0625, 0.25)) + 0.0625;
	float2 sample_uv, sample_direction;
	const float2x2 rotMatrix = float2x2(0.76465, -0.64444, 0.64444, 0.76465); //cos/sin 2.3999632 * 16
	sincos(2.3999632 * 16 * sample_jitter, sample_direction.x, sample_direction.y); // 2.3999632 * 16
	sample_direction *= radius;
	//float max_radius = radius * (float)(samples - 1 + sample_jitter);
	float max_distance_inv = 1.0 / max_dist;

	// DSSDO Calculation
	//bentNormal = n;
	[loop]
	for (uint j = 0; j < samples; j++)
	{
		sample_uv = input.uv + sample_direction.xy * (j + sample_jitter);
		sample_direction.xy = mul(sample_direction.xy, rotMatrix);
		//ao += doAmbientOcclusion(FGFlag, input.uv, sample_uv, max_radius, p, n, bentNormal);
		float3 sample_pos = getPosition(sample_uv);
		float3 center_to_sample = sample_pos - p;
		float  dist = length(center_to_sample);
		float3 center_to_sample_normalized = center_to_sample / dist;
		float  attenuation = 1 - saturate(dist * max_distance_inv);
		float  dp = dot(n, center_to_sample_normalized);

		attenuation    = attenuation * attenuation * step(attenuation_angle_threshold, dp);
		occlusion_sh2 += attenuation * sh2_weight * float4(center_to_sample_normalized, 1);
		ao += attenuation;
		bentNormal    += (1 - attenuation) * center_to_sample_normalized;
	}

	ao = 1 - ao / (float)samples;
	output.ssao.xyz *= lerp(black_level, ao, ao);
	//bentNormal = occlusion_sh2.xyz;
	//bentNormal.xy += occlusion_sh2.xy;
	//bentNormal.z  -= occlusion_sh2.z;
	//bentNormal.xyz = lerp(bentNormal.xyz, n, ao);
	bentNormal = -bentNormal;
	bentNormal.z = -bentNormal.z;
	output.bentNormal.xyz = normalize(bentNormalInit * n - bentNormal);

	if (debug)
		output.bentNormal.xyz = output.bentNormal.xyz * 0.5 + 0.5;

	if (debug == 1) {
		output.ssao.xyz = output.bentNormal.xxx;
	}
	else if (debug == 2) {
		output.ssao.xyz = output.bentNormal.yyy;
	}
	else if (debug == 3) {
		output.ssao.xyz = output.bentNormal.zzz;
	}

	return output;
}
*/