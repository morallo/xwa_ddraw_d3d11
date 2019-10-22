/*
 * Screen-Space Directional Occlusion, based on Ritschel's paper with
 * code adapted from Pascal Gilcher's MXAO.
 * Adapted for XWA by Leo Reyes.
 * Licensed under the MIT license. See LICENSE.txt
 */
// The Foreground 3D position buffer (linear X,Y,Z)
Texture2D    texPos   : register(t0);
SamplerState sampPos  : register(s0);

// The Background 3D position buffer (linear X,Y,Z)
Texture2D    texPos2  : register(t1);
SamplerState sampPos2 : register(s1);

// The normal buffer
Texture2D    texNorm   : register(t2);
SamplerState sampNorm  : register(s2);

// The color buffer
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
	float4 ssao        : SV_TARGET0;
	float4 bentNormal  : SV_TARGET1; // Bent normal map output
};

// SSAOPixelShaderCBuffer
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
	float moire_offset, amplifyFactor;
	uint fn_enable;
	// 64 bytes
	float fn_max_xymult, fn_scale, fn_sharpness, nm_intensity;
	// 80 bytes
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
	return texNorm.Sample(sampNorm, uv).xyz;
}

struct ColNorm {
	float3 col;
	float3 N;
};

/*
 * From Pascal Gilcher's SSR shader.
 * https://github.com/martymcmodding/qUINT/blob/master/Shaders/qUINT_ssr.fx
 * (Used with permission from the author)
 */
float3 get_normal_from_color(float2 uv, float2 offset)
{
	float3 offset_swiz = float3(offset.xy, 0);
	float hpx = dot(texColor.SampleLevel(sampColor, float2(uv + offset_swiz.xz), 0).xyz, 0.333) * fn_scale;
	float hmx = dot(texColor.SampleLevel(sampColor, float2(uv - offset_swiz.xz), 0).xyz, 0.333) * fn_scale;
	float hpy = dot(texColor.SampleLevel(sampColor, float2(uv + offset_swiz.zy), 0).xyz, 0.333) * fn_scale;
	float hmy = dot(texColor.SampleLevel(sampColor, float2(uv - offset_swiz.zy), 0).xyz, 0.333) * fn_scale;

	float dpx = getPositionFG(uv + offset_swiz.xz).z;
	float dmx = getPositionFG(uv - offset_swiz.xz).z;
	float dpy = getPositionFG(uv + offset_swiz.zy).z;
	float dmy = getPositionFG(uv - offset_swiz.zy).z;

	float2 xymult = float2(abs(dmx - dpx), abs(dmy - dpy)) * fn_sharpness;
	//xymult = saturate(1.0 - xymult);
	xymult = saturate(fn_max_xymult - xymult);

	float3 normal;
	normal.xy = float2(hmx - hpx, hmy - hpy) * xymult / offset.xy * 0.5;
	normal.z = 1.0;

	return normalize(normal);
}

float3 blend_normals(float3 n1, float3 n2)
{
	//return normalize(float3(n1.xy*n2.z + n2.xy*n1.z, n1.z*n2.z));
	n1 += float3( 0,  0, 1);
	n2 *= float3(-1, -1, 1);
	return n1 * dot(n1, n2) / n1.z - n2;
}

// inline
inline ColNorm doSSDODirect(bool FGFlag, in float2 input_uv, in float2 sample_uv, in float3 color,
	in float3 P, in float3 Normal, in float3 light,
	in float cur_radius, in float max_radius,
	in float3 FakeNormal)
{
	ColNorm output;
	output.col = 0;
	output.N   = 0;
	float3 occluder = FGFlag ? getPositionFG(sample_uv) : getPositionBG(sample_uv);
	// diff: Vector from current pos (P) to the sampled neighbor
	//       If the occluder is farther than P, then diff.z will be positive
	//		 If the occluder is closer than P, then  diff.z will be negative
	const float3 diff = occluder - P;
	const float diff_sqr = dot(diff, diff);
	// v: Normalized (occluder - P) vector
	const float3 v = diff * rsqrt(diff_sqr);
	const float max_dist_sqr = max_dist * max_dist;
	const float weight = saturate(1 - diff_sqr / max_dist_sqr);

	float ao_dot = max(0.0, dot(Normal, v) - bias);
	float ao_factor = ao_dot * weight;
	// This formula is wrong; but it worked pretty well:
	//float visibility = saturate(1 - step(bias, ao_factor));
	float visibility = 1 - ao_dot;
	float2 uv_diff = sample_uv - input_uv;
	float3 B = 0;
	if (diff.z > 0.0) // occluder is farther than P -- no occlusion, visibility is 1.
	{
		B.x =  uv_diff.x;
		B.y = -uv_diff.y;
		//B.z =  -(max_radius - cur_radius);
		B.z = 0.01 * (max_radius - cur_radius) / max_radius;
		//B.z = 0.1;
		//B = -v;
		// Adding the normalized B to BentNormal seems to yield better normals
		// if B is added to BentNormal before normalization, the resulting normals
		// look more faceted
		if (fn_enable) B = blend_normals(nm_intensity * FakeNormal, B);
		B = normalize(B);
		//BentNormal += B;
		// I think we can get rid of the visibility term and just return the following
		// from this case or 0 outside this "if" block.
		//if (debug == 2)
		//	return intensity * saturate(dot(B, light));
		//return result + color * intensity * saturate(dot(B, light));
	}
	output.N = B;
	output.col = visibility * saturate(dot(B, light));
	if (debug == -1) output.col = visibility;
	//return visibility * saturate(dot(B, light));
	return output;

	//return visibility;
	//return result + color * ao_factor * saturate(dot(Normal, light));
	//return color * saturate(dot(Normal, light));

	/*
	//float3 ao = ao_factor;
	//if (ao_factor > 0.1)
	{
		float3 occluder_col = texColor.SampleLevel(sampColor, sample_uv, 0).xyz;
		float3 occluder_N	= texNorm.SampleLevel(sampNorm, sample_uv, 0).xyz;
		float3 diffuse		= texDiff.SampleLevel(sampDiff, sample_uv, 0).xyz;
		float occ_normal_factor = saturate(dot(Normal, occluder_N));
		//float occ_light_factor = dot(occluder_N, light);
		//float diff_factor = dot(0.333, diffuse);
		float diff_factor = diffuse.r;
		//occluder_col *= saturate(ssdo_area * occ_normal_factor * occ_light_factor * rsqrt(diff_sqr));
		//ssdo += occluder_col * occ_normal_factor * (1 - ao_dot) * weight * diff_factor;
		//ssdo += occluder_col * occ_normal_factor * weight * diff_factor;
		//ssdo += occluder_col * ao_factor * occ_normal_factor * diff_factor;
		ssdo += occluder_col * ao_factor * diff_factor;
	}
	ssdo = ssdo_area * ssdo;
	return intensity * ao;
	//float3 ssdo = intensity * ssdo_area * ao_factor * occ_light_factor * occ_cur_factor * occluder_col;
	//return intensity * lerp(ao, ssdo, ao_factor);
	//return intensity * pow(ao_factor, power);
	*/
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	ColNorm ssdo_aux;
	float3 P1 = getPositionFG(input.uv);
	float3 P2 = getPositionBG(input.uv);
	float3 n = getNormal(input.uv);
	float3 color = texColor.SampleLevel(sampColor, input.uv, 0).xyz;
	//float3 diff = texDiff.SampleLevel(sampDiff, input.uv, 0).xyz;
	//float3 bentNormal = float3(0, 0, 0);
	// A value of bentNormalInit == 0.2 seems to work fine.
	float3 bentNormal = bentNormalInit * n; // Initialize the bentNormal with the normal
	//float3 bentNormal = float3(0, 0, 0.01);
	float3 ssdo = 0;
	float3 p;
	float radius = sample_radius;
	bool FGFlag;
	
	output.ssao = 1;
	output.bentNormal = float4(0, 0, 0.01, 1);

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
	if (p.z > INFINITY_Z) return output; // SSAO

	// Enable perspective-correct radius
	if (z_division) 	radius /= p.z;

	float3 light = LightVector.xyz;
	//light = mul(viewMatrix, float4(light, 0)).xyz;

	float2 offset = float2(1 / screenSizeX, 1 / screenSizeY);
	float3 FakeNormal = 0; 
	if (fn_enable) FakeNormal = get_normal_from_color(input.uv, offset);

	float sample_jitter = dot(floor(input.pos.xy % 4 + 0.1), float2(0.0625, 0.25)) + 0.0625;
	float2 sample_uv, sample_direction;
	const float2x2 rotMatrix = float2x2(0.76465, -0.64444, 0.64444, 0.76465); //cos/sin 2.3999632 * 16 
	sincos(2.3999632 * 16 * sample_jitter, sample_direction.x, sample_direction.y); // 2.3999632 * 16
	// After sincos, sample_direction is unitary and 2D
	sample_direction *= radius;
	// Then we multiply it by "radius", so now sample_direction has length "radius"
	float max_radius = radius * (float)(samples + sample_jitter);

	// SSDO Direct Calculation
	[loop]
	for (uint j = 0; j < samples; j++)
	{
		sample_uv = input.uv + sample_direction.xy * (j + sample_jitter);
		sample_direction.xy = mul(sample_direction.xy, rotMatrix);
		ssdo_aux = doSSDODirect(FGFlag, input.uv, sample_uv, color,
			p, n, light,
			radius * (j + sample_jitter), max_radius,
			FakeNormal);
		ssdo += intensity * ssdo_aux.col;
		bentNormal += ssdo_aux.N;
	}
	output.ssao.xyz = ssdo / (float)samples;
	if (fn_enable)
		bentNormal = blend_normals(nm_intensity * FakeNormal, bentNormal); // bentNormal is not really used, it's just for debugging.
	float BLength = length(bentNormal);
	//if (BLength > 0.01) bentNormal /= BLength;
	bentNormal /= BLength;
	output.bentNormal.xyz = bentNormal * 0.5 + 0.5;
	//output.bentNormal.xyz = bentNormal;
	return output;

	/*
	float B = length(bentNormal);
	if (B > 0.001) {
		// The version that worked did the following:
		bentNormal = -bentNormal / B;

		//bentNormal = bentNormal / B;
		//output.bentNormal = float4(bentNormal, 1);
		//output.bentNormal = float4(bentNormal * 0.5 + 0.5, 1);
		output.bentNormal = float4(bentNormal, 1);
		if (debug == 1)
			output.bentNormal = float4(bentNormal.xxx * 0.5 + 0.5, 1);
		else if (debug == 2)
			output.bentNormal = float4(bentNormal.yyy * 0.5 + 0.5, 1);
		else if (debug == 3)
			output.bentNormal = float4(bentNormal.zzz * 0.5 + 0.5, 1);
		else if (debug == 4)
			output.bentNormal = float4(bentNormal * 0.5 + 0.5, 1);
	}
	//output.bentNormal = float4(bentNormal, 1);
	*/
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