// Based on Pascal Gilcher's MXAO implementation and on
// the following:
// https://www.gamedev.net/articles/programming/graphics/a-simple-and-practical-approach-to-ssao-r2753/
// Adapted for XWA by Leo Reyes.
// Licensed under the MIT license. See LICENSE.txt

// The Foreground 3D position buffer (linear X,Y,Z)
Texture2D    texPos   : register(t0);
SamplerState sampPos  : register(s0);

// The Background 3D position buffer (linear X,Y,Z)
Texture2D    texPos2  : register(t1);
SamplerState sampPos2 : register(s1);

// The normal buffer
Texture2D    texNorm   : register(t2);
SamplerState sampNorm  : register(s2);

// The original color buffer 
Texture2D    texColor  : register(t3);
SamplerState sampColor : register(s3);

// The SSDO buffer with direct lighting from the previous pass
Texture2D    texSSDO  : register(t4);
SamplerState sampSSDO : register(s4);

#define INFINITY_Z0 15000
#define INFINITY_Z1 20000
#define INFINITY_FADEOUT_RANGE 5000

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
	//float4 bentNormal  : SV_TARGET1; // Bent normal map output
};

// SSAOPixelShaderCBuffer
cbuffer ConstantBuffer : register(b3)
{
	float screenSizeX, screenSizeY, indirect_intensity, bias;
	// 16 bytes
	float intensity, near_sample_radius, black_level;
	uint samples;
	// 32 bytes
	uint z_division;
	float bentNormalInit, max_dist, power;
	// 48 bytes
	uint debug;
	float moire_offset, amplifyFactor;
	uint fn_enable;
	// 64 bytes
	float fn_max_xymult, fn_scale, fn_sharpness, nm_intensity_near;
	// 80 bytes
	float far_sample_radius, nm_intensity_far, ambient, amplifyFactor2;
	// 96 bytes
	float x0, y0, x1, y1; // Viewport limits in uv space
	// 112 bytes
	float3 invLightColor;
	float unused4;
	// 128 bytes
};

cbuffer ConstantBuffer : register(b4)
{
	matrix projEyeMatrix;
	matrix viewMatrix;
	matrix fullViewMatrix;
	float4 LightVector;
};

struct BlurData {
	float3 pos;
	float3 normal;
};

//interface IPosition {
//	float3 getPosition(in float2 uv);
//};

//class ForegroundPos : IPosition {
float3 getPositionFG(in float2 uv, in float level) {
	// The use of SampleLevel fixes the following error:
	// warning X3595: gradient instruction used in a loop with varying iteration
	// This happens because the texture is sampled within an if statement (if FGFlag then...)
	return texPos.SampleLevel(sampPos, uv, level).xyz;
}
//};

//class BackgroundPos : IPosition {
float3 getPositionBG(in float2 uv, in float level) {
	return texPos2.SampleLevel(sampPos2, uv, level).xyz;
}
//};

inline float3 getNormal(in float2 uv, in float level) {
	return texNorm.SampleLevel(sampNorm, uv, level).xyz;
}

inline float3 doSSDOIndirect(bool FGFlag, in float2 sample_uv, in float3 P, in float3 Normal,
	in float cur_radius, in float max_radius)
{
	float miplevel = cur_radius / max_radius * 4;
	float2 sample_uv_sub = sample_uv * amplifyFactor;
	float3 occluder_Normal = getNormal(sample_uv, miplevel); // I can probably read this normal off of the bent buffer later (?)
	float3 occluder_color = texColor.SampleLevel(sampColor, sample_uv, miplevel).xyz;
	float3 occluder_ssdo  = texSSDO.SampleLevel(sampSSDO, sample_uv_sub, miplevel).xyz;
	float3 occluder = FGFlag ? getPositionFG(sample_uv, 0) : getPositionBG(sample_uv, miplevel);
	// diff: Vector from current pos (P) to the sampled neighbor
	const float3 diff = occluder - P;
	const float diff_sqr = dot(diff, diff);
	// v: Normalized (occluder - P) vector
	const float3 v = diff * rsqrt(diff_sqr);
	//const float max_dist_sqr = max_dist * max_dist;
	//const float weight = saturate(1 - diff_sqr / max_dist_sqr);
	const float weight = (1 - cur_radius * cur_radius / (max_radius * max_radius));
	// TODO: Make ambient a configurable parameter
	const float ambient = 0.15;
	occluder_color = (ambient + occluder_ssdo) * occluder_color;

	//float ao_dot = max(0.0, dot(Normal, v) - bias);
	//float ao_factor = ao_dot * weight;
	// This formula is wrong; but it worked pretty well:
	//float visibility = saturate(1 - step(bias, ao_factor));
	//float visibility = 1 - ao_dot;
	//float2 uv_diff = sample_uv - input_uv;
	//float ssdo_dist = min(1, diff_sqr);
	//float3 B = 0;
	if (diff.z < 0.0) { // If there's occlusion, then compute B and indirect lighting
		// The bent normal is probably not needed for indirect lighting. Only the actual
		// normal in the center point and the occluder's normal
		//B.x =  uv_diff.x;
		//B.y = -uv_diff.y;
		////B.z =  -(max_radius - cur_radius);
		//B.z = 0.01 * (max_radius - cur_radius) / max_radius;
		//B = normalize(B);
		//return 0; // Center is occluded
		//if (debug == 3) {
			//return (1 - visibility) * saturate(dot(B, -occluder_Normal)) * weight;
			//return occluder_color * saturate(dot(B, -occluder_Normal)) * weight;
			//return occluder_color * saturate(dot(occluder_Normal, -v)) * weight;

			// This returns something that looks like a nice Z-component normal:
			// (maybe I can use this to compute the bent normal's z component!)
			//return float3(0, 1, 0);
		//}
		// According to the reference SSDO implementation, we should be doing something like this:
		return occluder_color * saturate(dot(occluder_Normal, -v)) * weight;
		//return occluder_color * saturate(dot(B, -occluder_Normal)) * weight;
	}
	return 0; // Center is not occluded, no indirect lighting

	
	//return occluder_color * ssdo_area * saturate(dot(B, -occluder_Normal)) * weight;
	/*else {
		if (debug == 3)
			return 0;
		else
			return 0;
	}*/
	

	//return visibility;
	//return result + color * ao_factor * saturate(dot(Normal, light));
	//return color * saturate(dot(Normal, light));
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float3 P1 = getPositionFG(input.uv, 0);
	float3 P2 = getPositionBG(input.uv, 0);
	float3 n = getNormal(input.uv, 0);
	float3 ssdo;
	float3 p;
	float radius;
	bool FGFlag;
	output.ssao = float4(0, 0, 0, 1);

	if (P1.z < P2.z) {
		p = P1;
		FGFlag = true;
	}
	else {
		p = P2;
		FGFlag = false;
	}
	// This apparently helps prevent z-fighting noise
	//p += 0.01 * n;
	float m_offset = max(moire_offset, moire_offset * (p.z * 0.1));
	//p += m_offset * n;
	p.z -= m_offset;

	// Early exit: do not compute SSAO for objects at infinity
	if (p.z > INFINITY_Z1) return output;

	// Interpolate between near_sample_radius at z == 0 and far_sample_radius at 1km+
	// We need to use saturate() here or we actually get negative numbers!
	radius = lerp(near_sample_radius, far_sample_radius, saturate(p.z / 1000.0));
	//float nm_intensity = (1 - ssao_mask) * lerp(nm_intensity_near, nm_intensity_far, saturate(p.z / 4000.0));

	//float3 light = 1;
	//float3 light  = normalize(float3(1, 1, -0.5));
	//float3 light = normalize(float3(-1, 1, -0.5));
	float3 light = LightVector.xyz;
	//light = mul(viewMatrix, float4(light, 0)).xyz;

	float sample_jitter = dot(floor(input.pos.xy % 4 + 0.1), float2(0.0625, 0.25)) + 0.0625;
	float2 sample_uv, sample_direction;
	const float2x2 rotMatrix = float2x2(0.76465, -0.64444, 0.64444, 0.76465); //cos/sin 2.3999632 * 16 
	sincos(2.3999632 * 16 * sample_jitter, sample_direction.x, sample_direction.y); // 2.3999632 * 16
	sample_direction *= radius;
	float max_radius = radius * (float)(samples + sample_jitter);

	// SSDO Indirect Calculation
	ssdo = 0;
	[loop]
	for (uint j = 0; j < samples; j++)
	{
		sample_uv = input.uv + sample_direction.xy * (j + sample_jitter);
		sample_direction.xy = mul(sample_direction.xy, rotMatrix);
		ssdo += doSSDOIndirect(FGFlag, sample_uv, p, n, 
			radius * (j + sample_jitter), max_radius);
	}
	ssdo = indirect_intensity * ssdo / (float)samples;
	// Start fading the effect at INFINITY_Z0 and fade out completely at INFINITY_Z1
	output.ssao.xyz = lerp(ssdo, 0, saturate((p.z - INFINITY_Z0) / INFINITY_FADEOUT_RANGE));
	//output.ssao.xyz = ssdo;

	//ssdo = saturate(ssdo / (float)samples);
	//output.ssao.xyz *= lerp(black_level, ao, ao);
	//if (debug == 3)
	//output.ssao.xyz = ssdo;
	//else
	//	output.ssao.xyz = saturate(color + ssdo);
	//output.bentNormal.xyz = normalize(bentNormal);

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

	float3 P1 = getPositionFG(input.uv);
	float3 P2 = getPositionBG(input.uv);
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
		float3 sample_pos = FGFlag ? getPositionFG(sample_uv) : getPositionBG(sample_uv);
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