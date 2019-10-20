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

// The color buffer (with the accumulated first pass SSDO)
Texture2D    texColor  : register(t3);
SamplerState sampColor : register(s3);

// The diffuse buffer
//Texture2D    texDiff  : register(t4);
//SamplerState sampDiff : register(s4);

#define INFINITY_Z 10000

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

cbuffer ConstantBuffer : register(b3)
{
	float screenSizeX, screenSizeY, indirect_intensity, bias;
	// 16 bytes
	float intensity, sample_radius, black_level;
	uint samples;
	// 32 bytes
	uint z_division;
	float bentNormalInit, max_dist, power;
	// 48 bytes
	uint debug;
	float moire_offset, amplifyFactor, unused3;
	// 64 bytes
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
float3 getPositionFG(in float2 uv) {
	// The use of SampleLevel fixes the following error:
	// warning X3595: gradient instruction used in a loop with varying iteration
	// This happens because the texture is sampled within an if statement (if FGFlag then...)
	return texPos.SampleLevel(sampPos, uv, 0).xyz;
}
//};

//class BackgroundPos : IPosition {
float3 getPositionBG(in float2 uv) {
	return texPos2.SampleLevel(sampPos2, uv, 0).xyz;
}
//};

inline float3 getNormal(in float2 uv) {
	return texNorm.SampleLevel(sampNorm, uv, 0).xyz;
}

inline float3 doSSDOIndirect(bool FGFlag, in float2 input_uv_sub, in float2 sample_uv, in float3 color,
	in float3 P, in float3 Normal /*, in float3 light,
	in float cur_radius, in float max_radius */)
{
	float3 occluder_Normal = getNormal(sample_uv); // I can probably read this normal off of the bent buffer later (?)
	float3 occluder_color = texColor.SampleLevel(sampColor, sample_uv, 0).xyz;
	float3 occluder = FGFlag ? getPositionFG(sample_uv) : getPositionBG(sample_uv);
	// diff: Vector from current pos (P) to the sampled neighbor
	const float3 diff = occluder - P;
	const float diff_sqr = dot(diff, diff);
	// v: Normalized (occluder - P) vector
	const float3 v = diff * rsqrt(diff_sqr);
	const float max_dist_sqr = max_dist * max_dist;
	const float weight = saturate(1 - diff_sqr / max_dist_sqr);

	//float ao_dot = max(0.0, dot(Normal, v) - bias);
	//float ao_factor = ao_dot * weight;
	// This formula is wrong; but it worked pretty well:
	//float visibility = saturate(1 - step(bias, ao_factor));
	//float visibility = 1 - ao_dot;
	//float2 uv_diff = sample_uv - input_uv_sub;
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
	float2 input_uv_sub = amplifyFactor * input.uv;
	float3 P1 = getPositionFG(input_uv_sub);
	float3 P2 = getPositionBG(input_uv_sub);
	float3 n = getNormal(input_uv_sub);
	float3 color = texColor.SampleLevel(sampColor, input_uv_sub, 0).xyz;
	//float3 diff =  texDiff.SampleLevel(sampDiff, input_uv_sub, 0).xyz;
	//float3 bentNormal = float3(0, 0, 0);
	// A value of bentNormalInit == 0.2 seems to work fine.
	//float3 bentNormal = bentNormalInit * n; // Initialize the bentNormal with the normal
	float3 ssdo = 0;
	float3 p;
	float radius = sample_radius;
	bool FGFlag;
	output.ssao = float4(color, 1);
	//output.bentNormal = float4(0, 0, 0, 1);

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
	if (p.z > INFINITY_Z) return output;

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
	//float max_radius = radius * (float)(samples - 1 + sample_jitter);

	// SSDO Indirect Calculation
	[loop]
	for (uint j = 0; j < samples; j++)
	{
		sample_uv = input_uv_sub + sample_direction.xy * (j + sample_jitter);
		sample_direction.xy = mul(sample_direction.xy, rotMatrix);
		ssdo += doSSDOIndirect(FGFlag, input_uv_sub, sample_uv, color,
			p, n /*, light, radius * (j + sample_jitter), max_radius */);
	}
	//ao = 1 - ao / (float)samples;
	ssdo = indirect_intensity * ssdo / (float)samples;
	//ssdo = saturate(ssdo / (float)samples);
	//output.ssao.xyz *= lerp(black_level, ao, ao);
	//if (debug == 3)
	output.ssao.xyz = ssdo;
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